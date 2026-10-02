# SPDX-License-Identifier: GPL-2.0-only
"""Import generator deliveries: data/models-ai/bodies/<KEY>/model.glb ->
Assets/models_hd/body_<KEY>.hdm (and one copy per alias).

Per delivery: read it (delivery.py), check the game data still matches the
export (skeleton hash), align it to the original rest mesh (align.py), check
the fit (silhouette.py, validate.py), derive skin weights (bind.py) and pack
the engine file (hdm.py). A debug .glb of the aligned mesh, coloured by bone
group, goes to --debug for inspection in Blender. Nothing is written for a
delivery that fails, and nothing at all with dry_run."""
from __future__ import annotations

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
from .gltf import ARRAY_BUFFER, UNSIGNED_INT, GlbBuilder
from .hdm import TEXTURE_MIME, VERTEX, HdmError, HdmMesh, write_hdm
from .manifest import BodyRecord
from .mesh import Surface
from .original import rest_mesh, to_gltf_points
from .silhouette import silhouette_iou
from .skeleton import skeleton_hash, validate
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
    data: pathlib.Path            # INDARK folder (bodies, palette)
    models: pathlib.Path          # export folder: manifest.json, bodies/<KEY>/reference/
    src: pathlib.Path             # delivery tree: bodies/<KEY>/model.glb
    dest: pathlib.Path            # engine folder for body_<KEY>.hdm
    debug: pathlib.Path | None    # debug .glb files; None writes none


@dataclass
class BuildOutcome:
    metrics: dict
    warnings: list[str] = field(default_factory=list)
    failure: str | None = None    # set when nothing may be written
    hdm: bytes = b""
    debug: bytes = b""


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


def build_hdm(record: BodyRecord, body, palette, delivery: Delivery, reference_dir) -> BuildOutcome:
    """Align, check, bind and pack one delivery."""
    rest, mesh = rest_mesh(body, palette)
    fit = align(delivery.surface, mesh.surface)
    fitted = Surface(fit.apply(delivery.positions), delivery.triangles)
    report = check_fit(fitted, mesh.surface, record.zv, silhouette_iou(fitted, reference_dir), delivery.texture_size)
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


def run_import(paths: ImportPaths, records: list[BodyRecord], only: set[str] | None = None,
               dry_run: bool = False, log=print) -> ImportResult:
    """Import every delivery under `paths.src`/bodies (or only the keys in `only`)."""
    by_key = {r.key: r for r in records}
    result = ImportResult()
    palette = decode_palette(Pak(paths.data / f"{PALETTE_PAK}.PAK").read(PALETTE_ENTRY))
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
        pak = paks.setdefault(record.hqr, Pak(paths.data / f"{record.hqr}.PAK"))
        body = parse_body(pak.read(record.body))
        if validate(body) or skeleton_hash(body) != record.skeleton_hash:
            result.failed[key] = "game data does not match the export (run make export-models again)"
            continue
        try:
            delivery = read_delivery((folder / DELIVERY_NAME).read_bytes())
        except DeliveryError as exc:
            result.failed[key] = str(exc)
            continue
        try:
            outcome = build_hdm(record, body, palette, delivery, paths.models / record.dir / "reference")
        except (ValueError, OSError) as exc:  # a reference view missing or unreadable, a degenerate fit
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
    for key, reason in sorted(result.failed.items()):
        log(f"error: {key}: {reason}")
    for key, warnings in sorted(result.warnings.items()):
        for w in warnings:
            log(f"warning: {key}: {w}")
    return result
