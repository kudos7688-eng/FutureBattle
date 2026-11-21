# Measures the recording of the sound check (Setup/Test-SoundFX.ps1, BattleFXCheck.cpp, 2026-11-22). Each sound is found
# from the audio clock that the game wrote with it (FX_CHECK_EVENT lines in run.log next to the recording, relative to
# FX_CHECK_RECORD); a sound counts as heard when the mix gets at least RISE_DB louder than just before it. Prints
# AUDIO_CHECK lines and draws <wav>.png (envelope with the event marks). Exit code 1 if a sound is not heard.
#
#   python -I Setup/Analyze-SoundFX.py <fx-audio.wav>
import os, re, struct, sys
import numpy as np

RISE_DB = 6.0
WINDOW = {'incoming_whistle': 1.7, 'victory': .6, 'defeat': .6}  # seconds measured after a sound starts (default .4)
LATENCY = .03  # a sound starts a little after the clock value written with it
# The first version of the check (before the clock was logged, 2026-11-22 6:39) used this schedule.
SCHEDULE = [('rifle', 1., 1.5), ('machine_gun', 2.2, 2.5), ('bullet_impacts', 3.2, 3.2), ('explosion_1', 4., 4.),
            ('explosion_2.5', 5.5, 5.5), ('rocket_launch', 7.5, 7.5), ('mortar_launch', 8.5, 8.5), ('cannon_fire', 9.5, 9.5),
            ('incoming_whistle', 10.5, 10.5), ('explosion_2', 12.3, 12.3), ('objective_taken', 13.5, 13.5),
            ('objective_lost', 15., 15.), ('victory', 16.5, 16.5), ('defeat', 18.5, 18.5)]


def onset_track(env):
    return np.maximum(0., env - np.concatenate([np.full(3, env[0]), env[:-3]]))


def strongest(rise, step, t, lo, hi):
    """The offset in [lo, hi] that puts the strongest onset at time t (start of a sound)."""
    best, score = lo, -1.
    for o in np.arange(lo, hi + 1e-9, .01):
        i, j = int((t + o - .03) / step), int((t + o + .12) / step)
        if 0 <= i < len(rise):
            v = float(rise[i:max(j, i + 1)].max())
            if v > score:
                best, score = float(o), v
    return best


def align(env, step, events):
    """Where each logged sound is in the recording. The recording starts a little after the clock value logged for its
    start (about 0.45 s) and runs short of the audio clock at a steady rate (about 9% in the runs of 2026-11-22, the device
    at 48 kHz): find the scale and offset that put the most onset strength (rise of the 20 ms loudness over 60 ms) at the
    logged starts, then let each sound settle within +-0.15 s."""
    rise = onset_track(env)
    best = (1., 0., -1.)
    for scale in np.arange(.85, 1.0501, .0025):
        for o in np.arange(-1., 1.0001, .02):
            total = 0.
            for _, first, _ in events:
                t = first * scale + o
                i, j = int((t - .03) / step), int((t + .12) / step)
                if 0 <= i < len(rise):
                    total += rise[i:max(j, i + 1)].max()
            if total > best[2]:
                best = (float(scale), float(o), total)
    scale, offset, _ = best
    out = []
    for name, first, last in events:
        guess = first * scale + offset
        fine = strongest(rise, step, guess, -.15, .15)
        out.append((name, guess + fine, last * scale + offset + fine, scale, offset))
    return out


def events_from_log(folder):
    """[(name, first, last)] in seconds after the recording starts; consecutive same names form one group."""
    log = os.path.join(folder, 'run.log')
    if not os.path.exists(log):
        raise SystemExit(f'no run.log next to the recording: {log}')
    record, raw = None, []
    for line in open(log, encoding='utf-8', errors='replace'):
        m = re.search(r'FX_CHECK_RECORD audio=(-?[0-9.]+)', line)
        if m:
            record = float(m.group(1))
        m = re.search(r'FX_CHECK_EVENT (\S+) audio=(-?[0-9.]+)', line)
        if m:
            raw.append((m.group(1), float(m.group(2))))
    if record is None or record < 0 or not raw:
        # Runs before the clock was logged: BattleFXCheck's schedule, minus the 0.5 s before the recording starts.
        print('AUDIO_CHECK note: no audio clock in run.log, using the fixed schedule')
        return [(n, a - .5, b - .5) for n, a, b in SCHEDULE]
    groups = []
    for name, clock in raw:
        t = clock - record
        if groups and groups[-1][0] == name:
            groups[-1][2] = t
        else:
            groups.append([name, t, t])
    return [(n, a, b) for n, a, b in groups]


