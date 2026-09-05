"""槍足軽のRig、Idleと8フレームWalkの8方向Spriteを生成する。"""
import argparse
import json
import math
from pathlib import Path
import struct
import sys
import zlib

import bpy
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Vector


TILE = 64
SCALE = 3.4
PIVOT = (0.5, 0.88)


def material(name, color):
    result = bpy.data.materials.new(name)
    result.diffuse_color = (*color, 1)
    if result.node_tree is None:
        result.use_nodes = True
    shader = result.node_tree.nodes.get('Principled BSDF')
    shader.inputs['Base Color'].default_value = (*color, 1)
    shader.inputs['Roughness'].default_value = 1
    return result


def finish(obj, name, mat, root):
    obj.name = name
    obj.data.materials.append(mat)
    obj.parent = root
    return obj


def block(name, position, size, mat, root):
    bpy.ops.mesh.primitive_cube_add(size=1, location=position)
    obj = bpy.context.object
    obj.scale = size
    return finish(obj, name, mat, root)


def rod(name, start, end, radius, mat, root, vertices=8):
    delta = Vector(end) - Vector(start)
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius,
                                      depth=delta.length, location=(Vector(start) + Vector(end)) * 0.5)
    obj = bpy.context.object
    obj.rotation_euler = delta.to_track_quat('Z', 'Y').to_euler()
    return finish(obj, name, mat, root)


def make_character():
    root = bpy.data.objects.new('Ashigaru', None)
    bpy.context.collection.objects.link(root)
    armor = material('Armor_TeamTint', (0.36, 0.36, 0.36))
    lacing = material('Lacing_TeamTint', (0.12, 0.12, 0.12))
    cloth = material('Warm_Linen', (0.54, 0.48, 0.34))
    pants = material('Brown_Trousers', (0.16, 0.13, 0.09))
    skin = material('Skin', (0.52, 0.32, 0.17))
    wood = material('Spear_Wood', (0.20, 0.12, 0.055))
    metal = material('Spear_Iron', (0.39, 0.44, 0.48))
    dark = material('Dark_Hair', (0.027, 0.018, 0.012))
    # BlenderはZ-up、正面は-Y。関節ごとのパーツを分けて将来のRig化に備える。
    for side in (-1, 1):
        x = side * 0.14
        rod('Trouser', (x, 0, 0.69), (x, -0.02 * side, 0.39), 0.105, pants, root)
        rod('Shin_Wrap', (x, -0.02 * side, 0.39), (x, -0.045 * side, 0.10), 0.068, cloth, root)
        block('Sandal', (x, -0.075 - 0.045 * side, 0.045), (0.15, 0.28, 0.075), wood, root)
        block('Sandal_Strap', (x, -0.07 - 0.045 * side, 0.09), (0.13, 0.035, 0.022), cloth, root)
    block('Torso', (0, 0, 1.00), (0.43, 0.29, 0.43), armor, root)
    block('Belt', (0, -0.015, 0.83), (0.47, 0.32, 0.065), cloth, root)
    for z in (0.90, 0.98, 1.06, 1.14):
        block('Armor_Lacing_Front', (0, -0.151, z), (0.42, 0.019, 0.022), lacing, root)
        block('Armor_Lacing_Back', (0, 0.151, z), (0.42, 0.019, 0.022), lacing, root)
    for x in (-0.16, 0, 0.16):
        for y in (-0.17, 0.17):
            panel = block('Kusazuri', (x, y, 0.72), (0.145, 0.065, 0.23), armor, root)
            panel.rotation_euler.x = -0.16 if y < 0 else 0.16
            for z in (0.65, 0.72, 0.79):
                block('Skirt_Lacing', (x, y * 1.23, z), (0.135, 0.02, 0.012), lacing, root)
    rod('Left_UpperArm', (-0.25, 0, 1.14), (-0.34, -0.04, 0.95), 0.078, cloth, root)
    rod('Left_Forearm', (-0.34, -0.04, 0.95), (-0.34, -0.12, 0.79), 0.065, armor, root)
    rod('Right_UpperArm', (0.25, 0, 1.14), (0.37, -0.09, 0.97), 0.078, cloth, root)
    rod('Right_Forearm', (0.37, -0.09, 0.97), (0.43, -0.25, 1.10), 0.062, armor, root)
    for x in (-0.27, 0.27):
        block('Shoulder_Plate', (x, 0, 1.16), (0.14, 0.31, 0.13), armor, root)
    block('Left_Hand', (-0.34, -0.12, 0.76), (0.09, 0.10, 0.11), skin, root)
    block('Right_Hand', (0.43, -0.25, 1.10), (0.095, 0.10, 0.12), skin, root)
    rod('Neck', (0, 0, 1.20), (0, 0, 1.32), 0.072, skin, root)
    block('Head', (0, -0.012, 1.40), (0.23, 0.22, 0.25), skin, root)
    block('Hair', (0, 0.06, 1.44), (0.24, 0.12, 0.23), dark, root)
    block('Nose', (0, -0.141, 1.40), (0.045, 0.055, 0.052), skin, root)
    for x in (-0.065, 0.065):
        block('Eye', (x, -0.126, 1.445), (0.029, 0.012, 0.021), dark, root)
    bpy.ops.mesh.primitive_cone_add(vertices=12, radius1=0.34, radius2=0.055, depth=0.17, location=(0, 0, 1.61))
    finish(bpy.context.object, 'Jingasa', armor, root)
    rod('Hat_Rim', (0, 0, 1.515), (0, 0, 1.54), 0.34, lacing, root, vertices=12)
    # 低解像度でも消えにくい太さの槍。
    rod('Yari_Shaft', (0.43, -0.25, 0.08), (0.43, -0.25, 2.72), 0.04, wood, root)
    bpy.ops.mesh.primitive_cone_add(vertices=4, radius1=0.057, radius2=0, depth=0.30, location=(0.43, -0.25, 2.86))
    finish(bpy.context.object, 'Yari_Blade', metal, root)
    rod('Yari_Socket', (0.43, -0.25, 2.59), (0.43, -0.25, 2.72), 0.03, metal, root)
    return root


