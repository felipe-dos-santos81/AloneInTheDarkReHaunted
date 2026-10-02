import io

import numpy as np
from PIL import Image

from aitd_models.delivery import read_delivery
from aitd_models.gltf import read_glb
from aitd_models.identity import SWATCH, identity_glb
from aitd_models.original import build_original_glb
from model_helpers import chain_rest_mesh, synthetic_palette_rgb


def test_identity_delivery_is_the_rest_mesh_with_its_colours():
    body, _rest, mesh = chain_rest_mesh()
    original = build_original_glb(body, synthetic_palette_rgb())
    d = read_delivery(identity_glb(original))
    assert np.allclose(d.positions, mesh.positions, atol=1e-3)
    assert d.texture_size == (SWATCH, SWATCH)
    swatch = np.asarray(Image.open(io.BytesIO(d.texture)).convert("RGB"))
    texel = swatch[(d.uv[:, 1] * SWATCH).astype(int), (d.uv[:, 0] * SWATCH).astype(int)]
    assert np.array_equal(texel, np.round(mesh.colors * 255).astype(np.uint8))


def test_identity_is_static():
    body, _rest, _mesh = chain_rest_mesh()
    doc = read_glb(identity_glb(build_original_glb(body, synthetic_palette_rgb()))).doc
    assert "skins" not in doc and "animations" not in doc
