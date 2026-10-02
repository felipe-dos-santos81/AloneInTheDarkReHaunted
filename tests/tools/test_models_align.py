import numpy as np

from aitd_models.align import Similarity, align, rigid_fit, yaw
from aitd_models.mesh import Surface
from model_helpers import chain_rest_mesh, subdivide


def target():
    _body, _rest, mesh = chain_rest_mesh()
    return mesh.surface


def test_rigid_fit_recovers_a_rotation_and_offset():
    rng = np.random.default_rng(0)
    pts = rng.normal(size=(50, 3)) * 100
    s = Similarity(1.0, yaw(33) @ np.array([[1, 0, 0], [0, 0, -1], [0, 1, 0]]), np.array([5.0, -7.0, 11.0]))
    fit = rigid_fit(pts, s.apply(pts))
    assert fit.scale == 1.0
    assert np.allclose(fit.apply(pts), s.apply(pts), atol=1e-9)


def test_then_composes_in_order():
    a = Similarity(2.0, yaw(90), np.array([1.0, 0, 0]))
    b = Similarity(0.5, yaw(-30), np.array([0, 3.0, 0]))
    p = np.array([[10.0, 20.0, 30.0]])
    assert np.allclose(a.then(b).apply(p), b.apply(a.apply(p)))


def test_identity_delivery_aligns_to_identity():
    pos, tris = surface = target()
    height = np.ptp(pos[:, 1])
    s = align(surface, surface)
    assert abs(s.scale - 1) < 5e-3
    assert np.abs(s.rotation - np.eye(3)).max() < 1e-2
    assert np.linalg.norm(s.apply(pos) - pos, axis=1).max() < 5e-3 * height


def test_recovers_rotation_scale_and_offset():
    pos, tris = target()
    height = np.ptp(pos[:, 1])
    dense, dense_tris = subdivide(pos, tris)
    rng = np.random.default_rng(1)
    jittered = dense + rng.normal(scale=1e-3 * height, size=dense.shape)
    moved = Similarity(0.37, yaw(90), np.array([123.0, -45.0, 300.0])).apply(jittered)
    back = align(Surface(moved, dense_tris), target()).apply(moved)
    err = np.linalg.norm(back - dense, axis=1)
    assert np.percentile(err, 95) < 5e-3 * height


def test_a_delivery_facing_backwards_is_turned_round():
    pos, tris = target()
    height = np.ptp(pos[:, 1])
    turned = Similarity(1.0, yaw(180), np.zeros(3)).apply(pos)
    back = align(Surface(turned, tris), target()).apply(turned)
    assert np.linalg.norm(back - pos, axis=1).max() < 1e-2 * height



def displaced(pos, tris, amount, seed=0):
    """`pos` pushed along its smooth normals by a low-frequency field of
    amplitude `amount` x height: a mesh that resembles the original the way
    generated art does, not a noisy copy of it."""
    face = np.cross(pos[tris[:, 1]] - pos[tris[:, 0]], pos[tris[:, 2]] - pos[tris[:, 0]])
    normals = np.zeros_like(pos)
    for k in range(3):
        np.add.at(normals, tris[:, k], face)
    normals /= np.maximum(np.linalg.norm(normals, axis=1, keepdims=True), 1e-9)
    height = np.ptp(pos[:, 1])
    rng = np.random.default_rng(seed)
    field = np.zeros(len(pos))
    for _ in range(4):
        k = rng.normal(size=3)
        field += np.sin(2 * np.pi * (pos @ (k / np.linalg.norm(k))) / (0.5 * height) + rng.uniform(0, 6.3))
    return pos + (amount * height * field / 4)[:, None] * normals


def test_a_mesh_that_only_resembles_the_original_keeps_its_size_and_feet():
    pos, tris = target()
    height = np.ptp(pos[:, 1])
    dense, dense_tris = subdivide(*subdivide(*subdivide(pos, tris)))
    art = displaced(dense, dense_tris, 0.02)
    moved = Similarity(0.37, yaw(30), np.array([10.0, -20.0, 5.0])).apply(art)
    s = align(Surface(moved, dense_tris), target())
    back = s.apply(moved)
    assert abs(s.scale * 0.37 - 1) < 0.02
    assert abs(back[:, 1].max() - art[:, 1].max()) < 0.01 * height
