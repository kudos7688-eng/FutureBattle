# Compare trial matches with their base matches item by item (MEASURE.md, rule of 2026-11-24): the score alone misleads, because it
# is "unspent credits + living soldiers + points held at the end" and moves with the opponent's hiring. Prints, per pair and as a mean,
# the change of (team 0 - team 1) for the score and its parts, and per team: damage dealt, kills, combat deaths, leaders lost,
# hire spending. Read only; no build and no lock needed.
#   python -I Setup/Analyze-MatchDelta.py <base> <trial> [<base> <trial> ...] [--root <dir>] [--count LABEL=REGEX[@SUBJECT]] [--no-log]
# A name without a path separator is looked up under Setup/Benchmarks/SightTrial-20261111 (or --root).
#
# After the item-by-item part (unchanged since 2026-11-24) three parts follow (2026-11-25, Docs/review-2026-11-25/03-development-policy.md 2.3):
#   CONDITIONS   build, map, seed, FPS, rendering, trial side, save check and pass mark of both matches of a pair, taken from
#                manifest.json / result.json, with a mark where they differ or are unknown, and the arguments only one of them has.
#                These are the command-line arguments, not the effective settings (defaults and per-map values are not in the manifest).
#   ACTIVATIONS  how often the intended action was logged in run.log, per team. A zero is one of: "0" = the action could have
#                happened and did not; "no subject" = what it acts on was not in the match (switch off, nothing reserved ...);
#                "0 (subject unknown)" = the log cannot tell; "no record" = no run.log (deleted or a reused result).
#                --count adds an indicator: LABEL=REGEX counts lines matching REGEX; @SUBJECT is a regex whose presence means
#                the subject existed.
#   ALL MATCHES  one line per pair, every pair given, whichever way its result went, and pairs that cannot be read.
import json, os, re, sys, statistics

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Benchmarks', 'SightTrial-20261111')

def folder(name):
    return name if os.path.isdir(name) else os.path.join(ROOT, name)

def load(name):
    with open(os.path.join(folder(name), 'metrics.json'), encoding='utf-8-sig') as f:
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

# ---- conditions (manifest.json, result.json) ----

UNKNOWN = '?'

def read_json(path):
    try:
        with open(path, encoding='utf-8-sig') as f:
            return json.load(f)
    except (OSError, ValueError):
        return None

def switch_args(args):
    # The arguments that can change a match: everything from the map on, without the output paths.
    out = []
    for a in args[1:]:
        if a.startswith(('-abslog=', '-BattleSightExperiment=', '-BattleMapStats=', '-trace', '-tracefile')):
            continue
        out.append(a)
    return out

