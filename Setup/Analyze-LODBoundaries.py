"""Check C-stage per-unit transitions, protected Actors, and boundary stress traces."""
import csv, json, statistics, sys
from collections import defaultdict, deque
from pathlib import Path

p = Path(sys.argv[1]); runs=[]; segments=[]; events=[]
for f in sorted(p.glob('seed*-C-*-r*.json')):
    d=json.loads(f.read_text(encoding='utf-8-sig')); ss=d['samples']; actors=d['actor_indices']; probe=d['lod_probe_index']
    assert len(ss)==900 and len(actors)==50
    assert d['camera_errors']==d['diagnostic_errors']==d['visibility_errors']==d['visibility_missing']==0
    totals=[0]*500; window=deque(); rolling=[0]*500; peak=0; changes=0
    for i,s in enumerate(ss):
        codes=s['lod_codes'];assert len(codes)==500 and set(codes)<=set('012')
        if d['filter']!='Reference':assert all(codes[a]=='0' for a in actors),(f,i)
        if i:
            changed=[j for j,(a,b) in enumerate(zip(ss[i-1]['lod_codes'],codes)) if a!=b]
            assert len(changed)==s['lod_changes'],(f,i,len(changed),s['lod_changes'])
            changes+=len(changed)
            for j in changed:totals[j]+=1;rolling[j]+=1
            window.append(changed)
            if len(window)>30:
                for j in window.popleft():rolling[j]-=1
            peak=max(peak,max(rolling))
        if s['lod_changes']:
            events.append(dict(seed=d['seed'],profile=d['profile'],view=d['view'],filter=d['filter'],run=d['run'],
                frame=i,changes=s['lod_changes'],shown_changes=s['lod_changes_shown'],frame_ms=s['frame_ms'],gpu=s['gpu'],lod_set_ms=s['lod_set_ms']))
    assert all(int(s['probe_lod'])==int(s['lod_codes'][probe]) for s in ss)
    visible=sum((int.from_bytes(bytes.fromhex(s['original_drawn_bits']),'little')>>probe)&1 for s in ss)
    row={k:d[k] for k in ['seed','profile','view','filter','run','lod_policy','lod_stress']}
    row.update(changes=changes,changes_per_second=changes/30.,shown_changes_per_second=sum(s['lod_changes_shown'] for s in ss[1:])/30.,
        max_unit_switches_in_one_second=peak,max_unit_switches_total=max(totals),probe_submitted_frames=visible,
        gpu=statistics.mean(s['gpu'] for s in ss),frame_ms=statistics.mean(s['frame_ms'] for s in ss),
        frame_p95_ms=sorted(s['frame_ms'] for s in ss)[854],frame_max_ms=max(s['frame_ms'] for s in ss),
        lod_set_ms=statistics.mean(s['lod_set_ms'] for s in ss),
        changed_setter_calls=sum(s['lod_set_changed_calls'] for s in ss),
        changed_setter_total_ms=sum(s['lod_set_changed_ms'] for s in ss),
        predicted_mismatch_max=max(s['predicted_lod_mismatches'] for s in ss))
    runs.append(row)
    if d['lod_stress']:
        for segment in range(6):
            begin,end=segment*150,(segment+1)*150
            n=sum(ss[i]['probe_lod']!=ss[i-1]['probe_lod'] for i in range(begin+1,end))
            segments.append({**{k:row[k] for k in ['seed','profile','view','filter','run']},
                'boundary':1 if segment<3 else 2,'amplitude_percent':[5,15,25][segment%3],
                'probe_switches':n,'probe_switches_per_second':n/5.,
                'gpu':statistics.mean(s['gpu'] for s in ss[begin:end])})
assert runs
def write(name,rows):
    if rows:
        with (p/name).open('w',encoding='utf-8-sig',newline='') as out:
            w=csv.DictWriter(out,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
write('boundary-runs.csv',runs);write('boundary-segments.csv',segments);write('lod-transition-events.csv',events)
groups=defaultdict(list)
for r in runs:groups[r['profile'],r['view'],r['filter']].append(r)
lines=['# C：LOD切替境界の検証','',f"{len(runs)}走行。方針 {runs[0]['lod_policy']}。境界ストレス={runs[0]['lod_stress']}。",
    'ReferenceはBの方式、H0以降は操作可能Actorを常にLOD0とし、Massの切替幅を0/10/20/30%にする。', '',
    '|設定|視点|条件|全兵切替/秒|画面表示対象の切替/秒|1兵の最大切替/秒|GPU ms|frame p95 ms|LOD setter ms/frame|',
    '|---|---|---|---:|---:|---:|---:|---:|---:|']
for (profile,view,filt),rr in sorted(groups.items()):
    mean=lambda k:statistics.mean(r[k] for r in rr)
    lines.append(f"|{profile}|{view}|{filt}|{mean('changes_per_second'):.2f}|{mean('shown_changes_per_second'):.2f}|{max(r['max_unit_switches_in_one_second'] for r in rr)}|{mean('gpu'):.3f}|{mean('frame_p95_ms'):.3f}|{mean('lod_set_ms'):.5f}|")
lines+=['','|設定|視点|条件|実変更呼出し数|変更setter合計 ms|1変更平均 µs|','|---|---|---|---:|---:|---:|']
for (profile,view,filt),rr in sorted(groups.items()):
    calls=sum(r['changed_setter_calls'] for r in rr); elapsed=sum(r['changed_setter_total_ms'] for r in rr)
    lines.append(f"|{profile}|{view}|{filt}|{calls}|{elapsed:.4f}|{elapsed*1000/calls if calls else 0:.4f}|")
if segments:
    lines+=['','ストレスは各境界を1Hz・±5/15/25%で5秒ずつ往復する。区間間のジャンプはプローブ切替頻度から除く。','',
        '|条件|振幅|プローブ切替/秒（両境界・視点・反復平均）|','|---|---:|---:|']
    for filt in ['Reference','H0','H10','H20','H30']:
        for amplitude in [5,15,25]:
            rr=[r for r in segments if r['filter']==filt and r['amplitude_percent']==amplitude]
            if not rr:continue
            lines.append(f"|{filt}|{amplitude}%|{statistics.mean(r['probe_switches_per_second'] for r in rr):.2f}|")
lines+=['','切替/秒は記録の時間軸（30フレーム=1秒）に正規化した値。ベンチマークの実時間の秒数ではない。ストレスのTPS/FPSは同じプローブカメラに置き換え、FPSだけ元の操作兵の非表示規則が残る。',
    '全フレームで操作可能ActorのLOD0を検査。各兵のLODコードと切替カウンタを照合。',
    'GPU/frame差にはLOD構成の違いが含まれ、切替処理単独の費用とは解釈しない。setter時間はCPUの直接呼出し範囲だけで、後続の描画処理を含まない。',
    'ヒステリシスは往復切替を減らす。単発の輪郭変化を補間する処理は追加しておらず、ポップイン完全解消の証明ではない。',
    '自然移動とストレスは別の出力として比較する。ストレス視点は通常TPS/FPSカメラを置き換えたプローブ用で、通常視点の費用へ外挿しない。','']
(p/'boundary-report.md').write_text('\n'.join(lines),encoding='utf-8')
print('Boundary validation:',len(runs),'runs; protected Actor LOD0 and transition counters verified')
