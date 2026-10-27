#!/usr/bin/env python3
"""Validate the shipped GLBs without Blender or third-party Python packages.

Usage: python3 tools/validate_assets.py [--assets-dir assets/models]
Checks the full runtime inventory, embedded resources, binary accessor bounds,
skinning weights/joints, geometry bounds, and the three animated character clips.
This validates file structure/data; use the native smoke test for raylib playback.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import struct
import sys


CHARACTERS = (
    "professor", "student_f01", "student_f02", "student_f03",
    "student_m01", "student_m02", "student_m03",
)
PROPS = ("floor", "wall", "podium", "glyph", "pillar")
CLIPS = {"Idle", "Walk", "Celebrate"}
COMPONENTS = {
    5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2),
    5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4),
}
WIDTHS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4,
          "MAT2": 4, "MAT3": 9, "MAT4": 16}


class InvalidAsset(Exception):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise InvalidAsset(message)


def finite(values, label: str) -> None:
    require(all(isinstance(v, (int, float)) and math.isfinite(v) for v in values),
            f"{label}: non-finite or non-numeric data")


def item(document: dict, collection: str, index: int) -> dict:
    values = document.get(collection, [])
    require(isinstance(index, int) and 0 <= index < len(values),
            f"invalid {collection} index {index}")
    return values[index]


class GLB:
    def __init__(self, path: Path):
        data = path.read_bytes()
        require(len(data) >= 20, "truncated GLB header")
        magic, version, length = struct.unpack_from("<4sII", data)
        require(magic == b"glTF" and version == 2, "expected glTF 2.0 GLB")
        require(length == len(data), "GLB declared byte length differs from file")
        chunks = []
        offset = 12
        while offset < length:
            require(offset + 8 <= length, "truncated GLB chunk header")
            size, kind = struct.unpack_from("<I4s", data, offset)
            offset += 8
            require(size % 4 == 0 and offset + size <= length,
                    "invalid GLB chunk length/alignment")
            chunks.append((kind, data[offset:offset + size]))
            offset += size
        require(chunks and chunks[0][0] == b"JSON", "missing first JSON chunk")
        require(sum(kind == b"JSON" for kind, _ in chunks) == 1,
                "multiple JSON chunks")
        self.doc = json.loads(chunks[0][1])
        require(self.doc.get("asset", {}).get("version") == "2.0",
                "invalid glTF asset version")
        binaries = [chunk for kind, chunk in chunks if kind == b"BIN\0"]
        require(len(binaries) == 1, "expected one embedded binary chunk")
        self.binary = binaries[0]
        self.cache: dict[int, list[tuple]] = {}

    def accessor(self, index: int) -> list[tuple]:
        if index in self.cache:
            return self.cache[index]
        accessor = item(self.doc, "accessors", index)
        component_type = accessor.get("componentType")
        require(component_type in COMPONENTS, f"accessor {index}: component type")
        kind = accessor.get("type")
        require(kind in WIDTHS, f"accessor {index}: shape")
        count = accessor.get("count", 0)
        require(isinstance(count, int) and 0 < count <= 10_000_000,
                f"accessor {index}: invalid element count")
        require("sparse" not in accessor,
                f"accessor {index}: sparse data unsupported by this asset pipeline")
        view = item(self.doc, "bufferViews", accessor.get("bufferView", -1))
        fmt, component_size = COMPONENTS[component_type]
        components = WIDTHS[kind]
        # Small integer matrix columns require glTF's four-byte alignment.
        columns = int(kind[-1]) if kind.startswith("MAT") else 1
        rows = columns if columns > 1 else components
        column_size = rows * component_size
        column_stride = (column_size + 3) // 4 * 4 if columns > 1 else column_size
        packed_size = column_stride * columns
        stride = view.get("byteStride", packed_size)
        require(isinstance(stride, int) and stride >= packed_size and
                stride % component_size == 0,
                f"accessor {index}: invalid byte stride")
        local_offset = accessor.get("byteOffset", 0)
        require(isinstance(local_offset, int) and local_offset >= 0 and
                local_offset % component_size == 0,
                f"accessor {index}: invalid byte offset")
        require(local_offset + (count - 1) * stride + packed_size <= view["byteLength"],
                f"accessor {index}: binary range exceeds buffer view")
        start = view.get("byteOffset", 0) + local_offset
        values = []
        unpack = struct.Struct("<" + fmt * rows)
        for row in range(count):
            value = tuple(v for col in range(columns) for v in unpack.unpack_from(
                self.binary, start + row * stride + col * column_stride))
            if accessor.get("normalized"):
                require(component_type in (5120, 5121, 5122, 5123),
                        f"accessor {index}: invalid normalized type")
                if component_type in (5120, 5122):
                    divisor = 127 if component_type == 5120 else 32767
                    value = tuple(max(-1.0, v / divisor) for v in value)
                else:
                    divisor = 255 if component_type == 5121 else 65535
                    value = tuple(v / divisor for v in value)
            values.append(value)
        self.cache[index] = values
        return values

    def validate_structure(self) -> None:
        buffers = self.doc.get("buffers", [])
        require(len(buffers) == 1 and "uri" not in buffers[0],
                "buffer must be embedded (no external or data URI buffers)")
        size = buffers[0].get("byteLength", 0)
        require(isinstance(size, int) and 0 < size <= len(self.binary) and
                len(self.binary) - size <= 3, "embedded buffer length mismatch")
        for image in self.doc.get("images", []):
            if "uri" in image:
                require(image["uri"].startswith("data:"),
                        f"external image reference: {image['uri']}")
            else:
                item(self.doc, "bufferViews", image.get("bufferView", -1))
        for index, view in enumerate(self.doc.get("bufferViews", [])):
            start, length = view.get("byteOffset", 0), view.get("byteLength", 0)
            require(view.get("buffer") == 0 and isinstance(start, int) and
                    isinstance(length, int) and start >= 0 and length > 0 and
                    start + length <= size, f"bufferView {index}: invalid range")
        for index, accessor in enumerate(self.doc.get("accessors", [])):
            width = WIDTHS.get(accessor.get("type"), 0)
            for bound in ("min", "max"):
                if bound in accessor:
                    finite(accessor[bound], f"accessor {index} {bound}")
                    require(len(accessor[bound]) == width,
                            f"accessor {index}: wrong {bound} length")
            if "min" in accessor and "max" in accessor:
                require(all(a <= b for a, b in zip(accessor["min"], accessor["max"])),
                        f"accessor {index}: reversed bounds")
            for value in self.accessor(index):
                finite(value, f"accessor {index}")

    def validate_geometry(self) -> tuple[int, int, list[float]]:
        meshes = self.doc.get("meshes", [])
        require(meshes, "no mesh geometry")
        low, high = [math.inf] * 3, [-math.inf] * 3
        vertex_count = triangle_count = 0
        for mesh in meshes:
            require(mesh.get("primitives"), "mesh has no primitives")
            for primitive in mesh["primitives"]:
                attributes = primitive.get("attributes", {})
                index = attributes.get("POSITION", -1)
                positions = self.accessor(index)
                position_accessor = item(self.doc, "accessors", index)
                require(position_accessor["type"] == "VEC3" and
                        position_accessor["componentType"] == 5126,
                        "positions must be float VEC3")
                require("min" in position_accessor and "max" in position_accessor,
                        "position accessor must declare bounds")
                for axis in range(3):
                    a = min(p[axis] for p in positions)
                    b = max(p[axis] for p in positions)
                    require(abs(a - position_accessor["min"][axis]) < 0.001 and
                            abs(b - position_accessor["max"][axis]) < 0.001,
                            "position bounds disagree with vertex data")
                    low[axis], high[axis] = min(low[axis], a), max(high[axis], b)
                vertex_count += len(positions)
                require(primitive.get("mode", 4) == 4, "expected triangle primitives")
                count = len(positions)
                if "indices" in primitive:
                    indices = self.accessor(primitive["indices"])
                    require(all(len(i) == 1 and isinstance(i[0], int) and
                                0 <= i[0] < len(positions) for i in indices),
                            "triangle index outside vertex range")
                    count = len(indices)
                require(count % 3 == 0, "incomplete triangle")
                triangle_count += count // 3
        extents = [b - a for a, b in zip(low, high)]
        require(max(extents) >= 0.05 and max(extents) <= 20,
                f"implausible geometry dimensions: {extents}")
        require(all(abs(v) <= 100 for v in low + high), "geometry too far from origin")
        return vertex_count, triangle_count, extents

    def validate_skinning(self) -> int:
        skins = self.doc.get("skins", [])
        require(skins, "character has no skin/skeleton")
        joint_total = 0
        for skin in skins:
            joints = skin.get("joints", [])
            require(len(joints) >= 8 and len(set(joints)) == len(joints),
                    "character needs at least eight distinct skeleton joints")
            for joint in joints:
                item(self.doc, "nodes", joint)
            if "inverseBindMatrices" in skin:
                matrices = self.accessor(skin["inverseBindMatrices"])
                require(len(matrices) == len(joints) and len(matrices[0]) == 16,
                        "inverse bind matrix count/shape mismatch")
            joint_total += len(joints)
        skinned_meshes = 0
        for node in self.doc.get("nodes", []):
            if "mesh" not in node:
                continue
            require("skin" in node, "unskinned character mesh")
            skin = item(self.doc, "skins", node["skin"])
            mesh = item(self.doc, "meshes", node["mesh"])
            skinned_meshes += 1
            for primitive in mesh["primitives"]:
                attributes = primitive["attributes"]
                joints = self.accessor(attributes.get("JOINTS_0", -1))
                weights = self.accessor(attributes.get("WEIGHTS_0", -1))
                count = len(self.accessor(attributes["POSITION"]))
                require(len(joints) == len(weights) == count,
                        "skin accessor/vertex counts differ")
                require("JOINTS_1" not in attributes and "WEIGHTS_1" not in attributes,
                        "more than four bone influences; raylib export limit exceeded")
                for j, w in zip(joints, weights):
                    require(len(j) == len(w) == 4, "skin attributes must be VEC4")
                    require(all(isinstance(v, int) and 0 <= v < len(skin["joints"])
                                for v in j), "joint index outside skin palette")
                    require(all(0 <= value <= 1 for value in w), "invalid skin weight")
                    require(abs(sum(w) - 1.0) < 0.005, "skin weights do not sum to one")
        require(skinned_meshes > 0, "skin has no attached meshes")
        return joint_total

    def validate_animations(self) -> list[str]:
        animations = self.doc.get("animations", [])
        names = [animation.get("name", "") for animation in animations]
        require(CLIPS.issubset(names), f"missing clips: {', '.join(sorted(CLIPS - set(names)))}")
        require(len(names) == len(set(names)), "duplicate animation names")
        bone_nodes = {joint for skin in self.doc["skins"] for joint in skin["joints"]}
        for animation in animations:
            name = animation.get("name", "unnamed")
            channels = animation.get("channels", [])
            samplers = animation.get("samplers", [])
            require(channels and samplers, f"clip {name}: no channels/samplers")
            animated_bone = False
            for channel in channels:
                target = channel.get("target", {})
                node = target.get("node", -1)
                item(self.doc, "nodes", node)
                animated_bone |= node in bone_nodes
                require(target.get("path") in ("translation", "rotation", "scale"),
                        f"clip {name}: unexpected channel target")
                index = channel.get("sampler", -1)
                require(isinstance(index, int) and 0 <= index < len(samplers),
                        f"clip {name}: invalid sampler")
                sampler = samplers[index]
                inputs = self.accessor(sampler["input"])
                outputs = self.accessor(sampler["output"])
                require(len(inputs) >= 2 and all(len(t) == 1 for t in inputs),
                        f"clip {name}: fewer than two scalar keyframes")
                times = [t[0] for t in inputs]
                require(times[0] >= 0 and all(a < b for a, b in zip(times, times[1:])),
                        f"clip {name}: invalid keyframe timing")
                require(0 < times[-1] - times[0] <= 30, f"clip {name}: implausible duration")
                interpolation = sampler.get("interpolation", "LINEAR")
                require(interpolation in ("LINEAR", "STEP", "CUBICSPLINE"),
                        f"clip {name}: invalid interpolation")
                multiplier = 3 if interpolation == "CUBICSPLINE" else 1
                require(len(outputs) == len(inputs) * multiplier,
                        f"clip {name}: keyframe input/output count mismatch")
                width = 4 if target["path"] == "rotation" else 3
                require(all(len(value) == width for value in outputs),
                        f"clip {name}: output shape mismatch")
            require(animated_bone, f"clip {name}: no animated skeleton bones")
        return names


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assets-dir", type=Path,
                        default=Path(__file__).resolve().parent.parent / "assets" / "models")
    args = parser.parse_args()
    failed = 0
    for name in CHARACTERS + PROPS:
        path = args.assets_dir / f"{name}.glb"
        try:
            require(path.is_file(), f"missing asset: {path}")
            glb = GLB(path)
            glb.validate_structure()
            vertices, triangles, bounds = glb.validate_geometry()
            details = ""
            if name in CHARACTERS:
                joints = glb.validate_skinning()
                clips = glb.validate_animations()
                details = f" | {joints} joints | {', '.join(clips)}"
            dimensions = " x ".join(f"{v:.2f}" for v in bounds)
            print(f"PASS {path.name}: {vertices} vertices, {triangles} triangles | "
                  f"bounds {dimensions}{details}")
        except (InvalidAsset, OSError, ValueError, KeyError, TypeError, struct.error) as error:
            failed += 1
            print(f"FAIL {path.name}: {error}", file=sys.stderr)
    total = len(CHARACTERS) + len(PROPS)
    print(f"\n{total - failed}/{total} assets valid. "
          f"Inventory: {len(CHARACTERS)} characters, {len(PROPS)} props.")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
