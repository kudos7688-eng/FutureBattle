"""Report D paired costs against the identical BC baseline and background controls."""
import json,sys
from pathlib import Path
p=Path(sys.argv[1]);d=json.loads((p/'summary.json').read_text())
idx={(r['profile'],r['view'],r['filter']):r for r in d['conditions']}
lines=['# D：影・材質による追加削減','',
    f"{d['runs']}走行。B/C基準＝Screen120_48＋20%ヒステリシス＋操作可能Actor LOD0。",
    'Light3配置が主比較、Current1配置は4条件のみの参考。各2反復。削減は正が改善。',
    '', '|設定|視点|BC GPU ms|兵の増分 ms|Shadow30削減 ms|Shadow60削減 ms|Material40削減 ms|Combined削減 ms|',
    '|---|---|---:|---:|---:|---:|---:|---:|']
for profile in ['Light','Current']:
    for view in ['Recorded','RTS60','TPS','FPS']:
        base=idx[profile,view,'All']['gpu'];bg=idx[profile,view,'None']['gpu']
        fields=[]
        for filt in ['Shadow30','Shadow60','Material40','Combined']:
            r=idx.get((profile,view,filt));fields.append(f"{base-r['gpu']:+.3f}" if r else '未測定')
        lines.append(f"|{profile}|{view}|{base:.3f}|{base-bg:.3f}|"+'|'.join(fields)+'|')
lines+=['','VSM directional resolution biasは−1.5→−0.5（+1）。背景にも効くため兵の増分の差を分離する。','',
    '|設定|視点|画面全体のGPU削減 ms|兵の増分の削減 ms|','|---|---|---:|---:|']
for profile in ['Light','Current']:
    for view in ['Recorded','RTS60','TPS','FPS']:
        if (profile,view,'Bias1') not in idx:continue
        g=lambda f:idx[profile,view,f]['gpu']
        lines.append(f"|{profile}|{view}|{g('All')-g('Bias1'):+.3f}|{g('All')-g('None')-g('Bias1')+g('Bias1None'):+.3f}|")
lines+=['','## 測定条件と限界','',
    '- GPU以外のゲーム/描画/RHIスレッド、フレーム時間、p95、提出人数、三角形数、ドローコールはcondition-means.csvとruns.csv。配置単位の対応差はcontrasts.csv/paired-differences.csv。',
    '- 影距離30/60m、材質距離40mには20%幅を適用。遠ざかる時の切替は36/72/48m、近づく時は24/48/32m。距離はカメラからの3D距離。',
    '- 影30m＋材質40mがCombined。影と材質の単独効果を足し算して合成効果としない。',
    '- Material40は既存M_ArenaCoverを一時複製した不透明単色材質。材質スロット数と武器材質は維持。装甲の細部を失う代理試験である。',
    '- 元材質はMasked、簡略材質はOpaque。マスク処理や材質定数も変わるため、テクスチャ削減単独の効果とは解釈しない。',
    '- コンパイラ命令数/サンプラー数と実メッシュの状態検査は各rawフォルダーのmaterial-report.md、material-validation.csv、material-compiler-statistics.json。',
    '- 画像QAはT5D-QA-20261109に分離。ブラーを外した比較でRTSの接地感低下、遠方材質の細部減少を確認。長い影・急旋回・全時間帯の切替品質を保証しない。',
    '- 基準機での実測、1000体通常AI込み、敵味方/兵種判別の合格、個別アニメーション/FXは未検証。',
    '',f"DLL SHA256: `{d['dll_sha256']}`",'']
(p/'shadow-material-costs.md').write_text('\n'.join(lines),encoding='utf-8')
print('Wrote',p/'shadow-material-costs.md')
