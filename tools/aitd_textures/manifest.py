# SPDX-License-Identifier: GPL-2.0-only
"""manifest.json: what export wrote, so import can report coverage, spot
files that were never upscaled, and know where animation frames go."""
from __future__ import annotations

import hashlib
import json
import pathlib
from dataclasses import dataclass, field

import numpy as np

from .files import atomic_write_bytes

SCHEMA = 2
READABLE_SCHEMAS = (1, 2)  # schema 1 predates animations
GAME = "aitd1"
MANIFEST_NAME = "manifest.json"
PALETTE_INFO = {"pak": "ITD_RESS", "entry": 3}


class ManifestError(Exception):
    pass


@dataclass
class ImageRecord:
    path: str  # relative to the export folder, e.g. backgrounds/CAMERA00_000.png
    target: str  # flat engine name, e.g. CAMERA00_000.png
    kind: str  # "camera" or "screen"
    pak: str
    entry: int
    floor: int | None
    size: tuple[int, int]
    sha256: str  # over the decoded (H, W, 3) uint8 bytes

    def to_json(self) -> dict:
        return {"path": self.path, "target": self.target, "kind": self.kind,
                "pak": self.pak, "entry": self.entry, "floor": self.floor,
                "size": list(self.size), "sha256": self.sha256}

    @classmethod
    def from_json(cls, doc: dict) -> "ImageRecord":
        try:
            return cls(doc["path"], doc["target"], doc["kind"], doc["pak"],
                       int(doc["entry"]), doc["floor"],
                       (int(doc["size"][0]), int(doc["size"][1])), doc["sha256"])
        except (KeyError, TypeError, ValueError, IndexError) as exc:
            raise ManifestError(f"bad image record {doc!r}: {exc}")


@dataclass
class AnimationJob:
    """One animated background for the upscaler to re-animate. What each
    field obliges it to do is in docs/texture-contract.md."""
    name: str  # engine name, e.g. CAMERA03_008 or StartupMenuBackground
    kind: str  # "camera", "screen" or "menu"
    floor: int | None
    still: str  # 320x200 still to render and animate, relative to the export folder
    reference: str  # folder of frame_NNNN.png: the existing clip, motion guidance only
    reference_frames: int
    reference_size: tuple[int, int]
    frames_dir: str  # where the upscaler writes frame_NNNN.png
    fps: float
    loop: bool
    max_frames: int | None
    engine: str  # where import writes the frames, e.g. anim_CAMERA03_008/
    active: bool  # False when the engine never asks for the name

    def to_json(self) -> dict:
        return {"name": self.name, "kind": self.kind, "floor": self.floor,
                "still": self.still, "reference": self.reference,
                "reference_frames": self.reference_frames,
                "reference_size": list(self.reference_size),
                "frames_dir": self.frames_dir, "fps": self.fps, "loop": self.loop,
                "max_frames": self.max_frames, "engine": self.engine,
                "active": self.active}

    @classmethod
    def from_json(cls, doc: dict) -> "AnimationJob":
        try:
            max_frames = doc["max_frames"]
            return cls(doc["name"], doc["kind"], doc["floor"], doc["still"],
                       doc["reference"], int(doc["reference_frames"]),
                       (int(doc["reference_size"][0]), int(doc["reference_size"][1])),
                       doc["frames_dir"], float(doc["fps"]), bool(doc["loop"]),
                       None if max_frames is None else int(max_frames),
                       doc["engine"], bool(doc["active"]))
        except (KeyError, TypeError, ValueError, IndexError) as exc:
            raise ManifestError(f"bad animation job {doc!r}: {exc}")


@dataclass
class Manifest:
    schema: int
    game: str
    data_dir: str
    palette: dict
    records: list[ImageRecord] = field(default_factory=list)
    animations: list[AnimationJob] = field(default_factory=list)

    def by_path(self) -> dict[str, ImageRecord]:
        return {r.path: r for r in self.records}

    def jobs_by_name(self) -> dict[str, AnimationJob]:
        return {j.name: j for j in self.animations}


def sha256_rgb(pixels: np.ndarray) -> str:
    return hashlib.sha256(np.ascontiguousarray(pixels, dtype=np.uint8).tobytes()).hexdigest()


def write_manifest(path, data_dir, records: list[ImageRecord],
                   animations: list[AnimationJob] = ()) -> None:
    doc = {"schema": SCHEMA, "game": GAME, "data_dir": str(data_dir),
           "palette": dict(PALETTE_INFO), "images": [r.to_json() for r in records],
           "animations": [a.to_json() for a in animations]}
    atomic_write_bytes(path, (json.dumps(doc, indent=1) + "\n").encode("utf-8"))


def read_manifest(path) -> Manifest:
    path = pathlib.Path(path)
    try:
        doc = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise ManifestError(f"{path}: {exc}")
    if not isinstance(doc, dict) or doc.get("schema") not in READABLE_SCHEMAS:
        found = doc.get("schema") if isinstance(doc, dict) else "?"
        raise ManifestError(f"{path}: unsupported schema {found} (want one of {READABLE_SCHEMAS})")
    try:
        records = [ImageRecord.from_json(d) for d in doc["images"]]
        animations = [AnimationJob.from_json(d) for d in doc.get("animations", [])]
        return Manifest(doc["schema"], doc["game"], doc["data_dir"], dict(doc["palette"]),
                        records, animations)
    except (KeyError, TypeError) as exc:
        raise ManifestError(f"{path}: {exc}")
