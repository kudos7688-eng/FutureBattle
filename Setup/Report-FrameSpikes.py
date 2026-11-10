"""Descriptive spike frequency and per-run tables; no causal inference."""
import json,math,statistics as st
from collections import Counter
from importlib.machinery import SourceFileLoader
from pathlib import Path
A=SourceFileLoader('spikes',str(Path(__file__).with_name('Analyze-FrameSpikes.py'))).load_module()
ss=json.loads((A.OUT/'summary.json').read_text());extra=[]
lines=['# 各走行のp95超フレーム（厳密に > p95）','',
'閾値は各走行でnearest-rank方式。DLL/視点/フラグ/trace有無を混合してp95を算出しない。差分は「p95超平均−それ以外平均」で、p95−全体平均とは異なる。msは排他的な7つのPhase3 scopeを加算し、残差=GT−7scope。sight/reportはperception内の子scopeなので二重加算しない。GTはviewport間、scopeはStep間のため残差は近似であり厳密な未計測CPU時間ではない。','',
'|走行|N|GT平均/p95/最大 ms|上位数 /秒|認識Δ|判断Δ|pathΔ|followΔ|RVOΔ|shotΔ|表示Δ|残差Δ|GT待ちΔ|',
'|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|']
for s in ss:
    m=s['metrics'];d=lambda k:m[k]['spike_mean']-m[k]['normal_mean']
    lines.append(f"|{s['run']}|{s['n']}|{s['gt_mean']:.3f}/{s['gt_p95']:.3f}/{s['gt_max']:.3f}|{s['spikes']} / {s['spikes_per_second']:.2f}|"+'|'.join(f'{d(k):.3f}' for k in A.PARTS+['residual_ms','gt_wait_ms'])+'|')
    p=A.ROOT/'Setup/Benchmarks'/s['run'];r=A.read(p/'frames.csv');n=len(r);g=[float(x['gt_ms']) for x in r];q=s['gt_p95'];ix={i for i,x in enumerate(g) if x>q}
    first=next((i for i,x in enumerate(r) if float(x['shots'])>0),n)
    isolated=sum(i-1 not in ix and i+1 not in ix for i in ix);clusters=sum(i-1 not in ix for i in ix)
    persecond=Counter(int(float(r[i]['time'])) for i in ix)
    detrended={}
    for k in ['gt_ms','shots','pending_reports_peak']:
        v=[float(x[k]) for x in r]
        if k!='gt_ms':v=[0]+[b-a for a,b in zip(v,v[1:])]
        groups={sec:[v[i] for i,x in enumerate(r) if int(float(x['time']))==sec] for sec in range(10,70)}
        detrended[k]=[v[i]-st.mean(groups[int(float(x['time']))]) for i,x in enumerate(r)]
    e={'run':s['run'],'spikes':len(ix),'isolated':isolated,'clusters':clusters,'clusters_per_second':clusters/60,'pre_first_shot_n':first,'pre_first_shot_spikes':sum(i<first for i in ix),'first_shot_time':float(r[first]['time']) if first<n else None,'seconds_with_spike':len(persecond),'max_spikes_per_second':max(persecond.values()),'detrended_shots_r':A.corr(detrended['gt_ms'],detrended['shots']),'detrended_peak_increment_r':A.corr(detrended['gt_ms'],detrended['pending_reports_peak'])}
    # Approximate fixed-lag sample autocorrelation, with the represented time reported.
    dt=(float(r[-1]['time'])-float(r[0]['time']))/(n-1)
    for seconds in [.05,.2,1]:
        lag=max(1,round(seconds/dt));e['acf_'+str(seconds)]=A.corr(g[:-lag],g[lag:]);e['acf_actual_s_'+str(seconds)]=lag*dt
    extra.append(e)
lines+=['','## 時間構造・戦況（フレーム単位）','','孤立=前後1frameがp95以下。連続する上位frameを1群と数える。これは発生形態であって原因分類ではない。detrended rは1秒ごとの平均を除去したPearson相関。最初のCSV行の累積差分は不明なので0とする。周期自己相関は平均frame間隔からの近似で、厳密な時系列スペクトルではない。','','|走行|孤立/上位|連続群数|初発砲前 上位/N|上位の出た秒数/60|秒内最大|発砲差分r（秒内補正）|ピーク増分r（秒内補正）|約50ms/200ms/1s自己相関|','|---|---:|---:|---:|---:|---:|---:|---:|---:|']
for e in extra:
    f=lambda v:f'{v:.3f}' if v is not None else 'NA'
    lines.append(f"|{e['run']}|{e['isolated']}/{e['spikes']}|{e['clusters']}|{e['pre_first_shot_spikes']}/{e['pre_first_shot_n']}|{e['seconds_with_spike']}/60|{e['max_spikes_per_second']}|{f(e['detrended_shots_r'])}|{f(e['detrended_peak_increment_r'])}|"+'/'.join(f(e['acf_'+str(x)]) for x in [.05,.2,1])+'|')
(A.OUT/'tables.md').write_text('\n'.join(lines)+'\n',encoding='utf-8');(A.OUT/'temporal.json').write_text(json.dumps(extra,indent=2),encoding='utf-8')
print('Wrote per-run tables and temporal evidence')
