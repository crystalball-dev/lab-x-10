"""Automated offline checks for LAB X-10.

Runs the LabX10Render harness through a battery of renders and verifies them with analyze.py.
Usage:  python Tests/run_tests.py [--library <stalker sounds folder>] [--specimens <STALKER SPECIMENS folder>]
                                  [--only 1,2,5] [--skip-pluginval] [--fuzz-seconds 300]
Without --library or --specimens the harness finds the folders itself, as the plugin does.

Writes WAVs to Tests/out (ignored by git) and a JSON report to build/test-report.json.
Test numbers follow the plan in LAB_X10_HANDOFF.md section 4.
"""

import argparse
import json
import math
import pathlib
import re
import shutil
import subprocess
import sys

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parent.parent
HARNESS = ROOT / "build" / "LabX10Render_artefacts" / "Release" / "LabX10Render.exe"
BUNDLE = ROOT / "build" / "LabX10_artefacts" / "Release" / "VST3" / "LAB X-10.vst3"
OUT = ROOT / "Tests" / "out"
sys.path.insert(0, str(ROOT / "Tests"))
import analyze as an  # noqa: E402

# A neutral patch: one pure sine, no drift, no effects, no regulation.
CLEAN = ["gen_a_wave=3", "gen_a_shape=0", "gen_a_level=1", "uni_voices=1", "gen_b_level=0", "gen_b_fm=0",
         "gen_b_ring=0", "sub_level=0", "spark_level=0", "noise_level=0", "specimen_level=0",
         "filter_model=2", "filter_cutoff=20000", "filter_res=0", "filter_drive=0", "filter_env=0",
         "filter_keytrack=0", "accent_amount=0", "env1_attack=0.001", "env1_sustain=1", "env1_release=0.05",
         "emit_mix=0", "emit_drive=0", "res_level=0", "lfo_depth=0", "cool_rate=0", "reg_amount=0", "weight=0",
         "scorch=0", "master_volume=-6", "voice_mode=1", "glide=0", "drop_amount=0", "width=1"]

STAT = re.compile(r'(\w+)=("([^"]*)"|\S+)')
E2 = 82.4069
SPECIMENS = None   # STALKER SPECIMENS library folder from --specimens


def render(name, extra, library=None, timeout=600):
    OUT.mkdir(parents=True, exist_ok=True)
    wav = OUT / f"{name}.wav"
    cmd = [str(HARNESS), "--out", str(wav)] + extra
    if library:
        cmd += ["--library", library]
    if SPECIMENS:
        cmd += ["--specimens", SPECIMENS]
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    stats = {}
    for line in proc.stdout.splitlines():
        for key, value, quoted in STAT.findall(line):
            stats[key] = quoted if value.startswith('"') else value
    stats["exit"] = proc.returncode
    if proc.returncode != 0:
        stats["stderr"] = (proc.stderr.strip() or proc.stdout.strip())[-400:]
    return wav, stats


def sets(values):
    out = []
    for v in values:
        out += ["--set", v]
    return out


