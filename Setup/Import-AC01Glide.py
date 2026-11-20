"""Import the exoskeleton soldier's glide clips (Art/AC01_Tripo_v2/Exports/Anims/A_AC01T2_<OneHand|Dual>_Glide.fbx,
made by Art/AC01_Tripo_v2/Scripts/13_glide_anims.py) onto the shared mannequin skeleton under /Game/Art/AC01T2/Anims.
Run in the full editor with -ExecutePythonScript (Setup/Import-AC01Glide.ps1).
"""
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[1]
DEST = '/Game/Art/AC01T2/Anims'
tools = u.AssetToolsHelpers.get_asset_tools()
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
u.SystemLibrary.execute_console_command(world, 'Interchange.FeatureFlags.Import.FBX 0')
skeleton = u.load_asset('/Game/Characters/Mannequins/Meshes/SK_Mannequin')
assert skeleton
done = {}
for family in ('OneHand', 'Dual'):
    name = 'A_AC01T2_%s_Glide' % family
    fbx = ROOT / 'Art/AC01_Tripo_v2/Exports/Anims' / (name + '.fbx')
    assert fbx.exists(), fbx
    t = u.AssetImportTask(); t.filename = str(fbx); t.destination_path = DEST; t.destination_name = name
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
    # Check this task really imported the clip; an old asset of the same name would otherwise pass the checks below.
    path = DEST + '/' + name
    assert any(str(p).split('.')[0] == path for p in t.imported_object_paths), (name, list(t.imported_object_paths))
    seq = u.load_asset(path)
    assert isinstance(seq, u.AnimSequence), name
    assert seq.get_play_length() > 0.1, name
    assert seq.get_skeleton() == skeleton, name
    done[name] = round(seq.get_play_length(), 3)
u.log('AC01_GLIDE_IMPORT_OK ' + str(done))
