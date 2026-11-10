"""Summarize UE CSV CPU captures; use the final header and exclude metadata.

Usage: python Summarize-CPUProfile.py <benchmark-profile.csv> [...]
Exclusive GameThread times can be compared without double-counting nested scopes.
Worker totals and render-thread times overlap; never add them to GameThread time.
"""
import csv
import json
import math
import statistics
import sys
from pathlib import Path


def summarize(path):
    with Path(path).open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.reader(stream))
    headers = [row for row in rows if row and row[0] == "EVENTS"]
    if not headers or not rows[-1][0].startswith("[HasHeaderRowAtEnd]"):
        raise ValueError(f"Incomplete UE CSV export: {path}")
    header = headers[-1]  # UE can append new stat columns during capture.
    data = [row for row in rows if row and row[0] != "EVENTS" and not row[0].startswith("[")]
    # Capture begins/ends during GameMode tick. The final frame also requests
    # a screenshot and serializes JSON; neither belongs to steady combat.
    data = data[1:-1]
    if not data:
        raise ValueError(f"No complete interior frames: {path}")
    result = {}
    for index, name in enumerate(header):
        if not (name.startswith("Exclusive/GameThread/") or name in (
            "FrameTime", "GameThreadTime", "RenderThreadTime", "GPUTime",
            "AnimationParallelEvaluation/TotalTaskTime", "AnimationParallelEvaluation/NumberOfTasks")):
            continue
        values = [float(row[index]) if index < len(row) and row[index] else 0.0 for row in data]
        values.sort()
        result[name] = dict(mean=statistics.mean(values), p95=values[math.ceil(len(values)*.95)-1], max=values[-1])
        if name == "FrameTime":
            result[name]["p99"] = values[math.ceil(len(values) * .99) - 1]
            result[name]["over_16_67_pct"] = 100 * sum(value > 16.67 for value in values) / len(values)
    return dict(file=str(path), frames=len(data), discarded_boundary_frames=2, stats=result)


if __name__ == "__main__":
    print(json.dumps([summarize(path) for path in sys.argv[1:]], indent=2))
