# Ambient Trellis

A small battery-powered ambient box that plays itself. Slow FM pad chords swell
in and glide from one to the next, while soft, high plucked notes fall like
raindrops. Put it next to the bed and let it play you to sleep.

The pads don't pick chords. Touching them plucks a note from the current chord,
like touching water, so you can play along but never hit a wrong note. Without
any interaction the box is a generative player.

Written in C++ for the Arduino-Pico core (Raspberry Pi Pico / RP2040).

## Hardware

A boxy enclosure with the 4×4 pads on top, and the SHIFT key and display on the side.

| Part | Notes |
|---|---|
| Raspberry Pi Pico (RP2040) | 8 MB flash |
| PCM5102A I2S DAC | line/headphone out |
| Adafruit NeoTrellis 4×4 | silicone pads + NeoPixels (seesaw, I2C `0x2E`) |
| 0.96" OLED, SSD1306 128×64 | I2C `0x3C` |
| LIS3DH accelerometer | I2C `0x18` / `0x19`, used for tilt and shake |
| Single key | SHIFT |
| Battery | |

### Pin map

| Function | GPIO | Physical pin |
|---|---|---|
| I2S BCK | GP18 | 24 |
| I2S LRCK | GP19 | 25 |
| I2S DIN | GP20 | 26 |
| I2C1 SDA (NeoTrellis, OLED, LIS3DH) | GP26 | 31 |
| I2C1 SCL | GP27 | 32 |
| SHIFT key (to GND, internal pull-up) | GP6 | 9 |

LRCK must be BCK + 1. The I2C devices are on the Pico's second bus, so the code
uses `Wire1`.

### PCM5102A board jumpers

| Jumper | Function | Setting |
|---|---|---|
| 1 | FLT | L |
| 2 | DEMP | L |
| 3 | XSMT (soft mute) | **H** |
| 4 | FMT | L (I2S) |
| SCK | system clock | tied to GND (internal PLL) |

The DAC wiring and jumpers were checked with a 440 Hz CircuitPython test tone.

## Sound

### Chord pads (bass and mid)

- FM voices: one modulator and two detuned carriers per note (left and right),
  for width and shimmer.
- Long volume swells, with brightness (FM index) slowly rising as each chord blooms.
- Very slow LFOs on brightness and detune, so a held chord never sits still.
- Chords are built from the current scale (7th chords, sometimes with a 9th).
  Notes that two chords share keep sounding, and the other notes are placed
  as close as possible to the previous chord, so changes glide.
- Chord changes follow the tempo: one chord every 1, 2, 4 or 8 bars (default 4)
  at 53–68 BPM.

### Plucks (high)

- Karplus-Strong plucked strings: soft, gentle and high.
- They play random notes from the current chord. **Density** sets how often.
- Short delay on the plucks only (forward, ping-pong, reverse, reverse ping-pong).
  It runs at half the sample rate to save memory, which also makes the repeats darker.

### Master

- Reverb with a long, dark tail.
- Soft compressor and limiter, so many notes plus reverb never distort.
  It's written as a separate, reusable file.

## LEDs

### Play mode: a pond of notes

The 16 pads are 16 places for notes, not fixed keys. Each sounding chord note
takes a random free pad and glows in its note colour, fading in and out with
the actual level of the sound. When the chord changes, the old notes fade out
while the new ones fade in on free pads.

| Note | Colour |
|---|---|
| C | red |
| D | green |
| E | yellow |
| F | orange |
| G | blue |
| A | turquoise |
| B | pink |

