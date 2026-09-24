# SPDX-License-Identifier: GPL-2.0-only
"""Export every animated AITD1 body for the image-to-3D generator:
bodies/<KEY>/original.glb, reference/<view>.png, reference/views.json, and
manifest.json. Alias groups (byte-identical bodies) are exported once, under
their canonical key: the first in HQR order (LISTBODY before LISTBOD2), then
by entry index."""
from __future__ import annotations

import hashlib
import io
import json
import pathlib
from collections import defaultdict
from dataclasses import dataclass, field

import numpy as np
from PIL import Image

from aitd_textures.catalog import PALETTE_ENTRY, PALETTE_PAK
from aitd_textures.decode import decode_palette
from aitd_textures.explode import ExplodeError
from aitd_textures.files import atomic_write_bytes
from aitd_textures.pak import Pak, PakError

from .body import Animation, BodyError, parse_anim, parse_body
from .manifest import MANIFEST_NAME, BodyRecord, write_manifest
from .mesh import build_mesh
from .original import build_original_glb
from .pose import pose_float, rest_states, skin
from .raster import View, framing_for, render
from .skeleton import skeleton_hash, validate

# Each body HQR with the animation HQR the engine pairs it with (vars.cpp listBodySelect/listAnimSelect).
BODY_PAKS = (("LISTBODY", "LISTANIM"), ("LISTBOD2", "LISTANI2"))
VIEWS = (View("front", 0.0), View("three_quarter", 45.0), View("side", 90.0), View("back", 180.0))
REFERENCE_SIZE = 1024
PREVIEW_ANIMS = 3
CHARACTER_MIN_GROUPS = 6
PRIM_NAMES = {0: "line", 1: "poly", 2: "point", 3: "sphere", 6: "big_point", 7: "zixel",
              8: "poly_tex", 9: "poly_tex", 10: "poly_tex"}


@dataclass
class ExportResult:
    records: list[BodyRecord] = field(default_factory=list)
    written: list[str] = field(default_factory=list)   # canonical keys whose folder was written
    skipped: list[str] = field(default_factory=list)   # "<HQR> entry <n>: <reason>"


def _png_rgba(pixels: np.ndarray) -> bytes:
    buf = io.BytesIO()
    Image.fromarray(np.ascontiguousarray(pixels, dtype=np.uint8)).save(buf, format="PNG")
    return buf.getvalue()


def _load_anims(pak: Pak, hqr: str, log) -> dict[str, Animation]:
    anims = {}
    for i in range(pak.count):
        try:
            anims[f"{hqr}_{i:03d}"] = parse_anim(pak.read(i))
        except (PakError, ExplodeError, BodyError) as exc:
            log(f"warning: {hqr} entry {i}: {exc}")
    return anims


