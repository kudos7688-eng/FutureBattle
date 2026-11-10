"""Compare completed perception benchmarks and their UE CSV profiles."""
import argparse
import importlib
import json
from pathlib import Path

profile = importlib.import_module("Summarize-CPUProfile")


def summarize(path):
    path = Path(path)
    report = json.loads(path.read_text(encoding="utf-8-sig"))
    if not report.get("binary_fingerprint"):
        raise ValueError(f"Benchmark has not completed: {path}")
    csv = profile.summarize(path.with_name(path.stem + "-profile.csv"))
    stats = csv["stats"]
    return dict(
        report=str(path), mode=report["sense_mode"],
        binary=report["binary_fingerprint"], source=report["source_fingerprint"],
        frame=stats["FrameTime"], scan=stats["Exclusive/GameThread/BattleSightScan"],
        perception=report["perception_ms_per_frame"],
        scan_ms_per_second=report["perception_scan_ms_per_frame"]["mean"] * report["frames"] / report["duration_seconds"],
        observers=report["counts"].get("perception_observers"),
        candidates=report["counts"].get("perception_candidates"),
        sight_rays=report["counts"]["sight_rays"],
        shots=report["counts"]["shots"], failures=report["counts"]["move_failures"],
        timing=report.get("sensor_timing", {}),
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("reports", nargs="+")
    parser.add_argument("--output")
    args = parser.parse_args()
    results = [summarize(p) for p in dict.fromkeys(args.reports)]
    if len({(r["binary"], r["source"]) for r in results}) != 1:
        raise ValueError("Compare reports from the same source and DLL")
    if args.output:
        Path(args.output).write_text(json.dumps(results, indent=2), encoding="utf-8")
    for r in results:
        print(Path(r["report"]).name, r["mode"],
              "frame mean/p95", round(r["frame"]["mean"], 3), round(r["frame"]["p95"], 3),
              "scan mean/p95", round(r["scan"]["mean"], 3), round(r["scan"]["p95"], 3),
              "scan ms/s", round(r["scan_ms_per_second"], 2),
              "candidates", r["candidates"], "rays", r["sight_rays"], "failures", r["failures"])
