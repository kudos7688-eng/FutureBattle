"""Keep performance and instrumented same-world sensory audits strictly separate."""
import csv, json, math, re, statistics, argparse
from pathlib import Path

def distribution(values):
    v=sorted(map(float,values))
    if not v: return dict(n=0,min=None,median=None,p95=None,max=None,mean=None,over_16_67_pct=None)
    return dict(n=len(v),min=v[0],median=statistics.median(v),p95=v[math.ceil(.95*len(v))-1],max=v[-1],mean=statistics.mean(v),over_16_67_pct=100*sum(x>16.67 for x in v)/len(v))

def report(perf,sense,out):
    out.mkdir(parents=True,exist_ok=True)
    results=[]; audits=[]; hashes=set()
    for root,is_audit in [(perf,False),(sense,True)]:
        plan=json.loads((root/'plan.json').read_text(encoding='utf-8-sig'))
        assert plan['status']=='complete'
        manifests=sorted(root.glob('*/manifest.json')); assert len(manifests)==12
        for path in manifests:
            m=json.loads(path.read_text(encoding='utf-8-sig'));hashes.add(m['dll_sha256'])
            assert m['status']=='passed' and m['sense_audit']==is_audit and m['stable'] and not m['nullrhi']
            log=(path.parent/'run.log').read_text(encoding='utf-8-sig')
            assert 'P3_CAMERA PASS' in log and 'P3_RESULT failures=0' in log and 'NVIDIA GeForce RTX 5070 Ti' in log
            assert f"P3_SENSE mode={m['sense']} grid=0 far=0 slices=4 audit={int(is_audit)}" in log
            rows=list(csv.DictReader((path.parent/'frames.csv').open(encoding='utf-8-sig')))
            assert rows and all(r['view']==m['view'] and int(r['alive'])==m['units'] and int(r['rvo_registered'])==m['units'] and float(r['gpu_ms'])>0 for r in rows)
            base=dict(units=m['units'],view=m['view'],mode=m['sense'],folder=path.parent.name)
            if not is_audit:
                timing={k:distribution(r[k] for r in rows) for k in ['gt_ms','rt_ms','gpu_ms','perception_ms','sight_scan_ms','decision_ms','follow_ms','rvo_ms','path_ms','shot_ms','display_ms','report_delivery_ms','report_queue_ms']}
                late=max(float(r['sensor_lateness_max_ms']) for r in rows) if m['sense']=='A' else float(re.search(r'P3_BATCH_LATENESS max_ms=([\d.]+)',log)[1])
                results.append(dict(**base,timing=timing,lateness_max_ms=late,pending_peak=max(int(r['pending_reports_peak']) for r in rows)))
            else:
                events=list(csv.DictReader((path.parent/'frames-sense.csv').open(encoding='utf-8-sig')))
                counts=re.search(r'P3_SENSE_AUDIT frames=(\d+) contact_mismatch=(\d+) timer_mismatch=(\d+) report_mismatch=(\d+)',log)
                assert counts
                d=dict(**base,frames=int(counts[1]),contact_mismatch=int(counts[2]),timer_mismatch=int(counts[3]),report_mismatch=int(counts[4]),distributions={},paired={})
                for mode in ['A','B']:
                    for kind in ['direct','shared']:
                        d['distributions'][f'{mode}_{kind}']=distribution(e['time'] for e in events if e['mode']==mode and e['kind']==kind)
                for kind in ['direct','shared']:
                    maps=[{(e['observer'],e['target']):e for e in events if e['mode']==mode and e['kind']==kind} for mode in ['A','B']]
                    common=maps[0].keys()&maps[1].keys()
                    delta=[float(maps[1][k]['time'])-float(maps[0][k]['time']) for k in common]
                    d['paired'][kind]=dict(common=len(common),only_A=len(maps[0].keys()-maps[1].keys()),only_B=len(maps[1].keys()-maps[0].keys()),same_time=sum(x==0 for x in delta),B_minus_A_seconds=distribution(delta))
                audits.append(d)
    assert len(hashes)==1
    (out/'summary.json').write_text(json.dumps(dict(dll=next(iter(hashes)),performance=results,audits=audits,validation='24 PASS, views/alive/RVO/GPU and effective settings checked'),ensure_ascii=False,indent=2),encoding='utf-8')
    lines=['### 新DLLでの性能再測定（監査OFF）','', '|人数|視点|設定|GT平均 ms|p95 ms|>16.67ms %|認識|視線走査|判断|実行同期|RVO|最大期限遅れ ms|pending peak|','|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|']
    for r in results:
        t=r['timing']; vals=[t['gt_ms']['mean'],t['gt_ms']['p95'],t['gt_ms']['over_16_67_pct']]+[t[k]['mean'] for k in ['perception_ms','sight_scan_ms','decision_ms','follow_ms','rvo_ms']]+[r['lateness_max_ms']]
        lines.append(f"|{r['units']}|{r['view']}|{r['mode']}|"+'|'.join(f'{v:.3f}' for v in vals)+f"|{r['pending_peak']}|")
    lines+=['','### 同じworld入力での感覚監査（性能集計から除外）','','時刻は走行開始からの秒。分布は初回直接発見のobserver-targetペア。駆動はAIへ接続した設定、他方はshadow。','','|人数|視点|駆動|観測|件数|最小|中央値|p95|最大|','|---|---|---|---|---:|---:|---:|---:|---:|']
    for r in audits:
        for mode in ['A','B']:
            d=r['distributions'][mode+'_direct']
            lines.append(f"|{r['units']}|{r['view']}|{r['mode']}|{mode}|{d['n']}|"+'|'.join(f"{d[k]:.6f}" if d[k] is not None else '未観測' for k in ['min','median','p95','max'])+'|')
    lines+=['','|人数|視点|駆動|監査frame|Contact不一致frame|発見タイマー不一致frame|pending不一致frame|直接発見・共通ペア中の同時刻件数|','|---|---|---|---:|---:|---:|---:|---|']
    for r in audits:
        p=r['paired']['direct'];lines.append(f"|{r['units']}|{r['view']}|{r['mode']}|{r['frames']}|{r['contact_mismatch']}|{r['timer_mismatch']}|{r['report_mismatch']}|{p['same_time']}/{p['common']}|")
    lines+=['','共有初回分布・ペア差・片側のみ発見の件数・全スレッド内訳は同フォルダsummary.json。元frames-sense.csvには時刻/位置/sourceも保存。','']
    (out/'tables.md').write_text('\n'.join(lines),encoding='utf-8')
    print('\n'.join(lines[:17]))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('perf',type=Path);p.add_argument('sense',type=Path);p.add_argument('out',type=Path);a=p.parse_args();report(a.perf,a.sense,a.out)
