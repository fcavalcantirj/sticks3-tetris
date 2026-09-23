## Locked control map

DECIDED BY THE OWNER 2026-09-08 after hands-on testing. This map is FINAL; the tilt spike is closed (`docs/TILT-LOG.md` ends `SPIKE: CLOSED`). Do not propose another control experiment.

The player holds the stick UPRIGHT IN ONE HAND, screen facing their face, IR end up, USB-C end down with the cable hanging. Every binding below preserves that: the screen never leaves the player's view.

**Tilt LEFT / RIGHT -> move the piece left / right.** A roll about the screen normal (the Z axis, the one pointing at the player's face). The screen stays square to the face throughout, which is why this gesture works and why it gets the primary control. Mapping is ABSOLUTE, not incremental: the tilt ANGLE selects the target column directly, so moving eight columns costs the same single gesture as moving one.
The selected column is clamped to the columns the current piece can occupy against the walls, and a piece stopped by the stack is steered from where it actually is, so levelling the stick never over-travels.
The steering direction and range are learned in the CALIBRATE TILT MAZE.

**DIP THE FRONT DOWN, IR end tipping AWAY from the face -> ROTATE CW.** Edge-triggered: exactly ONE rotation per dip, and the gesture must return near neutral before it can fire again. Holding the dip does not spin the piece repeatedly.

**DIP TOWARD YOURSELF, IR end tipping TOWARD the face -> SOFT DROP,** non-locking, releasing at neutral. Level-triggered after 120 ms of continuous toward-dip, then active for as long as the dip is held.

Both dips are the SAME axis in OPPOSITE directions, so they can never be confused with each other.

**WHILE THE DIP AXIS IS ENGAGED IN EITHER DIRECTION, LEFT/RIGHT IS FROZEN.** The player steers, then rotates or drops. This single rule removes the cross-axis coupling the S3b drill measured as `unmove=90`: a hand cannot pitch without also rolling slightly, so nothing listens to roll while pitched. Steering during a drop is not needed — the piece is already where the player put it.

BLUE FRONT key = KEY1 / GPIO11 / `M5.BtnA`; press -> Rotate CW, applied IMMEDIATELY on the press edge; hold >= 900 ms -> Hold / swap, and the rotation the press already fired is ABSORBED and harmless because the swapped-in piece respawns in its spawn orientation. SIDE key = KEY2 / GPIO12 / `M5.BtnB`; press -> HARD DROP, applied IMMEDIATELY on the press edge, but ONLY while the blue key is UP: a side press edge that arrives while blue is already down is the PAUSE MODIFIER, emits NOTHING and arms the chord instead, because a rotation is absorbed by the swap and a hard drop never can be. In TILT_DIP, pressing BLUE four times within 600 ms opens the CALIBRATE screen; the four rotations it fires are kept by rule (a) and cancel out for every piece. Holding SIDE alone for >= 700 ms remains a secondary re-zero, but its press edge hard-drops the piece first and it waits while a dip is engaged. PAUSE / menu = press and hold BLUE, then press SIDE, and keep both down >= 800 ms; that whole gesture emits RotateCw (absorbed) and then Pause, never a HardDrop, Hold or re-zero. Rotation direction is a SETTING that flips what blue and the away-dip do in play; menu bindings are never affected; CCW in play is CW three times. The small side Reset/Power key is NEVER read or bound.

### Why no other orientation gesture exists

Holding the stick upright, the ONLY rotation that keeps the screen square to the player's face is roll about the screen normal — and that is spent on left/right. Any second orientation control must move the screen off-axis; the question is only how far. A dip ANGLES the screen and stays readable. Twisting the stick about its long axis TURNS THE SCREEN ASIDE, away from the player's eyes, and is therefore unusable no matter how cleanly a sensor detects it. That is a geometric fact about a hand-held screen, not a tuning problem, and it is why the gyro twist channel was abandoned. Do not re-propose it.

### The dip is read on Z, never on X

`include/imu_axes.h` records `kTiltAxisSoftDrop = 0`, the X / long axis. THAT AXIS IS WRONG FOR THIS GESTURE and must not be used by the production dip detector. Dipping by an angle changes `ax` as `cos(angle)`, an EVEN function: `cos(+45) == cos(-45)`, so X cannot tell a forward dip from a backward one — the exact distinction this map depends on — and it is flat near zero, so it is mush at small angles. This is why `SOFT_DROP_ON` fired continuously through every S3b drill.

The dip is read on **Z, the face normal**, where the signal goes as `-sin(angle)`: signed, and linear straight through zero.
Z is the dip axis for the upright grip; a calibrated basis uses the learned direction instead.

### Measured constants

Steering: `kGPerColumn = 0.09` G per column; the neutral is centred on board position 4.5, NOT on column 4, because ten columns have no integer centre and centring on 4 makes the right edge need 29% more rotation than the left (the owner detected this by hand and estimated 30%). Column selection is `clamp(floor(sig / kGPerColumn + 5.0), 0, 9)` with a `kColHysteresis = 0.02` G Schmitt band on the quantiser boundary. Both board edges sit at +/-0.36 G = +/-21.1 degrees.

On entering a game, the CALIBRATE screen captures the neutral from a 20-sample still window (0.040 G peak-to-peak) or from an explicit BLUE press on a resting sample; the 1200/3000 ms floors remain only for Diagnostics and as a backstop. That first quiet capture is then frozen for the run; only the explicit SIDE-held re-zero below may replace it.

Dip: `kDipEngageG = 0.25` (14.5 degrees) with release at `kDipReleaseG = 0.15`. Toward soft drop requires `kDipSoftDropDwellMs = 120` ms continuously past engage; away rotation remains instant. After either direction releases, the signal must remain inside the release band for `kDipNeutralDwellMs = 90` ms before re-arming. Margins, all computed from the measured geometry: a natural dip of about 45 degrees puts 0.707 G on the Z axis, 2.8x above the engage threshold; steering all the way to a board edge bleeds only `1 - cos(21.1 deg)` = 0.067 G into the same plane, 3.7x below it. Steering cannot fake a dip and a dip cannot be missed.

There is NO DAS and NO ARR on any tilt path. Incremental tilt stepping was retired when the S3b absolute-aim drill returned `ratio = 0.76` — far placements were FASTER than near ones, so distance no longer costs time. The button DAS/ARR values of 167 / 33 ms apply ONLY to the `BUTTONS_ONLY` profile's side-key repeat.

### Calibration

The full CALIBRATE tutorial is HOLD STILL, four BOX pages, MAZE, ROTATE, and DONE. The box pages teach right tilt, left tilt, top away, and top toward, with a 5 s capture window for each that starts on the first motion (the page shows GO until you move, with an 8 s ceiling) and a retry under 0.20 G. In the MAZE, follow the green course from A in the centre to B: the one-cell ball is the live tilt, its column is exactly the play column, dip away moves it up, and dip toward moves it down. The ball rests on the start row with about 2.6 degrees of pitch slack. Leaving the course turns the ball red and counts one stray; progress is earned only along the course. SIDE cycles TILT SPEED during the tutorial. The maze has a 90 s limit that always counts from step entry whether or not the ball has armed on the start cell, gives up after 12 s without progress, and BLUE skips it. ROTATE ends after two away dips, 12 s, or BLUE. The game never waits on a step. Steering and dip directions are learned from the player's own motion because a tilt in a hand-held grip also pitches the stick; the MAZE practices those axes without learning from maze pushes. The dip band engages at 60% of the comfortable dip, with a 0.12 G floor and a 0.25 G cap, and releases at 60% of engage. While PLAYING, the resting dip is corrected continuously while you are not gesturing, with a 2 s time constant, so the rotate and soft-drop thresholds keep up with a wandering resting pitch instead of lagging behind it; the offset still never moves while a dip is engaged (past the 0.25 G band). The MAZE does not track continuously, because there a held dip is the navigation and not a rest; instead it seeds the resting dip once from 500 ms of stillness on entry ([NEU] src=seed), re-seeds until the ball arms if you settle into a new pose, and that seed carries forward into play. The pre-arm maze banner reads HOLD STILL: moving prevents the seed, so holding still is always the right instruction until the ball has centred and armed. The tracker runs whenever no BLUE tap sequence is in progress: one press arms a 600 ms window that suspends drift tracking, and an idle tracker only resumes once that window has elapsed. The resting dip is tracked as a one-dimensional offset outside gestures (no button down, no soft drop, signal under engage, 500 ms after any dip action); the steering neutral never moves by itself. Each side has its own comfortable steering scale, so a full comfortable tilt on either side reaches that side's wall. Steering amplification is capped at 2x. TILT SPEED multiplies steering like pointer speed, from SLOW 0.5x through FAST 1.5x. BLUE x4 repeats the full tutorial. A box page counts only a tilt in its own direction: the right page scores the positive right projection, the left page scores the negative of the right direction, so the residual tilt from the previous page never wins. The maze ball arms only after it has sat on the start cell (4,10)/(5,10) for 500 ms, so a hand still holding the last box pose produces no stray, no progress and no give-up time until the player has returned to the middle.

On-screen guidance: the Intro page lists the four moves (TILT = MOVE, TOP AWAY = ROTATE, TOP TOWARD = DROP, SIDE = SPEED) before the binding table, then BLUE = BEGIN; the four pose steps render as BOX pages — a 2x2 green box appears at the right edge (right tilt), then the left edge, then the top, then the bottom, and the one instruction on screen is BALL INTO THE BOX — just get the ball into the box — with MORE, TRY AGAIN while the gesture is too small and GOT IT for 500 ms after the ball enters the box; the live ball follows the stick in a grip-independent tangent-plane view of the raw tilt so any motion shows on screen; during the MAZE a banner reads FOLLOW THE GREEN PATH, turns to RED = OFF THE PATH when the ball leaves the course, and to DONE on reaching B; during ROTATE a banner reads TOP AWAY = ROTATE until the first two-away dip, then ONCE MORE, with the button route shown as BLUE; the DONE summary repeats TOP AWAY = ROTATE and TOP TOWARD = DROP above BLUE = PLAY.

## Button grammar

The grammar is HAND-ROLLED: a pure state machine fed `(bool bluePressed, bool sidePressed, uint32_t nowMs)` and polled over `isPressed()` only. It may NOT use `setHoldThresh()`, `wasHold()`, `wasSingleClicked()` or `wasDoubleClicked()` — a sibling project shipped `setHoldThresh(600)` and its hold could never fire because M5Unified's click detector consumes the event first, and `tools/gates.sh` fails the build on those symbols.

Thresholds the machine emits on: debounce 8 ms; press edge fires its action with zero deliberate delay; blue hold at 900 ms of continuous press; chord at 800 ms with BOTH keys down; release edge always emitted.

Diagnostics uses the same ordered BLUE-first chord only on the Title screen, but the router requires 2000 ms of continuous both-key hold and emits it once per gesture. The ordinary 800 ms chord has no Title-screen action. Release both keys after entry; one fresh BLUE press returns to Title.

Three precedence rules, settled here because later input tasks implement them:

(a) A press-edge action is NEVER retracted — the rotation a blue hold follows has already been applied and is ABSORBED harmlessly, because the swapped-in piece respawns in its spawn orientation — and the release edge emits no further game action, so one physical gesture yields at most its press-edge action plus its hold or chord action and never a duplicate of either.

(b) While BOTH keys are down the per-key hold timers are SUSPENDED and the chord owns the gesture, so holding both keys for 1.0 s emits exactly one chord and zero holds, and a blue hold fires only if the other key was up at its 900 ms mark.

(c) The PAUSE MODIFIER: a SIDE press edge that arrives while blue is already DOWN emits nothing at all and arms the chord instead, so no destructive action is ever part of the pause gesture — a rotation is absorbed by the swap, a hard drop is irreversible, and a pause that first drops the player's piece is unusable. The pause gesture is therefore ORDERED, and is taught that way in this document, on the Instructions screen and in the walkthrough: press and hold BLUE, then press SIDE, keep both down for 800 ms, which emits exactly RotateCw (absorbed) then Pause. Release both before 800 ms and nothing further is emitted at all — an aborted pause. The asymmetry is stated and not hidden: press SIDE FIRST and its HARD DROP has already fired on its own press edge, rule (a) never retracts it, and adding blue afterwards still pauses on a piece that is already dropped — which is exactly why the taught order is blue first.

## Control profiles

TWO profiles, not three. `TILT_FULL` and `TILT_HORIZONTAL` are RETIRED — they were rungs of a degradation ladder built when it was still unknown whether tilt worked at all. It does, and the map above is the answer, so the ladder is gone. Both profiles are implemented and host-tested unconditionally.

### TILT_DIP — the DEFAULT

The map above. Setting `imuControls` selects it and it is ON by default. Tilt Y moves left/right by absolute column; the away-dip rotates; the toward-dip soft-drops; left/right never freezes: steering and the dips are independent learned axes; blue press also rotates, blue hold swaps, side press hard-drops while blue is up, side hold for 700 ms manually re-zeroes once when no dip is engaged, and the blue-first chord pauses.

Blue rotating AND the away-dip rotating is deliberate redundancy, not an accident: a player can always rotate with a thumb, the dip is the hands-free path, and the two emit the identical `ROTATE_CW` action so nothing downstream has to know which produced it.

### BUTTONS_ONLY

Selected when the player turns `imuControls` OFF in Settings. The IMU is then not read for gameplay at all. Blue press = rotate CW; side press = move RIGHT one column WITH WRAP via `RuleProfile.horizontalWrap`; side held >= 167 ms auto-repeats right every 33 ms, wrapping; side held >= 900 ms = hard drop while KEEPING the repeats already fired; blue held >= 900 ms = hold; both held >= 800 ms = pause; there is NO soft drop in this profile.

`horizontalWrap` is implemented regardless. Rule (c) applies to BOTH profiles: in each, a side press edge arriving while blue is down emits nothing, and the side key's own timers stay suspended for as long as both keys are down, so BUTTONS_ONLY's 900 ms side hard drop cannot fire inside a chord and its right-repeat does not run there either.

## Bindings table

Machine-readable binding list the on-device Instructions screen is generated from. One row per binding as `profile | control | gesture | action | threshold_ms`, pipe-delimited, no trailing spaces, one row per line, so the generator can split on `|` without a parser. The chord's control column is spelled `BLUE 1ST+SIDE` in every row, byte-identical to the `gestureLabel` the input code puts in `bindings.h`, so the blue-first order reaches the on-device Instructions screen. The chord gesture appears as gesture `hold` on control `BLUE 1ST+SIDE`; the two dips appear as gesture `dip` on controls `DIP_AWAY` and `DIP_TOWARD`; every other gesture named in this document (press, release, hold, tilt, repeat) appears as its own row, so the document and the screen never drift apart.

TILT_DIP | BLUE | press | ROTATE_CW | 0
TILT_DIP | BLUE | hold | HOLD | 900
TILT_DIP | BLUE | release | NONE | 0
TILT_DIP | SIDE | press | HARD_DROP | 0
TILT_DIP | SIDE | press (tutorial) | TILT SPEED | -
TILT_DIP | SIDE | hold | RE-ZERO | 700
TILT_DIP | BLUE | tap x4 | RE-CALIBRATE | 600
TILT_DIP | SIDE | release | NONE | 0
TILT_DIP | BLUE 1ST+SIDE | hold | PAUSE | 800
TILT_DIP | TILT_Y | tilt | MOVE_LR | 0
TILT_DIP | DIP_AWAY | dip | ROTATE_CW | 0
TILT_DIP | DIP_TOWARD | dip | SOFT_DROP | 120
BUTTONS_ONLY | BLUE | press | ROTATE_CW | 0
BUTTONS_ONLY | BLUE | hold | HOLD | 900
BUTTONS_ONLY | BLUE | release | NONE | 0
BUTTONS_ONLY | SIDE | press | MOVE_RIGHT_WRAP | 0
BUTTONS_ONLY | SIDE | repeat | MOVE_RIGHT_WRAP | 33
BUTTONS_ONLY | SIDE | hold | HARD_DROP | 900
BUTTONS_ONLY | SIDE | release | NONE | 0
BUTTONS_ONLY | BLUE 1ST+SIDE | hold | PAUSE | 800
