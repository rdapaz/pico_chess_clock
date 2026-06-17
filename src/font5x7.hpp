// font5x7.hpp — a 5x7 LED dot-matrix font renderer used for every glyph on screen
// (clocks, headers, footer, settings). Lowercase input is rendered as uppercase.
// Colours are picosystem color_t values (uint16_t) — kept as uint16_t here so this
// header stays free of the picosystem include.
// COPIED verbatim from pomodoro_timer/src/font5x7.hpp (SPEC §10), namespace -> chess.
#pragma once
#include <cstdint>

namespace chess {

// 90° rotations for face-to-face play (SPEC §8 / settings "Facing").
enum class Rot : uint8_t { R0, CW, CCW };  // upright, 90° clockwise, 90° counter-clockwise

// Pixel width of a string at the given dot pitch `cell` (5 cols + 1 gap per char).
int led_text_w(const char* s, int cell);

// Draw `s` in the 5x7 dot font at pitch `cell`, optionally rotated. For R0, (x,y) is the
// text's top-left; for CW/CCW it is the top-left of the rotated bounding box, which is
// (7*cell) wide by led_text_w(s,cell) tall. Lit dots use `on`; if `show_grid`, unlit dots
// are drawn in `off` (LED-panel look). Returns led_text_w(s,cell).
int led_text_rot(const char* s, int x, int y, int cell,
                 uint16_t on, uint16_t off, bool show_grid, Rot rot);

// Convenience: upright text (Rot::R0). (x,y) is the top-left.
int led_text(const char* s, int x, int y, int cell,
             uint16_t on, uint16_t off, bool show_grid);

}  // namespace chess
