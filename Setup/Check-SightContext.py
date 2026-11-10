import csv,json,sys,re
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')

def audit(p):
    log=(p/'run.log').read_text(encoding='utf-8-sig')
    m=re.search(r'P3_CONTEXT_AUDIT frames=(\d+) contacts=(\d+) timers=(\d+) reports=(\d+) direct=(\d+)/(\d+) shared=(\d+)/(\d+)',log)
    assert m, 'Missing context audit'
    nums=list(map(int,m.groups()))
    assert nums[0]>0 and nums[1:4]==[0,0,0], nums
    rows=list(csv.DictReader(open(p/'frames-sense.csv',encoding='utf-8-sig')))
    off=[{k:v for k,v in r.items() if k!='mode'} for r in rows if r['mode']=='OFF']
    on=[{k:v for k,v in r.items() if k!='mode'} for r in rows if r['mode']=='ON']
    assert off==on, 'First discovery/sharing event sequence mismatch'
    result={'audit_frames':nums[0],'mismatch_frames':nums[1:4],'direct':nums[4:6],'shared':nums[6:8],'first_event_sequence_equal':True}
    (p/'context-validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(p.name,result)
    return result

paths=[Path(x) for x in sys.argv[1:]]
for p in paths:
    if json.load(open(p/'manifest.json',encoding='utf-8-sig'))['context_audit']: audit(p)
if len(paths)==2:
    a,b=[list(csv.DictReader(open(p/'frames-context-state.csv',encoding='utf-8-sig'))) for p in paths]
    mismatch=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),None)
    result={'frames_off':len(a),'frames_on':len(b),'first_mismatch':mismatch,'equal':a==b,'shooting_exercised':int(a[-1]['shots'])>0,
        'final_off':a[-1],'final_on':b[-1]}
    (paths[1]/'replay-comparison.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    assert a==b, result
    m=json.load(open(paths[0]/'manifest.json',encoding='utf-8-sig'))
    assert len(a)>=(m['warmup']+m['duration'])*30-3, 'Replay must cover the complete fixed-step window'
    if 'replay-Open-' in paths[0].name or 'replay1000-' in paths[0].name: assert int(a[-1]['shots'])>0, 'Positive control must exercise shooting'
    print('FULL_STATE_REPLAY_MATCH',len(a),'shots',a[-1]['shots'],'deaths',a[-1]['deaths'])
