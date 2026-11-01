# RiddimForge

A reggae instrument plugin for **Logic Pro, LUNA, Cubase and FL Studio on Mac** (AU + VST3, plus a standalone app).
It has two sampled drum kits and 13 recorded instruments cut from your own sessions, built into the plugin itself,
plus synthesized alternatives for every part, so there's no sample library to install.

**What's inside**

| Part | Sounds |
|---|---|
| **Sampled kits (Drums tab)** | **Studio Kit**: multi-mic kit from your first session (Kick In/Out, Snare Top/Bottom, Hi-Hat, 3 toms, 2 rototoms, timbales, tambourine, crashes, bleed-free overheads, room). **Kit 2**: punchy one-drop kit from "Reggae Instrumental 2" (kick, cross-stick, snare, hats, toms, crash, shaker, jam block, tambourine). Velocity layers + round robin, Handy Drums-style mic mixer, replace any drum with your own sample |
| **Recorded instruments** | From your stems: Session Bass, 5-String Fingered bass; Skanky, Chop and Clean guitars; Bubble, Perc and Organ 4 organs; Session and Acoustic pianos; Lead Guitar and two Pluck Guitars. Pick them in each part's **Type** menu (marked "(recorded)") |
| **Generator tab** | Drum Monkey / Unison-style pattern generator: 1–16 bars of drums with fills, bass lines, skank guitar, organ bubble, piano and lead melodies, each with its own style type and complexity, drag-to-DAW per part |
| Drum kit | Kick, boom kick, cross-stick (one-drop "tok"), high-tuned reggae snare, rimshot, clap, 6 toms, hi-hats (closed / pedal / open with choke), crash ×2, splash, china, ride ×2, ride bell, syndrum |
| Timbales & percussion | Timbales hi/lo, timbale rim (paila) & shell, bongos, congas (mute / open / low / slap), cowbell, tambourine, shaker, maracas, cabasa, guiro short/long, claves, wood blocks, agogo, triangle |
| Nyabinghi | Bass (akete), funde, repeater, repeater slap |
| Dub FX | Dub siren, air horn, laser zap, syndrum pew |
| Skank guitar | Plucked-string (Karplus-Strong) chop with palm-mute and tone |
| Piano | Grand, electric (FM), honky-tonk |
| Organ | Tonewheel drawbars (Bubble, Jazz 888, Full, Flute), key click, percussion, rotary speaker |
| Bass | Roots sub, dub, digital 80s (Sleng Teng-style), finger |
| Lead | Melodica, square lead, synth horns, bubble synth, whistle |
| Pad | Strings, warm pad, choir, glass |
| Riddim sequencer | One Drop, Rockers, Steppers, Rocksteady, Ska, Lovers Rock, Nyabinghi, Dancehall Dembow, with fills, swing and humanize |
| Chords | 14 progressions in any key, or play chords live on MIDI channel 16 and the skank, bubble, piano, bass and pad follow |
| Mixer & FX | Level/pan/echo/reverb per part, ping-pong tape echo synced to tempo, reverb, one-knob dub filter |
| MIDI export | Export or drag the riddim straight into your DAW as editable MIDI |

---

## 1. Build it on your Mac (one time, ~10 minutes)

You need three free things:

1. **Xcode** from the Mac App Store. Open it once so it finishes installing components.
2. **Command line tools**: open Terminal and run `xcode-select --install`
3. **CMake**: download the macOS installer from cmake.org, or with Homebrew: `brew install cmake`

Then in Terminal (drag the `RiddimForge` folder onto the Terminal window to paste its path after `cd `):

```bash
cd ~/Downloads/RiddimForge
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 8
```

The first run downloads JUCE 9.0.4 (the audio framework) from GitHub, so you need internet. It builds a
**universal binary** (Apple Silicon + Intel, macOS 11+) and copies the plugins into place automatically:

| Format | Installed to | Used by |
|---|---|---|
| AU | `~/Library/Audio/Plug-Ins/Components/RiddimForge.component` | Logic Pro, LUNA, FL Studio |
| VST3 | `~/Library/Audio/Plug-Ins/VST3/RiddimForge.vst3` | Cubase, FL Studio, LUNA |
| Standalone app | `build/RiddimForge_artefacts/Release/Standalone/RiddimForge.app` | Try it without a DAW |

Prefer working in Xcode? Use `cmake -B build -G Xcode`, open `build/RiddimForge.xcodeproj`, pick the
`RiddimForge_All` scheme and build.

### Make your DAW see it

