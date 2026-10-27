"""Original academy assets. Run with Blender --background --python tools/generate_assets.py.

No downloads, add-ons, or external textures are required. Blender 4.4+.
Coordinates in Blender are Z-up, facing -Y; GLB export converts to Y-up/+Z.
"""
import bpy
import math
import json
import sys
from pathlib import Path
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
MODELS = ROOT / 'assets/models'
SOURCE = ROOT / 'assets/source'
PREVIEWS = ROOT / 'assets/previews'
ONLY_PROFESSOR = '--only-professor' in sys.argv
CHARACTERS_ONLY = '--characters-only' in sys.argv
for directory in (MODELS, SOURCE, PREVIEWS):
    directory.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.context.scene.render.fps = 30
bpy.context.preferences.filepaths.save_version = 0
for action in list(bpy.data.actions):
    bpy.data.actions.remove(action)

def material(name, rgb, roughness=.55, metal=0, emission=0):
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*rgb, 1)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*rgb, 1)
    bsdf.inputs['Roughness'].default_value = roughness
    bsdf.inputs['Metallic'].default_value = metal
    if emission:
        bsdf.inputs['Emission Color'].default_value = (*rgb, 1)
        bsdf.inputs['Emission Strength'].default_value = emission
    return mat

ivory = material('Ivory cotton', (.83,.79,.66))
white = material('Eye sclera', (.9,.88,.79), .3)
black = material('Graphite leather', (.025,.035,.043), .4)
iris = material('Hazel iris', (.075,.15,.14), .3)
pupil = material('Pupil', (.008,.012,.016), .3)
gold = material('Brushed brass', (.65,.42,.13), .3, .72)
stone = material('Limestone', (.42,.49,.47), .84)
darkstone = material('Deep teal stone', (.08,.16,.17), .68)
floorstone = material('Warm travertine', (.67,.65,.54), .85)
glow = material('Luminous aquamarine', (.07,.85,.69), .22, .25, 3)
parts = []

def finish(obj, name, mat, bone=None, bevel=0, smooth=True):
    obj.name = name
    obj.data.materials.append(mat)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        mod = obj.modifiers.new('Soft crafted edges', 'BEVEL')
        mod.width = bevel
        mod.segments = 2
        bpy.ops.object.modifier_apply(modifier=mod.name)
    if smooth:
        for polygon in obj.data.polygons:
            polygon.use_smooth = True
    if bone:
        group = obj.vertex_groups.new(name=bone)
        group.add(list(range(len(obj.data.vertices))), 1.0, 'REPLACE')
    parts.append(obj)
    return obj

def ellipsoid(name, loc, scale, mat, bone=None, segments=18, rings=10):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, location=loc)
    obj = bpy.context.object
    obj.scale = scale
    return finish(obj,name,mat,bone)

def box(name, loc, scale, mat, bone=None, bevel=.015):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    obj=bpy.context.object
    obj.scale=scale
    return finish(obj,name,mat,bone,bevel,False)

def cylinder(name, a, b, radius1, radius2, mat, bone=None, vertices=16):
    a,b=Vector(a),Vector(b)
    bpy.ops.mesh.primitive_cone_add(vertices=vertices,radius1=radius1,radius2=radius2,depth=(b-a).length,location=(a+b)/2)
    obj=bpy.context.object
    obj.rotation_euler=(b-a).to_track_quat('Z','Y').to_euler()
    return finish(obj,name,mat,bone,.008)

def torso(name, rings, mat, bone='Spine'):
    vertices=[]
    n=24
    for z,rx,ry,cy in rings:
        for j in range(n):
            t=j*math.tau/n
            vertices.append((rx*math.cos(t),cy+ry*math.sin(t),z))
    faces=[]
    for i in range(len(rings)-1):
        for j in range(n):
            faces.append((i*n+j,i*n+(j+1)%n,(i+1)*n+(j+1)%n,(i+1)*n+j))
    faces.extend([tuple(range(n-1,-1,-1)),tuple((len(rings)-1)*n+j for j in range(n))])
    mesh=bpy.data.meshes.new(name)
    mesh.from_pydata(vertices,[],faces)
    mesh.update()
    obj=bpy.data.objects.new(name,mesh)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active=obj
    obj.select_set(True)
    return finish(obj,name,mat,bone)

