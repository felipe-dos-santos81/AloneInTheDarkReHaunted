import pathlib
import subprocess
import sys

from model_helpers import write_model_data_dir

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import models  # noqa: E402


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
