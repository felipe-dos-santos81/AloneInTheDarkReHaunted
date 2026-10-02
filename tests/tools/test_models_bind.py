import numpy as np

from aitd_models.bind import allowed_groups, bind, closest_on_triangles, pack, weld
from aitd_models.pose import ROTATE, pose_float
from model_helpers import chain_rest_mesh, subdivide


def chain_bind(positions, triangles):
    body, _rest, mesh = chain_rest_mesh()
    return bind(positions, triangles, mesh.positions.reshape(-1, 3, 3), mesh.groups.reshape(-1, 3),
                [g.parent for g in body.groups])


def test_closest_point_regions():
    a, b, c = np.array([[0.0, 0, 0]]), np.array([[10.0, 0, 0]]), np.array([[0.0, 10, 0]])
    pts = np.array([[-5.0, -5, 0], [5, -5, 0], [2, 2, 3], [20, 0, 0]])
    dist, bary = closest_on_triangles(pts, a, b, c)
    assert np.allclose(dist[:, 0], [np.hypot(5, 5), 5, 3, 10])
    assert np.allclose(bary[:, 0], [(1, 0, 0), (0.5, 0.5, 0), (0.6, 0.2, 0.2), (0, 1, 0)])


def test_pack_keeps_four_and_sums_to_255():
    w = np.array([[0.5, 0.2, 0.1, 0.1, 0.1, 0.0], [1, 0, 0, 0, 0, 0], [1 / 3, 1 / 3, 1 / 3, 0, 0, 0]])
    joints, q = pack(w)
    assert (q.astype(int).sum(axis=1) == 255).all()
    assert joints[0].tolist()[:2] == [0, 1] and q[0, 0] > q[0, 1]
    assert joints[1].tolist() == [0, 0, 0, 0] and q[1].tolist() == [255, 0, 0, 0]
    assert sorted(q[2].tolist()) == [0, 85, 85, 85]


def test_allowed_groups_is_self_parent_children():
    ok = allowed_groups([-1, 0, 1, 0])
    assert ok[1].tolist() == [True, True, True, False]
    assert ok[3].tolist() == [True, False, False, True]


def test_weld_merges_equal_positions():
    ids = weld(np.array([[0.0, 0, 0], [1, 0, 0], [0, 0, 0.0000001], [1, 0, 0]]))
    assert ids[0] == ids[2] and ids[1] == ids[3] and ids[0] != ids[1]


def test_identity_binds_every_corner_to_its_group():
    _body, _rest, mesh = chain_rest_mesh()
    b = chain_bind(mesh.positions, np.arange(len(mesh.positions)).reshape(-1, 3))
    assert (b.labels == mesh.groups).mean() >= 0.98
    assert not b.ambiguous.any()
    assert (b.packed.astype(int).sum(axis=1) == 255).all()


def test_posed_subdivision_follows_the_original_away_from_seams():
    """A vertex whose closest original triangle lies in one group must move
    with that group: within 2 % of the height over 50 random poses."""
    body, rest, mesh = chain_rest_mesh()
    soup = np.arange(len(mesh.positions)).reshape(-1, 3)
    pos, tris = subdivide(*subdivide(mesh.positions, soup))
    b = chain_bind(pos, tris)
    tri = mesh.positions.reshape(-1, 3, 3)
    dist, _ = closest_on_triangles(pos, tri[:, 0], tri[:, 1], tri[:, 2])
    corner_groups = mesh.groups.reshape(-1, 3)[dist.argmin(axis=1)]
    single = (corner_groups == corner_groups[:, :1]).all(axis=1)
    owner = corner_groups[:, 0]
    inverse_rest = np.linalg.inv(rest.group_matrices())
    height = np.ptp(mesh.positions[:, 1])
    hom = np.c_[pos, np.ones(len(pos))]
    rng = np.random.default_rng(2)
    worst = 0.0
    for _ in range(50):
        states = [(ROTATE, tuple(int(x) for x in rng.integers(-200, 200, 3))) for _ in body.groups]
        skin = pose_float(body, states).group_matrices() @ inverse_rest
        blended = sum((b.packed[:, k:k + 1] / 255.0) * np.einsum("vij,vj->vi", skin[b.joints[:, k]], hom)[:, :3]
                      for k in range(4))
        rigid = np.einsum("vij,vj->vi", skin[owner], hom)[:, :3]
        worst = max(worst, float(np.linalg.norm(blended - rigid, axis=1)[single].max()))
    assert worst < 0.02 * height


def test_overlapping_unrelated_parts_are_ambiguous():
    # Groups 2 (head, under 1) and 3 (leg, under 0) are unrelated; two
    # triangles, one per group, a hair apart.
    tri = np.array([[[0.0, 0, 0], [100, 0, 0], [0, 100, 0]], [[0.0, 0, 1], [100, 0, 1], [0, 100, 1]]])
    groups = np.array([[2, 2, 2], [3, 3, 3]])
    pos = np.array([[10.0, 10, 0.5], [20, 10, 0.5], [10, 20, 0.5]])
    b = bind(pos, np.array([[0, 1, 2]]), tri, groups, [-1, 0, 1, 0])
    assert b.ambiguous.all()


def test_smoothing_never_reaches_an_unrelated_group():
    _body, _rest, mesh = chain_rest_mesh()
    soup = np.arange(len(mesh.positions)).reshape(-1, 3)
    pos, tris = subdivide(mesh.positions, soup)
    b = chain_bind(pos, tris)
    ok = allowed_groups([-1, 0, 1, 0])
    assert (b.weights[~ok[b.labels]] == 0).all()


def test_a_degenerate_original_triangle_gives_no_nan():
    tri = np.array([[[0.0, 0, 0], [0, 0, 0], [0, 0, 0]], [[0.0, 0, 0], [100, 0, 0], [0, 100, 0]]])
    pos = np.array([[0.0, 0, 5], [50, 10, 5], [10, 50, 5]])
    b = bind(pos, np.array([[0, 1, 2]]), tri, np.array([[1, 1, 1], [0, 0, 0]]), [-1, 0])
    assert np.isfinite(b.weights).all()
    assert (b.packed.astype(int).sum(axis=1) == 255).all()
