"""Read-only outcome analysis; causal comparisons are whole matches, not episode credit."""
import argparse
import collections
import json
import math
import pathlib
import re
import statistics

p = argparse.ArgumentParser()
p.add_argument('--prefix', default='flank-effect-r1')
p.add_argument('--expected', type=int, default=48)
a = p.parse_args()
root = pathlib.Path(__file__).resolve().parent / 'Benchmarks/SightTrial-20261111'
read = lambda f: json.loads(f.read_text(encoding='utf-8-sig'))
verified = read(root / (a.prefix + '-verified.json'))
assert verified['verified'] and len(verified['runs']) == a.expected

def fields(line):
    return {k: float(v) for k, v in re.findall(r'(\w+)=(-?\d+(?:\.\d+)?)', line)}

def episode_analysis(folder, metrics, teams):
    starts, leaves, states, shots = [], collections.defaultdict(list), collections.defaultdict(list), []
    for line in (folder / 'run.log').read_text(encoding='utf-8-sig').splitlines():
        if 'BATTLE_FLANK_BEGIN ' in line:
            e = fields(line)
            assert teams & (1 << int(e['team'])), 'Disabled faction flanked'
            starts.append(e)
        elif 'BATTLE_FLANK_LEAVE ' in line:
            e = fields(line)
            e['reason'] = line.split('reason=', 1)[1]
            leaves[int(e['squad'])].append(e)
        elif 'BATTLE_FLANK_STATE ' in line:
            e = fields(line)
            states[int(e['squad'])].append(e)
        elif 'BATTLE_FLANK_SHOT ' in line:
            shots.append(fields(line))
    for team in (0, 1):
        observed = [s for s in shots if s['team'] == team]
        m = metrics[f'flank_trial_team{team}']
        assert len(observed) == m['shots']
        assert abs(sum(s['damage'] for s in observed) - m['damage']) < .1
        assert sum(s['kill'] for s in observed) == m['direct_kills']
    assert len(shots) == metrics['shots']
    episodes = []
    for i, e in enumerate(starts):
        sid, start = int(e['squad']), e['t']
        next_start = next((n['t'] for n in starts[i+1:] if n['squad'] == sid), metrics['match_seconds'])
        stop = next((x for x in leaves[sid] if start <= x['t'] <= next_start), None)
        if stop:
            end, reason = stop['t'], stop['reason']
        else:
            stop = next((x for x in states[sid] if start + .001 < x['t'] <= next_start and (x['action'] != 3 or x['living'] == 0)), None)
            end = stop['t'] if stop else next_start
            reason = 'STATE_EXIT_OR_DEATH' if stop else 'MATCH_END_OR_NEXT_PLAN'
        until = min(end + 30, next_start, metrics['match_seconds'])
        row = dict(e, end=end, followup_end=until, reason=reason, arrived=reason.startswith('FLANK ARRIVED'))
        row['censored_followup'] = until < end + 29.99
        for phase, lo, hi in [('moving', start, end), ('post', end, until)]:
            own = [s for s in shots if lo <= s['t'] < hi and s['squad'] == sid]
            incoming = [s for s in shots if lo <= s['t'] < hi and s['victim_squad'] == sid]
            row[phase] = dict(seconds=hi-lo, shots=len(own), damage=sum(s['damage'] for s in own),
                              direct_kills=sum(s['kill'] for s in own), received=sum(s['damage'] for s in incoming),
                              side_damage=sum(s['damage'] for s in own if s['angle'] >= 60),
                              rear_damage=sum(s['damage'] for s in own if s['angle'] >= 120))
        row['post_point_presence'] = any(end <= s['t'] < until and s['point'] > 0 for s in states[sid])
        episodes.append(row)
    return episodes

