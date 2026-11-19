# Draws the sector plan of map I (BattleMapI.h) as Docs/MapI-Sectors.svg. No engine, no build.
#   python -I Setup/Export-MapI.py
# The sector table, roads, river, hills and objectives are read from BattleMapI.h, so the picture follows the source.
import io, os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'Game', 'FutureBattle', 'Source', 'FutureBattle', 'Prototype', 'BattleMapI.h')
text = io.open(SRC, encoding='utf-8').read()
east = re.findall(r'TEXT\("([A-Za-z]{10})"\)', text)
assert len(east) == 14, len(east)
COLS, ROWS, CELL = 20, 14, 5000


def kind(c, r):
    if c < COLS // 2:
        c, r = COLS - 1 - c, ROWS - 1 - r
    return east[ROWS - 1 - r][c - COLS // 2]


def num(name):
    return float(re.search(name + r'=(-?[0-9.]+)', text).group(1))


river = [num(n) for n in ('RiverX0', 'RiverX1', 'RiverY0', 'RiverY1', 'BridgeY', 'BridgeHalf', 'FordY', 'FordHalf')]
hills = [tuple(float(v) for v in m) for m in re.findall(
    r'H\.X=(-?[0-9.]+);H\.Y=(-?[0-9.]+);H\.W=([0-9.]+);H\.H=([0-9.]+);H\.Top=([0-9.]+);H\.Run=([0-9.]+)', text)]
objectives = [(float(a), float(b)) for a, b in re.findall(r'FVector\((-?[0-9]+),(-?[0-9]+),0\)', text.split('inline TArray<FVector> Objectives()')[1])]

S = 0.012  # pixels per cm
W, H = 100000 * S, 70000 * S
PAD = 30


def px(x, y):
    # World +X (east) to the right, +Y (north) up.
    return PAD + (x + 50000) * S, PAD + (35000 - y) * S


FILL = {'H': '#cbb69a', 'W': '#b7a58f', 'P': '#d9d4c7', 'O': '#f2d16b', 'D': '#f0b24a', 'F': '#e6dc9a', 'T': '#7fae6f',
        'R': '#dfe9e3', 'h': '#b9c48a', 'S': '#c9d6ea'}
NAME = {'H': '家', 'W': '倉庫', 'P': '広場', 'O': '拠点', 'D': '拠点(集積所)', 'F': '畑', 'T': '林', 'R': '川', 'h': '丘', 'S': '本拠地'}
out = []
out.append('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" font-family="sans-serif">' % (W + 2 * PAD, H + 2 * PAD + 70))
out.append('<rect width="100%" height="100%" fill="#ffffff"/>')
count = {}
for c in range(COLS):
    for r in range(ROWS):
        k = kind(c, r)
        count[k] = count.get(k, 0) + 1
        x, y = px(-50000 + c * CELL, -35000 + (r + 1) * CELL)
        out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s" stroke="#8a8a8a" stroke-width="0.5"/>' % (x, y, CELL * S, CELL * S, FILL[k]))
        if k in 'HWOD':
            # The building: a house north-west of the middle, in the north-east corner on an objective plaza, a warehouse in the north.
            cx, cy = -50000 + (c + .5) * CELL, -35000 + (r + .5) * CELL
            sign = 1 if c >= COLS // 2 else -1
            bx, by, bw, bh = {'H': (-300, 400, 1600, 1100), 'O': (1350, 1500, 1400, 1000), 'D': (0, 1450, 2600, 1100), 'W': (0, 300, 2800, 1800)}[k]
            x0, y0 = px(cx + sign * bx - bw / 2, cy + sign * by + bh / 2)
            out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="#6b5a48"/>' % (x0, y0, bw * S, bh * S))
# Hills (top and footprint), both halves.
for hx, hy, hw, hh, top, run in hills:
    for sign in (1, -1):
        x0, y0 = px(sign * hx - hw / 2 - run, sign * hy + hh / 2 + run)
        out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="#93a35c" stroke="#5d6b33"/>' % (x0, y0, (hw + 2 * run) * S, (hh + 2 * run) * S))
        x1, y1 = px(sign * hx - hw / 2, sign * hy + hh / 2)
        out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="#6f7f3c"/>' % (x1, y1, hw * S, hh * S))
        tx, ty = px(sign * hx, sign * hy)
        out.append('<text x="%.1f" y="%.1f" font-size="11" text-anchor="middle" fill="#ffffff">%dm</text>' % (tx, ty + 4, top / 100))
# Rivers with bridge and ford.
rx0, rx1, ry0, ry1, by, bh, fy, fh = river
for sign in (1, -1):
    xa, ya = px(min(sign * rx0, sign * rx1), max(sign * ry0, sign * ry1))
    out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="#4f8fd0"/>' % (xa, ya, (rx1 - rx0) * S, (ry1 - ry0) * S))
    for mid, half, colour, label in ((by, bh, '#7a5a3a', '橋'), (fy, fh + 150, '#d8c98a', '浅瀬')):
        xb, yb = px(min(sign * rx0, sign * rx1) - 200, sign * mid + half)
        out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%.1f" fill="%s"/>' % (xb, yb, (rx1 - rx0 + 400) * S, 2 * half * S, colour))
        tx, ty = px(sign * (rx1 + 1500), sign * mid)
        out.append('<text x="%.1f" y="%.1f" font-size="11" text-anchor="middle" fill="#1d3f66">%s</text>' % (tx, ty + 4, label))
# Roads.
roads = [(True, 0, -42000, 42000), (True, 20000, -35000, 35000), (True, -20000, -35000, 35000)] + [(False, x, -20000, 20000) for x in (-35000, -25000, -10000, 0, 10000, 25000, 35000)]
for along_x, at, a, b in roads:
    (x0, y0), (x1, y1) = (px(a, at), px(b, at)) if along_x else (px(at, a), px(at, b))
    out.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="#3a3f45" stroke-width="%.1f"/>' % (x0, y0, x1, y1, 800 * S))
# Objectives and homes.
for i, (ox, oy) in enumerate(objectives):
    x, y = px(ox, oy)
    out.append('<circle cx="%.1f" cy="%.1f" r="9" fill="none" stroke="#b3261e" stroke-width="2.5"/>' % (x, y))
    out.append('<text x="%.1f" y="%.1f" font-size="12" font-weight="bold" text-anchor="middle" fill="#b3261e">%d</text>' % (x, y + 4, i))
for sign, colour, label in ((-1, '#0b7a66', '西の本拠地'), (1, '#a3261c', '東の本拠地')):
    x, y = px(sign * 42000, 0)
    out.append('<rect x="%.1f" y="%.1f" width="22" height="22" fill="%s"/>' % (x - 11, y - 11, colour))
    out.append('<text x="%.1f" y="%.1f" font-size="12" text-anchor="middle" fill="%s">%s</text>' % (x, y - 16, colour, label))
# Legend.
lx, ly = PAD, H + PAD + 22
for k in 'HWPODFTRhS':
    out.append('<rect x="%.1f" y="%.1f" width="16" height="16" fill="%s" stroke="#8a8a8a"/>' % (lx, ly, FILL[k]))
    label = '%s %d' % (NAME[k], count.get(k, 0))
    out.append('<text x="%.1f" y="%.1f" font-size="13" fill="#222">%s</text>' % (lx + 21, ly + 13, label))
    lx += 34 + 14 * len(label)
out.append('<text x="%d" y="%.1f" font-size="13" fill="#222">マップ I（sectors-v1）：1000m×700m、50m の区画 20×14。北が上。赤丸の数字＝拠点、茶色＝入れる建物、黒線＝道路</text>' % (PAD, H + PAD + 60))
out.append('</svg>')
dst = os.path.join(ROOT, 'Docs', 'MapI-Sectors.svg')
io.open(dst, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
print('sectors', count, '->', dst)
