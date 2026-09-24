# SPDX-License-Identifier: GPL-2.0-only
"""Minimal binary glTF 2.0 (.glb) writing and reading, standard library +
numpy only. Covers what the model pipeline needs: float/uint accessors,
one buffer, nodes, a skin, animations, and embedded PNG/JPEG images."""
from __future__ import annotations

import json
import struct
from dataclasses import dataclass, field

import numpy as np

GLB_MAGIC = 0x46546C67  # "glTF"
CHUNK_JSON = 0x4E4F534A
CHUNK_BIN = 0x004E4942

FLOAT, UNSIGNED_BYTE, UNSIGNED_SHORT, UNSIGNED_INT = 5126, 5121, 5123, 5125
ARRAY_BUFFER = 34962
_DTYPES = {FLOAT: np.float32, UNSIGNED_BYTE: np.uint8, UNSIGNED_SHORT: np.uint16, UNSIGNED_INT: np.uint32}
_COMPONENTS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


class GltfError(ValueError):
    pass


def _pad(data: bytes, fill: bytes) -> bytes:
    return data + fill * (-len(data) % 4)


@dataclass
class GlbBuilder:
    """Accumulates one binary buffer and the JSON that indexes it."""
    doc: dict = field(default_factory=lambda: {"asset": {"version": "2.0", "generator": "aitd_models"}})
    _bin: bytearray = field(default_factory=bytearray)

    def _list(self, key: str) -> list:
        return self.doc.setdefault(key, [])

    def add(self, key: str, item: dict) -> int:
        items = self._list(key)
        items.append(item)
        return len(items) - 1

    def view(self, data: bytes, target: int | None = None) -> int:
        self._bin += b"\0" * (-len(self._bin) % 4)
        view = {"buffer": 0, "byteOffset": len(self._bin), "byteLength": len(data)}
        if target is not None:
            view["target"] = target
        self._bin += data
        return self.add("bufferViews", view)

    def accessor(self, array: np.ndarray, gltf_type: str, component: int = FLOAT, *,
                 target: int | None = None, normalized: bool = False, bounds: bool = False) -> int:
        array = np.ascontiguousarray(array, dtype=_DTYPES[component])
        count = array.shape[0]
        if array.size != count * _COMPONENTS[gltf_type]:
            raise GltfError(f"array shape {array.shape} does not fit {gltf_type}")
        acc = {"bufferView": self.view(array.tobytes(), target), "componentType": component,
               "count": count, "type": gltf_type}
        if normalized:
            acc["normalized"] = True
        if bounds:
            flat = array.reshape(count, -1)
            acc["min"] = [float(v) for v in flat.min(axis=0)]
            acc["max"] = [float(v) for v in flat.max(axis=0)]
        return self.add("accessors", acc)

    def image(self, data: bytes, mime: str) -> int:
        return self.add("images", {"bufferView": self.view(data), "mimeType": mime})

    def to_bytes(self) -> bytes:
        doc = dict(self.doc)
        if self._bin:
            doc["buffers"] = [{"byteLength": len(self._bin)}]
        js = _pad(json.dumps(doc, separators=(",", ":"), sort_keys=True).encode(), b" ")
        chunks = struct.pack("<II", len(js), CHUNK_JSON) + js
        if self._bin:
            bin_ = _pad(bytes(self._bin), b"\0")
            chunks += struct.pack("<II", len(bin_), CHUNK_BIN) + bin_
        return struct.pack("<III", GLB_MAGIC, 2, 12 + len(chunks)) + chunks


