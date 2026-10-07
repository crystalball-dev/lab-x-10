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

`python Tests/run_tests.py`: 62 checks over the plan's 22 tests, all passing (2026-10-07, 48 kHz unless noted).

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
| 14 | 20 specimen sources | 17 tables play E2 on a harmonic within 0.05 cents; 3 NOISY sources audible |
| 15 | 16 presets on a bass line | RMS -13.3 to -18.8 dBFS, peaks -2.4 dBFS or lower |
| 16-18 | Stealing, round trip, preset switch in a tail | pass; tail only falls (-1.3 dB) |
| 19 | 300 s randomised session | no faults, nothing rising in silence |
| 20 | Monolith Wall, 6 voices × 7 unison, 4x | 31× real time |
| 22 | pluginval | strictness 5 and 10 SUCCESS |