def export_models(data_dir, out_dir, log=print, only: set[str] | None = None,
                  ssaa: int = 4, size: int = REFERENCE_SIZE) -> ExportResult:
    """Export every animated body under `data_dir` into `out_dir`.

    `only` limits which folders are (re)written: any keys, each resolved to
    its canonical key; the manifest always lists every body. Raises PakError
    when a required PAK is missing, and ValueError for an unknown key in
    `only` (before writing anything)."""
    data_dir, out_dir = pathlib.Path(data_dir), pathlib.Path(out_dir)
    palette = decode_palette(Pak(data_dir / f"{PALETTE_PAK}.PAK").read(PALETTE_ENTRY))
    result = ExportResult()
    found = []  # (key, hqr, index, raw, body, anims)
    for hqr, anim_hqr in BODY_PAKS:
        pak = Pak(data_dir / f"{hqr}.PAK")
        anims = _load_anims(Pak(data_dir / f"{anim_hqr}.PAK"), anim_hqr, log)
        for i in range(pak.count):
            try:
                raw = pak.read(i)
                body = parse_body(raw)
            except (PakError, ExplodeError, BodyError) as exc:
                result.skipped.append(f"{hqr} entry {i}: {exc}")
                continue
            if not body.animated:
                continue
            problems = validate(body)
            if problems:
                result.skipped.append(f"{hqr} entry {i}: {'; '.join(problems)}")
                continue
            found.append((f"{hqr}_{i:03d}", hqr, i, raw, body, anims))

    # found is in HQR order (LISTBODY first), so the first key of a group is canonical.
    by_sha, by_skeleton = defaultdict(list), defaultdict(list)
    for key, _hqr, _i, raw, body, _anims in found:
        by_sha[hashlib.sha256(raw).hexdigest()].append(key)
        by_skeleton[skeleton_hash(body)].append(key)

    canonical_of = {key: by_sha[hashlib.sha256(raw).hexdigest()][0] for key, _h, _i, raw, _b, _a in found}
    if only is not None:
        unknown = sorted(set(only) - set(canonical_of))
        if unknown:
            raise ValueError(f"unknown body key(s): {', '.join(unknown)} (animated bodies are named like LISTBODY_011)")
        for key in sorted(only):
            if canonical_of[key] != key:
                log(f"{key} is an alias of {canonical_of[key]}; exporting {canonical_of[key]}")
        only = {canonical_of[key] for key in only}

    for key, hqr, index, raw, body, anims in found:
        sha = hashlib.sha256(raw).hexdigest()
        shash = skeleton_hash(body)
        rest = pose_float(body, rest_states(body))
        posed = skin(body, rest.group_matrices())
        mesh = build_mesh(body, posed, palette)
        preview = [name for name, a in sorted(anims.items()) if a.num_groups == len(body.groups)][:PREVIEW_ANIMS]
        counts: dict[str, int] = defaultdict(int)
        for prim in body.primitives:
            counts[PRIM_NAMES.get(prim.type, str(prim.type))] += 1
        kind = "skip" if mesh.triangle_count == 0 else (
            "character" if len(body.groups) >= CHARACTER_MIN_GROUPS else "prop")
        record = BodyRecord(
            key=key, hqr=hqr, body=index, canonical=by_sha[sha][0],
            target=f"body_{key}.glb", kind=kind, body_sha256=sha, skeleton_hash=shash,
            zv=list(body.zv), height=float(np.ptp(posed[:, 1])), triangles=mesh.triangle_count,
            prim_counts=dict(sorted(counts.items())),
            groups=[{"parent": g.parent, "pivot_rest": [float(c) for c in rest.joints[gi][:3, 3]], "count": g.count}
                    for gi, g in enumerate(body.groups)],
            preview_anims=preview,
            aliases=[k for k in by_sha[sha] if k != key],
            skeleton_siblings=[k for k in by_skeleton[shash] if k not in by_sha[sha]])
        result.records.append(record)
        if not record.is_canonical or kind == "skip" or (only is not None and key not in only):
            continue
        folder = out_dir / record.dir
        atomic_write_bytes(folder / "original.glb",
                           build_original_glb(body, palette, [(n, anims[n]) for n in preview]))
        framing = framing_for(mesh.positions, size)
        for view in VIEWS:
            atomic_write_bytes(folder / "reference" / f"{view.name}.png",
                               _png_rgba(render(mesh, view, framing, ssaa)))
        views_doc = {"framing": framing.to_json(),
                     "views": [{"name": v.name, "yaw_deg": v.yaw_deg, "file": f"{v.name}.png",
                                "basis_right_down_forward": v.basis().tolist()} for v in VIEWS]}
        atomic_write_bytes(folder / "reference" / "views.json", (json.dumps(views_doc, indent=1) + "\n").encode())
        result.written.append(key)
        log(f"exported {key} ({kind}, {len(body.groups)} groups, {mesh.triangle_count} triangles)")

    for reason in result.skipped:
        log(f"warning: skipped {reason}")
    write_manifest(out_dir / MANIFEST_NAME, data_dir, result.records,
                   [{"name": v.name, "yaw_deg": v.yaw_deg} for v in VIEWS], size)
    canonical = sum(1 for r in result.records if r.is_canonical and r.kind != "skip")
    log(f"exported {len(result.written)} of {canonical} canonical bodies "
        f"({len(result.records)} animated bodies) to {out_dir}"
        + (f", {len(result.skipped)} skipped" if result.skipped else ""))
    return result
