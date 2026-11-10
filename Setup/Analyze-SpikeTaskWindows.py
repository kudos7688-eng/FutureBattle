"""Join unnamed GraphTask lifecycle to named CPU scopes in selected windows."""
import sys,json,bisect,runpy
from pathlib import Path
from collections import defaultdict
a=runpy.run_path(str(Path(__file__).with_name('Analyze-SpikeRoot.py')));rows,save=a['rows'],a['save']
p=Path(sys.argv[1]);selection=json.loads((p/'selection.json').read_text())['selections']
timers={r['Id']:r['Name'] for r in rows(p/'timers.csv')};threads={r['trace_thread']:int(r['system_thread']) for r in rows(p/'system-threads.csv') if int(r['system_thread'])}
named=defaultdict(set)
for s in selection:
    for r in rows(p/(s['label']+'.csv')):
        name=timers[r['TimerId']]
        if name in ('FParallelAnimationEvaluationTask','FParallelAnimationCompletionTask'):
            named[threads[r['ThreadId']]].add((float(r['StartTime']),float(r['EndTime']),name))
named={tid:sorted(v) for tid,v in named.items()};starts={tid:[x[0] for x in v] for tid,v in named.items()}
windows=sorted((s['start']-.25,s['end']+.25) for s in selection);pending={};output=[]
def relevant(t):return any(x<=t<=y for x,y in windows)
for r in rows(p/'tasks.csv'):
    event=r['event']
    if event not in ('Launched','Scheduled','Started','Finished'):continue
    t=float(r['time']);tid=int(r['system_thread']);task=r['task_id']
    if event=='Launched':
        if relevant(t):pending[task]={'launched':t,'name':r['name']}
    elif task in pending:
        d=pending[task]
        if event=='Scheduled':d['scheduled']=t
        elif event=='Started':d.update(started=t,thread=tid)
        elif event=='Finished':
            pending.pop(task)
            if 'started' not in d or 'scheduled' not in d:continue
            tid=d['thread'];start=d['started']
            if tid not in named:continue
            i=bisect.bisect_left(starts[tid],start-1e-7)
            if i<len(named[tid]):
                x,y,name=named[tid][i]
                if x<=start+.00005 and y<=t+1e-7:
                    output.append({'task':task,'name':name,'thread':tid,'start':start,'end':t,'scope_start':x,'scope_end':y,'start_gap_us':(x-start)*1e6,'queue_ms':(start-d['scheduled'])*1000,'dependency_ms':(d['scheduled']-d['launched'])*1000,'execution_ms':(t-start)*1000})
save(p/'selected-animation-tasks.csv',output)
print(p.name,'named animation tasks',len(output),'scopes',sum(map(len,named.values())))
