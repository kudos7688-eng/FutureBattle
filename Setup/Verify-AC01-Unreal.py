"""Check actual playback in PIE, after materials have finished compiling."""
import unreal as u
import time,json
from pathlib import Path
out=Path(__file__).resolve().parents[1]/'Art/AC01'
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
assert levels.load_level('/Game/Art/AC01/Lvl_AC01Preview')
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
cam=actors.spawn_actor_from_class(u.CameraActor,u.Vector(280,-300,155),u.MathLibrary.find_look_at_rotation(u.Vector(280,-300,155),u.Vector(0,0,88)))
cam.set_actor_label('AC01 presentation camera')
cam.camera_component.set_editor_property('field_of_view',34)
levels.save_current_level()
levels.editor_set_viewport_realtime(True)
u.EditorPythonScripting.set_keep_python_script_alive(True)
levels.editor_request_begin_play()
started=time.monotonic();samples=[];sample_times=[];view_set=[False];shot=[False];ended=[False]
report=json.loads((out/'unreal-validation.json').read_text(encoding='utf-8'))
def tick(delta):
    elapsed=time.monotonic()-started
    worlds=u.EditorLevelLibrary.get_pie_worlds(False)
    if worlds:
        play=worlds[0]
        pawns=u.GameplayStatics.get_all_actors_of_class(play,u.SkeletalMeshActor)
        if pawns:
            comp=pawns[0].skeletal_mesh_component
            if not view_set[0]:
                cams=u.GameplayStatics.get_all_actors_of_class(play,u.CameraActor)
                pc=u.GameplayStatics.get_player_controller(play,0)
                if pc and cams:pc.set_view_target_with_blend(cams[-1],0)
                view_set[0]=True
            if elapsed>=3+len(samples)*.23 and len(samples)<4:
                p=comp.get_socket_location('foot_l');samples.append([p.x,p.y,p.z]);sample_times.append(comp.get_position())
            if elapsed>12 and not shot[0]:
                u.AutomationLibrary.take_high_res_screenshot(1000,1200,str(out/'Preview/Unreal.png'))
                shot[0]=True
    if elapsed>16 and not ended[0]:
        report['foot_samples_cm']=samples;report['animation_positions_seconds']=sample_times
        report['animation_moves']=len({tuple(round(v,2) for v in p) for p in samples})>1
        report['play_in_editor_verified']=report['animation_moves']
        (out/'unreal-validation.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        u.log('AC01_PIE_CHECK '+json.dumps(report))
        levels.editor_request_end_play();ended[0]=True
    if elapsed>18:
        u.unregister_slate_post_tick_callback(handle)
        u.SystemLibrary.execute_console_command(world,'QUIT_EDITOR')
handle=u.register_slate_post_tick_callback(tick)
