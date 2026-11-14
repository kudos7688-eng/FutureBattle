"""Summarize completed sight trials without mixing preliminary or DLL revisions."""
import argparse
import datetime
import hashlib
import json
import math
import pathlib
import statistics
import struct

p = argparse.ArgumentParser()
p.add_argument('--weapon', action='store_true')
p.add_argument('--prefix', required=True)
p.add_argument('--performance-prefix', required=True)
p.add_argument('--output', default='report-stage1')
a = p.parse_args()
behavior_count = 36 if a.weapon else 48
distances = (50, 75, 100) if a.weapon else (50, 75, 100, 150)
performance_distances = (50, 75, 100) if a.weapon else (50, 100, 150)
range_flag = '-BattleWeaponRange=' if a.weapon else '-BattleSightRange='
root = pathlib.Path(__file__).resolve().parent / 'Benchmarks/SightTrial-20261111'

def read_runs(prefix):
    runs = []
    for directory in sorted(root.glob(prefix + '-*')):
        if not directory.is_dir():
            continue
        manifest = json.loads((directory / 'manifest.json').read_text(encoding='utf-8-sig'))
        result = json.loads((directory / 'result.json').read_text(encoding='utf-8-sig'))
        metrics = json.loads((directory / 'metrics.json').read_text(encoding='utf-8-sig'))
        if not result['passed'] or result['dll'] != manifest['dll']:
            raise ValueError(f'Failed or changed binary: {directory}')
        arguments = manifest['arguments']
        field = next(x[-1] for x in arguments if x.startswith('/Game/Prototype/Lvl_BattleWide_'))
        distance = int(next(x.split('=')[1] for x in arguments if x.startswith(range_flag))) // 100
        if a.weapon and '-BattleSightRange=10000' not in arguments:
            raise ValueError(f'Weapon comparison requires recognition 100m: {directory}')
        runs.append(dict(name=directory.name, map=field, distance=distance,
                         seed=int(next(x.split('=')[1] for x in arguments if x.startswith('-BattleTrialSeed='))),
                         swapped='-BattleTrialSwap' in arguments, manifest=manifest, metrics=metrics))
    return runs

behavior = read_runs(a.prefix)
performance = read_runs(a.performance_prefix)
if len(behavior) != behavior_count or len(performance) != 9:
    raise ValueError(f'Expected {behavior_count} behavior + 9 performance runs, got {len(behavior)} + {len(performance)}')
expected = {(field, distance, seed, swapped) for field in ('A', 'B')
            for distance in distances for seed in (7, 19, 43) for swapped in (False, True)}
actual = {(r['map'], r['distance'], r['seed'], r['swapped']) for r in behavior}
if actual != expected:
    raise ValueError('Missing or duplicate behavior condition')
for distance in performance_distances:
    rows = [r for r in performance if r['distance'] == distance]
    if len(rows) != 3 or any(r['map'] != 'B' or r['seed'] != 7 or r['swapped'] for r in rows):
        raise ValueError('Invalid performance repetitions')
dlls = {r['manifest']['dll'] for r in behavior + performance}
if len(dlls) != 1:
    raise ValueError('Mixed binaries')
source_sets = {tuple(sorted((f['path'], f['sha256']) for f in r['manifest']['files']))
               for r in behavior + performance}
if len(source_sets) != 1:
    raise ValueError('Source/config manifests differ between runs')
for field in ('A', 'B'):
    if len({r['manifest']['map'] for r in behavior + performance if r['map'] == field}) != 1:
        raise ValueError(f'Map {field} changed between runs')
if any(not r['manifest']['timingComparable'] for r in performance):
    raise ValueError('Concurrent rendered measurement')
saved_render_settings = {tuple(sorted((f['path'], f['sha256']) for f in r['manifest']['savedSettings']
                                     if pathlib.Path(f['path']).name in ('GameUserSettings.ini', 'Engine.ini')))
                         for r in performance}
