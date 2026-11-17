"""T5 A-stage: repeated runs within paired placement seeds, never independent frames."""
import argparse,csv,hashlib,json,random,statistics
from pathlib import Path

BASE=['frame_ms','game_thread','render_thread','rhi_thread','gpu','display_sync','draw_calls','draw_primitives','actor_shown','mass_shown','actor_frustum','mass_frustum','actor_drawn','mass_drawn','actor_body_triangles','mass_body_triangles','visibility_observer_ms','potential_frustum','registered_bodies','extra_visible','added_inside','missing_potential','registrations','wakeups','visibility_changes','updated_bodies','population_floor_excess']
KEYS=['stage','profile','view','filter']
BOOT={}
def interval(v):
 n=len(v)
 if n<2:return v[0],None,None
 if min(v)==max(v):return v[0],v[0],v[0]
 if n not in BOOT:
  r=random.Random(20261106);BOOT[n]=[tuple(r.randrange(n) for _ in range(n)) for _ in range(20000)]
 a=sorted(sum(v[i] for i in t)/n for t in BOOT[n]);return statistics.mean(v),a[499],a[19499]
def write(p,name,rows):
 if not rows:return
 with (p/name).open('w',encoding='utf-8-sig',newline='') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
def main(p,pilot,additional=(),output=None):
 folders=[p,*additional];files=[];manifests=[]
 for folder in folders:
  m=json.loads((folder/'manifest.json').read_text(encoding='utf-8-sig'));assert m['status']=='complete' and m['machine'],folder
  assert hashlib.sha256((folder/'UnrealEditor-FutureBattle.dll').read_bytes()).hexdigest()==m['dll_sha256'].lower(),folder
  fs=list(folder.glob('seed*-r*.json'));assert len(fs)==sum(r['conditions'] for r in m['runs']),folder
  files+=fs;manifests.append(m)
 assert len({m['dll_sha256'] for m in manifests})==1
 p=output or p;p.mkdir(exist_ok=True,parents=True)
 runs=[];settings={};checks={'runs':0,'frames':0,'camera_errors':0,'diagnostic_errors':0};reference={};events=[];masks={}
 for f in sorted(files):
  d=json.loads(f.read_text(encoding='utf-8-sig'));assert pilot or not d['visual_qa_only'],f
  assert d['frames']==900 and d['camera_errors']==d['diagnostic_errors']==d['shots']==d['rays']==d['paths']==d['visibility_missing']==d['visibility_errors']==0,f
  ss=d['samples'];assert len(ss)==900
  row={k:d[k] for k in ['seed',*KEYS,'run']}
  for metric in BASE:row[metric]=statistics.mean(s[metric] for s in ss)
  for metric in ['tick_enabled_bodies','proxy_seen_enemies','marker_count','marker_max_age_frames','hidden_shadow_bodies','lod_changes','lod_changes_shown','requested_lod0','requested_lod1','requested_lod2','predicted_lod_mismatches','lod_set_ms','probe_lod','probe_distance_cm','shadow_changes','material_changes','shadow_suppressed_bodies','simplified_material_bodies']:
   if metric in ss[0]:row[metric]=statistics.mean(s[metric] for s in ss)
  if 'original_frustum_bits' in ss[0] and d['stage'] in ['A1','A3','Shadow','B']:
   masks[d['seed'],d['stage'],d['profile'],d['view'],d['filter'],d['run']]=[(int.from_bytes(bytes.fromhex(s['original_frustum_bits']),'little'),int.from_bytes(bytes.fromhex(s['original_drawn_bits']),'little'),s['gpu'],s['render_thread']) for s in ss]
  for metric in ['shown','frustum','drawn','body_triangles']:row[metric]=row['actor_'+metric]+row['mass_'+metric]
  row['fps']=1000/row['frame_ms'];row['frame_p95_ms']=sorted(s['frame_ms'] for s in ss)[854];row['frame_max_ms']=max(s['frame_ms'] for s in ss)
  row['population_floor_excess_max']=max(s['population_floor_excess'] for s in ss)
  row['population_floor_excess_frames']=sum(s['population_floor_excess']>0 for s in ss)
  row['registrations_total']=sum(s['registrations'] for s in ss);row['wakeups_total']=sum(s['wakeups'] for s in ss)
  for s in ss:
   assert s['actor_drawn']<=s['actor_frustum'] and s['mass_drawn']<=s['mass_frustum'],f
   assert s.get('transform_errors',0)==0,f
   assert 0<=s.get('marker_max_age_frames',0)<=300,f
   if d['stage']=='A1':
    target=int(d['filter'][5:]);expected=target+s['population_floor_excess'];assert s['registered_bodies']==expected,(f,s['frame'],s['registered_bodies'],expected)
    if d['view']!='Recorded' and target:
     assert s['added_inside']==0,f
     if 'missing_required' in s:
      assert s['missing_required']==0 and s['kept_required_hash']==s['required_original_hash'],f
     else:
      assert s['missing_potential']==0,f
      if 'kept_hash' in s:assert s['kept_hash']==s['potential_hash'],f
   if d['filter'].endswith('Sleep'):assert s['updated_bodies']==s['actor_shown']+s['mass_shown'],f
   if d['filter'] in ['None','Total0','Bias1None']:assert s['actor_drawn']==s['mass_drawn']==0,f
   k=(d['seed'],d['view'],s['frame'])
   if k in reference:assert reference[k]==s['potential_hash'],(f,k)
   else:reference[k]=s['potential_hash']
   if s['registrations'] or s['wakeups']:
    events.append({**{k:d[k] for k in ['seed',*KEYS,'run']},'frame':s['frame'],'registrations':s['registrations'],'wakeups':s['wakeups'],'frame_ms':s['frame_ms']})
  if d['profile']!='Current':assert not d['settings']['rhi_supports_ray_tracing'] and not d['settings']['ray_tracing_enabled'],f
  if d['profile'] in ['Light','ClassicShadow','NoShadow']:assert d['settings']['r.DynamicGlobalIlluminationMethod']==d['settings']['r.Lumen.DiffuseIndirect.Allow']==d['settings']['r.Lumen.Reflections.Allow']==0,f
  if d['profile'] in settings:assert settings[d['profile']]==d['settings'],f
  else:settings[d['profile']]=d['settings']
  assert row['gpu']>0;checks['runs']+=1;checks['frames']+=900;runs.append(row)
 write(p,'runs.csv',runs);write(p,'transition-events.csv',events)
 maskrows=[]
 for (seed,stage,profile,view,fil,run),ss in masks.items():
  base='Total500' if stage=='A1' else 'All';ref=masks.get((seed,stage,profile,view,base,run))
  if not ref or base==fil:continue
  row={'seed':seed,'stage':stage,'profile':profile,'view':view,'filter':fil,'run':run,'reference':base}
  for j,name in [(0,'frustum'),(1,'drawn')]:
   missing=[b[j]&~a[j] for a,b in zip(ss,ref)];added=[a[j]&~b[j] for a,b in zip(ss,ref)]
   row[name+'_identical_frames']=sum(x==y==0 for x,y in zip(missing,added));row[name+'_missing_soldier_frames']=sum(x.bit_count() for x in missing);row[name+'_extra_soldier_frames']=sum(x.bit_count() for x in added)
   row[name+'_max_missing_in_frame']=max(x.bit_count() for x in missing)
   if name=='drawn':
    streak={};longest=0
    for bits in missing:
     now={}
     while bits:
      bit=bits&-bits;bits^=bit;now[bit]=streak.get(bit,0)+1;longest=max(longest,now[bit])
     streak=now
    row['max_consecutive_missing_drawn_frames']=longest
  exact=[a[0:2]==b[0:2] for a,b in zip(ss,ref)];ii=[i for i in range(2,len(ss)) if all(exact[i-2:i+1])]
  row['exact_both_three_consecutive_frames']=len(ii)
  for j,n in [(2,'gpu'),(3,'render_thread')]:row['matched_subset_'+n+'_delta']=statistics.mean(ss[i][j]-ref[i][j] for i in ii) if ii else ''
  maskrows.append(row)
 write(p,'original-screen-set-checks.csv',maskrows)
 (p/'effective-settings.json').write_text(json.dumps(settings,indent=2),encoding='utf-8')
 (p/'validation.json').write_text(json.dumps(checks,indent=2),encoding='utf-8')
 metrics=[k for k in runs[0] if k not in ['seed',*KEYS,'run']]
 grouped={}
 for r in runs:grouped.setdefault(tuple(r[k] for k in ['seed',*KEYS]),[]).append(r)
 if not pilot:
  expected=set()
  for manifest in manifests:
   for seed in manifest['seeds']:
    for stage in manifest['stages']:
     views,filters,profiles={
      'D':(['Recorded','RTS60','TPS','FPS'],['All','None','Shadow30','Shadow60','Material40','Combined','Bias1','Bias1None'],['Current','Light']),
      'C':(['TPS','FPS'],['Reference','H0','H10','H20','H30'],['Current','Light']),
      'B':(['Recorded','RTS60','TPS','FPS'],['All','None','LOD0','LOD1','LOD2','Dist10_30','Dist20_50','Screen120_48'],['Current','Light']),
      'Shadow':(['TPS','FPS'],['All','FrustumHide','FrustumShadow'],['Current','Light']),
      'A1':(['Recorded','TPS','FPS'],['Total0','Total250','Total500','Total1000'],['Current','Light']),
      'A2':(['Recorded','RTS60','TPS','FPS'],['All','None'],['Current','Software','Light','ClassicShadow','NoShadow','Medium','Low']),
      'A3':(['TPS','FPS'],['All','None','FrustumHide','FrustumSleep','DistanceHide','DistanceSleep','KnowledgeProxy'],['Current','Light'])}[stage]
     filters=manifest.get('filters_override') or filters
     for profile in manifest['profiles_override'] or profiles:
      expected.update((seed,stage,profile,view,filt) for view in views for filt in filters)
  assert set(grouped)==expected,('missing/extra cells',expected-set(grouped),set(grouped)-expected)
 seedrows=[]
 for key,rr in grouped.items():
  assert pilot or sorted(r['run'] for r in rr)==[1,2],(key,len(rr))
  seedrows.append({**dict(zip(['seed',*KEYS],key)),**{m:statistics.mean(r[m] for r in rr) for m in metrics}})
 write(p,'seed-metrics.csv',seedrows)
 means=[]
 for key in sorted({tuple(r[k] for k in KEYS) for r in seedrows}):
  rr=[r for r in seedrows if tuple(r[k] for k in KEYS)==key]
  means.append({**dict(zip(KEYS,key)),'seeds':len(rr),**{m:statistics.mean(r[m] for r in rr) for m in metrics}})
 write(p,'condition-means.csv',means)
 if pilot:print(json.dumps(checks));return
 idx={(r['seed'],*tuple(r[k] for k in KEYS)):r for r in seedrows};ids=sorted({r['seed'] for r in seedrows});contrasts=[];paired=[]
 def compare(label,terms,denominator=None):
  use=[s for s in ids if all((s,*k) in idx for _,k in terms)]
  if not use:return
  for m in (['gpu','render_thread'] if denominator else metrics):
   vals=[]
   for s in use:
    div=idx[(s,*denominator[0])][denominator[1]] if denominator else 1
    if div<=0:continue
    v=sum(sign*idx[(s,*k)][m] for sign,k in terms)/div;vals.append(v);paired.append({'comparison':label,'seed':s,'metric':m,'difference':v})
   if vals:
    mean,lo,hi=interval(vals);contrasts.append({'comparison':label,'metric':m,'difference':mean,'ci_low':lo,'ci_high':hi,'seeds':len(vals)})
 profiles=sorted(settings)
 for profile in profiles:
  for view in ['Recorded','RTS60','TPS','FPS']:
   a=('A2',profile,view,'All');b=('A2',profile,view,'None')
   compare(f'A2:{profile}:{view}:All-None',[(1,a),(-1,b)])
   for denom in ['shown','drawn']:compare(f'A2:{profile}:{view}:per_{denom}',[(1,a),(-1,b)],(a,denom))
   before={'Software':'Current','Light':'Software','ClassicShadow':'Light','NoShadow':'Light','Medium':'Current','Low':'Current'}.get(profile)
   if before:
    c=('A2',before,view,'All');e=('A2',before,view,'None')
    compare(f'A2:{profile}-{before}:{view}:soldier_increment',[(1,a),(-1,b),(-1,c),(1,e)])
    compare(f'A2:{profile}-{before}:{view}:All',[(1,a),(-1,c)])
  for view in ['Recorded','TPS','FPS']:
   for total in [0,250,1000]:compare(f'A1:{profile}:{view}:{total}-500',[(1,('A1',profile,view,f'Total{total}')),(-1,('A1',profile,view,'Total500'))])
  for view in ['TPS','FPS']:
   for a,b in [('FrustumHide','All'),('FrustumSleep','All'),('FrustumSleep','FrustumHide'),('DistanceHide','FrustumHide'),('DistanceSleep','DistanceHide'),('KnowledgeProxy','All'),('None','All')]:
    compare(f'A3:{profile}:{view}:{a}-{b}',[(1,('A3',profile,view,a)),(-1,('A3',profile,view,b))])
 for profile in profiles:
  for view in ['TPS','FPS']:
   for a,b in [('FrustumHide','All'),('FrustumShadow','All'),('FrustumShadow','FrustumHide')]:
    compare(f'Shadow:{profile}:{view}:{a}-{b}',[(1,('Shadow',profile,view,a)),(-1,('Shadow',profile,view,b))])
 for profile in profiles:
  for view in ['Recorded','RTS60','TPS','FPS']:
   for filt in ['All','LOD0','LOD1','LOD2','Dist10_30','Dist20_50','Screen120_48']:
    a=('B',profile,view,filt);b=('B',profile,view,'None');ref=('B',profile,view,'All')
    compare(f'B:{profile}:{view}:{filt}-None',[(1,a),(-1,b)])
    compare(f'B:{profile}:{view}:{filt}-All',[(1,a),(-1,ref)])
    compare(f'B:{profile}:{view}:{filt}:per_drawn',[(1,a),(-1,b)],(a,'drawn'))
 for profile in profiles:
  for view in ['TPS','FPS']:
   for a,b in [('H0','Reference'),('H10','H0'),('H20','H0'),('H30','H0')]:
    compare(f'C:{profile}:{view}:{a}-{b}',[(1,('C',profile,view,a)),(-1,('C',profile,view,b))])
 for profile in profiles:
  for view in ['Recorded','RTS60','TPS','FPS']:
   for filt in ['All','Shadow30','Shadow60','Material40','Combined']:
    a=('D',profile,view,filt);b=('D',profile,view,'None');ref=('D',profile,view,'All')
    compare(f'D:{profile}:{view}:{filt}-None',[(1,a),(-1,b)])
    compare(f'D:{profile}:{view}:{filt}-All',[(1,a),(-1,ref)])
   compare(f'D:{profile}:{view}:Bias1-soldier-increment',[(1,('D',profile,view,'Bias1')),(-1,('D',profile,view,'Bias1None')),(-1,('D',profile,view,'All')),(1,('D',profile,view,'None'))])
 write(p,'contrasts.csv',contrasts);write(p,'paired-differences.csv',paired)
 summary={'seeds':ids,'runs':len(runs),'raw_folders':[str(f.resolve()) for f in folders],'dll_sha256':manifests[0]['dll_sha256'],'conditions':means,'contrasts':contrasts,'settings':settings,'bootstrap':{'draws':20000,'unit':'paired placement seed, average repetitions within seed','interval':'percentile 95%, no multiplicity correction'}}
 (p/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
 print(json.dumps(checks))
 for r in means:print(*(r[k] for k in KEYS),'GPU',round(r['gpu'],3),'RT',round(r['render_thread'],3),'drawn',round(r['drawn'],1))
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('folder',type=Path);a.add_argument('--pilot',action='store_true');a.add_argument('--additional',type=Path,action='append',default=[]);a.add_argument('--output',type=Path);args=a.parse_args();main(args.folder,args.pilot,args.additional,args.output)
