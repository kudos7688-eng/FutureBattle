# Copies the sounds and the explosion effect used by BattleAudio / BattleEffects (2026-11-22) from the user's asset projects
# (C:\Users\nanek\Documents\FPS\アセット, both UE 5.8 projects like this one) into Game/FutureBattle/Content, keeping the
# same /Game paths so the effects' own references (materials, textures, curves) still resolve.
# Sources and terms are listed in Docs/ASSET_SOURCES_AUDIO_FX.md. Only plain SoundWaves without references to other
# packages are taken (Lyra's sounds that point at its sound classes / mix are skipped on purpose).
#
#   python Setup/Import-AudioFXAssets.py            copy (existing files are overwritten)
#   python Setup/Import-AudioFXAssets.py --dry-run  list only
#   python Setup/Import-AudioFXAssets.py --list-md  print the Markdown table used in the asset document
import os, re, shutil, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEST = os.path.normpath(os.path.join(HERE, '..', 'Game', 'FutureBattle', 'Content'))
ASSETS = r'C:\Users\nanek\Documents\FPS\アセット'
LYRA = os.path.join(ASSETS, 'LyraStarterGame', 'Content')
TPS = os.path.join(ASSETS, 'TPSMultiplayershooterwith', 'Content')

def numbered(prefix, first, last, width=2):
    return [f'{prefix}{i:0{width}d}' if width else f'{prefix}{i}' for i in range(first, last + 1)]

# (source root, label, [package paths]); "closure" entries also take every /Game package they reference.
SOUNDS = [
    (TPS, 'TPS', [f'/Game/Multiplayercovershooter/sounds/wav/{n}' for n in
        numbered('M4_Designed_Oneshot_', 1, 4, 0) + numbered('M4_Designed_Dist_Oneshot_', 1, 4, 0) +
        numbered('AX50_Designed_Oneshot_', 1, 4, 0) + numbered('AX50_Designed_Dist_Oneshot_', 1, 4, 0) +
        ['SW_reload_begin', 'SW_reload_end', 'SW_trigger_empty', 'SW_aiming_1', 'SW_sniper_zoom_in', 'SW_sniper_zoom_out']]),
    (LYRA, 'Lyra', [f'/Game/Audio/Sounds/Explosions/{n}' for n in
        numbered('Explosions_Grenade_Punch-Close_', 1, 4) + numbered('Explosions_Grenade_Noise-Exterior_', 1, 4) +
        numbered('Explosions_Grenade_Punch-Distant_', 1, 4) + numbered('Explosions_Grenade_Noise-Exterior-Distant_', 1, 4)] +
        [f'/Game/Audio/Sounds/Grenade/{n}' for n in numbered('grenade_explode_thump_', 1, 3)] +
        [f'/Game/Audio/SoundWaves/Weapons/{n}' for n in numbered('SFX_BulletImpact_', 1, 8)] +
        [f'/Game/Audio/Sounds/Impacts/{n}' for n in numbered('Lyra_Plyr_BulletImpact_', 1, 6)] +
        [f'/Game/Audio/Sounds/WhizBys/{n}' for n in numbered('sfx_AR_Whizbys_Oncoming_Close_', 1, 4) + numbered('sfx_AR_Whizbys_Oncoming_Mid_', 1, 2)]),
]
# The TPS bullet impact effect (P_Wep_BulletImpact_Rifle) was tried and dropped on 2026-11-22: it did not show in the game.
EFFECTS = [
    (LYRA, 'Lyra', ['/Game/Effects/Particles/Explosion/NS_Grenade_Explosion']),
]

REF = re.compile(rb'(/Game/[A-Za-z0-9_/\-]+)')

def file_of(root, package):
    return os.path.join(root, *package[len('/Game/'):].split('/')) + '.uasset'

def resolve(root, package):
    """The file of a referenced package. FName keeps a trailing _N (N without leading zero) as a number, so a name table
    entry 'T_Smoke_8' may stand for 'T_Smoke_8_8': try the plain name first, then base_N."""
    path = file_of(root, package)
    if os.path.exists(path):
        return package
    folder, base = os.path.split(path[:-len('.uasset')])
    if os.path.isdir(folder):
        for name in sorted(os.listdir(folder)):
            m = re.fullmatch(re.escape(base) + r'_([1-9][0-9]*|0)\.uasset', name)
            if m:
                return package + '_' + m.group(1)
    return None

def references(root, package):
    data = open(file_of(root, package), 'rb').read()
    found = set()
    for raw in REF.findall(data):
        ref = raw.decode()
        if ref == package or package.startswith(ref + '_'):  # its own name (maybe split into base + number)
            continue
        found.add(ref)
    return found

def closure(root, start):
    seen, missing, todo = set(), set(), [start]
    while todo:
        package = todo.pop()
        actual = resolve(root, package)
        if actual is None:
            missing.add(package)
            continue
        if actual in seen:
            continue
        seen.add(actual)
        todo.extend(references(root, actual))
    return seen, missing

def main():
    dry = '--dry-run' in sys.argv
    table = '--list-md' in sys.argv
    plan = []  # (label, kind, package, source file)
    for root, label, packages in SOUNDS:
        for package in packages:
            actual = resolve(root, package)
            if actual is None:
                sys.exit(f'missing sound {package} in {root}')
            outside = references(root, actual)
            if outside:
                sys.exit(f'{package} refers to other packages {sorted(outside)}; not a plain SoundWave, skip it')
            plan.append((label, 'sound', actual, file_of(root, actual)))
    for root, label, starts in EFFECTS:
        for start in starts:
            packages, missing = closure(root, start)
            # Names that only appear as text (authoring history in the source project, missing there as well).
            for m in sorted(missing):
                print(f'note: {start} mentions {m}, which the source project does not have either (not copied)')
            for package in sorted(packages):
                plan.append((label, 'effect', package, file_of(root, package)))
    total = 0
    if table:
        print('| 取り込み先（/Game） | 出典 | 種類 | 大きさ |')
        print('|---|---|---|---|')
    for label, kind, package, source in plan:
        size = os.path.getsize(source)
        total += size
        target = file_of(DEST, package)
        if table:
            print(f'| `{package}` | {label} | {kind} | {size // 1024} KB |')
        elif dry:
            print(f'{label:4} {kind:6} {package}  {size // 1024} KB')
        else:
            os.makedirs(os.path.dirname(target), exist_ok=True)
            shutil.copy2(source, target)
    print(f'{len(plan)} files, {total / 1048576:.1f} MB' + (' (dry run)' if dry or table else f' copied into {DEST}'))

if __name__ == '__main__':
    main()
