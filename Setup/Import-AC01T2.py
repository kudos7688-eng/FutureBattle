"""Import the Tripo-based AC01 (v2) and its one-handed clips into their own UE folder
and build a preview level. Existing soldiers, maps and the shared skeleton are not replaced.
Run in the full editor with -ExecutePythonScript and PythonScriptPlugin enabled.
"""
from pathlib import Path
import json
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Art' / 'AC01_Tripo_v2'
DEST = '/Game/Art/AC01T2'
tools = u.AssetToolsHelpers.get_asset_tools()
editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
actors = u.get_editor_subsystem(u.EditorActorSubsystem)
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
world = editor.get_editor_world()
u.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.FBX 0')
skeleton = u.load_asset('/Game/Characters/Mannequins/Meshes/SK_Mannequin')
assert skeleton
report = {'import_ok': False}

# ---------------- mesh ----------------
task = u.AssetImportTask()
task.filename = str(OUT / 'Exports/SK_AC01T2_LOD0.fbx')
task.destination_path = DEST
task.destination_name = 'SK_AC01T2'
task.automated = True; task.replace_existing = True; task.save = True
task.factory = u.FbxFactory()
opt = u.FbxImportUI()
opt.import_as_skeletal = True
opt.mesh_type_to_import = u.FBXImportType.FBXIT_SKELETAL_MESH
opt.automated_import_should_detect_type = False
opt.import_mesh = True; opt.import_animations = False
opt.import_materials = False; opt.import_textures = False
opt.create_physics_asset = True
opt.skeleton = skeleton
d = opt.skeletal_mesh_import_data
d.set_editor_property('update_skeleton_reference_pose', False)
d.set_editor_property('use_t0_as_ref_pose', False)
d.set_editor_property('import_meshes_in_bone_hierarchy', False)
d.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
task.options = opt
tools.import_asset_tasks([task])
mesh = u.load_asset(DEST + '/SK_AC01T2')
assert isinstance(mesh, u.SkeletalMesh), 'Skeletal import failed'
assert mesh.get_editor_property('skeleton') == skeleton, 'Must retain existing project skeleton'
sub = u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
for lod in [1, 2]:
    result = sub.import_lod(mesh, lod, str(OUT / f'Exports/SK_AC01T2_LOD{lod}.fbx'))
    assert result == lod, (lod, result)

# ---------------- textures + material ----------------
textures = {}
for name in ['BaseColor', 'ORM', 'Normal', 'Emissive']:
    t = u.AssetImportTask(); t.filename = str(OUT / f'Textures/T_AC01T_{name}.png'); t.destination_path = DEST + '/Textures'
    t.destination_name = 'T_AC01T2_' + name; t.automated = True; t.replace_existing = True; t.save = True
    tools.import_asset_tasks([t])
    tex = u.load_asset(DEST + '/Textures/T_AC01T2_' + name); assert tex, name
    tex.set_editor_property('srgb', name in ('BaseColor', 'Emissive'))
    if name == 'BaseColor':
        # Lift dark painted panels while retaining the texture's white point.
        tex.set_editor_property('adjust_brightness_curve', 0.76)
    if name == 'ORM': tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_MASKS)
    if name == 'Normal':
        tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('flip_green_channel', True)  # baked in Blender (OpenGL convention)
    textures[name] = tex
    u.EditorAssetLibrary.save_loaded_asset(tex)
mat = u.load_asset(DEST + '/M_AC01T2')
ml = u.MaterialEditingLibrary
if mat is None: mat = tools.create_asset('M_AC01T2', DEST, u.Material, u.MaterialFactoryNew())
else: ml.delete_all_material_expressions(mat)
samplers = {'BaseColor': u.MaterialSamplerType.SAMPLERTYPE_COLOR, 'ORM': u.MaterialSamplerType.SAMPLERTYPE_MASKS,
            'Normal': u.MaterialSamplerType.SAMPLERTYPE_NORMAL, 'Emissive': u.MaterialSamplerType.SAMPLERTYPE_COLOR}