def line(name, points, radius, mat, bone=None):
    for i in range(len(points)-1):
        cylinder(name,points[i],points[i+1],radius,radius,mat,bone,8)

def ring(name, loc, major, minor, mat, bone=None, rot=(math.pi/2,0,0)):
    bpy.ops.mesh.primitive_torus_add(major_segments=32,minor_segments=6,location=loc,major_radius=major,minor_radius=minor,rotation=rot)
    return finish(bpy.context.object,name,mat,bone)

def mesh_join(objects,name):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    bpy.ops.object.join()
    obj=bpy.context.object
    obj.name=name
    bpy.context.scene.cursor.location=(0,0,0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    return obj

def export(path,objects,animated=False):
    # Rebuild the complete editable source while leaving unrelated runtime files intact.
    if ONLY_PROFESSOR and path.name != 'professor.glb':
        return
    if CHARACTERS_ONLY and not (path.stem == 'professor' or path.stem.startswith('student_')):
        return
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,
        export_yup=True,export_animations=animated,export_animation_mode='NLA_TRACKS',
        export_skins=True,export_all_influences=False,export_force_sampling=True,
        export_optimize_animation_size=False,export_anim_slide_to_zero=True,
        export_materials='EXPORT',export_cameras=False,export_lights=False)

def make_rig(name):
    bpy.ops.object.armature_add(enter_editmode=True, location=(0,0,0))
    rig=bpy.context.object
    rig.name=name+'_Rig'
    bones=rig.data.edit_bones
    bones.remove(bones[0])
    specs=[('Root',(0,0,0),(0,0,.2),None),
        ('Hips',(0,0,.93),(0,0,1.07),'Root'),
        ('Spine',(0,0,1.07),(0,0,1.48),'Hips'),
        ('Head',(0,0,1.48),(0,0,1.78),'Spine')]
    for side,s in [('L',1),('R',-1)]:
        specs += [(f'UpperArm.{side}',(s*.245,0,1.45),(s*.31,0,1.17),'Spine'),
                  (f'Forearm.{side}',(s*.31,0,1.17),(s*.345,-.015,.92),f'UpperArm.{side}'),
                  (f'Hand.{side}',(s*.345,-.015,.92),(s*.35,-.02,.80),f'Forearm.{side}'),
                  (f'Thigh.{side}',(s*.105,0,.98),(s*.105,0,.56),'Hips'),
                  (f'Shin.{side}',(s*.105,0,.56),(s*.105,0,.16),f'Thigh.{side}'),
                  (f'Foot.{side}',(s*.105,0,.16),(s*.105,-.15,.08),f'Shin.{side}')]
    for n,h,t,parent in specs:
        bone=bones.new(n)
        bone.head=h; bone.tail=t
        if parent: bone.parent=bones[parent]
    bpy.ops.object.mode_set(mode='OBJECT')
    rig.show_in_front=True
    return rig

