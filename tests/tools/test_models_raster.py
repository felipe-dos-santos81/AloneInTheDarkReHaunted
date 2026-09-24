import numpy as np
import pytest

from aitd_models.mesh import Mesh
from aitd_models.raster import View, framing_for, render


def mesh_of(*triangles, colours=None):
    pos = np.array(triangles, float).reshape(-1, 3)
    n = len(pos) // 3
    cols = np.array(colours if colours is not None else [(1, 0, 0)] * n, np.float32)
    return Mesh(pos, np.repeat(cols, 3, axis=0), np.zeros(len(pos), int), np.arange(n))


@pytest.mark.parametrize("yaw", [0, 45, 90, 180, 270])
def test_view_bases_are_right_handed_rotations(yaw):
    right, down, forward = View("v", yaw).basis()
    assert np.allclose(np.cross(right, down), forward)
    assert np.allclose(down, [0, 1, 0])


def test_yaw_zero_is_the_engine_camera():
    assert np.allclose(View("front", 0).basis(), np.eye(3))


def test_framing_centres_and_fits_every_yaw():
    f = framing_for(np.array([[-100, -1800, -50], [100, 0, 50]], float), 100)
    assert f.centre == (0.0, -900.0, 0.0)
    assert f.units_per_pixel == pytest.approx(1800 * 1.1 / 100)


def test_background_is_transparent_and_coverage_is_opaque():
    m = mesh_of([[-50, -50, 0], [50, -50, 0], [0, 50, 0]])
    img = render(m, View("front", 0), framing_for(m.positions, 64), ssaa=2)
    assert img.shape == (64, 64, 4)
    assert img[0, 0, 3] == 0 and img[32, 32, 3] == 255
    assert tuple(img[32, 32, :3]) == (255, 0, 0)  # facing the camera: full headlight


def test_nearer_triangle_wins():
    far = [[-50, -50, 10], [50, -50, 10], [0, 50, 10]]
    near = [[-50, -50, -10], [50, -50, -10], [0, 50, -10]]
    m = mesh_of(far, near, colours=[(1, 0, 0), (0, 0, 1)])
    img = render(m, View("front", 0), framing_for(m.positions, 32), ssaa=1)
    assert tuple(img[16, 16, :3]) == (0, 0, 255)


def test_front_and_back_are_not_mirrored():
    # A marker at engine +x (the character's left) and the head at engine -y.
    marker = [[80, -10, 0], [100, -10, 0], [90, 10, 0]]
    head = [[-10, -100, 0], [10, -100, 0], [0, -80, 0]]
    m = mesh_of(marker, head, colours=[(0, 1, 0), (0, 0, 1)])
    f = framing_for(m.positions, 64)
    front = render(m, View("front", 0), f, ssaa=1)
    back = render(m, View("back", 180), f, ssaa=1)
    green_cols = lambda img: np.nonzero((img[..., 1] > 128) & (img[..., 3] > 0))[1]
    assert green_cols(front).mean() > 32 > green_cols(back).mean()
    blue_rows = np.nonzero((front[..., 2] > 128) & (front[..., 3] > 0))[0]
    assert blue_rows.mean() < 32  # the head is at the top
