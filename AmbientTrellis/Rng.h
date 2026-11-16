// Tiny, fast random numbers (xorshift32).
#pragma once
#include <stdint.h>

struct Rng {
  uint32_t s = 2463534242u;

  void seed(uint32_t v) { s = v ? v : 2463534242u; }

  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
  }

  // 0 .. n-1
  uint32_t below(uint32_t n) { return n ? next() % n : 0; }

  // lo .. hi (inclusive)
  int32_t range(int32_t lo, int32_t hi) { return lo + (int32_t)below((uint32_t)(hi - lo + 1)); }
};
