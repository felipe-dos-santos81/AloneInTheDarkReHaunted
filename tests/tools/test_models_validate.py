import numpy as np

from aitd_models.validate import check_fit, inside_zv
from model_helpers import chain_rest_mesh

GOOD_IOU = {"front": 0.99, "side": 0.98}


def chain():
    body, _rest, mesh = chain_rest_mesh()
    return body, mesh.positions, np.arange(len(mesh.positions)).reshape(-1, 3)


def test_inside_zv_allows_ten_percent():
    zv = (-100, 100, -300, 100, -50, 50)
    assert inside_zv(np.array([[-119.0, 0, 0], [119, 0, 0]]), zv)
    assert not inside_zv(np.array([[-121.0, 0, 0]]), zv)


def test_identity_fit_passes():
    body, pos, tris = chain()
    report = check_fit(pos, tris, pos, tris, body.zv, GOOD_IOU, (2048, 2048))
    assert report.failures == [] and report.warnings == []
    assert report.metrics["chamfer_p95_pct"] < 4 and report.metrics["inside_zv"]


def test_a_misplaced_mesh_fails_on_chamfer():
    body, pos, tris = chain()
    report = check_fit(pos + (0, 0, 60), tris, pos, tris, body.zv, GOOD_IOU, (16, 16))
    assert any(f.startswith("chamfer p95") for f in report.failures)


def test_a_poor_silhouette_fails():
    body, pos, tris = chain()
    report = check_fit(pos, tris, pos, tris, body.zv, {"front": 0.99, "side": 0.6}, (16, 16))
    assert report.failures == ["silhouette IoU 0.600 in the side view (at least 0.85)"]


def test_soft_limits_warn():
    body, pos, tris = chain()
    many = np.resize(tris, (30_006, 3))
    report = check_fit(pos * 1.5, many, pos, tris, body.zv, GOOD_IOU, (4096, 1024))
    assert "30006 triangles (more than 30000)" in report.warnings
    assert "texture 4096x1024 (more than 2048 recommended)" in report.warnings
    assert "mesh reaches outside the collision box + 10 %" in report.warnings
