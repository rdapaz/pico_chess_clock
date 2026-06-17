// storage.cpp — RP2040 flash persistence (SPEC §7, PICOSYSTEM_API §7).
// COPIED from pomodoro_timer/src/storage.cpp (SPEC §10): the record struct now wraps
// the chess Settings and uses a new magic; the flash mechanics are unchanged.
//
// Layout: a single versioned, CRC32-checked record in the last 4 KB sector of flash,
// programmed as one 256-byte page. Reads are memory-mapped (XIP); writes erase the
// sector + program one page with interrupts disabled.
//
// CAVEAT (verify on hardware): picosystem may run display/audio on core1. flash_range_*
// requires no code executing from flash on EITHER core during the op. We disable
// interrupts here (rare writes, only on settings-close). If you ever see a hang on save,
// pause core1 around the write or use the SDK's flash_safe_execute().
#include "storage.hpp"

#include <cstring>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

namespace chess {

static constexpr uint32_t REC_MAGIC   = 0x43484553;  // 'CHES'
static constexpr uint16_t REC_VERSION = 2;  // bumped: Settings gained `rotated`

#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (16u * 1024u * 1024u)  // PicoSystem = 16 MB (board header)
#endif

// Last sector of flash, offset relative to flash start (for erase/program).
static constexpr uint32_t FLASH_TARGET_OFFSET = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;

#pragma pack(push, 1)
struct FlashRecord {
  uint32_t magic;
  uint16_t version;
  uint16_t size;       // sizeof(FlashRecord) — sanity check
  Settings settings;
  uint32_t crc32;      // over all preceding bytes
};
#pragma pack(pop)

static bool     g_dirty            = false;
static bool     g_have_dirty_since = false;
static uint32_t g_dirty_since_ms   = 0;

// Bitwise CRC32 (IEEE 802.3 polynomial, reflected). Small + dependency-free.
static uint32_t crc32_calc(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int k = 0; k < 8; ++k) {
      crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : (crc >> 1);
    }
  }
  return ~crc;
}

void storage_mark_dirty() { g_dirty = true; }

void storage_load(Settings& s) {
  const FlashRecord* rec =
      reinterpret_cast<const FlashRecord*>(XIP_BASE + FLASH_TARGET_OFFSET);

  if (rec->magic == REC_MAGIC && rec->version == REC_VERSION &&
      rec->size == sizeof(FlashRecord)) {
    uint32_t want = crc32_calc(reinterpret_cast<const uint8_t*>(rec),
                               sizeof(FlashRecord) - sizeof(uint32_t));
    if (want == rec->crc32) {
      s = rec->settings;  // valid saved record
      return;
    }
  }
  s = Settings{};  // first run / corrupt / new schema -> defaults
}

void storage_save_now(const Settings& s) {
  static_assert(sizeof(FlashRecord) <= FLASH_PAGE_SIZE, "record must fit one flash page");

  uint8_t page[FLASH_PAGE_SIZE];
  std::memset(page, 0xFF, sizeof(page));

  FlashRecord rec{};
  rec.magic    = REC_MAGIC;
  rec.version  = REC_VERSION;
  rec.size     = sizeof(FlashRecord);
  rec.settings = s;
  rec.crc32    = crc32_calc(reinterpret_cast<const uint8_t*>(&rec),
                            sizeof(FlashRecord) - sizeof(uint32_t));
  std::memcpy(page, &rec, sizeof(FlashRecord));

  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
  flash_range_program(FLASH_TARGET_OFFSET, page, FLASH_PAGE_SIZE);
  restore_interrupts(ints);

  g_dirty            = false;
  g_have_dirty_since = false;
}

void storage_flush(const Settings& s, uint32_t now) {
  if (!g_dirty) {
    g_have_dirty_since = false;
    return;
  }
  if (!g_have_dirty_since) {       // start the debounce window on first dirty frame
    g_dirty_since_ms   = now;
    g_have_dirty_since = true;
    return;
  }
  if (static_cast<uint32_t>(now - g_dirty_since_ms) >= 2000u) {  // ~2 s of quiet
    storage_save_now(s);
  }
}

}  // namespace chess
