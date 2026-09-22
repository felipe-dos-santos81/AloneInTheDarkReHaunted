# SPDX-License-Identifier: GPL-2.0-only
"""Animated backgrounds: export the engine's anim_<NAME>/ clips as upscale
jobs (a still to render, the old clip as motion reference) for
docs/texture-contract.md."""
from __future__ import annotations

import os
import pathlib
import re
import shutil
from dataclasses import dataclass, field

import numpy as np
from PIL import Image

from .catalog import (
    ANIM_PREFIX,
    DARK_SUFFIX,
    ENGINE_FPS,
    GRASSMASK_SUFFIX,
    animation_active,
    animation_engine,
    animation_floor,
    animation_kind,
    animation_max_frames,
)
from .files import save_png
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


def _copy_reference(frames: list[pathlib.Path], ref_dir: pathlib.Path) -> None:
    tmp = ref_dir.with_name(ref_dir.name + ".tmp")
    if tmp.exists():
        shutil.rmtree(tmp)
    tmp.mkdir(parents=True)
    for number, frame in enumerate(frames, 1):
        shutil.copyfile(frame, tmp / frame_name(number))
    os.replace(tmp, ref_dir)


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
            _copy_reference(frames, ref_dir)
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
