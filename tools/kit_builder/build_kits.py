"""Slice the uploaded stems into multi-mic drum samples for RiddimForge.

Kit A "Studio Kit"  : multi-mic session (close mics + modelled bleed-free overheads + generated room)
Kit B "Kit 2"       : single-output drum tracks from 'Reggae Instrumental 2'
Writes FLAC files + kits.json manifest into RiddimForge/Resources/Kits.
"""
import json, os, subprocess, numpy as np, scipy.signal as ss, scipy.io.wavfile as wavio
from load import load, mono, onsets
from load2 import load2

SR = 48000
OUT = "/home/claude/RiddimForge/Resources/Kits"
PRE_ROLL = 48          # samples kept before each onset
H_PRE = 1024           # non-causal offset used by the overhead filters

# mic buses (must match C++ order)
BUSES = ["Kick In", "Kick Out", "Snare Top", "Snare Bottom", "Hi-Hat", "Tom Hi", "Tom Mid", "Tom Floor",
         "Rototoms", "Timbales", "Percussion", "Cymbals", "Overheads", "Room"]
B = {n: i for i, n in enumerate(BUSES)}

rng = np.random.default_rng(7)

# ---------------------------------------------------------------- helpers
def fade(x, fin=16, fout=480):
    x = x.copy(); n = len(x)
    fin = min(fin, n // 4); fout = min(fout, n // 2)
    if fin: x[:fin] *= np.linspace(0, 1, fin)[:, None] if x.ndim == 2 else np.linspace(0, 1, fin)
    if fout:
        w = np.cos(np.linspace(0, np.pi / 2, fout)) ** 2
        x[-fout:] *= w[:, None] if x.ndim == 2 else w
    return x

def trim_tail(chans, floor_db=-66):
    """cut all channels once every channel has decayed below floor (relative to its own peak)"""
    end = 0
    for c in chans:
        a = np.abs(c); p = a.max() + 1e-12
        idx = np.where(a > p * 10 ** (floor_db / 20))[0]
        if len(idx): end = max(end, idx[-1])
    return [c[:end + 1 + 480] for c in chans]

def make_room_ir(seed=3, rt60=0.65, length=1.2):
    r = np.random.default_rng(seed); n = int(length * SR)
    t = np.arange(n) / SR
    env = 10 ** (-3 * t / rt60)
    ir = np.zeros((n, 2))
    for ch in range(2):
        noise = r.standard_normal(n) * env
        sos = ss.butter(2, 6500, 'low', fs=SR, output='sos'); noise = ss.sosfilt(sos, noise)
        sos = ss.butter(2, 120, 'high', fs=SR, output='sos'); noise = ss.sosfilt(sos, noise)
        pre = int(0.011 * SR); noise = np.concatenate([np.zeros(pre), noise[:-pre]])
        for k in range(8):  # early reflections
            d = int(r.uniform(0.006, 0.035) * SR); ir[d, ch] += r.uniform(0.25, 0.6) * (-1) ** k
        ir[:, ch] += noise * 0.08
    return ir / np.sqrt((ir ** 2).sum() / 2)

ROOM_IR = make_room_ir()

def room_from(stereo, gain=0.55):
    m = stereo.mean(axis=1)
    out = np.stack([ss.fftconvolve(m, ROOM_IR[:, c]) for c in range(2)], axis=1)
    return out * gain

def write_flac(path, chans):
    n = max(len(c) for c in chans)
    data = np.zeros((n, len(chans)), np.float32)
    for i, c in enumerate(chans): data[:len(c), i] = c
    peak = np.abs(data).max()
    if peak > 0.98: data *= 0.98 / peak
    tmp = path + ".wav"
    wavio.write(tmp, SR, (np.clip(data, -1, 1) * 8388607).astype(np.int32) << 8)
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", tmp, "-c:a", "flac", "-compression_level", "8",
                    "-sample_fmt", "s32", "-bits_per_raw_sample", "24", path], check=True)
    os.remove(tmp)
    return n

