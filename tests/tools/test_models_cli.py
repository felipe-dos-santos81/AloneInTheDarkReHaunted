import pathlib
import subprocess
import sys

import models  # tools/ is on sys.path (conftest.py)
from model_helpers import write_model_data_dir

ROOT = pathlib.Path(__file__).resolve().parents[2]


def run(tmp_path, *extra):
    data = write_model_data_dir(tmp_path / "INDARK")
    lines = []
    code = models.main(["export", "--data", str(data), "--out", str(tmp_path / "out"),
                        "--size", "32", "--ssaa", "1", *extra], root=tmp_path, log=lines.append)
    return code, lines


def test_export_reports_skipped_entries_with_exit_1(tmp_path):
    code, lines = run(tmp_path)
    assert code == 1
    assert (tmp_path / "out" / "manifest.json").is_file()
    assert lines[-1].startswith("exported 2 of 2 canonical bodies (5 animated bodies) to ")
    assert lines[-1].endswith(", 2 skipped")


def test_bodies_filter(tmp_path):
    code, _lines = run(tmp_path, "--bodies", "LISTBOD2_001")
    assert code == 1
    assert [p.name for p in (tmp_path / "out" / "bodies").iterdir()] == ["LISTBOD2_001"]


def test_unknown_body_is_a_usage_error(tmp_path):
    code, lines = run(tmp_path, "--bodies", "NOPE_001")
    assert code == 2 and "unknown body key(s): NOPE_001" in lines[-1]


def test_missing_data_is_a_usage_error(tmp_path):
    lines = []
    assert models.main(["export", "--data", str(tmp_path / "none")], root=tmp_path, log=lines.append) == 2
    assert lines[-1].startswith("error: no ITD_RESS.PAK")


def test_bad_sizes_are_usage_errors(tmp_path):
    code, lines = run(tmp_path, "--ssaa", "0")
    assert code == 2 and "--ssaa" in lines[-1]


def test_runs_as_a_script(tmp_path):
    proc = subprocess.run([sys.executable, str(ROOT / "tools" / "models.py"), "--help"],
                          capture_output=True, text=True)
    assert proc.returncode == 0 and "export" in proc.stdout
def export_for_import(tmp_path):
    data = write_model_data_dir(tmp_path / "INDARK")
    assert models.main(["export", "--data", str(data), "--out", str(tmp_path / "models"), "--size", "32",
                        "--ssaa", "1"], root=tmp_path, log=lambda _m: None) == 1
    return data


def test_identity_then_import(tmp_path):
    data = export_for_import(tmp_path)
    lines = []
    assert models.main(["identity", "--models", str(tmp_path / "models"), "--out", str(tmp_path / "ai"),
                        "--bodies", "LISTBODY_000"], root=tmp_path, log=lines.append) == 0
    assert (tmp_path / "ai/bodies/LISTBODY_000/model.glb").is_file()
    code = models.main(["import", "--data", str(data), "--models", str(tmp_path / "models"),
                        "--src", str(tmp_path / "ai"), "--dest", str(tmp_path / "dest"),
                        "--debug", str(tmp_path / "debug")], root=tmp_path, log=lines.append)
    assert code == 0
    assert lines[-1] == "imported 1 bodies (3 files), 0 failed"
    assert (tmp_path / "dest/body_LISTBODY_000.hdm").is_file()


def test_import_dry_run_and_failures(tmp_path):
    data = export_for_import(tmp_path)
    (tmp_path / "ai/bodies/LISTBOD2_000").mkdir(parents=True)
    lines = []
    code = models.main(["import", "--data", str(data), "--models", str(tmp_path / "models"),
                        "--src", str(tmp_path / "ai"), "--dest", str(tmp_path / "dest"), "--dry-run"],
                       root=tmp_path, log=lines.append)
    assert code == 1
    assert lines[-1] == "dry run: would import 0 bodies (0 files), 1 failed"
    assert not (tmp_path / "dest").exists()


def test_import_with_nothing_delivered_is_fine(tmp_path):
    lines = []
    assert models.main(["import"], root=tmp_path, log=lines.append) == 0
    assert lines[-1].startswith("nothing to import: ")


