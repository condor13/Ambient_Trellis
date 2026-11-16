// Optimize this file for speed (the Arduino-Pico default is size).
#pragma GCC optimize("O2")

#include "AudioEngine.h"
#include "../util/FastCode.h"

void AudioEngine::init(uint32_t seed) {
  pads.init(seed);
  plucks.init(seed * 2654435761u + 1);
  delay_.init();
  reverb_.init();
  comp_.setup(16000, 3, 9);
}

void AudioEngine::handleEvent(uint32_t ev) {
  switch (eventCmd(ev)) {
    case EV_NOTE_ON:  pads.noteOn(eventNote(ev), eventVel(ev)); break;
    case EV_NOTE_OFF: pads.noteOff(eventNote(ev)); break;
    case EV_ALL_OFF:  pads.releaseAll(); break;
    case EV_PLUCK:    plucks.pluck(eventNote(ev), eventVel(ev), params.pluckDecayMs); break;
  }
}

void AUDIO_FAST(AudioEngine::render)(int16_t *out, int frames) {
  // 1. Messages from core 0
  uint32_t ev;
  while (events.pop(ev)) handleEvent(ev);

  // 2. Settings for this block
  pads.control(params);
  delay_.setTime(params.delaySamples);
  delay_.feedback = (int32_t)params.delayFeedback;
  reverb_.decay = (int32_t)params.reverbDecay;

  const int32_t vol = (int32_t)params.volume;
  int32_t wet = (int32_t)params.reverbWet;
  if (wet > 256) wet = 256;
  const int32_t dry = 256 - wet / 2;
  const int32_t pDry = (int32_t)params.pluckDry;
  const int32_t pSend = (int32_t)params.pluckSend;
  const int32_t dMix = (int32_t)params.delayMix;

  // 3. Instruments
  for (int i = 0; i < frames; i++) padL_[i] = padR_[i] = plkL_[i] = plkR_[i] = 0;
  pads.render(padL_, padR_, frames);
  plucks.render(plkL_, plkR_, frames);

  for (int i = 0; i < frames; i++) {
    plk_[2 * i]     = (plkL_[i] * vol) >> 8;
    plk_[2 * i + 1] = (plkR_[i] * vol) >> 8;
  }

  // 4. Pluck echoes
  delay_.process(plk_, echo_, frames);

  // 5. Mix: what you hear directly (main) and what goes into the reverb (send)
  for (int i = 0; i < frames; i++) {
    for (int c = 0; c < 2; c++) {
      int k = 2 * i + c;
      int32_t pad = (((c ? padR_[i] : padL_[i]) >> 2) * vol) >> 8;
      int32_t echo = (echo_[k] * dMix) >> 8;
      main_[k] = ((pad * dry) >> 8) + ((plk_[k] * pDry) >> 8) + echo;
      send_[k] = pad + ((plk_[k] * pSend) >> 8) + echo;
    }
  }

  // 6. Reverb (added to main), then compressor -> 16-bit output
  reverb_.process(send_, main_, frames, wet);
  comp_.process(main_, out, frames);
}
