"""Home supply trips that stall at the first route point with a wall ahead (reads results only; no build, no lock).

Usage (from Setup/Benchmarks/SightTrial-20261111): python -I ../../Analyze-HomeTripStalls.py <result folder> [...]
A trip runs from BATTLE_ST2_TRANSITION reason=HOME SUPPLY to the squad's next other transition (or the end of the match).
Stalls are BATTLE_MOVE_STALL_DETAIL lines with order=1 action=6 wp=1/N clear=0. Leader positions (BATTLE_ST_RANGE) are only
logged while an enemy is near, so "got within 30 m of home" undercounts arrivals; the home is taken as x=-420 m / +420 m, y=0 (map K)."""
import re,sys,collections,statistics
for r in sys.argv[1:]:
    t=0;trans=collections.defaultdict(list);stall=collections.defaultdict(list);pos=collections.defaultdict(list);team={}
    for l in open(r+'/run.log',encoding='utf-8',errors='replace'):
        if 'ST_SAMPLE t=' in l:
            m=re.search(r't=([\d.]+) squad=(\d+) team=(\d)',l);t=float(m[1]);team[int(m[2])]=int(m[3])
        elif 'ST2_TRANSITION' in l:
            m=re.search(r'squad=(\d+) action=(.*?) reason=(.*?) goal=',l);trans[int(m[1])].append((t,m[3]))
        elif 'ST_RANGE squad=' in l:
            m=re.search(r'squad=(\d+) .*leader=V\(X=(-?[\d.]+), Y=(-?[\d.]+)',l);pos[int(m[1])].append((t,float(m[2])/100,float(m[3])/100))
        elif 'MOVE_STALL_DETAIL' in l:
            m=re.search(r'squad=(\d+) order=(\d) action=(\d) .*wp=(\d+)/(\d+) .*clear=(\d)',l)
            if m[2]=='1' and m[3]=='6':stall[int(m[1])].append((t,m[4]=='1' and m[6]=='0'))
    end=t;trips=[]
    for s,v in trans.items():
        for i,(a,why) in enumerate(v):
            if not why.startswith('HOME SUPPLY') or (i>0 and v[i-1][1].startswith('HOME SUPPLY')):continue
            j=i
            while j+1<len(v) and v[j+1][1].startswith('HOME SUPPLY'):j+=1
            b=v[j+1][0] if j+1<len(v) else end
            hx=-420 if team.get(s,0)==0 else 420
            pp=[(x[0],((x[1]-hx)**2+x[2]**2)**.5) for x in pos[s] if a<=x[0]<=b]
            w=sum(x[1] for x in stall[s] if a<=x[0]<=b+3)
            trips.append((s,a,b-a,w,pp[0][1] if pp else None,min(x[1] for x in pp) if pp else None,pp[-1][1] if pp else None,len(pp)))
    stuck=[x for x in trips if x[3]>=3]
    havepos=[x for x in trips if x[7]>0]
    print('== %s: home trips %d; with 3+ wall stalls at first point: %d; with 1-2: %d; none: %d'%(r,len(trips),len(stuck),sum(0<x[3]<3 for x in trips),sum(x[3]==0 for x in trips)))
    print('   trips with leader positions logged: %d; got within 30 m of home: %d'%(len(havepos),sum(x[5]<=30 for x in havepos)))
    for x in sorted(stuck,key=lambda x:-x[3]):print('     squad %d from t=%.0f for %.0f s: wall stalls %d; distance to home first/min/last %s'%(x[0],x[1],x[2],x[3],'%.0f/%.0f/%.0f m (n=%d)'%(x[4],x[5],x[6],x[7]) if x[7] else 'not logged'))
