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
from .hdm import VERTEX, HdmError, HdmMesh, write_hdm
from .manifest import BodyRecord
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


def target_name(key: str) -> str:
    return f"body_{key}.hdm"


def debug_glb(positions: np.ndarray, triangles: np.ndarray, uv: np.ndarray, texture: bytes,
              labels: np.ndarray) -> bytes:
    """The aligned mesh in glTF axes, textured, with COLOR_0 = a colour per bone group."""
    rng = np.random.default_rng(7)
    group_colours = rng.uniform(0.15, 1.0, (max(int(labels.max()) + 1, 1), 3))
    g = GlbBuilder()
    mime = "image/png" if texture.startswith(b"\x89PNG") else "image/jpeg"
    tex = g.add("textures", {"source": g.image(texture, mime)})
    material = g.add("materials", {"name": "delivery", "doubleSided": True,
                                   "pbrMetallicRoughness": {"baseColorTexture": {"index": tex},
                                                            "metallicFactor": 0.0, "roughnessFactor": 1.0}})
    attrs = {"POSITION": g.accessor(to_gltf_points(positions), "VEC3", target=ARRAY_BUFFER, bounds=True),
             "TEXCOORD_0": g.accessor(uv, "VEC2", target=ARRAY_BUFFER),
             "COLOR_0": g.accessor(group_colours[labels], "VEC3", target=ARRAY_BUFFER)}
    prim = {"attributes": attrs, "material": material,
            "indices": g.accessor(triangles.reshape(-1), "SCALAR", UNSIGNED_INT)}
    mesh = g.add("meshes", {"name": "body", "primitives": [prim]})
    g.add("scenes", {"nodes": [g.add("nodes", {"name": "body", "mesh": mesh})]})
    g.doc["scene"] = 0
    return g.to_bytes()


def build_hdm(record: BodyRecord, body, palette, delivery: Delivery, reference_dir,
              result: ImportResult) -> tuple[bytes, bytes] | None:
    """(hdm bytes, debug glb bytes), or None after recording why not."""
    key = record.key
    rest, mesh = rest_mesh(body, palette)
    target_triangles = np.arange(len(mesh.positions)).reshape(-1, 3)
    sim = align(delivery.positions, delivery.triangles, mesh.positions, target_triangles)
    positions = sim.apply(delivery.positions)
    normals = delivery.normals @ sim.rotation.T
    iou = silhouette_iou(positions, delivery.triangles, reference_dir)
    report = check_fit(positions, delivery.triangles, mesh.positions, target_triangles, record.zv,
                       iou, delivery.texture_size)
    report.metrics["align"] = {"scale": round(sim.scale, 6),
                               "translation": [round(float(t), 3) for t in sim.translation]}
    result.metrics[key] = report.metrics
    if report.warnings:
        result.warnings[key] = report.warnings
    if report.failures:
        result.failed[key] = "; ".join(report.failures)
        return None
    binding = bind(positions, delivery.triangles, mesh.positions.reshape(-1, 3, 3),
                   mesh.groups.reshape(-1, 3), [g.parent for g in body.groups])
    ambiguous = 100 * float(binding.ambiguous.mean())
    report.metrics["ambiguous_pct"] = round(ambiguous, 3)
    if ambiguous > AMBIGUOUS_WARN:
        result.warnings.setdefault(key, []).append(
            f"{ambiguous:.1f} % of vertices lie as close to an unrelated part (check the debug .glb)")
    vertices = np.zeros(len(positions), VERTEX)
    vertices["position"], vertices["normal"], vertices["uv"] = positions, normals, delivery.uv
    vertices["joints"], vertices["weights"] = binding.joints, binding.packed
    hdm = HdmMesh(len(body.groups), int(record.skeleton_hash, 16), vertices,
                  delivery.triangles.reshape(-1).astype(np.uint32), delivery.texture, delivery.texture_kind)
    try:
        data = write_hdm(hdm)
    except HdmError as exc:
        result.failed[key] = f"packed mesh rejected: {exc}"
        return None
    return data, debug_glb(positions, delivery.triangles, delivery.uv, delivery.texture, binding.labels)


def run_import(data_dir, models_dir, records: list[BodyRecord], src, dest, debug_dir,
               only: set[str] | None = None, dry_run: bool = False, log=print) -> ImportResult:
    """Import every delivery under `src`/bodies (or only the keys in `only`)."""
    data_dir, models_dir, src, dest = map(pathlib.Path, (data_dir, models_dir, src, dest))
    by_key = {r.key: r for r in records}
    result = ImportResult()
    palette = decode_palette(Pak(data_dir / f"{PALETTE_PAK}.PAK").read(PALETTE_ENTRY))
    paks: dict[str, Pak] = {}
    folders = sorted(p for p in (src / "bodies").iterdir() if p.is_dir()) if (src / "bodies").is_dir() else []
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
        if record.kind == "skip":
            result.failed[key] = "nothing drawable in the original body"
            continue
        if not (folder / DELIVERY_NAME).is_file():
            result.failed[key] = f"no {DELIVERY_NAME}"
            continue
        pak = paks.setdefault(record.hqr, Pak(data_dir / f"{record.hqr}.PAK"))
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
            built = build_hdm(record, body, palette, delivery, models_dir / record.dir / "reference", result)
        except (ValueError, OSError) as exc:  # a reference view missing or unreadable, a degenerate fit
            result.failed[key] = f"import failed: {exc}"
            continue
        if built is None:
            continue
        data, debug = built
        result.imported.append(key)
        for k in [key, *record.aliases]:
            result.written.append(str(dest / target_name(k)))
            if not dry_run:
                atomic_write_bytes(dest / target_name(k), data)
        if not dry_run and debug_dir is not None:
            atomic_write_bytes(pathlib.Path(debug_dir) / f"body_{key}.glb", debug)
        m = result.metrics[key]
        log(f"{'checked' if dry_run else 'imported'} {key} (+{len(record.aliases)} aliases): "
            f"{len(delivery.positions)} vertices, {len(delivery.triangles)} triangles, "
            f"chamfer p95 {m['chamfer_p95_pct']} %, IoU min {min(m['iou'].values())}")
    for key, reason in sorted(result.failed.items()):
        log(f"error: {key}: {reason}")
    for key, warnings in sorted(result.warnings.items()):
        for w in warnings:
            log(f"warning: {key}: {w}")
    return result
