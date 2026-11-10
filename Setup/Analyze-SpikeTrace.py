"""Export and account only selected neighborhoods in already-recorded CPU traces."""
import csv,json,sys,statistics as st
from collections import Counter,defaultdict
from pathlib import Path
from importlib.machinery import SourceFileLoader
A=SourceFileLoader('spikes',str(Path(__file__).with_name('Analyze-FrameSpikes.py'))).load_module()
RUNS=['AnimTasks-20261110/trace-RTS-0','AnimTasks-20261110/trace-RTS-1','GTBothOn-20261110/RTS-light','GTBothOn-20261110/TPS-light']
def prepare():
    specs=[]
    for label in RUNS:
        p=A.ROOT/'Setup/Benchmarks'/label;rows=A.read(p/'frames.csv');n=len(rows)
        all_ticks=[r for r in A.read(p/'gt-events.csv') if r['TimerName']=='FEngineLoop::Tick']
        ticks=all_ticks[-n-2:-2]
        assert len(ticks)==n
        gt=[float(r['gt_ms']) for r in rows];q=A.quant(gt);high=sorted([i for i,v in enumerate(gt) if v>q],key=lambda i:gt[i])
        selections={'maximum':high[-1],'typical_tail':high[len(high)//2],'queue_record':max(range(1,n),key=lambda i:float(rows[i]['pending_reports_peak'])-float(rows[i-1]['pending_reports_peak']))}
        commands=[];folder=A.OUT/label.replace('/','_');folder.mkdir(exist_ok=True)
        for kind,i in selections.items():
            lo=max(0,i-2);hi=min(n-1,i+2); start=ticks[lo]['StartTime'];end=ticks[hi]['EndTime'];dst=folder/(kind+'.csv')
            commands.append(f'TimingInsights.ExportTimingEvents "{dst.as_posix()}" -threads=GameThread -columns=TimerId,StartTime,EndTime,Depth -startTime={start} -endTime={end}')
            specs.append({'run':label,'kind':kind,'file':str(dst),'index':i,'ticks':[dict(t,index=j,csv_line=j+2,time=rows[j]['time'],gt_ms=rows[j]['gt_ms']) for j,t in enumerate(ticks) if lo<=j<=hi]})
        commands.append(f'TimingInsights.ExportTimingEvents "{(folder/"periodic.csv").as_posix()}" -threads=GameThread -timers=*Garbage*,*Streaming*,*ShaderCompil*,*AsyncLoading* -columns=TimerId,StartTime,EndTime,Depth -startTime={ticks[0]["StartTime"]} -endTime={ticks[-1]["EndTime"]}')
        (folder/'export.rsp').write_text('\n'.join(commands),encoding='utf-8-sig')
    (A.OUT/'trace-selections.json').write_text(json.dumps(specs,indent=2),encoding='utf-8')
def analyze():
    specs=json.loads((A.OUT/'trace-selections.json').read_text());out=[];leaves=[]
    roots={'BattleGameMode':'game_mode','USkinnedMeshComponent_TickComponent':'animation_tick','FParallelAnimationCompletionTask':'animation_completion','USkeletalMeshComponent_CompleteParallelAnimationEvaluation':'animation_completion','UWorld_SendAllEndOfFrameUpdates':'render_updates','UCharacterMovementComponent_TickComponent':'character_movement','HUD Time':'hud','Total Slate Tick Time':'slate'}
    waits={'WaitForTasks','GameThreadWaitForTask','FTaskGraphInterface::WaitUntilTasksComplete','Sleep','FEvent_Wait'}
    for s in specs:
        p=A.ROOT/'Setup/Benchmarks'/s['run']; names={r['Id']:r['Name'] for r in A.read(p/'timers.csv')};events=A.read(Path(s['file']))
        for t in s['ticks']:
            start,end=float(t['StartTime']),float(t['EndTime']);stack=[];groups=Counter();leaf=Counter();incl=Counter();calls=Counter()
            def finish():
                name,depth,dur,child,group=stack.pop();v=dur-child;groups[group]+=v;leaf[(group,name)]+=v;incl[name]+=dur
                if stack:stack[-1][3]+=dur
            for r in events:
                a,b=float(r['StartTime']),float(r['EndTime']);d=int(r['Depth'])
                if b<=start+1e-8 or a>=end-1e-8:continue
                name=names[r['TimerId']]
                calls[name]+=1
                while stack and stack[-1][1]>=d:finish()
                group=stack[-1][4] if stack else 'other'
                if group=='other':group=roots.get(name,'other')
                if name in waits:group='wait'
                stack.append([name,d,min(b,end)-max(a,start),0,group])
            while stack:finish()
            base={'run':s['run'],'kind':s['kind'],'offset':t['index']-s['index'],'csv_line':t['csv_line'],'world_time':t['time'],'trace_start':start,'trace_end':end,'gt_csv':float(t['gt_ms']),'tick_ms':1000*(end-start)}
            out.append(dict(base,**{g:1000*groups[g] for g in ['game_mode','animation_tick','animation_completion','render_updates','character_movement','hud','slate','wait','other']},accounted_ms=sum(groups.values())*1000))
            for (group,name),v in leaf.most_common(30):leaves.append(dict(base,group=group,name=name,self_ms=v*1000,inclusive_ms=incl[name]*1000,calls=calls[name]))
            for name,v in incl.items():
                if any(x in name.lower() for x in ['garbage','collectgarbage','streaming','shadercompil','async loading']):
                    leaves.append(dict(base,group='periodic_probe',name=name,self_ms=sum(vv for (gg,nn),vv in leaf.items() if nn==name)*1000,inclusive_ms=v*1000,calls=calls[name]))
    A.writecsv(A.OUT/'trace-frames.csv',out);A.writecsv(A.OUT/'trace-leaves.csv',leaves)
    periodic=[];periodic_summary=[];alignment=[]
    for label in RUNS:
        p=A.ROOT/'Setup/Benchmarks'/label;rows=A.read(p/'frames.csv');n=len(rows)
        names={r['Id']:r['Name'] for r in A.read(p/'timers.csv')}
        all_ticks=[r for r in A.read(p/'gt-events.csv') if r['TimerName']=='FEngineLoop::Tick']
        ticks=all_ticks[-n-2:-2];old_ticks=all_ticks[-n-1:-1]
        gt=[float(r['gt_ms']) for r in rows];v=[1000*(float(t['EndTime'])-float(t['StartTime'])) for t in old_ticks]
        for k in range(-2,3):
            alignment.append(dict(run=label,trace_offset_from_old_window=k,r=A.corr(gt[max(0,-k):min(n,n-k)],v[max(0,k):min(n,n+k)])))
        groups=defaultdict(list)
        for r in A.read(A.OUT/label.replace('/','_')/'periodic.csv'):
            name=names[r['TimerId']];dur=(float(r['EndTime'])-float(r['StartTime']))*1000;groups[name].append(dur)
            if dur<1:continue
            i=next((i for i,t in enumerate(ticks) if float(t['StartTime'])<=float(r['StartTime'])<float(t['EndTime'])),None)
            periodic.append(dict(run=label,name=name,trace_start=r['StartTime'],ms=dur,csv_line=i+2 if i is not None else None,world_time=rows[i]['time'] if i is not None else None,gt_ms=rows[i]['gt_ms'] if i is not None else None))
        for name,durations in groups.items():periodic_summary.append(dict(run=label,name=name,calls=len(durations),max_ms=max(durations),inclusive_total_ms=sum(durations),over_1ms=sum(x>=1 for x in durations)))
    A.writecsv(A.OUT/'periodic-events.csv',periodic);A.writecsv(A.OUT/'periodic-summary.csv',periodic_summary);A.writecsv(A.OUT/'alignment.csv',alignment)
    print('Accounted',len(out),'selected trace frames')
if __name__=='__main__':prepare() if sys.argv[1]=='prepare' else analyze()
