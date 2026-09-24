#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Export the original AITD1 animated bodies for the image-to-3D generator.

    tools/models.py export [--data DIR] [--out DIR] [--bodies KEY[,KEY...]]
                           [--size PX] [--ssaa N]

Defaults are relative to the repository root: data/aitd1 and data/models.
Keys look like LISTBODY_011; an alias exports its canonical body. Exit codes:
0 done, 1 some bodies were skipped, 2 usage or data error.
"""
from __future__ import annotations

import argparse
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from aitd_models.export import REFERENCE_SIZE, export_models  # noqa: E402
from aitd_textures.decode import DataNotFound, find_data_dir  # noqa: E402
from aitd_textures.pak import PakError  # noqa: E402

EXIT_OK = 0
EXIT_FINDINGS = 1
EXIT_USAGE = 2
DEFAULTS = {"data": "data/aitd1", "models": "data/models"}


def default_root() -> pathlib.Path:
    return HERE.parent


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="models.py", description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    exp = sub.add_parser("export", help="write original.glb, reference renders and manifest.json per body")
    exp.add_argument("--data", type=pathlib.Path, help=f"INDARK folder or any folder above it (default {DEFAULTS['data']})")
    exp.add_argument("--out", type=pathlib.Path, help=f"output folder (default {DEFAULTS['models']})")
    exp.add_argument("--bodies", help="comma-separated keys to (re)write, e.g. LISTBODY_011 (default: all)")
    exp.add_argument("--size", type=int, default=REFERENCE_SIZE, help=f"reference render size in pixels (default {REFERENCE_SIZE})")
    exp.add_argument("--ssaa", type=int, default=4, help="supersampling factor per axis (default 4)")
    return parser


def cmd_export(args, root: pathlib.Path, log) -> int:
    if args.size < 16 or not 1 <= args.ssaa <= 8:
        log("error: --size must be at least 16 and --ssaa between 1 and 8")
        return EXIT_USAGE
    only = {k.strip() for k in args.bodies.split(",") if k.strip()} if args.bodies else None
    try:
        data_dir = find_data_dir(args.data if args.data is not None else root / DEFAULTS["data"])
        out = args.out if args.out is not None else root / DEFAULTS["models"]
        result = export_models(data_dir, out, log, only=only, ssaa=args.ssaa, size=args.size)
    except (DataNotFound, PakError, ValueError, OSError) as exc:
        log(f"error: {exc}")
        return EXIT_USAGE
    return EXIT_FINDINGS if result.skipped else EXIT_OK


def main(argv=None, root: pathlib.Path | None = None, log=print) -> int:
    args = build_parser().parse_args(argv)
    root = pathlib.Path(root) if root is not None else default_root()
    return cmd_export(args, root, log)


if __name__ == "__main__":
    sys.exit(main())
