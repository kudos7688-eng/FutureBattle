"""Validate D shadow/material states and report GPU effects and compiler statistics."""
import csv,json,statistics,sys
from pathlib import Path
p=Path(sys.argv[1]); files=sorted(p.glob('seed*-D-*-r*.json')); rows=[]; stats=[]
for f in files:
    d=json.loads(f.read_text(encoding='utf-8-sig')); ss=d['samples']; actors=d['actor_indices']
    assert len(ss)==900 and len(actors)==50
    assert d['camera_errors']==d['diagnostic_errors']==d['visibility_errors']==d['visibility_missing']==0
    assert all(s['shadow_state_errors']==s['material_state_errors']==0 for s in ss),f
    assert all(s['predicted_lod_mismatches']==0 for s in ss),f
    assert all(all(s['lod_codes'][a]=='0' for a in actors) for s in ss),f
    cs=d['condition_settings']; bias=cs['base_directional_bias']+(1 if d['filter'].startswith('Bias1') else 0)
    assert cs['r.Shadow.Virtual.ResolutionLodBiasDirectional']==bias
    if d['filter'] not in ['Shadow30','Shadow60','Combined']: assert all(s['shadow_suppressed_bodies']==0 for s in ss)
    if d['filter'] not in ['Material40','Combined']: assert all(s['simplified_material_bodies']==0 for s in ss)
    row={k:d[k] for k in ['seed','profile','view','filter','run']}
    row.update({k:statistics.mean(s[k] for s in ss) for k in ['gpu','shadow_suppressed_bodies','simplified_material_bodies','shadow_changes','material_changes']})
    rows.append(row)
    if not stats:
        stats=d['material_statistics']
        assert all(s['shader_map_present'] for s in stats)
assert rows
with (p/'material-validation.csv').open('w',encoding='utf-8-sig',newline='') as out:
    w=csv.DictWriter(out,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
(p/'material-compiler-statistics.json').write_text(json.dumps(stats,ensure_ascii=False,indent=2),encoding='utf-8')
lines=['# D：影と材質の探索','',f'{len(rows)}走行。操作可能ActorのLOD0、影/材質の実コンポーネント状態、影バイアスを検査。',
    f"LOD方式={d['lod_policy']}、ヒステリシス={cs['hysteresis_percent']}%。影/材質の距離境界にも同じ幅を適用。遠ざかる時は基準距離×(1+幅)、近づく時は×(1−幅)で切替。",
    '', '|設定|視点|条件|GPU ms|影省略人数|簡略材質人数|','|---|---|---|---:|---:|---:|']
for key in sorted({tuple(r[k] for k in ['profile','view','filter']) for r in rows}):
    rr=[r for r in rows if tuple(r[k] for k in ['profile','view','filter'])==key]
    lines.append('|'+ '|'.join(key)+f"|{statistics.mean(r['gpu'] for r in rr):.3f}|{statistics.mean(r['shadow_suppressed_bodies'] for r in rr):.1f}|{statistics.mean(r['simplified_material_bodies'] for r in rr):.1f}|")
lines+=['','## 材質のコンパイラ統計','',
    '|材質|blend mode (0=opaque, 1=masked)|推定VS texture samples|推定PS texture samples|命令数統計あり|',
    '|---|---:|---:|---:|---|']
for s in stats:
    lines.append(f"|{s['asset']}|{s['blend_mode']}|{s.get('estimated_texture_samples_vs','未取得')}|{s.get('estimated_texture_samples_ps','未取得')}|{s['instruction_counts_available']}|")
lines+=['',
    '同じshader / vertex factory / permutation / frequencyだけを対応比較する。命令数はコンパイラ統計でありGPU時間そのものではない。ゼロや未提供値を費用ゼロとしない。','',
    '|元材質|Shader|VF|周波数|元命令数|簡略命令数|元samplers|簡略samplers|','|---|---|---|---|---:|---:|---:|---:|']
simple=next(s for s in stats if 'T5_SimpleFarMaterial' in s['asset'])
key=lambda s:tuple(s[k] for k in ['shader','vertex_factory','permutation','frequency'])
lookup={key(s):s for s in simple['base_pass_shaders']}
count=0
for original in stats:
    if original is simple:continue
    for s in original['base_pass_shaders']:
        t=lookup.get(key(s))
        if not t or s['frequency_name']!='pixel':continue
        count+=1
        lines.append(f"|{original['asset']}|{s['shader']}|{s['vertex_factory']}|{s['frequency_name']}|{s['instructions']}|{t['instructions']}|{s['samplers']}|{t['samplers']}|")
if not count:lines+=['','対応するpixel shader統計なし。詳細はmaterial-compiler-statistics.json。']
lines+=['','表はロードされたshader mapの対応するコンパイル済みvariant。各drawで実際に使ったshaderをGPUトレースで同定した結果ではない。']
lines+=['','距離はカメラとの3D距離。高いRTSカメラでは広範囲の影が消えるため、通常プレイへの一律適用はできない。',
    '元Quinn材質はMasked、簡略版はOpaque。テクスチャだけでなくマスク処理/材質定数も変わる代理試験で、テクスチャ参照単独の費用差とは扱わない。',
    '簡略材質は既存M_ArenaCoverの一時複製。テクスチャ細部を失う代理条件で、兵種/陣営識別の維持を保証しない。2材質スロットを維持し、描画バッチ統合は行わない。',
    'Bias1は全体のVSM directional biasを+1にするため、背景も変わる。兵の効果はBias1−Bias1NoneとAll−Noneの差で比較する。',
    'Light3配置/Current1配置の探索。1000体60FPSと基準機換算は未検証。','']
(p/'material-report.md').write_text('\n'.join(lines),encoding='utf-8')
print('D validation:',len(rows),'runs; component states and bias verified; matched pixel rows:',count)
