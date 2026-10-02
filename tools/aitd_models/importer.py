# SPDX-License-Identifier: GPL-2.0-only
"""Import generator deliveries: data/models-ai/bodies/<KEY>/model.glb ->
Assets/models_hd/body_<KEY>.hdm (and one copy per alias).

Per delivery: read the original body -- from the game data, or from the
export's body.bin and palette.bin when no game data is given -- and check it
still matches the export (SHA-256, skeleton hash); read the delivery
(delivery.py), align it to the original rest mesh (align.py), check
the fit (silhouette.py, validate.py), derive skin weights (bind.py), check
the bound mesh does not tear in the export's preview animations (stretch.py)
and pack the engine file (hdm.py). A debug .glb of the aligned mesh, coloured by bone
group, goes to --debug for inspection in Blender, with a report per key
(body_<KEY>.json: status, reason, warnings, every metric, files written) for
imported and failed bodies alike, in --report when given, else beside the
debug .glb. No .hdm is written for a delivery that fails, and with dry_run
nothing but the reports asked for in --report."""
from __future__ import annotations

import hashlib
import json
import pathlib
from dataclasses import dataclass, field

import numpy as np

from aitd_textures.catalog import PALETTE_ENTRY, PALETTE_PAK
from aitd_textures.decode import decode_palette
from aitd_textures.files import atomic_write_bytes
from aitd_textures.pak import Pak

from .align import align
from .bind import bind
from .body import parse_body
from .delivery import Delivery, DeliveryError, read_delivery
from .export import BODY_NAME, PALETTE_NAME
from .gltf import ARRAY_BUFFER, UNSIGNED_INT, GlbBuilder, read_glb
from .hdm import TEXTURE_MIME, VERTEX, HdmError, HdmMesh, write_hdm
from .manifest import BodyRecord
from .mesh import Surface
from .original import rest_mesh, to_gltf_points
from .silhouette import silhouette_iou
from .skeleton import skeleton_hash, validate
from .stretch import STRETCH_AREA_PCT, STRETCH_RATIO, posed_stretch, preview_skins, torn_pct
from .validate import check_fit

DELIVERY_NAME = "model.glb"
AMBIGUOUS_WARN = 1.0  # % of vertices


@dataclass
class ImportResult:
    imported: list[str] = field(default_factory=list)            # canonical keys
    written: list[str] = field(default_factory=list)             # every .hdm path written (or that would be)
    failed: dict[str, str] = field(default_factory=dict)         # key -> reason
    warnings: dict[str, list[str]] = field(default_factory=dict)
    metrics: dict[str, dict] = field(default_factory=dict)


@dataclass
class ImportPaths:
    data: pathlib.Path | None     # INDARK folder (bodies, palette); None reads the export's body.bin and palette.bin
    models: pathlib.Path          # export folder: manifest.json, bodies/<KEY>/reference/
    src: pathlib.Path             # delivery tree: bodies/<KEY>/model.glb
    dest: pathlib.Path            # engine folder for body_<KEY>.hdm
    debug: pathlib.Path | None    # debug .glb files, and body_<KEY>.json reports unless `report` is set; None writes neither
    report: pathlib.Path | None = None  # body_<KEY>.json reports, written even by a dry run


@dataclass
class BuildOutcome:
    metrics: dict
    warnings: list[str] = field(default_factory=list)
    failure: str | None = None    # set when nothing may be written
    hdm: bytes = b""
    debug: bytes = b""


class ExportDataError(ValueError):
    pass


def read_palette(paths: ImportPaths) -> np.ndarray:
    if paths.data is None:
        return decode_palette((paths.models / PALETTE_NAME).read_bytes())
    return decode_palette(Pak(paths.data / f"{PALETTE_PAK}.PAK").read(PALETTE_ENTRY))


def read_raw_body(paths: ImportPaths, record: BodyRecord, paks: dict[str, Pak]) -> bytes:
    """The body entry from the game data, or the export's body.bin, which
    must still be the body the manifest describes."""
    if paths.data is not None:
        return paks.setdefault(record.hqr, Pak(paths.data / f"{record.hqr}.PAK")).read(record.body)
    file = paths.models / record.dir / BODY_NAME
    if not file.is_file():
        raise ExportDataError(f"no {BODY_NAME} in the export (run make export-models again)")
    raw = file.read_bytes()
    if hashlib.sha256(raw).hexdigest() != record.body_sha256:
        raise ExportDataError(f"{BODY_NAME} does not match the manifest (run make export-models again)")
    return raw