if len(saved_render_settings) != 1:
    raise ValueError('Rendered settings changed between performance runs')
for r in performance:
    screenshot = root / r['name'] / 'metrics.json.png'
    data = screenshot.read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n' or struct.unpack('>II', data[16:24]) != (1920,1080):
        raise ValueError(f'Wrong rendered resolution: {screenshot}')

def mean(rows, key):
    return statistics.mean(r['metrics'][key] for r in rows)

def percentile(values, fraction):
    values = sorted(values)
    return values[max(0, math.ceil(len(values) * fraction) - 1)] if values else None

lines = ['# Stage 2 射程比較（認識100m固定）' if a.weapon else '# Stage 1 認識距離比較', '', f'DLL SHA256: `{next(iter(dlls))}`', '',
         '挙動48走行と描画9走行を別集計。予備走行は含めない。各条件3seed×陣営入替の6走行は探索用であり、勝率の精密推定ではない。通常既定50mは変更していない。', '',
         '描画条件：1920×1080、RTS全景、Low（Scalability各項目0）、ResolutionQuality100、VSync/動的解像度OFF、VAT既定ON。測定機は各manifestに保存。', '',
         '|Map|認識m|Team0勝/6|左側勝/6|試合秒平均|初認識m平均|初射撃m平均|射撃平均|射程外・直接観測人秒平均|生存0/1平均|',
         '|---|---:|---:|---:|---:|---:|---:|---:|---:|---|']
for field in ('A', 'B'):
    for distance in distances:
        rows = [r for r in behavior if r['map'] == field and r['distance'] == distance]
        wins = sum(r['metrics']['winner'] == 0 for r in rows)
        left = sum(r['metrics']['winner'] == int(r['swapped']) for r in rows)
        lines.append(f"|{field}|{distance}|{wins}|{left}|{mean(rows,'match_seconds'):.2f}|{mean(rows,'first_sight_cm')/100:.2f}|{mean(rows,'first_shot_cm')/100:.2f}|{mean(rows,'shots'):.1f}|{mean(rows,'visible_outside_person_seconds'):.1f}|{mean(rows,'ally_alive'):.1f}/{mean(rows,'enemy_alive'):.1f}|")
lines += ['', '描画性能はB/seed7/通常側、各3回。以下は各走行の統計値の平均（全フレームを結合したp95ではない）。戦闘経過・生存数・表示数の差を含み、認識処理だけの因果効果ではない。', '',
          '|認識m|wall平均ms|wall p95 ms|wall最大ms|認識平均ms/frame|認識p95 ms/frame|',
          '|---:|---:|---:|---:|---:|---:|']
for distance in performance_distances:
    rows = [r for r in performance if r['distance'] == distance]
    values = [statistics.mean(r['metrics'][key][stat] for r in rows)
              for key, stat in [('frame_wall_ms','mean'),('frame_wall_ms','p95'),('frame_wall_ms','max'),
                                ('perception_ms_per_frame','mean'),('perception_ms_per_frame','p95')]]
    lines.append('|' + str(distance) + '|' + '|'.join(f'{v:.3f}' for v in values) + '|')
lines += ['', '計測定義はINSTRUCTION_SightTrial_20261111.mdを参照。到達人数は終了時スナップショットで命令成功率ではない。交戦距離と射程外時間は5Hz標本・既知位置による。並行headlessの時間値は性能に使わない。', '']
if a.weapon:
    lines = [line.replace('挙動48走行', '挙動36走行').replace('通常既定50mは変更していない。', '通常認識は100m、通常射程は50m。新マップ任務割当を点対称化したDLLで測定。').replace('|認識m|', '|射程m|') for line in lines]
