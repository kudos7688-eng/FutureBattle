"""Offline frame-aligned UE/ETW/task analysis. Does not alter measurements."""
import csv,json,math,statistics as st,sys,bisect
from pathlib import Path
from collections import defaultdict,Counter
def rows(p):
    with open(p,encoding='utf-8-sig',newline='') as f:yield from csv.DictReader(f)
def save(p,r):
    if not r:return
    with open(p,'w',encoding='utf-8',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(r[0]));w.writeheader();w.writerows(r)
def q95(v):return sorted(v)[math.ceil(.95*len(v))-1]
def corr(a,b):return st.correlation(a,b) if len(a)>2 and max(a)>min(a) and max(b)>min(b) else None
def prepare(p):
    r=list(rows(p/'frames.csv'));n=len(r);gt=[float(x['gt_ms']) for x in r];all_ticks=list(rows(p/'ticks.csv'))
    align=[]
    for end in range(1,5):
        t=all_ticks[-n-end:-end]
        if len(t)==n:align.append((corr(gt,[(float(x['EndTime'])-float(x['StartTime']))*1000 for x in t]),end))
    best,end=max(align);assert best>.85,(p,align)
    ticks=all_ticks[-n-end:-end];q=q95(gt);spikes=sorted([i for i in range(n) if gt[i]>q],key=lambda i:gt[i],reverse=True)
    chosen=[]
    for i in spikes:
        if all(abs(i-j)>8 for j in chosen):chosen.append(i)
        if len(chosen)==5:break
    for k in [.25,.5,.75]:
        i=spikes[int(k*(len(spikes)-1))]
        if all(abs(i-j)>3 for j in chosen):chosen.append(i)
    selections=[]
    for number,i in enumerate(chosen):
        active=float(r[i]['sight_scan_ms'])>0
        candidates=[j for j in range(max(1,i-50),min(n-1,i+51)) if gt[j]<=q and (float(r[j]['sight_scan_ms'])>0)==active and all(abs(j-k)>2 for k in chosen)]
        j=min(candidates,key=lambda j:abs(float(r[j]['time'])-float(r[i]['time']))) if candidates else min(range(n),key=lambda j:abs(gt[j]-st.median(gt)))
        for kind,k in [('spike',i),('matched',j)]:
            selections.append({'label':f'{number:02}-{kind}','pair':number,'kind':kind,'index':k,'csv_line':k+2,'gt_ms':gt[k],'time':float(r[k]['time']),'start':float(ticks[max(0,k-1)]['StartTime']),'end':float(ticks[min(n-1,k+1)]['EndTime'])})
    commands=[]
    for s in selections:commands.append(f'TimingInsights.ExportTimingEvents "{(p/(s["label"]+".csv")).as_posix()}" -columns=ThreadId,TimerId,StartTime,EndTime,Depth -startTime={s["start"]:.9f} -endTime={s["end"]:.9f}')
    (p/'windows-export.rsp').write_text('\n'.join(commands),encoding='utf-8-sig')
    (p/'selection.json').write_text(json.dumps({'alignment':align,'selected_tail_exclusion':end,'alignment_r':best,'p95':q,'selections':selections},indent=2),encoding='utf-8')
    save(p/'aligned-frames.csv',[{'index':i,'csv_line':i+2,'start':t['StartTime'],'end':t['EndTime'],**r[i]} for i,t in enumerate(ticks)])
    print(p.name,'alignment',best,'tail exclusion',end,'windows',len(selections))
class Intervals:
    def __init__(self,a):
        a.sort();self.lo=[x[0] for x in a];self.hi=[x[1] for x in a];self.prefix=[0.]
        for l,h in a:self.prefix.append(self.prefix[-1]+h-l)
    def before(self,t):
        i=bisect.bisect_right(self.hi,t);v=self.prefix[i]
        if i<len(self.lo) and self.lo[i]<t:v+=t-self.lo[i]
        return v
    def amount(self,a,b):return max(0.,self.before(b)-self.before(a))