def read_wav(path):
    data = open(path, 'rb').read()
    if data[:4] != b'RIFF' or data[8:12] != b'WAVE':
        raise SystemExit(f'not a WAV file: {path}')
    pos, fmt, frames = 12, None, None
    while pos + 8 <= len(data):
        tag, size = data[pos:pos + 4], struct.unpack('<I', data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if tag == b'fmt ':
            fmt = struct.unpack('<HHIIHH', body[:16])
        elif tag == b'data':
            frames = body
        pos += 8 + size + (size & 1)
    if fmt is None or frames is None:
        raise SystemExit('WAV without fmt or data chunk')
    kind, channels, rate, _, _, bits = fmt
    if kind == 3 and bits == 32:
        x = np.frombuffer(frames, '<f4')
    elif bits == 16:
        x = np.frombuffer(frames, '<i2') / 32768.0
    elif bits == 32:
        x = np.frombuffer(frames, '<i4') / 2147483648.0
    else:
        raise SystemExit(f'unsupported WAV format {kind}/{bits} bits')
    x = x[: len(x) // channels * channels].reshape(-1, channels).astype(np.float64)
    return x, rate


def rms_track(x, rate, win_s=.02):
    win = max(1, int(win_s * rate))
    mono = x.mean(axis=1)
    n = len(mono) // win
    env = np.sqrt((mono[: n * win].reshape(n, win) ** 2).mean(axis=1))
    return 20 * np.log10(np.maximum(env, 1e-10)), win / rate


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    path = sys.argv[1]
    x, rate = read_wav(path)
    events = events_from_log(os.path.dirname(os.path.abspath(path)))
    env, step = rms_track(x, rate)
    print(f'AUDIO_CHECK file={os.path.basename(path)} seconds={len(x) / rate:.1f} rate={rate} channels={x.shape[1]}')
    aligned = align(env, step, events)
    print(f'AUDIO_CHECK clock_to_recording scale={aligned[0][3]:.4f} offset_s={aligned[0][4]:+.2f}')
    events = [(n, a, b) for n, a, b, _, _ in aligned]
    seconds = len(x) / rate

    def span(a, b):
        i, j = max(0, int(a / step)), max(0, int(b / step))
        return env[i:max(j, i + 1)]

    failures = 0
    for name, first, last in events:
        if first + LATENCY + .1 > seconds:
            failures += 1
            print(f'AUDIO_CHECK FAIL {name}: at={first:.2f}s after the end of the recording ({seconds:.1f} s)')
            continue
        start = first + LATENCY
        after = span(start - .03, last + LATENCY + WINDOW.get(name, .4))
        before = span(start - .35, start - .05)
        loud = float(after.max()) if len(after) else -200.
        base = float(np.median(before)) if len(before) else -200.
        a, b = int(max(0., start - .03) * rate), min(len(x), int((last + LATENCY + WINDOW.get(name, .4)) * rate))
        peak = 20 * np.log10(max(float(np.abs(x[a:b]).max()) if b > a else 0., 1e-10))
        ok = loud - base >= RISE_DB and loud > -60.
        if name == 'incoming_whistle':
            # It swells from a whisper: heard when its end is RISE_DB louder than its first half second.
            early = span(start + .2, start + .6)
            end = span(last + LATENCY + WINDOW[name] - .35, last + LATENCY + WINDOW[name] - .05)
            base = float(np.median(early)) if len(early) else -200.
            loud = float(end.max()) if len(end) else -200.
            ok = loud - base >= RISE_DB and loud > -60.
        failures += not ok
        print(f'AUDIO_CHECK {"PASS" if ok else "FAIL"} {name}: at={first:.2f}s loudest_rms_db={loud:.1f} peak_db={peak:.1f} before_db={base:.1f} rise_db={loud - base:.1f}')
    tail = span(events[-1][2] + 3., seconds)
    if len(tail):
        print(f'AUDIO_CHECK quiet_after_last_sound_db={float(np.median(tail)):.1f}')
    peak = 20 * np.log10(max(float(np.abs(x).max()), 1e-10))
    clipped = float((np.abs(x) >= .999).mean() * 100)
    print(f'AUDIO_CHECK mix_peak_db={peak:.1f} clipped_samples_percent={clipped:.3f}')
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        t = np.arange(len(env)) * step
        fig, (top, bottom) = plt.subplots(2, 1, figsize=(14, 7), sharex=True)
        top.plot(t, env, lw=.8)
        for name, first, last in events:
            top.axvspan(first, last + WINDOW.get(name, .4), color='orange', alpha=.15)
            top.text(first, -3, name, rotation=90, fontsize=7, va='top')
        top.set_ylim(-100, 0)
        top.set_ylabel('RMS dBFS (20 ms)')
        with np.errstate(divide='ignore'):
            bottom.specgram(x.mean(axis=1) + 1e-9, NFFT=2048, Fs=rate, noverlap=1536, cmap='magma')
        bottom.set_ylim(0, 12000)
        bottom.set_xlabel('seconds after the recording starts')
        bottom.set_ylabel('Hz')
        plt.tight_layout()
        out = os.path.splitext(path)[0] + '.png'
        plt.savefig(out, dpi=80)
        print(f'AUDIO_CHECK plot={out}')
    except ImportError:
        pass
    print(f'AUDIO_CHECK_RESULT failures={failures}')
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