def make_rig(character):
    """各パーツを1本のBoneへ剛体ウェイトで割り当てる最小Rig。"""
    meshes = [obj for obj in character.children if obj.type == 'MESH']
    data = bpy.data.armatures.new('Ashigaru_Rig')
    rig = bpy.data.objects.new('Ashigaru_Rig', data)
    bpy.context.collection.objects.link(rig)
    rig.parent = character
    bpy.context.view_layer.objects.active = rig
    rig.select_set(True)
    bpy.ops.object.mode_set(mode='EDIT')
    root_bone = data.edit_bones.new('root')
    root_bone.head = (0, 0, 0)
    root_bone.tail = (0, 0, 0.7)
    for side, x in (('L', -0.14), ('R', 0.14)):
        thigh = data.edit_bones.new('thigh.' + side)
        thigh.head, thigh.tail = (x, 0, 0.72), (x, 0, 0.40)
        thigh.parent = root_bone
        shin = data.edit_bones.new('shin.' + side)
        shin.head, shin.tail = thigh.tail, (x, 0, 0.10)
        shin.parent = thigh
    arm = data.edit_bones.new('arm.L')
    arm.head, arm.tail = (-0.25, 0, 1.14), (-0.34, -0.04, 0.95)
    arm.parent = root_bone
    bpy.ops.object.mode_set(mode='OBJECT')
    for obj in meshes:
        bone = 'root'
        side = 'L' if obj.location.x < 0 else 'R'
        if obj.name.startswith('Trouser'):
            bone = 'thigh.' + side
        elif obj.name.startswith(('Shin_Wrap', 'Sandal')):
            bone = 'shin.' + side
        elif obj.name.startswith(('Left_UpperArm', 'Left_Forearm', 'Left_Hand')):
            bone = 'arm.L'
        group = obj.vertex_groups.new(name=bone)
        group.add(list(range(len(obj.data.vertices))), 1, 'REPLACE')
        modifier = obj.modifiers.new('Rigid_Skin', 'ARMATURE')
        modifier.object = rig
    for bone in rig.pose.bones:
        bone.rotation_mode = 'XYZ'
    scene = bpy.context.scene
    scene.render.fps = 8
    scene.frame_start, scene.frame_end = 1, 8
    for frame in range(10):
        scene.frame_set(frame)
        phase = (frame - 1) * math.tau / 8
        stride = 0 if frame == 0 else math.sin(phase)
        for bone in rig.pose.bones:
            bone.rotation_euler = (0, 0, 0)
        rig.pose.bones['thigh.L'].rotation_euler.x = stride * 0.48
        rig.pose.bones['thigh.R'].rotation_euler.x = -stride * 0.48
        lift = 0 if frame == 0 else math.cos(phase)
        rig.pose.bones['shin.L'].rotation_euler.x = -max(0, -lift) * 0.50
        rig.pose.bones['shin.R'].rotation_euler.x = -max(0, lift) * 0.50
        rig.pose.bones['arm.L'].rotation_euler.x = -stride * 0.24
        character.location.z = 0
        bpy.context.view_layer.update()
        graph = bpy.context.evaluated_depsgraph_get()
        lowest = min((obj.evaluated_get(graph).matrix_world @ vertex.co).z
                     for obj in meshes for vertex in obj.evaluated_get(graph).data.vertices)
        character.location.z = max(0, 0.006 - lowest)
        for bone in rig.pose.bones:
            bone.keyframe_insert(data_path='rotation_euler', frame=frame)
        character.keyframe_insert(data_path='location', frame=frame)
    rig.animation_data.action.name = 'Walk_8fps_with_idle_at_frame_0'
    scene.frame_set(0)
    return rig