runs = []
for v in verified['runs']:
    folder = root / v['name']
    m = v['metrics']
    match = re.fullmatch(re.escape(a.prefix) + r'-([AB])-s(\d+)-x([01])-(strict|relaxed|on0|on1)', v['name'])
    assert match
    field, seed, swap, variant = match.groups()
    assert int(seed) in (7,19,43)
    manifest = read(folder / 'manifest.json')
    args = manifest['arguments']
    assert '-BattleFlankTrial' in args and '-BattleArea' in args
    assert f'/Game/Prototype/Lvl_BattleWide_{field}' in args and f'-BattleTrialSeed={seed}' in args
    assert ('-BattleTrialSwap' in args) == bool(int(swap))
    assert all(x in args for x in ('-BattleSquadFlank=1','-BattleSightRange=10000','-BattleWeaponRange=5000','-BattleRangeAware=1','-UseFixedTimeStep','-FPS=60','-nullrhi'))
    assert manifest['behaviorConcurrency'] == 4
    assert '-BattleVerifyMixedSave' not in args
    mask = 1 if variant == 'on0' else 2 if variant == 'on1' else 3
    assert f'-BattleFlankTeams={mask}' in args
    assert f'-BattleFlankRelaxed={int(variant != "strict")}' in args
    assert m['ruleset'] == 'Area' and 0 < m['match_seconds'] <= 601
    if 'TIME LIMIT' in m['reason']:
        assert m['match_seconds'] >= 599
        for team,side in enumerate(('ally','enemy')):
            assert m[side+'_score'] == m[side+'_alive']+m[side+'_resources']+20*sum(x['owner']==team for x in m['final_objectives'])
    else:
        assert 'BASE DESTROYED' in m['reason'] and m['winner'] in (0,1)
        assert m['enemy_base_hp' if m['winner']==0 else 'ally_base_hp'] == 0
    assert m['completed_replacements'] + m['ally_resources'] + m['enemy_resources'] == 200
    run = dict(name=v['name'], map=field, seed=int(seed), swap=int(swap), variant=variant, metrics=m,
               episodes=episode_analysis(folder, m, mask))
    runs.append(run)

def totals(subset):
    episodes = [e for r in subset for e in r['episodes']]
    arrived = [e for e in episodes if e['arrived']]
    enabled_minutes=sum(r['metrics']['match_seconds']*(2 if r['variant'] in ('strict','relaxed') else 1)/60 for r in subset)
    return dict(runs=len(subset), attempts=sum(r['metrics']['tactical_events'].get('FlankAttempt',0) for r in subset), accepted=len(episodes), arrived=len(arrived),
                enabled_team_minutes=enabled_minutes, accepted_per_team_minute=len(episodes)/enabled_minutes if enabled_minutes else 0,
                arrival_rate=len(arrived)/len(episodes) if episodes else 0,
                solo=sum(e['support'] == -1 for e in episodes),
                solo_arrived=sum(e['support'] == -1 for e in arrived),
                solo_arrived_post_damage=sum(e['post']['damage'] for e in arrived if e['support'] == -1),
                continued_solo=sum(r['metrics']['tactical_events'].get('FlankContinuedSolo', 0) for r in subset),
                support_aborts=sum('REQUIRED FIRE SUPPORT LOST' in e['reason'] for e in episodes),
                arrived_post_shooting=sum(e['post']['shots'] > 0 for e in arrived),
                arrived_post_damaging=sum(e['post']['damage'] > 0 for e in arrived),
                arrived_post_damage=sum(e['post']['damage'] for e in arrived),
                arrived_post_received=sum(e['post']['received'] for e in arrived),
                arrived_post_side_damage=sum(e['post']['side_damage'] for e in arrived),
                arrived_post_rear_damage=sum(e['post']['rear_damage'] for e in arrived),
                arrived_post_point_presence=sum(e['post_point_presence'] for e in arrived),
                arrived_censored=sum(e['censored_followup'] for e in arrived),
                moving_received=sum(e['moving']['received'] for e in episodes),
                moving_seconds=sum(e['moving']['seconds'] for e in episodes))

groups = {v: totals([r for r in runs if r['variant'] == v]) for v in ('strict','relaxed','on0','on1')}
by_map = {field:{v:totals([r for r in runs if r['map']==field and r['variant']==v]) for v in groups} for field in ('A','B')}
duels = []
for r in runs:
    if r['variant'] not in ('on0','on1'):
        continue
    on = 0 if r['variant'] == 'on0' else 1
    m = r['metrics']
    sides = ['ally','enemy']
    terminal_score=lambda t:m[sides[t]+'_alive']+m[sides[t]+'_resources']+20*sum(p['owner']==t for p in m['final_objectives'])
    outcome = 0 if m['winner'] == -1 else 1 if m['winner'] == on else -1
    d = dict(name=r['name'], map=r['map'], seed=r['seed'], swap=r['swap'], on_team=on, outcome=outcome,
             on_side=on ^ r['swap'], winner_team=m['winner'], winner_side=(m['winner'] ^ r['swap']) if m['winner']>=0 else -1,
             reason=m['reason'], seconds=m['match_seconds'], score_margin=terminal_score(on)-terminal_score(1-on),
             alive_margin=m[sides[on]+'_alive']-m[sides[1-on]+'_alive'])
    for key in ('damage','direct_kills','owned_point_seconds'):
        d[key+'_margin'] = m[f'flank_trial_team{on}'][key]-m[f'flank_trial_team{1-on}'][key]
    duels.append(d)
