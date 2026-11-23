"""Import the v3 exoskeleton body (seamless under-suit + v2 armour, Art/AC01_Tripo_v2/Scripts/16_undersuit.py) as
/Game/Art/AC01T2/SK_AC01T3 with LOD1-2, on the v2 body's skeleton, material and physics asset. The v2 body is not changed.
The material slot is named AC01_InnerFrame so the runtime does not add its joint spheres. Run with Setup/Import-AC01T3.ps1.
Paint (2026-11-24, Art/AC01_Tripo_v2/Scripts/20_paint_masks.py): the body carries a vertex colour mask (R paint, G frame metal,
B edge wear, A team accent, the rest rubber) and gets its own material M_AC01T3, which mixes the colours from parameters
(PaintColor, AccentColor, FrameColor, RubberColor, BareColor; the game sets the team ones, BattleExo.cpp) and keeps the v2
textures' normal, ambient occlusion and the visor glow (VisorColor).
"""
import unreal as u
from pathlib import Path
root = Path(__file__).resolve().parents[1]
folder = root / 'Art/AC01_Tripo_v2/Exports/T3'
v2 = u.load_asset('/Game/Art/AC01T2/SK_AC01T2')
assert v2
skeleton = v2.skeleton
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
opt.skeletal_mesh_import_data.set_editor_property('vertex_color_import_option', u.VertexColorImportOption.REPLACE)
task.options = opt
u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
asset = '/Game/Art/AC01T2/SK_AC01T3'
assert any(str(p).split('.')[0] == asset for p in task.imported_object_paths), list(task.imported_object_paths)
mesh = u.load_asset(asset)
assert mesh.skeleton == skeleton
sub = u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
for lod in (1, 2):
    assert sub.import_lod(mesh, lod, str(folder / f'SK_AC01T3_LOD{lod}.fbx')) == lod
# ---------------- paint material ----------------
tools = u.AssetToolsHelpers.get_asset_tools(); ml = u.MaterialEditingLibrary
material = u.load_asset('/Game/Art/AC01T2/M_AC01T3')
if material is None: material = tools.create_asset('M_AC01T3', '/Game/Art/AC01T2', u.Material, u.MaterialFactoryNew())
else: ml.delete_all_material_expressions(material)
def tex(name, kind, x, y):
    n = ml.create_material_expression(material, u.MaterialExpressionTextureSample, x, y)
    n.set_editor_property('texture', u.load_asset('/Game/Art/AC01T2/Textures/T_AC01T2_' + name)); n.set_editor_property('sampler_type', kind); return n
def param(name, value, x, y):
    n = ml.create_material_expression(material, u.MaterialExpressionVectorParameter, x, y)
    n.set_editor_property('parameter_name', name); n.set_editor_property('default_value', u.LinearColor(*value, 1.0)); return n
def scalar(name, value, x, y):
    n = ml.create_material_expression(material, u.MaterialExpressionScalarParameter, x, y)
    n.set_editor_property('parameter_name', name); n.set_editor_property('default_value', value); return n
def const(value, x, y):
    n = ml.create_material_expression(material, u.MaterialExpressionConstant, x, y); n.set_editor_property('r', value); return n
def lerp(a, a_out, b, b_out, alpha, alpha_out, x, y):
    n = ml.create_material_expression(material, u.MaterialExpressionLinearInterpolate, x, y)
    ml.connect_material_expressions(a, a_out, n, 'A'); ml.connect_material_expressions(b, b_out, n, 'B'); ml.connect_material_expressions(alpha, alpha_out, n, 'Alpha'); return n
def mul(a, a_out, b, b_out, x, y):
    n = ml.create_material_expression(material, u.MaterialExpressionMultiply, x, y)
    ml.connect_material_expressions(a, a_out, n, 'A'); ml.connect_material_expressions(b, b_out, n, 'B'); return n
vc = ml.create_material_expression(material, u.MaterialExpressionVertexColor, -1400, 0)
rubber, paint = param('RubberColor', (.02, .02, .022), -1400, 200), param('PaintColor', (.5, .52, .55), -1400, 350)
frame, accent = param('FrameColor', (.075, .08, .085), -1400, 500), param('AccentColor', (.05, .2, .5), -1400, 650)
bare, wear = param('BareColor', (.42, .42, .43), -1400, 800), scalar('WearAmount', .6, -1400, 950)
c = lerp(rubber, '', paint, '', vc, 'R', -1100, 200)
c = lerp(c, '', frame, '', vc, 'G', -950, 300)
c = lerp(c, '', accent, '', vc, 'A', -800, 400)
worn = mul(vc, 'B', wear, '', -950, 900)
c = lerp(c, '', bare, '', worn, '', -650, 500)
orm = tex('ORM', u.MaterialSamplerType.SAMPLERTYPE_MASKS, -1100, 1100)
# Grime in the crevices: the baked ambient occlusion darkens the colour a little (and still feeds the lighting).
grime = lerp(const(.55, -800, 1100), '', const(1., -800, 1180), '', orm, 'R', -650, 1100)
ml.connect_material_property(mul(c, '', grime, '', -450, 500), '', u.MaterialProperty.MP_BASE_COLOR)
ml.connect_material_property(orm, 'R', u.MaterialProperty.MP_AMBIENT_OCCLUSION)
metal = ml.create_material_expression(material, u.MaterialExpressionMax, -650, 700)
ml.connect_material_expressions(vc, 'G', metal, 'A'); ml.connect_material_expressions(worn, '', metal, 'B')
ml.connect_material_property(mul(metal, '', const(.85, -650, 800), '', -450, 700), '', u.MaterialProperty.MP_METALLIC)
r = lerp(const(.75, -1100, 1300), '', const(.5, -1100, 1380), '', vc, 'R', -950, 1300)
r = lerp(r, '', const(.35, -950, 1450), '', vc, 'G', -800, 1300)
r = lerp(r, '', const(.3, -800, 1450), '', worn, '', -650, 1300)
ml.connect_material_property(r, '', u.MaterialProperty.MP_ROUGHNESS)
ml.connect_material_property(tex('Normal', u.MaterialSamplerType.SAMPLERTYPE_NORMAL, -650, 1550), 'RGB', u.MaterialProperty.MP_NORMAL)
glow = tex('Emissive', u.MaterialSamplerType.SAMPLERTYPE_COLOR, -1100, 1700)
ml.connect_material_property(mul(glow, 'R', param('VisorColor', (4.0, .35, .25), -1100, 1900), '', -650, 1750), '', u.MaterialProperty.MP_EMISSIVE_COLOR)
material.set_editor_property('two_sided', True)
material.set_editor_property('used_with_skeletal_mesh', True)
ml.recompile_material(material)
assert u.EditorAssetLibrary.save_loaded_asset(material, False)
slots = mesh.materials
assert len(slots) == 1, len(slots)
slot = slots[0]; slot.material_interface = material; slot.material_slot_name = 'AC01_InnerFrame'; slots[0] = slot
mesh.materials = slots
if v2.physics_asset: mesh.set_editor_property('physics_asset', v2.physics_asset)
assert u.EditorAssetLibrary.save_loaded_asset(mesh, False)
u.log('AC01_T3_IMPORT_OK ' + str([sub.get_num_verts(mesh, i) for i in range(3)]) + ' vertex_colors=%s material=%s' % (mesh.get_editor_property('has_vertex_colors'), mesh.materials[0].material_interface.get_name()))
