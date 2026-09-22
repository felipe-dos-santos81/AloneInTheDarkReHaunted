import numpy as np
from PIL import Image

from aitd_textures.decode import decode_palette
from aitd_textures.export import export_all
from aitd_textures.manifest import MANIFEST_NAME, read_manifest, sha256_rgb
from aitd_textures.pak import Pak
from helpers import pak_bytes as _pak_bytes, synthetic_palette


def _quiet(*_args, **_kwargs):
    pass


def test_export_writes_every_plate_and_screen(synthetic_data_dir, tmp_path):
    out = tmp_path / "textures"
    result = export_all(synthetic_data_dir, out, log=_quiet)
    assert result.skipped == []
    assert (result.cameras, result.screens) == (9, 13)
    assert (out / "backgrounds" / "CAMERA00_001.png").is_file()
    assert (out / "backgrounds" / "CAMERA07_000.png").is_file()
    assert (out / "screens" / "ITD_RESS_013.png").is_file()
    with Image.open(out / "backgrounds" / "CAMERA00_000.png") as im:
        assert im.size == (320, 200) and im.mode == "RGB"
    assert list(out.rglob("*.tmp")) == []


def test_export_manifest_describes_each_file(synthetic_data_dir, tmp_path):
    out = tmp_path / "textures"
    export_all(synthetic_data_dir, out, log=_quiet)
    m = read_manifest(out / MANIFEST_NAME)
    assert m.data_dir == str(synthetic_data_dir)
    assert len(m.records) == 22
    rec = m.by_path()["backgrounds/CAMERA00_001.png"]
    assert (rec.target, rec.kind, rec.pak, rec.entry, rec.floor, rec.size) == (
        "CAMERA00_001.png", "camera", "CAMERA00", 1, 0, (320, 200))
    with Image.open(out / rec.path) as im:
        assert rec.sha256 == sha256_rgb(np.asarray(im))
    scr = m.by_path()["screens/ITD_RESS_006.png"]
    assert (scr.kind, scr.pak, scr.entry, scr.floor) == ("screen", "ITD_RESS", 6, None)


def test_export_pixels_are_palette_lookups(synthetic_data_dir, tmp_path):
    out = tmp_path / "textures"
    export_all(synthetic_data_dir, out, log=_quiet)
    palette = decode_palette(Pak(synthetic_data_dir / "ITD_RESS.PAK").read(3))
    raw = Pak(synthetic_data_dir / "CAMERA01.PAK").read(0)
    expected = palette[np.frombuffer(raw, dtype=np.uint8).reshape(200, 320)]
    with Image.open(out / "backgrounds" / "CAMERA01_000.png") as im:
        assert np.array_equal(np.asarray(im), expected)


def test_export_title_screen_uses_its_embedded_palette(synthetic_data_dir, tmp_path):
    out = tmp_path / "textures"
    export_all(synthetic_data_dir, out, log=_quiet)
    raw = Pak(synthetic_data_dir / "ITD_RESS.PAK").read(13)
    inverted = decode_palette(raw[2:770])
    assert not np.array_equal(inverted, decode_palette(synthetic_palette()))
    expected = inverted[np.frombuffer(raw[770:], dtype=np.uint8).reshape(200, 320)]
    with Image.open(out / "screens" / "ITD_RESS_013.png") as im:
        assert np.array_equal(np.asarray(im), expected)


def test_export_skips_entries_of_the_wrong_size(synthetic_data_dir, tmp_path):
    (synthetic_data_dir / "CAMERA04.PAK").write_bytes(_pak_bytes([b"tiny"]))
    out = tmp_path / "textures"
    result = export_all(synthetic_data_dir, out, log=_quiet)
    assert len(result.skipped) == 1 and result.skipped[0].startswith("CAMERA04 entry 0")
    assert (result.cameras, result.screens) == (8, 13)
    assert not (out / "backgrounds" / "CAMERA04_000.png").exists()
    assert "backgrounds/CAMERA04_000.png" not in read_manifest(out / MANIFEST_NAME).by_path()


def _clip(folder, color=(9, 9, 9)):
    folder.mkdir(parents=True)
    Image.new("RGB", (1280, 800), color).save(folder / "ezgif-frame-001.png")


def test_export_adds_animation_jobs_to_the_manifest(synthetic_data_dir, tmp_path):
    anims = tmp_path / "hd"
    _clip(anims / "anim_CAMERA00_001")
    _clip(anims / "anim_StartupMenuBackground")
    out = tmp_path / "textures"
    logs = []
    result = export_all(synthetic_data_dir, out, log=logs.append, anims_dir=anims)
    assert [j.name for j in result.animations] == ["CAMERA00_001", "StartupMenuBackground"]
    assert result.synthesized == 1 and result.skipped == []
    m = read_manifest(out / MANIFEST_NAME)
    assert m.animations == result.animations
    assert m.animations[0].still == "backgrounds/CAMERA00_001.png"
    assert m.animations[1].still == "animations/StartupMenuBackground/still.png"
    assert any("2 animations (1 synthesized stills)" in line for line in logs)


def test_export_without_an_anims_folder_writes_no_jobs(synthetic_data_dir, tmp_path):
    logs = []
    result = export_all(synthetic_data_dir, tmp_path / "t", log=logs.append, anims_dir=tmp_path / "missing")
    assert result.animations == [] and result.skipped == []
    assert read_manifest(tmp_path / "t" / MANIFEST_NAME).animations == []
    assert any(line.startswith("no animations:") for line in logs)


def test_an_unusable_clip_is_reported_as_skipped(synthetic_data_dir, tmp_path):
    (tmp_path / "hd" / "anim_CAMERA00_000").mkdir(parents=True)
    result = export_all(synthetic_data_dir, tmp_path / "t", log=_quiet, anims_dir=tmp_path / "hd")
    assert result.skipped == ["anim_CAMERA00_000: no PNG frames"]
    assert (result.cameras, result.screens) == (9, 13)
