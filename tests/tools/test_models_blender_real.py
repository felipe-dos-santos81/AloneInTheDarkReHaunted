"""Runs only with the AITD1 INDARK folder under data/aitd1 and Blender
installed (BLENDER, default the macOS app): Carnby, refined by the real
Blender stage and textured from his hand-made atlas, must pass the import
gate with the scores the plan measured."""
import os
import pathlib

import pytest

from aitd_models.blender.run import blender_stage, run_bodies
from aitd_models.export import export_models
from aitd_models.importer import ImportPaths, run_import
from aitd_models.manifest import read_manifest
from aitd_textures.decode import DataNotFound, find_data_dir

ROOT = pathlib.Path(__file__).resolve().parents[2]
BLENDER = pathlib.Path(os.environ.get("BLENDER", "/Applications/Blender.app/Contents/MacOS/Blender"))
KEY = "LISTBODY_011"


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
    return runs[0], result


def test_carnby_passes_the_gate(gate):
    run, result = gate
    assert (run.status, run.texture) == ("delivered", "atlas"), run.detail
    assert run.triangles <= 30000
    assert result.imported == [KEY], result.failed
    metrics = result.metrics[KEY]
    assert metrics["chamfer_p95_pct"] < 2.0
    assert min(metrics["iou"].values()) > 0.95
    assert metrics["ambiguous_pct"] == 0.0