def conditions(name, j):
    path = folder(name)
    man, res = read_json(os.path.join(path, 'manifest.json')), read_json(os.path.join(path, 'result.json'))
    c = {k: UNKNOWN for k in ('build', 'dll', 'map', 'seed', 'fps', 'render', 'side', 'save', 'passed')}
    c['args'] = None
    if res is not None:
        c['passed'] = str(res.get('passed', UNKNOWN))
    if man is None:
        return c
    args = [str(a) for a in (man.get('arguments') or [])]
    if not args:
        return c                                                # a manifest without arguments tells nothing: everything stays unknown
    c['args'] = switch_args(args)
    project = args[0].strip('"').replace('\\', '/') if args else ''
    m = re.search(r'/BuildSnapshots/([^/]+)/', project)
    if m:
        c['build'] = m.group(1)                                 # frozen build started from its own copy
    else:
        m = re.search(r'/(FutureBattle[^/]*)/Game/', project)
        if m:
            c['build'] = m.group(1) + ' (not frozen)'           # started from a work tree: the sources may have moved since
    if man.get('reusedFrom'):
        c['build'] += ' reused:' + os.path.basename(str(man['reusedFrom']).replace('\\', '/').rstrip('/'))
    c['dll'] = str(man.get('dll') or UNKNOWN)[:8]
    for a in args:
        m = re.match(r'/Game/.*Lvl_Battle(\w+)$', a)
        if m:
            c['map'] = m.group(1) + ' ' + str(man.get('map') or UNKNOWN)[:6]
    def value(key):
        for a in args:
            if a.lower().startswith(key.lower() + '='):
                return a.split('=', 1)[1]
        return None
    c['seed'] = value('-BattleTrialSeed') or UNKNOWN
    fps = value('-FPS')
    c['fps'] = (fps + ('' if '-UseFixedTimeStep' in args else ' (not fixed)')) if fps else UNKNOWN
    c['render'] = 'none' if '-nullrhi' in args else 'rendered'
    # Trial side: every "...Team=" / "...Teams=" argument, as passed. "...Team=N" is a team number, "...Teams=N" a bit mask
    # (1 = team 0, 2 = team 1, 3 = both); -BattleSniperTeam is the sniper team unit, not a side. None of them = no side argument.
    sides = [a.lstrip('-') for a in args if re.match(r'-Battle\w*Teams?=\d+$', a) and not a.startswith('-BattleSniperTeam=')]
    c['side'] = ' '.join(sides) if sides else 'no side argument'
    at = value('-BattleVerifySaveAt')
    if '-BattleVerifyMixedSave' in args:
        c['save'] = 'round trip' + (' at %s s' % at if at else '') + (' verified' if j and j.get('mixed_save_round_trip_verified') else ' NOT verified')
    else:
        c['save'] = 'no round trip'
    return c

COND_ROWS = [('build', 'build'), ('dll', 'dll'), ('map', 'map'), ('seed', 'seed'), ('fps', 'FPS'), ('render', 'rendering'),
             ('side', 'trial side'), ('save', 'save check'), ('passed', 'passed')]
# A difference in these is what a base/trial pair may differ in; a difference in the others makes the pair incomparable.
MAY_DIFFER = ('build', 'dll', 'side')

def print_conditions(base, trial, cb, ct):
    print('%s -> %s' % (base, trial))
    notes = []
    for key, label in COND_ROWS:
        b, t = cb[key], ct[key]
        mark = ''
        if UNKNOWN in (b, t) or b.startswith(UNKNOWN) or t.startswith(UNKNOWN):
            mark = '   ?? unknown'; notes.append(label + ' unknown')
        elif b != t:
            mark = '   (differs)' if key in MAY_DIFFER else '   !! MISMATCH'
            if key not in MAY_DIFFER: notes.append(label + ' mismatch')
        if key == 'passed' and 'True' not in (b, t) and not mark: mark = '   !! not passed'
        if key == 'passed' and (b == 'False' or t == 'False'): notes.append('not passed')
        print('  %-11s %-44s | %-44s%s' % (label, b, t, mark))
    if cb['args'] is None or ct['args'] is None:
        print('  arguments   no manifest.json for %s' % ' and '.join(n for n, c in ((base, cb), (trial, ct)) if c['args'] is None))
        notes.append('arguments unknown')
    else:
        only_b = [a for a in cb['args'] if a not in ct['args']]; only_t = [a for a in ct['args'] if a not in cb['args']]
        print('  only base   %s' % (' '.join(only_b) or '(nothing)'))
        print('  only trial  %s' % (' '.join(only_t) or '(nothing)'))
        if not only_b and not only_t and cb['dll'] == ct['dll']: notes.append('same arguments and same dll')
        if only_b: notes.append('base has %d argument(s) the trial lacks' % len(only_b))
    if 'not frozen' in cb['build'] or 'not frozen' in ct['build']: notes.append('build not frozen')
    return notes

# ---- activations (run.log) ----

