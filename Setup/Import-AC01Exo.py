"""Import what the exoskeleton soldier's AC01 look needs on top of Import-AC01T2.py: the dual-equipment clips
(Art/AC01_Tripo_v2/Exports/Anims/A_AC01T2_Dual_*.fbx) and the two weapon models (Art/Weapons/<name>/).
Adds assets under /Game/Art/AC01T2 only. Run in the full editor with -ExecutePythonScript.
"""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art'
DEST = '/Game/Art/AC01T2'
tools = u.AssetToolsHelpers.get_asset_tools()
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
u.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.FBX 0')
skeleton = u.load_asset('/Game/Characters/Mannequins/Meshes/SK_Mannequin')
assert skeleton
ml = u.MaterialEditingLibrary
report = {'animations': {}, 'weapons': {}}

for fbx in sorted((ART / 'AC01_Tripo_v2/Exports/Anims').glob('A_AC01T2_Dual_*.fbx')):
    t = u.AssetImportTask(); t.filename = str(fbx); t.destination_path = DEST + '/Anims'; t.destination_name = fbx.stem
    t.automated = True; t.replace_existing = True; t.save = True; t.factory = u.FbxFactory()
    o = u.FbxImportUI()
    o.import_as_skeletal = True; o.automated_import_should_detect_type = False
    o.mesh_type_to_import = u.FBXImportType.FBXIT_ANIMATION
    o.import_mesh = False; o.import_animations = True; o.import_materials = False; o.import_textures = False
    o.skeleton = skeleton
    o.anim_sequence_import_data.set_editor_property('animation_length', u.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    o.anim_sequence_import_data.set_editor_property('import_bone_tracks', True)
    o.anim_sequence_import_data.set_editor_property('remove_redundant_keys', False)
    t.options = o
    tools.import_asset_tasks([t])
    seq = u.load_asset(DEST + '/Anims/' + fbx.stem)
    assert isinstance(seq, u.AnimSequence), fbx.stem
    report['animations'][fbx.stem] = {'seconds': round(seq.get_play_length(), 3), 'skeleton_ok': seq.get_skeleton() == skeleton}

samplers = {'BaseColor': u.MaterialSamplerType.SAMPLERTYPE_COLOR, 'ORM': u.MaterialSamplerType.SAMPLERTYPE_MASKS, 'Normal': u.MaterialSamplerType.SAMPLERTYPE_NORMAL}
for name in ('Rifle', 'Launcher'):
    src = ART / 'Weapons' / name
    t = u.AssetImportTask(); t.filename = str(src / 'Exports' / ('SM_%s.fbx' % name)); t.destination_path = DEST + '/Weapons'
    t.destination_name = 'SM_AC01_' + name; t.automated = True; t.replace_existing = True; t.save = True; t.factory = u.FbxFactory()
    o = u.FbxImportUI()
    o.import_as_skeletal = False; o.automated_import_should_detect_type = False
    o.mesh_type_to_import = u.FBXImportType.FBXIT_STATIC_MESH
    o.import_mesh = True; o.import_animations = False; o.import_materials = False; o.import_textures = False
    d = o.static_mesh_import_data
    d.set_editor_property('combine_meshes', True)
    d.set_editor_property('auto_generate_collision', False)
    d.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    t.options = o
    tools.import_asset_tasks([t])
    mesh = u.load_asset(DEST + '/Weapons/SM_AC01_' + name)
    assert isinstance(mesh, u.StaticMesh), name
    mesh.set_editor_property('allow_cpu_access', True)  # the tracer start reads the muzzle vertices (ABattleUnit::TracerStart)
    mat = u.load_asset(DEST + '/Weapons/M_AC01_' + name)
    if mat is None: mat = tools.create_asset('M_AC01_' + name, DEST + '/Weapons', u.Material, u.MaterialFactoryNew())
    else: ml.delete_all_material_expressions(mat)
    for i, key in enumerate(('BaseColor', 'ORM', 'Normal')):
        tt = u.AssetImportTask(); tt.filename = str(src / 'Textures' / ('T_%s_%s.png' % (name, key))); tt.destination_path = DEST + '/Weapons'
        tt.destination_name = 'T_AC01_%s_%s' % (name, key); tt.automated = True; tt.replace_existing = True; tt.save = True
        tools.import_asset_tasks([tt])
        tex = u.load_asset(DEST + '/Weapons/T_AC01_%s_%s' % (name, key)); assert tex, key
        tex.set_editor_property('srgb', key == 'BaseColor')
        if key == 'ORM': tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_MASKS)
        if key == 'Normal':
            tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property('flip_green_channel', True)  # baked in Blender (OpenGL convention)
        u.EditorAssetLibrary.save_loaded_asset(tex)
        node = ml.create_material_expression(mat, u.MaterialExpressionTextureSample, -600, i * 260)
        node.set_editor_property('texture', tex); node.set_editor_property('sampler_type', samplers[key])
        if key == 'BaseColor': ml.connect_material_property(node, 'RGB', u.MaterialProperty.MP_BASE_COLOR)
        elif key == 'Normal': ml.connect_material_property(node, 'RGB', u.MaterialProperty.MP_NORMAL)
        else:
            ml.connect_material_property(node, 'G', u.MaterialProperty.MP_ROUGHNESS)
            ml.connect_material_property(node, 'B', u.MaterialProperty.MP_METALLIC)
    ml.recompile_material(mat)
    mesh.set_material(0, mat)
    u.EditorAssetLibrary.save_loaded_asset(mat); u.EditorAssetLibrary.save_loaded_asset(mesh)
    b = mesh.get_bounding_box()
    report['weapons'][name] = {'asset': mesh.get_path_name(), 'triangles': mesh.get_num_triangles(0),
                               'box_min_cm': [round(b.min.x, 1), round(b.min.y, 1), round(b.min.z, 1)],
                               'box_max_cm': [round(b.max.x, 1), round(b.max.y, 1), round(b.max.z, 1)]}

(ART / 'AC01_Tripo_v2' / 'unreal-exo-import.json').write_text(json.dumps(report, indent=1), encoding='utf-8')
u.log('AC01EXO_IMPORT_OK ' + json.dumps(report))
u.SystemLibrary.execute_console_command(world, 'QUIT_EDITOR')