def target_name(key: str) -> str:
    return f"body_{key}.hdm"


def debug_glb(fitted: Surface, uv: np.ndarray, texture: bytes, texture_kind: int, labels: np.ndarray) -> bytes:
    """The aligned mesh in glTF axes, textured, with COLOR_0 = a colour per bone group."""
    rng = np.random.default_rng(7)
    group_colours = rng.uniform(0.15, 1.0, (max(int(labels.max()) + 1, 1), 3))
    g = GlbBuilder()
    material = g.textured_material(texture, TEXTURE_MIME[texture_kind], "delivery")
    attrs = {"POSITION": g.accessor(to_gltf_points(fitted.positions), "VEC3", target=ARRAY_BUFFER, bounds=True),
             "TEXCOORD_0": g.accessor(uv, "VEC2", target=ARRAY_BUFFER),
             "COLOR_0": g.accessor(group_colours[labels], "VEC3", target=ARRAY_BUFFER)}
    g.single_mesh_scene({"attributes": attrs, "material": material,
                         "indices": g.accessor(fitted.triangles.reshape(-1), "SCALAR", UNSIGNED_INT)})
    return g.to_bytes()


def check_stretch(outcome: BuildOutcome, fitted: Surface, binding, original_glb: bytes, groups: int) -> None:
    """Pose the bound mesh at every key of the preview animations; fail it
    when too much of it tears (fused limbs pulled apart)."""
    skins = preview_skins(read_glb(original_glb), groups)
    if not skins:
        outcome.metrics["stretch"] = "no animation"
        return
    worst, area = posed_stretch(fitted.positions, fitted.triangles, binding.joints.astype(int),
                                binding.packed / 255.0, skins)
    torn = torn_pct(worst, area, STRETCH_RATIO)
    outcome.metrics["stretch_torn_pct"] = round(torn, 3)
    outcome.metrics["stretch_max"] = round(float(worst.max()), 2)
    if torn > STRETCH_AREA_PCT:
        outcome.failure = (f"{torn:.2f} % of the surface tears beyond {STRETCH_RATIO:g}x its rest size in the "
                           f"preview animations (limbs fused together?)")


def build_hdm(record: BodyRecord, body, palette, delivery: Delivery, export_dir) -> BuildOutcome:
    """Align, check, bind and pack one delivery. `export_dir` is the body's
    export folder (original.glb, reference/)."""
    rest, mesh = rest_mesh(body, palette)
    fit = align(delivery.surface, mesh.surface)
    fitted = Surface(fit.apply(delivery.positions), delivery.triangles)
    report = check_fit(fitted, mesh.surface, record.zv, silhouette_iou(fitted, export_dir / "reference"),
                       delivery.texture_size)
    report.metrics["align"] = {"scale": round(fit.scale, 6),
                               "translation": [round(float(t), 3) for t in fit.translation]}
    outcome = BuildOutcome(report.metrics, list(report.warnings))
    if report.failures:
        outcome.failure = "; ".join(report.failures)
        return outcome
    binding = bind(fitted.positions, fitted.triangles, mesh.positions.reshape(-1, 3, 3),
                   mesh.groups.reshape(-1, 3), [g.parent for g in body.groups])
    ambiguous = 100 * float(binding.ambiguous.mean())
    outcome.metrics["ambiguous_pct"] = round(ambiguous, 3)
    if ambiguous > AMBIGUOUS_WARN:
        outcome.warnings.append(f"{ambiguous:.1f} % of vertices lie as close to an unrelated part (check the debug .glb)")
    check_stretch(outcome, fitted, binding, (export_dir / "original.glb").read_bytes(), len(body.groups))
    if outcome.failure:
        return outcome
    vertices = np.zeros(len(fitted.positions), VERTEX)
    vertices["position"], vertices["uv"] = fitted.positions, delivery.uv
    vertices["normal"] = delivery.normals @ fit.rotation.T
    vertices["joints"], vertices["weights"] = binding.joints, binding.packed
    try:
        outcome.hdm = write_hdm(HdmMesh(len(body.groups), int(record.skeleton_hash, 16), vertices,
                                        fitted.triangles.reshape(-1).astype(np.uint32), delivery.texture,
                                        delivery.texture_kind))
    except HdmError as exc:
        outcome.failure = f"packed mesh rejected: {exc}"
        return outcome
    outcome.debug = debug_glb(fitted, delivery.uv, delivery.texture, delivery.texture_kind, binding.labels)
    return outcome


