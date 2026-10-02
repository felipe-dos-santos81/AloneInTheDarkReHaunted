# SPDX-License-Identifier: GPL-2.0-only
"""The identity delivery: an exported original.glb turned into a model.glb
that meets the generator contract, so import and the runtime can be proven
before any generated art exists. The mesh is the original rest mesh (same
axes, scale and pose); its palette colours move from COLOR_0 into a 16x16
base-colour texture, one texel per colour, sampled at texel centres."""
from __future__ import annotations

import numpy as np

from aitd_textures.files import png_bytes

from .gltf import ARRAY_BUFFER, GlbBuilder, GltfError, read_glb

SWATCH = 16
NEAREST, CLAMP = 9728, 33071


def identity_glb(original: bytes) -> bytes:
    glb = read_glb(original)
    attrs = glb.doc["meshes"][0]["primitives"][0]["attributes"]
    positions = glb.accessor(attrs["POSITION"])
    normals = glb.accessor(attrs["NORMAL"])
    colours = np.round(glb.accessor(attrs["COLOR_0"]) * 255).astype(np.uint8)
    unique, slot = np.unique(colours, axis=0, return_inverse=True)
    if len(unique) > SWATCH * SWATCH:
        raise GltfError(f"{len(unique)} colours do not fit a {SWATCH}x{SWATCH} swatch")
    swatch = np.zeros((SWATCH * SWATCH, 3), np.uint8)
    swatch[:len(unique)] = unique
    slot = slot.reshape(-1)
    uv = np.stack([(slot % SWATCH + 0.5) / SWATCH, (slot // SWATCH + 0.5) / SWATCH], axis=1)

    g = GlbBuilder()
    image = g.image(png_bytes(swatch.reshape(SWATCH, SWATCH, 3)), "image/png")
    sampler = g.add("samplers", {"magFilter": NEAREST, "minFilter": NEAREST, "wrapS": CLAMP, "wrapT": CLAMP})
    texture = g.add("textures", {"source": image, "sampler": sampler})
    material = g.add("materials", {"name": "palette", "doubleSided": True, "pbrMetallicRoughness": {
        "baseColorTexture": {"index": texture}, "metallicFactor": 0.0, "roughnessFactor": 1.0}})
    primitive = {"attributes": {
        "POSITION": g.accessor(positions, "VEC3", target=ARRAY_BUFFER, bounds=True),
        "NORMAL": g.accessor(normals, "VEC3", target=ARRAY_BUFFER),
        "TEXCOORD_0": g.accessor(uv, "VEC2", target=ARRAY_BUFFER)}, "material": material}
    mesh = g.add("meshes", {"name": "body", "primitives": [primitive]})
    g.add("scenes", {"nodes": [g.add("nodes", {"name": "body", "mesh": mesh})]})
    g.doc["scene"] = 0
    return g.to_bytes()
