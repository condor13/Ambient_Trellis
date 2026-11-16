// Host test: runs the same sequencer + audio engine as the Pico and writes a
// WAV file, so the sound can be checked without hardware.
//
//   g++ -O2 -std=c++17 -I AmbientTrellis/src tools/render_wav.cpp AmbientTrellis/src/*/*.cpp -o render_wav
//   ./render_wav out.wav [seconds] [reverbWet 0-256] [stop-at-second, -1 = never] [pluck density 0-16]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "audio/AudioEngine.h"
#include "music/Sequencer.h"

static void put32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

int main(int argc, char **argv) {
  const char *path = argc > 1 ? argv[1] : "out.wav";
  int seconds = argc > 2 ? atoi(argv[2]) : 120;
  int stopAt = argc > 4 ? atoi(argv[4]) : -1;

  static AudioEngine engine;
  static Sequencer seq;
  engine.init(12345);
  seq.begin(&engine, 777);
  if (argc > 3) engine.params.reverbWet = (uint32_t)atoi(argv[3]);
  if (argc > 5) seq.setDensity((uint8_t)atoi(argv[5]));
  seq.start(0);

  const uint32_t blocks = (uint32_t)seconds * cfg::SAMPLE_RATE / cfg::BLOCK;
  std::vector<int16_t> pcm;
  pcm.reserve(blocks * cfg::BLOCK * 2);
  int16_t buf[cfg::BLOCK * 2];

  uint32_t lastCount = 0;
  int peak = 0;
  long clipped = 0;
  uint32_t samplesDone = 0;
  for (uint32_t b = 0; b < blocks; b++) {
    uint32_t nowMs = (uint32_t)((uint64_t)samplesDone * 1000 / cfg::SAMPLE_RATE);
    if (stopAt >= 0 && nowMs == (uint32_t)stopAt * 1000) seq.stop();
    seq.tick(nowMs);
    if (seq.chordCount() != lastCount) {
      lastCount = seq.chordCount();
      uint8_t n[8];
      int k = seq.notes(n);
      printf("%6.1fs  %-8s", nowMs / 1000.0, seq.chordName());
      for (int i = 0; i < k; i++) printf(" %d", n[i]);
      printf("\n");
    }
    engine.render(buf, cfg::BLOCK);
    samplesDone += cfg::BLOCK;
    for (int i = 0; i < cfg::BLOCK * 2; i++) {
      int a = abs(buf[i]);
      if (a > peak) peak = a;
      if (a >= 32000) clipped++;
      pcm.push_back(buf[i]);
    }
  }

  FILE *f = fopen(path, "wb");
  if (!f) { perror(path); return 1; }
  uint32_t dataBytes = (uint32_t)pcm.size() * 2;
  fwrite("RIFF", 1, 4, f); put32(f, 36 + dataBytes); fwrite("WAVE", 1, 4, f);
  fwrite("fmt ", 1, 4, f); put32(f, 16); put16(f, 1); put16(f, 2);
  put32(f, cfg::SAMPLE_RATE); put32(f, cfg::SAMPLE_RATE * 4); put16(f, 4); put16(f, 16);
  fwrite("data", 1, 4, f); put32(f, dataBytes);
  fwrite(pcm.data(), 2, pcm.size(), f);
  fclose(f);

  printf("peak %d (%.1f dBFS), samples near full scale: %ld\n", peak,
         20.0 * __builtin_log10(peak / 32767.0), clipped);
  return 0;
}