def write_report(folder: pathlib.Path, key: str, result: ImportResult, record: BodyRecord | None) -> None:
    """body_<KEY>.json: what the import decided about one key, and why."""
    imported = key in result.imported
    report = {"key": key, "status": "imported" if imported else "failed", "reason": result.failed.get(key),
              "warnings": result.warnings.get(key, []), "metrics": result.metrics.get(key, {}),
              "files": [target_name(k) for k in [key, *record.aliases]] if imported else []}
    atomic_write_bytes(folder / f"body_{key}.json", (json.dumps(report, indent=1) + "\n").encode())


def run_import(paths: ImportPaths, records: list[BodyRecord], only: set[str] | None = None,
               dry_run: bool = False, log=print) -> ImportResult:
    """Import every delivery under `paths.src`/bodies (or only the keys in `only`)."""
    by_key = {r.key: r for r in records}
    result = ImportResult()
    palette = read_palette(paths)
    paks: dict[str, Pak] = {}
    bodies = paths.src / "bodies"
    folders = sorted(p for p in bodies.iterdir() if p.is_dir()) if bodies.is_dir() else []
    for key in sorted((only or set()) - {f.name for f in folders}):
        result.failed[key] = "no delivery folder"
    for folder in folders:
        key = folder.name
        if only is not None and key not in only:
            continue
        record = by_key.get(key)
        if record is None:
            result.failed[key] = "not an animated body in the export manifest"
            continue
        if not record.is_canonical:
            result.failed[key] = f"an alias: deliver it as {record.canonical}"
            continue
        if not record.has_export_folder:
            result.failed[key] = "nothing drawable in the original body"
            continue
        if not (folder / DELIVERY_NAME).is_file():
            result.failed[key] = f"no {DELIVERY_NAME}"
            continue
        try:
            body = parse_body(read_raw_body(paths, record, paks))
        except ExportDataError as exc:
            result.failed[key] = str(exc)
            continue
        if validate(body) or skeleton_hash(body) != record.skeleton_hash:
            result.failed[key] = "game data does not match the export (run make export-models again)"
            continue
        try:
            delivery = read_delivery((folder / DELIVERY_NAME).read_bytes())
        except DeliveryError as exc:
            result.failed[key] = str(exc)
            continue
        try:
            outcome = build_hdm(record, body, palette, delivery, paths.models / record.dir)
        except (ValueError, OSError) as exc:  # an export file missing or unreadable, a degenerate fit
            result.failed[key] = f"import failed: {exc}"
            continue
        result.metrics[key] = outcome.metrics
        if outcome.warnings:
            result.warnings[key] = outcome.warnings
        if outcome.failure:
            result.failed[key] = outcome.failure
            continue
        result.imported.append(key)
        for k in [key, *record.aliases]:
            result.written.append(str(paths.dest / target_name(k)))
            if not dry_run:
                atomic_write_bytes(paths.dest / target_name(k), outcome.hdm)
        if not dry_run and paths.debug is not None:
            atomic_write_bytes(paths.debug / f"body_{key}.glb", outcome.debug)
        metrics = outcome.metrics
        log(f"{'checked' if dry_run else 'imported'} {key} (+{len(record.aliases)} aliases): "
            f"{len(delivery.positions)} vertices, {len(delivery.triangles)} triangles, "
            f"chamfer p95 {metrics['chamfer_p95_pct']} %, IoU min {min(metrics['iou'].values())}")
    reports = paths.report if paths.report is not None else (None if dry_run else paths.debug)
    if reports is not None:
        for key in sorted({*result.imported, *result.failed}):
            write_report(reports, key, result, by_key.get(key))
    for key, reason in sorted(result.failed.items()):
        log(f"error: {key}: {reason}")
    for key, warnings in sorted(result.warnings.items()):
        for w in warnings:
            log(f"warning: {key}: {w}")
    return result
