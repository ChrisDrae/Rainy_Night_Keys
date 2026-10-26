# Keys · Sampling the Mk II

What you need to turn a real Rhodes Mark II into the Keys sample library: before the session, on
the day, and what to hand over afterwards. The prep tool (`KeysSamplePrep`, milestone M4) does the
slicing, velocity sorting and mapping, so your job on the day is to play cleanly and keep the
signal chain constant.

**The short version:**
1. Get the piano serviced and voiced the way you love it. That sound is what gets sampled.
2. Record DI + amp + a room/mechanics pair, all at once, at 48 kHz / 24-bit, and never touch the
   gain again.
3. Each key gets one "crescendo take" of about 20 strikes from whisper to bark, and each strike rings
   out.
4. Add the release, mechanics, pedal and room-tone passes.
5. Hand over the raw files and the session log.

---

## 1. Prepare the piano (2–4 weeks before)

The piano's voicing is baked into every sample, so this is the most important step.

- [ ] **Service by a Rhodes tech**:
  - Tuning: tines are tuned by sliding the tuning spring.
  - Voicing: pickup-to-tine alignment sets the balance of bark against bell. Decide on it by ear
    before the session, and photograph the settings.
  - Hammer tips, plus damper felts and their alignment, so nothing buzzes or rings when released.
  - Escapement and key dip, so every key responds the same and dynamics stay consistent.
- [ ] Fix dead, buzzy or clacky notes. Check the harp and pickups for hum.
- [ ] A smooth, quiet sustain pedal: oil the rod and check the pedal felt.
- [ ] Ask about **spare tines**. Very hard strikes can break old tines, and the loudest layer needs
  hard strikes.
- [ ] If the piano isn't yours, get the owner's written OK to sample it (an email is enough). Note
  its model, year and serial number.
- [ ] Move the piano into the recording room **24 h before** so it settles to the room temperature.
  Tine pitch drifts with temperature.

## 2. Gear checklist

| Item | Why / notes |
|---|---|
| Audio interface, ≥ 4 inputs, 24-bit / 48 kHz | One device means one clock, so every channel stays sample-aligned. 48 kHz is enough; 96 kHz doubles the size for little gain. |
| DI box with **thru/link** (active, Hi-Z ≥ 1 MΩ) | The Mk II pickup is passive and high-impedance. The thru feeds the amp, so the DI and the amp record the same strike. Ground lift helps against hum. |
| Amp, clean (a Twin-style tube amp is the classic) | Keep it clean; saturation can be added in the plugin. |
| Dynamic mic (SM57-type) on the cab; a ribbon is a nice extra | The "Amp" source. |
| Pair of small-diaphragm condensers | "Room": hammer thump, key noise and the acoustic sound of the tines, which matter a lot for realism. |
| Stands, shock mounts, cables (+ spares), power strip | |
| Closed-back headphones | |
| Strobe-style tuner (or tuner app) | Tuning checks every hour. |
| Laptop with Live, 2 × external drives (1 TB each is plenty) | Back up at every break. |
| Thermometer | Write the temperature in the log. |
| Tape, pen, the printed key list and session log | |
| A helper (optional, recommended) | One person plays, one runs the DAW and the log. |

**No MIDI is needed.** The prep tool measures every strike's loudness afterwards and builds the
velocity layers from that.

### The amp-bleed problem, and two ways to solve it

With the amp in the same room, the room mics hear the amp too, and the "Room" source stops being
the acoustic mechanics.

- **Option A: isolate the amp.** Put the amp in another room or a closet, run a long cable from the
  DI thru, and record all four channels at once.
