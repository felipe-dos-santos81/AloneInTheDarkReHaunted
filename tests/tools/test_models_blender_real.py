"""Runs only with the AITD1 INDARK folder under data/aitd1 and Blender
installed (BLENDER, default the macOS app): Carnby, refined by the real
Blender stage and textured from his hand-made atlas, must pass the import
gate with the scores the plan measured."""
import io
import os
import pathlib

import numpy as np
import pytest
from PIL import Image

from aitd_models.blender.run import blender_stage, run_bodies
from aitd_models.export import export_models
from aitd_models.gltf import read_glb
from aitd_models.importer import ImportPaths, run_import
from aitd_models.manifest import read_manifest
from aitd_textures.decode import DataNotFound, find_data_dir

ROOT = pathlib.Path(__file__).resolve().parents[2]
BLENDER = pathlib.Path(os.environ.get("BLENDER", "/Applications/Blender.app/Contents/MacOS/Blender"))
KEY = "LISTBODY_011"
DISTINCT = 1000  # colours the baked texture must hold
OUTWARD = 0.6  # share of the surface, by area, facing away from the vertical axis
GREEN = 1.1      # its mean green over the mean of red and blue: Carnby's suit is green


@pytest.fixture(scope="module")
def gate(tmp_path_factory):
    try:
        data_dir = find_data_dir(ROOT / "data" / "aitd1")
    except DataNotFound:
        pytest.skip("no AITD1 game data under data/aitd1")
    if not BLENDER.is_file():
        pytest.skip(f"no Blender at {BLENDER}")
    tmp = tmp_path_factory.mktemp("blender_real")
    export_models(data_dir, tmp / "models", lambda _m: None, only={KEY}, size=256, ssaa=1)
    _doc, records = read_manifest(tmp / "models" / "manifest.json")
    record = next(r for r in records if r.key == KEY)
    runs = run_bodies([record], tmp / "models", tmp / "ai", tmp / "work", blender_stage(BLENDER), lambda _m: None)
    result = run_import(ImportPaths(None, tmp / "models", tmp / "ai", tmp / "dest", None), records, {KEY},
                        dry_run=True, log=lambda _m: None)
    return runs[0], result, tmp / "ai" / "bodies" / KEY / "model.glb"


def test_carnby_passes_the_gate(gate):
    run, result, delivery = gate
    assert (run.status, run.texture) == ("delivered", "atlas"), run.detail
    assert run.triangles <= 30000
    assert result.imported == [KEY], result.failed
    metrics = result.metrics[KEY]
    assert metrics["chamfer_p95_pct"] < 2.0
    assert min(metrics["iou"].values()) > 0.95
    assert metrics["ambiguous_pct"] == 0.0
    # the bake itself: a black, flat or unpainted texture would pass every check above
    glb = read_glb(delivery.read_bytes())
    view = glb.doc["bufferViews"][glb.doc["images"][0]["bufferView"]]
    pixels = np.asarray(Image.open(io.BytesIO(glb.bin[view["byteOffset"]:view["byteOffset"] + view["byteLength"]])).convert("RGB"))
    assert len(np.unique(pixels.reshape(-1, 3), axis=0)) > DISTINCT
    mean = pixels.reshape(-1, 3).mean(axis=0)
    assert mean[1] > GREEN * (mean[0] + mean[2]) / 2
    # faces point out of the body (the originals mostly face in; measured 21 % unfixed, 79 % fixed), by area,
    # away from the vertical axis: the engine lights the front only, and the AO bake reads a back face as occluded
    prim = glb.doc["meshes"][0]["primitives"][0]
    p = glb.accessor(prim["attributes"]["POSITION"]).astype(float)
    tri = glb.accessor(prim["indices"]).astype(np.int64).reshape(-1, 3)
    face = np.cross(p[tri[:, 1]] - p[tri[:, 0]], p[tri[:, 2]] - p[tri[:, 0]])
    radial = p[tri].mean(axis=1) - p.mean(axis=0)
    radial[:, 1] = 0
    area = np.linalg.norm(face, axis=1)
    assert area[(radial * face).sum(axis=1) > 0].sum() > OUTWARD * area.sum()
