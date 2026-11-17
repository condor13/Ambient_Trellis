# How Ambient Trellis works

This explains the firmware from the top down: first the big picture, then each
piece of math. Every section names the file where the code lives.

---

## 1. The big picture

The Pico has two processor cores, and we give each one a clear job:

| Core | Job | Files |
|---|---|---|
| **Core 0** | Decides *what* to play (chords, plucks), reads the pads, drives the LEDs, the menu and the display | `src/music/`, `src/ui/`, `src/hw/`, `AmbientTrellis.ino` |
| **Core 1** | Makes the *sound*: 32,000 stereo samples per second | `src/audio/*` |

Core 0 sends small messages ("start note 62", "pluck this string") to core 1.
Core 1 never waits for core 0, so the audio never stutters, even if core 0 is
busy drawing the display.

**Rule of thumb:** anything slow (float math, decisions) happens on core 0.
Core 1 only does fast integer work that repeats every sample.

### Blocks

Core 1 doesn't compute one sample at a time. It computes a **block** of 32
samples (1 millisecond) at once:

1. Read the messages from core 0.
2. Update the slow things once: envelopes, LFOs, brightness (the "control rate", 1000×/s).
3. Compute the fast things for 32 samples: oscillators, strings, delay, reverb (the "audio rate", 32,000×/s).
4. Hand the 32 samples to the DAC.

Slow things change smoothly enough at 1000 updates per second, so we save a
lot of work by not updating them 32,000 times per second.

### Signal flow (`src/audio/AudioEngine.cpp`)

```
  pads ----------------------------+--> main ---+
                                   |            |
  plucks --+-- (soft) ------------>|            +--> compressor --> DAC
           +-- (strong) --> send --+--> reverb -+
           +--> delay -------------+
```

- **main** is what you hear directly.
- **send** is what goes into the reverb.

The plucks are quiet in *main* but loud in *send*. That's why they sound far
away: you mostly hear their reverb.

---

## 2. Fixed-point numbers: fractions without floats

The RP2040 has no floating-point hardware. A float multiply takes about 50–100
clock cycles in software; an integer multiply takes 1. So all audio math uses
**integers that represent fractions**. This is called *fixed point*.

The trick: decide that some big number means "1.0".

| Name | "1.0" is | Example |
|---|---|---|
| **Q8**  | 256    | `volume = 225` means 225/256 = 0.88 |
| **Q15** | 32768  | `decay = 27500` means 0.84 |
| **Q16** | 65536  | amplitude `65535` ≈ 1.0 |
| **Q24** | 16,777,216 | the pad envelope position |

Multiplying a sample by a Q15 number:

```
result = (sample * gain) >> 15
```

`>> 15` (shift right by 15 bits) is the same as dividing by 32768, but it costs
one cycle.

Watch out for:
- **Overflow:** a 32-bit integer holds up to about ±2.1 billion. Before every
  multiply, the code makes sure the two numbers can't exceed that. That's why
  you'll see comments like "keeps the math inside 32 bits".
- **Rounding:** `>> 15` always rounds *down*. In feedback loops (strings,
  reverb) that tiny error repeats thousands of times per second and can add up.
  Section 7 shows how the plucks deal with this.

Floats are still used where speed doesn't matter, such as when a note *starts*
(computing its frequency once).

---

## 3. Oscillators: the phase accumulator (`PadSynth.cpp`)

A sine wave is "going round a circle". We keep the position on the circle in a
32-bit unsigned integer, the **phase**:

- 0 = start of the circle
- 2³² (4,294,967,296) = one full turn, and it wraps back to 0 by itself, because
  that's what a 32-bit integer does when it overflows. Free circular math!

Every sample, the phase moves forward by a fixed **step**:

```
step = frequency / sample_rate * 2^32
```

For A4 (440 Hz): `440 / 32000 * 2^32 ≈ 59,055,800`.

To turn the phase into a sound, we look it up in a **sine table**: 2048
precomputed values of one sine cycle, built once at start-up. The top 11 bits
of the phase pick the entry (`phase >> 21`). A table lookup is much faster than
computing `sin()`.

---

## 4. FM synthesis: the pad sound (`PadSynth.cpp`)

FM (frequency modulation) uses one oscillator, the **modulator**, to wobble the
position of another, the **carrier**, very fast:

