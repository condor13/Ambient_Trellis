#include "Screen.h"
#include <math.h>
#include <string.h>
#include "../util/PadMap.h"

namespace {

constexpr float K_PI = 3.14159265f;   // (Arduino already defines PI as a macro)

// Lily pads: fixed places on the pond (x, y, radius, notch direction)
struct Pad { float x, y, r, notch; };
constexpr Pad LILIES[2] = {{18, 49, 8, 0.6f}, {108, 13, 7, 3.6f}};

float frand(Rng &r) { return (float)(r.next() >> 8) / 16777216.0f; }   // 0..1

}  // namespace

void Screen::begin(uint32_t seed, uint32_t nowMs) {
  rng_.seed(seed);
  for (auto &f : fish_) {
    f.x = 20 + frand(rng_) * 88;
    f.y = 12 + frand(rng_) * 40;
    f.heading = frand(rng_) * 2 * K_PI;
    f.turn = 0;
    f.phase = frand(rng_) * 2 * K_PI;
    f.spots = (uint8_t)rng_.next();
  }
  for (auto &r : ripples_) r.alive = false;
  lastMs_ = nowMs;
}

void Screen::addRipple(float x, float y, float maxR) {
  for (auto &r : ripples_) {
    if (!r.alive) { r = Ripple{x, y, 1, maxR, true}; return; }
  }
}

void Screen::rippleRandom(float maxRadius) {
  addRipple(10 + frand(rng_) * 108, 8 + frand(rng_) * 48, maxRadius);
}

// Where a logical pad sits on the display grid (4 columns of 32 px, 4 rows of 16 px):
// like the pads on the board if OLED_GRID_FOLLOWS_PADS, otherwise unturned;
// then mirrored left <-> right if OLED_GRID_MIRROR. (Only the boxes move; the
// text itself stays readable.)
static uint8_t gridCell(uint8_t pad) {
  uint8_t cell = cfg::OLED_GRID_FOLLOWS_PADS ? padmap::toPhysical(pad) : pad;
  if (cfg::OLED_GRID_MIRROR) cell = (uint8_t)((cell / 4) * 4 + (3 - cell % 4));
  return cell;
}

void Screen::ripplePad(uint8_t pad) {
  // The pond is the pad grid, stretched.
  uint8_t cell = gridCell(pad);
  addRipple((float)(cell % 4) * 32 + 16, (float)(cell / 4) * 16 + 8, 14);
}

void Screen::overlay(const char *title, const char *value, uint32_t nowMs, uint32_t ms) {
  strncpy(ovTitle_, title ? title : "", sizeof(ovTitle_) - 1);
  ovTitle_[sizeof(ovTitle_) - 1] = 0;
  strncpy(ovValue_, value ? value : "", sizeof(ovValue_) - 1);
  ovValue_[sizeof(ovValue_) - 1] = 0;
  ovUntilMs_ = nowMs + ms;
}

// ------------------------------------------------------------------ the pond

void Screen::updatePond(float dt, float speed) {
  for (auto &f : fish_) {
    // Wander: the turning speed drifts randomly, gently.
    f.turn += (frand(rng_) - 0.5f) * 3.0f * dt;
    if (f.turn > 0.8f) f.turn = 0.8f;
    if (f.turn < -0.8f) f.turn = -0.8f;
    f.heading += f.turn * dt;

    // Near the edge: steer towards the middle of the pond.
    const float margin = 14;
    if (f.x < margin || f.x > 128 - margin || f.y < margin * 0.6f || f.y > 64 - margin * 0.6f) {
      float want = atan2f(32 - f.y, 64 - f.x);
      float diff = want - f.heading;
      while (diff > K_PI) diff -= 2 * K_PI;
      while (diff < -K_PI) diff += 2 * K_PI;
      f.heading += diff * 1.5f * dt;
    }

    f.x += cosf(f.heading) * speed * dt;
    f.y += sinf(f.heading) * speed * dt;
    // The tail beats faster when swimming faster.
    f.phase += dt * (2.5f + speed * 0.25f);
  }

  for (auto &r : ripples_) {
    if (!r.alive) continue;
    r.r += 12.0f * dt;
    if (r.r > r.maxR) r.alive = false;
  }
}

