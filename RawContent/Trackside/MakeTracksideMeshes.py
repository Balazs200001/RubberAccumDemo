import os

import bmesh
import bpy

OUTPUT_DIRECTORY = os.path.dirname(os.path.abspath(__file__))
SCENE_NAME = "TracksideMeshes"

CONCRETE = "Concrete"
OPEN_STAND_SEATS = "OpenStandSeats"
COVERED_STAND_SEATS = "CoveredStandSeats"
ROOF = "Roof"
FENCE_STEEL = "FenceSteel"
ARMCO_STEEL = "ArmcoSteel"

VIEWPORT_COLOURS = {
    CONCRETE: (0.32, 0.32, 0.31, 1.0),
    OPEN_STAND_SEATS: (0.015, 0.07, 0.30, 1.0),
    COVERED_STAND_SEATS: (0.45, 0.015, 0.02, 1.0),
    ROOF: (0.55, 0.56, 0.57, 1.0),
    FENCE_STEEL: (0.06, 0.07, 0.065, 1.0),
    ARMCO_STEEL: (0.22, 0.23, 0.24, 1.0),
}

STAND_WIDTH = 12.0
PLINTH_BOTTOM = -4.0
FRONT_WALL_TOP = 1.0
WALKWAY_DEPTH = 0.8
TIER_COUNT = 7
TREAD = 0.85
RISE = 0.45
BACK_RAIL_HEIGHT = 1.1
BACK_WALL_THICKNESS = 0.3
AISLE = 0.45
SEAT_BLOCKS = 3
SEAT_INSET = 0.12
SEAT_DEPTH = 0.35
SEAT_HEIGHT = 0.22
ROOF_FRONT_OVERHANG = 0.6
ROOF_HEIGHT_AT_FRONT = 5.9
ROOF_HEIGHT_AT_BACK = 6.6
ROOF_THICKNESS = 0.18

POST_HALF_WIDTH = 0.04
FENCE_POST_STRAIGHT_HEIGHT = 2.2
FENCE_POST_TOP_HEIGHT = 2.6
FENCE_POST_LEAN_TOWARD_TRACK = 0.2
ARMCO_POST_BOTTOM = -0.25
ARMCO_POST_TOP = 0.62