def create_animations(rig):
    rig.animation_data_create()
    for clip,length in [('Idle',81),('Walk',33),('Celebrate',65)]:
        action=bpy.data.actions.new(clip)
        rig.animation_data.action=action
        for frame in range(1,length+1,4):
            t=(frame-1)/(length-1)*math.tau
            for pb in rig.pose.bones:
                pb.rotation_mode='XYZ'
                pb.rotation_euler=(0,0,0)
                pb.location=(0,0,0)
            if clip=='Idle':
                rig.pose.bones['Spine'].rotation_euler.x=.018*math.sin(t)
                rig.pose.bones['Head'].rotation_euler.y=.035*math.sin(t)
                rig.pose.bones['Hips'].location.y=.006*math.sin(t)
                for side in ('L','R'):
                    rig.pose.bones['Forearm.'+side].rotation_euler.x=.035
            elif clip=='Walk':
                rig.pose.bones['Hips'].location.y=.025*(1-math.cos(t*2))
                rig.pose.bones['Spine'].rotation_euler.y=.035*math.sin(t)
                for side,s in [('L',1),('R',-1)]:
                    wave=math.sin(t)*s
                    rig.pose.bones['Thigh.'+side].rotation_euler.x=wave*.50
                    rig.pose.bones['Shin.'+side].rotation_euler.x=-max(0,-wave)*.65
                    rig.pose.bones['Foot.'+side].rotation_euler.x=max(0,wave)*.13
                    rig.pose.bones['UpperArm.'+side].rotation_euler.x=-wave*.36
                    rig.pose.bones['Forearm.'+side].rotation_euler.x=.10+max(0,wave)*.12
            else:
                amount=math.sin(t*.5)**2
                rig.pose.bones['Hips'].location.y=.035*math.sin(t*2)**2
                rig.pose.bones['Spine'].rotation_euler.x=-.08*amount
                for side,s in [('L',1),('R',-1)]:
                    rig.pose.bones['UpperArm.'+side].rotation_euler.z=-s*2.35*amount
                    rig.pose.bones['Forearm.'+side].rotation_euler.x=.3*amount
                    rig.pose.bones['Hand.'+side].rotation_euler.z=.2*math.sin(t*4)*amount
            if rig.name.startswith('professor') and clip == 'Idle':
                # Open teaching gesture, with books carried by the opposite hand.
                rig.pose.bones['UpperArm.R'].rotation_euler.z=.48
                rig.pose.bones['Forearm.R'].rotation_euler.x=.78+.05*math.sin(t)
                rig.pose.bones['Hand.R'].rotation_euler.y=-.35
            for pb in rig.pose.bones:
                pb.keyframe_insert('rotation_euler',frame=frame,group=pb.name)
                pb.keyframe_insert('location',frame=frame,group=pb.name)
        track=rig.animation_data.nla_tracks.new()
        track.name=clip
        track.strips.new(clip,1,action)
        rig.animation_data.action=None
    for track in rig.animation_data.nla_tracks:
        track.mute=track.name!='Idle'
    # Exporter includes muted NLA tracks; restore rest transforms for bounds.
    bpy.context.scene.frame_set(1)

