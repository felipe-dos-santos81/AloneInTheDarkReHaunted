import pytest

from aitd_models.body import INFO_ANIM, BodyError, parse_anim, parse_body
from model_helpers import CHAIN_GROUPS, CHAIN_ORDER, CHAIN_VERTICES, anim_bytes, body_bytes


def test_parses_the_chain_body():
    b = parse_body(body_bytes())
    assert b.flags == 3 and b.animated
    assert b.zv == (-100, 100, -300, 100, -50, 50)
    assert b.vertices == tuple(CHAIN_VERTICES)
    assert [(g.start, g.count, g.pivot, g.parent, g.self_id) for g in b.groups] == CHAIN_GROUPS
    assert b.order == tuple(CHAIN_ORDER)
    assert [(p.type, p.color, p.points, p.size) for p in b.primitives] == [
        (1, 10, (1, 3, 4), 0), (1, 20, (4, 5, 6), 0), (1, 30, (2, 7, 8), 0),
        (0, 40, (3, 5), 0), (3, 50, (6,), 25), (2, 60, (0,), 0)]


def test_reads_rest_group_states():
    states = [(0, (0, 0, 0)), (1, (1, 2, 3)), (2, (-4, 5, -6)), (112, (0, 0, 0))]
    b = parse_body(body_bytes(rest_states=states))
    assert [(g.state_type, g.delta) for g in b.groups] == states


def test_ignores_trailing_bytes_like_the_engine():
    assert parse_body(body_bytes(trailing=b"\xAA" * 96)).vertices == tuple(CHAIN_VERTICES)


def test_non_animated_body_has_no_groups():
    b = parse_body(body_bytes(flags=1, prims=[(1, 0, 5, [0, 1, 2], 0)]))
    assert not b.animated and b.groups == () and b.order == ()
    assert not b.flags & INFO_ANIM


def test_skips_texture_uvs_on_types_9_and_10():
    b = parse_body(body_bytes(prims=[(9, 0, 7, [0, 1, 2], 0), (2, 0, 8, [3], 0)]))
    assert [(p.type, p.points) for p in b.primitives] == [(9, (0, 1, 2)), (2, (3,))]


@pytest.mark.parametrize("raw, message", [
    (body_bytes()[:40], "truncated"),
    (body_bytes(prims=[(5, 0, 0, [0], 0)]), "unknown primitive type 5"),
    (body_bytes(flags=0x0A), "INFO_OPTIMISE"),
    (body_bytes(prims=[(1, 0, 5, [0, 1, 99], 0)]), "out of range"),
])
def test_rejects_bad_bodies(raw, message):
    with pytest.raises(BodyError, match=message):
        parse_body(raw)


def test_parses_an_animation():
    a = parse_anim(anim_bytes([
        (10, (0, 0, -5), [(0, (1, 2, 3)), (1, (4, 5, 6))]),
        (20, (1, 0, -7), [(2, (7, 8, 9)), (0, (-1, -2, -3))]),
    ]))
    assert a.num_groups == 2
    assert [f.timestamp for f in a.frames] == [10, 20]
    assert a.frames[1].step == (1, 0, -7)
    assert a.frames[1].states == ((2, (7, 8, 9)), (0, (-1, -2, -3)))


def test_rejects_the_aitd2_animation_layout():
    raw = anim_bytes([(10, (0, 0, 0), [(0, (1, 2, 3))])]) + b"\0" * 8
    with pytest.raises(BodyError, match="AITD1 layout"):
        parse_anim(raw)