paired = []
for r in runs:
    if r['variant'] != 'on0':continue
    pair = [d for d in duels if (d['map'],d['seed'],d['swap']) == (r['map'],r['seed'],r['swap'])]
    assert len(pair) == 2
    paired.append(dict(map=r['map'],seed=r['seed'],swap=r['swap'],score_margin=statistics.mean(d['score_margin'] for d in pair),
                       both_time_limit=all('TIME LIMIT' in d['reason'] for d in pair),
                       same_winner=pair[0]['winner_team']==pair[1]['winner_team'],
                       damage_margin=statistics.mean(d['damage_margin'] for d in pair),
                       point_seconds_margin=statistics.mean(d['owned_point_seconds_margin'] for d in pair)))

result = dict(verified=True, complete_matrix=len(runs)==48, source_verification=a.prefix+'-verified.json', groups=groups, by_map=by_map, duels=duels, paired=paired, runs=runs)
(root / (a.prefix+'-analysis.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
lines = ['# 迂回条件緩和と対戦寄与の検証', '',
         f'集計済み{len(runs)}/48戦。'+('全条件完了。' if len(runs)==48 else '途中集計であり、最終結論ではない。'), '',
         'Area・認識100m・射程50m・A/B・seed7/19/43・配置入替。従来両軍/緩和両軍/緩和0のみ/緩和1のみを同一DLLで比較。通常既定は変更していない。並行headlessは性能評価に使わない。', '',
         '緩和は、直近5秒被害の開始許容を0から生存兵最大HP合計の10%までへ広げ、既知の射程内敵数が生存兵数以下なら、露出経路20m以内、または露出率80%以下の30m以内を単独でも許可する。元から許可していた露出率65%以下の経路は維持。援護喪失時は直近2.5秒の自部隊観測と残りNav経路で同じ危険基準を再評価する。健康70%以上・生存7人以上・武装60%以上、20秒期限、15秒再試行待ち、同時迂回上限、緊急撤退を維持。露出率は選択した接敵位置からの静的視線サンプルであり、全敵の危険領域の厳密な合成ではない。', '',
         '|条件|戦数|検討|採用|単独開始|援護喪失後続行|到達|到達割合|援護喪失中止|',
         '|---|---:|---:|---:|---:|---:|---:|---:|---:|']
for key,g in groups.items():
    lines.append(f"|{key}|{g['runs']}|{g['attempts']}|{g['accepted']}|{g['solo']}|{g['continued_solo']}|{g['arrived']}|{100*g['arrival_rate']:.1f}%|{g['support_aborts']}|")
lines += ['', '本拠地破壊による早期終了を含むため、補助として有効軍・対戦1分あたりの採用数も示す（両軍ONは対戦時間×2軍、片軍ONは×1軍）。']
for key,g in groups.items():lines.append(f"- {key}: {g['accepted_per_team_minute']:.3f}回/軍分（計{g['enabled_team_minutes']:.1f}軍分）")
lines += ['', '最初から専任援護を要求せず開始した事例の到達：']
for key,g in groups.items():lines.append(f"- {key}: {g['solo_arrived']}/{g['solo']}回、到達後与損害{g['solo_arrived_post_damage']:.0f} HP。")
lines.append('ここで単独とは専任援護を割り当てない意味であり、近隣の味方による通常射撃まで除外したものではない。援護喪失後の単独切替とは別集計。')
lines += ['', '## 到達後の関連指標', '',
          '終了後最大30秒を集計。次の迂回開始または試合終了で打ち切り、同一部隊の区間を重複させない。戦闘全体への因果寄与や、迂回がなければ得られなかった損害とは区別する。実HP損害のみ、直接撃破と隊長死亡による部隊停止は別。側面は被弾時の敵の向きから60度以上、背面は120度以上（側面値に含む）。拠点関与は少なくとも1兵が実際の占領半径4.5mに入ったことで、占領成功の因果帰属ではない。', '',
          '|条件|到達後射撃/到達|与損害あり|与損害|被損害|側背面与損害|うち背面|拠点関与|30秒未満打切り|',
          '|---|---:|---:|---:|---:|---:|---:|---:|---:|']
for key,g in groups.items():
    lines.append(f"|{key}|{g['arrived_post_shooting']}/{g['arrived']}|{g['arrived_post_damaging']}|{g['arrived_post_damage']:.0f}|{g['arrived_post_received']:.0f}|{g['arrived_post_side_damage']:.0f}|{g['arrived_post_rear_damage']:.0f}|{g['arrived_post_point_presence']}|{g['arrived_censored']}|")
lines += ['', '|条件|全迂回の移動部隊秒|移動中被損害|移動部隊秒あたり被損害|', '|---|---:|---:|---:|']
for key,g in groups.items():
    rate=g['moving_received']/g['moving_seconds'] if g['moving_seconds'] else 0
    lines.append(f"|{key}|{g['moving_seconds']:.1f}|{g['moving_received']:.0f}|{rate:.2f}|")
wins=sum(d['outcome']==1 for d in duels);losses=sum(d['outcome']==-1 for d in duels);draws=len(duels)-wins-losses
lines += ['', '## 迂回あり対なし', '', f'迂回あり側：{wins}勝{losses}敗{draws}分。うち本拠地破壊で終了した試合は{sum("BASE DESTROYED" in d["reason"] for d in duels)}。味方援護・撤退・射撃等は両軍で有効。OFF軍はST3b迂回割当だけを無効化し、ログで採用0を確認。得点差は終了時の生存兵＋資源＋中央拠点×20。本拠地破壊があれば、その勝利を得点差より優先する。', '',
          '|map|seed|配置入替|決着|陣営交代2戦の平均得点差（あり−なし）|与損害差|保有拠点秒差|',
          '|---|---:|---:|---|---:|---:|---:|']
for d in paired:
    finish='時間切れ2戦' if d['both_time_limit'] else '本拠地破壊を含む'
    lines.append(f"|{d['map']}|{d['seed']}|{d['swap']}|{finish}|{d['score_margin']:.1f}|{d['damage_margin']:.0f}|{d['point_seconds_margin']:.1f}|")
if paired:
    margins=[d['score_margin'] for d in paired]
    lines += ['', f"陣営交代ペアの平均得点差：{statistics.mean(margins):+.1f}、中央値{statistics.median(margins):+.1f}。プラスの条件は{sum(x>0 for x in margins)}/{len(margins)}。"]
    timed=[d['score_margin'] for d in paired if d['both_time_limit']]
    if timed:lines.append(f"両戦とも600秒まで続いた{len(timed)}組に限ると平均得点差{statistics.mean(timed):+.1f}。本拠地破壊を含む組は終了時刻が異なるため、上の全組の得点/損害/拠点時間差を同じ600秒時点の効果とは扱わない。")
    lines.append(f"迂回有効軍を交換しても勝利軍が同じだった組：{sum(d['same_winner'] for d in paired)}/{len(paired)}。位置や軍IDの影響が残るため、勝敗数だけで迂回の寄与を判断しない。")
for field in ('A','B'):
    ds=[d for d in duels if d['map']==field]
    if ds:lines.append(f"マップ{field}：{sum(d['outcome']==1 for d in ds)}勝{sum(d['outcome']==-1 for d in ds)}敗{sum(d['outcome']==0 for d in ds)}分、平均得点差{statistics.mean(d['score_margin'] for d in ds):+.1f}。物理配置では左側{sum(d['winner_side']==0 for d in ds)}勝・右側{sum(d['winner_side']==1 for d in ds)}勝。")
lines += ['', '同じmap/seed/配置で有効軍を交換した2戦の平均により、軍IDや配置の固定的優劣を抑える。少数シード、状態依存の乱数消費、同じマップ/配置の相関があるため、全試合を独立標本とみなす有意差検定や一般的な勝率保証はしない。部隊間の援護再配分も含むAI全体の効果であり、側方位置だけの純粋な効果ではない。', '',
          '途中死亡/手動状態変更は1Hzの部隊スナップショットで検出するため、終了時刻に最大約1秒の誤差がある。補充による人数増加と隊長死亡の間接損失を局所与損害に混ぜない。移動待ちなど既存の制約を含む現行試作の評価。', '']
(root / (a.prefix+'-report.md')).write_text('\n'.join(lines),encoding='utf-8')
print(json.dumps(dict(groups=groups, duel_wins=wins,duel_losses=losses,duel_draws=draws),ensure_ascii=False))
