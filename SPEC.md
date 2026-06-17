# Pico Chess Clock — Specification

A two-player **chess clock** for the Pimoroni PicoSystem, built as a sibling of the
`pomodoro_timer` firmware and **launcher-ready from day one** (drops into the multi-app
launcher via the app-SDK). Written to be picked up and built in a fresh session.

- **Project home:** `C:\Users\ricdeez\Projects\pico_chess_clock` (new; not yet scaffolded).
- **Base to clone from:** `C:\Users\ricdeez\Projects\pomodoro_timer` (GitHub `rdapaz/pomodoro_timer`).
- **Shared build setup + flashing gotchas:** see `Desktop\HANDOFF-picosystem-launcher-and-winbledon.md` §1.

---

## 1. Concept
Two independent countdown clocks, one per player, shown **side-by-side** on the 240×240 screen
(left half | right half). Only the **side-to-move's** clock runs. A player ends their turn by
pressing a button on **their** side, which stops their clock (applying any increment) and starts
the opponent's. First clock to hit 0 **flags** (loses). It's the pomodoro's twin: same countdown
core, font/UI, settings, flash persistence, audio, LED, build — with a dual-clock + turn engine
swapped in.

## 2. Launcher integration — build this in from the start (explicit requirement)
This app must be slot-ready the moment it exists, using the launcher app-SDK at
`C:\Users\ricdeez\Projects\picosystem_launcher\launcher_sdk\`:
- **CMake:** after `picosystem_executable(chess_clock ...)`, add
  `include(<launcher>/launcher_sdk/app_slot.cmake)` then `launcher_app(chess_clock)` — builds
  **standalone @ 0x10000000** (test/flash now); `launcher_app(chess_clock SLOT n)` is the one-flag
  switch to a launcher slot.
- **main.cpp:** `#include "launcher_app.h"` + `LAUNCHER_DECLARE_APP("Chess Clock", 1);` (embeds the
  menu name + magic).
- **Return path:** a Settings row **"Exit to launcher"** calls `launcher::return_to_launcher()`
  (sets the watchdog scratch flag + reboots; standalone = harmless restart).
- Net effect: identical to how Winbledon was wired — `+1 include, +1 macro, +1 CMake line`. The app
  is a normal picosystem app, so it chain-loads like any other once the launcher is unparked.

## 3. Controls — left/right, two thumb-clusters
| Input | Owner | Action |
|---|---|---|
| **D-pad** (any of ◀▶▲▼) | **LEFT** player | end LEFT's turn |
| **A / B / X / Y** (any) | **RIGHT** player | end RIGHT's turn |
| **Hold X + A** (~0.5 s) | either | pause / resume both clocks |
| **Hold B + Y** (~0.8 s) | either | reset → READY (same time control) |
| In READY: **Hold X** (~0.8 s) | — | open Settings |

**Press rule (one consistent rule for all states):** a press by a side **stops that side and
starts the other**, but **only the running side's press is "live"** (a press by the idle side is
ignored, so your opponent can't hit your clock). In **READY** (nothing running) the first press by
a side starts the **opponent's** clock — i.e. you press to pass the move (default first-to-move =
LEFT, so RIGHT presses to start, or make it a setting). Debounce presses (~50 ms) and require the
chord buttons to be near-simultaneous so a normal single press never triggers pause/reset.

## 4. LED scheme (single RGB LED = whose move, at a glance)
- **LEFT running → BLUE.** **RIGHT running → RED.** (User-specified.)
- READY → dim white. PAUSED → amber, slow pulse.
- **Low time** (active side under the warn threshold) → active colour (blue/red), fast pulse.
- **FLAGGED** → fast red flash (with the alarm).
- Brightness scaled by the Settings `brightness`. Reuse the pomodoro `pulse_level()` helper.

## 5. Clock engine (pure logic; pass `now_ms` in — like pomodoro)
State: `READY, RUN_LEFT, RUN_RIGHT, PAUSED, FLAGGED`. Per side: `remaining_ms` (uint32).
`turn_start_ms` = `time()` ms when the running side's turn began.
- Live remaining of the active side = `remaining_ms[active] - (now - turn_start_ms)`, clamped ≥ 0.
  Hitting 0 → `FLAGGED` (active side loses; other side wins).
- **On a live press** (active side ends turn): `elapsed = now - turn_start_ms`;
  `remaining_ms[active] = max(0, remaining_ms[active] - elapsed)`; **apply increment** (Fischer:
  `remaining_ms[active] += increment_ms`); `active = other`; `turn_start_ms = now`; bump that
  side's move counter.
- Use the same **drift-free `time()` ms, wrap-safe unsigned deltas** as pomodoro. Never use `tick`.
- **Delay modes (phase 2):** *Simple/US delay* — the main clock doesn't decrement until `delay_ms`
  of the move has elapsed. *Bronstein* — on press, add back the lesser of elapsed and `delay_ms`.
  Ship **Fischer increment first** (most common); delay modes after.

## 6. Time controls & presets
Preset = (base minutes, increment seconds). Selectable in Settings; `Custom` exposes base +
increment (+ delay mode in phase 2).
| Preset | base+inc |
|---|---|
| Bullet | 1+0 |
| Blitz | **3+2** (default), 5+0 |
| Rapid | 10+5, 15+10 |
| Classical | 30+0 |
| Custom | base 1–180 min, inc 0–60 s |

