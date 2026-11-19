"""Run in UE Python commandlet; export the project's actual Quinn bind skeleton."""
from pathlib import Path
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Art' / 'AC01' / 'RigReference'
OUT.mkdir(parents=True, exist_ok=True)
mesh = unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple')
assert mesh, 'Project Quinn mesh missing'
for asset, filename in [(mesh, 'QuinnReference.fbx'),
                        (unreal.load_asset('/Game/Characters/Mannequins/Anims/Rifle/Walk/MF_Rifle_Walk_Fwd'), 'QuinnWalk.fbx'),
                        (unreal.load_asset('/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS'), 'QuinnIdle.fbx')]:
    assert asset
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = str(OUT / filename)
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    options = unreal.FbxExportOption()
    options.ascii = False
    options.level_of_detail = False
    options.collision = False
    options.bake_material_inputs = unreal.FbxMaterialBakeMode.DISABLED
    options.export_preview_mesh = False
    task.options = options
    assert unreal.Exporter.run_asset_export_task(task), filename
    assert (OUT / filename).stat().st_size > 0
(OUT / 'source.json').write_text(json.dumps({'mesh': mesh.get_path_name(), 'skeleton': mesh.get_editor_property('skeleton').get_path_name()}, indent=2), encoding='utf-8')
unreal.log('AC01_RIG_EXPORT_OK')
unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), 'QUIT_EDITOR')