Sharps and flats are a mix of the two neighbouring colours (C# between red and green).

**Plucks** always glow **water blue**, never in their note colour, and fade out
with a fast flicker, like light on water. Generative plucks appear on a free
pad; **touching a pad** plucks a note of the current chord and that pad glows.

### Menu mode

- Settings are **red**.
- On a value page, every pad the parameter uses glows dimmed (80 of 255) and the chosen
  value is at full brightness. Four-option parameters light only 4 pads.
- Exceptions:
  - PLAY: green when playing, red when stopped
  - ROOT: note colours plus indigo scale pads
  - LITE: every pad shows its own brightness level, the current one in white

## SHIFT menu

1. Press SHIFT (GP6) to open the menu. The grid shows the 16 parameters in red.
2. Tap a parameter. The grid shows its value.
3. Tap a pad 1–16 to set the value. The box goes back to the parameter list.
4. Press SHIFT to exit. The menu also closes by itself after 10 seconds without a touch.

SHIFT + PLAY toggles start/stop directly. Stop lets the chord and reverb fade
out naturally, and Start fades the first chord in.

| | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| **Row 1** | PLAY | DENS | ROOT | DRFT |
| **Row 2** | VERB | LFO | BPM | BARS ⁽⁴⁾ |
| **Row 3** | ATK | REL | BRIT | VOL |
| **Row 4** | DLY | DMOD ⁽⁴⁾ | LITE | SLEEP |

⁽⁴⁾ = four options, so only 4 pads are lit.

| # | Parameter | Values |
|---|---|---|
| # | Parameter | Values | Default |
|---|---|---|---|
| 1 | PLAY | start / stop | playing |
| 2 | DENS | pluck density, sparse → dense | 4 (rare) |
| 3 | ROOT | pads 1–12 = C … B; pads 13–16 = scale: Minor, Dorian, Major, Lydian. Changing it moves to the new key's home chord right away. | D Dorian |
| 4 | DRFT | how far the progression wanders, 1 = stays home | 6 |
| 5 | VERB | reverb, dry → wet | 10 |
| 6 | LFO | very slow (~1 min) → faster (~2 s) | 5 (~24 s) |
| 7 | BPM | 53 → 68 | 60 |
| 8 | BARS | 1 / 2 / 4 / 8 bars per chord | 4 |
| 9 | ATK | chord swell time, 1 → 16 s | 7 s |
| 10 | REL | chord fade time, 0.5 → 15.5 s | 7.5 s |
| 11 | BRIT | pad brightness (FM amount) | 7 |
| 12 | VOL | master volume | 14 |
| 13 | DLY | pluck delay amount, 1 = off | 7 |
| 14 | DMOD | forward / ping-pong / reverse / reverse ping-pong | ping-pong |
| 15 | LITE | LED (and later display) brightness, very dim at the low end for the bedroom | 10 |
| 16 | SLEEP | 1–15 = fade out and stop after 10–150 minutes; 16 = never | never |

Settings will be saved to flash a few seconds after a change (step 8), so the
box starts the next day with the settings last used. Until then it starts with
the defaults.

## Motion (LIS3DH)

General behaviour, not a menu setting:

- **Tilt:** affects the randomness (how adventurous the progression and plucks are).
- **Shake:** to be decided, for example "stirring the pond" with a cascade of plucks.

## Display

- **Play mode:** a pond with koi drawn in code. They swim faster or slower with
  the LFO speed, and ripples spread when the chord changes. When something
  changes, a short text such as `REVERB 12` appears over the pond and fades out.
- **Menu:** a 4×4 grid of text labels that matches the pads. A value page
  shows the full parameter name and a value bar.

## Software design

- Arduino-Pico core (Earle Philhower), CPU overclocked to 250 MHz.
- Core 1: audio only, 32 kHz, 16-bit stereo, fixed-point (the RP2040 has no FPU).
- Core 0: chord sequencer, pads, LEDs, OLED, accelerometer, menu.
- The audio engine is plain C++ with no Arduino dependencies, so it can also be
  compiled on a computer to render a test WAV file.

**[docs/HOW_IT_WORKS.md](docs/HOW_IT_WORKS.md)** explains all the math: fixed-point
numbers, FM, the chord engine, Karplus-Strong strings, the reverb and more.

### Code layout

```
AmbientTrellis/
  AmbientTrellis.ino        setup and loop for both cores
  src/Config.h              pins, sample rate, block size
  src/util/                 random numbers, cross-core queue, sine table, speed macros
  src/audio/                core 1: AudioEngine (mixer), PadSynth, PluckSynth,
                            PluckDelay, Reverb, SoftComp, SoundParams, Events
  src/music/                core 0: Sequencer (chords, voice leading, plucks)
  src/app/                  core 0: Settings (the 16 menu parameters and what they mean)
  src/ui/                   core 0: PondLeds, Menu, Colour (what the pads show; plain logic)
  src/hw/                   core 0: TrellisPads (NeoTrellis driver)
tools/render_wav.cpp        renders the same code to a WAV file on a computer
```

### Libraries

- Raspberry Pi Pico/RP2040 board package (Earle F. Philhower)
- Adafruit seesaw Library
- Adafruit SSD1306 + Adafruit GFX
- Adafruit LIS3DH

## Building and uploading

1. Install **Arduino IDE 2**.
2. Under **File → Preferences → Additional boards manager URLs**, add
   `https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`.
3. In **Boards Manager**, install **Raspberry Pi Pico/RP2040** (Earle F. Philhower).
4. In **Library Manager**, install **Adafruit seesaw Library** (with its dependencies).
5. Open `AmbientTrellis/AmbientTrellis.ino`.
6. Select **Tools → Board → Raspberry Pi Pico**, **Tools → CPU Speed → 250 MHz**
   and **Tools → USB Stack → Pico SDK**.
7. For the first upload, hold **BOOTSEL** while plugging in the Pico, then upload.
   After that, the normal serial port works.

### Testing the sound on a computer

The audio engine, sequencer and LED logic also compile on a PC (everything
except `src/hw/`), and render a WAV file:

```
g++ -O2 -std=c++17 -I AmbientTrellis/src tools/render_wav.cpp AmbientTrellis/src/{audio,music,util,ui,app}/*.cpp -o render_wav
./render_wav out.wav 120
```

## Current state: step 5

- Generative FM pad chords, plate reverb, rare soft plucks with a pluck delay.
- **NeoTrellis pond:** chord notes glow in their note colours on random free
  pads; plucks glow water blue and flicker out. Blue ripple at start-up.
- **Touch a pad:** plucks a note of the current chord.
- **SHIFT menu** with all 16 parameters, including the sleep timer and 4 delay modes.
- Serial Monitor (115200 baud) shows chords, plucks, menu changes (the texts the
  display will show) and the audio CPU load.
- No display yet; settings are not saved yet.

## Roadmap

1. FM chord pads with generative progression at a fixed tempo
2. Reverb
3. Plucks and pluck delay
4. NeoTrellis: pond-of-notes LEDs, touch plucks
5. SHIFT menu
6. OLED: koi pond and menu
7. Tilt and shake
8. Settings saved to flash, sleep timer
