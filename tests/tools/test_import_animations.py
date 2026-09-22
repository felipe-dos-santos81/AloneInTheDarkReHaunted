import dataclasses

import pytest
from PIL import Image

from aitd_textures import importer
from aitd_textures.animations import make_job
from aitd_textures.importer import run_import, validate_sequence
from aitd_textures.manifest import Manifest


def _quiet(*_args, **_kwargs):
    pass


def write_frames(folder, count, size=(640, 400), mode="RGB", colors=None):
    folder.mkdir(parents=True, exist_ok=True)
    for i in range(1, count + 1):
        color = colors[i - 1] if colors else (10, 20, 30)
        if mode == "RGBA":
            color = color + (255,)
        Image.new(mode, size, color).save(folder / f"frame_{i:04d}.png")
    return folder


def job(name="CAMERA03_008"):
    return make_job(name, f"backgrounds/{name}.png", 3, (1280, 800))


def manifest_for(*names):
    return Manifest(2, "aitd1", "", {}, [], [job(n) for n in names])


def test_a_clean_sequence_passes(tmp_path):
    d = write_frames(tmp_path / "frames", 3)
    seq, findings = validate_sequence(d, job())
    assert findings == []
    assert [p.name for p in seq.frames] == ["frame_0001.png", "frame_0002.png", "frame_0003.png"]
    assert seq.size == (640, 400) and seq.verbatim is True and seq.job == job()


def test_gaps_in_numbering_are_errors(tmp_path):
    d = write_frames(tmp_path / "frames", 3)
    (d / "frame_0002.png").unlink()
    seq, findings = validate_sequence(d, job())
    assert seq is None and [f.kind for f in findings] == ["frames"]
    assert "expected frame_0002.png, found frame_0003.png" in findings[0].message


def test_numbering_starts_at_one(tmp_path):
    d = tmp_path / "frames"
    d.mkdir()
    Image.new("RGB", (640, 400)).save(d / "frame_0000.png")
    seq, findings = validate_sequence(d, job())
    assert seq is None and "expected frame_0001.png, found frame_0000.png" in findings[0].message


def test_stray_files_are_errors(tmp_path):
    d = write_frames(tmp_path / "frames", 2)
    (d / "notes.txt").write_text("x")
    (d / "frame_3.png").write_bytes(b"")
    seq, findings = validate_sequence(d, job())
    assert seq is None and [f.kind for f in findings] == ["frames"]
    assert "frame_3.png" in findings[0].message and "notes.txt" in findings[0].message


def test_an_empty_folder_is_an_error(tmp_path):
    d = tmp_path / "frames"
    d.mkdir()
    seq, findings = validate_sequence(d, job())
    assert seq is None and findings[0].kind == "frames" and "no frames" in findings[0].message


def test_every_frame_must_match_frame_one(tmp_path):
    d = write_frames(tmp_path / "frames", 2)
    Image.new("RGB", (320, 200)).save(d / "frame_0003.png")
    seq, findings = validate_sequence(d, job())
    assert seq is None and [f.kind for f in findings] == ["frames"]
    assert findings[0].path == d / "frame_0003.png"
    assert "320x200" in findings[0].message and "640x400" in findings[0].message


def test_frame_one_must_be_16_10_and_not_too_large(tmp_path, monkeypatch):
    d = write_frames(tmp_path / "a", 2, size=(640, 360))
    assert [f.kind for f in validate_sequence(d, job())[1]] == ["aspect"]
    monkeypatch.setattr(importer, "MAX_SIDE", 500)
    d = write_frames(tmp_path / "b", 2)
    assert [f.kind for f in validate_sequence(d, job())[1]] == ["too_large"]


