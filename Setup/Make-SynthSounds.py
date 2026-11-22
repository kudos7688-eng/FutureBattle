# Synthesizes the sounds the asset projects do not have (2026-11-22, Claude; no outside material): anti-tank rocket launch,
# mortar launch, incoming shell whistle, objective captured by us / by the enemy, victory and defeat.
# Output: Setup/SynthAudio/SW_Synth_*.wav (48 kHz, 16-bit, mono so the engine can place them in 3D).
# Import into /Game/Audio/Battle with Setup/Import-SynthSounds.ps1 (UE's ImportAssets commandlet).
# Deterministic (fixed seed): running it again gives the same files.
#
#   python Setup/Make-SynthSounds.py
import os, wave
import numpy as np

SR = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'SynthAudio')
rng = np.random.default_rng(20261122)

def timeline(seconds):
    return np.arange(int(seconds * SR)) / SR

def noise(n):
    return rng.standard_normal(n)

def band(x, lo=None, hi=None, order=2):
    """Zero-phase filter in the frequency domain (Butterworth-like magnitude)."""
    spectrum = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / SR)
    gain = np.ones_like(f)
    if hi is not None:
        gain /= np.sqrt(1 + (f / hi) ** (2 * order))
    if lo is not None:
        with np.errstate(divide='ignore'):
            gain /= np.sqrt(1 + (lo / np.maximum(f, 1e-6)) ** (2 * order))
    return np.fft.irfft(spectrum * gain, len(x))

def envelope(t, attack, decay, start=0.0):
    u = np.clip(t - start, 0, None)
    e = (1 - np.exp(-u / max(attack, 1e-5))) * np.exp(-u / decay)
    e[t < start] = 0
    return e

def sweep(t, freq):
    """Sine whose frequency follows the array freq (Hz)."""
    return np.sin(2 * np.pi * np.cumsum(freq) / SR)

def soft_clip(x, drive):
    return np.tanh(drive * x) / np.tanh(drive)

def unit(x):
    return x / (np.max(np.abs(x)) + 1e-12)

def finish(x, fade_in=0.001, fade_out=0.03, peak=0.89):
    x = unit(x)
    n_in, n_out = int(fade_in * SR), int(fade_out * SR)
    if n_in:
        x[:n_in] *= np.linspace(0, 1, n_in)
    if n_out:
        x[-n_out:] *= np.linspace(1, 0, n_out)
    return x * peak

