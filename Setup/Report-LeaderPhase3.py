"""Summarize paired real-time mixed-AI trials. Never combine returned-to-RTS samples with FPS/TPS."""
import argparse, csv, json, math, statistics
from pathlib import Path

def stats(values):
    values = sorted(float(v) for v in values if math.isfinite(float(v)))
    if not values:
        return dict(samples=0, mean=None, p95=None, over_16_67_pct=None)
    return dict(samples=len(values), mean=statistics.mean(values),
                p95=values[math.ceil(.95*len(values))-1],
                over_16_67_pct=100*sum(v>16.67 for v in values)/len(values))

def report(root):
    plan=json.loads((root/'plan.json').read_text(encoding='utf-8-sig')) if (root/'plan.json').exists() else {}
    if plan.get('status')=='superseded': raise ValueError('Superseded run set must not be used as final evidence')
    runs=[]
    raw={}
    for path in sorted(root.glob('*/manifest.json')):
        try: m=json.loads(path.read_text(encoding='utf-8-sig'))
        except json.JSONDecodeError: continue
        if m['status']!='passed' or m.get('nullrhi'):
            continue
        with (path.parent/'frames.csv').open(encoding='utf-8-sig',newline='') as f:
            allrows=list(csv.DictReader(f))
        rows=[r for r in allrows if r['view']==m['view']]
        raw[path.parent.name]=rows
        timing={k:stats([r[k] for r in rows if k!='gpu_ms' or float(r[k])>0])
                for k in ['gt_ms','rt_ms','gpu_ms','wall_ms','gt_wait_ms','rt_wait_ms',
                          'perception_ms','decision_ms','path_ms','follow_ms','rvo_ms','shot_ms','display_ms',
                          'sight_scan_ms','report_delivery_ms','report_queue_ms']}
        run={k:m[k] for k in ['units','decision','view','layout','seed','stable','dll_sha256']}
        run.update(folder=path.parent.name, samples=len(rows), total_samples=len(allrows),
                   actual_view_seconds=sum(float(r['wall_ms']) for r in rows)/1000,
                   timing=timing, gpu_missing=sum(float(r['gpu_ms'])<=0 for r in rows),
                   alive=stats([r['alive'] for r in rows]),
                   final=allrows[-1] if allrows else None,
                   valid_registration=all(int(r['alive'])==int(r['rvo_registered']) for r in allrows))
        runs.append(run)
    dlls={r['dll_sha256'] for r in runs}
    if len(dlls)>1:
        raise ValueError('Multiple DLLs: keep exploratory and accepted data separate')
    pairs=[]
    for a in runs:
        if a['decision']!='Individual': continue
        b=next((b for b in runs if b['decision']=='Leader' and all(a[k]==b[k] for k in ['units','view','layout','seed','stable'])),None)
        if not b or not a['samples'] or not b['samples']: continue
        pairs.append(dict(units=a['units'],view=a['view'],layout=a['layout'],seed=a['seed'],stable=a['stable'],
                          individual=a['folder'],leader=b['folder'],
                          gt_delta_ms=b['timing']['gt_ms']['mean']-a['timing']['gt_ms']['mean'],
                          alive_delta=b['alive']['mean']-a['alive']['mean']))
        # Exploratory conditioning on both elapsed time and living population.
        # Never imply identical tactics or a causal estimate from this subset.
        buckets=[]
        for run in (a,b):
            d={}
            for row in raw[run['folder']]:
                key=(int(float(row['time'])//5),int(int(row['alive'])//(run['units']*.05)))
                d.setdefault(key,[]).append(float(row['gt_ms']))
            buckets.append(d)
        common=sorted(buckets[0].keys() & buckets[1].keys())
        pairs[-1]['matched_time_population_bins']=len(common)
        pairs[-1]['matched_gt_delta_ms']=statistics.mean(statistics.mean(buckets[1][k])-statistics.mean(buckets[0][k]) for k in common) if common else None
    (root/'summary.json').write_text(json.dumps(dict(complete=len(runs)==plan.get('expected_runs',len(runs)),expected_runs=plan.get('expected_runs'),runs=runs,pairs=pairs),ensure_ascii=False,indent=2),encoding='utf-8')
    print(f'{len(runs)} runs, {len(pairs)} paired comparisons; DLLs={len(dlls)}')
    for n in [500,1000]:
        for view in ['RTS','TPS','FPS']:
            for mode in ['Individual','Leader']:
                group=[r for r in runs if r['units']==n and r['view']==view and r['decision']==mode]
                if group and all(r['samples'] for r in group):
                    print(n,view,mode,'GT mean',round(statistics.mean(r['timing']['gt_ms']['mean'] for r in group),3),
                          'RT',round(statistics.mean(r['timing']['rt_ms']['mean'] for r in group),3),
                          'GPU',round(statistics.mean(r['timing']['gpu_ms']['mean'] for r in group if r['timing']['gpu_ms']['mean'] is not None),3),
                          'alive',round(statistics.mean(r['alive']['mean'] for r in group),1))
    # Complete per-condition distributions, not averages of p95 values.
    fields=['units','view','layout','decision','runs','samples','view_seconds','alive_mean']
    metrics=['gt_ms','rt_ms','gpu_ms','wall_ms']
    fields += [k+'_'+stat for k in metrics for stat in ['mean','p95','over_16_67_pct']]
    with (root/'conditions.csv').open('w',encoding='utf-8-sig',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=fields);writer.writeheader()
        for n in [500,1000]:
            for view in ['RTS','TPS','FPS']:
                for layout in ['Open','Cross','Gate']:
                    for mode in ['Individual','Leader']:
                        group=[r for r in runs if (r['units'],r['view'],r['layout'],r['decision'])==(n,view,layout,mode)]
                        data=[row for r in group for row in raw[r['folder']]]
                        if not data:continue
                        row=dict(units=n,view=view,layout=layout,decision=mode,runs=len(group),samples=len(data),view_seconds=sum(float(r['wall_ms']) for r in data)/1000,alive_mean=statistics.mean(int(r['alive']) for r in data))
                        for k in metrics:
                            st=stats([r[k] for r in data if k!='gpu_ms' or float(r[k])>0])
                            row.update({k+'_'+s:st[s] for s in ['mean','p95','over_16_67_pct']})
                        writer.writerow(row)
    return runs

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('root',type=Path)
    report(parser.parse_args().root)
