# SPDX-License-Identifier: GPL-2.0-only
"""Read a generator delivery, `data/models-ai/bodies/<KEY>/model.glb`
(docs/model-contract.md): one static textured mesh, Y up, facing +Z, any
scale and offset. Every triangle primitive of every mesh node in the default
scene is flattened into one triangle list in engine space (y down, facing
-z; see original.py). Skins and animations are ignored."""
from __future__ import annotations

import io
from dataclasses import dataclass

import numpy as np
from PIL import Image

from .gltf import Glb, GltfError, read_glb, trs_matrix
from .hdm import MAX_TRIANGLES, TEXTURE_JPEG, TEXTURE_PNG
from .original import FLIP, METRES_PER_UNIT

MAX_TEXTURE_SIDE = 4096
TRIANGLES = 4


class DeliveryError(ValueError):
    pass


@dataclass
class Delivery:
    positions: np.ndarray   # (V, 3) float64, engine space (glTF scale divided by 0.001)
    normals: np.ndarray     # (V, 3) float64 unit, engine space
    uv: np.ndarray          # (V, 2) float64
    triangles: np.ndarray   # (T, 3) int64
    texture: bytes
    texture_kind: int       # hdm.TEXTURE_PNG or hdm.TEXTURE_JPEG
    texture_size: tuple[int, int]


def _texture_kind(data: bytes) -> int:
    if data.startswith(b"\x89PNG\r\n\x1a\n"):
        return TEXTURE_PNG
    if data.startswith(b"\xff\xd8\xff"):
        return TEXTURE_JPEG
    raise DeliveryError("the base-colour image is neither PNG nor JPEG")


def _base_color_image(glb: Glb, material_index) -> tuple[int, int]:
    """(image index, texcoord set) of a material's base-colour texture."""
    if material_index is None:
        raise DeliveryError("a primitive has no material, so no base-colour texture")
    try:
        info = glb.doc["materials"][material_index]["pbrMetallicRoughness"]["baseColorTexture"]
        return glb.doc["textures"][info["index"]]["source"], info.get("texCoord", 0)
    except (KeyError, IndexError, TypeError):
        raise DeliveryError(f"material {material_index} has no base-colour texture")


def _vertex_normals(positions: np.ndarray, triangles: np.ndarray) -> np.ndarray:
    """Area-weighted smooth normals, for a delivery without NORMAL."""
    tri = positions[triangles]
    face = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    out = np.zeros_like(positions)
    for k in range(3):
        np.add.at(out, triangles[:, k], face)
    return out


def read_delivery(data: bytes) -> Delivery:
    try:
        glb = read_glb(data)
    except GltfError as exc:
        raise DeliveryError(str(exc))
    doc = glb.doc
    if doc.get("extensionsRequired"):
        raise DeliveryError(f"required extensions are not supported: {', '.join(doc['extensionsRequired'])}")
    nodes = doc.get("nodes", [])
    if doc.get("scenes"):
        roots = doc["scenes"][doc.get("scene", 0)].get("nodes", [])
    else:  # no scene: every node that is nobody's child
        children = {c for n in nodes for c in n.get("children", [])}
        roots = [i for i in range(len(nodes)) if i not in children]
    pos, nrm, uvs, tris, images = [], [], [], [], set()
    base = 0

    def walk(i: int, parent: np.ndarray) -> None:
        nonlocal base
        world = parent @ trs_matrix(nodes[i])
        if "mesh" in nodes[i]:
            normal_matrix = np.linalg.inv(world[:3, :3]).T
            for prim in doc["meshes"][nodes[i]["mesh"]]["primitives"]:
                if prim.get("mode", TRIANGLES) != TRIANGLES:
                    raise DeliveryError(f"primitive mode {prim['mode']} is not triangles")
                image, texcoord = _base_color_image(glb, prim.get("material"))
                images.add(image)
                attrs = prim["attributes"]
                if f"TEXCOORD_{texcoord}" not in attrs:
                    raise DeliveryError(f"a primitive has no TEXCOORD_{texcoord}")
                p = glb.accessor(attrs["POSITION"])
                n = glb.accessor(attrs["NORMAL"]) if "NORMAL" in attrs else None
                idx = (glb.accessor(prim["indices"]).astype(np.int64) if "indices" in prim
                       else np.arange(len(p), dtype=np.int64))
                if len(idx) % 3 or (len(idx) and (idx.min() < 0 or idx.max() >= len(p))):
                    raise DeliveryError("bad triangle indices")
                idx = idx.reshape(-1, 3)
                p = p @ world[:3, :3].T + world[:3, 3]
                n = _vertex_normals(p, idx) if n is None else n @ normal_matrix.T
                pos.append(p)
                nrm.append(n)
                uvs.append(glb.accessor(attrs[f"TEXCOORD_{texcoord}"]))
                tris.append(idx + base)
                base += len(p)
        for c in nodes[i].get("children", []):
            walk(c, world)

    try:
        for root in roots:
            walk(root, np.eye(4))
    except (KeyError, IndexError, TypeError, GltfError) as exc:
        raise DeliveryError(f"bad glTF structure: {exc}")
    if not tris:
        raise DeliveryError("no triangles in the default scene")
    if len(images) != 1:
        raise DeliveryError(f"{len(images)} base-colour images; the contract allows exactly one")
    triangles = np.concatenate(tris)
    if len(triangles) > MAX_TRIANGLES:
        raise DeliveryError(f"{len(triangles)} triangles (at most {MAX_TRIANGLES})")
    positions, normals, uv = np.concatenate(pos), np.concatenate(nrm), np.concatenate(uvs)
    tri = positions[triangles]
    if not np.linalg.norm(np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]), axis=1).sum() > 0:
        raise DeliveryError("the mesh has no area")
    if not (np.isfinite(positions).all() and np.isfinite(normals).all() and np.isfinite(uv).all()):
        raise DeliveryError("non-finite vertex data")
    length = np.linalg.norm(normals, axis=1, keepdims=True)
    normals = np.divide(normals, length, out=np.tile([0.0, 1.0, 0.0], (len(normals), 1)), where=length > 1e-12)
    texture, _mime = glb.image_bytes(images.pop())
    kind = _texture_kind(texture)
    try:
        size = Image.open(io.BytesIO(texture)).size
    except Exception as exc:  # Pillow raises several types for a corrupt image
        raise DeliveryError(f"unreadable base-colour image: {exc}")
    if max(size) > MAX_TEXTURE_SIDE:
        raise DeliveryError(f"texture is {size[0]}x{size[1]} (at most {MAX_TEXTURE_SIDE} per side)")
    return Delivery((positions @ FLIP.T) / METRES_PER_UNIT, normals @ FLIP.T, uv, triangles, texture, kind, size)
