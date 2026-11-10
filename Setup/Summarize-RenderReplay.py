"""T1 paired, seed-level bootstrap. Repetitions averaged before resampling seeds."""
import argparse,csv,json,random,re,statistics,struct,hashlib
from pathlib import Path
METRICS=['fps','frame_ms','p95_ms','game_thread','render_thread','rhi_thread','gpu','display_sync','mean_alive']
FEATURES=['AllOn','NoFar','NoShadow','SlowSync','AllOff']

def writecsv(path,rows):
    with path.open('w',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)

def main(folder,expected,baseline):
    raw=[]
    for p in folder.glob('seed*-r*.json'):
        r=json.loads(p.read_text(encoding='utf-8-sig'));r['run']=int(re.search(r'-r(\d+)',p.stem)[1]);raw.append(r)
    seeds=sorted({r['seed'] for r in raw});assert len(seeds)==expected,(len(seeds),expected)
    groups={}
    for r in raw:
        assert r['frames']==900 and r['shots']==r['rays']==r['paths']==0 and r['camera_errors']==0
        groups.setdefault((r['seed'],r['arm'],r['feature']),[]).append(r)
    assert len(raw)==expected*32 and len(groups)==expected*16
    means={};table=[]
    for (seed,arm,feature),rows in sorted(groups.items()):
        assert sorted(r['run'] for r in rows)==[1,2]
        m={k:statistics.mean(r[k] for r in rows) for k in METRICS}
        m.update(added_unit_frames=rows[0]['added_unit_frames'],removed_unit_frames=rows[0]['removed_unit_frames'])
        means[seed,arm,feature]=m;table.append(dict(seed=seed,arm=arm,feature=feature,**m))
    for seed in seeds:
        for feature in FEATURES:
            assert means[seed,'A',feature]['mean_alive']==means[seed,'C',feature]['mean_alive']
    # Shared bootstrap draw indices make all contrast intervals reproducible.
    rng=random.Random(20261106);n=len(seeds)
    draws=[[rng.randrange(n) for _ in seeds] for _ in range(20000)]
    def estimate(values):
        bs=sorted(sum(values[i] for i in draw)/n for draw in draws)
        return statistics.mean(values),bs[499],bs[19499]
    contrasts=[]
    def contrast(name,feature,metric,values):
        avg,lo,hi=estimate(values)
        contrasts.append(dict(contrast=name,feature=feature,metric=metric,mean=avg,ci95_low=lo,ci95_high=hi))
    for feature in FEATURES:
        for left,right in [('B','A'),('C','A'),('B','C')]:
            for metric in METRICS:
                contrast(left+'-'+right,feature,metric,[means[s,left,feature][metric]-means[s,right,feature][metric] for s in seeds])
    for metric in METRICS:
        contrast('D-A','AllOn',metric,[means[s,'D','AllOn'][metric]-means[s,'A','AllOn'][metric] for s in seeds])
    for feature in FEATURES[1:]:
        for arm in 'ABC':
            for metric in METRICS:
                contrast(arm+':feature-AllOn',feature,metric,[means[s,arm,feature][metric]-means[s,arm,'AllOn'][metric] for s in seeds])
        for metric in METRICS:
            contrast('(B-A):feature-AllOn',feature,metric,[(means[s,'B',feature][metric]-means[s,'A',feature][metric])-(means[s,'B','AllOn'][metric]-means[s,'A','AllOn'][metric]) for s in seeds])
    checks=[]
    for p in sorted(folder.glob('*.fbr')):
        with p.open('rb') as f:h=struct.unpack('<8i',f.read(32))
        assert p.stat().st_size==32+h[3]*(28+500*32) and h[2]==500 and h[7]==30
        checks.append(dict(file=p.name,frames=h[3],first_shot=h[4],sha256=hashlib.file_digest(p.open('rb'),'sha256').hexdigest()))
    regressions=[]
    if baseline:
        def results(text):
            found={}
            for line in text.splitlines():
                if 'FIELD_TRIAL mode=' not in line:continue
                fields=dict(re.findall(r'(\w+)=([^ ]+)',line.strip()))
                for key in ['fps','fps30','combatFPS','p95ms']:fields.pop(key,None)
                found[int(fields['seed']),fields['mode']]=fields
            return found
        old=results(baseline.read_text(encoding='utf-8-sig'));new=results((folder/'results.txt').read_text(encoding='utf-8-sig'))
        for key,fields in new.items():
            assert fields==old[key],(key,fields,old[key]);regressions.append(dict(seed=key[0],mode=key[1],identical=True))
        assert len(regressions)==2*expected
    writecsv(folder/'runs.csv',[{k:r[k] for k in ['seed','arm','feature','run']+METRICS+['added_unit_frames','removed_unit_frames','camera_errors']} for r in raw])
    writecsv(folder/'seed-means.csv',table);writecsv(folder/'contrasts.csv',contrasts)
    conditions=[]
    for arm,features in [('A',FEATURES),('B',FEATURES),('C',FEATURES),('D',['AllOn'])]:
        for feature in features:
            conditions.append(dict(arm=arm,feature=feature,**{m:statistics.mean(means[s,arm,feature][m] for s in seeds) for m in METRICS}))
    writecsv(folder/'condition-means.csv',conditions)
    repeatability=[]
    for metric in METRICS:
        differences=[abs(rows[0][metric]-rows[1][metric]) for rows in groups.values()]
        repeatability.append(dict(metric=metric,median_absolute_difference=statistics.median(differences),max_absolute_difference=max(differences)))
    summary=dict(seeds=seeds,repetitions=2,conditions=conditions,contrasts=contrasts,recordings=checks,regressions=regressions,repeatability=repeatability,
                 caveats=['C matches team/representation counts using fixed hash rank; B positions/orientations remain. Shortages restored at last B positions; added/removed unit-frames reported.',
                          'C-A residual is spatial/display distribution, not proof of sensing algorithm overhead. Replay has no sensing/AI/shots.',
                          'D repeats A through identical replay path as null control.',
                          'NoFar hides soldier mesh plus weapon beyond 150m; overhead camera is 300m, so all soldiers qualify. Environment remains.',
                          'SlowSync changes Mass transform frequency from 30 to 7.5Hz; visibility/death/LOD remain 30Hz.',
                          'LOD is recorded predicted LOD and forced during replay. Animation uses fixed 30Hz clock. Recording performance excluded.',
                          'Timing APIs overlap; do not sum. 95% intervals resample seeds after averaging repetitions; exploratory, no multiplicity adjustment.'])
    (folder/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    print(json.dumps(dict(conditions=conditions,regressions=len(regressions)),indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder',type=Path);p.add_argument('--expected',type=int,default=20);p.add_argument('--baseline',type=Path)
    a=p.parse_args();main(a.folder,a.expected,a.baseline)
