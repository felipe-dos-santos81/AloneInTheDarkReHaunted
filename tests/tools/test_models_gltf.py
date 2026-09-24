import json
import struct

import numpy as np
import pytest

from aitd_models.gltf import (UNSIGNED_BYTE, GlbBuilder, GltfError, quaternion, read_glb, trs_matrix)
from aitd_models.pose import rotation


def build():
    g = GlbBuilder()
    pos = g.accessor(np.array([[0, 0, 0], [1, 2, 3], [-1, 0.5, 2]]), "VEC3", bounds=True)
    idx = g.accessor(np.array([[0, 1, 2, 255]]), "VEC4", UNSIGNED_BYTE, normalized=True)
    img = g.image(b"\x89PNG fake", "image/png")
    g.add("nodes", {"name": "n"})
    return g.to_bytes(), pos, idx, img


def test_round_trip():
    data, pos, idx, img = build()
    glb = read_glb(data)
    assert np.allclose(glb.accessor(pos), [[0, 0, 0], [1, 2, 3], [-1, 0.5, 2]])
    assert np.allclose(glb.accessor(idx), [[0, 1 / 255, 2 / 255, 1.0]])
    assert glb.doc["accessors"][pos]["min"] == [-1, 0, 0] and glb.doc["accessors"][pos]["max"] == [1, 2, 3]
    assert glb.image_bytes(img) == (b"\x89PNG fake", "image/png")
    assert glb.doc["nodes"] == [{"name": "n"}]


def test_layout_is_aligned():
    data, *_ = build()
    magic, version, length = struct.unpack_from("<III", data)
    assert (magic, version, length) == (0x46546C67, 2, len(data)) and length % 4 == 0
    json_len = struct.unpack_from("<I", data, 12)[0]
    assert json_len % 4 == 0
    doc = json.loads(data[20:20 + json_len])
    assert all(v["byteOffset"] % 4 == 0 for v in doc["bufferViews"])


@pytest.mark.parametrize("data, message", [
    (b"nope" * 5, "not a binary glTF"),
    (b"glTF" + struct.pack("<II", 2, 999) + b"\0" * 8, "says 999 bytes"),
    (b"glTF" + struct.pack("<II", 2, 20) + struct.pack("<II", 0, 0x004E4942), "no JSON chunk"),
])
def test_rejects_bad_files(data, message):
    with pytest.raises(GltfError, match=message):
        read_glb(data)


def test_quaternion_round_trips_through_trs():
    for d in [(0, 0, 0), (100, 0, 0), (0, 700, 0), (0, 0, 300), (300, 700, 50), (512, 512, 0)]:
        r = rotation(d)
        u, _, vt = np.linalg.svd(r)
        r = u @ vt
        assert np.allclose(trs_matrix({"rotation": list(quaternion(r))})[:3, :3], r, atol=1e-9)


def test_trs_composes_translation_rotation_scale():
    m = trs_matrix({"translation": [1, 2, 3], "rotation": [0, 0, np.sqrt(0.5), np.sqrt(0.5)], "scale": [2, 1, 1]})
    assert np.allclose(m @ [1, 0, 0, 1], [1, 4, 3, 1])


def test_quaternion_refuses_a_reflection():
    with pytest.raises(GltfError, match="reflection"):
        quaternion(np.diag([1.0, 1.0, -1.0]))
