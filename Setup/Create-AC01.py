"""Create the AC01 reference-inspired robot, using FutureBattle's exported Quinn rig.
Run with Blender 5.3 --background --python Setup/Create-AC01.py.
All dimensions are metres; the original Epic bind transforms are retained.
"""
import bpy, math, json, random, sys, io
from mathutils import Vector, Matrix
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Art' / 'AC01'
OUT.mkdir(parents=True, exist_ok=True)
for d in ['Exports', 'Textures', 'Preview']: (OUT/d).mkdir(exist_ok=True)
random.seed(41)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(OUT/'RigReference/QuinnReference.fbx'), use_anim=False)
rig = next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
rig.matrix_world=Matrix.Scale(.01,4)
bpy.context.view_layer.update()
for o in list(bpy.context.scene.objects):
    if o!=rig: bpy.data.objects.remove(o,do_unlink=True)
rig.name='root'
rig.data.name='AC01_QuinnBindSkeleton'
rig.show_in_front=True
rig.data.display_type='STICK'
bones={b.name:rig.matrix_world@b.head_local for b in rig.data.bones}
parts=[]

def material(name,color,metal=.65,rough=.48,wear=True,glow=0):
    m=bpy.data.materials.new(name); m.use_nodes=True
    m.diffuse_color=(*color,1)
    n=m.node_tree.nodes; l=m.node_tree.links; p=n.get('Principled BSDF')
    p.inputs['Base Color'].default_value=(*color,1)
    p.inputs['Metallic'].default_value=metal; p.inputs['Roughness'].default_value=rough
    if wear:
        tex=n.new('ShaderNodeTexNoise'); tex.inputs['Scale'].default_value=62
        tex.inputs['Detail'].default_value=3; tex.inputs['Roughness'].default_value=.8
        coord=n.new('ShaderNodeTexCoord'); l.new(coord.outputs['Object'],tex.inputs['Vector'])
        ramp=n.new('ShaderNodeValToRGB')
        ramp.color_ramp.elements[0].position=.22
        ramp.color_ramp.elements[0].color=(*[v*.82 for v in color],1)
        ramp.color_ramp.elements[1].position=.78
        ramp.color_ramp.elements[1].color=(*[min(v*1.14,.8) for v in color],1)
        l.new(tex.outputs['Fac'],ramp.inputs[0]); l.new(ramp.outputs[0],p.inputs['Base Color'])
        bump=n.new('ShaderNodeBump'); bump.inputs['Strength'].default_value=.14; bump.inputs['Distance'].default_value=.0006
        l.new(tex.outputs['Fac'],bump.inputs['Height']); l.new(bump.outputs[0],p.inputs['Normal'])
    if glow:
        p.inputs['Emission Color'].default_value=(*color,1); p.inputs['Emission Strength'].default_value=glow
    return m

armor=material('AC01_Painted_Slate',(.062,.082,.116),.58,.43)
edge=material('AC01_Exposed_Titanium',(.14,.15,.17),.82,.36)
dark=material('AC01_Recess_Gunmetal',(.038,.048,.061),.7,.42)
rubber=material('AC01_Flexible_Graphite',(.014,.019,.025),.12,.72)
bronze=material('AC01_Burnished_Copper',(.24,.12,.064),.78,.4)
visor=material('AC01_Ruby_Visor',(.12,.002,.009),.68,.2,False,.15)
eye=material('AC01_Red_Sensors',(1,.008,.012),.25,.22,False,5)
white=material('AC01_Stencils',(.63,.67,.69),.2,.65,False)
mats=[armor,edge,dark,rubber,bronze,visor,eye,white]

def finish(o,name,mat,bone,detail=0,bevel=0):
    o.name=name; o.data.materials.clear();o.data.materials.append(mat)
    bpy.context.view_layer.objects.active=o
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        mod=o.modifiers.new('Machined edge','BEVEL');mod.width=bevel;mod.segments=1
        bpy.ops.object.modifier_apply(modifier=mod.name)
    if o.type=='MESH':
        group=o.vertex_groups.new(name=bone);group.add(list(range(len(o.data.vertices))),1,'REPLACE')
        o['detail']=detail
        for f in o.data.polygons: f.use_smooth=False
    parts.append(o)
    return o

def box(name,loc,size,mat,bone,bevel=.008,detail=0,rot=None):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
    o=bpy.context.object;o.dimensions=size
    if rot is not None:o.rotation_mode='QUATERNION';o.rotation_quaternion=rot
    return finish(o,name,mat,bone,detail,bevel)

