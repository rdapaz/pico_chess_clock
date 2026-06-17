// audio.hpp — piezo cues (SPEC §9). One tone sounds at a time, so multi-note cues are
// sequenced from update() via audio_update().
// COPIED from pomodoro_timer/src/audio.hpp (SPEC §10), cues swapped for the clock.
#pragma once
#include "settings.hpp"

namespace chess {

void audio_init();  // build the shared voice; call once from init()

// Start a cue. Volume is captured from `s` at call time; volume 0 (mute) -> silent.
// A new cue replaces any in-flight cue.
void audio_cue_click(const Settings& s);     // short tick when a side presses
void audio_cue_lowtime(const Settings& s);   // beep when the active side crosses warn_sec
void audio_cue_flag(const Settings& s);      // distinct alarm when a side flags

void audio_update();  // advance the sequencer; call every update() frame

}  // namespace chess
