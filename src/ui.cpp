// ui.cpp — LED-panel rendering for the chess-clock screens (SPEC §8).
// Everything is drawn in the shared 5x7 dot font (font5x7). The screen is split into a
// LEFT half (x 0..119) and a RIGHT half (x 120..239) with a divider at x=120; the
// active side gets a tinted background + bright digits, the idle side stays dark + dim.
#include "ui.hpp"

#include <string>
#include "picosystem.hpp"
#include "font5x7.hpp"

using namespace picosystem;

namespace chess {

static constexpr int SCREEN   = 240;
static constexpr int HALF     = 120;
static constexpr int CLOCKCEL = 3;     // MM:SS at cell 3 -> 87px wide, 21px tall (fits a 120 half)
static constexpr int CLOCK_Y  = 96;

// Per-side palette. The active running side lights up (blue = LEFT, red = RIGHT per
// SPEC §4); every other case is the dim "idle" look.
struct Palette { color_t bg, on, grid; };

static Palette side_palette(const Clock& c, uint8_t side) {
  if (c.state == State::FLAGGED) {                                // game over (SPEC §8)
    return side == c.flagged_side
      ? Palette{ rgb(6, 1, 1), rgb(15, 5, 4), rgb(5, 1, 1) }      // loser: red
      : Palette{ rgb(1, 5, 1), rgb(6, 15, 7), rgb(1, 4, 1) };     // winner: green
  }
  bool active = is_running(c.state) && side == c.active;
  if (active) {
    return side == SIDE_LEFT
      ? Palette{ rgb(1, 2, 6),  rgb(7, 12, 15), rgb(1, 3, 6) }   // blue
      : Palette{ rgb(6, 1, 1),  rgb(15, 8, 7),  rgb(6, 2, 2) };  // red
  }
  return Palette{ rgb(1, 1, 2), rgb(6, 6, 7), rgb(2, 2, 3) };    // idle / READY / paused
}

// Clock text: MM:SS normally; under 20 s show SS.t (tenths) so the final seconds read
// clearly and animate (SPEC §8).
static std::string format_clock(uint32_t ms) {
  if (ms < 20000u) {
    uint32_t tenths = ms / 100u;                 // floor to 0.1 s, counts down to 0.0
    uint32_t ss = tenths / 10u, t = tenths % 10u;
    return str(static_cast<int32_t>(ss)) + "." + str(static_cast<int32_t>(t));
  }
  uint32_t secs = (ms + 999u) / 1000u;           // ceil so it shows 00:00 only at true 0
  uint32_t mm = secs / 60, ss = secs % 60;
  if (mm > 99) mm = 99;
  return (mm < 10 ? "0" : "") + str(static_cast<int32_t>(mm)) + ":" +
         (ss < 10 ? "0" : "") + str(static_cast<int32_t>(ss));
}

static void draw_side(const Clock& c, const Settings& s, uint8_t side, uint32_t now) {
  const int x0 = (side == SIDE_LEFT) ? 0 : HALF;
  Palette p = side_palette(c, side);

  // Tinted background for this half.
  pen(p.bg);
  frect(x0, 0, HALF, SCREEN);

  if (c.state == State::FLAGGED) {
    // Game over: big result word per half (SPEC §8).
    const char* word = (side == c.flagged_side) ? "TIME" : "WIN";
    int ww = led_text_w(word, 3);
    led_text(word, x0 + (HALF - ww) / 2, 14, 3, p.on, 0, false);
  } else {
    // Top strip: move count (left) + increment tag (right). Left half also names the
    // time control (SPEC §8).
    TimeControl tc = resolved_tc(s);
    std::string mv  = "M" + str(static_cast<int32_t>(c.moves[side]));
    std::string inc = "+" + str(static_cast<int32_t>(tc.inc_sec));
    led_text(mv.c_str(), x0 + 6, 8, 2, p.on, 0, false);
    int iw = led_text_w(inc.c_str(), 2);
    led_text(inc.c_str(), x0 + HALF - 6 - iw, 8, 2, p.on, 0, false);
    if (side == SIDE_LEFT) {
      const char* nm = preset_short(s.preset);   // compact so it fits the 120px half
      int nw = led_text_w(nm, 2);
      led_text(nm, x0 + (HALF - nw) / 2, 24, 2, p.on, 0, false);
    }
  }

  // Big centred clock (full dot grid for the LED-panel look).
  std::string t = format_clock(live_remaining_ms(c, side, now));
  int cw = led_text_w(t.c_str(), CLOCKCEL);
  led_text(t.c_str(), x0 + (HALF - cw) / 2, CLOCK_Y, CLOCKCEL, p.on, p.grid, true);
}

// Face-to-face half (settings "Facing"): the LEFT half is rotated 90° clockwise, the
// RIGHT half 90° counter-clockwise, so a player on each side reads an upright clock.
// Rotating the digits onto the 240px tall axis lets them be much larger than the split
// layout's single-line MM:SS.
static void draw_side_rot(const Clock& c, const Settings& s, uint8_t side, uint32_t now) {
  const int x0  = (side == SIDE_LEFT) ? 0 : HALF;
  const Rot rot = (side == SIDE_LEFT) ? Rot::CW : Rot::CCW;
  Palette p = side_palette(c, side);

  pen(p.bg);
  frect(x0, 0, HALF, SCREEN);

  // Place a rotated string: its box is (7*cell) wide by led_text_w tall. `cx_in_half` is
  // the box's left edge within the half; it is always centred along the 240px axis.
  auto draw_rot = [&](const char* str_, int cell, int cx_in_half, color_t on, color_t grid, bool gridded) {
    int rotH = led_text_w(str_, cell);
    int x = x0 + cx_in_half;
    int y = (SCREEN - rotH) / 2;
    led_text_rot(str_, x, y, cell, on, grid, gridded, rot);
  };

  if (c.state == State::FLAGGED) {
    const char* word = (side == c.flagged_side) ? "TIME" : "WIN";
    const int cell = 6;
    draw_rot(word, cell, (HALF - 7 * cell) / 2, p.on, 0, false);  // big result, centred
    return;
  }

  // Big rotated clock, centred across the half.
  std::string t = format_clock(live_remaining_ms(c, side, now));
  const int ccell = 7;                                  // ~2.3x the split-layout digits
  draw_rot(t.c_str(), ccell, (HALF - 7 * ccell) / 2, p.on, p.grid, true);

  // Small info / state line along the outer edge (away from the centre divider).
  std::string info;
  if (c.state == State::READY)       info = "START";
  else if (c.state == State::PAUSED) info = "PAUSE";
  else { info = "M" + str(static_cast<int32_t>(c.moves[side])) +
                " +" + str(static_cast<int32_t>(resolved_tc(s).inc_sec)); }
  const int icell = 2, ibox = 7 * icell;
  int icx = (side == SIDE_LEFT) ? 3 : (HALF - 3 - ibox);  // outer edge of the half
  draw_rot(info.c_str(), icell, icx, p.on, 0, false);
}

void ui_init() {}  // nothing to precompute for the LED renderer

void ui_draw_main(const Clock& c, const Settings& s, uint32_t now) {
  pen(0, 0, 0);
  clear();

  if (s.rotated) {  // face-to-face mode: rotated per-half clocks, no shared bottom strip
    draw_side_rot(c, s, SIDE_LEFT, now);
    draw_side_rot(c, s, SIDE_RIGHT, now);
    pen(4, 4, 5);
    frect(HALF - 1, 0, 2, SCREEN);
    return;
  }

  draw_side(c, s, SIDE_LEFT, now);
  draw_side(c, s, SIDE_RIGHT, now);

  // Vertical divider at x=120.
  pen(4, 4, 5);
  frect(HALF - 1, 0, 2, SCREEN);

  // Bottom strip: state line + control hint (SPEC §8). Running clocks speak for
  // themselves; FLAGGED/PAUSED carry the chord hint, READY the start prompt.
  const char* msg = (c.state == State::READY)   ? "PRESS TO START"
                  : (c.state == State::PAUSED)  ? "PAUSED  X+A RESUME"
                  : (c.state == State::FLAGGED) ? "HOLD B+Y NEW GAME"
                  : nullptr;
  if (msg) {
    int w = led_text_w(msg, 2);
    pen(0, 0, 0);
    frect((SCREEN - w) / 2 - 6, 214, w + 12, 20);  // clear a band behind the text
    led_text(msg, (SCREEN - w) / 2, 218, 2, rgb(13, 13, 14), 0, false);
  }
}

// ---- settings screen ---------------------------------------------------------
static std::string value_str(const Settings& s, int i) {
  switch (i) {
    case F_TIME_CONTROL: return std::string(preset_short(s.preset));
    case F_BASE:         return str(static_cast<int32_t>(s.base_min)) + " min";
    case F_INCREMENT:    return str(static_cast<int32_t>(s.inc_sec)) + " s";
    case F_DELAY:        return std::string(delay_name(s.delay_mode));
    case F_FIRST_MOVER:  return s.first_mover_left ? std::string("Left")
                                                   : std::string("Right");
    case F_WARN:         return str(static_cast<int32_t>(s.warn_sec)) + " s";
    case F_VOLUME:       return s.volume == 0 ? std::string("Mute")
                                              : str(static_cast<int32_t>(s.volume));
    case F_BRIGHT:       return str(static_cast<int32_t>(s.brightness));
    case F_FACING:       return s.rotated ? std::string("On") : std::string("Off");
    case F_EXIT:         return std::string(">");
  }
  return "";
}

void ui_draw_settings(const Settings& s, uint8_t sel) {
  pen(0, 0, 1);
  clear();
  led_text("SETTINGS", 8, 8, 3, rgb(14, 14, 15), 0, false);
  pen(3, 3, 5);
  frect(0, 32, SCREEN, 1);

  // Only the currently-visible rows (Custom base/inc appear when preset==CUSTOM).
  uint8_t list[FIELD_COUNT];
  int n = visible_fields(s, list);
  int selpos = 0;
  for (int i = 0; i < n; ++i) if (list[i] == sel) { selpos = i; break; }

  const int VIS = 5, y0 = 40, pitch = 34;
  int first = selpos - 2;
  if (first > n - VIS) first = n - VIS;
  if (first < 0) first = 0;

  for (int v = 0; v < VIS; ++v) {
    int li = first + v;
    if (li >= n) break;
    uint8_t f = list[li];
    int ry = y0 + v * pitch;
    bool seld = (f == sel);

    if (seld) {
      pen(rgb(15, 11, 2));               // amber highlight bar
      frect(6, ry - 3, 228, pitch - 6);
    }

    color_t lab = seld ? rgb(2, 1, 0) : rgb(13, 13, 14);
    led_text(FIELDS[f].label, 14, ry + 2, 3, lab, 0, false);

    std::string val = value_str(s, f);
    int vw = led_text_w(val.c_str(), 3);
    color_t vcol = seld ? rgb(2, 1, 0) : rgb(15, 15, 15);
    int vx = 218 - vw;
    led_text(val.c_str(), vx, ry + 2, 3, vcol, 0, false);

    if (seld && FIELDS[f].type != FieldType::ACTION) {
      led_text("<", vx - 16, ry + 2, 3, rgb(2, 1, 0), 0, false);
      led_text(">", 222, ry + 2, 3, rgb(2, 1, 0), 0, false);
    }
  }

  // Scrollbar (sized to the visible-row count).
  int track_y = y0, track_h = VIS * pitch - 6;
  pen(2, 2, 4);
  frect(234, track_y, 4, track_h);
  if (n > VIS) {
    int thumb_h = track_h * VIS / n;
    int thumb_y = track_y + (track_h - thumb_h) * first / (n - VIS);
    pen(8, 8, 11);
    frect(234, thumb_y, 4, thumb_h);
  } else {
    pen(8, 8, 11);
    frect(234, track_y, 4, track_h);   // everything fits -> full thumb
  }

  int hw = led_text_w("UP/DN  L/R  Y", 2);
  led_text("UP/DN  L/R  Y", (SCREEN - hw) / 2, 222, 2, rgb(8, 8, 9), 0, false);
}

}  // namespace chess
