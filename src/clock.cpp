// clock.cpp — two-clock + turn engine (SPEC §5).
//
// Phase 1 (scaffold): the data model + the pure read helpers (clock_init,
// live_remaining_ms) are implemented so the split screen renders the configured
// starting time. The turn engine (clock_press / clock_tick / pause / resume / reset)
// is stubbed and lands in phase 2 — see the per-function TODOs.
//
// All durations use wrap-safe unsigned deltas (now - turn_start_ms); `now` is the
// picosystem time() ms clock, passed in by the caller. Never use `tick`.
#include "clock.hpp"

namespace chess {

void clock_init(Clock& c, const Settings& s) {
  TimeControl tc = resolved_tc(s);
  uint32_t base_ms = static_cast<uint32_t>(tc.base_min) * 60u * 1000u;

  c = Clock{};
  c.state          = State::READY;
  c.remaining_ms[SIDE_LEFT]  = base_ms;
  c.remaining_ms[SIDE_RIGHT] = base_ms;
  c.active         = s.first_mover_left ? SIDE_LEFT : SIDE_RIGHT;  // side to move first
  c.inc_ms         = static_cast<uint32_t>(tc.inc_sec) * 1000u;
  c.delay_mode     = s.delay_mode;
}

uint32_t live_remaining_ms(const Clock& c, uint8_t side, uint32_t now) {
  uint32_t banked = c.remaining_ms[side];
  // Only the running side burns time, and only while actually running.
  if (is_running(c.state) && side == c.active) {
    uint32_t elapsed = static_cast<uint32_t>(now - c.turn_start_ms);  // wrap-safe
    return (elapsed >= banked) ? 0u : (banked - elapsed);
  }
  return banked;
}

// ---- turn engine — TODO(phase 2) -------------------------------------------
// Implement per SPEC §5:
//   elapsed = now - turn_start_ms;
//   remaining_ms[active] = max(0, remaining_ms[active] - elapsed);
//   remaining_ms[active] += inc_ms;            // Fischer
//   active = other(active); turn_start_ms = now; moves[old_active]++;
// honouring the press rule (SPEC §3): idle-side presses are ignored; in READY the
// first press by a side starts the OTHER side's clock.
void clock_press(Clock& /*c*/, uint8_t /*side*/, uint32_t /*now*/) {
  // TODO(phase 2): start/stop turns + Fischer increment + flag-on-zero.
}

bool clock_tick(Clock& /*c*/, uint32_t /*now*/) {
  // TODO(phase 2): if live_remaining_ms(active) == 0 -> FLAGGED, flagged_side = active.
  return false;
}

void clock_pause(Clock& /*c*/, uint32_t /*now*/) {
  // TODO(phase 4): bank the active side's elapsed, remember paused_from, -> PAUSED.
}

void clock_resume(Clock& /*c*/, uint32_t /*now*/) {
  // TODO(phase 4): restore paused_from, turn_start_ms = now.
}

void clock_reset(Clock& c, const Settings& s) {
  clock_init(c, s);  // back to READY with the same time control
}

}  // namespace chess
