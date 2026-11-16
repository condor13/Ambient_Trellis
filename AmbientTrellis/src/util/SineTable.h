// SineTable - one cycle of a sine wave, 2048 steps, shared by all synths.
//
// A full turn of a phase is 2^32, so the top 11 bits pick the entry.
#pragma once
#include <stdint.h>
#include "FastCode.h"

extern int16_t g_sineTab[2048];

// Fill the table (call once at start-up, on the core that plays audio).
void sineTableInit();

// sin(phase) as ±32767, where phase 2^32 = one full turn.
static ALWAYS_INLINE int32_t sineLookup(uint32_t phase) { return g_sineTab[phase >> 21]; }
