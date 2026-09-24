"""Runs only when the AITD1 INDARK folder is present under data/aitd1:
exports two real bodies and checks what the generator will receive."""
import pathlib
from collections import Counter

import numpy as np
import pytest

from aitd_models.body import PRIM_POLY, parse_body
from aitd_models.export import export_models
from aitd_models.gltf import read_glb
from aitd_models.mesh import build_mesh
from aitd_models.original import to_gltf_points
from aitd_models.pose import pose_float, skin
from aitd_textures.decode import DataNotFound, decode_palette, find_data_dir
from aitd_textures.pak import Pak
from model_helpers import gltf_skinned_positions

ROOT = pathlib.Path(__file__).resolve().parents[2]


@pytest.fixture(scope="module")
def real(tmp_path_factory):
    try:
        data_dir = find_data_dir(ROOT / "data" / "aitd1")
    except DataNotFound:
        pytest.skip("no AITD1 game data under data/aitd1")
    out = tmp_path_factory.mktemp("models")
    result = export_models(data_dir, out, lambda _m: None, only={"LISTBODY_011", "LISTBOD2_011"}, size=128, ssaa=1)
    return data_dir, out, result


def test_manifest_counts(real):
    _data, _out, result = real
    assert result.skipped == []
    assert len(result.records) == 214
    canonical = [r for r in result.records if r.is_canonical]
    assert len(canonical) == 76
    assert sum(1 for r in canonical if r.hqr == "LISTBOD2") == 11
    assert Counter(r.kind for r in canonical) == {"character": 42, "prop": 33, "skip": 1}
    assert sorted(result.written) == ["LISTBOD2_011", "LISTBODY_011"]


def test_the_hero_glb_moves_like_the_engine(real):
    data_dir, out, result = real
    record = next(r for r in result.records if r.key == "LISTBODY_011")
    glb = read_glb((out / record.dir / "original.glb").read_bytes())
    body = parse_body(Pak(data_dir / "LISTBODY.PAK").read(11))
    palette = decode_palette(Pak(data_dir / "ITD_RESS.PAK").read(3))
    from aitd_models.body import parse_anim
    anim = parse_anim(Pak(data_dir / "LISTANIM.PAK").read(int(record.preview_anims[0].split("_")[1])))
    rest_mesh = build_mesh(body, skin(body, pose_float(body, [(0, (0, 0, 0))] * len(body.groups)).group_matrices()), palette)
    polys = np.repeat([body.primitives[p].type == PRIM_POLY for p in rest_mesh.prim_index], 3)
    for key, frame in enumerate(anim.frames):
        want = to_gltf_points(build_mesh(body, skin(body, pose_float(body, list(frame.states)).group_matrices()), palette).positions)
        assert np.abs(gltf_skinned_positions(glb, 0, key) - want)[polys].max() < 1e-3  # metres
    assert 1.6 < record.height / 1000 < 2.0  # a person, in metres
