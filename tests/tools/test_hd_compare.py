import numpy as np
from PIL import Image

import hd_compare  # tools/ is on sys.path (conftest.py)

W, H = 640, 400  # two frame pixels per game pixel


def write_frames(tmp_path, shift_px=0, lit_scale=1.0):
    """A busy background; the classic body is a painted 80x120-pixel block, the
    unlit HD body the same block in one flat colour moved by shift_px frame
    pixels, the lit one the classic block times lit_scale; the hidden frame has
    no body."""
    rng = np.random.default_rng(0)
    background = (rng.random((H, W, 3)) * 80).astype(np.uint8)  # busy, but never the bodies' colours
    classic = background.copy()
    classic[100:220, 200:280] = (180, 60, 60)
    classic[100:220:6, 200:280] = (230, 90, 90)  # painted detail the swatch lacks
    unlit, lit = background.copy(), background.copy()
    unlit[100:220, 200 + shift_px:280 + shift_px] = (90, 200, 40)
    lit[100:220, 200 + shift_px:280 + shift_px] = (classic[100:220, 200:280] * lit_scale).astype(np.uint8)
    for name, img in (("classic", classic), ("unlit", unlit), ("lit", lit), ("hidden", background)):
        Image.fromarray(img).save(tmp_path / f"hdcompare_{name}.png")
    (tmp_path / "hdcompare.txt").write_text("# key x0 y0 x1 y1\nLISTBODY_011 100 50 139 109\n")
    return tmp_path


def test_a_body_in_place_passes_whatever_its_colours(tmp_path):
    lines = []
    assert hd_compare.main([str(write_frames(tmp_path))], log=lines.append) == 0
    assert lines[0] == "LISTBODY_011: box (100, 50, 139, 109), iou 1.000, brightness 1.00, ok"
    assert lines[-1] == "1 bodies, 0 failed"


def test_a_body_drawn_out_of_place_fails(tmp_path):
    lines = []
    assert hd_compare.main([str(write_frames(tmp_path, shift_px=12))], log=lines.append) == 1
    assert lines[0].endswith("BELOW LIMIT")


def test_brightness_is_lit_over_classic_on_the_classic_silhouette(tmp_path):
    d = write_frames(tmp_path, lit_scale=0.5)
    frames = {n: np.asarray(Image.open(d / f"hdcompare_{n}.png")) for n in hd_compare.FRAMES}
    (s,) = hd_compare.score(frames, hd_compare.read_boxes(d / "hdcompare.txt"))
    assert 0.45 < s.brightness < 0.6


def test_a_box_with_no_body_scores_one(tmp_path):
    d = write_frames(tmp_path)
    frames = {n: np.asarray(Image.open(d / f"hdcompare_{n}.png")) for n in hd_compare.FRAMES}
    (s,) = hd_compare.score(frames, [("LISTBODY_011", (0, 0, 10, 10))])
    assert s.iou == 1.0 and s.brightness == 1.0


def test_missing_frames_are_a_usage_error(tmp_path):
    lines = []
    assert hd_compare.main([str(tmp_path)], log=lines.append) == 2
    assert lines[-1].startswith("error: ")
