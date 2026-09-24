# SPDX-License-Identifier: GPL-2.0-only
"""data/models/manifest.json: every animated body the export saw, what it
wrote for the canonical ones, and what the generator must deliver."""
from __future__ import annotations

import json
import pathlib
from dataclasses import asdict, dataclass, field

from aitd_textures.files import atomic_write_bytes

SCHEMA = 1
CONTRACT = 1
GAME = "aitd1"
MANIFEST_NAME = "manifest.json"
AXES = "gltf = diag(1, -1, -1) * engine * 0.001 (y up, metres, front +z)"


class ManifestError(Exception):
    pass


@dataclass
class BodyRecord:
    key: str                   # "LISTBODY_011"
    hqr: str                   # "LISTBODY"
    body: int                  # entry index in the HQR
    canonical: str             # key whose folder holds the export (itself when canonical)
    target: str                # engine file name: "body_LISTBODY_011.glb"
    kind: str                  # "character" (>= 6 groups), "prop", or "skip" (nothing drawable)
    body_sha256: str
    skeleton_hash: str
    zv: list[int]
    height: float              # rest-pose extent along y, engine units
    triangles: int
    prim_counts: dict[str, int]
    groups: list[dict]         # {"parent", "pivot_rest": [x, y, z], "count"}
    preview_anims: list[str] = field(default_factory=list)
    aliases: list[str] = field(default_factory=list)
    skeleton_siblings: list[str] = field(default_factory=list)

    @property
    def is_canonical(self) -> bool:
        return self.canonical == self.key

    @property
    def dir(self) -> str:
        return f"bodies/{self.canonical}"


def write_manifest(path, data_dir, records: list[BodyRecord], views: list[dict], size: int) -> None:
    doc = {"schema": SCHEMA, "contract": CONTRACT, "game": GAME, "data_dir": str(data_dir),
           "axes": AXES, "bind_pose": "rest", "reference_size": size, "views": views,
           "bodies": [asdict(r) for r in records]}
    atomic_write_bytes(pathlib.Path(path), (json.dumps(doc, indent=1) + "\n").encode())


def read_manifest(path) -> tuple[dict, list[BodyRecord]]:
    try:
        doc = json.loads(pathlib.Path(path).read_text())
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ManifestError(f"cannot read {path}: {exc}")
    if not isinstance(doc, dict) or doc.get("schema") != SCHEMA:
        raise ManifestError(f"{path}: unsupported manifest schema {doc.get('schema') if isinstance(doc, dict) else doc!r}")
    try:
        records = [BodyRecord(**r) for r in doc["bodies"]]
    except (KeyError, TypeError) as exc:
        raise ManifestError(f"{path}: bad body record: {exc}")
    return doc, records
