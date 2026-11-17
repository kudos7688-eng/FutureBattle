"""Generate CODEMAP.md and CODEMAP_SYMBOLS.md: where each feature lives in the game source.

Run from anywhere (UE ships a Python):
  & 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/ThirdParty/Python3/Win64/python.exe' -I Setup/Export-CodeMap.py

CODEMAP.md         = the hand-written feature table (Setup/CodeMap-Features.md, with @references resolved to file:line)
                     followed by one row per module (types it declares, size, one-line role).
CODEMAP_SYMBOLS.md = every function definition with its line, one line per file. Meant for Grep, not for reading.

In Setup/CodeMap-Features.md:
  @Class::Function  -> `Class::Function` (File.cpp:line)       a function defined in a .cpp or inline in a .h
  @Module           -> `Module`                                 a module (file stem); checked to exist
  "## モジュールの説明" section, lines "Module | text"          -> the role column of the module table
A reference that no longer resolves is printed, marked in the output, and makes the script exit 1.
"""
import os
import re
import sys
from collections import defaultdict

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
SRC = os.path.join(ROOT, 'Game', 'FutureBattle', 'Source', 'FutureBattle')
PROTO = os.path.join(SRC, 'Prototype')
FEATURES = os.path.join(ROOT, 'Setup', 'CodeMap-Features.md')
OUT_MAP = os.path.join(ROOT, 'CODEMAP.md')
OUT_SYM = os.path.join(ROOT, 'CODEMAP_SYMBOLS.md')

# A definition at column 0: optional return type, then Qualified::Name( or a free function name(.
QUALIFIED = re.compile(r'^(?![#/\s}])(?:[\w:<>\*&,\s]*?[\s\*&])?((?:\w+::)+~?\w+)\s*\(')
FREE = re.compile(r'^(?:static|inline|FORCEINLINE)\s[\w:<>\*&,\s]*?[\s\*&](\w+)\s*\([^;]*$')
INLINE = re.compile(r'^\s+(?:(?:static|inline|virtual|FORCEINLINE|constexpr)\s+)*[\w:<>\*&,]+[\s\*&]+(\w+)\s*\([^;]*\)\s*(?:const\s*)?(?:override\s*)?\{')
TYPE = re.compile(r'^\s*(class|struct|enum class|enum|namespace)\s+(?:[A-Z]+_API\s+)?(\w+)\s*(?::[^;{]*)?(?:\{.*)?$')
KEYWORDS = {'if', 'for', 'while', 'switch', 'return', 'else', 'do', 'sizeof', 'check', 'ensure', 'UE_LOG', 'TEXT'}
TEST = re.compile(r'(Smoke|Trial|Soak|Benchmark|Probe|Pilot|Checks|Test|Audit|Diagnostic|Experiment|Validation|Shot|Visual|Preview)$')


def read(path):
    with open(path, encoding='utf-8', errors='replace') as f:
        return f.read().splitlines()


def scan(path):
    """-> (line count, [(qualified name, line)], [type names], first top-of-file comment)"""
    lines = read(path)
    is_header = path.endswith('.h')
    funcs, types, comment, scope = [], [], '', []
    for i, line in enumerate(lines, 1):
        if not comment and i <= 12 and line.lstrip().startswith('//'):
            comment = line.lstrip().lstrip('/').strip()
        if is_header:
            m = TYPE.match(line)
            if m and not line.rstrip().endswith(';'):
                if m.group(2) not in types:
                    types.append(m.group(2))
                if not line[:1].isspace():
                    scope = [m.group(2)]
                continue
            m = INLINE.match(line)
            if m and m.group(1) not in KEYWORDS and scope:
                funcs.append((scope[0] + '::' + m.group(1), i))
            continue
        m = QUALIFIED.match(line)
        if m and not line.rstrip().endswith(';'):
            funcs.append((m.group(1), i))
            continue
        m = FREE.match(line)
        if m and m.group(1) not in KEYWORDS:
            funcs.append((m.group(1), i))
    return len(lines), funcs, types, comment


modules = defaultdict(dict)  # stem -> {'.h': scan, '.cpp': scan}
for n in sorted(os.listdir(PROTO)):
    stem, ext = os.path.splitext(n)
    if ext in ('.h', '.cpp'):
        modules[stem][ext] = scan(os.path.join(PROTO, n))

symbol = {}  # qualified name -> 'File.ext:line' (first definition wins; .cpp before .h)
for ext in ('.cpp', '.h'):
    for stem in sorted(modules):
        if ext in modules[stem]:
            for name, line in modules[stem][ext][1]:
                symbol.setdefault(name, f'{stem}{ext}:{line}')