# label, regex counted, subject rule. A subject rule is a list of tests tried in order; the first that decides wins:
#   ('arg_off', regex)  an argument matching -> no subject          ('arg_on', regex)  an argument matching -> subject
#   ('log', regex)      a log line matching -> subject              ('need_arg', regex) no argument matching -> no subject (default-0 switch)
#   ('need_log', regex, why) no log line matching -> no subject     ('yes',) subject        nothing decides -> unknown
INDICATORS = [
    ('BATTLE_AREA_HIRE', r'BATTLE_AREA_HIRE ', [('arg_on', r'-BattleArea$'), ('need_arg', r'-BattleArea$')]),
    ('BATTLE_COMPOSITION_EXECUTE success=1', r'BATTLE_COMPOSITION_EXECUTE .*success=1',
     [('need_arg', r'-BattleAreaCompositionPlanner=[1-9]'), ('need_arg', r'-BattleAreaCompositionPlannerTeams=[1-9]'),
      ('need_log', r'BATTLE_COMPOSITION_DECISION ', 'the planner logged no decision (not this team, or not reached)'), ('yes',)]),
    ('BATTLE_HIRE_RESERVE op=buy', r'BATTLE_HIRE_RESERVE .*op=buy',
     [('need_log', r'BATTLE_HIRE_RESERVE .*op=add', 'nothing was reserved'), ('yes',)]),
    ('BATTLE_ARMS_ASSAULT', r'BATTLE_ARMS_ASSAULT ', [('arg_off', r'-BattleArmsAssault=0$'), ('arg_on', r'-BattleArmsAssault=2$')]),
    ('BATTLE_ARMS_CHARGE', r'BATTLE_ARMS_CHARGE ', [('arg_off', r'-BattleArmsCharge=0$')]),
    ('BATTLE_ARMS_GRENADE', r'BATTLE_ARMS_GRENADE ', [('arg_off', r'-BattleGrenades=0$'), ('arg_on', r'-BattleGrenades=[1-9]'), ('log', r'BATTLE_ARMS_')]),
    ('BATTLE_ARMS_MELEE', r'BATTLE_ARMS_MELEE ', [('arg_off', r'-BattleMelee=0$'), ('arg_on', r'-BattleMelee=[1-9]'), ('log', r'BATTLE_ARMS_')]),
    ('BATTLE_FRONT_FOCUS2_GROUP', r'BATTLE_FRONT_FOCUS2_GROUP ',
     [('need_arg', r'-BattleFrontPlan=[1-9]'), ('need_arg', r'-BattleFrontPlanFocus2=[1-9]'), ('yes',)]),
    ('BATTLE_FRONT_ORDER_STEP', r'BATTLE_FRONT_ORDER_STEP ',
     [('need_arg', r'-BattleFrontSectionOrders=[1-9]'), ('need_arg', r'-BattleFrontPlan=[1-9]'), ('need_arg', r'-BattleFrontPlanPlayer=[1-9]'),
      ('need_log', r'BATTLE_FRONT_PLAYER_ROLE .*role=[56]|BATTLE_FRONT_ORDER_SCRIPT ', 'no section was set to advance or retreat'), ('yes',)]),
    # The resume lines carry no team: they are counted under "no team". ok=0 is a resume that failed.
    ('BATTLE_MISSION_RESUME ok=1', r'BATTLE_MISSION_RESUME .*\bok=[1-9]', []),
    ('BATTLE_MISSION_RESUME ok=0', r'BATTLE_MISSION_RESUME .*\bok=0', []),
]

def scan_log(name, indicators):
    # -> None when there is no run.log, else {label: {'n': [team0, team1, other], 'seen': set of subject regexes found}}
    path = os.path.join(folder(name), 'run.log')
    if not os.path.isfile(path):
        return None
    counts = {label: [0, 0, 0] for label, _, _ in indicators}
    rx = [(label, re.compile(pattern)) for label, pattern, _ in indicators]
    subjects = {}
    for _, _, rule in indicators:
        for test in rule:
            if test[0] in ('log', 'need_log'): subjects[test[1]] = re.compile(test[1])
    seen, battle_lines = set(), 0
    team = re.compile(r'\bteam=(\d+)')
    with open(path, 'rb') as f:
        bom = f.read(2)
    with open(path, encoding='utf-16' if bom in (b'\xff\xfe', b'\xfe\xff') else 'utf-8', errors='replace') as f:
        for line in f:
            if 'BATTLE_' not in line: continue
            battle_lines += 1
            for label, r in rx:
                if r.search(line):
                    m = team.search(line)
                    counts[label][int(m.group(1)) if m and m.group(1) in '01' else 2] += 1
            for key, r in subjects.items():
                if key not in seen and r.search(line): seen.add(key)
    return {'n': counts, 'seen': seen, 'lines': battle_lines}

