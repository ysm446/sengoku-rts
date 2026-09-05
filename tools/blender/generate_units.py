"""刀武士・弓足軽・騎馬武者の再生成可能なモデルと8方向素材を作る。"""
import argparse
import json
import math
from pathlib import Path
import sys
import warnings

warnings.filterwarnings('ignore', category=DeprecationWarning)

sys.path.insert(0, str(Path(__file__).resolve().parent))
import generate_ashigaru as base
import bpy
from mathutils import Matrix, Vector


def pivot_rotation(point, angle, axis='X'):
    p = Vector(point)
    return Matrix.Translation(p) @ Matrix.Rotation(angle, 4, axis) @ Matrix.Translation(-p)


def ellipsoid(name, position, size, mat, root):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=6, location=position)
    obj = bpy.context.object
    obj.scale = size
    return base.finish(obj, name, mat, root)


def character(kind):
    root = base.make_character()
    root.name = kind
    for obj in list(root.children):
        if obj.name.startswith(('Yari_', 'Jingasa', 'Hat_Rim')):
            bpy.data.objects.remove(obj, do_unlink=True)
    armor = bpy.data.materials['Armor_TeamTint']
    wood = bpy.data.materials['Spear_Wood']
    iron = bpy.data.materials['Spear_Iron']
    dark = bpy.data.materials['Dark_Hair']
    gold = base.material('Brass_Detail', (.58, .39, .09))
    ellipsoid('Kabuto', (0, .015, 1.56), (.23, .22, .18), armor, root)
    for z in (1.42, 1.48, 1.54):
        base.block('Neck_Guard', (0, .14, z), (.48, .20, .045), armor, root)
    if kind != 'archer':
        for side in (-1, 1):
            base.rod('Crest', (0, -.21, 1.61), (side*.20, -.21, 1.88), .028, gold, root)
            base.block('Sode', (side*.32, 0, 1.08), (.11, .37, .26), armor, root)
        base.rod('Sword_Grip', (.43, -.25, 1.05), (.43, -.25, 1.28), .048, dark, root)
        base.block('Sword_Guard', (.43, -.25, 1.30), (.18, .12, .035), gold, root)
        for i in range(7):
            z = 1.33+i*.105
            y = -.25-.09*(i/7)**2
            base.rod('Sword_Blade', (.43, y, z), (.43, y-.018, z+.115), .032, iron, root, 4)
        base.rod('Scabbard', (-.24, .05, .93), (-.28, .58, .62), .046, dark, root)
    else:
        for obj in list(root.children):
            if obj.name.startswith(('Left_', 'Right_')):
                bpy.data.objects.remove(obj, do_unlink=True)
        # 弓の曲線は分節メッシュ。弦の変形は各ポーズで作り直す。
        for i in range(14):
            a, b = -1+i/7, -1+(i+1)/7
            def bow_point(t):
                return (-.40, -.37-.28*(1-t*t), 1.23+1.05*t)
            base.rod('Bow', bow_point(a), bow_point(b), .032, wood, root)
        base.block('Quiver', (.20, .24, 1.02), (.20, .17, .62), wood, root)
        for i in range(4):
            x = .11+i*.045
            base.rod('Quiver_Arrow', (x,.25,1.12),(x,.25,1.65),.014,wood,root)
            base.block('Feather', (x,.25,1.57),(.03,.08,.12),iron,root)
    if kind == 'cavalry':
        for obj in root.children:
            obj.location.z += .90
        coat = base.material('Horse_Chestnut', (.26,.105,.043))
        mane = base.material('Horse_Mane', (.045,.028,.015))
        tack = base.material('Horse_Tack', (.12,.055,.025))
        ellipsoid('Horse_Body',(0,.10,1.02),(.36,.82,.38),coat,root)
        ellipsoid('Horse_Chest',(0,-.47,1.13),(.30,.40,.43),coat,root)
        base.rod('Horse_Neck',(0,-.55,1.12),(0,-.95,1.70),.23,coat,root)
        ellipsoid('Horse_Head',(0,-1.12,1.70),(.17,.31,.22),coat,root)
        ellipsoid('Horse_Muzzle',(0,-1.34,1.56),(.14,.22,.15),coat,root)
        for side in (-1,1):
            base.rod('Horse_Ear',(side*.10,-1.02,1.83),(side*.13,-.99,2.04),.045,coat,root)
            base.block('Horse_Eye',(side*.16,-1.17,1.77),(.025,.045,.035),dark,root)
        base.rod('Mane',(0,-.56,1.34),(0,-.92,1.88),.11,mane,root)
        base.rod('Tail',(0,.79,1.15),(0,1.18,.47),.09,mane,root)
        base.block('Saddle',(0,.05,1.39),(.59,.50,.09),tack,root)
        for side in (-1,1):
            base.rod('Reins',(side*.15,-1.23,1.61),(side*.23,-.25,1.91),.022,tack,root)
            for end,y in (('Front',-.48),('Hind',.65)):
                prefix = f'HorseLeg_{end}_{side}'
                base.rod(prefix+'_Upper',(side*.24,y,1),(side*.26,y,.53),.075,coat,root)
                base.rod(prefix+'_Lower',(side*.26,y,.53),(side*.27,y-.06,.13),.048,coat,root)
                base.block(prefix+'_Hoof',(side*.27,y-.095,.08),(.13,.20,.13),mane,root)
        for obj in root.children:
            if obj.name.startswith(('Trouser','Shin_Wrap','Sandal')):
                side = -1 if obj.location.x < 0 else 1
                obj.location.x += side*.22
                obj.location.z += .08
    return root


