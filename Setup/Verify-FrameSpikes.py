"""Evidence integrity checks for the offline investigation (no game tests)."""
import json,hashlib
from pathlib import Path
from importlib.machinery import SourceFileLoader
A=SourceFileLoader('spikes',str(Path(__file__).with_name('Analyze-FrameSpikes.py'))).load_module()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
s=read(A.OUT/'summary.json');assert len(s)==35 and sum(x['n'] for x in s)==92701
for x in s:
    p=A.ROOT/'Setup/Benchmarks'/x['run']
    assert sha(p/'frames.csv')==x['source_sha256'].upper()
    assert read(p/'manifest.json')['status']=='passed'
    assert x['deaths_delta']==0
frames=A.read(A.OUT/'trace-frames.csv');assert len(frames)==60
err=max(abs(float(r['tick_ms'])-float(r['accounted_ms'])) for r in frames);assert err<.001
for r in frames:
    assert float(r['tick_ms'])>0 and all(float(r[k])>=-.001 for k in ['game_mode','animation_tick','animation_completion','render_updates','character_movement','wait','other'])
manifest=read(A.ROOT/'Setup/Benchmarks/AnimTasks-20261110/trace-RTS-1/manifest.json')
for x in manifest['source_hashes']+manifest['config_hashes']:assert sha(Path(x['Path']))==x['Hash'],x['Path']
assert sha(A.ROOT/'Game/FutureBattle/Content/Prototype/Lvl_BattleField500.umap')==manifest['map_hash']
assert sha(A.ROOT/'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll')==manifest['dll_sha256']
protected=read(A.ROOT/'Setup/Benchmarks/Leader2AB-before-20261109/protected-trial-hashes.json');assert len(protected)==16
for x in protected:assert sha(Path(x['Path']))==x['Hash'],x['Path']
result={'runs':35,'frames':92701,'input_csv_hashes_match':True,'selected_trace_frames':60,'max_trace_accounting_error_ms':err,
'current_dll':manifest['dll_sha256'],'source_hash_count':len(manifest['source_hashes']),'config_hash_count':len(manifest['config_hashes']),
'current_sources_configs_map_match_latest_measurement':True,'protected_16_match':True,'new_game_runs':0,'game_builds':0,'game_regression_tests_rerun':False,
'script_hashes':{p.name:sha(p) for p in [A.ROOT/'Setup'/name for name in ['Analyze-FrameSpikes.py','Report-FrameSpikes.py','Analyze-SpikeTrace.py','Export-SpikeTrace.ps1','Verify-FrameSpikes.py']]}}
(A.OUT/'verification.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result,indent=2))
