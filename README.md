# Pico Chess Clock — Pimoroni PicoSystem (C++)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform: Pimoroni PicoSystem](https://img.shields.io/badge/platform-Pimoroni%20PicoSystem-e6007e.svg)](https://shop.pimoroni.com/products/picosystem)
[![Language: C++17](https://img.shields.io/badge/C%2B%2B-17-00599c.svg)](https://en.cppreference.com/w/cpp/17)

A two-player **chess clock** for the [Pimoroni PicoSystem](https://shop.pimoroni.com/products/picosystem)
(RP2040 handheld: 240×240 LCD, D-pad + A/B/X/Y, piezo, RGB LED, LiPo battery). Two
countdown clocks sit side-by-side — only the side-to-move's clock runs; press a button
on your side to stop yours and start your opponent's. Built as a sibling of
[`pomodoro_timer`](https://github.com/rdapaz/pomodoro_timer) and **launcher-ready from
day one** via the PicoSystem launcher app-SDK.

> **Status: Phase 1 (scaffold).** Builds standalone and shows the split-screen READY
> state; the turn engine, settings logic, and polish land in phases 2–4. See
> [docs/SPEC.md](docs/SPEC.md) §12 for the phase plan.

## Controls (target design — SPEC §3)

| Input | Owner | Action |
|---|---|---|
| **D-pad** (◀▶▲▼) | LEFT player | end LEFT's turn *(phase 2)* |
| **A / B / X / Y** | RIGHT player | end RIGHT's turn *(phase 2)* |
| **Hold X + A** | either | pause / resume *(phase 4)* |
| **Hold B + Y** | either | reset to READY *(phase 4)* |
| **Hold X** (in READY) | — | open Settings ✅ |

In Settings: **▲/▼** select · **◀/▶** change · **A** next field · **Y** close & save ·
the **Exit** row hands back to the launcher.

## LED scheme (SPEC §4)

LEFT running → **blue** · RIGHT running → **red** · READY → dim white · PAUSED → amber
slow pulse · low time → active colour fast pulse · FLAGGED → fast red flash. Scaled by
the brightness setting.

## Build & flash (WSL / Ubuntu)

The chess clock **reuses the SDKs vendored in the sibling `pomodoro_timer`** repo (no
separate SDK checkout). With that repo present alongside this one:

```bash
# one-shot configure + build (sets PICO_SDK_PATH / PICOSYSTEM_DIR for you)
bash scripts/setup-wsl.sh
# -> build/chess_clock.uf2
```

Or manually (SPEC §11):

```bash
mkdir -p build && cd build
PICO_SDK_PATH=../../pomodoro_timer/third_party/pico-sdk \
PICOSYSTEM_DIR=../../pomodoro_timer/third_party/picosystem \
  cmake .. -G Ninja && ninja
```

**Flash:** power off, hold **X**, toggle power on (still holding X) — it mounts as a USB
drive labelled `RPI-RP2` (letter varies; **never `D:`**). Copy `build/chess_clock.uf2`
onto it (`cmd /c copy /b chess_clock.uf2 <drive>:\` after ~2 s).

## Launcher integration (SPEC §2)

Slot-ready from the start: `main.cpp` carries `LAUNCHER_DECLARE_APP("Chess Clock", 1)`,
`CMakeLists.txt` calls `launcher_app(chess_clock)` (standalone @ `0x10000000`), and the
Settings **Exit** row calls `launcher::return_to_launcher()`. Switch to a launcher slot
with the one-flag change `launcher_app(chess_clock SLOT n)`.

## Layout

```
src/
  main.cpp        loop, input, LED, module wiring, LAUNCHER_DECLARE_APP
  clock.*         two-clock + turn engine (pure logic, now_ms passed in)
  settings.hpp    Settings + presets + field metadata
  ui.*            split L|R rendering + settings screen (uses font5x7)
  audio.*         click / low-time / flag-alarm cues + sequencer
  storage.*       flash persistence (CRC32 record in the last sector)
  font5x7.*       shared 5×7 LED dot-matrix font (copied from pomodoro_timer)
docs/SPEC.md      the specification
scripts/setup-wsl.sh  configure + build helper
```

## License

[MIT](LICENSE) © 2026 Ricardo Da Paz.

Built with the [Pimoroni `picosystem`](https://github.com/pimoroni/picosystem) library
and the [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk), which carry
their own licenses.
