# SPDX-License-Identifier: GPL-2.0-only
"""Export the original 320x200 plates and screens as PNG, plus manifest.json."""
from __future__ import annotations

import pathlib
from dataclasses import dataclass, field

from .catalog import (
    CAMERA_FLOORS,
    PALETTE_ENTRY,
    PALETTE_PAK,
    ImageSpec,
    camera_pak_name,
    camera_specs,
    screen_specs,
)
from .decode import PALETTE_BYTES, SCREEN_HEIGHT, SCREEN_PIXELS, SCREEN_WIDTH, decode_image, decode_palette
from .explode import ExplodeError
from .files import save_png
from .manifest import MANIFEST_NAME, ImageRecord, sha256_rgb, write_manifest
from .pak import Pak, PakError


@dataclass
class ExportResult:
    records: list[ImageRecord] = field(default_factory=list)
    skipped: list[str] = field(default_factory=list)  # "<PAK> entry <n>: <reason>"

    @property
    def cameras(self) -> int:
        return sum(1 for r in self.records if r.kind == "camera")

    @property
    def screens(self) -> int:
        return sum(1 for r in self.records if r.kind == "screen")


def _specs(data_dir: pathlib.Path, paks: dict[str, Pak]) -> list[ImageSpec]:
    specs: list[ImageSpec] = []
    for floor in CAMERA_FLOORS:
        name = camera_pak_name(floor)
        paks[name] = Pak(data_dir / f"{name}.PAK")
        specs += camera_specs(floor, paks[name].count)
    return specs + screen_specs()


def export_all(data_dir, out_dir, log=print) -> ExportResult:
    """Decode every catalogued entry under `data_dir` into `out_dir`.
    Raises PakError when a required PAK is missing; a bad entry is skipped."""
    data_dir = pathlib.Path(data_dir)
    out_dir = pathlib.Path(out_dir)
    paks: dict[str, Pak] = {PALETTE_PAK: Pak(data_dir / f"{PALETTE_PAK}.PAK")}
    palette = decode_palette(paks[PALETTE_PAK].read(PALETTE_ENTRY))
    result = ExportResult()
    for spec in _specs(data_dir, paks):
        try:
            raw = paks[spec.pak].read(spec.entry)
            expected = spec.pixel_offset + SCREEN_PIXELS
            if len(raw) != expected:
                raise ValueError(f"expected {expected} bytes, got {len(raw)}")
            pal = palette
            if spec.palette_offset is not None:
                pal = decode_palette(raw[spec.palette_offset:spec.palette_offset + PALETTE_BYTES])
            pixels = decode_image(raw, pal, spec.pixel_offset)
        except (PakError, ExplodeError, ValueError) as exc:
            reason = f"{spec.pak} entry {spec.entry}: {exc}"
            result.skipped.append(reason)
            log(f"warning: skipped {reason}")
            continue
        save_png(out_dir / spec.rel_path, pixels)
        result.records.append(ImageRecord(spec.rel_path, spec.target, spec.kind, spec.pak,
                                          spec.entry, spec.floor, (SCREEN_WIDTH, SCREEN_HEIGHT),
                                          sha256_rgb(pixels)))
    write_manifest(out_dir / MANIFEST_NAME, data_dir, result.records)
    summary = f"exported {result.cameras} cameras and {result.screens} screens to {out_dir}"
    if result.skipped:
        summary += f", {len(result.skipped)} skipped"
    log(summary)
    return result
