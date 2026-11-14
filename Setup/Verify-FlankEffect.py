"""Final evidence gate; no backfilled measurement metadata."""
import hashlib
import json
import pathlib
import zipfile

setup=pathlib.Path(__file__).resolve().parent
bench=setup/'Benchmarks'
trial=bench/'SightTrial-20261111'
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
snap=read(bench/'BuildSnapshots/flank-effect-r1/manifest.json')
assert hashlib.sha256(pathlib.Path(snap['sourceArchive']).read_bytes()).hexdigest().upper()==snap['sourceArchiveSha256']
with zipfile.ZipFile(snap['sourceArchive']) as z:
    for f in snap['files']:
        assert hashlib.sha256(pathlib.Path(f['path']).read_bytes()).hexdigest().upper()==f['sha256']
        rel=f['path'].replace('\\','/').split('/Game/FutureBattle/',1)[1]
        assert hashlib.sha256(z.read(rel)).hexdigest().upper()==f['sha256']
functional={}
for folder,n in [('SquadFlank-ST3b-20261112/effect-r1',43),('Area-20261112/flank-effect-r1',24),('MixedSave-ST4b-20261112/flank-effect-r1',35)]:
    result=read(bench/folder/'result.json');manifest=read(bench/folder/'manifest.json')
    assert result['passed'] and result['checks']==n and result['dll']==manifest['dll']==snap['dll']
    assert manifest['machine']
    if not folder.startswith('SquadFlank'):
        assert '-BattleFlankRelaxed=1' in manifest['arguments'] and '-BattleFlankTeams=1' in manifest['arguments']
    functional[folder]=result
basic=(trial/'regression-flank-effect-r1.txt').read_text(encoding='utf-8-sig')
assert 'BATTLE_SMOKE_RESULT failures=0' in basic
assert basic.count('BATTLE_TEST PASS:')==210
analysis=read(trial/'flank-effect-r1-analysis.json')
assert analysis['verified'] and analysis['complete_matrix'] and len(analysis['runs'])==48
proof=read(trial/'flank-effect-r1-verified.json')
assert proof['verified'] and all(r['dll']==snap['dll'] for r in proof['runs'])
render=read(trial/'flank-effect-rendered-r1-verified.json')
assert render['verified'] and len(render['runs'])==1
r=render['runs'][0]
assert r['dll']==snap['dll'] and r['rendered'] and r['flank_relaxed']==1 and r['flank_teams']==3
assert r['metrics']['mixed_save_round_trip_verified']
log=(trial/r['name']/'run.log').read_text(encoding='utf-8-sig')
assert 'NVIDIA GeForce RTX 5070 Ti' in log and 'D3D12' in log
out=dict(verified=True,dll=snap['dll'],source_archive_sha256=snap['sourceArchiveSha256'],current_source_mismatches=0,
         functional=functional,basic_checks=210,matrix_runs=48,render=r['name'],save_roundtrip=True,
         analysis='flank-effect-r1-analysis.json',render_performance_comparison=False)
(trial/'verification-flank-effect-final.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(out,ensure_ascii=False))