@dataclass
class Glb:
    doc: dict
    bin: bytes

    def accessor(self, index: int) -> np.ndarray:
        """Decode accessor `index` as float64 (normalized ints scaled to 0..1)."""
        try:
            acc = self.doc["accessors"][index]
            view = self.doc["bufferViews"][acc["bufferView"]]
            dtype = np.dtype(_DTYPES[acc["componentType"]])
            n = _COMPONENTS[acc["type"]]
        except (KeyError, IndexError, TypeError) as exc:
            raise GltfError(f"bad accessor {index}: {exc}")
        if "sparse" in acc:
            raise GltfError("sparse accessors are not supported")
        start = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
        stride = view.get("byteStride", dtype.itemsize * n)
        count = acc["count"]
        end = start + stride * (count - 1) + dtype.itemsize * n if count else start
        if end > view.get("byteOffset", 0) + view["byteLength"] or end > len(self.bin):
            raise GltfError(f"accessor {index} runs past its buffer view")
        out = (np.ndarray((count, n), dtype, buffer=self.bin, offset=start, strides=(stride, dtype.itemsize))
               .astype(np.float64) if count else np.zeros((0, n)))
        if acc.get("normalized"):
            out /= np.iinfo(dtype).max
        return out if n > 1 else out[:, 0]

    def image_bytes(self, index: int) -> tuple[bytes, str]:
        img = self.doc["images"][index]
        view = self.doc["bufferViews"][img["bufferView"]]
        start = view.get("byteOffset", 0)
        return self.bin[start:start + view["byteLength"]], img.get("mimeType", "")


def read_glb(data: bytes) -> Glb:
    if len(data) < 20:
        raise GltfError("file too small for a GLB")
    magic, version, length = struct.unpack_from("<III", data, 0)
    if magic != GLB_MAGIC or version != 2:
        raise GltfError("not a binary glTF 2.0 file")
    if length > len(data):
        raise GltfError(f"GLB says {length} bytes, file has {len(data)}")
    p, doc, bin_ = 12, None, b""
    while p + 8 <= length:
        size, kind = struct.unpack_from("<II", data, p)
        body = data[p + 8:p + 8 + size]
        if len(body) != size:
            raise GltfError("truncated GLB chunk")
        if kind == CHUNK_JSON and doc is None:
            try:
                doc = json.loads(body.decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError) as exc:
                raise GltfError(f"bad JSON chunk: {exc}")
        elif kind == CHUNK_BIN and not bin_:
            bin_ = bytes(body)
        p += 8 + size
    if doc is None:
        raise GltfError("GLB has no JSON chunk")
    return Glb(doc, bin_)


def trs_matrix(node: dict) -> np.ndarray:
    """A node's local 4x4 transform (matrix, or T @ R @ S)."""
    if "matrix" in node:
        return np.array(node["matrix"], float).reshape(4, 4).T
    t = np.array(node.get("translation", [0, 0, 0]), float)
    x, y, z, w = node.get("rotation", [0, 0, 0, 1])
    s = np.array(node.get("scale", [1, 1, 1]), float)
    r = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    m = np.eye(4)
    m[:3, :3] = r * s
    m[:3, 3] = t
    return m


def quaternion(r: np.ndarray) -> np.ndarray:
    """Unit quaternion (x, y, z, w) of the rotation nearest to 3x3 `r`
    (Shepperd's method: divide by the largest of the four candidates)."""
    u, _, vt = np.linalg.svd(np.asarray(r, float))
    m = u @ vt
    if np.linalg.det(m) < 0:
        raise GltfError("matrix is a reflection, not a rotation")
    trace = m[0, 0] + m[1, 1] + m[2, 2]
    if trace > 0:
        s = np.sqrt(trace + 1.0) * 2
        q = [(m[2, 1] - m[1, 2]) / s, (m[0, 2] - m[2, 0]) / s, (m[1, 0] - m[0, 1]) / s, s / 4]
    elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
        s = np.sqrt(1.0 + m[0, 0] - m[1, 1] - m[2, 2]) * 2
        q = [s / 4, (m[0, 1] + m[1, 0]) / s, (m[0, 2] + m[2, 0]) / s, (m[2, 1] - m[1, 2]) / s]
    elif m[1, 1] > m[2, 2]:
        s = np.sqrt(1.0 + m[1, 1] - m[0, 0] - m[2, 2]) * 2
        q = [(m[0, 1] + m[1, 0]) / s, s / 4, (m[1, 2] + m[2, 1]) / s, (m[0, 2] - m[2, 0]) / s]
    else:
        s = np.sqrt(1.0 + m[2, 2] - m[0, 0] - m[1, 1]) * 2
        q = [(m[0, 2] + m[2, 0]) / s, (m[1, 2] + m[2, 1]) / s, s / 4, (m[1, 0] - m[0, 1]) / s]
    q = np.array(q)
    return q / np.linalg.norm(q)
