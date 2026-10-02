# SPDX-License-Identifier: GPL-2.0-only
"""The import budgets (docs/model-contract.md, spec §5.4) that need the
aligned mesh: fit to the original, silhouette, collision box, and the soft
size limits. Hard limits on the file itself (triangles, texture side,
extensions, one image) are refused earlier by delivery.read_delivery, and the
packed result is checked again by hdm.check."""
from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

from .align import chamfer, sample_surface
from .mesh import Surface, size

TRIANGLES_WARN = 30_000
TEXTURE_WARN = 2048
CHAMFER_P95_FAIL = 4.0   # % of the body's size (mesh.size)
IOU_FAIL = 0.85
ZV_MARGIN = 0.10         # of each axis' extent
CHAMFER_SAMPLES = 5000


@dataclass
class FitReport:
    failures: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    metrics: dict = field(default_factory=dict)


def inside_zv(positions: np.ndarray, zv) -> bool:
    """Whether the bounding box lies inside ZV [x1, x2, y1, y2, z1, z2] grown by ZV_MARGIN."""
    lo, hi = np.array(zv[0::2], float), np.array(zv[1::2], float)
    pad = ZV_MARGIN * (hi - lo)
    return bool((positions.min(axis=0) >= lo - pad).all() and (positions.max(axis=0) <= hi + pad).all())


def check_fit(fitted: Surface, target: Surface, zv, iou: dict[str, float],
              texture_size: tuple[int, int]) -> FitReport:
    """`fitted` is the aligned delivery (engine space); `iou` from silhouette_iou."""
    report = FitReport()
    unit = size(target.positions) / 100
    d = chamfer(sample_surface(fitted, CHAMFER_SAMPLES, seed=3), sample_surface(target, CHAMFER_SAMPLES, seed=4))
    mean, p95 = d.mean() / unit, float(np.percentile(d, 95)) / unit
    report.metrics = {"chamfer_mean_pct": round(mean, 3), "chamfer_p95_pct": round(p95, 3),
                      "iou": {k: round(v, 4) for k, v in iou.items()},
                      "inside_zv": inside_zv(fitted.positions, zv),
                      "triangles": int(len(fitted.triangles)), "texture": list(texture_size)}
    if p95 > CHAMFER_P95_FAIL:
        report.failures.append(f"chamfer p95 {p95:.2f} % of size (at most {CHAMFER_P95_FAIL} %)")
    worst = min(iou, key=iou.get)
    if iou[worst] < IOU_FAIL:
        report.failures.append(f"silhouette IoU {iou[worst]:.3f} in the {worst} view (at least {IOU_FAIL})")
    if not report.metrics["inside_zv"]:
        report.warnings.append("mesh reaches outside the collision box + 10 %")
    if len(fitted.triangles) > TRIANGLES_WARN:
        report.warnings.append(f"{len(fitted.triangles)} triangles (more than {TRIANGLES_WARN})")
    if max(texture_size) > TEXTURE_WARN:
        report.warnings.append(f"texture {texture_size[0]}x{texture_size[1]} (more than {TEXTURE_WARN} recommended)")
    return report
