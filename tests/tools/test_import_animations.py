import dataclasses

from PIL import Image

from aitd_textures import importer
from aitd_textures.animations import make_job
from aitd_textures.importer import validate_sequence
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