def render_sprite(scene, character, output, label, direction):
    character.rotation_euler.z = math.radians(direction * 45)
    bpy.context.view_layer.update()
    scene.render.filepath = str(output / f'raw_{label}_{direction}.png')
    bpy.ops.render.render(write_still=True)
    image = bpy.data.images.load(scene.render.filepath, check_existing=False)
    image.colorspace_settings.name = 'Non-Color'
    values = list(image.pixels[:])
    pixels = bytearray(TILE * TILE * 4)
    count = 0
    for y in range(TILE):
        for x in range(TILE):
            src = ((TILE - 1 - y) * TILE + x) * 4
            dst = (y * TILE + x) * 4
            if values[src + 3] < 0.5:
                continue
            count += 1
            for channel in range(3):
                pixels[dst + channel] = min(255, max(0, round(values[src + channel] * 15) * 17))
            pixels[dst + 3] = 255
            assert 0 < x < TILE - 1 and 0 < y < TILE - 1, f'{label}/{direction}: sprite touches border'
    bpy.data.images.remove(image)
    assert count > 100, f'{label}/{direction}: empty or undersized sprite'
    png(output / f'{label}_{direction}.png', TILE, TILE, pixels)
    return pixels, count


def png(path, width, height, pixels):
    """外部Pythonパッケージを使わずRGBA PNGを出力する。入力は上から下。"""
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    rows = b''.join(b'\0' + bytes(pixels[y * width * 4:(y + 1) * width * 4]) for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', width, height, 8, 6, 0, 0, 0)) +
                     chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b''))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    # factory-startupで開いた専用プロセス内のSceneだけを初期化する。
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for existing in list(bpy.data.materials):
        bpy.data.materials.remove(existing)
    scene = bpy.context.scene
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 16
    scene.cycles.use_denoising = False
    scene.cycles.seed = 0
    scene.render.resolution_x = scene.render.resolution_y = TILE
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.view_settings.view_transform = 'Standard'
    scene.view_settings.look = 'None'
    scene.view_settings.exposure = 0
    scene.view_settings.gamma = 1
    if scene.world.node_tree is None:
        scene.world.use_nodes = True
    scene.world.node_tree.nodes.get('Background').inputs[0].default_value = (0.7, 0.7, 0.7, 1)
    scene.world.node_tree.nodes.get('Background').inputs[1].default_value = 0.7
    character = make_character()
    rig = make_rig(character)
    elevation = math.atan2(78, math.sqrt(65 * 65 * 2))
    target = Vector((0, 0, (PIVOT[1] - 0.5) * SCALE / math.cos(elevation)))
    camera_data = bpy.data.cameras.new('Sprite_Orthographic')
    camera = bpy.data.objects.new('Sprite_Orthographic', camera_data)
    scene.collection.objects.link(camera)
    camera.location = target + Vector((0, -10 * math.cos(elevation), 10 * math.sin(elevation)))
    camera.rotation_euler = (target - camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera_data.type = 'ORTHO'
    camera_data.ortho_scale = SCALE
    scene.camera = camera
    light_data = bpy.data.lights.new('Key_Soft', 'AREA')
    light_data.energy = 400
    light_data.shape = 'DISK'
    light_data.size = 5
    light = bpy.data.objects.new('Key_Soft', light_data)
    scene.collection.objects.link(light)
    light.location = (-3, -4, 7)
    light.rotation_euler = (Vector((0, 0, 1)) - light.location).to_track_quat('-Z', 'Y').to_euler()
    bpy.context.view_layer.update()
    projected = world_to_camera_view(scene, camera, Vector((0, 0, 0)))
    assert abs(projected.x - PIVOT[0]) < 1e-5 and abs(1 - projected.y - PIVOT[1]) < 1e-5

    sheets = {}
    counts = {}
    for animation, frames in (('idle', [0]), ('walk', list(range(1, 9)))):
        sheet = bytearray(TILE * 8 * TILE * len(frames) * 4)
        occupied = []
        for row, frame in enumerate(frames):
            scene.frame_set(frame)
            for direction in range(8):
                pixels, count = render_sprite(scene, character, output, f'{animation}_{row}', direction)
                occupied.append(count)
                for y in range(TILE):
                    dst = ((row * TILE + y) * TILE * 8 + direction * TILE) * 4
                    sheet[dst:dst + TILE * 4] = pixels[y * TILE * 4:(y + 1) * TILE * 4]
        png(output / f'ashigaru_{animation}.png', TILE * 8, TILE * len(frames), sheet)
        sheets[animation] = sheet
        counts[animation] = occupied
    sheet = sheets['idle']
    # 拡大比較用。最近傍で8倍にし、背景はゲームの地面に近い黄土色にする。
    preview = bytearray()
    for y in range(TILE * 8):
        for x in range(TILE * 8 * 8):
            src = ((y // 8) * TILE * 8 + x // 8) * 4
            preview.extend(sheet[src:src + 4] if sheet[src + 3] else bytes((169, 150, 108, 255)))
    png(output / 'preview.png', TILE * 8 * 8, TILE * 8, preview)
    metadata = {'schema': 1, 'blender': bpy.app.version_string, 'animation': 'idle',
                'tile_size': TILE, 'directions': 8, 'pivot_top_left': PIVOT,
                'world_size': SCALE, 'camera_elevation_degrees': math.degrees(elevation),
                'direction_order': '0 = front; each next tile rotates model +45 degrees about Blender Z',
                'opaque_pixels': counts['idle'], 'palette_levels_per_channel': 16}
    (output / 'ashigaru_idle.json').write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    metadata.update(animation='walk', frames=8, fps=8, layout='rows=frames, columns=directions',
                    opaque_pixels=counts['walk'], bones=list(rig.pose.bones.keys()))
    (output / 'ashigaru_walk.json').write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    character.rotation_euler.z = 0
    scene.frame_set(1)
    bpy.ops.wm.save_as_mainfile(filepath=str(output / 'ashigaru.blend'))
    print('SENGOKU_ASSET_READY', output, flush=True)


if __name__ == '__main__':
    main()
