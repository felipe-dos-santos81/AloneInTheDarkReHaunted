import numpy as np

from aitd_models.body import parse_anim, parse_body
from aitd_models.gltf import read_glb
from aitd_models.original import FLIP, METRES_PER_UNIT, build_original_glb, rest_mesh
from aitd_models.pose import pose_float
from aitd_models.stretch import STRETCH_RATIO, posed_stretch, preview_skins, torn_pct
from model_helpers import body_bytes, chain_anim_bytes, gltf_skinned_positions, synthetic_palette_rgb


def chain_glb(animated=True):
    body = parse_body(body_bytes())
    anims = [("LISTANIM_000", parse_anim(chain_anim_bytes()))] if animated else []
    return body, read_glb(build_original_glb(body, synthetic_palette_rgb(), anims))


def rigid(groups):
    joints = np.zeros((len(groups), 4), int)
    joints[:, 0] = groups
    weights = np.zeros((len(groups), 4))
    weights[:, 0] = 1.0
    return joints, weights


def test_preview_skins_match_the_glb_skinning_at_every_key():
    body, glb = chain_glb()
    _rest, mesh = rest_mesh(body, synthetic_palette_rgb())
    skins = preview_skins(glb, len(body.groups))
    assert len(skins) == 3  # two frames, then the loop back to frame 0
    homog = np.c_[mesh.positions, np.ones(len(mesh.positions))]
    for k, m in enumerate(skins):
        ours = np.einsum("vij,vj->vi", m[mesh.groups], homog)[:, :3]
        theirs = gltf_skinned_positions(glb, 0, k) / METRES_PER_UNIT @ FLIP.T
        assert np.abs(ours - theirs).max() < 1e-3


def test_preview_skins_match_the_engine_pose():
    body, glb = chain_glb()
    anim = parse_anim(chain_anim_bytes())
    skins = preview_skins(glb, len(body.groups))
    rest = pose_float(body, [(0, (0, 0, 0))] * len(body.groups)).group_matrices()
    for k, frame in enumerate(anim.frames):
        posed = pose_float(body, frame.states[:len(body.groups)]).group_matrices()
        # original.glb stores float32 metres and quaternions: 0.012 units measured
        assert np.abs(skins[k] - posed @ np.linalg.inv(rest)).max() < 0.05


def test_a_body_without_animation_has_no_poses():
    body, glb = chain_glb(animated=False)
    assert preview_skins(glb, len(body.groups)) == []


def two_legs():
    """Two 12-unit-wide legs 46 units apart, each a quad of two triangles,
    and the poses: rest, then leg 1 swung 1000 units forward at the foot."""
    legs = np.array([(-35, 0, 0), (-23, 0, 0), (-23, 800, 0), (-35, 800, 0),
                     (23, 0, 0), (35, 0, 0), (35, 800, 0), (23, 800, 0)], float)
    tris = np.array([(0, 1, 2), (0, 2, 3), (4, 5, 6), (4, 6, 7)])
    groups = np.array([0, 0, 0, 0, 1, 1, 1, 1])
    swing = np.eye(4)
    swing[2, 1] = 1000 / 800  # shear: z grows with y, so the foot moves 1000 forward
    return legs, tris, groups, [np.stack([np.eye(4), np.eye(4)]), np.stack([np.eye(4), swing])]


def test_separate_legs_do_not_tear():
    legs, tris, groups, skins = two_legs()
    worst, area = posed_stretch(legs, tris, *rigid(groups), skins)
    assert worst.max() < STRETCH_RATIO and torn_pct(worst, area) == 0.0


def test_a_bridge_between_the_legs_tears():
    legs, tris, groups, skins = two_legs()
    bridged = np.vstack([tris, [(1, 4, 7), (1, 7, 2)]])  # the 46-unit gap, filled
    worst, area = posed_stretch(legs, bridged, *rigid(groups), skins)
    assert worst[5] > STRETCH_RATIO  # the triangle along the feet: 46 units at rest, ~1000 posed
    assert worst[:4].max() < STRETCH_RATIO  # the legs themselves only shear (1.6x)
    assert torn_pct(worst, area) > 25.0


def test_degenerate_triangles_count_as_unstretched():
    legs, tris, groups, skins = two_legs()
    flat = np.vstack([tris, [(1, 1, 4)]])  # an edge of length 0
    worst, area = posed_stretch(legs, flat, *rigid(groups), skins)
    assert np.isfinite(worst).all() and area[4] == 0.0
