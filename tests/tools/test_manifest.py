import json

import numpy as np
import pytest
from PIL import Image

from aitd_textures.manifest import (
    SCHEMA,
    ImageRecord,
    ManifestError,
    read_manifest,
    sha256_rgb,
    write_manifest,
)


def _records():
    return [
        ImageRecord("backgrounds/CAMERA00_000.png", "CAMERA00_000.png", "camera",
                    "CAMERA00", 0, 0, (320, 200), "a" * 64),
        ImageRecord("screens/ITD_RESS_013.png", "ITD_RESS_013.png", "screen",
                    "ITD_RESS", 13, None, (320, 200), "b" * 64),
    ]


def test_manifest_round_trip(tmp_path):
    path = tmp_path / "manifest.json"
    write_manifest(path, tmp_path / "INDARK", _records())
    m = read_manifest(path)
    assert m.schema == SCHEMA and m.game == "aitd1"
    assert m.data_dir == str(tmp_path / "INDARK")
    assert m.palette == {"pak": "ITD_RESS", "entry": 3}
    assert m.records == _records()
    assert set(m.by_path()) == {"backgrounds/CAMERA00_000.png", "screens/ITD_RESS_013.png"}
    assert list(tmp_path.glob("*.tmp")) == []


def test_manifest_json_shape(tmp_path):
    path = tmp_path / "manifest.json"
    write_manifest(path, tmp_path, _records())
    doc = json.loads(path.read_text())
    assert doc["schema"] == 1
    assert doc["images"][1] == {
        "path": "screens/ITD_RESS_013.png", "target": "ITD_RESS_013.png",
        "kind": "screen", "pak": "ITD_RESS", "entry": 13, "floor": None,
        "size": [320, 200], "sha256": "b" * 64,
    }


def test_read_manifest_rejects_other_schema_and_missing_keys(tmp_path):
    path = tmp_path / "manifest.json"
    path.write_text(json.dumps({"schema": 99, "game": "aitd1", "data_dir": "", "palette": {}, "images": []}))
    with pytest.raises(ManifestError, match="schema"):
        read_manifest(path)
    path.write_text(json.dumps({"schema": 1, "game": "aitd1", "data_dir": "", "palette": {},
                                "images": [{"path": "x"}]}))
    with pytest.raises(ManifestError):
        read_manifest(path)
    path.write_text("{not json")
    with pytest.raises(ManifestError):
        read_manifest(path)


def test_sha256_rgb_is_encoder_independent(tmp_path):
    pixels = (np.arange(200 * 320 * 3) % 253).astype(np.uint8).reshape(200, 320, 3)
    direct = sha256_rgb(pixels)
    out = tmp_path / "x.png"
    Image.fromarray(pixels).save(out, compress_level=0)
    with Image.open(out) as im:
        assert sha256_rgb(np.asarray(im)) == direct
    assert sha256_rgb(pixels[:, ::-1]) != direct