def hair_cap(mat, variant):
    vertices=[]; faces=[]; n=32; rows=9
    for i in range(rows+1):
        for j in range(n):
            phi=j*math.tau/n
            # The face is -Y; reveal brows and temples with a shaped hairline.
            lower=(1.16 if variant=='bun' else 1.47) if math.sin(phi)<-.35 else 1.93
            theta=.025+(lower-.025)*i/rows
            if variant=='bun':
                vertices.append((.152*math.sin(theta)*math.cos(phi), .008+.140*math.sin(theta)*math.sin(phi),1.688+.184*math.cos(theta)))
            else:
                vertices.append((.143*math.sin(theta)*math.cos(phi), .015+.123*math.sin(theta)*math.sin(phi),1.681+.175*math.cos(theta)))
    # Rings descend from the crown. Traverse down before moving around the ring
    # so the surface normals point OUTWARD (raylib culls inward-facing quads).
    for i in range(rows):
        for j in range(n):
            faces.append((i*n+j,(i+1)*n+j,(i+1)*n+(j+1)%n,i*n+(j+1)%n))
    # Close the polar ring with real triangles: no tiny bald hole from overhead.
    center=Vector((0,.008,1.688) if variant=='bun' else (0,.015,1.681))
    pole=len(vertices)
    vertices.append(tuple(center+Vector((0,0,.184 if variant=='bun' else .175))))
    for j in range(n):faces.append((pole,j,(j+1)%n))
    mesh=bpy.data.meshes.new('Hair cap'); mesh.from_pydata(vertices,[],faces);mesh.update()
    assert all(face.normal.dot(face.center-center)>0 for face in mesh.polygons), 'Hair cap normals must face outwards'
    obj=bpy.data.objects.new('Sculpted hairline',mesh);bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active=obj;obj.select_set(True);finish(obj,'Sculpted hairline',mat,'Head')
    if variant in ('long','bob'):
        length=.24 if variant=='long' else .13
        for s in (-1,1):
            ellipsoid('Side hair',(s*.118,.025,1.64-length*.25),(.045,.115,length),mat,'Head')
        ellipsoid('Back hair',(0,.09,1.61-length*.35),(.139,.06,length),mat,'Head')
        for s in (-1,1):
            for i in range(4):
                line('Hair strand',[(s*(.12+i*.005),-.04+i*.025,1.74),(s*(.133+i*.003),-.04+i*.025,1.48 if variant=='long' else 1.57)],.005,mat,'Head')
    elif variant=='pony':
        ellipsoid('Ponytail',(0,.17,1.60),(.072,.077,.22),mat,'Head')
        ring('Hair tie',(0,.14,1.72),.055,.012,gold,'Head',rot=(0,0,0))
    elif variant=='curls':
        for i in range(16):
            angle=i*math.tau/16
            ellipsoid('Coiled hair',(.116*math.cos(angle),.014+.095*math.sin(angle),1.80),(.048,.047,.047),mat,'Head',12,8)
        for i in range(5):
            ellipsoid('Crown curl',((i-2)*.043,0,1.845),(.046,.055,.032),mat,'Head',12,8)
    elif variant=='part':
        for i in range(6):
            obj=ellipsoid('Side swept lock',(-.09+i*.036,-.055,1.81),(.042,.072,.055),mat,'Head')
            obj.rotation_euler.y=-.35
    elif variant=='bun':
        ellipsoid('Low gathered bun',(0,.125,1.75),(.069,.067,.068),mat,'Head')

