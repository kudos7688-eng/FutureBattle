"""Generate the quantitative A-stage appendix from validated paired-seed data."""
import csv,json,sys,statistics
from pathlib import Path

def make(p):
 d=json.loads((p/'summary.json').read_text());C={(r['stage'],r['profile'],r['view'],r['filter']):r for r in d['conditions']};K={(r['comparison'],r['metric']):r for r in d['contrasts']}
 def ci(k,m='gpu',n=3):
  x=K[k,m];return f"{x['difference']:.{n}f} [{x['ci_low']:.{n}f}, {x['ci_high']:.{n}f}]"
 views=['Recorded','RTS60','TPS','FPS'];profiles=['Current','Software','Light','ClassicShadow','NoShadow','Medium','Low']
 lines=['## T5 A段階：描画費用の診断（数量表）','',f"{len(d['seeds'])}配置、各条件2反復、{d['runs']}走行。DLL SHA256 `{d['dll_sha256']}`。区間は各配置内で反復平均後、同じ配置を対応させて20,000回再抽出した百分位95%区間。全フレームを独立標本にしていない。",'', '### A2：兵表示のGPU増分（All−None、ms/フレーム）','', '|描画設定|'+'|'.join(views)+'|','|---|'+'---:|'*4]
 for profile in profiles:lines.append('|'+profile+'|'+'|'.join(ci(f'A2:{profile}:{v}:All-None') for v in views)+'|')
 lines+=['','設定を変更したときの兵増分の変化（後−前、負なら兵の増分が縮小）。背景の削減と兵の削減を混同しない。','', '|後−前|'+'|'.join(views)+'|','|---|'+'---:|'*4]
 for after,before in [('Software','Current'),('Light','Software'),('ClassicShadow','Light'),('NoShadow','Light'),('Medium','Current'),('Low','Current')]:
  lines.append('|'+after+'−'+before+'|'+'|'.join(ci(f'A2:{after}-{before}:{v}:soldier_increment') for v in views)+'|')
 lines+=['','### 基準機に近い描画設定Light（実行機は同じRTX 5070 Ti）','', '|視点|表示対象／視錐台内／描画提出|全員表示FPS|GPU全体ms|兵GPU増分ms|増分/描画提出人数 ms/人|増分/表示対象人数 ms/人|描画スレッド増分ms|','|---|---:|---:|---:|---:|---:|---:|---:|']
 for v in views:
  r=C['A2','Light',v,'All'];k=f'A2:Light:{v}:'
  lines.append(f"|{v}|{r['shown']:.1f} / {r['frustum']:.1f} / {r['drawn']:.1f}|{r['fps']:.1f}|{r['gpu']:.3f}|{ci(k+'All-None')}|{ci(k+'per_drawn',n=5)}|{ci(k+'per_shown',n=5)}|{ci(k+'All-None','render_thread')}|")
 lines+=['','比は各配置で増分を当該配置の人数で割ってから平均。画面外・影などの仕事も分子に含むため、一体のメッシュ固有の単価ではない。','', '### A1：登録された表示スロットと画面外の追加・除去','', '|設定|視点|公称枠|平均登録枠|表示対象|主画面提出|GPU全体ms|対500枠GPU差ms|描画スレッド差ms|250超過フレーム/走行|','|---|---|---:|---:|---:|---:|---:|---:|---:|---:|']
 for profile in ['Current','Light']:
  for v in ['Recorded','TPS','FPS']:
   for n in (0,250,500,1000):
    r=C['A1',profile,v,'Total'+str(n)];k=f'A1:{profile}:{v}:{n}-500';g=ci(k) if n!=500 else '対照';rt=ci(k,'render_thread') if n!=500 else '対照'
    lines.append(f"|{profile}|{v}|{n}|{r['registered_bodies']:.2f}|{r['shown']:.1f}|{r['drawn']:.1f}|{r['gpu']:.3f}|{g}|{rt}|{r['population_floor_excess_frames']:.2f}|")
 lines+=['','元の兵の実レンダラー視錐台IDと遮蔽後提出IDを、同じ配置・視点・反復の500体対照とフレームごとに比較。追加分はこのID検査から除く。位置/向きは元記録から同一（保持すべき集合のID/記録位置ハッシュ、実配置誤差を検査）。','', '|設定|視点|枠|視錐台ID完全一致率|提出ID完全一致率|提出不足(人/フレーム)|提出追加(人/フレーム)|','|---|---|---:|---:|---:|---:|---:|']
 masks=list(csv.DictReader((p/'original-screen-set-checks.csv').open(encoding='utf-8-sig')))
 for profile in ['Current','Light']:
  for v in ['TPS','FPS']:
   for n in (250,1000):
    rr=[r for r in masks if (r['stage'],r['profile'],r['view'],r['filter'])==('A1',profile,v,'Total'+str(n))];denom=900*len(rr)
    vals=[sum(int(r[k]) for r in rr)/denom for k in ['frustum_identical_frames','drawn_identical_frames','drawn_missing_soldier_frames','drawn_extra_soldier_frames']]
    lines.append(f'|{profile}|{v}|{n}|{100*vals[0]:.3f}%|{100*vals[1]:.2f}%|{vals[2]:.3f}|{vals[3]:.3f}|')
 lines+=['','描画提出集合は遮蔽履歴/歩行姿勢でも変わるため、視錐台内の一致から提出集合の完全一致を推定しない。3フレーム続けて両集合が一致した部分でのGPU差はoriginal-screen-set-checks.csvにも保存（条件付きの補助値）。0枠は画面内維持を要求しない対照。Recordedは画面内も増減する総量比較であり、画面外の効果の推定には使わない。', '', '### A3：表示停止と更新停止','', '|設定|視点|表示方式|主画面提出|反映更新人数|GPU全体ms|GPU差ms|描画スレッド差ms|復帰回数/走行|マーカー平均数|','|---|---|---|---:|---:|---:|---:|---:|---:|---:|']
 for profile in ['Current','Light']:
  for v in ['TPS','FPS']:
   for a,b in [('All',''),('None','All'),('FrustumHide','All'),('FrustumSleep','FrustumHide'),('DistanceHide','FrustumHide'),('DistanceSleep','DistanceHide'),('KnowledgeProxy','All')]:
    r=C['A3',profile,v,a];k=f'A3:{profile}:{v}:{a}-{b}';g=ci(k) if b else '対照';rt=ci(k,'render_thread') if b else '対照'
    lines.append(f"|{profile}|{v}|{a}{'（対'+b+'）' if b else ''}|{r['drawn']:.1f}|{r['updated_bodies']:.1f}|{r['gpu']:.3f}|{g}|{rt}|{r['wakeups_total']:.1f}|{r['marker_count']:.1f}|")
 lines+=['', '上記の「更新」は記録位置/向きの表示反映。Sleepは非表示の本体コンポーネントtickも止める。共有ポーズ元は表示中のMassのため残る。HideではUEの既存の可視性に基づく姿勢最適化を無効化しないため、この差は純粋なアニメーション評価単価ではない。', '', '詳細なframe/GT/描画/RHI/GPU、表示反映時間、draw calls、RHI全パスのprimitives、主画面本体三角形数、表示対象/視錐台/描画提出、反復別値、全対応差はCSVを参照。']
 lines+=['','### 次段階の判断に使う比較','','画面外500枠追加は、同じ画面内を保ったときの追加負荷。設定変更は背景を差し引いた兵表示増分の変化。両者には同じ描画処理が含まれ得るため、足して寄与率にしない。','',
 '|視点|画面外500枠追加（現行）|画面外500枠追加（Light）|HWRT停止|さらにLumen停止|Lightで影停止|Lightで画面外表示停止|さらに表示更新停止|',
 '|---|---:|---:|---:|---:|---:|---:|---:|']
 for v in ('TPS','FPS'):
  tests=[f'A1:Current:{v}:1000-500',f'A1:Light:{v}:1000-500',f'A2:Software-Current:{v}:soldier_increment',f'A2:Light-Software:{v}:soldier_increment',f'A2:NoShadow-Light:{v}:soldier_increment',f'A3:Light:{v}:FrustumHide-All',f'A3:Light:{v}:FrustumSleep-FrustumHide']
  lines.append('|'+v+'|'+'|'.join(ci(k) for k in tests)+'|')
 lines+=['','単位はGPU ms/フレーム、角括弧は配置単位の95%区間。HWRT停止にはRHI経路全体の切替を含むため、特定のレイトレーシングパスだけの時間とは解釈しない。LightのAll−Noneには、兵本体・武器・影・遮蔽などが残る。材質の寄与はこの比較単独では分からない。RTX 5070 Tiで描画設定を合わせた値をGTX 1660 Super／RTX 3060の実フレーム時間に換算することはできない。']
 if len(d['seeds'])<10:lines.insert(2,'探索段階の少数配置。区間は参考値であり、精密な効果推定・採用判定には使わない。有望な比較だけ配置を追加する。')
 (p/'quantitative-report.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
if __name__=='__main__':make(Path(sys.argv[1]))
