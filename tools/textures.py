#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Export original AITD1 textures and import upscaled replacements.

    tools/textures.py export [--data DIR] [--out DIR] [--anims DIR]
    tools/textures.py import [--src DIR] [--dest DIR] [--originals DIR]
                             [--dark mirror|all|none] [--dark-factor F] [--dry-run]

Defaults are relative to the repository root: data/aitd1, data/textures,
data/textures-ai and Assets/backgrounds_hd (also where export finds the
engine's anim_<NAME>/ clips).
"""
from __future__ import annotations

import argparse
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from aitd_textures.decode import DataNotFound, find_data_dir  # noqa: E402
from aitd_textures.export import export_all  # noqa: E402
from aitd_textures.importer import DARK_POLICIES, DEFAULT_DARK_FACTOR, ImportResult, run_import  # noqa: E402
from aitd_textures.manifest import MANIFEST_NAME, ManifestError, read_manifest  # noqa: E402
from aitd_textures.pak import PakError  # noqa: E402

EXIT_OK = 0
EXIT_FINDINGS = 1
EXIT_USAGE = 2
DEFAULTS = {
    "data": "data/aitd1",
    "textures": "data/textures",
    "textures_ai": "data/textures-ai",
    "dest": "Assets/backgrounds_hd",
    "anims": "Assets/backgrounds_hd",
}


def default_root() -> pathlib.Path:
    return HERE.parent


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="textures.py", description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    exp = sub.add_parser("export", help="write the original 320x200 plates and screens as PNG")
    exp.add_argument("--data", type=pathlib.Path, help=f"INDARK folder or any folder above it (default {DEFAULTS['data']})")
    exp.add_argument("--out", type=pathlib.Path, help=f"output folder (default {DEFAULTS['textures']})")
    exp.add_argument("--anims", type=pathlib.Path,
                     help=f"folder holding the engine's anim_<NAME>/ clips (default {DEFAULTS['anims']})")

    imp = sub.add_parser("import", help="validate upscaled PNGs and copy them into backgrounds_hd")
    imp.add_argument("--src", type=pathlib.Path, help=f"upscaled tree (default {DEFAULTS['textures_ai']})")
    imp.add_argument("--dest", type=pathlib.Path, help=f"engine HD folder (default {DEFAULTS['dest']})")
    imp.add_argument("--originals", type=pathlib.Path, help=f"export folder holding {MANIFEST_NAME} (default {DEFAULTS['textures']})")
    imp.add_argument("--dark", choices=DARK_POLICIES, default="mirror",
                     help="derive _DARK variants: mirror existing ones (default), all cameras, or none")
    imp.add_argument("--dark-factor", type=float, default=DEFAULT_DARK_FACTOR,
                     help=f"brightness multiplier for _DARK variants (default {DEFAULT_DARK_FACTOR})")
    imp.add_argument("--dry-run", action="store_true", help="check everything, write nothing")
    return parser


def cmd_export(args, root: pathlib.Path, log) -> int:
    try:
        data_dir = find_data_dir(args.data if args.data is not None else root / DEFAULTS["data"])
    except (DataNotFound, PakError, ValueError, OSError) as exc:
        log(f"error: {exc}")
        return EXIT_USAGE
    out = args.out if args.out is not None else root / DEFAULTS["textures"]
    anims = args.anims if args.anims is not None else root / DEFAULTS["anims"]
    try:
        result = export_all(data_dir, out, log, anims)
    except (DataNotFound, PakError, ValueError, OSError) as exc:
        log(f"error: {exc}")
        return EXIT_USAGE
    return EXIT_FINDINGS if result.skipped else EXIT_OK


def summarize(result: ImportResult, dest: pathlib.Path, dry_run: bool, log) -> None:
    by_kind: dict[str, int] = {}
    for f in result.errors:
        by_kind[f.kind] = by_kind.get(f.kind, 0) + 1
    label = "would import" if dry_run else "imported"
    log(f"{'dry run: ' if dry_run else ''}{label} {len(result.imported)} file(s) into {dest}")
    log(f"  dark variants derived: {len(result.dark)}")
    log(f"  skipped (identical to original): {len(result.skipped)}")
    log(f"  not replaced (destination keeps its current file): {len(result.not_replaced)}")
    log(f"  errors: {sum(by_kind.values())}" + (" (" + ", ".join(f"{k} {v}" for k, v in sorted(by_kind.items())) + ")" if by_kind else ""))


def cmd_import(args, root: pathlib.Path, log) -> int:
    default_src = root / DEFAULTS["textures_ai"]
    src = args.src if args.src is not None else default_src
    if not src.is_dir():
        if args.src is None or src.resolve() == default_src.resolve():
            log(f"nothing to import: {src} does not exist")
            return EXIT_OK
        log(f"error: {src} is not a directory")
        return EXIT_USAGE
    dest = args.dest if args.dest is not None else root / DEFAULTS["dest"]
    originals = args.originals if args.originals is not None else root / DEFAULTS["textures"]
    manifest = None
    manifest_path = originals / MANIFEST_NAME
    if manifest_path.is_file():
        try:
            manifest = read_manifest(manifest_path)
        except ManifestError as exc:
            log(f"error: {exc}")
            return EXIT_USAGE
    result = run_import(src, dest, manifest, args.dark, args.dark_factor, args.dry_run, log)
    summarize(result, dest, args.dry_run, log)
    return EXIT_FINDINGS if result.errors else EXIT_OK


def main(argv=None, root: pathlib.Path | None = None, log=print) -> int:
    args = build_parser().parse_args(argv)
    root = pathlib.Path(root) if root is not None else default_root()
    if args.command == "export":
        return cmd_export(args, root, log)
    return cmd_import(args, root, log)


if __name__ == "__main__":
    sys.exit(main())
