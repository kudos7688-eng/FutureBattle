import json,sys,re,hashlib,difflib
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
setup=Path(__file__).resolve().parent;repo=setup.parent;root=Path(sys.argv[1]);source=setup/'Benchmarks/PerceptionGrid-20261110-source'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
dll=sha(repo/'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll')
off=read(setup/'Benchmarks/PerceptionGrid-20261110-off/manifest.json')
assert off['status']=='complete' and off['matches']==40 and off['smoke_pass']==210 and off['dll_sha256']==dll
manifests=list(root.glob('*/manifest.json'));assert len(manifests)==23,len(manifests)
audits={};fixtures={};replays={}
for p in manifests:
 m=read(p);log=(p.parent/'run.log').read_text(encoding='utf-8-sig')
 assert m['status']=='passed' and m['dll_sha256']==dll and m['machine'] and m['context_reuse']==1 and m['hidden_bones']==1
 assert f"PERCEPTION_GRID enabled={m['perception_grid']} far=0" in log
 assert f"P3_SENSE mode=B grid={m['perception_grid']} far=0 slices=4" in log
 assert 'SIGHT_CONTEXT_REUSE enabled=1' in log and 'HIDDEN_BONES enabled=1' in log
 if not m['nullrhi']:
  adapter=re.search(r'Chosen D3D12 Adapter Id = (\d+)',log).group(1)
  assert f'Found D3D12 adapter {adapter}: NVIDIA GeForce RTX 5070 Ti' in log
 for x in m['source_hashes']+m['config_hashes']:assert sha(Path(x['Path']))==x['Hash'],x['Path']
 assert sha(repo/'Game/FutureBattle/Content/Prototype/Lvl_BattleField500.umap')==m['map_hash']
 if m['grid_audit']:
  a=read(p.parent/'grid-validation.json');assert a['mismatch_frames']==[0,0,0] and a['first_event_sequence_equal'] and a['direct'][0]>0 and a['shared'][0]>0
  audits[m['view']]=a
 if m['functional']:
  assert log.count('P3_CHECK PASS')==25
  f=list(map(int,re.search(r'P3_GRID_FIXTURE checks=(\d+) mismatches=(\d+) direct_samples=(\d+) shared_samples=(\d+)',log).groups()))
  assert f[0]==480 and f[1]==0 and f[2]>0 and f[3]>0
  fixtures[str(m['perception_grid'])]=f
for view in ['RTS','TPS','FPS']:
 r=read(root/f'replay1000-{view}-1/replay-comparison.json');assert r['equal'] and r['frames_off']>=1347 and int(r['final_on']['shots'])>0 and int(r['final_on']['deaths'])>0
 replays[view]=r
protected=read(setup/'Benchmarks/Leader2AB-before-20261109/protected-trial-hashes.json');assert len(protected)==16
for x in protected:assert sha(Path(x['Path']))==x['Hash'],x['Path']
allowed={'BattleLeaderRuntime.cpp','BattlePhase3.cpp','BattlePhase3.h'}
changed=[Path(x['Path']).name for x in read(source/'before-hashes.json') if sha(Path(x['Path']))!=x['Hash']]
assert set(changed)==allowed,changed
prototype=repo/'Game/FutureBattle/Source/FutureBattle/Prototype';patch=[]
assert 'int32 Grid=0;' in (prototype/'BattleLeaderRuntime.cpp').read_text(encoding='utf-8-sig')
for name in sorted(allowed):patch+=difflib.unified_diff((source/name).read_text(encoding='utf-8-sig').splitlines(True),(prototype/name).read_text(encoding='utf-8-sig').splitlines(True),fromfile='before/'+name,tofile='after/'+name)
patch+=difflib.unified_diff([],(prototype/'BattleGridTrial.cpp').read_text(encoding='utf-8-sig').splitlines(True),fromfile='/dev/null',tofile='after/BattleGridTrial.cpp')
patch+=difflib.unified_diff((source/'Test-PerceptionAudit.ps1').read_text(encoding='utf-8-sig').splitlines(True),(setup/'Test-PerceptionAudit.ps1').read_text(encoding='utf-8-sig').splitlines(True),fromfile='before/Test-PerceptionAudit.ps1',tofile='after/Test-PerceptionAudit.ps1')
(source/'implementation.patch').write_text(''.join(patch),encoding='utf-8')
result={'dll':dll,'passed_runs':23,'integrated_pass':210,'off_matches':40,'functional_pass_each_mode':25,'mixed_fixture':fixtures,'sense_audit':audits,'replay':replays,'protected_count':16,'protected_changed':[],'changed_existing_source':changed,'new_source':['BattleGridTrial.cpp'],'all_run_sources_configs_map_frozen':True,'default_grid':False,'existing_grid_algorithm_unchanged':True,'context_and_hidden_bones_on_for_comparison':True,'other_optimization_implemented':False}
result['test_tool_hashes']={n:sha(setup/n) for n in ['Test-PerceptionAudit.ps1','Measure-PerceptionGrid.ps1','Check-PerceptionGrid.py','Report-PerceptionGrid.py','Verify-PerceptionGrid.py']}
(root/'verification.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps({k:v for k,v in result.items() if k!='replay'},indent=2))