def sphere(name,loc,size,mat,bone,detail=0):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16,ring_count=8,radius=1,location=loc)
    o=bpy.context.object;o.scale=size
    o=finish(o,name,mat,bone,detail)
    for p in o.data.polygons:p.use_smooth=True
    return o

def cyl(name,a,b,r,mat,bone,vertices=16,r2=None,detail=0):
    a,b=Vector(a),Vector(b);v=b-a
    if v.length<1e-6:return None
    bpy.ops.mesh.primitive_cone_add(vertices=vertices,radius1=r,radius2=r if r2 is None else r2,depth=v.length,location=(a+b)*.5)
    o=bpy.context.object;o.rotation_mode='QUATERNION';o.rotation_quaternion=v.to_track_quat('Z','Y')
    o=finish(o,name,mat,bone,detail)
    for p in o.data.polygons: p.use_smooth=len(p.vertices)==4
    return o

def plate(name,c,w,h,d,mat,bone,outline=None,detail=0):
    # Front is -Y. Chamfered silhouette, bevelled metal edges.
    if outline is None: outline=[(-.38,.5),(.38,.5),(.5,.32),(.46,-.33),(.26,-.5),(-.26,-.5),(-.46,-.33),(-.5,.32)]
    x,y,z=c
    verts=[(x+u*w,y+dy,z+v*h) for dy in [-d/2,d/2] for u,v in outline]
    n=len(outline);faces=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]
    faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(o)
    return finish(o,name,mat,bone,detail,min(.006,d*.24))

def bolt(loc,bone,axis=(0,-1,0),r=.0055):
    c=Vector(loc);v=Vector(axis)
    cyl('Hex fastener',c-v*.002,c+v*.003,r,edge,bone,6,detail=2)
    cyl('Fastener socket',c+v*.003,c+v*.0035,r*.42,dark,bone,6,detail=2)

def hose(name,points,r,bone,mat=rubber,ribbed=True):
    # Polygonal flexible conduit, capped and watertight.
    points=[Vector(p) for p in points]
    verts=[];faces=[];count=8
    for j,c in enumerate(points):
        tangent=(points[min(j+1,len(points)-1)]-points[max(j-1,0)]).normalized()
        u=tangent.cross(Vector((1,0,0)))
        if u.length<.1:u=tangent.cross(Vector((0,1,0)))
        u.normalize();v=tangent.cross(u)
        for i in range(count):verts.append(c+r*(math.cos(i*math.tau/count)*u+math.sin(i*math.tau/count)*v))
    for j in range(len(points)-1):
        for i in range(count):faces.append((j*count+i,j*count+(i+1)%count,(j+1)*count+(i+1)%count,(j+1)*count+i))
    faces.extend([tuple(range(count-1,-1,-1)),tuple((len(points)-1)*count+i for i in range(count))])
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(o);finish(o,name,mat,bone,1)
    for p in mesh.polygons:p.use_smooth=True
    if ribbed:
        for j in range(len(points)-1):
            a,b=points[j:j+2];delta=b-a
            num=max(1,int(delta.length/.009))
            for i in range(num):
                c=a+delta*(i+.5)/num
                cyl('Conduit rib',c-delta.normalized()*.0015,c+delta.normalized()*.0015,r*1.18,edge,bone,8,detail=2)

def text_mark(text,loc,size,bone):
    bpy.ops.object.text_add(location=loc,rotation=(math.pi/2,0,0))
    o=bpy.context.object;o.data.body=text;o.data.align_x='CENTER';o.data.size=size;o.data.extrude=.00015
    bpy.ops.object.convert(target='MESH');return finish(bpy.context.object,'Stencil '+text,white,bone,2)

def segment_box(name,a,b,width,depth,mat,bone,front=0,bevel=.008,detail=0):
    a,b=Vector(a),Vector(b)
    q=(b-a).to_track_quat('Z','Y')
    if width<.04:
        return box(name,(a+b)*.5+Vector((0,front,0)),(width,depth,(b-a).length),mat,bone,bevel,detail,q)
    center=(a+b)*.5+Vector((0,front,0));length=(b-a).length
    cross=[(-.32,-.5),(.32,-.5),(.5,-.28),(.5,.28),(.32,.5),(-.32,.5),(-.5,.28),(-.5,-.28)]
    rings=[(-.5,.72),(-.34,1),(.28,.96),(.5,.68)]
    verts=[center+q@Vector((x*width*scale,y*depth*scale,z*length)) for z,scale in rings for x,y in cross]
    faces=[tuple(range(7,-1,-1)),tuple(range(24,32))]
    for j in range(3):
        for i in range(8):faces.append((j*8+i,j*8+(i+1)%8,(j+1)*8+(i+1)%8,(j+1)*8+i))
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);bpy.context.collection.objects.link(o)
    return finish(o,name,mat,bone,detail,min(bevel,.004))