- **Option B: re-amp later (recommended if you can't isolate).** On the day, record **DI + Room**
  only. Afterwards, play the raw DI files back through a re-amp box (e.g. Radial ProRMP / X-Amp)
  into the amp, and record the mic.
  - It needs no piano and no player, and it's quick.
  - The prep tool aligns the result to the DI automatically by cross-correlation.
  - You can also try several amps or settings later without another piano session.

## 3. Signal chain and channel map

```
                        ┌──────────────► thru ──► amp ──► SM57 ──► In 2  "amp"   (Option A)
Mk II out ──► DI box ───┤
                        └──► XLR ──────────────────────────────► In 1  "di"
SDC pair over the action (30–50 cm, aimed at the hammers) ──────► In 3/4 "roomL" / "roomR"
```

- **Piano controls:** the Mk II Stage has Volume and Bass Boost on the name rail. Set Volume to
  **max** (best signal-to-noise), set Bass Boost to a fixed position, and **note both in the log**.
  Then leave them alone.
- **No pedals or effects in the chain**, and no interface DSP, EQ or compression. Plain, dry signals
  only.
- **Room mics:** start about 40 cm above the hammer line, spaced about 60 cm, aimed down at the
  action. Listen for a balance of hammer thump against tine ring.

## 4. Levels (set once, then never touch)

1. Play the **hardest strike you'll use, in the bass**, on the loudest key.
2. Set each input so that strike peaks at about **−6 dBFS**. Use a pad on the DI if needed.
3. Check that the softest strike in the top octave still clears the noise floor clearly.
4. **Write all gain positions down and tape over the knobs.**
5. Record **60 s of silence** on all channels and listen loudly on headphones for hum, buzz, fans
   and traffic. Fix it now; you can't fix it in the samples.

Room prep: turn off heating, AC, fridges and anything with a fan, put phones in airplane mode, and
avoid noisy clothing. Long bass decays expose every bit of noise.

## 5. Session flow

| Step | What | Notes |
|---|---|---|
| 0 | Setup, levels, 60 s silence | ~1.5 h |
| 1 | **Room tone**: 30 s per chain, nobody moving | start of session |
| 2 | **Crescendo pass**, bass → treble, in segments of about one octave | the core; see §6 |
| 3 | **Release pass**: 3 strikes per key | §7 |
| 4 | **Mechanics pass** | §8 |
| 5 | **Pedal pass** | §8 |
| 6 | **Room tone** again | end of session |

Every hour: check the tuning on 3 reference keys (A2, A4, A6), write the temperature in the log,
and play back the last few keys to spot-check them. At every break, back up to **both** drives.

### How to record in Live

- One audio track per input: `di`, `amp`, `roomL`, `roomR`.
- Arrangement recording: **one continuous recording per segment**, e.g. "crescendo E1–D#2".
- After the session, export each track over each segment as a mono WAV (*Export All Individual
  Tracks*), 48 kHz / 24-bit, no normalize, no dither.
- File names: `<pass>_<segment>_<source>.wav`, e.g. `cresc_E1-D#2_di.wav`.

You don't have to mark individual notes. The tool finds each strike and measures its pitch, so it
knows which key it is. The log only has to say which segment covers which keys, plus anything that
went wrong.

## 6. The crescendo take (per key)

1. Play about **20 strikes on one key**, from the softest that still sounds (a whisper; include one
   or two "ghost" strikes) up to a full bark. Spread the strikes evenly in loudness.
2. **Let each strike ring until it has died away**, up to a cap:

   | Register | Keys | Ring cap |
   |---|---|---|
   | Bass | E1–B2 | 18 s |
   | Mid | C3–D#5 | 9 s |
   | Top | E5–E7 | 4 s |

   Soft strikes die sooner, so move on as soon as one is gone.
3. Keep your hand off the key until the ring-out ends. **Don't let the damper drop during the ring**:
   hold the key down, or hold the sustain pedal throughout and lift it only between strikes.
   Pick one method and stick to it; *hold the key* is the default.
4. Go to the next key. If a strike goes wrong (a buzz, a double strike, a noise), just play an extra
   one and note it in the log.

Afterwards the tool sorts every key's strikes by measured attack loudness, picks the layers and
round-robins (e.g. 8 layers × 2 round-robins out of 20 strikes) and flags outliers.

## 7. Release pass (per key)

Do three strikes per key, at **soft, medium and hard**:

1. Strike.
2. Hold for about **1.5 s** (bass/mid) or **0.8 s** (top).
3. Release the key cleanly, and let the damper noise and the remaining ring-out finish (about 1 s).

That captures the damper landing on a still-ringing tine, which is the "release sample". The plugin
plays it at key-up, scaled to how loud the note still is at that moment.

## 8. Mechanics and pedal

- **Key noises**: on 12 keys spread across the keyboard, press the key so slowly that the hammer
  doesn't reach the tine. That gives the key thump without a note. Then release it. Do it 3 times per
  key.
- **Pedal**: 10 × pedal down and 10 × pedal up, at mixed speeds (slow, normal, stomped), with no
  notes sounding.
- **Optional extras (Tier C)**:
  - Pedal-down ring: strike with the pedal held, to capture any sympathetic shimmer.
  - Half-pedal releases.
  - Re-strikes on a still-ringing note.

## 9. How much to sample

| Tier | Coverage | Strikes played | Strikes kept | Recording time | Day |
|---|---|---|---|---|---|
| A · Sketch | every 3rd key (25 keys), 5 layers × 2 RR, 2 release levels | ~14 per key ≈ 350 | ~250 + 50 | ~1.5 h | half a day with setup |
| **B · Core (target)** | all 73 keys, 8 layers × 2 RR, 3 release levels, mechanics, pedal | ~20 per key ≈ 1,460 | ~1,170 + 220 | ~5 h | one long day, or two half-days |
| C · Deep | 12 layers, 3–4 RR, pedal-down ring, half-pedal, re-strikes | ~30+ per key | ~3,000+ | 8 h+ | 2+ days |

Tier B time, worked out: (20 bass keys × 20 strikes × 19 s) + (28 mid × 20 × 10 s) +
(25 top × 20 × 5 s) ≈ 4.4 h of strikes, plus about 30 min of release, mechanics and pedal passes. Add
setup, tuning checks, spot checks and breaks.

**Sizes for Tier B (4 channels):**
- Raw session: ~9 GB of WAV.
- Kept strikes: ~6.5 GB of WAV, or **~3.5 GB as FLAC** in the finished library. That's why the
  engine streams from disk (milestone M5).

**Recommendation:** if you're unsure how much time you'll get, record **Tier A first**, in the first
hours of the day. That gives you a complete, playable library. Then continue with the remaining keys
toward Tier B.

## 10. What to hand over afterwards

```
session-YYYY-MM-DD/
  raw/                   cresc_E1-D#2_di.wav, cresc_E1-D#2_roomL.wav, …   (mono WAV, 48 kHz / 24-bit)
  roomtone/              roomtone_start_di.wav, roomtone_end_roomL.wav, …
  session-log.csv        one row per segment (template: docs/templates/session-log.csv)
  photos/                piano settings, mic positions, gain knobs
  notes.txt              piano model/year/serial, tech's notes, anything unusual
```

### Session log columns

| Column | Example | Meaning |
|---|---|---|
| `segment` | `cresc_E1-D#2` | the file stem, without `_<source>.wav` |
| `pass` | `cresc` · `release` · `keynoise` · `pedal` · `roomtone` · `extra` | what kind of recording |
| `first_key`, `last_key` | `E1`, `D#2` | key range covered (blank for pedal/room tone) |
| `strikes_per_key` | `20` | planned strikes per key |
| `time` | `10:42` | start time |
| `temp_c` | `21.5` | room temperature |
| `tuning_a4_hz` | `440.4` | reading at the last tuning check |
| `notes` | `F#1 strike 7 buzzed, extra strike played` | anything the tool or a listener should know |

## 11. What happens next (the prep tool)

For reference, so you know why the rules above matter:

1. Find each strike's onset on the DI. Use the **same cut points for every source**, so they stay
   aligned. Re-amped files are aligned by cross-correlation first.
2. Measure pitch (YIN), assign the key, and flag anything more than ±5 cents off.
3. Trim each tail where it meets the noise floor (taken from the room tone), and fade it.
4. Measure attack loudness, sort the strikes and pick velocity layers and round-robins. Outliers
   (double strikes, buzzes) are flagged in a QA report with a loudness curve per key.
5. Keep relative levels: the gain goes into the manifest, never normalized away. Keeping gain
   constant on the day is what makes this work.
6. Write 24-bit FLACs plus `library.json`, ready to load in Keys from ☰ → Library.
