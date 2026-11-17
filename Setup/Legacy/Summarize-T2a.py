"""T2a paired seed bootstrap (20,000 draws), no frame/shot pseudoreplication."""
import argparse, csv, importlib.util, json, math, random, statistics
from pathlib import Path

HERE=Path(__file__).resolve().parent
def module(name, file):
    spec=importlib.util.spec_from_file_location(name,HERE/file);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
old=module('field','Summarize-FieldDiagnostics.py')
reg=module('reg','Check-T2aRegression.py')

def csvout(path, rows):
    with path.open('w',encoding='utf-8-sig',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)

def quantile(values,q):
    return sorted(values)[max(0,math.ceil(q*len(values))-1)] if values else 0.

def analyze(folder,expected):
    reg.check(folder,expected*2)
    data={};flat=[];events=[];hist=[]
    arms=['Individual','Off','Cap1','Cap2','Expected']
    for seed in range(1,expected+1):
        for arm in arms:
            name=f'seed{seed}-'+('Individual-Off' if arm=='Individual' else f'Squad-{arm}')
            row=reg.rows(folder/(name+'.log'))[-1]
            diag=json.loads((folder/(name+'-diagnostics.json')).read_text(encoding='utf-8-sig'))
            extra=json.loads((folder/(name+'-t2a.json')).read_text(encoding='utf-8-sig'))
            assert row['success']=='1' and row['failures']=='0' and extra['combat_alive_frames']==900
            assert extra['eligible_attempts']==extra['allocated_shared_shots']+extra['deferred_attempts']
            assert diag['shots']==sum(x['shots'] for x in diag['shot_bins'])==int(row['shots'])
            assert diag['raw_hits']==sum(x['raw_hits'] for x in diag['shot_bins'])
            assert diag['applied_hits']==sum(x['applied_hits'] for x in diag['shot_bins'])==int(row['hits'])
            d=old.derived(row,diag)
            d['support_shot_percent']=100*sum(b['shots'] for b in diag['shot_bins'] if b['weapon']=='support')/int(row['shots'])
            d['shared_fov_reject_percent']=100*diag['shared_reject_fov']/diag['shared_checks'] if diag['shared_checks'] else 0.
            d['shots_per_match_second']=int(row['shots'])/float(row['time'])
            d['applied_hits_per_match_second']=int(row['hits'])/float(row['time'])
            # NullRHI performance counters are intentionally not compared.
            d={k:v for k,v in d.items() if 'fps' not in k and '_ms' not in k}
            delays=[x['delay_seconds'] for x in extra['wait_episodes'] if x['outcome']=='fired']
            d.update(combat_mean_alive=extra['combat_mean_alive'],deferred_attempts=extra['deferred_attempts'],
                     shared_allocated=extra['allocated_shared_shots'],wait_episodes=len(extra['wait_episodes']),
                     wait_fired=len(delays),wait_censored=len(extra['wait_episodes'])-len(delays),
                     fired_delay_total_s=sum(delays),fired_delay_mean_s=statistics.mean(delays) if delays else 0.,fired_delay_p50_s=quantile(delays,.5),
                     fired_delay_p95_s=quantile(delays,.95),fired_delay_max_s=max(delays,default=0.),
                     team0_win=int(row['winner'])==0,team1_win=int(row['winner'])==1,draw=int(row['winner'])==-1,
                     shared_reject_fov=diag['shared_reject_fov'],shared_checks=diag['shared_checks'])
            data[seed,arm]=(d,diag,extra);flat.append(dict(seed=seed,arm=arm,**d))
            events.extend(dict(seed=seed,arm=arm,**x) for x in extra['wait_episodes'])
            for label,lo,hi in [('0-.2',0,.200001),('.2-.4',.200001,.400001),('.4-1',.400001,1.000001),('1-2',1.000001,2.000001),('2-5',2.000001,5.000001),('>5',5.000001,float('inf'))]:
                hist.append(dict(seed=seed,arm=arm,delay_bin_s=label,count=sum(lo<=x<hi for x in delays)))
    csvout(folder/'seed-metrics.csv',flat)
    if events: csvout(folder/'wait-episodes.csv',events)
    csvout(folder/'delay-histogram.csv',hist)
    means=[dict(arm=arm,**{k:statistics.mean(data[s,arm][0][k] for s in range(1,expected+1)) for k in data[1,arm][0]}) for arm in arms]
    csvout(folder/'condition-means.csv',means)
    rng=random.Random(20261106); indices=[[rng.randrange(expected) for _ in range(expected)] for _ in range(20000)]
    contrasts=[];differences=[]
    for arm in arms[1:]:
        for reference in ['Individual','Off']:
            if arm==reference: continue
            for metric in data[1,arm][0]:
                # A conditional delay distribution is undefined for OFF (no held shots).
                # Compare total delayed seconds and episode counts, not an invented zero conditional mean.
                if metric in {'fired_delay_mean_s','fired_delay_p50_s','fired_delay_p95_s','fired_delay_max_s'}: continue
                a=[data[s,reference][0][metric] for s in range(1,expected+1)];b=[data[s,arm][0][metric] for s in range(1,expected+1)]
                out=old.paired_stats(a,b,indices)
                contrasts.append(dict(arm=arm,reference=reference,metric=metric,reference_mean=out['individual_mean'],arm_mean=out['squad_mean'],difference=out['paired_difference'],ci_low=out['ci95'][0],ci_high=out['ci95'][1]))
                differences.extend(dict(seed=s,arm=arm,reference=reference,metric=metric,difference=b[s-1]-a[s-1]) for s in range(1,expected+1))
            # Per-seed relative early-loss difference, in percent.
            relative=[100*(data[s,arm][0]['losses_30s']/data[s,reference][0]['losses_30s']-1) for s in range(1,expected+1)]
            out=old.paired_stats([0.]*expected,relative,indices)
            contrasts.append(dict(arm=arm,reference=reference,metric='losses_30s_relative_percent',reference_mean=0.,arm_mean=out['squad_mean'],difference=out['paired_difference'],ci_low=out['ci95'][0],ci_high=out['ci95'][1]))
    csvout(folder/'contrasts.csv',contrasts);csvout(folder/'paired-differences.csv',differences)
    pooled={}
    for arm in arms:
        ee=[e for e in events if e['arm']==arm]; ds=[e['delay_seconds'] for e in ee if e['outcome']=='fired']
        pooled[arm]={'wait_outcomes':{reason:sum(e['outcome']==reason for e in ee) for reason in ['fired','target_changed','shooter_dead','match_end']},'fired_delay_mean_s':statistics.mean(ds) if ds else 0.,'fired_delay_p50_s':quantile(ds,.5),'fired_delay_p95_s':quantile(ds,.95),'fired_delay_max_s':max(ds,default=0.),'winner_counts':{k:sum(data[s,arm][0][k] for s in range(1,expected+1)) for k in ['team0_win','team1_win','draw']},'winner_changed_vs_off':sum(data[s,arm][0]['team0_win']!=data[s,'Off'][0]['team0_win'] or data[s,arm][0]['team1_win']!=data[s,'Off'][0]['team1_win'] for s in range(1,expected+1))}
        censored=[e['delay_seconds'] for e in ee if e['outcome']!='fired']
        pooled[arm].update(censored_delay_mean_s=statistics.mean(censored) if censored else 0.,censored_delay_p95_s=quantile(censored,.95),censored_delay_max_s=max(censored,default=0.),fired_delay_defined=bool(ds))
    report={'seeds':expected,'bootstrap_draws':20000,'bootstrap_seed':20261106,'means':means,'contrasts':contrasts,'pooled':pooled,'regression_matches':expected*2}
    (folder/'summary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    for m in means: print(m['arm'],{k:round(m[k],4) for k in ['applied_hit_percent','raw_hit_percent','overkill_percent_of_shots','duplicate_target_shot_percent','losses_30s','combat_mean_alive','deferred_attempts']})
    print('Summary:',folder/'summary.json')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder',type=Path);p.add_argument('--expected',type=int,default=20);a=p.parse_args();analyze(a.folder,a.expected)
