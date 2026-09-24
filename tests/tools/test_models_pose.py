import random

import numpy as np
import pytest

from aitd_models.body import parse_body
from aitd_models.pose import pose_exact, pose_float, pose_int, rest_states, rotation, skin
from model_helpers import CHAIN_REST, body_bytes, rest

ROT_G1_Z90 = [(0, (0, 0, 0)), (0, (0, 0, 256)), (0, (0, 0, 0)), (0, (0, 0, 0))]
MIXED = [(0, (0, 0, 0)), (1, (0, -10, 5)), (2, (256, 0, -128)), (7, (9, 9, 9))]
DEPTH = [0, 0, 0, 1, 1, 2, 2, 1, 1]  # group depth of each chain vertex


@pytest.fixture
def b():
    return parse_body(body_bytes())


def float_pose(b, states, angles=(0, 0, 0)):
    return skin(b, pose_float(b, states, angles).group_matrices())


def test_rest_pose_applies_the_pivot_chain(b):
    assert rest_states(b) == rest()
    assert pose_int(b, rest()) == [list(v) for v in CHAIN_REST]
    assert np.array_equal(pose_exact(b, rest()), np.array(CHAIN_REST, float))
    assert np.allclose(float_pose(b, rest()), CHAIN_REST, atol=1e-12)


def test_integer_rotation_matches_the_engine_arithmetic(b):
    # Group 1 turns 90 degrees about z; its child (group 2) follows.
    assert pose_int(b, ROT_G1_Z90) == [
        [0, 0, 0], [0, -100, 0], [50, 0, 0], [198, -100, 0], [98, -100, 0],
        [146, -72, 0], [176, -100, 20], [50, 100, 0], [70, 100, 10]]
    assert np.allclose(pose_exact(b, ROT_G1_Z90)[3:7], [
        [199.994, -100, 0], [99.997, -100, 0], [149.995, -70.001, 0], [179.995, -100, 20]], atol=1e-3)


def test_translate_zoom_and_junk_types(b):
    # Group 1 translates, group 2 zooms (x2 in x, x0.5 in z), type 7 is a no-op.
    assert pose_int(b, MIXED) == [
        [0, 0, 0], [0, -100, 0], [50, 0, 0], [0, -310, 5], [0, -210, 5],
        [60, -260, 5], [0, -290, 15], [50, 100, 0], [70, 100, 10]]


def test_group_zero_takes_the_actor_angles(b):
    ignored = [(0, (300, 300, 300))] + rest()[1:]
    assert pose_int(b, ignored) == pose_int(b, rest())
    assert pose_int(b, rest(), (0, 256, 0)) != pose_int(b, rest())


def test_a_full_turn_is_not_skipped(b):
    # 1024 & 0x3FF == 0, but the engine only skips a raw 0: table[0] is 4.
    assert pose_int(b, rest(), (0, 1024, 0))[2] == [48, 0, 0]
    assert float_pose(b, rest(), (0, 1024, 0))[2][0] == pytest.approx(49.998, abs=1e-3)
    assert np.array_equal(rotation((0, 0, 0)), np.eye(3))


def test_rotation_order_is_y_then_x_then_z():
    d = (100, 200, 300)
    assert np.allclose(rotation(d), rotation((0, 0, 300)) @ rotation((100, 0, 0)) @ rotation((0, 200, 0)))


def test_float_matrices_equal_the_exact_engine_loops(b):
    rng = random.Random(7)
    for _ in range(300):
        states = [(rng.choice([0, 0, 0, 1, 2, 5]), tuple(rng.randrange(-1024, 1024) for _ in range(3)))
                  for _ in range(4)]
        states[0] = (rng.choice([0, 2, 5]), states[0][1])  # a translating root has no matrix form
        angles = tuple(rng.randrange(-2048, 2048) for _ in range(3))
        assert np.allclose(float_pose(b, states, angles), pose_exact(b, states, angles), atol=1e-9)


def test_integer_drift_stays_within_the_depth_bound(b):
    rng = random.Random(11)
    bound = 10 * (np.array(DEPTH) + 1)
    for _ in range(300):
        states = [(0, tuple(rng.randrange(-1024, 1024) for _ in range(3))) for _ in range(4)]
        err = np.abs(np.array(pose_int(b, states), float) - pose_exact(b, states)).max(axis=1)
        assert (err <= bound).all()


def test_rejects_a_translating_root(b):
    with pytest.raises(ValueError, match="group 0 translates"):
        pose_float(b, [(1, (0, 0, 0))] + rest()[1:], (5, 0, 0))


def test_rejects_a_state_list_of_the_wrong_length(b):
    with pytest.raises(ValueError, match="3 states for 4 groups"):
        pose_int(b, rest(3))