# ---- hand-written part -------------------------------------------------------------------------------------------
missing = []
role = {}
feature_lines = []
REF = re.compile(r'@((?:\w+::)*~?\w+)')


def resolve(m):
    name = m.group(1)
    if '::' in name:
        if name in symbol:
            return f'`{name}` ({symbol[name]})'
    elif name in modules:
        return f'`{name}`'
    missing.append(name)
    return f'`{name}` (**未解決**)'


if os.path.exists(FEATURES):
    in_roles = False
    for line in read(FEATURES):
        if line.startswith('## '):
            in_roles = line.strip() == '## モジュールの説明'
            if in_roles:
                continue
        if in_roles:
            if '|' in line:
                k, v = (s.strip() for s in line.split('|', 1))
                if k in modules:
                    role[k] = v
                elif k:
                    missing.append(k + '（モジュールの説明）')
            continue
        feature_lines.append(REF.sub(resolve, line))

# ---- CODEMAP.md --------------------------------------------------------------------------------------------------
def row(stem):
    parts = modules[stem]
    size = ' + '.join(f'{parts[e][0]}' for e in ('.h', '.cpp') if e in parts)
    exts = '/'.join(e for e in ('.h', '.cpp') if e in parts)
    types = []
    for e in ('.h', '.cpp'):
        if e in parts:
            types += [t for t in parts[e][2] if t not in types]
    shown = ', '.join(types[:8]) + (f' ほか{len(types) - 8}' if len(types) > 8 else '')
    text = role.get(stem) or next((parts[e][3] for e in ('.h', '.cpp') if e in parts and parts[e][3]), '')
    return f'| `{stem}` | {exts} | {size} | {shown} | {text.replace("|", "/")} |'


core = [s for s in sorted(modules) if not TEST.search(s) and not s.endswith('Commandlet')]
tests = [s for s in sorted(modules) if s not in core]
total = sum(p[0] for m in modules.values() for p in m.values())

out = ['# コード索引（どこに何があるか）', '',
       '`Setup/Export-CodeMap.py` が生成する。**手で編集しない**。機能の表とモジュールの説明は `Setup/CodeMap-Features.md` に書き、作り直す。',
       '実装を探すときは、まずこの表で入口を決め、関数の行は [CODEMAP_SYMBOLS.md](CODEMAP_SYMBOLS.md) を Grep で引く（全文は読まない）。',
       '行番号は生成時点のもの。ずれていたら作り直す。起動引数は [SWITCHES.md](SWITCHES.md)。', '',
       f'対象は `Game/FutureBattle/Source/FutureBattle/Prototype/`（{len(modules)} モジュール、{total} 行）。'
       '同じ階層の `Variant_*` と `FutureBattle*.cpp/.h` は UE テンプレート由来で、ゲームのルールは置いていない。', '']
out += feature_lines
out += ['', '## モジュール一覧（本体）', '', '行数は .h + .cpp。型は宣言順で先頭8個まで。', '',
        '| モジュール | 種類 | 行数 | 宣言している型 | 役割 |', '|---|---|---|---|---|']
out += [row(s) for s in core]
out += ['', '## モジュール一覧（試験・計測・生成）', '',
        '名前が Smoke / Trial / Soak / Benchmark / Probe / Pilot / Checks / Test / Audit / Diagnostic / Experiment / Validation / Shot / Visual / Preview / Commandlet で終わるもの。', '',
        '| モジュール | 種類 | 行数 | 宣言している型 | 役割 |', '|---|---|---|---|---|']
out += [row(s) for s in tests]

sym = ['# 関数の索引（Grep 用）', '',
       '`Setup/Export-CodeMap.py` が生成する。**手で編集しない**。1行が1ファイルで、`関数名:行` を定義順に並べる。',
       '`Grep "HireSquad" CODEMAP_SYMBOLS.md` のように引く。.h は本体をその場に書いた関数だけを載せる（宣言だけの関数は .cpp 側）。', '']
for stem in sorted(modules):
    for ext in ('.h', '.cpp'):
        if ext in modules[stem] and modules[stem][ext][1]:
            sym.append(f'- `{stem}{ext}`: ' + ', '.join(f'{n}:{l}' for n, l in modules[stem][ext][1]))

for path, body in ((OUT_MAP, out), (OUT_SYM, sym)):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(body) + '\n')

print(f'{len(modules)} modules, {len(symbol)} functions -> CODEMAP.md, CODEMAP_SYMBOLS.md')
no_role = [s for s in core if not role.get(s)]
if no_role:
    print(f'no role text for {len(no_role)} core modules: ' + ' '.join(no_role))
if missing:
    print('UNRESOLVED: ' + ', '.join(missing), file=sys.stderr)
    sys.exit(1)
