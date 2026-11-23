# Compare trial matches with their base matches item by item (MEASURE.md, rule of 2026-11-24): the score alone misleads, because it
# is "unspent credits + living soldiers + points held at the end" and moves with the opponent's hiring. Prints, per pair and as a mean,
# the change of (team 0 - team 1) for the score and its parts, and per team: damage dealt, kills, combat deaths, leaders lost,
# hire spending. Read only; no build and no lock needed.
#   python -I Setup/Analyze-MatchDelta.py <base> <trial> [<base> <trial> ...] [--root <dir>]
# A name without a path separator is looked up under Setup/Benchmarks/SightTrial-20261111 (or --root).
import json, os, sys, statistics

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Benchmarks', 'SightTrial-20261111')

def load(name):
    path = name if os.path.isdir(name) else os.path.join(ROOT, name)
    with open(os.path.join(path, 'metrics.json'), encoding='utf-8-sig') as f:
        return json.load(f)

def facts(j):
    owners = [o.get('owner', -1) for o in j.get('final_objectives', [])]
    out = {}
    for team, side in ((0, 'ally'), (1, 'enemy')):
        acts = j.get(side + '_type_activity', [])
        ls = j.get('leader_survival', {}).get('team%d' % team, {})
        out[team] = {
            'score': j.get(side + '_score', 0), 'credits': j.get(side + '_resources', 0), 'alive': j.get(side + '_alive', 0),
            'points': owners.count(team),
            'damage': sum(a.get('damage', 0) for a in acts), 'kills': sum(a.get('kills', 0) for a in acts),
            'rifle_damage': acts[0].get('damage', 0) if acts else 0,
            'deaths': ls.get('leader_combat_deaths', 0) + ls.get('follower_combat_deaths', 0),
            'leaders_lost': ls.get('leader_combat_deaths', 0), 'hire_spent': j.get(side + '_hire_spent', 0),
        }
    return out

KEYS = ['score', 'credits', 'alive', 'points', 'damage', 'rifle_damage', 'kills', 'deaths', 'leaders_lost', 'hire_spent']

def main(argv):
    global ROOT
    if '--root' in argv:
        i = argv.index('--root'); ROOT = argv[i + 1]; del argv[i:i + 2]
    if len(argv) < 2 or len(argv) % 2:
        print(__doc__ or 'usage: Analyze-MatchDelta.py <base> <trial> [<base> <trial> ...] [--root <dir>]'); return 2
    pairs = [(argv[i], argv[i + 1]) for i in range(0, len(argv), 2)]
    rows = []
    for base, trial in pairs:
        b, t = facts(load(base)), facts(load(trial))
        rows.append((base, trial, b, t))
        print('%s -> %s' % (base, trial))
        for k in KEYS:
            print('  %-13s team0 %8.0f -> %8.0f   team1 %8.0f -> %8.0f   change of (team0 - team1) %+8.0f' % (
                k, b[0][k], t[0][k], b[1][k], t[1][k], (t[0][k] - t[1][k]) - (b[0][k] - b[1][k])))
    print('MEAN over %d pair(s): base -> trial, and how many pairs moved up' % len(rows))
    for k in KEYS:
        line = '  %-13s' % k
        for team in (0, 1):
            bs = [r[2][team][k] for r in rows]; ts = [r[3][team][k] for r in rows]
            line += ' team%d %8.0f -> %8.0f (%+5.0f%%, up %d/%d)  ' % (team, statistics.mean(bs), statistics.mean(ts),
                100 * (sum(ts) - sum(bs)) / sum(bs) if sum(bs) else 0, sum(y > x for x, y in zip(bs, ts)), len(rows))
        ds = [(r[3][0][k] - r[3][1][k]) - (r[2][0][k] - r[2][1][k]) for r in rows]
        print(line + 'diff change %+7.0f (up %d/%d)' % (statistics.mean(ds), sum(d > 0 for d in ds), len(ds)))
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