```
output = sin( carrier_phase + index * sin(modulator_phase) )
```

- With **index = 0**, you hear a pure sine (soft, flute-like).
- As the **index** rises, new harmonics appear and the sound gets brighter and
  richer. That's the "bloom" when a chord swells in.
- The modulator runs at the same frequency as the carrier (ratio 1:1), which
  gives a warm, string-like series of harmonics.

### Our pad voice

Each of the 16 voices has **one modulator and two carriers**:

- The left carrier is tuned 4 cents flat, the right one 4 cents sharp.
  Slightly different pitches drift in and out of phase, which gives the wide
  stereo shimmer. Bass notes are kept almost centred, because very wide bass
  feels odd on headphones.
- The modulator runs 0.05–0.12 Hz off the carrier, so the timbre slowly turns
  over by itself, like a slow phaser.

### What moves over time

All of this runs once per block, in `PadSynth::control()`:

| What | How |
|---|---|
| **Volume swell** | the envelope (next section) |
| **Brightness bloom** | the index rises from almost 0 to `brightness` over `evolveMs` (14 s) |
| **Breathing** | a slow LFO (24 s) moves the index ±25%, each voice at its own phase |
| **Drift** | a second, slower LFO bends the left/right detune by about ±1.5 cents |
| **Register** | `keyScale`: bass gets less FM (soft), the middle the most, high notes less again |

To avoid steps between blocks, the amplitude and index are **ramped**: each
block computes a small step, and every sample adds it.

---

## 5. The envelope: a slow swell (`PadSynth.cpp`)

The envelope position `lvl` goes from 0 to 1 (in Q24) in a straight line during
the attack, and back down during the release.

The actual volume is **lvl squared**:

```
amplitude = lvl * lvl
```

Squaring makes the beginning very gentle: at 10% of the attack time, the volume
is only 1%. Then it rises faster. Your ears hear that as a natural swell rather
than a fade.

When a chord changes, old notes release and new ones attack at the same time,
so you get a crossfade. Because both curves are squared, there's a little "breath" (dip)
in the middle. That dip is tuned by the release time (7.5 s) and by how late the
new notes start (1.1–3.4 s).

---

## 6. Chords and voice leading (`src/music/Sequencer.cpp`)

### Building a chord

A scale is 7 notes. D Dorian is D E F G A B C. A chord is built by **stacking
every other note** of the scale, starting from one degree:

```
degree i  (D):  D  F  A  C   → Dm7
degree IV (G):  G  B  D  F   → G7
```

This automatically gives the right chord quality for every degree. The code
then names the chord by measuring the intervals (a major or minor third, perfect or
diminished fifth, and so on).

**The 9th:** sometimes (35%) the 9th is added for a lusher sound, but only when
it's a whole tone above the root. A half-tone 9th (like F over E) would clash.

**Rootless voicing:** the bass already plays the root, so the upper voices play
3rd, 5th and 7th, plus the root or the 9th. This keeps the middle clear.

### Choosing the next chord

Each chord degree has a **weight** (how much it is liked in this scale). Some are
0 and are never used, like the diminished chord in a major key. Then:

- Chords that **share notes** with the current chord get a bonus. Shared notes
  make smooth transitions.
- The home chord (I) gets a bonus.
- **Drift** turns these bonuses down. Low drift means staying close to home;
  high drift means wandering more freely.

Then a weighted random choice picks the next chord. It's like a die where some
sides are bigger than others.

### Voice leading: why changes glide

For every new note, the code picks the octave that's **closest to the centre of
the previous chord**. So the voices move by small steps instead of jumping.

Details:
- The centre slowly pulls back towards D4, so the chords never drift off too
  high or too low.
- If two notes would be a semitone apart (a harsh rub), one moves an octave away.
- The **bass** takes the root in the octave closest to the previous bass note.
- Notes **shared** by both chords simply keep sounding and are not restarted.
- New notes don't all start at once. Each waits a random 1.1–3.4 s, like a slow
  strum.

---

## 7. Plucks: Karplus-Strong strings (`PluckSynth.cpp`)

This is one of the most elegant tricks in digital sound.

**The idea:** a string vibrates because a wave runs along it, bounces at the
ends, and comes back. One round trip takes exactly one period of the note. We
imitate that with a **delay line exactly one period long**:

