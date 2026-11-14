import json,re,hashlib,csv,difflib,sys
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
repo=Path(__file__).resolve().parent.parent;root=repo/'Setup/Benchmarks/VAT-Adoption-20261111';phase=root/'phase3'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
dll=sha(repo/'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll')
normal={}
for name,mode in [('leader-default',1),('leader-off',0),('leader-ui',1),('leader-ui-720',1)]:
 p=root/name;m=read(p/'manifest.json');log=(p/'run.log').read_text(encoding='utf-8-sig')
 assert m['status']=='passed' and m['dll_sha256']==dll and m['machine']
 assert f'VAT_DISPLAY requested={mode} default=1 scope=mass_followers' in log
 assert not re.search(r'Ensure condition failed:|Assertion failed:|Fatal error:',log)
 n=log.count('BATTLE_LEADER_TEST PASS') if not name.startswith('leader-ui') else log.count('BATTLE_LEADER_UI PASS')
 assert n==(39 if not name.startswith('leader-ui') else 10),n
 if name!='leader-off':assert not any('BattleDisplayLOD=' in a for a in m['arguments'])
 normal[name]=n
 if name.startswith('leader-ui'):assert float(re.search(r'far_mean=([\d.]+)',log).group(1))>0
runs=list(phase.glob('*/manifest.json'));assert len(runs)==14
for p in runs:
 m=read(p);log=(p.parent/'run.log').read_text(encoding='utf-8-sig')
 assert m['status']=='passed' and m['dll_sha256']==dll and m['machine']
 assert f"VAT_DISPLAY requested={m['display_lod']} default=1 scope=mass_followers" in log
 for x in m['source_hashes']+m['config_hashes']+m['asset_hashes']:assert sha(Path(x['Path']))==x['Hash']
 if m['functional']:assert log.count('P3_CHECK PASS')==25
 if m['vat_audit']:
  a=list(map(int,re.search(r'P3_VAT_AUDIT checks=(\d+) errors=(\d+) enters=(\d+) returns=(\d+)',log).groups()))
  assert a==[12960,0,108,108],a
  rows=list(csv.DictReader(open(p.parent/'frames-vat-audit.csv',encoding='utf-8-sig')))
  assert len(rows)==12960 and all(r['time_equal']==r['bones_equal']==r['weapon_equal']=='1' for r in rows)
for view in ['RTS','TPS','FPS']:
 m=read(phase/f'replay1000-{view}-1/replay-comparison.json');assert m['equal'] and m['frames_on']==1349
reg=read(root/'regression/manifest.json');assert reg['status']=='complete' and reg['dll_sha256']==dll and reg['matches']==40 and reg['smoke_pass']==210
protected=read(repo/'Setup/Benchmarks/Leader2AB-before-20261109/protected-trial-hashes.json')
assert len(protected)==16
for x in protected:assert sha(Path(x['Path']))==x['Hash']
baseline=read(repo/'Setup/Benchmarks/DisplayLOD-20261111-r11/perf-RTS-r1-0/manifest.json')
changed=[Path(x['Path']).name for x in baseline['source_hashes'] if sha(Path(x['Path']))!=x['Hash']]
assert set(changed)=={'BattleLeaderRuntime.cpp','BattleVATDisplay.cpp','BattleVATDisplay.h'},changed
for x in baseline['config_hashes']+baseline['asset_hashes']:assert sha(Path(x['Path']))==x['Hash']
assert sha(repo/'Game/FutureBattle/Content/Prototype/Lvl_BattleField500.umap')==baseline['map_hash']
for x in read(repo/'Setup/Benchmarks/DisplayLOD-20261111-source/before-assets.json'):assert sha(Path(x['Path']))==x['Hash']
patch=[]
for x in read(root/'before-hashes.json'):
 p=Path(x['Path']);patch+=difflib.unified_diff((root/'before'/p.name).read_text(encoding='utf-8-sig').splitlines(True),p.read_text(encoding='utf-8-sig').splitlines(True),fromfile='before/'+p.name,tofile='after/'+p.name)
(root/'implementation.patch').write_text(''.join(patch),encoding='utf-8')
r={'dll':dll,'normal_leader_checks':normal,'phase3_runs':len(runs),'phase3_checks_each_mode':25,'pose_checks_per_run':12960,'pose_mismatches':0,'fixed_replay_frames_each_view':1349,'fixed_replay_equal_all_three_views':True,'integrated_pass':210,'off_matches':40,'protected16_unchanged':True,'original_assets_configs_map_unchanged':True,'changed_source':changed,'default_display_lod':1,'scope':'Mass followers in leader runtime; all-Actor normal field unaffected','new_performance_measurement':False}
(root/'verification.json').write_text(json.dumps(r,indent=2),encoding='utf-8');print(json.dumps(r,indent=2))
