# SPDX-License-Identifier: GPL-2.0-only
"""manifest.json: what export wrote, so import can report coverage and spot
files that were never upscaled."""
from __future__ import annotations

import hashlib
import json
import pathlib
from dataclasses import dataclass, field

import numpy as np

from .files import atomic_write_bytes

SCHEMA = 1
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
class Manifest:
    schema: int
    game: str
    data_dir: str
    palette: dict
    records: list[ImageRecord] = field(default_factory=list)

    def by_path(self) -> dict[str, ImageRecord]:
        return {r.path: r for r in self.records}


def sha256_rgb(pixels: np.ndarray) -> str:
    return hashlib.sha256(np.ascontiguousarray(pixels, dtype=np.uint8).tobytes()).hexdigest()


def write_manifest(path, data_dir, records: list[ImageRecord]) -> None:
    doc = {"schema": SCHEMA, "game": GAME, "data_dir": str(data_dir),
           "palette": dict(PALETTE_INFO), "images": [r.to_json() for r in records]}
    atomic_write_bytes(path, (json.dumps(doc, indent=1) + "\n").encode("utf-8"))


def read_manifest(path) -> Manifest:
    path = pathlib.Path(path)
    try:
        doc = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise ManifestError(f"{path}: {exc}")
    if not isinstance(doc, dict) or doc.get("schema") != SCHEMA:
        raise ManifestError(f"{path}: unsupported schema {doc.get('schema') if isinstance(doc, dict) else '?'} (want {SCHEMA})")
    try:
        records = [ImageRecord.from_json(d) for d in doc["images"]]
        return Manifest(doc["schema"], doc["game"], doc["data_dir"], dict(doc["palette"]), records)
    except (KeyError, TypeError) as exc:
        raise ManifestError(f"{path}: {exc}")