CHARACTERS=[
    ('student_f01','Lucia','long',(.64,.37,.23),(.07,.13,.15),(.045,.40,.39),(.075,.13,.21)),
    ('student_f02','Valeria','bob',(.88,.62,.43),(.20,.075,.035),(.42,.28,.58),(.70,.65,.52)),
    ('student_f03','Camila','pony',(.32,.16,.10),(.025,.019,.018),(.72,.40,.12),(.12,.16,.19)),
    ('professor','Profesora Elena','bun',(.83,.62,.43),(.09,.035,.012),(.11,.31,.62),(.025,.028,.034)),
    ('student_m01','Mateo','curls',(.31,.15,.083),(.032,.024,.018),(.62,.16,.14),(.10,.15,.24)),
    ('student_m02','Diego','part',(.85,.61,.43),(.20,.12,.059),(.11,.22,.36),(.65,.56,.41)),
    ('student_m03','Andres','short',(.56,.32,.17),(.035,.027,.025),(.18,.35,.24),(.12,.13,.16)),
]
cast=[]
for idx,(filename,display,hairtype,skincolor,haircolor,coatcolor,pantscolor) in enumerate(CHARACTERS):
    parts=[]
    skin=material(display+' skin',skincolor,.64)
    hair=material(display+' hair',haircolor,.65)
    hair.use_backface_culling=True
    coat=material(display+' woven fabric',coatcolor,.82)
    pants=material(display+' trousers',pantscolor,.89)
    lip=material(display+' lips',tuple(c*.68 for c in skincolor),.65)
    female='_f' in filename or filename=='professor'
    professor=filename=='professor'
    width=.188 if female else .216
    torso('Tailored torso',[(.94,width*.89,.115,0),(1.08,width*.86,.121,0),(1.34,width,.137,0),(1.45,width*.95,.115,0),(1.49,.095,.072,0)],coat)
    ellipsoid('Pelvis',(0,0,.95),(width*.88,.115,.12),pants,'Hips')
    cylinder('Neck',(0,0,1.46),(0,0,1.60),.062,.058,skin,'Head')
    ellipsoid('Face',(0,-.006,1.681),(.138,.116,.168) if professor else (.132,.109,.164),skin,'Head',24,16)
    ellipsoid('Chin',(0,-.047,1.565),(.084,.065,.052) if professor else (.073,.064,.044),skin,'Head')
    ellipsoid('Nose',(0,-.119 if professor else -.113,1.676),(.019,.028,.029) if professor else (.021,.033,.033),skin,'Head',16,10)
    for s in (-1,1):
        ellipsoid('Ear',(s*.132,.001,1.68),(.027,.020,.044),skin,'Head',12,8)
        ellipsoid('Eye',(s*.050,-.119 if professor else -.103,1.712),(.034,.013,.018) if professor else (.030,.011,.014),white,'Head',16,8)
        ellipsoid('Iris',(s*.050,-.132 if professor else -.114,1.712),(.012,.004,.014) if professor else (.010,.004,.011),iris,'Head',12,8)
        ellipsoid('Pupil',(s*.050,-.136 if professor else -.118,1.712),(.005,.003,.008) if professor else (.004,.003,.006),pupil,'Head',12,8)
        ellipsoid('Catchlight',(s*.047,-.139 if professor else -.120,1.716),(.0025,.001,.0025),white,'Head',8,6)
        line('Eyebrow',[(s*.025,-.116,1.744),(s*.052,-.119,1.752),(s*.079,-.107,1.744)] if professor else [(s*.025,-.110,1.739),(s*.052,-.113,1.745),(s*.077,-.101,1.736)],.005 if professor else .006,hair,'Head')
    if professor:
        mouth=material('Warm smile',(.22,.055,.035),.7)
        ellipsoid('Smiling mouth',(0,-.116,1.623),(.042,.011,.018),mouth,'Head',20,10)
        ellipsoid('Smile teeth',(0,-.128,1.629),(.034,.004,.008),white,'Head',18,8)
        line('Lower smiling lip',[(-.036,-.119,1.628),(-.023,-.124,1.612),(0,-.127,1.608),(.023,-.124,1.612),(.036,-.119,1.628)],.004,lip,'Head')
    else:
        line('Soft smile',[(-.029,-.108,1.617),(0,-.118,1.612),(.029,-.108,1.617)],.004,lip,'Head')
    hair_cap(hair,hairtype)
    # Shirt inset, placket, collars, pockets and buttons distinguish actual clothing.
    if not professor:
        box('Shirt insert',(0,-.123,1.342),(.11,.024,.245),ivory,'Spine',.008)
    for s in (-1,1):
        collar=box('Folded collar',(s*.050,-.140,1.447),(.065,.018,.074),ivory,'Spine',.007)
        collar.rotation_euler.y=s*.4
        if not professor:
            pocket=box('Chest pocket',(s*.125,-.134,1.307),(.068,.012,.068),coat,'Spine',.006)
            line('Pocket seam',[(s*.092,-.146,1.332),(s*.157,-.146,1.332)],.0025,gold,'Spine')
    for z in ((1.10,1.19,1.28,1.37) if professor else (1.15,1.24,1.33)): ellipsoid('Blouse button' if professor else 'Brass button',(0 if professor else .018,-.141,z),(.006,.004,.006) if professor else (.009,.004,.009),ivory if professor else gold,'Spine',8,6)
    if filename=='professor':
        sapphire=material('Sapphire earrings',(.035,.15,.55),.25,.2)
        for s in (-1,1):
            ellipsoid('Blue earring',(s*.140,-.008,1.640),(.011,.010,.017),sapphire,'Head',12,8)
        torso('White blouse hem',[(.922,.162,.116,0),(.962,.168,.120,0)],ivory,'Spine')
        # Books form part of the skinned mesh, so every clip carries them with the hand.
        pages=material('Book pages',(.87,.85,.75),.9)
        redbook=material('Red book cover',(.64,.025,.055),.55)
        tealbook=material('Teal book cover',(.018,.30,.24),.55)
        for i,cover in enumerate((tealbook,redbook)):
            x=.367+i*.025; y=-.058-i*.062; z=1.047
            box('Book paper block',(x,y,z),(.205,.042,.295),pages,'Hand.L',.005)
            for face in (-1,1):
                box('Book hard cover',(x,y+face*.027,z),(.225,.012,.315),cover,'Hand.L',.006)
            box('Book spine',(x-.108,y,z),(.018,.066,.315),cover,'Hand.L',.007)
            for pagez in (z-.12,z-.105,z+.11,z+.125):
                line('Page edges',[(x-.09,y-.023,pagez),(x+.09,y-.023,pagez)],.0017,ivory,'Hand.L')
    else:
        # Canvas backpack and two straps are anchored to the torso.
        box('Canvas backpack',(0,.144,1.258),(.26,.12,.30),pants,'Spine',.04)
        box('Backpack pocket',(0,.213,1.19),(.19,.025,.12),coat,'Spine',.018)
        for s in (-1,1):
            line('Canvas strap',[(s*.15,.06,1.47),(s*.15,-.135,1.41),(s*.15,-.139,1.17),(s*.16,.075,1.12)],.018,pants,'Spine')
    for side,s in [('L',1),('R',-1)]:
        shoulder=(s*.234,0,1.425); elbow=(s*.31,0,1.17); wrist=(s*.345,-.015,.92)
        ellipsoid('Shoulder',shoulder,(.071,.083,.085),coat,'UpperArm.'+side)
        cylinder('Upper sleeve',shoulder,elbow,.076,.060,coat,'UpperArm.'+side)
        ellipsoid('Elbow',elbow,(.058,.063,.061),coat,'Forearm.'+side)
        cylinder('Lower sleeve',elbow,(s*.342,-.013,.96),.060,.043,coat,'Forearm.'+side)
        cylinder('Cuff',(s*.341,-.013,.98),(s*.347,-.016,.947),.047,.047,ivory,'Forearm.'+side)
        ellipsoid('Hand',(s*.35,-.02,.885),(.040,.030,.066),skin,'Hand.'+side)
        ellipsoid('Thumb',(s*.323,-.039,.897),(.019,.024,.035),skin,'Hand.'+side)
        cylinder('Trouser thigh',(s*.105,0,.96),(s*.105,0,.56),.077 if professor else .091,.059 if professor else .068,pants,'Thigh.'+side)
        ellipsoid('Knee',(s*.105,0,.56),(.059,.063,.067) if professor else (.068,.071,.073),pants,'Shin.'+side)
        cylinder('Trouser shin',(s*.105,0,.56),(s*.105,0,.145),.059 if professor else .068,.045 if professor else .052,pants,'Shin.'+side)
        box('Sole',(s*.105,-.055,.035),(.137,.265,.055),ivory if filename!='professor' else black,'Foot.'+side,.022)
        ellipsoid('Shoe',(s*.105,-.045,.088),(.069,.131,.062),black,'Foot.'+side)
        if filename!='professor':
            for i in range(3):
                line('Shoelace',[(s*.105-.035,-.06+i*.022,.139),(s*.105+.035,-.06+i*.022,.139)],.003,ivory,'Foot.'+side)
    mesh=mesh_join(parts,filename+'_Mesh')
    rig=make_rig(filename)
    mesh.parent=rig
    modifier=mesh.modifiers.new('Skeleton','ARMATURE');modifier.object=rig
    create_animations(rig)
    # The GLTF NLA exporter only visits enabled tracks.
    for track in rig.animation_data.nla_tracks: track.mute=False
    export(MODELS/(filename+'.glb'),[rig,mesh],True)
    for track in rig.animation_data.nla_tracks: track.mute=track.name!='Idle'
    rig.location.x=(idx-3)*.79
    cast.append((rig,mesh))

