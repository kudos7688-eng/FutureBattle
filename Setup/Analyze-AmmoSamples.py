"""How far ammunition runs down in evaluation matches (reads results only; no build, no lock).

Usage: python -I Setup/Analyze-AmmoSamples.py <result folder name or path> [...]
       (a bare name is looked up under Setup/Benchmarks/SightTrial-20261111/)

Per match, from run.log and metrics.json:
  - BATTLE_ST_SAMPLE (per squad: living, ammo, reserve): the lowest rounds per living member as a share of the squad's full
    load at its first sample, and the rounds fired (sum of the drops between samples with the same number of living members).
  - BATTLE_SHOT_AT (needs -BattleShotLog): shots per soldier.
  - AREA_POINT_SUPPLY / AREA_TEAM_SUPPLY: refills at owned points and from supply vehicles (refills at home are not logged).
  - tactical_events: supply trips and their reasons; type_activity: shots, damage, kills and deaths per side and squad type.
Rifle squads are recognised by 8 members at the first sample (UBattleTuning::RifleMembers)."""
import collections, json, os, re, statistics, sys

SAMPLE = re.compile(r'BATTLE_ST_SAMPLE t=([\d.]+) squad=(\d+) team=(\d) managed=\d living=(\d+) .*? ammo=(\d+) reserve=(\d+)')
SHOT = re.compile(r'BATTLE_SHOT_AT t=[\d.]+ unit=(\d+) team=\d type=(-?\d+)')
SUPPLY = re.compile(r'AREA_(POINT|TEAM)_SUPPLY time=\d+ squad=\d+ team=\d rounds=(\d+)')
BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Benchmarks', 'SightTrial-20261111')


def analyse(run):
    folder = run if os.path.isdir(run) else os.path.join(BASE, run)
    first, last, low, fired = {}, {}, {}, collections.Counter()
    shots, shot_type = collections.Counter(), {}
    supply_rounds, supply_count = collections.Counter(), collections.Counter()
    with open(os.path.join(folder, 'run.log'), encoding='utf-8', errors='replace') as log:
        for line in log:
            if 'BATTLE_ST_SAMPLE' in line:
                m = SAMPLE.search(line)
                if not m:
                    continue
                squad, living, total = int(m[2]), int(m[4]), int(m[5]) + int(m[6])
                if squad not in first:
                    first[squad] = (living, total, int(m[3]))
                if squad in last and last[squad][0] == living and last[squad][1] > total:
                    fired[squad] += last[squad][1] - total
                last[squad] = (living, total)
                if living > 0 and first[squad][0] > 0 and first[squad][1] > 0:
                    share = (total / living) / (first[squad][1] / first[squad][0])
                    if squad not in low or share < low[squad][0]:
                        low[squad] = (share, float(m[1]), living, total)
            elif 'BATTLE_SHOT_AT' in line:
                m = SHOT.search(line)
                if m:
                    shots[int(m[1])] += 1
                    shot_type[int(m[1])] = int(m[2])
            elif '_SUPPLY time=' in line:
                m = SUPPLY.search(line)
                if m:
                    supply_rounds[m[1]] += int(m[2])
                    supply_count[m[1]] += 1
    metrics = json.load(open(os.path.join(folder, 'metrics.json'), encoding='utf-8-sig'))
    print('== %s  score %s:%s  match %.0f s' % (os.path.basename(folder.rstrip('/\\')), metrics.get('ally_score'), metrics.get('enemy_score'), metrics.get('match_seconds', 0)))
    events = metrics.get('tactical_events', {})
    print('  supply events:', {k: v for k, v in sorted(events.items()) if 'Supply' in k} or 'none')
    print('  refills logged: points %d times / %d rounds, supply vehicles %d times / %d rounds' % (
        supply_count['POINT'], supply_rounds['POINT'], supply_count['TEAM'], supply_rounds['TEAM']))
    for side in ('ally', 'enemy'):
        rows = [t for t in metrics.get(side + '_type_activity', []) if t['person_seconds'] > 0 or t['shots'] > 0]
        print('  %s: ' % side + ', '.join('type%d shots=%d damage=%d kills=%d deaths=%d' % (
            t['type'], t['shots'], t['damage'], t['kills'], t['deaths']) for t in rows))
    rifle = sorted(s for s in first if first[s][0] == 8)
    if rifle:
        full = collections.Counter(first[s][1] for s in rifle).most_common(1)[0][0]
        lows = sorted(low[s][0] for s in rifle if s in low)
        fire = sorted((fired[s] for s in rifle), reverse=True)
        print('  rifle squads: %d, full load %d rounds; rounds fired: sum %d, most %s, median %d' % (len(rifle), full, sum(fire), fire[:5], fire[len(fire) // 2]))
        print('  lowest rounds per living member (share of full): median %.2f, least %.2f; squads below 0.5: %d, below 0.25: %d, at 0: %d' % (
            statistics.median(lows), lows[0], sum(x < .5 for x in lows), sum(x < .25 for x in lows), sum(x <= 0 for x in lows)))
    other = sorted((low[s][0], s, first[s][0], first[s][1]) for s in first if first[s][0] != 8 and s in low)[:4]
    if other:
        print('  other squads with the least left: ' + ', '.join('squad %d (%d members, %d rounds) %.2f' % (s, n, r, share) for share, s, n, r in other))
    carbine = sorted((n for u, n in shots.items() if shot_type[u] == 0), reverse=True)
    if carbine:
        print('  shots per rifle-squad soldier (logged, soldiers who fired: %d): most %s, median %d, 30 or more: %d' % (
            len(carbine), carbine[:5], carbine[len(carbine) // 2], sum(n >= 30 for n in carbine)))


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for name in sys.argv[1:]:
        analyse(name)
