"""Investigation board shared by every Claude / Codex session (2026-11-22, Docs/design-investigation-board.md; user decision).

Parallel sessions (one per work tree) used to investigate the same question without knowing it (close fights 11-22 12:45 arms /
13:35 soldierai; the west/east bias in sector AI and group control). STATUS only shows finished work and differs per branch until
merged. This board lives outside the work trees, next to the measurement lock, so every session sees a write at once:
  C:\\Users\\nanek\\Documents\\FPS\\.lock\\board.json           open investigations (claims)
  C:\\Users\\nanek\\Documents\\FPS\\.lock\\board-findings.jsonl  findings and closed claims (append-only)

Usage (any Python 3; from Bash or PowerShell):
  python -I Setup/Board.py find 近い戦い 詰め寄り                  # BEFORE investigating: open claims + past findings that match
  python -I Setup/Board.py claim --who 個人AI --topic "近い戦いが起きない原因" \\
         --keys 近い戦い,詰め寄り,BattleArmsCharge,BattleArea.cpp [--question "..."] [--hours 3]
                                                                # registers; prints overlapping claims/findings (exit 2 on overlap)
  python -I Setup/Board.py note --who 個人AI --fact "走り出しの86%は80m以上から" --ref "STATUS 11-22 12:45" [--keys ...]
  python -I Setup/Board.py done --who 個人AI [--id C3] --result "原因は詰め寄りの出発距離" --ref "STATUS 11-22 13:35"
  python -I Setup/Board.py status                                # open claims (stale ones marked)
  python -I Setup/Board.py drop --who 個人AI [--id C3]             # abandon a claim without a result
  python -I Setup/Board.py plan --who 個人AI --question "問い" --why "既存結果では不明" \\
         --runs "1条件×2シード、I、2本" --stop "差がなければ終了" [--keys a,b] [--stage 3] [--ref ...]
                                                                # stage: 2 activation, 3 baseline comparison, 4 adoption (239/277)

A text that starts with "-" (a switch name) must be written as --fact=-Battle... / --result=-Battle... (argparse).
Matching is plain case-insensitive substring matching of every key / topic word against the other entry's topic, keys, question
and fact. Keys carry most of the weight: give switch names, file names, function names and 2-4 Japanese words.
The board is a marker, not an enforcement: on overlap, read the other's notes or ask its session (SendMessage) before going on.
"""
import argparse
import json
import os
import re
import sys
import time
from datetime import datetime, timedelta

DIR = os.environ.get('BOARD_DIR', r'C:\Users\nanek\Documents\FPS\.lock')
BOARD = os.path.join(DIR, 'board.json')
FINDINGS = os.path.join(DIR, 'board-findings.jsonl')
MUTEX = os.path.join(DIR, 'board.mutex')

try:
    sys.stdout.reconfigure(encoding='utf-8')
    sys.stderr.reconfigure(encoding='utf-8')
except Exception:
    pass


def now():
    return datetime.now().astimezone()


def iso(t):
    return t.strftime('%Y-%m-%dT%H:%M:%S%z')


def parse(s):
    return datetime.strptime(s, '%Y-%m-%dT%H:%M:%S%z')


def short(s):
    return parse(s).strftime('%m-%d %H:%M')