def pick_spread(hits, n):
    """choose n hits spread across the level range (round robins + velocity layers)"""
    if len(hits) <= n: return hits
    hits = sorted(hits, key=lambda h: h[1])
    idx = np.unique(np.round(np.linspace(0, len(hits) - 1, n)).astype(int))
    return [hits[i] for i in idx]

def level_db(x, s, dur=0.03):
    return float(20 * np.log10(np.abs(x[s:s + int(dur * SR)]).max() + 1e-9))

def smooth_bursts(seg, keep_attack=0.06, med=41):
    """remove short transients (hat leftovers) from a decaying cymbal tail via temporal median in the STFT"""
    import scipy.ndimage as nd
    out = np.zeros_like(seg)
    for c in range(seg.shape[1]):
        f, t, Z = ss.stft(seg[:, c], fs=SR, nperseg=1024, noverlap=768)
        M = np.abs(Z); Ms = nd.median_filter(M, size=(1, med), mode='nearest')
        # also follow the decay: running minimum of a smoothed envelope never lets the tail grow back
        mask = np.minimum(1.0, Ms / (M + 1e-12))
        ka = int(keep_attack * SR / 256); mask[:, :ka] = 1.0
        _, y = ss.istft(Z * mask, fs=SR, nperseg=1024, noverlap=768)
        out[:, c] = y[:len(seg)]
    return out

# ---------------------------------------------------------------- overhead model (Kit A)
from ohmodel2 import stft_stats, solve
OH_SRC = ["KICK 2", "SN T", "HI HAT", "TOM 1", "TOM 2", "TOM 3", "ROTO 1", "ROTO 2", "TIMBALE", "TAMBOURINE"]

def get_h():
    hs = {}
    for oh in ["OH L", "OH R"]:
        f = f"h_{oh.replace(' ', '_')}.npy"
        if os.path.exists(f):
            h = np.load(f)
        else:
            Rcc, Rcy = stft_stats(OH_SRC, mono(load(oh))); h = solve(Rcc, Rcy, 0.05); np.save(f, h)
        hs[oh] = h
    return hs

def oh_render(close, src, H, scale=1.0, maxlen=None):
    i = OH_SRC.index(src)
    outs = []
    for oh in ["OH L", "OH R"]:
        y = ss.fftconvolve(close, H[oh][i])[H_PRE:]
        outs.append(y * scale)
    n = min(len(o) for o in outs)
    if maxlen: n = min(n, maxlen)
    return np.stack([o[:n] for o in outs], axis=1)

def borrowed_scale(mic, borrow, hits, H):
    """level match for drums whose overhead filter is borrowed from a neighbour"""
    ratios = []
    ohm = (mono(load("OH L")) + mono(load("OH R"))) * 0.5
    x = mono(load(mic))
    for s, _ in hits:
        seg = x[s - PRE_ROLL:s + int(0.04 * SR)]
        pred = oh_render(seg, borrow, H).mean(axis=1)
        a = np.abs(ohm[s - PRE_ROLL:s + int(0.04 * SR)]).max(); b = np.abs(pred).max() + 1e-9
        ratios.append(a / b)
    return float(np.clip(np.median(ratios), 0.3, 3.0)) if ratios else 1.0

