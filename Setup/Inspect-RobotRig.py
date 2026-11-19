import bpy, json
from pathlib import Path
root = Path(__file__).resolve().parents[1] / 'Art' / 'AC01'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(root / 'RigReference' / 'QuinnReference.fbx'), use_anim=False)
rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
data = {'name':rig.name, 'scale':list(rig.scale), 'matrix':[list(r) for r in rig.matrix_world], 'bones':{b.name:{'head':list(rig.matrix_world @ b.head_local), 'tail':list(rig.matrix_world @ b.tail_local), 'parent':b.parent.name if b.parent else None} for b in rig.data.bones}}
(root / 'RigReference' / 'bones.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in data['bones'].items() if k in ['pelvis','spine_01','spine_02','spine_03','spine_04','spine_05','neck_01','neck_02','head','upperarm_l','lowerarm_l','hand_l','thigh_l','calf_l','foot_l','ball_l']}))
