# OpenTracker

> **This is the semi fork (2.2.1-semi).** What it adds is listed right below, under
> [New in 2.2.1-semi](#new-in-221-semi-this-fork); the full guide is **[FORK.md](FORK.md)**.
> Polyend Backstage feature requests (all 254 Tracker topics) and what this fork can do about them:
> **[FEATURE_REQUESTS.md](FEATURE_REQUESTS.md)**, the doable ones with notes: **[YAPILABILIR.md](YAPILABILIR.md)**.

OpenTracker is an open-source, clean-room firmware for the **original Polyend Tracker**,
written from behavioural specifications. It contains no Polyend code, fonts, icons or
graphics, and is not affiliated with or endorsed by Polyend.

## New in 2.2.1-semi (this fork)

Image: `PolyendTrackerOTs221.ptf` (installs like the upstream image, see Install). Everything
the generators write is ordinary steps and step FX, so the patterns also play on stock 1.9.x
and on the Tracker Mini. Full guide with every parameter: **[FORK.md](FORK.md)**.

### Latest update (2.2.1-semi.6)

| **Track Link: a track plays live with another track's notes** | **Linked track: "<1" on its header** |
|---|---|
| ![Track Link popup](screenshots/2.2.1-semi/link-popup.png) | ![Linked track in the grid](screenshots/2.2.1-semi/link-grid.png) |

- **Track Link** (Pattern page 3, F4): a track follows another track live. Whenever the source
  plays a note (steps, rolls, play modes, Mutate, live Grids, pads), the linked track plays
  its own instrument at the same moment: Follow or fixed note, transpose, level, chance, FX
  copy. Mute the source and only the layer is heard. Saved with the project.

### Previous update (2.2.1-semi.5)

| **Generate: Grids (Mutable drum map, live)** | **Generate: Marbles (deja vu)** |
|---|---|
| ![Generate: Grids](screenshots/2.2.1-semi/gen-grids.png) | ![Generate: Marbles](screenshots/2.2.1-semi/gen-marbles.png) |
| **Master 4/5: colour (ToTape, Console, Density, PurestDrive)** | **Master Reverb: Texture, Plate, Room** |
| ![Master colour](screenshots/2.2.1-semi/master-colour.png) | ![Master Reverb: Plate](screenshots/2.2.1-semi/reverb-plate.png) |
| **Sample Editor: Tuner, Paulstretch, Granular, Strand** | **Tuner: note, cents, Write sets Tune / Finetune** |
| ![Sample Editor: new tools](screenshots/2.2.1-semi/editor-tuner-list.png) | ![Tuner](screenshots/2.2.1-semi/editor-tuner.png) |

- **Generate: Grids** (Mutable Instruments Grids' drum map, GPL-3.0): kick, snare and hats from
  an X / Y point between 25 patterns, with densities, Chaos, Accent, 1/16 or 1/32 and a kit on
  three tracks. The kit stays **live**: Perform **Grids X / Y / Density** redraw it as it plays.
- **Generate: Marbles:** random notes with deja vu: a T generator (Coin, Clusters, Drums, two
  tracks) and an X generator (Spread, Bias, Steps from chromatic to root only, Range).
- **Genotype Drummer** (after Noise86's MarkovDrummer): a Markov chain over all the selected
  drum tracks together, so the parts keep their relation.
- **Play modes M1..M3** (Marbles deja vu), **TM8 / TM16** (Turing Machine) and **R1 / R2**
  (randLoops after SickoCV: the notes drift by scale degrees); the Perform FX **Loop Lock**
  locks or loosens them all.
- **Perform DeRez:** sample-rate reduction on the tracks' samples (after Airwindows DeRez).
- **Master Reverb models Texture** (granular cloud after Clouds / Beads), **Plate** (Dattorro's
  plate, the network of Valley's Plateau) and **Room** (after Dragonfly Room).
- **Master colour** (after Airwindows): ToTape, Console, Density, PurestDrive, in the Master 4/5
  Saturation list or Config > Master colour.
- **Sample Editor:** **Tuner** (YIN pitch detection, writes Tune / Finetune), **Paulstretch**,
  **Granular** (after Argotlunar) and **Strand** (after Leviathan's Strand).
- **LFO shapes** Rungler, Gingerbread, Rossler, Drift (Ornstein-Uhlenbeck) and Bridge (Brownian
  bridge); a stock firmware plays them as Random.
- **PLT synth model** (Synth 1-3): **2-Op FM** and **Harmonic** in the manner of Plaits.

### Earlier update (2.2.1-semi.4)

| **Master 5/5: Sidechain (two sources, targets on the pads)** | **Theme: Acid House (animated)** |
|---|---|
| ![Master 5/5: Sidechain](screenshots/2.2.1-semi/master-sidechain.png) | ![Theme: Acid House](screenshots/2.2.1-semi/theme-acidhouse.png) |
| **Theme: Plasma (animated)** | **Theme: Colour Wheel (animated)** |
| ![Theme: Plasma](screenshots/2.2.1-semi/theme-plasma.png) | ![Theme: Colour Wheel](screenshots/2.2.1-semi/theme-colourwheel.png) |
| **Theme: Ember (animated)** | **Theme: Night Drive (animated)** |
| ![Theme: Ember](screenshots/2.2.1-semi/theme-ember.png) | ![Theme: Night Drive](screenshots/2.2.1-semi/theme-nightdrive.png) |
| **Theme: Glitch (animated)** | |
| ![Theme: Glitch](screenshots/2.2.1-semi/theme-glitch.png) | |

- **Master 5/5 Sidechain:** two sidechains (SC 1 / SC 2), each with its own source (track 1..16
  or line in L / R), mode (**Trigger** on notes, **Follow** on audio with a threshold),
  attack / hold / release / depth, and its own **targets**: tracks 1..8, synth slots 1..3,
  delay and reverb returns, line in. The pads toggle the targets, Shift + pad picks the source
  track; nothing is ducked unless it is a target. Saved with the project (`otfork.bin`).
- **BBCut Mutate / random seeds:** every re-cut takes a fresh seed, the Perform BBCut A / B
  offsets steer the next cuts, the first play after boot re-cuts, and the Generate popup's
  random draws differ after every boot.
- **Call+Resp:** the form repeats over the whole selection (odd lengths too, 3 x 10 steps say),
  repeats swap the two answers, Auto picks 32 / 16 / 8 / 4 / 2 steps per part, and the default
  answer is "Q + closure".
- **Six new animated themes:** Acid House, Plasma, Colour Wheel, Ember, Night Drive, Glitch
  (29 themes in all).

### Earlier update (2.2.1-semi.3)

| **Perform: generator controls, Genotype G2, wavetable FM** | **Perform: FX chooser (Shift + encoder) with help** |
|---|---|
| ![Perform: generator controls](screenshots/2.2.1-semi/perf-generator-fx.png) | ![Perform: FX chooser](screenshots/2.2.1-semi/perf-chooser.png) |
| **Perform: scenes (Fx1 recalls, Fx2 stores)** | **Master 4/4: Dynamics** |
| ![Perform: scenes](screenshots/2.2.1-semi/perf-scenes.png) | ![Master 4/4: Dynamics](screenshots/2.2.1-semi/master-dynamics.png) |
| **Master Reverb: Hall / Shimmer / Gated models** | **Shift lock tag and title level meter** |
| ![Master Reverb: models](screenshots/2.2.1-semi/master-reverb-model.png) | ![Shift lock and level meter](screenshots/2.2.1-semi/shift-lock-meter.png) |
| **Config: Scale lock, Screen saver, Level meter, effect models** | |
| ![Config: new options](screenshots/2.2.1-semi/cfg-new-options.png) | |

- **Shift lock:** tap Shift twice; a SHIFT tag shows until the next Shift press.
- **Scale lock:** the encoder moves notes through the project scale.
- **Pattern dashes:** drawn pixel-exact, every row shows its column colours on the device.
- **Perform:** generator controls (Acid Accent / Slide / Mutate, BBCut A / B / Rolls / Recut),
  **Genotype** play modes G1..G3, **wavetable FM** between tracks (WT FM Depth / Source), an FX
  chooser with help (Shift + encoder in edit mode), **scenes** with morph (Fx1 / Fx2 + F1..F8)
  and **Perform recording** into FX 2 (Insert).
- **Master 4/4 Dynamics:** limiter mode, stereo link, ceiling; saturation drive, mix, tone.
- **Reverb models** Hall, Shimmer, Gated and **delay models** Tape, Lo-fi.
- **Screen saver** (the Data screen) and a **title level meter**.

### Screenshots (2.2.1-semi, simulator)

| **Generate: Acid, live preview on the right** | **Generate: Acid, page 2 (Shift)** |
|---|---|
| ![Generate: Acid, live preview on the right](screenshots/2.2.1-semi/gen-acid.png) | ![Generate: Acid, page 2 (Shift)](screenshots/2.2.1-semi/gen-acid-page2.png) |
| **Generate: Call+Resp (Form, C / R tags)** | **Generate: BBCut (Jungle, Mutate)** |
| ![Generate: Call+Resp (Form, C / R tags)](screenshots/2.2.1-semi/gen-callresp.png) | ![Generate: BBCut (Jungle, Mutate)](screenshots/2.2.1-semi/gen-bbcut.png) |
| **Generate: BBCut page 2 (Rolls, Pitch, Timing, Dynamics)** | **Generate: Turnaround (FX Soft / Medium / Hard)** |
| ![Generate: BBCut page 2 (Rolls, Pitch, Timing, Dynamics)](screenshots/2.2.1-semi/gen-bbcut-page2.png) | ![Generate: Turnaround (FX Soft / Medium / Hard)](screenshots/2.2.1-semi/gen-turnaround.png) |
| **Generate: Genotype** | **Generate: Station (what the radio gives)** |
| ![Generate: Genotype](screenshots/2.2.1-semi/gen-genotype.png) | ![Generate: Station (what the radio gives)](screenshots/2.2.1-semi/gen-station.png) |
| **Six generated tracks** | **Acid line on the ACD synth (Glide, accents)** |
| ![Six generated tracks](screenshots/2.2.1-semi/gen-multitrack.png) | ![Acid line on the ACD synth (Glide, accents)](screenshots/2.2.1-semi/acid-result.png) |
| **Data screen: Barcode** | **Data screen: Matrix, Tracks colour** |
| ![Data screen: Barcode](screenshots/2.2.1-semi/data-barcode.png) | ![Data screen: Matrix, Tracks colour](screenshots/2.2.1-semi/data-matrix.png) |
| **Data screen: vertical view** | **Data screen: Rain** |
| ![Data screen: vertical view](screenshots/2.2.1-semi/data-vertical.png) | ![Data screen: Rain](screenshots/2.2.1-semi/data-rain.png) |
| **Data screen: Pulse** | **Display font: Pixel (Departure Mono)** |
| ![Data screen: Pulse](screenshots/2.2.1-semi/data-pulse.png) | ![Display font: Pixel (Departure Mono)](screenshots/2.2.1-semi/font-pixel.png) |
| **Theme: Vaporwave** | **Theme: Game Boy** |
| ![Theme: Vaporwave](screenshots/2.2.1-semi/theme-vaporwave.png) | ![Theme: Game Boy](screenshots/2.2.1-semi/theme-gameboy.png) |
| **Theme: Blueprint** | **Theme: Spectrum (animated)** |
| ![Theme: Blueprint](screenshots/2.2.1-semi/theme-blueprint.png) | ![Theme: Spectrum (animated)](screenshots/2.2.1-semi/theme-spectrum.png) |
| **Sample Editor: Transcode** | **Transcode applied (G.726, 8 generations, bit errors)** |
| ![Sample Editor: Transcode](screenshots/2.2.1-semi/fx-transcode.png) | ![Transcode applied (G.726, 8 generations, bit errors)](screenshots/2.2.1-semi/fx-transcode-applied.png) |
| **Sample Editor: DCT Crush** | **Theme: Corporate** |
| ![Sample Editor: DCT Crush](screenshots/2.2.1-semi/fx-dct.png) | ![Theme: Corporate](screenshots/2.2.1-semi/theme-corporate.png) |
| **Theme: Amber CRT** | **Theme: Green Phosphor** |
| ![Theme: Amber CRT](screenshots/2.2.1-semi/theme-amber.png) | ![Theme: Green Phosphor](screenshots/2.2.1-semi/theme-green.png) |
| **Theme: Datamatic** | **Theme: Thermal** |
| ![Theme: Datamatic](screenshots/2.2.1-semi/theme-datamatic.png) | ![Theme: Thermal](screenshots/2.2.1-semi/theme-thermal.png) |
| **Theme: Rave Flyer** | **Theme: Jungle** |
| ![Theme: Rave Flyer](screenshots/2.2.1-semi/theme-rave.png) | ![Theme: Jungle](screenshots/2.2.1-semi/theme-jungle.png) |

### Generate (Pattern, page 3, F3, edit mode on)
A Fill-style popup: F1 picks the generator, F2..F7 its parameters, **Shift** the second page,
F8 writes the result into the selection (or the cursor track). The right column is a **live
preview** of exactly what F8 writes; the encoder push draws a new random result, and a strip
explains the selected parameter. One undo step per Generate; Seed 0 is random.
- **Acid:** a 303 line in the TB-3PO manner. Density -7..+7 adds or removes notes in a fixed
  order, so the melody stays; accent, slide, range, root, gate. The line stays **live**: the
  Perform FX **Acid Density** moves its density, **Mutate** re-rolls it every 1 / 2 / 4 / 8 loops.
- **Call+Resp:** call and answer after Nordl3's generator (46 styles, 38 answers) with a Form
  (C R, C R C R, C C R, ...), steps per part, ending, answer octave and motif length. **Two
  tracks** puts the call on one track and the answer on the next, taking turns.
- **BBCut:** Nick Collins' procedures on a sliced break: Classic, ChooseCut, **Jungle**, WarpCut,
  SQPusher1/2, CageCut, Timeline, Permute, ThrashCut. Rolls end cleanly where they stop; roll
  style, per-hit pitch (±1..3), timing and dynamics are selectable, and **Mutate** gives new cuts
  every N loops while it plays.
- **Turnaround:** phrase-end rolls, stutters, drops, reverse and builds on a budget, with an FX
  strength (Soft / Medium / Hard) and tidy timing.
- **Genotype:** learns the selected steps and plays a variation made only of them.
- **Station:** a numbers-station line read from the radio (RDS name, PI, frequency); the
  preview shows what it read and the digits.
- **Grids:** the Mutable Instruments Grids drum map (X / Y, densities, Chaos, Accent, kit on
  three tracks), live under the Perform FX Grids X / Y / Density.
- **Marbles:** random notes with deja vu (T and X generators, Steps, Range, two-track mode).
- Genotype mode **Drummer**: a Markov chain over all the selected drum tracks together.
- **Track Link** (F4 on the same page): the cursor track plays live with another track's notes,
  its own instrument (note, transpose, level, chance, FX copy).

### Live (Performance)
- **Play modes B1..B10** (BBCut), **T1..T4** (TILT: Bounce, Flipper, Gravity, Multiball) and
  **G1..G3** (Genotype: the track's own steps in a new order every loop), **M1..M3** (Marbles
  deja vu), **TM8 / TM16** (Turing Machine) and **R1 / R2** (randLoops) in "Pattern Play Mode";
  **Loop Lock** locks or loosens M, TM and R.
- **Generator controls as Perform FX:** Acid Density / Accent / Slide / Mutate on the live Acid
  tracks, BBCut A / B / Rolls / Recut on the B modes, and **WT FM Depth / Source**: wavetable
  tracks frequency-modulated by another track, sample by sample. **Grids X / Y / Density** on
  live Grids kits, **DeRez** (sample-rate reduction).
- **FX chooser:** Shift + encoder in edit mode opens every Perform FX as a grid, with help.
- **Scenes:** Fx2 + F1..F8 stores, Fx1 + F1..F8 recalls, with a morph of 1/4 to 4 bars.
- **Perform recording:** Insert writes the playing Perform FX into FX 2 of the pattern.

### Master
- **Master 4/5 Dynamics:** limiter mode, stereo link and ceiling, saturation drive, mix and tone.
- **Master 5/5 Sidechain:** two sidechains with their own source, envelope and targets (pads).
- **Reverb models** Hall, Shimmer, Gated, Texture, Plate, Room and **delay models** Tape, Lo-fi
  (in the stock effects' memory, a few percent of the audio block more than stock).
- **Master colour** after Airwindows: ToTape, Console, Density, PurestDrive.

### Sample Editor effects
- **Transcode:** G.711 / G.726 codec generation loss with bit errors and packet loss.
- **DCT Crush:** spectral block crush.
- **Radio** (CIPHER): shortwave band, QSB fading, heterodyne whistle, noise and static crashes.
- **Clipper** (NEXUSCLIP): seven clip curves with drive, knee, ceiling and level match.
- **Tuner** (YIN; writes Tune / Finetune), **Paulstretch**, **Granular** (after Argotlunar),
  **Strand** (after Leviathan's Strand).

### Instruments and synths
- **LFO shapes** Rungler, Gingerbread, Rossler, Drift, Bridge (chaos and random walks).
- **PLT synth model:** 2-Op FM and Harmonic (16 partials) in the manner of Plaits.

### Config, Mixer, Song, Pattern
- **Shift lock** (double tap Shift), **Scale lock** (notes step through the project scale),
  **screen saver** (the Data screen), **title level meter**.
- **29 display themes** (26 new), eight of them animated (Cyber Pulse, Spectrum, Acid House,
  Plasma, Colour Wheel, Ember, Night Drive, Glitch), and a third
  display font, **Pixel** (Departure Mono, OFL).
- **Auto slice: Breaks** (finds every hit of a fast break) and **Beat Slice thru** (a Beat
  Slice note plays on past its slice).
- **Global Mixer:** synth slots mute (tap) and solo (Shift). **Song:** hold Note / Instr / Fx1 /
  Fx2 to colour the blocks. **Pattern:** the FX list opens at the last effect used in the column.

### Radio and Data screen
- **RDS:** the station name shows in the Sample Recorder title. **Shift + seek** scans the band.
- **Data screen** (File, F7): Barcode, Matrix, Numbers, Rain and Pulse, horizontal or vertical,
  in three colour modes; only the tracks with notes, over the whole screen. Pads mute tracks,
  the encoder sets the tempo.

### Build
- The fork builds without Games by default. Its flash budget is 800 KB (upstream 720 KB); the
  hard limit, the stock 1.9.2 image extent (877,664 B), is checked on every build. Image size
  about 792 KB; all host tests and simulator checks pass.

## Screenshots (2.2.1, on an original Tracker)

| Pattern (synth drums, FX) | 8-track view (Pattern + Note) |
|---|---|
| ![Pattern](screenshots/2.2.1/pattern.png) | ![8-track view](screenshots/2.2.1/pattern-8-tracks.png) |
| **Synth Parameters (ACD)** | **Edit Patch (PERC kit)** |
| ![Synth Parameters](screenshots/2.2.1/synth-parameters.png) | ![Edit Patch](screenshots/2.2.1/edit-patch-perc.png) |
| **Sample Editor, stereo sample** | **Live resampling** |
| ![Sample Editor](screenshots/2.2.1/sample-editor-stereo.png) | ![Live resampling](screenshots/2.2.1/live-resampling.png) |
| **Performance FX** | **Config: USB Storage** |
| ![Performance](screenshots/2.2.1/performance.png) | ![Config, USB Storage](screenshots/2.0.1/config-usb-storage.png) |

2.x brings the newer Tracker Mini features to the original hardware: 16 tracks, synth
engines, stereo samples, Mini-compatible project files and USB Storage mode. These notes cover
the whole 2.x line (2.0 to 2.2.1); the full list is in the release notes of the
[2.2.1 download](https://github.com/sandlbn/polyend-tracker-re/releases).

## New in 2.1 to 2.2.1

- **All five Mini synth models play:** FAT (2.1) and WTFM (2.2) joined ACD, VAP and PERC, with
  74 presets of our own in the preset pack (`OpenTracker-Presets-1.2`).
- **Live resampling** (2.1.2, contributed by syamajala): record the mix, one track or Line In
  into a new instrument while the pattern keeps playing.
- **Waveforms like the Tracker+ / Mini:** sample envelopes, stereo samples as two lanes, and a
  preview playhead on the Sample Recorder and Sample Editor.
- **Closer to the stock manual:** about 30 details fixed after a check against the stock 1.9.2
  manual (live-record velocity, Slice-mode pads, Performance labels, MIDI CC lists and more).
- **User-report fixes:** live recording (first note with Pre-roll, OFF steps, red cursor), MIDI
  channels on the pads, a crack in the headphones at power-on, headphone level +1 dB.
- **More free memory:** Games is no longer built into the image (`make OT_GAMES=1` brings it
  back), and the file browsers share one buffer (PR #28, syamajala).

## New in 2.0

### Tracker Mini project compatibility
- Projects are saved in the **Tracker Mini 2.2.1 format**: 16-track patterns, the three
  synth slots, track names and mutes for tracks 13–16.
- **Synth patches** use the Mini's `.patch` files. They are stored with the project and in a
  library at `Patches/<ENGINE>/`. All 299 Mini factory patches load and save back unchanged.
- Older original-Tracker projects open as they are, with tracks 13–16 empty.

### 16 tracks
- Tracks 13–16 work like MIDI tracks 9–12: notes, MIDI channels, CC A–F, pitch bend,
  velocity, mute and solo. Performance Play mode, Pattern length and Step repeater act on them
  too (no voice FX, levels or per-track patterns, as on the Mini).
- Pattern pages through all 16 tracks. Song shows a scrolling 12-track window.
- Fill, copy and paste work across all 16 tracks.

### Synth engines (instruments S1–S3)
- **Three synth slots**, played from any track as instruments S1–S3.
- Five engines, our own sound design following the Mini's parameter sets:
  - **ACD**: acid bass and lead, with a 24 dB and a ladder-style filter, and glide;
  - **VAP**: virtual-analog poly, with morphing oscillators, hard sync, 15 filter types and a
    6-slot modulation matrix;
  - **PERC**: drum kit with kick, snare, hats, cymbal, perc and tom;
  - **FAT** (2.1): a stack of detuned saws for supersaw pads, leads and basses;
  - **WTFM** (2.2): two-operator wavetable FM, from electric pianos to bells.
- **Macros:** FX columns A–F drive macros 1–6, and Volume drives macro 7.
- **Voices:** a shared pool of 4 voices by default, adjustable in Config > General > Synth
  voices (1–8).
- **Screens, after the Mini's layout:**
  - Synth Parameters: model, polyphony, gain, pan, sends.
  - Synth Macros.
  - Edit Patch: sections on the pads; Save, Save As, Init.
  - Load Patch popup with Preview.
  - Synth 1–3 level columns with meters on Global Mixer page 3.
- Exports include separate stems `synth_1..3.wav`.

### Stereo samples
- Stereo WAVs keep both channels on import.
- They play in stereo in one-shot, loop, slice and beat-slice modes. Pan works as balance.

### Performance and monitoring
- **CPU meter** in the title bar (Config > General > CPU meter: Auto, Always or Off).
- **Voice limiter:** if the audio gets close to its limit, only synth voices are cut; sample
  tracks never are.
- **Faster loading:** project loading does about half the work in our host model (not yet
  timed on the device).
- **Smoother display:** frames go to the screen over DMA without waiting.
- **More code in fast internal RAM:** the hottest audio and synth code runs from it.
- **USB serial diagnostics:** CPU, peak breakdown, display, SD and radio/recorder lines.

### USB Storage mode (2.0.1)
- **Config > USB > USB Storage: On** shows the SD card on a connected computer as a USB drive,
  as on the Tracker Mini. Copy projects, samples and patches without taking the card out.
- Playback stops and the project is saved first. Keep the power and the USB cable connected
  while files are transferred.
- To finish: eject the drive on the computer, then press **Exit** (or set the row back to
  **Off**). The project reloads from the card. If the computer still uses the drive, Exit
  first reminds you to eject it.
- The setting is always **Off** after power-on.
- About 1 MB/s (full-speed USB; measured 1,033 KB/s on a real Tracker). Tested with Linux
  and macOS; Linux can take a few seconds longer to notice the drive. USB MIDI and the
  serial log keep working.
- If the cable is pulled during a transfer, the Tracker recovers within half a second.
- The drive has a stable serial number, so a computer keeps recognising the same Tracker.
  On Windows, the Tracker may appear once as a new device (new COM/MIDI port).

## Install

1. Copy `PolyendTrackerOTv221.ptf` (this fork: `PolyendTrackerOTs221.ptf`) to `/Firmware` on the SD card. Keep the name; the
   bootloader only accepts 24-character names.
2. Go to **Config > Firmware update**, select it and confirm. If the screen stays dark
   after the update, press the power button once.

**Going back:** keep the stock `PolyendTracker_1.9.2.ptf` in `/Firmware`. Install it the
same way, or put its file name into `/Firmware/ot_restore.txt` and power-cycle. Keep only the
image you want, plus stock, in `/Firmware`: the rear reset button installs a random `.ptf`
from that folder.

**Note:** projects saved by 2.x use the Mini's 16-track patterns. Stock firmware for the
original Tracker reads only tracks 1–8 of those patterns (expected from the stock loader's
rules; not yet confirmed on a stock unit).

## Known limitations

- The synth sound is own design: settings match the Mini, the sound is close but not
  identical.
- The synth voice pool defaults to 4 because of the processor's speed.
- USB Storage mode: back up the card before first use; Linux systems without a desktop may need
  `partprobe` or a re-plug to see the card.
- The USB ID is the pid.codes test ID 1209:0001.
- Live resampling from Line In starts 10–14 ms late (the round trip through external gear);
  compensation is planned.
- Games (the NES player) is not in the release image since 2.2.1.

## Test songs

The package's `Projects/` folder holds eleven OT Test songs (among them one per synth engine
and an IDM track using three engines at once) and seven BLAST GRID synth remixes
(Title, World 1, World 4, Battle, Battle FM, Boss, Final Boss). Copy them to `/Projects` on
the card.

Downloads: https://github.com/sandlbn/polyend-tracker-re/releases
