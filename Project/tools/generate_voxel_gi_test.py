"""Generate the small, dependency-free glTF room used by the Voxel GI test scene."""

import json
import struct
from pathlib import Path


OUTPUT = Path(__file__).resolve().parents[1] / "data/model/VoxelGITest"
OUTPUT.mkdir(parents=True, exist_ok=True)


def quad(vertices, normal):
    a, b, c, d = vertices
    ab = tuple(b[i] - a[i] for i in range(3))
    ac = tuple(c[i] - a[i] for i in range(3))
    cross = (ab[1] * ac[2] - ab[2] * ac[1],
             ab[2] * ac[0] - ab[0] * ac[2],
             ab[0] * ac[1] - ab[1] * ac[0])
    indices = [0, 1, 2, 0, 2, 3]
    if sum(cross[i] * normal[i] for i in range(3)) < 0:
        indices = [0, 2, 1, 0, 3, 2]
    return vertices, [normal] * 4, indices


surfaces = [
    # Five inward-facing room surfaces; the front is open to the camera.
    ("floor", 0, [(-2, 0, -2), (2, 0, -2), (2, 0, 2), (-2, 0, 2)], (0, 1, 0)),
    ("ceiling", 0, [(-2, 3, -2), (2, 3, -2), (2, 3, 2), (-2, 3, 2)], (0, -1, 0)),
    ("left", 0, [(-2, 0, -2), (-2, 0, 2), (-2, 3, 2), (-2, 3, -2)], (1, 0, 0)),
    ("right", 0, [(2, 0, -2), (2, 0, 2), (2, 3, 2), (2, 3, -2)], (-1, 0, 0)),
    ("back", 0, [(-2, 0, 2), (2, 0, 2), (2, 3, 2), (-2, 3, 2)], (0, 0, -1)),
    # A short white screen demonstrates occlusion between the two emitters.
    ("screen", 0, [(-0.08, 0, 0.2), (-0.08, 0, 1.8), (-0.08, 2.5, 1.8), (-0.08, 2.5, 0.2)], (-1, 0, 0)),
    ("screen-back", 0, [(0.08, 0, 0.2), (0.08, 0, 1.8), (0.08, 2.5, 1.8), (0.08, 2.5, 0.2)], (1, 0, 0)),
    ("red-emitter", 1, [(-1.8, 1.1, 1.82), (-0.35, 1.1, 1.82), (-0.35, 2.45, 1.82), (-1.8, 2.45, 1.82)], (0, 0, -1)),
    ("blue-emitter", 2, [(0.35, 1.1, 1.82), (1.8, 1.1, 1.82), (1.8, 2.45, 1.82), (0.35, 2.45, 1.82)], (0, 0, -1)),
]

buffer = bytearray()
buffer_views = []
accessors = []
primitives = []


def add_accessor(data, component_type, accessor_type, count, minimum=None, maximum=None, target=None):
    while len(buffer) % 4:
        buffer.append(0)
    offset = len(buffer)
    buffer.extend(data)
    view = {"buffer": 0, "byteOffset": offset, "byteLength": len(data)}
    if target:
        view["target"] = target
    view_index = len(buffer_views)
    buffer_views.append(view)
    accessor = {"bufferView": view_index, "componentType": component_type,
                "count": count, "type": accessor_type}
    if minimum is not None:
        accessor["min"] = minimum
        accessor["max"] = maximum
    accessors.append(accessor)
    return len(accessors) - 1


for name, material, corners, normal in surfaces:
    positions, normals, indices = quad(corners, normal)
    texcoords = [(0, 0), (1, 0), (1, 1), (0, 1)]
    position_data = b"".join(struct.pack("<3f", *p) for p in positions)
    normal_data = b"".join(struct.pack("<3f", *n) for n in normals)
    texcoord_data = b"".join(struct.pack("<2f", *uv) for uv in texcoords)
    index_data = struct.pack("<6H", *indices)
    pos = add_accessor(position_data, 5126, "VEC3", 4,
                       [min(p[i] for p in positions) for i in range(3)],
                       [max(p[i] for p in positions) for i in range(3)], 34962)
    norm = add_accessor(normal_data, 5126, "VEC3", 4, target=34962)
    uv = add_accessor(texcoord_data, 5126, "VEC2", 4, target=34962)
    idx = add_accessor(index_data, 5123, "SCALAR", 6, target=34963)
    primitives.append({"attributes": {"POSITION": pos, "NORMAL": norm,
                                      "TEXCOORD_0": uv},
                       "indices": idx, "material": material, "mode": 4,
                       "extras": {"name": name}})

model = {
    "asset": {"version": "2.0", "generator": "generate_voxel_gi_test.py"},
    "scene": 0, "scenes": [{"name": "Voxel GI Test", "nodes": [0]}],
    "nodes": [{"mesh": 0, "name": "Static room"}],
    "meshes": [{"name": "Room and emitters", "primitives": primitives}],
    "materials": [
        {"name": "Matte white", "pbrMetallicRoughness": {
            "baseColorFactor": [0.8, 0.8, 0.8, 1], "metallicFactor": 0,
            "roughnessFactor": 1}, "doubleSided": True},
        {"name": "Red emitter", "pbrMetallicRoughness": {
            "baseColorFactor": [0.8, 0.1, 0.08, 1], "metallicFactor": 0,
            "roughnessFactor": 1}, "emissiveFactor": [1, 0.04, 0.02],
            "doubleSided": True},
        {"name": "Blue emitter", "pbrMetallicRoughness": {
            "baseColorFactor": [0.08, 0.18, 0.8, 1], "metallicFactor": 0,
            "roughnessFactor": 1}, "emissiveFactor": [0.02, 0.08, 1],
            "doubleSided": True},
    ],
    "buffers": [{"uri": "scene.bin", "byteLength": len(buffer)}],
    "bufferViews": buffer_views, "accessors": accessors,
}

(OUTPUT / "scene.bin").write_bytes(buffer)
(OUTPUT / "scene.gltf").write_text(json.dumps(model, indent=2) + "\n", encoding="utf-8")
print(f"Wrote {OUTPUT / 'scene.gltf'} and {len(buffer)} bytes of geometry")
