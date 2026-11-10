"""Paired field-trial analysis; stdlib only. Run with UE bundled Python.
CIs bootstrap paired seed differences (Squad - Individual), not individual shots.
"""
import argparse, json, math, random, re, statistics
from pathlib import Path

def parse_results(path):
    return [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in path.read_text(encoding='utf-8-sig').splitlines() if 'FIELD_TRIAL mode=' in line]

def derived(row, diag):
    shots = int(row['shots'])
    raw = diag['raw_hits']; hit = int(row['hits'])
    bins = diag['shot_bins']
    return {
        'combat_fps': float(row['combatFPS']), 'combat_frame_ms': 1000/float(row['combatFPS']),
        'initial_fps': float(row['fps30']), 'whole_match_fps': float(row['fps']),
        'applied_hit_percent': 100*hit/shots, 'raw_hit_percent': 100*raw/shots,
        'overkill_percent_of_shots': 100*diag['overkill_hits']/shots,
        'spread_miss_percent': 100*diag['spread_misses']/shots,
        'duplicate_target_shot_percent': 100*diag['duplicate_target_shots']/shots,
        'moving_shot_percent': 100*sum(b['shots'] for b in bins if b['moving'])/shots,
        'shot_distance_m': sum(b['distance_sum_m'] for b in bins)/shots,
        'spread_deg': sum(b['spread_sum_deg'] for b in bins)/shots,
        'losses_30s': int(row['losses30']), 'losses_final': 500-sum(map(int,row['alive'].split('/'))),
        'duration_capped_s': float(row['time']), 'timeout': float(row['time'])>=180,
        'first_discovery_s': float(row['first']), 'confirmation_delay_s': float(row['delay']),
        'shots': shots, 'raw_hits': raw, 'overkill_hits': diag['overkill_hits'],
        **{'combat_'+k+'_ms':v for k,v in diag['combat_ms_per_frame'].items()}
    }

def paired_stats(a,b,indices):
    ds=[y-x for x,y in zip(a,b)]
    bootstrap=sorted(sum(ds[i] for i in draw)/len(ds) for draw in indices)
    nonzero=[d for d in ds if abs(d)>1e-12]
    k=min(sum(d>0 for d in nonzero),sum(d<0 for d in nonzero)); n=len(nonzero)
    sign_p=min(1.,2*sum(math.comb(n,j) for j in range(k+1))/2**n) if n else 1.
    return dict(individual_mean=statistics.mean(a), squad_mean=statistics.mean(b),
                paired_difference=statistics.mean(ds), ci95=[bootstrap[int(.025*len(bootstrap))],bootstrap[int(.975*len(bootstrap))]],
                squad_higher=sum(d>0 for d in ds), squad_lower=sum(d<0 for d in ds), equal=sum(d==0 for d in ds), sign_p=sign_p)

def analyze(path,expected):
    rows=parse_results(path); by={}
    prefix=path.name.removesuffix('-field-results.txt')
    for row in rows:
        assert row['success']=='1' and row['failures']=='0' and row['contacts']=='100' and row['combatFrames']=='900',row
        key=(int(row['seed']),row['mode']); assert key not in by,key
        diag=json.loads((path.parent/f"{prefix}-field-{row['seed']}-{row['mode']}-diagnostics.json").read_text(encoding='utf-8-sig'))
        assert diag['combat_frames']==900 and diag['frames']==int(row['frames'])
        for window in ['combat_ms_per_frame','match_ms_per_frame']:
            t=diag[window]
            assert 0 <= t['find_ray'] <= t['find_inclusive'] <= t['scan_inclusive'] <= t['step']
            assert 0 <= t['ray'] <= t['scan_inclusive'] and t['gpu']>0 and t['render_thread']>0
            assert abs(t['find_excluding_rays']-(t['find_inclusive']-t['find_ray']))<1e-6

        assert diag['raw_hits']==sum(b['raw_hits'] for b in diag['shot_bins'])
        assert int(row['hits'])==sum(b['applied_hits'] for b in diag['shot_bins'])
        assert int(row['shots'])==sum(b['shots'] for b in diag['shot_bins'])
        assert diag['raw_hits']-int(row['hits'])==diag['overkill_hits']
        assert diag['raw_hits']+diag['spread_misses']==int(row['shots'])
        by[key]=(row,diag,derived(row,diag))
    seeds=sorted(set(k[0] for k in by)); assert len(seeds)==expected and len(by)==expected*2
    for seed in seeds: assert (seed,'Individual') in by and (seed,'Squad') in by
    rng=random.Random(20261106); indices=[[rng.randrange(expected) for _ in seeds] for _ in range(20000)]
    metrics={}
    for metric in by[seeds[0],'Individual'][2]:
        metrics[metric]=paired_stats([by[s,'Individual'][2][metric] for s in seeds],[by[s,'Squad'][2][metric] for s in seeds],indices)
    totals={}
    for mode in ['Individual','Squad']:
        dd=[by[s,mode][1] for s in seeds]
        keys=['shots','raw_hits','applied_hits','overkill_hits','spread_misses','shared_checks','shared_reject_dead','shared_reject_range','shared_reject_fov','shared_reject_terrain','target_changes','duplicate_target_shots','shot_target_groups']
        t={k:sum(d[k] for d in dd) for k in keys}
        t['shot_bins']=[]
        for i in range(12):
            proto=dd[0]['shot_bins'][i]
            t['shot_bins'].append({**{k:proto[k] for k in ['moving','weapon','distance_bin']},**{k:sum(d['shot_bins'][i][k] for d in dd) for k in ['shots','raw_hits','applied_hits','distance_sum_m','spread_sum_deg']}})
        totals[mode]=t
    # Symmetric mixture/within-bin decomposition of pooled raw geometric hit-rate difference.
    composition=within=0.; unsupported=[]
    for i,(a,b) in enumerate(zip(totals['Individual']['shot_bins'],totals['Squad']['shot_bins'])):
        wa=a['shots']/totals['Individual']['shots']; wb=b['shots']/totals['Squad']['shots']
        if not a['shots'] or not b['shots']:
            if a['shots'] or b['shots']: unsupported.append(i)
            continue
        ra=a['raw_hits']/a['shots']; rb=b['raw_hits']/b['shots']
        composition+=(wb-wa)*(ra+rb)/2; within+=(rb-ra)*(wa+wb)/2
    return dict(source=str(path),seeds=seeds,bootstrap_resamples=20000,ci_unit='paired seed; Squad - Individual',
                metrics=metrics,totals=totals,pooled_raw_hit_decomposition_pp=dict(composition=100*composition,within_bins=100*within,unsupported_bins=unsupported),
                rows=[dict(result=by[s,m][0],derived=by[s,m][2]) for s in seeds for m in ['Individual','Squad']])

if __name__=='__main__':
    p=argparse.ArgumentParser(); p.add_argument('results',type=Path); p.add_argument('--expected',type=int,default=20); a=p.parse_args()
    report=analyze(a.results,a.expected); out=a.results.with_name(a.results.stem+'-analysis.json'); out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    for k in ['combat_fps','applied_hit_percent','raw_hit_percent','overkill_percent_of_shots','losses_30s','losses_final','duration_capped_s','shot_distance_m','moving_shot_percent','combat_find_excluding_rays_ms','combat_ray_ms','combat_render_thread_ms','combat_gpu_ms']:
        v=report['metrics'][k]; print(f"{k}: I={v['individual_mean']:.4f}, S={v['squad_mean']:.4f}, delta={v['paired_difference']:.4f}, CI95={v['ci95']}, +/-={v['squad_higher']}/{v['squad_lower']}")
    print(out)
