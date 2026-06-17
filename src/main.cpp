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

// Any D-pad button pressed this frame = LEFT player's input; any A/B/X/Y = RIGHT's.
static bool dpad_pressed()  { return pressed(UP) || pressed(DOWN) || pressed(LEFT) || pressed(RIGHT); }
static bool abxy_pressed()  { return pressed(A) || pressed(B) || pressed(X) || pressed(Y); }
static bool any_pressed()   { return dpad_pressed() || abxy_pressed(); }

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
  // Press rule (SPEC §3): D-pad = LEFT player, A/B/X/Y = RIGHT player. clock_press()
  // enforces "only the running side's press is live" and the READY pass-the-move start.
  switch (g_clock.state) {
    case State::FLAGGED:
      // Phase-2 stopgap so a 3+2 game is replayable: any button -> READY. Phase 4
      // replaces this with the B+Y reset chord and a proper game-over screen.
      if (any_pressed()) clock_reset(g_clock, g_settings);
      return;

    case State::READY:
      // X is overloaded in READY: hold (~0.8 s) opens Settings; a tap is a RIGHT press
      // (passes the move to the first mover). Other buttons act on press.
      if (pressed(X)) { g_x_down = now; g_x_consumed = false; }
      if (button(X) && !g_x_consumed &&
          static_cast<uint32_t>(now - g_x_down) >= LONG_PRESS_MS) {
        g_mode = Mode::SETTINGS;
        g_sel = 0;
        g_x_consumed = true;
        return;
      }
      if (btn_released(X) && !g_x_consumed) clock_press(g_clock, SIDE_RIGHT, now);  // X tap
      if (pressed(A) || pressed(B) || pressed(Y)) clock_press(g_clock, SIDE_RIGHT, now);
      if (dpad_pressed())                         clock_press(g_clock, SIDE_LEFT, now);
      return;

    case State::RUN_LEFT:
    case State::RUN_RIGHT:
      if (abxy_pressed()) clock_press(g_clock, SIDE_RIGHT, now);
      if (dpad_pressed()) clock_press(g_clock, SIDE_LEFT, now);
      // TODO(phase 4): chords — hold X+A pause/resume, hold B+Y reset; press-click,
      //   low-time, and flag audio cues.
      return;

    default: return;  // PAUSED (phase 4)
  }
}

static void handle_settings_input() {
  // Navigation skips hidden rows (Custom base/inc unless preset==CUSTOM; Delay phase 6).
  if (pressed(UP))   g_sel = field_step(g_settings, g_sel, -1);
  if (pressed(DOWN)) g_sel = field_step(g_settings, g_sel, +1);

  if (g_sel == F_EXIT) {
    if (pressed(A)) {                       // Exit to launcher (SPEC §2)
      storage_save_now(g_settings);
      launcher::return_to_launcher();       // standalone: harmless restart
    }
  } else {
    if (pressed(LEFT))  settings_adjust(g_settings, g_sel, -1);
    if (pressed(RIGHT)) settings_adjust(g_settings, g_sel, +1);
    if (pressed(A))     g_sel = field_step(g_settings, g_sel, +1);  // confirm / next field
  }

  if (pressed(B)) g_sel = field_step(g_settings, g_sel, -1);
  if (pressed(Y)) {                         // close & save — the only flash write (SPEC §7)
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

  // Flag detection first, so a press on the same frame the clock hits 0 doesn't save a
  // side that already ran out. Skipping input on the firing frame also stops a player
  // mashing their button at time-out from instantly tripping the FLAGGED reset.
  bool just_flagged = clock_tick(g_clock, now);  // TODO(phase 4): flag-alarm cue here.

  if (g_mode == Mode::GAME) { if (!just_flagged) handle_game_input(now); }
  else                      handle_settings_input();

  audio_update();
  backlight(backlight_for(g_settings.brightness));
  led_update(now);
  // No per-frame flash write: settings persist only on Settings-close (SPEC §7).

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
