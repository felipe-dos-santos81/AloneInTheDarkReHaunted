import struct
import zlib

import numpy as np
import pytest

from aitd_models.hdm import HEADER, VERTEX, HdmError, read_hdm, write_hdm
from model_helpers import TINY_HDM, TINY_PNG, tiny_hdm

# Byte offsets in the tiny fixture: 32-byte header, 4 x 40-byte vertices,
# 6 x u32 indices, 69 texture bytes, CRC.
VERTS, INDICES = 32, 32 + 4 * 40
TEXTURE = INDICES + 6 * 4


def resign(data: bytes) -> bytes:
    """Recompute the trailing CRC, so a test reaches the check after it."""
    return data[:-4] + struct.pack("<I", zlib.crc32(data[:-4]))


def cut(data: bytes, offset: int, size: int) -> bytes:
    """`data` without `size` bytes at `offset` (re-sign with `patch` after)."""
    return data[:offset] + data[offset + size:]


def patch(data: bytes, offset: int, fmt: str, *values) -> bytes:
    out = bytearray(data)
    struct.pack_into(fmt, out, offset, *values)
    return resign(bytes(out))


def test_writer_reproduces_the_shared_fixture():
    assert write_hdm(tiny_hdm()) == TINY_HDM.read_bytes()


def test_layout():
    data = TINY_HDM.read_bytes()
    assert len(data) == 32 + 4 * 40 + 6 * 4 + len(TINY_PNG) + 4 == 289
    assert HEADER.unpack_from(data) == (b"AHDM", 1, 2, 0x0123456789ABCDEF, 4, 6, len(TINY_PNG), 1, b"\0\0\0")
    assert struct.unpack_from("<3f3f2f4B4B", data, VERTS + 40) == (100, 0, 0, 0, 0, -1, 1, 0, 0, 1, 0, 0, 128, 127, 0, 0)
    assert struct.unpack_from("<6I", data, INDICES) == (0, 1, 2, 2, 1, 3)
    assert data[TEXTURE:TEXTURE + len(TINY_PNG)] == TINY_PNG
    assert struct.unpack_from("<I", data, len(data) - 4)[0] == zlib.crc32(data[:-4])


def test_round_trip():
    mesh = read_hdm(TINY_HDM.read_bytes())
    want = tiny_hdm()
    assert (mesh.group_count, mesh.skeleton_hash, mesh.texture, mesh.texture_kind) == \
           (want.group_count, want.skeleton_hash, want.texture, want.texture_kind)
    assert mesh.vertices.tobytes() == want.vertices.tobytes()
    assert mesh.indices.tolist() == want.indices.tolist()


@pytest.mark.parametrize("mutate, message", [
    (lambda d: d[:30], "too short"),
    (lambda d: resign(b"XHDM" + d[4:]), "bad magic"),
    (lambda d: patch(d, 4, "<H", 2), "unsupported version 2"),
    (lambda d: patch(d, 29, "<B", 1), "reserved"),
    (lambda d: resign(d[:-4] + b"\0" + d[-4:]), "header says"),
    (lambda d: d[:TEXTURE] + bytes([d[TEXTURE] ^ 1]) + d[TEXTURE + 1:], "CRC mismatch"),
    (lambda d: patch(d, 6, "<H", 0), "group count 0"),
    (lambda d: patch(d, 6, "<H", 33), "group count 33"),
    (lambda d: patch(d, 28, "<B", 3), "unknown texture kind 3"),
    (lambda d: patch(d, INDICES + 20, "<I", 4), "index 4 >= vertex count 4"),
    (lambda d: patch(d, VERTS + 2 * 40 + 32, "<B", 2), "joint 2 >= group count 2"),
    (lambda d: patch(d, VERTS + 40 + 36, "<B", 127), "weights sum to 254"),
    (lambda d: patch(d, VERTS, "<f", float("nan")), "non-finite position"),
    (lambda d: patch(d, VERTS + 3 * 40 + 12, "<f", float("inf")), "non-finite normal"),
    (lambda d: patch(d, VERTS + 24, "<f", float("nan")), "non-finite uv"),
    (lambda d: patch(d, 16, "<I", 150_001), "counts over budget"),
    (lambda d: patch(cut(d, VERTS + 2 * 40, 2 * 40), 16, "<I", 2), "vertex count 2 outside 3..150000"),
    (lambda d: patch(cut(d, INDICES + 20, 4), 20, "<I", 5), "index count 5 is not 1..50000 triangles"),
    (lambda d: patch(cut(d, TEXTURE, len(TINY_PNG)), 24, "<I", 0), "texture of 0 bytes"),
])
def test_rejections(mutate, message):
    with pytest.raises(HdmError, match=message):
        read_hdm(mutate(TINY_HDM.read_bytes()))


def test_writer_refuses_what_the_reader_would():
    mesh = tiny_hdm()
    mesh.vertices["weights"][0] = (200, 0, 0, 0)
    with pytest.raises(HdmError, match="weights sum to 200"):
        write_hdm(mesh)
    mesh = tiny_hdm()
    mesh.indices = np.array([0, 1], np.uint32)
    with pytest.raises(HdmError, match="index count 2"):
        write_hdm(mesh)


def test_vertex_record_is_40_bytes():
    assert VERTEX.itemsize == 40 and HEADER.size == 32
