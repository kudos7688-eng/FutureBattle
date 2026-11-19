"""Check saved Blender rig, skin weights, atlas and real project animation."""
import bpy, json, math, bmesh
from pathlib import Path
from mathutils import Vector
out=Path(__file__).resolve().parents[1]/'Art/AC01'
bpy.ops.wm.open_mainfile(filepath=str(out/'AC01_Robot.blend'))
rig=bpy.data.objects['root'];body=bpy.data.objects['SK_AC01_LOD0']
reference=json.loads((out/'RigReference/bones.json').read_text(encoding='utf-8'))
max_bind_error=max(((rig.matrix_world@rig.data.bones[name].head_local)-Vector(value['head'])).length for name,value in reference['bones'].items())
assert max_bind_error<1e-5,max_bind_error
report={'bind_head_error_m':max_bind_error,'lods':{}}
for i in range(3):
    obj=bpy.data.objects[f'SK_AC01_LOD{i}']
    assert len(obj.data.materials)==1
    assert len(obj.data.uv_layers)==1
    assert all(len(v.groups)==1 and abs(v.groups[0].weight-1)<1e-6 for v in obj.data.vertices)
    assert all(all(math.isfinite(c) for c in v.co) for v in obj.data.vertices)
    assert all(all(math.isfinite(c) for c in p.uv) for p in obj.data.uv_layers.active.data)
    bm=bmesh.new();bm.from_mesh(obj.data)
    nonmanifold=sum(not e.is_manifold for e in bm.edges);bm.free()
    report['lods'][obj.name]={'triangles':len(obj.data.polygons),'nonmanifold_edges':nonmanifold}
assert all(im.packed_file for im in bpy.data.images if im.name.startswith('T_AC01_'))
rig.animation_data_create()
report['animations']={}
for name in ['AC01_RifleIdle','AC01_RifleWalk']:
    action=bpy.data.actions[name]
    rig.animation_data.action=action
    if len(action.slots):rig.animation_data.action_slot=action.slots[0]
    points=[];sizes=[]
    for frame in [action.frame_range[0],sum(action.frame_range)/2,action.frame_range[1]-.1]:
        bpy.context.scene.frame_set(int(frame));bpy.context.view_layer.update()
        points.append(list(rig.matrix_world@rig.pose.bones['foot_l'].head))
        eval_obj=body.evaluated_get(bpy.context.evaluated_depsgraph_get())
        coords=[eval_obj.matrix_world@v.co for v in eval_obj.data.vertices]
        sizes.append([max(v[k] for v in coords)-min(v[k] for v in coords) for k in range(3)])
    assert all(.8<size[2]<2.2 for size in sizes),(name,sizes)
    report['animations'][name]={'foot_positions_m':points,'bounds_sizes_m':sizes}
walk=report['animations']['AC01_RifleWalk']['foot_positions_m']
assert len({tuple(round(v,5) for v in p) for p in walk})>1,'Walk must move foot bones'
report['walk_deforms_mesh']=True
(out/'blender-validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('AC01_BLENDER_VALIDATION_OK',json.dumps(report),flush=True)
scene=bpy.context.scene
rig.animation_data.action=bpy.data.actions['AC01_RifleWalk'];rig.animation_data.action_slot=rig.animation_data.action.slots[0]
scene.frame_set(10)
bpy.context.view_layer.update()
eval_obj=body.evaluated_get(bpy.context.evaluated_depsgraph_get())
coords=[eval_obj.matrix_world@v.co for v in eval_obj.data.vertices]
center=Vector([(max(v[k] for v in coords)+min(v[k] for v in coords))*.5 for k in range(3)])
scene.camera.location=center+Vector((2.5,-4,1.2))
scene.camera.rotation_euler=(center-scene.camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.resolution_x=800;scene.render.resolution_y=960;scene.cycles.samples=24
scene.render.filepath=str(out/'Preview/Walk.png')
bpy.ops.render.render(write_still=True)
