import pytest

from aitd_textures.explode import ExplodeError, explode
from aitd_textures.pak import FLAG_RAW, Pak, PakError


def test_count_info_and_read_raw_entries(tmp_path, pak_bytes):
    path = tmp_path / "TEST.PAK"
    path.write_bytes(pak_bytes([b"alpha", b"", b"gamma" * 3]))
    pak = Pak(path)
    assert pak.count == 3
    assert pak.read(0) == b"alpha"
    assert pak.read(1) == b""
    assert pak.read(2) == b"gamma" * 3
    info = pak.info(2)
    assert (info.disc_size, info.uncompressed_size, info.flag) == (15, 15, FLAG_RAW)


def test_entry_out_of_range(tmp_path, pak_bytes):
    path = tmp_path / "TEST.PAK"
    path.write_bytes(pak_bytes([b"x"]))
    pak = Pak(path)
    with pytest.raises(PakError, match="out of range"):
        pak.read(1)
    with pytest.raises(PakError, match="out of range"):
        pak.info(-1)


def test_missing_and_too_small_files(tmp_path):
    with pytest.raises(PakError, match="not found"):
        Pak(tmp_path / "NOPE.PAK")
    small = tmp_path / "SMALL.PAK"
    small.write_bytes(b"\0\0")
    with pytest.raises(PakError, match="too small"):
        Pak(small)


def test_truncated_entry(tmp_path, pak_bytes):
    path = tmp_path / "TEST.PAK"
    path.write_bytes(pak_bytes([b"0123456789"])[:-4])
    with pytest.raises(PakError, match="truncated"):
        Pak(path).read(0)


def test_unknown_flag(tmp_path, pak_bytes):
    path = tmp_path / "TEST.PAK"
    path.write_bytes(pak_bytes([b"x"], flag=9))
    with pytest.raises(PakError, match="unknown flag"):
        Pak(path).read(0)


def test_explode_rejects_truncated_stream():
    with pytest.raises(ExplodeError):
        explode(b"\x00", 10, 0)