- **Logic Pro**: open Logic → Settings → Plug-in Manager, find RiddimForge, click *Reset & Rescan Selection*.
  Then on a Software Instrument track: Instrument slot → AU Instruments → RiddimForge.
  You can check validation in Terminal with `auval -v aumu Rfg1 Ridm`.
- **LUNA**: Settings → Plugins → Rescan. Add it on an Instrument track.
- **Cubase**: Studio → VST Plug-in Manager → Rescan. Add it on an Instrument track.
- **FL Studio**: Options → Manage plugins → Find plugins (tick "Rescan"). It appears under Generators.

If Logic reports a validation failure, re-sign the component and rescan:

```bash
codesign --force --deep -s - ~/Library/Audio/Plug-Ins/Components/RiddimForge.component
killall -9 AudioComponentRegistrar
```

---

## 2. Using it

### Instant riddim
Load it, press play in your DAW. With **RIDDIM** on and the clock on **Host Sync**, it plays the selected style
locked to your DAW's tempo and bar position. Switch the clock to **Free Run** to jam without a transport
(or in the standalone app).

On the **Riddim** tab pick the style, key, chord progression and chord length, then use the green buttons
to choose which parts the riddim plays. Swing (0–20% is a straight-ish roots feel, 40%+ shuffles), Humanize
adds small timing and velocity variation, and Fills drops a timbale or tom roll every 4 or 8 bars.

### Play it yourself
| MIDI channel | Plays |
|---|---|
| 1 | Whatever **"MIDI Ch 1 / Keyboard Plays"** is set to (default: Drums & Perc) |
| 2 | Skank guitar |
| 3 | Piano |
| 4 | Organ |
| 5 | Bass |
| 6 | Lead |
| 7 | Pad |
| 10 | Drums & percussion (map below) |
| 16 | Chord input for the riddim (set Chord Source to "MIDI Ch 16") |

Most DAWs send on channel 1, so the simplest way to play one part is: set **"MIDI Ch 1 / Keyboard Plays"**
to that part. For a full band, load a few instances on separate tracks, each set to a different part, and
leave **RIDDIM** on in just one of them (or none).

- **FL Studio**: piano roll note colours are MIDI channels, so colour 10 notes play drums, colour 5 plays bass, etc.
- **Cubase / LUNA**: the channel of the notes in your MIDI part is respected; set the track's MIDI channel to *Any*.
- **Logic**: easiest is one instance per part using the Ch 1 setting.

Sustain pedal (CC64) works on piano, organ, pad and lead. Pitch bend is ±2 semitones. Mod wheel adds
vibrato to the lead and pad.

### Live chord following
Set **Chord Source** to **MIDI Ch 16**, then hold chords on channel 16 (from your keyboard or a MIDI region).
The skank, organ bubble, piano, bass line and pad all re-voice to whatever you hold, including 7ths,
minor 7ths, sus4 and diminished.

### Get the MIDI into your DAW
On the Riddim tab choose a length (4–32 bars), then either click **EXPORT MIDI...** or drag the
**DRAG MIDI TO DAW** box onto your arrangement. You get one track per part on the channels above, so you
can turn the riddim off and edit every hit and chord.

### Drums tab (sampled kits)
Pick **Studio Kit**, **Kit 2** or **Synth Kit** at the top.

- **Mic mixer**: one strip per mic (Kick In, Kick Out, Snare Top, Snare Bottom, Hi-Hat, Tom Hi/Mid/Floor, Rototoms,
  Timbales, Percussion, Cymbals, Overheads, Room) with fader, pan, mute and solo. Default levels follow your
  session's balance; the Room is generated (see notes below) and starts at -4 dB.
- **Drum buttons** (right): green = in this kit, gold = your own sample, dark = not in this kit (it falls back to the
  synth sound). Click one to hear it and edit its **Tune**, **Level** and **Decay**.
- **Replace a sound**: drag a WAV/AIFF/FLAC/MP3 from Finder onto any drum button, or select it and click
  **LOAD SAMPLE...**. **RESET** goes back to the kit sound. Replaced samples are remembered with your song
  (by file path, so keep the file where it is).
- Soft snare hits automatically switch to your ghost-stroke samples.

### Recorded instruments (Sounds tab)
Each part's **Type** menu lists the synth sounds first, then your recorded ones marked **(recorded)**. Bass, skank,
organ and piano default to your recordings.

- **Basses, lead and pluck guitars** are note instruments: every key plays the nearest recorded note, re-tuned from
  its measured pitch. Basses and leads are monophonic (a new note cuts the last), like a real player.
