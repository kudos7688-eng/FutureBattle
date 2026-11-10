"""Plot measured T5-1/2 results; does not modify game assets."""
import argparse,csv,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('folder',type=Path);p.add_argument('--library');a=p.parse_args()
if a.library:sys.path.insert(0,a.library)
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
def read(name):
 with (a.folder/name).open(encoding='utf-8-sig',newline='') as f:return list(csv.DictReader(f))
rows=read('condition-means.csv');index={(r['view'],r['filter']):r for r in rows};contrasts=read('contrasts.csv')
views=['Recorded','RTS300','RTS150','RTS60','TPS','FPS'];labels=['T1 fixed','RTS 300m','RTS 150m','RTS 60m','TPS','FPS']
fig,axs=plt.subplots(2,2,figsize=(13,8),layout='constrained')
ax=axs[0,0];x=list(range(6));base=[float(index[v,'None']['gpu']) for v in views];body=[float(index[v,'All']['gpu'])-b for v,b in zip(views,base)]
ax.bar(x,base,label='Scene, soldiers hidden',color='#cbd5e1');ax.bar(x,body,bottom=base,label='Soldier display increment',color='#0284c7');ax.set_xticks(x,labels,rotation=15);ax.set_ylabel('GPU ms / frame');ax.set_title('Current assets: GPU cost by camera');ax.legend(fontsize=8)
ax=axs[0,1]
for delta,key,label,color in [(-.18,'frustum','Within frustum','#2563eb'),(.18,'drawn','Main-view raster submissions','#16a34a')]:ax.bar([i+delta for i in x],[float(index[v,'All'][key]) for v in views],width=.35,label=label,color=color)
ax.set_xticks(x,labels,rotation=15);ax.set_ylabel('Mean soldiers / frame');ax.set_title('Visibility (submitted meshes, not pixel counts)');ax.legend(fontsize=8)
ax=axs[1,0];filters=['None','Actors','Mass','NoWeapons','All'];xx=list(range(5))
for delta,key,label,color in [(-.18,'gpu','GPU','#0284c7'),(.18,'render_thread','Render thread','#f97316')]:ax.bar([i+delta for i in xx],[float(index['Recorded',f][key]) for f in filters],width=.35,label=label,color=color)
ax.set_xticks(xx,['No soldiers','Actors only','Mass only','No weapons','All']);ax.set_ylabel('ms / frame');ax.set_title('T1 fixed camera: display isolation');ax.legend(fontsize=8)
ax=axs[1,1];pairs=[('per_actor:marginal','Actor, marginal'),('per_mass:marginal','Mass, marginal'),('per_soldier:all','Mixed, total'),('per_soldier:weapons','Weapons')]
for i,(comp,label) in enumerate(pairs):
 r=next(r for r in contrasts if r['comparison']==comp and r['metric']=='gpu');m,lo,hi=[float(r[k]) for k in ['difference','ci_low','ci_high']];ax.errorbar(m,i,xerr=[[m-lo],[hi-m]],fmt='o',color='#0284c7',capsize=4)
ax.axvline(.0141,color='#dc2626',ls='--',label='T1 reference 0.0141');ax.set_yticks(range(4),[s for _,s in pairs]);ax.invert_yaxis();ax.set_xlabel('GPU ms / shown soldier (paired seed 95% CI)');ax.set_title('Per-soldier display increments');ax.legend(fontsize=8)
fig.suptitle('T5-1 / T5-2 | 500 soldiers, 1920 x 1080, replay only',fontsize=16)
fig.savefig(a.folder/'T5-current-assets.png',dpi=160);fig.savefig(a.folder/'T5-current-assets.svg');plt.close(fig)
