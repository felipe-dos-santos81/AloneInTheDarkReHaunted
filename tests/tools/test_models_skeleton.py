from aitd_models.body import parse_body
from aitd_models.skeleton import children, descendants, owners, skeleton_hash, validate
from model_helpers import CHAIN_GROUPS, CHAIN_VERTICES, body_bytes

CHAIN_HASH = "22a03bb7f51b6bf4"  # also pinned in TatouSource/tests/engine/test_body_skeleton.cpp


def chain(**kwargs):
    return parse_body(body_bytes(**kwargs))


def test_chain_is_valid():
    assert validate(chain()) == []


def test_owners_children_descendants():
    b = chain()
    assert owners(b) == [0, 0, 0, 1, 1, 2, 2, 3, 3]
    assert children(b) == [[1, 3], [2], [], []]
    assert descendants(b, 0) == [1, 2, 3]
    assert descendants(b, 1) == [2]


def test_reports_each_broken_invariant():
    groups = list(CHAIN_GROUPS)
    assert validate(chain(flags=1, prims=[])) == ["not animated (no INFO_ANIM)"]
    bad_parent = groups[:1] + [(3, 2, 1, 2, 1)] + groups[2:]
    assert "group 1: parent 2 is not an earlier group" in validate(chain(groups=bad_parent))
    bad_pivot = groups[:1] + [(3, 2, 5, 0, 1)] + groups[2:]
    assert "group 1: pivot vertex 5 is not in parent group 0" in validate(chain(groups=bad_pivot))
    gap = groups[:3] + [(7, 1, 2, 0, 3)]
    assert "group vertex ranges do not cover every vertex exactly once" in validate(chain(groups=gap))
    assert "group 1 is processed before one of its descendants" in validate(chain(order=[1, 2, 3, 0]))
    bad_self = groups[:3] + [(7, 2, 2, 0, 9)]
    assert "group 3: m_numGroup is 9" in validate(chain(groups=bad_self))
    moved_root = [(1, 0, 0)] + CHAIN_VERTICES[1:]
    assert "root pivot is not vertex 0 at the origin" in validate(chain(vertices=moved_root))


def test_rejects_vertex_zero_outside_the_root_and_a_body_without_vertices():
    # The engine's root pivot pass adds vertex 0 to every root vertex; the
    # float pose assumes it stays at the origin, which only a root vertex does.
    stray = chain(vertices=[(0, 0, 0), (10, -50, 0), (0, -100, 0), (5, -20, 3)],
                  groups=[(1, 2, 0, -1, 0), (0, 1, 2, 0, 1), (3, 1, 2, 0, 2)], order=[1, 2, 0], prims=[])
    assert validate(stray) == ["vertex 0 is not in the root group"]
    empty = chain(vertices=[], groups=[(0, 0, 0, -1, 0)], order=[0], prims=[])
    assert validate(empty) == ["body has no vertices"]

def test_rejects_more_than_32_groups():
    verts = [(0, 0, 0)] * 33
    groups = [(0, 1, 0, -1, 0)] + [(i, 1, 0, 0, i) for i in range(1, 33)]
    assert validate(chain(vertices=verts, groups=groups, order=list(range(32, -1, -1)), prims=[])) == [
        "33 groups (must be 1..32)"]


def test_hash_is_pinned_and_sensitive():
    # The engine (M2) computes this same value: keep it in sync with bodyPose.cpp.
    assert skeleton_hash(chain()) == CHAIN_HASH
    moved = [*CHAIN_VERTICES[:8], (21, 100, 10)]
    assert skeleton_hash(chain(vertices=moved)) != CHAIN_HASH
    recoloured = chain(prims=[(1, 0, 99, [1, 3, 4], 0)])
    assert skeleton_hash(recoloured) == CHAIN_HASH  # primitives are not part of the skeleton
