# Keys · UI design: "Lightbox"

> **Status:** this describes the first direction, *Lightbox*. The app is built from the second,
> **Nocturne** ([`mockup/nocturne.html`](mockup/nocturne.html)): darker, with every control on one
> screen in tiles. Its controls, sound design and colour field come from this document; the
> differences are listed in [As built](#as-built-nocturne) at the end. The
> [user guide](user-guide.md) describes the finished window.

The reference for building the editor (milestone M3). The interactive draft lives in
[`docs/mockup/lightbox.html`](mockup/lightbox.html) and is published as a private artifact:
https://claude.ai/artifact/VUCQE99Kb6EHmjca5W36LB

## Idea

**The sound's color is the picture's color.** The whole window is a slow, living field of light.
Each color knob owns a hue, and turning it changes the sound and tints the field by the same amount,
so the window shows what you hear. Notes bloom as soft glows at their key position and fade with the
note. The look borrows the *mood* of Teletone Audio's instruments (few evocative knobs, reactive
artwork, noise and mechanics layers). The artwork, names and layout are our own.

## Window

- 1000 × 640 at 100 %, scalable 75 %–150 % at a fixed aspect ratio (`setResizable` + a
  `ComponentBoundsConstrainer`, the whole editor drawn through one `AffineTransform`).
- The keyboard strip (on-screen keys) adds 100 px. It is hidden by default in the plugin and shown by
  default in the Standalone app.

```
┌────────────────────────────────────────────────────────────────────┐
│ Keys  TINE & BELL        [DEMO TINES]     ◂ Dusk Ballad ▸      ☰   │ topbar   56
│                                                                    │
│                 color field (stage)                                │ stage   ~220
│                                                                    │
│ TINE ━━━━━━━━━━━━━●━━━━━━━━━━━━ BELL   │  DI ▮  AMP ▮  ROOM ▮      │ hero     48
│ [ drawer: fine controls of one knob, slides open ]                 │ drawer 0 / 90
│  ◯ WARMTH   ◯ BARK   ◯ BODY   ◯ PATINA   ◯ MOTION   ◯ SPACE        │ macros  130
│  ◦ dust  ◦ mechanics  ◦ touch  ◦ tune  ◦ volume          ▾ keys    │ smalls   56
├────────────────────────────────────────────────────────────────────┤
│  ▌▐▌▐▐▌▐▌▐▐▌▐▌▐▐▌▐▌▐▐▌▐▌▐▐▌  on-screen keys C2–C7   · pedal         │ keys    100
└────────────────────────────────────────────────────────────────────┘
```

The field covers the whole window. A scrim (transparent → 86 % deck color) behind the lower
controls keeps them legible; the keys strip is opaque.

## Color and type

| Token | Value | Use |
|---|---|---|
| ink | `#0e0b09` | outside the window, deepest shadow |
| deck | `#17120f` | window chrome, scrim (Pocket Looper family) |
| cream | `#f3e6cc` | text, knob rings, pointer |
| cream-dim | `#a8957a` | captions, values |
| line | cream at 14 % | hairlines, outlines |
| Warmth | `#e8a33d` amber | knob arc + field hue |
| Bark | `#d0603f` rust | 〃 |
| Body | `#b0669a` plum | 〃 |
| Patina | `#bf9c6c` sepia | 〃 |
| Motion | `#9f86dc` violet | 〃 |
| Space | `#6f92cf` dusk blue | 〃 |
| Bell | `#f2d27a` gold (celeste), `#cfe3f0` silver (vibes) | sparkles, fader end |

Fonts (all SIL OFL, bundled with `juce_add_binary_data`):

- **Fraunces** italic for the name "Keys", the preset name and drawer titles.
- **Jost** 500, upper-case with 0.2 em tracking, for every label. It has the look of 1970s gear
  lettering.
- **DM Mono** for values (tabular figures).

## Controls

### Color knobs (the six big ones)

| Knob | Sound, 0 → 100 | Field | Drawer |
|---|---|---|---|
| Warmth | clean → warm preamp: low-mid lift, softer top, gentle tube saturation | amber white balance | Drive, Tilt, Character (Tube · Transistor) |
| Bark | round, bell-like → growl: pickup asymmetry adds 2nd/3rd harmonics, more with velocity | rust hot spots that flicker | Velocity → bark, Tine ding |
| Body | thin → full: low-mid resonance, doubled round-robin voice, spread across the keys | bigger, denser plum light | Resonance, Double, Width |
| Patina | fresh → worn tape: wow/flutter, saturation, narrowing band | sepia grade, grain, vignette | Wow, Flutter, Age, Tape speed |
| Motion | still → moving: suitcase-style stereo pan vibrato, then chorus | light drifts and swirls faster | Mode (Vibrato · Chorus · Both), Rate, Sync to host |
| Space | dry → spring / plate / room | dusk-blue haze, softer edges | Type, Pre-delay, Decay, Tone |

