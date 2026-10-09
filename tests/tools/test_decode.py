import numpy as np
import pytest

from aitd_data.decode import DataNotFound, decode_palette, find_data_dir


def test_find_data_dir_accepts_the_indark_folder_itself(tmp_path):
    (tmp_path / "ITD_RESS.PAK").write_bytes(b"x")
    assert find_data_dir(tmp_path) == tmp_path


def test_find_data_dir_searches_nested_folders(tmp_path):
    indark = tmp_path / "Alone.app" / "Contents" / "Resources" / "game" / "INDARK"
    indark.mkdir(parents=True)
    (indark / "ITD_RESS.PAK").write_bytes(b"x")
    assert find_data_dir(tmp_path) == indark


def test_find_data_dir_raises_when_absent(tmp_path):
    with pytest.raises(DataNotFound, match="ITD_RESS.PAK"):
        find_data_dir(tmp_path / "nothing")


def test_decode_palette_keeps_8bit_values():
    raw = bytes(range(256)) * 3
    pal = decode_palette(raw)
    assert pal.shape == (256, 3) and pal.dtype == np.uint8
    assert pal[0].tolist() == [0, 1, 2]
    assert pal[85].tolist() == [255, 0, 1]


def test_decode_palette_scales_6bit_values_by_four():
    pal = decode_palette(bytes([63, 0, 1] * 256))
    assert pal[0].tolist() == [252, 0, 4]


def test_decode_palette_rejects_wrong_length():
    with pytest.raises(ValueError, match="768"):
        decode_palette(b"\0" * 767)
