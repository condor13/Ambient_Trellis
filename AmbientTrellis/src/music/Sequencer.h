// Sequencer - the generative player. Runs on core 0.
//
// Chords: every few bars it picks the next chord from the scale, places its
// notes close to the previous chord (so changes glide) and sends note events to
// the audio engine. Notes shared by both chords simply keep sounding.
//
// Plucks: on every 8th note there is a small chance of a soft, high pluck,
// always a note of the current chord (so it can never sound wrong).
// Touching a pad plucks one too, right away.
#pragma once
#include <stdint.h>
#include "../audio/AudioEngine.h"
#include "../util/Rng.h"

class Sequencer {
 public:
  static constexpr int NUM_SCALES = 4;
  static constexpr int MAX_NOTES = 6;    // bass + up to 5 upper notes

  void begin(AudioEngine *engine, uint32_t seed);
  void tick(uint32_t nowMs);             // call often (every loop)

  void start(uint32_t nowMs);
  void stop();
  void nextChordNow(uint32_t nowMs);     // skip ahead (test helper)

  // A pad was touched: pluck a note of the current chord ("touching water").
  void touch(uint8_t pad);

  // Settings (menu values later)
  void setRoot(uint8_t pc)    { root_ = pc % 12; }
  void setScale(uint8_t s)    { scale_ = s % NUM_SCALES; }
  void setBpm(uint8_t bpm)    { bpm_ = bpm; applyTempo(); }
  void setBars(uint8_t bars)  { bars_ = bars; }
  void setDrift(uint8_t d)    { drift_ = d < 1 ? 1 : (d > 16 ? 16 : d); }
  void setDensity(uint8_t d)  { density_ = d > 16 ? 16 : d; }   // plucks, 0 = none

  bool playing() const        { return playing_; }
  const char *chordName() const { return name_; }
  const char *scaleName() const;
  uint8_t root() const        { return root_; }
  uint32_t chordCount() const { return count_; }
  uint32_t pluckCount() const { return pluckCount_; }
  uint8_t lastPluckNote() const { return lastSent_; }
  int notes(uint8_t *out) const;         // current chord notes (MIDI)

  static const char *noteName(uint8_t pc);

 private:
  void applyTempo();
  void playChord(int degree, uint32_t nowMs);
  void tickPlucks(uint32_t nowMs);
  uint8_t pickPluckNote();
  int  pickNextDegree();
  void buildChord(int degree, uint8_t *pcs, int &nPcs, bool &add9);
  void makeName(int degree, bool add9);
  int  commonTones(int a, int b);

  AudioEngine *eng_ = nullptr;
  Rng rng_;

  uint8_t root_ = 2;       // D
  uint8_t scale_ = 1;      // Dorian
  uint8_t bpm_ = 60;
  uint8_t bars_ = 4;
  uint8_t drift_ = 6;
  uint8_t density_ = 4;    // rare

  bool playing_ = false;
  int degree_ = 0;
  uint32_t nextChangeMs_ = 0;
  uint32_t count_ = 0;

  uint8_t cur_[MAX_NOTES];     // sounding notes
  int nCur_ = 0;
  int center_ = 62;            // where the upper voices sit
  int bass_ = 38;

  // note-ons waiting for their strum time
  struct Pending { uint8_t note, vel; uint32_t at; bool used; };
  Pending pend_[MAX_NOTES];

  // Plucks (own random numbers, so they don't change the chord progression)
  Rng pluckRng_;
  uint32_t nextEighthMs_ = 0;
  uint8_t lastPluck_ = 79;
  uint8_t lastSent_ = 0;
  uint32_t pluckCount_ = 0;
  Pending pluckPend_[4];

  char name_[12] = "-";
};
