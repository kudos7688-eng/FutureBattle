"""Read-only compact progress and fixed-seed regression check during long T1 runs."""
import json,re,sys
from pathlib import Path
p=Path(sys.argv[1]);base=Path(__file__).parent/'Benchmarks/20261106-003624-diag-armed-500wide-field-results.txt'
def parse(text):
    rows={}
    for line in text.splitlines():
        if 'FIELD_TRIAL mode=' not in line:continue
        r=dict(re.findall(r'(\w+)=([^ ]+)',line.strip()))
        for k in ('fps','fps30','combatFPS','p95ms'):r.pop(k,None)
        rows[int(r['seed']),r['mode']]=r
    return rows
old=parse(base.read_text(encoding='utf-8-sig'))
new=parse((p/'results.txt').read_text(encoding='utf-8-sig')) if (p/'results.txt').exists() else {}
for k,v in new.items():assert v==old[k],(k,v,old[k])
files=list(p.glob('seed*-r*.json'));counts={}
for f in files:
    r=json.loads(f.read_text(encoding='utf-8-sig'));assert r['shots']==r['rays']==r['paths']==0 and r['frames']==900 and r['camera_errors']==0
    counts[r['seed']]=counts.get(r['seed'],0)+1
print(json.dumps(dict(regression_matches=len(new),replay_completed=len(files),per_seed=dict(sorted(counts.items())))))