# ---------------------------------------------------------------- Kit A
def kit_a():
    H = get_h()
    kit = {"name": "Studio Kit", "outputGainDb": 5.0, "description": "Multi-mic kit from your session: close mics, bleed-free overheads, generated room",
           "articulations": []}

    def same_mic_next(mic, s, hits_all):
        nxt = [h for h, _ in hits_all if h > s + int(0.02 * SR)]
        return nxt[0] if nxt else s + SR * 4

    def add_artic(key, name, notes, mic_a, bus_a, hits, maxlen, mic_b=None, bus_b=None, oh_src=None, oh_scale=1.0,
                  choke=0, mic_hits=None, keep=8, tune=0.0):
        hits = pick_spread(hits, keep)
        samples = []
        for j, (s, lv) in enumerate(sorted(hits, key=lambda h: h[0])):
            start = s - PRE_ROLL
            nxt = same_mic_next(mic_a, s, mic_hits) if mic_hits else s + maxlen
            end = min(start + maxlen, nxt - 64)
            a = mono(load(mic_a))[start:end].copy()
            chans, routing = [fade(a)], [[bus_a, "mono"]]
            if mic_b:
                # close mic B can catch other drums (e.g. kick in snare bottom): cut it at its own next hit too
                bh = [h for h, _ in onsets(load(mic_b), thresh_db=-50) if h > s + int(0.03 * SR)]
                end_b = min(end, (bh[0] - 64) if bh else end)
                bb = np.zeros(end - start, np.float32); bb[:end_b - start] = fade(mono(load(mic_b))[start:end_b].copy())
                chans.append(bb); routing.append([bus_b, "mono"])
            if oh_src:
                oh = oh_render(a, oh_src, H, oh_scale, maxlen=maxlen)
                oh = fade(oh, fout=2400)
                rm = fade(room_from(oh)[:maxlen], fout=4800)
                chans += [oh[:, 0], oh[:, 1], rm[:, 0], rm[:, 1]]
                routing += [[B["Overheads"], "L"], [B["Overheads"], "R"], [B["Room"], "L"], [B["Room"], "R"]]
            chans = trim_tail(chans)
            fn = f"studio_{key}_{j + 1:02d}.flac"
            write_flac(f"{OUT}/{fn}", chans)
            samples.append({"file": fn, "level": round(level_db(mono(load(mic_a)), s), 2)})
        kit["articulations"].append({"key": key, "name": name, "notes": notes, "choke": choke, "tune": tune,
                                     "routing": routing, "samples": samples})
        print(f"  {name:22s} {len(samples)} samples")

    sr = SR
    # --- kick
    k2 = onsets(load("KICK 2"), thresh_db=-40)
    add_artic("kick", "Kick", [36, 35], "KICK 2", B["Kick In"], k2, int(0.9 * sr), "KICK 1", B["Kick Out"], "KICK 2",
              mic_hits=k2, keep=10)
    # --- snare classes
    st = onsets(load("SN T"), thresh_db=-40)
    sx = mono(load("SN T"))
    def cent(s):
        seg = sx[s:s + 3840] * np.hanning(3840); S = np.abs(np.fft.rfft(seg)); f = np.fft.rfftfreq(3840, 1 / sr)
        return (S * f).sum() / S.sum()
    crack = [(s, l) for s, l in st if l > -12 and cent(s) >= 3000]
    body = [(s, l) for s, l in st if l > -12 and cent(s) < 3000]
    ghost = [(s, l) for s, l in st if l <= -14 and cent(s) < 6000]
    add_artic("snare", "Snare (crack)", [38], "SN T", B["Snare Top"], crack, int(0.9 * sr), "SN B", B["Snare Bottom"],
              "SN T", mic_hits=st, keep=10)
    add_artic("snare_fill", "Snare (fill / center)", [40], "SN T", B["Snare Top"], body, int(0.9 * sr), "SN B",
              B["Snare Bottom"], "SN T", mic_hits=st, keep=6)
    add_artic("snare_ghost", "Snare (ghost)", [], "SN T", B["Snare Top"], ghost, int(0.6 * sr), "SN B",
              B["Snare Bottom"], "SN T", mic_hits=st, keep=4)
    # --- hats: exclude hits that coincide with a snare hit (snare bleed in hat mic)
    hh = onsets(load("HI HAT"), thresh_db=-40)
    snare_times = np.array([s for s, _ in st])
    hx = mono(load("HI HAT"))
    def hat_decay(s):
        seg = np.abs(hx[s:s + sr]); hop = 256; n = len(seg) // hop
        e = 20 * np.log10(seg[:n * hop].reshape(n, hop).max(1) + 1e-9); p = e[:6].max()
        i = np.where(e < p - 25)[0]; return i[0] * hop / sr if len(i) else 1.0
    clean = [(s, l) for s, l in hh if (np.abs(snare_times - s).min() > 0.03 * sr)]
    closed = [(s, l) for s, l in clean if hat_decay(s) < 0.15]
    opened = [(s, l) for s, l in clean if hat_decay(s) >= 0.3]
    add_artic("hat_closed", "Hi-Hat Closed", [42, 44], "HI HAT", B["Hi-Hat"], closed, int(0.45 * sr),
              oh_src="HI HAT", choke=1, mic_hits=hh, keep=10)
    add_artic("hat_open", "Hi-Hat Open", [46], "HI HAT", B["Hi-Hat"], opened, int(1.2 * sr), oh_src="HI HAT",
              choke=1, mic_hits=hh, keep=6)
    # --- toms (by pitch: TOM 3 = 110 Hz high, TOM 1 = 89 Hz mid, TOM 2 = 67 Hz floor)
    t1 = [(s, l) for s, l in onsets(load("TOM 1"), thresh_db=-40) if l > -20]
    t2 = [(s, l) for s, l in onsets(load("TOM 2"), thresh_db=-40) if l > -20]
    t3 = onsets(load("TOM 3"), thresh_db=-40)
    sc3 = borrowed_scale("TOM 3", "TOM 1", t3, H)
    add_artic("tom_hi", "Tom Hi", [48, 50], "TOM 3", B["Tom Hi"], t3, int(1.2 * sr), oh_src="TOM 1", oh_scale=sc3,
              mic_hits=t3, keep=4)
    add_artic("tom_mid", "Tom Mid", [45, 47], "TOM 1", B["Tom Mid"], t1, int(1.2 * sr), oh_src="TOM 1", mic_hits=t1, keep=6)
    add_artic("tom_floor", "Tom Floor", [41, 43], "TOM 2", B["Tom Floor"], t2, int(1.4 * sr), oh_src="TOM 2",
              mic_hits=t2, keep=6)
    # --- rototoms / timbales / tambourine (sparse: borrowed overhead filters, level matched)
    r1 = onsets(load("ROTO 1"), thresh_db=-40)
    r2 = [(s, l) for s, l in onsets(load("ROTO 2"), thresh_db=-40) if l > -35]
    add_artic("roto_hi", "Rototom Hi", [93], "ROTO 1", B["Rototoms"], r1, int(1.3 * sr), oh_src="SN T",
              oh_scale=borrowed_scale("ROTO 1", "SN T", r1, H), mic_hits=r1, keep=3)
    add_artic("roto_lo", "Rototom Lo", [94], "ROTO 2", B["Rototoms"], r2, int(1.5 * sr), oh_src="SN T",
              oh_scale=borrowed_scale("ROTO 2", "SN T", r2, H), mic_hits=r2, keep=5)
    ti = onsets(load("TIMBALE"), thresh_db=-40)
    tx = mono(load("TIMBALE"))
    def tf0(s):
        seg = tx[s + 240:s + 9600] * np.hanning(9360); S = np.abs(np.fft.rfft(seg, 1 << 16))
        f = np.fft.rfftfreq(1 << 16, 1 / sr); m = (f > 200) & (f < 1200); return f[m][np.argmax(S[m])]
    ti_hi = [(s, l) for s, l in ti if tf0(s) > 500]; ti_lo = [(s, l) for s, l in ti if tf0(s) <= 500]
    tsc = borrowed_scale("TIMBALE", "SN T", ti, H)
    add_artic("timbale_hi", "Timbale Hi", [65, 79], "TIMBALE", B["Timbales"], ti_hi, int(0.9 * sr), oh_src="SN T",
              oh_scale=tsc, mic_hits=ti, keep=2)
    add_artic("timbale_lo", "Timbale Lo", [66], "TIMBALE", B["Timbales"], ti_lo, int(0.9 * sr), oh_src="SN T",
              oh_scale=tsc, mic_hits=ti, keep=2)
    tb = onsets(load("TAMBOURINE"), thresh_db=-60, rel_db=-30)
    tb = sorted(tb, key=lambda h: -h[1])[:24]
    add_artic("tamb", "Tambourine", [54], "TAMBOURINE", B["Percussion"], tb, int(0.5 * sr), oh_src="HI HAT",
              oh_scale=borrowed_scale("TAMBOURINE", "HI HAT", tb, H) * 0.5,
              mic_hits=onsets(load("TAMBOURINE"), thresh_db=-60, rel_db=-30), keep=8)
    # --- crashes recovered from the overhead residual (overhead + room only), cut before leftover leaks
    rl, rr = np.load("res_OH_L.npy"), np.load("res_OH_R.npy")
    def crash_artic(key, name, notes, items):
        samples = []
        for j, (t, cut) in enumerate(items):
            s = int(t * sr) - 480
            seg = np.stack([rl[s:s + int(2.0 * sr)], rr[s:s + int(2.0 * sr)]], axis=1)
            hf = np.abs(ss.sosfilt(ss.butter(4, 4000, 'high', fs=sr, output='sos'), seg.mean(1)))
            on = max(0, int(np.argmax(hf > 0.3 * hf.max())) - PRE_ROLL)
            seg = smooth_bursts(seg[on:on + int(cut * sr)])
            seg = fade(seg, fout=int(min(0.45, cut * 0.45) * sr))
            rm = room_from(seg)[:int((cut + 0.5) * sr)]
            rm = fade(rm, fout=int(0.5 * sr))
            n = len(rm); segp = np.zeros((n, 2), np.float32); segp[:len(seg)] = seg
            fn = f"studio_{key}_{j + 1:02d}.flac"
            write_flac(f"{OUT}/{fn}", [segp[:, 0], segp[:, 1], rm[:, 0], rm[:, 1]])
            samples.append({"file": fn, "level": round(float(20 * np.log10(np.abs(seg).max() + 1e-9)), 2)})
        kit["articulations"].append({"key": key, "name": name, "notes": notes, "choke": 0, "tune": 0.0,
                                     "routing": [[B["Overheads"], "L"], [B["Overheads"], "R"],
                                                 [B["Room"], "L"], [B["Room"], "R"]], "samples": samples})
        print(f"  {name:22s} {len(samples)} samples")
    crash_artic("crash", "Crash (from overheads)", [49, 57], [(14.66, 1.30), (8.87, 0.88)])
    crash_artic("crash_choke", "Crash Choke", [55], [(12.42, 0.40)])
    return kit

