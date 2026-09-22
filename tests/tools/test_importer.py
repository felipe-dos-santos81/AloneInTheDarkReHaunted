import numpy as np
import pytest
from PIL import Image

from aitd_textures import importer
from aitd_textures.export import export_all
from aitd_textures.importer import derive_dark, run_import, validate_file
from aitd_textures.manifest import MANIFEST_NAME, read_manifest


def _quiet(*_args, **_kwargs):
    pass


def make_png(path, size=(1280, 800), color=(200, 100, 50), mode="RGB"):
    path.parent.mkdir(parents=True, exist_ok=True)
    Image.new(mode, size, color).save(path)
    return path


@pytest.fixture
def tree(tmp_path):
    src = tmp_path / "textures-ai"
    dest = tmp_path / "backgrounds_hd"
    dest.mkdir()
    return src, dest


def test_valid_png_is_copied_verbatim(tree):
    src, dest = tree
    p = make_png(src / "backgrounds" / "CAMERA00_000.png")
    result = run_import(src, dest, log=_quiet)
    assert result.errors == [] and result.warnings == []
    assert result.imported == [dest / "CAMERA00_000.png"]
    assert (dest / "CAMERA00_000.png").read_bytes() == p.read_bytes()
    assert result.dark == []
    assert list(dest.glob("*.tmp")) == []


def test_unknown_names_are_errors_and_not_written(tree):
    src, dest = tree
    make_png(src / "backgrounds" / "foo.png")
    make_png(src / "backgrounds" / "CAMERA00_000_DARK.png")
    result = run_import(src, dest, log=_quiet)
    assert sorted(f.kind for f in result.errors) == ["name", "name"]
    assert list(dest.iterdir()) == []


