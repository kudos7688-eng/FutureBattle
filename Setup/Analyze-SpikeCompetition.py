"""Offline OS occupancy, core sharing and selected scope evidence."""
import sys,json,bisect,statistics as st
from pathlib import Path
from collections import defaultdict,Counter
import runpy
api=runpy.run_path(str(Path(__file__).with_name('Analyze-SpikeRoot.py')))
rows,save,q95,corr,Intervals=[api[k] for k in ['rows','save','q95','corr','Intervals']]
p=Path(sys.argv[1]).resolve()
m=json.loads((p/'manifest.json').read_text(encoding='utf-8-sig'));pid=m['pid']
c=next(rows(p/'trace-clock.csv'));base=int(c['start_cycle']);freq=int(c['frequency'])
frames=list(rows(p/'aligned-frames.csv'));starts=[float(r['start']) for r in frames];ends=[float(r['end']) for r in frames]
g=[float(r['gt_ms']) for r in frames];q=q95(g);hi=[i for i,x in enumerate(g) if x>q];lo=[i for i,x in enumerate(g) if x<=q]
threadpid={int(r['tid']):int(r['pid']) for r in rows(p/'os-threads.csv')}
procname={r['Id']:r['ProcessName'] for r in m['background_cpu_before']+m['background_cpu_after']};procname[pid]='UnrealEditor-Cmd (target)';procname[0]='Idle'
gt=json.loads((p/'scheduler-summary.json').read_text())['gt_tid']
worker=set(json.loads((p/'scheduler-summary.json').read_text())['worker_tids'])
gtcpu=defaultdict(list);siblings=defaultdict(list);running={};cpu_continuity_errors=0;state=Counter();occupancy=defaultdict(lambda:[0.]*len(frames));switches=0
topology={int(r['cpu']):(int(r['group']),int(r['core'])) for r in rows(p/'cpu-topology.csv')}
def add(pid,a,b):
    i=max(0,bisect.bisect_right(ends,a))
    while i<len(frames) and starts[i]<b:
        occupancy[pid][i]+=max(0,min(b,ends[i])-max(a,starts[i]))*1000;i+=1
for r in rows(p/'cswitch.csv'):
    t=(int(r['qpc'])-base)/freq;cpu=int(r['cpu']);old=int(r['old_tid']);new=int(r['new_tid']);state[int(r['old_state'])]+=1;switches+=1
    if cpu in running:
        a,tid=running[cpu]
        if tid!=old:cpu_continuity_errors+=1
        if t>starts[0] and a<ends[-1]:
            add(threadpid.get(tid,-1) if tid else 0,a,t)
            if tid==gt:gtcpu[cpu].append((a,t))
            if tid and tid!=gt:siblings[cpu].append((a,t,'worker' if tid in worker else 'game' if threadpid.get(tid)==pid else 'external'))
    running[cpu]=(t,new)
out=[]
for process,v in occupancy.items():
    if process in (0,pid):continue
    rates=[v[i]/((ends[i]-starts[i])*1000) for i in range(len(v))]
    out.append({'pid':process,'name':procname.get(process,'unknown'),'mean_cpu_ms':st.mean(v),'spike_cpu_ms':st.mean(v[i] for i in hi),'normal_cpu_ms':st.mean(v[i] for i in lo),'spike_cores':st.mean(rates[i] for i in hi),'normal_cores':st.mean(rates[i] for i in lo),'r_gt_cores':corr(g,rates)})
save(p/'external-processes.csv',sorted(out,key=lambda r:r['mean_cpu_ms'],reverse=True))
top=sorted(out,key=lambda r:r['mean_cpu_ms'],reverse=True)[:10]
save(p/'external-frame-occupancy.csv',[{'csv_line':i+2,'time':frames[i]['time'],'gt_ms':g[i],**{str(r['pid'])+'_'+r['name']:occupancy[r['pid']][i]/((ends[i]-starts[i])*1000) for r in top}} for i in range(len(frames))])
shared=defaultdict(list)
for cpu,a in gtcpu.items():
    for sib,events in siblings.items():
        if cpu==sib or topology.get(cpu)!=topology.get(sib):continue
        for kind in ['worker','game','external']:
            ix=Intervals([(x,y) for x,y,k in events if k==kind])
            for x,y in a:
                # store actual overlap intervals, preserving frame boundaries
                i=max(0,bisect.bisect_right(ends,x))
                while i<len(frames) and starts[i]<y:
                    shared[kind].append((i,ix.amount(max(x,starts[i]),min(y,ends[i]))*1000));i+=1
columns={k:[0.]*len(frames) for k in ['worker','game','external']}
for k,items in shared.items():
    for i,v in items:columns[k][i]+=v
save(p/'core-sharing-frames.csv',[{'csv_line':i+2,'gt_ms':g[i],**{k+'_sibling_ms':v[i] for k,v in columns.items()}} for i in range(len(frames))])
summary={'cpu_continuity_errors':cpu_continuity_errors,'switches':switches,'states':dict(state),'sibling':{k:{'spike_ms':st.mean(v[i] for i in hi),'normal_ms':st.mean(v[i] for i in lo),'r_gt':corr(g,v)} for k,v in columns.items()}}
interrupt=defaultdict(list)
for r in rows(p/'interrupts.csv'):
    x=(int(r['initial_qpc'])-base)/freq;y=(int(r['end_qpc'])-base)/freq
    if x<=y and y>starts[0] and x<ends[-1]:interrupt[int(r['cpu'])].append((x,y))
iv=[0.]*len(frames)
for cpu,events in interrupt.items():
    merged=[]
    for x,y in sorted(events):
        if merged and x<=merged[-1][1]:merged[-1]=(merged[-1][0],max(y,merged[-1][1]))
        else:merged.append((x,y))
    ix=Intervals(merged)
    for x,y in gtcpu[cpu]:
        i=max(0,bisect.bisect_right(ends,x))
        while i<len(frames) and starts[i]<y:iv[i]+=ix.amount(max(x,starts[i]),min(y,ends[i]))*1000;i+=1
summary['gt_interrupt_overlap_ms']={'max':max(iv),'spike':st.mean(iv[i] for i in hi),'normal':st.mean(iv[i] for i in lo)}
(p/'competition-summary.json').write_text(json.dumps(summary,indent=2));print(p.name,json.dumps(summary))
