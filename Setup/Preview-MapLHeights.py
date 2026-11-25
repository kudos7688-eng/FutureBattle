# Offline copy of BattleMapL::MakeField (BattleMapL.h) to check heights and slopes and draw Docs/MapL-Heights.png. Units: cm.
# Keep it in step with BattleMapL.h by hand (python -I Setup/Preview-MapLHeights.py Docs/MapL-Heights.png).
import math, sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

HalfX, HalfY, Cell, Step = 50000., 35000., 5000., 250.
RiverX0, RiverX1, RiverY0, RiverY1 = 5500., 7100., 10000., 35000.
East = ["TRFTTFTFTF","TRHFTTFHFT","HRHHFTHTFF","HROHTHDHFT","HRHPHFTFTF","HHPHhTHSSS","OHHPHFTSSS","HPHHFTHSSS","HHWHTFHSSS","PHHFTHFTFT","HHFOHFTHFF","FHTHFTTFTF","TFHhhFTFFT","TFThhTTFTF"]
Columns, Rows = 20, 14
def kind(c, r):
    if c < 0 or r < 0 or c >= Columns or r >= Rows: return ' '
    if c < Columns // 2: c, r = Columns - 1 - c, Rows - 1 - r
    return East[Rows - 1 - r][c - Columns // 2]
def centre(c, r): return (-HalfX + Cell * (c + .5), -HalfY + Cell * (r + .5))
def smooth(t): t = min(max(t, 0.), 1.); return t * t * (3 - 2 * t)
def bump(d, h, rad): return 0. if d >= rad else h * math.cos(math.pi / 2 * d / rad) ** 2
def segdist(x, y, x0, y0, x1, y1):
    ax, ay = x1 - x0, y1 - y0; t = max(0., min(1., ((x - x0) * ax + (y - y0) * ay) / max(1., ax * ax + ay * ay)))
    return math.hypot(x - (x0 + ax * t), y - (y0 + ay * t))
def raw(x, y):
    tau = 2 * math.pi
    h = 260 + 110 * math.cos(tau * (x / 26000 + y / 41000)) + 80 * math.sin(tau * x / 19000) * math.sin(tau * y / 23000) \
        + 55 * math.cos(tau * (y / 9500 - x / 13000)) + 35 * math.cos(tau * (x / 7300 + y / 11800))
    for s in (1, -1):
        px, py = s * x, s * y
        h += bump(math.hypot(px - 22500, py - 7500), 600, 4000)
        h += bump(segdist(px, py, 18500, -29500, 21500, -29500), 1000, 6000)
        h += bump(math.hypot(px - 36000, py - 27000), 450, 9000)
        h += bump(math.hypot(px - 12500, py + 6500), 300, 8000)
        mid = (RiverX0 + RiverX1) / 2
        along = max(0., 1 - (RiverY0 - py) / 4000) if py < RiverY0 else 1.
        h -= along * bump(abs(px - mid), 300, 3200)
    h = max(h, 0.)
    for s in (1, -1):
        px, py = s * x, s * y
        dx, dy = max(0., 34000 - px), max(0., abs(py) - 12000)
        h *= smooth(math.hypot(dx, dy) / 9000)
    return h
# Pads: houses (H, O) and warehouses (W, D), east half
pads = []
for c in range(Columns // 2, Columns):
    for r in range(Rows):
        k = kind(c, r); mx, my = centre(c, r)
        if k == 'H': pads.append((mx - 300, my + 400, 1600, 1100))
        elif k == 'O': pads.append((mx + 1350, my + 1500, 1400, 1000))
for c in range(Columns // 2, Columns):
    for r in range(Rows):
        k = kind(c, r); mx, my = centre(c, r)
        if k == 'W': pads.append((mx, my + 300, 2800, 1800))
        elif k == 'D': pads.append((mx, my + 1450, 2600, 1100))
NX, NY = int(round(2 * HalfX / Step)) + 1, int(round(2 * HalfY / Step)) + 1
xs = -HalfX + np.arange(NX) * Step; ys = -HalfY + np.arange(NY) * Step
Z = np.array([[raw(x, y) for x in xs] for y in ys], dtype=np.float32)
plans = []
for (px, py, w, hh) in pads:
    for sg in (1, -1):
        cx, cy = sg * px, sg * py; level = raw(cx, cy); hx, hy = w / 2 + 300, hh / 2 + 300; plans.append((cx, cy, hx, hy, level))
        i0 = max(0, int(math.floor((cx - hx - 6000 + HalfX) / Step))); i1 = min(NX - 1, int(math.ceil((cx + hx + 6000 + HalfX) / Step)))
        j0 = max(0, int(math.floor((cy - hy - 6000 + HalfY) / Step))); j1 = min(NY - 1, int(math.ceil((cy + hy + 6000 + HalfY) / Step)))
        sub = Z[j0:j1 + 1, i0:i1 + 1].astype(np.float64)
        DX = np.maximum(0., np.abs(xs[i0:i1 + 1] - cx) - hx)[None, :]; DY = np.maximum(0., np.abs(ys[j0:j1 + 1] - cy) - hy)[:, None]
        allowed = .25 * np.hypot(DX, DY)
        Z[j0:j1 + 1, i0:i1 + 1] = (level + np.clip(sub - level, -allowed, allowed)).astype(np.float32)
flat = Z.ravel()
n = flat.size
for idx in range(n):
    p = n - 1 - idx
    if p < idx: flat[idx] = flat[p]
Z = flat.reshape(Z.shape)
# symmetry
sym = np.abs(Z - Z[::-1, ::-1]).max()
gx = np.gradient(Z, Step, axis=1); gy = np.gradient(Z, Step, axis=0); slope = np.hypot(gx, gy)
print('grid', NX, NY, 'low %.1f high %.1f cm' % (Z.min(), Z.max()), 'symmetry max diff %.4f' % sym)
print('slope: mean %.1f%%  p90 %.1f%%  p99 %.1f%%  max %.1f%% (%.1f deg)' % (slope.mean() * 100, np.percentile(slope, 90) * 100, np.percentile(slope, 99) * 100, slope.max() * 100, math.degrees(math.atan(slope.max()))))
for name, (x, y) in {'knoll': (22500, 7500), 'ridge': (20000, -29500), 'centre': (0, 0), 'obj4': (2500, 2500), 'bridgeE': (6300, 20000), 'home': (42000, 0)}.items():
    i, j = int(round((x + HalfX) / Step)), int(round((y + HalfY) / Step)); print('  %-8s %7.1f cm' % (name, Z[j, i]))
# Line of sight sample: how many pairs 150-300m apart along x are blocked by the ground at 150cm eye height
fig, ax = plt.subplots(figsize=(14, 10))
im = ax.imshow(Z / 100., origin='lower', extent=(-500, 500, -350, 350), cmap='terrain', vmin=-2, vmax=16)
cs = ax.contour(xs / 100., ys / 100., Z / 100., levels=np.arange(0, 16, 1), colors='k', linewidths=.4)
ax.clabel(cs, cs.levels[::2], fmt='%dm', fontsize=7)
for c in range(Columns + 1): ax.axvline(-500 + c * 50, color='w', lw=.3, alpha=.4)
for r in range(Rows + 1): ax.axhline(-350 + r * 50, color='w', lw=.3, alpha=.4)
for c in range(Columns):
    for r in range(Rows):
        mx, my = centre(c, r); ax.text(mx / 100, my / 100, kind(c, r), ha='center', va='center', fontsize=7, color='k', alpha=.6)
plt.colorbar(im, ax=ax, label='height (m)')
ax.set_title('Map L height field (grid 2.5m, contour 1m); letters = sector kinds of map I')
ax.set_xlabel('x (m)'); ax.set_ylabel('y (m)')
out = sys.argv[1] if len(sys.argv) > 1 else 'MapL-Heights.png'
fig.savefig(out, dpi=110, bbox_inches='tight'); print('wrote', out)
j, i = np.unravel_index(np.argmax(slope), slope.shape)
print('max slope at x=%.0f y=%.0f (m) h=%.1f' % (xs[i] / 100, ys[j] / 100, Z[j, i]))
idx = np.argsort(slope.ravel())[::-1][:4000:400]
for k in idx:
    jj, ii = np.unravel_index(k, slope.shape); print('  slope %.0f%% at (%.0f, %.0f) m kind=%s' % (slope[jj, ii] * 100, xs[ii] / 100, ys[jj] / 100, kind(int((xs[ii] + HalfX) // Cell), int((ys[jj] + HalfY) // Cell))))

worst = 0.
for (cx, cy, hx, hy, level) in plans:
    m = (np.abs(xs[None, :] - cx) <= hx) & (np.abs(ys[:, None] - cy) <= hy)
    worst = max(worst, float(np.abs(Z[m] - level).max()))
print('pad flatness: worst deviation %.2f cm over %d pads' % (worst, len(plans)))
