// Canvas - a 128x64 black-and-white picture in RAM, with simple drawing.
//
// The memory layout is the same as the SSD1306 display's: 8 "pages" of 8 rows,
// each byte is one column of 8 pixels (bit 0 at the top). So a finished picture
// can be sent to the display as it is.
//
// Plain C++ (no hardware), so pictures can also be drawn and checked on a computer.
#pragma once
#include <stdint.h>

class Canvas {
 public:
  static constexpr int W = 128;
  static constexpr int H = 64;
  static constexpr int BYTES = W * H / 8;

  void clear();
  void pixel(int x, int y, bool on = true);
  bool get(int x, int y) const;

  void hline(int x, int y, int w, bool on = true);
  void vline(int x, int y, int h, bool on = true);
  void line(int x0, int y0, int x1, int y1, bool on = true);
  void rect(int x, int y, int w, int h, bool on = true);
  void fillRect(int x, int y, int w, int h, bool on = true);
  void circle(int cx, int cy, int r, bool on = true, int dotted = 1);   // dotted: every n-th pixel
  void fillCircle(int cx, int cy, int r, bool on = true);

  // Text with the built-in 5x7 font. scale 1 = 6 px per character, 2 = 12 px.
  void text(int x, int y, const char *s, int scale = 1, bool on = true);
  static int textWidth(const char *s, int scale = 1);
  void textCentered(int cx, int y, const char *s, int scale = 1, bool on = true);

  const uint8_t *data() const { return buf_; }

 private:
  void glyph(int x, int y, char c, int scale, bool on);
  uint8_t buf_[BYTES];
};
