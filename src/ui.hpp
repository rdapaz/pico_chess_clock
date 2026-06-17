// ui.hpp — 240x240 rendering (SPEC §8). Reads state, never mutates it.
#pragma once
#include <cstdint>
#include "settings.hpp"
#include "clock.hpp"

namespace chess {

void ui_init();  // call once from init()

// Main screen: split L|R clocks (divider at x=120). Each half shows a big centred
// MM:SS clock (SS.t under 20 s — phase 2) plus a thin top strip (moves, +inc, and on
// the left the time-control name). Bottom strip shows the state. `now` is time() ms.
void ui_draw_main(const Clock& c, const Settings& s, uint32_t now);

// Settings overlay: scrollable field list with the selected row highlighted.
void ui_draw_settings(const Settings& s, uint8_t sel);

}  // namespace chess
