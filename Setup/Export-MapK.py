# Draws the sector plan of map K (draft, Docs/design-map-k.md) as Docs/MapK-Sectors.svg. No engine, no build.
#   python -I Setup/Export-MapK.py
# Draft stage: the sector table lives here until BattleMapK.h exists; then this script should read the header (as Export-MapI.py does).
import io, os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COLS, ROWS, CELL = 12, 8, 5000
HX, HY = COLS * CELL / 2, ROWS * CELL / 2
# East half (columns 6..11, x = 0..300m), rows from the north (7) to the south (0). The west half is the 180-degree rotation.
EAST = [
    'AHQBHG',  # 7
    'QBOHAU',  # 6  north objective (col 8)
    'HPAYBH',  # 5
    'BQHCSS',  # 4  home base S (cols 10-11, rows 3-4)
    'POAHSS',  # 3  middle objective (col 7)
    'AHYBQH',  # 2
    'HKUOHA',  # 1  south objective (col 9)
    'GAWHBG',  # 0
]
assert len(EAST) == ROWS and all(len(r) == COLS // 2 for r in EAST)
# Streets (centre lines, cm): (along_x, at, from, to, width). Each piece and its 180-degree partner are both listed by rotate().
AVENUE = [(True, 5000, 10000, 20000, 1000), (False, 10000, 0, 5000, 1000), (True, 0, 0, 10000, 1000)]
STREETS = [(True, 10000, 0, 15000, 600), (True, -10000, 10000, 25000, 600), (False, 10000, -20000, -5000, 600),
           (False, 20000, 5000, 20000, 600), (False, 20000, -20000, -5000, 600), (True, 15000, 15000, 30000, 600), (True, -15000, 0, 10000, 600)]


def kind(c, r):
    if c < COLS // 2:
        c, r = COLS - 1 - c, ROWS - 1 - r
    return EAST[ROWS - 1 - r][c - COLS // 2]


def rotate(pieces):
    return pieces + [(ax, -at, -b, -a, w) for ax, at, a, b, w in pieces]


S = 0.02  # pixels per cm
W, H = 2 * HX * S, 2 * HY * S
PAD = 34


def px(x, y):
    return PAD + (x + HX) * S, PAD + (HY - y) * S


FILL = {'H': '#cbb69a', 'B': '#8f9aa8', 'A': '#b9a7b5', 'C': '#d8c7a0', 'K': '#a89a8c', 'U': '#bdb7ad', 'Y': '#c9c9c9',
        'G': '#9cc48a', 'Q': '#d9b99a', 'W': '#b7a58f', 'P': '#e3ded1', 'O': '#f2d16b', 'S': '#c9d6ea'}
NAME = {'H': '家(入れる)', 'B': 'ビル', 'A': '集合住宅', 'C': '教会', 'K': '工場', 'U': '廃墟', 'Y': '駐車場', 'G': '公園',
        'Q': '商店', 'W': '倉庫', 'P': '広場', 'O': '拠点', 'S': '本拠地'}
SHORT = {'H': '家', 'B': 'ビル', 'A': '集住', 'C': '教会', 'K': '工場', 'U': '廃墟', 'Y': '駐車', 'G': '公園', 'Q': '商店',
         'W': '倉庫', 'P': '広場', 'O': '拠点', 'S': ''}
out = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" font-family="sans-serif">' % (W + 2 * PAD, H + 2 * PAD + 96),
       '<rect width="100%" height="100%" fill="#ffffff"/>']
count, objectives = {}, []
for c in range(COLS):
    for r in range(ROWS):
        k = kind(c, r)
        count[k] = count.get(k, 0) + 1
        x, y = px(-HX + c * CELL, -HY + (r + 1) * CELL)
        out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s" stroke="#7d7d7d" stroke-width="0.6"/>' % (x, y, CELL * S, CELL * S, FILL[k]))
        cx, cy = px(-HX + (c + .5) * CELL, -HY + (r + .5) * CELL)
        if k == 'O':
            objectives.append((c, r, cx, cy))
        elif SHORT[k]:
            out.append('<text x="%.1f" y="%.1f" font-size="15" text-anchor="middle" fill="#2b2b2b">%s</text>' % (cx, cy + 5, SHORT[k]))
# Column letters and row numbers (as on the command map: A.. from the west, 1.. from the south).
for c in range(COLS):
    x, y = px(-HX + (c + .5) * CELL, HY)
    out.append('<text x="%.1f" y="%.1f" font-size="13" text-anchor="middle" fill="#555">%s</text>' % (x, y - 8, chr(65 + c)))
for r in range(ROWS):
    x, y = px(-HX, -HY + (r + .5) * CELL)
    out.append('<text x="%.1f" y="%.1f" font-size="13" text-anchor="middle" fill="#555">%d</text>' % (x - 14, y + 5, r + 1))
for pieces, colour in ((rotate(STREETS), '#5a6068'), (rotate(AVENUE), '#22262b')):
    for along_x, at, a, b, w in pieces:
        (x0, y0), (x1, y1) = (px(a, at), px(b, at)) if along_x else (px(at, a), px(at, b))
        out.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="%s" stroke-width="%.1f" stroke-linecap="square"/>' % (x0, y0, x1, y1, colour, w * S))
for i, (c, r, cx, cy) in enumerate(sorted(objectives)):
    out.append('<circle cx="%.1f" cy="%.1f" r="15" fill="none" stroke="#b3261e" stroke-width="3"/>' % (cx, cy))
    out.append('<text x="%.1f" y="%.1f" font-size="16" font-weight="bold" text-anchor="middle" fill="#b3261e">%d</text>' % (cx, cy + 6, i))
for sign, colour, label in ((-1, '#0b7a66', '西の本拠地'), (1, '#a3261c', '東の本拠地')):
    x, y = px(sign * 25000, 0)
    out.append('<rect x="%.1f" y="%.1f" width="26" height="26" fill="%s"/>' % (x - 13, y - 13, colour))
    out.append('<text x="%.1f" y="%.1f" font-size="14" text-anchor="middle" fill="%s">%s</text>' % (x, y - 22, colour, label))
lx, ly = PAD, H + PAD + 20
for k in 'HBAQCKUWYGPOS':
    label = '%s %d' % (NAME[k], count.get(k, 0))
    if lx + 26 + 14 * len(label) > W + PAD:
        lx, ly = PAD, ly + 24
    out.append('<rect x="%.1f" y="%.1f" width="16" height="16" fill="%s" stroke="#7d7d7d"/>' % (lx, ly, FILL[k]))
    out.append('<text x="%.1f" y="%.1f" font-size="13" fill="#222">%s</text>' % (lx + 21, ly + 13, label))
    lx += 36 + 14 * len(label)
out.append('<text x="%d" y="%.1f" font-size="13" fill="#222">マップ K（案）：600m×400m、50m の区画 12×8。北が上。赤丸＝拠点、太い黒線＝大通り（戦車が通れる）、灰色の線＝通り。畑・林なし</text>' % (PAD, ly + 44))
out.append('</svg>')
dst = os.path.join(ROOT, 'Docs', 'MapK-Sectors.svg')
io.open(dst, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
print('sectors', count, 'objectives', [(chr(65 + c) + str(r + 1)) for c, r, _, _ in sorted(objectives)], '->', dst)