# Load bearing torso, with separate spine segments for crouching and aiming.
sphere('Torso understructure',(0,.012,1.315),(.215,.115,.208),dark,'spine_04')
plate('Main breastplate',(0,-.101,1.337),.405,.275,.105,armor,'spine_04',
      [(-.45,.5),(-.2,.44),(-.1,.25),(.1,.25),(.2,.44),(.45,.5),(.5,.12),(.43,-.43),(.12,-.5),(-.12,-.5),(-.43,-.43),(-.5,.12)])
plate('Sternum shield',(0,-.171,1.393),.122,.092,.024,edge,'spine_04')
plate('Sternum inset',(0,-.187,1.395),.092,.056,.012,armor,'spine_04')
for s in [-1,1]:
    box('Collar upright',(s*.153,-.005,1.464),(.083,.16,.182),armor,'spine_05',.018)
    plate('Collar face',(s*.153,-.098,1.46),.065,.125,.02,edge,'spine_05')
    box('Collar latch',(s*.15,-.116,1.43),(.031,.023,.041),bronze,'spine_05',.004,1)
    plate('Chest vent rim',(s*.12,-.17,1.307),.109,.089,.026,edge,'spine_04')
    box('Chest vent recess',(s*.12,-.188,1.307),(.08,.012,.058),dark,'spine_04',.004)
    for i in range(7):box('Chest cooling fin',(s*.12,-.198,1.282+i*.008),(.074,.008,.003),edge,'spine_04',.0005,1)
    for z in [1.24,1.18]:
        x=s*(.175 if z>1.2 else .14)
        cyl('Abdominal coupling',(x,-.02,z),(x,-.125,z),.032,edge,'spine_03',16)
        cyl('Abdominal coupling inset',(x,-.126,z),(x,-.137,z),.022,dark,'spine_03',12,detail=1)
    for x in [s*.175,s*.052]:bolt((x,-.158,1.37),'spine_04')

for i,(z,w,bn) in enumerate([(1.205,.205,'spine_03'),(1.12,.18,'spine_02'),(1.043,.164,'spine_01')]):
    sphere('Waist articulation',(0,0,z),(.135,.089,.068),rubber,bn)
    plate('Overlapping abdominal plate',(0,-.103,z),w,.09,.046,armor,bn)
    for s in [-1,1]:
        hose('Waist wiring',[(s*.08,.035,z+.04),(s*.134,-.015,z),(s*.1,-.05,z-.04)],.008,bn,ribbed=False)
text_mark('AC 01',(0,-.158,1.245),.016,'spine_03')

# Pelvis / hip protection.
sphere('Pelvis chassis',(0,.006,.934),(.172,.103,.105),dark,'pelvis')
box('Utility belt',(0,-.005,1.002),(.338,.18,.055),dark,'pelvis',.012)
plate('Pelvis front guard',(0,-.114,.916),.127,.177,.04,armor,'pelvis')
plate('Pelvis rear guard',(0,.12,.935),.241,.158,.052,armor,'pelvis')
for s in [-1,1]:
    box('Belt cartridge',(s*.133,-.1,1.003),(.064,.048,.058),armor,'pelvis',.008)
    hose('Hip bridge',[(s*.075,-.1,.996),(s*.14,-.125,1.02),(s*.19,-.062,1.045)],.012,'pelvis',edge,False)
    sphere('Hip bearing',bones['thigh_'+('l' if s>0 else 'r')],(.089,.077,.091),dark,'pelvis')

# Neck and helmet: enclosed angular face, wraparound ruby glass and red sensors.
for z in [1.51+i*.014 for i in range(7)]:
    cyl('Neck bellows',(0,.01,z-.004),(0,.01,z+.004),.055,rubber,'neck_02',16)
for s in [-1,1]:
    hose('Neck cable',[(s*.052,-.055,1.45),(s*.036,-.073,1.51),(s*.047,-.065,1.59)],.007,'neck_01',edge,False)
sphere('Helmet base',(0,.008,1.679),(.103,.104,.13),armor,'head')
plate('Face metal frame',(0,-.083,1.675),.193,.217,.076,edge,'head',
      [(-.33,.5),(.33,.5),(.5,.25),(.45,-.17),(.23,-.5),(-.23,-.5),(-.45,-.17),(-.5,.25)])
