// font5x7.cpp — 5x7 LED dot-matrix glyphs + renderer. Square dots via frect.
// COPIED verbatim from pomodoro_timer/src/font5x7.cpp (SPEC §10), namespace -> chess.
#include "font5x7.hpp"
#include "picosystem.hpp"

using namespace picosystem;

namespace chess {

// Each glyph: 7 rows, low 5 bits per row (bit4 = leftmost column).
static const uint8_t G_09[10][7] = {
  {0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110},  // 0
  {0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110},  // 1
  {0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111},  // 2
  {0b11111,0b00010,0b00100,0b00010,0b00001,0b10001,0b01110},  // 3
  {0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010},  // 4
  {0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110},  // 5
  {0b00110,0b01000,0b10000,0b11110,0b10001,0b10001,0b01110},  // 6
  {0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000},  // 7
  {0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110},  // 8
  {0b01110,0b10001,0b10001,0b01111,0b00001,0b00010,0b01100},  // 9
};

static const uint8_t G_AZ[26][7] = {
  {0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001},  // A
  {0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110},  // B
  {0b01110,0b10001,0b10000,0b10000,0b10000,0b10001,0b01110},  // C
  {0b11100,0b10010,0b10001,0b10001,0b10001,0b10010,0b11100},  // D
  {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111},  // E
  {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000},  // F
  {0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b01111},  // G
  {0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001},  // H
  {0b01110,0b00100,0b00100,0b00100,0b00100,0b00100,0b01110},  // I
  {0b00111,0b00010,0b00010,0b00010,0b00010,0b10010,0b01100},  // J
  {0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001},  // K
  {0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111},  // L
  {0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001},  // M
  {0b10001,0b10001,0b11001,0b10101,0b10011,0b10001,0b10001},  // N
  {0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110},  // O
  {0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000},  // P
  {0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101},  // Q
  {0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001},  // R
  {0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110},  // S
  {0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100},  // T
  {0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110},  // U
  {0b10001,0b10001,0b10001,0b10001,0b10001,0b01010,0b00100},  // V
  {0b10001,0b10001,0b10001,0b10101,0b10101,0b11011,0b10001},  // W
  {0b10001,0b10001,0b01010,0b00100,0b01010,0b10001,0b10001},  // X
  {0b10001,0b10001,0b01010,0b00100,0b00100,0b00100,0b00100},  // Y
  {0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111},  // Z
};

static const uint8_t SYM_SPACE[7] = {0,0,0,0,0,0,0};
static const uint8_t SYM_COLON[7] = {0b00000,0b00100,0b00100,0b00000,0b00100,0b00100,0b00000};
static const uint8_t SYM_SLASH[7] = {0b00001,0b00010,0b00010,0b00100,0b01000,0b01000,0b10000};
static const uint8_t SYM_MINUS[7] = {0b00000,0b00000,0b00000,0b11111,0b00000,0b00000,0b00000};
static const uint8_t SYM_DOT[7]   = {0b00000,0b00000,0b00000,0b00000,0b00000,0b01100,0b01100};
static const uint8_t SYM_LT[7]    = {0b00010,0b00100,0b01000,0b10000,0b01000,0b00100,0b00010};
static const uint8_t SYM_GT[7]    = {0b01000,0b00100,0b00010,0b00001,0b00010,0b00100,0b01000};
static const uint8_t SYM_PLUS[7]  = {0b00000,0b00100,0b00100,0b11111,0b00100,0b00100,0b00000};

static const uint8_t* glyph(char c) {
  if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);  // uppercase
  if (c >= 'A' && c <= 'Z') return G_AZ[c - 'A'];
  if (c >= '0' && c <= '9') return G_09[c - '0'];
  switch (c) {
    case ':': return SYM_COLON;
    case '/': return SYM_SLASH;
    case '-': return SYM_MINUS;
    case '.': return SYM_DOT;
    case '<': return SYM_LT;
    case '>': return SYM_GT;
    case '+': return SYM_PLUS;
    default:  return SYM_SPACE;
  }
}

static constexpr int GW = 5, GH = 7, ADVANCE = 6;  // 5 cols + 1 gap

int led_text_w(const char* s, int cell) {
  int n = 0;
  for (const char* p = s; *p; ++p) ++n;
  if (n == 0) return 0;
  return (n * ADVANCE - 1) * cell;  // drop the trailing gap
}

// One pass over the string drawing only lit (or only unlit) dots with the current pen.
// Each dot's top-left is computed in the unrotated local frame (lx,ly) then mapped to
// screen space per `rot`. The rotated bounding box is H wide by W tall, anchored at (x,y).
static void draw_pass(const char* s, int x, int y, int cell, bool want_lit, Rot rot) {
  int dot = cell - 1; if (dot < 1) dot = 1;
  const int W = led_text_w(s, cell);   // unrotated text width
  const int H = GH * cell;             // unrotated text height
  int cx = 0;                          // local x cursor (relative to text origin)
  for (const char* p = s; *p; ++p) {
    const uint8_t* g = glyph(*p);
    for (int r = 0; r < GH; ++r) {
      uint8_t row = g[r];
      for (int k = 0; k < GW; ++k) {
        bool lit = (row >> (GW - 1 - k)) & 1u;
        if (lit != want_lit) continue;
        int lx = cx + k * cell, ly = r * cell;
        int sx, sy;
        switch (rot) {
          case Rot::CW:  sx = x + (H - ly - dot); sy = y + lx;             break;
          case Rot::CCW: sx = x + ly;             sy = y + (W - lx - dot); break;
          default:       sx = x + lx;             sy = y + ly;             break;
        }
        frect(sx, sy, dot, dot);
      }
    }
    cx += ADVANCE * cell;
  }
}

int led_text_rot(const char* s, int x, int y, int cell,
                 uint16_t on, uint16_t off, bool show_grid, Rot rot) {
  if (show_grid) {
    pen(static_cast<color_t>(off));
    draw_pass(s, x, y, cell, false, rot);
  }
  pen(static_cast<color_t>(on));
  draw_pass(s, x, y, cell, true, rot);
  return led_text_w(s, cell);
}

int led_text(const char* s, int x, int y, int cell,
             uint16_t on, uint16_t off, bool show_grid) {
  return led_text_rot(s, x, y, cell, on, off, show_grid, Rot::R0);
}

}  // namespace chess
