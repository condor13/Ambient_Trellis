// Lock-free single-producer / single-consumer queue.
// Core 0 (sequencer) pushes, core 1 (audio) pops. T is the item type
// (a packed 32-bit event by default, or a small struct).
#pragma once
#include <stdint.h>

template <uint32_t N, typename T = uint32_t>
class EventQueue {
  static_assert((N & (N - 1)) == 0, "N must be a power of two");

 public:
  bool push(const T &v) {
    uint32_t h = head_;
    if (h - tail_ >= N) return false;   // full
    buf_[h & (N - 1)] = v;
    __sync_synchronize();               // data before index
    head_ = h + 1;
    return true;
  }

  bool pop(T &v) {
    uint32_t t = tail_;
    if (t == head_) return false;       // empty
    __sync_synchronize();
    v = buf_[t & (N - 1)];
    __sync_synchronize();
    tail_ = t + 1;
    return true;
  }

 private:
  T buf_[N];
  volatile uint32_t head_ = 0;
  volatile uint32_t tail_ = 0;
};