def scheduler(p):
    clock=next(rows(p/'trace-clock.csv'));base=int(clock['start_cycle']);freq=int(clock['frequency'])
    meta=json.loads((p/'etw-metadata.json').read_text());assert meta['frequency']==freq and meta['layout_errors']==0
    assert meta['status']==0 and meta['events_lost']==0 and meta['buffers_lost']==0,meta
    threads={int(x['trace_thread']):x for x in rows(p/'system-threads.csv') if int(x['system_thread'])>0 and x['name']!='(null)'}
    names={int(x['system_thread']):x['name'] for x in threads.values()};gt=next(t for t,n in names.items() if n=='GameThread')
    workers={t for t,n in names.items() if 'Worker' in n or 'TaskGraph' in n}
    topology={int(x['cpu']):int(x['efficiency_class']) for x in rows(p/'cpu-topology.csv')}
    manifest=json.loads((p/'manifest.json').read_text(encoding='utf-8-sig'));pid=int(manifest['pid'])
    all_game_tids={int(x['tid']) for x in rows(p/'os-threads.csv') if int(x['pid'])==pid}
    readies=defaultdict(list)
    for x in rows(p/'ready.csv'):readies[int(x['tid'])].append((int(x['qpc'])-base)/freq)
    for a in readies.values():a.sort()
    intervals=defaultdict(lambda:defaultdict(list));running={};off={};state_counts=Counter();switch_count=Counter();cpu_running={};cpu_intervals=defaultdict(lambda:defaultdict(list))
    for x in rows(p/'cswitch.csv'):
        t=(int(x['qpc'])-base)/freq;new=int(x['new_tid']);old=int(x['old_tid']);cpu=int(x['cpu'])
        if cpu in cpu_running:
            start,tid=cpu_running[cpu]
            if t>=start:
                kind='idle' if tid==0 else 'game' if tid in all_game_tids else 'external'
                cpu_intervals[cpu][kind].append((start,t))
        cpu_running[cpu]=(t,new)
        if old in names:
            if old in running:
                start,run_cpu=running.pop(old)
                if t>=start:
                    intervals[old]['running'].append((start,t));intervals[old]['performance_cpu' if topology.get(run_cpu)==1 else 'efficiency_cpu'].append((start,t))
            off[old]=(t,int(x['old_state']),int(x['old_wait_reason']));state_counts[int(x['old_state'])]+=1;switch_count[old]+=1
        if new in names:
            running[new]=(t,cpu)
            if new in off:
                start,state,reason=off.pop(new)
                if t<start:continue
                if state in (1,3,7):intervals[new]['ready'].append((start,t))
                else:
                    a=readies[new];j=bisect.bisect_left(a,start-1e-9)
                    if j<len(a) and a[j]<=t+1e-9:
                        split=min(t,max(start,a[j]));intervals[new]['blocked'].append((start,split));intervals[new]['ready'].append((split,t))
                    else:intervals[new]['unknown_offcpu'].append((start,t))
    indexed={tid:{k:Intervals(a) for k,a in states.items()} for tid,states in intervals.items()}
    cpu_indexed={cpu:{k:Intervals(a) for k,a in states.items()} for cpu,states in cpu_intervals.items()}
    save(p/'gt-os-intervals.csv',[{'kind':k,'start':a,'end':b} for k,v in intervals[gt].items() for a,b in v])
    def amount(tid,k,a,b):return indexed[tid][k].amount(a,b)*1000 if tid in indexed and k in indexed[tid] else 0.
    result=[]
    for r in rows(p/'aligned-frames.csv'):
        a,b=float(r['start']),float(r['end']);d={'index':int(r['index']),'csv_line':int(r['csv_line']),'time':float(r['time']),'gt_ms':float(r['gt_ms']),'tick_ms':(b-a)*1000}
        for k in ['running','ready','blocked','unknown_offcpu','performance_cpu','efficiency_cpu']:
            d['gt_'+k+'_ms']=amount(gt,k,a,b);d['workers_'+k+'_cpu_ms']=sum(amount(t,k,a,b) for t in workers)
        for k in ['idle','game','external']:
            d['system_'+k+'_cpu_ms']=sum(states[k].amount(a,b)*1000 for states in cpu_indexed.values() if k in states)
        d['gt_accounted_ms']=sum(d['gt_'+k+'_ms'] for k in ['running','ready','blocked','unknown_offcpu']);result.append(d)
    save(p/'scheduler-frames.csv',result)
    q=q95([d['gt_ms'] for d in result]);spike=[d for d in result if d['gt_ms']>q];normal=[d for d in result if d['gt_ms']<=q]
    summary={'gt_tid':gt,'worker_tids':sorted(workers),'old_states':dict(state_counts),'gt_switches':switch_count[gt], 'max_accounting_error_ms':max(abs(d['tick_ms']-d['gt_accounted_ms']) for d in result),'metrics':{k:{'all_mean':st.mean(d[k] for d in result),'spike_mean':st.mean(d[k] for d in spike),'normal_mean':st.mean(d[k] for d in normal),'r_gt':corr([d['gt_ms'] for d in result],[d[k] for d in result])} for k in result[0] if k.endswith('_ms')}}
    (p/'scheduler-summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8');print(p.name,'GT thread',gt,'max accounting error ms',summary['max_accounting_error_ms'])
