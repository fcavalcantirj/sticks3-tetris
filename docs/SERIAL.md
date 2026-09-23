# Stackfall serial protocol

Every line this firmware will ever print, on any build, for the rest of the project.

## Rules

115200 baud on the native USB CDC port. ASCII only. Exactly one line per event
terminated by `\n`. Every line starts at column 0 with `[TAG] ` (tag in capitals,
one space, then the body). The body is space-separated `key=value` pairs with no
spaces inside a value. No line exceeds 120 characters. Nothing in the firmware
may print to serial except through these tags — an untagged `printf` is a bug,
because every device task concludes by grepping this protocol.

IMU samples are in G, never converted. `M5.Imu.getAccel` and `getImuData().accel`
report G over a +/-8 G full scale and M5Unified applies no board-specific axis
correction for this board, so no conversion constant may appear under `src/`.
`getAccel` rate-limits the sensor read to one per 256 us and the BMI270 runs its
reset-default ODR (ACC_CONF/GYR_CONF are never written), so every filter takes
`dt` as a parameter and clamps it — none may assume a fixed sample period.

## Tags

[BOOT] fw=<version> sha=<short git sha> board=<n> psram=<bytes> heap=<bytes> w=<n> h=<n> rot=<n> sprite=<ok|fallback> bytes=<64800|0> imu=<bmi270|none> — once after every subsystem is ready; this single line is the boot-complete marker and is never repeated.
[HB] n=<counter> up=<ms> heap=<bytes> fps=<n> — every 5000 ms, forever, from the first loop; liveness proof, never conclude something did not happen until n has incremented at least twice.
[K] t=<ms> blue=<0|1> side=<0|1> ev=<press|release|hold|chord> — one line per button edge or timer event.
[G] t=<ms> ax=<f> ay=<f> az=<f> — accel sample in G, never converted.
[NEU] t=<ms> yG=<f> zG=<f> col=<n> ppY=<f> ppZ=<f> src=<still|timeout|forced|manual|drift|seed> xG=<f> dipoff=<G> — once per neutral capture; col is the captured Y value projected onto the zero-referenced board, ppY/ppZ are the captured window's raw peak-to-peak spans, src identifies the qualifying path, xG is the captured long-axis neutral, and dipoff is the tracked one-dimensional resting dip offset. src=drift is the continuous resting-dip tracker in play; src=seed is the one-shot maze-entry seed that captures the resting dip after 500 ms of stillness so the ball is not pinned off the course (it re-seeds until the maze arms and carries forward into play).
[CAL] t=<ms> step=<intro|still|maze|rotate|done> n=<0..20> ppX=<f> ppY=<f> ppZ=<f> bx=<n> by=<n> on=<0|1> prog=<n> stray=<n> reachR=<G> reachL=<G> best=<G> win=<0|1> rot=<0..2> — every 250 ms while CALIBRATING; n and the peak-to-peak spans describe the live still window, bx/by are the maze ball cell, on reports whether it is on the green course, prog is course progress, stray counts off-course transitions, reachR/reachL are the learned side ranges, best is the largest deflection held for 300 ms during the current box pose, win is whether the box pose window has started (GO until the player moves), and rot is completed away dips.
[BASIS] t=<ms> s=<x>,<y>,<z> f=<x>,<y>,<z> steer=<G> dip=<G> asym=<G> gain=<f> dipx=<f> steerx=<f> steerR=<G> steerL=<G> src=<calib|identity> — once at boot and once whenever a calibration activates a new basis; s and f are the learned orthogonal steering and dip directions, dipx and steerx are the live effective normalisation factors, and steerR/steerL are the learned comfortable right/left ranges; tracked resting-dip drift never re-prints this line; see [NEU] src=drift dipoff=.
[COL] t=<ms> sig=<f> col=<n> lo=<f> hi=<f> piece=<n> dip=<f> x=<f> y=<f> z=<f> — 4 Hz while Playing after neutral capture; lo and hi are the absolute Y-axis magnitudes at the two board-edge thresholds under the current frozen neutral; piece is the active piece's real column, -1 when no piece is known, dip is the normalised dip signal seen by the detector, and x/y/z are the last raw accelerometer sample in G.
[TRACE] t=<ms> scr=<TITLE|PLAYING|PAUSED|GAMEOVER|HIGHSCORES|SETTINGS|INSTRUCTIONS|DIAGNOSTICS|CALIBRATING> act=<NONE|LEFT|RIGHT|SOFT_DROP|SOFT_DROP_OFF|ROTATE_CW|ROTATE_CCW|HARD_DROP|HOLD|PAUSE|CONFIRM|BACK|MENU_UP|MENU_DOWN|DIAG|CALIBRATE|TILT_SPEED> src=<BTN|TILT> q=<depth>/16 drop=<n> — field list in this order and case; ACTION is the ActionKind enumerator in upper snake case, q is ActionQueue depth over capacity 16, drop is its lifetime dropped count; src is by action kind: LEFT, RIGHT and SOFT_DROP are TILT, while rotations print BTN whether from BLUE or from the dip; only producer is formatTrace in sf::InputRouter.
[EV] t=<ms> ev=<LOCK|CLEAR|QUAD|TSPIN|TSPINMINI|B2B|COMBO|PC|LEVEL|TOPOUT> n=<n> — one line per scoring-relevant engine event.
[SC] score=<n> level=<n> lines=<n> mode=<endless|lines40|time180> — score snapshot.
[PWR] t=<ms> vbat=<mV> charging=<0|1> vbus=<mV> bright=<n> vol=<n> — power snapshot.
[SEED] value=0x<8 hex digits> src=first-press — once, when the first Title-screen button press seeds the game randomizer.
[DIAG] state=<on|off> OR ax=<G> ay=<G> az=<G> tilt=<LEFT|RIGHT|NEUT|SOFT> blue=<0|1> side=<0|1> heap=<bytes> minheap=<bytes> fps=<n> sfxdrop=<n> imustale=<n> — state edges plus one bounded snapshot per second while Diagnostics is open.
[PERF] t=<ms> loop_p50=<us> loop_p95=<us> loop_max=<us> frame_p95=<us> heap_min=<bytes> — loop and frame budget percentiles.
[SCREEN] full=<0|1> cap=<x,y,w,h> hold=<x,y,w,h> n0=<x,y,w,h> n1=<x,y,w,h> n2=<x,y,w,h> — every changed Playing draw; reports the actual NEXT caption, HOLD preview and three NEXT preview rectangles used by the panel path.
[NVS] op=<load|save|default|migrate> schema=<n> bytes=<n> ok=<0|1> — persistent-store operation result.
[HASH] replay=<name> h=<8 hex digits> — host-versus-device parity hash for a named replay.
[PROBE] s=<n> <free-form key=value pairs> — spikes only, never in the shipped game. A spike's one-shot measurements MUST be printed from sfProbeReport() and never from sfProbeSetup(), because output emitted during setup is lost before the USB-CDC host attaches (precedent: the S1 s=1 sprite/dram_used line was never seen until it was repeated from sfProbeReport() on the three-heartbeat window). Continuous or event-driven spike output (button edges, IMU samples) may stay in sfProbeLoop(); only one-shot boot-time lines are at risk.
[ERR] where=<symbol> code=<n> — fault report naming the failing symbol.
