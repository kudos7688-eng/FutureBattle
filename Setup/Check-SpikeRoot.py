"""Checks and compact diagnostic tables for the spike investigation."""
import sys,json,runpy,statistics as st,math
from pathlib import Path
from collections import Counter,defaultdict
a=runpy.run_path(str(Path(__file__).with_name('Analyze-SpikeRoot.py')));rows,save,q95=[a[k] for k in ['rows','save','q95']]
p=Path(sys.argv[1]);results=[];phase=[];table=['|走行|frames|GT平均|GT p95|GT最大|RT平均|GPU平均|wall平均|','|---|---:|---:|---:|---:|---:|---:|---:|']
for run in sorted(p.glob('*-r*')):
    if not (run/'frames.csv').exists():continue
    f=list(rows(run/'frames.csv'));g=[float(r['gt_ms']) for r in f];q=q95(g);spikes=[i for i,x in enumerate(g) if x>q]
    table.append('|'+run.name+'|'+str(len(f))+'|'+ '|'.join(f'{x:.3f}' for x in [st.mean(g),q,max(g),st.mean(float(r['rt_ms']) for r in f),st.mean(float(r['gpu_ms']) for r in f),st.mean(float(r['wall_ms']) for r in f)])+'|')
    for period,bins in [(1,10),(.05,5)]:
        total=Counter(int((float(r['time'])%period)/period*bins)%bins for r in f);hit=Counter(int((float(f[i]['time'])%period)/period*bins)%bins for i in spikes)
        for b in range(bins):phase.append({'run':run.name,'period_seconds':period,'bin':b,'frames':total[b],'spikes':hit[b],'rate_pct':100*hit[b]/total[b] if total[b] else None})
    d={'run':run.name,'spikes':len(spikes),'spikes_per60s':len(spikes),'isolated':sum(i-1 not in spikes and i+1 not in spikes for i in spikes),'deaths':int(f[-1]['deaths'])-int(f[0]['deaths'])}
    if (run/'scope-frames.csv').exists():
        ss=list(rows(run/'scope-frames.csv'));err=max(abs(float(r['tick_ms'])-float(r['accounted_ms'])) for r in ss);assert err<1e-6;d['scope_accounting_max_error_ms']=err
        meta=json.loads((run/'etw-metadata.json').read_text());assert meta['events_lost']==meta['buffers_lost']==meta['layout_errors']==0;d['etw']=meta
        comp=json.loads((run/'competition-summary.json').read_text());assert comp['cpu_continuity_errors']==0
        selected=list(rows(run/'selected-animation-tasks.csv'));d['selected_task_matches']=len(selected);d['task_start_gap_max_us']=max(float(r['start_gap_us']) for r in selected)
        names={r['Id']:r['Name'] for r in rows(run/'timers.csv')};mx={};seen=set()
        cpu_threads={r['trace_thread'] for r in rows(run/'system-threads.csv') if int(r['system_thread'])}
        for s in json.loads((run/'selection.json').read_text())['selections']:
            for r in rows(run/(s['label']+'.csv')):
                if r['ThreadId'] not in cpu_threads:continue
                name=names[r['TimerId']]
                if not any(k in name.lower() for k in ['stream','shader','garbage','asyncLoading'.lower()]):continue
                key=(r['ThreadId'],r['TimerId'],r['StartTime'],r['EndTime'])
                if key in seen:continue
                seen.add(key);duration=(float(r['EndTime'])-float(r['StartTime']))*1000
                if name not in mx or duration>mx[name]['duration_ms']:mx[name]={'name':name,'duration_ms':duration,'thread':r['ThreadId'],'start':r['StartTime'],'end':r['EndTime']}
        save(run/'selected-background-scope-max.csv',sorted(mx.values(),key=lambda r:r['duration_ms'],reverse=True))
    results.append(d)
save(p/'period-phase.csv',phase);(p/'measurement-table.md').write_text('\n'.join(table)+'\n',encoding='utf-8');(p/'checks.json').write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=2))
