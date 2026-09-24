import numpy as np
import pytest

from aitd_models.body import PRIM_POLY, parse_anim, parse_body
from aitd_models.gltf import read_glb
from aitd_models.mesh import build_mesh
from aitd_models.original import build_original_glb, key_times, to_gltf_points
from aitd_models.pose import pose_float, rest_states, skin
from model_helpers import (CHAIN_REST, anim_bytes, body_bytes, chain_anim_bytes, gltf_skinned_positions,
                           rest, synthetic_palette_rgb)

PALETTE = synthetic_palette_rgb()


@pytest.fixture
def body():
    return parse_body(body_bytes())


@pytest.fixture
def anim():
    return parse_anim(chain_anim_bytes())


def engine_positions(body, states):
    return to_gltf_points(build_mesh(body, skin(body, pose_float(body, states).group_matrices()), PALETTE).positions)


def poly_corners(body):
    rest_mesh = build_mesh(body, np.array(CHAIN_REST, float), PALETTE)
    return np.repeat([body.primitives[p].type == PRIM_POLY for p in rest_mesh.prim_index], 3)


def test_axes_are_y_up_metres_front_plus_z():
    assert np.allclose(to_gltf_points([[10, -300, -40]]), [[0.010, 0.300, 0.040]])


def test_rest_pose_is_the_bind_pose(body):
    glb = read_glb(build_original_glb(body, PALETTE))
    assert np.allclose(gltf_skinned_positions(glb), engine_positions(body, rest_states(body)), atol=1e-9)
    assert [n["name"] for n in glb.doc["nodes"][:4]] == ["g00", "g01", "g02", "g03"]
    assert glb.doc["nodes"][0]["children"] == [1, 3] and glb.doc["nodes"][1]["children"] == [2]
    assert "animations" not in glb.doc


def test_every_key_reproduces_the_engine_pose(body, anim):
    glb = read_glb(build_original_glb(body, PALETTE, [("walk", anim)]))
    polys = poly_corners(body)
    frames = [anim.frames[0], anim.frames[1], anim.frames[0]]  # the clip loops back to frame 0
    for key, frame in enumerate(frames):
        got = gltf_skinned_positions(glb, 0, key)
        want = engine_positions(body, list(frame.states))
        assert np.abs(got - want)[polys].max() < 2e-4  # metres; the sine table is not quite orthonormal


def test_a_zoomed_group_gets_a_geo_joint(body, anim):
    glb = read_glb(build_original_glb(body, PALETTE, [("walk", anim)]))
    names = [n["name"] for n in glb.doc["nodes"]]
    assert "g02_geo" in names and "g01_geo" not in names
    geo = names.index("g02_geo")
    assert geo in glb.doc["nodes"][names.index("g02")]["children"]
    assert geo in glb.doc["skins"][0]["joints"]
    scale = [c for c in glb.doc["animations"][0]["channels"] if c["target"]["path"] == "scale"]
    assert [c["target"]["node"] for c in scale] == [geo]


def test_key_times_follow_the_25_hz_timer(anim):
    assert key_times(anim) == pytest.approx([0.0, 20 / 25, 20 / 25 + 10 / 25])
    assert key_times(parse_anim(anim_bytes([(7, (0, 0, 0), rest())]))) == [0.0]


def test_mesh_attributes(body):
    glb = read_glb(build_original_glb(body, PALETTE))
    attrs = glb.doc["meshes"][0]["primitives"][0]["attributes"]
    assert set(attrs) == {"POSITION", "NORMAL", "COLOR_0", "JOINTS_0", "WEIGHTS_0"}
    assert np.allclose(np.linalg.norm(glb.accessor(attrs["NORMAL"]), axis=1), 1.0)
    assert np.allclose(glb.accessor(attrs["WEIGHTS_0"]), [1, 0, 0, 0])
    assert glb.doc["materials"][0]["doubleSided"] is True


def test_rejects_an_animation_for_another_skeleton(body):
    three = parse_anim(anim_bytes([(5, (0, 0, 0), rest(3))]))
    with pytest.raises(ValueError, match="3 groups, body has 4"):
        build_original_glb(body, PALETTE, [("other", three)])


def test_degenerate_faces_get_a_unit_normal():
    # Real bodies carry zero-area faces; glTF requires every NORMAL to be unit length.
    body = parse_body(body_bytes(prims=[(1, 0, 70, [0, 1, 1], 0), (1, 0, 71, [1, 3, 4], 0)]))
    glb = read_glb(build_original_glb(body, PALETTE))
    normals = glb.accessor(glb.doc["meshes"][0]["primitives"][0]["attributes"]["NORMAL"])
    assert np.allclose(np.linalg.norm(normals, axis=1), 1.0)
