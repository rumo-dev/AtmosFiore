"""Generate a compact, dependency-free glTF room for the DDGI fog comparison."""

import json
import struct
from pathlib import Path


OUTPUT = Path(__file__).resolve().parents[1] / "data/model/DDGIFogTest"
OUTPUT.mkdir(parents=True, exist_ok=True)


def quad(corners, normal):
    a, b, c, d = corners
    ab = tuple(b[i] - a[i] for i in range(3))
    ac = tuple(c[i] - a[i] for i in range(3))
    cross = (ab[1] * ac[2] - ab[2] * ac[1],
             ab[2] * ac[0] - ab[0] * ac[2],
             ab[0] * ac[1] - ab[1] * ac[0])
    indices = [0, 1, 2, 0, 2, 3]
    if sum(cross[i] * normal[i] for i in range(3)) < 0:
        indices = [0, 2, 1, 0, 3, 2]
    return corners, [normal] * 4, indices


surfaces = [
    # Open front; the camera looks down the two fog-filled lanes.
    ("floor", 0, [(-2.5, 0, -1.5), (2.5, 0, -1.5), (2.5, 0, 3.5), (-2.5, 0, 3.5)], (0, 1, 0)),
    ("ceiling", 0, [(-2.5, 3, -1.5), (2.5, 3, -1.5), (2.5, 3, 3.5), (-2.5, 3, 3.5)], (0, -1, 0)),
    ("left-wall", 0, [(-2.5, 0, -1.5), (-2.5, 0, 3.5), (-2.5, 3, 3.5), (-2.5, 3, -1.5)], (1, 0, 0)),
    ("right-wall", 0, [(2.5, 0, -1.5), (2.5, 0, 3.5), (2.5, 3, 3.5), (2.5, 3, -1.5)], (-1, 0, 0)),
    ("back-wall", 0, [(-2.5, 0, 3.5), (2.5, 0, 3.5), (2.5, 3, 3.5), (-2.5, 3, 3.5)], (0, 0, -1)),
    # Thick partition makes leakage across the two colored fog volumes obvious.
    ("divider-left", 1, [(-0.12, 0, -0.1), (-0.12, 0, 3.3), (-0.12, 2.8, 3.3), (-0.12, 2.8, -0.1)], (-1, 0, 0)),
    ("divider-right", 1, [(0.12, 0, -0.1), (0.12, 0, 3.3), (0.12, 2.8, 3.3), (0.12, 2.8, -0.1)], (1, 0, 0)),
    ("divider-top", 1, [(-0.12, 2.8, -0.1), (0.12, 2.8, -0.1), (0.12, 2.8, 3.3), (-0.12, 2.8, 3.3)], (0, 1, 0)),
    ("warm-emitter", 2, [(-2.33, 0.35, -0.75), (-2.33, 0.35, 2.75), (-2.33, 2.65, 2.75), (-2.33, 2.65, -0.75)], (1, 0, 0)),
    ("cyan-emitter", 3, [(2.33, 0.35, -0.75), (2.33, 0.35, 2.75), (2.33, 2.65, 2.75), (2.33, 2.65, -0.75)], (-1, 0, 0)),
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
    accessor = {"bufferView": len(buffer_views), "componentType": component_type,
                "count": count, "type": accessor_type}
    if minimum is not None:
        accessor["min"] = minimum
        accessor["max"] = maximum
    buffer_views.append(view)
    accessors.append(accessor)
    return len(accessors) - 1


for name, material, corners, normal in surfaces:
    positions, normals, indices = quad(corners, normal)
    position = add_accessor(b"".join(struct.pack("<3f", *p) for p in positions),
                            5126, "VEC3", 4,
                            [min(p[i] for p in positions) for i in range(3)],
                            [max(p[i] for p in positions) for i in range(3)], 34962)
    normal_accessor = add_accessor(b"".join(struct.pack("<3f", *n) for n in normals),
                                   5126, "VEC3", 4, target=34962)
    texcoord = add_accessor(struct.pack("<8f", 0, 0, 1, 0, 1, 1, 0, 1),
                            5126, "VEC2", 4, target=34962)
    index = add_accessor(struct.pack("<6H", *indices), 5123, "SCALAR", 6, target=34963)
    primitives.append({"attributes": {"POSITION": position, "NORMAL": normal_accessor,
                                      "TEXCOORD_0": texcoord},
                       "indices": index, "material": material, "mode": 4,
                       "extras": {"name": name}})

model = {
    "asset": {"version": "2.0", "generator": "generate_ddgi_fog_test.py"},
    "scene": 0, "scenes": [{"name": "DDGI Fog Comparison", "nodes": [0]}],
    "nodes": [{"mesh": 0, "name": "Static fog test room"}],
    "meshes": [{"name": "Warm and cyan emission lanes", "primitives": primitives}],
    "materials": [
        {"name": "Dark room", "pbrMetallicRoughness": {
            "baseColorFactor": [0.18, 0.20, 0.23, 1], "metallicFactor": 0,
            "roughnessFactor": 1}, "doubleSided": True},
        {"name": "Dark partition", "pbrMetallicRoughness": {
            "baseColorFactor": [0.035, 0.04, 0.05, 1], "metallicFactor": 0,
            "roughnessFactor": 1}, "doubleSided": True},
        {"name": "Warm emission", "pbrMetallicRoughness": {
            "baseColorFactor": [0.35, 0.09, 0.04, 1], "metallicFactor": 0,
            "roughnessFactor": 1}, "emissiveFactor": [1, 0.10, 0.025],
            "doubleSided": True},
        {"name": "Cyan emission", "pbrMetallicRoughness": {
            "baseColorFactor": [0.04, 0.20, 0.35, 1], "metallicFactor": 0,
            "roughnessFactor": 1}, "emissiveFactor": [0.025, 0.35, 1],
            "doubleSided": True},
    ],
    "buffers": [{"uri": "scene.bin", "byteLength": len(buffer)}],
    "bufferViews": buffer_views, "accessors": accessors,
}

(OUTPUT / "scene.bin").write_bytes(buffer)
(OUTPUT / "scene.gltf").write_text(json.dumps(model, indent=2) + "\n", encoding="utf-8")
print(f"Wrote {OUTPUT / 'scene.gltf'} and {len(buffer)} bytes of geometry")
