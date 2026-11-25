"""Stalls of moving soldiers by cause and place (reads results only; no build, no lock).

Usage (from Setup/Benchmarks/SightTrial-20261111): python -I ../../Analyze-Stalls.py <result folder> [...]
A stall is one BATTLE_MOVE_STALL record: a soldier with a move order, not yet arrived, who moved less than 40 cm in 3 s.
Soldiers who have arrived, hold a place or stopped to fight are not recorded.

Cause, in this order (user 2026-11-25: "trying to move and cannot" is the problem, "waiting in place" is not):
  wall    the straight line to his next route point is blocked by the world (clear=0); "first" = the first point of the route
          (a member cut off from the squad path by a wall)
  queue   he holds back by the queue rule behind an ally (BATTLE_MOVE_STALL_WHY scale < 0.15): waiting in line, not stuck
  pushed  he walks on (scale >= 0.15) and does not get on: against bodies (allies within 1.5 m) or alone
Builds before BATTLE_MOVE_STALL_WHY have no scale: those stalls are sorted by the allies within 1.5 m only ("no-why").
BATTLE_MOVE_STALL_END gives how long a stall lasted; a stall with no END lasted to the end of the match or of the record.
Places: 5 m cells, the east half turned onto the west (maps built by 180-degree rotation)."""
import re,sys,collections,math
ps=re.compile(r'BATTLE_MOVE_STALL unit=(\d+) team=(\d) squad=(\d+) pos=X=([-\d.]+) Y=([-\d.]+) Z=([-\d.]+) waypoint=X=([-\d.]+) Y=([-\d.]+)')
pd=re.compile(r'BATTLE_MOVE_STALL_DETAIL unit=(\d+) squad=(\d+) order=(\d+) action=(\d+) sector=(\d) final=(\d) wp=(\d+)/(\d+) goal=(\S+ \S+ \S+|-) clear=(\d) own=(\d+) team=(\d+) enemy=(\d+) still=(\d+) cover=(\d)')
pw=re.compile(r'BATTLE_MOVE_STALL_WHY unit=(\d+) scale=([\d.]+) blocker=(-?\d+)')
pe=re.compile(r'BATTLE_MOVE_STALL_END unit=(\d+) seconds=([\d.]+) moved=(\d)')
tot=collections.Counter();spots=collections.Counter();spotcat=collections.defaultdict(collections.Counter);n=0
secs=collections.defaultdict(list);opened=collections.Counter()
for r in sys.argv[1:]:
    last=None;rec=None;cur={}
    def close(rec):
        global n
        c=rec['c'];tot[c]+=1;n+=1;spots[rec['k']]+=1;spotcat[rec['k']][c.split()[0]]+=1
    pend=None
    for l in open(r+'/run.log',encoding='utf-8',errors='replace'):
        if 'BATTLE_MOVE_STALL' not in l: continue
        m=pe.search(l)
        if m:
            u=int(m.group(1))
            if u in cur: secs[cur.pop(u)].append(float(m.group(2)))
            continue
        m=pw.search(l)
        if m:
            if pend and pend['u']==int(m.group(1)):
                sc=float(m.group(2))
                if not pend['c'].startswith('wall'):
                    pend['c']=('queue  holding back behind an ally (waiting in line)' if sc<.15 else ('pushed against bodies (allies within 1.5m)' if pend['al'] else 'pushed alone, way clear'))
            continue
        m=ps.search(l)
        if m and 'DETAIL' not in l: last=m;continue
        m=pd.search(l)
        if m and last and last.group(1)==m.group(1):
            if pend: close(pend);cur[pend['u']]=pend['c']
            x,y=float(last.group(4)),float(last.group(5))
            wp,clear=int(m.group(7)),int(m.group(10));al=int(m.group(11))+int(m.group(12))
            if clear==0 and wp==1: c='wall   ahead, first route point (cut off from the squad path)'
            elif clear==0: c='wall   ahead, later route point'
            else: c='no-why %s'%('3+ allies within 1.5m' if al>=3 else '1-2 allies within 1.5m' if al else 'alone')
            fx,fy=(x,y) if x<0 else (-x,-y)
            pend=dict(u=int(m.group(1)),c=c,al=al,k=(round(fx/500)*5,round(fy/500)*5));last=None
    if pend: close(pend);cur[pend['u']]=pend['c']
    for u,c in cur.items(): opened[c]+=1
print('matches',len(sys.argv)-1,'stalls',n)
print('  %-64s %5s %5s  %s'%('cause','count','share','lasted: median / 10s+ / 30s+ / never ended'))
for c in sorted(tot):
    v=sorted(secs[c]);med=('%.0fs'%v[len(v)//2]) if v else '-'
    print('  %-64s %5d %4.0f%%  %s / %d / %d / %d'%(c,tot[c],100*tot[c]/max(1,n),med,sum(1 for x in v if x>=10),sum(1 for x in v if x>=30),opened[c]))
print('places with most stalls: cell(m) count {cause: count}')
for k,v in spots.most_common(12):
    print('  (%5d,%5d) %4d  %s'%(k[0],k[1],v,dict(sorted(spotcat[k].items()))))
top=sum(v for k,v in spots.most_common(20));print('top 20 cells hold %d%% of the stalls; cells with stalls: %d'%(100*top/max(1,n),len(spots)))