- **Guitars, organs and pianos** were recorded as chords, so they are chord instruments: the riddim and the
  Generator play them automatically; from MIDI, play a chord (2+ notes) and the closest recorded chop is re-pitched
  to it. A single key falls back to that part's synth sound.
- Your sessions were mostly major chords (Session 1 is G–F, Session 2 is Am–Em–G). When a sound has no minor chop,
  a minor chord plays its relative major (C over an A bass = Am7); a missing major plays the relative minor (Em over
  C = Cmaj7). Chops with no third (power chords) fit both.
- Soft skank/chop strokes (velocity under ~40) use your muted scratches.
- Each recorded sound is level-matched to the synth sound it replaces, so your mixer settings carry over.

### Generator tab (Drum Monkey / Unison style)
1. Choose a **Length** (1, 2, 4, 8 or 16 bars), how often **Drum Fills** happen and how busy they are.
2. For each part pick a **type** (e.g. Bass: Roots One Drop, Steppers Pulse, Walking Ska, Dub Sparse, Digital
   Dancehall; Skank: Double Chop, Offbeat Ska, Muted Scratch; Organ: Classic/Double/Shuffle Bubble; Lead: Melodica
   Line, Horn Riff, Call & Response) and a **Complexity**.
3. Hit **GENERATE** on a part, or **GENERATE ALL**. Every click is a new variation; the preview shows the result and
   a playhead.
4. Generated parts replace that part's style pattern while **PLAY** is lit. **USE STYLE** goes back to the built-in
   pattern.
5. **DRAG** a single part, or **DRAG ALL MIDI**, into your DAW.

Bass, guitar, organ and piano lines are stored relative to the chord, so changing the key or progression afterwards
re-voices them instead of breaking them. Changing a type or complexity regenerates that part with the same "seed",
so you can tweak without losing the variation you liked. Generated parts are saved with your song.

### Mixer
Each part has a level fader, pan, **Echo** send (tempo-synced ping-pong tape echo, default dotted 1/8) and
**Verb** send. The **Dub Filter** knob sweeps a resonant low-pass to the left and high-pass to the right;
centre is off. Automate it for dub drops.

---

## 3. Drum & percussion note map (channel 10)

