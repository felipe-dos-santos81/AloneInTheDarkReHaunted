import numpy as np
import pytest

from aitd_models.delivery import read_delivery
from aitd_models.export import export_models
from aitd_models.gltf import read_glb
from aitd_models.hdm import read_hdm
from aitd_models.identity import identity_glb
from aitd_models.importer import ImportPaths, run_import
from aitd_models.manifest import read_manifest
from model_helpers import TINY_PNG, chain_rest_mesh, delivery_glb, write_model_data_dir


@pytest.fixture
def setup(tmp_path):
    """Export the synthetic INDARK; deliver the identity of LISTBODY_000."""
    data = write_model_data_dir(tmp_path / "INDARK")
    export_models(data, tmp_path / "models", lambda _m: None, size=128, ssaa=1, jobs=1)
    _doc, records = read_manifest(tmp_path / "models" / "manifest.json")
    src = tmp_path / "ai"
    deliver(src, "LISTBODY_000", identity_glb((tmp_path / "models/bodies/LISTBODY_000/original.glb").read_bytes()))
    return data, tmp_path / "models", records, src, tmp_path / "dest", tmp_path / "debug"


def deliver(src, key, data):
    (src / "bodies" / key).mkdir(parents=True, exist_ok=True)
    (src / "bodies" / key / "model.glb").write_bytes(data)


def run(setup, **kw):
    data, models, records, src, dest, debug = setup
    lines = []
    result = run_import(ImportPaths(data, models, src, dest, debug), records, log=lines.append, **kw)
    return result, lines


def test_identity_import_writes_the_body_and_its_aliases(setup):
    result, lines = run(setup)
    dest, debug = setup[4], setup[5]
    assert result.imported == ["LISTBODY_000"] and result.failed == {}
    assert sorted(p.name for p in dest.iterdir()) == \
        ["body_LISTBOD2_000.hdm", "body_LISTBODY_000.hdm", "body_LISTBODY_002.hdm"]
    files = {p.read_bytes() for p in dest.iterdir()}
    assert len(files) == 1
    mesh = read_hdm(files.pop())
    body, _rest, rest_mesh = chain_rest_mesh()
    record = next(r for r in setup[2] if r.key == "LISTBODY_000")
    assert mesh.group_count == 4 and mesh.skeleton_hash == int(record.skeleton_hash, 16)
    assert np.allclose(mesh.vertices["position"], rest_mesh.positions, atol=0.5)
    assert (mesh.vertices["joints"][:, 0] == rest_mesh.groups).mean() >= 0.98
    assert read_glb((debug / "body_LISTBODY_000.glb").read_bytes()).doc["meshes"]
    assert lines[0].startswith("imported LISTBODY_000 (+2 aliases): ")


def test_dry_run_writes_nothing(setup):
    result, lines = run(setup, dry_run=True)
    assert result.imported == ["LISTBODY_000"] and len(result.written) == 3
    assert not setup[4].exists() and not setup[5].exists()
    assert lines[0].startswith("checked LISTBODY_000")


def test_only_limits_the_keys(setup):
    deliver(setup[3], "LISTBOD2_001", b"junk")
    result, _ = run(setup, only={"LISTBODY_000"})
    assert result.imported == ["LISTBODY_000"] and result.failed == {}


def test_a_requested_key_without_a_delivery_fails(setup):
    result, lines = run(setup, only={"LISTBODY_000", "LISTBOD2_001"})
    assert result.imported == ["LISTBODY_000"]
    assert result.failed == {"LISTBOD2_001": "no delivery folder"}
    assert "error: LISTBOD2_001: no delivery folder" in lines


@pytest.mark.parametrize("key, data, reason", [
    ("LISTBOD2_000", b"", "an alias: deliver it as LISTBODY_000"),
    ("NOPE_001", b"", "not an animated body in the export manifest"),
    ("LISTBOD2_001", None, "no model.glb"),
    ("LISTBOD2_001", b"junk", "too small for a GLB"),
])
def test_bad_deliveries_fail_with_a_reason(setup, key, data, reason):
    src = setup[3]
    if data is None:
        (src / "bodies" / key).mkdir(parents=True)
    else:
        deliver(src, key, data)
    result, lines = run(setup)
    assert reason in result.failed[key]
    assert f"error: {key}: " in "\n".join(lines)
    assert result.imported == ["LISTBODY_000"]


def test_a_mesh_of_the_wrong_shape_fails_the_fit(setup):
    src = setup[3]
    cube = np.array([(x, y, z) for x in (0, 1) for y in (0, 1) for z in (0, 1)], float) * 0.1
    tris = [(0, 1, 3), (0, 3, 2), (4, 6, 7), (4, 7, 5), (0, 4, 5), (0, 5, 1),
            (2, 3, 7), (2, 7, 6), (0, 2, 6), (0, 6, 4), (1, 5, 7), (1, 7, 3)]
    deliver(src, "LISTBOD2_001", delivery_glb(cube, tris, np.zeros((8, 2)), TINY_PNG))
    result, _ = run(setup)
    assert "silhouette IoU" in result.failed["LISTBOD2_001"] or "chamfer" in result.failed["LISTBOD2_001"]
    assert not (setup[4] / "body_LISTBOD2_001.hdm").exists()


