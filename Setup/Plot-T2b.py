"""Optional standalone Matplotlib figure; measurements/analysis need only stdlib."""
import argparse,csv,json,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('folder',type=Path);p.add_argument('--library',type=Path);a=p.parse_args()
if a.library:sys.path.insert(0,str(a.library))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
r=json.loads((a.folder/'summary.json').read_text(encoding='utf-8'))
arms=['Individual','Off','Rate540','Rate1080']
labels=['Individual OFF','Squad OFF','Turn 540 deg/s','Turn 1080 deg/s']
colors=['#333333','#d55e00','#0072b2','#009e73']
fig,axes=plt.subplots(2,2,figsize=(12,8),layout='constrained')
for arm,label,color in zip(arms,labels,colors):
    rows=sorted((x for x in r['curve_means'] if x['arm']==arm and x['seconds']<=30),key=lambda x:x['seconds'])
    assert all(x['n']==20 for x in rows)
    axes[0,0].plot([x['seconds'] for x in rows],[x['mean_alive'] for x in rows],marker='o',color=color,label=label)
    if arm!='Individual':
        rows=sorted((x for x in r['curve_contrasts'] if x['arm']==arm and x['reference']=='Individual' and x['seconds']<=30),key=lambda x:x['seconds'])
        ts=[x['seconds'] for x in rows]
        axes[0,1].plot(ts,[x['difference'] for x in rows],marker='o',color=color,label=label)
        axes[0,1].fill_between(ts,[x['ci_low'] for x in rows],[x['ci_high'] for x in rows],color=color,alpha=.16)
axes[0,0].set(title='Survival curve: first 30 seconds of combat',xlabel='Seconds since first shot',ylabel='Alive soldiers (seed mean)')
axes[0,0].legend(fontsize=8)
axes[0,1].axhline(0,color='#555555',linewidth=1,linestyle='--')
axes[0,1].set(title='Difference vs Individual OFF (paired 95% CI)',xlabel='Seconds since first shot',ylabel='Additional alive soldiers')
means={x['arm']:x for x in r['means']}
for ax,key,title,ylabel in [(axes[1,0],'combat_window_shots','Shots in the 30-second combat window','Shots (seed mean)'),(axes[1,1],'shots_per_alive_shooter_second','Shooting normalized by alive-shooter exposure','Shots / alive soldier / second')]:
    values=[means[x][key] for x in arms];ax.bar(labels,values,color=colors,width=.65)
    ax.set(title=title,ylabel=ylabel);ax.tick_params(axis='x',labelrotation=15,labelsize=8)
    for i,v in enumerate(values):ax.text(i,v,f'{v:.0f}' if key=='combat_window_shots' else f'{v:.3f}',ha='center',va='bottom',fontsize=9)
    ax.set_ylim(0,max(values)*1.15)
for ax in axes.flat:ax.grid(axis='y',alpha=.2);ax.set_axisbelow(True)
fig.suptitle('T2b: shared-contact turn-to-face trial | 500 soldiers, 20 paired seeds, T2a OFF',fontsize=13)
fig.savefig(a.folder/'T2b-survival-and-shooting.png',dpi=160)
fig.savefig(a.folder/'T2b-survival-and-shooting.svg')
plt.close(fig)
with (a.folder/'survival-curves.csv').open(encoding='utf-8-sig',newline='') as f: raw=list(csv.DictReader(f))
fig,axes=plt.subplots(2,2,figsize=(11,7),sharex=True,sharey=True,layout='constrained')
for ax,arm,label,color in zip(axes.flat,arms,labels,colors):
    for seed in r['seeds']:
        rows=sorted((x for x in raw if x['arm']==arm and int(x['seed'])==seed),key=lambda x:float(x['seconds']))
        ax.plot([float(x['seconds']) for x in rows],[float(x['alive']) for x in rows],color=color,alpha=.3,linewidth=1)
        ax.plot(float(rows[-1]['seconds']),float(rows[-1]['alive']),'.',color=color,alpha=.6)
    ax.set(title=label,xlabel='Seconds since first shot',ylabel='Alive soldiers',ylim=(0,510));ax.grid(alpha=.2)
fig.suptitle('Full observed survival trajectories: 20 seeds per condition\nLines stop at the last 5-second sample before match end; no carry-forward',fontsize=12)
fig.savefig(a.folder/'T2b-survival-full.png',dpi=160)
fig.savefig(a.folder/'T2b-survival-full.svg')
(a.folder/'plot-tool.json').write_text(json.dumps({'library':'Matplotlib','version':matplotlib.__version__,'source':'https://matplotlib.org/','scope':'offline result figure only; not a game dependency'},indent=2),encoding='utf-8')
