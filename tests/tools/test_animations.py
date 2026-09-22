# SPDX-License-Identifier: GPL-2.0-only
import shutil

import numpy as np
from PIL import Image

from aitd_textures.animations import (
    FRAME_RE,
    discover,
    export_animations,
    frame_name,
    make_job,
    replace_folder,
    write_menu_frames,
)
from aitd_textures.manifest import ImageRecord


def _quiet(*_args, **_kwargs):
    pass


def make_clip(folder, count, size=(1280, 800), prefix="ezgif-frame-"):
    folder.mkdir(parents=True, exist_ok=True)
    for i in range(1, count + 1):
        Image.new("RGB", size, (i, 2 * i, 3 * i)).save(folder / f"{prefix}{i:03d}.png")
    return folder


def record(path):
    """An exported image record; export_animations only reads path and target."""
    return ImageRecord(path, path.rsplit("/", 1)[1], "camera", "X", 0, None, (320, 200), "0" * 64)


def test_frame_names_sort_in_play_order():
    names = [frame_name(n) for n in (1, 2, 10, 100, 1000)]
    assert names == sorted(names) and names[0] == "frame_0001.png"
    assert FRAME_RE.match("frame_0042.png") and not FRAME_RE.match("frame_42.png")


def test_make_job_for_a_camera_the_menu_and_a_disabled_screen():
    job = make_job("CAMERA03_008", "backgrounds/CAMERA03_008.png", 300, (1280, 800))
    assert job.to_json() == {
        "name": "CAMERA03_008", "kind": "camera", "floor": 3,
        "still": "backgrounds/CAMERA03_008.png",
        "reference": "animations/CAMERA03_008/reference",
        "reference_frames": 300, "reference_size": [1280, 800],
        "frames_dir": "animations/CAMERA03_008/frames",
        "fps": 12.5, "loop": True, "max_frames": None,
        "engine": "anim_CAMERA03_008/", "active": True,
    }
    menu = make_job("StartupMenuBackground", "animations/StartupMenuBackground/still.png", 60, (1280, 800))
    assert (menu.kind, menu.floor, menu.max_frames, menu.engine) == (
        "menu", None, 512, "StartupMenuBackground_NNN.png")
    assert make_job("ITD_RESS_012_DISABLED", "x", 1, (1, 1)).active is False


def test_discover_skips_folders_that_are_not_jobs(tmp_path):
    for name in ("anim_CAMERA07_004", "anim_CAMERA07_004_grassmask", "anim_CAMERA02_007_DARK",
                 "anim_CAMERA03_008.tmp", "anim_CAMERA03_008.old", "anim_", "backgrounds",
                 "anim_ITD_RESS_002_NOTATOU"):
        (tmp_path / name).mkdir()
    (tmp_path / "anim_file.png").write_bytes(b"")
    assert [p.name for p in discover(tmp_path)] == ["anim_CAMERA07_004", "anim_ITD_RESS_002_NOTATOU"]


def test_reference_is_copied_renamed_and_byte_identical(tmp_path):
    anims = tmp_path / "hd"
    clip = make_clip(anims / "anim_CAMERA03_008", 3)
    (clip / ".DS_Store").write_bytes(b"x")
    out = tmp_path / "textures"
    result = export_animations(anims, out, [record("backgrounds/CAMERA03_008.png")], log=_quiet)
    ref = out / "animations" / "CAMERA03_008" / "reference"
    assert sorted(p.name for p in ref.iterdir()) == ["frame_0001.png", "frame_0002.png", "frame_0003.png"]
    assert (ref / "frame_0002.png").read_bytes() == (clip / "ezgif-frame-002.png").read_bytes()
    assert [j.name for j in result.jobs] == ["CAMERA03_008"]
    job = result.jobs[0]
    assert (job.still, job.reference_frames, job.reference_size) == (
        "backgrounds/CAMERA03_008.png", 3, (1280, 800))
    assert result.synthesized == 0 and result.skipped == []
    assert not (out / "animations" / "CAMERA03_008" / "still.png").exists()
    assert list(out.rglob("*.tmp")) == []


def test_still_is_synthesized_when_no_plate_has_the_name(tmp_path):
    anims = tmp_path / "hd"
    make_clip(anims / "anim_ITD_RESS_002_NOTATOU", 1)
    out = tmp_path / "textures"
    # a plate for the base slot exists, but a suffixed variant never borrows it
    result = export_animations(anims, out, [record("screens/ITD_RESS_002.png")], log=_quiet)
    job = result.jobs[0]
    assert job.still == "animations/ITD_RESS_002_NOTATOU/still.png"
    assert result.synthesized == 1
    with Image.open(out / job.still) as im:
        assert im.size == (320, 200) and im.mode == "RGB"


