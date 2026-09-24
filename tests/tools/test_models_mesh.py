import numpy as np
import pytest

from aitd_models.body import parse_body
from aitd_models.mesh import LINE_HALF_WIDTH, build_mesh, face_normals, icosphere
from model_helpers import CHAIN_REST, body_bytes, synthetic_palette_rgb

PALETTE = synthetic_palette_rgb()


@pytest.fixture
def mesh():
    b = parse_body(body_bytes())
    return build_mesh(b, np.array(CHAIN_REST, float), PALETTE)


def test_icosphere_is_a_closed_outward_unit_sphere():
    verts, faces = icosphere(2)
    assert (len(verts), len(faces)) == (162, 320)
    assert np.allclose(np.linalg.norm(verts, axis=1), 1.0)
    tri = verts[faces]
    normals = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    assert (np.einsum("ij,ij->i", normals, tri.mean(axis=1)) > 0).all()


def test_triangle_budget_per_primitive(mesh):
    # 3 one-triangle polys, an 8-triangle line prism, a 320-triangle sphere; the point is dropped.
    assert mesh.triangle_count == 3 + 8 + 320
    assert Counter_of(mesh.prim_index) == {0: 1, 1: 1, 2: 1, 3: 8, 4: 320}


def Counter_of(values):
    unique, counts = np.unique(values, return_counts=True)
    return dict(zip(unique.tolist(), counts.tolist()))


def test_each_corner_keeps_its_own_group(mesh):
    assert mesh.groups[:9].tolist() == [0, 1, 1, 1, 2, 2, 0, 3, 3]
    assert set(mesh.groups[9:9 + 24].tolist()) == {1, 2}   # the line spans groups 1 and 2
    assert set(mesh.groups[9 + 24:].tolist()) == {2}        # the sphere sits on vertex 6


def test_colours_come_from_the_palette(mesh):
    assert np.allclose(mesh.colors[0], PALETTE[10] / 255.0)
    assert np.allclose(mesh.colors[-1], PALETTE[50] / 255.0)


def test_sphere_and_prism_geometry(mesh):
    sphere = mesh.positions[3 * 11:]
    assert np.allclose(np.linalg.norm(sphere - CHAIN_REST[6], axis=1), 25.0)
    prism = mesh.positions[9:9 + 24]
    axis = np.array(CHAIN_REST[5], float) - CHAIN_REST[3]
    axis /= np.linalg.norm(axis)
    offsets = prism - np.array(CHAIN_REST[3], float)
    radial = offsets - np.outer(offsets @ axis, axis)
    assert np.allclose(np.linalg.norm(radial, axis=1), LINE_HALF_WIDTH * np.sqrt(2))


def test_face_normals_are_unit_or_zero():
    tri = np.array([[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 0], [1, 1, 1], [2, 2, 2]], float)
    assert np.allclose(face_normals(tri), [[0, 0, 1], [0, 0, 0]])


def test_a_body_with_only_points_has_no_triangles():
    b = parse_body(body_bytes(prims=[(2, 0, 60, [0], 0), (7, 0, 61, [3], 0)]))
    assert build_mesh(b, np.array(CHAIN_REST, float), PALETTE).triangle_count == 0
