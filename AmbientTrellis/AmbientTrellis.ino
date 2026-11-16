// Ambient Trellis - step 1: generative FM pad chords.
//
// Core 0: chord sequencer (+ pads, LEDs, display later)
// Core 1: audio engine -> PCM5102A over I2S
//
// Board: Raspberry Pi Pico (Arduino-Pico core by Earle Philhower)
// Tools -> CPU Speed: 250 MHz (overclock) recommended (133 MHz also works for step 1)
//
// Step 1 test controls:
//   The box starts playing by itself.
//   SHIFT (GP6) = jump to the next chord now.
//   Serial Monitor (115200) shows the chords and the audio CPU load.

#include <I2S.h>
#include "Config.h"
#include "AudioEngine.h"
#include "Sequencer.h"

AudioEngine engine;
Sequencer seq;
I2S i2s(OUTPUT);

volatile bool engineReady = false;
volatile uint32_t audioUsMax = 0;   // longest block render time (µs)

// ---------------------------------------------------------------- core 0

void setup() {
  Serial.begin(115200);
  pinMode(cfg::PIN_SHIFT, INPUT_PULLUP);

  while (!engineReady) delay(1);    // wait for core 1 to set up the engine

  seq.begin(&engine, rp2040.hwrand32());
  seq.start(millis());
}

void loop() {
  uint32_t now = millis();
  seq.tick(now);

  // SHIFT: next chord (temporary, until the menu exists)
  static bool lastKey = HIGH;
  static uint32_t keyMs = 0;
  bool key = digitalRead(cfg::PIN_SHIFT);
  if (key != lastKey && now - keyMs > 30) {
    keyMs = now;
    lastKey = key;
    if (key == LOW) seq.nextChordNow(now);
  }

  // Print each new chord
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

  // Audio load every 5 s. One block is 1000 µs of audio.
  static uint32_t loadMs = 0;
  if (now - loadMs >= 5000) {
    loadMs = now;
    uint32_t us = audioUsMax;
    audioUsMax = 0;
    Serial.printf("audio load (worst block): %lu%%  @ %lu MHz\n",
                  (unsigned long)(us / 10), (unsigned long)(rp2040.f_cpu() / 1000000));
  }
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