def test_synthesized_still_is_an_area_average(tmp_path):
    rng = np.random.default_rng(1)
    small = rng.integers(0, 256, (200, 320, 3), dtype=np.uint8)
    big = np.repeat(np.repeat(small, 4, axis=0), 4, axis=1)
    folder = tmp_path / "hd" / "anim_StartupMenuBackground"
    folder.mkdir(parents=True)
    Image.fromarray(big).save(folder / "ezgif-frame-001.png")
    out = tmp_path / "textures"
    export_animations(tmp_path / "hd", out, [], log=_quiet)
    with Image.open(out / "animations" / "StartupMenuBackground" / "still.png") as im:
        assert np.array_equal(np.asarray(im), small)


def test_reference_is_write_once(tmp_path):
    anims = tmp_path / "hd"
    make_clip(anims / "anim_CAMERA03_008", 3)
    out = tmp_path / "textures"
    export_animations(anims, out, [], log=_quiet)
    shutil.rmtree(anims / "anim_CAMERA03_008")
    make_clip(anims / "anim_CAMERA03_008", 5, prefix="reanimated-")  # what an import leaves behind
    result = export_animations(anims, out, [], log=_quiet)
    ref = out / "animations" / "CAMERA03_008" / "reference"
    assert len(list(ref.iterdir())) == 3
    assert result.jobs[0].reference_frames == 3


def test_folders_without_png_frames_are_skipped(tmp_path):
    anims = tmp_path / "hd"
    (anims / "anim_CAMERA01_000").mkdir(parents=True)
    tga = anims / "anim_CAMERA01_001"
    tga.mkdir()
    Image.new("RGB", (64, 40)).save(tga / "f001.tga")
    make_clip(anims / "anim_CAMERA01_002", 2, size=(640, 400))
    logs = []
    result = export_animations(anims, tmp_path / "out", [], log=logs.append)
    assert [j.name for j in result.jobs] == ["CAMERA01_002"]
    assert result.skipped == ["anim_CAMERA01_000: no PNG frames",
                              "anim_CAMERA01_001: non-PNG frames (f001.tga)"]
    assert any(line.startswith("warning: skipped anim_CAMERA01_000") for line in logs)
    assert not (tmp_path / "out" / "animations" / "CAMERA01_000").exists()


def test_replace_folder_swaps_in_exactly_the_new_files(tmp_path):
    final = tmp_path / "anim_X"
    final.mkdir()
    (final / "ezgif-frame-001.png").write_bytes(b"old")
    replace_folder(final, [("frame_0001.png", b"a"), ("frame_0002.png", b"b")])
    assert sorted(p.name for p in final.iterdir()) == ["frame_0001.png", "frame_0002.png"]
    assert (final / "frame_0002.png").read_bytes() == b"b"
    assert sorted(p.name for p in tmp_path.iterdir()) == ["anim_X"]


def test_replace_folder_creates_a_missing_folder_and_clears_leftovers(tmp_path):
    hd = tmp_path / "hd"
    for leftover in ("anim_Y.tmp", "anim_Y.old"):
        (hd / leftover).mkdir(parents=True)
        (hd / leftover / "junk.png").write_bytes(b"x")
    replace_folder(hd / "anim_Y", [("frame_0001.png", b"a")])
    assert (hd / "anim_Y" / "frame_0001.png").read_bytes() == b"a"
    assert sorted(p.name for p in hd.iterdir()) == ["anim_Y"]


def test_write_menu_frames_removes_higher_numbers_only(tmp_path):
    for n in range(1, 5):
        (tmp_path / f"StartupMenuBackground_{n:03d}.png").write_bytes(b"old")
    (tmp_path / "StartupMenuBackground.png").write_bytes(b"still")
    (tmp_path / "StartupMenuBackgroundWithArt_003.png").write_bytes(b"art")
    write_menu_frames(tmp_path, [b"a", b"b"])
    assert sorted(p.name for p in tmp_path.iterdir()) == [
        "StartupMenuBackground.png", "StartupMenuBackgroundWithArt_003.png",
        "StartupMenuBackground_001.png", "StartupMenuBackground_002.png"]
    assert (tmp_path / "StartupMenuBackground_002.png").read_bytes() == b"b"
