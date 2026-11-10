import csv,json,math,sys,statistics,re
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')
root=Path(sys.argv[1]);runs=[]
keys=['gt_ms','rt_ms','gpu_ms','wall_ms','perception_ms','sight_scan_ms','decision_ms','path_ms','follow_ms','rvo_ms','shot_ms','display_ms']
def stats(v):
    a=sorted(v);return {'mean':statistics.mean(a),'p95':a[math.ceil(.95*len(a))-1],'over_16_67_pct':100*sum(x>16.67 for x in a)/len(a)}
for p in sorted(root.glob('perf-*')):
    m=json.load(open(p/'manifest.json',encoding='utf-8-sig'));r=list(csv.DictReader(open(p/'frames.csv',encoding='utf-8-sig')))
    assert m['context_reuse']==1 and m['hidden_bones']==1 and m['perception_grid']==1 and m['anim_tasks'] in [0,1]
    assert m['status']=='passed' and m['performance_measurement'] and not m['context_audit'] and not m['cpu_trace']
    assert all(int(x['alive'])==1000 and int(x['rvo_registered'])==1000 and x['view']==m['view'] and float(x['gpu_ms'])>0 for x in r)
    d={'folder':p.name,'view':m['view'],'reuse':m['anim_tasks'],'dll':m['dll_sha256'],'frames':len(r),
       'timing':{k:stats([float(x[k]) for x in r]) for k in keys},
       'sensor_lateness_max_ms':max(float(x['sensor_lateness_max_ms']) for x in r),
       'batch_lateness_max_ms_including_warmup':float(re.search(r'P3_BATCH_LATENESS max_ms=([\d.]+)',(p/'run.log').read_text(encoding='utf-8-sig')).group(1)),
       'decision_lateness_max_ms':max(float(x['decision_lateness_max_ms']) for x in r),
       'pending_peak':max(int(x['pending_reports_peak']) for x in r),
       'shots_interval':int(r[-1]['shots'])-int(r[0]['shots']),
       'last_mean_distance_cm':float(r[-1]['mean_distance_cm']),
       'paths_interval':int(r[-1]['paths'])-int(r[0]['paths'])}
    work=list(map(int,re.search(r'P3_GRID_WORK observers=(\d+) candidates=(\d+) far_skipped=(\d+)',(p/'run.log').read_text(encoding='utf-8-sig')).groups()))
    assert work[0]>0 and work[2]==0
    d['observer_updates_including_warmup']=work[0];d['candidates_including_warmup']=work[1];d['candidates_per_observer_update']=work[1]/work[0]
    runs.append(d)
assert len(runs)==12 and len(set(r['dll'] for r in runs))==1
lines=['# 非表示アニメ更新タスク削減：1000体追加効果','',
 '同DLL・SightContext再利用/非表示部下骨更新削減/認識グリッドON共通・Leader・高HP・Open・seed1・通常B（grid ON/far OFF、4分割）・1080p。warmup10秒＋60秒。各条件2反復、監査/trace/固定刻みOFF。過去DLLと混合しない。','',
 '|視点|AnimTasks|GT平均の2走行平均（走行範囲）|p95の走行範囲|超過率の走行範囲%|認識平均|視線平均|判断平均|follow平均|RVO平均|',
 '|---|---|---:|---:|---:|---:|---:|---:|---:|---:|']
effects={}
for view in ['RTS','TPS','FPS']:
    means={}
    for reuse in [0,1]:
        rs=[r for r in runs if r['view']==view and r['reuse']==reuse]; assert len(rs)==2
        mean={k:statistics.mean(r['timing'][k]['mean'] for r in rs) for k in keys};means[reuse]=mean
        gt=[r['timing']['gt_ms']['mean'] for r in rs];p95=[r['timing']['gt_ms']['p95'] for r in rs];over=[r['timing']['gt_ms']['over_16_67_pct'] for r in rs]
        lines.append(f"|{view}|{'ON' if reuse else 'OFF'}|{mean['gt_ms']:.3f} ({min(gt):.3f}–{max(gt):.3f})|{min(p95):.3f}–{max(p95):.3f}|{min(over):.1f}–{max(over):.1f}|"+'|'.join(f'{mean[k]:.3f}' for k in ['perception_ms','sight_scan_ms','decision_ms','follow_ms','rvo_ms'])+'|')
    effects[view]={k:{'off':means[0][k],'on':means[1][k],'reduction_ms':means[0][k]-means[1][k],'reduction_pct':100*(1-means[1][k]/means[0][k])} for k in ['gt_ms','perception_ms','sight_scan_ms']}
lines+=['','## 削減量と残存予算','',
 '|視点|GT削減ms (%)|認識の観測差ms (%)|視線の観測差ms (%)|ON GT平均から16.67msまでの差|',
 '|---|---:|---:|---:|---:|']
for view,e in effects.items():
    lines.append('|'+view+'|'+ '|'.join(f"{e[k]['reduction_ms']:.3f} ({e[k]['reduction_pct']:.1f}%)" for k in ['gt_ms','perception_ms','sight_scan_ms'])+f"|{e['gt_ms']['on']-16.67:.3f}|")
lines+=['','アニメ更新タスク削減を追加した比較。認識側は両条件同設定。実時間cadenceや戦況等も変わるため、観測差すべてがアニメタスク単独の費用とは限らない。','',
 'この差は平均GTだけの算術で、p95やGPUを含む60FPS達成に必要な改善量の保証ではない。これ以外の最適化の実削減量は未測定。','',
 '## 各走行と仕事量','',
 '|走行|frames|GT平均|GT p95|16.67ms超過率%|wall平均|RT平均|GPU平均|バッチ期限遅れ最大（warmup含む）|pendingピーク|測定区間shots|',
 '|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|']
for r in runs:
    t=r['timing'];v=[t['gt_ms']['mean'],t['gt_ms']['p95'],t['gt_ms']['over_16_67_pct'],t['wall_ms']['mean'],t['rt_ms']['mean'],t['gpu_ms']['mean'],r['batch_lateness_max_ms_including_warmup']]
    lines.append(f"|{r['folder']}|{r['frames']}|"+'|'.join(f'{x:.3f}' for x in v)+f"|{r['pending_peak']}|{r['shots_interval']}|")
lines+=['','CSVのsensor_lateness_max_msは個別scheduler用で通常Bでは0になるため、遅延ゼロの根拠にしない。上表はログのP3_BATCH_LATENESSを使用（warmupを含む全70秒）。総発見遅延そのものは未測定。','',
 '2反復の範囲は信頼区間ではない。実時間走行ではframe cadence・接敵・共有量が変わるため、GT差の全てをアニメタスク削減単独の時間としない。発見時刻/戦闘状態の等価性は別の同入力監査・固定刻み試験で判定。開発機のみ、基準PC換算未取得。']
lines+=['','## 候補数（warmupを含む全走行）','', '|走行|observer更新数|考慮した敵候補数|候補/observer更新|','|---|---:|---:|---:|']
for r in runs:lines.append(f"|{r['folder']}|{r['observer_updates_including_warmup']}|{r['candidates_including_warmup']}|{r['candidates_per_observer_update']:.3f}|")
lines+=['','候補数は既存PerceptionCandidatesカウンター（Considerへ渡った敵数）。グリッド構築・cell探索・sort自体の仕事量や物理trace数とは異なる。分母は更新したobserver数。監査走行の二重走査は含めない。']
(root/'performance-summary.json').write_text(json.dumps({'dll':runs[0]['dll'],'runs':runs,'effects':effects},indent=2),encoding='utf-8')
(root/'performance-tables.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print(json.dumps(effects,indent=2))
