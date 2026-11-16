# Ambient Trellis

A small battery-powered ambient machine: slow, evolving FM pads that swell in and
glide from chord to chord. Chords only. You play it on a 4×4 NeoTrellis, and it
can also drift through chord progressions by itself.

Written in C++ for the Arduino-Pico core (Raspberry Pi Pico / RP2040).

## Hardware

| Part | Notes |
|---|---|
| Raspberry Pi Pico (RP2040) | 8 MB flash |
| PCM5102A I2S DAC | line/headphone out |
| Adafruit NeoTrellis 4×4 | silicone pads + NeoPixels (seesaw, I2C `0x2E`) |
| 0.96" OLED, SSD1306 128×64 | I2C `0x3C` |
| LIS3DH accelerometer | I2C `0x18` / `0x19`, used for tilt and shake |
| Single key | used as SHIFT |
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

- FM voices with two detuned voices per note, for width and shimmer.
- Long volume swells, with brightness (FM index) slowly rising as each chord blooms.
- Very slow LFOs on brightness and detune, so a held chord never sits still.
- Reverb with a long, dark tail.
- Smooth chord changes: the next chord uses the inversion closest to the current
  one and crossfades in.

## Controls

### Play mode

| Row | Pads |
|---|---|
| 1–2 | The 7 chords of the scale (I ii iii IV V vi vii°) and a root + fifth drone |
| 3 | Colour: plain / sus2 / add9 / 7th |
| 4 | Voicing: close / open / low octave / high shimmer |

Pads glow and "breathe" with the level of the sound.

### SHIFT menu

Hold SHIFT to open the menu while held, or double-tap it to keep it open.

| Row | Pad 1 | Pad 2 | Pad 3 | Pad 4 |
|---|---|---|---|---|
| 1 | LATCH on/off | DRIFT on/off | ROOT (next) | SCALE (next) |
| 2 | Evolve: slow | · | · | fast |
| 3 | Attack/Release: slow | · | · | fast |
| 4 | LFO: slow | · | · | fast |

Rows 2–4 are 4-level bar graphs.

- **Latch:** the last chord keeps playing after you let go.
- **Drift:** the machine moves through chords on its own.
- **Evolve:** how fast brightness rises inside a chord, and how often Drift changes chord.

### Motion (LIS3DH)

Heavily smoothed, so movement bends the sound slowly:

- Tilt forward/back: brightness
- Tilt left/right: stereo width / reverb size
- Gentle shake: shimmer, or a nudge to the next chord in Drift mode
- Can be turned off from the menu

## Display

The OLED shows the root, scale and current chord name, with a live
oscilloscope underneath. When the menu is open it shows the parameter being
changed instead.

## Software design

- Arduino-Pico core (Earle Philhower), CPU overclocked to 250 MHz.
- Core 1: audio only, 32 kHz, 16-bit, fixed-point (the RP2040 has no FPU).
- Core 0: pads, LEDs, OLED, accelerometer, menu.
- Settings (root, scale, speeds, last chord) saved to flash.

### Libraries

- Raspberry Pi Pico/RP2040 board package (Earle F. Philhower)
- Adafruit seesaw Library
- Adafruit SSD1306 + Adafruit GFX
- Adafruit LIS3DH

## Roadmap

1. One FM chord swelling in through the DAC
2. Reverb
3. Pads with breathing LEDs
4. OLED: root, scale, oscilloscope
5. SHIFT menu
6. Drift mode (generative progressions)
7. Tilt and shake
8. Saving settings to flash
