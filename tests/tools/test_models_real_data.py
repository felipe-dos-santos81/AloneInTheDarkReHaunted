"""Runs only when the AITD1 INDARK folder is present under data/aitd1. Pins
the facts about the real bodies that the model pipeline's design rests on
(docs/superpowers/specs/2026-09-23-hd-character-models-design.md §1, §3)."""
import pathlib
from collections import Counter

import numpy as np
import pytest

from aitd_models.body import parse_anim, parse_body
from aitd_models.pose import pose_exact, pose_float, pose_int, skin
from aitd_models.skeleton import skeleton_hash, validate
from aitd_textures.decode import DataNotFound, find_data_dir
from aitd_textures.pak import Pak

ROOT = pathlib.Path(__file__).resolve().parents[2]
PAIRS = (("LISTBODY", "LISTANIM"), ("LISTBOD2", "LISTANI2"))


@pytest.fixture(scope="module")
def game():
    try:
        data_dir = find_data_dir(ROOT / "data" / "aitd1")
    except DataNotFound:
        pytest.skip("no AITD1 game data under data/aitd1")
    out = {}
    for hqr, anim_hqr in PAIRS:
        bodies = {i: parse_body(raw) for i in range(Pak(data_dir / f"{hqr}.PAK").count)
                  for raw in [Pak(data_dir / f"{hqr}.PAK").read(i)]}
        anim_pak = Pak(data_dir / f"{anim_hqr}.PAK")
        anims = [parse_anim(anim_pak.read(i)) for i in range(anim_pak.count)]
        out[hqr] = ({i: b for i, b in bodies.items() if b.animated}, anims)
    return out


def test_every_animated_body_is_poseable(game):
    for hqr, (bodies, _anims) in game.items():
        assert len(bodies) == 107, hqr
        assert all(validate(b) == [] for b in bodies.values()), hqr
        assert max(len(b.groups) for b in bodies.values()) == 28
        assert all(g.delta == (0, 0, 0) for b in bodies.values() for g in b.groups)


def test_the_two_body_sets_differ_under_the_same_number(game):
    carnby, emily = game["LISTBODY"][0][11], game["LISTBOD2"][0][11]
    assert (len(carnby.vertices), len(emily.vertices)) == (163, 177)
    assert skeleton_hash(carnby) == "e62f145c363d9692"
    assert skeleton_hash(carnby) != skeleton_hash(emily)


def test_float_pose_matches_the_engine_on_real_animations(game):
    for bodies, anims in game.values():
        for b in bodies.values():
            depth = [0] * len(b.groups)
            for gi, g in enumerate(b.groups[1:], 1):
                depth[gi] = depth[g.parent] + 1
            bound = np.zeros(len(b.vertices))
            for gi, g in enumerate(b.groups):
                bound[g.start:g.start + g.count] = 10 * (depth[gi] + 1)
            for anim in [a for a in anims if a.num_groups == len(b.groups)][:2]:
                for frame in anim.frames:
                    states = list(frame.states)
                    assert states[0][0] == 0  # group 0 always rotates
                    exact = pose_exact(b, states, (0, 300, 0))
                    assert np.allclose(skin(b, pose_float(b, states, (0, 300, 0)).group_matrices()), exact, atol=1e-6)
                    drift = np.abs(np.array(pose_int(b, states, (0, 300, 0)), float) - exact).max(axis=1)
                    assert (drift <= bound).all()


def test_characters_face_minus_z(game):
    # Root steps move an actor along its facing (walkStep rotates them with
    # the body's own root matrix): most keyframes step toward -z.
    steps = Counter(np.sign(f.step[2]) for _b, anims in game.values() for a in anims for f in a.frames)
    assert steps[-1] > 2 * steps[1]
