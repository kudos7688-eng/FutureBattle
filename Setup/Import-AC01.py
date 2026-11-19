"""Import AC01 into its own UE folder; never replace the existing soldiers.
Run in the full editor with -ExecutePythonScript and PythonScriptPlugin enabled.
"""
from pathlib import Path
import json
import unreal as u

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Art'/'AC01'
DEST='/Game/Art/AC01'
tools=u.AssetToolsHelpers.get_asset_tools()
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
world=editor.get_editor_world()
u.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')

task=u.AssetImportTask()
task.filename=str(OUT/'Exports/SK_AC01_LOD0.fbx')
task.destination_path=DEST
task.destination_name='SK_AC01'
task.automated=True
task.replace_existing=True
task.save=True
task.factory=u.FbxFactory()
opt=u.FbxImportUI()
opt.import_as_skeletal=True
opt.mesh_type_to_import=u.FBXImportType.FBXIT_SKELETAL_MESH
opt.automated_import_should_detect_type=False
opt.import_mesh=True
opt.import_animations=False
opt.import_materials=False
opt.import_textures=False
opt.create_physics_asset=True
opt.skeleton=u.load_asset('/Game/Characters/Mannequins/Meshes/SK_Mannequin')
opt.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose',False)
opt.skeletal_mesh_import_data.set_editor_property('use_t0_as_ref_pose',False)
opt.skeletal_mesh_import_data.set_editor_property('import_meshes_in_bone_hierarchy',False)
opt.skeletal_mesh_import_data.set_editor_property('normal_import_method',u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
task.options=opt
tools.import_asset_tasks([task])
mesh=u.load_asset(DEST+'/SK_AC01')
assert isinstance(mesh,u.SkeletalMesh), 'Skeletal import failed'
assert mesh.get_editor_property('skeleton')==opt.skeleton, 'Must retain existing project skeleton'

sub=u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
for lod in [1,2]:
    result=sub.import_lod(mesh,lod,str(OUT/f'Exports/SK_AC01_LOD{lod}.fbx'))
    assert result==lod, (lod,result)

textures={}
for name in ['BaseColor','ORM','Emissive']:
    t=u.AssetImportTask();t.filename=str(OUT/f'Textures/T_AC01_{name}.png');t.destination_path=DEST+'/Textures';t.automated=True;t.replace_existing=True;t.save=True
    tools.import_asset_tasks([t])
    tex=u.load_asset(DEST+'/Textures/T_AC01_'+name);assert tex
    tex.set_editor_property('srgb',name=='BaseColor')
    if name=='ORM':tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_MASKS)
    textures[name]=tex
    u.EditorAssetLibrary.save_loaded_asset(tex)

mat=u.load_asset(DEST+'/M_AC01')
if mat is None:mat=tools.create_asset('M_AC01',DEST,u.Material,u.MaterialFactoryNew())
else:u.MaterialEditingLibrary.delete_all_material_expressions(mat)
ml=u.MaterialEditingLibrary
for name,tex in textures.items():
    node=ml.create_material_expression(mat,u.MaterialExpressionTextureSample,-600,0)
    node.set_editor_property('texture',tex)
    node.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_COLOR if name=='BaseColor' else u.MaterialSamplerType.SAMPLERTYPE_MASKS if name=='ORM' else u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    if name=='BaseColor':ml.connect_material_property(node,'RGB',u.MaterialProperty.MP_BASE_COLOR)
    elif name=='ORM':
        for pin,prop in [('R',u.MaterialProperty.MP_AMBIENT_OCCLUSION),('G',u.MaterialProperty.MP_ROUGHNESS),('B',u.MaterialProperty.MP_METALLIC)]:ml.connect_material_property(node,pin,prop)
    else:
        mult=ml.create_material_expression(mat,u.MaterialExpressionMultiply,-300,200)
        mult.set_editor_property('const_b',4)
        ml.connect_material_expressions(node,'RGB',mult,'A')
        ml.connect_material_property(mult,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
ml.recompile_material(mat)
slots=mesh.get_editor_property('materials')
slots[0].set_editor_property('material_interface',mat)
mesh.set_editor_property('materials',slots)
u.EditorAssetLibrary.save_loaded_asset(mat)
u.EditorAssetLibrary.save_loaded_asset(mesh)

# A separate saved scene plays the actual project's walking clip in the editor.
if u.EditorAssetLibrary.does_asset_exist(DEST+'/Lvl_AC01Preview'):
    assert levels.load_level(DEST+'/Lvl_AC01Preview')
    for old in actors.get_all_level_actors():
        if not isinstance(old,(u.WorldSettings,u.Brush)):
            actors.destroy_actor(old)
else:
    assert levels.new_level(DEST+'/Lvl_AC01Preview')
world=editor.get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',u.GameModeBase)
actor=actors.spawn_actor_from_class(u.SkeletalMeshActor,u.Vector(0,0,0),u.Rotator(yaw=-90))
actor.set_actor_label('AC01 | Quinn walk preview')
comp=actor.skeletal_mesh_component
comp.set_skeletal_mesh_asset(mesh)
comp.set_simulate_physics(False)
walk=u.load_asset('/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd')
comp.override_animation_data(walk,True,True,0,1)
comp.set_update_animation_in_editor(True)
comp.set_editor_property('visibility_based_anim_tick_option',u.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
comp.set_editor_property('enable_update_rate_optimizations',False)
comp.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
floor=actors.spawn_actor_from_class(u.StaticMeshActor,u.Vector(0,0,-3),u.Rotator())
floor.set_actor_label('Preview floor')
floor.static_mesh_component.set_static_mesh(u.load_asset('/Engine/BasicShapes/Cube'))
floor.set_actor_scale3d(u.Vector(8,8,.05))
for loc,rot,power in [(u.Vector(100,-250,330),u.Rotator(pitch=-40,yaw=100),8),(u.Vector(-200,100,250),u.Rotator(pitch=-35,yaw=-35),5)]:
    a=actors.spawn_actor_from_class(u.DirectionalLight,loc,rot)
    a.light_component.set_editor_property('intensity',power)
sky=actors.spawn_actor_from_class(u.SkyLight,u.Vector(0,0,250),u.Rotator())
sky.light_component.set_editor_property('intensity',1)
start=actors.spawn_actor_from_class(u.PlayerStart,u.Vector(250,-250,120),u.Rotator(yaw=135))
levels.save_current_level()

report={'asset':mesh.get_path_name(),'skeleton':mesh.get_editor_property('skeleton').get_path_name(),
        'lod_vertices':[sub.get_num_verts(mesh,i) for i in [0,1,2]],'bone_count':comp.get_num_bones(),
        'mesh_bounds_cm':str(mesh.get_bounds()),'preview_level':DEST+'/Lvl_AC01Preview',
        'animation':walk.get_path_name(),'import_ok':True}
(OUT/'unreal-validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
u.log('AC01_IMPORT_OK '+json.dumps(report))
u.EditorLevelLibrary.set_level_viewport_camera_info(u.Vector(260,-290,160),u.MathLibrary.find_look_at_rotation(u.Vector(260,-290,160),u.Vector(0,0,94)))
u.EditorLevelLibrary.editor_set_game_view(True)
# Run Verify-AC01-Unreal.py separately for actual Play In Editor verification.
u.SystemLibrary.execute_console_command(world,'QUIT_EDITOR')
