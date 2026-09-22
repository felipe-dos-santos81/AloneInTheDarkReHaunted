"""Runs only when the GOG/Steam INDARK folder is present under data/aitd1.
Pure-Python explode of 157 plates takes about a minute."""
import pathlib

import numpy as np
import pytest
from PIL import Image

from aitd_textures.decode import DataNotFound, find_data_dir
from aitd_textures.export import export_all
from aitd_textures.importer import run_import
from aitd_textures.manifest import MANIFEST_NAME, read_manifest

ROOT = pathlib.Path(__file__).resolve().parents[2]
ENGINE_DUMP = ROOT / "TatouSource/build/macos-arm64/Fitd/Tatou.app/Contents/Resources/backgrounds_dump"


def _quiet(*_args, **_kwargs):
    pass


@pytest.fixture(scope="module")
def real_export(tmp_path_factory):
    try:
        data_dir = find_data_dir(ROOT / "data" / "aitd1")
    except DataNotFound:
        pytest.skip("no AITD1 game data under data/aitd1")
    out = tmp_path_factory.mktemp("textures")
    return out, export_all(data_dir, out, log=_quiet)


def test_real_export_counts(real_export):
    _out, result = real_export
    assert result.skipped == []
    assert (result.cameras, result.screens) == (144, 13)


@pytest.mark.parametrize("rel, dump_name", [
    ("backgrounds/CAMERA00_000.png", "CAMERA00_F00_C00.png"),
    ("backgrounds/CAMERA02_007.png", "CAMERA02_F02_C07.png"),
    ("backgrounds/CAMERA05_010.png", "CAMERA05_F05_C10.png"),
    ("screens/ITD_RESS_006.png", "ITD_RESS_0006.png"),
])
def test_real_export_matches_the_engine_dump(real_export, rel, dump_name):
    if not (ENGINE_DUMP / dump_name).is_file():
        pytest.skip("engine backgrounds_dump not present in the build tree")
    out, _result = real_export
    with Image.open(out / rel) as ours, Image.open(ENGINE_DUMP / dump_name) as theirs:
        assert np.array_equal(np.asarray(ours), np.asarray(theirs.convert("RGB")))


def test_real_originals_are_reported_unchanged(real_export, tmp_path):
    out, _result = real_export
    manifest = read_manifest(out / MANIFEST_NAME)
    result = run_import(out, tmp_path / "dest", manifest, dry_run=True, log=_quiet)
    assert result.errors == []
    assert result.imported == []
    assert len(result.skipped) == 157
