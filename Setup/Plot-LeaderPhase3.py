"""Plot accepted Phase3 distributions with standard matplotlib; no game assets are changed."""
import argparse,csv,sys,json,statistics
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('folder',type=Path);p.add_argument('--library');a=p.parse_args()
if a.library:sys.path.insert(0,a.library)
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
with (a.folder/'conditions.csv').open(encoding='utf-8-sig',newline='') as f:
    rows=list(csv.DictReader(f))
fig,axes=plt.subplots(2,3,figsize=(15,8),layout='constrained',sharey='row')
colors=['#64748b','#0891b2']
layouts=[x for x in ['Open','Cross','Gate'] if any(r['layout']==x for r in rows)]
stable=len(layouts)==1
for i,n in enumerate([500,1000]):
    for j,view in enumerate(['RTS','TPS','FPS']):
        ax=axes[i,j]
        for mi,mode in enumerate(['Individual','Leader']):
            group=[next(r for r in rows if (int(r['units']),r['view'],r['layout'],r['decision'])==(n,view,layout,mode)) for layout in layouts]
            x=[k+(mi-.5)*.36 for k in range(len(layouts))]
            ax.bar(x,[float(r['gt_ms_mean']) for r in group],width=.34,color=colors[mi],label=mode+' mean')
            ax.scatter(x,[float(r['gt_ms_p95']) for r in group],color=colors[mi],marker='D',s=22,label=mode+' p95')
        ax.axhline(16.67,color='#dc2626',ls='--',lw=1,label='16.67 ms budget')
        ax.set_xticks(range(len(layouts)),[{'Open':'Parallel','Cross':'Cross','Gate':'Gates'}[x] for x in layouts]);ax.set_title(f'{n} units / {view}')
        ax.set_ylabel('Game thread CPU (ms)');ax.grid(axis='y',alpha=.15)
axes[0,0].legend(fontsize=8)
fig.suptitle(('Mixed individual AI vs leader decisions — high-health supplement\nOne seed, parallel layout only; separate from normal-health combat' if stable else 'Mixed individual AI vs leader decisions — normal-health combat\n2 seeds per condition; death changes population; returned-to-RTS frames excluded'),fontsize=13)
fig.savefig(a.folder/'game-thread.png',dpi=150);plt.close(fig)
data=json.loads((a.folder/'summary.json').read_text(encoding='utf-8'))['runs']
fig,axes=plt.subplots(1,2,figsize=(14,5),layout='constrained')
keys=['perception_ms','decision_ms','path_ms','follow_ms','rvo_ms','shot_ms','display_ms']
labels=['Perception incl. reports','Decisions incl. tactics','Path queries','Execution + sync','UE RVO','Shot / damage','Visibility + LOD']
palette=['#1d4ed8','#0891b2','#14b8a6','#f59e0b','#ea580c','#be123c','#94a3b8']
for ax,n in zip(axes,[500,1000]):
    groups=[[r for r in data if r['units']==n and r['view']==view and r['decision']==mode] for view in ['RTS','TPS','FPS'] for mode in ['Individual','Leader']]
    base=[0.]*6
    for key,label,color in zip(keys,labels,palette):
        heights=[statistics.mean(r['timing'][key]['mean'] for r in group) for group in groups]
        ax.bar(range(6),heights,bottom=base,label=label,color=color);base=[x+y for x,y in zip(base,heights)]
    ax.set_xticks(range(6),['RTS\nInd','RTS\nLead','TPS\nInd','TPS\nLead','FPS\nInd','FPS\nLead'])
    ax.set_title(f'{n} units');ax.set_ylabel('Instrumented work (ms/frame)');ax.grid(axis='y',alpha=.15)
axes[1].legend(fontsize=8)
fig.suptitle('Exclusive custom-work scopes (equal-weight run means)\nNot total game thread time: engine animation, movement and HUD also cost CPU',fontsize=12)
fig.savefig(a.folder/'cpu-breakdown.png',dpi=150)
