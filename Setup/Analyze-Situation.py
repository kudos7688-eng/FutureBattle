"""Summarize the strategy layer's situation map and focus choices from match logs (Docs/design-strategy-layer.md).

Usage (UE's Python, isolated):
  python.exe -I Setup/Analyze-Situation.py <match output dir or run.log> [...]

Reads BATTLE_SITUATION lines (-BattleSituationLog=1 or -BattleStrategy>=1) and BATTLE_STRATEGY lines (-BattleStrategy>=2).
For each match and team: how far the known enemy count is from the true count per theatre, how often a theatre is unknown,
and how often the state agrees with the state computed from the true enemy power (evaluation only; the AI never reads it).
"""
import os
import re
import sys
from collections import defaultdict

SIT = re.compile(r"BATTLE_SITUATION team=(\d) t=(\d+) theatre=(\d+) kind=(\w+) state=(\w+) own=(\d+) ownP=([\d.]+) fresh=(\d+) "
                 r"mem=([\d.]+) knownP=([\d.]+) seenAge=(-?\d+) pointAge=(-?\d+) owner=(-?\d+) \| true=(\d+) trueP=([\d.]+)")
STRAT = re.compile(r"BATTLE_STRATEGY team=(\d) t=([\d.]+) phase=(\d) focus=(-?\d+) old=(-?\d+) reason=(\w+)")


def true_state(own_p, true_n, true_p):
    # Same rule as BuildSituation, with the true enemy power in place of the known one (no "unknown" for the truth).
    if true_n >= 3 and true_p >= own_p * 1.2:
        return "enemy"
    if own_p > 0 and own_p >= true_p * 1.5:
        return "own"
    return "even"


def log_path(arg):
    return os.path.join(arg, "run.log") if os.path.isdir(arg) else arg


def summarize(path):
    rows = defaultdict(list)
    strat = defaultdict(list)
    seen = set()
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = SIT.search(line)
            if m:
                key = (int(m.group(1)), int(m.group(2)), int(m.group(3)))
                if key in seen:  # a restore may log the same bucket twice
                    continue
                seen.add(key)
                team, t, th, kind, state = int(m.group(1)), int(m.group(2)), int(m.group(3)), m.group(4), m.group(5)
                own_p = float(m.group(7))
                known = int(m.group(8)) + float(m.group(9))
                true_n, true_p = int(m.group(14)), float(m.group(15))
                rows[team].append((t, th, kind, state, own_p, known, float(m.group(10)), true_n, true_p))
                continue
            m = STRAT.search(line)
            if m:
                strat[int(m.group(1))].append((float(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5)), m.group(6)))
    out = []
    for team in sorted(set(rows) | set(strat)):
        r = [x for x in rows.get(team, []) if x[2] == "point"]
        if r:
            n = len(r)
            unknown = sum(1 for x in r if x[3] == "unknown")
            known_rows = [x for x in r if x[3] != "unknown"]
            err = sum(abs(x[5] - x[7]) for x in known_rows) / max(1, len(known_rows))
            under = sum(max(0.0, x[7] - x[5]) for x in known_rows) / max(1, len(known_rows))
            agree = sum(1 for x in known_rows if x[3] == true_state(x[4], x[7], x[8]))
            hidden = sum(x[7] for x in r if x[3] == "unknown") / max(1, unknown)
            out.append(f"  team {team}: point theatres x times = {n}, unknown {unknown / n:.0%} (true enemies there on average {hidden:.1f}), "
                       f"known-vs-true error {err:.1f} soldiers (missed {under:.1f}), state agrees with truth {agree / max(1, len(known_rows)):.0%}")
        s = strat.get(team, [])
        if s:
            switches = sum(1 for x in s if x[2] != x[3])
            focus_times = defaultdict(float)
            for i, x in enumerate(s):
                end = s[i + 1][0] if i + 1 < len(s) else None
                if x[2] >= 0 and end is not None:
                    focus_times[x[2]] += end - x[0]
            reasons = defaultdict(int)
            for x in s:
                reasons[x[4]] += 1
            out.append(f"  team {team}: strategy reviews {len(s)}, focus switches {switches}, first focus at {s[0][0]:.0f}s, "
                       f"reasons {dict(reasons)}, focus seconds by point {dict(sorted(focus_times.items()))}")
    return out


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    for arg in argv[1:]:
        path = log_path(arg)
        if not os.path.exists(path):
            print(f"{arg}: no run.log")
            continue
        print(arg)
        lines = summarize(path)
        print("\n".join(lines) if lines else "  (no BATTLE_SITUATION / BATTLE_STRATEGY lines)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
