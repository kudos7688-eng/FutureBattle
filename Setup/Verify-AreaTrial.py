"""Verify the final prototype evidence without retroactively adding provenance."""
import hashlib
import json
import pathlib
import re
import zipfile

root = pathlib.Path(__file__).resolve().parent
bench = root / 'Benchmarks'
trial = bench / 'SightTrial-20261111'
def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))
snapshot = read(bench / 'BuildSnapshots/area-r6/manifest.json')
movement_snapshot = read(bench / 'BuildSnapshots/area-r5/manifest.json')
for snap in [snapshot, movement_snapshot]:
    archive = pathlib.Path(snap['sourceArchive'])
    assert hashlib.sha256(archive.read_bytes()).hexdigest().upper() == snap['sourceArchiveSha256']
    with zipfile.ZipFile(archive) as z:
        for item in snap['files']:
            relative = item['path'].replace('\\', '/').split('/Game/FutureBattle/', 1)[1]
            assert hashlib.sha256(z.read(relative)).hexdigest().upper() == item['sha256']
checks = {}
for folder in ['Area-20261112/probe-r6', 'MovementStability-20261111/area-r5',
               'ContactMovement-20261111/area-r5', 'WideMixed-20261111/area-r5',
               'MixedFast-20261112/area-r6', 'MixedSave-ST4b-20261112/area-r6']:
    result = read(bench / folder / 'result.json')
    manifest = read(bench / folder / 'manifest.json')
    expected = movement_snapshot if folder.endswith('area-r5') else snapshot
    assert result['passed'] and result['dll'] == expected['dll'], folder
    assert manifest.get('machine') and manifest['dll'] == expected['dll'], folder
    checks[folder] = result
assert 'BATTLE_SMOKE_RESULT failures=0' in (trial / 'regression-area-r6.txt').read_text(encoding='utf-8-sig')
matrix = read(trial / 'report-area-final-r6.json')
assert matrix['verified'] and len(matrix['runs']) == 5
for row in matrix['runs']:
    assert row['dll'] == snapshot['dll']
    m = row['metrics']
    assert m['ruleset'] == 'Area' and 0 < m['match_seconds'] <= 601
    assert m['completed_replacements'] + m['ally_resources'] + m['enemy_resources'] == 200
    if 'TIME LIMIT' in m['reason']:
        for team, side in enumerate(['ally', 'enemy']):
            expected = m[side + '_alive'] + m[side + '_resources'] + 20 * sum(p['owner'] == team for p in m['final_objectives'])
            assert m[side + '_score'] == expected
    if row['name'].endswith('A-0') or row['rendered']:
        assert m['mixed_save_round_trip_verified']
for item in snapshot['files']:
    assert hashlib.sha256(pathlib.Path(item['path']).read_bytes()).hexdigest().upper() == item['sha256']
assert hashlib.sha256(pathlib.Path(snapshot['sourceArchive']).read_bytes()).hexdigest().upper() == snapshot['sourceArchiveSha256']
out = {'verified': True, 'dll': snapshot['dll'], 'source_archive': snapshot['sourceArchiveSha256'],
       'current_source_mismatches': 0, 'functional': checks, 'area_matrix': matrix}
(trial / 'verification-area-final-r6.json').write_text(json.dumps(out, ensure_ascii=False, indent=2), encoding='utf-8')
lines = ['# 工程8・1エリア戦の検証', '',
         f"DLL: `{snapshot['dll']}`。ソースZIP、現在のソース/設定、マップ、各走行前の測定機記録を確認。", '',
         '|条件|秒|勝者|生存0/1|資源0/1|補充完了|迂回到達|決着|',
         '|---|---:|---:|---|---|---:|---:|---|']
for row in matrix['runs']:
    m = row['metrics']
    log = (trial / row['name'] / 'run.log').read_text(encoding='utf-8-sig')
    arrivals = len(re.findall(r'BATTLE_FLANK_END[^\r\n]*reason=FLANK ARRIVED', log))
    lines.append(f"|{row['name']}|{m['match_seconds']:.2f}|{m['winner']}|{m['ally_alive']}/{m['enemy_alive']}|{m['ally_resources']}/{m['enemy_resources']}|{m['completed_replacements']}|{arrivals}|{m['reason']}|")
lines += ['', '資源と兵数の保存則、時間切れの得点式、途中保存/読込を照合。並行headlessの時間値は性能比較対象外。単独描画も保存I/Oを含む一戦確認であり、旧測定との性能比較には使用しない。少数seedで戦術優位や公平性を確定しない。',
          '', '最終r6では描画中読込のTaskGraph再入クラッシュを修正。VAT表示器の描画設定を再起動せず、登録兵の表示だけを再構成する。エリア専用・基本・混成購入/保存・通常4戦/描画1戦はr6。移動4・接触3・固定広域の回帰は直前r5の証拠としてDLLを分けて照合した（r6は表示再初期化とUNKNOWN表示のみ変更）。r5の描画途中読込失敗は成功統計へ含めない。',
          '', '表の迂回到達は一戦全体のBATTLE_FLANK_ENDログから数えた。A0と単独描画Bは80秒時点で読込を挟むため、metrics.jsonの戦術イベント/部隊時間カウンタは読込後の区間となる。ゲーム内の計画・時計・資源・置換履歴は保持する。読込を挟む条件と挟まない条件を完全同一入力の反復とは扱わない。',
          '', '専用機構試験は本拠地への実射撃、資源0での決定勝利、同点、補充途中/完了後保存、不正参照/資源過予約、阻塞時無消費、隊長死亡取消、情報秘匿を含む。通常戦闘で専用試験の全場面が自然発生したという意味ではない。',
          '', '描画Bの終盤には、帰還した第3/第6部隊が遠い防衛目標への移動待ちを表示していた。帰還・補充・任務復帰の状態遷移と、実際に前線へ再到達することは区別する。到着枠を確保できない場合は出発前でも待機する実装があり、予約競合と途中衝突の寄与は今回のログだけでは分離できない。全条件で無停滞とはしておらず、通常戦闘の待機解消は継続課題。',
          '', '追加/補充兵はskeletal fallbackで表示し、読込時にVATへ組み込む。混成FastMatchの一戦内登録上限は死亡分込み500。1エリア戦は現行500体＋置換済み死亡履歴最大200。支援砲撃は旧全Actorモードのみ。配布用cook/パッケージ、基準PC性能、長期バランス評価は対象外。', '']
(trial / 'report-area-final.md').write_text('\n'.join(lines), encoding='utf-8')
print(trial / 'report-area-final.md')
