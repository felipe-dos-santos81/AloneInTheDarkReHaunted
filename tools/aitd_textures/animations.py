# SPDX-License-Identifier: GPL-2.0-only
"""Animated backgrounds: export the engine's anim_<NAME>/ clips as upscale
jobs, and write imported frame sequences where the engine reads them."""
from __future__ import annotations

import os
import pathlib
import re
import shutil
from collections.abc import Iterable
from dataclasses import dataclass, field

import numpy as np
from PIL import Image

from .catalog import (
    ANIM_PREFIX,
    DARK_SUFFIX,
    ENGINE_FPS,
    GRASSMASK_SUFFIX,
    MENU_ANIMATION,
    MENU_FRAME_RE,
    animation_active,
    animation_engine,
    animation_floor,
    animation_kind,
    animation_max_frames,
    menu_frame_name,
)
from .files import atomic_write_bytes, save_png
from .manifest import AnimationJob, ImageRecord

ANIMATIONS_FOLDER = "animations"
REFERENCE_FOLDER = "reference"
FRAMES_FOLDER = "frames"
STILL_NAME = "still.png"
STILL_SIZE = (320, 200)
FRAME_RE = re.compile(r"^frame_(\d{4})\.png$")
# anim_ folders that are not upscale jobs: the grass-mask cache and the _DARK
# variants import derives. A "." in the name marks import's .tmp/.old leftovers.
NOT_JOB_SUFFIXES = (GRASSMASK_SUFFIX, DARK_SUFFIX)
OTHER_IMAGE_SUFFIXES = (".tga", ".bmp")  # the engine plays them; this pipeline does not


def frame_name(number: int) -> str:
    return f"frame_{number:04d}.png"


def job_folder(name: str) -> str:
    return f"{ANIMATIONS_FOLDER}/{name}"


def make_job(name: str, still: str, reference_frames: int,
             reference_size: tuple[int, int]) -> AnimationJob:
    return AnimationJob(
        name=name, kind=animation_kind(name), floor=animation_floor(name), still=still,
        reference=f"{job_folder(name)}/{REFERENCE_FOLDER}", reference_frames=reference_frames,
        reference_size=(int(reference_size[0]), int(reference_size[1])),
        frames_dir=f"{job_folder(name)}/{FRAMES_FOLDER}", fps=ENGINE_FPS, loop=True,
        max_frames=animation_max_frames(name), engine=animation_engine(name),
        active=animation_active(name))


@dataclass
class AnimationExport:
    jobs: list[AnimationJob] = field(default_factory=list)
    skipped: list[str] = field(default_factory=list)  # "anim_<NAME>: <reason>"
    synthesized: int = 0  # stills made from reference frame 1


def _is_job_folder(path: pathlib.Path) -> bool:
    name = path.name[len(ANIM_PREFIX):]
    return (path.is_dir() and path.name.startswith(ANIM_PREFIX) and name != ""
            and "." not in name and not name.endswith(NOT_JOB_SUFFIXES))


def discover(anims_dir) -> list[pathlib.Path]:
    """The anim_<NAME>/ folders that are upscale jobs, sorted by name."""
    return sorted((p for p in pathlib.Path(anims_dir).iterdir() if _is_job_folder(p)),
                  key=lambda p: p.name)


def _frames_of(folder: pathlib.Path) -> tuple[list[pathlib.Path], str | None]:
    """The clip's PNG frames in the engine's play order, or why it is not usable."""
    files = sorted((p for p in folder.iterdir() if p.is_file()), key=lambda p: p.name)
    others = [p.name for p in files if p.suffix.lower() in OTHER_IMAGE_SUFFIXES]
    if others:
        return [], f"non-PNG frames ({', '.join(others[:3])})"
    frames = [p for p in files if p.suffix.lower() == ".png"]
    return frames, None if frames else "no PNG frames"


def _reference_frames(ref_dir: pathlib.Path) -> list[pathlib.Path]:
    return sorted((p for p in ref_dir.iterdir() if FRAME_RE.match(p.name)), key=lambda p: p.name)


def _synthesize_still(frame: pathlib.Path, out: pathlib.Path) -> None:
    with Image.open(frame) as im:
        small = im.convert("RGB").resize(STILL_SIZE, Image.Resampling.BOX)
    save_png(out, np.asarray(small))


def export_animations(anims_dir, out_dir, records: list[ImageRecord], log=print) -> AnimationExport:
    """One job per anim_<NAME>/ folder in `anims_dir`. The reference clip is
    copied once: a later export keeps it, so a re-animated clip imported into
    anims_dir never replaces the original motion. The still is the exported
    plate named exactly <NAME>.png, else reference frame 1 shrunk to 320x200."""
    out_dir = pathlib.Path(out_dir)
    plates = {r.target: r.path for r in records}
    result = AnimationExport()
    for folder in discover(anims_dir):
        name = folder.name[len(ANIM_PREFIX):]
        ref_dir = out_dir / job_folder(name) / REFERENCE_FOLDER
        if not ref_dir.is_dir():
            frames, reason = _frames_of(folder)
            if reason:
                result.skipped.append(f"{folder.name}: {reason}")
                log(f"warning: skipped {folder.name}: {reason}")
                continue
            replace_folder(ref_dir, ((frame_name(n), f.read_bytes()) for n, f in enumerate(frames, 1)))
        frames = _reference_frames(ref_dir)
        if not frames:
            reason = f"{ref_dir} holds no frame_NNNN.png; delete it to re-export"
            result.skipped.append(f"{folder.name}: {reason}")
            log(f"warning: skipped {folder.name}: {reason}")
            continue
        with Image.open(frames[0]) as im:
            size = im.size
        still = plates.get(f"{name}.png")
        if still is None:
            still = f"{job_folder(name)}/{STILL_NAME}"
            _synthesize_still(frames[0], out_dir / still)
            result.synthesized += 1
        result.jobs.append(make_job(name, still, len(frames), size))
    return result


def replace_folder(final, files: Iterable[tuple[str, bytes]]) -> None:
    """Make folder `final` hold exactly `files` ((name, bytes) pairs). A complete
    new folder is swapped in, so the engine never sees old and new frames mixed;
    leftovers of an interrupted swap (.tmp, .old) are cleared first."""
    final = pathlib.Path(final)
    tmp = final.with_name(final.name + ".tmp")
    old = final.with_name(final.name + ".old")
    for leftover in (tmp, old):
        if leftover.exists():
            shutil.rmtree(leftover)
    tmp.mkdir(parents=True)
    for name, data in files:
        (tmp / name).write_bytes(data)
    if final.exists():
        os.replace(final, old)
    os.replace(tmp, final)
    if old.exists():
        shutil.rmtree(old)


def write_menu_frames(dest, frames: Iterable[bytes]) -> None:
    """Write StartupMenuBackground_001.png ... into `dest`, then delete the
    higher-numbered ones: the menu reads until the first missing number, so an
    old frame past the new end would keep playing."""
    dest = pathlib.Path(dest)
    count = 0
    for count, data in enumerate(frames, 1):
        atomic_write_bytes(dest / menu_frame_name(count), data)
    for path in dest.glob(f"{MENU_ANIMATION}_*.png"):
        m = MENU_FRAME_RE.match(path.name)
        if m and int(m.group(1)) > count:
            path.unlink()
