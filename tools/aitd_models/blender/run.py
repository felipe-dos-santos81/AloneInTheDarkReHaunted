# SPDX-License-Identifier: GPL-2.0-only
"""`tools/models.py blender`: every canonical character body through
prepare (remaster.py), the headless Blender stage (stage.py) and finish,
into the delivery tree, with a run.md line per body. A body that fails
fails alone."""
from __future__ import annotations

import json
import pathlib
import subprocess
import time
from dataclasses import dataclass
from typing import Callable

import numpy as np

from aitd_textures.decode import decode_palette
from aitd_textures.files import atomic_write_bytes

from ..body import parse_body
from ..export import PALETTE_NAME
from ..manifest import BodyRecord
from ..original import rest_mesh
from .remaster import atlas_paths, corner_arrays, finish_glb, levels, read_edits

HERE = pathlib.Path(__file__).resolve().parent
STAGE = HERE / "stage.py"
EDITS = HERE / "edits"
ATLASES = HERE.parents[2] / "Assets" / "atlases"
DELIVERY = "model.glb"
STAGE_SETTINGS = {"merge_distance": 0.0005, "crease_angle": 1.0, "cage": 0.02, "ray": 0.06,
                  "bake_size": 2048, "samples_emit": 8, "samples_ao": 64}
STAGE_TIMEOUT_S = 900

Stage = Callable[[pathlib.Path], None]


@dataclass
class BodyRun:
    key: str
    status: str            # "delivered", "failed" or "skipped"
    detail: str = ""
    triangles: int = 0
    texture: str = ""      # "atlas", "alias atlas" or "palette"
    seconds: float = 0.0


def characters(records: list[BodyRecord]) -> list[BodyRecord]:
    """The bodies this generator replaces: canonical, exported characters."""
    return [r for r in records if r.kind == "character" and r.has_export_folder]


def texture_source(key: str, paths: dict) -> str:
    found = [p for p in paths.values() if p is not None]
    if not found:
        return "palette"
    return "atlas" if any(p.stem.endswith(key) for p in found) else "alias atlas"


def prepare(record: BodyRecord, models: pathlib.Path, work: pathlib.Path,
            atlas_dir: pathlib.Path = ATLASES, edits_dir: pathlib.Path = EDITS):
    """Write work/job.json and work/corners.npz; returns (edits, atlas paths).
    A skipped body writes nothing and returns no paths."""
    edits_file = edits_dir / f"{record.key}.json"
    peek = read_edits(edits_file, 100)  # the group names run g00..g99; the real count is checked below
    if peek.skip:
        return peek, {}
    folder = models / record.dir
    body = parse_body((folder / "body.bin").read_bytes())
    palette = decode_palette((models / PALETTE_NAME).read_bytes())
    edits = read_edits(edits_file, len(body.groups))
    paths = atlas_paths(record.key, record.aliases, atlas_dir)
    _rest, mesh = rest_mesh(body, palette)
    arrays = corner_arrays(body, mesh, paths, edits)
    work.mkdir(parents=True, exist_ok=True)
    np.savez(work / "corners.npz", **arrays)
    job = {"key": record.key, "original": str((folder / "original.glb").resolve()),
           "atlases": {k: (str(p.resolve()) if p else None) for k, p in paths.items()},
           "levels": levels(len(body.groups), mesh.triangle_count, edits), "flat": sorted(edits.crease),
           **STAGE_SETTINGS}
    (work / "job.json").write_text(json.dumps(job, indent=1) + "\n")
    return edits, paths


def blender_stage(blender: pathlib.Path) -> Stage:
    """Run stage.py in a headless Blender; a failure raises RuntimeError
    with the end of Blender's output."""
    def stage(work: pathlib.Path) -> None:
        done = subprocess.run([str(blender), "--background", "--factory-startup", "--python", str(STAGE),
                               "--", str(work)], capture_output=True, text=True, timeout=STAGE_TIMEOUT_S)
        if done.returncode != 0 or not (work / "refined.npz").is_file():
            tail = (done.stdout + done.stderr).strip().splitlines()[-5:]
            raise RuntimeError("Blender stage failed: " + " | ".join(tail))
    return stage


def run_bodies(records: list[BodyRecord], models: pathlib.Path, out: pathlib.Path, work: pathlib.Path,
               stage: Stage, log=print, atlas_dir: pathlib.Path = ATLASES,
               edits_dir: pathlib.Path = EDITS) -> list[BodyRun]:
    runs = []
    for record in records:
        start = time.monotonic()
        body_work = work / record.key
        delivery = out / "bodies" / record.key / DELIVERY
        try:
            edits, paths = prepare(record, models, body_work, atlas_dir, edits_dir)
            if edits.skip:
                delivery.unlink(missing_ok=True)
                runs.append(BodyRun(record.key, "skipped", edits.skip))
                log(f"skipped {record.key}: {edits.skip}")
                continue
            for stale in ("refined.npz", "color.npy", "ao.npy"):
                (body_work / stale).unlink(missing_ok=True)
            stage(body_work)
            glb, triangles = finish_glb(body_work)
            atomic_write_bytes(delivery, glb)
            for bake in ("color.npy", "ao.npy"):
                (body_work / bake).unlink(missing_ok=True)
            run = BodyRun(record.key, "delivered", "", triangles, texture_source(record.key, paths),
                          time.monotonic() - start)
            log(f"delivered {record.key}: {triangles} triangles, {run.texture}, {run.seconds:.0f} s")
        except Exception as exc:  # a body fails alone, whatever went wrong; Ctrl-C still stops the run
            detail = str(exc)
            try:
                delivery.unlink(missing_ok=True)  # never leave an earlier delivery for import-models
            except OSError as gone:
                detail += f" (and the earlier delivery could not be removed: {gone})"
            run = BodyRun(record.key, "failed", detail, seconds=time.monotonic() - start)
            log(f"error: {record.key}: {exc}")
        runs.append(run)
    return runs


def write_report(path: pathlib.Path, runs: list[BodyRun]) -> None:
    lines = ["| Body | Status | Detail | Triangles | Texture | Seconds |", "|---|---|---|---|---|---|"]
    for r in runs:
        detail = r.detail.replace("|", "/").replace("\n", " ")
        lines.append(f"| {r.key} | {r.status} | {detail} | {r.triangles or ''} | {r.texture} | {r.seconds:.0f} |")
    atomic_write_bytes(path, ("\n".join(lines) + "\n").encode())
