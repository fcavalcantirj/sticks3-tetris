#include "stackfall/engine/game.h"

#include "stackfall/core/ghost.h"
#include "stackfall/core/kicks.h"

namespace sf {

void Game::reset(const RuleProfile& rules, uint32_t seed, uint32_t nowMs) {
  rules_ = rules;
  rules_.seed = seed;
  tickMs_ = rules_.tickMs;
  if (tickMs_ == 0) {
    tickMs_ = 8;
  }

  board_.clear();
  queue_.configure(rules_.nextQueueMin, rules_.nextPreviewCount);
  queue_.reset(seed);

  hold_ = PieceId::I;
  holdEmpty_ = true;
  holdUsed_ = false;

  score_ = 0;
  lines_ = 0;
  level_ = rules_.startLevel;
  chain_ = ChainState{-1, false};

  elapsedMs_ = 0;
  tickAccumMs_ = 0;
  lastNowMs_ = nowMs;
  wallMs_ = 0;
  discardedMs_ = 0;
  lastMoveWasRotation_ = false;
  lastKickIndex_ = -1;
  softDropHeld_ = false;
  pendingSoftDropCells_ = 0;
  pendingHardDropCells_ = 0;
  stallCount_ = 0;

  status_ = GameStatus::Playing;
  overReason_ = OverReason::None;
  events_ = EventQueue{};

  gravity_.reset(static_cast<int>(level_));
  lock_.configure(rules_.lockDelayMs, rules_.lockResetLimit);

  spawn(queue_.pop());
}

void Game::update(uint32_t nowMs) {
  if (status_ != GameStatus::Playing) {
    return;
  }

  // Raw wall-clock delta, accumulated BEFORE any clamping so stalls cannot
  // gift the player free time. Timed game modes read wallMs_; lock delay and
  // gravity read elapsedMs_, the engine's fixed tick clock.
  uint32_t rawDt = nowMs - lastNowMs_;
  lastNowMs_ = nowMs;

  if (rawDt == 0) {
    return;
  }

  wallMs_ += rawDt;

  uint32_t dtMs = rawDt;
  if (dtMs > kStallThresholdMs) {
    dtMs = tickMs_;
    ++stallCount_;
  }

  // Clamp the accumulator to the catch-up budget so a uint16_t can never wrap.
  // Anything beyond the budget is time the game has already decided to drop.
  uint32_t accum = static_cast<uint32_t>(tickAccumMs_) + dtMs;
  uint32_t maxAccum = static_cast<uint32_t>(kMaxCatchUpTicks) * tickMs_;
  if (accum > maxAccum) {
    discardedMs_ += accum - maxAccum;
    accum = maxAccum;
  }
  tickAccumMs_ = static_cast<uint16_t>(accum);

  uint8_t ticks = 0;
  while (tickAccumMs_ >= tickMs_ && ticks < kMaxCatchUpTicks &&
         status_ == GameStatus::Playing) {
    tickAccumMs_ -= tickMs_;
    tick();
    ++ticks;
  }
}

bool Game::move(int dCol) {
  if (status_ != GameStatus::Playing) {
    return false;
  }

  int newCol = static_cast<int>(active_.col) + dCol;

  if (rules_.horizontalWrap) {
    const Shape& s = shapeOf(active_.id, static_cast<int>(active_.state));
    int minOff = 0;
    int maxOff = 0;
    for (int i = 0; i < 4; ++i) {
      int off = static_cast<int>(s[static_cast<size_t>(i)].col);
      if (off < minOff) minOff = off;
      if (off > maxOff) maxOff = off;
    }
    if (newCol + maxOff > Board::kWidth - 1) {
      newCol = -minOff;
    } else if (newCol + minOff < 0) {
      newCol = (Board::kWidth - 1) - maxOff;
    }
  }

  if (board_.collides(active_.id, static_cast<int>(active_.state), newCol,
                      static_cast<int>(active_.row))) {
    return false;
  }

  bool wasGrounded = lock_.grounded();

  active_.col = static_cast<int8_t>(newCol);
  lastMoveWasRotation_ = false;
  lastKickIndex_ = -1;

  events_.push(GameEvent{EventType::Move, static_cast<uint8_t>(active_.col), 0,
                         0, elapsedMs_ / tickMs_});

  updateGround();
  // Only count a reset when the piece was ALREADY grounded before the move. A
  // piece slid into place from the air must not burn a reset on first ground.
  if (wasGrounded && lock_.grounded()) {
    lock_.onMoveReset(elapsedMs_);
  }
  return true;
}

bool Game::rotate(Turn t) {
  if (status_ != GameStatus::Playing) {
    return false;
  }

  int kickIndex = -1;
  if (!tryRotate(board_, active_, t, kickIndex)) {
    return false;
  }

  lastKickIndex_ = static_cast<int8_t>(kickIndex);
  lastMoveWasRotation_ = true;

  events_.push(GameEvent{EventType::Rotate,
                         static_cast<uint8_t>(active_.state),
                         static_cast<uint8_t>(kickIndex), 0,
                         elapsedMs_ / tickMs_});
  if (kickIndex != 0) {
    events_.push(GameEvent{EventType::Kick, static_cast<uint8_t>(kickIndex),
                           0, 0, elapsedMs_ / tickMs_});
  }

  updateGround();
  return true;
}

bool Game::hold() {
  if (status_ != GameStatus::Playing) {
    return false;
  }

  if (!rules_.holdEnabled) {
    events_.push(GameEvent{EventType::HoldDenied, 0, 0, 0,
                           elapsedMs_ / tickMs_});
    return false;
  }

  if (holdUsed_) {
    events_.push(GameEvent{EventType::HoldDenied, 0, 0, 0,
                           elapsedMs_ / tickMs_});
    return false;
  }

  PieceId outgoing = active_.id;
  PieceId incoming;
  if (holdEmpty_) {
    hold_ = outgoing;
    holdEmpty_ = false;
    incoming = queue_.pop();
  } else {
    incoming = hold_;
    hold_ = outgoing;
  }

  bool ok = trySpawn(board_, incoming, active_);
  if (!ok) {
    status_ = GameStatus::GameOver;
    overReason_ = OverReason::BlockOut;
    events_.push(GameEvent{EventType::GameOver,
                           static_cast<uint8_t>(OverReason::BlockOut), 0, 0,
                           elapsedMs_ / tickMs_});
    return false;
  }

  holdUsed_ = true;
  lock_.reset();
  lastMoveWasRotation_ = false;
  lastKickIndex_ = -1;
  pendingSoftDropCells_ = 0;
  pendingHardDropCells_ = 0;
  gravity_.reset(static_cast<int>(level_));

  events_.push(GameEvent{EventType::Hold,
                         static_cast<uint8_t>(active_.id), active_.state, 0,
                         elapsedMs_ / tickMs_});

  updateGround();
  return true;
}

void Game::hardDrop() {
  if (status_ != GameStatus::Playing) {
    return;
  }

  int cells = dropDistance(board_, active_);
  active_.row = static_cast<int8_t>(static_cast<int>(active_.row) + cells);
  pendingHardDropCells_ = static_cast<uint16_t>(cells);
  lastMoveWasRotation_ = false;
  lastKickIndex_ = -1;

  events_.push(GameEvent{EventType::HardDrop, static_cast<uint8_t>(cells), 0,
                         static_cast<int32_t>(cells) * 2,
                         elapsedMs_ / tickMs_});

  lockPiece();
}

void Game::setSoftDrop(bool held) {
  if (status_ != GameStatus::Playing) {
    return;
  }

  if (softDropHeld_ == held) {
    return;
  }
  softDropHeld_ = held;
  events_.push(GameEvent{EventType::SoftDrop, held ? static_cast<uint8_t>(1)
                                                   : static_cast<uint8_t>(0),
                         0, 0, elapsedMs_ / tickMs_});
}

int Game::ghostRow() const {
  if (!rules_.ghostEnabled) {
    return static_cast<int>(active_.row);
  }
  return sf::ghostRow(board_, active_);
}

void Game::injectForTest(const Board& b, ActivePiece active, PieceId hold,
                         bool holdEmpty, uint32_t nowMs) {
  board_ = b;
  active_ = active;
  hold_ = hold;
  holdEmpty_ = holdEmpty;
  holdUsed_ = false;
  lastMoveWasRotation_ = false;
  lastKickIndex_ = -1;
  pendingSoftDropCells_ = 0;
  pendingHardDropCells_ = 0;
  softDropHeld_ = false;

  elapsedMs_ = nowMs;
  lastNowMs_ = nowMs;

  // Fully reset per-piece timing state so the injected piece gets a fresh lock
  // delay and gravity accumulator instead of inheriting them from the previous
  // piece.
  gravity_.reset(static_cast<int>(level_));
  lock_.configure(rules_.lockDelayMs, rules_.lockResetLimit);

  status_ = GameStatus::Playing;
  overReason_ = OverReason::None;

  updateGround();
}

void Game::spawn(PieceId id) {
  bool ok = trySpawn(board_, id, active_);
  if (!ok) {
    status_ = GameStatus::GameOver;
    overReason_ = OverReason::BlockOut;
    events_.push(GameEvent{EventType::GameOver,
                           static_cast<uint8_t>(OverReason::BlockOut), 0, 0,
                           elapsedMs_ / tickMs_});
    return;
  }

  holdUsed_ = false;
  lock_.reset();
  lastMoveWasRotation_ = false;
  lastKickIndex_ = -1;
  pendingSoftDropCells_ = 0;
  pendingHardDropCells_ = 0;
  softDropHeld_ = false;

  // A new piece must start with a fresh gravity accumulator, not inherit the
  // previous piece's partial fall or the lock-delay dwell.
  gravity_.reset(static_cast<int>(level_));

  events_.push(GameEvent{EventType::Spawn,
                         static_cast<uint8_t>(active_.id), active_.state, 0,
                         elapsedMs_ / tickMs_});

  updateGround();
}

void Game::tick() {
  // Gravity: move down one row at a time, stopping at the first collision.
  // Do not step gravity while already grounded: the accumulator must not
  // advance during lock delay, or the next piece inherits a partial fall.
  if (!isGrounded()) {
    int cells = gravity_.step(tickMs_, softDropHeld_);
    for (int i = 0; i < cells; ++i) {
      if (isGrounded()) {
        break;
      }
      ++active_.row;
      if (softDropHeld_) {
        ++pendingSoftDropCells_;
      }
    }
    updateGround();
  }

  // Advance the engine clock. Lock delay and gravity read this value.
  elapsedMs_ += tickMs_;

  // Mode timer / line target. Timed modes use wallMs_ so stalls cannot be
  // used to stretch the clock; Endless and FortyLine ignore elapsed time.
  if (modeStatus(rules_, lines_, wallMs_) == ModeStatus::Complete) {
    status_ = GameStatus::GameOver;
    overReason_ = OverReason::ModeComplete;
    events_.push(GameEvent{EventType::GameOver,
                           static_cast<uint8_t>(OverReason::ModeComplete), 0,
                           0, elapsedMs_ / tickMs_});
    return;
  }

  // Lock delay.
  if (lock_.expired(elapsedMs_)) {
    lockPiece();
  }
}

void Game::updateGround() {
  bool grounded = isGrounded();
  if (grounded && !lock_.grounded()) {
    lock_.onGrounded(elapsedMs_);
  } else if (!grounded && lock_.grounded()) {
    lock_.onAirborne();
  }
}

void Game::lockPiece() {
  board_.place(active_.id, static_cast<int>(active_.state),
               static_cast<int>(active_.col), static_cast<int>(active_.row));

  events_.push(GameEvent{EventType::Lock, 0, 0, 0, elapsedMs_ / tickMs_});

  if (classifyLock(active_) == TopOut::LockOut) {
    status_ = GameStatus::GameOver;
    overReason_ = OverReason::LockOut;
    events_.push(GameEvent{EventType::GameOver,
                           static_cast<uint8_t>(OverReason::LockOut), 0, 0,
                           elapsedMs_ / tickMs_});
    return;
  }

  // Classify the T-spin BEFORE clearing lines; the corner test uses the
  // board state at the moment of lock.
  SpinKind spin = SpinKind::None;
  if (rules_.tSpinsEnabled && active_.id == PieceId::T &&
      lastMoveWasRotation_) {
    SpinQuery q{active_.id, active_.state, active_.col, active_.row,
                lastKickIndex_, lastMoveWasRotation_};
    spin = classifySpin(board_, q);
  }

  int clearedRows[4];
  int clearCount = board_.clearLines(clearedRows);

  ClearKind kind = ClearKind::None;
  if (clearCount == 1) {
    kind = ClearKind::Single;
  } else if (clearCount == 2) {
    kind = ClearKind::Double;
  } else if (clearCount == 3) {
    kind = ClearKind::Triple;
  } else if (clearCount >= 4) {
    kind = ClearKind::Quad;
  }

  ChainResult cr = advanceChain(chain_, kind, spin, rules_.maxCombo);
  chain_ = cr.next;

  // Perfect clear is checked after compression so a cell hidden in the buffer
  // cannot be mistaken for a clear.
  bool perfect = rules_.perfectClearEnabled && isPerfectClear(board_);

  // Build ONE ClearEvent and compute the score exactly once. The level is
  // the one the player was on before this lock's lines are added.
  ClearEvent ev;
  ev.clear = kind;
  ev.spin = spin;
  ev.comboCount = rules_.combosEnabled ? cr.comboForScoring : 0;
  ev.backToBack = rules_.backToBackEnabled ? cr.payB2b : false;
  ev.perfectClear = rules_.perfectClearEnabled ? perfect : false;
  ev.softDropCells = pendingSoftDropCells_;
  ev.hardDropCells = pendingHardDropCells_;
  ev.level = level_;

  ScoreBreakdown bd = scoreFor(ev);
  score_ += bd.total;

  // Push scoring events in the required order, each carrying its matching
  // ScoreBreakdown component in value.
  if (spin != SpinKind::None) {
    events_.push(GameEvent{EventType::TSpin,
                           static_cast<uint8_t>(spin), 0, bd.base,
                           elapsedMs_ / tickMs_});
  }
  if (clearCount > 0) {
    events_.push(GameEvent{EventType::LineClear,
                           static_cast<uint8_t>(clearCount), 0, bd.base,
                           elapsedMs_ / tickMs_});
  }
  if (cr.comboForScoring > 0) {
    events_.push(GameEvent{EventType::Combo,
                           static_cast<uint8_t>(cr.comboForScoring), 0,
                           bd.combo, elapsedMs_ / tickMs_});
  }
  if (cr.payB2b) {
    events_.push(GameEvent{EventType::BackToBack, 1, 0, bd.b2bBonus,
                           elapsedMs_ / tickMs_});
  }
  if (perfect) {
    events_.push(GameEvent{EventType::PerfectClear,
                           static_cast<uint8_t>(clearCount), 0, bd.perfect,
                           elapsedMs_ / tickMs_});
  }

  uint8_t oldLevel = level_;
  lines_ += static_cast<uint32_t>(clearCount);
  level_ = levelFor(rules_, lines_);
  if (level_ != oldLevel) {
    events_.push(GameEvent{EventType::LevelUp, level_, 0, 0,
                           elapsedMs_ / tickMs_});
  }
  gravity_.setLevel(static_cast<int>(level_));

  spawn(queue_.pop());
}

bool Game::isGrounded() const {
  return board_.collides(active_.id, static_cast<int>(active_.state),
                         static_cast<int>(active_.col),
                         static_cast<int>(active_.row) + 1);
}

}  // namespace sf
