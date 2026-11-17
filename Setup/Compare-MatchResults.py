# Compares the metrics.json of two matches run with the same arguments and reports whether the match itself was identical.
# Timing values (frame times, AI milliseconds, path query times) are ignored; everything else must be equal.
# Usage (UE's Python):
#   python.exe -I Setup/Compare-MatchResults.py <before> <after> [<before> <after> ...]
# Each name is a folder under Setup/Benchmarks/SightTrial-20261111 (or a path to a folder holding metrics.json).
# Exit code 0 = every pair identical, 1 = a difference or a missing file.
import hashlib, io, json, os, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Benchmarks', 'SightTrial-20261111')
TIMING = ('duration_seconds', 'custom_ai_ms_per_second', 'sensor_timing', 'path_query_ms', 'engine_thread_samples_available')


def is_timing(key):
    return key in TIMING or '_ms' in key or key.endswith('_scope')


def load(name):
    folder = name if os.path.isdir(name) else os.path.join(ROOT, name)
    with io.open(os.path.join(folder, 'metrics.json'), encoding='utf-8-sig') as f:
        return json.load(f)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()[:12]


def main(names):
    if len(names) < 2 or len(names) % 2:
        print(__doc__ or 'usage: Compare-MatchResults.py <before> <after> ...')
        return 1
    failed = False
    for before, after in zip(names[0::2], names[1::2]):
        try:
            a, b = load(before), load(after)
        except (OSError, ValueError) as e:
            print('%s vs %s: MISSING (%s)' % (before, after, e))
            failed = True
            continue
        keys = sorted(k for k in set(a) | set(b) if not is_timing(k))
        diff = [k for k in keys if a.get(k) != b.get(k)]
        summary = 'winner=%s score=%s/%s shots=%s shot_distances=%s compared_keys=%d' % (
            b.get('winner'), b.get('ally_score'), b.get('enemy_score'), b.get('shots'), digest(b.get('shot_distances_cm')), len(keys))
        if diff:
            failed = True
            print('%s vs %s: DIFFERENT in %d keys: %s' % (before, after, len(diff), ', '.join(diff[:12])))
            print('  before winner=%s score=%s/%s shots=%s' % (a.get('winner'), a.get('ally_score'), a.get('enemy_score'), a.get('shots')))
            print('  after  ' + summary)
        else:
            print('%s vs %s: IDENTICAL %s' % (before, after, summary))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
