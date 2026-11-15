#!/usr/bin/env python3
"""Synthesizes the placeholder SFX and music loops (public domain, made for Crypt Crew).

Replace with the Kenney / Tallbeard packs listed in TODO_ASSETS.md when available.
Usage: python3 tools/gen_audio.py assets/audio
Deterministic (fixed seeds), stdlib only.
"""
import math
import os
import random
import struct
import sys
import wave

SR = 22050


def write(path, samples):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    peak = max(1e-6, max(abs(s) for s in samples))
    gain = 0.89 / peak
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, s * gain)) * 32767)) for s in samples))


def env(t, a, d):
    """attack/decay envelope"""
    if t < a:
        return t / a
    return math.exp(-(t - a) / d)


def noise(rng):
    return rng.uniform(-1, 1)


class OnePole:
    def __init__(self, cutoff):
        self.set(cutoff)
        self.y = 0.0

    def set(self, cutoff):
        self.a = 1 - math.exp(-2 * math.pi * cutoff / SR)

    def __call__(self, x):
        self.y += self.a * (x - self.y)
        return self.y


def whoosh(dur, f0, f1, seed, amp=1.0):
    rng = random.Random(seed)
    lp, hp = OnePole(f0), OnePole(f0 * 0.3)
    out = []
    n = int(dur * SR)
    for i in range(n):
        t = i / SR
        k = i / n
        lp.set(f0 + (f1 - f0) * k)
        x = noise(rng)
        y = lp(x)
        y = y - hp(y)
        out.append(y * math.sin(math.pi * k) ** 1.5 * amp)
    return out


def thud(dur, freq, seed, crack=0.4):
    rng = random.Random(seed)
    lp = OnePole(2500)
    out = []
    for i in range(int(dur * SR)):
        t = i / SR
        f = freq * (1 + 2.5 * math.exp(-t * 40))
        body = math.sin(2 * math.pi * f * t) * env(t, 0.002, 0.08)
        c = lp(noise(rng)) * env(t, 0.001, 0.02) * crack * 3
        out.append(body + c)
    return out


def clang(dur, base, seed):
    rng = random.Random(seed)
    partials = [(1.0, 1.0), (2.76, 0.6), (5.4, 0.4), (8.93, 0.25)]
    out = []
    for i in range(int(dur * SR)):
        t = i / SR
        s = sum(a * math.sin(2 * math.pi * base * r * t) * math.exp(-t * (3 + r * 2)) for r, a in partials)
        s += noise(rng) * env(t, 0.001, 0.01) * 0.8
        out.append(s)
    return out


def tones(notes, seed=0, wav="tri"):
    """notes: list of (start, dur, freq, amp)"""
    end = max(s + d for s, d, _, _ in notes)
    out = [0.0] * int((end + 0.05) * SR)
    for s, d, f, a in notes:
        i0 = int(s * SR)
        for i in range(int(d * SR)):
            t = i / SR
            ph = (f * t) % 1.0
            if wav == "tri":
                v = 4 * abs(ph - 0.5) - 1
            elif wav == "sq":
                v = 1 if ph < 0.5 else -1
            else:
                v = math.sin(2 * math.pi * ph)
            out[i0 + i] += v * a * env(t, 0.005, d * 0.35)
    return out


def mix(*tracks):
    n = max(len(t) for t in tracks)
    return [sum(t[i] if i < len(t) else 0 for t in tracks) for i in range(n)]


def rattle(dur, seed):
    rng = random.Random(seed)
    out = [0.0] * int(dur * SR)
    for _ in range(26):
        t0 = rng.uniform(0, dur * 0.8)
        f = rng.uniform(900, 2600)
        i0 = int(t0 * SR)
        for i in range(int(0.04 * SR)):
            if i0 + i < len(out):
                t = i / SR
                out[i0 + i] += math.sin(2 * math.pi * f * t) * math.exp(-t * 120) * rng.uniform(0.3, 1)
    return out


def growl(dur, seed):
    rng = random.Random(seed)
    lp = OnePole(500)
    out = []
    for i in range(int(dur * SR)):
        t = i / SR
        f = 70 + 25 * math.sin(2 * math.pi * 7 * t) + 30 * math.exp(-t * 2)
        saw = 2 * ((f * t) % 1) - 1
        s = lp(saw * 0.7 + noise(rng) * 0.6)
        out.append(s * env(t, 0.15, dur * 0.4))
    return out


def fire(dur, seed):
    rng = random.Random(seed)
    lp = OnePole(900)
    return [lp(noise(rng)) * env(i / SR, 0.03, dur * 0.35) * (1 + 0.5 * math.sin(i / SR * 60)) for i in range(int(dur * SR))]


