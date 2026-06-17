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

// Chord trackers (SPEC §3): X+A = pause/resume, B+Y = reset. Each fires once per hold.
static uint32_t g_xa_t = 0, g_by_t = 0;
static bool     g_xa_held = false, g_by_held = false;
static bool     g_xa_fired = false, g_by_fired = false;

// Low-time warning fired this turn, per side (re-arms when the side climbs back above
// the warn threshold). Cleared at the start of each new game.
static bool g_warned[2] = {false, false};

static constexpr uint32_t LONG_PRESS_MS   = 800;  // hold-X to open Settings (SPEC §3)
static constexpr uint32_t PAUSE_CHORD_MS  = 500;  // hold X+A ~0.5 s
static constexpr uint32_t RESET_CHORD_MS  = 800;  // hold B+Y ~0.8 s
static const uint32_t ALL_BTNS[] = {A, B, X, Y, UP, DOWN, LEFT, RIGHT};

static bool was_down(uint32_t b)     { return (g_prev_btn_mask >> b) & 1u; }
static bool btn_released(uint32_t b) { return was_down(b) && !button(b); }

// Any D-pad button pressed this frame = LEFT player's input; any A/B/X/Y = RIGHT's.
static bool dpad_pressed()  { return pressed(UP) || pressed(DOWN) || pressed(LEFT) || pressed(RIGHT); }
static bool abxy_pressed()  { return pressed(A) || pressed(B) || pressed(X) || pressed(Y); }

static uint8_t backlight_for(uint8_t brightness) {  // 1..10 -> ~46..100
  return static_cast<uint8_t>(40 + brightness * 6);
}

// Start a fresh game from the current settings and re-arm the low-time warnings.
static void new_game() {
  clock_reset(g_clock, g_settings);   // -> READY with the current time control
  g_warned[SIDE_LEFT] = g_warned[SIDE_RIGHT] = false;
}

// Run a live press through the engine and play the matching cue (SPEC §9).
static void press_side(uint8_t side, uint32_t now) {
  switch (clock_press(g_clock, side, now)) {
    case PressResult::STARTED:
    case PressResult::SWITCHED: audio_cue_click(g_settings); break;  // confirmation tick
    case PressResult::FLAGGED:  audio_cue_flag(g_settings);  break;
    default: break;  // IGNORED (idle-side press)
  }
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
  // ---- chords first (SPEC §3); a held chord suppresses single-press actions --------
  // Both partner buttons held together = a chord, not a turn-end. (A single press never
  // has its partner down, so it still passes straight through below.)
  bool xa = button(X) && button(A);   // pause / resume
  bool by = button(B) && button(Y);   // reset -> READY

  if (xa) {
    g_x_consumed = true;              // X is in a chord — don't let its release start a game
    if (!g_xa_held) { g_xa_held = true; g_xa_t = now; g_xa_fired = false; }
    else if (!g_xa_fired && static_cast<uint32_t>(now - g_xa_t) >= PAUSE_CHORD_MS) {
      if (is_running(g_clock.state))           { clock_pause(g_clock, now);  audio_cue_click(g_settings); }
      else if (g_clock.state == State::PAUSED) { clock_resume(g_clock, now); audio_cue_click(g_settings); }
      g_xa_fired = true;   // only pause/resume act; in READY/FLAGGED the chord is a no-op
    }
  } else { g_xa_held = false; }

  if (by) {
    if (!g_by_held) { g_by_held = true; g_by_t = now; g_by_fired = false; }
    else if (!g_by_fired && static_cast<uint32_t>(now - g_by_t) >= RESET_CHORD_MS) {
      new_game();
      audio_cue_click(g_settings);
      g_by_fired = true;
    }
  } else { g_by_held = false; }

  if (xa || by) return;  // a chord is being held -> no turn-end / start this frame

  // ---- single-press actions (SPEC §3): D-pad = LEFT, A/B/X/Y = RIGHT ---------------
  switch (g_clock.state) {
    case State::READY:
      // X is overloaded in READY: hold X alone (~0.8 s) opens Settings; a tap is a RIGHT
      // press (passes the move to the first mover). A/B/Y and the D-pad act on press.
      if (pressed(X)) { g_x_down = now; g_x_consumed = false; }
      if (button(X) && !button(A) && !g_x_consumed &&
          static_cast<uint32_t>(now - g_x_down) >= LONG_PRESS_MS) {
        g_mode = Mode::SETTINGS;
        g_sel = 0;
        g_x_consumed = true;
        return;
      }
      if (btn_released(X) && !g_x_consumed)        press_side(SIDE_RIGHT, now);  // X tap
      if (pressed(A) || pressed(B) || pressed(Y))  press_side(SIDE_RIGHT, now);
      if (dpad_pressed())                          press_side(SIDE_LEFT, now);
      return;

    case State::RUN_LEFT:
    case State::RUN_RIGHT:
      if (abxy_pressed()) press_side(SIDE_RIGHT, now);
      if (dpad_pressed()) press_side(SIDE_LEFT, now);
      return;

    default: return;  // PAUSED -> resume via X+A; FLAGGED -> reset via B+Y
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
    if (g_clock.state == State::READY) new_game();  // reflect new control (Settings open only in READY)
  }
}

// Low-time warning beep when the active side first crosses warn_sec (SPEC §9). Re-arms
// once that side climbs back above the threshold (e.g. after an increment).
static void update_lowtime_warning(uint32_t now) {
  if (!is_running(g_clock.state) || g_settings.warn_sec == 0) return;
  uint8_t  a       = g_clock.active;
  uint32_t warn_ms = static_cast<uint32_t>(g_settings.warn_sec) * 1000u;
  uint32_t rem     = live_remaining_ms(g_clock, a, now);
  if (rem <= warn_ms) {
    if (!g_warned[a]) { audio_cue_lowtime(g_settings); g_warned[a] = true; }
  } else {
    g_warned[a] = false;
  }
}

// ---- picosystem callbacks ----------------------------------------------------
void init() {
  storage_load(g_settings);     // CRC-checked; defaults on first run / corrupt
  new_game();
  audio_init();
  ui_init();
  backlight(backlight_for(g_settings.brightness));
}

void update(uint32_t /*tick*/) {
  uint32_t now = time();        // ms clock. Never use tick for durations.

  // Flag detection first, so a press on the same frame the clock hits 0 doesn't save a
  // side that already ran out; the flag alarm fires on that frame (SPEC §9). Skipping
  // input on the firing frame also avoids a chord button registering a stray action.
  bool just_flagged = clock_tick(g_clock, now);
  if (just_flagged) audio_cue_flag(g_settings);

  update_lowtime_warning(now);

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
