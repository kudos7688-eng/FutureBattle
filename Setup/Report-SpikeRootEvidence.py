"""Produce reviewable evidence tables without modifying raw recordings."""
import sys,json,runpy,statistics as st,math
from pathlib import Path
from collections import defaultdict,Counter
a=runpy.run_path(str(Path(__file__).with_name('Analyze-SpikeRoot.py')));rows,save,q95,corr=[a[k] for k in ['rows','save','q95','corr']]
p=Path(sys.argv[1]);allsummary=[]
for run in sorted(p.glob('trace-r*')):
    f=list(rows(run/'aligned-frames.csv'));os=list(rows(run/'scheduler-frames.csv'));n=len(f);g=[float(x['gt_ms']) for x in f];threshold=q95(g);hi=[i for i,v in enumerate(g) if v>threshold];lo=[i for i,v in enumerate(g) if v<=threshold]
    gc=[(float(r['StartTime']),float(r['EndTime'])) for r in rows(run/'gc-events.csv') if float(r['EndTime'])-float(r['StartTime'])>.001]
    isgc=[any(float(r['start'])<y and float(r['end'])>x for x,y in gc) for r in f]
    e=[float(r['gt_efficiency_cpu_ms']) for r in os];ready=[float(r['gt_ready_ms']) for r in os]
    classes=Counter('GC' if isgc[i] else 'E-core >=1ms' if e[i]>=1 else 'ready >=1ms' if ready[i]>=1 else 'P-dominant' for i in hi)
    spans=[];start=None
    for i in range(n+1):
        if i<n and e[i]>=1:
            if start is None:start=i
        elif start is not None:
            spans.append({'start_csv_line':start+2,'end_csv_line':i+1,'start_time':f[start]['time'],'end_time':f[i-1]['time'],'frames':i-start,'spikes':sum(g[j]>threshold for j in range(start,i))});start=None
    save(run/'e-core-episodes.csv',spans)
    counts={'p95':threshold,'frames':n,'spikes':len(hi),'isolated':sum((i-1 not in hi) and (i+1 not in hi) for i in hi),'spikes_before_first_shot':sum(int(f[i]['shots'])==0 for i in hi),'classifications':dict(classes),'gc_events':gc,'e_frames':sum(x>=1 for x in e),'e_spikes':sum(e[i]>=1 for i in hi)}
    metrics={}
    for k in ['gt_ms','wall_ms','perception_ms','decision_ms','follow_ms','rvo_ms','shots','sight_scan_ms','pending_reports_peak']:
        if k not in f[0]:continue
        v=[float(r[k]) for r in f]
        if k in ('shots','pending_reports_peak'):v=[0]+[v[i]-v[i-1] for i in range(1,n)]
        metrics[k]={'spike':st.mean(v[i] for i in hi),'normal':st.mean(v[i] for i in lo),'r_gt':corr(g,v)}
    summary={'run':run.name,**counts,'csv_metrics':metrics}
    if (run/'task-frames.csv').exists():
        tf=list(rows(run/'task-frames.csv'))
        summary['all_tasks']={k:{'spike':st.mean(float(tf[i][k]) for i in hi),'normal':st.mean(float(tf[i][k]) for i in lo),'r_gt':corr(g,[float(r[k]) for r in tf])} for k in ['all_count','all_queue_mean_ms','all_queue_p95_ms','all_execution_sum_ms']}
    scope=[r for r in rows(run/'scope-frames.csv') if int(r['offset'])==0]
    pairs=defaultdict(dict)
    for r in scope:pairs[r['pair']][r['kind']]=r
    paired=[]
    leaves=list(rows(run/'scope-leaves.csv'))
    groupos=defaultdict(Counter)
    for r in leaves:
        if int(r['offset'])!=0:continue
        key=(r['pair'],r['kind'],r['csv_line'],r['group'])
        for k in ['self_ms','running_ms','ready_ms','blocked_ms','efficiency_cpu_ms']:groupos[key][k]+=float(r[k])
    save(run/'scope-os-evidence.csv',[dict(zip(['pair','kind','csv_line','group'],k),**v) for k,v in groupos.items()])
    for pair,parts in pairs.items():
        s,b=parts['spike'],parts['matched'];i=int(s['csv_line'])-2
        d={'pair':pair,'spike_csv_line':i+2,'normal_csv_line':b['csv_line'],'spike_time':s['time'],'gc':isgc[i],'gt_spike':float(s['gt_ms']),'gt_normal':float(b['gt_ms']),'e_core_ms':e[i]}
        for key in ['tick_ms','game_mode','animation_tick','animation_completion','render_updates','character_movement','wait','other']:d[key+'_delta']=float(s[key])-float(b[key])
        for key in ['USkinnedMeshComponent_TickComponent','FParallelAnimationCompletionTask','UCharacterMovementComponent_TickComponent','MoveComponent']:
            for kind in ['spike','matched']:
                v=[r for r in leaves if r['pair']==pair and r['kind']==kind and int(r['offset'])==0 and r['name']==key];d[key+'_'+kind+'_calls']=sum(int(r['calls']) for r in v)
        paired.append(d)
    save(run/'paired-evidence.csv',paired)
    selected=[]
    if (run/'selected-animation-tasks.csv').exists():
        ts=list(rows(run/'selected-animation-tasks.csv'))
        for s in scope:
            i=int(s['csv_line'])-2;start,end=float(f[i]['start']),float(f[i]['end'])
            for name in ['FParallelAnimationEvaluationTask','FParallelAnimationCompletionTask']:
                v=[r for r in ts if r['name']==name and start<=float(r['start'])<end]
                if v:selected.append({'pair':s['pair'],'kind':s['kind'],'csv_line':i+2,'name':name,'count':len(v),'queue_mean_ms':st.mean(float(r['queue_ms']) for r in v),'queue_p95_ms':q95([float(r['queue_ms']) for r in v]),'queue_max_ms':max(float(r['queue_ms']) for r in v),'execution_sum_ms':sum(float(r['execution_ms']) for r in v)})
        save(run/'selected-task-evidence.csv',selected)
    save(run/'spike-classification.csv',[{'csv_line':i+2,'time':f[i]['time'],'gt_ms':g[i],'class':'GC' if isgc[i] else 'E-core >=1ms' if e[i]>=1 else 'ready >=1ms' if ready[i]>=1 else 'P-dominant','gt_running_ms':os[i]['gt_running_ms'],'gt_ready_ms':ready[i],'gt_e_ms':e[i]} for i in hi])
    allsummary.append(summary)
(p/'evidence.json').write_text(json.dumps(allsummary,indent=2));print(json.dumps(allsummary,indent=2))
