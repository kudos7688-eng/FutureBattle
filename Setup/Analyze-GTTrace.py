"""Exclusive, ancestry-aware GT accounting. No game data or settings are changed."""
import csv, json, sys, math
from pathlib import Path
from collections import Counter, defaultdict

def stats(values):
    a=sorted(values)
    return dict(n=len(a),mean=sum(a)/len(a),p95=a[math.ceil(.95*len(a))-1],over_16_67_pct=100*sum(x>16.67 for x in a)/len(a))

def analyze(folder):
    p=Path(folder)
    frames=list(csv.DictReader(open(p/'frames.csv',encoding='utf-8-sig')))
    perf={k:stats([float(r[k]) for r in frames]) for k in frames[0] if k.endswith('_ms')}
    result={'folder':str(p),'performance':perf,'validation':{
        'all_alive':all(int(r['alive'])==1000 for r in frames),
        'all_rvo':all(int(r['rvo_registered'])==1000 for r in frames),
        'views':sorted(set(r['view'] for r in frames)),
        'gpu_present':all(float(r['gpu_ms'])>0 for r in frames)},
        'last_counters':{k:frames[-1][k] for k in ['shots','deaths','paths','path_failures','decisions']}}
    if not (p/'window-events.csv').exists():
        (p/'analysis.json').write_text(json.dumps(result,indent=2),encoding='utf-8'); return result
    w=json.load(open(p/'window.json',encoding='utf-8-sig'))
    names={int(r['Id']):r['Name'] for r in csv.DictReader(open(p/'timers.csv',encoding='utf-8-sig'))}
    roots={'BattleGameMode':'game_mode','USkinnedMeshComponent_TickComponent':'animation_tick',
        'FParallelAnimationCompletionTask':'animation_completion',
        'USkeletalMeshComponent_CompleteParallelAnimationEvaluation':'animation_completion',
        'UWorld_SendAllEndOfFrameUpdates':'render_updates',
        'UCharacterMovementComponent_TickComponent':'character_movement',
        'HUD Time':'hud','Total Slate Tick Time':'slate'}
    waits={'WaitForTasks','GameThreadWaitForTask','FTaskGraphInterface::WaitUntilTasksComplete','Sleep','FEvent_Wait'}
    stack=[]; totals=Counter(); leaf=defaultdict(Counter); inclusive=Counter(); calls=Counter()
    frame_groups=[]; current=None; negative=0; events=0; ticks=0
    def finish():
        nonlocal negative
        node=stack.pop()
        name,depth,duration,children,group=node
        exclusive=duration-children
        if exclusive < -1e-7: negative+=1
        # Export timestamps are rounded. Keep signed tiny self-time errors so
        # parent/child subtraction telescopes; clamping would bias large traces.
        if group:
            totals[group]+=exclusive; leaf[group][name]+=exclusive
            if current is not None: current[group]+=exclusive
        inclusive[name]+=duration; calls[name]+=1
        if stack: stack[-1][3]+=duration
    with open(p/'window-events.csv',encoding='utf-8-sig',newline='') as f:
        for r in csv.DictReader(f):
            start=float(r['StartTime']); end=float(r['EndTime']); depth=int(r['Depth'])
            if end<=w['start']+2e-6 or start>=w['end']-2e-6: continue
            name=names[int(r['TimerId'])]
            while stack and stack[-1][1]>=depth: finish()
            if name=='FEngineLoop::Tick':
                if current is not None: frame_groups.append(dict(current))
                current=Counter(); ticks+=1
            if current is None: continue
            inherited=stack[-1][4] if stack else 'other'
            group=inherited
            if inherited=='other': group=roots.get(name,'other')
            # Explicit wait leaves are kept separate, never classify a task-pumping parent as idle.
            if name in waits: group='wait_scope'
            stack.append([name,depth,min(end,w['end'])-max(start,w['start']),0.0,group])
            events+=1
    while stack: finish()
    if current is not None: frame_groups.append(dict(current))
    n=w['frames']
    result['trace']={'accounting_version':2,'window':w,'events':events,'ticks':ticks,'negative_self_times':negative,
        'groups_ms':{k:v*1000/n for k,v in totals.items()},
        'group_frame_stats':{k:stats([g.get(k,0)*1000 for g in frame_groups]) for k in totals},
        'top_exclusive_ms':{k:[{'name':name,'ms':v*1000/n} for name,v in c.most_common(15)] for k,c in leaf.items()},
        'top_inclusive_ms':[{'name':name,'ms':v*1000/n,'calls_per_frame':calls[name]/n} for name,v in inclusive.most_common(80)]}
    (p/'analysis.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result

if __name__=='__main__':
    for folder in sys.argv[1:]:
        r=analyze(folder)
        print(folder, 'GT',round(r['performance']['gt_ms']['mean'],3),r.get('trace',{}).get('groups_ms',{}),flush=True)
