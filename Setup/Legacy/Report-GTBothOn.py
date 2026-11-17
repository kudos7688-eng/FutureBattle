import csv,json,sys,importlib.util
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
spec=importlib.util.spec_from_file_location('analysis',Path(__file__).with_name('Analyze-GTTrace.py'))
mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
root=Path(sys.argv[1]);views=['RTS','TPS','FPS'];known=['perception_ms','decision_ms','path_ms','follow_ms','rvo_ms','shot_ms','display_ms'];runs={}
for view in views:
 for kind in ['control','light','trace']:
  p=root/f'{view}-{kind}'
  if not (p/'analysis.json').exists():mod.analyze(p)
  runs[f'{view}-{kind}']=json.loads((p/'analysis.json').read_text(encoding='utf-8'))
lines=['# 両最適化ON：残存GT費用の再測定','',
 '同DLL・SightContext再利用ON・非表示部下骨更新削減ON、1000体Leader・高HP・Open・seed1・通常B・1080p。warmup10秒＋60秒。各条件1走行。時間はms/frame。旧測定と混合しない。','',
 '|視点|GT平均|GT p95|16.67ms超過率%|認識|視線（認識の内数）|判断|経路|実行follow|RVO|射撃|表示scope|概算未分類残差|',
 '|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|']
budgets={}
for v in views:
 p=runs[f'{v}-control']['performance'];gt=p['gt_ms']['mean'];res=gt-sum(p[k]['mean'] for k in known)
 values=[gt,p['gt_ms']['p95'],p['gt_ms']['over_16_67_pct'],p['perception_ms']['mean'],p['sight_scan_ms']['mean'],*[p[k]['mean'] for k in known[1:]],res]
 lines.append('|'+v+'|'+'|'.join(f'{x:.3f}' for x in values)+'|')
 saving=.5*p['perception_ms']['mean']+.25*p['follow_ms']['mean']+.3*p['rvo_ms']['mean']
 budgets[v]={'gt':gt,'required_ms':gt-16.67,'residual_ms':res,'assumed_perception50_follow25_rvo30_saving_ms':saving,'still_required_ms':gt-saving-16.67,'required_fraction_of_residual_pct':100*(gt-saving-16.67)/res}
lines+=['','残差はGTから既存7scopeを引いた概算。視線は認識に含むので加算しない。','',
 '|視点|wall平均|RT平均|RT p95|GPU平均|GPU p95|', '|---|---:|---:|---:|---:|---:|']
for v in views:
 p=runs[f'{v}-control']['performance'];a=[p[k][s] for k,s in [('wall_ms','mean'),('rt_ms','mean'),('rt_ms','p95'),('gpu_ms','mean'),('gpu_ms','p95')]]
 lines.append('|'+v+'|'+'|'.join(f'{x:.3f}' for x in a)+'|')
lines+=['','## トレース負荷と独立走行の変動','', '|視点|対照GT|軽量GT|差%|詳細GT|差%|','|---|---:|---:|---:|---:|---:|']
for v in views:
 c,l,d=[runs[f'{v}-{k}']['performance']['gt_ms']['mean'] for k in ['control','light','trace']]
 lines.append(f'|{v}|{c:.3f}|{l:.3f}|{100*(l/c-1):.1f}|{d:.3f}|{100*(d/c-1):.1f}|')
lines+=['','計測負荷と戦況/仕事量変動を含む。軽量/詳細の費用を対照の残差にそのまま転用しない。','',
 '## 軽量トレース：GT上の排他的な区分','',
 '|視点|GameMode配下|アニメTick|アニメGT反映|描画データ更新|CharacterMovement|待機scope|その他|分類合計|EngineTick平均|',
 '|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|']
cats=['game_mode','animation_tick','animation_completion','render_updates','character_movement','wait_scope','other']
for v in views:
 t=runs[f'{v}-light']['trace'];g=t['groups_ms'];engine=next(x['ms'] for x in t['top_inclusive_ms'] if x['name']=='FEngineLoop::Tick')
 assert t['ticks']==t['window']['frames'] and abs(sum(g.values())-engine)<.001
 a=[g.get(k,0) for k in cats]+[sum(g.values()),engine]
 lines.append('|'+v+'|'+'|'.join(f'{x:.3f}' for x in a)+'|')
lines+=['','親子scopeを二重計上せず祖先で分類。GameMode配下には認識/移動/RVO等が含まれる。待機は依存待ちを含み削減可能時間と同義ではない。GPU/worker時間はこのGT区分に加算しない。軽量のHUD/Slate個別時間は未測定（その他に含む）で、0msという意味ではない。','',
 '## 詳細トレースの主な自己時間','']
details={}
for v in views:
 p=root/f'{v}-trace';n=json.loads((p/'window.json').read_text())['frames'];r=list(csv.DictReader(open(p/'window-statistics.csv',encoding='utf-8-sig')))
 rows=sorted([{'name':x['Name'],'self_ms':float(x['Excl'])*1000/n,'inclusive_ms':float(x['Incl'])*1000/n,'calls_per_frame':int(x['Count'])/n} for x in r],key=lambda x:-x['self_ms'])
 details[v]=rows[:80]
 lines += [f'### {v}（同じ詳細走行のみ）','', '|scope|自己ms/frame|包括ms/frame|回数/frame|','|---|---:|---:|---:|']
 for x in rows[:16]:lines.append(f"|{x['name']}|{x['self_ms']:.3f}|{x['inclusive_ms']:.3f}|{x['calls_per_frame']:.1f}|")
 lines+=['']
lines+=['## 約7msへ届く条件の算術（実装後予測ではない）','',
 '認識50%・follow25%・RVO30%の削減を仮に達成した場合を同じ対照走行だけで計算。各削減率は未測定であり、期待値ではない。','',
 '|視点|平均GTの不足|上記3項目の仮の削減|それでも必要な削減|概算残差に対する割合%|','|---|---:|---:|---:|---:|']
for v,b in budgets.items():lines.append('|'+v+'|'+'|'.join(f'{b[k]:.3f}' for k in ['required_ms','assumed_perception50_follow25_rvo30_saving_ms','still_required_ms','required_fraction_of_residual_pct'])+'|')
lines+=['','費用の大きさは削減可能額そのものではない。追加最適化後の削減量/p95/感覚・戦闘・表示の等価性は未測定。開発機のみ、基準PC換算未取得。']
(root/'tables.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
(root/'summary.json').write_text(json.dumps({'runs':runs,'budgets':budgets,'detailed_self_times':details},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(budgets,indent=2))
