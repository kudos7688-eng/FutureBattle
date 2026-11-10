"""Report each trace/control cohort separately; no historical measurements merged."""
import json,csv,sys,importlib.util
from pathlib import Path

spec=importlib.util.spec_from_file_location('gt_analysis',Path(__file__).with_name('Analyze-GTTrace.py'))
mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
root=Path(sys.argv[1])
views=['RTS','TPS','FPS']
known=['perception_ms','decision_ms','path_ms','follow_ms','rvo_ms','shot_ms','display_ms']
results={}
for view in views:
    for kind in ['control','trace','light']:
        p=root/f'{view}-{kind}'
        if not (p/'analysis.json').exists(): mod.analyze(p)
        results[f'{view}-{kind}']=json.load(open(p/'analysis.json',encoding='utf-8'))
def mean(view,kind,key): return results[f'{view}-{kind}']['performance'][key]['mean']
lines=['# GTBreakdown-20261110 集計表','',
 '1000体・Leader・高HP・Open・seed1・通常B、warmup10秒＋60秒。各条件1走行。単位ms/frame。過去のPhase3と混合しない。','',
 '## トレースなし対照','',
 '|視点|GT平均|GT p95|16.67ms超過率%|wall平均|RT平均|GPU平均|GPU p95|認識|follow|RVO|概算残差|',
 '|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|']
for v in views:
    p=results[f'{v}-control']['performance'];g=p['gt_ms'];res=g['mean']-sum(p[k]['mean'] for k in known)
    values=[g['mean'],g['p95'],g['over_16_67_pct'],p['wall_ms']['mean'],p['rt_ms']['mean'],p['gpu_ms']['mean'],p['gpu_ms']['p95'],p['perception_ms']['mean'],p['follow_ms']['mean'],p['rvo_ms']['mean'],res]
    lines.append('|'+v+'|'+'|'.join(f'{x:.3f}' for x in values)+'|')
lines+=['','## 計測負荷と走行変動の確認','',
 '|視点|対照GT|軽量GT|軽量/対照差%|詳細GT|詳細/対照差%|','|---|---:|---:|---:|---:|---:|']
for v in views:
    c,l,d=[mean(v,k,'gt_ms') for k in ['control','light','trace']]
    lines.append(f'|{v}|{c:.3f}|{l:.3f}|{100*(l/c-1):.1f}|{d:.3f}|{100*(d/c-1):.1f}|')
lines+=['','この差は計測負荷と独立走行の軌跡・接敵差を含み、計測器単独の因果的overheadとはしない。','',
 '## 軽量トレースの内訳','',
 '既存scope外の概算残差を、同走行のエンジン区間で分類。親子の時間を重複計上しない。scope境界・frame対応差は残項に含む。待機scopeは依存待機を含み、GTから全額除外したり削減可能額に数えたりしない。','',
 '|視点|GT|既存7scope合計|概算残差|アニメTick|アニメ反映|描画データ更新|CharacterMovement|タスク待ちscope|その他・境界差|',
 '|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|']
budgets={}
for v in views:
    r=results[f'{v}-light'];p=r['performance'];t=r['trace'];g=t['groups_ms']
    custom=sum(p[k]['mean'] for k in known);res=p['gt_ms']['mean']-custom
    cats=[g.get(k,0) for k in ['animation_tick','animation_completion','render_updates','character_movement','wait_scope']]
    values=[p['gt_ms']['mean'],custom,res,*cats,res-sum(cats)]
    lines.append('|'+v+'|'+'|'.join(f'{x:.3f}' for x in values)+'|')
    visual=sum(cats[:3]);after=p['gt_ms']['mean']-.9*p['perception_ms']['mean']
    budgets[v]={'after_perception_90pct_ms':after,'additional_to_16_67_ms':after-16.67,
        'animation_and_render_domain_ms':visual,'after_perception90_and_visual50_ms':after-.5*visual,
        'trace_ticks':t['ticks'],'csv_frames':t['window']['frames'],
        'partition_ms':sum(g.values()),'engine_ticks_ms':next(x['ms'] for x in t['top_inclusive_ms'] if x['name']=='FEngineLoop::Tick')}
lines+=['','## 60FPSへの条件付き算術（最適化結果ではない）','',
 '|視点|認識90%削減仮定後|さらに必要な削減|アニメ＋描画更新の費用全体|認識90%＋アニメ/描画50%削減仮定後|',
 '|---|---:|---:|---:|---:|']
for v,b in budgets.items():
    lines.append('|'+v+'|'+'|'.join(f'{b[k]:.3f}' for k in ['after_perception_90pct_ms','additional_to_16_67_ms','animation_and_render_domain_ms','after_perception90_and_visual50_ms'])+'|')
lines+=['','同じ軽量走行の費用だけで計算。各削減率は未測定。独立走行の改善量や他作品の数字を加算していない。アニメ/描画費用は削減可能額そのものではない。最適化後p95・RT/GPUは未測定。開発機のみ、基準PC換算未取得。']
(root/'tables.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
(root/'summary.json').write_text(json.dumps({'runs':results,'budgets':budgets},ensure_ascii=False,indent=2),encoding='utf-8')
print('\n'.join(lines))
