"""Runs only when the AITD1 INDARK folder is present under data/aitd1: the
identity round trip on the slice's bodies (Carnby and Emily with the lamp,
and the window creature). Each original, delivered as its own model, must
import back onto itself and move with the game's animations."""
import pathlib

import numpy as np
import pytest

from aitd_models.body import BodyError, parse_anim, parse_body
from aitd_models.export import export_models
from aitd_models.hdm import read_hdm
from aitd_models.identity import identity_glb
from aitd_models.importer import ImportPaths, run_import
from aitd_models.manifest import read_manifest
from aitd_models.original import rest_mesh
from aitd_models.pose import pose_float
from aitd_textures.decode import DataNotFound, decode_palette, find_data_dir
from aitd_textures.pak import Pak

ROOT = pathlib.Path(__file__).resolve().parents[2]
KEYS = ("LISTBODY_011", "LISTBOD2_011", "LISTBODY_024")
ANIMS = {"LISTBODY": "LISTANIM", "LISTBOD2": "LISTANI2"}


@pytest.fixture(scope="module")
def imported(tmp_path_factory):
    try:
        data_dir = find_data_dir(ROOT / "data" / "aitd1")
    except DataNotFound:
        pytest.skip("no AITD1 game data under data/aitd1")
    tmp = tmp_path_factory.mktemp("real_import")
    export_models(data_dir, tmp / "models", lambda _m: None, only=set(KEYS), size=256, ssaa=1)
    _doc, records = read_manifest(tmp / "models" / "manifest.json")
    for key in KEYS:
        out = tmp / "ai" / "bodies" / key
        out.mkdir(parents=True)
        (out / "model.glb").write_bytes(identity_glb((tmp / "models" / "bodies" / key / "original.glb").read_bytes()))
    result = run_import(ImportPaths(data_dir, tmp / "models", tmp / "ai", tmp / "dest", None), records,
                        log=lambda _m: None)
    return data_dir, tmp / "dest", result


@pytest.mark.parametrize("key", KEYS)
def test_identity_round_trip(imported, key):
    data_dir, dest, result = imported
    assert key in result.imported, result.failed.get(key)
    assert min(result.metrics[key]["iou"].values()) >= 0.97
    assert result.metrics[key]["stretch_torn_pct"] == 0.0
    hqr, index = key.split("_")
    body = parse_body(Pak(data_dir / f"{hqr}.PAK").read(int(index)))
    rest, mesh = rest_mesh(body, decode_palette(Pak(data_dir / "ITD_RESS.PAK").read(3)))
    hdm = read_hdm((dest / f"body_{key}.hdm").read_bytes())
    pos = hdm.vertices["position"].astype(float)
    height = np.ptp(mesh.positions[:, 1])
    assert np.linalg.norm(pos - mesh.positions, axis=1).max() < 5e-3 * height
    assert (hdm.vertices["joints"][:, 0] == mesh.groups).mean() >= 0.99

    anims = Pak(data_dir / f"{ANIMS[hqr]}.PAK")
    frames = []
    for i in range(anims.count):
        try:
            anim = parse_anim(anims.read(i))
        except BodyError:
            continue
        if anim.num_groups == len(body.groups):
            frames += [list(f.states[:len(body.groups)]) for f in anim.frames]
    inverse_rest = np.linalg.inv(rest.group_matrices())
    weights = hdm.vertices["weights"] / 255.0
    hom = np.c_[pos, np.ones(len(pos))]
    moved = []
    for k in np.random.default_rng(0).choice(len(frames), size=min(50, len(frames)), replace=False):
        skin = pose_float(body, frames[k]).group_matrices() @ inverse_rest
        blended = sum(weights[:, j:j + 1] * np.einsum("vij,vj->vi", skin[hdm.vertices["joints"][:, j]], hom)[:, :3]
                      for j in range(4))
        rigid = np.einsum("vij,vj->vi", skin[mesh.groups], hom)[:, :3]
        moved.append(np.linalg.norm(blended - rigid, axis=1))
    assert np.percentile(np.array(moved), 99) < 1e-2 * height


def test_the_export_alone_imports_the_same_files(imported):
    _data_dir, dest, _result = imported
    tmp = dest.parent
    _doc, records = read_manifest(tmp / "models" / "manifest.json")
    result = run_import(ImportPaths(None, tmp / "models", tmp / "ai", tmp / "dest-export", None), records,
                        log=lambda _m: None)
    assert sorted(result.imported) == sorted(KEYS)
    for key in KEYS:
        assert (tmp / "dest-export" / f"body_{key}.hdm").read_bytes() == (dest / f"body_{key}.hdm").read_bytes()
