"""Refresh only the suit geometry, preserving the installed visor material and shared rig."""
import unreal as u
from pathlib import Path
root=Path(__file__).resolve().parents[1]
folder=root/'Art/AC01_Tripo_v2/Exports'
asset='/Game/Art/AC01T2/SK_AC01T2'
mesh=u.load_asset(asset)
assert mesh
skeleton=mesh.skeleton
material=mesh.materials[0].material_interface
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
u.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
task=u.AssetImportTask()
task.filename=str(folder/'SK_AC01T2_LOD0.fbx')
task.destination_path='/Game/Art/AC01T2'
task.destination_name='SK_AC01T2'
task.automated=True
task.replace_existing=True
task.save=False
task.factory=u.FbxFactory()
opt=u.FbxImportUI()
opt.automated_import_should_detect_type=False
opt.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH
opt.import_as_skeletal=True
opt.import_mesh=True
opt.import_animations=False
opt.import_textures=False
opt.import_materials=False
opt.create_physics_asset=False
opt.skeleton=skeleton
opt.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose',False)
opt.skeletal_mesh_import_data.set_editor_property('normal_import_method',u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
task.options=opt
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
assert task.imported_object_paths, 'Missing imported LOD0'
mesh=u.load_asset(asset)
assert mesh.skeleton==skeleton
sub=u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
for lod in (1,2):
    assert sub.import_lod(mesh,lod,str(folder/f'SK_AC01T2_LOD{lod}.fbx'))==lod
slots=mesh.materials
assert len(slots)==1
slot=slots[0]
slot.material_interface=material
slot.material_slot_name="AC01_InnerFrame"
slots[0]=slot
mesh.materials=slots
assert mesh.materials[0].material_interface==material
assert u.EditorAssetLibrary.save_loaded_asset(mesh,False)
u.log('AC01_BODY_REFRESH_OK '+str([sub.get_num_verts(mesh,i) for i in range(3)]))