def switch_name(pattern):
    return re.sub(r'=\[.*$|\$$', '', pattern)

def subject(rule, args, seen):
    # -> (True/False/None, why)
    # args None = no manifest: the tests on arguments decide nothing then.
    def has(pattern): return any(re.search(pattern, a) for a in args)
    for test in rule:
        kind = test[0]
        if args is None and kind in ('arg_off', 'arg_on', 'need_arg'):
            if kind == 'need_arg': return None, ''
            continue
        if kind == 'yes': return True, ''
        if kind == 'arg_off' and has(test[1]): return False, 'switched off (%s)' % switch_name(test[1])
        if kind == 'arg_on' and has(test[1]): return True, ''
        if kind == 'log' and test[1] in seen: return True, ''
        if kind == 'need_arg' and not has(test[1]): return False, 'switch not passed (%s)' % switch_name(test[1])
        if kind == 'need_log' and test[1] not in seen: return False, test[2]
    return None, ''

def activation_text(label, rule, log, args):
    if log is None: return 'no record (no run.log)', None
    if not log['lines']: return 'no record (run.log has no BATTLE_ lines)', None
    n = log['n'][label]
    if sum(n):
        return 'team0 %d  team1 %d%s' % (n[0], n[1], '  no team %d' % n[2] if n[2] else ''), n
    has, why = subject(rule, args, log['seen'])
    if has is False: return 'no subject: ' + why, n
    if has is None: return '0 (subject unknown: the log does not say whether it could happen)', n
    return '0 (could have happened, did not)', n

def short_activation(text, n):
    if n is not None and sum(n): return '%d/%d' % (n[0], n[1]) + ('+%d no team' % n[2] if n[2] else '')
    return 'no rec' if text.startswith('no record') else 'no subj' if text.startswith('no subject') else '0?' if 'unknown' in text else '0'

