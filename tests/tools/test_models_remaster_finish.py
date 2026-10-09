import numpy as np

from aitd_models.blender.remaster import composite, linear_to_srgb, model_glb
from aitd_models.delivery import read_delivery
from model_helpers import TINY_PNG


def test_composite_darkens_by_ao_and_flips_rows():
    colour = np.ones((2, 1, 3))
    ao = np.array([[0.0], [1.0]])  # Blender row 0 is the bottom
    out = composite(colour, ao)
    assert out[:, 0, 0].tolist() == [255, round(255 * float(linear_to_srgb(np.array([0.7]))[0]))]


def test_model_glb_round_trips_through_the_delivery_reader():
    # a bent quad in Blender space (z up); vertex 2 is one Blender vertex with two UVs,
    # a UV seam between the bent triangles, so its two glTF vertices must share a normal
    positions = np.array([[0, 0, 0], [1, 0, 0], [1, 0, 2], [0, 1, 2]], float)
    loops = np.array([0, 1, 2, 0, 2, 3])
    uvs = np.array([[0, 0], [1, 0], [1, 1], [0, 0], [0.5, 1], [0, 1]], float)
    delivery = read_delivery(model_glb(positions, loops, uvs, TINY_PNG))
    assert len(delivery.positions) == 5            # vertex 2 split by its two UVs
    assert len(delivery.triangles) == 2
    # Blender (x, y, z) -> glTF (x, z, -y) -> engine (x, -y, -z) / 0.001
    assert sorted(set(delivery.positions[:, 1].round().tolist())) == [-2000.0, 0.0]
    assert sorted(set(delivery.positions[:, 0].round().tolist())) == [0.0, 1000.0]
    assert sorted(set(delivery.positions[:, 2].round().tolist())) == [0.0, 1000.0]
    seam = np.flatnonzero((delivery.positions == [1000.0, -2000.0, 0.0]).all(axis=1))
    assert len(seam) == 2
    assert np.allclose(delivery.normals[seam[0]], delivery.normals[seam[1]])
    # v flipped: Blender's (0.5, 1) is glTF's (0.5, 0)
    assert any(np.allclose(row, [0.5, 0.0]) for row in delivery.uv)