summaries = []
for field in ('A', 'B'):
    for distance in distances:
        rows = [r for r in behavior if r['map'] == field and r['distance'] == distance]
        shot_distances = [x / 100 for r in rows for x in r['metrics']['shot_distances_cm'] if x >= 0]
        engagement_distances = [x / 100 for r in rows for x in r['metrics']['engagement_distances_cm']]
        summaries.append(dict(map=field, range_m=distance,
            shots_distance_m_p50=percentile(shot_distances,.5), shots_distance_m_p95=percentile(shot_distances,.95),
            engagement_distance_m_p50=percentile(engagement_distances,.5), engagement_distance_m_p95=percentile(engagement_distances,.95),
            path_requests_mean=mean(rows,'path_requests'), max_path_m=max(r['metrics']['max_path_cm']/100 for r in rows),
            current_reservation_arrivals_mean=mean(rows,'alive_within_1m_current_reservation'),
            reports_delivered_mean=statistics.mean(r['metrics']['counts']['reports_delivered'] for r in rows),
            perception_candidates_mean=statistics.mean(r['metrics']['counts']['perception_candidates'] for r in rows),
            shared_outside_person_seconds_mean=mean(rows,'shared_outside_person_seconds'),
            memory_outside_person_seconds_mean=mean(rows,'memory_outside_person_seconds')))
lines += ['|Map|認識m|認識開始距離p50/p95 m|射撃距離p50/p95 m|経路要求平均|最大経路m|終了時予約先1m内平均|共有配信平均|',
          '|---|---:|---|---|---:|---:|---:|---:|']
for s in summaries:
    lines.append(f"|{s['map']}|{s['range_m']}|{s['engagement_distance_m_p50']:.2f}/{s['engagement_distance_m_p95']:.2f}|{s['shots_distance_m_p50']:.2f}/{s['shots_distance_m_p95']:.2f}|{s['path_requests_mean']:.1f}|{s['max_path_m']:.1f}|{s['current_reservation_arrivals_mean']:.1f}|{s['reports_delivered_mean']:.0f}|")
lines += ['', '距離分布は6走行の標本を結合した値。経路要求の時間は各metrics.jsonのpath_query_msに保存（並行headlessの所要時間は比較しない）。候補数は距離判定前の敵候補数で、今回既定OFFの空間グリッドを有効にした場合の候補数とは異なる。混成経路のsight_rays等には未接続の既存カウンタがあるため、0を処理ゼロと解釈しない。通常全軍の停滞は警告ログだけでは判定できず、別の移動・接触検証と区別する。', '']
if a.weapon:
    lines = [line.replace('|認識m|', '|射程m|') for line in lines]
else:
    for item in summaries:
        item['sight_m'] = item.pop('range_m')
mismatches = []
for item in behavior[0]['manifest']['files']:
    path = pathlib.Path(item['path'])
    if hashlib.sha256(path.read_bytes()).hexdigest().upper() != item['sha256']:
        mismatches.append(str(path))
project = root.parents[2]
for field in ('A', 'B'):
    path = project / f'Game/FutureBattle/Content/Prototype/Lvl_BattleWide_{field}.umap'
    expected = next(r['manifest']['map'] for r in behavior if r['map'] == field)
    if hashlib.sha256(path.read_bytes()).hexdigest().upper() != expected:
        mismatches.append(str(path))
binary = project / 'Game/FutureBattle/Binaries/Win64/UnrealEditor-FutureBattle.dll'
if hashlib.sha256(binary.read_bytes()).hexdigest().upper() != next(iter(dlls)):
    mismatches.append(str(binary))
verification = dict(verified_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(), dll=next(iter(dlls)), behavior_runs=len(behavior), performance_runs=len(performance),
                    source_mismatches=mismatches, runs=[r['name'] for r in behavior + performance])
(root / (a.output + '.md')).write_text('\n'.join(lines), encoding='utf-8')
(root / (a.output + '-verification.json')).write_text(json.dumps(verification, ensure_ascii=False, indent=2), encoding='utf-8')
(root / (a.output + '-summary.json')).write_text(json.dumps(summaries, ensure_ascii=False, indent=2), encoding='utf-8')
if mismatches:
    raise ValueError('Source changed since measurements')
print(root / (a.output + '.md'))
