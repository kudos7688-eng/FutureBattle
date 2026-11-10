import json,sys,re,hashlib,difflib
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
setup=Path(__file__).resolve().parent; repo=setup.parent; root=Path(sys.argv[1]); source=setup/'Benchmarks/HiddenBones-20261110-source'
def read(p): return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest().upper()
dll=sha(repo/'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll')
off=read(setup/'Benchmarks/HiddenBones-20261110-off/manifest.json')
assert off['status']=='complete' and off['matches']==40 and off['smoke_pass']==210 and off['dll_sha256']==dll
manifests=list(root.glob('*/manifest.json')); assert len(manifests)==23,len(manifests)
pose={}; replay={}
for p in manifests:
 m=read(p); log=(p.parent/'run.log').read_text(encoding='utf-8-sig')
 assert m['status']=='passed' and m['dll_sha256']==dll and m['machine'] and m['context_reuse']==1
 assert f"HIDDEN_BONES enabled={m['hidden_bones']}" in log and 'SIGHT_CONTEXT_REUSE enabled=1' in log
 if not m['nullrhi']:
  adapter=re.search(r'Chosen D3D12 Adapter Id = (\d+)',log).group(1)
  assert f'Found D3D12 adapter {adapter}: NVIDIA GeForce RTX 5070 Ti' in log
 for x in m['source_hashes']+m['config_hashes']: assert sha(Path(x['Path']))==x['Hash'],x['Path']
 assert sha(repo/'Game/FutureBattle/Content/Prototype/Lvl_BattleField500.umap')==m['map_hash']
 if m['bone_audit']:
  match=re.search(r'P3_BONE_AUDIT checks=(\d+) errors=(\d+) reduced_ticks=(\d+) reveals=(\d+)',log)
  assert match
  counts=list(map(int,match.groups())); assert counts[0]==3222 and counts[1]==0 and counts[2]>0 and counts[3]==2,counts
  assert log.count('eligible=1 notifies=0')==9
  pose[m['view']]={'checks':counts[0],'errors':counts[1],'reduced_ticks':counts[2],'reveals':counts[3],'bone_transform_tolerance':1e-6,'animation_time_exact':True}
 if m['functional']: assert log.count('P3_CHECK PASS')==25
for view in ['RTS','TPS','FPS']:
 r=read(root/f'replay1000-{view}-1/replay-comparison.json')
 assert r['equal'] and r['frames_off']>=1347 and int(r['final_on']['shots'])>0 and int(r['final_on']['deaths'])>0
 replay[view]=r
protected=read(setup/'Benchmarks/Leader2AB-before-20261109/protected-trial-hashes.json');assert len(protected)==16
for x in protected: assert sha(Path(x['Path']))==x['Hash'],x['Path']
allowed={'BattleLeaderRuntime.cpp','BattleLeaderRuntime.h','BattlePhase3.cpp','BattlePhase3.h'}
changed=[Path(x['Path']).name for x in read(source/'before-hashes.json') if sha(Path(x['Path']))!=x['Hash']]
assert set(changed)==allowed,changed
prototype=repo/'Game/FutureBattle/Source/FutureBattle/Prototype';patch=[]
for name in sorted(allowed):
 patch+=difflib.unified_diff((source/name).read_text(encoding='utf-8-sig').splitlines(True),(prototype/name).read_text(encoding='utf-8-sig').splitlines(True),fromfile='before/'+name,tofile='after/'+name)
added=['BattleFollowerBones.cpp','BattleFollowerBones.h','BattleHiddenBoneTrial.cpp','BattleHiddenBoneTrial.h']
for name in added:patch+=difflib.unified_diff([],(prototype/name).read_text(encoding='utf-8-sig').splitlines(True),fromfile='/dev/null',tofile='after/'+name)
patch+=difflib.unified_diff((source/'Test-PerceptionAudit.ps1').read_text(encoding='utf-8-sig').splitlines(True),(setup/'Test-PerceptionAudit.ps1').read_text(encoding='utf-8-sig').splitlines(True),fromfile='before/Test-PerceptionAudit.ps1',tofile='after/Test-PerceptionAudit.ps1')
(source/'implementation.patch').write_text(''.join(patch),encoding='utf-8')
result={'dll':dll,'passed_runs':23,'integrated_pass':210,'off_matches':40,'functional_pass_each_mode':25,'pose':pose,'replay':replay,'protected_count':16,'protected_changed':[], 'changed_existing_source':changed,'new_source':added,'all_run_sources_configs_map_frozen':True,'default_hidden_bones':False,'context_on_for_comparison':True,'candidate_reduction_implemented':False,'other_display_optimization_implemented':False}
result['test_tool_hashes']={n:sha(setup/n) for n in ['Test-PerceptionAudit.ps1','Measure-HiddenBones.ps1','Check-SightContext.py','Report-HiddenBones.py','Verify-HiddenBones.py']}
(root/'verification.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps({k:v for k,v in result.items() if k!='replay'},indent=2))
