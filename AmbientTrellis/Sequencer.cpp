#include "Sequencer.h"
#include <string.h>

namespace {

struct Scale {
  const char *name;
  uint8_t steps[7];
  // How much each chord degree is liked (0 = never used).
  uint8_t weight[7];
};

const Scale SCALES[Sequencer::NUM_SCALES] = {
  //            I  II III IV  V  VI VII
  {"Minor",  {0, 2, 3, 5, 7, 8, 10}, {4, 1, 3, 3, 2, 3, 2}},
  {"Dorian", {0, 2, 3, 5, 7, 9, 10}, {4, 3, 2, 3, 2, 1, 2}},
  {"Major",  {0, 2, 4, 5, 7, 9, 11}, {4, 3, 2, 3, 2, 3, 0}},
  {"Lydian", {0, 2, 4, 6, 7, 9, 11}, {4, 3, 2, 0, 2, 2, 1}},
};

const char *NOTE_NAMES[12] = {"C", "C#", "D", "Eb", "E", "F",
                              "F#", "G", "Ab", "A", "Bb", "B"};

constexpr int UPPER_LO = 50;   // D3
constexpr int UPPER_HI = 76;   // E5
constexpr int BASS_LO = 33;    // A1
constexpr int BASS_HI = 45;    // A2
constexpr int HOME_CENTER = 62;

// Pitch class of scale degree d (wraps past the octave), relative to the root.
inline int degPc(const Scale &s, int d) { return s.steps[d % 7]; }

void sortNotes(uint8_t *n, int count) {
  for (int i = 1; i < count; i++)
    for (int j = i; j > 0 && n[j - 1] > n[j]; j--) {
      uint8_t t = n[j]; n[j] = n[j - 1]; n[j - 1] = t;
    }
}

}  // namespace

const char *Sequencer::noteName(uint8_t pc) { return NOTE_NAMES[pc % 12]; }
const char *Sequencer::scaleName() const { return SCALES[scale_].name; }

void Sequencer::begin(AudioEngine *engine, uint32_t seed) {
  eng_ = engine;
  rng_.seed(seed);
  for (auto &p : pend_) p.used = false;
}

int Sequencer::notes(uint8_t *out) const {
  for (int i = 0; i < nCur_; i++) out[i] = cur_[i];
  return nCur_;
}

void Sequencer::start(uint32_t nowMs) {
  if (playing_) return;
  playing_ = true;
  degree_ = 0;                     // always start at home
  playChord(degree_, nowMs);
}

void Sequencer::stop() {
  if (!playing_) return;
  playing_ = false;
  for (auto &p : pend_) p.used = false;
  nCur_ = 0;
  eng_->events.push(packEvent(EV_ALL_OFF));   // notes fade out with their release
  strcpy(name_, "-");
}

void Sequencer::nextChordNow(uint32_t nowMs) {
  if (!playing_) return;
  degree_ = pickNextDegree();
  playChord(degree_, nowMs);
}

void Sequencer::tick(uint32_t nowMs) {
  if (!playing_) return;

  // Strummed note-ons that are due
  for (auto &p : pend_) {
    if (p.used && (int32_t)(nowMs - p.at) >= 0) {
      eng_->events.push(packEvent(EV_NOTE_ON, p.note, p.vel));
      p.used = false;
    }
  }

  if ((int32_t)(nowMs - nextChangeMs_) >= 0) {
    degree_ = pickNextDegree();
    playChord(degree_, nowMs);
  }
}

int Sequencer::commonTones(int a, int b) {
  const Scale &s = SCALES[scale_];
  int n = 0;
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++)
      if (degPc(s, a + 2 * i) == degPc(s, b + 2 * j)) n++;
  return n;
}

// Weighted random choice. Low drift = stay close to home and to chords that
// share notes with the current one. High drift = wander freely.
int Sequencer::pickNextDegree() {
  const Scale &s = SCALES[scale_];
  int w[7];
  int total = 0;
  for (int d = 0; d < 7; d++) {
    w[d] = 0;
    if (d == degree_ || s.weight[d] == 0) continue;
    w[d] = s.weight[d] * 4 + commonTones(d, degree_) * (17 - drift_) / 2;
    if (d == 0) w[d] += 16 - drift_;
    total += w[d];
  }
  if (total == 0) return 0;
  int r = (int)rng_.below((uint32_t)total);
  for (int d = 0; d < 7; d++) {
    if (r < w[d]) return d;
    r -= w[d];
  }
  return 0;
}

// Pitch classes of the upper voices. The bass plays the chord root, so the
// upper voices are 3-5-7 plus either the root or (sometimes) the 9th.
void Sequencer::buildChord(int degree, uint8_t *pcs, int &nPcs, bool &add9) {
  const Scale &s = SCALES[scale_];
  int r  = degPc(s, degree);
  int p3 = degPc(s, degree + 2);
  int p5 = degPc(s, degree + 4);
  int p7 = degPc(s, degree + 6);
  int p9 = degPc(s, degree + 1);

  // Only a whole-tone 9th sounds dreamy; a minor 9th would clash.
  bool nineOk = ((p9 - r + 12) % 12) == 2;
  add9 = nineOk && rng_.below(100) < 35;

  nPcs = 0;
  pcs[nPcs++] = (uint8_t)((add9 ? p9 : r) + root_) % 12;
  pcs[nPcs++] = (uint8_t)(p3 + root_) % 12;
  pcs[nPcs++] = (uint8_t)(p5 + root_) % 12;
  pcs[nPcs++] = (uint8_t)(p7 + root_) % 12;
}