plate('Ruby face shield',(0,-.128,1.683),.166,.151,.021,visor,'head',
      [(-.36,.5),(-.18,.3),(.18,.3),(.36,.5),(.5,.28),(.42,-.05),(.17,-.5),(-.17,-.5),(-.42,-.05),(-.5,.28)])
plate('Helmet brow',(0,-.131,1.752),.143,.078,.031,armor,'head')
plate('Crown reinforcement',(0,-.064,1.79),.106,.042,.079,edge,'head')
plate('Chin respirator',(0,-.117,1.591),.094,.057,.04,armor,'head')
box('Chin intake',(0,-.143,1.587),(.036,.012,.024),dark,'head',.003)
for s in [-1,1]:
    x=s*.046
    sphere('Sensor bezel',(x,-.143,1.701),(.019,.007,.014),dark,'head',1)
    sphere('Red optical sensor',(x,-.15,1.701),(.008,.004,.008),eye,'head',1)
    cyl('Ear actuator',(s*.087,.011,1.683),(s*.118,.011,1.683),.041,dark,'head',20)
    cyl('Ear armored disc',(s*.117,.011,1.683),(s*.124,.011,1.683),.032,edge,'head',16)
    plate('Jaw cheek',(s*.071,-.091,1.621),.046,.085,.043,armor,'head')
    hose('Helmet breathing tube',[(s*.024,-.141,1.591),(s*.063,-.131,1.6),(s*.097,-.077,1.629),(s*.103,-.024,1.655)],.011,'head',edge,True)
    bolt((s*.047,-.152,1.76),'head',r=.004)
for z in [1.729,1.75,1.771]:box('Rear helmet rail',(0,.104,z),(.103,.013,.009),dark,'head',.002,1)

# Backpack with stepped housings, clamps, cooling grille and symmetric cable banks.
plate('Back powerpack',(0,.152,1.366),.306,.312,.13,armor,'spine_04')
plate('Backpack rear inset',(0,.227,1.376),.245,.219,.021,edge,'spine_04')
plate('Backpack center panel',(0,.242,1.38),.206,.171,.016,armor,'spine_04')
for s in [-1,1]:
    box('Backpack latch',(s*.083,.26,1.445),(.025,.026,.051),bronze,'spine_04',.005,1)
    box('Backpack utility cell',(s*.163,.137,1.393),(.05,.085,.145),armor,'spine_04',.01)
    for k in range(3):
        hose('Back power bus',[(s*.133,.154,1.38-k*.026),(s*.182,.207,1.3-k*.023),(s*.16,.248,1.252-k*.015),(s*.058,.25,1.254-k*.018)],.007,'spine_04',dark,True)
    hose('Shoulder power loop',[(s*.124,.124,1.489),(s*.205,.14,1.479),(s*.23,.142,1.41),(s*.211,.14,1.353)],.009,'spine_04',edge,False)
box('Back battery module',(0,.2,1.183),(.154,.091,.104),armor,'spine_03',.014)
box('Back radiator frame',(0,.158,1.084),(.185,.075,.075),edge,'spine_01',.01)
box('Back radiator void',(0,.204,1.084),(.155,.018,.056),dark,'spine_01',.002)
for i in range(12):box('Back radiator fin',(-.071+i*.013,.218,1.084),(.004,.011,.052),edge,'spine_01',.0007,1)
for x in [-.1,.1]:
    for z in [1.31,1.44]:bolt((x,.258,z),'spine_04',(0,1,0))

