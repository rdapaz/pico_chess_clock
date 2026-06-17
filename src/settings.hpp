// settings.hpp — user settings, time-control presets, and Settings-screen field
// metadata (SPEC §6, §7). Header-only. Persisted as part of the flash record (storage.*).
#pragma once
#include <cstdint>

namespace chess {

// ---- Time-control presets (SPEC §6) -----------------------------------------
// base minutes + increment seconds. CUSTOM exposes base/inc (+ delay mode, phase 2).
enum Preset : uint8_t {
  PRESET_BULLET_1_0 = 0,   // 1+0
  PRESET_BLITZ_3_2,        // 3+2  (default)
  PRESET_BLITZ_5_0,        // 5+0
  PRESET_RAPID_10_5,       // 10+5
  PRESET_RAPID_15_10,      // 15+10
  PRESET_CLASSICAL_30_0,   // 30+0
  PRESET_CUSTOM,           // base 1..180 min, inc 0..60 s
  PRESET_COUNT
};

// Delay/increment timing mode (SPEC §5). Fischer ships in phase 2; the delay modes
// follow. Listed here so the Settings model + persistence are stable from day one.
enum DelayMode : uint8_t {
  DELAY_FISCHER = 0,  // increment added after each move (most common)
  DELAY_SIMPLE,       // US delay: main clock waits delay_ms before decrementing (phase 2)
  DELAY_BRONSTEIN,    // add back min(elapsed, delay_ms) on press (phase 2)
  DELAY_COUNT
};

struct TimeControl { uint8_t base_min; uint8_t inc_sec; };

// Fixed (base, increment) for a non-CUSTOM preset; CUSTOM falls back to 3+2 here
// (callers use resolved_tc() to substitute the user's stored base/inc).
inline TimeControl preset_tc(uint8_t preset) {
  switch (preset) {
    case PRESET_BULLET_1_0:     return {1, 0};
    case PRESET_BLITZ_5_0:      return {5, 0};
    case PRESET_RAPID_10_5:     return {10, 5};
    case PRESET_RAPID_15_10:    return {15, 10};
    case PRESET_CLASSICAL_30_0: return {30, 0};
    default:                    return {3, 2};  // PRESET_BLITZ_3_2 / fallback
  }
}

inline const char* preset_name(uint8_t preset) {
  switch (preset) {
    case PRESET_BULLET_1_0:     return "Bullet 1+0";
    case PRESET_BLITZ_3_2:      return "Blitz 3+2";
    case PRESET_BLITZ_5_0:      return "Blitz 5+0";
    case PRESET_RAPID_10_5:     return "Rapid 10+5";
    case PRESET_RAPID_15_10:    return "Rapid 15+10";
    case PRESET_CLASSICAL_30_0: return "Classic 30+0";
    default:                    return "Custom";
  }
}

// Compact label for tight spots (settings value column, main-screen top strip).
inline const char* preset_short(uint8_t preset) {
  switch (preset) {
    case PRESET_BULLET_1_0:     return "1+0";
    case PRESET_BLITZ_3_2:      return "3+2";
    case PRESET_BLITZ_5_0:      return "5+0";
    case PRESET_RAPID_10_5:     return "10+5";
    case PRESET_RAPID_15_10:    return "15+10";
    case PRESET_CLASSICAL_30_0: return "30+0";
    default:                    return "Custom";
  }
}

inline const char* delay_name(uint8_t mode) {
  switch (mode) {
    case DELAY_SIMPLE:    return "Delay";
    case DELAY_BRONSTEIN: return "Bronstein";
    default:              return "Fischer";
  }
}

// User-configurable settings (SPEC §7), persisted whole in the flash record.
struct Settings {
  uint8_t preset           = PRESET_BLITZ_3_2;  // index into Preset; default 3+2
  uint8_t base_min         = 3;    // CUSTOM base minutes      (1..180)
  uint8_t inc_sec          = 2;    // CUSTOM increment seconds (0..60)
  uint8_t delay_mode       = DELAY_FISCHER;     // 0..DELAY_COUNT-1
  bool    first_mover_left = true; // LEFT moves first -> RIGHT presses to start (SPEC §3)
  uint8_t warn_sec         = 20;   // low-time warning threshold seconds (0..60)
  uint8_t volume           = 6;    // 0..10 (0 = mute)
  uint8_t brightness       = 7;    // 1..10 (LED + backlight scale)
};

// Effective (base, increment) for the current settings: presets are fixed, CUSTOM
// uses the stored base_min/inc_sec.
inline TimeControl resolved_tc(const Settings& s) {
  if (s.preset == PRESET_CUSTOM) return {s.base_min, s.inc_sec};
  return preset_tc(s.preset);
}

// ---- Settings-screen fields (top -> bottom; SPEC §7) ------------------------
// "Time control · [Custom: Base, Increment, Delay] · First mover · Low-time warn ·
//  Volume · Brightness · Exit to launcher".
enum SettingsField : uint8_t {
  F_TIME_CONTROL = 0,  // cycle through presets
  F_BASE,              // CUSTOM only (phase 3: hide unless preset == CUSTOM)
  F_INCREMENT,         // CUSTOM only
  F_DELAY,             // CUSTOM only (Fischer until phase 2 adds the delay modes)
  F_FIRST_MOVER,       // LEFT / RIGHT moves first
  F_WARN,              // low-time warn seconds
  F_VOLUME,
  F_BRIGHT,
  F_EXIT,              // "Exit to launcher" -> launcher::return_to_launcher() (phase 3)
  FIELD_COUNT
};

enum class FieldType : uint8_t { CHOICE, VALUE, TOGGLE, ACTION };

struct FieldMeta {
  const char* label;
  FieldType   type;
  uint8_t     min, max, step;  // meaningful for VALUE / CHOICE
};

// One row per SettingsField (C++17 inline variable -> no ODR issues across TUs).
inline constexpr FieldMeta FIELDS[FIELD_COUNT] = {
  {"Time",   FieldType::CHOICE, 0, PRESET_COUNT - 1, 1},
  {"Base",   FieldType::VALUE,  1, 180, 1},
  {"Inc",    FieldType::VALUE,  0,  60, 1},
  {"Delay",  FieldType::CHOICE, 0, DELAY_COUNT - 1, 1},
  {"First",  FieldType::TOGGLE, 0,   1, 1},
  {"Warn",   FieldType::VALUE,  0,  60, 1},
  {"Volume", FieldType::VALUE,  0,  10, 1},
  {"Bright", FieldType::VALUE,  1,  10, 1},
  {"Exit",   FieldType::ACTION, 0,   0, 0},
};

// Pointer to the uint8 backing a VALUE/CHOICE field, or nullptr for TOGGLE/ACTION.
inline uint8_t* field_ptr(Settings& s, uint8_t field) {
  switch (field) {
    case F_TIME_CONTROL: return &s.preset;
    case F_BASE:         return &s.base_min;
    case F_INCREMENT:    return &s.inc_sec;
    case F_DELAY:        return &s.delay_mode;
    case F_WARN:         return &s.warn_sec;
    case F_VOLUME:       return &s.volume;
    case F_BRIGHT:       return &s.brightness;
    default:             return nullptr;  // F_FIRST_MOVER (toggle), F_EXIT (action)
  }
}

// Apply a +/-1 (times step) change to a field, clamped to range. TOGGLE flips on any
// non-zero delta; ACTION rows are handled by the UI/main. Cycling the time control into
// CUSTOM seeds base/inc from the outgoing preset, so Custom starts where you left off.
inline void settings_adjust(Settings& s, uint8_t field, int delta) {
  if (field == F_FIRST_MOVER) {
    if (delta != 0) s.first_mover_left = !s.first_mover_left;
    return;
  }
  const FieldMeta& m = FIELDS[field];
  if (m.type != FieldType::VALUE && m.type != FieldType::CHOICE) return;
  uint8_t* p = field_ptr(s, field);
  if (!p) return;
  uint8_t old = *p;
  int v = static_cast<int>(old) + delta * static_cast<int>(m.step);
  if (v < m.min) v = m.min;
  if (v > m.max) v = m.max;
  *p = static_cast<uint8_t>(v);

  if (field == F_TIME_CONTROL && s.preset == PRESET_CUSTOM && old != PRESET_CUSTOM) {
    TimeControl tc = preset_tc(old);     // seed Custom from the preset you came from
    s.base_min = tc.base_min;
    s.inc_sec  = tc.inc_sec;
  }
}

// ---- visible-field helpers (Settings screen) --------------------------------
// The Custom rows (Base/Increment) only show when the preset is CUSTOM; the Delay row
// is hidden until the delay modes land (SPEC §12 phase 6). The field stays in the model
// + flash record either way so persistence is stable across phases.
inline bool field_visible(const Settings& s, uint8_t field) {
  if (field == F_DELAY) return false;
  if ((field == F_BASE || field == F_INCREMENT) && s.preset != PRESET_CUSTOM) return false;
  return true;
}

// Fill `out` (size >= FIELD_COUNT) with the currently-visible field ids, in order;
// returns the count.
inline uint8_t visible_fields(const Settings& s, uint8_t* out) {
  uint8_t n = 0;
  for (uint8_t f = 0; f < FIELD_COUNT; ++f) {
    if (field_visible(s, f)) out[n++] = f;
  }
  return n;
}

// Next/previous visible field id relative to `sel` (wraps). dir = +1 / -1.
inline uint8_t field_step(const Settings& s, uint8_t sel, int dir) {
  uint8_t list[FIELD_COUNT];
  uint8_t n = visible_fields(s, list);
  if (n == 0) return sel;
  int idx = 0;
  for (uint8_t i = 0; i < n; ++i) if (list[i] == sel) { idx = i; break; }
  idx = (idx + dir + n) % n;
  return list[idx];
}

}  // namespace chess
