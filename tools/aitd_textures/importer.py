# SPDX-License-Identifier: GPL-2.0-only
"""Validate upscaled PNGs the way the engine will consume them and copy them
into the flat backgrounds_hd/ folder, deriving _DARK variants for cameras."""
from __future__ import annotations

import pathlib
from dataclasses import dataclass, field

import numpy as np
from PIL import Image

from .catalog import kind_of_pak, parse_target
from .files import atomic_write_bytes, save_png
from .manifest import Manifest, sha256_rgb

DARK_POLICIES = ("mirror", "all", "none")
DARK_SUFFIX = "_DARK"
DEFAULT_DARK_FACTOR = 0.10  # DARK_ROOM_BRIGHTNESS in rendererBGFX.cpp
ASPECT = 320 / 200
ASPECT_TOLERANCE = 0.01  # relative
MAX_SIDE = 8192
SOURCE_FOLDERS = ("backgrounds", "screens")


@dataclass
class Finding:
    path: pathlib.Path
    kind: str  # name | invalid | aspect | too_large | unchanged | size
    severity: str  # error | warning
    message: str


@dataclass
class Candidate:
    path: pathlib.Path
    target: str
    kind: str  # camera | screen
    pixels: np.ndarray  # (H, W, 3) uint8
    copy_verbatim: bool


@dataclass
class ImportResult:
    imported: list[pathlib.Path] = field(default_factory=list)
    dark: list[pathlib.Path] = field(default_factory=list)
    skipped: list[pathlib.Path] = field(default_factory=list)  # unchanged originals
    not_replaced: list[str] = field(default_factory=list)  # manifest paths absent from src
    findings: list[Finding] = field(default_factory=list)

    @property
    def errors(self) -> list[Finding]:
        return [f for f in self.findings if f.severity == "error"]

    @property
    def warnings(self) -> list[Finding]:
        return [f for f in self.findings if f.severity == "warning"]


def _load(path: pathlib.Path):
    """Open, verify, reopen and load. verify() cannot be followed by load()
    on the same object, so the file is opened twice."""
    try:
        with Image.open(path) as probe:
            probe.verify()
        im = Image.open(path)
        im.load()
        return im, None
    except Exception as exc:  # Pillow raises many types for broken files
        return None, f"{type(exc).__name__}: {exc}"


def validate_file(path, expected_sha: str | None = None) -> tuple[Candidate | None, list[Finding]]:
    path = pathlib.Path(path)
    parsed = parse_target(path.name)
    if parsed is None:
        return None, [Finding(path, "name", "error",
                              "unknown name; expected CAMERA0F_NNN.png or ITD_RESS_NNN.png")]
    pak, _entry = parsed
    im, err = _load(path)
    if im is None:
        return None, [Finding(path, "invalid", "error", err)]
    w, h = im.size
    if abs(w / h - ASPECT) / ASPECT > ASPECT_TOLERANCE:
        return None, [Finding(path, "aspect", "error", f"{w}x{h} is not 16:10; the engine would stretch it")]
    if w > MAX_SIDE or h > MAX_SIDE:
        return None, [Finding(path, "too_large", "error", f"{w}x{h} exceeds {MAX_SIDE} px per side")]
    pixels = np.asarray(im.convert("RGB"))
    if expected_sha is not None and sha256_rgb(pixels) == expected_sha:
        return None, [Finding(path, "unchanged", "warning", "identical to the original; not upscaled, skipped")]
    findings: list[Finding] = []
    if w % 320 or h % 200:
        findings.append(Finding(path, "size", "warning", f"{w}x{h} is not an integer multiple of 320x200"))
    verbatim = im.format == "PNG" and im.mode in ("RGB", "RGBA")
    return Candidate(path, path.name, kind_of_pak(pak), pixels, verbatim), findings


def derive_dark(pixels: np.ndarray, factor: float) -> np.ndarray:
    return np.clip(np.rint(pixels.astype(np.float32) * factor), 0, 255).astype(np.uint8)


def dark_name(target: str) -> str:
    return target[: -len(".png")] + DARK_SUFFIX + ".png"


def _wants_dark(policy: str, dest: pathlib.Path, target: str) -> bool:
    if policy == "all":
        return True
    if policy == "none":
        return False
    return (dest / dark_name(target)).exists()


def run_import(src, dest, manifest: Manifest | None = None, dark: str = "mirror",
               dark_factor: float = DEFAULT_DARK_FACTOR, dry_run: bool = False,
               log=print) -> ImportResult:
    if dark not in DARK_POLICIES:
        raise ValueError(f"dark policy must be one of {DARK_POLICIES}, got {dark!r}")
    src = pathlib.Path(src)
    dest = pathlib.Path(dest)
    result = ImportResult()
    by_path = manifest.by_path() if manifest else {}
    seen: set[str] = set()
    for folder in SOURCE_FOLDERS:
        for path in sorted((src / folder).glob("*.png")):
            rel = f"{folder}/{path.name}"
            seen.add(rel)
            record = by_path.get(rel)
            cand, findings = validate_file(path, record.sha256 if record else None)
            result.findings += findings
            for f in findings:
                log(f"{f.severity}: {rel}: {f.message}")
            if cand is None:
                if any(f.kind == "unchanged" for f in findings):
                    result.skipped.append(path)
                continue
            target = dest / cand.target
            if not dry_run:
                if cand.copy_verbatim:
                    atomic_write_bytes(target, path.read_bytes())
                else:
                    save_png(target, cand.pixels)
            result.imported.append(target)
            if cand.kind == "camera" and _wants_dark(dark, dest, cand.target):
                dark_path = dest / dark_name(cand.target)
                if not dry_run:
                    save_png(dark_path, derive_dark(cand.pixels, dark_factor))
                result.dark.append(dark_path)
    result.not_replaced = [rel for rel in by_path if rel not in seen]
    return result
