"""Report B-stage costs from validated paired placement results."""
import json, sys
from pathlib import Path

p = Path(sys.argv[1])
d = json.loads((p / 'summary.json').read_text())
c = {(r['profile'], r['view'], r['filter']): r for r in d['conditions'] if r['stage'] == 'B'}
filters = ['All', 'LOD0', 'LOD1', 'LOD2', 'Dist10_30', 'Dist20_50', 'Screen120_48']
lines = ['# B：同一骨格のLOD比較', '',
    f"{d['runs']}走行。1920×1080、500体記録（50 Actor＋450 Mass）、900測定フレーム・120ウォームアップ。",
    'Lightが主比較、Currentは参考。条件ごとに配置数を明記し、1配置の参考条件に配置間の信頼区間を付けない。', '',
    '|設定|視点|条件|配置数|GPU ms|兵の増分 ms|元LODからのGPU削減 ms|主画面提出人数|増分/提出人数 ms|LOD0/1/2要求人数|',
    '|---|---|---|---:|---:|---:|---:|---:|---:|---|']
for profile in sorted({k[0] for k in c}):
    for view in ['Recorded', 'RTS60', 'TPS', 'FPS']:
        baseline = c[profile, view, 'All']['gpu']
        background = c[profile, view, 'None']['gpu']
        for filt in filters:
            r = c[profile, view, filt]
            increment = r['gpu'] - background
            # This ratio of displayed means is descriptive; paired per-seed ratios are in contrasts.csv.
            per = increment / r['drawn'] if r['drawn'] else 0
            lods = '/'.join(f"{r[f'requested_lod{i}']:.1f}" for i in range(3))
            lines.append(f"|{profile}|{view}|{filt}|{r['seeds']}|{r['gpu']:.3f}|{increment:.3f}|{baseline-r['gpu']:+.3f}|{r['drawn']:.1f}|{per:.5f}|{lods}|")
lines += ['', '## 条件と解釈', '',
    '- Allは元記録のActor LOD2/Mass LOD0を維持。LOD0/1/2は両方式を同一LODに固定。',
    '- Dist10_30は10/30m、Dist20_50は20/50m。Screen120_48は180cmの兵の投影高推定120/48px。距離はカメラとの3D距離、FOVに応じた投影倍率を使う。',
    '- Massの共有姿勢元は必要な骨を保持するLOD0。表示側はリーダーLOD追従を無効にして独立指定する。個別アニメーション化や更新停止は行わない。',
    '- 主画面提出人数は画素寄与を保証しない。兵の増分には画面外の影/更新等も含むため、人数で割った値はメッシュ単独の単価ではない。',
    '- 上表の人数比は平均値どうしの比。配置ごとの対応増分・人数比・参考95%区間はcontrasts.csvに別記。',
    '- RTS300/RTS150は条件数を抑えるため省略。高高度でLOD2となる予測を実測結果として扱わない。',
    '- 画像QAでは記録時計にアニメーション位相を揃える。性能走行は通常の姿勢更新を使う。',
    '- 兵本体は両軍共通資産で、HUD非表示条件だけでは敵味方識別の合格判定はできない。武器の形/色と輪郭の劣化を確認する。',
    '- 通常プレイ、基準機での実測、1000体60FPSは未検証。',
    '', f"DLL SHA256: `{d['dll_sha256']}`", '']
(p / 'lod-report.md').write_text('\n'.join(lines), encoding='utf-8')
print('Wrote', p / 'lod-report.md')
