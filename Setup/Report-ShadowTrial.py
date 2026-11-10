"""Validate matched shadow-caster sets and report the T5 Shadow experiment."""
import json, sys, statistics
from pathlib import Path

p = Path(sys.argv[1])
data = {}
for f in p.glob('seed*-Shadow-*-r*.json'):
    d = json.loads(f.read_text(encoding='utf-8-sig'))
    data[d['seed'], d['profile'], d['view'], d['filter'], d['run']] = d
checks = []
for (seed, profile, view, filt, run), d in data.items():
    if filt != 'FrustumShadow':
        continue
    base = data[seed, profile, view, 'All', run]
    hide = data[seed, profile, view, 'FrustumHide', run]
    row = dict(seed=seed, profile=profile, view=view, run=run)
    for name in ('body', 'weapon'):
        field = f'shadow_{name}_bits'
        eq = sum(a[field] == b[field] for a, b in zip(d['samples'], base['samples']))
        assert eq == 900, (row, name, eq)
        row[name + '_caster_set_identical_frames'] = eq
        row[name + '_casters_removed_by_hide_mean'] = statistics.mean(
            (int.from_bytes(bytes.fromhex(a[field]), 'little') &
             ~int.from_bytes(bytes.fromhex(b[field]), 'little')).bit_count()
            for a, b in zip(base['samples'], hide['samples']))
    assert all(s['updated_bodies'] == 500 and s['tick_enabled_bodies'] == 500 for s in d['samples'])
    assert all(s['hidden_shadow_bodies'] == 0 for s in base['samples'] + hide['samples'])
    row['hidden_shadow_bodies_mean'] = statistics.mean(s['hidden_shadow_bodies'] for s in d['samples'])
    checks.append(row)
assert checks
(p / 'shadow-validation.json').write_text(json.dumps(checks, indent=2), encoding='utf-8')
print('Shadow caster eligibility sets identical to All:', len(checks), 'runs x 900 frames, bodies and weapons')
if not (p / 'summary.json').exists():
    sys.exit(0)
d = json.loads((p / 'summary.json').read_text())
c = {(r['profile'], r['view'], r['filter']): r for r in d['conditions']}
lines = ['# 影を保護した画面外非表示の検証', '',
    f"{len(d['seeds'])}配置・各2反復、{d['runs']}走行。1920×1080、500表示スロット（50 Actor＋450 Mass）、元の戦闘記録を再生。",
    'GPU時間は配置ごとに反復平均してから配置間で平均。削減量は通常表示−対象条件で、正が改善。', '',
    '|設定|視点|通常GPU ms|影も消すGPU ms|影を残すGPU ms|影も消す削減 ms|影を残す削減 ms|削減の残存率|',
    '|---|---|---:|---:|---:|---:|---:|---:|']
for profile in sorted({k[0] for k in c}):
    for view in ('TPS', 'FPS'):
        a, h, s = [c[profile, view, f]['gpu'] for f in ('All', 'FrustumHide', 'FrustumShadow')]
        ratio = f'{(a-s)/(a-h)*100:.1f}%' if a-h > .05 else '分母が小さく評価しない'
        lines.append(f'|{profile}|{view}|{a:.3f}|{h:.3f}|{s:.3f}|{a-h:+.3f}|{a-s:+.3f}|{ratio}|')
lines += ['', '## 配置単位の対応差（GPU、対象条件−通常表示）', '',
    '負が改善。3配置の95%区間は探索の参考値で、精密な採用判断には使わない。', '',
    '|設定|視点|条件|平均差 ms|参考95%区間 ms|', '|---|---|---|---:|---:|']
for r in d['contrasts']:
    if r['metric'] != 'gpu' or not r['comparison'].endswith('-All'):
        continue
    _, profile, view, comparison = r['comparison'].split(':')
    lines.append(f"|{profile}|{view}|{comparison}|{r['difference']:+.4f}|[{r['ci_low']:+.4f}, {r['ci_high']:+.4f}]|")
lines += ['', '## 検証範囲', '',
    '- 本体・武器をHiddenInGameで隠し、影維持条件では画面外の生存・表示許可対象だけCastHiddenShadowを有効化。距離制限・更新停止は追加しない。',
    '- 比較3条件すべて同じコンポーネント単位の表示経路を使用。従来A3とは別の同一DLL対照で比較する。',
    '- 本体・武器の影を落とせる集合が、影維持と通常表示で全記録フレーム一致。これはレンダラーの影対象資格であり、全影ピクセルの完全一致を意味しない。',
    '- 位置反映とメッシュTickは全条件維持。既存のUE姿勢最適化は維持し、個体ごとの姿勢評価回数が同じとは保証しない。',
    '- 画面外判定には既存の半径2.5mの余裕を使用。全画面内メッシュの完全保持を保証する正式な実装ではない。',
    '- LightはHWRT・Lumen停止。Currentは現行描画設定。反射・GIの保持、通常AI込みの性能、急旋回時の復帰、基準機での実性能はこの再生比較だけでは検証できない。',
    '- 少数配置の探索。95%区間は参考値。詳細な配置別対応差、主画面ID差、フレームp95は同フォルダのCSVに保存。',
    '- 影維持で削減が消えても、その差すべてを影描画パス単独の時間とは解釈しない。影のために残るスキニング等の処理や表示切替も含む。',
    '- 画像QAは別フォルダT5-shadow-final-QA-20261109。低い太陽角度・固定姿勢で影の欠けを比較し、性能には使用しない。動的な影の滑らかさや全視点での完全一致は未検証。',
    '', f"DLL SHA256: `{d['dll_sha256']}`", '']
(p / 'shadow-report.md').write_text('\n'.join(lines), encoding='utf-8')
print('\n'.join(lines[:12]))
