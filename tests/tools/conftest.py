"""Test fixtures for the texture tools. Puts tools/ on sys.path so the
package imports without installation."""
import pathlib
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

import helpers  # noqa: E402  (tests/tools/ is on sys.path: no __init__.py here)


@pytest.fixture
def pak_bytes():
    return helpers.pak_bytes