# Arms and legs use rigid armor sections. No cross-joint weights on metal plates.
for side,s in [('l',1),('r',-1)]:
    upper,fore,hand='upperarm_'+side,'lowerarm_'+side,'hand_'+side
    shoulder,elbow,wrist=(bones[b] for b in [upper,fore,hand])
    sphere('Shoulder ball',shoulder,(.096,.093,.096),dark,upper)
    # Shoulder bells wider than the Quinn body, while retaining existing joint positions.
    c=shoulder+Vector((s*.025,0,-.025))
    sphere('Shoulder bell core',c,(.125,.12,.117),edge,upper)
    plate('Shoulder blue panel',c+Vector((s*.025,-.085,-.005)),.175,.169,.07,armor,upper)
    plate('Shoulder upper cap',c+Vector((s*.023,-.045,.072)),.193,.062,.114,edge,upper)
    plate('Shoulder lower rim',c+Vector((s*.027,-.055,-.077)),.174,.042,.08,edge,upper)
    text_mark('AC 01',c+Vector((s*.023,-.125,.014)),.021,upper)
    for z in [-.05,.055]:bolt(c+Vector((s*.082,-.126,z)),upper)
    d=elbow-shoulder
    cyl('Upper arm servo',shoulder+d*.2,elbow-d*.19,.057,dark,upper,16)
    segment_box('Bicep armor',shoulder+d*.28,elbow-d*.21,.115,.114,armor,upper,front=-.016,bevel=.018)
    segment_box('Bicep spine',shoulder+d*.28+Vector((0,.065,0)),elbow-d*.21+Vector((0,.065,0)),.06,.039,edge,upper,bevel=.008)
    for k in [-1,1]:
        a=shoulder+d*.3+Vector((s*.052,k*.04,0));b=elbow-d*.17+Vector((s*.041,k*.035,0))
        cyl('Arm piston casing',a,a.lerp(b,.55),.016,dark,upper,12)
        cyl('Arm piston rod',a.lerp(b,.5),b,.009,edge,upper,10)
    cyl('Elbow hinge',elbow+Vector((-.065,0,0)),elbow+Vector((.065,0,0)),.054,edge,fore,20)
    cyl('Elbow cap',elbow+Vector((s*.066,0,0)),elbow+Vector((s*.075,0,0)),.039,armor,fore,16)
    v=wrist-elbow
    cyl('Forearm inner frame',elbow+v*.16,wrist-v*.1,.045,dark,fore,12)
    segment_box('Forearm gauntlet',elbow+v*.19,elbow+v*.65,.119,.111,armor,fore,-.005,.018)
    segment_box('Forearm taper',elbow+v*.61,wrist-v*.13,.089,.085,armor,fore,-.002,.012)
    for dx in [-.019,0,.019]:
        a=elbow+v*.49+Vector((dx,-.06,0));b=elbow+v*.91+Vector((dx,-.045,0))
        cyl('Forearm chrome rail',a,b,.007,edge,fore,10,detail=1)
    cyl('Wrist seal',wrist-v.normalized()*.03,wrist+v.normalized()*.015,.047,rubber,hand,16)
    # Palm and each articulated finger follow the real Quinn hand bones.
    hand_tail=(rig.matrix_world@rig.data.bones[hand].tail_local)
    finger_bases=[bones[n] for n in ['index_01_'+side,'middle_01_'+side,'ring_01_'+side,'pinky_01_'+side]]
    center=sum(finger_bases,Vector())/4
    segment_box('Palm glove',wrist,center,.076,.043,dark,hand,bevel=.011)
    segment_box('Dorsal hand plate',wrist+(center-wrist)*.15,center,.079,.034,armor,hand,front=-.02,bevel=.009)
    for finger in ['thumb','index','middle','ring','pinky']:
        for idx in range(1,4):
            name=f'{finger}_{idx:02d}_{side}'
            if name not in bones:continue
            a=bones[name]
            nxt=f'{finger}_{idx+1:02d}_{side}'
            b=bones[nxt] if nxt in bones else rig.matrix_world@rig.data.bones[name].tail_local
            if (b-a).length>.055: b=a+(b-a).normalized()*.021
            sphere('Finger knuckle',a,(.010,.010,.010),dark,name,1)
            segment_box('Armored finger',a.lerp(b,.1),a.lerp(b,.85),.016,.017,edge,name,bevel=.003,detail=1)
    # Legs.
    thigh,calf,foot='thigh_'+side,'calf_'+side,'foot_'+side
    hip,knee,ankle=(bones[b] for b in [thigh,calf,foot])
    v=knee-hip
    cyl('Thigh internal strut',hip+v*.08,knee-v*.06,.072,dark,thigh,16)
    segment_box('Thigh shell',hip+v*.15,knee-v*.22,.173,.169,edge,thigh,bevel=.025)
    segment_box('Thigh front plate',hip+v*.12,knee-v*.36,.143,.071,armor,thigh,front=-.078,bevel=.019)
    segment_box('Outer thigh guard',hip+v*.13+Vector((s*.087,.008,0)),knee-v*.2+Vector((s*.081,.008,0)),.044,.137,armor,thigh,bevel=.012)
    for k in [-1,1]:
        a=hip+v*.22+Vector((k*.038,.077,0));b=knee-v*.17+Vector((k*.038,.073,0))
        cyl('Thigh rear piston',a,a.lerp(b,.54),.017,dark,thigh,12)
        cyl('Thigh rear polished rod',a.lerp(b,.48),b,.01,edge,thigh,10)
    plate('Thigh upper tag',(hip.x,-.137,.84),.067,.025,.014,bronze,thigh,detail=1)
    for dz in [-.025,.025]:bolt((hip.x+s*.046,-.123,.78+dz),thigh)
    cyl('Knee through axle',knee+Vector((-.087,0,0)),knee+Vector((.087,0,0)),.058,dark,calf,20)
    for k in [-1,1]:cyl('Knee cap',knee+Vector((k*.084,0,0)),knee+Vector((k*.094,0,0)),.045,edge,calf,16)
    plate('Floating kneepad',knee+Vector((0,-.079,.011)),.142,.175,.064,armor,calf)
    plate('Kneepad upper ridge',knee+Vector((0,-.115,.065)),.107,.036,.019,edge,calf)
    v=ankle-knee
    cyl('Shin inner strut',knee+v*.14,ankle-v*.07,.056,dark,calf,14)
    segment_box('Shin main armor',knee+v*.3,ankle-v*.13,.155,.133,armor,calf,front=-.012,bevel=.02)
    plate('Shin upper overlapping panel',(knee.x,-.082,.326),.157,.139,.07,edge,calf)
    plate('Shin blue inset',(knee.x,-.123,.329),.118,.098,.016,armor,calf)
    plate('Ankle front shield',(ankle.x,-.068,.139),.139,.111,.062,armor,calf)
    for k in [-1,1]:
        a=knee+v*.26+Vector((k*.04,.074,0));b=ankle-v*.12+Vector((k*.04,.064,0))
        cyl('Calf hydraulic ram',a,a.lerp(b,.5),.015,dark,calf,12)
        cyl('Calf hydraulic rod',a.lerp(b,.45),b,.009,edge,calf,12)
        cyl('Calf piston collar',a.lerp(b,.47),a.lerp(b,.52),.02,edge,calf,12,detail=1)
    # Broad armored boots stay inside the existing infantry capsule.
    box('Boot rubber sole',(ankle.x,-.074,.031),(.16,.259,.044),rubber,foot,.012)
    box('Boot heel',(ankle.x,.018,.078),(.139,.111,.098),edge,foot,.018)
    box('Boot instep',(ankle.x,-.075,.081),(.143,.145,.088),armor,foot,.018)
    box('Boot toe',(ankle.x,-.161,.058),(.159,.075,.06),edge,foot,.014)
    for x in [-.045,.045]:box('Toe armored digit',(ankle.x+x,-.175,.057),(.045,.072,.066),armor,foot,.01)
    for i in range(5):box('Sole tread',(ankle.x,-.17+i*.043,.01),(.153,.018,.014),dark,foot,.002,1)

