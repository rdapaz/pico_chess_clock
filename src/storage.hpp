// storage.hpp — flash persistence for Settings (SPEC §7).
// One CRC32-checked, versioned record in the LAST 4 KB sector of the 16 MB QSPI flash.
// COPIED from pomodoro_timer/src/storage.hpp (SPEC §10), struct swapped, namespace -> chess.
#pragma once
#include "settings.hpp"

namespace chess {

// Load the saved settings; on first run / corrupt / version-mismatch, fills defaults.
void storage_load(Settings& s);

// Mark the record dirty (call after any settings change). The actual flash write is
// deferred + debounced to protect flash endurance (writes only on quiet, SPEC §7).
void storage_mark_dirty();

// Call every update() frame. Writes the record iff it's dirty AND the debounce window
// (~2 s of quiet) has elapsed. `now` is time() ms.
void storage_flush(const Settings& s, uint32_t now);

// Force an immediate synchronous write (e.g. on Settings-close). Clears the dirty flag.
void storage_save_now(const Settings& s);

}  // namespace chess