def test_corrupt_png_is_invalid(tree):
    src, dest = tree
    p = make_png(src / "backgrounds" / "CAMERA00_000.png")
    data = p.read_bytes()
    p.write_bytes(data[: len(data) // 2])
    result = run_import(src, dest, log=_quiet)
    assert [f.kind for f in result.errors] == ["invalid"]
    assert result.imported == []


def test_wrong_aspect_is_error(tree):
    src, dest = tree
    make_png(src / "backgrounds" / "CAMERA00_000.png", size=(1280, 720))
    result = run_import(src, dest, log=_quiet)
    assert [f.kind for f in result.errors] == ["aspect"]


def test_oversized_is_error(tree, monkeypatch):
    monkeypatch.setattr(importer, "MAX_SIDE", 1000)
    src, dest = tree
    make_png(src / "backgrounds" / "CAMERA00_000.png", size=(1280, 800))
    result = run_import(src, dest, log=_quiet)
    assert [f.kind for f in result.errors] == ["too_large"]


def test_non_multiple_size_is_a_warning_but_imported(tree):
    src, dest = tree
    make_png(src / "backgrounds" / "CAMERA00_000.png", size=(1000, 625))
    result = run_import(src, dest, log=_quiet)
    assert result.errors == []
    assert [f.kind for f in result.warnings] == ["size"]
    assert (dest / "CAMERA00_000.png").is_file()


def test_unchanged_original_is_skipped(synthetic_data_dir, tmp_path):
    originals = tmp_path / "textures"
    export_all(synthetic_data_dir, originals, log=_quiet)
    manifest = read_manifest(originals / MANIFEST_NAME)
    src = tmp_path / "textures-ai"
    (src / "backgrounds").mkdir(parents=True)
    (src / "backgrounds" / "CAMERA00_000.png").write_bytes(
        (originals / "backgrounds" / "CAMERA00_000.png").read_bytes())
    dest = tmp_path / "dest"
    result = run_import(src, dest, manifest, log=_quiet)
    assert result.errors == []
    assert [f.kind for f in result.warnings] == ["unchanged"]
    assert result.skipped == [src / "backgrounds" / "CAMERA00_000.png"]
    assert result.imported == [] and not (dest / "CAMERA00_000.png").exists()


def test_palette_mode_is_reencoded_as_rgb(tree):
    src, dest = tree
    p = make_png(src / "screens" / "ITD_RESS_013.png", mode="P", color=3)
    result = run_import(src, dest, log=_quiet)
    assert result.errors == []
    with Image.open(dest / "ITD_RESS_013.png") as im:
        assert im.mode == "RGB" and im.size == (1280, 800)
    assert (dest / "ITD_RESS_013.png").read_bytes() != p.read_bytes()


def test_rgba_is_copied_verbatim(tree):
    src, dest = tree
    p = make_png(src / "backgrounds" / "CAMERA01_002.png", mode="RGBA", color=(1, 2, 3, 128))
    run_import(src, dest, log=_quiet)
    assert (dest / "CAMERA01_002.png").read_bytes() == p.read_bytes()


def test_dark_mirror_derives_only_where_a_dark_file_exists(tree):
    src, dest = tree
    make_png(src / "backgrounds" / "CAMERA02_007.png", color=(200, 100, 50))
    make_png(src / "backgrounds" / "CAMERA02_008.png")
    (dest / "CAMERA02_007_DARK.png").write_bytes(b"stale")
    result = run_import(src, dest, dark="mirror", log=_quiet)
    assert result.dark == [dest / "CAMERA02_007_DARK.png"]
    with Image.open(dest / "CAMERA02_007_DARK.png") as im:
        assert im.mode == "RGB" and im.size == (1280, 800)
        assert tuple(np.asarray(im)[0, 0]) == (20, 10, 5)
    assert not (dest / "CAMERA02_008_DARK.png").exists()


def test_dark_all_and_none(tree):
    src, dest = tree
    make_png(src / "backgrounds" / "CAMERA02_007.png")
    make_png(src / "screens" / "ITD_RESS_013.png")
    result = run_import(src, dest, dark="all", dark_factor=0.5, log=_quiet)
    assert result.dark == [dest / "CAMERA02_007_DARK.png"]  # never for screens
    with Image.open(dest / "CAMERA02_007_DARK.png") as im:
        assert tuple(np.asarray(im)[0, 0]) == (100, 50, 25)
    (dest / "CAMERA02_007_DARK.png").unlink()
    result = run_import(src, dest, dark="none", log=_quiet)
    assert result.dark == [] and not (dest / "CAMERA02_007_DARK.png").exists()


def test_invalid_dark_policy_is_rejected(tree):
    src, dest = tree
    with pytest.raises(ValueError, match="dark"):
        run_import(src, dest, dark="dim", log=_quiet)


def test_derive_dark_rounds_and_clips():
    px = np.array([[[201, 100, 255]]], dtype=np.uint8)
    assert derive_dark(px, 0.1).tolist() == [[[20, 10, 26]]]
    assert derive_dark(px, 2.0).tolist() == [[[255, 200, 255]]]
    assert derive_dark(px, 0.1).dtype == np.uint8


def test_dry_run_checks_everything_but_writes_nothing(tree):
    src, dest = tree
    make_png(src / "backgrounds" / "CAMERA00_000.png")
    make_png(src / "backgrounds" / "CAMERA00_001.png", size=(100, 100))
    (dest / "CAMERA00_000_DARK.png").write_bytes(b"stale")
    result = run_import(src, dest, dark="mirror", dry_run=True, log=_quiet)
    assert result.imported == [dest / "CAMERA00_000.png"]
    assert result.dark == [dest / "CAMERA00_000_DARK.png"]
    assert [f.kind for f in result.errors] == ["aspect"]
    assert not (dest / "CAMERA00_000.png").exists()
    assert (dest / "CAMERA00_000_DARK.png").read_bytes() == b"stale"


def test_coverage_against_manifest(synthetic_data_dir, tmp_path):
    originals = tmp_path / "textures"
    export_all(synthetic_data_dir, originals, log=_quiet)
    manifest = read_manifest(originals / MANIFEST_NAME)
    src = tmp_path / "textures-ai"
    make_png(src / "backgrounds" / "CAMERA00_000.png")
    make_png(src / "backgrounds" / "CAMERA03_005.png")  # valid name, not in manifest
    result = run_import(src, tmp_path / "dest", manifest, log=_quiet)
    assert result.errors == []
    assert sorted(p.name for p in result.imported) == ["CAMERA00_000.png", "CAMERA03_005.png"]
    assert len(result.not_replaced) == 21
    assert "backgrounds/CAMERA00_000.png" not in result.not_replaced
    assert "screens/ITD_RESS_013.png" in result.not_replaced


def test_validate_file_reports_each_rule_once(tmp_path):
    good = make_png(tmp_path / "CAMERA00_000.png")
    cand, findings = validate_file(good)
    assert findings == [] and cand is not None
    assert (cand.target, cand.kind, cand.copy_verbatim) == ("CAMERA00_000.png", "camera", True)
    assert cand.pixels.shape == (800, 1280, 3)
    cand, findings = validate_file(tmp_path / "missing.png")
    assert cand is None and findings[0].kind == "name"


def test_m_aitd_layout_is_imported_under_engine_names(tree):
    src, dest = tree
    make_png(src / "backgrounds" / "floor02" / "camera007.png")
    make_png(src / "screens" / "ress13.png")
    make_png(src / "alt_backgrounds" / "floor06" / "camera000.png")
    result = run_import(src, dest, log=_quiet)
    assert result.errors == []
    assert sorted(p.name for p in result.imported) == [
        "CAMERA02_007.png", "ITD_RESS_013.png", "ITD_RESS_017.png"]
    assert (dest / "CAMERA02_007.png").is_file()


def test_unknown_nested_names_are_still_errors(tree):
    src, dest = tree
    make_png(src / "backgrounds" / "floor09" / "camera000.png")
    result = run_import(src, dest, log=_quiet)
    assert [f.kind for f in result.errors] == ["name"]
    assert list(dest.iterdir()) == []


def test_folders_beside_the_source_folders_are_ignored(tree):
    src, dest = tree
    make_png(src / "guides" / "floor00" / "camera000.png")
    make_png(src / ".quality" / "screens" / "ress13.png" / "attempt-1.png")
    make_png(src / "palette.png", size=(256, 1))
    result = run_import(src, dest, log=_quiet)
    assert result.findings == [] and result.imported == []


def test_unchanged_check_matches_m_aitd_names_against_the_manifest(synthetic_data_dir, tmp_path):
    originals = tmp_path / "textures"
    export_all(synthetic_data_dir, originals, log=_quiet)
    manifest = read_manifest(originals / MANIFEST_NAME)
    src = tmp_path / "textures-ai"
    (src / "backgrounds" / "floor00").mkdir(parents=True)
    (src / "backgrounds" / "floor00" / "camera000.png").write_bytes(
        (originals / "backgrounds" / "CAMERA00_000.png").read_bytes())
    result = run_import(src, tmp_path / "dest", manifest, log=_quiet)
    assert result.errors == []
    assert [f.kind for f in result.warnings] == ["unchanged"]
    assert result.imported == []
    assert "backgrounds/CAMERA00_000.png" not in result.not_replaced
