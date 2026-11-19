"""Read-only: print what the imported AC01T2 mesh looks like inside Unreal."""
import unreal as u, json
mesh = u.load_asset('/Game/Art/AC01T2/SK_AC01T2')
sub = u.get_editor_subsystem(u.SkeletalMeshEditorSubsystem)
info = {'verts': [sub.get_num_verts(mesh, i) for i in range(sub.get_lod_count(mesh))]}
mats = mesh.get_editor_property('materials')
info['materials'] = [{'slot': str(m.get_editor_property('material_slot_name')),
                      'material': m.get_editor_property('material_interface').get_path_name() if m.get_editor_property('material_interface') else None} for m in mats]
try:
    aid = mesh.get_editor_property('asset_import_data')
    info['source'] = aid.get_first_filename()
except Exception as e:
    info['source'] = 'n/a ' + str(e)
mat = u.load_asset('/Game/Art/AC01T2/M_AC01T2')
info['material_asset'] = bool(mat)
if mat:
    info['two_sided'] = bool(mat.get_editor_property('two_sided'))
    info['expressions'] = u.MaterialEditingLibrary.get_num_material_expressions(mat)
    info['used_textures'] = [t.get_name() for t in u.MaterialEditingLibrary.get_used_textures(mat)]
level = u.get_editor_subsystem(u.LevelEditorSubsystem)
level.load_level('/Game/Art/AC01T2/Lvl_AC01T2Preview')
acts = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
row = []
for a in acts:
    if isinstance(a, u.SkeletalMeshActor):
        c = a.skeletal_mesh_component
        m0 = c.get_material(0)
        row.append([a.get_actor_label(), m0.get_path_name() if m0 else None, len(c.get_editor_property('override_materials'))])
info['actors'] = row[:3]
u.log('AC01T2_INSPECT ' + json.dumps(info))
u.SystemLibrary.execute_console_command(u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world(), 'QUIT_EDITOR')
