# LAB X-10 design notes

## Sonic identity

Lab X-10 is a bunker full of generators feeding the Rainbow emitter, a weapon assembled from parts of other
labs and run in cycles so it can cool down. Each idea became a group of sounds, tuned against measurements of
the reference recordings in the `stalker sounds` pack (averaged 8192-point FFT of the mono decodes).

| Pillar | Reference recordings | Measured character | Synth feature |
|---|---|---|---|
| The generators | `ugrnd_lab_2`, `ugrnd_ambient_machine_2/3`, `ugrnd_drone_2`, `transformer_hum1`, `cooling_run` | hums at 43, 75 to 86, 108 and 124 Hz, almost nothing above 250 Hz | clean, phase-locked SUB with a HUM shape; TABLE frames cut at the hum |
| The emitter at full power | (design target: Boys Noize, Nine Inch Nails) | saturated, mid-forward, pumping | EMITTER above the split, SCORCH, COOLDOWN, MONOLITH unison |
| The sidetrack and the keypad | (design target: TB-303 lines) | squelch, slides, accents | diode ladder, legato slides, accent with sweep charge |
| Components from X-16 and X-18 | the machine recordings above | periodic enough to cut single cycles from | 16-frame wavetables with FFT mip levels |
| Bloodsuckers and Monolith | `sucker_growl_2` (38 Hz), `forest_hidden_3` (54 Hz), `vampire_sucking` (43 Hz) | low growls with noisy tops | growl tables, LFO wobble, RESONATOR vowels |
| Switchgear | `switch_1` (flatness 0.15), `sparks1` (6.3 kHz, flatness 0.86) | short broadband transients | SPARK burst at note-on |

Guiding rules:

1. **The sub never moves, everything above it burns.** Below the split the signal is mono, phase-locked,
   undistorted and level-regulated. The emitter, LFO pitch and level, COOLDOWN, RESONATOR and stereo width act
   only above it.
2. **Built for the piano roll.** Legato slides, velocity accents, DROP and SCORCH make basslines playable from
   notes alone.
3. **Power has a duty cycle.** The LFO and COOLDOWN lock to the host's bar position while it plays and keep the
   last tempo when it stops.

## Signal flow

Per voice, up to six (mono modes use spare voices to hand notes over cleanly):

```
GEN B ──FM / ring──> GEN A × MONOLITH (1..7, alternating pan) ─┐
SUB (sine | triangle | square | hum, LOCK, DROP) ── route ──┬──┤   (Filter route)
SPARK + NOISE ─────────────────────────────────────────────┼──┼─> drive ─> FILTER ─┬─> AMP ENV
SPECIMEN (TABLE | GRAIN) ──────────────────────────────────┘  │   (diode | ladder | svf)
                                                              └─────────────────────┘ (Direct route)
```

Global, per 16-sample control block:

```
voice sum ─> EMITTER [ oversampled: LR4 split ─> low band ─> mono, clean
                                              └> high band ─> pre-emphasis ─> shaper ─> Δ high-pass ─> tone ─> mix ]
          ─> latency pad ─> RESONATOR (high band) ─> COOLDOWN × LFO level (high band) ─> width (high band)
          ─> low + high ─> master volume ─> DC blocker ─> REGULATOR (compressor, weight, look-ahead limiter)
          ─> fault guard ─> preset fade ─> safety clip
```

## Modules

- **GENERATOR A**: PolyBLEP saw whose SHAPE subtracts a half-cycle-shifted saw (saw to square), PolyBLEP pulse
  (SHAPE narrows it), polyBLAMP triangle and sine with wavefolding, and TABLE, which reads the SPECIMEN wavetable
  with SHAPE as the frame position. MONOLITH spreads 1 to 7 copies symmetrically over ±DETUNE cents and pans
  neighbours to opposite sides with constant power, normalised by 1/√N.
- **GENERATOR B**: sine, triangle, saw or square at a ratio of A. FM is phase modulation of every A copy; RING
  multiplies A by B.
- **SUB**: one or two octaves below the note (or at it), never moved by the LFO. HUM is a sine with its second
  harmonic at -9 dB and third at -18 dB. Route Direct (default) skips drive and filter.
- **LOCK**: Off lets phases run free; Sub restarts the sub at zero on every note; All restarts every oscillator
  and reseeds the noise, so repeated notes are sample-identical. In Mono and Legato a retriggered note with LOCK
  on starts in a fresh voice while the old one fades out over 5 ms: hits stay identical without clicks.