# Modular environment: each module stays in its local origin for export.
environment=[]
parts=[]
box('Travertine foundation',(0,0,-.085),(3,3,.17),floorstone,bevel=.035)
box('Inset paving',(0,0,-.006),(2.85,2.85,.024),stone,bevel=.025)
for s in (-1,1):
    box('Brass border X',(0,s*1.42,.009),(2.82,.018,.009),gold,bevel=.002)
    box('Brass border Y',(s*1.42,0,.009),(.018,2.82,.009),gold,bevel=.002)
floor=mesh_join(parts,'floor'); export(MODELS/'floor.glb',[floor]); environment.append(floor)
parts=[]
box('Wall body',(0,0,.63),(3,.20,1.20),darkstone,bevel=.025)
box('Wall plinth',(0,0,.10),(3,.29,.20),stone,bevel=.02)
box('Wall coping',(0,0,1.285),(3,.29,.13),stone,bevel=.025)
for s in (-1,1):
    box('Brass inlay',(0,s*.104,1.13),(2.95,.008,.025),gold,bevel=.003)
    for x in (-1.0,0,1.0):
        box('Masonry joint',(x,s*.105,.61),(.012,.005,.92),stone,bevel=.001)
wall=mesh_join(parts,'wall'); export(MODELS/'wall.glb',[wall]); environment.append(wall)
parts=[]
cylinder('Stepped foot',(0,0,0),(0,0,.12),1.05,1.05,darkstone,vertices=48)
cylinder('Podium limestone',(0,0,.12),(0,0,.56),.97,.88,stone,vertices=48)
cylinder('Brass band',(0,0,.49),(0,0,.54),.90,.90,gold,vertices=48)
cylinder('Polished cap',(0,0,.56),(0,0,.63),.93,.93,darkstone,vertices=48)
ring('Inlaid ring',(0,0,.638),.76,.012,gold,rot=(0,0,0))
podium=mesh_join(parts,'podium');export(MODELS/'podium.glb',[podium]);environment.append(podium)
parts=[]
bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=.27,location=(0,0,0))
crystal=bpy.context.object; crystal.scale=(.73,.73,1.4);finish(crystal,'Crystal',glow,smooth=False)
ring('Floating halo',(0,0,0),.30,.014,gold,rot=(.40,.55,0))
for s in (-1,1):
    cylinder('Crystal tip',(0,0,s*.36),(0,0,s*.43),.045,.006,gold,vertices=6)
