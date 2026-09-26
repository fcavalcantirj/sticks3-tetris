# Stackfall plays itself: a heuristic, a language model and one USB cable

*2026-09-26. Everything below was measured that day, either on the host (the real game
engine compiled for the Mac) or on the real M5StickS3. Each number says which.*

**Video (2.5 min):** [watch it play](https://drive.google.com/file/d/1ffaFAq8eJvOVujt2KnG6DF8Hn3Vkoui8/preview): the lookahead heuristic (not JEV) plays a 40-line game on the real stick at the watch pace: 40 lines in 101 pieces, 14,922 points, 153.5 s, 0 misplaced pieces (firmware v1.2.0).

Stackfall is an original falling-block puzzle for a single M5StickS3: a 135×240 screen, an
IMU, two buttons, and a player who steers by tilting the stick. This article covers the
autopilot we built for it. Two brains play the game and compete: a classic hand-weighted
heuristic, and a small language model that picks moves with no training at all.

**The short version.**

- **On the real stick, the heuristic never misplaced a piece.** It finished a 40-line game in
  about 10 seconds at full speed, and in two and a half minutes at a pace a human can follow.
- **The language model, called JEV here, reads a list of candidate moves and answers with one
  letter.**
  - At 0.6 billion parameters it plays exactly like a random player.
  - At 4 billion it starts clearing lines, but still tops out quickly.

## The idea: decide once per piece, from outside the game

The game engine is pure C++17: board, SRS-style rotation with wall kicks, gravity, lock
delay, scoring. It never includes a hardware header, so it compiles and runs its 428 tests on
a Mac. The firmware wraps the engine with the display, IMU and buttons.

The autopilot lives on the PC. For each new piece, it:

1. **Enumerates every reachable placement.** It tries each rotation path from the spawn
   position (none, one, two or three clockwise turns, or one counter-clockwise), then every
   column reachable by single-column shifts, then a hard drop. Each step is simulated exactly
   as the engine would do it, wall kicks included.
2. **Scores each result by four features** of the board it leaves behind: lines cleared,
   holes, bumpiness and aggregate height.
3. **Lets a brain pick one**, then sends that whole move to the stick as one batch.

Every brain chooses from the same list. That makes them comparable move by move: the
heuristic, the language model, a random floor, and later a learned scorer.

The plan was host first: prove the brain against the real engine before touching the
device, then add a thin serial link to the firmware, then drive the stick over USB.

## One protocol, two devices

The link is plain text over the stick's native USB serial port, and every line fits in 120
characters. After each new piece, the stick prints two lines. These are real, piece 150 of
the watch-pace game:

```
[BOARD] n=150 top=34 r=606467FFFVFV
[STATE] n=150 t=458404 g=2 scr=P st=P why=- a=T0,3,20 h=-0 q=ZJI sc=3338 lv=2 ln=14
```

- **The board line** encodes each row as its 10-bit occupancy in two base-32 characters.
  `00` is an empty row and `VV` a full one. Only rows from the first occupied one down are
  sent, so even a completely full 40-row board takes 109 characters.
- **The state line** carries a piece counter `n`, which only ever goes up. It also carries the
  screen, the game status, the active piece (id, rotation, column, row), hold, the three
  previewed pieces, score, level and lines.

The PC answered that piece with:

```
M 150 CCR
M 150 H
```

That means: apply rotate-clockwise, rotate-clockwise, right to piece 150, and then the hard
drop. At full speed this is a single line, `M 150 CCRH`; the watch pace, described below,
splits off the drop so you can see the piece land. The ops map one to one onto the game's
existing actions and go through the same entry point the tilt controls use. The stick refuses a batch with a `nak` if `n` is not the current piece, so a late
answer can never land on the next piece.

**The link is off at every boot.** It prints nothing and ignores everything except the switch-on
command. So a player who never plugs in a PC gets exactly the old firmware. Once a PC
switches it on, a 3-second watchdog switches it back off if the PC goes quiet. In drive mode,
tilt and game-key actions are dropped while a game runs, so the stick can't fight the PC.
Pause and the menus still work.

The part that paid off most is `sf_twin`, a "virtual device" for the host. It is the real
engine and the very same link code, behind stdin and stdout instead of USB. The PC driver
cannot tell it from the stick. The whole protocol, the driver, and every brain were
exercised on the twin, tens of thousands of pieces of it, before the firmware was ever
flashed.

## Is the PC's model of the board right?

The enumerator is a small Python copy of the engine's rules. Copies drift, so it was checked
against the real engine two ways, both on the host:

- **Tables.** The twin prints the engine's shape, kick, spawn and transition tables straight
  from the C++ headers, and the Python tables must match exactly.
- **Probes.** On every state of real games, every candidate move was executed on a copy of
  the engine's game and compared with the Python prediction: the resulting board, lines
  cleared, top-outs, and the next piece's spawn. That was **213,634 probes over 4,358 game
  states, with 0 mismatches**. Random move strings were included to exercise wall kicks the
  enumerator never uses.

This check surfaced a real engine quirk. In the buttons-only control profile, the engine
wraps pieces around the side walls, and its wrap test measures a piece's column span from
zero instead of from the piece's first filled column. So a vertical I-piece wraps before its
cells can ever reach column 0 by shifting. The heuristic tops out in 59–97 pieces on that
profile. On the default tilt profile, which the autopilot uses, the issue doesn't exist.

## The baseline: four numbers and a weighted sum

The heuristic scores a placement as

    −0.510066·height + 0.760666·lines − 0.35663·holes − 0.184483·bumpiness

These are the published genetic-algorithm weights of Yiyuan Lee's 2013 bot, recalled rather
than re-fetched; the soak below is what proves they work here.

**Scoring one piece at a time was not enough** (host, 20 fixed seeds × 1000 pieces). It
topped out in 3 of 20 games, at pieces 179, 301 and 373. It happily trades a hole for a line
clear, and the holes pile up.

**Looking one piece ahead fixed it.** The lookahead version places the previewed next piece on
each candidate's result and scores the pair, with the same features and weights. It survived
all **20 of 20 games of 1000 pieces**, with 396–399 lines each. That's close to the maximum,
since 1000 pieces bring 4000 cells, and the model's prediction was confirmed after every
piece. Decisions took 21 ms at the median and 85 ms at worst. This became the baseline.

## JEV: asking a small language model to choose

JEV (the approach from "jev in 25 lines" by nobodywho) is a decision method, not a game AI:

1. Show a local model the board and the candidates, each under a one-letter label.
2. Run one forward pass over the prompt.
3. Read the model's next-token probabilities for those letters.

There's no text generation, no training and no reinforcement learning. The only thing
iterated is the prompt. We ran it with llama-cpp-python on the Mac's GPU, using Qwen3 models.

One trap along the way: in llama-cpp-python 0.3.35, the familiar `scores[n_tokens - 1]` row
is only filled when the model is built with `logits_all=True`. That flag computes logits for
every prompt position, 151,936 values each. Reading the last position directly
(`llama_get_logits_ith(ctx, -1)`) gives the same answer at a fraction of the cost: the same
argmax, with logits within 0.018 of each other.

**It plays like a coin.** Host bench: 20 seeds, 300-piece cap, mean per game.

| brain | pieces | lines | points | agrees with baseline (top-1 / top-3) |
|---|---|---|---|---|
| lookahead heuristic (baseline) | 300 | 118.3 | 102,290 | — |
| one-piece heuristic | 293.9 | 112.5 | 98,853 | 61.8% / 88.6% |
| random | 17.6 | 0 | 362 | 5.7% / 14.7% |
| JEV 0.6B, board only | 20.1 | 0 | 401 | 5.7% / 16.7% |
| JEV 0.6B, four features only | 21.9 | 0 | 436 | 6.4% / 20.8% |
| JEV 0.6B, both | 23.2 | 0.1 | 466 | 8.4% / 23.2% |

**Why.** Two small tests on the same 40 states found a split:

- **With about 23 options, the model mostly answers "A".** It chose A in 19 of 40 states, with an
  average probability of 0.33 where uniform would be 0.054. That's position bias, not reading.
- **With only three options** (the best, the worst and a middle move), it picked the best in 25
  of 40 states, where chance would be 13. So it can compare moves, just not in a long list.

**Climbing the ladder.** No training was allowed at this stage: fine-tuning was reserved as a
last resort. So we tried only prompt and model levers, first on 40 fixed states, then in full
games. All on the host, with games capped at 100 pieces.

| configuration | pieces | lines | points |
|---|---|---|---|
| random | ~18 | 0 | ~365 |
| 0.6B, full list | 21.9 | 0 | 436 |
| 0.6B, knockout in groups of 3 | 23.8 | 0.1 | 500 |
| 1.7B, knockout in groups of 3 | 26.6 | 0.2 | 581 |
| 1.7B, knockout in pairs | 28.6 | 0.6 | 669 |
| **4B, full list** | **56.2** | **8.7** | **2,226** |
| lookahead heuristic, same cap | 100 | 38.2 | 14,411 |

The small 0.6B runs above used the 300-piece cap, but they never reach 100 pieces, so the
rows are comparable. What each lever did:

- **Letter-prior calibration made things worse.** Measuring the model's letter preferences on a
  content-free prompt and dividing them out left it 2.5% top-1, down from 7.5%.
- **Knockout rounds helped the small models.** Showing the candidates a few at a time, with
  winners advancing, was up to 45% top-1 for 1.7B in pairs on clean boards.
- **Model size moved the actual play.** The 4B model reads the full list with little letter
  bias and is the first configuration that clears lines. It still topped out in every game,
  and it needs about 3 seconds per move.

We also evaluated Ollaya, a local decision-model server that returns per-option
probabilities. It handled 52 options, but on this Mac it answered in 217 ms at the median,
513 ms at p95 and up to 2.5 s. It agreed with the heuristic on 2 of 30 states, so it stayed
out.

## Gravity is not the clock

On the stick, the piece keeps falling while the PC thinks. The design keeps that harmless:

- The engine resets a new piece's gravity timer when it spawns.
- The PC plans from the spawn state and sends the whole placement as one batch.
- The stick applies the batch before its next engine tick.

So a batch that arrives before the first gravity step lands exactly as planned. Before any
hardware, the twin emulated the round trip by advancing the engine clock before each batch.
A late batch still lands the same while the piece is above the stack:

- up to 50 ms: 0 divergences at every level, even level 15's 7 ms per row;
- up to 100 ms: clean through level 14;
- up to 250 ms: clean through level 12.

For JEV's 3.3-second moves in a 40-line game, that emulation showed 0 divergences over 34
pieces.

The first latency test on the real stick showed 204 ms per round trip. That wasn't the USB
link: the PC's serial reader asked for 4096 bytes with a 0.2 s timeout, and so it waited out
the timeout on every short line. After reading only what had arrived, the round trip on the
stick was **1.2 ms at the median and 11 ms at p95**, with one outlier of 979 ms in 5,075
pings. Opening the port never reset the stick. Its heartbeat counter kept counting through
every connection.

## On the real stick

The firmware change is small:

- a pure, host-tested link module (the wire format and the link's state machine);
- about 70 lines of glue in `main.cpp`;
- a 3.4 KB larger image (599,840 bytes);
- no change to the engine, input or display code.

All results below are from the real stick, a 40-line game each time.

| run | result |
|---|---|
| heuristic, full speed (the acceptance test) | 40 lines in 102 pieces, 16,050 points, about 10 seconds; **0 divergences**; worst round trip per piece 372 ms against a 473 ms budget at level 4 |
| heuristic, watch pace | 40 lines in 101 pieces, 14,922 points, about 2.5 minutes, 0 divergences |
| JEV 4B | 5 lines and 1,152 points after about 34 moves, 0 divergences, before we switched it off |
| heuristic, taking over JEV's game | cleared from JEV's messy board to 40 lines in 70 more pieces, 15,166 points, 0 divergences |

Full speed was too fast to watch. So the PC grew a watch pace:

1. The new piece rests at the top for half a second.
2. It slides into its chosen spot.
3. It rests for a second.
4. It drops.

That's about 1.6 seconds per piece. That watch-pace game is the one [on video](https://drive.google.com/file/d/1ffaFAq8eJvOVujt2KnG6DF8Hn3Vkoui8/preview). The owner's verdict: *"this rhythm good for human eyes,
slow for testing highest score. THIS IS GOLD."*

## What we learned

- **Host first, with the real engine behind the real protocol, is the whole trick.** By the
  time the stick was flashed, the protocol, the driver and the brains had already played tens
  of thousands of pieces against the same code. The only real bug the device session turned
  up was on the PC side: the slow serial read.
- **Check the copy against the original, exhaustively.** A 200,000-probe conformance run is
  cheap on a laptop, and it found no mismatch between the Python model and the engine. That
  includes the engine's wrap quirk, which the model reproduces exactly.
- **A one-piece greedy heuristic is not enough, and one piece of lookahead is.**
- **Zero-shot small language models are position-biased pickers.** Below a few billion
  parameters they choose letters, not moves. Small groups help a little, size helps more, and
  none of it comes near a four-number weighted sum.
- **Slow the robot down for people.** The same brain feels broken at 10 pieces a second and
  delightful at 1.6 seconds per piece.

## Next

- **The 8B model** is the next rung for JEV.
- **Fine-tuning** stays the last resort. Distilling the heuristic's choices into the model
  would likely close most of the gap, but it would change the question from "can it play
  zero-shot" to "can it imitate."
- **Keep bot games out of the high-score table.** They currently land in it: a 10-second
  40-line game sits at the top of a table ranked by time.

## Code

This public repository is at v1.0.0. The autopilot described here (the firmware's USB
link, the host twin, the PC brains and tools) lives in the development repository and
arrives here with the next export (v1.2.0). The serial protocol it extends is in
[docs/SERIAL.md](../SERIAL.md).
