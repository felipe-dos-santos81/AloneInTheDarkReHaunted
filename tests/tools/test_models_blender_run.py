import dataclasses
import io
import json
import pathlib

import numpy as np
import pytest
from PIL import Image

import models
from aitd_models.blender.run import blender_stage, characters, run_bodies, write_report
from aitd_models.delivery import read_delivery
from aitd_models.export import export_models
from aitd_models.manifest import read_manifest
from model_helpers import write_model_data_dir


@pytest.fixture
def export(tmp_path):
    data = write_model_data_dir(tmp_path / "INDARK")
    export_models(data, tmp_path / "models", lambda _m: None, size=128, ssaa=1, jobs=1)
    _doc, records = read_manifest(tmp_path / "models" / "manifest.json")
    by_key = {r.key: r for r in records}
    return tmp_path, by_key


def plant_outputs(work: pathlib.Path) -> None:
    """What stage.py leaves in the work folder: a quad and two 4x4 bakes."""
    np.savez(work / "refined.npz", positions=np.array([[0, 0, 0], [1, 0, 0], [1, 0, 2], [0, 0, 2]], np.float32),
             loop_vertex=np.array([0, 1, 2, 0, 2, 3]), loop_uv=np.zeros((6, 2), np.float32))
    np.save(work / "color.npy", np.full((4, 4, 3), 0.5, np.float16))
    np.save(work / "ao.npy", np.ones((4, 4), np.float16))


def fake_stage(calls):
    def stage(work: pathlib.Path) -> None:
        calls.append(json.loads((work / "job.json").read_text()))
        plant_outputs(work)
    return stage


def failing_stage(work):
    raise RuntimeError("Blender stage failed: boom")


def run(export, stage, keys=("LISTBODY_000", "LISTBOD2_001"), edits=None, atlas="body_LISTBODY_000.png", stale=()):
    """`stale` keys start with an earlier delivery and earlier stage outputs."""
    tmp, by_key = export
    atlases = tmp / "atlases"
    atlases.mkdir(exist_ok=True)
    (atlases / atlas).write_bytes(b"png")
    edits_dir = tmp / "edits"
    edits_dir.mkdir(exist_ok=True)
    for key, doc in (edits or {}).items():
        (edits_dir / f"{key}.json").write_text(json.dumps(doc))
    for key in stale:
        (tmp / "ai/bodies" / key).mkdir(parents=True)
        (tmp / "ai/bodies" / key / "model.glb").write_bytes(b"old")
        (tmp / "work" / key).mkdir(parents=True)
        plant_outputs(tmp / "work" / key)
    lines = []
    runs = run_bodies([by_key[k] for k in keys], tmp / "models", tmp / "ai", tmp / "work", stage, lines.append,
                      atlases, edits_dir)
    return tmp, runs, lines


def test_a_delivered_body_writes_its_model_and_drops_the_bakes(export):
    calls = []
    tmp, runs, _ = run(export, fake_stage(calls), stale=("LISTBODY_000",))
    assert [(r.key, r.status, r.texture) for r in runs] == [
        ("LISTBODY_000", "delivered", "atlas"), ("LISTBOD2_001", "delivered", "palette")]
    assert runs[0].triangles == 2
    delivery = read_delivery((tmp / "ai/bodies/LISTBODY_000/model.glb").read_bytes())
    assert len(delivery.triangles) == 2
    assert Image.open(io.BytesIO(delivery.texture)).mode == "RGB"  # no mask: no alpha
    assert not (tmp / "work/LISTBODY_000/color.npy").exists()
    assert (tmp / "work/LISTBODY_000/refined.npz").exists()
    assert calls[0]["atlases"]["body"].endswith("body_LISTBODY_000.png") and calls[0]["levels"] == [2, 2, 2, 2]  # 331 triangles (the sphere is an icosphere)


@pytest.mark.parametrize("first, detail", [
    (failing_stage, "boom"),
    (lambda work: None, "refined.npz"),  # a stage that wrote nothing must not finish the stale outputs
    (lambda work: (work / "refined.npz").write_bytes(b"junk"), ""),  # a truncated output
], ids=["error", "no-output", "corrupt"])
def test_a_failing_body_fails_alone_and_leaves_no_stale_delivery(export, first, detail):
    calls = []
    stages = iter([first, fake_stage(calls)])
    tmp, runs, lines = run(export, lambda work: next(stages)(work), stale=("LISTBODY_000",))
    assert [r.status for r in runs] == ["failed", "delivered"]
    assert detail in runs[0].detail and any(line.startswith("error: LISTBODY_000") for line in lines)
    assert not (tmp / "ai/bodies/LISTBODY_000/model.glb").exists()


def test_a_skipped_body_never_reaches_blender_and_a_bad_edit_fails_it(export):
    calls = []
    tmp, runs, _ = run(export, fake_stage(calls), stale=("LISTBODY_000",),
                       edits={"LISTBODY_000": {"skip": {"reason": "fused legs"}}, "LISTBOD2_001": {"smooth": {}}})
    assert [(r.status, r.detail) for r in runs] == [("skipped", "fused legs"),
                                                     ("failed", "LISTBOD2_001.json: unknown field smooth")]
    assert calls == []
    assert not (tmp / "ai/bodies/LISTBODY_000/model.glb").exists()
    assert not (tmp / "work/LISTBODY_000/corners.npz").exists() and not (tmp / "work/LISTBODY_000/job.json").exists()


def test_the_report_has_a_line_per_body(export):
    tmp, runs, _ = run(export, fake_stage([]))
    write_report(tmp / "run.md", runs)
    lines = (tmp / "run.md").read_text().splitlines()
    assert len(lines) == 2 + len(runs) and lines[2].startswith("| LISTBODY_000 | delivered |")


def test_characters_are_the_canonical_exported_ones(export):
    _tmp, by_key = export
    base = by_key["LISTBODY_000"]

    def record(key, kind, canonical=None):
        return dataclasses.replace(base, key=key, kind=kind, canonical=canonical or key)

    records = [record("A_000", "character"), record("A_001", "character", "A_000"),  # an alias
               record("B_000", "prop"), record("C_000", "skip"), record("D_000", "character")]
    assert [r.key for r in characters(records)] == ["A_000", "D_000"]


def test_the_command_refuses_a_missing_blender_and_a_non_character(export, tmp_path):
    tmp, _ = export
    lines = []
    args = ["blender", "--models", str(tmp / "models"), "--out", str(tmp / "ai")]
    assert models.main(args + ["--blender", str(tmp / "nope")], root=tmp, log=lines.append) == 2
    assert "Blender not found" in lines[-1]
    assert models.main(args + ["--bodies", "LISTBODY_000"], root=tmp, log=lines.append) == 2
    assert "not canonical character bodies: LISTBODY_000" in lines[-1]


def test_an_alias_atlas_is_reported_as_such(export):
    _tmp, runs, _ = run(export, fake_stage([]), keys=("LISTBODY_000",), atlas="ramp_LISTBOD2_000.png")
    assert runs[0].texture == "alias atlas"


@pytest.mark.parametrize("script, message", [
    ("exit 0", "Blender stage failed"),                 # a clean exit that wrote nothing
    ("echo 'Error: no bake' >&2; exit 3", "Error: no bake"),
])
def test_blender_stage_fails_on_no_output_or_an_error(tmp_path, script, message):
    blender = tmp_path / "blender"
    blender.write_text(f"#!/bin/sh\n{script}\n")
    blender.chmod(0o755)
    (tmp_path / "work").mkdir()
    with pytest.raises(RuntimeError, match=message):
        blender_stage(blender)(tmp_path / "work")