Sampled kits use the same notes. Extra sampled notes: **93 Rototom Hi**, **94 Rototom Lo** (A5/A#5), and **55** plays
the Studio Kit's choked crash.

Note names use C3 = middle C (MIDI 60), as in Logic, Cubase and FL Studio. Click the **Pads** tab to hear each one.

| # | Note | Sound | # | Note | Sound |
|---|---|---|---|---|---|
| 35 | B0 | Kick Boom | 60 | C3 | Bongo Hi |
| 36 | C1 | Kick | 61 | C#3 | Bongo Lo |
| 37 | C#1 | Cross Stick | 62 | D3 | Conga Mute |
| 38 | D1 | Snare | 63 | D#3 | Conga Open |
| 39 | D#1 | Hand Clap | 64 | E3 | Conga Low |
| 40 | E1 | Rimshot | 65 | F3 | Timbale Hi |
| 41 | F1 | Floor Tom Lo | 66 | F#3 | Timbale Lo |
| 42 | F#1 | Hi-Hat Closed | 67 | G3 | Agogo Hi |
| 43 | G1 | Floor Tom Hi | 68 | G#3 | Agogo Lo |
| 44 | G#1 | Hi-Hat Pedal | 69 | A3 | Cabasa |
| 45 | A1 | Low Tom | 70 | A#3 | Maracas |
| 46 | A#1 | Hi-Hat Open | 71 | B3 | Timbale Rim (Paila) |
| 47 | B1 | Low-Mid Tom | 72 | C4 | Timbale Shell |
| 48 | C2 | Hi-Mid Tom | 73 | C#4 | Guiro Short |
| 49 | C#2 | Crash | 74 | D4 | Guiro Long |
| 50 | D2 | High Tom | 75 | D#4 | Claves |
| 51 | D#2 | Ride | 76 | E4 | Wood Block Hi |
| 52 | E2 | China | 77 | F4 | Wood Block Lo |
| 53 | F2 | Ride Bell | 78 | F#4 | Conga Slap Hi |
| 54 | F#2 | Tambourine | 79 | G4 | Timbale Hi 2 |
| 55 | G2 | Splash | 80 | G#4 | Triangle Mute |
| 56 | G#2 | Cowbell | 81 | A4 | Triangle Open |
| 57 | A2 | Crash 2 | 82 | A#4 | Shaker |
| 58 | A#2 | Syndrum | 84 | C5 | Nyabinghi Bass |
| 59 | B2 | Ride 2 | 85 | C#5 | Funde |
|  |  |  | 86 | D5 | Repeater |
|  |  |  | 87 | D#5 | Repeater Slap |
|  |  |  | 88 | E5 | Syndrum Pew |
|  |  |  | 89 | F5 | Syndrum Low |
|  |  |  | 90 | F#5 | Dub Siren |
|  |  |  | 91 | G5 | Air Horn |
|  |  |  | 92 | G#5 | Laser Zap |

The kit (35–59) goes to the **Drums** mixer strip; 60 and up go to **Perc**, so you can put echo on the
percussion and FX without washing out the kit. This follows the General MIDI layout where it can, so GM
drum MIDI files play sensibly.

---

## 4. Make your own riddims

All patterns live in `Source/engine/Riddim.cpp` in the `styles()` list, as plain text. Each line is 16
steps (one bar of 16th notes) or 32 steps (two bars):

```
{ 36, "........X......." }   // kick on beat 3 = one drop
{ 42, "x.xgx.xgx.xgx.xg" }   // hi-hat
```

- Drums: `X` accent, `x` normal, `o` medium, `g` ghost, `.` rest. The number is the drum note from the map above.
- Skank / piano: `x` plays the current chord.
- Organ bubble: `L` low chord, `H` high chord.
- Bass: `R` root, `3` third (major or minor to match the chord), `5` fifth, `6` sixth, `7` flat seventh,
  `O` octave, `2`, `4`, `r` soft root; `-` holds the previous note.

Add a new entry to the list, rebuild, and it appears in the Style menu. Chord progressions are in `progs()`
in the same file.

---

## 5. Project layout

```
RiddimForge/
├── CMakeLists.txt            build script (AU, VST3, Standalone; embeds Resources/Kits)
├── Resources/Kits/           the two sampled kits (24-bit FLAC + kits.json)
├── Resources/Instruments/    13 recorded instruments (24-bit FLAC + instruments.json)
├── Source/
│   ├── engine/               all DSP, no JUCE dependency
│   │   ├── Drums.*           kit, timbales, percussion, nyabinghi, dub FX
│   │   ├── Instruments.*     skank guitar, piano, organ, bass, lead, pad
│   │   ├── Riddim.*          styles, progressions, chord detection, voicings
│   │   ├── Effects.h         dub echo, reverb, dub filter
│   │   ├── DrumSampler.*     multi-mic sample playback + mic mixer
│   │   ├── Generator.*       drum / bass / skank / organ / piano / lead pattern generator
│   │   ├── SampledInstrument.* recorded note + chord-chop instruments
│   │   └── Engine.*          MIDI routing, sequencer clock, mixer
│   └── plugin/               JUCE wrapper: parameters, state, UI
└── tools/
    ├── render_demo.cpp       offline renderer: bounces demos to WAV with no DAW
    └── kit_builder/          the Python scripts that sliced your stems into kits and instruments
```

Want different branding? Change `COMPANY_NAME`, `PLUGIN_MANUFACTURER_CODE` and `BUNDLE_ID` in
`CMakeLists.txt` before your first build (changing the codes later makes DAWs see it as a new plugin).

### Offline renderer (optional)

```bash
cd tools
clang++ -O2 -std=c++17 -I../Source/engine render_demo.cpp ../Source/engine/*.cpp -o render_demo
./render_demo ~/Desktop
```

## 6. About the sampled kits

- **Studio Kit** comes from your first session. Close mics are your real recordings. Your overheads had every drum
  playing at once, so each drum's overhead sound was *calculated* from the whole 89-second session (how that drum
  sounds in your overhead mics) and rendered without bleed. Crashes were recovered from what was left in the overheads.
- `1_Drums.wav` (likely your room mic), `1_Ride.wav` and `1_BGV.wav` were silent in the upload, even though the
  master shows a ride and room playing. The **Room** channel is therefore generated. Re-export those stems and the
  kit can be rebuilt with the real room and ride (`tools/kit_builder/build_kits.py`).
- Kit 2's master is exactly the sum of its stems, so it plays at the balance it was mixed at.
- Instruments: every note sample's pitch was checked two ways. Slides, bends and ghost notes whose pitch didn't
  match were dropped, and re-pitched notes land within about 20 cents (the lowest session-bass notes about 30–40).

## License note

RiddimForge's own code is yours to use. The drum kits come from your recordings. If any of those tracks were played
by sample-based instruments (e.g. a drum VST), check that library's licence before you distribute the plugin. It builds on JUCE, which is free under the AGPLv3 or a JUCE
commercial licence; if you plan to sell or distribute the plugin closed-source, check JUCE's licence
terms at juce.com.