void Sequencer::makeName(int degree, bool add9) {
  const Scale &s = SCALES[scale_];
  int r = degPc(s, degree);
  int i3 = (degPc(s, degree + 2) - r + 12) % 12;
  int i5 = (degPc(s, degree + 4) - r + 12) % 12;
  int i7 = (degPc(s, degree + 6) - r + 12) % 12;

  const char *q;
  if      (i3 == 4 && i5 == 7 && i7 == 11) q = add9 ? "maj9" : "maj7";
  else if (i3 == 3 && i5 == 7 && i7 == 10) q = add9 ? "m9" : "m7";
  else if (i3 == 4 && i5 == 7 && i7 == 10) q = add9 ? "9" : "7";
  else if (i3 == 3 && i5 == 6 && i7 == 10) q = "m7b5";
  else if (i3 == 4 && i5 == 6 && i7 == 11) q = "maj7#11";
  else                                     q = "";

  strcpy(name_, NOTE_NAMES[(r + root_) % 12]);
  strcat(name_, q);
}

void Sequencer::playChord(int degree, uint32_t nowMs) {
  // Anything still waiting from the previous chord goes out now.
  for (auto &p : pend_) {
    if (p.used) {
      eng_->events.push(packEvent(EV_NOTE_ON, p.note, p.vel));
      p.used = false;
    }
  }

  uint8_t pcs[5];
  int nPcs;
  bool add9;
  buildChord(degree, pcs, nPcs, add9);
  makeName(degree, add9);

  // Upper voices: each note as close as possible to the current centre.
  uint8_t up[5];
  for (int i = 0; i < nPcs; i++) {
    int below = center_ - ((center_ - pcs[i]) % 12 + 12) % 12;   // <= centre
    int n = (center_ - below <= below + 12 - center_) ? below : below + 12;
    while (n < UPPER_LO) n += 12;
    while (n > UPPER_HI) n -= 12;
    up[i] = (uint8_t)n;
  }
  sortNotes(up, nPcs);
  // Avoid semitone rubs inside the chord: move one note an octave away.
  for (int i = 1; i < nPcs; i++) {
    if (up[i] - up[i - 1] == 1) {
      if (up[i] + 12 <= UPPER_HI) up[i] += 12;
      else if (up[i - 1] - 12 >= UPPER_LO) up[i - 1] -= 12;
    }
  }
  sortNotes(up, nPcs);

  int sum = 0;
  for (int i = 0; i < nPcs; i++) sum += up[i];
  center_ = (sum / nPcs * 3 + HOME_CENTER) / 4;   // drift, but stay near home

  // Bass: the chord root, close to the previous bass note.
  int bpc = (degPc(SCALES[scale_], degree) + root_) % 12;
  int b = BASS_LO + ((bpc - BASS_LO) % 12 + 12) % 12;   // lowest in range
  if (b + 12 <= BASS_HI && (b + 12 - bass_) < (bass_ - b)) b += 12;
  bass_ = b;

  uint8_t next[MAX_NOTES];
  int nNext = 0;
  next[nNext++] = (uint8_t)b;
  for (int i = 0; i < nPcs; i++) {
    bool dup = false;
    for (int j = 0; j < nNext; j++) if (next[j] == up[i]) dup = true;
    if (!dup) next[nNext++] = up[i];
  }

  // Release notes that are not in the new chord.
  for (int i = 0; i < nCur_; i++) {
    bool keep = false;
    for (int j = 0; j < nNext; j++) if (next[j] == cur_[i]) keep = true;
    if (!keep) eng_->events.push(packEvent(EV_NOTE_OFF, cur_[i]));
  }

  // Start new notes after a short breath: bass first, the others strummed in slowly.
  int slot = 0;
  for (int j = 0; j < nNext; j++) {
    bool held = false;
    for (int i = 0; i < nCur_; i++) if (cur_[i] == next[j]) held = true;
    if (held) continue;
    uint8_t vel = (j == 0) ? 92 : (uint8_t)rng_.range(62, 82);
    uint32_t delay = (j == 0) ? 1100 : (uint32_t)rng_.range(1400, 3400);
    pend_[slot++] = Pending{next[j], vel, nowMs + delay, true};
  }

  for (int j = 0; j < nNext; j++) cur_[j] = next[j];
  nCur_ = nNext;

  uint32_t chordMs = 60000u * 4u * bars_ / (bpm_ ? bpm_ : 60);
  nextChangeMs_ = nowMs + chordMs;
  count_++;
}
