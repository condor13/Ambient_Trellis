// AUDIO_FAST(func): keeps hot audio functions in RAM on the Pico, where code
// runs faster than from flash. On a computer it does nothing.
//
// ALWAYS_INLINE: small helpers used inside the audio loops. The Arduino-Pico
// core optimizes for size by default and would otherwise turn them into real
// function calls, which then run from flash.
#pragma once

#define ALWAYS_INLINE inline __attribute__((always_inline))

#if defined(ARDUINO)
#include <Arduino.h>   // the Pico core defines __not_in_flash_func here
#endif

#if defined(__not_in_flash_func)
#define AUDIO_FAST(f) __not_in_flash_func(f)
#else
#define AUDIO_FAST(f) f
#endif
