import os
import pathlib

import pytest
from PIL import Image

import helpers
import textures
from aitd_textures.animations import make_job
from aitd_textures.manifest import read_manifest, write_manifest


def _upscale(src_png, dst_png, factor=2):
    dst_png.parent.mkdir(parents=True, exist_ok=True)
    with Image.open(src_png) as im:
        im.resize((im.width * factor, im.height * factor), Image.NEAREST).save(dst_png)


@pytest.fixture
def logs():
    return []


def test_export_then_import_round_trip(synthetic_data_dir, tmp_path, logs):
    originals = tmp_path / "textures"
    assert textures.main(["export", "--data", str(synthetic_data_dir), "--out", str(originals)],
                         root=tmp_path, log=logs.append) == 0
    assert (originals / "manifest.json").is_file()
    assert any("exported 9 cameras and 13 screens" in line for line in logs)

    ai = tmp_path / "textures-ai"
    _upscale(originals / "backgrounds" / "CAMERA00_000.png", ai / "backgrounds" / "CAMERA00_000.png")
    _upscale(originals / "screens" / "ITD_RESS_013.png", ai / "screens" / "ITD_RESS_013.png")
    dest = tmp_path / "dest"
    rc = textures.main(["import", "--src", str(ai), "--dest", str(dest), "--originals", str(originals)],
                       root=tmp_path, log=logs.append)
    assert rc == 0
    assert (dest / "CAMERA00_000.png").is_file() and (dest / "ITD_RESS_013.png").is_file()
    assert any("imported" in line and "2" in line for line in logs)
    assert any("not replaced" in line and "20" in line for line in logs)


def test_export_without_game_data_is_a_usage_error(tmp_path, logs):
    rc = textures.main(["export", "--data", str(tmp_path / "nope"), "--out", str(tmp_path / "o")],
                       root=tmp_path, log=logs.append)
    assert rc == 2
    assert any("ITD_RESS.PAK" in line for line in logs)


def test_export_unreadable_data_dir_exits_2_not_a_traceback(tmp_path, logs):
    if hasattr(os, "geteuid") and os.geteuid() == 0:
        pytest.skip("chmod has no effect when running as root")
    data_root = tmp_path / "data"
    locked = data_root / "locked"
    locked.mkdir(parents=True)
    original_mode = locked.stat().st_mode
    locked.chmod(0o000)
    try:
        rc = textures.main(["export", "--data", str(locked), "--out", str(tmp_path / "out")],
                           root=tmp_path, log=logs.append)
    finally:
        locked.chmod(original_mode)
    assert rc == 2
    assert any(line.startswith("error:") for line in logs)


def test_export_bad_palette_entry_exits_2_not_a_traceback(tmp_path, logs):
    data_dir = tmp_path / "data"
    data_dir.mkdir()
    entries = [b"x"] * 4
    entries[3] = b"short"  # palette must be exactly 768 bytes
    (data_dir / "ITD_RESS.PAK").write_bytes(helpers.pak_bytes(entries))
    rc = textures.main(["export", "--data", str(data_dir), "--out", str(tmp_path / "out")],
                       root=tmp_path, log=logs.append)
    assert rc == 2
    assert any(line.startswith("error:") for line in logs)


def test_export_uses_repo_relative_defaults(synthetic_data_dir, tmp_path, logs):
    root = tmp_path / "repo"
    (root / "data").mkdir(parents=True)
    (root / "data" / "aitd1").symlink_to(synthetic_data_dir, target_is_directory=True)
    assert textures.main(["export"], root=root, log=logs.append) == 0
    assert (root / "data" / "textures" / "manifest.json").is_file()


def test_import_with_absent_default_src_is_a_notice(tmp_path, logs):
    assert textures.main(["import"], root=tmp_path, log=logs.append) == 0
    assert any("nothing to import" in line for line in logs)
    assert not (tmp_path / "Assets").exists()


def test_import_with_absent_explicit_src_is_a_usage_error(tmp_path, logs):
    rc = textures.main(["import", "--src", str(tmp_path / "missing")], root=tmp_path, log=logs.append)
    assert rc == 2


def test_import_with_explicit_src_matching_default_absent_is_a_notice(tmp_path, logs):
    # make check-textures always passes --src explicitly (the Makefile default),
    # so an absent default must still get the friendly notice, not exit 2.
    root = tmp_path
    rc = textures.main(["import", "--src", str(root / "data/textures-ai")], root=root, log=logs.append)
    assert rc == 0
    assert any("nothing to import" in line for line in logs)


def test_import_validation_errors_exit_1(tmp_path, logs):
    ai = tmp_path / "ai"
    (ai / "backgrounds").mkdir(parents=True)
    Image.new("RGB", (1280, 720)).save(ai / "backgrounds" / "CAMERA00_000.png")
    rc = textures.main(["import", "--src", str(ai), "--dest", str(tmp_path / "dest")],
                       root=tmp_path, log=logs.append)
    assert rc == 1
    assert any("error: backgrounds/CAMERA00_000.png" in line for line in logs)


def test_import_dry_run_and_dark_flags(tmp_path, logs):
    ai = tmp_path / "ai"
    (ai / "backgrounds").mkdir(parents=True)
    Image.new("RGB", (640, 400), (100, 100, 100)).save(ai / "backgrounds" / "CAMERA02_007.png")
    dest = tmp_path / "dest"
    rc = textures.main(["import", "--src", str(ai), "--dest", str(dest), "--dark", "all",
                        "--dark-factor", "0.5", "--dry-run"], root=tmp_path, log=logs.append)
    assert rc == 0
    assert not dest.exists()
    assert any("dry run" in line for line in logs)
    rc = textures.main(["import", "--src", str(ai), "--dest", str(dest), "--dark", "all",
                        "--dark-factor", "0.5"], root=tmp_path, log=logs.append)
    assert rc == 0
    with Image.open(dest / "CAMERA02_007_DARK.png") as im:
        assert im.getpixel((0, 0)) == (50, 50, 50)


