// audio.cpp — cue tables + non-blocking sequencer (SPEC §9).
// COPIED from pomodoro_timer/src/audio.cpp (SPEC §10): the same voice + sequencer,
// with chess cues — a press click, a low-time warning beep, and a flag alarm.
#include "audio.hpp"
#include "picosystem.hpp"

using namespace picosystem;

namespace chess {

struct Note { uint16_t hz; uint16_t ms; };

// Press click: one short high tick (turn confirmation). Low-time: a single clear beep.
// Flag alarm: a distinct 3-note descending-then-up phrase so it stands out from a click.
static const Note CLICK[]   = {{1318, 28}};                              // E6 tick
static const Note LOWTIME[] = {{1760, 90}, {1760, 90}};                  // A6 double-beep
static const Note FLAG[]    = {{988, 180}, {740, 180}, {988, 320}};      // B5 F#5 B5

static voice_t g_voice;

struct Tune {
  const Note* seq  = nullptr;
  uint8_t     len  = 0;
  uint8_t     idx  = 0;
  uint32_t    next = 0;   // time() ms at which the next note should fire
  uint8_t     vol  = 0;   // 0..100
};
static Tune g_tune;

void audio_init() {
  // attack, decay, sustain-level(0..100), release — short, blip-like (PICOSYSTEM_API §4).
  g_voice = voice(8, 60, 70, 60);
}

static void cue(const Note* seq, uint8_t len, const Settings& s) {
  if (s.volume == 0) {            // mute -> cancel any playing cue, play nothing
    g_tune.seq = nullptr;
    g_tune.len = 0;
    g_tune.idx = 0;
    return;
  }
  g_tune.seq  = seq;
  g_tune.len  = len;
  g_tune.idx  = 0;
  g_tune.next = time();                                   // first note plays this frame
  g_tune.vol  = static_cast<uint8_t>(s.volume * 10);      // 0..10 -> 0..100
}

void audio_cue_click(const Settings& s)   { cue(CLICK,   sizeof(CLICK)   / sizeof(Note), s); }
void audio_cue_lowtime(const Settings& s) { cue(LOWTIME, sizeof(LOWTIME) / sizeof(Note), s); }
void audio_cue_flag(const Settings& s)    { cue(FLAG,    sizeof(FLAG)    / sizeof(Note), s); }

void audio_update() {
  if (!g_tune.seq || g_tune.idx >= g_tune.len) return;
  if (static_cast<int32_t>(g_tune.next - time()) > 0) return;  // wrap-safe "not yet"
  const Note& n = g_tune.seq[g_tune.idx++];
  play(g_voice, n.hz, n.ms, g_tune.vol);
  g_tune.next = time() + n.ms + 20u;  // small gap so notes stay distinct
}

}  // namespace chess
