import numpy as np
from PIL import Image

from aitd_textures.files import atomic_write_bytes, save_png


def test_atomic_write_creates_parents_and_leaves_no_tmp(tmp_path):
    target = tmp_path / "a" / "b" / "file.bin"
    atomic_write_bytes(target, b"hello")
    assert target.read_bytes() == b"hello"
    assert list(tmp_path.rglob("*.tmp")) == []


def test_atomic_write_replaces_existing(tmp_path):
    target = tmp_path / "file.bin"
    target.write_bytes(b"old")
    atomic_write_bytes(target, b"new")
    assert target.read_bytes() == b"new"


def test_save_png_round_trips_rgb_pixels(tmp_path):
    pixels = (np.arange(200 * 320 * 3) % 251).astype(np.uint8).reshape(200, 320, 3)
    out = tmp_path / "img.png"
    save_png(out, pixels)
    with Image.open(out) as im:
        assert im.format == "PNG" and im.mode == "RGB" and im.size == (320, 200)
        assert np.array_equal(np.asarray(im), pixels)
    assert list(tmp_path.glob("*.tmp")) == []
