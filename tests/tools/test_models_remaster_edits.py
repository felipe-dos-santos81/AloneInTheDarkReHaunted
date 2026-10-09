import json

import pytest

from aitd_models.blender.remaster import (BRIDGE, KIND_BODY, KIND_PALETTE, EditError, Edits, corner_arrays, corner_uv,
                                          engine_rest_vertices, levels, projection, read_edits, srgb_to_linear)
from aitd_models.body import parse_body
from aitd_models.original import rest_mesh
from model_helpers import body_bytes, synthetic_palette_rgb


def chain():
    return parse_body(body_bytes())


def test_levels_take_an_edit_over_the_budget():
    assert levels(3, 340, Edits(subdivide={1: 0})) == [2, 0, 2]


def write(tmp_path, doc):
    path = tmp_path / "K.json"
    path.write_text(json.dumps(doc))
    return path


def test_read_edits_without_a_file_is_no_edits(tmp_path):
    assert read_edits(tmp_path / "missing.json", 4) == Edits()


def test_read_edits_reads_every_field(tmp_path):
    edits = read_edits(write(tmp_path, {"subdivide": {"g01": 1}, "crease": ["g03"],
                                        "projection": {"g02": "palette"}, "skip": {"reason": "fused"}}), 4)
    assert edits == Edits({1: 1}, {3}, {2: "palette"}, "fused")


@pytest.mark.parametrize("doc", [
    [],                                 # not an object
    {"smooth": {"g01": 0.5}},          # unknown field
    {"subdivide": {"g04": 1}},          # no such group
    {"subdivide": {"g01": 5}},          # level out of range
    {"projection": {"g01": "side"}},    # not a projection
    {"skip": {"reason": " "}},          # no reason
    {"subdivide": [1]},                 # wrong container
    {"crease": 5},
    {"crease": [1]},                    # not a name
    {"projection": "front"},
    {"projection": {"g01": ["front"]}},  # unhashable value
    {"skip": "x"},
    {"g01": 1},                         # a group key at the top level
])
def test_read_edits_refuses_bad_files(tmp_path, doc):
    with pytest.raises(EditError):
        read_edits(write(tmp_path, doc), 4)



def test_corner_arrays_on_the_chain():
    palette = synthetic_palette_rgb()
    body = chain()
    _rest, mesh = rest_mesh(body, palette)
    arrays = corner_arrays(body, mesh, {"body": "b", "ramp": None, "other": None}, Edits())
    polys = mesh.prim_index < 3
    assert (arrays["kind"][polys] == KIND_BODY).all() and (arrays["kind"][~polys] == KIND_PALETTE).all()
    assert (arrays["tri_group"][polys] == BRIDGE).all()  # every chain polygon spans groups
    # the linear palette colour of each triangle's primitive
    want = srgb_to_linear(palette[[body.primitives[i].color for i in mesh.prim_index]] / 255.0)
    assert arrays["palette"] == pytest.approx(want, abs=1e-6)


def test_projection_edits_change_their_group_and_leave_bridges_alone():
    # triangle 0 lies in group 0 alone; triangle 1 spans groups 0 and 3
    body = parse_body(body_bytes(prims=[(1, 0, 10, [0, 1, 2], 0), (1, 0, 20, [2, 7, 8], 0)]))
    _rest, mesh = rest_mesh(body, synthetic_palette_rgb())
    paths = {"body": "b", "ramp": None, "other": None}
    plain = corner_arrays(body, mesh, paths, Edits())
    assert plain["tri_group"].tolist() == [0, BRIDGE]
    palette = corner_arrays(body, mesh, paths, Edits(projection={0: "palette"}))
    assert palette["kind"].tolist() == [KIND_PALETTE, KIND_BODY]
    front = corner_arrays(body, mesh, paths, Edits(projection={0: "front"}))
    assert front["w_front"][0] == 1.0 and front["w_front"][1] == plain["w_front"][1]


def test_corner_arrays_keep_the_order_past_odd_polygons():
    # a two-point polygon draws nothing; a type-9 polygon draws but takes no atlas
    prims = [(1, 0, 10, [1, 3], 0), (9, 0, 20, [4, 5, 6], 0), (1, 0, 30, [2, 7, 8], 0)]
    body = parse_body(body_bytes(prims=prims))
    _rest, mesh = rest_mesh(body, synthetic_palette_rgb())
    arrays = corner_arrays(body, mesh, {"body": "b", "ramp": None, "other": None}, Edits())
    assert arrays["kind"].tolist() == [KIND_PALETTE, KIND_BODY]
    rest = engine_rest_vertices(body)
    pmin, prange = projection(rest)
    assert arrays["uv_front"][3:] == pytest.approx(corner_uv(rest[[2, 7, 8], :2], pmin, prange, True))