class MeshParts:
    def __init__(self, bm, slot_names):
        self.bm = bm
        self.slot_names = slot_names

    def assign(self, faces, slot_name):
        for face in faces:
            face.material_index = self.slot_names.index(slot_name)

    def add_box(self, minimum, maximum, slot_name, top_shift_x=0.0):
        (x0, y0, z0), (x1, y1, z1) = minimum, maximum
        corners = [
            (x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
            (x0 + top_shift_x, y0, z1), (x1 + top_shift_x, y0, z1),
            (x1 + top_shift_x, y1, z1), (x0 + top_shift_x, y1, z1),
        ]
        v = [self.bm.verts.new(corner) for corner in corners]
        faces = [
            self.bm.faces.new((v[0], v[3], v[2], v[1])),
            self.bm.faces.new((v[4], v[5], v[6], v[7])),
            self.bm.faces.new((v[0], v[1], v[5], v[4])),
            self.bm.faces.new((v[1], v[2], v[6], v[5])),
            self.bm.faces.new((v[2], v[3], v[7], v[6])),
            self.bm.faces.new((v[3], v[0], v[4], v[7])),
        ]
        self.assign(faces, slot_name)

    def add_extruded_profile(self, profile, y_min, y_max, slot_name):
        near = [self.bm.verts.new((x, y_min, z)) for x, z in profile]
        far = [self.bm.verts.new((x, y_max, z)) for x, z in profile]
        faces = [self.bm.faces.new(near), self.bm.faces.new(list(reversed(far)))]
        for index in range(len(profile)):
            following = (index + 1) % len(profile)
            faces.append(self.bm.faces.new((near[index], near[following], far[following], far[index])))
        self.assign(faces, slot_name)


def stand_back_face():
    return WALKWAY_DEPTH + TIER_COUNT * TREAD


def top_tier_height():
    return FRONT_WALL_TOP + TIER_COUNT * RISE


def stand_profile(back_wall_top):
    points = [(0.0, PLINTH_BOTTOM), (0.0, FRONT_WALL_TOP), (WALKWAY_DEPTH, FRONT_WALL_TOP)]
    for tier in range(TIER_COUNT):
        tread_front = WALKWAY_DEPTH + tier * TREAD
        tread_height = FRONT_WALL_TOP + (tier + 1) * RISE
        points.append((tread_front, tread_height))
        points.append((tread_front + TREAD, tread_height))
    back = stand_back_face()
    points.append((back, back_wall_top))
    points.append((back + BACK_WALL_THICKNESS, back_wall_top))
    points.append((back + BACK_WALL_THICKNESS, PLINTH_BOTTOM))
    return points


def add_seat_rows(parts, slot_name):
    block_width = (STAND_WIDTH - (SEAT_BLOCKS + 1) * AISLE) / SEAT_BLOCKS
    for tier in range(TIER_COUNT):
        seat_front = WALKWAY_DEPTH + tier * TREAD + SEAT_INSET
        seat_bottom = FRONT_WALL_TOP + (tier + 1) * RISE
        for block in range(SEAT_BLOCKS):
            block_start = -STAND_WIDTH / 2 + AISLE + block * (block_width + AISLE)
            parts.add_box(
                (seat_front, block_start, seat_bottom),
                (seat_front + SEAT_DEPTH, block_start + block_width, seat_bottom + SEAT_HEIGHT),
                slot_name)


def add_roof(parts):
    front = -ROOF_FRONT_OVERHANG
    back = stand_back_face() + BACK_WALL_THICKNESS
    profile = [
        (front, ROOF_HEIGHT_AT_FRONT),
        (back, ROOF_HEIGHT_AT_BACK),
        (back, ROOF_HEIGHT_AT_BACK + ROOF_THICKNESS),
        (front, ROOF_HEIGHT_AT_FRONT + ROOF_THICKNESS),
    ]
    parts.add_extruded_profile(profile, -STAND_WIDTH / 2, STAND_WIDTH / 2, ROOF)


def build_open_grandstand(parts):
    parts.add_extruded_profile(
        stand_profile(top_tier_height() + BACK_RAIL_HEIGHT), -STAND_WIDTH / 2, STAND_WIDTH / 2, CONCRETE)
    add_seat_rows(parts, OPEN_STAND_SEATS)


def build_covered_grandstand(parts):
    parts.add_extruded_profile(stand_profile(ROOF_HEIGHT_AT_BACK), -STAND_WIDTH / 2, STAND_WIDTH / 2, CONCRETE)
    add_seat_rows(parts, COVERED_STAND_SEATS)
    add_roof(parts)


def build_fence_post(parts):
    parts.add_box(
        (-POST_HALF_WIDTH, -POST_HALF_WIDTH, 0.0),
        (POST_HALF_WIDTH, POST_HALF_WIDTH, FENCE_POST_STRAIGHT_HEIGHT),
        FENCE_STEEL)
    parts.add_box(
        (-POST_HALF_WIDTH, -POST_HALF_WIDTH, FENCE_POST_STRAIGHT_HEIGHT),
        (POST_HALF_WIDTH, POST_HALF_WIDTH, FENCE_POST_TOP_HEIGHT),
        FENCE_STEEL,
        top_shift_x=-FENCE_POST_LEAN_TOWARD_TRACK)


def build_armco_post(parts):
    parts.add_box(
        (-POST_HALF_WIDTH, -POST_HALF_WIDTH, ARMCO_POST_BOTTOM),
        (POST_HALF_WIDTH, POST_HALF_WIDTH, ARMCO_POST_TOP),
        ARMCO_STEEL)


MESHES = {
    "SM_GrandstandOpen": (build_open_grandstand, [CONCRETE, OPEN_STAND_SEATS]),
    "SM_GrandstandCovered": (build_covered_grandstand, [CONCRETE, COVERED_STAND_SEATS, ROOF]),
    "SM_FencePost": (build_fence_post, [FENCE_STEEL]),
    "SM_ArmcoPost": (build_armco_post, [ARMCO_STEEL]),
}


def material_for(slot_name):
    material = bpy.data.materials.get(slot_name) or bpy.data.materials.new(slot_name)
    material.diffuse_color = VIEWPORT_COLOURS[slot_name]
    return material


def remove_previous(name):
    previous = bpy.data.objects.get(name)
    if previous is not None:
        bpy.data.objects.remove(previous)
    previous_mesh = bpy.data.meshes.get(name)
    if previous_mesh is not None:
        bpy.data.meshes.remove(previous_mesh)


def build_object(scene, name, build, slot_names):
    remove_previous(name)
    mesh = bpy.data.meshes.new(name)
    for slot_name in slot_names:
        mesh.materials.append(material_for(slot_name))
    bm = bmesh.new()
    build(MeshParts(bm, slot_names))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    scene.collection.objects.link(obj)
    return obj


def export_fbx(obj, view_layer):
    for other in view_layer.objects:
        other.select_set(False)
    obj.select_set(True)
    view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=os.path.join(OUTPUT_DIRECTORY, obj.name + ".fbx"),
        use_selection=True,
        object_types={'MESH'},
        mesh_smooth_type='FACE',
        colors_type='NONE',
        add_leaf_bones=False,
        bake_anim=False)


def main():
    window = bpy.context.window
    original_scene = window.scene
    scene = bpy.data.scenes.get(SCENE_NAME) or bpy.data.scenes.new(SCENE_NAME)
    objects = [build_object(scene, name, build, slots) for name, (build, slots) in MESHES.items()]
    try:
        window.scene = scene
        for obj in objects:
            export_fbx(obj, window.view_layer)
    finally:
        window.scene = original_scene
    for obj in objects:
        triangles = sum(len(polygon.vertices) - 2 for polygon in obj.data.polygons)
        print(obj.name, len(obj.data.polygons), "faces", triangles, "triangles", [m.name for m in obj.data.materials])


if __name__ == "__main__":
    main()
