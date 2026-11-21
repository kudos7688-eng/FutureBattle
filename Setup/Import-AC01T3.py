"""Import the v3 exoskeleton body (seamless under-suit + v2 armour, Art/AC01_Tripo_v2/Scripts/16_undersuit.py) as
/Game/Art/AC01T2/SK_AC01T3 with LOD1-2, on the v2 body's skeleton, material and physics asset. The v2 body is not changed.
The material slot is named AC01_InnerFrame so the runtime does not add its joint spheres. Run with Setup/Import-AC01T3.ps1.
"""
import unreal as u
from pathlib import Path
root = Path(__file__).resolve().parents[1]
folder = root / 'Art/AC01_Tripo_v2/Exports/T3'
v2 = u.load_asset('/Game/Art/AC01T2/SK_AC01T2')
assert v2
skeleton = v2.skeleton; material = v2.materials[0].material_interface
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
u.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.FBX 0')
for lod in range(3): assert (folder / f'SK_AC01T3_LOD{lod}.fbx').exists(), lod
task = u.AssetImportTask()
task.filename = str(folder / 'SK_AC01T3_LOD0.fbx'); task.destination_path = '/Game/Art/AC01T2'; task.destination_name = 'SK_AC01T3'
task.automated = True; task.replace_existing = True; task.save = False; task.factory = u.FbxFactory()
opt = u.FbxImportUI()
opt.automated_import_should_detect_type = False
opt.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
opt.import_as_skeletal = True; opt.import_mesh = True; opt.import_animations = False
opt.import_textures = False; opt.import_materials = False; opt.create_physics_asset = False
opt.skeleton = skeleton
opt.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', False)
opt.skeletal_mesh_import_data.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
task.options = opt
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
asset = '/Game/Art/AC01T2/SK_AC01T3'
assert any(str(p).split('.')[0] == asset for p in task.imported_object_paths), list(task.imported_object_paths)
mesh = u.load_asset(asset)
assert mesh.skeleton == skeleton
sub = u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
for lod in (1, 2):
    assert sub.import_lod(mesh, lod, str(folder / f'SK_AC01T3_LOD{lod}.fbx')) == lod
slots = mesh.materials
assert len(slots) == 1, len(slots)
slot = slots[0]; slot.material_interface = material; slot.material_slot_name = 'AC01_InnerFrame'; slots[0] = slot
mesh.materials = slots
if v2.physics_asset: mesh.set_editor_property('physics_asset', v2.physics_asset)
assert u.EditorAssetLibrary.save_loaded_asset(mesh, False)
u.log('AC01_T3_IMPORT_OK ' + str([sub.get_num_verts(mesh, i) for i in range(3)]))
