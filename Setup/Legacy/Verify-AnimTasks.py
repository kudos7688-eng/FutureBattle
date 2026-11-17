import json,sys,re,hashlib,difflib,csv
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
setup=Path(__file__).resolve().parent;repo=setup.parent;root=Path(sys.argv[1]);source=setup/'Benchmarks/AnimTasks-20261110-source'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
dll=sha(repo/'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll')
off=read(setup/'Benchmarks/AnimTasks-20261110-off/manifest.json')
assert off['status']=='complete' and off['matches']==40 and off['smoke_pass']==210 and off['dll_sha256']==dll
manifests=list(root.glob('*/manifest.json'));assert len(manifests)==25,len(manifests)
poses={};functional=[];replays={};traces={}
for p in manifests:
 m=read(p);log=(p.parent/'run.log').read_text(encoding='utf-8-sig')
 assert m['status']=='passed' and m['dll_sha256']==dll and m['machine'] and m['context_reuse']==1 and m['hidden_bones']==1 and m['perception_grid']==1
 assert f"ANIM_TASKS enabled={m['anim_tasks']} scope=hidden_update_only" in log
 assert 'P3_SENSE mode=B grid=1 far=0 slices=4' in log
 assert 'SIGHT_CONTEXT_REUSE enabled=1' in log and 'HIDDEN_BONES enabled=1' in log
 if not m['nullrhi']:
  adapter=re.search(r'Chosen D3D12 Adapter Id = (\d+)',log).group(1)
  assert f'Found D3D12 adapter {adapter}: NVIDIA GeForce RTX 5070 Ti' in log
 for x in m['source_hashes']+m['config_hashes']:assert sha(Path(x['Path']))==x['Hash'],x['Path']
 assert sha(repo/'Game/FutureBattle/Content/Prototype/Lvl_BattleField500.umap')==m['map_hash']
 if m['anim_task_audit']:
  a=list(map(int,re.search(r'P3_ANIM_TASK_AUDIT checks=(\d+) errors=(\d+) reduced_ticks=(\d+) reveals=(\d+)',log).groups()))
  assert a[0]==3222 and a[1]==0 and a[2]>0 and a[3]==2
  poses[m['view']]=a
 if m['functional']:
  assert log.count('P3_CHECK PASS')==25
  functional.append(m['anim_tasks'])
 if m['cpu_trace']:
  assert not m['light_trace']
  w=read(p.parent/'window.json');stats=list(csv.DictReader(open(p.parent/'window-statistics.csv',encoding='utf-8-sig')))
  names=['FParallelAnimationCompletionTask','PreUpdateAnimation','PostUpdateAnimation','USkeletalMeshComponent_TickAnimation','USkeletalMeshComponent_CompleteParallelAnimationEvaluation']
  traces[str(m['anim_tasks'])]={x['Name']:{'calls_per_frame':int(x['Count'])/w['frames'],'inclusive_ms_per_frame':float(x['Incl'])*1000/w['frames'],'exclusive_ms_per_frame':float(x['Excl'])*1000/w['frames']} for x in stats if x['Name'] in names}
for view in ['RTS','TPS','FPS']:
 a=read(root/f'replay1000-{view}-1/replay-comparison.json');assert a['equal'] and a['frames_off']>=1347 and int(a['final_on']['shots'])>0 and int(a['final_on']['deaths'])>0
 replays[view]=a
assert len(poses)==3 and sorted(functional)==[0,1]
assert traces['1']['FParallelAnimationCompletionTask']['calls_per_frame'] < traces['0']['FParallelAnimationCompletionTask']['calls_per_frame']
protected=read(setup/'Benchmarks/Leader2AB-before-20261109/protected-trial-hashes.json');assert len(protected)==16
for x in protected:assert sha(Path(x['Path']))==x['Hash'],x['Path']
allowed={'BattleLeaderRuntime.cpp','BattleLeaderRuntime.h','BattlePhase3.cpp','BattlePhase3.h'}
changed=[Path(x['Path']).name for x in read(source/'before-hashes.json') if sha(Path(x['Path']))!=x['Hash']]
assert set(changed)==allowed,changed
prototype=repo/'Game/FutureBattle/Source/FutureBattle/Prototype';patch=[]
assert 'int32 AnimTasks=0;' in (prototype/'BattleLeaderRuntime.cpp').read_text(encoding='utf-8-sig')
for name in sorted(allowed):patch+=difflib.unified_diff((source/name).read_text(encoding='utf-8-sig').splitlines(True),(prototype/name).read_text(encoding='utf-8-sig').splitlines(True),fromfile='before/'+name,tofile='after/'+name)
new=['BattleAnimTasks.cpp','BattleAnimTasks.h','BattleAnimTaskTrial.cpp','BattleAnimTaskTrial.h']
for name in new:patch+=difflib.unified_diff([],(prototype/name).read_text(encoding='utf-8-sig').splitlines(True),fromfile='/dev/null',tofile='after/'+name)
patch+=difflib.unified_diff((source/'Test-PerceptionAudit.ps1').read_text(encoding='utf-8-sig').splitlines(True),(setup/'Test-PerceptionAudit.ps1').read_text(encoding='utf-8-sig').splitlines(True),fromfile='before/Test-PerceptionAudit.ps1',tofile='after/Test-PerceptionAudit.ps1')
(source/'implementation.patch').write_text(''.join(patch),encoding='utf-8')
result={'dll':dll,'passed_runs':25,'integrated_pass':210,'off_matches':40,'functional_pass_each_mode':25,'pose':poses,'replay':replays,'trace':traces,'protected_count':16,'protected_changed':[],'changed_existing_source':changed,'new_source':new,'all_run_sources_configs_map_frozen':True,'default_anim_tasks':0,'prior_three_optimizations_on':True}
result['test_tool_hashes']={n:sha(setup/n) for n in ['Test-PerceptionAudit.ps1','Measure-AnimTasks.ps1','Report-AnimTasks.py','Verify-AnimTasks.py']}
(root/'verification.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps({k:v for k,v in result.items() if k!='replay'},indent=2))
