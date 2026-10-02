#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Export the original AITD1 animated bodies for the image-to-3D generator,
and import what it delivers.

    tools/models.py export   [--data DIR] [--out DIR] [--bodies KEY[,KEY...]]
                             [--size PX] [--ssaa N]
    tools/models.py identity [--models DIR] [--out DIR] --bodies KEY[,KEY...]
    tools/models.py import   [--data DIR] [--models DIR] [--src DIR] [--dest DIR]
                             [--debug DIR] [--report DIR] [--bodies KEY[,KEY...]] [--dry-run]

Defaults are relative to the repository root: data/aitd1, data/models,
data/models-ai, Assets/models_hd and data/models-hd-debug; `identity` writes
data/models-identity. Keys look like LISTBODY_011; an alias exports its
canonical body. `identity` turns exported original.glb files into contract
deliveries (the import's end-to-end oracle). `import` without --data reads
the bodies and the palette the export carries (body.bin, palette.bin), and
the game data only for an export written before it did. Exit codes: 0 done, 1 some
bodies were skipped or failed, 2 usage or data error.
"""
from __future__ import annotations

import argparse
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from aitd_models.export import PALETTE_NAME, REFERENCE_SIZE, export_models  # noqa: E402
from aitd_models.gltf import GltfError  # noqa: E402
from aitd_models.identity import identity_glb  # noqa: E402
from aitd_models.importer import DELIVERY_NAME, ImportPaths, run_import  # noqa: E402
from aitd_models.manifest import MANIFEST_NAME, ManifestError, read_manifest  # noqa: E402
from aitd_textures.files import atomic_write_bytes  # noqa: E402
from aitd_textures.decode import DataNotFound, find_data_dir  # noqa: E402
from aitd_textures.pak import PakError  # noqa: E402

EXIT_OK = 0
EXIT_FINDINGS = 1
EXIT_USAGE = 2
DEFAULTS = {"data": "data/aitd1", "models": "data/models", "models_ai": "data/models-ai",
            "identity": "data/models-identity", "dest": "Assets/models_hd", "debug": "data/models-hd-debug"}


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

    ide = sub.add_parser("identity", help="turn exported original.glb files into contract deliveries")
    ide.add_argument("--models", type=pathlib.Path, help=f"export folder (default {DEFAULTS['models']})")
    ide.add_argument("--out", type=pathlib.Path, help=f"delivery tree to write (default {DEFAULTS['identity']})")
    ide.add_argument("--bodies", required=True, help="comma-separated canonical keys, e.g. LISTBODY_011")

    imp = sub.add_parser("import", help="align, bind and pack deliveries into body_<KEY>.hdm")
    imp.add_argument("--data", type=pathlib.Path,
                     help=f"INDARK folder or any folder above it (default: the export's own {PALETTE_NAME} "
                          f"and body.bin files, else {DEFAULTS['data']})")
    imp.add_argument("--models", type=pathlib.Path, help=f"export folder holding {MANIFEST_NAME} (default {DEFAULTS['models']})")
    imp.add_argument("--src", type=pathlib.Path, help=f"delivery tree (default {DEFAULTS['models_ai']})")
    imp.add_argument("--dest", type=pathlib.Path, help=f"engine folder (default {DEFAULTS['dest']})")
    imp.add_argument("--debug", type=pathlib.Path, help=f"debug .glb and report folder (default {DEFAULTS['debug']})")
    imp.add_argument("--bodies", help="comma-separated keys to import (default: every delivery)")
    imp.add_argument("--report", type=pathlib.Path,
                     help="--debug still gets the .glb, --report takes the body_<KEY>.json reports "
                          "here instead of beside it, even with --dry-run")
    imp.add_argument("--dry-run", action="store_true", help="check everything, write nothing but --report")
    return parser


def _parse_keys(text: str | None) -> set[str] | None:
    return {k.strip() for k in text.split(",") if k.strip()} if text else None


def cmd_export(args, root: pathlib.Path, log) -> int:
    if args.size < 16 or not 1 <= args.ssaa <= 8:
        log("error: --size must be at least 16 and --ssaa between 1 and 8")
        return EXIT_USAGE
    only = _parse_keys(args.bodies)
    try:
        data_dir = find_data_dir(args.data if args.data is not None else root / DEFAULTS["data"])
        out = args.out if args.out is not None else root / DEFAULTS["models"]
        result = export_models(data_dir, out, log, only=only, ssaa=args.ssaa, size=args.size)
    except (DataNotFound, PakError, ValueError, OSError) as exc:
        log(f"error: {exc}")
        return EXIT_USAGE
    return EXIT_FINDINGS if result.skipped else EXIT_OK


def _manifest(models: pathlib.Path, log):
    try:
        return read_manifest(models / MANIFEST_NAME)[1]
    except ManifestError as exc:
        log(f"error: {exc} (run make export-models first)")
        return None


def cmd_identity(args, root: pathlib.Path, log) -> int:
    models = args.models if args.models is not None else root / DEFAULTS["models"]
    out = args.out if args.out is not None else root / DEFAULTS["identity"]
    records = _manifest(models, log)
    if records is None:
        return EXIT_USAGE
    by_key = {r.key: r for r in records}
    keys = sorted(_parse_keys(args.bodies) or ())
    bad = [k for k in keys if k not in by_key or not by_key[k].has_export_folder]
    if bad:
        log(f"error: not exported canonical bodies: {', '.join(bad)}")
        return EXIT_USAGE
    failed = 0
    for key in keys:
        try:
            data = identity_glb((models / by_key[key].dir / "original.glb").read_bytes())
        except (OSError, GltfError) as exc:
            log(f"error: {key}: {exc}")
            failed += 1
            continue
        atomic_write_bytes(out / "bodies" / key / DELIVERY_NAME, data)
        log(f"wrote {out / 'bodies' / key / DELIVERY_NAME}")
    return EXIT_FINDINGS if failed else EXIT_OK


def cmd_import(args, root: pathlib.Path, log) -> int:
    default_src = root / DEFAULTS["models_ai"]
    src = args.src if args.src is not None else default_src
    if not src.is_dir():
        if src.resolve() == default_src.resolve():
            log(f"nothing to import: {src} does not exist")
            return EXIT_OK
        log(f"error: {src} is not a directory")
        return EXIT_USAGE
    models = args.models if args.models is not None else root / DEFAULTS["models"]
    records = _manifest(models, log)
    if records is None:
        return EXIT_USAGE
    only = _parse_keys(args.bodies)
    canonical_of = {r.key: r.canonical for r in records}
    unknown = sorted((only or set()) - set(canonical_of))
    if unknown:
        log(f"error: unknown body key(s): {', '.join(unknown)}")
        return EXIT_USAGE
    if only is not None:
        for key in sorted(only):
            if canonical_of[key] != key:
                log(f"{key} is an alias of {canonical_of[key]}; importing {canonical_of[key]}")
        only = {canonical_of[key] for key in only}
    try:
        if args.data is None and (models / PALETTE_NAME).is_file():
            data_dir = None
        else:
            data_dir = find_data_dir(args.data if args.data is not None else root / DEFAULTS["data"])
        paths = ImportPaths(data_dir, models, src, args.dest if args.dest is not None else root / DEFAULTS["dest"],
                            args.debug if args.debug is not None else root / DEFAULTS["debug"], args.report)
        result = run_import(paths, records, only, args.dry_run, log)
    except (DataNotFound, PakError, OSError) as exc:
        log(f"error: {exc}")
        return EXIT_USAGE
    label = "would import" if args.dry_run else "imported"
    log(f"{'dry run: ' if args.dry_run else ''}{label} {len(result.imported)} bodies "
        f"({len(result.written)} files), {len(result.failed)} failed")
    return EXIT_FINDINGS if result.failed else EXIT_OK


def main(argv=None, root: pathlib.Path | None = None, log=print) -> int:
    args = build_parser().parse_args(argv)
    root = pathlib.Path(root) if root is not None else default_root()
    command = {"export": cmd_export, "identity": cmd_identity, "import": cmd_import}[args.command]
    return command(args, root, log)


if __name__ == "__main__":
    sys.exit(main())