def music_explore(seconds=16.0):
    rng = random.Random(7)
    n = int(seconds * SR)
    out = [0.0] * n
    # Drone (D minor) with slow beating.
    for i in range(n):
        t = i / SR
        out[i] += 0.25 * math.sin(2 * math.pi * 73.42 * t) + 0.18 * math.sin(2 * math.pi * 110.0 * t + 0.3 * math.sin(t * 0.5))
        out[i] += 0.10 * math.sin(2 * math.pi * 146.83 * t * 1.002)
    # Sparse minor arpeggio (bell-ish), loops cleanly every 16 s.
    scale = [293.66, 349.23, 440.0, 523.25, 392.0, 349.23, 329.63, 293.66]
    for k in range(16):
        f = scale[k % len(scale)] * (0.5 if k % 4 == 3 else 1)
        i0 = int(k * SR)
        for i in range(int(1.6 * SR)):
            if i0 + i >= n:
                break
            t = i / SR
            out[i0 + i] += 0.22 * math.sin(2 * math.pi * f * t) * math.exp(-t * 2.2) + 0.06 * math.sin(2 * math.pi * f * 2.01 * t) * math.exp(-t * 4)
    # Wind noise.
    lp = OnePole(300)
    for i in range(n):
        out[i] += lp(noise(rng)) * 0.3 * (0.6 + 0.4 * math.sin(2 * math.pi * i / n))
    return out


def music_boss(seconds=12.0):
    rng = random.Random(9)
    n = int(seconds * SR)
    out = [0.0] * n
    bpm = 140
    beat = 60 / bpm
    lp = OnePole(400)
    bass = [73.42, 73.42, 87.31, 73.42, 65.41, 73.42, 98.0, 92.5]
    steps = int(seconds / (beat / 2))
    for k in range(steps):
        i0 = int(k * beat / 2 * SR)
        f = bass[k % len(bass)]
        for i in range(int(beat / 2 * SR)):
            if i0 + i >= n:
                break
            t = i / SR
            saw = 2 * ((f * t) % 1) - 1
            out[i0 + i] += lp(saw) * 0.5 * math.exp(-t * 3)
    for k in range(int(seconds / beat)):
        i0 = int(k * beat * SR)
        kick = k % 2 == 0
        for i in range(int(0.25 * SR)):
            if i0 + i >= n:
                break
            t = i / SR
            if kick:
                out[i0 + i] += math.sin(2 * math.pi * (55 + 120 * math.exp(-t * 30)) * t) * math.exp(-t * 12) * 0.9
            else:
                out[i0 + i] += noise(rng) * math.exp(-t * 25) * 0.45
    # Choir-ish pad.
    for i in range(n):
        t = i / SR
        out[i] += 0.07 * (math.sin(2 * math.pi * 293.66 * t) + math.sin(2 * math.pi * 349.23 * t) + math.sin(2 * math.pi * 440 * t * 1.003))
    return out


def main(root):
    sfx = os.path.join(root, "sfx")
    music = os.path.join(root, "music")
    W = lambda name, s: write(os.path.join(sfx, "placeholder_" + name + ".wav"), s)
    W("swing", whoosh(0.22, 900, 3500, 1))
    W("swing_heavy", whoosh(0.35, 400, 1800, 2))
    W("hit", thud(0.18, 120, 3))
    W("hit_heavy", thud(0.3, 80, 4, crack=0.8))
    W("crit", mix(thud(0.25, 140, 5, crack=0.9), clang(0.3, 1200, 6)))
    W("block", clang(0.35, 520, 7))
    W("dodge", whoosh(0.28, 500, 1400, 8, amp=0.7))
    W("gold", tones([(0, 0.12, 1318.5, 0.6), (0.07, 0.25, 1975.5, 0.5)], wav="sin"))
    W("revive", tones([(0, 0.2, 523.25, 0.5), (0.12, 0.2, 659.25, 0.5), (0.24, 0.2, 783.99, 0.5), (0.36, 0.5, 1046.5, 0.6)], wav="tri"))
    W("door", mix(thud(0.5, 55, 9, crack=0.2), clang(0.5, 180, 10)))
    W("boss_roar", growl(1.6, 11))
    W("enemy_die", rattle(0.6, 12))
    W("player_hurt", mix(thud(0.2, 160, 13), whoosh(0.15, 1500, 600, 14, amp=0.5)))
    W("downed", tones([(0, 0.3, 392, 0.5), (0.2, 0.3, 311.13, 0.5), (0.4, 0.6, 261.63, 0.5)], wav="tri"))
    W("fire", fire(0.8, 15))
    W("magic", mix(tones([(0, 0.25, 880, 0.3), (0.03, 0.25, 1320, 0.2)], wav="sin"), whoosh(0.25, 2000, 5000, 16, amp=0.4)))
    W("chest", mix(clang(0.4, 330, 17), tones([(0.15, 0.3, 1046.5, 0.4), (0.25, 0.4, 1567.98, 0.4)], wav="sin")))
    W("relic", tones([(0, 0.15, 783.99, 0.4), (0.1, 0.15, 987.77, 0.4), (0.2, 0.4, 1174.66, 0.5)], wav="sin"))
    W("ui_move", tones([(0, 0.05, 1200, 0.4)], wav="sq"))
    W("ui_confirm", tones([(0, 0.06, 900, 0.4), (0.05, 0.1, 1350, 0.4)], wav="sq"))
    W("ui_back", tones([(0, 0.06, 700, 0.4), (0.05, 0.1, 500, 0.4)], wav="sq"))
    W("slam", mix(thud(0.6, 45, 18, crack=1.0), rattle(0.5, 19)))
    W("telegraph", tones([(0, 0.18, 220, 0.4)], wav="sq"))
    W("bolt", whoosh(0.3, 3000, 900, 20, amp=0.6))
    write(os.path.join(music, "placeholder_explore.wav"), music_explore())
    write(os.path.join(music, "placeholder_boss.wav"), music_boss())


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "assets/audio")
