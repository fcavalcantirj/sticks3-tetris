#pragma once

#include <cstddef>
#include <cstdint>

#include "stackfall/engine/game.h"

namespace sf {

constexpr uint32_t kFnv1aBasis = 0x811C9DC5u;
constexpr uint32_t kFnv1aPrime = 0x01000193u;

inline uint32_t fnv1a(const uint8_t* data, size_t len,
                      uint32_t basis = 0x811C9DC5u) {
  uint32_t h = basis;
  for (size_t i = 0; i < len; ++i) {
    h ^= data[i];
    h *= kFnv1aPrime;
  }
  return h;
}

// Incremental FNV-1a encoder. Multi-byte integers are emitted little-endian
// explicitly: raw structs and their uninitialised padding are never hashed.
struct StateHash {
  uint32_t h = 0x811C9DC5u;

  void u8(uint8_t value) {
    h ^= value;
    h *= kFnv1aPrime;
  }

  void u16(uint16_t value) {
    u8(static_cast<uint8_t>(value));
    u8(static_cast<uint8_t>(value >> 8));
  }

  void u32(uint32_t value) {
    u8(static_cast<uint8_t>(value));
    u8(static_cast<uint8_t>(value >> 8));
    u8(static_cast<uint8_t>(value >> 16));
    u8(static_cast<uint8_t>(value >> 24));
  }

  uint32_t value() const { return h; }
};

// Canonical engine-state order. Changing this order invalidates every replay:
// board; active; hold; queue; score/lines/level; chain; lock; gravity;
// tick accumulator; elapsed time; pending drop cells; status/reason.
//
// Deliberately excluded as cosmetic or caller-clock state: ghost row, event
// queue and dropped counter, banner/animation timers, frame counters,
// brightness, lastNowMs and stallCount. The exact list above also excludes
// every other Game field, including wall-clock diagnostics.
inline uint32_t stateHash(const Game& game) {
  StateHash hash;

  for (int row = 0; row < Board::kTotalRows; ++row) {
    for (int col = 0; col < Board::kWidth; ++col) {
      hash.u8(static_cast<uint8_t>(game.board().at(col, row)));
    }
  }

  hash.u8(static_cast<uint8_t>(game.active().id));
  hash.u8(game.active().state);
  hash.u8(static_cast<uint8_t>(game.active().col));
  hash.u8(static_cast<uint8_t>(game.active().row));

  hash.u8(static_cast<uint8_t>(game.heldPiece()));
  hash.u8(game.holdEmpty() ? 1u : 0u);
  hash.u8(game.holdUsed() ? 1u : 0u);

  const int queueSize = game.queue().size();
  hash.u8(static_cast<uint8_t>(queueSize));
  for (int i = 0; i < queueSize; ++i) {
    hash.u8(static_cast<uint8_t>(game.queue().peek(i)));
  }

  hash.u32(static_cast<uint32_t>(game.score()));
  hash.u32(game.lines());
  hash.u8(game.level());

  hash.u16(static_cast<uint16_t>(game.chain().comboCount));
  hash.u8(game.chain().b2b ? 1u : 0u);

  hash.u8(game.lock().resets());
  hash.u8(game.lock().grounded() ? 1u : 0u);
  hash.u32(game.lock().deadlineStart());

  hash.u32(game.gravity().accumulatorMs());
  hash.u16(game.tickAccumMs());
  hash.u32(game.elapsedMs());
  hash.u16(game.pendingSoftDropCells());
  hash.u16(game.pendingHardDropCells());
  hash.u8(static_cast<uint8_t>(game.status()));
  hash.u8(static_cast<uint8_t>(game.overReason()));

  return hash.value();
}

}  // namespace sf