for i, (name, tex) in enumerate(textures.items()):
    node = ml.create_material_expression(mat, u.MaterialExpressionTextureSample, -600, i * 260)
    node.set_editor_property('texture', tex)
    node.set_editor_property('sampler_type', samplers[name])
    if name == 'BaseColor': ml.connect_material_property(node, 'RGB', u.MaterialProperty.MP_BASE_COLOR)
    elif name == 'Normal': ml.connect_material_property(node, 'RGB', u.MaterialProperty.MP_NORMAL)
    elif name == 'ORM':
        ml.connect_material_property(node, 'G', u.MaterialProperty.MP_ROUGHNESS)
        # Keep a diffuse response on the painted shells even under little sky light.
        metal = ml.create_material_expression(mat, u.MaterialExpressionMultiply, -300, i * 260)
        metal.set_editor_property('const_b', 0.62)
        ml.connect_material_expressions(node, 'B', metal, 'A')
        ml.connect_material_property(metal, '', u.MaterialProperty.MP_METALLIC)
    else:
        # The visor glow takes its colour from a parameter so the game can tint it per team
        # (UBattleLeaderRuntime::ApplySquadLook); the default is the painted red.
        tint = ml.create_material_expression(mat, u.MaterialExpressionVectorParameter, -600, i * 260 + 200)
        tint.set_editor_property('parameter_name', 'VisorColor')
        tint.set_editor_property('default_value', u.LinearColor(4.0, 0.35, 0.25, 1.0))
        mult = ml.create_material_expression(mat, u.MaterialExpressionMultiply, -300, i * 260)
        ml.connect_material_expressions(node, 'R', mult, 'A')
        ml.connect_material_expressions(tint, '', mult, 'B')
        ml.connect_material_property(mult, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
# Parts are separate shells and some are open where they met a neighbour; draw both sides
# so those openings show the inside of the part instead of a see-through hole.
mat.set_editor_property('two_sided', True)
ml.recompile_material(mat)
# Editing the struct returned by get_editor_property only changes a copy: build the slot list anew.
old = mesh.get_editor_property('materials')
slot = u.SkeletalMaterial()
slot.set_editor_property('material_interface', mat)
slot.set_editor_property('material_slot_name', old[0].get_editor_property('material_slot_name'))
for prop in ('uv_channel_data',):  # optional; the imported slot name is read-only from Python
    try: slot.set_editor_property(prop, old[0].get_editor_property(prop))
    except Exception as e: report['slot_' + prop] = str(e)
mesh.set_editor_property('materials', [slot])
assigned = mesh.get_editor_property('materials')[0].get_editor_property('material_interface')
assert assigned == mat, 'material slot still empty: %s' % assigned
report['material_assigned'] = assigned.get_path_name()
u.EditorAssetLibrary.save_loaded_asset(mat)
u.EditorAssetLibrary.save_loaded_asset(mesh)

# ---------------- animations ----------------
anims = {}
anim_errors = {}
for fbx in sorted((OUT / 'Exports/Anims').glob('A_AC01T2_OneHand_*.fbx')):
    name = fbx.stem
    t = u.AssetImportTask(); t.filename = str(fbx); t.destination_path = DEST + '/Anims'; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = True; t.factory = u.FbxFactory()
    o = u.FbxImportUI()
    o.import_as_skeletal = True; o.automated_import_should_detect_type = False
    o.mesh_type_to_import = u.FBXImportType.FBXIT_ANIMATION
    o.import_mesh = False; o.import_animations = True; o.import_materials = False; o.import_textures = False
    o.skeleton = skeleton
    try:
        o.anim_sequence_import_data.set_editor_property('animation_length', u.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
        o.anim_sequence_import_data.set_editor_property('import_bone_tracks', True)
        o.anim_sequence_import_data.set_editor_property('remove_redundant_keys', False)
    except Exception as e:
        anim_errors['_options'] = str(e)
    t.options = o
    tools.import_asset_tasks([t])
    seq = u.load_asset(DEST + '/Anims/' + name)
    if isinstance(seq, u.AnimSequence):
        anims[name] = {'seconds': round(seq.get_play_length(), 3), 'skeleton_ok': seq.get_skeleton() == skeleton}
    else:
        found = [p for p in u.EditorAssetLibrary.list_assets(DEST + '/Anims') if name in p]
        anim_errors[name] = 'not an AnimSequence after import; assets: %s' % found
report['animations'] = anims; report['animation_errors'] = anim_errors

# ---------------- preview level: one actor per clip ----------------
LEVEL = DEST + '/Lvl_AC01T2Preview'
if u.EditorAssetLibrary.does_asset_exist(LEVEL):
    assert levels.load_level(LEVEL)
    for old in actors.get_all_level_actors():
        if not isinstance(old, (u.WorldSettings, u.Brush)): actors.destroy_actor(old)
else:
    assert levels.new_level(LEVEL)
world = editor.get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', u.GameModeBase)
show = ['Idle', 'Walk_Fwd', 'Fire', 'Reload', 'Crouch_Idle', 'Crouch_Walk_Fwd', 'Jog_Fwd', 'Death_Front_01']
lineup = [('OneHand ' + s, mesh, DEST + '/Anims/A_AC01T2_OneHand_' + s) for s in show]
lineup.append(('Project rifle idle on AC01T2', mesh, '/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS'))
lineup.append(('Quinn reference', u.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple'), '/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS'))
placed = []
for i, (label, sk, anim_path) in enumerate(lineup):
    anim = u.load_asset(anim_path)
    if not anim or not sk: continue
    a = actors.spawn_actor_from_class(u.SkeletalMeshActor, u.Vector(i * 260.0, 0, 0), u.Rotator(yaw=150))
    a.set_actor_label('AC01T2 | ' + label)
    c = a.skeletal_mesh_component
    c.set_skeletal_mesh_asset(sk)
    c.set_simulate_physics(False)
    c.override_animation_data(anim, True, True, 0, 1)
    c.set_update_animation_in_editor(True)
    c.set_editor_property('visibility_based_anim_tick_option', u.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    c.set_editor_property('enable_update_rate_optimizations', False)
    c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
    placed.append(label)
floor = actors.spawn_actor_from_class(u.StaticMeshActor, u.Vector(len(lineup) * 130.0, 0, -3), u.Rotator())
floor.set_actor_label('Preview floor')
floor.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
floor.set_actor_scale3d(u.Vector(len(lineup) * 3.2, 10, .05))
for loc, rot, power in [(u.Vector(100, -250, 330), u.Rotator(pitch=-40, yaw=100), 8), (u.Vector(-200, 100, 250), u.Rotator(pitch=-35, yaw=-35), 4)]:
    l = actors.spawn_actor_from_class(u.DirectionalLight, loc, rot)
    l.light_component.set_editor_property('intensity', power)
sky = actors.spawn_actor_from_class(u.SkyLight, u.Vector(0, 0, 250), u.Rotator())
sky.light_component.set_editor_property('intensity', 1)
actors.spawn_actor_from_class(u.PlayerStart, u.Vector(250, -500, 120), u.Rotator(yaw=90))
for k in range(0, len(lineup), 2):  # one camera per pair of actors
    pos = u.Vector(k * 260.0 + 130, -620, 150)
    cam = actors.spawn_actor_from_class(u.CameraActor, pos, u.MathLibrary.find_look_at_rotation(pos, u.Vector(k * 260.0 + 130, 0, 95)))
    cam.set_actor_label('AC01T2 camera %d' % (k // 2))
    cam.camera_component.set_editor_property('field_of_view', 44)
    cam.camera_component.set_editor_property('constrain_aspect_ratio', False)
levels.save_current_level()

b = mesh.get_bounds()
report.update({'asset': mesh.get_path_name(), 'skeleton': mesh.get_editor_property('skeleton').get_path_name(),
               'lod_vertices': [sub.get_num_verts(mesh, i) for i in [0, 1, 2]],
               'mesh_bounds_cm': {'origin': [b.origin.x, b.origin.y, b.origin.z], 'extent': [b.box_extent.x, b.box_extent.y, b.box_extent.z]},
               'preview_level': LEVEL, 'lineup': placed, 'import_ok': True})
(OUT / 'unreal-validation.json').write_text(json.dumps(report, indent=1), encoding='utf-8')
u.log('AC01T2_IMPORT_OK ' + json.dumps(report))
u.SystemLibrary.execute_console_command(world, 'QUIT_EDITOR')