```
      +---------------- delay (1 period) ----------------+
      |                                                   |
 in --+--> [ read ] --> [ low-pass ] --> [ loss ] --> [ write ]
                |
               out
```

1. **Fill** the delay line with a short sound (the "pluck").
2. Every sample, **read** the oldest value, **soften** it a little (low-pass) and
   make it a bit **quieter** (loss), then **write** it back.
3. The sound goes round and round, once per period, so you hear that pitch.
   Each trip removes some high frequencies, so the tone gets rounder as it
   fades, just like a real string.

### The details that make it sound good

**The pluck itself:** mostly one smooth sine cycle (90%, round and soft), plus
a little filtered noise (10%, the "string" character). Pure noise sounds
twangy and scratchy; this sounds like a soft harp or kalimba. On top of that:
- every string fades in over 8 ms, so there's no hard edge at the start;
- all plucks pass through a gentle low-pass (about 3 kHz, `pluckTone`).

**Starting a string is split across the cores.** Working out a string's
recipe needs `pow`, `cos` and `sqrt`, which are slow without a floating-point
unit. At first this ran on core 1 and every new pluck made one audio block
take longer than 1 ms (the "100%" in the load log). Now:
- `PluckSynth::design()` runs on **core 0** and computes the recipe
  (`PluckSpec`): loop length, allpass coefficient, low-pass, loss, pan.
- It travels to core 1 through its own small queue.
- `PluckSynth::start()` on **core 1** only fills the string, with integer math
  and the shared sine table (`src/util/SineTable.*`).

**Exact tuning:** a period is rarely a whole number of samples. At 32 kHz, G5 is
40.8 samples. A delay can only be whole samples, so the fraction is made with
a tiny **allpass filter**:

```
x[n] = c * (s[n] - x[n-1]) + s[n-1]        with   c = (1 - frac) / (1 + frac)
```

It delays the sound by `frac` of a sample without making any frequency quieter.
Reading "between two samples" (linear interpolation) would also work, but it
softens high notes on every trip, and high notes make over 1000 trips per second.

**The same ring time for every note:** high notes go round the loop more often
per second, so they'd die faster. The code computes, per note:
- the loss per trip that gives -60 dB after 3.2 seconds:
  `perTrip = 10^(-3 / (frequency * seconds))`
- how strong the low-pass may be (`a`) so that it fits within that loss.

**Precision:** this was the hardest part. With normal rounding, a quiet string
could get stuck: when the loss per trip is less than half a unit, the number
never changes again and the string hums forever at a tiny level. Two fixes:
- The string stores its samples **×16** (4 extra bits), so rounding errors are
  16 times smaller.
- The loss is **at least 3/16 of a unit per trip**, more than the filters'
  rounding can ever add back. So every string really ends in silence.

### When do plucks happen? (`Sequencer.cpp`)

On every 8th note there's a small chance of a pluck:

```
chance per 8th note = density² × 0.35%      (density 4 → 5.6%, about one every 9 s)
```

The note is always **a note of the current chord**, in the high register
(C5–G6), preferably within a fifth of the previous pluck. So the plucks make a
little, slow melody that can never sound wrong. Each one is up to 60 ms late at
random, for a human feel.

---

## 7b. The pond of notes on the pads (`src/ui/PondLeds.cpp`)

The 16 pads are not fixed keys but 16 **places** in a pond. About 30 times a
second, core 0 copies from the audio engine which notes are sounding and how
loud they are (each synth voice publishes `voiceNote` and `voiceLevel`). Then:

1. **Every pad is a "slot"** that can hold one sounding note.
2. A note that starts sounding gets a **random free slot**.
3. A slot's brightness **follows the loudness** of its note. So a chord note
   swells in on the pads exactly as you hear it swell in.
4. When a note stops (or its voice is reused for another note), its slot is
   marked **gone**: it fades out where it is, and only once it's dark does the
   slot become free again. So during a chord change you see the old chord fade
   while the new notes appear elsewhere.

**Brightness on a log scale.** Our ears and eyes both work in ratios: twice as
loud doesn't feel twice as strong. So the level is mapped with a logarithm:
every doubling of loudness adds the same step of light. Without that, a pluck's
glow vanished after its first loud burst, while you could still hear it for
two more seconds. After that there's a **gamma curve** (brightness²), because
LEDs look far too bright at low values otherwise.