def write(name, x):
    os.makedirs(OUT, exist_ok=True)
    data = (np.clip(x, -1, 1) * 32767).astype('<i2')
    with wave.open(os.path.join(OUT, name + '.wav'), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(data.tobytes())
    print(f'{name}.wav  {len(x) / SR:.2f} s')

def rocket_launch():
    # Ignition crack, a low thump from the back-blast and a hiss that fades as the rocket leaves.
    t = timeline(1.5); n = len(t)
    crack = band(noise(n), lo=2000) * envelope(t, .0004, .012)
    thump = sweep(t, 45 + 70 * np.exp(-t / .05)) * envelope(t, .002, .15)
    bright = band(noise(n), lo=1500, hi=7000) * envelope(t, .015, .35)
    body = band(noise(n), lo=300, hi=2500) * envelope(t, .04, .8)
    rumble = band(noise(n), hi=200) * envelope(t, .02, .6)
    sparks = (rng.random(n) < 0.004 * np.exp(-t / .25)) * rng.uniform(-1, 1, n)
    sparks = band(sparks, lo=1000) * 3
    x = .8 * unit(crack) + .9 * unit(thump) + .45 * unit(bright) + .6 * unit(body) + .5 * unit(rumble) + .25 * unit(sparks)
    return finish(soft_clip(x, 1.3), fade_out=.08)

def mortar_launch():
    # The tube's "thoomp": a short low pop, a ring of the tube and a puff of air.
    t = timeline(1.0); n = len(t)
    thump = sweep(t, 48 + 60 * np.exp(-t / .06)) * envelope(t, .0015, .22)
    pop = band(noise(n), hi=1200) * envelope(t, .001, .05)
    ring = (np.sin(2 * np.pi * 170 * t) + .3 * np.sin(2 * np.pi * 340 * t)) * envelope(t, .001, .09)
    air = band(noise(n), lo=150, hi=900) * envelope(t, .01, .45)
    x = 1.0 * unit(thump) + .9 * unit(pop) + .35 * unit(ring) + .35 * unit(air)
    return finish(soft_clip(x, 1.5), fade_out=.1)

def shell_incoming():
    # A falling whistle that grows louder until the impact (the explosion sound follows it in the game).
    T = 1.9; t = timeline(T); n = len(t)
    freq = 1600 * (550 / 1600) ** (t / T) * (1 + .012 * np.sin(2 * np.pi * 7 * t))
    phase = 2 * np.pi * np.cumsum(freq) / SR
    tone = np.sin(phase) + .25 * np.sin(2 * phase)
    loud = .08 + .92 * (t / T) ** 1.6
    air = band(noise(n), lo=800, hi=4000) * (t / T) ** 2
    x = unit(tone) * loud + .25 * unit(air)
    return finish(x, fade_in=.05, fade_out=.025)

def bell(t, freq, start, decay, partials=((1, 1.0), (2, .35), (3, .12), (4.2, .06))):
    x = np.zeros_like(t)
    for ratio, amp in partials:
        x += amp * np.sin(2 * np.pi * freq * ratio * (t - start)) * envelope(t, .004, decay / ratio ** .5, start)
    return x

def brass(t, freq, start, decay, partials=8, cutoff=3000):
    x = np.zeros_like(t)
    for k in range(1, partials + 1):
        if freq * k > cutoff * 2:
            break
        x += (1 / k) / np.sqrt(1 + (freq * k / cutoff) ** 4) * np.sin(2 * np.pi * freq * k * (t - start))
    return x * envelope(t, .012, decay, start)

def capture_friendly():
    t = timeline(1.0)
    x = bell(t, 659.25, 0, .45) + bell(t, 987.77, .11, .5) + .25 * bell(t, 1318.5, .22, .4)
    return finish(x, fade_out=.1)

def capture_enemy():
    # Falling tritone with a hollow (odd partials) timbre.
    t = timeline(1.1); odd = ((1, 1.0), (3, .3), (5, .12))
    x = bell(t, 440.0, 0, .55, odd) + bell(t, 311.13, .14, .65, odd)
    return finish(band(x, hi=2500), fade_out=.12)

def victory():
    t = timeline(1.8)
    x = brass(t, 523.25, 0, .25) + brass(t, 659.25, .12, .25) + brass(t, 783.99, .24, .25)
    x += 1.2 * brass(t, 1046.5, .38, .9) + .5 * brass(t, 783.99, .38, .9) + .4 * brass(t, 659.25, .38, .9)
    return finish(x, fade_out=.25)

def defeat():
    t = timeline(2.0)
    x = brass(t, 440.0, 0, .35, cutoff=1800) + brass(t, 349.23, .18, .35, cutoff=1800) + brass(t, 293.66, .36, .35, cutoff=1800)
    x += brass(t, 220.0, .56, 1.0, cutoff=1400) + .6 * brass(t, 261.63, .56, 1.0, cutoff=1400) + .5 * brass(t, 329.63, .56, 1.0, cutoff=1400)
    return finish(x, fade_out=.3)

if __name__ == '__main__':
    write('SW_Synth_RocketLaunch', rocket_launch())
    write('SW_Synth_MortarLaunch', mortar_launch())
    write('SW_Synth_ShellIncoming', shell_incoming())
    write('SW_Synth_CaptureFriendly', capture_friendly())
    write('SW_Synth_CaptureEnemy', capture_enemy())
    write('SW_Synth_Victory', victory())
    write('SW_Synth_Defeat', defeat())
