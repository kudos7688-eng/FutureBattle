"""Generate SWITCHES.md: every -Battle* command-line switch the game reads, with its default and where it is read.

Run from anywhere (UE ships a Python):
  & 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' -I Setup/Export-Switches.py

The default is inferred, not executed: the initial value of the variable passed to FParse::Value (same or previous
lines), or the in-class initialiser of the struct field it writes. Check the source line when it matters.
"""
import os
import re
import sys
from collections import defaultdict

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
SRC = os.path.join(ROOT, 'Game', 'FutureBattle', 'Source')
OUT = os.path.join(ROOT, 'SWITCHES.md')

CALL = re.compile(r'FParse::(Value|Param|Bool)\s*\(\s*FCommandLine::Get\(\)\s*,\s*TEXT\("(-?Battle[A-Za-z0-9_]+)(=?)"\)\s*(?:,\s*([^)]*?))?\)')
NAME_ONLY = re.compile(r'TEXT\("(-?Battle[A-Za-z0-9_]+)(=?)"\)')
COMMENT = re.compile(r'//\s*(.*)$')


def read(path):
    with open(path, encoding='utf-8', errors='replace') as f:
        return f.read().splitlines()


files = {}
for d, _, names in os.walk(SRC):
    for n in names:
        if n.endswith(('.cpp', '.h')):
            p = os.path.join(d, n)
            files[os.path.relpath(p, ROOT).replace('\\', '/')] = read(p)

# Member initialisers in declarations: "float A=1.f, B=2.f; bool bC=true; int32 D{3};" (headers first).
field_default = {}
TYPES = r'(?:bool|int8|int16|int32|int64|uint8|uint16|uint32|float|double|int|FString|FName|FVector|ECollisionChannel)'
DECL = re.compile(r'^\s*(?:(?:static|const|constexpr|mutable)\s+)*' + TYPES + r'\s+(.*)$')
DECLARATOR = re.compile(r'^\s*([A-Za-z_][A-Za-z0-9_]*)\s*(?:=\s*(.+?)|\{(.*)\})\s*$')


def split_top(s, sep):
    parts, depth, cur = [], 0, ''
    for ch in s:
        if ch in '({<[':
            depth += 1
        elif ch in ')}>]':
            depth -= 1
        if ch == sep and depth == 0:
            parts.append(cur)
            cur = ''
        else:
            cur += ch
    parts.append(cur)
    return parts


field_values = defaultdict(list)
for rel in sorted(r for r in files if r.endswith('.h')):
    for line in files[rel]:
        code = line.split('//')[0]
        for stmt in code.split(';'):
            stmt = re.sub(r'^.*?\b(?:struct|class)\s+\w+[^{=]*\{', '', stmt)
            stmt = re.sub(r'^[\s}]*(?:(?:public|private|protected)\s*:)?', '', stmt)
            m = DECL.match(stmt)
            if not m:
                continue
            for d in split_top(m.group(1), ','):
                dm = DECLARATOR.match(d)
                if dm:
                    v = (dm.group(2) or dm.group(3) or '').strip()
                    if v and v not in field_values[dm.group(1)]:
                        field_values[dm.group(1)].append(v)
# Same member name in several structs with different initialisers: show them all.
field_default = {k: (v[0] if len(v) == 1 else ' / '.join(v) + '（同名のメンバーが複数）') for k, v in field_values.items()}


def infer_default(lines, i, target):
    if not target:
        return ''
    target = target.strip().lstrip('&*')
    leaf = re.split(r'\.|->', target)[-1]
    pat = re.compile(r'\b' + re.escape(leaf) + r'\s*(?:=\s*([^;,\]{}]+)|\{([^;{}]*)\})')
    found = ''
    for j in range(i, max(-1, i - 4), -1):
        code = lines[j].split('//')[0]
        if j == i:
            code = code.split('FParse::')[0]
        m = pat.search(code)
        if m:
            found = (m.group(1) or m.group(2) or '').strip()
            break
    if not found:
        return field_default.get(leaf, '')
    # "S.bFlag?1:0" or "Tuning.Value": show the member's initialiser too.
    ref = re.match(r'^[A-Za-z_]\w*(?:\.|->)([A-Za-z_]\w*)', found)
    if ref and ref.group(1) in field_default:
        return f'{found}（{ref.group(1)}={field_default[ref.group(1)]}）'
    return found


switches = defaultdict(list)
for rel, lines in sorted(files.items()):
    for i, line in enumerate(lines):
        found = False
        for m in CALL.finditer(line):
            found = True
            kind, name, eq, target = m.group(1), m.group(2).lstrip('-'), m.group(3), m.group(4)
            default = infer_default(lines, i, target) if kind == 'Value' else ('off' if kind == 'Param' else '')
            c = COMMENT.search(line)
            switches[name].append((rel, i + 1, kind, default, (c.group(1).strip() if c else '')))
        if not found:
            for m in NAME_ONLY.finditer(line):
                name = m.group(1).lstrip('-')
                if 'FParse' in line or 'Battle' in name:
                    c = COMMENT.search(line)
                    switches[name].append((rel, i + 1, 'other', '', (c.group(1).strip() if c else '')))


def cell(s):
    return s.replace('|', '\\|').replace('\n', ' ')[:120]


out = []
out.append('# 起動引数（試験切替）の一覧')
out.append('')
out.append('`Setup/Export-Switches.py` が生成する。**手で編集しない**（ソースを変えたら作り直す）。')
out.append('既定値はソースから推定したもの（`FParse::Value` に渡す変数の初期値、または構造体のメンバーの初期値）。'
           '空欄は推定できなかったもの。`UPROPERTY(Config)` の値は `Config/DefaultGame.ini` で上書きされることがある。大事なときは「場所」の行と ini を確かめる。`Param` は付けるだけでONになる引数。')
out.append('')
out.append(f'計 {len(switches)} 種類。')
out.append('')
out.append('| 引数 | 読み方 | 既定（推定） | 場所 | 行のコメント |')
out.append('|---|---|---|---|---|')
for name in sorted(switches, key=str.lower):
    uses = switches[name]
    kinds = sorted({u[2] for u in uses})
    defaults = [u[3] for u in uses if u[3]]
    default = defaults[0] if defaults else ''
    if len(set(defaults)) > 1:
        default = ' / '.join(sorted(set(defaults)))
    locs = ', '.join(f'{os.path.basename(u[0])}:{u[1]}' for u in uses[:3]) + (f' ほか{len(uses)-3}' if len(uses) > 3 else '')
    comment = next((u[4] for u in uses if u[4]), '')
    out.append(f'| `-{name}` | {"/".join(kinds)} | {cell(default)} | {locs} | {cell(comment)} |')

with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(out) + '\n')
print(f'{len(switches)} switches -> {OUT}')
