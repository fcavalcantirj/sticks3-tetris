# Stackfall rules

## Matrix
10 columns x 20 VISIBLE rows, stored as 10 x 40 with 20 hidden buffer rows.
`boardRow` runs 0..39 top-down; hidden rows are 0..19, visible rows are
20..39, and the renderer maps `row = boardRow - 20`.
Seven pieces: I, J, L, O, S, T, Z.
Spawn: origin column 3 in the piece's own bounding box, with the topmost
OCCUPIED cell of the spawn state at `boardRow` 19 — one row above the first
visible row — so an ordinary spawn is invisible and a block-out is not.

## Rotation
Four states 0/R/2/L, SRS-style, five candidate offsets per transition tried
in order. A SEPARATE kick table for I and for JLSTZ; O never kicks because
it never moves.

## Randomizer
A pinned XorShift32, written exactly as:

```
x ^= x << 13; x ^= x >> 17; x ^= x << 5;
```

on a `uint32_t`, with a seed of 0 replaced by `0x9E3779B9` before the first
step (a zero state is an absorbing state and would emit one piece forever).
7-bag: each bag is a permutation of all seven pieces produced by
Fisher-Yates over that generator, so any seven consecutive pieces from a bag
boundary contain each piece exactly once.

## Queue
At least 7 pieces held internally, exactly 3 shown in the sidebar.

## Hold
One slot, usable once per active piece, re-armed on lock.

## Ghost
The landing projection of the active piece, drawn as an outline, never
colliding with itself.

## Timings
`lockDelayMs = 500`, `lockResetLimit = 15` (the 16th legal move or rotation
on the ground does NOT reset the timer and the piece locks on schedule),
`dasMs = 167`, `arrMs = 33`.
Gravity comes from a table indexed by `level - 1`:

```
kGravityMsByLevel[15] = { 1000, 793, 618, 473, 355, 262, 190, 135, 94, 64, 43, 28, 18, 11, 7 }
```

in milliseconds per row, and level > 15 clamps to the last entry (7 ms).
Levels start at 1 and advance by 1 for every 10 lines cleared. Every one of
these is an integer in milliseconds — no floats reach the engine, because
float behaviour differs between clang-on-macOS and xtensa-gcc and would
break the host-versus-device parity hash.

## Scoring
All values multiplied by the current level unless stated: Single 100,
Double 300, Triple 500, QUAD 800. Soft drop +1 per cell and hard drop +2 per
cell, NOT multiplied by level. T-spin no-lines 400, single 800, double 1200,
triple 1600; T-spin mini no-lines 100, mini single 200, mini double 400.
Back-to-back multiplies the line-clear component of a difficult clear (a
QUAD, or any T-spin that clears at least one line) by 1.5 when the previous
line-clearing piece was also difficult — computed in integer arithmetic as
`(v * 3) / 2`, never with a float. Combo scores `50 * comboCount * level`
where `comboCount` is the number of CONSECUTIVE line-clearing pieces minus
one, so the first clear scores no combo bonus and a lock without a clear
resets it to zero. Perfect clear adds Single 800, Double 1200, Triple 1800,
QUAD 2000, and a back-to-back QUAD perfect clear 3200.

## Modes
Neutral names only, no trademarked mode names: `Endless` (plays until
top-out, level rises forever), `Lines40` (ends when the 40th line clears; the
elapsed time is the score to beat), `Time180` (ends after 180000 ms; the
score at expiry is final).

## RuleProfile
Declare the struct every rule constant is read from, so no `if (mode == ...)`
ever appears in the board engine:

```
RotationSystem rotation; RandomizerType randomizer; uint16_t lockDelayMs; uint8_t lockResetLimit; uint16_t dasMs; uint16_t arrMs; bool holdEnabled; bool ghostEnabled; bool tSpinsEnabled; bool combosEnabled; bool backToBackEnabled; bool horizontalWrap; ScoringProfile scoring;
```

## Deterministic state hash
Replay and host-versus-device parity use 32-bit FNV-1a (offset basis
`0x811C9DC5`, prime `0x01000193`). Multi-byte integers are emitted
little-endian one byte at a time; raw structs and padding are never hashed.

The canonical order is: all 400 board cells row-major from row 0; active
piece id/state/column/row; held piece plus empty/used flags; next-queue size
and every queued piece; score, lines and level; combo count and back-to-back
flag; lock resets, grounded flag and deadline start; gravity accumulator;
tick accumulator; elapsed engine milliseconds; pending soft- and hard-drop
cells; game status; game-over reason.

Excluded state is cosmetic or caller-clock-dependent: ghost projection,
events and their dropped counter, banners and animation timers, frame
counters, brightness, the caller's last timestamp and stall count. The hash
also excludes all other engine fields not named in the canonical order.

## Naming
Never write the trademarked franchise name or its piece-name variant anywhere
in `src/` or `release/` — `tools/gates.sh` greps for both — and call a
four-line clear QUAD in every UI string.