def main(argv):
    global ROOT
    if '--root' in argv:
        i = argv.index('--root'); ROOT = argv[i + 1]; del argv[i:i + 2]
    indicators, use_log = list(INDICATORS), True
    if '--no-log' in argv:
        argv.remove('--no-log'); use_log = False
    while '--count' in argv:
        i = argv.index('--count')
        if i + 1 >= len(argv):
            print('--count needs LABEL=REGEX[@SUBJECT]'); return 2
        spec = argv[i + 1]; del argv[i:i + 2]
        label, _, rest = spec.partition('=')
        pattern, _, subj = (rest or re.escape(label)).partition('@')
        try:
            re.compile(pattern); re.compile(subj)
        except re.error as e:
            print('--count %s: bad regular expression (%s)' % (spec, e)); return 2
        indicators.append((label, pattern, [('need_log', subj, 'no line matching ' + subj), ('yes',)] if subj else []))
    if len(argv) < 2 or len(argv) % 2:
        print(__doc__ or 'usage: Analyze-MatchDelta.py <base> <trial> [<base> <trial> ...] [--root <dir>] [--count LABEL=REGEX[@SUBJECT]] [--no-log]'); return 2
    pairs = [(argv[i], argv[i + 1]) for i in range(0, len(argv), 2)]
    rows, unreadable = [], {}
    for base, trial in pairs:
        try:
            jb, jt = load(base), load(trial)
        except (OSError, ValueError) as e:
            # A pair that cannot be read stays in the list (ALL MATCHES) instead of stopping the run or dropping out silently.
            unreadable[(base, trial)] = '%s: %s' % (type(e).__name__, e); continue
        b, t = facts(jb), facts(jt)
        rows.append((base, trial, b, t, jb, jt))
        print('%s -> %s' % (base, trial))
        for k in KEYS:
            print('  %-13s team0 %8.0f -> %8.0f   team1 %8.0f -> %8.0f   change of (team0 - team1) %+8.0f' % (
                k, b[0][k], t[0][k], b[1][k], t[1][k], (t[0][k] - t[1][k]) - (b[0][k] - b[1][k])))
    if rows:
        print('MEAN over %d pair(s): base -> trial, and how many pairs moved up' % len(rows))
        for k in KEYS:
            line = '  %-13s' % k
            for team in (0, 1):
                bs = [r[2][team][k] for r in rows]; ts = [r[3][team][k] for r in rows]
                line += ' team%d %8.0f -> %8.0f (%+5.0f%%, up %d/%d)  ' % (team, statistics.mean(bs), statistics.mean(ts),
                    100 * (sum(ts) - sum(bs)) / sum(bs) if sum(bs) else 0, sum(y > x for x, y in zip(bs, ts)), len(rows))
            ds = [(r[3][0][k] - r[3][1][k]) - (r[2][0][k] - r[2][1][k]) for r in rows]
            print(line + 'diff change %+7.0f (up %d/%d)' % (statistics.mean(ds), sum(d > 0 for d in ds), len(ds)))

    print('')
    print('CONDITIONS (command-line arguments of each match, not the effective settings): base | trial')
    conds, notes = {}, {}
    for base, trial, b, t, jb, jt in rows:
        cb, ct = conditions(base, jb), conditions(trial, jt)
        conds[(base, trial)] = (cb, ct)
        notes[(base, trial)] = print_conditions(base, trial, cb, ct)

    print('')
    acts = {}
    if use_log:
        print('ACTIVATIONS (lines in run.log): base | trial')
        for base, trial, b, t, jb, jt in rows:
            lb, lt = scan_log(base, indicators), scan_log(trial, indicators)
            cb, ct = conds[(base, trial)]
            print('%s -> %s' % (base, trial))
            acts[(base, trial)] = []
            for label, _, rule in indicators:
                tb, nb = activation_text(label, rule, lb, cb['args']); tt, nt = activation_text(label, rule, lt, ct['args'])
                print('  %-37s %-34s | %s' % (label, tb, tt))
                acts[(base, trial)].append((label, short_activation(tb, nb), short_activation(tt, nt)))
    else:
        print('ACTIVATIONS skipped (--no-log)')

    print('')
    print('ALL MATCHES: %d pair(s) given, %d read. Change of (team0 - team1) from base to trial; nothing is left out by its direction.' % (len(pairs), len(rows)))
    print('  %-52s %7s %7s %7s %7s  %s' % ('base -> trial', 'score', 'damage', 'kills', 'deaths', 'notes'))
    by_pair = {(r[0], r[1]): r for r in rows}
    for base, trial in pairs:
        title = ('%s -> %s' % (base, trial))[:52]
        if (base, trial) in unreadable:
            print('  %-52s %s' % (title, 'NOT READ  ' + unreadable[(base, trial)])); continue
        _, _, b, t, _, _ = by_pair[(base, trial)]
        d = {k: (t[0][k] - t[1][k]) - (b[0][k] - b[1][k]) for k in ('score', 'damage', 'kills', 'deaths')}
        print('  %-52s %+7.0f %+7.0f %+7.0f %+7.0f  %s' % (title, d['score'], d['damage'], d['kills'], d['deaths'], '; '.join(notes[(base, trial)]) or '-'))
        if (base, trial) in acts:
            fired = [a for a in acts[(base, trial)] if a[1] not in ('no subj', '0?') or a[2] not in ('no subj', '0?')]
            print('      activations base|trial (team0/team1): %s' % ('  '.join('%s %s|%s' % (a[0].replace('BATTLE_', ''), a[1], a[2]) for a in fired) or 'none with a subject'))
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
