"""How much the soldiers actually fire in evaluation matches (reads results only; no build, no lock).

Usage: python -I Setup/Analyze-FireRate.py <result folder name or path> [...]
       (a bare name is looked up under Setup/Benchmarks/SightTrial-20261111/)

Per match, from metrics.json and run.log:
  - shots per rifleman and per rifleman-minute (ally/enemy_type_activity type 0: shots, person_seconds),
  - the ceiling if a rifleman fired without pause (UBattleTuning::AIShotInterval, 1.4 s by default),
  - how the shots split: aimed at a seen enemy (BATTLE_SHOT_AT, needs -BattleShotLog) and cover fire at a point
    (cover_diagnostics.arms_cover_fire_shots),
  - squad time by plan (tactical_squad_seconds) and by known contacts (BATTLE_ST_SAMPLE contacts=),
  - how many shots a peek out of cover ends with (cover_diagnostics.peek_end_shots_*),
  - how unevenly the aimed shots are spread over the soldiers (BATTLE_SHOT_AT unit=)."""
import collections, json, os, re, sys

SAMPLE = re.compile(r'BATTLE_ST_SAMPLE t=([\d.]+) squad=(\d+) team=(\d) managed=\d living=(\d+) .*? contacts=(\d+) ')
SHOT = re.compile(r'BATTLE_SHOT_AT t=([\d.]+) unit=(\d+) team=\d type=(-?\d+) d_cm=(\d+)')
BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Benchmarks', 'SightTrial-20261111')
INTERVAL = 1.4


def analyse(run):
    folder = run if os.path.isdir(run) else os.path.join(BASE, run)
    m = json.load(open(os.path.join(folder, 'metrics.json'), encoding='utf-8-sig'))
    shots, first_living, seen = collections.Counter(), {}, {}
    contact_squad, all_squad, contact_person, all_person = 0., 0., 0., 0.
    with open(os.path.join(folder, 'run.log'), encoding='utf-8', errors='replace') as log:
        for line in log:
            if 'BATTLE_ST_SAMPLE' in line:
                s = SAMPLE.search(line)
                if not s:
                    continue
                t, squad, living, contacts = float(s[1]), int(s[2]), int(s[4]), int(s[5])
                first_living.setdefault(squad, living)
                step = t - seen.get(squad, t)
                seen[squad] = t
                if 0 < step < 5 and living > 0:
                    all_squad += step
                    all_person += step * living
                    if contacts > 0:
                        contact_squad += step
                        contact_person += step * living
            elif 'BATTLE_SHOT_AT' in line:
                s = SHOT.search(line)
                if s and s[3] == '0':
                    shots[int(s[2])] += 1
    rifle = [a[0] for a in (m['ally_type_activity'], m['enemy_type_activity'])]
    rifle_shots = sum(a['shots'] for a in rifle)
    rifle_seconds = sum(a['person_seconds'] for a in rifle)
    c = m.get('cover_diagnostics', {})
    plan = m.get('tactical_squad_seconds', {})
    plan_total = sum(plan.values()) or 1
    print('==', os.path.basename(folder.rstrip('/\\')), '(%.0f s)' % m['match_seconds'])
    for side in ('ally', 'enemy'):
        act = m[side + '_type_activity']
        print(' %s: score %s, shots %d, damage dealt %.0f, kills %d, deaths %d, alive at end %s' % (
            side, m.get(side + '_score'), sum(a['shots'] for a in act), sum(a['damage'] for a in act), sum(a['kills'] for a in act),
            sum(a['deaths'] for a in act), m.get(side + '_alive')))
    ev = m.get('tactical_events', {})
    print(' result: winner %s (%s); supply trips home %d (out of ammunition %d, low ammunition %d), to a supply vehicle %d' % (
        m.get('winner'), m.get('reason'), ev.get('HomeSupplyReturn', 0), ev.get('HomeSupplyReturnAmmo', 0), ev.get('HomeSupplyReturnLowAmmo', 0), ev.get('FieldSupplyGo', 0)))
    print(' all shots %d, rifle shots %d, rifleman-seconds %.0f (about %.0f riflemen alive on average)' % (
        m['shots'], rifle_shots, rifle_seconds, rifle_seconds / m['match_seconds']))
    print(' per rifleman: %.1f shots in the match, %.2f per minute; ceiling at one shot per %.1f s: %.0f per minute -> %.1f%% of the ceiling' % (
        rifle_shots / (rifle_seconds / m['match_seconds']), rifle_shots / rifle_seconds * 60, INTERVAL, 60 / INTERVAL,
        100 * rifle_shots * INTERVAL / rifle_seconds))
    print(' split: aimed at a seen enemy (rifle, logged) %d, cover fire at a point %d' % (sum(shots.values()), c.get('arms_cover_fire_shots', 0)))
    print(' squad time by plan: ' + ', '.join('%s %.0f%%' % (k, 100 * v / plan_total) for k, v in sorted(plan.items(), key=lambda x: -x[1])))
    if all_squad:
        print(' squad time with a known contact %.0f%% (soldier time %.0f%%); rifle shots per soldier-minute in that time %.1f (%.0f%% of the ceiling)' % (
            100 * contact_squad / all_squad, 100 * contact_person / all_person, rifle_shots / contact_person * 60,
            100 * rifle_shots * INTERVAL / contact_person))
    engage = plan.get('ENGAGE', 0)
    if engage:
        print(' if every soldier of a squad in ENGAGE fired without pause (8 per squad): %.0f shots; actual share %.0f%%' % (
            engage * 8 / INTERVAL, 100 * m['shots'] / (engage * 8 / INTERVAL)))
    peeks = [c.get('peek_end_shots_0', 0), c.get('peek_end_shots_1', 0), c.get('peek_end_shots_2plus', 0)]
    if sum(peeks):
        print(' peeks out of cover ending with 0 / 1 / 2+ shots: %d / %d / %d (%.0f%% with none)' % (*peeks, 100 * peeks[0] / sum(peeks)))
    if shots:
        soldiers = sum(v for v in first_living.values() if v == 8)
        ranked = sorted(shots.values(), reverse=True)
        top = ranked[:max(1, len(ranked) // 10)]
        print(' aimed rifle shots: %d soldiers fired at least once (rifle squad members at first sample %d); top tenth of the shooters fired %.0f%%; most by one soldier %d' % (
            len(ranked), soldiers, 100 * sum(top) / sum(ranked), ranked[0]))


if __name__ == '__main__':
    for name in sys.argv[1:]:
        analyse(name)