**Smoothing:** displayed brightness rises quickly and falls slowly, so nothing
flickers. Touch glows fall even slower, like a ripple settling.

**Plucks** are always water blue, never their note colour, so chords and
plucks are easy to tell apart. As a pluck fades, its brightness jumps randomly
a little below its value every frame (30 times a second): a fast flicker. The
quieter it gets, the deeper the flicker, so it sparkles out like light on water.

**Touch plucks** carry a **tag**: the number of the pad that was touched. It
travels with the pluck recipe to core 1 and comes back with the voice's level,
so the LED code knows exactly which pad should glow water blue.

**Chord colours:** C red, D green, E yellow, F orange, G blue, A turquoise,
B pink. A sharp is the average of its two neighbours (C# is between red and green).

The hardware side (`src/hw/TrellisPads.cpp`) is kept separate and thin: it
only reads key presses and sends 16 colours over I2C. So the pond logic can be
tested on a computer.

---

## 7c. The SHIFT menu (`src/ui/Menu.cpp`, `src/app/Settings.cpp`)

Three pieces, each with one job:

- **`Settings`** holds the 16 values exactly as you pressed them (pad 1..16).
  It knows what each value *means*: for example VERB 10 → reverb wet 153/256,
  BPM 8 → 60 BPM, LFO 5 → a 24 s cycle. `apply()` sends one setting to the
  sound engine, the sequencer or the LEDs.
- **`Menu`** is a small *state machine* with three states:

  ```
  CLOSED --SHIFT--> LIST --tap parameter--> VALUE --tap pad--> VALUE (value set)
     ^               |  ^                     |
     +-----SHIFT-----+  +--------SHIFT--------+
  (and 10 s without a touch: back to CLOSED)
  ```

  It turns pad presses into setting changes and draws the menu colours. It
  never touches the sound itself; it only reports "parameter X changed".
- **The sketch** connects them: it applies the change and shows it.

**The sweep** is just time: when the menu opens, row *r* appears after
r × 60 ms (with a short white flash at the front). When it closes, the rows
disappear from the bottom up, and the pond shows through again.

Some mappings are on a curve rather than a straight line, because that's
how we hear and see:
- **LFO** goes from 60 s to 2 s in equal *ratios* (each step is about 20% faster).
- **LIGHT** is squared (value² / 256), so the lowest steps are very dim, for a
  dark bedroom.

The **sleep timer** is a deadline: when SLEEP is set (or playing starts), it
remembers "now + N minutes". When that time comes, it does the same as
pressing STOP, so the music fades out naturally.

---

## 7d. The display (`src/ui/Canvas.*`, `src/ui/Screen.*`, `src/hw/Oled.*`)

**The picture** is 128 × 64 black-or-white pixels = 8192 bits = 1 KB. It's
stored the way the SSD1306 chip wants it: 8 horizontal strips ("pages") of 8
pixel rows; each byte is one column of 8 pixels in a strip. So pixel (x, y)
is bit `y % 8` of byte `(y / 8) × 128 + x`, and the finished picture can be
sent as it is.

**Drawing** (`Canvas`) is a handful of classics:
- **Lines** with Bresenham's algorithm: step along the longer direction and
  keep an error counter for the other one. Only additions, no division.
- **Circles** with the midpoint algorithm: compute one eighth of the circle
  and mirror it 8 ways. Dotted circles skip some points, which is how ripples
  "fade out" on a display that has no grey.
- **Text** with a built-in 5×7 font: each character is 7 rows of 5 bits.

**The koi** (`Screen`):
- Each fish has a position, a direction (heading) and a turning speed. The
  turning speed drifts randomly a little every frame, so the fish wander in
  smooth curves. Near the edge, the heading is pulled towards the middle.
- The body is 6 filled circles along a "spine" behind the head. Each spine point
  sways sideways by `sin(phase - i × 0.9)`. Because every point is a bit
  behind the one before it, the sway travels down the body like a real swimming
  wave, getting bigger towards the tail.
- The fin, eyes and spots are a few lines and black pixels. The spots come from
  the fish's own random number, so each koi has its own pattern.
- Speed comes from the LFO setting: 6 px/s at the slowest, 36 px/s at the
  fastest. The tail beats faster when the fish swims faster.

**Sending it** (`Oled`): sending 1 KB over I2C at 400 kHz takes about 25 ms.
Doing it in one go would freeze the pads for that long. So the picture is
copied and then sent **one strip per loop** (about 3 ms each), with key
reading and LED updates in between. About 20 pictures per second.

---

## 7e. Turning the pads and the display (`src/util/PadMap.h`)

All the code thinks in **logical pads** 0..15, row by row from the top left,
with pad 0 = PLAY. Only the NeoTrellis driver translates between logical pads
and the real board, using `PAD_ROTATION` (0, 90, 180 or 270 degrees clockwise).

Turning a 4×4 grid 90° clockwise moves row `r`, column `c` to row `c`,
column `3 - r`. So the top row becomes the right-hand column, which puts PLAY
on the top-right pad. For a touch, the driver works backwards (physical pad →
logical pad); for the LEDs, it gives each physical LED the colour of its
logical pad. Nothing else in the code has to know.

The display turns 180° inside the SSD1306 itself: two start-up commands decide
which corner is pixel (0, 0), and swapping both turns the picture upside down
(`OLED_FLIP`). The display's menu grid and touch ripples use the same pad map,
so the labels sit where the pads physically are.

---

## 8. The pluck delay: ping-pong echoes (`PluckDelay.cpp`)

Two delay lines, left and right, connected in a ring:

```
input + feedback --> [ left delay ] --> [ right delay ] --+
       ^                                                  |
       +-------------- low-pass <-------------------------+
```

The first echo comes from the left, the second from the right, the third from the
left again (quieter and darker), and so on. That's the **ping-pong** mode; DMOD
picks one of four:

| Mode | What happens |
|---|---|
| Forward | plain echoes in the middle |
| Ping-pong | echoes bounce left, right, left … |
| Reverse | every echo plays backwards |
| Reverse ping-pong | backwards on the left, then bouncing forwards to the right |

**Reverse** works like this: a second reader walks *backwards* through the
recent sound while the writer walks forwards, so they move apart by 2 samples
every step. When the reader has covered the whole delay length, it jumps back
and starts the next piece. Each piece gets a smooth window (half a sine: silent
→ full → silent), so the jumps can't click and every echo *swells in*. That
swell is the classic dreamy "reverse" sound. The low-pass in the feedback makes every
repeat a little darker, so the echoes melt into the reverb.

- **Time:** a dotted 8th note (3/4 of a beat), so the echoes sit in the tempo.
  At 60 BPM that's 750 ms.
- **Half sample rate:** the delay runs at 16 kHz. Two input samples are averaged
  into one, and on output each value is used twice (with a halfway value in
  between). That halves the memory (56 KB instead of 112 KB) and the work, and the
  echoes are dark anyway.

---

## 9. The reverb: a Dattorro plate (`Reverb.cpp`)

This is the classic plate reverb published by Jon Dattorro in 1997. It has
three parts.

### a) Pre-delay and input diffusion

- A 25 ms **pre-delay** keeps the original sound clear before the reverb starts.
- A gentle **low-pass** makes it darker.
- Four **allpass diffusers** smear the sound in time. An allpass lets every
  frequency through at the same volume but scrambles the timing. A click goes
  in, and a soft "shhh" comes out.

### b) The tank: where it rings

Two halves, left and right, feed each other in a figure eight:

```
   +--> [ allpass ~ ] --> [ delay ] --> [ damping ] --> [ allpass ] --> [ delay ] --+
   |                                                                                |
   |  (left half)                                                    x decay        |
   |                                                                                v
   +-- x decay <-- [ delay ] <-- [ allpass ] <-- [ damping ] <-- [ delay ] <-- [ allpass ~ ] <--+
                                    (right half)
```

- **Decay** (0.84): how much survives each trip. Higher means a longer tail.
- **Damping:** a low-pass in the loop, so highs die faster than lows. That's
  what makes the tail **dark**, like a real room.
- The allpasses marked `~` are **modulated**: their length moves ±17 samples
  very slowly (0.6 Hz). That stops the tail from sounding metallic or ringing
  at fixed frequencies, and gives the gentle "moving" quality you hear.

### c) Output taps

The left and right outputs are each made by adding 7 points ("taps") read from
different places in the tank. Because the left and right outputs use different
points, they're different, so the reverb is wide and stereo.

The delay lengths (723, 4788, 1935 … samples) are from Dattorro's paper, scaled
from his 29,761 Hz to our 32,000 Hz. They're chosen to share no common factors,
so the echoes never line up into audible patterns.

Memory: about 50 KB. CPU: about 10% of core 1.

---

## 10. Compressor and soft clipper (`SoftComp.h`)

The last step before the DAC. It's a separate, reusable file.

**Compressor** (once per block):
1. Find the loudest sample in the block (the peak).
2. Follow it with an **envelope**: jump up instantly, sink back slowly (about 0.5 s).
3. If the envelope is above the **threshold**, reduce the gain so that
   everything above the threshold is 3 times smaller (ratio 3:1).
4. Change the gain smoothly across the block, so there are no clicks.

**Soft clipper** (every sample): up to 67% of full scale, nothing happens.
Above that, the curve bends smoothly towards the maximum and never quite
reaches it:

```
y = knee + excess * headroom / (excess + headroom)
```

So even if everything plays at once, the output never hard-clips.

---

## 11. Talking between the cores (`src/util/EventQueue.h`)

Core 0 writes, core 1 reads, at the same time. Two tools keep that safe:

**Events (a ring buffer):** a list of 64 slots with a *head* (where core 0 writes
next) and a *tail* (where core 1 reads next). Only core 0 moves the head, and
only core 1 moves the tail, so they never need to lock each other out.
`__sync_synchronize()` (a "memory barrier") makes sure the message is fully
written before the head moves.

**Settings (`SoundParams.h`):** each setting is one 32-bit number. The RP2040
always reads and writes 32-bit numbers in one piece, so core 1 sees either the
old value or the new one, never half of each.

---

## 12. Speed tricks (`src/util/FastCode.h`)

Core 1 has 1 ms to compute each 1 ms block. Currently it needs about 20–25%
of that. The tricks:

- **`AUDIO_FAST`:** puts the hot audio functions in RAM. Code normally runs from
  the flash chip, which is slower and shared with core 0.
- **`ALWAYS_INLINE`:** tiny helpers (read a delay line, one allpass step) are
  pasted directly into the loops instead of being called. The Arduino-Pico core
  optimizes for size by default and would otherwise turn them into real
  function calls. Doing this took the reverb from 25% down to about 10% of core 1.
- **`#pragma GCC optimize("O2")`:** the audio files are compiled for speed.
- **Tables and shifts:** a sine table instead of `sin()`, and `>>` instead of division.

---

## 13. File map

```
AmbientTrellis/
  AmbientTrellis.ino        setup and loop for both cores
  src/Config.h              pins, sample rate, block size
  src/util/
    FastCode.h              RAM placement and inlining macros
    Rng.h                   fast random numbers (xorshift)
    EventQueue.h            core 0 -> core 1 message queue
    SineTable.*             one sine cycle, shared by the synths
    PadMap.h                turning the pad grid (logical <-> physical pads)
  src/audio/                (core 1)
    AudioEngine.*           receives events, mixes everything
    Events.h                message types
    SoundParams.h           all sound settings
    PadSynth.*              FM chord pads
    PluckSynth.*            Karplus-Strong plucks
    PluckDelay.*            ping-pong echo for the plucks
    Reverb.*                Dattorro plate reverb
    SoftComp.h              compressor + soft clipper (reusable)
  src/music/                (core 0)
    Sequencer.*             chords, voice leading, plucks, touch
  src/app/                  (core 0)
    Settings.*              the 16 menu parameters: values, names, meaning
  src/ui/                   (core 0)
    PondLeds.*              the pond of notes on the pads
    Menu.*                  the SHIFT menu (state machine + colours)
    Colour.h                LED colours and brightness curves
    Canvas.*                1-bit picture, lines, circles, 5x7 font
    Screen.*                koi pond, menu pages, text overlays
  src/hw/                   (core 0)
    TrellisPads.*           NeoTrellis keys and LEDs over I2C
    Oled.*                  SSD1306 display driver, sends a picture in 8 strips
tools/
  render_wav.cpp            runs the same code on a computer and writes a WAV
  screen_preview.cpp        draws display frames on a computer
```
