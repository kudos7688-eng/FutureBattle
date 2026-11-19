"""Run in the UE editor (-ExecutePythonScript): export the project's rifle clips as FBX.

Read-only for the project: nothing is saved or modified. Output goes to
Art/AC01/RigReference/Anims/<asset name>.fbx (source material for the AC01 one-handed clips).
"""
from pathlib import Path
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Art' / 'AC01' / 'RigReference' / 'Anims'
OUT.mkdir(parents=True, exist_ok=True)
BASE = '/Game/Characters/Mannequins/Anims/Rifle/'
CLIPS = [
    'MM_Rifle_Fire', 'MM_Rifle_Reload', 'MM_Rifle_DryFire', 'MM_Rifle_Equip',
    'Crouch/MM_Rifle_Crouch_Idle', 'Crouch/MM_Rifle_Crouch_Walk_Fwd',
    'Crouch/MM_Rifle_Crouch_Entry', 'Crouch/MM_Rifle_Crouch_Exit',
    'HitReact/MM_HitReact_Front_Lgt_01', 'HitReact/MM_HitReact_Front_Med_01',
    'HitReact/MM_HitReact_Front_Hvy_01', 'HitReact/MM_HitReact_Back_Med_01',
    'Jog/MF_Rifle_Jog_Fwd',
    'Walk/MF_Rifle_Walk_Bwd', 'Walk/MF_Rifle_Walk_Left', 'Walk/MF_Rifle_Walk_Right',
]
DEATH = '/Game/Characters/Mannequins/Anims/Death/'
CLIPS_ABS = [DEATH + 'MM_Death_Front_01', DEATH + 'MM_Death_Back_01']
report = {}
for path in [BASE + c for c in CLIPS] + CLIPS_ABS:
    asset = unreal.load_asset(path)
    name = path.split('/')[-1]
    if not asset or not isinstance(asset, unreal.AnimSequence):
        report[name] = 'missing or not an AnimSequence'
        continue
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = str(OUT / (name + '.fbx'))
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
    ok = unreal.Exporter.run_asset_export_task(task)
    info = {'exported': bool(ok), 'path': path, 'length_s': asset.get_play_length()}
    try:
        info['additive'] = str(asset.get_editor_property('additive_anim_type'))
    except Exception as e:
        info['additive'] = 'unknown'
    report[name] = info
(OUT / 'clips.json').write_text(json.dumps(report, indent=1), encoding='utf-8')
unreal.log('AC01_ANIM_EXPORT_OK ' + json.dumps({k: (v['exported'] if isinstance(v, dict) else v) for k, v in report.items()}))
unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), 'QUIT_EDITOR')
