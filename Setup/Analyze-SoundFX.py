# Measures the recording of the sound check (Setup/Test-SoundFX.ps1, BattleFXCheck.cpp, 2026-11-22): the loudness of the
# game's mix in each event's window against the quiet before the first event. Prints AUDIO_CHECK lines and draws
# <wav>.png (envelope with the event marks). Exit code 1 if an event is not heard.
#
#   python -I Setup/Analyze-SoundFX.py <fx-audio.wav>
import os, struct, sys
import numpy as np

# (seconds after the recording starts, window length, name). BattleFXCheck starts the recording 0.5 s into the check.
EVENTS = [
    (0.5, 0.7, 'rifle shots x6'), (1.7, 0.6, 'machine-gun shots x4'), (2.7, 0.4, 'bullet impacts'),
    (3.5, 0.8, 'explosion size 1'), (5.0, 1.0, 'explosion size 2.5'), (7.0, 0.8, 'rocket launch (synth)'),
    (8.0, 0.8, 'mortar launch (synth)'), (9.0, 0.8, 'cannon fire'), (10.0, 1.7, 'incoming whistle (synth)'),
    (11.8, 1.0, 'explosion size 2'), (13.0, 0.9, 'objective taken (synth)'), (14.5, 0.9, 'objective lost (synth)'),
    (16.0, 1.5, 'victory (synth)'), (18.0, 1.8, 'defeat (synth)'),
]
LEAD = 0.1   # a sound starts a little after the frame that asked for it
QUIET = (0.05, 0.4)  # nothing plays yet (the battle is paused)
MARGIN_DB = 20.0

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

def loudness_db(x, rate, start, length):
    a, b = int(max(0, start) * rate), int(max(0, start + length) * rate)
    seg = x[a:b]
    if len(seg) == 0:
        return -200.0, -200.0
    win = max(1, int(.05 * rate))  # 50 ms RMS, loudest window
    mono = seg.mean(axis=1)
    n = len(mono) // win
    rms = np.sqrt((mono[: n * win].reshape(n, win) ** 2).mean(axis=1)) if n else np.array([np.sqrt((mono ** 2).mean())])
    peak = np.abs(seg).max()
    return 20 * np.log10(max(rms.max(), 1e-10)), 20 * np.log10(max(peak, 1e-10))

def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    path = sys.argv[1]
    x, rate = read_wav(path)
    print(f'AUDIO_CHECK file={os.path.basename(path)} seconds={len(x) / rate:.1f} rate={rate} channels={x.shape[1]}')
    floor, _ = loudness_db(x, rate, QUIET[0], QUIET[1] - QUIET[0])
    print(f'AUDIO_CHECK quiet_before_first_event rms_db={floor:.1f}')
    failures = 0
    for at, length, name in EVENTS:
        rms, peak = loudness_db(x, rate, at - LEAD, length + 2 * LEAD)
        ok = rms > max(floor, -90.0) + MARGIN_DB
        failures += not ok
        print(f'AUDIO_CHECK {"PASS" if ok else "FAIL"} {name}: rms_db={rms:.1f} peak_db={peak:.1f} at={at:.1f}s')
    clipped = float((np.abs(x) >= .999).mean() * 100)
    print(f'AUDIO_CHECK clipped_samples_percent={clipped:.3f}')
    try:
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        win = int(.02 * rate)
        mono = x.mean(axis=1)
        n = len(mono) // win
        env = 20 * np.log10(np.maximum(np.sqrt((mono[: n * win].reshape(n, win) ** 2).mean(axis=1)), 1e-6))
        t = np.arange(n) * win / rate
        fig, (top, bottom) = plt.subplots(2, 1, figsize=(14, 7), sharex=True)
        top.plot(t, env, lw=.8)
        for at, length, name in EVENTS:
            top.axvspan(at - LEAD, at + length + LEAD, color='orange', alpha=.15)
            top.text(at, -5, name, rotation=90, fontsize=7, va='top')
        top.set_ylim(-100, 0); top.set_ylabel('RMS dBFS (20 ms)')
        bottom.specgram(mono, NFFT=2048, Fs=rate, noverlap=1536, cmap='magma'); bottom.set_ylim(0, 12000)
        bottom.set_xlabel('seconds after the recording starts'); bottom.set_ylabel('Hz')
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
