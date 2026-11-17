#include "Canvas.h"
#include <string.h>

namespace {

// 5x7 font: each character is 7 rows of 5 bits (the top bit is the left pixel).
// Only what the box needs: space, a few symbols, digits, A-Z and 'b' (for Eb, Bb).
const char FONT_CHARS[] = " #%-./0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZb";
const uint8_t FONT[][7] = {
  {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b00000},  // space
  {0b01010, 0b11111, 0b01010, 0b01010, 0b11111, 0b01010, 0b00000},  // #
  {0b11001, 0b11010, 0b00010, 0b00100, 0b01000, 0b01011, 0b10011},  // %
  {0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000},  // -
  {0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b01100},  // .
  {0b00001, 0b00010, 0b00010, 0b00100, 0b01000, 0b01000, 0b10000},  // /
  {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110},  // 0
  {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},  // 1
  {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111},  // 2
  {0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110},  // 3
  {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010},  // 4
  {0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110},  // 5
  {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110},  // 6
  {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000},  // 7
  {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110},  // 8
  {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100},  // 9
  {0b00000, 0b01100, 0b01100, 0b00000, 0b01100, 0b01100, 0b00000},  // :
  {0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001},  // A
  {0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110},  // B
  {0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110},  // C
  {0b11100, 0b10010, 0b10001, 0b10001, 0b10001, 0b10010, 0b11100},  // D
  {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111},  // E
  {0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000},  // F
  {0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111},  // G
  {0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001},  // H
  {0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110},  // I
  {0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100},  // J
  {0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001},  // K
  {0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111},  // L
  {0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001},  // M
  {0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001},  // N
  {0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110},  // O
  {0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000},  // P
  {0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101},  // Q
  {0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001},  // R
  {0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110},  // S
  {0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100},  // T
  {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110},  // U
  {0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100},  // V
  {0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010},  // W
  {0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001},  // X
  {0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100, 0b00100},  // Y
  {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111},  // Z
  {0b10000, 0b10000, 0b10110, 0b11001, 0b10001, 0b10001, 0b11110},  // b
};

int glyphIndex(char c) {
  if (c >= 'a' && c <= 'z' && c != 'b') c = (char)(c - 'a' + 'A');   // only 'b' is lowercase
  const char *p = strchr(FONT_CHARS, c);
  return (p && c) ? (int)(p - FONT_CHARS) : 0;
}

}  // namespace

void Canvas::clear() { memset(buf_, 0, sizeof(buf_)); }

void Canvas::pixel(int x, int y, bool on) {
  if (x < 0 || x >= W || y < 0 || y >= H) return;
  uint8_t &b = buf_[(y >> 3) * W + x];
  uint8_t m = (uint8_t)(1u << (y & 7));
  if (on) b |= m; else b &= (uint8_t)~m;
}

bool Canvas::get(int x, int y) const {
  if (x < 0 || x >= W || y < 0 || y >= H) return false;
  return (buf_[(y >> 3) * W + x] >> (y & 7)) & 1;
}

void Canvas::hline(int x, int y, int w, bool on) { for (int i = 0; i < w; i++) pixel(x + i, y, on); }
void Canvas::vline(int x, int y, int h, bool on) { for (int i = 0; i < h; i++) pixel(x, y + i, on); }

void Canvas::line(int x0, int y0, int x1, int y1, bool on) {
  // Bresenham: step along the longer direction, correct the other with an error term
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
  int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    pixel(x0, y0, on);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void Canvas::rect(int x, int y, int w, int h, bool on) {
  hline(x, y, w, on);
  hline(x, y + h - 1, w, on);
  vline(x, y, h, on);
  vline(x + w - 1, y, h, on);
}

void Canvas::fillRect(int x, int y, int w, int h, bool on) {
  for (int j = 0; j < h; j++) hline(x, y + j, w, on);
}

void Canvas::circle(int cx, int cy, int r, bool on, int dotted) {
  // Midpoint circle: one eighth is computed, the rest mirrored
  int x = r, y = 0, err = 1 - r, n = 0;
  while (x >= y) {
    if (dotted <= 1 || (n % dotted) == 0) {
      pixel(cx + x, cy + y, on); pixel(cx - x, cy + y, on);
      pixel(cx + x, cy - y, on); pixel(cx - x, cy - y, on);
      pixel(cx + y, cy + x, on); pixel(cx - y, cy + x, on);
      pixel(cx + y, cy - x, on); pixel(cx - y, cy - x, on);
    }
    n++;
    y++;
    if (err < 0) err += 2 * y + 1;
    else { x--; err += 2 * (y - x) + 1; }
  }
}

void Canvas::fillCircle(int cx, int cy, int r, bool on) {
  for (int y = -r; y <= r; y++)
    for (int x = -r; x <= r; x++)
      if (x * x + y * y <= r * r + r) pixel(cx + x, cy + y, on);
}

void Canvas::glyph(int x, int y, char c, int scale, bool on) {
  const uint8_t *g = FONT[glyphIndex(c)];
  for (int row = 0; row < 7; row++)
    for (int col = 0; col < 5; col++)
      if (g[row] & (0x10 >> col)) fillRect(x + col * scale, y + row * scale, scale, scale, on);
}

void Canvas::text(int x, int y, const char *s, int scale, bool on) {
  for (; *s; s++, x += 6 * scale) glyph(x, y, *s, scale, on);
}

int Canvas::textWidth(const char *s, int scale) {
  int n = (int)strlen(s);
  return n ? n * 6 * scale - scale : 0;     // no gap after the last character
}

void Canvas::textCentered(int cx, int y, const char *s, int scale, bool on) {
  text(cx - textWidth(s, scale) / 2, y, s, scale, on);
}
