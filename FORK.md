# OpenTracker, semi fork (2.2.1-semi)

A fork of [sandlbn/polyend-tracker-re](https://github.com/sandlbn/polyend-tracker-re)
(OpenTracker 2.2.1) for the original Polyend Tracker, with generators and tools
from the semi plug-in family (Acid, Call & Response, Breakbeat / BBCut,
TURNAROUND, GENOTYPE / TILT, TRANSCODE / FRAMELOSS, CIPHER, NEXUSCLIP, DATASCREEN).

Image: `PolyendTrackerOTs221.ptf` (24 characters, installs like the upstream
image: copy to `/Firmware`, Config > Firmware update). Keep the stock
`PolyendTracker_1.9.2.ptf` (and the upstream `PolyendTrackerOTv221.ptf`) in
`/Firmware` to go back.

Everything a generator writes is ordinary steps and step FX, so a pattern made
with this fork plays the same on stock 1.9.x and on the Tracker Mini. The new
play modes (B1..B10, T1..T4, G1..G3), the fork's Perform FX, the effects and
send-effect models, themes and the Pixel font are fork-only.

## Everywhere

- **Shift lock**: tap Shift twice quickly (nothing else in between): Shift
  stays held, a red SHIFT tag shows in the title. The next Shift press
  releases it.
- **Level meter** (Config > General): a small L / R master meter in the title
  bar, with a red mark when the output reaches full scale.
- **Screen saver** (Config > General): after 1 / 2 / 5 / 10 minutes without a
  key, pad or encoder the Data screen fills the display; the first input
  returns to the page you were on (and does nothing else).
- **Pattern dashes**: the empty-cell dashes are drawn as 1-px bitmaps, so
  every row shows them in their column colours on the device (they used to
  look faded on all rows but the last).
- **Scale lock** (Config > General): the encoder on a note (or a selection)
  steps through the project scale (Config > Project > Notes scale, Root note)
  instead of semitones; an off-scale note snaps in. Shift still moves octaves.

## Generate (Pattern, page 3, F3)

Edit mode on, Pattern page 3 (More, More), **F3 Generate**. Laid out like Fill:
**F1** the generator, **F2..F7** its parameters (lists or bars; pads pick a list
item or spread over the range), **F8** Generate, **Pattern** or **Rec** closes.

- **Shift** turns to the second page of parameters (the title shows 1/2, 2/2).
- The strip above the bottom bar explains the selected parameter.
- The right column is a **live preview**: the generator runs on a copy of the
  pattern whenever a parameter changes, with the same random draw F8 writes,
  so the preview is exactly the result. A playhead runs over it while the
  pattern plays. The **encoder push** draws a new random result.
- The target is the selection, or the cursor track over the whole pattern with
  a single-cell cursor. One undo step per Generate. **Seed** 0 is random; any
  other seed rebuilds the same result.

| Generator | What it writes | Page 1 | Page 2 |
|---|---|---|---|
| **Acid** | a 303 line in the project scale (TB-3PO idea): Glide on slid notes, Volume 100 on accents, Gate on short notes | Density (-7..+7), Accent, Slide, Range (1-3 oct), Mutate, Seed | Mut. every (1/2/4/8 loops), Gate, Root |
| **Call+Resp** | call phrases and answers (port of Nordl3's Call / Response Generator: 46 styles, 38 answer strategies) | Style, Answer, Density, Layout (one track / Answer mine / Two tracks), Form, Seed | Steps per part, Ending, answer octave, Motif |
| **BBCut** | Nick Collins' BBCut procedures (via BBenCut) as hits of a **sliced** instrument | Proc, A / B (per procedure), Source, Mutate, Seed | Rolls, Pitch, Timing, Dynamics |
| **Turnaround** | phrase-end edits on a budget: the end of every phrase, half and quarter phrase gets a tail of edits | Phrase, Budget, Style (Rolls, Stutter, Drop, Reverse, Build, Mix), Intensity, FX (Soft / Medium / Hard), Seed | Timing (Tidy / Keep) |
| **Genotype** | learns the selected steps and plays a variation made only of them | Mode (Markov / Ball / both), Identity, Order, Variety, Seed | |
| **Station** | a numbers-station line from the radio: RDS name, PI code, frequency read as digits, in groups of five | Source, Mode (Notes / Slices), Rate, Groups, Repeat, Seed | |

**Acid.** Every step gets a pitch, an accent, a slide wish and a threshold: a
step plays while its threshold is under the density ((d + 8) / 16 of the
steps). Changing the density adds or removes notes in a fixed order, the
melody stays. A generated acid track stays live (up to four): the Perform FX
**Acid Density** (-7..+7) moves its density on the tracks the FX is on, and
**Mutate** re-rolls part of the line every N loops. Editing the track by hand
ends it.

**Call+Resp.** Form: C R, C R C R, C C R, C R R2 C, C C C R, C R C R2 (C = the
call, R = the answer, R2 = a second answer). Steps: 4 / 8 / 16 / 32 per part,
or Auto (the selection divided by the parts). **Two tracks** alternates: the
call plays on the first track in the call parts, the answer on the second track
in the answer parts. Ending: the style's way, on the root, open, rising,
falling. The preview tags the parts C / R.

**BBCut.** Procedures: Classic, ChooseCut, **Jungle** (amen-style edits in
eighths: double snares, kick shifts, a repeated half, a snare fill every 1 / 2 /
4 bars), WarpCut, SQPusher1, SQPusher2, CageCut, Timeline, Permute, ThrashCut.

- Cut offsets pick slices, cuts of a step or longer become hits (consecutive
  slices while a long cut plays on), shorter runs become Roll FX.
- A Tracker roll runs until the next note, so every run now ends with a Roll 0
  where it stops.
- **Rolls:** Clean, Fade out, Fade in, Pitch up / Pitch down (a Slide FX on the
  roll), Stutter (a fresh hit per step), Single (one hit).
- **Pitch:** a random ±1..3 semitones per hit (Tune FX on Beat Slice, the note
  in Slice mode). **Timing** / **Dynamics** off drop Micro-move / Volume FX.
- CageCut and Timeline draw their forms, a rotation and shuffled blocks from
  the seed; Permute uses eight musical permutations in eighth groups; ThrashCut
  writes no volume or timing FX.
- **Mutate:** new cuts every 1 / 2 / 4 / 8 loops while it plays.

BBCut wants a sliced sample: Auto Slice (Breaks detector, below) or equal
slices in Sample Playback. With Beat Slice the hits are notes 24 + slice; in
Slice mode a unity note and the Slice FX. For **Station / Slices**, record or
load a voice counting 0..9 and give it 10 slices. The Station preview shows
what was read from the radio and the digits it will play.

**Turnaround.** FX Soft / Medium / Hard: rolls up to 2 / 4 / 8 per step, build
volume from 70 / 50 / 30. Timing Tidy removes Micro-move inside the edited
steps. Stutters copy the hit only (not its FX), Drop cuts the last steps before
the downbeat, rolls end at the downbeat.

### Live (Performance)

**Perform FX chooser.** In edit mode (Rec), hold Shift and turn the encoder:
all Perform FX open as a grid (the encoder or a pad picks, F7 / F8 shows what
the FX does); releasing Shift (or pushing the encoder) puts it in the column.

**Generator controls** (new Perform FX, under the stock ones):

| FX | Range | What it does |
|---|---|---|
| Acid Density | -7..+7 | live Acid tracks: more / fewer notes, the melody stays |
| Acid Accent / Acid Slide | -100..+100 | live Acid tracks: more / fewer accents and slides (the line's own random order) |
| Acid Mutate | -100..+100 | live Acid tracks: how much re-rolls at each Mutate loop |
| BBCut A / BBCut B | -6..+6 | B1..B10: the procedure's A / B setting up or down (new cuts at once) |
| BBCut Rolls | None, Dense, Fade | B1..B10: no rolls, more rolls, fade-out rolls |
| BBCut Recut | Now, 1..8 loops, Locked | Now: new cuts at every pad press; else overrides Config > BB play modes |
| WT FM Depth | -100..+100 | wavetable tracks: FM from another track (below) |
| WT FM Source | Left, Trk 1..8 | the modulator: the track to the left or a chosen track |

**Scenes** (SETBOOK): hold **Fx2** and press **F1..F8** to store the Perform
state (every column's row and value, the tracks' FX switches) as scene 1..8;
hold **Fx1** and press F1..F8 to recall it. Fx1 + encoder sets the **morph**
(off, 1/4, 1/2, 1, 2, 4 bars): number FX glide to the scene's values over that
time, list FX switch at the end. While Fx1 / Fx2 is held the bottom bar shows
the scenes. Scenes are kept in the config (global, like the global presets).

**Perform recording** (BLACKBOX): **Insert** starts / stops writing the Perform
FX into the pattern while it plays: on every step of a track with FX on, the
first active column that has a step FX counterpart (Volume, Panning, Tune,
filters, sends, position, direction, LFO speeds, Step Repeater as Roll, Bit
Depth, Overdrive) is written into **FX 2**. One undo step; REC shows in the
title.

### Play modes (Performance, Pattern Play Mode)

- **B1..B10**: one mode per BBCut procedure (same order as the Proc list). They
  reorder any track's own steps with BBCut cuts and rolls, with the A / B set in
  the Generate popup. **Config > BB play modes**: new cuts at every stop, every
  1 / 2 / 4 / 8 loops, or locked; time is mixed into the seed.
- **T1..T4 (TILT)**: a pinball ball plays the track. T1 Bounce (ping-pong, the
  speed drawn at every wall), T2 Flipper (forward, flippers kick it back), T3
  Gravity (thrown up the track, slows, falls back, bounces), T4 Multiball (two
  balls from both ends, taking turns). A new game with every B-mode re-cut.
- **G1..G3 (Genotype)**: the track plays its own steps in a new order every
  loop: at a step with a note it may jump to another step holding the same
  note (on the same beat phase) and go on from there. G1 low, G2 medium, G3
  high variety.
- **Wavetable FM**: with WT FM Depth on a wavetable track, its pitch follows
  another track's sound sample by sample (through zero: strong negative
  values play the window backwards). Two or three tracks can modulate each
  other.

## Sample Editor effects

| Effect | Parameters |
|---|---|
| **Transcode** | Codec (u-law, A-law, G.726 40k / 32k / 24k / 16k), Rate (8 / 11 / 16 / 22 kHz), Generations (1-16 tandem passes with a D/A-A/D hop between), Bit errors (per 10k bits), Loss (10 ms packets), Conceal (Silence / Repeat / Glitch), Mix |
| **DCT Crush** | Block (64-512), Keep (% strongest bins), Bits (magnitude quantiser), Freeze (% frames that repeat the last spectrum), Mix |
| **Radio** (CIPHER) | Band (2.4 / 3 / 4 / 6 kHz), Fading (QSB depth), Fade rate, Whistle (heterodyne), Pitch, Noise (band noise, rising in the fades), Crashes (static), Mix |
| **Clipper** (NEXUSCLIP) | Curve (Hard, Smooth, Crisp, Classic, Pro, Tube, Tape), Drive, Knee, Match (normalise to 0 dBFS first), Ceiling, Mix |

G.726 follows the ITU algorithm in the classic Sun g72x structure (SNR on a busy
signal: 40k 34 dB, 32k 29 dB, 24k 23 dB, 16k 17 dB; u/A-law 37 dB). The Radio
effect's random parts are seeded the same for both channels of a stereo sample.

## Master

- **Master 4/4 Dynamics** (Master key, fourth page): limiter **mode** (Subtle /
  Extreme), stereo **link** (both sides limited from the louder one),
  **ceiling** (0..-12 dB after the limiter), the gain-reduction meter;
  **saturation** mode, **drive** (0..+24 dB into the curve, half of it made up),
  **mix** (dry / saturated) and **tone** (a low-pass after it). Drive, mix,
  tone, ceiling and link are kept in the config.
- **Reverb models** (Master Reverb F6, or Config > Reverb model): Stock,
  **Hall** (a large 4-line feedback delay network, slow modulation, damping in
  the loop), **Shimmer** (the hall with an octave-up shifter fed back, the
  octave builds in the tail), **Gated** (short and dense, closed after the
  hit). Size, Damping, Predelay and Diffusion drive all of them.
- **Delay models** (Config > Delay model): Stock, **Tape** (wow and flutter
  on the read head, repeats that saturate and darken), **Lo-fi** (repeats that
  lose bits and bandwidth). Time, Sync, Feedback, Filter and Ping-pong drive
  all of them.
- Both run in the stock effects' memory. Cost in the bench's busiest set
  (est. share of the audio block over the stock effect): Hall +1.6 %,
  Shimmer +5.2 %, Gated +2.6 %, Tape +1.8 %, Lo-fi +2.9 %.

## Config

- **General > Display theme**: 23 themes. Original, Monochrome, Anniversary,
  then Early Cyber, Corporate, Amber CRT, Green Phosphor, Thermal, Workstation,
  Datamatic, Dubplate, Rave Flyer, Jungle, Ice, Sunset, Teletext, Vaporwave,
  Blueprint, Game Boy, Sepia, Hi Contrast, and two animated ones: **Cyber Pulse**
  (accent and cursor breathe) and **Spectrum** (the hue turns). The new ones
  recolour Original (Workstation: Monochrome) in RAM; the stock config byte
  keeps the base theme.
- **General > Display font**: Original, New, **Pixel** (Departure Mono at its
  native 11 px, a 1-px grid). The stock config byte keeps Original for Pixel.
- **General > Auto slice**: Stock or **Breaks** (spectral-flux onsets, 46 ms
  spacing, up to 48 slices): every hit of a 170 BPM break.
- **General > BB play modes**: when the B modes get new cuts (above).
- **General > Beat Slice thru**: On, a Beat Slice note plays on to the sample
  end instead of stopping at the next slice (good with BBCut and long chops).
- **General > Scale lock / Screen saver / Level meter / Reverb model / Delay
  model**: see above.
- All of these live in the OpenTracker config extension record (24 bytes);
  the Perform scenes in a record of their own after it.

## Mixer, Song, Pattern

- **Global Mixer 3/4**: the synth columns mute / unmute with a tap; Shift solos
  one synth (again: unsolo). Session state, not saved in the project.
- **Song**: hold **Note** (average pitch), **Instr** (most used instrument),
  **Fx1** / **Fx2** (how many steps carry that FX) to colour the blocks.
- **Pattern**: an empty FX cell opens the FX list at the last effect used in
  that column.

## Radio (Sample Recorder, source Radio)

- RDS is decoded (programme service name and PI): the station name shows on the
  right of the title.
- **Shift + seek** (F3 / F4) scans the band: seek, stay 4 s, seek on. "SCAN"
  in the title; a seek key without Shift stops it.

## Data screen (File, F7 "Data")

An Ikeda / datamatics style view of what plays. Only the tracks with notes are
shown, and they share the whole screen.

- **F1 Barcode**, **F2 Matrix** (32 steps around the playhead), **F3 Numbers**
  (raw hex step data), **F4 Rain** (each track's last steps as falling hex
  bytes), **F5 Pulse** (a circular sequencer, one ring per track).
- **F6** horizontal / vertical, **F7** colour: Mono, Theme, Tracks (a hue per
  track), **F8** or Play starts / stops.
- **Pads 1-16** mute / unmute tracks 1-16 (lit: a note now, dim: on, dark:
  muted); the bottom two rows show the next steps. The **encoder** sets the
  tempo.

## Building and checks

The fork builds **without Games by default** (`OT_GAMES ?= 0`). Its flash
budget is **800,000 B** (`tools/telemetry_check.py`; upstream 720,000). The
hard limit is the stock 1.9.2 image extent, 0xD6460 = 877,664 B, which
`tools/check_image.py` checks on every build: nothing may be written past it.
The image is about 746 KB.

```
make                                    # build/nogames/PolyendTrackerOTs221.ptf (VERSION 2.2.1)
make host-test                          # all host tests
make perf-gate                          # QEMU instruction counts + flash size
make -C sim                             # desktop simulator
make fonts                              # regenerate the UI fonts (Pillow)
```

Audio path changes against upstream: Beat Slice thru (a slice end point),
synth slot mute (a gain), the TILT / Genotype play modes (a step looked up per
track per step), the master peak for the level meter, the Dynamics stage and
linked limiter (only when used), the reverb / delay models (only when
selected) and wavetable FM (only with WT FM Depth on). `make perf-gate` has
bench scenarios for the models (wHall .. wLofi).

Sources: TB-3PO idea (Phazerville Hemisphere / charlesvestal schwung-tb3po),
Nordl3 Call / Response Generator (MIT), bencodec/BBenCut and Nick Collins'
BBCut, ITU-T G.711 / G.726, Silicon Labs AN230 (RDS registers), Departure Mono
by Helena Zhang (OFL-1.1, third_party/fonts).