def test_stale_export_is_refused(setup):
    data, models, records, src, dest, debug = setup
    for r in records:
        if r.key == "LISTBODY_000":
            r.skeleton_hash = "0" * 16
    result, _ = run((data, models, records, src, dest, debug))
    assert "does not match the export" in result.failed["LISTBODY_000"]


def test_normals_follow_the_alignment(setup):
    data, models, records, src, dest, debug = setup
    run(setup)
    mesh = read_hdm((dest / "body_LISTBODY_000.hdm").read_bytes())
    delivered = read_delivery((src / "bodies/LISTBODY_000/model.glb").read_bytes())
    cos = (mesh.vertices["normal"] * delivered.normals).sum(axis=1)
    assert cos.min() > 0.99


def test_a_missing_reference_view_fails_that_body_only(setup):
    (setup[1] / "bodies/LISTBODY_000/reference/side.png").unlink()
    result, _ = run(setup)
    assert result.failed["LISTBODY_000"].startswith("import failed: ")


def test_reimport_replaces_the_file_and_leaves_no_temporary(setup):
    run(setup)
    dest = setup[4]
    first = (dest / "body_LISTBODY_000.hdm").read_bytes()
    _body, _rest, mesh = chain_rest_mesh()
    from aitd_models.original import to_gltf_points
    soup = np.arange(len(mesh.positions)).reshape(-1, 3)
    deliver(setup[3], "LISTBODY_000",
            delivery_glb(to_gltf_points(mesh.positions), soup, np.full((len(soup) * 3, 2), 0.5)))
    result, _ = run(setup)
    assert result.imported == ["LISTBODY_000"]
    assert (dest / "body_LISTBODY_000.hdm").read_bytes() != first
    assert not list(dest.glob("*.tmp"))


def test_a_failing_body_writes_none_of_its_aliases(setup):
    data, models, records, src, dest, debug = setup
    for r in records:
        if r.key == "LISTBODY_000":
            r.skeleton_hash = "0" * 16
    run((data, models, records, src, dest, debug))
    assert not dest.exists()


def test_every_key_gets_a_report(setup):
    import json
    deliver(setup[3], "LISTBOD2_001", b"junk")
    run(setup)
    debug = setup[5]
    ok = json.loads((debug / "body_LISTBODY_000.json").read_text())
    assert ok["status"] == "imported" and ok["reason"] is None
    assert {"chamfer_mean_pct", "chamfer_p95_pct", "iou", "inside_zv", "align", "ambiguous_pct"} <= set(ok["metrics"])
    assert sorted(ok["files"]) == ["body_LISTBOD2_000.hdm", "body_LISTBODY_000.hdm", "body_LISTBODY_002.hdm"]
    bad = json.loads((debug / "body_LISTBOD2_001.json").read_text())
    assert bad["status"] == "failed" and "too small for a GLB" in bad["reason"] and bad["files"] == []


def test_a_dry_run_writes_no_report(setup):
    run(setup, dry_run=True)
    assert not setup[5].exists()


def test_the_export_alone_imports_the_same_file(setup):
    data, models, records, src, dest, debug = setup
    run(setup)
    from_data = (dest / "body_LISTBODY_000.hdm").read_bytes()
    result, _ = run((None, models, records, src, tmp := dest.parent / "dest2", debug))
    assert result.imported == ["LISTBODY_000"]
    assert (tmp / "body_LISTBODY_000.hdm").read_bytes() == from_data


def test_a_body_bin_that_differs_from_the_manifest_fails_that_body(setup):
    data, models, records, src, dest, debug = setup
    body = models / "bodies/LISTBODY_000/body.bin"
    body.write_bytes(body.read_bytes() + b"\0")
    result, _ = run((None, models, records, src, dest, debug))
    assert result.failed["LISTBODY_000"] == "body.bin does not match the manifest (run make export-models again)"


def test_an_export_without_body_bin_fails_that_body(setup):
    data, models, records, src, dest, debug = setup
    (models / "bodies/LISTBODY_000/body.bin").unlink()
    result, _ = run((None, models, records, src, dest, debug))
    assert result.failed["LISTBODY_000"] == "no body.bin in the export (run make export-models again)"


def test_a_dry_run_writes_reports_where_asked_and_nothing_else(setup):
    import json
    data, models, records, src, dest, debug = setup
    deliver(src, "LISTBOD2_001", b"junk")
    report = debug.parent / "report"
    run_import(ImportPaths(data, models, src, dest, debug, report), records, dry_run=True, log=lambda _m: None)
    assert sorted(p.name for p in report.iterdir()) == ["body_LISTBOD2_001.json", "body_LISTBODY_000.json"]
    assert json.loads((report / "body_LISTBODY_000.json").read_text())["status"] == "imported"
    assert not dest.exists() and not debug.exists()


def test_a_real_import_writes_reports_to_the_report_folder_when_given(setup):
    data, models, records, src, dest, debug = setup
    report = debug.parent / "report"
    run_import(ImportPaths(data, models, src, dest, debug, report), records, log=lambda _m: None)
    assert [p.name for p in report.iterdir()] == ["body_LISTBODY_000.json"]
    assert [p.name for p in debug.iterdir()] == ["body_LISTBODY_000.glb"]
