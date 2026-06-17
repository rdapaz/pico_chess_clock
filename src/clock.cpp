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

// Begin `side`'s turn now (helper for press/start). Sets the running state + clock.
static void begin_turn(Clock& c, uint8_t side, uint32_t now) {
  c.active        = side;
  c.turn_start_ms = now;
  c.state         = (side == SIDE_LEFT) ? State::RUN_LEFT : State::RUN_RIGHT;
}

// ---- turn engine (SPEC §3 press rule + §5 increment) ------------------------
void clock_press(Clock& c, uint8_t side, uint32_t now) {
  switch (c.state) {
    case State::READY:
      // First press passes the move: it starts the OPPONENT's clock (SPEC §3), so the
      // non-first-mover taps to set the first mover running. No time banked/incremented.
      begin_turn(c, other(side), now);
      return;

    case State::RUN_LEFT:
    case State::RUN_RIGHT: {
      if (side != c.active) return;  // idle-side press is ignored (SPEC §3)

      uint32_t elapsed = static_cast<uint32_t>(now - c.turn_start_ms);  // wrap-safe
      uint32_t banked  = c.remaining_ms[side];
      if (elapsed >= banked) {       // ran out exactly on the press -> flag (clock_tick
        c.remaining_ms[side] = 0;    // normally catches this first; defensive here)
        c.flagged_side = side;
        c.state = State::FLAGGED;
        return;
      }
      // Bank the time spent, then apply the Fischer increment (SPEC §5). Delay modes
      // (Simple/Bronstein) are a later phase.
      c.remaining_ms[side] = (banked - elapsed) + c.inc_ms;
      c.moves[side] += 1;
      begin_turn(c, other(side), now);  // hand the move to the opponent
      return;
    }

    default: return;  // PAUSED / FLAGGED: not a live press
  }
}

bool clock_tick(Clock& c, uint32_t now) {
  if (!is_running(c.state)) return false;
  if (live_remaining_ms(c, c.active, now) == 0) {   // active side hit 0 -> FLAGGED
    c.remaining_ms[c.active] = 0;
    c.flagged_side = c.active;
    c.state = State::FLAGGED;
    return true;
  }
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
