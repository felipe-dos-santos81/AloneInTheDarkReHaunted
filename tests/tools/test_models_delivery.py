import io

import numpy as np
import pytest
from PIL import Image

from aitd_models.delivery import DeliveryError, read_delivery
from aitd_models.hdm import TEXTURE_JPEG, TEXTURE_PNG
from model_helpers import TINY_PNG, delivery_glb

QUAD = [(0, 0, 0), (1, 0, 0), (0, 1, 0), (1, 1, 1)]   # glTF axes, metres
QUAD_TRIS = [[0, 1, 2], [2, 1, 3]]
QUAD_UV = [(0, 0), (1, 0), (0, 1), (1, 1)]


def image_bytes(w, h, fmt="PNG"):
    buf = io.BytesIO()
    Image.new("RGB", (w, h)).save(buf, format=fmt)
    return buf.getvalue()


def test_converts_to_engine_space():
    d = read_delivery(delivery_glb(QUAD, QUAD_TRIS, QUAD_UV))
    # engine = diag(1, -1, -1) * gltf / 0.001
    assert np.allclose(d.positions, [(0, 0, 0), (1000, 0, 0), (0, -1000, 0), (1000, -1000, -1000)])
    assert d.triangles.tolist() == QUAD_TRIS
    assert np.allclose(d.uv, QUAD_UV)
    assert d.texture == TINY_PNG and d.texture_kind == TEXTURE_PNG and d.texture_size == (1, 1)


def test_missing_normals_are_computed_and_unit():
    d = read_delivery(delivery_glb(QUAD[:3], [(0, 1, 2)], QUAD_UV[:3]))
    # glTF +z normal becomes engine -z
    assert np.allclose(d.normals, [(0, 0, -1)] * 3)


def test_node_transform_is_applied():
    d = read_delivery(delivery_glb(QUAD, QUAD_TRIS, QUAD_UV, normals=[(0, 0, 1)] * 4,
                                   node={"translation": [0, 2, 0], "scale": [2, 2, 2]}))
    assert np.allclose(d.positions[1], (2000, -2000, 0))
    assert np.allclose(d.normals, [(0, 0, -1)] * 4)


def test_jpeg_texture_kind():
    d = read_delivery(delivery_glb(QUAD, QUAD_TRIS, QUAD_UV, texture=image_bytes(8, 4, "JPEG")))
    assert d.texture_kind == TEXTURE_JPEG and d.texture_size == (8, 4)


@pytest.mark.parametrize("kwargs, message", [
    ({"doc": {"extensionsRequired": ["KHR_draco_mesh_compression"]}}, "KHR_draco_mesh_compression"),
    ({"images": 2}, "2 base-colour images"),
    ({"mode": 1}, "mode 1 is not triangles"),
    ({"texture": image_bytes(4097, 1)}, "4097x1"),
    ({"texture": b"GIF89a" + bytes(20)}, "neither PNG nor JPEG"),
])
def test_rejections(kwargs, message):
    with pytest.raises(DeliveryError, match=message):
        read_delivery(delivery_glb(QUAD, QUAD_TRIS, QUAD_UV, **kwargs))


def test_rejects_too_many_triangles():
    n = 50_001
    pos = np.zeros((3 * n, 3))
    pos[:, 0] = np.arange(3 * n) % 3
    pos[:, 1] = np.arange(3 * n) % 2
    with pytest.raises(DeliveryError, match="50001 triangles"):
        read_delivery(delivery_glb(pos, np.arange(3 * n).reshape(-1, 3), np.zeros((3 * n, 2))))


def test_rejects_a_mesh_without_texture():
    from aitd_models.gltf import ARRAY_BUFFER, GlbBuilder
    g = GlbBuilder()
    attrs = {"POSITION": g.accessor(np.array(QUAD[:3], float), "VEC3", target=ARRAY_BUFFER)}
    mesh = g.add("meshes", {"primitives": [{"attributes": attrs}]})
    g.add("scenes", {"nodes": [g.add("nodes", {"mesh": mesh})]})
    with pytest.raises(DeliveryError, match="no material"):
        read_delivery(g.to_bytes())


def test_rejects_garbage():
    with pytest.raises(DeliveryError, match="not a binary glTF"):
        read_delivery(b"x" * 64)


def test_rejects_a_mesh_without_area():
    with pytest.raises(DeliveryError, match="no area"):
        read_delivery(delivery_glb([(0, 0, 0), (1, 0, 0), (2, 0, 0)], [[0, 1, 2]], QUAD_UV[:3]))


def test_primitives_sharing_one_image_merge_under_nested_transforms():
    from aitd_models.gltf import ARRAY_BUFFER, UNSIGNED_INT, GlbBuilder
    g = GlbBuilder()
    tex = g.add("textures", {"source": g.image(TINY_PNG, "image/png")})
    mat = g.add("materials", {"pbrMetallicRoughness": {"baseColorTexture": {"index": tex}}})
    prims = []
    for dx in (0.0, 5.0):
        pts = np.array(QUAD[:3], float) + (dx, 0, 0)
        prims.append({"attributes": {"POSITION": g.accessor(pts, "VEC3", target=ARRAY_BUFFER),
                                     "TEXCOORD_0": g.accessor(np.zeros((3, 2)), "VEC2", target=ARRAY_BUFFER)},
                      "material": mat, "indices": g.accessor(np.array([0, 1, 2]), "SCALAR", UNSIGNED_INT)})
    child = g.add("nodes", {"mesh": g.add("meshes", {"primitives": prims}), "translation": [0, 0, 1]})
    g.add("scenes", {"nodes": [g.add("nodes", {"children": [child], "scale": [2, 2, 2]})]})
    d = read_delivery(g.to_bytes())
    assert d.triangles.tolist() == [[0, 1, 2], [3, 4, 5]]
    assert np.allclose(d.positions[3], (10000, 0, -2000))  # (5, 0, 1) scaled by 2, then to engine space


def test_an_image_by_uri_is_refused_not_a_crash():
    from aitd_models.gltf import read_glb
    from aitd_models.gltf import GlbBuilder
    glb = read_glb(delivery_glb(QUAD, QUAD_TRIS, QUAD_UV))
    glb.doc["images"][0] = {"uri": "texture.png"}
    b = GlbBuilder(doc=glb.doc)
    b._bin += glb.bin
    with pytest.raises(DeliveryError, match="image"):
        read_delivery(b.to_bytes())


def test_a_zero_scale_node_is_refused_not_a_crash():
    with pytest.raises(DeliveryError):
        read_delivery(delivery_glb(QUAD, QUAD_TRIS, QUAD_UV, node={"scale": [0, 0, 0]}))
