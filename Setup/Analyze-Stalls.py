import re,sys,collections,math
ps=re.compile(r'BATTLE_MOVE_STALL unit=(\d+) team=(\d) squad=(\d+) pos=X=([-\d.]+) Y=([-\d.]+) Z=([-\d.]+) waypoint=X=([-\d.]+) Y=([-\d.]+)')
pd=re.compile(r'BATTLE_MOVE_STALL_DETAIL unit=(\d+) squad=(\d+) order=(\d+) action=(\d+) sector=(\d) final=(\d) wp=(\d+)/(\d+) goal=(\S+ \S+ \S+|-) clear=(\d) own=(\d+) team=(\d+) enemy=(\d+) still=(\d+) cover=(\d)')
tot=collections.Counter();spots=collections.Counter();spotcat=collections.defaultdict(collections.Counter);n=0;byteam=collections.Counter();dist=collections.defaultdict(list)
for r in sys.argv[1:]:
    last=None
    for l in open(r+'/run.log',encoding='utf-8',errors='replace'):
        m=ps.search(l)
        if m and 'DETAIL' not in l: last=m;continue
        m=pd.search(l)
        if m and last and last.group(1)==m.group(1):
            x,y=float(last.group(4)),float(last.group(5));wx,wy=float(last.group(7)),float(last.group(8))
            wp,num,final,clear=int(m.group(7)),int(m.group(8)),int(m.group(6)),int(m.group(10))
            own,team,enemy,still=int(m.group(11)),int(m.group(12)),int(m.group(13)),int(m.group(14))
            dw=math.hypot(wx-x,wy-y)/100
            if clear==0 and wp==1: c='1 wall ahead, first point (cut off from squad path)'
            elif clear==0: c='2 wall ahead, later point'
            elif own+team>=3: c='3 crowd (3+ allies within 1.5m)'
            elif own+team>=1: c='4 1-2 allies within 1.5m'
            elif final: c='5 alone at final approach'
            else: c='6 alone, way clear'
            tot[c]+=1;n+=1;byteam[int(last.group(2))]+=1;dist[c].append(dw)
            # fold east onto west by rotation
            fx,fy=(x,y) if x<0 else (-x,-y)
            k=(round(fx/500)*5,round(fy/500)*5);spots[k]+=1;spotcat[k][c[0]]+=1
            last=None
print('matches',len(sys.argv)-1,'stalls',n,'by team',dict(byteam))
for c in sorted(tot):
    v=sorted(dist[c]);print('  %-55s %5d %4.0f%%  dist to point: median %.1fm'%(c,tot[c],100*tot[c]/n,v[len(v)//2]))
print('hot spots (5m cells, east folded onto west by rotation): cell(m) count [category counts]')
for k,v in spots.most_common(22):
    print('  (%5d,%5d) %4d  %s'%(k[0],k[1],v,dict(sorted(spotcat[k].items()))))
top=sum(v for k,v in spots.most_common(20));print('top 20 cells hold %d%% of stalls; cells with stalls: %d'%(100*top/n,len(spots)))