glyph=mesh_join(parts,'glyph');export(MODELS/'glyph.glb',[glyph]);environment.append(glyph)
parts=[]
box('Pillar foot',(0,0,.10),(.52,.52,.20),darkstone,bevel=.025)
box('Pillar shaft',(0,0,.84),(.34,.34,1.42),stone,bevel=.035)
for z in (.25,1.40):box('Pillar brass band',(0,0,z),(.365,.365,.034),gold,bevel=.008)
box('Pillar capital',(0,0,1.58),(.49,.49,.16),darkstone,bevel=.026)
ellipsoid('Lantern',(0,0,1.77),(.12,.12,.14),glow)
pillar=mesh_join(parts,'pillar');export(MODELS/'pillar.glb',[pillar]);environment.append(pillar)

# Authoring scene: source contains editable character meshes, skeletons, actions,
# and modular environment meshes. The render is separate from game exports.
for i,obj in enumerate(environment):
    obj.location=(0,8+i*4,0)
    obj.hide_render=True
parts=[]
stage=box('Gallery floor',(0,0,-.065),(11,7,.10),material('Gallery charcoal',(.025,.055,.06),.72),bevel=.02)
for idx,(rig,mesh) in enumerate(cast):
    # Place discreet stone discs under the seven characters.
    x=rig.location.x
    cylinder('Display base',(x,0,-.02),(x,0,.015),.36,.36,darkstone,vertices=48)
    ring('Display brass',(x,0,.018),.33,.008,gold,rot=(0,0,0))
    bpy.ops.object.text_add(location=(x,-.53,.012),rotation=(math.pi/2,0,0))
    txt=bpy.context.object;txt.name='Label '+CHARACTERS[idx][1];txt.data.body=CHARACTERS[idx][1]
    txt.data.align_x='CENTER';txt.data.size=.10;txt.data.extrude=.0005;txt.data.materials.append(ivory)