# ---------------------------------------------------------------- Kit B
def kit_b():
    kit = {"name": "Kit 2", "outputGainDb": 6.0, "description": "Punchy one-drop kit from 'Reggae Instrumental 2' with generated room",
           "articulations": []}
    sr = SR

    def add(key, name, notes, track, bus, hits, maxlen, keep, choke=0, filt=None):
        x = load2(track); allh = onsets(x, thresh_db=-50)
        hits = pick_spread(hits, keep); samples = []
        for j, (s, lv) in enumerate(sorted(hits, key=lambda h: h[0])):
            nxt = [h for h, _ in allh if h > s + int(0.02 * sr)]
            end = min(s - PRE_ROLL + maxlen, (nxt[0] - 64) if nxt else s + maxlen)
            seg = fade(x[s - PRE_ROLL:end].copy())
            rm = fade(room_from(seg, gain=0.4)[:maxlen], fout=4800)
            chans = trim_tail([seg[:, 0], seg[:, 1], rm[:, 0], rm[:, 1]])
            fn = f"kit2_{key}_{j + 1:02d}.flac"
            write_flac(f"{OUT}/{fn}", chans)
            samples.append({"file": fn, "level": round(level_db(mono(x), s), 2)})
        kit["articulations"].append({"key": key, "name": name, "notes": notes, "choke": choke, "tune": 0.0,
                                     "routing": [[bus, "L"], [bus, "R"], [B["Room"], "L"], [B["Room"], "R"]],
                                     "samples": samples})
        print(f"  {name:22s} {len(samples)} samples")

    k = onsets(load2("Insert 1"), thresh_db=-45)
    add("kick", "Kick", [36, 35], "Insert 1", B["Kick In"], k, int(0.9 * sr), 8)
    sn = onsets(load2("Insert 2"), thresh_db=-45)
    m2 = mono(load2("Insert 2"))
    def body_hz(s):
        seg = m2[s:s + 4800] * np.hanning(4800); S = np.abs(np.fft.rfft(seg, 1 << 15)); f = np.fft.rfftfreq(1 << 15, 1 / sr)
        msk = (f > 150) & (f < 3000); return f[msk][np.argmax(S[msk])]
    xs = [(s, l) for s, l in sn if body_hz(s) > 900]
    full = [(s, l) for s, l in sn if body_hz(s) <= 900 and l > -20]
    add("cross", "Cross Stick", [37], "Insert 2", B["Snare Top"], xs, int(0.6 * sr), 8)
    add("snare", "Snare", [38, 40], "Insert 2", B["Snare Top"], full, int(0.8 * sr), 4)
    hh = onsets(load2("Insert 4"), thresh_db=-45)
    hx = mono(load2("Insert 4"))
    def dec(s):
        seg = np.abs(hx[s:s + sr]); hop = 256; n = len(seg) // hop
        e = 20 * np.log10(seg[:n * hop].reshape(n, hop).max(1) + 1e-9); p = e[:6].max()
        i = np.where(e < p - 25)[0]; return i[0] * hop / sr if len(i) else 1.0
    add("hat_closed", "Hi-Hat Closed", [42, 44], "Insert 4", B["Hi-Hat"], [h for h in hh if dec(h[0]) < 0.15],
        int(0.4 * sr), 10, choke=1)
    op = [h for h in hh if dec(h[0]) >= 0.25]
    if op: add("hat_open", "Hi-Hat Open", [46], "Insert 4", B["Hi-Hat"], op, int(1.0 * sr), 4, choke=1)
    tm = onsets(load2("Insert 5"), thresh_db=-45)
    m5 = mono(load2("Insert 5"))
    def tp(s):
        seg = m5[s:s + 9600] * np.hanning(9600); S = np.abs(np.fft.rfft(seg, 1 << 16)); f = np.fft.rfftfreq(1 << 16, 1 / sr)
        msk = (f > 60) & (f < 400); return f[msk][np.argmax(S[msk])]
    add("tom_hi", "Tom Hi", [48, 50, 45, 47], "Insert 5", B["Tom Hi"], [h for h in tm if tp(h[0]) > 125], int(1.0 * sr), 2)
    add("tom_floor", "Tom Floor", [41, 43], "Insert 5", B["Tom Floor"], [h for h in tm if tp(h[0]) <= 125], int(1.2 * sr), 2)
    add("crash", "Crash", [49, 57], "Insert 7", B["Cymbals"], onsets(load2("Insert 7"), thresh_db=-45), int(2.5 * sr), 4)
    add("shaker", "Shaker", [82, 70], "Insert 10", B["Percussion"],
        sorted(onsets(load2("Insert 10"), thresh_db=-50), key=lambda h: -h[1])[:16], int(0.35 * sr), 6)
    add("block", "Jam Block", [76, 77, 75], "Insert 12", B["Percussion"],
        sorted(onsets(load2("Insert 12"), thresh_db=-50), key=lambda h: -h[1])[:16], int(0.3 * sr), 6)
    add("tamb", "Tambourine", [54], "Insert 14", B["Percussion"],
        sorted(onsets(load2("Insert 14"), thresh_db=-50), key=lambda h: -h[1])[:24], int(0.4 * sr), 8)
    return kit

if __name__ == "__main__":
    import shutil
    shutil.rmtree(OUT, ignore_errors=True); os.makedirs(OUT)
    print("Kit A (Studio Kit):"); a = kit_a()
    print("Kit B (Kit 2):"); b = kit_b()
    with open(f"{OUT}/kits.json", "w") as f:
        json.dump({"buses": BUSES, "sampleRate": SR, "kits": [a, b]}, f, indent=1)
    tot = sum(os.path.getsize(os.path.join(dp, fn)) for dp, _, fns in os.walk(OUT) for fn in fns)
    print("total size %.1f MB" % (tot / 1e6))
