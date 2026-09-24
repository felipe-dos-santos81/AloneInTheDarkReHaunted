import pathlib
import re

from aitd_models.cos_table import COS_TABLE

ROOT = pathlib.Path(__file__).resolve().parents[2]
ENGINE_TABLE = ROOT / "TatouSource" / "FitdLib" / "cosTable.cpp"


def engine_table():
    body = ENGINE_TABLE.read_text().split("cosTable[]", 1)[1].split("{", 1)[1].split("}", 1)[0]
    return [int(v) for v in re.findall(r"-?\d+", body)]


def test_table_is_the_engines():
    assert list(COS_TABLE) == engine_table()


def test_table_is_a_sine_with_the_engine_quirk_at_zero():
    assert len(COS_TABLE) == 1024
    assert COS_TABLE[0] == 4
    assert COS_TABLE[256] == 32767
    assert COS_TABLE[768] == -32767
