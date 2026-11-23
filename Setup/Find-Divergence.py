"""First frame where two matches differ (Docs/design-divergence-trace.md, ROADMAP stage 2).

Usage (UE's Python, isolated):
  python.exe -I Setup/Find-Divergence.py <match A dir or run.log> <match B dir or run.log> [--context 1.0]

Both matches must be run with -BattleStateTrace=<N> (the same N). Compares the BATTLE_STATE_TRACE lines frame by frame and prints
the first frame where a block hash differs (pos, hp, cover, sense, plan, match, shot - the order hints at the cause: movement,
damage, cover, perception, decisions, match rules, firing), then the BATTLE_* log lines of both matches within --context seconds
of it. With -BattleStateTraceDetail=<from>-<to> covering that time, the soldiers whose raw values differ are listed too.
Exit code 0 = no divergence in the traced frames ("state identical"), 1 = diverged, 2 = missing trace.
"""
import argparse
import os
import re
import sys

TRACE = re.compile(r"BATTLE_STATE_TRACE f=(\d+) t=([\d.]+) pos=(\w+) hp=(\w+) cover=(\w+) sense=(\w+) plan=(\w+) match=(\w+) shot=(\w+)")
UNIT = re.compile(r"BATTLE_STATE_UNIT f=(\d+) t=([\d.]+) id=(\d+) (.*)$")
EVENT = re.compile(r"(BATTLE_[A-Z_]+)\b.*?\b(?:time|t)=([\d.]+)")
BLOCKS = ["pos", "hp", "cover", "sense", "plan", "match", "shot"]


def log_path(arg):
    return os.path.join(arg, "run.log") if os.path.isdir(arg) else arg


def read(path):
    traces, units, events = {}, {}, []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = TRACE.search(line)
            if m:
                frame = int(m.group(1))
                if frame not in traces:  # a restore may repeat nothing; keep the first
                    traces[frame] = (float(m.group(2)), m.groups()[2:])
                continue
            m = UNIT.search(line)
            if m:
                units[(int(m.group(1)), int(m.group(3)))] = m.group(4).strip()
                continue
            if "BATTLE_STATE_" in line:
                continue
            m = EVENT.search(line)
            if m:
                events.append((float(m.group(2)), line[line.index(m.group(1)):].strip()))
    return traces, units, events


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--context", type=float, default=1.0)
    a = ap.parse_args(argv[1:])
    for x in (a.a, a.b):
        if not os.path.exists(log_path(x)):
            print(f"no run.log: {x}")
            return 2
    ta, ua, ea = read(log_path(a.a))
    tb, ub, eb = read(log_path(a.b))
    if not ta or not tb:
        print("missing BATTLE_STATE_TRACE lines (run both with -BattleStateTrace=<N>)")
        return 2
    common = sorted(set(ta) & set(tb))
    print(f"traced frames: A {len(ta)}, B {len(tb)}, common {len(common)}")
    for frame in common:
        (t1, h1), (t2, h2) = ta[frame], tb[frame]
        diff = [BLOCKS[i] for i in range(len(BLOCKS)) if h1[i] != h2[i]]
        if not diff:
            continue
        print(f"FIRST DIVERGENCE at frame {frame} (A t={t1:.3f}, B t={t2:.3f}): blocks {', '.join(diff)}")
        prev = [f for f in common if f < frame]
        if prev:
            print(f"  last identical traced frame: {prev[-1]} (t={ta[prev[-1]][0]:.3f})")
        ids = sorted({i for (f, i) in ua if f == frame} | {i for (f, i) in ub if f == frame})
        changed = [(i, ua.get((frame, i)), ub.get((frame, i))) for i in ids if ua.get((frame, i)) != ub.get((frame, i))]
        if changed:
            print(f"  soldiers with different raw values at that frame ({len(changed)}):")
            for i, x, y in changed[:20]:
                print(f"   id {i}\n    A: {x}\n    B: {y}")
        elif ids:
            print("  (detail lines present but identical at this frame)")
        else:
            print("  (no BATTLE_STATE_UNIT lines at this frame; rerun with -BattleStateTraceDetail=<from>-<to> around it)")
        lo, hi = t1 - a.context, t1 + a.context
        for name, ev in (("A", ea), ("B", eb)):
            near = [e for e in ev if lo <= e[0] <= hi]
            print(f"  {name} events within {a.context:.1f}s ({len(near)}):")
            for t, line in near[:25]:
                print(f"   {t:8.2f} {line[:160]}")
        return 1
    print("no divergence in the common traced frames (state identical)")
    if len(ta) != len(tb):
        print("note: one match has more traced frames (it ran longer or ended differently)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
