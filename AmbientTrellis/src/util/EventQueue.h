// Lock-free single-producer / single-consumer queue.
// Core 0 (sequencer) pushes, core 1 (audio) pops.
#pragma once
#include <stdint.h>

template <uint32_t N>
class EventQueue {
  static_assert((N & (N - 1)) == 0, "N must be a power of two");

 public:
  bool push(uint32_t v) {
    uint32_t h = head_;
    if (h - tail_ >= N) return false;   // full
    buf_[h & (N - 1)] = v;
    __sync_synchronize();               // data before index
    head_ = h + 1;
    return true;
  }

  bool pop(uint32_t &v) {
    uint32_t t = tail_;
    if (t == head_) return false;       // empty
    __sync_synchronize();
    v = buf_[t & (N - 1)];
    __sync_synchronize();
    tail_ = t + 1;
    return true;
  }

 private:
  uint32_t buf_[N];
  volatile uint32_t head_ = 0;
  volatile uint32_t tail_ = 0;
};
