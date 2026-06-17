// font5x7.hpp — a 5x7 LED dot-matrix font renderer used for every glyph on screen
// (clocks, headers, footer, settings). Lowercase input is rendered as uppercase.
// Colours are picosystem color_t values (uint16_t) — kept as uint16_t here so this
// header stays free of the picosystem include.
// COPIED verbatim from pomodoro_timer/src/font5x7.hpp (SPEC §10), namespace -> chess.
#pragma once
#include <cstdint>

namespace chess {

// Pixel width of a string at the given dot pitch `cell` (5 cols + 1 gap per char).
int led_text_w(const char* s, int cell);

// Draw `s` at top-left (x,y) in the 5x7 dot font, square dots at pitch `cell`.
// Lit dots use `on`; if `show_grid`, unlit dots are drawn in `off` (LED-panel look).
// Returns the width drawn (px).
int led_text(const char* s, int x, int y, int cell,
             uint16_t on, uint16_t off, bool show_grid);

}  // namespace chess