void Screen::drawFish(Canvas &c, const Fish &f) const {
  const float dx = cosf(f.heading), dy = sinf(f.heading);   // forward
  const float px = -dy, py = dx;                             // sideways
  const int N = 6;
  const int radius[N] = {3, 3, 3, 2, 2, 1};
  float sx[N], sy[N];

  // Spine: points behind the head, swaying more towards the tail.
  for (int i = 0; i < N; i++) {
    float back = i * 2.6f;
    float sway = sinf(f.phase - i * 0.9f) * 0.3f * i;
    sx[i] = f.x - dx * back + px * sway;
    sy[i] = f.y - dy * back + py * sway;
  }

  // Tail fin: two strokes spreading backwards from the last point.
  float tx = sx[N - 1], ty = sy[N - 1];
  float wag = sinf(f.phase - N * 0.9f) * 0.5f;
  for (int side = -1; side <= 1; side += 2) {
    float a = f.heading + K_PI + side * 0.45f + wag;
    c.line((int)lrintf(tx), (int)lrintf(ty), (int)lrintf(tx + cosf(a) * 5), (int)lrintf(ty + sinf(a) * 5));
  }

  // Side fins near the head
  for (int side = -1; side <= 1; side += 2) {
    float fx = sx[1] + px * side * 3, fy = sy[1] + py * side * 3;
    c.line((int)lrintf(fx), (int)lrintf(fy), (int)lrintf(fx - dx * 2 + px * side * 2),
           (int)lrintf(fy - dy * 2 + py * side * 2));
  }

  // Body: circles from tail to head
  for (int i = N - 1; i >= 0; i--) c.fillCircle((int)lrintf(sx[i]), (int)lrintf(sy[i]), radius[i]);

  // Koi spots (dark), different for every fish
  for (int i = 1; i < 5; i++) {
    if (!((f.spots >> i) & 1)) continue;
    float side = ((f.spots >> (i + 3)) & 1) ? 1.0f : -1.0f;
    c.pixel((int)lrintf(sx[i] + px * side), (int)lrintf(sy[i] + py * side), false);
  }

  // Eyes
  for (int side = -1; side <= 1; side += 2) {
    c.pixel((int)lrintf(f.x + dx * 1.5f + px * side * 1.5f), (int)lrintf(f.y + dy * 1.5f + py * side * 1.5f), false);
  }
}

void Screen::drawPond(Canvas &c) const {
  // Ripples: solid when small, more and more dotted as they spread (fading).
  for (const auto &r : ripples_) {
    if (!r.alive) continue;
    float life = r.r / r.maxR;
    int dotted = life < 0.35f ? 1 : (life < 0.7f ? 2 : 3);
    c.circle((int)lrintf(r.x), (int)lrintf(r.y), (int)lrintf(r.r), true, dotted);
  }

  for (const auto &f : fish_) drawFish(c, f);

  // Lily pads float on top: fish swim under them.
  for (const auto &p : LILIES) {
    int x = (int)p.x, y = (int)p.y, r = (int)p.r;
    c.fillCircle(x, y, r + 1, false);          // a dark gap around the pad
    c.circle(x, y, r);
    // the notch: a wedge cut in, like a real lily pad
    float a1 = p.notch - 0.4f, a2 = p.notch + 0.4f;
    for (float a = a1; a <= a2; a += 0.05f) {
      c.line(x, y, x + (int)lrintf(cosf(a) * (r + 1)), y + (int)lrintf(sinf(a) * (r + 1)), false);
    }
    c.line(x, y, x + (int)lrintf(cosf(a1) * r), y + (int)lrintf(sinf(a1) * r));
    c.line(x, y, x + (int)lrintf(cosf(a2) * r), y + (int)lrintf(sinf(a2) * r));
  }
}

