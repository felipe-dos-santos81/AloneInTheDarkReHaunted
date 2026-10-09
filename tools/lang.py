#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Extract the AITD1 language text to UTF-8 files, and pack a translation.

    tools/lang.py extract [--data DIR] [--out DIR]
    tools/lang.py pack    [--data DIR] [--src DIR] [--pak FILE] [--cpp FILE]

extract writes <out>/en and <out>/fr (default data/lang): messages.txt and
doc01.txt ... doc21.txt, one per PAK entry. pack encodes <src> (default
Assets/lang/pt-BR), checks it against ENGLISH.PAK (message numbers, image
codes, characters the bitmap font draws, message widths not listed in
width-ok.txt) and writes the embedded source (default
TatouSource/FitdLib/embedded/embedded_PORTUGUE_PAK.cpp) and, with --pak, a
PORTUGUE.PAK. Exit codes: 0 done, 1 problems found, 2 usage or data error.
"""
from __future__ import annotations

import argparse
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from aitd_data import lang  # noqa: E402
from aitd_data.decode import DataNotFound, find_data_dir  # noqa: E402
from aitd_data.files import atomic_write_bytes  # noqa: E402
from aitd_data.pak import Pak, PakError, pak_image  # noqa: E402

ROOT = HERE.parent
DEFAULTS = {"data": "data/aitd1", "out": "data/lang", "src": "Assets/lang/pt-BR",
            "cpp": "TatouSource/FitdLib/embedded/embedded_PORTUGUE_PAK.cpp"}


def _entries(pak: Pak) -> list[bytes]:
    return [pak.read(i) for i in range(lang.ENTRY_COUNT)]


def cmd_extract(args) -> int:
    data = find_data_dir(args.data or ROOT / DEFAULTS["data"])
    out = args.out or ROOT / DEFAULTS["out"]
    for code, pak in (("en", "ENGLISH"), ("fr", "FRANCAIS")):
        for i, entry in enumerate(_entries(Pak(data / f"{pak}.PAK"))):
            atomic_write_bytes(out / code / lang.entry_file(i), lang.decode(entry).encode("utf-8"))
    print(f"extracted English and French to {out}")
    return 0


def cmd_pack(args) -> int:
    data = find_data_dir(args.data or ROOT / DEFAULTS["data"])
    src = args.src or ROOT / DEFAULTS["src"]
    texts = [(src / lang.entry_file(i)).read_text(encoding="utf-8") for i in range(lang.ENTRY_COUNT)]
    problems = []
    for i, text in enumerate(texts):
        problems += [f"{lang.entry_file(i)}:{n}: the bitmap font cannot draw {ch!r}"
                     for n, ch in lang.bad_chars(text)]
    if problems:
        print("\n".join(problems))
        return 1
    pt = [lang.encode(t) for t in texts]
    en = _entries(Pak(data / "ENGLISH.PAK"))
    problems = lang.validate(pt, en)
    widths = lang.font_widths(Pak(data / "ITD_RESS.PAK").read(lang.FONT_ENTRY))
    ok_file = src / "width-ok.txt"
    allowed = {int(n) for n in ok_file.read_text().split()} if ok_file.is_file() else set()
    problems += [f"messages.txt: @{n} is wider than the English; shorten it, or list {n} in "
                 f"width-ok.txt once checked in game" for n in lang.too_wide(pt[0], en[0], widths, allowed)]
    if problems:
        print("\n".join(problems))
        return 1
    image = pak_image(pt)
    atomic_write_bytes(args.cpp or ROOT / DEFAULTS["cpp"], lang.embedded_cpp("PORTUGUE", image).encode())
    if args.pak:
        atomic_write_bytes(args.pak, image)
    print(f"packed {src}: {len(image)} bytes")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="lang.py", description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    data = argparse.ArgumentParser(add_help=False)
    data.add_argument("--data", type=pathlib.Path, help=f"INDARK folder or any folder above it (default {DEFAULTS['data']})")
    sub = parser.add_subparsers(dest="command", required=True)
    ext = sub.add_parser("extract", parents=[data], help="write the English and French text as UTF-8 files")
    ext.add_argument("--out", type=pathlib.Path, help=f"output folder (default {DEFAULTS['out']})")
    pck = sub.add_parser("pack", parents=[data], help="check and pack the Portuguese text")
    pck.add_argument("--src", type=pathlib.Path, help=f"translation folder (default {DEFAULTS['src']})")
    pck.add_argument("--cpp", type=pathlib.Path, help=f"embedded source to write (default {DEFAULTS['cpp']})")
    pck.add_argument("--pak", type=pathlib.Path, help="also write a PORTUGUE.PAK here")
    return parser


def main(argv=None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return cmd_extract(args) if args.command == "extract" else cmd_pack(args)
    except (DataNotFound, PakError, FileNotFoundError, UnicodeError) as err:
        print(f"error: {err}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
