#include "Settings.h"
#include <math.h>
#include <stdio.h>
#include "../audio/AudioEngine.h"
#include "../music/Sequencer.h"
#include "../ui/PondLeds.h"

namespace {

const char *SHORT[P_COUNT] = {"PLAY", "DENS", "ROOT", "DRFT", "VERB", "LFO", "BPM", "BARS",
                              "ATK", "REL", "BRIT", "VOL", "DLY", "DMOD", "LITE", "SLEEP"};
const char *LONG[P_COUNT] = {"PLAY", "DENSITY", "ROOT", "DRIFT", "REVERB", "LFO", "TEMPO", "BARS",
                             "ATTACK", "RELEASE", "BRIGHT", "VOLUME", "DELAY", "DELAY MODE",
                             "LIGHT", "SLEEP"};
const char *SCALE_NAMES[4] = {"MINOR", "DORIAN", "MAJOR", "LYDIAN"};
const char *NOTE_NAMES[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
const char *DMOD_NAMES[4] = {"FORWARD", "PING-PONG", "REVERSE", "REV PING"};
const uint8_t BARS[4] = {1, 2, 4, 8};

// ---- What each value 1..16 means -------------------------------------------
uint32_t attackMs(uint8_t v)  { return 1000u * v; }                 // 1 .. 16 s
uint32_t releaseMs(uint8_t v) { return 500u + 1000u * (v - 1); }    // 0.5 .. 15.5 s
uint32_t brightness(uint8_t v) { return 2500u + 3300u * (v - 1); }  // FM index 2500 .. 52000
uint32_t volume(uint8_t v)    { return 16u * v; }                   // 16 .. 256
uint32_t reverbWet(uint8_t v) { return 17u * (v - 1); }             // 0 (dry) .. 255
uint32_t delayMix(uint8_t v)  { return 17u * (v - 1); }             // 0 (off) .. 255
uint8_t  bpm(uint8_t v)       { return (uint8_t)(52 + v); }         // 53 .. 68
// LFO period: 1 = one minute, 16 = two seconds, evenly spaced on a log scale
uint32_t lfoMs(uint8_t v) {
  return (uint32_t)(60000.0f * powf(2.0f / 60.0f, (float)(v - 1) / 15.0f));
}

}  // namespace

void Settings::setDefaults() {
  value[P_PLAY]  = 1;
  value[P_DENS]  = 4;     // rare plucks
  value[P_ROOT]  = 3;     // D
  value[P_DRFT]  = 6;
  value[P_VERB]  = 10;    // ~60% wet
  value[P_LFO]   = 5;     // ~24 s
  value[P_BPM]   = 8;     // 60 BPM
  value[P_BARS]  = 3;     // 4 bars
  value[P_ATK]   = 7;     // 7 s
  value[P_REL]   = 8;     // 7.5 s
  value[P_BRIT]  = 7;
  value[P_VOL]   = 14;
  value[P_DLY]   = 7;
  value[P_DMOD]  = 2;     // ping-pong
  value[P_LITE]  = 10;
  value[P_SLEEP] = 16;    // never
  scale = 1;              // Dorian
  playing = true;
}

uint8_t Settings::options(Param p) { return (p == P_BARS || p == P_DMOD) ? 4 : 16; }
const char *Settings::shortName(Param p) { return p < P_COUNT ? SHORT[p] : "?"; }
const char *Settings::longName(Param p) { return p < P_COUNT ? LONG[p] : "?"; }

uint8_t Settings::ledBrightness() const {
  // Squared, so the low end is very dim (bedroom at night): 1 -> 1, 16 -> 255
  uint32_t v = value[P_LITE];
  uint32_t b = v * v * 255u / 256u;
  return (uint8_t)(b < 1 ? 1 : b);
}

uint32_t Settings::sleepMinutes() const {
  uint8_t v = value[P_SLEEP];
  return v >= 16 ? 0 : 10u * v;
}

void Settings::valueText(Param p, char *buf, int size) const {
  uint8_t v = value[p];
  switch (p) {
    case P_PLAY:  snprintf(buf, size, "%s", playing ? "PLAYING" : "STOPPED"); break;
    case P_ROOT:  snprintf(buf, size, "%s %s", NOTE_NAMES[(v - 1) % 12], SCALE_NAMES[scale & 3]); break;
    case P_BPM:   snprintf(buf, size, "%d BPM", bpm(v)); break;
    case P_BARS:  snprintf(buf, size, "%d BAR%s", BARS[(v - 1) & 3], v == 1 ? "" : "S"); break;
    case P_ATK:   snprintf(buf, size, "%lu S", (unsigned long)(attackMs(v) / 1000)); break;
    case P_REL:   snprintf(buf, size, "%lu.%lu S", (unsigned long)(releaseMs(v) / 1000),
                           (unsigned long)(releaseMs(v) % 1000 / 100)); break;
    case P_LFO: {
      uint32_t ms = lfoMs(v);
      snprintf(buf, size, "%lu S", (unsigned long)((ms + 500) / 1000));
      break;
    }
    case P_DLY:   if (v == 1) snprintf(buf, size, "OFF"); else snprintf(buf, size, "%d", v); break;
    case P_DMOD:  snprintf(buf, size, "%s", DMOD_NAMES[(v - 1) & 3]); break;
    case P_SLEEP:
      if (sleepMinutes() == 0) snprintf(buf, size, "NEVER");
      else snprintf(buf, size, "%lu MIN", (unsigned long)sleepMinutes());
      break;
    default:      snprintf(buf, size, "%d", v); break;
  }
}

void Settings::apply(Param p, AudioEngine &eng, Sequencer &seq, PondLeds &pond, uint32_t nowMs) const {
  uint8_t v = value[p];
  SoundParams &s = eng.params;
  switch (p) {
    case P_PLAY:
      if (playing) seq.start(nowMs);
      else seq.stop();
      break;
    case P_DENS:  seq.setDensity(v); break;
    case P_ROOT:
      seq.setRoot((uint8_t)((v - 1) % 12));
      seq.setScale(scale);
      seq.goHome(nowMs);          // move to the new key now, from its home chord
      break;
    case P_DRFT:  seq.setDrift(v); break;
    case P_VERB:  s.reverbWet = reverbWet(v); break;
    case P_LFO:   s.lfoMs = lfoMs(v); break;
    case P_BPM:   seq.setBpm(bpm(v)); break;
    case P_BARS:  seq.setBars(BARS[(v - 1) & 3]); break;
    case P_ATK:   s.attackMs = attackMs(v); break;
    case P_REL:   s.releaseMs = releaseMs(v); break;
    case P_BRIT:  s.brightness = brightness(v); break;
    case P_VOL:   s.volume = volume(v); break;
    case P_DLY:   s.delayMix = delayMix(v); break;
    case P_DMOD:  s.delayMode = (uint32_t)((v - 1) & 3); break;
    case P_LITE:  pond.brightness = ledBrightness(); break;
    case P_SLEEP: break;          // handled by the app's sleep timer
    default: break;
  }
}

void Settings::applyAll(AudioEngine &eng, Sequencer &seq, PondLeds &pond, uint32_t nowMs) const {
  // Everything except PLAY and ROOT (those restart the music) ...
  for (int p = 0; p < P_COUNT; p++) {
    if (p == P_PLAY || p == P_ROOT) continue;
    apply((Param)p, eng, seq, pond, nowMs);
  }
  // ... then the key, without jumping, and start if playing.
  seq.setRoot((uint8_t)((value[P_ROOT] - 1) % 12));
  seq.setScale(scale);
  if (playing) seq.start(nowMs);
}
