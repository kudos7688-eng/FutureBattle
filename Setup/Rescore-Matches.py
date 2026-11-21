"""Re-score finished matches under other score rules (ROADMAP stage 6, Docs/design-score-economy.md). No match is run.

Usage (UE's Python, isolated):
  python.exe -I Setup/Rescore-Matches.py --tag e1 [--baseline HI] [--root <SightTrial folder>] [--root ...]
  python.exe -I Setup/Rescore-Matches.py --tag ga1 --baseline FL0 --root ../FutureBattle-groupui/Setup/Benchmarks/SightTrial-20261111

For a one-team screen (Run-StrategyScreen folders econ-<cond>-<map>-s<seed>[-swap]-<tag>[f60][s1|s2]t<team> against the baseline
econ-<baseline>-<map>-s<seed>[-swap]-<tag>[f60][s1|s2]) it prints, per condition and map, the gain of the trial team under three rules:
  now  : the current Area score from metrics.json (resources + living soldiers + held points x 20 x value)
  B    : war score (ROADMAP 2026-11-14): points held over time (1 per point per 5 s, from *_point_owned_seconds)
         + enemy losses (leader_survival of the opponent: combat deaths + followers stopped by their leader's death) + resources
  C    : resources + living soldiers + points held over time (1 per point per 60 s) + held points at the end x 10
Gain per seed = mean over trial teams, maps' sides (swap) of (trial team lead - the same side's lead in the baseline match).
Mean and se over seeds (se = sample sd / sqrt(n)). B and C are approximations from the metrics (unit-shot losses only).
Folders without the needed metrics are skipped and counted. Both-team matrices (no t0/t1 suffix) are compared as condition vs
baseline with the lead of team 0 (ally) over the same seed/map/swap.
"""
import argparse
import glob
import json
import math
import os
import re
import sys
from collections import defaultdict

NAME = re.compile(r"^econ-(?P<cond>.+?)-(?P<map>[A-I])-s(?P<seed>\d+)(?P<swap>-swap)?-(?P<tag>.+?)(?P<conf>f\d+)?(?P<stage>s[12])?(?P<team>t[01])?$")


def scores(m):
    """(now, B, C) per team, or None for a rule the metrics cannot give."""
    now = [m.get("ally_score"), m.get("enemy_score")]
    res = [m.get("ally_resources"), m.get("enemy_resources")]
    alive = [m.get("ally_alive"), m.get("enemy_alive")]
    held = [m.get("ally_point_owned_seconds"), m.get("enemy_point_owned_seconds")]
    ls = m.get("leader_survival") or {}
    loss = []
    for t in (0, 1):
        x = ls.get(f"team{t}")
        loss.append(None if not x else x.get("leader_combat_deaths", 0) + x.get("follower_combat_deaths", 0) + x.get("leader_loss_followers", 0))
    final = m.get("final_objectives") or []
    owned_end = [sum(1 for o in final if o.get("owner") == t) for t in (0, 1)]
    out = {"now": now if None not in now else None, "B": None, "C": None}
    if None not in res and None not in held and None not in loss:
        out["B"] = [sum(held[t]) / 5.0 + loss[1 - t] + res[t] for t in (0, 1)]
    if None not in res and None not in held and None not in alive:
        out["C"] = [res[t] + alive[t] + sum(held[t]) / 60.0 + 10 * owned_end[t] for t in (0, 1)]
    return out


def load(folder):
    p = os.path.join(folder, "metrics.json")
    r = os.path.join(folder, "result.json")
    if not os.path.exists(p):
        return None
    if os.path.exists(r):
        try:
            if not json.load(open(r, encoding="utf-8-sig")).get("passed", False):
                return None
        except ValueError:
            return None
    try:
        return json.load(open(p, encoding="utf-8-sig"))
    except ValueError:
        return None


def mean_se(v):
    if not v:
        return None, None
    m = sum(v) / len(v)
    if len(v) < 2:
        return m, None
    sd = math.sqrt(sum((x - m) ** 2 for x in v) / (len(v) - 1))
    return m, sd / math.sqrt(len(v))


def fmt(m, se):
    if m is None:
        return "     n/a     "
    return f"{m:+7.1f}" + (f" +-{se:4.1f}" if se is not None else "       ")


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tag", required=True)
    ap.add_argument("--baseline", default="HI")
    ap.add_argument("--root", action="append", default=[])
    a = ap.parse_args(argv[1:])
    roots = a.root or [os.path.join(os.path.dirname(os.path.abspath(__file__)), "Benchmarks", "SightTrial-20261111")]
    runs = {}
    for root in roots:
        for d in glob.glob(os.path.join(root, f"econ-*-{a.tag}*")):
            n = NAME.match(os.path.basename(d))
            if not n or n.group("tag") != a.tag:
                continue
            runs[os.path.basename(d)] = (n.groupdict(), d)
    base = {}
    for name, (g, d) in runs.items():
        if g["cond"] == a.baseline and not g["team"]:
            base[(g["map"], g["seed"], bool(g["swap"]), g["conf"] or "", g["stage"] or "")] = d
    gains = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))  # (cond, map, conf) -> rule -> seed -> [gain]
    skipped = 0
    cache = {}
    def sc(d):
        if d not in cache:
            m = load(d)
            cache[d] = scores(m) if m else None
        return cache[d]
    for name, (g, d) in runs.items():
        if g["cond"] == a.baseline and not g["team"]:
            continue
        key = (g["map"], g["seed"], bool(g["swap"]), g["conf"] or "", g["stage"] or "")
        bd = base.get(key)
        if not bd:
            skipped += 1
            continue
        s, b = sc(d), sc(bd)
        if not s or not b:
            skipped += 1
            continue
        t = int(g["team"][1]) if g["team"] else 0
        for rule in ("now", "B", "C"):
            if s[rule] is None or b[rule] is None:
                continue
            lead = s[rule][t] - s[rule][1 - t]
            blead = b[rule][t] - b[rule][1 - t]
            gains[(g["cond"], g["map"], g["conf"] or "")][rule][g["seed"]].append(lead - blead)
    print(f"tag {a.tag}, baseline {a.baseline}: {len(runs)} folders, {len(base)} baseline matches, {skipped} skipped (no baseline / failed / missing metrics)")
    print(f"{'condition':<12}{'map':<4}{'fps':<5}{'seeds':>5}  {'now':^14}{'B (war)':^14}{'C (mixed)':^14} flip")
    for (cond, mp, conf) in sorted(gains):
        row = gains[(cond, mp, conf)]
        cells = []
        signs = []
        n = 0
        for rule in ("now", "B", "C"):
            per_seed = [sum(v) / len(v) for v in row[rule].values()] if row.get(rule) else []
            n = max(n, len(per_seed))
            m, se = mean_se(per_seed)
            cells.append(fmt(m, se))
            signs.append(None if m is None or se is None or abs(m) < 2 * se else (m > 0))
        clear = [x for x in signs if x is not None]
        flip = "yes" if len(set(clear)) > 1 else ""
        print(f"{cond:<12}{mp:<4}{(conf or '-'):<5}{n:>5}  {cells[0]:^14}{cells[1]:^14}{cells[2]:^14} {flip}")
    print("flip = the rules disagree on the sign where the difference is beyond 2 se")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