def test_import_without_an_export_is_a_usage_error(tmp_path):
    (tmp_path / "ai").mkdir()
    lines = []
    assert models.main(["import", "--src", str(tmp_path / "ai")], root=tmp_path, log=lines.append) == 2
    assert "run make export-models first" in lines[-1]


def test_identity_refuses_an_alias(tmp_path):
    export_for_import(tmp_path)
    lines = []
    assert models.main(["identity", "--models", str(tmp_path / "models"), "--bodies", "LISTBOD2_000"],
                       root=tmp_path, log=lines.append) == 2
    assert lines[-1] == "error: not exported canonical bodies: LISTBOD2_000"


def test_import_resolves_an_alias_to_its_canonical_key(tmp_path):
    data = export_for_import(tmp_path)
    models.main(["identity", "--models", str(tmp_path / "models"), "--out", str(tmp_path / "ai"),
                 "--bodies", "LISTBODY_000"], root=tmp_path, log=lambda _m: None)
    lines = []
    code = models.main(["import", "--data", str(data), "--models", str(tmp_path / "models"),
                        "--src", str(tmp_path / "ai"), "--dest", str(tmp_path / "dest"), "--dry-run",
                        "--bodies", "LISTBOD2_000"], root=tmp_path, log=lines.append)
    assert code == 0
    assert lines[0] == "LISTBOD2_000 is an alias of LISTBODY_000; importing LISTBODY_000"
    assert lines[-1] == "dry run: would import 1 bodies (3 files), 0 failed"


def test_identity_reports_one_unreadable_body_and_writes_the_rest(tmp_path):
    export_for_import(tmp_path)
    (tmp_path / "models/bodies/LISTBODY_000/original.glb").write_bytes(b"junk")
    lines = []
    code = models.main(["identity", "--models", str(tmp_path / "models"), "--out", str(tmp_path / "ai"),
                        "--bodies", "LISTBODY_000,LISTBOD2_001"], root=tmp_path, log=lines.append)
    assert code == 1
    assert (tmp_path / "ai/bodies/LISTBOD2_001/model.glb").is_file()
    assert any(line.startswith("error: LISTBODY_000: ") for line in lines)


def test_import_without_data_reads_the_export(tmp_path):
    export_for_import(tmp_path)  # no data/aitd1 under root: only the export can serve
    models.main(["identity", "--models", str(tmp_path / "models"), "--out", str(tmp_path / "ai"),
                 "--bodies", "LISTBODY_000"], root=tmp_path, log=lambda _m: None)
    lines = []
    code = models.main(["import", "--models", str(tmp_path / "models"), "--src", str(tmp_path / "ai"),
                        "--dest", str(tmp_path / "dest"), "--dry-run"], root=tmp_path, log=lines.append)
    assert code == 0 and lines[-1] == "dry run: would import 1 bodies (3 files), 0 failed"


def test_import_from_an_older_export_still_needs_the_game_data(tmp_path):
    export_for_import(tmp_path)
    (tmp_path / "models/palette.bin").unlink()
    models.main(["identity", "--models", str(tmp_path / "models"), "--out", str(tmp_path / "ai"),
                 "--bodies", "LISTBODY_000"], root=tmp_path, log=lambda _m: None)
    lines = []
    code = models.main(["import", "--models", str(tmp_path / "models"), "--src", str(tmp_path / "ai"),
                        "--dry-run"], root=tmp_path, log=lines.append)
    assert code == 2 and lines[-1].startswith("error: no ITD_RESS.PAK")


def test_import_dry_run_writes_the_report_asked_for(tmp_path):
    data = export_for_import(tmp_path)
    models.main(["identity", "--models", str(tmp_path / "models"), "--out", str(tmp_path / "ai"),
                 "--bodies", "LISTBODY_000"], root=tmp_path, log=lambda _m: None)
    code = models.main(["import", "--data", str(data), "--models", str(tmp_path / "models"),
                        "--src", str(tmp_path / "ai"), "--dry-run", "--report", str(tmp_path / "r")],
                       root=tmp_path, log=lambda _m: None)
    assert code == 0 and (tmp_path / "r/body_LISTBODY_000.json").is_file()
