// main.cpp — Chess Clock for the Pimoroni PicoSystem.
// picosystem owns the loop and calls init() once, then update(tick)+draw(tick) at 50 fps.
// This file owns the globals, input handling, the RGB-LED driver, and wires the pure
// logic (clock), audio, ui, and storage modules together. Launcher-ready from day one
// (SPEC §2): LAUNCHER_DECLARE_APP embeds the menu name + magic, and the "Exit to
// launcher" Settings row hands control back via the app-SDK.
#include "picosystem.hpp"
#include "launcher_app.h"

#include "settings.hpp"
#include "clock.hpp"
#include "audio.hpp"
#include "ui.hpp"
#include "storage.hpp"

using namespace picosystem;
using namespace chess;

// Embed the launcher metadata (name shown in the menu + 'PAPP' magic). Standalone
// builds ignore it; a SLOT build is chain-loaded by the launcher (SPEC §2).
LAUNCHER_DECLARE_APP("Chess Clock", 1);

// ---- globals -----------------------------------------------------------------
static Settings g_settings;     // loaded from flash in init()
static Clock    g_clock;        // two-clock + turn state

enum class Mode : uint8_t { GAME, SETTINGS };
static Mode    g_mode = Mode::GAME;
static uint8_t g_sel  = 0;      // highlighted Settings field

// long-press / release tracking (no released() in the SDK — we roll our own).
static uint32_t g_x_down = 0;
static bool     g_x_consumed = false;
static uint32_t g_prev_btn_mask = 0;

static constexpr uint32_t LONG_PRESS_MS = 800;  // hold-X to open Settings (SPEC §3)
static const uint32_t ALL_BTNS[] = {A, B, X, Y, UP, DOWN, LEFT, RIGHT};

static bool was_down(uint32_t b)     { return (g_prev_btn_mask >> b) & 1u; }
static bool btn_released(uint32_t b) { return was_down(b) && !button(b); }

static uint8_t backlight_for(uint8_t brightness) {  // 1..10 -> ~46..100
  return static_cast<uint8_t>(40 + brightness * 6);
}

// ---- RGB LED (SPEC §4) -------------------------------------------------------
// LEFT running = blue, RIGHT running = red; READY = dim white; PAUSED = amber slow
// pulse; low time = active colour fast pulse; FLAGGED = fast red flash.
static void base_colour(uint32_t now, uint8_t& r, uint8_t& g, uint8_t& b) {  // 0..100
  switch (g_clock.state) {
    case State::RUN_LEFT:  r = 0;   g = 40;  b = 100; break;  // blue
    case State::RUN_RIGHT: r = 100; g = 0;   b = 0;   break;  // red
    case State::PAUSED:    r = 100; g = 55;  b = 0;   break;  // amber
    case State::FLAGGED:   r = 100; g = 0;   b = 0;   break;  // red flash
    default:               r = 15;  g = 15;  b = 15;  break;  // READY = dim white
  }
  (void)now;
}

static float pulse_level(uint32_t now, uint32_t period) {  // triangle, never fully dark
  float t = static_cast<float>(now % period) / static_cast<float>(period);
  float tri = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f);
  return 0.25f + 0.75f * tri;
}

static void led_update(uint32_t now) {
  uint8_t r, g, b;
  base_colour(now, r, g, b);
  float gain = static_cast<float>(g_settings.brightness) / 10.0f;

  bool low_time = is_running(g_clock.state) &&
                  live_remaining_ms(g_clock, g_clock.active, now) <=
                      static_cast<uint32_t>(g_settings.warn_sec) * 1000u;

  if (g_clock.state == State::FLAGGED) gain *= pulse_level(now, 300);   // fast flash
  else if (low_time)                   gain *= pulse_level(now, 500);   // fast pulse
  else if (g_clock.state == State::PAUSED) gain *= pulse_level(now, 1600);  // slow pulse
  if (g_clock.state == State::READY)   gain *= 0.5f;                    // battery-aware idle dim

  led(static_cast<uint8_t>(r * gain),
      static_cast<uint8_t>(g * gain),
      static_cast<uint8_t>(b * gain));
}

// ---- input -------------------------------------------------------------------
static void handle_game_input(uint32_t now) {
  // In READY, hold X (~0.8 s) opens Settings (SPEC §3).
  if (g_clock.state == State::READY) {
    if (pressed(X)) { g_x_down = now; g_x_consumed = false; }
    if (button(X) && !g_x_consumed &&
        static_cast<uint32_t>(now - g_x_down) >= LONG_PRESS_MS) {
      g_mode = Mode::SETTINGS;
      g_sel = 0;
      g_x_consumed = true;
    }
  }

  // TODO(phase 2): the press rule (SPEC §3) — a live press by a side ends its turn and
  //   starts the other; idle-side presses are ignored. D-pad = LEFT player ends turn;
  //   A/B/X/Y = RIGHT player ends turn. clock_press() + audio_cue_click() on a live press.
  // TODO(phase 4): chords — hold X+A pause/resume, hold B+Y reset; low-time + flag cues.
}

static void handle_settings_input() {
  if (pressed(UP))   g_sel = (g_sel + FIELD_COUNT - 1) % FIELD_COUNT;
  if (pressed(DOWN)) g_sel = (g_sel + 1) % FIELD_COUNT;

  if (g_sel == F_EXIT) {
    if (pressed(A)) {                       // Exit to launcher (SPEC §2)
      storage_save_now(g_settings);
      launcher::return_to_launcher();       // standalone: harmless restart
    }
  } else {
    if (pressed(LEFT))  { settings_adjust(g_settings, g_sel, -1); storage_mark_dirty(); }
    if (pressed(RIGHT)) { settings_adjust(g_settings, g_sel, +1); storage_mark_dirty(); }
    if (pressed(A))     g_sel = (g_sel + 1) % FIELD_COUNT;  // confirm / next field
  }

  if (pressed(B)) g_sel = (g_sel + FIELD_COUNT - 1) % FIELD_COUNT;
  if (pressed(Y)) {                         // close & save
    g_mode = Mode::GAME;
    storage_save_now(g_settings);
    if (g_clock.state == State::READY) clock_init(g_clock, g_settings);  // reflect new control
  }
}

// ---- picosystem callbacks ----------------------------------------------------
void init() {
  storage_load(g_settings);     // CRC-checked; defaults on first run / corrupt
  clock_init(g_clock, g_settings);
  audio_init();
  ui_init();
  backlight(backlight_for(g_settings.brightness));
}

void update(uint32_t /*tick*/) {
  uint32_t now = time();        // ms clock. Never use tick for durations.

  // TODO(phase 2): clock_tick(g_clock, now) -> flag detection + flag cue when running.

  if (g_mode == Mode::GAME) handle_game_input(now);
  else                      handle_settings_input();

  audio_update();
  backlight(backlight_for(g_settings.brightness));
  led_update(now);
  storage_flush(g_settings, now);

  // Remember button state for next frame's release detection.
  g_prev_btn_mask = 0;
  for (uint32_t b : ALL_BTNS) {
    if (button(b)) g_prev_btn_mask |= (1u << b);
  }
}

void draw(uint32_t /*tick*/) {
  uint32_t now = time();
  if (g_mode == Mode::GAME) ui_draw_main(g_clock, g_settings, now);
  else                      ui_draw_settings(g_settings, g_sel);
}