- **DROP**: exponential pitch fall over DROP TIME (95 % done), on A, B and the sub.
- **FILTER**: all three are zero-delay-feedback (topology-preserving) designs, stable under per-sample cutoff
  modulation.
  - *Diode ladder*: Zavalishin's analysis in the TPT form popularised by Will Pirkle, four coupled stages, loop
    input saturated by a Padé tanh. Its resonant peak sits at 0.70 of the coefficient frequency, so the cutoff is
    scaled by 1/0.70 to put the peak where the knob says. Self-oscillation starts near k = 17; full resonance is
    k = 17.2. Output makeup 1 + 0.33 k keeps the level usable while preserving the bass loss relative to the peak.
  - *Transistor ladder*: four TPT one-poles with the feedback solved linearly and the loop input saturated;
    k up to 4.15, makeup 1 + 0.6 k.
  - *SVF*: LAB X-3's TPT state-variable filter, low-, band- and high-pass.
  - Accent: velocity at or above ACC VEL multiplies the filter-envelope depth by 1 + ACCENT, adds up to 3 dB and
    charges a sweep capacitor (+0.5 per accent, 0.5 s decay) that adds up to one more octave to consecutive
    accents. Keytrack is centred on MIDI 48.
- **EMITTER**: JUCE polyphase IIR oversampling (2x, 4x) on three channels; Linkwitz-Riley 4th-order split. The
  shaper input is pre-emphasised above 400 Hz by up to +12 dB in proportion to drive, the shaper fades in over
  the first 6 dB of drive (drive 0 is transparent), and only the difference it makes is kept, DC-blocked and
  high-passed at the split. Output compensation `0.7 / tanh (0.7 g)` holds the distorted band near a constant
  level as drive rises. Crush is sample-and-hold plus bit reduction driven by DRIVE, BIAS trading one for the
  other. Lower oversampling settings are padded with a delay to the 4x latency, so the reported latency never
  changes.
- **RESONATOR**: feedback comb at the newest note's pitch × RATIO, damped at 5 kHz, loop soft-limited, through
  three band-passes interpolated across the vowels U, O, A, E, I. It listens to the high band only.
