"""Read-only analysis of existing runs; output is isolated from source evidence."""
import csv, json, math, statistics as st, hashlib, re
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
OUT=ROOT/'Setup/Benchmarks/FrameSpikes-20261110'
SETS=['PerceptionGrid-20261110','AnimTasks-20261110','GTBothOn-20261110']
PARTS=['perception_ms','decision_ms','path_ms','follow_ms','rvo_ms','shot_ms','display_ms']
def read(p): return list(csv.DictReader(p.open(encoding='utf-8-sig',newline='')))
def mean(v): return st.mean(v) if v else None
def corr(a,b):
    if len(a)<3 or max(a)==min(a) or max(b)==min(b): return None
    return st.correlation(a,b)
def quant(a): return sorted(a)[math.ceil(.95*len(a))-1]
def writecsv(p,rows):
    if not rows:return
    with p.open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
def main():
    OUT.mkdir(exist_ok=True); summaries=[];tops=[];seconds=[];lags=[];neighbors=[];logs=[]
    for dataset in SETS:
      for p in sorted((ROOT/'Setup/Benchmarks'/dataset).glob('*/frames.csv')):
        run=p.parent
        if not (run.name.startswith('perf-') or dataset=='GTBothOn-20261110' or run.name.startswith('trace-')):continue
        raw=read(p); n=len(raw); a={k:[float(r[k]) for r in raw] for k in raw[0] if k!='view'}
        for k in ['shots','deaths','decisions','paths','pending_reports_peak']:
            a['delta_'+k]=[0]+[v-u for u,v in zip(a[k],a[k][1:])]
        a['residual_ms']=[a['gt_ms'][i]-sum(a[k][i] for k in PARTS) for i in range(n)]
        # Sample at start of Step records preceding costs, while time is current world time.
        a['work_time']=[a['time'][0]-a['wall_ms'][0]/1000]+a['time'][:-1]
        gt=a['gt_ms']; q=quant(gt); ix=[i for i,v in enumerate(gt) if v>q]; normal=[i for i in range(n) if i not in set(ix)]
        m=json.loads((run/'manifest.json').read_text(encoding='utf-8-sig')); label=dataset+'/'+run.name
        keys=[k for k in a if k.endswith('_ms') or k.startswith('delta_')]
        s={'run':label,'n':n,'dll':m.get('dll_sha256'),'trace':m.get('cpu_trace'), 'grid':m.get('perception_grid'),'anim':m.get('anim_tasks'),
           'gt_mean':mean(gt),'gt_p95':q,'gt_max':max(gt),'spikes':len(ix),'spikes_per_second':len(ix)/(a['time'][-1]-a['time'][0]),
           'metrics':{k:{'mean':mean(v),'p95':quant(v),'spike_mean':mean([v[i] for i in ix]),'normal_mean':mean([v[i] for i in normal]),'corr_gt':corr(gt,v)} for k,v in a.items() if k in keys},
           'spike_intervals_s':[a['time'][j]-a['time'][i] for i,j in zip(ix,ix[1:])],
           'batch_spike_pct':100*sum(a['sight_scan_ms'][i]>0 for i in ix)/len(ix),
           'batch_normal_pct':100*sum(a['sight_scan_ms'][i]>0 for i in normal)/len(normal),
           'pending_max':max(a['pending_reports_peak']),'pending_record_updates':sum(v>0 for v in a['delta_pending_reports_peak']),
           'shots_delta':a['shots'][-1]-a['shots'][0], 'deaths_delta':a['deaths'][-1]-a['deaths'][0],
           'source_sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
        for phase in [.05,.2,1.0]:
            bins=[0]*10; exp=[0]*10
            for i,t in enumerate(a['work_time']):
                b=min(9,int((t%phase)/phase*10));exp[b]+=1
                if i in ix:bins[b]+=1
            s['phase_'+str(phase)]={'all':exp,'spikes':bins,'rate':[x/y if y else None for x,y in zip(bins,exp)]}
        text=(run/'run.log').read_text(encoding='utf-8-sig'); match=re.search(r'P3_BATCH_LATENESS max_ms=([\d.]+)',text)
        s['batch_lateness_max_including_warmup']=float(match[1]) if match else None
        for j,line in enumerate(text.splitlines(),1):
            if re.search(r'Garbage|CollectGarbage|LogStreaming:|LogContentStreaming:|LogShaderCompilers:|P3_RESULT|P3_BATCH_LATENESS',line,re.I):logs.append({'run':label,'line':j,'text':line})
        for k in dict.fromkeys(['gt_ms','rt_ms','gpu_ms','wall_ms','pending_reports_peak']+keys):
            v=a[k];cut=quant(v)
            for i in sorted((i for i,x in enumerate(v) if x>cut),key=lambda i:v[i],reverse=True):
                tops.append({'run':label,'metric':k,'csv_line':i+2,'time':a['time'][i],'work_time':a['work_time'][i],'value':v[i],'p95':cut,'gt_ms':gt[i], **{x:a[x][i] for x in PARTS+['residual_ms','delta_shots','delta_deaths','delta_pending_reports_peak']}})
        for sec in range(10,70):
            ids=[i for i,t in enumerate(a['time']) if sec<=t<sec+1]
            if ids:seconds.append({'run':label,'second':sec,'n':len(ids),'spikes':sum(i in ix for i in ids),'gt_mean':mean([gt[i] for i in ids]),'shots':sum(a['delta_shots'][i] for i in ids),'pending_records':sum(a['delta_pending_reports_peak'][i]>0 for i in ids)})
        for lag in range(-3,4):
            for k in ['delta_shots','delta_pending_reports_peak','perception_ms','report_queue_ms','report_delivery_ms','decision_ms','follow_ms','rvo_ms','residual_ms','wall_ms','rt_ms','gpu_ms']:
                ids=[i for i in range(n) if 0<=i+lag<n]
                lags.append({'run':label,'metric':k,'lag':lag,'r':corr([gt[i] for i in ids],[a[k][i+lag] for i in ids])})
            ids=[i+lag for i in ix if 0<=i+lag<n]
            neighbors.append({'run':label,'offset':lag,'n':len(ids),**{k:mean([a[k][i] for i in ids]) for k in keys}})
        summaries.append(s)
    (OUT/'summary.json').write_text(json.dumps(summaries,indent=2),encoding='utf-8')
    for name,rows in [('top-frames',tops),('seconds',seconds),('lag-correlations',lags),('neighbors',neighbors),('log-evidence',logs)]:writecsv(OUT/(name+'.csv'),rows)
    print('Analyzed',len(summaries),'runs;',sum(s['n'] for s in summaries),'frames')
if __name__=='__main__':main()
