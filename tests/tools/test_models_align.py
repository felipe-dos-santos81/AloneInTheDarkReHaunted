import numpy as np

from aitd_models.align import IDENTITY, Similarity, align, umeyama, yaw
from model_helpers import chain_rest_mesh, subdivide


def target():
    _body, _rest, mesh = chain_rest_mesh()
    return mesh.positions, np.arange(len(mesh.positions)).reshape(-1, 3)


def test_umeyama_recovers_a_similarity():
    rng = np.random.default_rng(0)
    pts = rng.normal(size=(50, 3)) * 100
    s = Similarity(0.37, yaw(33) @ np.array([[1, 0, 0], [0, 0, -1], [0, 1, 0]]), np.array([5.0, -7.0, 11.0]))
    fit = umeyama(pts, s.apply(pts))
    assert np.allclose(fit.apply(pts), s.apply(pts), atol=1e-9)


def test_then_composes_in_order():
    a = Similarity(2.0, yaw(90), np.array([1.0, 0, 0]))
    b = Similarity(0.5, yaw(-30), np.array([0, 3.0, 0]))
    p = np.array([[10.0, 20.0, 30.0]])
    assert np.allclose(a.then(b).apply(p), b.apply(a.apply(p)))


def test_identity_delivery_aligns_to_identity():
    pos, tris = target()
    height = np.ptp(pos[:, 1])
    s = align(pos, tris, pos, tris)
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
    back = align(moved, dense_tris, pos, tris).apply(moved)
    err = np.linalg.norm(back - dense, axis=1)
    assert np.percentile(err, 95) < 5e-3 * height


def test_a_delivery_facing_backwards_is_turned_round():
    pos, tris = target()
    height = np.ptp(pos[:, 1])
    turned = Similarity(1.0, yaw(180), np.zeros(3)).apply(pos)
    back = align(turned, tris, pos, tris).apply(turned)
    assert np.linalg.norm(back - pos, axis=1).max() < 1e-2 * height


def test_identity_constant():
    p = np.array([[1.0, 2.0, 3.0]])
    assert np.array_equal(IDENTITY.apply(p), p)
