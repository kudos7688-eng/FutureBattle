"""Play the AC01T2 preview level in PIE, sample bone positions and take screenshots."""
import unreal as u
import time, json
from pathlib import Path

out = Path(__file__).resolve().parents[1] / 'Art/AC01_Tripo_v2'
shots = out / 'Preview' / 'Unreal'
shots.mkdir(parents=True, exist_ok=True)
levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
assert levels.load_level('/Game/Art/AC01T2/Lvl_AC01T2Preview')
world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
levels.editor_set_viewport_realtime(True)
u.EditorPythonScripting.set_keep_python_script_alive(True)
levels.editor_request_begin_play()
started = time.monotonic()
report = json.loads((out / 'unreal-validation.json').read_text(encoding='utf-8'))
state = {'shot': 0, 'next': 8.0, 'ended': False, 'samples': {}, 'sampled': 0}
PASSES = 2  # every camera twice, so looping clips are seen at two phases

def tick(delta):
    elapsed = time.monotonic() - started
    worlds = u.EditorLevelLibrary.get_pie_worlds(False)
    if worlds and not state['ended']:
        play = worlds[0]
        cams = sorted(u.GameplayStatics.get_all_actors_of_class(play, u.CameraActor), key=lambda c: c.get_actor_location().x)
        pawns = sorted(u.GameplayStatics.get_all_actors_of_class(play, u.SkeletalMeshActor), key=lambda a: a.get_actor_location().x)
        pc = u.GameplayStatics.get_player_controller(play, 0)
        if elapsed >= 5 + state['sampled'] * 0.4 and state['sampled'] < 4 and pawns:
            for a in pawns:
                c = a.skeletal_mesh_component; base = a.get_actor_location()
                row = {}
                for bone in ('hand_r', 'hand_l', 'head', 'foot_l', 'pelvis'):
                    p = c.get_socket_location(bone)
                    row[bone] = [round(p.x - base.x, 1), round(p.y - base.y, 1), round(p.z - base.z, 1)]
                state['samples'].setdefault(a.get_actor_label(), []).append(row)
            state['sampled'] += 1
        total = len(cams) * PASSES
        if cams and pc and state['shot'] < total and elapsed >= state['next']:
            i = state['shot'] % len(cams)
            if not state.get('armed'):
                pc.set_view_target_with_blend(cams[i], 0); state['armed'] = True; state['next'] = elapsed + 0.6
            else:
                u.AutomationLibrary.take_high_res_screenshot(1280, 900, str(shots / ('Lineup_%d_%d.png' % (i, state['shot'] // len(cams)))))
                state['shot'] += 1; state['armed'] = False; state['next'] = elapsed + 1.3
        if (cams and state['shot'] >= total and elapsed >= state['next'] + 1.5) or elapsed > 75:
            moves = {k: len({json.dumps(r) for r in v}) > 1 for k, v in state['samples'].items()}
            report['pie_bone_samples_cm'] = state['samples']
            report['pie_animation_moves'] = moves
            report['pie_screenshots'] = state['shot']
            report['play_in_editor_verified'] = bool(moves) and state['shot'] > 0
            (out / 'unreal-validation.json').write_text(json.dumps(report, indent=1), encoding='utf-8')
            u.log('AC01T2_PIE_CHECK ' + json.dumps({'moves': moves, 'shots': state['shot']}))
            levels.editor_request_end_play(); state['ended'] = True; state['end_time'] = elapsed
    if state['ended'] and elapsed > state['end_time'] + 3:
        u.unregister_slate_post_tick_callback(handle)
        u.SystemLibrary.execute_console_command(world, 'QUIT_EDITOR')
    if elapsed > 100:
        u.unregister_slate_post_tick_callback(handle)
        u.SystemLibrary.execute_console_command(world, 'QUIT_EDITOR')

handle = u.register_slate_post_tick_callback(tick)