def tasks(p):
    frames=list(rows(p/'aligned-frames.csv'));starts=[float(r['start']) for r in frames];end=float(frames[-1]['end']);pending={};names=Counter()
    agg=[defaultdict(list) for _ in frames];counts=[Counter() for _ in frames];events=Counter();missing=Counter();sample=[]
    for r in rows(p/'tasks.csv'):
        event=r['event'];events[event]+=1
        if event.startswith('Waiting'):continue
        task=r['task_id'];t=float(r['time'])
        if event=='Launched':pending[task]={'launched':t,'name':r['name'],'target':int(r['thread_to_execute'])}
        elif event=='Scheduled':pending.setdefault(task,{})['scheduled']=t
        elif event=='Started':pending.setdefault(task,{}).update(started=t,thread=int(r['system_thread']))
        elif event=='Finished':
            d=pending.pop(task,{})
            if 'started' not in d:missing['without_started']+=1;continue
            start=d['started'];i=bisect.bisect_right(starts,start)-1
            if i<0 or start>=end:continue
            name=d.get('name','');names[name]+=1
            category='anim_completion' if 'AnimationCompletion' in name else 'anim_worker' if 'Animation' in name else 'other'
            queue=(start-d['scheduled'])*1000 if 'scheduled' in d else None
            dependency=(d['scheduled']-d['launched'])*1000 if 'scheduled' in d and 'launched' in d else None
            execution=(t-start)*1000
            for group in ['all',category]:
                counts[i][group]+=1
                if queue is not None:agg[i][group+'_queue'].append(queue)
                if dependency is not None:agg[i][group+'_dependency'].append(dependency)
                agg[i][group+'_execution'].append(execution)
            if queue is not None and queue>1:sample.append({'task':task,'name':name,'csv_line':i+2,'start':start,'end':t,'queue_ms':queue,'dependency_ms':dependency,'thread':d.get('thread'),'target':d.get('target')})
        elif event=='Destroyed':pending.pop(task,None)
    out=[]
    for i,r in enumerate(frames):
        d={'index':i,'csv_line':i+2,'time':r['time'],'gt_ms':r['gt_ms']}
        # GraphTask names do not identify animation in this UE build. Use the
        # separate CPU-scope join for animation; never report unknown as zero.
        for group in ['all','other']:
            d[group+'_count']=counts[i][group]
            for metric in ['queue','dependency','execution']:
                v=agg[i][group+'_'+metric]
                for key,value in [('mean',st.mean(v) if v else 0),('p95',q95(v) if v else 0),('max',max(v) if v else 0),('sum',sum(v))]:d[f'{group}_{metric}_{key}_ms']=value
        out.append(d)
    save(p/'task-frames.csv',out);save(p/'task-queue-over1ms.csv',sample)
    (p/'task-summary.json').write_text(json.dumps({'events':events,'missing':missing,'names':names,'unclosed_after_eof':len(pending),'note':'Durations assigned to frame of task start, not exclusive CPU time. Queue=Scheduled to Started; dependency=Launched to Scheduled.'},indent=2),encoding='utf-8');print(p.name,'tasks in measured frames',sum(names.values()))
