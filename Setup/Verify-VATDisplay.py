import json,sys,re,hashlib,difflib,csv
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
setup=Path(__file__).resolve().parent;repo=setup.parent;root=Path(sys.argv[1]);source=setup/'Benchmarks/DisplayLOD-20261111-source'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
dll=sha(repo/'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll')
off=read(setup/'Benchmarks/DisplayLOD-20261111-off/manifest.json')
assert off['status']=='complete' and off['matches']==40 and off['smoke_pass']==210 and off['dll_sha256']==dll
manifests=list(root.glob('*/manifest.json'));poses={};quality={};functional=[];replays={}
for p in manifests:
 m=read(p);log=(p.parent/'run.log').read_text(encoding='utf-8-sig')
 assert m['status']=='passed' and m['dll_sha256']==dll and m['machine']
 assert m['context_reuse']==1 and m['hidden_bones']==1 and m['perception_grid']==1 and m['anim_tasks']==0
 assert 'P3_SENSE mode=B grid=1 far=0 slices=4' in log and 'ANIM_TASKS enabled=0' in log
 assert f"VAT_DISPLAY requested={m['display_lod']} default=0 trial_only=1" in log
 if not m['nullrhi']:
  adapter=re.search(r'Chosen D3D12 Adapter Id = (\d+)',log).group(1)
  assert f'Found D3D12 adapter {adapter}: NVIDIA GeForce RTX 5070 Ti' in log
 for x in m['source_hashes']+m['config_hashes']+m['asset_hashes']:assert sha(Path(x['Path']))==x['Hash'],x['Path']
 assert sha(repo/'Game/FutureBattle/Content/Prototype/Lvl_BattleField500.umap')==m['map_hash']
 if p.parent.name.startswith('perf-'):
  for setting in ['r.ShadowQuality=0','r.Lumen.DiffuseIndirect.Allow=0','r.Lumen.Reflections.Allow=0','r.RayTracing.Scene.BuildMode=0','r.ScreenPercentage=100']:
   assert 'P3_RENDER_SETTING '+setting in log,setting
  assert m['units']==1000 and m['warmup']==10 and m['duration']==60 and m['stable']
  assert not m['context_replay'] and not m['cpu_trace'] and not m['vat_audit']
 if m['vat_audit']:
  a=list(map(int,re.search(r'P3_VAT_AUDIT checks=(\d+) errors=(\d+) enters=(\d+) returns=(\d+)',log).groups()))
  assert a[0]==12960 and a[1]==0 and a[2]>=72 and a[3]>=72,a
  rows=list(csv.DictReader(open(p.parent/'frames-vat-audit.csv',encoding='utf-8-sig')))
  assert len(rows)==12960 and all(r['time_equal']==r['bones_equal']==r['weapon_equal']=='1' for r in rows)
  assert set(r['pose'] for r in rows)=={'0','1','2'} and any(r['dead']=='1' and r['far']=='1' for r in rows) and any(r['dead']=='1' and r['far']=='0' for r in rows)
  assert len(list(p.parent.glob('frames-vat-*.png')))==6
  if m.get('vat_quality'):
   for setting in ['r.ShadowQuality=5','r.Lumen.DiffuseIndirect.Allow=1','r.Lumen.Reflections.Allow=1','r.RayTracing.Scene.BuildMode=1']:
    assert 'P3_RENDER_SETTING '+setting in log,setting
   quality[m['view']]=a
  else:poses[m['view']]=a
 if m['display_lod']==1 and not m['nullrhi']:
  assert 'ray_wpo=-1 ray_simulation_count=-1' in log
  assert 'Falling back to static ray tracing geometry' not in log
 if m['functional']:
  assert log.count('P3_CHECK PASS')==25
  functional.append(m['display_lod'])
for view in ['RTS','TPS','FPS']:
 a=read(root/f'replay1000-{view}-1/replay-comparison.json')
 assert a['equal'] and a['frames_off']>=1347 and int(a['final_on']['shots'])>0 and int(a['final_on']['deaths'])>0
 replays[view]=a
assert len(poses)==3 and len(quality)==3 and sorted(functional)==[0,1]
assert len(list(root.glob('perf-*/manifest.json')))==12
protected=read(setup/'Benchmarks/Leader2AB-before-20261109/protected-trial-hashes.json');assert len(protected)==16
for x in protected:assert sha(Path(x['Path']))==x['Hash'],x['Path']
for group in ['before-assets.json','before-config.json']:
 for x in read(source/group):assert sha(Path(x['Path']))==x['Hash'],x['Path']
allowed={'BattleLeaderRuntime.cpp','BattleLeaderRuntime.h','BattlePhase3.cpp','FutureBattle.Build.cs'}
changed=[Path(x['Path']).name for x in read(source/'before-source.json') if sha(Path(x['Path']))!=x['Hash']]
assert set(changed)==allowed,changed
prototype=repo/'Game/FutureBattle/Source/FutureBattle/Prototype'
assert 'int32 DisplayLOD=0;' in (prototype/'BattleLeaderRuntime.cpp').read_text(encoding='utf-8-sig')
patch=[]
for name in sorted(allowed):
 target=prototype/name if name!='FutureBattle.Build.cs' else prototype.parent/name
 patch+=difflib.unified_diff((source/name).read_text(encoding='utf-8-sig').splitlines(True),target.read_text(encoding='utf-8-sig').splitlines(True),fromfile='before/'+name,tofile='after/'+name)
new=sorted(prototype.glob('BattleVAT*'))
for p in new:patch+=difflib.unified_diff([],p.read_text(encoding='utf-8-sig').splitlines(True),fromfile='/dev/null',tofile='after/'+p.name)
project=repo/'Game/FutureBattle/FutureBattle.uproject'
patch+=difflib.unified_diff((source/project.name).read_text(encoding='utf-8-sig').splitlines(True),project.read_text(encoding='utf-8-sig').splitlines(True),fromfile='before/'+project.name,tofile='after/'+project.name)
(source/'implementation.patch').write_text(''.join(patch),encoding='utf-8')
result={'dll':dll,'passed_runs':len(manifests),'integrated_pass':210,'off_matches':40,'functional_pass_each_mode':25,'pose':poses,'shadow_lumen_quality_fixture':quality,'replay':replays,'protected_count':16,'protected_changed':[],'original_assets_and_configs_unchanged':True,'changed_existing_source':changed,'new_source':[p.name for p in new],'all_run_sources_configs_map_assets_frozen':True,'default_display_lod':0,'prior_three_optimizations_on':True,'anim_tasks':0,'visual_tolerance_approval':False}
(root/'verification.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k!='replay'},indent=2))
