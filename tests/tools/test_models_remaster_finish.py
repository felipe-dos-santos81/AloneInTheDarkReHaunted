import numpy as np
from aitd_models.blender.remaster import LUMA, SEAM_DISTANCE, close_seams, composite, linear_to_srgb, model_glb, soften
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


def test_close_seams_shares_the_vertices_where_pieces_meet():
    # piece A, a triangle whose open edge 0-1 has piece B's corner (1, 0, 0) on it, and piece C,
    # whose corner sits a hair (under SEAM_DISTANCE) from A's corner (0, 2, 0)
    hair = SEAM_DISTANCE * 0.6
    positions = np.array([[0, 0, 0], [2, 0, 0], [0, 2, 0],          # A
                          [1, 0, 0], [2, -1, 0], [1, -1, 0],        # B
                          [hair, 2, 0], [-1, 3, 0], [-1, 2, 0]], float)  # C
    loops = np.arange(9)
    uvs = np.array([[0, 0], [1, 0], [0, 1]] * 3, float)
    p, round_p, lv, uv = close_seams(positions, positions, loops, uvs)
    tri = lv.reshape(-1, 3)
    assert len(tri) == 4                                   # A split in two at B's corner
    at = np.flatnonzero((p == [1, 0, 0]).all(axis=1))
    assert len(at) == 2                                    # B's corner and A's new one, at one position
    a_corner = [k for k, v in enumerate(lv) if v in at and v >= 9]
    assert np.allclose(uv[a_corner], [0.5, 0.0])           # the new corner's UV halfway along A's edge
    assert np.array_equal(p[6], p[2])                      # C's corner moved onto A's (by less than SEAM_DISTANCE)
    assert np.allclose(np.delete(p[:9], 6, axis=0), np.delete(positions, 6, axis=0))  # nothing else moved


def test_pieces_meeting_at_a_seam_share_a_normal_but_a_thin_plate_keeps_two():
    # two pieces folded 30 degrees along the edge (0,0,0)-(0,1,0), and a plate: one triangle twice, wound both ways
    c, s = np.cos(np.radians(30)), np.sin(np.radians(30))
    fold = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 0], [0, 1, 0], [-c, 0, s]], float)
    plate = np.array([[5, 0, 0], [6, 0, 0], [5, 1, 0], [5, 0, 0], [5, 1, 0], [6, 0, 0]], float)
    positions = np.concatenate([fold, plate])
    loops = np.arange(12)
    uvs = np.tile([[0, 0], [1, 0], [0, 1]], (4, 1)).astype(float)
    d = read_delivery(model_glb(positions, positions, loops, uvs, TINY_PNG))
    def normals_at(blender_xyz):
        engine = np.array([blender_xyz[0], -blender_xyz[2], blender_xyz[1]]) * 1000.0  # glTF (x, z, -y), engine (x, -y, -z)
        return d.normals[np.flatnonzero(np.isclose(d.positions, engine).all(axis=1))]
    shared = normals_at([0, 1, 0])
    assert len(shared) == 2 and np.allclose(shared[0], shared[1])   # the fold shades as one surface
    sides = normals_at([5, 1, 0])
    assert len(sides) == 2 and float(sides[0] @ sides[1]) < -0.99  # the plate's two sides stay apart