- **LFO**: sine, triangle, saw, square, sample-and-hold, and Drift (LAB X-3's PROGRAMMER walk). Synced rates
  read the host's quarter-note position while it plays, so the LFO stays on the grid even when a note starts
  mid-bar; NOTE retrigger restarts it on every note instead.
- **COOLDOWN**: Pump (exponential recovery, HOLD sets the time constant), Gate (HOLD sets the duty cycle), Saw
  and Sine shapes on a 1 ms smoothed gain applied to the high band.
- **REGULATOR**: compressor with threshold -30 dB × AMOUNT, ratio 1 + 7 × AMOUNT, 6 dB knee, 5 ms attack,
  80 ms release, half the static gain reduction as makeup; its detector holds peaks for 50 ms so a steady sub
  sees a steady gain. WEIGHT is an RBJ low shelf at 60 Hz. The limiter looks ahead 1.5 ms (a moving minimum of
  the required gain, then a box average of the same length), so the output never exceeds CEILING.
- **SCORCH** (plus mod wheel): +18 dB emitter drive, +25 cents detune, +0.2 resonance, +0.3 pump depth, +0.3
  resonator feedback, tone 0.3 darker, LFO depth up to 1.5×.
- **SPECIMEN tables**: for each of 16 evenly spaced windows (4096 samples), the loudest partial between 20 Hz
  and 1 kHz is found with a zero-padded FFT and refined by normalised autocorrelation within ±15 % of its period.
  If the correlation reaches 0.3, one period is cut from the nearest rising zero crossing, cross-faded with the
  period before it so it loops seamlessly, resampled to 2048 points with Hermite interpolation, DC-removed and
  levelled to 0.35 RMS. Otherwise the window becomes a cross-faded 2048-sample loop. Eleven mip levels keep only
  the harmonics that stay under 19 kHz across each octave. A source with fewer than half its frames periodic is
  reported as NOISY. Tables and grains are published to the audio thread together, lock-free, as in LAB X-3.
- **Hardening** (from LAB X-3): preset changes write every parameter once, fade out over 30 ms, clear voices
  and effect state, snap smoothers and fade back in; oversampling changes use the same fade. Non-finite samples
  are silenced and counted as FAULTS. MIDI IN rate in SYSTEM.

## 0.1.1: the STALKER SPECIMENS library

- Catalogue entries carry a root: FL's `stalker sounds` pack, or the STALKER SPECIMENS library at
  `<drive>:\_AUDIO\STALKER SPECIMENS` (sounds extracted from the user's own copies of the games; recognised by
  any `<game>\sounds` folder, the same convention LAB X-3 0.2.0 uses). Entries 1-20 are unchanged, so saved
  projects keep their sources; entries 21-37 are new, appended at the end.
- New sources, with the table pitch the cutter measured:

| # | Name | File | Table |
|---|---|---|---|
| 21 | Scorcher Radar | Shadow of Chornobyl `device/radar_idle.ogg` | 22 Hz |
| 22 | Radar Wind-Down | Shadow of Chornobyl `device/radar_stop.ogg` | 34 Hz |
| 23 | X-16 Brain | Shadow of Chornobyl `ambient/x16/x16_brain_run.ogg` | 256 Hz |
| 24 | X-16 Psy Field | Shadow of Chornobyl `ambient/x16/x16_psy_l.ogg` | 119 Hz |
| 25 | Monolith | Shadow of Chornobyl `ambient/monolith_1.ogg` | 58 Hz |
| 26 | Psy Blackout | Shadow of Chornobyl `affects/psy_blackout_r.ogg` | 105 Hz |
| 27 | Swamp Generator | Clear Sky `ambient/special/marsh_generator.ogg` | 100 Hz |
| 28 | Jupiter Generator | Call of Prypiat `ambient/jupiter/jup_b219_generator_looped.ogg` | 95 Hz |
| 29 | Pripyat Generator | Call of Prypiat `ambient/pripyat/pri_b306_generator_work.ogg` | 275 Hz |
| 30 | Underpass Transformer | Call of Prypiat `ambient/underpass/pas_b400_transformer.ogg` | noise-like |
| 31 | X7 Generator | S.T.A.L.K.E.R. 2 `GSCAudioVolumes/AV_Generators/SFX_AV_Generators_X7` | 25 Hz |
| 32 | Generator Sub | S.T.A.L.K.E.R. 2 `AV_Generators/SFX_AV_Generators_Large_Loop_16` | 28 Hz |
| 33 | Electro Anomaly | S.T.A.L.K.E.R. 2 `Anomalies/Electro/SFX_Anomaly_Electro_01` | 35 Hz |
| 34 | Arena Generator | S.T.A.L.K.E.R. 2 `AV_Generators/SFX_AV_Generators_Strelok_Arena_09` | 45 Hz |
| 35 | Mains Hum | S.T.A.L.K.E.R. 2 `AV_Generators/SFX_AV_Generators_Large_Loop_06` | 50 Hz |
| 36 | Psy Noise | S.T.A.L.K.E.R. 2 `Effects/Psy/SFX_Psy_Noise_Play_02` | 53 Hz |
| 37 | Hundred Hertz | S.T.A.L.K.E.R. 2 `AV_Generators/SFX_AV_Generators_Large_Loop_22` | 99 Hz |

- The loader silences non-finite samples and samples above +24 dBFS, with 10 ms either side, before
  normalising. Shadow of Chornobyl's `psy_blackout_l.ogg` decodes with 238 samples up to +71 dBFS at its start;
  the catalogue uses the clean right channel, and the guard protects files loaded with LOAD FILE.

## 0.1.2: RESONATOR read fix

- The RESONATOR comb read its delayed sample at a float position, wrapped by adding the buffer length when it
  went negative. When the delay sat a hair above a whole number of samples (a fractional part below about
  1.2e-4 at 44.1 or 48 kHz) and the write index equalled that number, the position came out a tiny negative
  value, adding the length rounded it up to exactly the length, and the comb read one sample past its buffer.
  That is heap memory: usually a small value, sometimes a huge one or a NaN, which the comb would turn into a
  click held at the REGULATOR ceiling or a fault that silences and resets every voice. No note at RATIO 1 hits
  it, but RATIO, pitch bend, glide and DROP move the delay through such values: a simulation of 0.1.1 found
  about one bad read per ten minutes of continuous movement, and a setting that stops on a bad value repeats it
  about 15 times a second. LAB X-3 (NOOSPHERE) and LAB X-19 (ENSEMBLE) found the same bug in their delay lines
  on 2026-10-09.
- Fix: `Resonator::taps` splits the delay into whole and fractional samples before wrapping, so both read
  indices are exact integers inside the buffer; a non-finite tuning falls back to the shortest delay. The
  interpolation is unchanged, and every level and pitch in the test battery matches 0.1.1.
- Test 23 (`LabX10Render --selftest-resonator`) checks the taps at seven sample rates, every write position and
  delays just above whole numbers: 3,114,370 cases, none outside the buffer, where 0.1.1's arithmetic reads past
  the end in 41,440 of them.

## Changes from the plan

Decided during the build, all in the plan's spirit:

- **SUB ROUTE** (79th parameter): the plan drew the sub through the filter. A Direct route (the default) keeps
  the sub out of the filter's resonance loss, which is what rule 1 asks for; Filter is still available.
- **Constant latency**: instead of reporting a new latency when EMIT OS changes (risk 1), the lower settings are
  padded to the 4x latency. FL never sees the latency change.
- **Own compressor and limiter**: JUCE's compressor detects each channel separately and its limiter always
  clips at 0 dBFS. The REGULATOR links the channels and limits at the CEILING setting with look-ahead.
- **Master volume before the regulator**, so CEILING is the true output ceiling.
- **Own transistor ladder**: JUCE's `LadderFilter::processSample` is protected and smooths its parameters per
  block, which does not suit per-voice, per-sample cutoff modulation.
- **Emitter voicing**: test 9 showed that a plain clipper on a C1 note leaves only about 5 % of the energy in
  250 Hz to 4 kHz, because clipping a periodic wave puts most energy back into its low harmonics. Drive-scaled
  pre-emphasis, a higher settled level and the high-pass on the shaper's contribution raised it to 27 % and
  keep the sub band within 0.01 dB of the dry render.
- **Table period detection**: autocorrelation alone locked onto sub-harmonics of noisy machinery (27 Hz for an
  81 Hz hum). Picking the loudest partial first and refining with autocorrelation lands on the hums measured in
  the plan.
- **Window**: 1220 × 760 with five control rows rather than four; stacked switches keep each panel narrow.
- **Filter slope band**: slopes are fitted from 2× to 8× the cutoff. Fitted further out, every 4-pole filter
  reads its asymptotic 24 dB/oct and the diode ladder's gentler transition does not show.
- Not built (stretch items in the plan): hard sync and a step sequencer.

## Verification

`python Tests/run_tests.py`: 80 checks over 23 tests (the plan's 22 plus the resonator read check), all passing
(0.1.2, 2026-10-10, 48 kHz unless noted).

| # | Test | Result |
|---|---|---|
| 1, 21 | A2 pitch at 48, 44.1, 96 kHz | +0.16, +0.05, +0.16 cents |
| 2, 21 | Sub purity, init patch, C1 sub | THD 0.002 %, 100 % below 250 Hz |
| 3 | LOCK All repeat | max difference 2.6e-7 (Off: 0.58) |
| 4 | Unison side below split vs above | -36.8 dB, peak spread 3.66 Hz at 110 Hz |
| 5 | Slopes diode / ladder / SVF | 21.1 / 23.0 / 11.5 dB/oct, resonance at 480 / 480 / 492 Hz for 500 Hz |
| 6 | Accent 127 vs 90 | +4.3 dB, brighter |
| 7 | Slide | largest step 0.56 semitone, settled within 0.1 cent |
| 8 | DROP 12 st | first 20 ms at 1.92× the settled pitch |
| 9 | Emitter clip 24 dB on C1 | 27.4 % in 250 Hz to 4 kHz, sub band +0.01 dB |
| 10 | Aliasing, folded C5 saw | 4x is 8.5 dB below off |
| 11 | Latency | 77 samples reported, residual 0 samples at off, 2x, 4x |
| 12 | COOLDOWN 1/4 at 128 BPM | 468.7 ms |
| 13 | LFO 1/8 at 120 BPM | 4.00 Hz |
| 14 | 37 specimen sources (20 pack, 17 library) | 33 tables play E2 on a harmonic within 0.05 cents; 4 NOISY sources audible |
| 15 | 16 presets on a bass line | RMS -13.3 to -18.8 dBFS, peaks -2.4 dBFS or lower |
| 16-18 | Stealing, round trip, preset switch in a tail | pass; tail only falls (-1.3 dB) |
| 19 | 300 s randomised session | no faults, nothing rising in silence |
| 20 | Monolith Wall, 6 voices × 7 unison, 4x | 33× real time |
| 22 | pluginval | strictness 5 and 10 SUCCESS |
| 23 | RESONATOR read taps | 3,114,370 cases inside the buffer (0.1.1's arithmetic: 41,440 past the end) |
