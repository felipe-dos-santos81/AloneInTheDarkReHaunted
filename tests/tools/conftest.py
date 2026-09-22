"""Test fixtures for the texture tools. Puts tools/ on sys.path so the
package imports without installation."""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))
