"""Shared baseline ledger (standard library, Python 3.11).

Usage:
  python -I Setup/Baseline.py set --who budget --commit SHA [--note ...] <folder>...
  python -I Setup/Baseline.py show [--all]
  python -I Setup/Baseline.py path --map I --seed 7
  python -I Setup/Baseline.py check <folder>...
  python -I Setup/Baseline.py equiv --who budget --switch-only [--note ...] <folder>
All commands accept --root DIR. Bare folder names are resolved under that root.
BASELINE_DIR overrides the shared ledger directory; use a temporary directory for tests.
"""
import argparse
from collections import Counter
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import time

DIR = Path(os.environ.get('BASELINE_DIR', r'C:\Users\nanek\Documents\FPS\.lock'))
LEDGER = DIR / 'baselines.json'
MUTEX = DIR / 'baseline.mutex'
MAIN_ROOT = Path(r'C:\Users\nanek\Documents\FPS\FutureBattle\Setup\Benchmarks\SightTrial-20261111')
ROOT = Path(__file__).resolve().parent / 'Benchmarks' / 'SightTrial-20261111'
if not ROOT.is_dir():
    ROOT = MAIN_ROOT

# Keep this predicate identical to Compare-MatchResults.py.
TIMING = ('duration_seconds', 'custom_ai_ms_per_second', 'sensor_timing', 'path_query_ms',
          'engine_thread_samples_available', 'decisions_per_second', 'sight_rays_per_second',
          'path_queries_per_second')


def is_timing(key):
    return key in TIMING or '_ms' in key or key.endswith('_scope')


def timestamp():
    return datetime.now().astimezone().strftime('%Y-%m-%dT%H:%M:%S%z')


