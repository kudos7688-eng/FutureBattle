import csv,json,sys,hashlib,difflib,re
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
setup=Path(__file__).resolve().parent;repo=setup.parent
root=Path(sys.argv[1]);source=setup/'Benchmarks/SightContext-20261110-source'
def read(p): return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest().upper()
dll=sha(repo/'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll')
off=read(setup/'Benchmarks/SightContext-20261110-off/manifest.json')
assert off['status']=='complete' and off['matches']==40 and off['smoke_pass']==210 and off['dll_sha256']==dll
manifests=list(root.glob('*/manifest.json'));assert len(manifests)==25,len(manifests)
for p in manifests:
    m=read(p);assert m['status']=='passed' and m['dll_sha256']==dll and m['machine']
    log=(p.parent/'run.log').read_text(encoding='utf-8-sig')
    assert f"SIGHT_CONTEXT_REUSE enabled={m['context_reuse']}" in log
    if not m['nullrhi']:
        adapter=re.search(r'Chosen D3D12 Adapter Id = (\d+)',log).group(1)
        assert f'Found D3D12 adapter {adapter}: NVIDIA GeForce RTX 5070 Ti' in log
    for item in m['source_hashes']: assert sha(Path(item['Path']))==item['Hash'],item['Path']
    for item in m['config_hashes']: assert sha(Path(item['Path']))==item['Hash'],item['Path']
    assert sha(repo/'Game/FutureBattle/Content/Prototype/Lvl_BattleField500.umap')==m['map_hash']
    if m['context_audit']:
        a=read(p.parent/'context-validation.json');assert a['mismatch_frames']==[0,0,0] and a['first_event_sequence_equal']
for layout in ['Open','Cross','Gate']:
    assert read(root/f'replay-{layout}-1/replay-comparison.json')['equal']
large=read(root/'replay1000-1/replay-comparison.json');assert large['equal'] and int(large['final_on']['shots'])>0
assert max(int(read(root/f'replay-{layout}-1/replay-comparison.json')['final_on']['deaths']) for layout in ['Open','Cross','Gate'])>0
functional={}
for v in [0,1]:
    log=(root/f'functional-{v}/run.log').read_text(encoding='utf-8-sig')
    functional[str(v)]=log.count('P3_CHECK PASS');assert functional[str(v)]==25
protected=read(setup/'Benchmarks/Leader2AB-before-20261109/protected-trial-hashes.json')
assert len(protected)==16
for x in protected:assert sha(Path(x['Path']))==x['Hash'],x['Path']
changed=[Path(x['Path']).name for x in read(source/'before-hashes.json') if sha(Path(x['Path']))!=x['Hash']]
allowed={'BattlePerception.cpp','BattlePerception.h','BattleLeaderRuntime.cpp','BattleLeaderRuntime.h','BattlePhase3.cpp','BattlePhase3.h'}
assert set(changed)==allowed,changed
prototype=repo/'Game/FutureBattle/Source/FutureBattle/Prototype'
old=(source/'BattleLeaderRuntime.cpp').read_text(encoding='utf-8-sig');new=(prototype/'BattleLeaderRuntime.cpp').read_text(encoding='utf-8-sig')
pattern=r'bool UBattleLeaderRuntime::CanSee\(.*?\n\}'
assert re.search(pattern,old,re.S).group()==re.search(pattern,new,re.S).group(),'Reference CanSee changed'
patch=[]
for name in sorted(allowed):
    a=(source/name).read_text(encoding='utf-8-sig').splitlines(True);b=(prototype/name).read_text(encoding='utf-8-sig').splitlines(True)
    patch+=difflib.unified_diff(a,b,fromfile='before/'+name,tofile='after/'+name)
patch+=difflib.unified_diff([], (prototype/'BattleContextTrial.cpp').read_text(encoding='utf-8-sig').splitlines(True),fromfile='/dev/null',tofile='after/BattleContextTrial.cpp')
patch+=difflib.unified_diff((source/'Test-PerceptionAudit.ps1').read_text(encoding='utf-8-sig').splitlines(True), (setup/'Test-PerceptionAudit.ps1').read_text(encoding='utf-8-sig').splitlines(True),fromfile='before/Test-PerceptionAudit.ps1',tofile='after/Test-PerceptionAudit.ps1')
(source/'implementation.patch').write_text(''.join(patch),encoding='utf-8')
result={'dll':dll,'passed_runs':25,'integrated_pass':210,'off_matches':40,'functional_pass':functional,
        'protected_count':16,'protected_changed':[],'reference_cansee_unchanged':True,'changed_existing_source':changed,
        'new_source':['BattleContextTrial.cpp'],'all_run_sources_configs_map_frozen':True,'default_reuse':False,
        'candidate_reduction_implemented':False,'display_optimization_implemented':False}
result['test_tool_hashes']={name:sha(setup/name) for name in ['Test-PerceptionAudit.ps1','Measure-SightContext.ps1','Check-SightContext.py','Report-SightContext.py','Verify-SightContext.py']}
(root/'verification.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result,indent=2))
