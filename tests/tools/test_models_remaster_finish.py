import numpy as np
from aitd_models.blender.remaster import LUMA, composite, linear_to_srgb, model_glb, soften
from aitd_models.delivery import read_delivery
from aitd_models.hdm import TRANSLUCENT_ALPHA
from model_helpers import TINY_PNG


def test_composite_darkens_by_ao_and_flips_rows():
    colour = np.ones((2, 1, 3))
    ao = np.array([[0.0], [1.0]])  # Blender row 0 is the bottom
    out = composite(colour, ao)
    assert out[:, 0, 0].tolist() == [255, round(255 * float(linear_to_srgb(np.array([0.7]))[0]))]
    # a transparency mask becomes alpha: translucent (the engine's material 2) where it is set
    out = composite(colour, ao, mask=np.array([[1.0], [0.0]]))
    assert out.shape == (2, 1, 4) and out[:, 0, 3].tolist() == [255, TRANSLUCENT_ALPHA]


def test_model_glb_round_trips_through_the_delivery_reader():
    # a flat quad in Blender space (z up) over a bent round surface; vertex 2 is one Blender vertex
    # with two UVs, a UV seam between the bent triangles, so its two glTF vertices must share a normal
    positions = np.array([[0, 0, 0], [1, 0, 0], [1, 1, 0], [0, 1, 0]], float)
    round_positions = np.array([[0, 0, 0], [1, 0, 0], [1, 0, 2], [0, 1, 2]], float)
    loops = np.array([0, 1, 2, 0, 2, 3])
    uvs = np.array([[0, 0], [1, 0], [1, 1], [0, 0], [0.5, 1], [0, 1]], float)
    delivery = read_delivery(model_glb(positions, round_positions, loops, uvs, TINY_PNG))
    assert len(delivery.positions) == 5            # vertex 2 split by its two UVs
    assert len(delivery.triangles) == 2
    # POSITION is the positions: Blender (x, y, z) -> glTF (x, z, -y) -> engine (x, -y, -z) / 0.001
    assert sorted(set(delivery.positions[:, 1].round().tolist())) == [0.0]
    assert sorted(set(delivery.positions[:, 0].round().tolist())) == [0.0, 1000.0]
    assert sorted(set(delivery.positions[:, 2].round().tolist())) == [0.0, 1000.0]
    # NORMAL is the round surface's: vertex 1 lies on its first triangle only, which faces Blender -y
    # (engine -z); the flat quad's normal would be Blender +z (engine -y)
    one = np.flatnonzero((delivery.positions == [1000.0, 0.0, 0.0]).all(axis=1))
    assert np.allclose(delivery.normals[one], [0.0, 0.0, -1.0])
    seam = np.flatnonzero((delivery.positions == [1000.0, 0.0, 1000.0]).all(axis=1))
    assert len(seam) == 2
    assert np.allclose(delivery.normals[seam[0]], delivery.normals[seam[1]])
    # v flipped: Blender's (0.5, 1) is glTF's (0.5, 0)
    assert any(np.allclose(row, [0.5, 0.0]) for row in delivery.uv)


def test_soften_evens_out_painted_facets_and_keeps_strong_detail():
    cloth = np.array([0.08, 0.2, 0.09])
    image = np.tile(cloth, (64, 64, 1))
    image[:32, 32:] *= np.exp(0.2)              # a painted facet: a low-contrast brightness step
    image[44:52, 44:52] *= np.exp(-2.0)         # a button: ten times the contrast, as wide as the radius
    image[:, :4] = 0.0                          # the bake's empty background
    out = soften(image, 8)
    brightness = np.log(np.maximum(out @ LUMA, 1e-12))
    assert brightness[16, 34] - brightness[16, 29] < 0.35 * 0.2   # measured 0.058
    assert brightness[48, 40] - brightness[48, 48] > 0.6 * 2.0    # measured 1.33
    assert np.allclose(out / out.sum(axis=-1, keepdims=True)[..., :1].clip(1e-12),
                       image / image.sum(axis=-1, keepdims=True)[..., :1].clip(1e-12))  # hue kept
    assert not out[:, :4].any()                 # the background stays empty
    # smoothing log brightness lowers the plain mean; the texture keeps its own (measured 2 % darker before)
    covered = image.max(axis=-1) > 0
    assert np.isclose((out @ LUMA)[covered].mean(), (image @ LUMA)[covered].mean(), rtol=1e-3)
    # a texel lifted past full brightness is scaled down whole, so composite's clip never shifts its hue
    lamp = np.tile([0.9, 0.9, 0.9], (16, 16, 1))
    lamp[:8] = [1.0, 0.6, 0.3]                 # a saturated glow beside brighter grey: lifted to 1.17 unclamped
    lifted = soften(lamp, 8)
    assert lifted.max() <= 1.0
    assert np.allclose(lifted / lifted.sum(axis=-1, keepdims=True), lamp / lamp.sum(axis=-1, keepdims=True))
    flat = np.tile(cloth, (16, 16, 1))
    flat[:, :4] = 0.0
    assert np.allclose(soften(flat, 8), flat)   # and never darkens the texels beside it
