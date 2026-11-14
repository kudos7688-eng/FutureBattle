"""Verify pinned-build tactical trials, including their archived C++ sources."""
import argparse
import hashlib
import json
import pathlib
import struct
import zipfile

p = argparse.ArgumentParser()
p.add_argument('--prefix', required=True)
p.add_argument('--expected', type=int, required=True)
p.add_argument('--output', required=True)
p.add_argument('--include', nargs='*', default=[])
a = p.parse_args()
root = pathlib.Path(__file__).resolve().parent / 'Benchmarks/SightTrial-20261111'
rows = []
archives = {}
for folder in sorted(set(root.glob(a.prefix + '-*')) | {root / name for name in a.include}):
    if not folder.is_dir():
        continue
    manifest = json.loads((folder / 'manifest.json').read_text(encoding='utf-8-sig'))
    result = json.loads((folder / 'result.json').read_text(encoding='utf-8-sig'))
    metrics = json.loads((folder / 'metrics.json').read_text(encoding='utf-8-sig'))
    snapshot = manifest['source_snapshot']
    if not result['passed'] or result['dll'] != manifest['dll'] or manifest['dll'] != snapshot['dll']:
        raise ValueError(f'Binary/result mismatch: {folder}')
    if manifest['files'] != snapshot['files'] or not manifest.get('machine'):
        raise ValueError(f'Missing provenance: {folder}')
    archive = pathlib.Path(snapshot['sourceArchive'])
    if str(archive) not in archives:
        digest = hashlib.sha256(archive.read_bytes()).hexdigest().upper()
        if digest != snapshot['sourceArchiveSha256']:
            raise ValueError(f'Changed source archive: {archive}')
        with zipfile.ZipFile(archive) as z:
            for item in snapshot['files']:
                relative = item['path'].replace('\\', '/').split('/Game/FutureBattle/', 1)[1]
                if hashlib.sha256(z.read(relative)).hexdigest().upper() != item['sha256']:
                    raise ValueError(f'Archived source mismatch: {relative}')
        archives[str(archive)] = digest
    arguments = manifest['arguments']
    def value(flag, fallback=None):
        return next((x.split('=', 1)[1] for x in arguments if x.startswith(flag + '=')), fallback)
    field = next(x[-1] for x in arguments if x.startswith('/Game/Prototype/Lvl_BattleWide_'))
    map_record = next(x for x in snapshot['maps'] if pathlib.Path(x['path']).name == f'Lvl_BattleWide_{field}.umap')
    if manifest['map'] != map_record['sha256']:
        raise ValueError('Map provenance mismatch')
    rendered = '-RenderOffscreen' in arguments
    if rendered:
        if not manifest['timingComparable'] or manifest['behaviorConcurrency'] != 1:
            raise ValueError('Rendered trial was not isolated')
        png = (folder / 'metrics.json.png').read_bytes()
        if png[:8] != b'\x89PNG\r\n\x1a\n' or struct.unpack('>II', png[16:24]) != (1920, 1080):
            raise ValueError('Unexpected rendered resolution')
    rows.append(dict(name=folder.name, map=field, seed=int(value('-BattleTrialSeed')),
                     swapped='-BattleTrialSwap' in arguments, flank=int(value('-BattleSquadFlank', '1')),
                     flank_relaxed=int(value('-BattleFlankRelaxed','0')), flank_teams=int(value('-BattleFlankTeams','3')),
                     sight_cm=int(value('-BattleSightRange', '10000')), weapon_cm=int(value('-BattleWeaponRange', '5000')),
                     rendered=rendered, dll=manifest['dll'], metrics=metrics))
if len(rows) != a.expected or len({r['dll'] for r in rows}) != 1:
    raise ValueError(f'Expected {a.expected} runs from one build, got {len(rows)}')
lines = ['# 戦術実装の確認', '', '固定ビルドのソースZIPをファイル単位で照合。作業中ソースと測定バイナリは区別する。', '',
         '|条件|迂回ON|対象軍mask|緩和|勝者|秒|生存0/1|迂回採用|到達|接触/援護喪失|迂回部隊秒|',
         '|---|---:|---:|---:|---:|---:|---|---:|---:|---|---:|']
performance_lines = []
for row in rows:
    m = row['metrics']
    e = m['tactical_events']
    duration = m['tactical_squad_seconds'].get('FLANK', 0)
    lines.append(f"|{row['name']}|{row['flank']}|{row['flank_teams']}|{row['flank_relaxed']}|{m['winner']}|{m['match_seconds']:.2f}|{m['ally_alive']}/{m['enemy_alive']}|{e.get('FLANK',0)}|{e.get('WideFlankArrived',0)}|{e.get('WideFlankLostContact',0)}/{e.get('WideFlankLostSupport',0)}|{duration:.2f}|")
    if row['rendered']:
        performance_lines.append(f"{row['name']} 描画wall平均 {m['frame_wall_ms']['mean']:.3f} ms / p95 {m['frame_wall_ms']['p95']:.3f} ms。")
lines += ['', *performance_lines]
lines += ['', '行動時間は1Hzの部隊標本。並行headlessの時間値を性能比較に使わない。少数seedの確認で公平性・戦術優位を確定しない。最終拠点所有と小数点以下を含む得点は各metrics.jsonに保存。', '']
(root / (a.output + '.md')).write_text('\n'.join(lines), encoding='utf-8')
(root / (a.output + '.json')).write_text(json.dumps(dict(verified=True, archives=archives, runs=rows), ensure_ascii=False, indent=2), encoding='utf-8')
print(root / (a.output + '.md'))
