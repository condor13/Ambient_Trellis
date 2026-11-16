#include "SineTable.h"
#include <math.h>

int16_t g_sineTab[2048];

void sineTableInit() {
  for (int i = 0; i < 2048; i++) {
    g_sineTab[i] = (int16_t)lrintf(32767.0f * sinf(6.2831853f * (float)i / 2048.0f));
  }
}