def test_bad_dark_policy_is_rejected_by_argparse(tmp_path):
    with pytest.raises(SystemExit) as exc:
        textures.main(["import", "--dark", "dim"], root=tmp_path, log=lambda *_: None)
    assert exc.value.code == 2


def test_default_root_is_the_repo(tmp_path):
    assert textures.default_root() == pathlib.Path(textures.__file__).resolve().parents[1]


def test_export_reads_animations_from_the_default_assets_folder(synthetic_data_dir, tmp_path, logs):
    root = tmp_path / "repo"
    clip = root / "Assets" / "backgrounds_hd" / "anim_ITD_RESS_011"
    clip.mkdir(parents=True)
    Image.new("RGB", (640, 400)).save(clip / "a.png")
    rc = textures.main(["export", "--data", str(synthetic_data_dir)], root=root, log=logs.append)
    assert rc == 0
    m = read_manifest(root / "data" / "textures" / "manifest.json")
    assert [j.name for j in m.animations] == ["ITD_RESS_011"]
    assert m.animations[0].still == "screens/ITD_RESS_011.png"


def test_export_anims_flag_overrides_the_default_and_a_skip_exits_1(synthetic_data_dir, tmp_path, logs):
    custom = tmp_path / "custom"
    (custom / "anim_CAMERA00_000").mkdir(parents=True)
    rc = textures.main(["export", "--data", str(synthetic_data_dir), "--out", str(tmp_path / "o"),
                        "--anims", str(custom)], root=tmp_path, log=logs.append)
    assert rc == 1
    assert any("anim_CAMERA00_000: no PNG frames" in line for line in logs)


def test_export_explicit_missing_anims_is_a_usage_error(synthetic_data_dir, tmp_path, logs):
    missing = tmp_path / "nope"
    rc = textures.main(["export", "--data", str(synthetic_data_dir), "--out", str(tmp_path / "o"),
                        "--anims", str(missing)], root=tmp_path, log=logs.append)
    assert rc == 2
    assert any(line == f"error: {missing} is not a directory" for line in logs)
    assert not (tmp_path / "o").exists()


def test_export_default_missing_anims_is_a_notice(synthetic_data_dir, tmp_path, logs):
    rc = textures.main(["export", "--data", str(synthetic_data_dir), "--out", str(tmp_path / "o")],
                       root=tmp_path, log=logs.append)
    assert rc == 0
    assert any(line.startswith("no animations:") for line in logs)


def test_export_explicit_anims_matching_default_absent_is_a_notice(synthetic_data_dir, tmp_path, logs):
    # make export-textures always passes --anims explicitly (the Makefile default:
    # anims ?= $(dest)), so an absent default folder must still get the friendly
    # notice, not exit 2.
    root = tmp_path
    rc = textures.main(["export", "--data", str(synthetic_data_dir), "--out", str(root / "o"),
                        "--anims", str(root / "Assets" / "backgrounds_hd")], root=root, log=logs.append)
    assert rc == 0
    assert any(line.startswith("no animations:") for line in logs)


def test_import_dry_run_summary_says_animations_to_import(tmp_path, logs):
    originals = tmp_path / "textures"
    write_manifest(originals / "manifest.json", tmp_path, [],
                   [make_job("CAMERA03_008", "backgrounds/CAMERA03_008.png", 2, (640, 400))])
    ai = tmp_path / "ai"
    frames = ai / "animations" / "CAMERA03_008" / "frames"
    frames.mkdir(parents=True)
    for i in (1, 2):
        Image.new("RGB", (640, 400)).save(frames / f"frame_{i:04d}.png")
    dest = tmp_path / "dest"
    rc = textures.main(["import", "--src", str(ai), "--dest", str(dest), "--originals", str(originals),
                        "--dry-run"], root=tmp_path, log=logs.append)
    assert rc == 0
    assert "  animations to import: 1 (2 frames)" in logs
    assert not any("animations imported:" in line for line in logs)
    assert not dest.exists()


def test_import_summary_counts_animations_and_shadowed_stills(tmp_path, logs):
    originals = tmp_path / "textures"
    write_manifest(originals / "manifest.json", tmp_path, [],
                   [make_job("CAMERA03_008", "backgrounds/CAMERA03_008.png", 2, (640, 400))])
    ai = tmp_path / "ai"
    frames = ai / "animations" / "CAMERA03_008" / "frames"
    frames.mkdir(parents=True)
    for i in (1, 2):
        Image.new("RGB", (640, 400)).save(frames / f"frame_{i:04d}.png")
    (ai / "backgrounds").mkdir()
    Image.new("RGB", (640, 400)).save(ai / "backgrounds" / "CAMERA00_000.png")
    dest = tmp_path / "dest"
    (dest / "anim_CAMERA00_000").mkdir(parents=True)
    rc = textures.main(["import", "--src", str(ai), "--dest", str(dest), "--originals", str(originals)],
                       root=tmp_path, log=logs.append)
    assert rc == 0
    assert "  animations imported: 1 (2 frames)" in logs
    assert "  shadowed by an animation: 1" in logs
    assert (dest / "anim_CAMERA03_008" / "frame_0002.png").is_file()