def main():
    global SPECIMENS
    parser = argparse.ArgumentParser()
    parser.add_argument("--library")
    parser.add_argument("--specimens")
    parser.add_argument("--only", help="comma-separated test numbers")
    parser.add_argument("--skip-pluginval", action="store_true")
    parser.add_argument("--fuzz-seconds", type=float, default=300.0)
    args = parser.parse_args()
    SPECIMENS = args.specimens

    if not HARNESS.exists():
        print(f"harness not built: {HARNESS}")
        return 2

    only = {int(x) for x in args.only.split(",")} if args.only else None
    lib = args.library
    presets = [line.split("\t")[1] for line in
               subprocess.run([str(HARNESS), "--list"], capture_output=True, text=True).stdout.splitlines() if "\t" in line]
    report = {"tests": []}

    def want(n):
        return only is None or n in only

    def record(number, name, ok, detail):
        report["tests"].append({"number": number, "name": name, "pass": bool(ok), **detail})
        print(f"{'PASS' if ok else 'FAIL'}  {number:2d} {name}  {json.dumps(detail)[:320]}")

    def load_mono(wav):
        data, sr = an.load(str(wav))
        return an.mono(data), data, sr

    # 1 + 21. Pitch accuracy: A2 must render at 110 Hz within 1 cent, at 44.1, 48 and 96 kHz.
    # 2 + 21. Sub purity on the init patch: THD below 1 %, more than 97 % of the energy below 250 Hz.
    for sr in (48000, 44100, 96000):
        number = 1 if sr == 48000 else 21
        if want(number):
            wav, st = render(f"pitch_a2_{sr}", ["--sr", str(sr), "--notes", "45", "--seconds", "3", "--hold", "2.5"]
                             + sets(CLEAN + ["lock=2"]), lib)
            m, _, fs = load_mono(wav)
            f0 = an.dominant_frequency(an.segment(m, fs, 0.2, 2.4), fs)
            cents = 1200 * math.log2(f0 / 110.0)
            record(number, f"pitch A2 = 110 Hz at {sr}", abs(cents) <= 1.0 and st["exit"] == 0,
                   {"f0": round(f0, 4), "cents": round(cents, 3)})
        number = 2 if sr == 48000 else 21
        if want(number):
            wav, st = render(f"sub_purity_{sr}", ["--sr", str(sr), "--preset", "Emitter Disabled", "--notes", "36",
                                                  "--seconds", "3", "--hold", "2.5"], lib)
            m, _, fs = load_mono(wav)
            seg = an.segment(m, fs, 0.5, 2.4)
            f_sub = 440.0 * 2 ** ((24 - 69) / 12)
            distortion = an.thd(seg, fs, f_sub)
            below = an.band_fraction(seg, fs, 0, 250)
            record(number, f"sub purity at {sr}: THD < 1 %, > 97 % below 250 Hz",
                   distortion < 0.01 and below > 0.97 and st["exit"] == 0,
                   {"thd_pct": round(100 * distortion, 3), "below_250_pct": round(100 * below, 2)})

    # 3. Phase lock: with lock All two hits of the same note are identical; with lock Off they differ.
    if want(3):
        patch = CLEAN + ["gen_a_wave=0", "gen_a_level=0.8", "sub_level=0.7", "spark_level=0.4", "noise_level=0.1",
                         "filter_model=1", "filter_cutoff=1200", "filter_res=0.3", "env1_release=0.05"]
        diffs = {}
        for lock in (2, 0):
            wav, st = render(f"lock_{lock}", ["--events", "36@0-0.6,36@1-1.6", "--seconds", "2"]
                             + sets(patch + [f"lock={lock}"]), lib)
            _, data, fs = load_mono(wav)
            a = data[:int(0.8 * fs)]
            b = data[int(1.0 * fs):int(1.8 * fs)]
            diffs[lock] = float(np.abs(a - b).max())
        record(3, "phase lock All repeats hits exactly, Off does not", diffs[2] < 1e-6 and diffs[0] > 1e-3,
               {"max_diff_lock_all": diffs[2], "max_diff_lock_off": round(diffs[0], 4)})

    # 4. Unison: side signal only above the split, and a detuned spread around 110 Hz.
    if want(4):
        wav, st = render("unison", ["--notes", "33", "--seconds", "4", "--hold", "4"]
                         + sets(CLEAN + ["gen_a_wave=0", "uni_voices=7", "uni_detune=30", "uni_spread=1",
                                         "emit_split=200", "sub_level=0.5", "master_volume=-12"]), lib)
        _, data, fs = load_mono(wav)
        seg = data[int(0.3 * fs):int(3.9 * fs)]
        side = 0.5 * (seg[:, 0] - seg[:, 1])
        mid = 0.5 * (seg[:, 0] + seg[:, 1])
        side_low = an.band_energy(side, fs, 20, 100)
        side_high = an.band_energy(side, fs, 200, 8000)
        mid_high = an.band_energy(mid, fs, 200, 8000)
        ratio_db = 10 * math.log10(side_low / (side_high + 1e-30) + 1e-30)
        presence_db = 10 * math.log10(side_high / (mid_high + 1e-30) + 1e-30)
        spread = an.peak_spread(mid, fs, 110.0, 6.0)
        record(4, "unison: side only above split, >= 2 Hz spread at 110 Hz",
               ratio_db < -30 and presence_db > -20 and spread >= 2.0,
               {"side_below_vs_above_db": round(ratio_db, 1), "side_vs_mid_above_db": round(presence_db, 1),
                "spread_hz": round(spread, 2)})

    # 5. Filter slopes and resonance from white noise.
    if want(5):
        noise = CLEAN + ["gen_a_level=0", "noise_level=0.5", "spark_colour=1", "master_volume=-12", "filter_cutoff=500"]
        dry_wav, _ = render("filter_dry", ["--notes", "48", "--seconds", "4", "--hold", "4"]
                            + sets(noise + ["filter_model=2", "filter_cutoff=20000"]), lib)
        dry = an.mono(an.load(str(dry_wav))[0])[4800:]
        limits = {0: ("diode", 15, 24), 1: ("ladder", 20, 28), 2: ("svf", 10, 14)}
        for model, (label, lo, hi) in limits.items():
            wav, st = render(f"filter_{label}", ["--notes", "48", "--seconds", "4", "--hold", "4"]
                             + sets(noise + [f"filter_model={model}", "filter_res=0"]), lib)
            m = an.mono(an.load(str(wav))[0])[4800:]
            slope = -an.slope_db_per_octave(m, dry, 48000, 1000, 4000)   # 2x to 8x the cutoff
            wav, st2 = render(f"filter_{label}_res", ["--notes", "48", "--seconds", "4", "--hold", "4"]
                              + sets(noise + [f"filter_model={model}", "filter_res=0.85"]), lib)
            m2 = an.mono(an.load(str(wav))[0])[4800:]
            peak = an.response_peak(m2, dry, 48000, 150, 2000)
            wav, st3 = render(f"filter_{label}_full", ["--notes", "48", "--seconds", "4", "--hold", "4"]
                              + sets(noise + [f"filter_model={model}", "filter_res=1"]), lib)
            full = an.analyse(str(wav))
            ok = (lo <= slope <= hi and abs(peak / 500.0 - 1.0) <= 0.10 and full["non_finite"] == 0
                  and full["peak_db"] <= 0.0 and st["exit"] == 0)
            record(5, f"filter {label}: slope {lo}-{hi} dB/oct, peak within 10 %, finite at full res", ok,
                   {"slope_db_oct": round(slope, 1), "res_peak_hz": round(peak, 1), "full_res_peak_db": full["peak_db"]})

    # 6. Accent: velocity 127 against 90 on an acid patch.
    if want(6):
        acid = CLEAN + ["gen_a_wave=0", "gen_a_level=0.9", "filter_model=0", "filter_cutoff=300", "filter_res=0.6",
                        "filter_env=0.6", "env2_attack=0.001", "env2_decay=0.25", "env2_sustain=0",
                        "accent_amount=1", "accent_vel=0.87", "master_volume=-12"]
        res = {}
        for vel in (127, 90):
            wav, st = render(f"accent_{vel}", ["--events", f"36@0.1-0.6:{vel}", "--seconds", "1"] + sets(acid), lib)
            m, _, fs = load_mono(wav)
            seg = an.segment(m, fs, 0.1, 0.5)
            track, _ = an.centroid_track(seg, fs, 1024, 480)
            res[vel] = (an.db(float(np.abs(seg).max())), float(np.mean(track[:10])))
        record(6, "accent: >= 3 dB louder and brighter", res[127][0] - res[90][0] >= 3.0 and res[127][1] > res[90][1],
               {"peak_gain_db": round(res[127][0] - res[90][0], 2), "centroid_127": round(res[127][1]),
                "centroid_90": round(res[90][1])})

    # 7. Slide: overlapping notes in Legato glide continuously and settle within three glide times.
    if want(7):
        wav, st = render("slide", ["--events", "36@0-1.2,43@1-3.5", "--seconds", "3.5"]
                         + sets(CLEAN + ["voice_mode=1", "glide=0.5"]), lib)
        m, _, fs = load_mono(wav)
        t, f = an.zero_crossing_track(m, fs)
        semis = 12 * np.log2(f / 440.0) + 69
        sel = (t > 0.1) & (t < 3.4)
        jumps = np.abs(np.diff(semis[sel]))
        settled = semis[(t > 2.5) & (t < 3.4)]
        err = float(np.abs(settled - 43).max()) * 100 if len(settled) else 999
        record(7, "slide: continuous, settles within 3 glide times", jumps.max() < 1.0 and err < 10.0,
               {"max_jump_semitones": round(float(jumps.max()), 3), "settle_error_cents": round(err, 2)})

    # 8. Drop: a 12-semitone drop starts at least 1.5 times higher than it settles.
    if want(8):
        wav, st = render("drop", ["--notes", "45", "--seconds", "1.5", "--hold", "1.2"]
                         + sets(CLEAN + ["drop_amount=12", "drop_time=250"]), lib)
        m, _, fs = load_mono(wav)
        t, f = an.zero_crossing_track(m, fs)
        early = f[t < 0.02]
        late = f[(t > 0.8) & (t < 1.1)]
        ratio = float(early.mean() / late.mean()) if len(early) and len(late) else 0.0
        record(8, "drop: first 20 ms >= 1.5x settled pitch", ratio >= 1.5, {"ratio": round(ratio, 3),
                                                                             "settled_hz": round(float(late.mean()), 2)})

    # 9. Emitter harmonics: C1 saw at 24 dB clip fills 250 Hz-4 kHz while the sub band stays put.
    if want(9):
        base = CLEAN + ["gen_a_wave=0", "gen_a_level=0.8", "emit_split=100", "master_volume=-12", "emit_os=1"]
        dry_wav, _ = render("emitter_dry", ["--notes", "24", "--seconds", "3", "--hold", "3"] + sets(base + ["emit_mix=0"]), lib)
        wet_wav, st = render("emitter_clip", ["--notes", "24", "--seconds", "3", "--hold", "3"]
                             + sets(base + ["emit_mix=1", "emit_mode=1", "emit_drive=24"]), lib)
        dry = an.mono(an.load(str(dry_wav))[0])[24000:]
        wet = an.mono(an.load(str(wet_wav))[0])[24000:]
        mid_share = an.band_fraction(wet, 48000, 250, 4000)
        sub_diff = 10 * math.log10(an.band_energy(wet, 48000, 20, 50) / (an.band_energy(dry, 48000, 20, 50) + 1e-30))
        record(9, "emitter: >= 25 % in 250 Hz-4 kHz, sub within 1 dB", mid_share >= 0.25 and abs(sub_diff) <= 1.0,
               {"share_250_4k_pct": round(100 * mid_share, 1), "sub_change_db": round(sub_diff, 2)})

    # 10. Aliasing: 4x oversampling cuts inharmonic energy of a folded C5 saw by at least 6 dB.
    if want(10):
        base = CLEAN + ["gen_a_wave=0", "gen_a_level=0.8", "emit_split=30", "emit_mix=1", "emit_mode=2",
                        "emit_drive=30", "master_volume=-12"]
        ratios = {}
        for os_index in (0, 2):
            wav, st = render(f"alias_os{os_index}", ["--notes", "72", "--seconds", "2", "--hold", "2"]
                             + sets(base + [f"emit_os={os_index}"]), lib)
            m = an.mono(an.load(str(wav))[0])[9600:]
            ratios[os_index] = an.inharmonic_ratio(m, 48000, 523.2511)
        gain = 10 * math.log10(ratios[0] / (ratios[2] + 1e-30))
        record(10, "aliasing: 4x at least 6 dB below off", gain >= 6.0,
               {"inharmonic_off_db": round(10 * math.log10(ratios[0] + 1e-30), 1),
                "inharmonic_4x_db": round(10 * math.log10(ratios[2] + 1e-30), 1), "improvement_db": round(gain, 1)})

    # 11. Latency: the reported latency is exactly the delay through the plugin, at every oversampling setting.
    if want(11):
        sr = 48000
        start = int(0.5 * sr)
        length = int(round(6.0 * sr / 2000.0))
        burst = np.zeros(sr)
        i = np.arange(length)
        burst[start:start + length] = 0.5 * (0.5 - 0.5 * np.cos(2 * np.pi * i / length)) * np.sin(2 * np.pi * 2000 * i / sr)
        lags = {}
        latency = None
        for os_index in (0, 1, 2):
            wav, st = render(f"latency_os{os_index}", ["--mute-voices", "--inject-burst", "2000@0.5", "--seconds", "1"]
                             + sets(CLEAN + [f"emit_os={os_index}", "emit_split=30", "master_volume=0"]), lib)
            m = an.mono(an.load(str(wav))[0])
            lags[os_index] = an.cross_correlation_lag(burst, m[:len(burst)], 200)
            latency = st.get("latency")
        record(11, "latency: reported delay matches the signal path at off/2x/4x", all(abs(v) <= 1 for v in lags.values()),
               {"reported_samples": latency, "residual_lag_off_2x_4x": [lags[0], lags[1], lags[2]]})

    # 12. COOLDOWN: quarter-note pump at 128 BPM repeats every 469 ms.
    if want(12):
        # Noise carrier: a pitched tone would bias the envelope autocorrelation toward whole periods.
        wav, st = render("cooldown", ["--bpm", "128", "--notes", "45", "--seconds", "8", "--hold", "8"]
                         + sets(CLEAN + ["gen_a_level=0", "noise_level=0.5", "spark_colour=1", "emit_split=30",
                                         "cool_rate=2", "cool_shape=0", "cool_depth=0.8", "cool_hold=0.3"]), lib)
        m, _, fs = load_mono(wav)
        env = an.envelope(m[int(0.5 * fs):], fs, 0.002)
        period = an.autocorr_period(env, 0.002, 0.3, 0.7)
        record(12, "cooldown period 469 ms +- 5 at 128 BPM", abs(period - 0.46875) <= 0.005,
               {"period_ms": round(1000 * period, 2)})

    # 13. LFO sync: eighth notes at 120 BPM modulate the cutoff at 4 Hz.
    if want(13):
        wav, st = render("lfo_sync", ["--bpm", "120", "--notes", "45", "--seconds", "6", "--hold", "6"]
                         + sets(CLEAN + ["gen_a_wave=0", "filter_model=1", "filter_cutoff=800", "lfo_sync=4",
                                         "lfo_target=0", "lfo_depth=0.8", "lfo_shape=0"]), lib)
        m, _, fs = load_mono(wav)
        track, step = an.centroid_track(m[int(0.5 * fs):], fs, 2048, 240)
        rate = an.modulation_frequency(track, step)
        record(13, "LFO 1/8 at 120 BPM = 4 Hz", abs(rate - 4.0) <= 0.1, {"modulation_hz": round(rate, 3)})

    # 14. Every SPECIMEN entry (FL pack and STALKER SPECIMENS library) loads; tables play E2 on pitch,
    #     noise-like sources stay finite and audible.
    if want(14):
        spec_patch = CLEAN + ["gen_a_level=0", "specimen_level=1", "specimen_mode=0", "specimen_track=1", "master_volume=-6"]
        catalogue = [line.split("\t") for line in
                     subprocess.run([str(HARNESS), "--list-specimens"], capture_output=True, text=True).stdout.splitlines()]
        for row in catalogue:
            choice = int(row[0])
            wav, st = render(f"specimen_{choice:02d}", ["--notes", "40", "--seconds", "2.5", "--hold", "2.2"]
                             + sets(spec_patch + [f"specimen_source={choice}"]), lib)
            m, data, fs = load_mono(wav)
            status = st.get("specimen", "")
            seg = an.segment(m, fs, 0.2, 2.1)
            level = an.rms_db(seg)
            loaded = "NOT FOUND" not in status and "UNREADABLE" not in status and st["exit"] == 0
            if "NOISY" in status:
                ok = loaded and level > -40.0
                detail = {"status": status, "rms_db": round(level, 1)}
            else:
                f0 = an.dominant_frequency(seg, fs)
                k = max(1, round(f0 / E2))
                cents = 1200 * math.log2(f0 / (k * E2))
                ok = loaded and abs(cents) <= 2.0 and level > -40.0
                detail = {"status": status, "dominant_hz": round(f0, 2), "harmonic": k, "cents": round(cents, 2)}
            record(14, f"specimen {choice:02d} {row[1]} ({row[2]})", ok, detail)

    # 15. Every factory preset: finite, under its ceiling, between -22 and -6 dBFS RMS on a bass line.
    line = "36@0-0.45,36@0.5-0.95,43@1-1.45,41@1.5-2.4,36@2.5-2.95,48@3-3.45,39@3.5-4.2"
    if want(15):
        for i, name in enumerate(presets):
            wav, st = render(f"preset_{i:02d}", ["--preset", str(i), "--events", line, "--seconds", "5", "--bpm", "128"], lib)
            m, data, fs = load_mono(wav)
            playing = data[:int(4.2 * fs)]
            level = an.rms_db(playing)
            peak = an.db(float(np.abs(data).max()))
            ok = st["exit"] == 0 and st.get("non_finite") == "0" and peak <= -0.2 and -22.0 <= level <= -6.0
            record(15, f"preset {i:02d} {name}", ok, {"peak_db": round(peak, 2), "rms_db": round(level, 2),
                                                      "faults": st.get("faults"), "rt_x": st.get("realtime_x")})

    # 16. Voice stealing: ten overlapping notes on six poly voices.
    if want(16):
        events = ",".join(f"{36 + 3 * k}@{0.2 * k:.1f}-{3.0 + 0.1 * k:.1f}" for k in range(10))
        wav, st = render("stealing", ["--preset", "Antenna Grounds", "--events", events, "--seconds", "5"], lib)
        res = an.analyse(str(wav))
        record(16, "voice stealing 10 on 6", st["exit"] == 0 and res["non_finite"] == 0 and res["peak_db"] <= 0.0,
               {"peak_db": res["peak_db"], "max_jump": res["max_jump"]})

    # 17. State round trip.
    if want(17):
        wav, st = render("roundtrip", ["--preset", "5", "--seconds", "0.5", "--roundtrip"], lib)
        record(17, "state round trip", st.get("roundtrip") == "PASS", {"state_bytes": st.get("state_bytes")})

    # 18. Switching presets while a tail rings must not swell it.
    if want(18):
        # Long release tail, presets browsed while it rings: the tail must only fall.
        wav, st = render("preset_switch_tail", ["--preset", "Psi Field", "--events", "36@0-2,43@0-2", "--seconds", "6",
                                                "--set", "env1_release=4", "--tail-from", "2",
                                                "--program-at", "4@2.6,13@3.1,1@3.6"], lib)
        rise = float(st.get("tail_rise_db", "-1000"))
        record(18, "preset switch during tail: no swell", st["exit"] == 0 and rise <= 3.0,
               {"tail_rise_db": rise, "tail_peak_db": st.get("tail_peak_db")})

    # 19. Randomised session with silent stretches: nothing may rise in silence or go non-finite.
    if want(19):
        cmd = [str(HARNESS), "--fuzz", "--seconds", str(args.fuzz_seconds), "--seed", "11", "--vary-blocks"]
        if lib:
            cmd += ["--library", lib]
        if SPECIMENS:
            cmd += ["--specimens", SPECIMENS]
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=3600)
        stats = {k: (q if v.startswith('"') else v) for k, v, q in STAT.findall(proc.stdout)}
        ok = (proc.returncode == 0 and stats.get("non_finite") == "0" and stats.get("growth_fx_only") == "0"
              and stats.get("faults") == "0")
        record(19, f"fuzz {args.fuzz_seconds:.0f} s", ok,
               {k: stats.get(k) for k in ("events", "preset_changes", "growth_fx_only", "worst_growth_fx_db",
                                          "non_finite", "faults", "block_max_ms")})

    # 20. CPU: heaviest preset, six poly voices, 4x oversampling, 48 kHz, 512-sample blocks.
    if want(20):
        wav, st = render("bench", ["--preset", "Monolith Wall", "--notes", "28,35,40,43,47,52", "--seconds", "10",
                                   "--hold", "9"] + sets(["voice_mode=2", "voices=6", "emit_os=2"]), lib)
        rt = float(st.get("realtime_x", 0))
        record(20, "cpu: 6 voices x 7 unison at 4x faster than 15x real time", rt > 15.0,
               {"realtime_x": rt, "block_max_ms": st.get("block_max_ms")})

    # 23. RESONATOR read taps stay inside the comb buffer at every sample rate, write position and delay,
    #     including delays a hair above a whole number, where 0.1.1's float wrap read one sample past the end.
    if want(23):
        proc = subprocess.run([str(HARNESS), "--selftest-resonator"], capture_output=True, text=True, timeout=600)
        stats = {k: (q if v.startswith('"') else v) for k, v, q in STAT.findall(proc.stdout)}
        record(23, "resonator read taps inside the buffer", proc.returncode == 0 and stats.get("bad") == "0",
               {k: stats.get(k) for k in ("checked", "bad", "old_past_end")})

    # 22. pluginval at strictness 5 and 10.
    if want(22) and not args.skip_pluginval:
        pluginval = shutil.which("pluginval") or r"F:\_DEV\tools\pluginval\pluginval.exe"
        for level in (5, 10):
            log = ROOT / "build" / f"pluginval-level{level}.log"
            proc = subprocess.run([pluginval, "--strictness-level", str(level), "--validate", str(BUNDLE)],
                                  capture_output=True, text=True, timeout=1800)
            log.write_text(proc.stdout + proc.stderr, encoding="utf-8")
            record(22, f"pluginval strictness {level}", proc.returncode == 0 and "SUCCESS" in proc.stdout,
                   {"exit": proc.returncode, "log": str(log.name)})

    passed = sum(t["pass"] for t in report["tests"])
    report["summary"] = f"{passed}/{len(report['tests'])} passed"
    print(report["summary"])
    (ROOT / "build").mkdir(exist_ok=True)
    (ROOT / "build" / "test-report.json").write_text(json.dumps(report, indent=2))
    return 0 if passed == len(report["tests"]) else 1


if __name__ == "__main__":
    sys.exit(main())