print('AC01 built parts:',len(parts),flush=True)

# Preserve part selection information for efficient distance LODs before joining.
def join_meshes(objs,name):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:o.select_set(True)
    bpy.context.view_layer.objects.active=objs[0];bpy.ops.object.join()
    obj=bpy.context.object;obj.name=name
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    return obj

lodparts={1:[],2:[]}
for o in parts:
    if o.type!='MESH':continue
    for lod,max_detail in [(1,1),(2,0)]:
        if o['detail']<=max_detail:
            c=o.copy();c.data=o.data.copy();bpy.context.collection.objects.link(c);lodparts[lod].append(c)
body=join_meshes(parts,'SK_AC01_LOD0')
for m in list(body.modifiers):body.modifiers.remove(m)
tri=body.modifiers.new('Export triangulation','TRIANGULATE');bpy.ops.object.modifier_apply(modifier=tri.name)
for lod in [1,2]:
    o=join_meshes(lodparts[lod],f'SK_AC01_LOD{lod}')
    dec=o.modifiers.new('Distance simplification','DECIMATE');dec.ratio=.60 if lod==1 else .29
    bpy.ops.object.modifier_apply(modifier=dec.name)
    tri=o.modifiers.new('Export triangulation','TRIANGULATE');bpy.ops.object.modifier_apply(modifier=tri.name)
    o.hide_render=True;o.hide_set(True)

# UV maps are authored for every distance LOD. Bake a single atlas shared by LOD0.
bpy.ops.object.select_all(action='DESELECT');body.hide_set(False);body.select_set(True);bpy.context.view_layer.objects.active=body
bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.smart_project(angle_limit=math.radians(70),island_margin=.003,area_weight=.6)
bpy.ops.object.mode_set(mode='OBJECT')
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=1
scene.render.bake.use_clear=True;scene.render.bake.margin=8
images={}
for key in ['BaseColor','ORM','Emissive']:
    image=bpy.data.images.new('T_AC01_'+key,width=2048,height=2048,alpha=False)
    if key!='BaseColor':image.colorspace_settings.name='Non-Color'
    images[key]=image

