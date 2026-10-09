# SPDX-License-Identifier: GPL-2.0-only
"""The engine's HD body file, `body_<KEY>.hdm` (version 1).

Little-endian, no padding:

    offset 0   char[4] "AHDM", u16 version, u16 group count,
           8   u64 skeleton hash, u32 vertex count, u32 index count,
           24  u32 texture bytes, u8 texture kind (1 PNG, 2 JPEG), u8 flags
               (1: some texels are translucent), u8[2] zero
           32  vertices, 40 bytes each: f32x3 position, f32x3 normal, f32x2 uv,
               u8x4 joints, u8x4 weights (sum 255)
               u32 indices, the texture bytes, u32 CRC-32 of everything before

Positions and normals are engine space in the rest pose (y down, facing -z);
joints are bone groups. Texture alpha: below TRANSLUCENT_ALPHA a hole; up to
OPAQUE_ALPHA - 1 translucent, blended at its own alpha in a second pass (128 is
the engine's transparent material); from OPAQUE_ALPHA opaque. TatouSource/FitdLib/models/hdmMesh.cpp reads it and
rejects exactly what `read_hdm` rejects."""
from __future__ import annotations

import io
import struct
import zlib
from dataclasses import dataclass

import numpy as np
from PIL import Image

MAGIC = b"AHDM"
VERSION = 1
HEADER = struct.Struct("<4sHHQIIIBB2s")
MAX_GROUPS = 32
MAX_TRIANGLES = 50_000
MAX_VERTICES = 3 * MAX_TRIANGLES
MAX_TEXTURE_BYTES = 64 << 20
TEXTURE_PNG, TEXTURE_JPEG = 1, 2
TEXTURE_MIME = {TEXTURE_PNG: "image/png", TEXTURE_JPEG: "image/jpeg"}
FLAG_TRANSLUCENT = 1
TRANSLUCENT_ALPHA, OPAQUE_ALPHA = 128, 253  # the alpha classes; mipChain.h and model_ps.sc match
VERTEX = np.dtype([("position", "<f4", 3), ("normal", "<f4", 3), ("uv", "<f4", 2),
                   ("joints", "u1", 4), ("weights", "u1", 4)])
assert HEADER.size == 32 and VERTEX.itemsize == 40


class HdmError(ValueError):
    pass


@dataclass
class HdmMesh:
    group_count: int
    skeleton_hash: int      # skeleton.skeleton_hash as an integer
    vertices: np.ndarray    # (V,) VERTEX
    indices: np.ndarray     # (I,) uint32, I a multiple of 3
    texture: bytes
    texture_kind: int       # TEXTURE_PNG or TEXTURE_JPEG
    translucent: bool = False  # FLAG_TRANSLUCENT: the engine draws a blended pass


def check(mesh: HdmMesh) -> None:
    """Raise HdmError naming the first rule the mesh breaks."""
    v, i = mesh.vertices, mesh.indices
    if not 1 <= mesh.group_count <= MAX_GROUPS:
        raise HdmError(f"group count {mesh.group_count} outside 1..{MAX_GROUPS}")
    if not 3 <= len(v) <= MAX_VERTICES:
        raise HdmError(f"vertex count {len(v)} outside 3..{MAX_VERTICES}")
    if not len(i) or len(i) % 3 or len(i) // 3 > MAX_TRIANGLES:
        raise HdmError(f"index count {len(i)} is not 1..{MAX_TRIANGLES} triangles")
    if mesh.texture_kind not in (TEXTURE_PNG, TEXTURE_JPEG):
        raise HdmError(f"unknown texture kind {mesh.texture_kind}")
    if not 0 < len(mesh.texture) <= MAX_TEXTURE_BYTES:
        raise HdmError(f"texture of {len(mesh.texture)} bytes")
    if int(i.max()) >= len(v):
        raise HdmError(f"index {int(i.max())} >= vertex count {len(v)}")
    if int(v["joints"].max()) >= mesh.group_count:
        raise HdmError(f"joint {int(v['joints'].max())} >= group count {mesh.group_count}")
    sums = v["weights"].astype(np.int64).sum(axis=1)
    if (sums != 255).any():
        bad = int(np.flatnonzero(sums != 255)[0])
        raise HdmError(f"vertex {bad}: weights sum to {int(sums[bad])}, not 255")
    for field in ("position", "normal", "uv"):
        if not np.isfinite(v[field]).all():
            raise HdmError(f"non-finite {field}")


def has_translucent_texels(texture: bytes) -> bool:
    """Whether any texel of the PNG or JPEG is translucent (TRANSLUCENT_ALPHA
    up to OPAQUE_ALPHA - 1)."""
    alpha = np.asarray(Image.open(io.BytesIO(texture)).convert("RGBA"))[..., 3]
    return bool(((alpha >= TRANSLUCENT_ALPHA) & (alpha < OPAQUE_ALPHA)).any())


def write_hdm(mesh: HdmMesh) -> bytes:
    check(mesh)
    out = HEADER.pack(MAGIC, VERSION, mesh.group_count, mesh.skeleton_hash, len(mesh.vertices),
                      len(mesh.indices), len(mesh.texture), mesh.texture_kind,
                      FLAG_TRANSLUCENT if mesh.translucent else 0, b"\0\0")
    out += np.ascontiguousarray(mesh.vertices, VERTEX).tobytes()
    out += np.ascontiguousarray(mesh.indices, "<u4").tobytes() + mesh.texture
    return out + struct.pack("<I", zlib.crc32(out))


def read_hdm(data: bytes) -> HdmMesh:
    if len(data) < HEADER.size + 4:
        raise HdmError(f"file of {len(data)} bytes is too short")
    magic, version, groups, shash, nv, ni, nt, kind, flags, reserved = HEADER.unpack_from(data)
    if magic != MAGIC:
        raise HdmError("not an .hdm file (bad magic)")
    if version != VERSION:
        raise HdmError(f"unsupported version {version}")
    if flags & ~FLAG_TRANSLUCENT or reserved != b"\0\0":
        raise HdmError("reserved header bytes are not zero")
    if nv > MAX_VERTICES or ni > 3 * MAX_TRIANGLES or nt > MAX_TEXTURE_BYTES:
        raise HdmError("counts over budget")
    size = HEADER.size + nv * VERTEX.itemsize + 4 * ni + nt + 4
    if len(data) != size:
        raise HdmError(f"file is {len(data)} bytes, header says {size}")
    (crc,) = struct.unpack_from("<I", data, size - 4)
    if crc != zlib.crc32(data[:size - 4]):
        raise HdmError("CRC mismatch")
    p = HEADER.size
    vertices = np.frombuffer(data, VERTEX, nv, p).copy()
    p += nv * VERTEX.itemsize
    indices = np.frombuffer(data, "<u4", ni, p).astype(np.uint32)
    p += 4 * ni
    mesh = HdmMesh(groups, shash, vertices, indices, bytes(data[p:p + nt]), kind, bool(flags & FLAG_TRANSLUCENT))
    check(mesh)
    return mesh
