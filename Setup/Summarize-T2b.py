"""Paired seed analysis for turn-to-face trials, with exposure-normalized shooting."""
import argparse,csv,importlib.util,json,math,random,statistics
from collections import Counter
from pathlib import Path
HERE=Path(__file__).resolve().parent
def module(name,file):
    s=importlib.util.spec_from_file_location(name,HERE/file);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
old=module('field','Summarize-FieldDiagnostics.py');reg=module('reg','Check-T2bRegression.py')
def writecsv(path,rows):
    if not rows:return
    with path.open('w',encoding='utf-8-sig',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
def quantile(xs,q):return sorted(xs)[max(0,math.ceil(q*len(xs))-1)] if xs else 0.
def dist(xs):return dict(n=len(xs),mean=statistics.mean(xs) if xs else None,p50=quantile(xs,.5) if xs else None,p95=quantile(xs,.95) if xs else None,maximum=max(xs) if xs else None)

def analyze(folder,expected):
    reg.check(folder,expected*2)
    arms=['Individual','Off','Rate540','Rate1080'];seeds=list(range(1,expected+1));data={};flat=[];turns=[];firsts=[];curves=[]
    for seed in seeds:
        for arm in arms:
            name=f'seed{seed}-'+('Individual-Off' if arm=='Individual' else f'Squad-{arm}')
            row=reg.rows(folder/(name+'.log'))[-1]
            j=json.loads((folder/(name+'-diagnostics.json')).read_text(encoding='utf-8-sig'))
            x=json.loads((folder/(name+'-t2b.json')).read_text(encoding='utf-8-sig'))
            assert row['success']=='1' and row['failures']=='0' and x['combat_window_frames']==900
            assert x['shots_during_turn']==0 and x['angular_speed_violations']==0
            assert x['max_step_angle_deg']<=x['turn_rate_deg_s']/30+1e-5
            assert x['turn_started']==len(x['turn_events'])
            assert int(row['shots'])==j['shots']==sum(b['shots'] for b in j['shot_bins'])
            assert int(row['hits'])==j['applied_hits']==sum(b['applied_hits'] for b in j['shot_bins'])
            assert j['raw_hits']==sum(b['raw_hits'] for b in j['shot_bins'])
            curve={int(z['seconds']):z for z in x['survival_curve']}
            assert curve[30]['shots_cumulative']-curve[0]['shots_cumulative']==x['combat_window_shots']
            assert abs(x['combat_window_shots']/x['alive_shooter_seconds']-x['shots_per_alive_shooter_second'])<1e-12
            assert all(curve[t]['alive']>=curve[t+5]['alive'] for t in sorted(curve)[:-1])
            for e in x['turn_events']:
                assert e['initial_angle_deg']<=180.00001 and e['elapsed_s']>=0
                if e['rotation_s']>=0: assert e['rotated_deg']<=e['rotation_s']*x['turn_rate_deg_s']+1e-5
                if e['outcome']=='fired': assert e['start_frame']<e['aligned_frame']<=e['confirmed_frame']<=e['end_frame']
            d={k:v for k,v in old.derived(row,j).items() if 'fps' not in k and '_ms' not in k}
            d.update(support_shot_percent=100*sum(b['shots'] for b in j['shot_bins'] if b['weapon']=='support')/j['shots'],
                     shared_checks=j['shared_checks'],shared_reject_fov=j['shared_reject_fov'],shared_fov_reject_percent=100*j['shared_reject_fov']/j['shared_checks'] if j['shared_checks'] else 0.,
                     shared_reject_range=j['shared_reject_range'],shared_reject_terrain=j['shared_reject_terrain'],team0_win=int(row['winner'])==0,team1_win=int(row['winner'])==1,draw=int(row['winner'])==-1)
            for k in ['combat_mean_alive','combat_window_shots','alive_shooter_seconds','shots_per_alive_shooter_second','turn_started','shots_after_turn','ready_fov_rejections','ready_scans_waiting_for_turn','lost_clear_ready_opportunities','max_turn_starts_per_frame','max_simultaneously_turning']:
                d[k]=x[k]
            d['first_shot_s']=x['first_shot_frame']/30.
            rot=[e['rotation_s'] for e in x['turn_events'] if e['rotation_s']>=0]
            delay=[e['delay_s'] for e in x['candidate_first_shots']]
            shared=[e['delay_s'] for e in x['candidate_first_shots'] if e['shared']]
            d.update(rotation_completed=len(rot),rotation_mean_s=statistics.mean(rot) if rot else 0.,rotation_p50_s=quantile(rot,.5),rotation_p95_s=quantile(rot,.95),rotation_max_s=max(rot,default=0.),
                     candidate_first_shots=len(delay),candidate_first_shot_mean_s=statistics.mean(delay),candidate_first_shot_p50_s=quantile(delay,.5),candidate_first_shot_p95_s=quantile(delay,.95),candidate_first_shot_max_s=max(delay),
                     shared_first_shot_mean_s=statistics.mean(shared) if shared else 0.)
            for outcome in ['fired','candidate_changed','candidate_lost','shooter_dead','match_end','range_rejected','terrain_rejected','fov_rejected']:
                d['turn_'+outcome]=sum(e['outcome']==outcome for e in x['turn_events'])
            data[seed,arm]=(d,j,x,curve)
            turns.extend(dict(seed=seed,arm=arm,**e) for e in x['turn_events'])
            firsts.extend(dict(seed=seed,arm=arm,**e) for e in x['candidate_first_shots'])
            curves.extend(dict(seed=seed,arm=arm,**e) for e in x['survival_curve'])
    for seed in seeds:
        for arm in arms:
            d,j,x,curve=data[seed,arm];reference=data[seed,'Individual'][3]
            d['curve_mae_vs_individual_5_30']=statistics.mean(abs(curve[t]['alive']-reference[t]['alive']) for t in range(5,31,5))
            flat.append(dict(seed=seed,arm=arm,**d))
    for name,rows in [('seed-metrics.csv',flat),('turn-events.csv',turns),('candidate-first-shots.csv',firsts),('survival-curves.csv',curves)]:writecsv(folder/name,rows)
    means=[dict(arm=a,**{k:statistics.mean(data[s,a][0][k] for s in seeds) for k in data[1,a][0]}) for a in arms]
    writecsv(folder/'condition-means.csv',means)
    draws={}
    def stats(a,b):
        n=len(a)
        if n not in draws:
            rng=random.Random(20261106);draws[n]=[[rng.randrange(n) for _ in range(n)] for _ in range(20000)]
        o=old.paired_stats(a,b,draws[n]);return o['paired_difference'],o['ci95'][0],o['ci95'][1]
    contrasts=[];paired=[]
    for arm in arms[1:]:
        for ref in ['Individual','Off']:
            if arm==ref:continue
            for metric in data[1,arm][0]:
                if metric.startswith('rotation_') and metric!='rotation_completed':continue
                if metric in {'shared_first_shot_mean_s','shared_fov_reject_percent'} and ref=='Individual':continue
                a=[data[s,ref][0][metric] for s in seeds];b=[data[s,arm][0][metric] for s in seeds];diff,lo,hi=stats(a,b)
                contrasts.append(dict(arm=arm,reference=ref,metric=metric,difference=diff,ci_low=lo,ci_high=hi))
                paired.extend(dict(seed=s,arm=arm,reference=ref,metric=metric,difference=b[i]-a[i]) for i,s in enumerate(seeds))
            ds=[100*(data[s,arm][0]['losses_30s']/data[s,ref][0]['losses_30s']-1) for s in seeds];diff,lo,hi=stats([0.]*expected,ds)
            contrasts.append(dict(arm=arm,reference=ref,metric='losses_30s_relative_percent',difference=diff,ci_low=lo,ci_high=hi))
    # Speed sensitivity uses the same seed pairing, not independent cohorts.
    for metric in data[1,'Rate540'][0]:
        a=[data[s,'Rate540'][0][metric] for s in seeds];b=[data[s,'Rate1080'][0][metric] for s in seeds];diff,lo,hi=stats(a,b)
        contrasts.append(dict(arm='Rate1080',reference='Rate540',metric=metric,difference=diff,ci_low=lo,ci_high=hi))
    writecsv(folder/'contrasts.csv',contrasts);writecsv(folder/'paired-differences.csv',paired)
    curve_means=[];curve_diff=[]
    for t in sorted(set(e['seconds'] for e in curves)):
        for arm in arms:
            available=[s for s in seeds if t in data[s,arm][3]]
            if available:curve_means.append(dict(seconds=t,arm=arm,n=len(available),mean_alive=statistics.mean(data[s,arm][3][t]['alive'] for s in available)))
        for arm in arms[1:]:
            for ref in ['Individual','Off']:
                if arm==ref:continue
                available=[s for s in seeds if t in data[s,arm][3] and t in data[s,ref][3]]
                if not available:continue
                a=[data[s,ref][3][t]['alive'] for s in available];b=[data[s,arm][3][t]['alive'] for s in available]
                diff,lo,hi=stats(a,b) if len(available)>1 else (b[0]-a[0],None,None)
                curve_diff.append(dict(seconds=t,arm=arm,reference=ref,n=len(available),difference=diff,ci_low=lo,ci_high=hi))
    writecsv(folder/'curve-means.csv',curve_means);writecsv(folder/'curve-contrasts.csv',curve_diff)
    pooled={}
    for arm in arms:
        ee=[e for e in turns if e['arm']==arm];ff=[e for e in firsts if e['arm']==arm]
        pooled[arm]=dict(rotation=dist([e['rotation_s'] for e in ee if e['rotation_s']>=0]),
                        turn_to_fired=dist([e['elapsed_s'] for e in ee if e['outcome']=='fired']),
                        candidate_first_shot=dist([e['delay_s'] for e in ff]),
                        shared_candidate_first_shot=dist([e['delay_s'] for e in ff if e['shared']]),
                        outcomes={k:sum(e['outcome']==k for e in ee) for k in sorted(set(e['outcome'] for e in ee))},
                        outcomes_before_alignment={k:sum(e['outcome']==k and e['aligned_frame']<0 for e in ee) for k in sorted(set(e['outcome'] for e in ee))},
                        max_turns_per_soldier=max(Counter((e['seed'],e['shooter']) for e in ee).values(),default=0),
                        max_turns_per_shooter_target=max(Counter((e['seed'],e['shooter'],e['target']) for e in ee).values(),default=0),
                        repeated_shooter_target_pairs=sum(n>1 for n in Counter((e['seed'],e['shooter'],e['target']) for e in ee).values()),
                        winner_counts={k:sum(data[s,arm][0][k] for s in seeds) for k in ['team0_win','team1_win','draw']},
                        winner_changes_vs_off=sum((data[s,arm][0]['team0_win'],data[s,arm][0]['team1_win'])!=(data[s,'Off'][0]['team0_win'],data[s,'Off'][0]['team1_win']) for s in seeds))
    report=dict(seeds=seeds,bootstrap_draws=20000,bootstrap_seed=20261106,means=means,contrasts=contrasts,curve_means=curve_means,curve_contrasts=curve_diff,pooled=pooled,regression_matches=expected*2)
    (folder/'summary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    for m in means:print(m['arm'],{k:round(m[k],5) for k in ['applied_hit_percent','losses_30s','combat_mean_alive','combat_window_shots','shots_per_alive_shooter_second','shared_fov_reject_percent','turn_started','curve_mae_vs_individual_5_30']})

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder',type=Path);p.add_argument('--expected',type=int,default=20);a=p.parse_args();analyze(a.folder,a.expected)
