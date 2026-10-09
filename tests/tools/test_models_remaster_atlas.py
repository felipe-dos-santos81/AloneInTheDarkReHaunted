import numpy as np
import pytest

from aitd_models.blender.remaster import (BRIDGE, KIND_BODY, KIND_GLASS, KIND_OTHER, KIND_PALETTE, KIND_RAMP, atlas_paths, budget_level,
                                          corner_uv, engine_rest_vertices, front_weight, kind_of, linear_to_srgb,
                                          mirror_uv, projection, srgb_to_linear, triangle_groups)
from aitd_models.body import PRIM_LINE, PRIM_POLY, PRIM_SPHERE, parse_body
from model_helpers import TINY_PNG, body_bytes


def chain():
    return parse_body(body_bytes())


def test_engine_rest_vertices_chain_pivots_already_offset():
    # computeRestPoseVertices: group 2 hangs from vertex 4, which group 1 has already moved.
    rest = engine_rest_vertices(chain())
    assert rest[3].tolist() == [0, -300, 0]
    assert rest[4].tolist() == [0, -200, 0]
    assert rest[5].tolist() == [30, -250, 0]
    assert rest[8].tolist() == [70, 100, 10]


def test_projection_pads_five_percent_and_floors_a_flat_range_at_one():
    pmin, prange = projection(np.array([[0.0, -100.0, 0.0], [200.0, -100.0, 5.0]]))
    assert pmin.tolist() == [-10.0, -100.05]
    assert prange.tolist() == pytest.approx([220.0, 1.1])


def test_corner_uv_front_left_half_back_mirrored_right_half():
    pmin, prange = np.array([0.0, 0.0]), np.array([100.0, 200.0])
    pts = np.array([[0.0, 0.0], [100.0, 50.0]])
    assert corner_uv(pts, pmin, prange, True).tolist() == [[0.0, 1.0], [0.5, 0.75]]
    assert corner_uv(pts, pmin, prange, False).tolist() == [[1.0, 1.0], [0.5, 0.75]]


@pytest.mark.parametrize("v, folded", [(0.3, 0.3), (0.0, 0.005), (1.0, 0.995), (1.25, 0.75), (-0.2, 0.2)])
def test_mirror_uv_folds_and_keeps_off_the_edges(v, folded):
    assert mirror_uv(np.array([v]))[0] == pytest.approx(folded)


@pytest.mark.parametrize("p2, weight", [
    ((0, 10, 0), 1.0),        # engine cross z < 0: faces the front
    ((0, -10, 0), 0.0),       # faces the back
    ((0, 0, 10), 0.5),        # edge-on
    ((0, 10, 30), 0.5 + 10 / np.hypot(10, 30)),  # tilted: blended
])
def test_front_weight(p2, weight):
    assert front_weight((0, 0, 0), (-10, 0, 0), p2) == pytest.approx(min(weight, 1.0))


def test_atlas_paths_prefer_flat_then_the_key_then_an_alias(tmp_path):
    for name in ("body_A.png", "flat_A.png", "ramp_B.png"):
        (tmp_path / name).write_bytes(TINY_PNG)
    paths = atlas_paths("A", ["B"], tmp_path)
    assert paths == {"body": tmp_path / "flat_A.png", "ramp": tmp_path / "ramp_B.png", "other": None}


FULL = {"body": "b", "ramp": "r", "other": "o"}
NO_RAMP = {"body": "b", "ramp": None, "other": "o"}


@pytest.mark.parametrize("prim_type, material, paths, kind", [
    (PRIM_POLY, 0, FULL, KIND_BODY), (PRIM_POLY, 4, FULL, KIND_RAMP), (PRIM_POLY, 1, FULL, KIND_OTHER),
    (PRIM_POLY, 2, FULL, KIND_GLASS), (PRIM_SPHERE, 2, FULL, KIND_GLASS),  # transparent: glass, never an atlas
    (PRIM_SPHERE, 0, FULL, KIND_PALETTE), (PRIM_LINE, 0, FULL, KIND_PALETTE), (9, 0, FULL, KIND_PALETTE),
    (PRIM_POLY, 4, NO_RAMP, KIND_PALETTE),                  # no ramp atlas on disk: palette
])
def test_kind_of_follows_the_engine(prim_type, material, paths, kind):
    assert kind_of(prim_type, material, paths) == kind


def test_triangle_groups_keep_a_triangle_spanning_groups_as_a_bridge():
    groups = np.array([[0, 0, 0], [1, 0, 0], [2, 2, 2]])
    assert triangle_groups(groups).tolist() == [0, BRIDGE, 2]


def test_srgb_conversions_match_the_standard_and_round_trip():
    assert srgb_to_linear(np.array([0.5]))[0] == pytest.approx(0.21404, abs=1e-5)
    assert linear_to_srgb(np.array([0.001]))[0] == pytest.approx(0.01292)
    c = np.linspace(0, 1, 11)
    assert linear_to_srgb(srgb_to_linear(c)) == pytest.approx(c)


# Bridges stay as they are: Carnby's 340 triangles, 121 of them bridges, fit level 3.
@pytest.mark.parametrize("triangles, bridges, level", [(340, 0, 2), (340, 121, 3), (360, 48, 3), (361, 48, 2),
                                                       (5000, 0, 1), (5001, 0, 0), (1, 0, 4)])
def test_budget_level_is_the_highest_within_30000(triangles, bridges, level):
    assert budget_level(triangles, bridges) == level
