"""Matched-seed T5-1/2 analysis. Repetitions average within seed; frames never independent."""
import argparse,csv,json,random,statistics
from pathlib import Path

METRICS=['fps','frame_ms','game_thread','render_thread','rhi_thread','gpu','display_sync','draw_calls','draw_primitives','actor_shown','mass_shown','actor_frustum','mass_frustum','actor_drawn','mass_drawn','shown','frustum','drawn','anchor_alive','actor_render_updates','mass_render_updates','actor_body_triangles','mass_body_triangles','body_triangles','visibility_observer_ms']
def write(folder,name,rows):
    if not rows:return
    with (folder/name).open('w',encoding='utf-8-sig',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
BOOTSTRAP_INDICES={}
def ci(values):
    n=len(values)
    if n not in BOOTSTRAP_INDICES:
        rng=random.Random(20261106)
        BOOTSTRAP_INDICES[n]=[tuple(rng.randrange(n) for _ in range(n)) for _ in range(20000)]
    means=sorted(sum(values[i] for i in indices)/n for indices in BOOTSTRAP_INDICES[n])
    return statistics.mean(values),means[499],means[19499]
def main(folder,additional=None,output=None):
    files=list(folder.glob('seed*-r*.json'))
    if additional:files.extend(additional.glob('seed*-r*.json'))
    folder=output or folder;folder.mkdir(exist_ok=True,parents=True)
    runs=[];assets=[];lods=[]
    for p in sorted(files):
        d=json.loads(p.read_text(encoding='utf-8-sig'))
        assert not d['visual_qa_only'],p
        assert d['frames']==900 and d['camera_errors']==d['shots']==d['rays']==d['paths']==d['visibility_missing']==d['visibility_errors']==0,p
        samples=d['samples'];assert len(samples)==900
        assert all(s['actor_drawn']<=s['actor_frustum'] and s['mass_drawn']<=s['mass_frustum'] for s in samples),p
        if d['filter']=='None':assert all(s['actor_drawn']==s['mass_drawn']==s['actor_shown']==s['mass_shown']==0 for s in samples),p
        if d['filter']=='Actors':assert all(s['mass_drawn']==s['mass_shown']==0 for s in samples),p
        if d['filter']=='Mass':assert all(s['actor_drawn']==s['actor_shown']==0 for s in samples),p
        assert all((s['actor_drawn']==0)==(s['actor_body_triangles']==0) and (s['mass_drawn']==0)==(s['mass_body_triangles']==0) for s in samples),p
        row={k:d[k] for k in ['seed','view','filter','run','anchor_index','anchor_unit_id']}
        for m in METRICS:
            if m in ['fps','shown','frustum','drawn','body_triangles']:continue
            row[m]=statistics.mean(s[m] for s in samples)
        row['fps']=1000/row['frame_ms']
        for m in ['shown','frustum','drawn','body_triangles']:row[m]=row['actor_'+m]+row['mass_'+m]
        row['frame_p95_ms']=sorted(s['frame_ms'] for s in samples)[854]
        assert row['gpu']>0 and row['draw_calls']>0 and row['draw_primitives']>0,p
        runs.append(row)
        for group in ['actor','mass']:
            for lod in range(8):
                lods.append({**{k:row[k] for k in ['seed','view','filter','run']},'group':group,'lod':lod,'predicted_mean_units':statistics.mean(s[f'{group}_lod{lod}'] for s in samples),'render_mean_units':statistics.mean(s[f'{group}_render_lod{lod}'] for s in samples)})
        for asset in d['assets']:
            for lod in asset['lods']:
                r={k:asset[k] for k in ['group','asset','type']};r['leader_pose']=asset.get('leader_pose',False);r.update(lod)
                if r not in assets:assets.append(r)
    write(folder,'runs.csv',runs);write(folder,'assets.csv',assets);write(folder,'lod-usage.csv',lods)
    keys=sorted({(r['seed'],r['view'],r['filter']) for r in runs});seeds=[]
    for seed,view,fil in keys:
        subset=[r for r in runs if (r['seed'],r['view'],r['filter'])==(seed,view,fil)]
        assert len(subset)==2,(seed,view,fil,len(subset))
        seeds.append({'seed':seed,'view':view,'filter':fil,**{m:statistics.mean(r[m] for r in subset) for m in METRICS}})
    write(folder,'seed-metrics.csv',seeds)
    index={(r['seed'],r['view'],r['filter']):r for r in seeds};ids=sorted({r['seed'] for r in seeds});conditions=sorted({(r['view'],r['filter']) for r in seeds})
    assert len(keys)==len(ids)*15 and len(conditions)==15
    means=[{'view':v,'filter':f,'seeds':len(ids),**{m:statistics.mean(index[s,v,f][m] for s in ids) for m in METRICS}} for v,f in conditions]
    write(folder,'condition-means.csv',means)
    contrasts=[];paired=[]
    def contrast(label,a,b,denominator=None):
        for m in METRICS if denominator is None else ['gpu','render_thread']:
            vals=[]
            for s in ids:
                aa=index[(s,*a)];bb=index[(s,*b)];div=1. if denominator is None else index[s,'Recorded','All'][denominator]
                val=(aa[m]-bb[m])/div;vals.append(val)
                paired.append({'comparison':label,'seed':s,'metric':m,'difference':val})
            mean,lo,hi=ci(vals);contrasts.append({'comparison':label,'metric':m,'difference':mean,'ci_low':lo,'ci_high':hi,'seeds':len(ids)})
    for view,fil in conditions:
        if fil!='None':contrast(f'{view}:{fil}-None',(view,fil),(view,'None'))
    for view in ['RTS300','RTS150','RTS60','TPS','FPS']:
        contrast(f'{view}:All-Recorded:All',(view,'All'),('Recorded','All'))
    contrast('Recorded:All-Mass',('Recorded','All'),('Recorded','Mass'))
    contrast('Recorded:All-Actors',('Recorded','All'),('Recorded','Actors'))
    contrast('Recorded:All-NoWeapons',('Recorded','All'),('Recorded','NoWeapons'))
    contrast('per_actor:marginal',('Recorded','All'),('Recorded','Mass'),'actor_shown')
    contrast('per_actor:isolated',('Recorded','Actors'),('Recorded','None'),'actor_shown')
    contrast('per_mass:marginal',('Recorded','All'),('Recorded','Actors'),'mass_shown')
    contrast('per_mass:isolated',('Recorded','Mass'),('Recorded','None'),'mass_shown')
    contrast('per_soldier:all',('Recorded','All'),('Recorded','None'),'shown')
    contrast('per_soldier:weapons',('Recorded','All'),('Recorded','NoWeapons'),'shown')
    write(folder,'contrasts.csv',contrasts);write(folder,'paired-differences.csv',paired)
    stability=[]
    for view,fil in conditions:
        if fil!='All':continue
        for metric in ['gpu','render_thread','draw_primitives']:
            for name,ss in [('first_half',ids[:len(ids)//2]),('second_half',ids[len(ids)//2:])]:
                stability.append({'view':view,'metric':metric,'subset':name,'n':len(ss),'all_minus_none':statistics.mean(index[s,view,'All'][metric]-index[s,view,'None'][metric] for s in ss)})
    write(folder,'stability.csv',stability)
    result={'visibility_method':'UE 5.8 main-view frustum and post-occlusion material mesh batches (cached and dynamic), unique body component IDs; not per-pixel visibility','body_triangles_method':'Triangle-list primitives submitted by visible body mesh batches for one main view, no repeated depth/shadow passes','rhi_primitives_method':'Whole-frame RHI primitive counter, includes passes and non-body geometry; not interchangeable with main body triangles','render_updates_method':'Legacy LastRenderTimeOnScreen fresh timestamp includes ray-tracing geometry updates and is not a visible-soldier count','seeds':ids,'runs':len(runs),'conditions':means,'contrasts':contrasts,'bootstrap':{'draws':20000,'seed':20261106,'unit':'paired placement seed; average two repetitions first','interval':'percentile 95%, no multiplicity correction'}}
    (folder/'summary.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    for r in means:print(r['view'],r['filter'],'GPU',round(r['gpu'],4),'RT',round(r['render_thread'],4),'FPS',round(r['fps'],2),'shown/frustum/drawn',*(round(r[k],2) for k in ['shown','frustum','drawn']))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder',type=Path);p.add_argument('--additional',type=Path);p.add_argument('--output',type=Path);a=p.parse_args();main(a.folder,a.additional,a.output)
