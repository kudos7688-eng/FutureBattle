# Summarise sector / cover metrics of experiment matches (2026-11-22, stage 1 of the roadmap).
# Formulas follow the sector-AI lane (STATUS 11-20 .. 11-21), so the numbers compare with its tables:
#   no-cover hit %   = victim_hit_no_cover / (no_cover + cover_peek + cover_entering + approaching_cover)
#   cover shots      = cover_shots
#   arrival cover %  = arrival_cover_slot / (arrival_cover_slot + arrival_open_slot)   (planned slots, not reached)
#   at place / tail  = arrival_walk_at_place_frames / arrival_walk_tail_cut_frames      (builds from c0f7818 on)
#   stalls           = BATTLE_MOVE_STALL events in run.log (the _DETAIL lines are not counted, so this is half of
#                      a plain `grep -c BATTLE_MOVE_STALL` on builds that print the detail line)
#   kills            = sum of 'kills' over ally_type_activity and enemy_type_activity
#   west win         = team 0 starts in the west unless the match ran with -Swap (UBattleMatch::AreaHome)
# Usage (from Setup/):
#   python -I Summarize-SectorMetrics.py "base=econ-HI-I-s*-r1" "combo=econ-SC-I-s*-r1" [--base base] [--detail]
# Each argument is label=glob; globs are relative to Benchmarks/SightTrial-20261111 unless absolute.
# The first group (or --base) is the reference: other groups are paired with it by seed and side (-swap).
import glob, json, math, os, re, statistics as st, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Benchmarks', 'SightTrial-20261111')


def load(path):
    with open(path, encoding='utf-8-sig') as f:
        return json.load(f)


def match(folder):
    name = os.path.basename(folder.rstrip('\\/'))
    r = {'name': name}
    try:
        res = load(os.path.join(folder, 'result.json'))
        m = load(os.path.join(folder, 'metrics.json'))
    except (OSError, ValueError) as e:
        r['error'] = str(e)
        return r
    cd = m.get('cover_diagnostics', {}) or {}
    g = lambda k: cd.get(k, 0) or 0
    seed = re.search(r'-s(\d+)', name)
    swap = '-swap' in name
    r.update(passed=bool(res.get('passed')), reused=bool(res.get('reusedFrom')), seed=int(seed.group(1)) if seed else -1, swap=swap)
    r['ally'], r['enemy'], w = m.get('ally_score', 0), m.get('enemy_score', 0), m.get('winner', -1)
    r['west_win'] = None if w not in (0, 1) else int((w == 0) != swap)
    r['kills'] = sum(t.get('kills', 0) for s in ('ally', 'enemy') for t in (m.get(s + '_type_activity') or []))
    hits = g('victim_hit_no_cover') + g('victim_hit_cover_peek') + g('victim_hit_cover_entering') + g('victim_hit_approaching_cover')
    r['nocover_pct'] = 100.0 * g('victim_hit_no_cover') / hits if hits else float('nan')
    r['nocover_hits'] = g('victim_hit_no_cover')
    r['cover_shots'] = g('cover_shots')
    slots = g('arrival_cover_slot') + g('arrival_open_slot')
    r['arrival_cover_pct'] = 100.0 * g('arrival_cover_slot') / slots if slots else float('nan')
    r['at_place_k'] = g('arrival_walk_at_place_frames') / 1000.0
    r['tail_cut_k'] = g('arrival_walk_tail_cut_frames') / 1000.0
    r['waypoint_pass'] = g('waypoint_pass')
    te = m.get('tactical_events', {}) or {}
    r['route_stage'] = te.get('SectorRouteStage', 0)
    r['flank_stage'] = te.get('SectorFlankStage', 0)
    r['seconds'] = m.get('match_seconds', 0)
    stalls = 0
    try:
        with open(os.path.join(folder, 'run.log'), encoding='utf-8', errors='replace') as f:
            for line in f:
                if 'BATTLE_MOVE_STALL ' in line:
                    stalls += 1
    except OSError:
        stalls = float('nan')
    r['stalls'] = stalls
    return r


COLS = [('ally', 'score0', '%.0f'), ('enemy', 'score1', '%.0f'), ('kills', 'kills', '%.0f'), ('stalls', 'stalls', '%.0f'),
        ('nocover_pct', 'nocover%', '%.1f'), ('nocover_hits', 'nocover', '%.0f'), ('cover_shots', 'covershot', '%.0f'),
        ('arrival_cover_pct', 'arrcover%', '%.1f'), ('at_place_k', 'atplace(k)', '%.0f'), ('tail_cut_k', 'tailcut(k)', '%.0f'),
        ('waypoint_pass', 'wppass', '%.0f'), ('route_stage', 'route', '%.0f'), ('flank_stage', 'flank', '%.0f'), ('seconds', 'secs', '%.0f')]
PAIRED = ['kills', 'stalls', 'nocover_pct', 'cover_shots', 'arrival_cover_pct', 'at_place_k', 'tail_cut_k']


def mean_se(values):
    v = [x for x in values if isinstance(x, (int, float)) and not math.isnan(x)]
    if not v:
        return float('nan'), float('nan')
    return st.mean(v), (st.stdev(v) / math.sqrt(len(v)) if len(v) > 1 else 0.0)


def main(argv):
    detail = '--detail' in argv
    base = None
    groups = []
    it = iter(argv)
    for a in it:
        if a == '--detail':
            continue
        if a == '--base':
            base = next(it)
            continue
        label, _, pattern = a.partition('=')
        if not pattern:
            sys.exit('expected label=glob, got ' + a)
        folders = sorted(glob.glob(pattern if os.path.isabs(pattern) else os.path.join(ROOT, pattern)))
        groups.append((label, [match(f) for f in folders if os.path.isdir(f)]))
    if not groups:
        sys.exit(__doc__ or 'no groups')
    base = base or groups[0][0]
    ref = dict(groups)[base]
    for label, rows in groups:
        ok = [r for r in rows if r.get('passed')]
        bad = [r['name'] for r in rows if not r.get('passed')]
        wins = [r['west_win'] for r in ok if r['west_win'] is not None]
        print('%s  n=%d passed=%d%s  west:east wins %d:%d  reused=%d' % (label, len(rows), len(ok), (' FAILED ' + ','.join(bad)) if bad else '',
              sum(wins), len(wins) - sum(wins), sum(r['reused'] for r in ok)))
        print('   ' + '  '.join('%s %s' % (h, (f + '+-' + f) % mean_se([r[k] for r in ok])) for k, h, f in COLS))
        if detail:
            for r in ok:
                print('     %-40s ' % r['name'] + ' '.join(f % r[k] for k, _, f in COLS))
        if label != base:
            key = lambda r: (r['seed'], r['swap'])
            refd = {key(r): r for r in ref if r.get('passed')}
            pairs = [(refd[key(r)], r) for r in ok if key(r) in refd]
            if pairs:
                parts = []
                for k in PAIRED:
                    d = [b[k] - a[k] for a, b in pairs if not (math.isnan(a[k]) or math.isnan(b[k]))]
                    if not d:
                        continue
                    m, se = mean_se(d)
                    same = all(x > 0 for x in d) or all(x < 0 for x in d)
                    parts.append('%s %+.1f+-%.1f%s' % (k, m, se, '' if same else ' (mixed)'))
                print('   vs %s, %d pairs: ' % (base, len(pairs)) + '  '.join(parts))


if __name__ == '__main__':
    main(sys.argv[1:])
