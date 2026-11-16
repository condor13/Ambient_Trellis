// Ambient Trellis - step 4: generative pads, reverb, plucks, NeoTrellis pond.
//
// Core 0: sequencer, pads and LEDs (display and menu later)
// Core 1: audio engine -> PCM5102A over I2S
//
// Board: Raspberry Pi Pico (Arduino-Pico core by Earle Philhower)
// Tools -> CPU Speed: 250 MHz (overclock)
// Tools -> USB Stack: Pico SDK
// Library: Adafruit seesaw Library (for the NeoTrellis)
//
// The code is organised in src/:
//   src/Config.h   pins and audio constants
//   src/util/      small helpers (random numbers, cross-core queue, sine table, RAM placement)
//   src/audio/     everything on core 1: synths, effects, mixer
//   src/music/     the generative sequencer (core 0)
//   src/ui/        what the LEDs show (plain logic, core 0)
//   src/hw/        hardware drivers: NeoTrellis (core 0)
// docs/HOW_IT_WORKS.md explains the math.
//
// Controls for now:
//   The box starts playing by itself.
//   Touch a pad = pluck a note of the current chord (glows blue).
//   SHIFT (GP6) = jump to the next chord (until the menu exists).
//   Serial Monitor (115200) shows chords, plucks and the audio CPU load.

#ifdef USE_TINYUSB
#include <Adafruit_TinyUSB.h>   // needed when Tools -> USB Stack is "Adafruit TinyUSB"
#endif
#include <I2S.h>
#include <Wire.h>
#include "src/Config.h"
#include "src/audio/AudioEngine.h"
#include "src/hw/TrellisPads.h"
#include "src/music/Sequencer.h"
#include "src/ui/PondLeds.h"

AudioEngine engine;
Sequencer seq;
TrellisPads pads;
PondLeds pond;
I2S i2s(OUTPUT);

volatile bool engineReady = false;
volatile uint32_t audioUsMax = 0;   // longest block render time (µs)

// ---------------------------------------------------------------- core 0

void setup() {
  Serial.begin(115200);
  pinMode(cfg::PIN_SHIFT, INPUT_PULLUP);

  // I2C bus 1 on GP26/27: NeoTrellis now, OLED and LIS3DH later.
  Wire1.setSDA(cfg::PIN_SDA);
  Wire1.setSCL(cfg::PIN_SCL);
  Wire1.begin();
  Wire1.setClock(400000);

  while (!engineReady) delay(1);    // wait for core 1 to set up the engine

  if (!pads.begin()) Serial.println("NeoTrellis not found (check I2C wiring). Playing without pads.");
  Wire1.setClock(400000);           // the seesaw library may reset it

  seq.begin(&engine, rp2040.hwrand32());
  seq.start(millis());
  pond.begin(rp2040.hwrand32(), millis());
}

// Pads: read touches, each one plucks a note.
static void taskKeys(uint32_t now) {
  static uint32_t last = 0;
  if (now - last < 20) return;
  last = now;
  pads.poll();
  uint8_t pad;
  while (pads.nextPress(pad)) seq.touch(pad);
}

// LEDs: copy what is sounding from the engine, let the pond draw it.
static void taskLeds(uint32_t now) {
  static uint32_t last = 0;
  if (now - last < 33) return;      // ~30 frames per second
  last = now;

  PondLeds::Input in;
  for (int i = 0; i < PondLeds::PAD_SOURCES; i++) {
    in.padNote[i] = engine.pads.voiceNote[i];
    in.padLevel[i] = engine.pads.voiceLevel[i];
  }
  for (int i = 0; i < PondLeds::PLUCK_SOURCES; i++) {
    in.pluckNote[i] = engine.plucks.voiceNote[i];
    in.pluckLevel[i] = engine.plucks.voiceLevel[i];
    in.pluckTag[i] = engine.plucks.voiceTag[i];
  }
  Rgb colours[PondLeds::PADS];
  pond.update(in, now, colours);
  pads.show(colours);
}

// SHIFT: next chord (temporary, until the menu exists)
static void taskShift(uint32_t now) {
  static bool lastKey = HIGH;
  static uint32_t keyMs = 0;
  bool key = digitalRead(cfg::PIN_SHIFT);
  if (key != lastKey && now - keyMs > 30) {
    keyMs = now;
    lastKey = key;
    if (key == LOW) seq.nextChordNow(now);
  }
}

// Serial Monitor: chords, plucks, audio load
static void taskSerial(uint32_t now) {
  static uint32_t shown = 0;
  if (seq.chordCount() != shown) {
    shown = seq.chordCount();
    uint8_t n[Sequencer::MAX_NOTES];
    int k = seq.notes(n);
    Serial.printf("%s %s  |  %-8s", Sequencer::noteName(seq.root()), seq.scaleName(),
                  seq.chordName());
    for (int i = 0; i < k; i++) Serial.printf(" %s%d", Sequencer::noteName(n[i]), n[i] / 12 - 1);
    Serial.println();
  }

  static uint32_t plucksShown = 0;
  if (seq.pluckCount() != plucksShown) {
    plucksShown = seq.pluckCount();
    uint8_t p = seq.lastPluckNote();
    Serial.printf("    pluck %s%d\n", Sequencer::noteName(p), p / 12 - 1);
  }

  // One block is 1000 µs of audio.
  static uint32_t loadMs = 0;
  if (now - loadMs >= 5000) {
    loadMs = now;
    uint32_t us = audioUsMax;
    audioUsMax = 0;
    Serial.printf("audio load (worst block): %lu%%  @ %lu MHz\n",
                  (unsigned long)(us / 10), (unsigned long)(rp2040.f_cpu() / 1000000));
  }
}

void loop() {
  uint32_t now = millis();
  seq.tick(now);
  taskKeys(now);
  taskLeds(now);
  taskShift(now);
  taskSerial(now);
}

// ---------------------------------------------------------------- core 1

void setup1() {
  engine.init(rp2040.hwrand32());

  i2s.setBCLK(cfg::PIN_I2S_BCK);    // LRCK = BCK + 1
  i2s.setDATA(cfg::PIN_I2S_DIN);
  i2s.setBitsPerSample(16);
  i2s.setBuffers(6, 128);
  i2s.begin(cfg::SAMPLE_RATE);

  engineReady = true;
}

void loop1() {
  static int16_t buf[cfg::BLOCK * 2];

  uint32_t t0 = micros();
  engine.render(buf, cfg::BLOCK);
  uint32_t dt = micros() - t0;
  if (dt > audioUsMax) audioUsMax = dt;

  for (int i = 0; i < cfg::BLOCK; i++) {
    i2s.write16(buf[2 * i], buf[2 * i + 1]);   // waits while the DMA buffer is full
  }
}
