
from aitd_models.export import export_models
from aitd_models.mesh import Surface
from aitd_models.silhouette import silhouette_iou
from model_helpers import chain_rest_mesh, write_model_data_dir


def reference(tmp_path):
    export_models(write_model_data_dir(tmp_path / "INDARK"), tmp_path / "out", lambda _m: None,
                  only={"LISTBODY_000"}, size=128, ssaa=1, jobs=1)
    return tmp_path / "out" / "bodies" / "LISTBODY_000" / "reference"


def test_the_original_matches_its_own_references(tmp_path):
    _body, _rest, mesh = chain_rest_mesh()
    iou = silhouette_iou(mesh.surface, reference(tmp_path))
    assert set(iou) == {"front", "three_quarter", "side", "back"}
    assert min(iou.values()) > 0.9


def test_a_shifted_mesh_does_not(tmp_path):
    _body, _rest, mesh = chain_rest_mesh()
    shifted = mesh.positions + (120, 0, 0)
    iou = silhouette_iou(Surface(shifted, mesh.surface.triangles), reference(tmp_path))
    assert iou["front"] < 0.85
