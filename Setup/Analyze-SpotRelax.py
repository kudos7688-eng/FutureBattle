# Spot relaxation (Docs/design-spot-relax.md): how active a match was. Reads metrics.json and run.log of result folders
# (names under Setup/Benchmarks/SightTrial-20261111, or paths) and prints one column per match. No build, no lock.
#   python -I Setup/Analyze-SpotRelax.py <result> [<result> ...] [--root <dir>]
import json, os, re, sys

def find(name, root):
    if os.path.isdir(name): return name
    for base in (root, os.path.join(os.path.dirname(os.path.abspath(__file__)), 'Benchmarks', 'SightTrial-20261111'),
                 r'C:\Users\nanek\Documents\FPS\FutureBattle\Setup\Benchmarks\SightTrial-20261111'):
        if base and os.path.isdir(os.path.join(base, name)): return os.path.join(base, name)
    return None

def read(folder):
    m = json.load(open(os.path.join(folder, 'metrics.json'), encoding='utf-8-sig'))
    r = {}
    r['得点 西/東'] = '%d/%d' % (m.get('ally_score', -1), m.get('enemy_score', -1))
    for label, key in (('発砲', 'shots'), ('与えた損害', 'damage'), ('撃破', 'kills'), ('戦死', 'deaths')):
        v = [sum(t.get(key, 0) for t in m.get(side + '_type_activity', [])) for side in ('ally', 'enemy')]
        r[label + ' 西/東'] = '%d/%d' % (v[0], v[1]); r['_' + key] = v[0] + v[1]
    r['与えた損害 合計'] = '%d' % r['_damage']; r['発砲 合計'] = '%d' % r['_shots']; r['撃破 合計'] = '%d' % r['_kills']
    ts = m.get('tactical_squad_seconds', {}); tot = sum(ts.values()) or 1
    r['ENGAGE の時間'] = '%.1f%%' % (100. * ts.get('ENGAGE', 0) / tot)
    r['RETREAT＋RECOVER の時間'] = '%.1f%%' % (100. * (ts.get('RETREAT', 0) + ts.get('RECOVER / DEFEND', 0)) / tot)
    ev = m.get('tactical_events', {})
    r['ENGAGE の回数'] = str(ev.get('ENGAGE', 0)); r['RETREAT の回数'] = str(ev.get('RETREAT', 0))
    r['向かった回数（SeekEnemyGo）'] = str(ev.get('SeekEnemyGo', 0))
    r['雇った小隊 西/東'] = '%s/%s' % (m.get('ally_hired_squads'), m.get('enemy_hired_squads'))
    r['残した資源 西/東'] = '%s/%s' % (m.get('ally_resources'), m.get('enemy_resources'))
    r['補充（完了）'] = str(m.get('completed_replacements'))
    r['本拠地攻撃の進み（最大）西/東'] = '%.2f/%.2f' % (m.get('ally_base_assault_max_progress', 0), m.get('enemy_base_assault_max_progress', 0))
    r['終了時の拠点 西/東'] = '%d/%d' % (sum(1 for o in m.get('final_objectives', []) if o.get('owner') == 0), sum(1 for o in m.get('final_objectives', []) if o.get('owner') == 1))
    r['拠点の100m以内の発砲'] = str(sum(o.get('shots_within_100m', 0) for o in m.get('final_objectives', [])))
    log = os.path.join(folder, 'run.log')
    if os.path.isfile(log):
        n = [0, 0]; known = [0, 0]; near = 0; steps = [0, 0]; back = [0, 0]; owner = {}; flips = 0; seek = [0, 0]; truth = 0; kinds = [0] * 6; relax = {}
        sample = re.compile(r'BATTLE_ST_SAMPLE t=[\d.]+ squad=\d+ team=(\d) managed=1 living=(\d+) .* contacts=(\d+)')
        with open(log, encoding='utf-8', errors='replace') as f:
            for line in f:
                if 'BATTLE_ST_SAMPLE' in line:
                    s = sample.search(line)
                    if s and int(s.group(2)) > 0:
                        t = int(s.group(1)); n[t] += 1; known[t] += int(s.group(3)) > 0
                elif 'BATTLE_FRONT_STEP' in line:
                    s = re.search(r'team=(\d)', line)
                    if s: steps[int(s.group(1))] += 1
                elif 'BATTLE_FRONT_BACK' in line:
                    s = re.search(r'team=(\d)', line)
                    if s: back[int(s.group(1))] += 1
                elif 'BATTLE_SEEK_ENEMY ' in line:
                    s = re.search(r'team=(\d)', line)
                    if s: seek[int(s.group(1))] += 1
                    s = re.search(r' kind=(\d)', line)
                    if s: kinds[int(s.group(1))] += 1
                elif 'BATTLE_SPOT_RELAX' in line:
                    s = re.search(r'team=(\d) t=\d+ share=(\d+) shots=(\d+) lost=(\d+) hq=(\d+)', line)
                    if s: relax[int(s.group(1))] = [int(s.group(i)) for i in range(2, 6)]
                elif 'BATTLE_SPOT_TRUTH' in line: truth += 1
        r['小隊が敵を知っている時間 西/東'] = '%.1f%%/%.1f%%' % (100. * known[0] / (n[0] or 1), 100. * known[1] / (n[1] or 1))
        r['小隊が敵を知っている時間 全体'] = '%.1f%%' % (100. * (known[0] + known[1]) / ((n[0] + n[1]) or 1))
        r['前線が進んだ回数 西/東'] = '%d/%d' % (steps[0], steps[1]); r['前線が下がった回数 西/東'] = '%d/%d' % (back[0], back[1])
        r['向かった回数 西/東（記録）'] = '%d/%d' % (seek[0], seek[1]); r['BATTLE_SPOT_TRUTH の行'] = str(truth)
        r['向かった先の種類（目撃/伝聞/発砲/拠点/司令部/真）'] = '/'.join(map(str, kinds))
        tot = [sum(v[i] for v in relax.values()) for i in range(4)] if relax else [0] * 4
        r['マスへ書いた数（伝聞/発砲/拠点/司令部）'] = '/'.join(map(str, tot))
    r['勝ち'] = {0: '西', 1: '東'}.get(m.get('winner'), str(m.get('winner'))) + ' (' + str(m.get('reason')) + ')'
    return r

def main():
    args = sys.argv[1:]; root = None
    if '--root' in args: i = args.index('--root'); root = args[i + 1]; del args[i:i + 2]
    cols = []
    for a in args:
        f = find(a, root)
        cols.append((os.path.basename(a.rstrip('/\\')), read(f) if f else None))
    keys = []
    for _, r in cols:
        for k in (r or {}):
            if not k.startswith('_') and k not in keys: keys.append(k)
    print('| | ' + ' | '.join(n for n, _ in cols) + ' |'); print('|---|' + '---|' * len(cols))
    for k in keys: print('| %s | ' % k + ' | '.join((r.get(k, '-') if r else 'NOT READ') for _, r in cols) + ' |')
    base = cols[0][1] if cols and cols[0][1] else None
    if base:
        for label, key in (('与えた損害 合計（先頭比）', '_damage'), ('発砲 合計（先頭比）', '_shots'), ('撃破 合計（先頭比）', '_kills')):
            print('| %s | ' % label + ' | '.join(('%+.0f%%' % (100. * (r[key] - base[key]) / (base[key] or 1)) if r else '-') for _, r in cols) + ' |')

if __name__ == '__main__': main()
