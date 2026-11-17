"""Compare OFF results and all non-timing diagnostics against the original 20 seeds."""
import argparse, json, re
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BASE = ROOT/'Benchmarks/20261106-003624-diag-armed-500wide-field-results.txt'

def rows(path):
    return [dict(re.findall(r'(\w+)=([^\s]+)', x)) for x in path.read_text(encoding='utf-8-sig').splitlines() if 'FIELD_TRIAL mode=' in x]

def check(folder, expected):
    baseline={(int(x['seed']),x['mode']):x for x in rows(BASE)}
    found=[]
    for path in sorted(folder.glob('seed*-Off.log')):
        row=rows(path)[-1]; key=int(row['seed']),row['mode']; old=baseline[key]
        # NullRHI cannot report WasRecentlyRendered; only rendering/timing fields differ.
        ignore={'fps','fps30','combatFPS','p95ms','drawn'}
        differences={k:(old[k],v) for k,v in row.items() if k not in ignore and old.get(k)!=v}
        assert not differences,(path,differences)
        diag=json.loads(path.with_name(path.stem+'-diagnostics.json').read_text(encoding='utf-8-sig'))
        original=json.loads((BASE.parent/f"{BASE.name.removesuffix('-field-results.txt')}-field-{key[0]}-{key[1]}-diagnostics.json").read_text(encoding='utf-8-sig'))
        for k,v in original.items():
            if k not in {'combat_ms_per_frame','match_ms_per_frame'}: assert diag[k]==v,(path,k,diag[k],v)
        extra=json.loads(path.with_name(path.stem+'-t2b.json').read_text(encoding='utf-8-sig'))
        assert extra['turn_started']==0 and not extra['turn_events']
        assert extra['angular_speed_violations']==0 and extra['shots_during_turn']==0
        assert extra['combat_window_frames']==900
        found.append({'seed':key[0],'mode':key[1],'match':True})
    assert len(found)==expected,(len(found),expected)
    report={'baseline':str(BASE),'matches':len(found),'ignored_result_fields':sorted(ignore),'rows':found}
    (folder/'regression.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(f'{len(found)}/{expected} OFF battles and non-timing diagnostics exactly match')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('folder',type=Path);p.add_argument('--expected',type=int,default=40);a=p.parse_args();check(a.folder,a.expected)
