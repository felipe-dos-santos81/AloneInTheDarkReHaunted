import json

import pytest

from aitd_models.blender import remaster
from aitd_models.blender.remaster import (BRIDGE, KIND_BODY, KIND_GLASS, KIND_PALETTE, KIND_RAMP, EditError, Edits, corner_arrays,
                                          corner_uv, engine_rest_vertices, levels, projection, read_edits,
                                          srgb_to_linear)
from aitd_models.body import parse_body
from aitd_models.original import rest_mesh
from model_helpers import body_bytes, synthetic_palette_rgb


def chain():
    return parse_body(body_bytes())


def test_levels_take_an_edit_over_the_budget():
    assert levels(3, 340, 121, Edits(subdivide={1: 0})) == [3, 0, 3]  # Carnby: 121 of 340 are bridges


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
    {"crease": 5},
    {"crease": [1]},                    # not a name
    {"projection": "front"},            # wrong container
    {"projection": {"g01": ["front"]}},  # unhashable value
    {"skip": "x"},
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
    # triangles 0 and 2 (glass) lie in group 0 alone; triangle 1 spans groups 0 and 3
    body = parse_body(body_bytes(prims=[(1, 0, 10, [0, 1, 2], 0), (1, 0, 20, [2, 7, 8], 0), (1, 2, 30, [0, 1, 2], 0)]))
    _rest, mesh = rest_mesh(body, synthetic_palette_rgb())
    paths = {"body": "b", "ramp": None, "other": None}
    plain = corner_arrays(body, mesh, paths, Edits())
    assert plain["tri_group"].tolist() == [0, BRIDGE, 0]
    palette = corner_arrays(body, mesh, paths, Edits(projection={0: "palette"}))
    assert palette["kind"].tolist() == [KIND_PALETTE, KIND_BODY, KIND_GLASS]  # glass stays glass
    front = corner_arrays(body, mesh, paths, Edits(projection={0: "front"}))
    assert front["w_front"][0] == 1.0 and front["w_front"][1] == plain["w_front"][1]


def test_corner_arrays_keep_the_order_past_odd_polygons(monkeypatch):
    # a two-point polygon draws nothing; a type-9 polygon draws but takes no atlas;
    # a ramp material, whose UVs (front and back) go through mirror_uv; that is the identity inside the
    # projection box, so a marker stands in to make the wiring visible; then a transparent polygon and a
    # transparent sphere (material 2), which the engine draws blended too
    monkeypatch.setattr(remaster, "mirror_uv", lambda v: v + 10)
    prims = [(1, 0, 10, [1, 3], 0), (9, 0, 20, [4, 5, 6], 0), (1, 0, 30, [2, 7, 8], 0), (1, 4, 40, [2, 7, 8], 0),
             (1, 2, 50, [2, 7, 8], 0), (3, 2, 60, [2], 30)]
    body = parse_body(body_bytes(prims=prims))
    _rest, mesh = rest_mesh(body, synthetic_palette_rgb())
    arrays = corner_arrays(body, mesh, {"body": "b", "ramp": "r", "other": None}, Edits())
    assert arrays["kind"][:4].tolist() == [KIND_PALETTE, KIND_BODY, KIND_RAMP, KIND_GLASS]
    assert len(arrays["kind"]) > 4 and (arrays["kind"][4:] == KIND_GLASS).all()
    rest = engine_rest_vertices(body)
    pmin, prange = projection(rest)
    corners = rest[[2, 7, 8], :2]
    front, back = corner_uv(corners, pmin, prange, True), corner_uv(corners, pmin, prange, False)
    assert arrays["uv_front"][3:6] == pytest.approx(front)
    assert arrays["uv_back"][3:6] == pytest.approx(back)
    assert arrays["uv_front"][6:9] == pytest.approx(front + 10)
    assert arrays["uv_back"][6:9] == pytest.approx(back + 10)
