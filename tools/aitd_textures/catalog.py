# SPDX-License-Identifier: GPL-2.0-only
"""Which AITD1 PAK entries are exportable images, and what the engine's HD
loader (TatouSource/FitdLib/hdBackground.cpp) calls them."""
from __future__ import annotations

import re
from dataclasses import dataclass

PALETTE_PAK = "ITD_RESS"
PALETTE_ENTRY = 3
CAMERA_FLOORS = range(8)
# ITD_RESS entries that are plain 64000-byte 320x200 screens.
SCREEN_ENTRIES = (6, 7, 8, 10, 11, 12, 14, 15, 16, 17, 18, 19)
# The title screen is 64770 bytes: u16 header, 768-byte palette, 64000 pixels.
TITRE_ENTRY = 13
TITRE_PALETTE_OFFSET = 2
TITRE_PIXEL_OFFSET = 770
# Exactly what hdBackground.cpp looks for: <NAME>_<idx:%03d>.png
TARGET_RE = re.compile(r"^(CAMERA0[0-7]|ITD_RESS)_(\d{3})\.png$")


@dataclass(frozen=True)
class ImageSpec:
    kind: str  # "camera" or "screen"
    pak: str  # "CAMERA00".."CAMERA07" or "ITD_RESS"
    entry: int
    floor: int | None
    pixel_offset: int = 0
    palette_offset: int | None = None  # None: global palette (ITD_RESS entry 3)

    @property
    def target(self) -> str:
        return f"{self.pak}_{self.entry:03d}.png"

    @property
    def rel_path(self) -> str:
        folder = "backgrounds" if self.kind == "camera" else "screens"
        return f"{folder}/{self.target}"


def camera_pak_name(floor: int) -> str:
    return f"CAMERA{floor:02d}"


def camera_specs(floor: int, count: int) -> list[ImageSpec]:
    pak = camera_pak_name(floor)
    return [ImageSpec("camera", pak, i, floor) for i in range(count)]


def screen_specs() -> list[ImageSpec]:
    specs = [ImageSpec("screen", PALETTE_PAK, e, None) for e in SCREEN_ENTRIES]
    specs.append(ImageSpec("screen", PALETTE_PAK, TITRE_ENTRY, None,
                           TITRE_PIXEL_OFFSET, TITRE_PALETTE_OFFSET))
    return sorted(specs, key=lambda s: s.entry)


def parse_target(name: str) -> tuple[str, int] | None:
    m = TARGET_RE.match(name)
    return (m.group(1), int(m.group(2))) if m else None


def kind_of_pak(pak: str) -> str:
    return "camera" if pak.startswith("CAMERA") else "screen"
