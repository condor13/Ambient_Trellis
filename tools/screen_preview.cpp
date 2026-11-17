// Host preview of the OLED: draws frames with the real Screen code and writes
// them as PBM images (convert to a GIF with ffmpeg, see below).
//
//   g++ -O2 -std=c++17 -I AmbientTrellis/src tools/screen_preview.cpp
//       AmbientTrellis/src/{audio,music,util,ui,app}/*.cpp -o screen_preview
//   ./screen_preview frames/
//   ffmpeg -framerate 20 -i frames/f%04d.pbm -vf scale=512:256:flags=neighbor pond.gif
#include <stdio.h>
#include <string.h>
#include "app/Settings.h"
#include "ui/Canvas.h"
#include "ui/Menu.h"
#include "ui/Screen.h"

static void save(const Canvas &c, const char *dir, int n) {
  char path[256];
  snprintf(path, sizeof(path), "%s/f%04d.pbm", dir, n);
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P1\n128 64\n");
  for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 128; x++) fputc(c.get(x, y) ? '0' : '1', f);   // white fish on black water
    fputc('\n', f);
  }
  fclose(f);
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : ".";
  static Canvas c;
  static Screen screen;
  static Menu menu;
  static Settings s;
  s.setDefaults();
  screen.begin(42, 0);
  screen.overlay("AMBIENT", "TRELLIS", 0);

  int frame = 0;
  for (uint32_t ms = 0; ms <= 16000; ms += 50) {
    if (ms == 2500 || ms == 9000) screen.rippleRandom(22);      // chord changes
    if (ms == 4000 || ms == 6200) screen.rippleRandom(10);      // plucks
    if (ms == 5000) screen.ripplePad(6);                        // a touch
    if (ms == 7000) { s.value[P_VERB] = 12; screen.overlay("REVERB", "12", ms); }
    if (ms == 11000) menu.shift(ms);                            // menu list
    if (ms == 12500) menu.press(P_BPM, s, ms);                  // value page
    if (ms == 14000) { menu.press(10, s, ms); }                 // set 63 BPM
    if (ms == 15000) { menu.shift(ms); menu.shift(ms); }        // back, close
    screen.draw(c, s, menu, ms);
    save(c, dir, frame++);
  }
  printf("%d frames\n", frame);
  return 0;
}
