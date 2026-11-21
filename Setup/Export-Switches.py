"""Generate SWITCHES.md using conservative, standard-library-only static reading.

Run with UE's Python: python.exe -I Setup/Export-Switches.py
This is not a C++ evaluator: unsupported expressions remain 未解決. Only
command-line readers (including proven local aliases / key wrappers) register keys.
"""
import configparser
import re
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / 'Game/FutureBattle/Source'
UNKNOWN = '未解決'
LEX = re.compile(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_]\w*|(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?[fFuUlL]*|::|->|==|!=|>=|<=|&&|\|\||\+\+|--|[^\s]')
IDENT = re.compile(r'[A-Za-z_]\w*\Z')
NUMBER = re.compile(r'[+-]?(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?[fFuUlL]*\Z')


def joined(tokens):
    return ''.join(tokens)


class Source:
    def __init__(self, path):
        self.path = path
        self.text = path.read_text(encoding='utf-8-sig')
        matches = [m for m in LEX.finditer(self.text) if not m[0].startswith(('//', '/*'))]
        self.t = [m[0] for m in matches]
        self.offsets = [m.start() for m in matches]
        self.pairs, self.scope, self.parents = {}, [], {None: None}
        brackets, scopes = [], [None]
        for i, token in enumerate(self.t):
            self.scope.append(scopes[-1])
            if token in ('(', '[', '{'):
                brackets.append(i)
                if token == '{':
                    self.parents[i] = scopes[-1]
                    scopes.append(i)
            elif token in (')', ']', '}') and brackets:
                start = brackets.pop()
                self.pairs[start] = i
                self.pairs[i] = start
                if token == '}':
                    scopes.pop()
        self.classes, self.functions = {}, {}
        for i in self.parents:
            if i is None:
                continue
            start = i - 1
            while start >= 0 and self.t[start] not in (';', '{', '}'):
                start -= 1
            prefix = self.t[start + 1:i]
            class_at = next((n for n, x in enumerate(prefix) if x in ('class', 'struct')), None)
            if class_at is not None:
                names = [x for x in prefix[class_at + 1:] if IDENT.fullmatch(x) and not x.endswith('_API')]
                if names:
                    self.classes[i] = names[0]
            # Named function definitions; lambdas retain their own lexical scope.
            end = i - 1
            if self.t[end] == 'const':
                end -= 1
            if self.t[end] == ')' and end in self.pairs:
                op = self.pairs[end]
                if op and IDENT.fullmatch(self.t[op - 1]) and self.t[op - 1] not in ('if', 'for', 'while', 'switch', 'catch'):
                    owner = self.t[op - 3] if op >= 3 and self.t[op - 2] == '::' else None
                    self.functions[i] = (self.t[op - 1], owner, self.parts(op + 1, end))
                elif op and self.t[op - 1] == ']':
                    capture = self.pairs[op - 1]
                    if capture >= 2 and self.t[capture - 1] == '=' and IDENT.fullmatch(self.t[capture - 2]):
                        self.functions[i] = (self.t[capture - 2], None, self.parts(op + 1, end))
        self.decls = []

    def parts(self, start, end):
        result, begin, i = [], start, start
        while i < end:
            if self.t[i] in ('(', '[', '{') and i in self.pairs:
                i = self.pairs[i] + 1
                continue
            if self.t[i] == ',':
                result.append((begin, i))
                begin = i + 1
            i += 1
        return result + [(begin, end)]

    def ancestors(self, pos):
        result, scope = [], self.scope[pos]
        while scope is not None:
            result.append(scope)
            scope = self.parents[scope]
        return result + [None]

    def expression_end(self, start):
        i = start
        while i < len(self.t) and self.t[i] not in (',', ';', ')', '}'):
            if self.t[i] in ('(', '[', '{') and i in self.pairs:
                i = self.pairs[i] + 1
            else:
                i += 1
        return i

    def declarations(self, types):
        for i, token in enumerate(self.t):
            if token not in types:
                continue
            j = i + 1
            while j < len(self.t) and self.t[j] in ('*', '&', 'const'):
                j += 1
            while j + 1 < len(self.t) and IDENT.fullmatch(self.t[j]):
                name, tail = self.t[j], self.t[j + 1]
                if tail not in ('=', '{', ';', ','):
                    break
                end = self.expression_end(j + 2) if tail == '=' else j + 1
                expr = self.t[j + 2:end] if tail == '=' else []
                if tail == '{':
                    end = self.pairs[j + 1] + 1
                    expr = self.t[j + 2:end - 1]
                self.decls.append((name, token, expr, j, self.scope[j], tail))
                if end >= len(self.t) or self.t[end] != ',':
                    break
                j = end + 1

    def declaration(self, name, pos):
        ancestors = self.ancestors(pos)
        candidates = [d for d in self.decls if d[0] == name and d[3] < pos and d[4] in ancestors]
        return min(candidates, key=lambda d: (ancestors.index(d[4]), -d[3])) if candidates else None

    def line(self, pos):
        return self.text.count('\n', 0, self.offsets[pos]) + 1


sources = [Source(p) for p in sorted(SRC.rglob('*')) if p.suffix in ('.cpp', '.h', '.inl')]
types = {'bool', 'int', 'int8', 'int16', 'int32', 'int64', 'uint8', 'uint16', 'uint32', 'uint64', 'float', 'double', 'FString', 'FName', 'auto', 'TCHAR'}
types.update(c for s in sources for c in s.classes.values())
fields = defaultdict(list)
for source in sources:
    source.declarations(types)
    for declaration in source.decls:
        if declaration[4] in source.classes:
            fields[(source.classes[declaration[4]], declaration[0])].append((source, declaration))

ini = configparser.ConfigParser(interpolation=None, strict=False, inline_comment_prefixes=(';',))
ini.optionxform = str
ini.read(ROOT / 'Game/FutureBattle/Config/DefaultGame.ini', encoding='utf-8-sig')
config = {}
for (owner, name), entries in fields.items():
    if owner != 'UBattleTuning' or len(entries) != 1:
        continue
    s, d = entries[0]
    # Require the property's own annotation, not a nearby Config member.
    type_at = d[3] - 1
    close = type_at - 1
    if close in s.pairs and s.t[close] == ')':
        opening = s.pairs[close]
        property_config = opening > 0 and s.t[opening - 1] == 'UPROPERTY' and 'Config' in s.t[opening + 1:close]
    else:
        property_config = False
    if property_config:
        config[name] = ini.get('/Script/FutureBattle.BattleTuning', name, fallback=joined(d[2]))


def literal(expr):
    value = joined(expr)
    if NUMBER.fullmatch(value) or value.lower() in ('true', 'false'):
        return value
    if re.fullmatch(r'TEXT\("(?:\\.|[^"\\])*"\)', value):
        return value[5:-1] if value != 'TEXT("")' else '空文字列'
    return UNKNOWN


def member(owner, name, source):
    entries = fields.get((owner, name), [])
    local = [e for e in entries if e[0] is source]
    return (local or entries)[0] if len(local or entries) == 1 else None


def resolve(source, expr, pos, depth=0):
    if depth > 12 or not expr:
        return UNKNOWN
    value = literal(expr)
    if value != UNKNOWN:
        return value
    text = joined(expr)
    # Bool-to-int initialisation still reports the underlying Config boolean.
    if text.endswith('?1:0'):
        return resolve(source, expr[:-4], pos, depth + 1)
    direct = re.fullmatch(r'GetDefault<UBattleTuning>\(\)->(\w+)', text)
    if direct:
        return literal([config.get(direct[1], '')])
    ref = re.fullmatch(r'(\w+)->(\w+)', text)
    if ref:
        decl = source.declaration(ref[1], pos)
        if decl and joined(decl[2]) == 'GetDefault<UBattleTuning>()':
            return literal([config.get(ref[2], '')])
    if not re.fullmatch(r'\w+(?:\.\w+)?', text):
        return UNKNOWN
    base, _, leaf = text.partition('.')
    decl = source.declaration(base, pos)
    ancestors = source.ancestors(pos)
    # Exact target assignments in visible scopes, bounded by this declaration.
    writes = []
    for i in range((decl[3] + 1) if decl else (next((a for a in ancestors if a in source.functions), 0) or 0), pos):
        if source.t[i] != '=' or source.scope[i] not in ancestors:
            continue
        start = i - len(expr)
        if start >= 0 and source.t[start:i] == expr and (start == 0 or source.t[start - 1] not in ('.', '->')):
            end = source.expression_end(i + 1)
            if end <= pos:
                writes.append((i, source.t[i + 1:end]))
    if writes:
        at, rhs = writes[-1]
        return resolve(source, rhs, at, depth + 1)
    if leaf:
        if not decl or decl[2]:  # aggregate / constructed object needs fuller evaluation
            return UNKNOWN
        entry = member(decl[1], leaf, source)
    elif decl:
        if decl[2]:
            return resolve(source, decl[2], decl[3], depth + 1)
        return '空文字列' if decl[1] == 'FString' else UNKNOWN
    else:
        owner = next((source.functions[a][1] for a in ancestors if a in source.functions and source.functions[a][1]), None)
        entry = member(owner, base, source)
    if entry:
        s, d = entry
        if d[2]:
            return resolve(s, d[2], d[3], depth + 1)
        return '空文字列' if d[1] == 'FString' else UNKNOWN
    return UNKNOWN


def command_line(source, expr, pos):
    if joined(expr) == 'FCommandLine::Get()':
        return True
    if len(expr) == 1:
        d = source.declaration(expr[0], pos)
        return bool(d and joined(d[2]) == 'FCommandLine::Get()')
    return False


def key(expr):
    m = re.fullmatch(r'TEXT\("-?([A-Za-z][A-Za-z0-9_-]*)=?"\)', joined(expr))
    return m[1] if m else None


switches = defaultdict(list)


def register(source, pos, name, kind, default, note=''):
    # Sentinel meaning is proven by the actual fallback expression, not a name guess.
    if default in ('-1', '-1.f') and name.startswith('Battle'):
        ini_key = name[6:]
        if ini_key in config:
            for s in sources:
                pattern = r'\?S\.(\w+):GetDefault<UBattleTuning>\(\)->' + re.escape(ini_key) + r'\b'
                if re.search(pattern, joined(s.t)):
                    default += '（= 設定値 ' + config[ini_key] + '）'
                    break
    line = source.line(pos)
    raw = source.text.splitlines()[line - 1]
    comment = re.search(r'//\s*(.*)', raw)
    switches[name].append((source.path.name, line, kind, default, note or (comment[1] if comment else '')))


for source in sources:
    t = source.t
    for i in range(len(t) - 4):
        if t[i:i + 3] not in (['FParse', '::', 'Value'], ['FParse', '::', 'Param'], ['FCString', '::', 'Strifind']) or t[i + 3] != '(':
            continue
        args = source.parts(i + 4, source.pairs[i + 3])
        if len(args) < 2 or not command_line(source, t[slice(*args[0])], i):
            continue
        kind = t[i + 2]
        name = key(t[slice(*args[1])])
        if name:
            default = resolve(source, t[slice(*args[2])], i) if kind == 'Value' and len(args) >= 3 else 'off'
            register(source, i, name, kind, default)
            continue
        # Local wrappers with a key parameter: prove the parameter reaches FParse.
        scope = next((a for a in source.ancestors(i) if a in source.functions), None)
        if scope is None:
            continue
        function, _, params = source.functions[scope]
        parameter_names = [t[b - 1] for a, b in params if b > a]
        key_expr = t[slice(*args[1])]
        if len(key_expr) != 1 or key_expr[0] not in parameter_names:
            continue
        index = parameter_names.index(key_expr[0])
        for j in range(len(t) - 1):
            if t[j] != function or t[j + 1] != '(':
                continue
            actuals = source.parts(j + 2, source.pairs[j + 1])
            if len(actuals) <= index:
                continue
            name = key(t[slice(*actuals[index])])
            if name:
                default = UNKNOWN
                if kind == 'Value' and len(args) >= 3:
                    target = t[slice(*args[2])]
                    d = source.declaration(target[0], i) if len(target) == 1 else None
                    if d and len(d[2]) == 1 and d[2][0] in parameter_names:
                        parameter = parameter_names.index(d[2][0])
                        if parameter < len(actuals):
                            default = resolve(source, t[slice(*actuals[parameter])], j)
                    elif d:
                        default = resolve(source, target, i)
                register(source, j, name, kind, default, f'{function} → {source.path.name}:{source.line(i)}（ヘルパー経由）')


def cell(value):
    return value.replace('|', '\\|').replace('\n', ' ')


out = ['# 起動引数（試験切替）の一覧', '',
       '`Setup/Export-Switches.py` が生成する。**手で編集しない**（ソースを変えたら作り直す）。',
       '既定値は C++ と DefaultGame.ini の静的読取りによる。解決できない値は「未解決」と記す。',
       'UBattleTuning の Config 値は DefaultGame.ini を優先する。既定は読取り先の初期値で、マップ・陣営・実行時の変更や後処理まで保証しない。Param / Strifind の off は引数なしを示す。読取り箇所で値が異なる場合は場所を併記する。', '',
       f'計 {len(switches)} 種類。', '',
       '| 引数 | 読み方 | 既定（推定） | 場所 | 行のコメント |',
       '|---|---|---|---|---|']
for name, uses in sorted(switches.items(), key=lambda item: item[0].lower()):
    # Presence probes must not override a Value reader's default.
    values = [u for u in uses if u[2] == 'Value'] or uses
    defaults = list(dict.fromkeys(u[3] for u in values))
    default = defaults[0] if len(defaults) == 1 else ' / '.join(f'{u[3]}（{u[0]}:{u[1]}）' for u in values)
    places = ', '.join(dict.fromkeys(f'{u[0]}:{u[1]}' for u in uses))
    note = next((u[4] for u in uses if u[4]), '')
    out.append(f'| `-{name}` | {"/".join(sorted({u[2] for u in uses}))} | {cell(default)} | {places} | {cell(note)} |')
(ROOT / 'SWITCHES.md').write_text('\n'.join(out) + '\n', encoding='utf-8')
print(f'{len(switches)} switches -> {ROOT / "SWITCHES.md"}')