Every color knob at 0 is an exact bypass of its stage. "Bare DI" (everything at 0) is the reference
preset.

### Tine ↔ Bell fader

At the left end only the tines sound, at the right end only the bell. In the middle both play at
full. Tine stays at 100 % up to the middle and the bell rises to 100 % by the middle, so the center
is the fullest sound. It snaps lightly to the middle.

Clicking either end label opens the **Bell drawer**:

- **Voice**: Celeste · Vibes · Both. With Both, vibes play below C4 and celeste above.
- **Octave**: 0 / +1.
- **Decay**.
- **Range**: the lowest key the bell plays on, with a 6-semitone fade below it.

### Signal mixer: DI · AMP · ROOM

Three short vertical faders set the levels of the three recorded sources. In demo mode, Amp and Room
are rendered from the modeled DI, so the faders always do something.

### Small knobs

- **Dust**: hiss and crackle.
- **Mechanics**: key thump, damper and pedal noise.
- **Touch**: velocity curve, from even to very dynamic.
- **Tune**: ±50 cents, bipolar.
- **Volume**: dB.

Clicking a small knob's label opens a compact drawer. Dust has a hiss/crackle balance; Mechanics has
a key/damper/pedal balance.

### Header

- The **library chip** shows `DEMO TINES` or, with a library loaded, e.g.
  `MK II · 73 KEYS · 8 VEL · DI/AMP/ROOM`. A warning variant appears if the library folder is
  missing.
- **Presets**: ◂ ▸ step through the presets, and the name opens a list (factory first, then user).
  Switching tweens the knobs over about 0.5 s, so the field glides into the new color.
- **☰ menu**:
  - Library: active library, Load folder…, Use demo.
  - Polyphony.
  - Oversampling.
  - Still background (reduced motion).
  - About and credits.

### Interaction rules (all knobs and faders)

- Drag vertically; hold Shift for fine steps.
- Double-click resets to the default.
- The scroll wheel works, and arrow keys work when focused.
- The value shows under the name on hover, focus or drag.
- Click a name to open its drawer. Only one drawer is open at a time, and clicking the name again or
  ✕ closes it.
- Every control is a host parameter with a stable ID (below), so MIDI mapping and automation work.

## The field: how it's drawn

The CPU budget is under 3 % at 30 fps.

1. **Canvas**: a `juce::Image` of about 384 px wide, at the window's aspect ratio, redrawn on a
   30 Hz `Timer`. It is drawn upscaled with high-quality resampling, which also does most of the
   softening.
2. **Base**: near-black, warmer as Warmth rises.
3. **Light blobs**: about 9 radial gradients composited additively ("screen"), one or two per color
   knob:
   - **Size** grows with the knob value and with Body.
   - **Opacity**: 0.08 + 0.5 × value.
   - **Position**: each blob follows a slow Lissajous path, at a speed of 0.06 + 0.9 × Motion.
4. **Bark** adds three small hot spots that flicker.
5. **Notes**:
   - A **glow** appears at the key's x position. Its brightness follows velocity, and its color mixes
     tine cream-amber with the bell color by the blend.
   - Glows decay with the note: from the natural decay while held, then about 0.35 s after release
     (unless the pedal is down). They drift slowly upward.
   - **Sparkles** (gold for celeste, silver for vibes) twinkle around the glow in proportion to the
     bell level.
6. **Grade**:
   - Warmth: an amber soft-light wash.
   - Space: a blue haze from the top, plus extra blur.
   - Patina: sepia, lower saturation, film grain (a pre-made noise tile at a random offset each
     frame) and a darker vignette.
   - Dust: occasional specks and scratches.
7. **Data from the audio thread**:
   - Note-on/off events go through an `SpscQueue` (from Pocket Looper).
   - Per-note levels come through a `TripleBuffer` snapshot.
   - The UI never locks or allocates on the audio side.
8. **Still background**: blob motion nearly stops and grain freezes. It is on by default when the OS
   asks for reduced motion.

## Parameters (stable IDs, never renamed once released)

