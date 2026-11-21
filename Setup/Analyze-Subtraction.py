"""Squad-type efficiency for the subtraction evaluation (Docs/design-subtraction-eval.md).

Usage (UE's Python, isolated):
  python.exe -I Setup/Analyze-Subtraction.py <match output dir> [...]
  python.exe -I Setup/Analyze-Subtraction.py --glob "econ-ALL-*-r1" [--root Setup/Benchmarks/SightTrial-20261111]

Sums metrics.json ally_type_activity / enemy_type_activity (both teams) over the given matches and prints, per squad type:
squads bought (from BATTLE_AREA_HIRE lines in run.log when present), soldier-seconds, kills, deaths, damage dealt and taken,
kills per death, and kills / damage per resource spent, each also relative to rifle squads (= 1.00).
The score gains of W1..W9 come from Run-StrategyScreen (-Baseline ALL); this script adds the per-type efficiency beside them.
Costs are the UBattleTuning defaults of 2026-11-22; pass --cost TYPE=VALUE to override (e.g. --cost 8=40).
"""
import argparse
import glob
import json
import os
import re
import sys

NAMES = ["rifle", "heavy", "scout", "anti-tank", "tank", "armoured car", "mortar", "SPG", "exo", "supply"]
COSTS = {0: 12, 1: 15, 2: 8, 3: 12, 4: 40, 5: 20, 6: 14, 7: 35, 8: 30, 9: 10}
HIRE = re.compile(r"BATTLE_AREA_HIRE team=(\d) squad=(\d+) auto=\d cost=(\d+).*?(?: type=(\d+))?\s*$")
KEYS = ["person_seconds", "shots", "damage", "kills", "damage_taken", "deaths"]


def type_of_cost(cost, costs):
    # A purchase log line has the cost but not the type; the cost identifies the type when costs are distinct.
    hits = [t for t, c in costs.items() if c == cost]
    return hits[0] if len(hits) == 1 else None


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("dirs", nargs="*")
    ap.add_argument("--glob")
    ap.add_argument("--root", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "Benchmarks", "SightTrial-20261111"))
    ap.add_argument("--cost", action="append", default=[])
    a = ap.parse_args(argv[1:])
    costs = dict(COSTS)
    for c in a.cost:
        t, v = c.split("=")
        costs[int(t)] = int(v)
    dirs = list(a.dirs)
    if a.glob:
        dirs += sorted(d for d in glob.glob(os.path.join(a.root, a.glob)) if os.path.isdir(d))
    if not dirs:
        ap.print_help()
        return 2
    total = {t: dict.fromkeys(KEYS, 0.0) for t in range(10)}
    bought = {t: 0 for t in range(10)}
    unknown_buys = 0
    used = 0
    for d in dirs:
        mp = os.path.join(d, "metrics.json")
        if not os.path.exists(mp):
            print(f"skip (no metrics.json): {d}")
            continue
        m = json.load(open(mp, encoding="utf-8"))
        for side in ("ally_type_activity", "enemy_type_activity"):
            for rec in m.get(side, []):
                t = int(rec.get("type", -1))
                if t in total:
                    for k in KEYS:
                        total[t][k] += float(rec.get(k, 0))
        log = os.path.join(d, "run.log")
        if os.path.exists(log):
            seen = set()
            with open(log, encoding="utf-8", errors="replace") as f:
                for line in f:
                    h = HIRE.search(line)
                    if h and (h.group(1), h.group(2)) not in seen:  # a restore may repeat a line
                        seen.add((h.group(1), h.group(2)))
                        t = int(h.group(4)) if h.group(4) else type_of_cost(int(h.group(3)), costs)
                        if t is None:
                            unknown_buys += 1
                        else:
                            bought[t] += 1
        used += 1
    print(f"matches: {used}  (purchases with an ambiguous cost: {unknown_buys})")
    rifle = total[0]
    rifle_spent = max(1, bought[0]) * costs[0]
    def rel(v, base):
        return f"{v / base:5.2f}" if base > 0 else "  -  "
    rk = rifle["kills"] / rifle_spent if bought[0] else 0
    print(f"{'type':<13}{'bought':>7}{'soldier-s':>11}{'kills':>8}{'deaths':>8}{'K/D':>6}{'kills/res':>10}{'x rifle':>8}{'dmg/res':>9}{'x rifle':>8}")
    for t in range(10):
        x = total[t]
        if x["person_seconds"] <= 0 and bought[t] == 0:
            continue
        spent = bought[t] * costs[t]
        kd = x["kills"] / x["deaths"] if x["deaths"] > 0 else float("inf")
        kpr = x["kills"] / spent if spent else 0
        dpr = x["damage"] / spent if spent else 0
        rd = rifle["damage"] / rifle_spent if bought[0] else 0
        print(f"{NAMES[t]:<13}{bought[t]:>7}{x['person_seconds']:>11.0f}{x['kills']:>8.0f}{x['deaths']:>8.0f}{kd:>6.2f}{kpr:>10.2f}{rel(kpr, rk):>8}{dpr:>9.1f}{rel(dpr, rd):>8}")
    print("Note: rifle 'bought' counts purchases only; the 25 starting squads are not in it, so rifle per-resource values are"
          " upper bounds. Compare the special types with each other and with W_X score gains, not only with rifles.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
