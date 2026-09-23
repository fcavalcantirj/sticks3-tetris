#pragma once

#include <cstdint>

#include "stackfall/core/board.h"
#include "stackfall/core/gravity.h"
#include "stackfall/core/hold.h"
#include "stackfall/core/lock.h"
#include "stackfall/core/queue.h"
#include "stackfall/core/spawn.h"
#include "stackfall/core/srs.h"
#include "stackfall/engine/events.h"
#include "stackfall/rules/combo.h"
#include "stackfall/rules/profile.h"
#include "stackfall/rules/scoring.h"
#include "stackfall/rules/tspin.h"

namespace sf {

enum class GameStatus : uint8_t { Playing, GameOver };
enum class OverReason : uint8_t { None, BlockOut, LockOut, ModeComplete };

// The Game owns the board, the active piece, the next queue, gravity, lock
// delay and all scoring state. It never reads a clock; update(nowMs) is driven
// by the HAL. Everything under src/stackfall/engine is still pure C++17 and
// host-tested.
class Game {
 public:
  static constexpr uint8_t kMaxCatchUpTicks = 8;
  static constexpr uint32_t kStallThresholdMs = 1000;

  Game() = default;

  void reset(const RuleProfile& rules, uint32_t seed, uint32_t nowMs);
  void update(uint32_t nowMs);

  // Horizontal move. Returns true if the piece changed column. Wrap is applied
  // when rules_.horizontalWrap is true (see core/kicks.h and rules/profile.h).
  bool move(int dCol);

  // Rotate CW or CCW with wall/floor kicks. Returns true if the piece rotated.
  bool rotate(Turn t);

  // Hold swap. Returns true if the hold was accepted.
  bool hold();

  // Hard drop to ghost row and lock immediately.
  void hardDrop();

  // Soft drop latch. Non-locking; releasing to neutral stops it.
  void setSoftDrop(bool held);

  // Ghost row, recomputed on demand. Presentation only; excluded from the
  // state hash.
  int ghostRow() const;

  const Board& board() const { return board_; }
  const ActivePiece& active() const { return active_; }
  PieceId heldPiece() const { return hold_; }
  bool holdEmpty() const { return holdEmpty_; }
  bool holdUsed() const { return holdUsed_; }
  const NextQueue& queue() const { return queue_; }
  const Gravity& gravity() const { return gravity_; }
  const LockDelay& lock() const { return lock_; }
  int32_t score() const { return score_; }
  uint32_t lines() const { return lines_; }
  uint8_t level() const { return level_; }
  const ChainState& chain() const { return chain_; }
  uint32_t elapsedMs() const { return elapsedMs_; }
  uint16_t tickAccumMs() const { return tickAccumMs_; }
  uint16_t tickMs() const { return tickMs_; }
  uint32_t wallMs() const { return wallMs_; }
  uint32_t discardedMs() const { return discardedMs_; }
  uint32_t lastNowMs() const { return lastNowMs_; }
  bool lastMoveWasRotation() const { return lastMoveWasRotation_; }
  int8_t lastKickIndex() const { return lastKickIndex_; }
  bool softDropHeld() const { return softDropHeld_; }
  uint16_t pendingSoftDropCells() const { return pendingSoftDropCells_; }
  uint16_t pendingHardDropCells() const { return pendingHardDropCells_; }
  uint16_t stallCount() const { return stallCount_; }
  GameStatus status() const { return status_; }
  OverReason overReason() const { return overReason_; }
  const RuleProfile& rules() const { return rules_; }
  const EventQueue& events() const { return events_; }

  // Test-only hook: seat an arbitrary position. The only caller outside this
  // file should be the scenario loader in test_scenarios.cpp (task 36).
  // nowMs sets both elapsedMs_ and lastNowMs_ so the injected piece gets a
  // fresh clock instead of inheriting the previous piece's state.
  void injectForTest(const Board& b, ActivePiece active, PieceId hold,
                     bool holdEmpty, uint32_t nowMs);

 private:
  void spawn(PieceId id);
  void tick();
  void updateGround();
  void lockPiece();
  bool isGrounded() const;
  bool tryHorizontalMove(int dCol);

  Board board_;
  ActivePiece active_{PieceId::I, 0, 0, 0};
  PieceId hold_ = PieceId::I;
  bool holdEmpty_ = true;
  bool holdUsed_ = false;
  NextQueue queue_;
  Gravity gravity_;
  LockDelay lock_;
  int32_t score_ = 0;
  uint32_t lines_ = 0;
  uint8_t level_ = 1;
  ChainState chain_{-1, false};
  uint32_t elapsedMs_ = 0;
  uint16_t tickAccumMs_ = 0;
  uint32_t lastNowMs_ = 0;
  // Wall clock: accumulated from raw dtMs before any clamping. Used by timed
  // game modes so stalls cannot gift the player free time.
  uint32_t wallMs_ = 0;
  // Milliseconds discarded by the tick-accumulator clamp so the loss is visible
  // rather than silently wrapping.
  uint32_t discardedMs_ = 0;
  bool lastMoveWasRotation_ = false;
  int8_t lastKickIndex_ = -1;
  bool softDropHeld_ = false;
  uint16_t pendingSoftDropCells_ = 0;
  uint16_t pendingHardDropCells_ = 0;
  uint16_t stallCount_ = 0;
  GameStatus status_ = GameStatus::Playing;
  OverReason overReason_ = OverReason::None;
  EventQueue events_;

  // Pending RuleProfile fields and the tasks that will consume them:
  // - dasMs, arrMs: consumed by the input/DAS-ARR task.
  RuleProfile rules_;

  // Tick duration taken from the active profile in reset().
  uint16_t tickMs_ = 8;
};

static_assert(RuleProfile{}.tickMs == 8, "profile default tick is 8 ms");
static_assert(RuleProfile{}.lockDelayMs == kLockDelayMs,
              "profile default lock delay matches core lock delay");
static_assert(RuleProfile{}.lockResetLimit == kMaxMoveResets,
              "profile default reset limit matches core reset limit");

}  // namespace sf
