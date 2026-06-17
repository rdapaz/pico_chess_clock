// clock.hpp — two-clock + turn engine (SPEC §5). Pure logic: no picosystem /
// hardware calls. The current time (ms) is passed in as `now` so this module stays
// testable and uses the same drift-free, wrap-safe unsigned-delta timing as pomodoro.
#pragma once
#include <cstdint>
#include "settings.hpp"

namespace chess {

enum class State : uint8_t {
  READY,      // nothing running; first press by a side starts the OTHER side (SPEC §3)
  RUN_LEFT,   // LEFT's clock is counting down
  RUN_RIGHT,  // RIGHT's clock is counting down
  PAUSED,     // both clocks suspended (chord X+A)
  FLAGGED     // a side hit 0 -> it loses, the other wins
};

enum Side : uint8_t { SIDE_LEFT = 0, SIDE_RIGHT = 1 };

inline uint8_t other(uint8_t side) { return side ^ 1u; }

struct Clock {
  State    state          = State::READY;
  uint32_t remaining_ms[2] = {0, 0};   // per side, banked time (excludes the live turn)
  uint8_t  active          = SIDE_LEFT; // side to move / running side
  uint32_t turn_start_ms   = 0;        // time() ms when the running side's turn began
  uint16_t moves[2]        = {0, 0};   // completed moves per side
  uint32_t inc_ms          = 0;        // Fischer increment, captured at init
  uint8_t  delay_mode      = DELAY_FISCHER;
  State    paused_from      = State::READY;  // valid while state == PAUSED
  uint8_t  flagged_side     = SIDE_LEFT;     // valid while state == FLAGGED (loser)
};

inline bool is_running(State st) { return st == State::RUN_LEFT || st == State::RUN_RIGHT; }
inline uint8_t running_side(State st) { return st == State::RUN_RIGHT ? SIDE_RIGHT : SIDE_LEFT; }

// Build a fresh game from settings: both sides = base time, READY, first mover per
// Settings.first_mover_left, increment captured from the resolved time control.
void clock_init(Clock& c, const Settings& s);

// Live remaining ms for `side`, clamped >= 0. For the running side this subtracts the
// in-progress turn elapsed (now - turn_start_ms); other states return the banked value.
uint32_t live_remaining_ms(const Clock& c, uint8_t side, uint32_t now);

// ---- turn engine (SPEC §5) — TODO(phase 2) ---------------------------------
// A live press by `side` ends that side's turn: bank elapsed, apply increment, hand
// the move to the other side, bump the move counter. Per the press rule (SPEC §3) a
// press by the idle side is ignored; in READY the first press starts the OTHER side.
void clock_press(Clock& c, uint8_t side, uint32_t now);

// Detect a flag: if the active side's live remaining hit 0, transition to FLAGGED.
// Call once per frame while running. Returns true on the frame the flag fires.
bool clock_tick(Clock& c, uint32_t now);

void clock_pause(Clock& c, uint32_t now);   // chord X+A: suspend both clocks
void clock_resume(Clock& c, uint32_t now);  // chord X+A again: resume the active side
void clock_reset(Clock& c, const Settings& s);  // chord B+Y: back to READY, same control

}  // namespace chess
