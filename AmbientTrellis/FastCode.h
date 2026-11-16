// AUDIO_FAST(func): keeps hot audio functions in RAM on the Pico, where code
// runs faster than from flash. On a computer it does nothing.
#pragma once

#if defined(ARDUINO)
#include <Arduino.h>   // the Pico core defines __not_in_flash_func here
#endif

#if defined(__not_in_flash_func)
#define AUDIO_FAST(f) __not_in_flash_func(f)
#else
#define AUDIO_FAST(f) f
#endif
