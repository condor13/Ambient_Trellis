// Ambient Trellis - step 6: generative pads, reverb, plucks, NeoTrellis pond,
// SHIFT menu, OLED koi pond.
//
// Core 0: sequencer, pads, LEDs, menu, display
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
//   src/app/       the 16 settings and what they mean (core 0)
//   src/ui/        what the LEDs show: pond and menu (plain logic, core 0)
//   src/hw/        hardware drivers: NeoTrellis, OLED (core 0)
// docs/HOW_IT_WORKS.md explains the math.
//
// Controls:
//   The box starts playing by itself.
//   Touch a pad = pluck a note of the current chord (glows blue).
//   SHIFT (GP6) = open the menu / back / close (see README).
//   Serial Monitor (115200) shows chords, plucks, menu changes and the audio load.

#ifdef USE_TINYUSB
#include <Adafruit_TinyUSB.h>   // needed when Tools -> USB Stack is "Adafruit TinyUSB"
#endif
#include <I2S.h>
#include <Wire.h>
#include "src/Config.h"
#include "src/app/Settings.h"
#include "src/audio/AudioEngine.h"
#include "src/hw/Oled.h"
#include "src/hw/TrellisPads.h"
#include "src/music/Sequencer.h"
#include "src/ui/Canvas.h"
#include "src/ui/Menu.h"
#include "src/ui/PondLeds.h"
#include "src/ui/Screen.h"

AudioEngine engine;
Sequencer seq;
TrellisPads pads;
PondLeds pond;
Menu menu;
Settings settings;
Oled oled;
Screen screen;
Canvas canvas;
I2S i2s(OUTPUT);

uint32_t sleepAtMs = 0;             // 0 = no sleep timer running

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
  Wire1.setClock(cfg::I2C_HZ);

  while (!engineReady) delay(1);    // wait for core 1 to set up the engine

  if (!pads.begin()) Serial.println("NeoTrellis not found (check I2C wiring). Playing without pads.");
  Wire1.setClock(cfg::I2C_HZ);      // the seesaw library may reset it
  if (!oled.begin(cfg::OLED_ADDR)) Serial.println("OLED not found at 0x3C. Playing without display.");

  seq.begin(&engine, rp2040.hwrand32());
  pond.begin(rp2040.hwrand32(), millis());
  settings.setDefaults();
  settings.applyAll(engine, seq, pond, millis());
  oled.setContrast(settings.ledBrightness());
  screen.begin(rp2040.hwrand32(), millis());
  screen.overlay("AMBIENT", "TRELLIS", millis(), 2500);
}

// Start the sleep timer (when SLEEP is set, or when playing starts).
static void armSleep(uint32_t now) {
  uint32_t min = settings.sleepMinutes();
  // (| 1 so a running timer is never exactly 0, which means "off")
  sleepAtMs = min ? ((now + min * 60000u) | 1u) : 0;
}

// A short text over the pond (and in the Serial Monitor).
static void showText(const char *title, const char *value) {
  screen.overlay(title, value, millis());
  if (value) Serial.printf("[%s %s]\n", title, value);
  else Serial.printf("[%s]\n", title);
}

static void showParam(Param p) {
  char buf[24];
  settings.valueText(p, buf, sizeof(buf));
  showText(Settings::longName(p), buf);
}

// A menu press or SHIFT did something: apply it and show it.
static bool changedInMenu = false;
static Param lastChanged = P_PLAY;

static void handleMenu(const Menu::Result &r, uint32_t now) {
  if (r.opened && !menu.onValuePage()) changedInMenu = false;
  if (r.changed) {
    settings.apply(r.param, engine, seq, pond, now);
    if (r.param == P_SLEEP || (r.param == P_PLAY && settings.playing)) armSleep(now);
    if (r.param == P_LITE) oled.setContrast(settings.ledBrightness());
    changedInMenu = true;
    lastChanged = r.param;
    showParam(r.param);
  } else if (r.opened || r.back) {
    if (menu.onValuePage()) showParam(menu.page());
    else showText("MENU", nullptr);
  }
  if (r.closed) {
    Serial.println("[MENU CLOSED]");
    if (changedInMenu) showParam(lastChanged);   // remind what changed, over the pond
  }
}

// Pads: in the menu they choose settings, otherwise each touch plucks a note.
static uint32_t touchPlucks = 0;    // plucks that already made their own ripple
static void taskKeys(uint32_t now) {
  static uint32_t last = 0;
  if (now - last < 20) return;
  last = now;
  pads.poll();
  uint8_t pad;
  while (pads.nextPress(pad)) {
    if (menu.isOpen()) {
      handleMenu(menu.press(pad, settings, now), now);
    } else {
      seq.touch(pad);
      screen.ripplePad(pad);
      touchPlucks++;
    }
  }
  handleMenu(menu.tick(now), now);   // 10 s timeout
}

// LEDs: the pond keeps running underneath, the menu is drawn on top when open.
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
  menu.draw(settings, now, colours);   // over the pond while open (and while closing)
  pads.show(colours);
}

// Display: ~20 pictures per second, each sent in 8 strips between other work.
static void taskDisplay(uint32_t now) {
  if (!oled.ok()) return;
  oled.sendSome();                   // one strip per loop

  // Ripples for chord changes and generative plucks
  static uint32_t chords = 0, plucks = 0;
  if (seq.chordCount() != chords) {
    chords = seq.chordCount();
    screen.rippleRandom(22);
  }
  while (plucks != seq.pluckCount()) {
    plucks++;
    if (touchPlucks) touchPlucks--;  // a touch: it already rippled at its pad
    else screen.rippleRandom(10);
  }

  static uint32_t last = 0;
  if (now - last < 50) return;
  last = now;
  screen.draw(canvas, settings, menu, now);
  oled.startFrame(canvas);
}

// SHIFT key: open / close the menu.
static void taskShift(uint32_t now) {
  static bool lastKey = HIGH;
  static uint32_t keyMs = 0;
  bool key = digitalRead(cfg::PIN_SHIFT);
  if (key != lastKey && now - keyMs > 30) {
    keyMs = now;
    lastKey = key;
    if (key == LOW) handleMenu(menu.shift(now), now);
  }
}

// Sleep timer: fade out and stop.
static void taskSleep(uint32_t now) {
  if (sleepAtMs && settings.playing && (int32_t)(now - sleepAtMs) >= 0) {
    sleepAtMs = 0;
    settings.playing = false;
    settings.apply(P_PLAY, engine, seq, pond, now);
    showText("SLEEP", "GOOD NIGHT");
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
  taskShift(now);
  taskKeys(now);
  taskLeds(now);
  taskDisplay(now);
  taskSleep(now);
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