## 7. Settings model + persistence (reuse pomodoro's `storage` + scrollable settings)
`Settings { uint8 preset; uint8 base_min; uint8 inc_sec; uint8 delay_mode; bool first_mover_left;
uint8 warn_sec; uint8 volume; uint8 brightness; ... }` — persisted in the **last flash sector** as
a CRC32-checked, versioned record (copy `pomodoro_timer/src/storage.*` verbatim and swap the
struct). Field list (scrollable, ≥16px LED font):
`Time control · [Custom: Base, Increment, Delay] · First mover · Low-time warn · Volume ·
Brightness · Exit to launcher`. Writes only on settings-close (wear-safe).

## 8. UI / screens (240×240, reuse `font5x7` LED dot-font — text ≥16px)
- **Main (two clocks):** vertical divider at x=120. **Left half** (x 0–119) = LEFT clock, **right
  half** (x 120–239) = RIGHT clock. Each: big centred clock (7-seg/dot digits sized to ~110px
  width). **Format:** `MM:SS`; when a side is **under 20 s show `SS.t`** (tenths). Active half:
  tinted background (blue-left / red-right) + bright digits; idle half: dark + dim.
- Thin top strip per half: move count + increment tag (e.g. `+2`) + (left only) time-control name.
- Bottom strip: state — READY "PRESS TO START", PAUSED "PAUSED", FLAGGED "TIME!" on the flagged
  side and "WIN" on the other.
- **Settings screen:** clone the pomodoro scrollable list (highlight bar, `< >` on selected,
  scrollbar).
- *Optional later:* a setting to rotate one half 180° for face-to-face (across-the-table) play.

## 9. Audio (piezo, reuse pomodoro `audio` sequencer)
- **Press click** — a short tick each time a side presses (confirmation).
- **Low-time warning** — a beep when the active side first crosses `warn_sec`.
- **Flag alarm** — a distinct 3-note/again alarm on flag. Volume + mute from Settings.

## 10. Module / file layout (mirror pomodoro)
```
src/
  main.cpp        loop, input (press rule + chords), LED, wiring, LAUNCHER_DECLARE_APP
  clock.hpp/.cpp  two-clock + turn engine + increment/delay (pure logic, now_ms passed in)
  settings.hpp    Settings + presets + field metadata
  ui.hpp/.cpp     split L|R rendering + settings screen (uses font5x7)
  audio.hpp/.cpp  click / low-time / flag-alarm cues
  storage.hpp/.cpp flash persistence (CRC32 record; copied from pomodoro, struct swapped)
  font5x7.hpp/.cpp COPIED from pomodoro_timer/src/
docs/  (this SPEC)
CMakeLists.txt    picosystem_executable + launcher_app() (app-SDK), standalone-first
```

## 11. Build (CMake outline)
Clone pomodoro's `CMakeLists.txt` (board = pimoroni_picosystem, reuse shared SDKs via
`PICO_SDK_PATH`/`PICOSYSTEM_DIR`), set target `chess_clock`, then:
```cmake
picosystem_executable(chess_clock ${SRCS})
target_link_libraries(chess_clock hardware_flash hardware_sync)   # for storage
if(NOT LAUNCHER_SDK)
  set(LAUNCHER_SDK "${CMAKE_CURRENT_LIST_DIR}/../picosystem_launcher/launcher_sdk")
endif()
include(${LAUNCHER_SDK}/app_slot.cmake)
launcher_app(chess_clock)          # standalone now; SLOT n for the launcher
```
Build (WSL):
```bash
cd /mnt/c/Users/ricdeez/Projects/pico_chess_clock && mkdir -p build && cd build
PICO_SDK_PATH=/mnt/c/Users/ricdeez/Projects/pomodoro_timer/third_party/pico-sdk \
PICOSYSTEM_DIR=/mnt/c/Users/ricdeez/Projects/pomodoro_timer/third_party/picosystem \
  cmake .. -G Ninja && ninja      # -> build/chess_clock.uf2
```
Flash: BOOTSEL = hold **X** on power → drive `RPI-RP2` (letter varies; never `D:`) →
`cmd /c copy /b chess_clock.uf2 <drive>:\` after ~2 s.

## 12. Implementation phases
1. **Scaffold + launcher hooks:** copy the pomodoro skeleton (font5x7, storage, audio, build),
   set up CMake with `launcher_app()` + `LAUNCHER_DECLARE_APP("Chess Clock",1)`, get an empty
   split screen building standalone.
2. **Clock engine:** two clocks + the press rule + Fischer increment + flag; LED blue/red; main UI
   with `MM:SS`/`SS.t`. Make a 3+2 game playable.
3. **Settings + persistence:** presets, custom base/increment, first-mover, warn, volume,
   brightness, "Exit to launcher"; CRC32 flash record.
4. **Polish:** pause/reset chords, low-time warning + audio, flag alarm + game-over screen,
   move counters.
5. **Launcher build:** `launcher_app(chess_clock SLOT n)` once the launcher is unparked; verify
   metadata + return-to-launcher.
6. *Phase-2 extras:* delay modes (simple/Bronstein), 180°-rotate option, time odds per side.

## 13. Open questions for the next session
- Confirm default first-mover convention (LEFT) and whether READY-start should be "opponent
  presses" (consistent rule) or "tap either to start the first mover" (more casual).
- Exact clock digit sizing in a 120-px half (single-line `MM:SS` vs stacked) — decide at the UI step.
- Whether to also surface time-control selection on a quick pre-game screen vs only in Settings.