class Locked:
    """Exclusive create of board.mutex; a mutex older than 60 s is a crashed caller and is removed."""

    def __enter__(self):
        os.makedirs(DIR, exist_ok=True)
        deadline = time.time() + 60
        while True:
            try:
                self.fd = os.open(MUTEX, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
                return self
            except FileExistsError:
                try:
                    if time.time() - os.path.getmtime(MUTEX) > 60:
                        os.remove(MUTEX)
                        continue
                except OSError:
                    pass
                if time.time() > deadline:
                    sys.exit('Board: board.mutex busy for 60 s')
                time.sleep(0.2)

    def __exit__(self, *a):
        os.close(self.fd)
        try:
            os.remove(MUTEX)
        except OSError:
            pass


def load_board():
    if not os.path.exists(BOARD):
        return {'next_id': 1, 'claims': []}
    with open(BOARD, encoding='utf-8') as f:
        text = f.read().strip()
    return json.loads(text) if text else {'next_id': 1, 'claims': []}


def save_board(b):
    tmp = BOARD + '.tmp'
    with open(tmp, 'w', encoding='utf-8') as f:
        json.dump(b, f, ensure_ascii=False, indent=1)
    os.replace(tmp, BOARD)


def load_findings():
    out = []
    if os.path.exists(FINDINGS):
        with open(FINDINGS, encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if line:
                    try:
                        out.append(json.loads(line))
                    except ValueError:
                        pass
    return out


def append_finding(rec):
    with open(FINDINGS, 'a', encoding='utf-8') as f:
        f.write(json.dumps(rec, ensure_ascii=False) + '\n')


def split_keys(s):
    return [k.strip() for k in re.split(r'[,、，]', s or '') if k.strip()]


def words(topic, keys):
    ws = list(keys)
    for w in re.split(r'[\s・/／]+', topic or ''):
        if len(w) >= 2:
            ws.append(w)
    return [w.lower() for w in ws]


def text_of(e):
    return ' '.join([e.get('topic', ''), ' '.join(e.get('keys', [])), e.get('question', ''),
                     e.get('fact', ''), e.get('result', ''), e.get('why', ''), e.get('runs', '')]).lower()


def hits(ws, e):
    t = text_of(e)
    return sorted({w for w in ws if w and w in t})


def stale(c):
    return now() > parse(c['until'])


def fmt_claim(c, matched=None):
    s = f"[{c['id']}] {c['who']}：{c['topic']}（{short(c['since'])}〜{short(c['until'])}"
    s += '、期限切れ' if stale(c) else ''
    s += '）'
    if c.get('keys'):
        s += f"\n      キー: {', '.join(c['keys'])}"
    if c.get('question'):
        s += f"\n      問い: {c['question']}"
    if matched:
        s += f"\n      一致: {', '.join(matched)}"
    return s


def fmt_finding(r, matched=None):
    kind = '結論' if r['kind'] == 'done' else ('取下げ' if r['kind'] == 'drop' else '事実')
    body = r.get('result') or r.get('fact') or ''
    if r['kind'] == 'plan':
        kind = '申請'
        body = f"問い: {r['question']} / 本数: {r['runs']} / やめる条件: {r['stop']} / 段階: {r['stage']}"
        body = ' '.join(body.splitlines())
    s = f"{short(r['time'])} {r['who']} [{kind}{'・' + r['claim'] if r.get('claim') else ''}] "
    if r['kind'] in ('done', 'drop') and r.get('topic'):
        s += f"{r['topic']} → "
    s += body
    if r.get('ref'):
        s += f"（{r['ref']}）"
    if matched:
        s += f"  ← 一致: {', '.join(matched)}"
    return s


def report_matches(ws, board, exclude_id=None, limit=15):
    claims = [(c, hits(ws, c)) for c in board['claims'] if c['id'] != exclude_id]
    claims = [(c, m) for c, m in claims if m]
    finds = [(r, hits(ws, r)) for r in load_findings()]
    finds = [(r, m) for r, m in finds if m]
    finds.sort(key=lambda x: (len(x[1]), x[0]['time']), reverse=True)
    if claims:
        print('■ 進行中の調査で重なるもの（先に相手のメモを読むか、そのセッションに聞く）:')
        for c, m in claims:
            print('  ' + fmt_claim(c, m))
    if finds:
        print(f'■ 過去にわかったこと（一致の多い順、{min(limit, len(finds))}/{len(finds)}件）:')
        for r, m in finds[:limit]:
            print('  ' + fmt_finding(r, m))
    if not claims and not finds:
        print('重なる調査・記録はなし。')
    return bool(claims)


def own_claim(board, who, cid):
    mine = [c for c in board['claims'] if c['who'] == who and (cid is None or c['id'] == cid)]
    if not mine:
        sys.exit(f'Board: {who} の進行中の調査' + (f' {cid}' if cid else '') + ' がない')
    if len(mine) > 1 and cid is None:
        sys.exit('Board: 進行中の調査が複数ある。--id で選ぶ: ' + ', '.join(c['id'] for c in mine))
    return mine[0]


def cmd_find(a):
    ws = [w.lower() for w in a.words]
    report_matches(ws, load_board(), limit=a.limit)


def cmd_claim(a):
    keys = split_keys(a.keys)
    with Locked():
        b = load_board()
        ws = words(a.topic, keys)
        same = [c for c in b['claims'] if c['who'] == a.who and c['topic'] == a.topic]
        overlap = report_matches(ws, b, exclude_id=same[0]['id'] if same else None, limit=a.limit)
        t = now()
        if same:
            c = same[0]
            c.update(keys=keys or c['keys'], question=a.question or c.get('question', ''),
                     until=iso(t + timedelta(hours=a.hours)))
            print(f'更新: {c["id"]}')
        else:
            c = {'id': f"C{b['next_id']}", 'who': a.who, 'topic': a.topic, 'keys': keys, 'question': a.question or '',
                 'since': iso(t), 'until': iso(t + timedelta(hours=a.hours))}
            b['next_id'] += 1
            b['claims'].append(c)
            print(f'登録: {c["id"]}')
        save_board(b)
    sys.exit(2 if overlap else 0)


def cmd_note(a):
    with Locked():
        b = load_board()
        c = None
        mine = [x for x in b['claims'] if x['who'] == a.who and (a.id is None or x['id'] == a.id)]
        if len(mine) == 1:
            c = mine[0]
        keys = split_keys(a.keys) or (c['keys'] if c else [])
        append_finding({'time': iso(now()), 'kind': 'note', 'who': a.who, 'claim': c['id'] if c else '',
                        'topic': c['topic'] if c else '', 'keys': keys, 'fact': a.fact, 'ref': a.ref or ''})
    print('記録した。')


def cmd_plan(a):
    with Locked():
        b = load_board()
        mine = [c for c in b['claims'] if c['who'] == a.who]
        c = mine[0] if len(mine) == 1 else None
        keys = split_keys(a.keys) or (c['keys'] if c else [])
        report_matches(words(a.question, keys), b)
        append_finding({'time': iso(now()), 'kind': 'plan', 'who': a.who,
                        'claim': c['id'] if c else '', 'keys': keys, 'question': a.question,
                        'why': a.why, 'runs': a.runs, 'stop': a.stop, 'stage': a.stage, 'ref': a.ref})
    print('申請を記録した。')


def close(a, kind):
    with Locked():
        b = load_board()
        c = own_claim(b, a.who, a.id)
        b['claims'] = [x for x in b['claims'] if x['id'] != c['id']]
        append_finding({'time': iso(now()), 'kind': kind, 'who': a.who, 'claim': c['id'], 'topic': c['topic'],
                        'keys': c['keys'], 'question': c.get('question', ''),
                        'result': getattr(a, 'result', '') or '', 'ref': getattr(a, 'ref', '') or ''})
        save_board(b)
    print(f"{'閉じた' if kind == 'done' else '取り下げた'}: {c['id']} {c['topic']}")


def cmd_status(a):
    b = load_board()
    if not b['claims']:
        print('進行中の調査はなし。')
    for c in sorted(b['claims'], key=lambda x: x['since']):
        print(fmt_claim(c))
    n = load_findings()
    print(f'（記録 {len(n)} 件。検索は find）')
    t = now()
    plans = [r for r in reversed(n) if r['kind'] == 'plan' and t - timedelta(hours=24) <= parse(r['time']) <= t]
    plans.sort(key=lambda r: r['time'], reverse=True)
    print(f'直近24時間の申請 {len(plans)} 件')
    for r in plans[:10]:
        print('  ' + fmt_finding(r))


def main():
    p = argparse.ArgumentParser(description='Shared investigation board (see module docstring).')
    s = p.add_subparsers(dest='cmd', required=True)
    f = s.add_parser('find')
    f.add_argument('words', nargs='+')
    f.add_argument('--limit', type=int, default=15)
    f.set_defaults(fn=cmd_find)
    c = s.add_parser('claim')
    c.add_argument('--who', required=True)
    c.add_argument('--topic', required=True)
    c.add_argument('--keys', default='')
    c.add_argument('--question', default='')
    c.add_argument('--hours', type=float, default=4)
    c.add_argument('--limit', type=int, default=10)
    c.set_defaults(fn=cmd_claim)
    n = s.add_parser('note')
    n.add_argument('--who', required=True)
    n.add_argument('--fact', required=True)
    n.add_argument('--ref', default='')
    n.add_argument('--keys', default='')
    n.add_argument('--id')
    n.set_defaults(fn=cmd_note)
    pl = s.add_parser('plan')
    for name in ('who', 'question', 'why', 'runs', 'stop'):
        pl.add_argument('--' + name, required=True)
    pl.add_argument('--keys', default='')
    pl.add_argument('--stage', type=int, choices=(2, 3, 4), default=3)
    pl.add_argument('--ref', default='')
    pl.set_defaults(fn=cmd_plan)
    d = s.add_parser('done')
    d.add_argument('--who', required=True)
    d.add_argument('--result', required=True)
    d.add_argument('--ref', default='')
    d.add_argument('--id')
    d.set_defaults(fn=lambda a: close(a, 'done'))
    r = s.add_parser('drop')
    r.add_argument('--who', required=True)
    r.add_argument('--id')
    r.set_defaults(fn=lambda a: close(a, 'drop'))
    st = s.add_parser('status')
    st.set_defaults(fn=cmd_status)
    a = p.parse_args()
    a.fn(a)


if __name__ == '__main__':
    main()