def scopes(p):
    frames=list(rows(p/'aligned-frames.csv'));selections=json.loads((p/'selection.json').read_text())['selections'];names={r['Id']:r['Name'] for r in rows(p/'timers.csv')};threadmap={r['Id']:r['Name'] for r in rows(p/'threads.csv')};gtid=next(k for k,v in threadmap.items() if v=='GameThread')
    osgroups=defaultdict(list)
    for r in rows(p/'gt-os-intervals.csv'):osgroups[r['kind']].append((float(r['start']),float(r['end'])))
    osindex={k:Intervals(a) for k,a in osgroups.items()}
    roots={'BattleGameMode':'game_mode','USkinnedMeshComponent_TickComponent':'animation_tick','FParallelAnimationCompletionTask':'animation_completion','USkeletalMeshComponent_CompleteParallelAnimationEvaluation':'animation_completion','UWorld_SendAllEndOfFrameUpdates':'render_updates','UCharacterMovementComponent_TickComponent':'character_movement','HUD Time':'hud','Total Slate Tick Time':'slate'}
    waits={'WaitForTasks','GameThreadWaitForTask','FTaskGraphInterface::WaitUntilTasksComplete','Sleep','FEvent_Wait'}
    output=[];leafout=[]
    for s in selections:
        events=defaultdict(list)
        for r in rows(p/(s['label']+'.csv')):events[r['ThreadId']].append(r)
        for i in range(max(0,s['index']-1),min(len(frames),s['index']+2)):
            f=frames[i];a,b=float(f['start']),float(f['end']);stack=[];groups=Counter();leaf=Counter();calls=Counter();osself=defaultdict(Counter)
            def finish():
                name,depth,start,stop,children,group=stack.pop();duration=stop-start;exclusive=duration-sum(y-x for x,y in children);groups[group]+=exclusive;leaf[(group,name)]+=exclusive
                for k in ['running','ready','blocked','unknown_offcpu','performance_cpu','efficiency_cpu']:
                    if k in osindex:osself[(group,name)][k]+=osindex[k].amount(start,stop)-sum(osindex[k].amount(x,y) for x,y in children)
                if stack:stack[-1][4].append((start,stop))
            for r in events[gtid]:
                start,stop=float(r['StartTime']),float(r['EndTime']);depth=int(r['Depth'])
                if stop<=a+1e-9 or start>=b-1e-9:continue
                while stack and stack[-1][1]>=depth:finish()
                name=names[r['TimerId']];group=stack[-1][5] if stack else 'other'
                if group=='other':group=roots.get(name,'other')
                if name in waits:group='wait'
                calls[name]+=1;stack.append([name,depth,max(a,start),min(b,stop),[],group])
            while stack:finish()
            common={'label':s['label'],'pair':s['pair'],'kind':s['kind'],'offset':i-s['index'],'csv_line':i+2,'time':f['time'],'gt_ms':f['gt_ms'],'tick_ms':(b-a)*1000}
            output.append(dict(common,**{k:groups[k]*1000 for k in ['game_mode','animation_tick','animation_completion','render_updates','character_movement','hud','slate','wait','other']},accounted_ms=sum(groups.values())*1000))
            for (group,name),v in leaf.items():
                leafout.append(dict(common,group=group,name=name,self_ms=v*1000,calls=calls[name],**{k+'_ms':osself[(group,name)][k]*1000 for k in ['running','ready','blocked','unknown_offcpu','performance_cpu','efficiency_cpu']}))
    save(p/'scope-frames.csv',output);save(p/'scope-leaves.csv',leafout);print(p.name,'scope frames',len(output))
def summarize(p):
    result=[]
    for path in sorted(p.glob('*/frames.csv')):
        run=path.parent;m=json.loads((run/'manifest.json').read_text(encoding='utf-8-sig'));r=list(rows(path));n=len(r)
        assert m['status']=='passed' and m['machine']
        assert all(int(x['alive'])==1000 and int(x['rvo_registered'])==1000 and x['view']=='RTS' and float(x['gpu_ms'])>0 for x in r)
        metrics={}
        for k in r[0]:
            if not k.endswith('_ms'):continue
            v=[float(x[k]) for x in r];metrics[k]={'mean':st.mean(v),'p95':q95(v),'max':max(v),'over16_67_pct':100*sum(x>16.67 for x in v)/n}
        d={'run':run.name,'dll':m['dll_sha256'],'trace':m['cpu_trace'],'frames':n,'metrics':metrics,'shots':int(r[-1]['shots'])-int(r[0]['shots']),'deaths':int(r[-1]['deaths'])-int(r[0]['deaths'])}
        if (run/'scheduler-summary.json').exists():d['scheduler']=json.loads((run/'scheduler-summary.json').read_text())
        if (run/'task-frames.csv').exists():
            ts=list(rows(run/'task-frames.csv'));g=[float(x['gt_ms']) for x in ts];q=q95(g);hi=[i for i,x in enumerate(g) if x>q];lo=[i for i,x in enumerate(g) if x<=q]
            d['task_metrics']={k:{'spike_mean':st.mean(float(ts[i][k]) for i in hi),'normal_mean':st.mean(float(ts[i][k]) for i in lo),'r_gt':corr(g,[float(x[k]) for x in ts])} for k in ts[0] if k.endswith('_ms') or k.endswith('_count')}
        result.append(d)
    (p/'summary.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps([{'run':x['run'],'gt':x['metrics']['gt_ms']} for x in result],indent=2))
if __name__=='__main__':
    mode,p=sys.argv[1],Path(sys.argv[2]).resolve()
    {'prepare':prepare,'scheduler':scheduler,'tasks':tasks,'scopes':scopes,'summarize':summarize}[mode](p)