def aim(obj,point):obj.rotation_euler=(Vector(point)-obj.location).to_track_quat('-Z','Y').to_euler()
world=bpy.data.worlds.new('Studio environment') if not bpy.data.worlds else bpy.data.worlds[0]
bpy.context.scene.world=world;world.use_nodes=True
world.node_tree.nodes['Background'].inputs[0].default_value=(.11,.15,.18,1)
world.node_tree.nodes['Background'].inputs[1].default_value=.45
for name,loc,power,color,size in [('Key',(1,-4,6),1600,(1,.84,.65),7),('Fill',(-4,-1,3),1100,(.57,.79,1),5),('Rim',(2,3,4),1900,(.55,1,.86),5)]:
    bpy.ops.object.light_add(type='AREA',location=loc)
    obj=bpy.context.object;obj.name=name;obj.data.energy=power;obj.data.color=color;obj.data.shape='DISK';obj.data.size=size;aim(obj,(0,0,1))
bpy.ops.object.camera_add(location=(3.4,-12,4.4))
camera=bpy.context.object;aim(camera,(0,0,.95));camera.data.type='ORTHO';camera.data.ortho_scale=6.55
scene=bpy.context.scene;scene.camera=camera
scene.render.engine='CYCLES';scene.cycles.samples=48;scene.cycles.use_denoising=True
scene.render.resolution_x=1920;scene.render.resolution_y=900;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.render.film_transparent=False
scene.view_settings.view_transform='AgX'
scene.render.fps=30;scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(SOURCE/'academy.blend'))
if '--no-render' not in sys.argv:
    if not ONLY_PROFESSOR:
        scene.render.filepath=str(PREVIEWS/'academy_cast.png')
        bpy.ops.render.render(write_still=True)
        for rig,mesh in cast:
            for track in rig.animation_data.nla_tracks:
                track.mute=track.name!='Celebrate'
        scene.frame_set(33)
        scene.render.filepath=str(PREVIEWS/'academy_celebrate.png')
        bpy.ops.render.render(write_still=True)
    # Individual preview also makes the reference-inspired face and books readable.
    for obj in scene.objects:
        if obj.type not in ('LIGHT','CAMERA'):
            obj.hide_render=obj.name not in ('professor_Mesh','Gallery floor')
    for rig,mesh in cast:
        for track in rig.animation_data.nla_tracks:
            track.mute=track.name!='Idle'
    scene.frame_set(1)
    camera.location=(2.8,-7.5,2.9);aim(camera,(0,0,.95));camera.data.ortho_scale=2.25
    for obj in scene.objects:
        if obj.type=='LIGHT':obj.data.energy*=.6
    scene.render.resolution_x=1000;scene.render.resolution_y=1200
    scene.render.filepath=str(PREVIEWS/'professor_reference.png')
    bpy.ops.render.render(write_still=True)
manifest={
  'generator':'tools/generate_assets.py','coordinates':'GLB: Y up, face +Z, feet at Y=0',
  'characters':[{'file':x[0]+'.glb','name':x[1],'clips':['Idle','Walk','Celebrate']} for x in CHARACTERS],
  'props':['floor.glb','wall.glb','podium.glb','glyph.glb','pillar.glb'],
  'license':'Original project assets; no third-party models or textures.'}
(SOURCE/'manifest.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False)+'\n')
print('ACADEMY ASSETS COMPLETE')