original={}
for m in mats:
    n=m.node_tree.nodes;p=n.get('Principled BSDF');out=n.get('Material Output')
    original[m.name]={'node':p,'color':p.inputs['Base Color'].links[0].from_socket if p.inputs['Base Color'].is_linked else tuple(p.inputs['Base Color'].default_value),
                      'rough':p.inputs['Roughness'].default_value,'metal':p.inputs['Metallic'].default_value,
                      'emit':tuple(p.inputs['Emission Color'].default_value),'strength':p.inputs['Emission Strength'].default_value}
    target=n.new('ShaderNodeTexImage');target.name='BAKE_TARGET'

for key,image in images.items():
    for m in mats:
        n=m.node_tree.nodes;l=m.node_tree.links;info=original[m.name]
        target=n.get('BAKE_TARGET');target.image=image;n.active=target
        for node in n:node.select=node==target
        em=n.get('BAKE_EMIT') or n.new('ShaderNodeEmission');em.name='BAKE_EMIT'
        for link in list(em.inputs['Color'].links):l.remove(link)
        if key=='BaseColor':
            value=info['color']
            if isinstance(value,tuple):em.inputs['Color'].default_value=value
            else:l.new(value,em.inputs['Color'])
        elif key=='ORM':em.inputs['Color'].default_value=(1,info['rough'],info['metal'],1)
        else:em.inputs['Color'].default_value=tuple(v*min(info['strength'],1) for v in info['emit'][:3])+(1,)
        l.new(em.outputs[0],n.get('Material Output').inputs['Surface'])
    print('Baking',key,flush=True)
    bpy.ops.object.bake(type='EMIT')
    image.filepath_raw=str(OUT/'Textures'/('T_AC01_'+key+'.png'));image.file_format='PNG';image.save()
for m in mats:
    m.node_tree.links.new(original[m.name]['node'].outputs[0],m.node_tree.nodes.get('Material Output').inputs['Surface'])

atlas=bpy.data.materials.new('M_AC01_Atlas');atlas.use_nodes=True
n=atlas.node_tree.nodes;l=atlas.node_tree.links;p=n.get('Principled BSDF')
for key,image in images.items():
    node=n.new('ShaderNodeTexImage');node.image=image;node.label=key
    if key=='BaseColor':l.new(node.outputs['Color'],p.inputs['Base Color'])
    elif key=='ORM':
        sep=n.new('ShaderNodeSeparateColor');l.new(node.outputs['Color'],sep.inputs[0]);l.new(sep.outputs['Green'],p.inputs['Roughness']);l.new(sep.outputs['Blue'],p.inputs['Metallic'])
    else:l.new(node.outputs['Color'],p.inputs['Emission Color']);p.inputs['Emission Strength'].default_value=4
body.data.materials.clear();body.data.materials.append(atlas)
for f in body.data.polygons:f.material_index=0

# Transfer UVs from LOD0 so all LODs use the same atlas and a single material slot.
for lod in [1,2]:
    o=bpy.data.objects[f'SK_AC01_LOD{lod}'];o.hide_set(False)
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    mod=o.modifiers.new('Atlas UV transfer','DATA_TRANSFER');mod.object=body;mod.use_loop_data=True;mod.data_types_loops={'UV'};mod.loop_mapping='POLYINTERP_NEAREST'
    bpy.ops.object.datalayout_transfer(modifier=mod.name)
    bpy.ops.object.modifier_apply(modifier=mod.name)
    o.data.materials.clear();o.data.materials.append(atlas)
    for f in o.data.polygons:f.material_index=0
    o.hide_set(True)

lods=[body]+[bpy.data.objects[f'SK_AC01_LOD{i}'] for i in [1,2]]
for o in lods:
    o.parent=rig;o.matrix_parent_inverse=rig.matrix_world.inverted()
    mod=o.modifiers.new('Quinn skeleton','ARMATURE');mod.object=rig
    # Every rigid metal vertex must have exactly one normalized influence.
    assert all(len(v.groups)==1 and abs(v.groups[0].weight-1)<1e-6 for v in o.data.vertices)
    assert all(g.name in rig.data.bones for g in o.vertex_groups)