def pose(root, originals, kind, animation, frame):
    phase = frame*math.tau/8
    walk = math.sin(phase) if animation == 'walk' else 0
    attack = (1-math.cos(phase))*.5 if animation == 'attack' else 0
    rider = .90 if kind == 'cavalry' else 0
    for obj in list(root.children):
        if obj.name.startswith('Animated_'):
            bpy.data.objects.remove(obj, do_unlink=True)
    for obj in root.children:
        obj.matrix_local = originals[obj.name].copy()
        transform = Matrix.Identity(4)
        side = -1 if obj.location.x < 0 else 1
        if obj.name.startswith(('Trouser','Shin_Wrap','Sandal')) and kind != 'cavalry':
            transform = pivot_rotation((side*.14,0,.72),walk*side*.45)
        if obj.name.startswith('HorseLeg_'):
            y = -.48 if 'Front' in obj.name else .65
            stride = math.sin(phase+(0 if side < 0 else math.pi)+(0 if y<0 else math.pi/2)) if animation=='walk' else 0
            transform = pivot_rotation((side*.24,y,1),stride*.46)
        if kind != 'archer' and obj.name.startswith(('Sword_', 'Right_')):
            angle = -.15 if animation != 'attack' else -.60+attack*2.0
            transform = pivot_rotation((.25,0,1.14+rider),angle)
        if kind == 'archer' and obj.name.startswith('Left_'):
            transform = pivot_rotation((-.25,0,1.14),-1.0)
        if kind == 'archer' and obj.name.startswith('Right_'):
            transform = pivot_rotation((.25,0,1.14),-.55) @ Matrix.Translation((-.12,attack*.30,0))
        if kind == 'cavalry' and not obj.name.startswith(('Horse_', 'HorseLeg_', 'Mane','Tail')):
            transform = Matrix.Translation((0,0,abs(walk)*.05)) @ transform
        obj.matrix_local = transform @ obj.matrix_local
    if kind == 'archer':
        # 引き絞り→放弦。射出後の矢はここでは描かず、将来の投射物計算で扱う。
        draw = (frame/4 if frame<=4 else max(0,1-(frame-4)/1.5)) if animation=='attack' else .18
        grip = (-.40+draw*.25,-.37+draw*.55,1.23)
        mat = bpy.data.materials['Warm_Linen']
        for z in (.18,2.28):
            base.rod('Animated_String',(-.40,-.37,z),grip,.013,mat,root)
        for side,hand,elbow in ((-1,(-.40,-.65,1.23),(-.36,-.32,1.12)),(1,grip,(.27,.17+draw*.15,1.16))):
            base.rod('Animated_Arm',(side*.25,0,1.14),elbow,.070,mat,root)
            base.rod('Animated_Forearm',elbow,hand,.060,bpy.data.materials['Armor_TeamTint'],root)
            base.block('Animated_Hand',hand,(.10,.10,.10),bpy.data.materials['Skin'],root)
        if animation!='attack' or frame<=4:
            base.rod('Animated_Arrow',grip,(-.40,-1.15,1.23),.020,bpy.data.materials['Spear_Wood'],root)
    root.location.z = abs(walk)*.025


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--unit',choices=['samurai','archer','cavalry'],required=True)
    args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
    scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=8;scene.cycles.use_denoising=False
    scene.render.resolution_x=scene.render.resolution_y=64;scene.render.resolution_percentage=100;scene.render.film_transparent=True
    scene.view_settings.view_transform='Standard';scene.view_settings.look='None'
    scene.world.use_nodes=True
    scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.7,.7,.7,1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value=.7
    root=character(args.unit)
    bpy.context.view_layer.update()
    originals={obj.name:obj.matrix_local.copy() for obj in root.children}
    camera_data=bpy.data.cameras.new('SpriteCamera');camera=bpy.data.objects.new('SpriteCamera',camera_data)
    scene.collection.objects.link(camera);scene.camera=camera;camera_data.type='ORTHO'
    light_data=bpy.data.lights.new('Key','AREA');light_data.energy=400;light_data.size=5
    light=bpy.data.objects.new('Key',light_data);scene.collection.objects.link(light);light.location=(-3,-4,7)
    light.rotation_euler=(Vector((0,0,1))-light.location).to_track_quat('-Z','Y').to_euler()
    elevation=math.atan2(78,math.sqrt(65*65*2))
    for animation,frames in (('idle',1),('walk',8),('attack',8)):
        scale = (6 if args.unit=='cavalry' else 5) if animation=='attack' else (5 if args.unit=='cavalry' else 3.4)
        pivot = .75 if animation=='attack' or args.unit=='cavalry' else .88
        target=Vector((0,0,(pivot-.5)*scale/math.cos(elevation)))
        camera.location=target+Vector((0,-10*math.cos(elevation),10*math.sin(elevation)))
        camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler();camera_data.ortho_scale=scale
        sheet=bytearray(512*64*frames*4);counts=[]
        for frame in range(frames):
            root.rotation_euler.z=0
            pose(root,originals,args.unit,animation,frame)
            for direction in range(8):
                pixels,count=base.render_sprite(scene,root,output,f'{animation}_{frame}',direction);counts.append(count)
                for y in range(64):
                    dst=((frame*64+y)*512+direction*64)*4
                    sheet[dst:dst+256]=pixels[y*256:(y+1)*256]
        base.png(output/f'{args.unit}_{animation}.png',512,64*frames,sheet)
        metadata={'schema':1,'unit':args.unit,'animation':animation,'directions':8,'frames':frames,'fps':8,
                  'world_size':scale,'pivot_top_left':[.5,pivot],'opaque_pixels':counts,'blender':bpy.app.version_string}
        (output/f'{args.unit}_{animation}.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
    root.rotation_euler.z=0;pose(root,originals,args.unit,'idle',0)
    bpy.ops.wm.save_as_mainfile(filepath=str(output/f'{args.unit}.blend'))
    print('SENGOKU_UNIT_READY',args.unit,flush=True)


if __name__=='__main__':
    main()
