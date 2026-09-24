import json

import numpy as np
import pytest
from PIL import Image

from aitd_models.export import VIEWS, export_models
from aitd_models.gltf import read_glb
from aitd_models.manifest import MANIFEST_NAME, ManifestError, read_manifest
from aitd_textures.pak import PakError
from model_helpers import write_model_data_dir


def run(tmp_path, **kwargs):
    data = write_model_data_dir(tmp_path / "INDARK")
    lines = []
    result = export_models(data, tmp_path / "models", lines.append, size=32, ssaa=1, **kwargs)
    return result, tmp_path / "models", lines


def test_exports_canonical_bodies_once(tmp_path):
    result, out, _lines = run(tmp_path)
    keys = [r.key for r in result.records]
    assert keys == ["LISTBODY_000", "LISTBODY_002", "LISTBODY_005", "LISTBOD2_000", "LISTBOD2_001"]
    assert result.written == ["LISTBODY_000", "LISTBOD2_001"]
    assert sorted(p.name for p in (out / "bodies").iterdir()) == ["LISTBOD2_001", "LISTBODY_000"]
    by_key = {r.key: r for r in result.records}
    assert by_key["LISTBOD2_000"].canonical == "LISTBODY_000"
    assert by_key["LISTBODY_000"].aliases == ["LISTBODY_002", "LISTBOD2_000"]
    assert by_key["LISTBOD2_001"].skeleton_siblings == ["LISTBODY_000", "LISTBODY_002", "LISTBODY_005", "LISTBOD2_000"]
    assert by_key["LISTBODY_005"].kind == "skip"
    assert by_key["LISTBODY_000"].kind == "prop"  # 4 groups: fewer than 6


def test_skips_broken_entries_and_keeps_going(tmp_path):
    result, _out, lines = run(tmp_path)
    assert [s.split(":")[0] for s in result.skipped] == ["LISTBODY entry 3", "LISTBODY entry 4"]
    assert "pivot vertex 5 is not in parent group 0" in result.skipped[1]
    assert any(line.startswith("warning: LISTANIM entry 2") for line in lines)


def test_writes_the_folder_contents(tmp_path):
    result, out, _lines = run(tmp_path)
    folder = out / "bodies" / "LISTBODY_000"
    glb = read_glb((folder / "original.glb").read_bytes())
    assert [a["name"] for a in glb.doc["animations"]] == ["LISTANIM_000"]
    for view in VIEWS:
        with Image.open(folder / "reference" / f"{view.name}.png") as im:
            assert (im.mode, im.size) == ("RGBA", (32, 32))
            assert np.asarray(im)[..., 3].max() == 255
    views = json.loads((folder / "reference" / "views.json").read_text())
    assert [v["yaw_deg"] for v in views["views"]] == [0.0, 45.0, 90.0, 180.0]
    assert views["framing"]["size"] == 32


def test_manifest_round_trips(tmp_path):
    result, out, _lines = run(tmp_path)
    doc, records = read_manifest(out / MANIFEST_NAME)
    assert records == result.records
    assert doc["contract"] == 1 and doc["bind_pose"] == "rest"
    record = records[0]
    assert record.target == "body_LISTBODY_000.glb" and record.dir == "bodies/LISTBODY_000"
    assert record.preview_anims == ["LISTANIM_000"]
    assert record.groups[1] == {"parent": 0, "pivot_rest": [0.0, -100.0, 0.0], "count": 2}
    assert record.height == 400.0 and record.triangles == 331


def test_only_rewrites_the_named_bodies_and_resolves_aliases(tmp_path):
    result, out, lines = run(tmp_path, only={"LISTBOD2_000"})
    assert result.written == ["LISTBODY_000"]
    assert "LISTBOD2_000 is an alias of LISTBODY_000; exporting LISTBODY_000" in lines
    assert len(result.records) == 5  # the manifest still lists everything


def test_unknown_keys_fail_before_writing(tmp_path):
    with pytest.raises(ValueError, match="unknown body key"):
        run(tmp_path, only={"LISTBODY_999"})
    assert not (tmp_path / "models").exists()


def test_missing_pak_is_an_error(tmp_path):
    data = write_model_data_dir(tmp_path / "INDARK")
    (data / "LISTBOD2.PAK").unlink()
    with pytest.raises(PakError, match="LISTBOD2.PAK"):
        export_models(data, tmp_path / "models", lambda _m: None, size=32, ssaa=1)


def test_a_foreign_manifest_is_rejected(tmp_path):
    path = tmp_path / MANIFEST_NAME
    path.write_text('{"schema": 99}')
    with pytest.raises(ManifestError, match="schema 99"):
        read_manifest(path)
