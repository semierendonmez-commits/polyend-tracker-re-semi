# OpenTracker, semi fork (2.2.3-semi)

A fork of [sandlbn/polyend-tracker-re](https://github.com/sandlbn/polyend-tracker-re)
(OpenTracker 2.2.3, merged from 2.2.1) for the original Polyend Tracker, with generators and tools
from the semi plug-in family (Acid, Call & Response, Breakbeat / BBCut,
TURNAROUND, GENOTYPE / TILT, TRANSCODE / FRAMELOSS, CIPHER, NEXUSCLIP, DATASCREEN).

Image: `PolyendTrackerOTs223.ptf` (24 characters, installs like the upstream
image: copy to `/Firmware`, Config > Firmware update). Keep the stock
`PolyendTracker_1.9.2.ptf` (and the upstream `PolyendTrackerOTv223.ptf`) in
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
  other seed rebuilds the same result. Seed 0 mixes in the time the popup
  opened (and every encoder push), so two boots never draw the same result.

| Generator | What it writes | Page 1 | Page 2 |
|---|---|---|---|
| **Acid** | a 303 line in the project scale (after TB-3PO): Glide on slid notes, Volume 100 on accents, Gate on short notes | Density (-7..+7), Accent, Slide, Octaves (Off / TB-3PO / Wide), Mutate, Seed | Mut. every (1/2/4/8 loops), Gate |
| **Call+Resp** | call phrases and answers (port of Nordl3's Call / Response Generator: 46 styles, 38 answer strategies) | Style, Answer, Density, Layout (one track / Answer mine / Two tracks), Form, Seed | Steps per part, Ending, answer octave, Motif |
| **BBCut** | Nick Collins' BBCut procedures (via BBenCut) as hits of a **sliced** instrument | Proc, A / B (per procedure), Source, Mutate, Seed | Rolls, Pitch, Timing, Dynamics |
| **Turnaround** | phrase-end edits on a budget: the end of every phrase, half and quarter phrase gets a tail of edits | Phrase, Budget, Style (Rolls, Stutter, Drop, Reverse, Build, Mix), Intensity, FX (Soft / Medium / Hard), Seed | Timing (Tidy / Keep) |
| **Genotype** | learns the selected steps and plays a variation made only of them | Mode (Markov / Ball / both / **Drummer**), Identity, Order, Variety, Seed | |
| **Station** | a numbers-station line from the radio: RDS name, PI code, frequency read as digits, in groups of five | Source, Mode (Notes / Slices), Rate, Groups, Repeat, Seed | |
| **Grids** | Mutable Instruments Grids' drum map: kick, snare and hats from a point between 25 patterns | X, Y, Kick, Snare, Hats (densities), Seed | Chaos, Accent (Volume 70 under the accent level), Res (1/16, 1/32), Parts (kit on three tracks, or one part) |
| **Marbles** | random notes with deja vu (after Mutable Marbles): a T generator for which steps play, an X generator for the pitch | Density, Deja vu, Length, Spread, Bias, Seed | Steps (Chromatic, Scale, Triad, Root+5th, Root), Range, T model (Coin, Clusters, Drums, 2 tracks), Jitter, Rate |

**Grids.** The map is the original's (25 nodes of 3 x 32 levels, GPL-3.0):
X / Y blend the four nodes around the point, a part fires where its level is
over 255 - density, levels over 192 are accents, Chaos lifts the levels by a
random amount once per bar. 1/16 reads the even steps (a bar per 16 steps),
1/32 every step (half time). Kit: the cursor track and the next two get kick,
snare and hats, each with the instrument and note already on it. A generated
Grids kit stays **live**: the Perform FX **Grids X / Y / Density** redraw it
at once, and with Chaos it gets new chaos every loop.

**Marbles.** Deja vu replays the last Length draws (both which steps play and
their pitches) instead of new ones: 100 is a locked loop of Length, 0 always
new. Spread 0 plays one pitch (the Bias), the middle a bell around it, the top
the two extremes. 2 tracks: every step plays on one of the two tracks, the
first with the Density's probability (Marbles' complementary T outputs).

**Genotype Drummer** (after Noise86's MarkovDrummer): the selected tracks'
hits as one state per step (which tracks play), learnt as a Markov chain on
the previous state(s) and the beat phase; the new order copies whole columns,
so a kick, snare and hats stay together.

**Acid.** Density works like Logarhythm's TB-3PO (O_C Hemisphere), in both
directions from 0:

- **Gates:** a step plays with 10 + 14 x |density| %: 0 is the sparsest,
  -7 and +7 play every step.
- **Pitches:** on the negative side the line narrows to the lowest scale
  degrees and repeats more (a new pitch repeats the last one 50 - 6 (d + 7) %
  of the time): -7 is the root only, -6 the root and the next degree (303 half
  steps), from +1 the whole scale. So -7 is a full, root-heavy line and +7 a
  full melodic one.
- **Octaves:** a new pitch jumps an octave up or down 20 % each (TB-3PO),
  never (Off) or up to two octaves (Wide).

Every step keeps its own random draws, so the density can move live and the
same density gives the same line back: rising |density| only adds notes. A
generated acid track stays live (up to four): the Perform FX **Acid Density**
(an offset of -14..+14, so any density can be reached) moves it on the tracks
the FX is on, and **Mutate** re-rolls part of the line every N loops. Editing
the track by hand ends it.

**Call+Resp.** Form: C R, C R C R, C C R, C R R2 C, C C C R, C R C R2 (C = the
call, R = the answer, R2 = a second answer). Steps: 4 / 8 / 16 / 32 per part,
or Auto: the largest of 32 / 16 / 8 / 4 / 2 that fits the parts in the
selection (an odd selection, 3 x 10 steps say, falls back to an equal split).
The form repeats until the selection is full, the last part cut short; on a
repeat the answers swap (R <-> R2), so a long selection is not the same two
bars again. The default answer strategy is "Q + closure" (a question, then a
closing phrase), which makes R clearly an answer. **Two tracks** alternates: the
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
- **Mutate:** new cuts every 1 / 2 / 4 / 8 loops while it plays; every re-cut
  takes a fresh seed (time mixed in), and the Perform **BBCut A / B** offsets
  move the A / B of the next cuts. The first play after a boot re-cuts too.

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
| Acid Density | -14..+14 | live Acid tracks: moves the TB-3PO density (0 sparse, +-7 full, below 0 root-heavy) |
| Acid Accent / Acid Slide | -100..+100 | live Acid tracks: more / fewer accents and slides (the line's own random order) |
| Acid Mutate | -100..+100 | live Acid tracks: how much re-rolls at each Mutate loop |
| BBCut A / BBCut B | -6..+6 | B1..B10: the procedure's A / B setting up or down (new cuts at once) |
| BBCut Rolls | None, Dense, Fade | B1..B10: no rolls, more rolls, fade-out rolls |
| BBCut Recut | Now, 1..8 loops, Locked | Now: new cuts at every pad press; else overrides Config > BB play modes |
| WT FM Depth | -100..+100 | wavetable tracks: FM from another track (below) |
| WT FM Source | Left, Trk 1..8 | the modulator: the track to the left or a chosen track |
| Grids X / Grids Y | -100..+100 | live Grids kits: moves the map point (redrawn at once) |
| Grids Density | -100..+100 | live Grids kits: more / fewer hits on all three parts |
| Loop Lock | -100..+100 | per track: M1..M3, TM8 / TM16, R1 / R2: + locks the loop, - lets it change more; TM: the Turing Machine knob |
| DeRez | 0..100 | sample-rate reduction (sample and hold) on the tracks' samples, after Airwindows DeRez |

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
- **M1..M3 (Marbles deja vu)**: every step position keeps the step it plays
  with the lock probability (85 / 60 / 30 %), else draws a new one (mostly a
  step with a note): a loop that slowly rewrites itself.
- **TM8 / TM16 (Turing Machine)**: an 8 / 16-bit shift register; its low byte
  picks the step, the bit coming back in may flip. **Loop Lock** on the track
  is the Turing Machine knob: 0 changes slowly (5 % per step), **+100 locks
  it** (an 8 / 16 step loop that repeats), -50 is random, -100 a locked loop
  of twice the length with the second half inverted.
- **R1 / R2 (randLoops, after SickoCV)**: the track plays forward, but every
  note drifts by scale degrees: each loop a note moves one degree up or down
  with 1 - lock (R1 slow, R2 faster), kept within +-4 degrees of the original.
- **Wavetable FM**: with WT FM Depth on a wavetable track, its pitch follows
  another track's sound sample by sample (through zero: strong negative
  values play the window backwards). Two or three tracks can modulate each
  other.

## Track Link (Pattern, page 3, F4)

A track can follow another track live: whenever the **source** track plays a
note, the linked track plays its own instrument at the same moment. Everything
that makes the source play counts: its steps, rolls, arps, Chance / Random FX,
the play modes (B, T, G, M, TM, R), Acid / BBCut Mutate, the live Grids kit,
the Perform step repeater and the pads. Use it to layer a mutating line with
another sound, double a break with a kick, or send a generated part to a MIDI
synth as well. Not a copy: the steps stay on the source, so every change is
followed at once.

| Field | |
|---|---|
| Source | Off, Track 1..16 (not itself) |
| Instrument | what the linked track plays (tracks 9..16: synths and MIDI) |
| Note | Follow (the source's note) or a fixed note (drum layers) |
| Transpose | -24..+24 semitones |
| Level | % of the source note's volume |
| Chance | % of the source's notes it plays |
| FX | None, or Copy: the source step's FX come along (slice, volume, ...) |

- Changes apply at once; F8, Pattern or Rec close the popup. The right column
  lists every link of the project; a linked track's header shows "<N".
- The linked track's own steps still play. The source's OFF / gate end the
  linked note, unless the linked track has played its own note since.
- **Muted source:** its steps keep running silently, so only the layer is
  heard (its Tempo, Swing, Break, CC and pitch bend FX are skipped). A muted
  linked track stays silent.
- Links do not chain (a track following a linked track hears only that
  track's own notes). Saved with the project (`otfork.bin`); a stock firmware
  ignores them.

## Sample Editor effects

| Effect | Parameters |
|---|---|
| **Transcode** | Codec (u-law, A-law, G.726 40k / 32k / 24k / 16k), Rate (8 / 11 / 16 / 22 kHz), Generations (1-16 tandem passes with a D/A-A/D hop between), Bit errors (per 10k bits), Loss (10 ms packets), Conceal (Silence / Repeat / Glitch), Mix |
| **DCT Crush** | Block (64-512), Keep (% strongest bins), Bits (magnitude quantiser), Freeze (% frames that repeat the last spectrum), Mix |
| **Radio** (CIPHER) | Band (2.4 / 3 / 4 / 6 kHz), Fading (QSB depth), Fade rate, Whistle (heterodyne), Pitch, Noise (band noise, rising in the fades), Crashes (static), Mix |
| **Clipper** (NEXUSCLIP) | Curve (Hard, Smooth, Crisp, Classic, Pro, Tube, Tape), Drive, Knee, Match (normalise to 0 dBFS first), Ceiling, Mix |

| **Tuner** | (none) YIN pitch detection on the selection: note, cents, Hz; **Write** sets Tune / Finetune so the unity note plays the nearest C |
| **Paulstretch** | Stretch x2..x32, Window (50 / 100 / 200 / 400 ms): magnitudes kept, phases randomised (Paul Nasca's algorithm) |
| **Granular** | Length (x0.25..x8), Grain (10-500 ms), Density (grains/s), Spray (position randomness), Pitch (random +-semitones per grain), Reverse (%) (after Ourednik's Argotlunar) |
| **Strand** | Segment (min length), Shuffle, Repeat, Reverse, Length: the selection cut at rising zero crossings and rebuilt (after Leviathan's Strand; no clicks) |

G.726 follows the ITU algorithm in the classic Sun g72x structure (SNR on a busy
signal: 40k 34 dB, 32k 29 dB, 24k 23 dB, 16k 17 dB; u/A-law 37 dB). The Radio
effect's random parts are seeded the same for both channels of a stereo sample.

## Master

- **Master 4/5 Dynamics** (Master key, fourth page): limiter **mode** (Subtle /
  Extreme), stereo **link** (both sides limited from the louder one),
  **ceiling** (0..-12 dB after the limiter), the gain-reduction meter;
  **saturation** mode, **drive** (0..+24 dB into the curve, half of it made up),
  **mix** (dry / saturated) and **tone** (a low-pass after it). Drive, mix,
  tone, ceiling and link are kept in the config.
- **Reverb models** (Master Reverb F6, or Config > Reverb model): Stock,
  **Hall** (a large 4-line feedback delay network, slow modulation, damping in
  the loop), **Shimmer** (the hall with an octave-up shifter fed back, the
  octave builds in the tail), **Gated** (short and dense, closed after the
  hit), **Texture** (a granular cloud after Clouds / Beads: four grains read
  the recent send at random places, some an octave up or down, fed back and
  diffused), **Plate** (Jon Dattorro's plate, the network of Valley's Plateau,
  its tank at half rate), **Room** (after Dragonfly Room: early reflections and
  a short dense tail). Size, Damping, Predelay and Diffusion drive all of them.
- **Master Reverb 2/2** (Shift on Master Reverb): **Low cut** (Off, 20 Hz ..
  1 kHz, a 12 dB/oct high-pass on the send) and **Width** (0..150 %) for every
  model, and two character values per model (50 = the model as designed):
  Hall Mod depth / Mod rate, Shimmer Shimmer / Mod depth, Gated Hold /
  Release, Texture Grain / Octaves, Plate Mod depth / Bandwidth, Room Early /
  Mod depth. Saved with the project (`otfork.bin`), each model keeps its own.
- **Master colour** (the Saturation list on Master 1/5 and 4/5 after the stock
  modes, or Config > Master colour, the same setting): **ToTape**,
  **Console**, **Density**,
  **PurestDrive** in the manner of Airwindows (MIT): ToTape a sine curve with
  a head bump and a soft top, Console the Spiral curve, Density and
  PurestDrive their sine blends. A colour replaces the stock mode (it does not
  run on top of it); Drive (Master 4/5) sets how hard it is hit, Mix and Tone
  work on it.
- **Delay models** (Config > Delay model): Stock, **Tape** (wow and flutter
  on the read head, repeats that saturate and darken), **Lo-fi** (repeats that
  lose bits and bandwidth). Time, Sync, Feedback, Filter and Ping-pong drive
  all of them.
- Both run in the stock effects' memory. Cost in the bench's busiest set
  (est. share of the audio block over the stock effect): Hall +1.6 %,
  Shimmer +5.2 %, Gated +2.6 %, Tape +1.8 %, Lo-fi +2.9 %, Texture +3.0 %,
  Plate +4.3 %, Room +4.9 %, colour +1.3 %; Low cut and Width are within the
  noise.

- **Master 5/5 Sidechain**: two sidechains, **SC 1** and **SC 2** (F1 switches),
  each with its own source, envelope and targets, saved with the project
  (`otfork.bin` next to `project.mt`; stock firmware ignores it).
  - **Source** (F2): Track 1..16 or Line in L / R. **Mode** (F3): **Trigger**
    (a note on the source track starts the envelope: sample, synth or MIDI
    notes, so a muted kick or a MIDI track can drive it) or **Follow** (the
    source's audio above the **Threshold**, F8, -60..0 dB; tracks 1..8 and
    the line input).
  - Envelope: **Attack** 0..100 ms, **Hold** 0..500 ms, **Release** 10 ms..1 s,
    **Depth** 0..100 % (F4..F7).
  - **Targets** (the pads): the top row toggles tracks 1..8 (pads 1..8) and
    synth slots 1..3 (pads 9..11); the second row Delay return, Reverb return,
    Line in (pads 13..15). Lit pads are the selected SC's targets, dim ones the
    other's. **Shift + pad 1..8** sets the source to that track. Nothing is
    ducked unless it is a target; a target of both SCs gets both.
  - The left column shows both envelopes live and the 14 targets with a dot
    per SC.
  - The stock Limiter sidechain (Master Limiter) is unchanged.
  - Cost: nothing while off; a trigger SC ducking four tracks about 0.7 % of
    the audio block; the bench's worst case (both SCs following audio, all 14
    targets) about 4 %.

## Instruments and synths

- **LFO shapes** (Instrument Parameters, an LFO's Shape): after the stock five,
  **Rungler** (Rob Hordijk's 8-bit register and 3-bit DAC, stepped),
  **Gingerbrd** (the Gingerbread-man map, smoothed), **Rossler** (the Rossler
  attractor, about one orbit per period), **Drift** (an Ornstein-Uhlenbeck walk
  pulled back to the middle), **Bridge** (a Brownian bridge: a random path that
  starts and ends in the middle every period). Kept in the .pti's spare LFO
  byte; a stock firmware plays them as Random. On the volume target they act
  as Random.
- **PLT synth model** (Synth 1-3, Model after PERC): two models in the manner
  of Mutable Instruments Plaits: **2-Op FM** (Harmonics = the ratio, Timbre =
  the index, Morph = feedback: below the middle the modulator on itself, above
  the carrier into the modulator) and **Harmonic** (16 partials: Harmonics =
  the strongest one, Timbre = the width of the bump, Morph = odd / even).
  Timbre Env and an LFO onto Timbre, ADSR, Tune, Glide, Volume, macros like the
  other models. Patches in Patches/PLT. A Tracker Mini resets such a slot.

## Config

- **General > Display theme**: 29 built-in themes, then the theme files of the
  card's `Themes` folder (2.2.3; **Export theme** saves the current one, a fork
  palette too, as a file to edit). Original, Monochrome, Anniversary,
  then Early Cyber, Corporate, Amber CRT, Green Phosphor, Thermal, Workstation,
  Datamatic, Dubplate, Rave Flyer, Jungle, Ice, Sunset, Teletext, Vaporwave,
  Blueprint, Game Boy, Sepia, Hi Contrast, and two animated ones: **Cyber Pulse**
  (accent and cursor breathe) and **Spectrum** (the hue turns), and six more
  animated: **Acid House** (the accent pumps yellow to pink on a 120 BPM beat),
  **Plasma** (the background drifts purple, blue, teal), **Colour Wheel** (the
  four column colours rotate), **Ember** (accent and cursor flicker like a
  fire), **Night Drive** (synthwave: the background pulses with the sun
  colour), **Glitch** (a grey page with rare short colour flashes). The new ones
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
  model / Master colour**: see above.
- All of these live in the OpenTracker config extension record (24 bytes);
  the Perform scenes in a record of their own after it.

## Mixer, Song, Pattern

- **Global Mixer 3/5**: the synth columns mute / unmute with a tap; Shift solos
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
budget is **860,000 B** (`tools/telemetry_check.py`; upstream 720,000). The
hard limit is the stock 1.9.2 image extent, 0xD6460 = 877,664 B, which
`tools/check_image.py` checks on every build: nothing may be written past it.
The image is about 840 KB (2.2.3 alone is about 684 KB).

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
selected), wavetable FM (only with WT FM Depth on), the sidechains (only
when set), the master colour, DeRez, the chaos LFO shapes, the PLT engine and
track links (only when set).
`make perf-gate` has bench scenarios for them (wHall .. wColor, wSidechain,
pltfm / pltharm).

Sources: Mutable Instruments Grids drum map (Emilie Gillet, GPL-3.0; the
node table in src/core/gen/grids_nodes.inc), ideas after Marbles, Plaits,
Clouds / Beads (MIT), Jon Dattorro's plate (JAES 1997) as in Valley's
Plateau, Dragonfly Room, Airwindows (Chris Johnson, MIT), HetrickCV /
Nonlinear Circuits (CC0) and ZetaCarinae, Noise86 MarkovDrummer, SickoCV
randLoops, Music Thing Turing Machine, Paul Nasca's Paulstretch, Ourednik's
Argotlunar, Leviathan's Strand, YIN (de Cheveigne and Kawahara 2002).
TB-3PO idea (Phazerville Hemisphere / charlesvestal schwung-tb3po),
Nordl3 Call / Response Generator (MIT), bencodec/BBenCut and Nick Collins'
BBCut, ITU-T G.711 / G.726, Silicon Labs AN230 (RDS registers), Departure Mono
by Helena Zhang (OFL-1.1, third_party/fonts).
