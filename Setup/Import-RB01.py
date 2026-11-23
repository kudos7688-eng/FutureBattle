"""Import RB01 (Tripo-generated infantry robot, Art/RB01_Tripo) as /Game/Art/RB01/SK_RB01 with LOD0-2 on the
existing SK_Mannequin skeleton, its PBR material, and Quinn's physics asset (same skeleton, near-identical joint
heights). Existing soldiers, maps and the shared skeleton are not replaced; the game uses it only with
-BattleSoldierBody=1. Run through Setup/Import-RB01.ps1 (full editor, -ExecutePythonScript).
The Art folder is not tracked by git: it is read from this checkout, or from the main checkout next to it.
"""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Art' / 'RB01_Tripo'
if not OUT.exists():
    OUT = ROOT.parent / 'FutureBattle' / 'Art' / 'RB01_Tripo'
assert (OUT / 'Exports' / 'SK_RB01_LOD0.fbx').exists(), OUT
DEST = '/Game/Art/RB01'
tools = u.AssetToolsHelpers.get_asset_tools()
editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
world = editor.get_editor_world()
u.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.FBX 0')
skeleton = u.load_asset('/Game/Characters/Mannequins/Meshes/SK_Mannequin')
physics = u.load_asset('/Game/Characters/Mannequins/Rigs/PA_Mannequin')
assert skeleton and physics
report = {'import_ok': False, 'source': str(OUT)}

# ---------------- mesh ----------------
task = u.AssetImportTask()
task.filename = str(OUT / 'Exports/SK_RB01_LOD0.fbx')
task.destination_path = DEST
task.destination_name = 'SK_RB01'
task.automated = True; task.replace_existing = True; task.save = True
task.factory = u.FbxFactory()
opt = u.FbxImportUI()
opt.import_as_skeletal = True
opt.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
opt.automated_import_should_detect_type = False
opt.import_mesh = True; opt.import_animations = False
opt.import_materials = False; opt.import_textures = False
opt.create_physics_asset = False
opt.skeleton = skeleton
d = opt.skeletal_mesh_import_data
d.set_editor_property('update_skeleton_reference_pose', False)
d.set_editor_property('use_t0_as_ref_pose', False)
d.set_editor_property('import_meshes_in_bone_hierarchy', False)
d.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
task.options = opt
tools.import_asset_tasks([task])
mesh = u.load_asset(DEST + '/SK_RB01')
assert isinstance(mesh, u.SkeletalMesh), 'Skeletal import failed'
assert mesh.get_editor_property('skeleton') == skeleton, 'Must retain existing project skeleton'
sub = u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
for lod in [1, 2]:
    result = sub.import_lod(mesh, lod, str(OUT / f'Exports/SK_RB01_LOD{lod}.fbx'))
    assert result == lod, (lod, result)
mesh.set_editor_property('physics_asset', physics)
# Note (2026-11-23): the measurement/play settings (sg.ViewDistanceQuality=0 -> r.SkeletalMeshLODBias=2) draw every
# soldier at LOD2, which is also the far VAT mesh. LOD screen sizes (here 1.0/0.5/0.25, Quinn 1.0/0.3/0.15) and UE's
# reduction settings could not be changed from Python in UE 5.8 (SourceModels/LODSettings edits are not applied).
report['lod_screen_size'] = [i.get_editor_property('screen_size').get_editor_property('default') for i in mesh.get_editor_property('source_models')]

# ---------------- textures + material ----------------
textures = {}
for name in ['BaseColor', 'ORM', 'Normal']:
    t = u.AssetImportTask(); t.filename = str(OUT / f'Textures/T_RB01_{name}.png'); t.destination_path = DEST + '/Textures'
    t.destination_name = 'T_RB01_' + name; t.automated = True; t.replace_existing = True; t.save = True
    tools.import_asset_tasks([t])
    tex = u.load_asset(DEST + '/Textures/T_RB01_' + name); assert tex, name
    tex.set_editor_property('srgb', name == 'BaseColor')
    if name == 'ORM': tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_MASKS)
    if name == 'Normal':
        tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('flip_green_channel', False)  # T_RB01_Normal is already DirectX (green flipped in 02b)
    textures[name] = tex
    u.EditorAssetLibrary.save_loaded_asset(tex)