class Locked:
    """Same exclusive-create and stale-lock policy as Board.py."""

    def __enter__(self):
        DIR.mkdir(parents=True, exist_ok=True)
        deadline = time.time() + 60
        while True:
            try:
                self.fd = os.open(MUTEX, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
                return self
            except FileExistsError:
                try:
                    if time.time() - MUTEX.stat().st_mtime > 60:
                        MUTEX.unlink()
                        continue
                except OSError:
                    pass
                if time.time() > deadline:
                    raise ValueError('baseline.mutex が60秒間使用中です。')
                time.sleep(0.2)

    def __exit__(self, *args):
        os.close(self.fd)
        try:
            MUTEX.unlink()
        except OSError:
            pass


def read_json(path):
    with open(path, encoding='utf-8-sig') as f:
        return json.load(f)


def load_ledger():
    return read_json(LEDGER) if LEDGER.exists() else {'eras': []}


def current_era(ledger=None):
    eras = (load_ledger() if ledger is None else ledger)['eras']
    return eras[-1] if eras else None


def save_ledger(ledger):
    tmp = LEDGER.with_suffix('.json.tmp')
    with open(tmp, 'w', encoding='utf-8') as f:
        json.dump(ledger, f, ensure_ascii=False, indent=2)
        f.write('\n')
    os.replace(tmp, LEDGER)


def resolve_folder(name, root=None):
    p = Path(name)
    if not p.is_absolute() and len(p.parts) == 1:
        local = Path(ROOT if root is None else root) / p
        # A work tree holds only its own results: a bare name not found there is looked up in main.
        p = local if local.is_dir() or not (MAIN_ROOT / p).is_dir() else MAIN_ROOT / p
    return p.resolve()


def normalized_args(arguments):
    return [a for a in arguments[1:] if a.split('=', 1)[0].lower() not in
            ('-abslog', '-battlesightexperiment', '-battlewidescreens')]


def read_match(name, root=None, with_metrics=True):
    folder = resolve_folder(name, root)
    man = read_json(folder / 'manifest.json')
    args = man.get('arguments')
    if not isinstance(args, list) or not args or not all(isinstance(a, str) for a in args):
        raise ValueError(f'{folder.name}: manifest の arguments がありません。')
    maps = [m.group(1) for a in args if (m := re.fullmatch(r'/Game/Prototype/Lvl_BattleWide_(\w+)', a))]
    def value(key):
        values = [a.split('=', 1)[1] for a in args if a.lower().startswith(key.lower() + '=')]
        if len(values) != 1:
            raise ValueError(f'{folder.name}: {key} を一意に読めません。')
        return values[0]
    if len(maps) != 1:
        raise ValueError(f'{folder.name}: マップを一意に読めません。')
    snapshot = re.search(r'/BuildSnapshots/([^/]+)/', args[0].replace('\\', '/'), re.I)
    dll = man.get('dll')
    if not isinstance(dll, str) or not dll.strip():
        raise ValueError(f'{folder.name}: DLL ハッシュがありません。')
    out = {'map': maps[0], 'seed': int(value('-BattleTrialSeed')), 'fps': float(value('-FPS')),
           'nullrhi': any(a.lower() == '-nullrhi' for a in args),
           'snapshot': snapshot.group(1) if snapshot else None, 'dll': dll,
           'folder': str(folder), 'name': folder.name, 'args': normalized_args(args)}
    if with_metrics:
        out['passed'] = read_json(folder / 'result.json').get('passed') is True
        metrics = read_json(folder / 'metrics.json')
        data = {k: v for k, v in metrics.items() if not is_timing(k)}
        out['fingerprint'] = hashlib.sha256(json.dumps(data, sort_keys=True).encode('utf-8')).hexdigest()[:16]
    return out


def find_entry(era, map_name, seed):
    return next((e for e in era['entries'] if e['map'] == map_name and e['seed'] == seed), None) if era else None


def dll_status(era, dll):
    if era and dll.casefold() == era['dll'].casefold():
        return 'BASELINE', None
    for eq in era.get('equivalents', []) if era else []:
        if dll.casefold() == eq['dll'].casefold():
            return 'EQUIVALENT', eq
    return 'UNREGISTERED', None


def compare_entry(entry, match):
    # Preserve argument order and duplicates: do not silently accept conflicting switches.
    if entry['args'] != match['args']:
        return 'args differ'
    return 'IDENTICAL' if entry['fingerprint'] == match['fingerprint'] else 'DIFFERENT'


def print_args_difference(entry, match):
    base, trial = Counter(entry['args']), Counter(match['args'])
    print('  基準だけ: ' + (' '.join((base - trial).elements()) or 'なし'))
    print('  試験だけ: ' + (' '.join((trial - base).elements()) or 'なし'))
    if base == trial:
        print('  引数の順序が違います。')


def validate_frozen(match):
    if not match['passed']:
        raise ValueError(f"{match['name']}: passed ではありません。")
    if not match['snapshot']:
        raise ValueError(f"{match['name']}: 凍結ビルドではありません。")


def cmd_set(a):
    matches = [read_match(n, a.root) for n in a.folders]
    first, seen = matches[0], set()
    for m in matches:
        validate_frozen(m)
        if m['fps'] != 30 or not m['nullrhi']:
            raise ValueError(f"{m['name']}: 30FPS・描画なしの結果が必要です。")
        if m['dll'].casefold() != first['dll'].casefold() or m['snapshot'] != first['snapshot']:
            raise ValueError('DLL または凍結ビルドが一致しません。')
        key = (m['map'], m['seed'])
        if key in seen:
            raise ValueError(f'マップ・シードが重複しています: {key}')
        seen.add(key)
    with Locked():
        ledger = load_ledger()
        era = {'id': 'E%d' % (len(ledger['eras']) + 1), 'commit': a.commit,
               'snapshot': first['snapshot'], 'dll': first['dll'], 'who': a.who,
               'time': timestamp(), 'note': a.note,
               'entries': [{k: m[k] for k in ('map', 'seed', 'folder', 'name', 'fingerprint', 'fps', 'args')}
                           for m in matches], 'equivalents': []}
        ledger['eras'].append(era)
        save_ledger(ledger)
    print(f"基準 {era['id']} を登録しました（{len(matches)} 本）。")


def cmd_show(a):
    eras = load_ledger()['eras']
    if not eras:
        raise ValueError('基準が登録されていません。')
    for era in eras if a.all else eras[-1:]:
        print('id | commit | 凍結ビルド | 時刻 | who')
        print(' | '.join(str(era[k]) for k in ('id', 'commit', 'snapshot', 'time', 'who')))
        print('map | seed | フォルダ名 | 指紋 | フォルダの存在')
        for e in era['entries']:
            print(f"{e['map']} | {e['seed']} | {e['name']} | {e['fingerprint']} | "
                  + ('あり' if Path(e['folder']).is_dir() else 'なし'))
        print('同等の登録:')
        for e in era['equivalents']:
            print('  ' + ' | '.join(str(e[k]) for k in ('dll', 'who', 'time', 'checked', 'map', 'seed', 'note')))
        if not era['equivalents']:
            print('  なし')


def cmd_path(a):
    entry = find_entry(current_era(), a.map, a.seed)
    if not entry:
        raise ValueError(f'マップ {a.map} シード {a.seed} の基準がありません。')
    print(entry['folder'])


def cmd_check(a):
    era = current_era()
    failed = False
    for name in a.folders:
        try:
            m = read_match(name, a.root)
            status, eq = dll_status(era, m['dll'])
            print(f"{m['name']}: {status}" + (f"（{eq['who']} {eq['time']}）" if eq else ''))
            entry = find_entry(era, m['map'], m['seed'])
            if entry:
                result = compare_entry(entry, m)
                print('  ' + result)
                if result == 'args differ':
                    print_args_difference(entry, m)
            else:
                print('  同じマップ・シードの基準がありません。')
        except (OSError, ValueError) as e:
            print(f'{name}: 読み取り失敗: {e}', file=sys.stderr)
            failed = True
    return int(failed)


def cmd_equiv(a):
    if not a.switch_only:
        raise ValueError('--switch-only の申告が必要です。1マップ1シードの一致は残りの同等の証明ではありません。'
                         '既定の経路・Config・マップに触れた差分は全部の基準を自分で回してください。')
    m = read_match(a.folder, a.root)
    validate_frozen(m)
    with Locked():
        ledger = load_ledger()
        era = current_era(ledger)
        entry = find_entry(era, m['map'], m['seed'])
        if not entry:
            raise ValueError('同じマップ・シードの基準がありません。')
        result = compare_entry(entry, m)
        if result != 'IDENTICAL':
            if result == 'args differ':
                print_args_difference(entry, m)
            raise ValueError(f'{result}: 同等として登録できません。')
        status, _ = dll_status(era, m['dll'])
        if status != 'UNREGISTERED':
            print(f'{status}: 登録済みです。')
            return
        era['equivalents'].append({'dll': m['dll'], 'who': a.who, 'time': timestamp(),
                                  'checked': m['folder'], 'map': m['map'], 'seed': m['seed'], 'note': a.note})
        save_ledger(ledger)
    print('IDENTICAL: DLL を同等として登録しました。')


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, 'reconfigure'):
            stream.reconfigure(encoding='utf-8')
    p = argparse.ArgumentParser(description='共用の基準試合の台帳')
    p.add_argument('--root', default=str(ROOT))
    sub = p.add_subparsers(dest='command', required=True)
    for name, fn in (('set', cmd_set), ('show', cmd_show), ('path', cmd_path),
                     ('check', cmd_check), ('equiv', cmd_equiv)):
        s = sub.add_parser(name)
        s.add_argument('--root', default=argparse.SUPPRESS)
        s.set_defaults(fn=fn)
        if name in ('set', 'equiv'):
            s.add_argument('--who', required=True)
            s.add_argument('--note', default='')
        if name in ('set', 'check'):
            s.add_argument('folders', nargs='+')
        if name == 'set':
            s.add_argument('--commit', required=True)
        elif name == 'show':
            s.add_argument('--all', action='store_true')
        elif name == 'path':
            s.add_argument('--map', required=True)
            s.add_argument('--seed', type=int, required=True)
        elif name == 'equiv':
            s.add_argument('--switch-only', action='store_true')
            s.add_argument('folder')
    a = p.parse_args(argv)
    try:
        return a.fn(a) or 0
    except (OSError, ValueError, KeyError, TypeError) as e:
        print(f'Baseline: {e}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
