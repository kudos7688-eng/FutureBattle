import json,re,statistics,sys
from pathlib import Path
root=Path(sys.argv[1]);s=json.loads((root/'performance-summary.json').read_text());out=[]
out+=['# 補足測定表','','各セルは2走行平均。GT/p95/超過率の各反復はperformance-tables.md。影/Lumen/反射OFFの既存ベンチ設定。','','|視点|RT OFF→ON ms|GPU OFF→ON ms|遠表示対象/900（平均）|可視遠表示（平均）|VAT presenter ms|','|---|---:|---:|---:|---:|---:|']
for view in ['RTS','TPS','FPS']:
 rs={mode:[r for r in s['runs'] if r['view']==view and r['reuse']==mode] for mode in [0,1]}
 avg=lambda mode,key:statistics.mean(r['timing'][key]['mean'] for r in rs[mode])
 vals=[]
 for r in rs[1]:
  log=(root/r['folder']/'run.log').read_text(encoding='utf-8-sig');m=re.search(r'VAT_DISPLAY_STATS frames=(\d+) far_mean=([\d.]+) visible_far_mean=([\d.]+) presenter_ms=([\d.]+)',log);assert m
  vals.append(list(map(float,m.groups()[1:])))
 counts=[statistics.mean(x[i] for x in vals) for i in range(3)]
 out.append(f'|{view}|{avg(0,"rt_ms"):.3f}→{avg(1,"rt_ms"):.3f}|{avg(0,"gpu_ms"):.3f}→{avg(1,"gpu_ms"):.3f}|{counts[0]:.1f}|{counts[1]:.1f}|{counts[2]:.3f}|')
out+=['','遠表示対象数にはfog非表示も含む。可視遠表示はvisibility flagで、画面内pixelのある人数とは違う。presenterはISM transform/custom data更新の自己計測で、表示切替を行うApplyLOD等すべてを含まない。RT/GPUも全frameの上限保証ではない。','','## 通常走行の既存scope内訳','','|視点/設定|認識|判断|経路|follow|RVO|射撃|表示|GT|','|---|---:|---:|---:|---:|---:|---:|---:|---:|']
for view in ['RTS','TPS','FPS']:
 for mode in [0,1]:
  rs=[r for r in s['runs'] if r['view']==view and r['reuse']==mode]
  nums=[statistics.mean(r['timing'][k]['mean'] for r in rs) for k in ['perception_ms','decision_ms','path_ms','follow_ms','rvo_ms','shot_ms','display_ms','gt_ms']]
  out.append('|'+view+(' ON' if mode else ' OFF')+'|'+'|'.join(f'{v:.3f}' for v in nums)+'|')
out+=['','既存scopeはネストするものがあるため加算してGTから引かない。親子重複を除く残差は別traceの表で示す。視線走査は認識に含まれる。']
(root/'supplement-tables.md').write_text('\n'.join(out)+'\n',encoding='utf-8')
print('\n'.join(out[:10]))