# Export bind meshes independently: Unreal's original skeleton is selected on import.
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
for o in lods:
    bpy.ops.object.select_all(action='DESELECT');o.hide_set(False);o.select_set(True);rig.select_set(True);bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.fbx(filepath=str(OUT/'Exports'/(o.name+'.fbx')),use_selection=True,object_types={'ARMATURE','MESH'},
        add_leaf_bones=False,bake_anim=False,use_armature_deform_only=False,axis_forward='-Y',axis_up='Z',
        apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',mesh_smooth_type='FACE',path_mode='RELATIVE')
    o.hide_set(o!=body)

report={'source_mesh':'SKM_Quinn_Simple','skeleton':'/Game/Characters/Mannequins/Meshes/SK_Mannequin',
        'rig_bones':len(rig.data.bones),'materials':1,'atlas':2048,
        'lods':{o.name:{'vertices':len(o.data.vertices),'triangles':len(o.data.polygons)} for o in lods},
        'weighted_vertices_checked':True,'body_height_m':max(v.co.z for v in body.data.vertices)-min(v.co.z for v in body.data.vertices)}
(OUT/'validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('AC01_GEOMETRY_OK',json.dumps(report),flush=True)

# Retain genuine project idle and walk actions in the .blend for animation previews.
for file,name in [('QuinnIdle.fbx','AC01_RifleIdle'),('QuinnWalk.fbx','AC01_RifleWalk')]:
    before=set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(OUT/'RigReference'/file),use_anim=True)
    imported=set(bpy.data.objects)-before
    ar=next((o for o in imported if o.type=='ARMATURE'),None)
    if ar and ar.animation_data and ar.animation_data.action:
        act=ar.animation_data.action;act.name=name;act.use_fake_user=True
        # Preview in place; the actual game continues to use its original UE clips.
        for layer in act.layers:
            for strip in layer.strips:
                for slot in act.slots:
                    bag=strip.channelbag(slot)
                    if bag:
                        for curve in list(bag.fcurves):
                            if curve.data_path=='location':bag.fcurves.remove(curve)
    for o in imported:bpy.data.objects.remove(o,do_unlink=True)

# Studio scene and cameras.
for o in lods:o.hide_render=o!=body;o.hide_set(o!=body)
rig.hide_render=True
floor=material('Studio floor',(.115,.135,.163),.0,.8,False)
box('Studio plinth',(0,0,-.025),(200,200,.04),floor,'pelvis',bevel=0) # remove studio from rigged parts list below
scene.world=bpy.data.worlds.new('AC01 Studio World')
scene.world.color=(.15,.15,.15)
world=scene.world;world.use_nodes=True;world.node_tree.nodes['Background'].inputs[0].default_value=(.18,.21,.26,1);world.node_tree.nodes['Background'].inputs[1].default_value=.45
def area(name,pos,power,size,color):
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size;data.color=color
    o=bpy.data.objects.new(name,data);scene.collection.objects.link(o);o.location=pos;o.rotation_euler=(Vector((0,0,1))-o.location).to_track_quat('-Z','Y').to_euler()
area('Key softbox',(2,-3,4),500,3,(.81,.9,1))
area('Warm fill',(-2,-1,2),270,2,(1,.79,.64))
area('Rim softbox',(1,2.5,3),700,2,(.62,.78,1))
camdata=bpy.data.cameras.new('Presentation camera');cam=bpy.data.objects.new('Presentation camera',camdata);scene.collection.objects.link(cam);scene.camera=cam
camdata.type='ORTHO';camdata.ortho_scale=2.16
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1000;scene.render.resolution_y=1200;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.view_settings.view_transform='AgX'
def camera(pos,target=(0,0,.92),scale=2.16):
    cam.location=pos;cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler();camdata.ortho_scale=scale
camera((2.5,-4,2.1))
bpy.ops.object.select_all(action='DESELECT');body.hide_set(False);body.select_set(True);bpy.context.view_layer.objects.active=body
for screen in bpy.data.screens:
    for a in screen.areas:
        if a.type=='VIEW_3D':
            a.spaces.active.region_3d.view_distance=2.6;a.spaces.active.region_3d.view_location=Vector((0,0,.9));a.spaces.active.shading.type='MATERIAL'
for image in images.values():image.pack()
scene['AC01_notes']='Quinn bind rig. Weapon-free body. Rigid armor weights; original project idle/walk actions retained. See Art/AC01/README.md.'
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'AC01_Robot.blend'))
for name,pos in [('Hero',(2.5,-4,2.1)),('Front',(0,-4,1)),('Back',(0,4,1)),('Side',(4,0,1))]:
    camera(pos);scene.render.filepath=str(OUT/'Preview'/(name+'.png'));bpy.ops.render.render(write_still=True)
print('AC01_COMPLETE',flush=True)