mat = u.load_asset(DEST + '/M_RB01')
ml = u.MaterialEditingLibrary
if mat is None: mat = tools.create_asset('M_RB01', DEST, u.Material, u.MaterialFactoryNew())
else: ml.delete_all_material_expressions(mat)
samplers = {'BaseColor': u.MaterialSamplerType.SAMPLERTYPE_COLOR, 'ORM': u.MaterialSamplerType.SAMPLERTYPE_MASKS,
            'Normal': u.MaterialSamplerType.SAMPLERTYPE_NORMAL}
# Tripo's maps are glossy (roughness ~0.4) and partly metallic. With the game's low settings (no Lumen reflections,
# black sky) that read as black/white blotches on the low LOD, so roughness is lifted into [ROUGH_MIN, 1] and metallic
# scaled down (2026-11-23). 1.0 / 0.0 and 1.0 reproduce Tripo's values.
ROUGH_MIN, METAL_SCALE = 0.6, 0.3
import os
USE_NORMAL_MAP = os.environ.get('RB01_NORMAL_MAP', '1') == '1'  # experiment switch (Import-RB01.ps1 -NoNormalMap)
if not USE_NORMAL_MAP: textures.pop('Normal')
def const_op(cls, x, y, value):
    e = ml.create_material_expression(mat, cls, x, y); e.set_editor_property('const_b', value); return e
for i, (name, tex) in enumerate(textures.items()):
    node = ml.create_material_expression(mat, u.MaterialExpressionTextureSample, -600, i * 260)
    node.set_editor_property('texture', tex)
    node.set_editor_property('sampler_type', samplers[name])
    if name == 'BaseColor': ml.connect_material_property(node, 'RGB', u.MaterialProperty.MP_BASE_COLOR)
    elif name == 'Normal': ml.connect_material_property(node, 'RGB', u.MaterialProperty.MP_NORMAL)
    else:
        rough_mul = const_op(u.MaterialExpressionMultiply, -350, i * 260, 1.0 - ROUGH_MIN)
        rough_add = const_op(u.MaterialExpressionAdd, -200, i * 260, ROUGH_MIN)
        metal_mul = const_op(u.MaterialExpressionMultiply, -350, i * 260 + 120, METAL_SCALE)
        assert ml.connect_material_expressions(node, 'G', rough_mul, 'A')
        assert ml.connect_material_expressions(rough_mul, '', rough_add, 'A')
        assert ml.connect_material_expressions(node, 'B', metal_mul, 'A')
        ml.connect_material_property(rough_add, '', u.MaterialProperty.MP_ROUGHNESS)
        ml.connect_material_property(metal_mul, '', u.MaterialProperty.MP_METALLIC)
report['material_rough_min'], report['material_metal_scale'], report['normal_map'] = ROUGH_MIN, METAL_SCALE, USE_NORMAL_MAP
ml.recompile_material(mat)
old = mesh.get_editor_property('materials')
slot = u.SkeletalMaterial()
slot.set_editor_property('material_interface', mat)
slot.set_editor_property('material_slot_name', old[0].get_editor_property('material_slot_name'))
try: slot.set_editor_property('uv_channel_data', old[0].get_editor_property('uv_channel_data'))
except Exception as e: report['slot_uv_channel_data'] = str(e)
mesh.set_editor_property('materials', [slot])
assigned = mesh.get_editor_property('materials')[0].get_editor_property('material_interface')
assert assigned == mat, 'material slot still empty: %s' % assigned
u.EditorAssetLibrary.save_loaded_asset(mat)
u.EditorAssetLibrary.save_loaded_asset(mesh)

b = mesh.get_bounds()
report.update({'asset': mesh.get_path_name(), 'skeleton': mesh.get_editor_property('skeleton').get_path_name(),
               'physics_asset': mesh.get_editor_property('physics_asset').get_path_name(),
               'material': assigned.get_path_name(),
               'lod_vertices': [sub.get_num_verts(mesh, i) for i in range(sub.get_lod_count(mesh))],
               'mesh_bounds_cm': {'origin': [b.origin.x, b.origin.y, b.origin.z], 'extent': [b.box_extent.x, b.box_extent.y, b.box_extent.z]},
               'import_ok': True})
(OUT / 'unreal-import.json').write_text(json.dumps(report, indent=1), encoding='utf-8')
u.log('RB01_IMPORT_OK ' + json.dumps(report))
u.SystemLibrary.execute_console_command(world, 'QUIT_EDITOR')