def test_a_broken_frame_is_invalid(tmp_path):
    d = write_frames(tmp_path / "frames", 3)
    data = (d / "frame_0002.png").read_bytes()
    (d / "frame_0002.png").write_bytes(data[: len(data) // 2])
    seq, findings = validate_sequence(d, job())
    assert seq is None and [f.kind for f in findings] == ["invalid"]
    assert findings[0].path == d / "frame_0002.png"


def test_max_frames_is_a_hard_limit(tmp_path):
    d = write_frames(tmp_path / "frames", 3)
    limited = dataclasses.replace(job("StartupMenuBackground"), max_frames=2)
    seq, findings = validate_sequence(d, limited)
    assert seq is None and [f.kind for f in findings] == ["frames"]
    assert "limit of 2" in findings[0].message


def test_mixed_modes_are_accepted_but_not_verbatim(tmp_path):
    d = write_frames(tmp_path / "frames", 2)
    Image.new("RGBA", (640, 400), (10, 20, 30, 255)).save(d / "frame_0003.png")
    seq, findings = validate_sequence(d, job())
    assert findings == [] and seq.verbatim is False


def test_loop_seam_is_a_warning(tmp_path):
    d = write_frames(tmp_path / "frames", 3, colors=[(0, 0, 0), (50, 50, 50), (200, 200, 200)])
    seq, findings = validate_sequence(d, job())
    assert seq is not None and [f.kind for f in findings] == ["seam"]
    assert findings[0].severity == "warning"
    one = write_frames(tmp_path / "one", 1, colors=[(0, 0, 0)])
    assert validate_sequence(one, job())[1] == []


def test_size_and_memory_warnings(tmp_path, monkeypatch):
    monkeypatch.setattr(importer, "MEMORY_BUDGET", 1000)
    d = write_frames(tmp_path / "frames", 2, size=(1000, 625))
    seq, findings = validate_sequence(d, job())
    assert seq is not None
    assert sorted(f.kind for f in findings) == ["memory", "size"]
    assert all(f.severity == "warning" for f in findings)


@pytest.fixture
def tree(tmp_path):
    src = tmp_path / "textures-ai"
    dest = tmp_path / "backgrounds_hd"
    dest.mkdir()
    return src, dest


def frames_dir(src, name):
    return src / "animations" / name / "frames"


def test_scene_animation_replaces_the_whole_folder(tree):
    src, dest = tree
    write_frames(frames_dir(src, "CAMERA07_004"), 2)
    old = dest / "anim_CAMERA07_004"
    old.mkdir()
    for i in range(1, 4):
        (old / f"ezgif-frame-{i:03d}.png").write_bytes(b"old")
    (dest / "anim_CAMERA07_004_grassmask").mkdir()
    (dest / "anim_CAMERA07_004_grassmask" / "grassmask_000.png").write_bytes(b"mask")
    result = run_import(src, dest, manifest_for("CAMERA07_004"), log=_quiet)
    assert result.errors == []
    assert result.animations == [dest / "anim_CAMERA07_004"] and result.animation_frames == 2
    assert sorted(p.name for p in old.iterdir()) == ["frame_0001.png", "frame_0002.png"]
    assert (old / "frame_0002.png").read_bytes() == (
        frames_dir(src, "CAMERA07_004") / "frame_0002.png").read_bytes()
    assert sorted(p.name for p in dest.iterdir()) == ["anim_CAMERA07_004"]  # grassmask cache gone


def test_a_rejected_sequence_leaves_the_destination_alone(tree):
    src, dest = tree
    d = write_frames(frames_dir(src, "CAMERA07_004"), 2)
    Image.new("RGB", (320, 200)).save(d / "frame_0003.png")
    old = dest / "anim_CAMERA07_004"
    old.mkdir()
    (old / "ezgif-frame-001.png").write_bytes(b"old")
    result = run_import(src, dest, manifest_for("CAMERA07_004"), log=_quiet)
    assert [f.kind for f in result.errors] == ["frames"]
    assert result.animations == []
    assert [p.name for p in old.iterdir()] == ["ezgif-frame-001.png"]
    assert sorted(p.name for p in dest.iterdir()) == ["anim_CAMERA07_004"]


def test_menu_frames_are_flat_and_stale_numbers_are_removed(tree):
    src, dest = tree
    write_frames(frames_dir(src, "StartupMenuBackground"), 3)
    for i in range(1, 6):
        (dest / f"StartupMenuBackground_{i:03d}.png").write_bytes(b"old")
    (dest / "StartupMenuBackground.png").write_bytes(b"still")
    (dest / "anim_StartupMenuBackground").mkdir()
    result = run_import(src, dest, manifest_for("StartupMenuBackground"), log=_quiet)
    assert result.errors == [] and result.animation_frames == 3
    assert result.animations == [dest / "StartupMenuBackground_001.png"]
    assert sorted(p.name for p in dest.glob("StartupMenuBackground_*.png")) == [
        "StartupMenuBackground_001.png", "StartupMenuBackground_002.png", "StartupMenuBackground_003.png"]
    assert (dest / "StartupMenuBackground_001.png").read_bytes() != b"old"
    assert (dest / "StartupMenuBackground.png").read_bytes() == b"still"
    assert (dest / "anim_StartupMenuBackground").is_dir()


def test_mixed_modes_are_written_as_rgb(tree):
    src, dest = tree
    d = write_frames(frames_dir(src, "CAMERA03_008"), 1)
    Image.new("RGBA", (640, 400), (10, 20, 30, 128)).save(d / "frame_0002.png")
    result = run_import(src, dest, manifest_for("CAMERA03_008"), log=_quiet)
    assert result.errors == []
    for p in sorted((dest / "anim_CAMERA03_008").iterdir()):
        with Image.open(p) as im:
            assert im.mode == "RGB" and im.size == (640, 400)


def test_animations_need_a_matching_job(tree):
    src, dest = tree
    write_frames(frames_dir(src, "CAMERA03_008"), 1)
    result = run_import(src, dest, None, log=_quiet)
    assert [f.kind for f in result.errors] == ["animation"]
    assert "manifest" in result.errors[0].message
    result = run_import(src, dest, manifest_for("CAMERA05_013"), log=_quiet)
    assert [f.kind for f in result.errors] == ["animation"]
    assert "CAMERA03_008" in result.errors[0].message
    assert list(dest.iterdir()) == []


def test_reference_and_still_are_never_imported(tree):
    src, dest = tree
    write_frames(src / "animations" / "ITD_RESS_002" / "reference", 2)
    Image.new("RGB", (320, 200)).save(src / "animations" / "ITD_RESS_002" / "still.png")
    result = run_import(src, dest, manifest_for("ITD_RESS_002"), log=_quiet)
    assert result.findings == [] and result.animations == [] and list(dest.iterdir()) == []


def test_dry_run_validates_animations_but_writes_nothing(tree):
    src, dest = tree
    write_frames(frames_dir(src, "CAMERA03_008"), 2)
    write_frames(frames_dir(src, "StartupMenuBackground"), 2)
    result = run_import(src, dest, manifest_for("CAMERA03_008", "StartupMenuBackground"),
                        dry_run=True, log=_quiet)
    assert result.errors == [] and result.animation_frames == 4
    assert sorted(p.name for p in result.animations) == ["StartupMenuBackground_001.png", "anim_CAMERA03_008"]
    assert list(dest.iterdir()) == []
