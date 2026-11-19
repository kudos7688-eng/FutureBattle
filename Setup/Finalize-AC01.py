"""Refresh explicit FBX unit transforms in the saved model, without rebaking."""
import bpy
from mathutils import Matrix
from pathlib import Path
out=Path(__file__).resolve().parents[1]/'Art/AC01'
bpy.ops.wm.open_mainfile(filepath=str(out/'AC01_Robot.blend'))
rig=bpy.data.objects['root'];rig.matrix_world=Matrix.Scale(.01,4)
for name in ['AC01_RifleIdle','AC01_RifleWalk']:
    action=bpy.data.actions[name]
    for layer in action.layers:
        for strip in layer.strips:
            for slot in action.slots:
                bag=strip.channelbag(slot)
                if bag:
                    for curve in list(bag.fcurves):
                        if curve.data_path=='location':bag.fcurves.remove(curve)
bpy.context.view_layer.update()
for i in range(3):
    o=bpy.data.objects[f'SK_AC01_LOD{i}'];o.hide_set(False)
    o.matrix_parent_inverse=rig.matrix_world.inverted();o.matrix_basis=Matrix.Identity(4)
bpy.context.view_layer.update()
assert abs((rig.matrix_world@rig.data.bones['head'].head_local).z-1.625031)<1e-4
for i in range(3):
    o=bpy.data.objects[f'SK_AC01_LOD{i}']
    bpy.ops.object.select_all(action='DESELECT');o.select_set(True);rig.select_set(True);bpy.context.view_layer.objects.active=rig
    bpy.ops.export_scene.fbx(filepath=str(out/'Exports'/(o.name+'.fbx')),use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,use_armature_deform_only=False,axis_forward='-Y',axis_up='Z',apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',mesh_smooth_type='FACE',path_mode='RELATIVE')
    o.hide_set(i!=0)
bpy.ops.object.select_all(action='DESELECT');body=bpy.data.objects['SK_AC01_LOD0'];body.select_set(True);bpy.context.view_layer.objects.active=body
bpy.context.scene.camera.location=(2.5,-4,2.1)
from mathutils import Vector
bpy.context.scene.camera.rotation_euler=(Vector((0,0,.92))-bpy.context.scene.camera.location).to_track_quat('-Z','Y').to_euler()
bpy.ops.wm.save_as_mainfile(filepath=str(out/'AC01_Robot.blend'))
print('AC01_BIND_FINALIZED')