| ID | Range | Default | Notes |
|---|---|---|---|
| `blend` | 0–1 | 0.38 | Tine ↔ Bell |
| `warmth` `bark` `body` `patina` `motion` `space` | 0–1 | per preset | color knobs |
| `dust` `mech` | 0–1 | 0.18, 0.35 | |
| `touch` | 0–1 | 0.5 | velocity exponent 0.45 → 1.8 |
| `tune` | −50…+50 ct | 0 | |
| `volume` | −∞…+6 dB | 0 dB | |
| `sigDi` `sigAmp` `sigRoom` | 0–1 | 0.85, 0.35, 0.25 | signal mixer |
| `warmthDrive` `warmthTilt` | 0–1 | 0.5 | 0.5 = the knob's own mapping |
| `warmthChar` | Tube · Transistor | Tube | |
| `barkVel` `barkDing` | 0–1 | 0.5 | |
| `bodyRes` `bodyDouble` `bodyWidth` | 0–1 | 0.5, 0.35, 0.5 | |
| `patinaWow` `patinaFlutter` `patinaAge` | 0–1 | 0.5 | |
| `patinaSpeed` | 7.5 · 15 ips | 15 | wow rate and top end |
| `motionMode` | Vibrato · Chorus · Both | Vibrato | |
| `motionRate` | 0.5–8 Hz | ~1.6 Hz | |
| `motionSync` | bool | off | rates become note values |
| `spaceType` | Spring · Plate · Room | Plate | |
| `spacePre` `spaceDecay` `spaceTone` | 0–120 ms, 0.5–4 s, 0–1 | 24 ms, 2 s, 0.55 | |
| `bellVoice` | Celeste · Vibes · Both | Celeste | |
| `bellOctave` | 0 · +1 | 0 | |
| `bellDecay` `bellRange` | 0–1, C2–C5 | 0.5, ≈F2 | |
| `dustBalance` `mechBalance` | 0–1 | 0.5 | small-knob drawers |

## Factory presets

| Preset | Character | Blend | Warmth | Bark | Body | Patina | Motion | Space |
|---|---|---|---|---|---|---|---|---|
| Dusk Ballad | the init sound: warm, a little bell | .38 | .62 | .30 | .58 | .35 | .32 | .42 |
| Sunday Tape | worn cassette, room reverb | .22 | .58 | .28 | .52 | .80 | .18 | .30 |
| Glass Lullaby | bell-forward, octave up | .74 | .30 | .08 | .42 | .18 | .26 | .66 |
| Basement Soul | barking, through the amp, spring | .10 | .72 | .78 | .72 | .24 | .58 | .20 |
| Snowglobe | mostly bell, chorus, big plate | .86 | .22 | .04 | .36 | .30 | .50 | .82 |
| Late Bus | vibes + tines, tape, wobbly | .46 | .50 | .36 | .60 | .56 | .62 | .55 |
| Bare DI | everything off: the reference | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

User presets are XML files in `Documents/Keys/Presets`.

## JUCE building blocks

| UI part | Class | Notes |
|---|---|---|
| Look and feel | `KeysLookAndFeel : LookAndFeel_V4` | same pattern as `PocketLookAndFeel` |
| Color field | `ColorField : Component, Timer` | owns the low-res image, reads the snapshot |
| Color knob | `MacroKnob` | `Slider` + name button + value label; the attachment is declared last (see Pocket Looper `Widgets.h`) |
| Tine ↔ Bell | `BlendFader` | horizontal `Slider`, custom paint, center detent |
| Signal mixer | `SignalMixer` | three vertical `Slider`s |
| Drawer | `Drawer` | animated height, one open at a time, built from a spec table like the mockup's `DRAWERS` |
| Presets | `PresetBar` | prev/next + `PopupMenu` |
| Library chip | `LibraryChip` | text and state from `LibraryLoader` |
| Keys | `MidiKeyboardComponent`, restyled | feeds the processor's `MidiKeyboardState` |

## As built (Nocturne)

The app follows [`mockup/nocturne.html`](mockup/nocturne.html). Where it differs from Lightbox:

- **Layout**: tiles instead of drawers. Only Warmth, Bark, Body and Space have a ▾ popover for
  fine controls; Patina's, Motion's and the bell's options sit on their tiles. Tune and Touch
  moved into the ☰ menu, which also holds a MIDI monitor for testing keyboards.
- **Parameters** that changed or were dropped (the IDs below are final):

  | ID | As built |
  |---|---|
  | `patinaWow` `patinaFlutter` `patinaNarrow` | on/off switches (Lightbox had wow and flutter as amounts, plus `patinaAge` and `patinaSpeed`) |
  | `motionRate` | a choice: 0.5, 1, 1.6, 2.5, 4, 6 or 8 Hz (`motionSync` was dropped) |
  | `bellRange` | a choice: C2, F2, C3, F3 or C4 (the bell fades in over the six keys below) |
  | `spacePre` `spaceDecay` | in ms (0–120) and seconds (0.5–4) |
  | `bellDecay`, `dustBalance`, `mechBalance` | not built |

- **Menu**: no polyphony or oversampling settings yet (64 voices; the colour chain's saturation
  stages use antiderivative anti-aliasing instead of oversampling). The Library section switches
  between sampled tines (any SFZ map in `assets/tines/<name>/`; jRhodes3d for now) and the
  modeled ones.
- **Presets** don't change Tune, Touch or Level, and switching glides continuous controls only
  (choices and switches change at once).
- **The field** is drawn into a ~144 px-wide buffer with its own screen / soft-light blending,
  grade and blur, then upscaled; the vignette is part of that buffer. Notes reach it through a
  lock-free queue from the audio thread. With the window open, the app used about 10 % of one
  core on the development machine (animation and audio together).