// ------------------------------------------------------------------ menu pages

void Screen::drawMenuList(Canvas &c, const Settings &s) const {
  for (int i = 0; i < P_COUNT; i++) {
    uint8_t cell = gridCell((uint8_t)i);
    int cx = (cell % 4) * 32 + 16, y = (cell / 4) * 16 + 4;
    const char *label = Settings::shortName((Param)i);
    if (i == P_PLAY && !s.playing) label = "STOP";
    c.textCentered(cx, y, label);
  }
  // Light grid lines between the cells
  for (int k = 1; k < 4; k++) {
    for (int y = 0; y < 64; y += 2) c.pixel(k * 32, y);
    for (int x = 0; x < 128; x += 2) c.pixel(x, k * 16 - 1);
  }
}

void Screen::drawValuePage(Canvas &c, const Settings &s, Param p) const {
  const char *name = Settings::longName(p);
  int scale = Canvas::textWidth(name, 2) <= 128 ? 2 : 1;
  c.textCentered(64, 1, name, scale);

  char val[24];
  s.valueText(p, val, sizeof(val));
  scale = Canvas::textWidth(val, 2) <= 128 ? 2 : 1;
  c.textCentered(64, 22, val, scale);

  if (p == P_ROOT) {
    // 12 little boxes for the roots (current filled), 4 for the scales
    int root = (s.value[P_ROOT] - 1) % 12;
    for (int i = 0; i < 12; i++) {
      int x = 2 + i * 8;
      if (i == root) c.fillRect(x, 48, 6, 10); else c.rect(x, 48, 6, 10);
    }
    for (int i = 0; i < 4; i++) {
      int x = 100 + i * 7;
      if (i == s.scale) c.fillRect(x, 48, 5, 10); else c.rect(x, 48, 5, 10);
    }
    return;
  }

  // The same bar as on the pads
  int n = Settings::options(p), v = s.value[p];
  int w = 128 / n;
  for (int i = 0; i < n; i++) {
    int x = i * w + 1;
    if (i < v) c.fillRect(x, 46, w - 2, 14); else c.rect(x, 46, w - 2, 14);
  }
}

void Screen::drawOverlay(Canvas &c) const {
  int wT = Canvas::textWidth(ovTitle_, 1);
  int scaleV = Canvas::textWidth(ovValue_, 2) <= 116 ? 2 : 1;
  int wV = Canvas::textWidth(ovValue_, scaleV);
  int w = (wT > wV ? wT : wV) + 12;
  int h = ovValue_[0] ? 34 : 17;
  int x = 64 - w / 2, y = 32 - h / 2;
  c.fillRect(x, y, w, h, false);
  c.rect(x, y, w, h);
  c.textCentered(64, y + 5, ovTitle_);
  if (ovValue_[0]) c.textCentered(64, y + 15, ovValue_, scaleV);
}

// ------------------------------------------------------------------ frame

void Screen::draw(Canvas &c, const Settings &s, const Menu &m, uint32_t nowMs) {
  float dt = (float)(nowMs - lastMs_) / 1000.0f;
  if (dt > 0.2f) dt = 0.2f;
  lastMs_ = nowMs;

  // Fish speed follows the LFO speed: 6 px/s (slowest) .. 36 px/s (fastest)
  float speed = 6.0f + 2.0f * (float)(s.value[P_LFO] - 1);
  updatePond(dt, speed);        // the pond keeps living while the menu is open

  c.clear();
  if (m.isOpen()) {
    if (m.onValuePage()) drawValuePage(c, s, m.page());
    else drawMenuList(c, s);
    return;
  }
  drawPond(c);
  if ((int32_t)(nowMs - ovUntilMs_) < 0) drawOverlay(c);
}
