#include <M5Unified.h>
#include <esp_system.h>

#include <cassert>
#include <cstddef>
#include <cstdint>

#include "control_profile.h"
#include "hal/sticks3/audio.h"
#include "hal/sticks3/buttons.h"
#include "hal/sticks3/clock.h"
#include "hal/sticks3/diagnostics.h"
#include "hal/sticks3/imu.h"
#include "hal/sticks3/panel.h"
#include "hal/sticks3/power.h"
#include "hal/sticks3/runtime_renderer.h"
#include "hal/sticks3/store.h"
#include "hal/sticks3/sysinfo.h"
#include "stackfall/app/app.h"
#include "stackfall/core/bag.h"
#include "stackfall/input/action.h"
#include "stackfall/input/router.h"
#include "stackfall/ui/plan.h"
#include "stackfall/ui/screens.h"

#ifndef SF_VERSION
#define SF_VERSION "unknown"
#endif

#ifndef SF_BUILD_ID
#define SF_BUILD_ID "unknown"
#endif

namespace {

constexpr uint32_t kFallbackSeed = 0x9E3779B9u;
constexpr uint32_t kPollMs = 5;
constexpr uint32_t kImuMs = 10;
constexpr uint32_t kAppMs = 8;
constexpr uint32_t kRenderMs = 33;
constexpr uint32_t kAccelLogMs = 100;
constexpr uint32_t kColumnLogMs = 250;
constexpr uint32_t kDiagLogMs = 1000;
constexpr uint32_t kFpsMs = 1000;
constexpr uint32_t kPowerMs = 1000;
constexpr uint32_t kHeartbeatMs = 5000;
constexpr uint32_t kPerfMs = 60000;
constexpr uint8_t kMaxAppCatchUp = 4;

static sf_hal::Panel gPanel;
static sf_hal::Audio gAudio;
static sf_hal::Imu gImu;
static sf_hal::Store gStore;
static sf_hal::Diagnostics gDiagnostics;
static sf_hal::RuntimeRenderer gRenderer;
static sf::App gApp;
static sf::input::InputRouter gRouter;
static sf::ui::DrawPlan gPlan;
static sf::PowerState gPowerState;

static sf::input::ActionQueue gActions;
static sf::XorShift32 gRng(kFallbackSeed);
static sf_hal::AccelSample gAccel;
static sf_hal::RawButtons gButtons{false, false, 0};

static uint32_t gLastPollMs = 0;
static uint32_t gLastImuMs = 0;
static uint32_t gLastAppMs = 0;
static uint32_t gLastRenderMs = 0;
static uint32_t gLastAccelLogMs = 0;
static uint32_t gLastColumnLogMs = 0;
static uint32_t gLastDiagLogMs = 0;
static uint32_t gLastFpsMs = 0;
static uint32_t gLastPowerMs = 0;
static uint32_t gLastHeartbeatMs = 0;
static uint32_t gLastPerfMs = 0;
static uint32_t gLastInputMs = 0;
static uint32_t gLastBlueEdgeMs = 0;
static uint32_t gLastSideEdgeMs = 0;
static uint32_t gHeartbeat = 0;
static uint16_t gFps = 0;
static uint8_t gSeenEvents = 0;
static uint16_t gSeenEventDrops = 0;
static bool gImuPresent = false;
static bool gSeeded = false;
static bool gFirstRunSeedPending = false;
static bool gCalibrationStarted = false;
static bool gScoreKnown = false;
static uint32_t gLastScore = 0;
static uint32_t gLastLines = 0;
static uint8_t gLastLevel = 0;
static uint8_t gLastMode = 0;
static uint32_t gSidePressedAtMs = 0;
static uint32_t gSeenNeutralCaptures = 0;
static bool gBasisReported = false;
static sf::input::TiltBasis gReportedBasis{};

// Protocol inventory for the source-versus-document gate. This image emits
// [BOOT] [HB] [K] [G] [NEU] [COL] [TRACE] [EV] [SC] [PWR] [SEED]
// [DIAG] [CAL] [BASIS] and [PERF].
// [HASH] is a host record, [PROBE] belongs to spike images,
// and [TAG] names the grammar. The HAL emits [NVS] and [ERR].

struct LoadedState {
  sf::Settings settings{};
  sf::ScoreEntry scores[sf::kScoreEntryCount]{};
};

static_assert(static_cast<uint8_t>(sf::AppAction::Diag) ==
              static_cast<uint8_t>(sf::input::ActionKind::Diag));
static_assert(static_cast<uint8_t>(sf::AppAction::Calibrate) ==
              static_cast<uint8_t>(sf::input::ActionKind::Calibrate));
static_assert(static_cast<uint8_t>(sf::AppAction::TiltSpeed) ==
              static_cast<uint8_t>(sf::input::ActionKind::TiltSpeed));

sf::ControlProfile effectiveProfile() {
  if (!gImuPresent) return sf::ControlProfile::BUTTONS_ONLY;
  return static_cast<sf::ControlProfile>(gApp.settings().controlProfile);
}

bool startsRun(sf::Screen screen, uint8_t menuIndex,
               sf::input::ActionKind action) {
  if (action != sf::input::ActionKind::Confirm) return false;
  return (screen == sf::Screen::Title && menuIndex == 0) ||
         (screen == sf::Screen::Paused && menuIndex == 1) ||
         (screen == sf::Screen::GameOver && menuIndex == 0);
}

void seedOnFirstTitlePress(bool blueEdge, bool sideEdge) {
  if (gSeeded || gApp.screen() != sf::Screen::Title ||
      (!blueEdge && !sideEdge)) return;

  uint32_t seed = esp_random() ^ static_cast<uint32_t>(sf_hal::nowUs());
  if (seed == 0) seed = kFallbackSeed;
  gRng.reseed(seed);
  gSeeded = true;
  gFirstRunSeedPending = true;
  Serial.printf("[SEED] value=0x%08lx src=first-press\n",
                static_cast<unsigned long>(seed));
}

void prepareRunSeed() {
  if (!gSeeded) {
    gRng.reseed(kFallbackSeed);
    gSeeded = true;
    gFirstRunSeedPending = true;
  }
  const uint32_t seed =
      gFirstRunSeedPending ? gRng.state() : gRng.next();
  gFirstRunSeedPending = false;
  gApp.setNextGameSeed(seed);
  gSeenEvents = 0;
  gSeenEventDrops = 0;
  gScoreKnown = false;
}

void printButtonLine(uint32_t now, const char* eventName) {
  Serial.printf("[K] t=%lu blue=%u side=%u ev=%s\n",
                static_cast<unsigned long>(now), gButtons.blue ? 1u : 0u,
                gButtons.side ? 1u : 0u, eventName);
}

void printPhysicalEdges(const sf_hal::RawButtons& next, uint32_t now,
                        bool& blueEdge, bool& sideEdge) {
  const bool oldBlue = gButtons.blue;
  const bool oldSide = gButtons.side;
  blueEdge = next.blue && !oldBlue;
  sideEdge = next.side && !oldSide;
  gButtons = next;
  if (next.blue != oldBlue || next.side != oldSide) {
    gLastInputMs = now;
  }
  if (next.blue != oldBlue) {
    gLastBlueEdgeMs = now;
  }
  if (next.side != oldSide) {
    gLastSideEdgeMs = now;
  }

  if (blueEdge) {
    printButtonLine(now, "press");
  } else if (!next.blue && oldBlue) {
    printButtonLine(now, "release");
  }
  if (sideEdge) {
    gSidePressedAtMs = now;
    printButtonLine(now, "press");
  } else if (!next.side && oldSide) {
    printButtonLine(now, "release");
  }
}

void printGestureTimer(const sf::input::GameAction& action) {
  if (action.kind == sf::input::ActionKind::Hold) {
    printButtonLine(action.tMs, "hold");
    return;
  }
  const bool chordAction = action.kind == sf::input::ActionKind::Pause ||
                           action.kind == sf::input::ActionKind::Back ||
                           action.kind == sf::input::ActionKind::Diag;
  if (chordAction && gButtons.blue && gButtons.side) {
    printButtonLine(action.tMs, "chord");
  } else if (action.kind == sf::input::ActionKind::HardDrop &&
             gButtons.side && !gButtons.blue &&
             action.tMs != gSidePressedAtMs) {
    printButtonLine(action.tMs, "hold");
  }
}

void processActions(uint32_t now) {
  sf::input::GameAction action{};
  while (gActions.pop(action)) {
    const sf::Screen before = gApp.screen();
    const bool begins = startsRun(before, gApp.menuIndex(), action.kind);
    if (begins) prepareRunSeed();

    printGestureTimer(action);
    char trace[121]{};
    sf::input::formatTrace(trace, sizeof(trace), action, before,
                           gActions.size(), gActions.dropped());
    Serial.printf("%s\n", trace);
    gApp.onAction(static_cast<sf::AppAction>(action.kind), action.tMs);
    const sf::Screen after = gApp.screen();

    if (before != sf::Screen::Calibrating &&
        after == sf::Screen::Calibrating) {
      gRouter.beginCalibration(action.kind == sf::input::ActionKind::Calibrate ||
                               !gRouter.basis().calibrated);
      gCalibrationStarted = true;
    }

    if (before != sf::Screen::Diagnostics &&
        after == sf::Screen::Diagnostics) {
      gLastDiagLogMs = now;
      Serial.printf("[DIAG] state=on\n");
    } else if (before == sf::Screen::Diagnostics &&
               after != sf::Screen::Diagnostics) {
      Serial.printf("[DIAG] state=off\n");
    }

    const bool exitsCalibration = before == sf::Screen::Calibrating &&
                                  after == sf::Screen::Playing;
    if (begins || exitsCalibration) {
      gLastAppMs = now;
      gRenderer.resetPlaying();
    }
    if (before != gApp.screen() || gApp.screen() != sf::Screen::Playing) {
      gRenderer.invalidate();
    }
  }
}

const char* modeName(uint8_t mode) {
  constexpr const char* kNames[] = {"endless", "lines40", "time180"};
  return mode < 3 ? kNames[mode] : kNames[0];
}

void printScoreIfChanged() {
  const sf::Game& game = gApp.game();
  const uint32_t score =
      game.score() > 0 ? static_cast<uint32_t>(game.score()) : 0u;
  const uint8_t mode = static_cast<uint8_t>(game.rules().mode);
  if (gScoreKnown && score == gLastScore && game.lines() == gLastLines &&
      game.level() == gLastLevel && mode == gLastMode) return;

  gScoreKnown = true;
  gLastScore = score;
  gLastLines = game.lines();
  gLastLevel = game.level();
  gLastMode = mode;
  Serial.printf("[SC] score=%lu level=%u lines=%lu mode=%s\n",
                static_cast<unsigned long>(score),
                static_cast<unsigned>(game.level()),
                static_cast<unsigned long>(game.lines()), modeName(mode));
}

void playEventSound(const sf::GameEvent& event, uint32_t now) {
  switch (event.type) {
    case sf::EventType::Move: gAudio.play(sf::SfxId::Move, now); break;
    case sf::EventType::Rotate: gAudio.play(sf::SfxId::Rotate, now); break;
    case sf::EventType::Hold: gAudio.play(sf::SfxId::Hold, now); break;
    case sf::EventType::HoldDenied: gAudio.play(sf::SfxId::Denied, now); break;
    case sf::EventType::Lock: gAudio.play(sf::SfxId::Lock, now); break;
    case sf::EventType::HardDrop: gAudio.play(sf::SfxId::HardDrop, now); break;
    case sf::EventType::LineClear: {
      const sf::SfxId cues[] = {sf::SfxId::Single, sf::SfxId::Single,
                                sf::SfxId::Double, sf::SfxId::Triple,
                                sf::SfxId::Quad};
      gAudio.play(cues[event.a > 4 ? 4 : event.a], now);
      break;
    }
    case sf::EventType::TSpin: gAudio.play(sf::SfxId::TSpin, now); break;
    case sf::EventType::LevelUp: gAudio.play(sf::SfxId::LevelUp, now); break;
    case sf::EventType::GameOver: gAudio.play(sf::SfxId::GameOver, now); break;
    case sf::EventType::Spawn:
    case sf::EventType::Kick:
    case sf::EventType::SoftDrop:
    case sf::EventType::Combo:
    case sf::EventType::BackToBack:
    case sf::EventType::PerfectClear:
      break;
  }
}

void printEvent(const sf::GameEvent& event, uint32_t now) {
  const char* name = nullptr;
  switch (event.type) {
    case sf::EventType::Lock: name = "LOCK"; break;
    case sf::EventType::LineClear:
      name = event.a >= 4 ? "QUAD" : "CLEAR";
      break;
    case sf::EventType::TSpin:
      name = event.a == static_cast<uint8_t>(sf::SpinKind::Mini)
                 ? "TSPINMINI" : "TSPIN";
      break;
    case sf::EventType::Combo: name = "COMBO"; break;
    case sf::EventType::BackToBack: name = "B2B"; break;
    case sf::EventType::PerfectClear: name = "PC"; break;
    case sf::EventType::LevelUp: name = "LEVEL"; break;
    case sf::EventType::GameOver: name = "TOPOUT"; break;
    case sf::EventType::Spawn:
    case sf::EventType::Move:
    case sf::EventType::Rotate:
    case sf::EventType::Kick:
    case sf::EventType::SoftDrop:
    case sf::EventType::HardDrop:
    case sf::EventType::Hold:
    case sf::EventType::HoldDenied:
      return;
  }
  Serial.printf("[EV] t=%lu ev=%s n=%u\n", static_cast<unsigned long>(now),
                name, static_cast<unsigned>(event.a));
}

void processNewEvents(uint32_t now) {
  sf::EventQueue pending = gApp.game().events();
  const uint8_t size = pending.size();
  const uint16_t drops = pending.dropped();
  const bool reset = drops < gSeenEventDrops ||
                     (drops == gSeenEventDrops && size < gSeenEvents);
  uint32_t fresh = reset ? size :
      static_cast<uint32_t>(size > gSeenEvents ? size - gSeenEvents : 0u) +
          static_cast<uint32_t>(drops - gSeenEventDrops);
  if (fresh > size) fresh = size;

  sf::GameEvent event{};
  for (uint32_t skip = size - fresh; skip > 0; --skip) pending.pop(event);
  while (pending.pop(event)) {
    playEventSound(event, now);
    printEvent(event, now);
  }
  gSeenEvents = size;
  gSeenEventDrops = drops;
}

void updatePower(uint32_t now) {
  sf::PowerIn in = sf_hal::readPower(now);
  in.lastInputMs = gLastInputMs;
  in.brightnessSetting = gApp.settings().brightness;
  in.volumeSetting = gApp.settings().volume;
  in.playing = gApp.screen() == sf::Screen::Playing;
  const sf::PowerOut out = sf::powerStep(gPowerState, in);

  gPanel.setBrightness(out.brightness);
  gAudio.setVolumeCap(out.volumeCap);
  gPanel.setBattery(in.batteryPct, in.charging, out.lowBattery);
  Serial.printf(
      "[PWR] t=%lu vbat=%u charging=%u vbus=%u bright=%u vol=%u\n",
      static_cast<unsigned long>(now), static_cast<unsigned>(in.vbatMv),
      in.charging ? 1u : 0u, static_cast<unsigned>(in.vbusMv),
      static_cast<unsigned>(out.brightness),
      static_cast<unsigned>(out.volume));
}

void refreshDiagnostics(uint32_t now) {
  gDiagnostics.refresh(gRouter, gButtons, now, gLastBlueEdgeMs,
                       gLastSideEdgeMs, gFps, gAudio.dropped(),
                       gImu.stats().stale);
}

void reportNeutralCapture() {
  if (gSeenNeutralCaptures == gRouter.neutralCaptureCount()) return;
  gSeenNeutralCaptures = gRouter.neutralCaptureCount();
  Serial.printf(
      "[NEU] t=%lu yG=%.3f zG=%.3f col=%d ppY=%.4f ppZ=%.4f src=%s "
      "xG=%.3f dipoff=%.3f\n",
      static_cast<unsigned long>(gRouter.neutralCapturedAtMs()),
      gRouter.neutralYG(), gRouter.neutralZG(), gRouter.neutralColumn(),
      gRouter.neutralWindowPpYG(), gRouter.neutralWindowPpZG(),
      sf::input::neutralCaptureSourceName(gRouter.lastCaptureSource()),
      gRouter.neutralXG(), gRouter.dipOffsetG());
  if (gRouter.lastCaptureWasManual()) {
    gRenderer.showReZeroBanner(gRouter.neutralCapturedAtMs());
  }
}

// Tracked resting-pitch drift is reported by [NEU], not as a new basis.
bool sameBasis(const sf::input::TiltBasis& left,
               const sf::input::TiltBasis& right) {
  return left.s.x == right.s.x &&
         left.s.y == right.s.y && left.s.z == right.s.z &&
         left.f.x == right.f.x && left.f.y == right.f.y &&
         left.f.z == right.f.z &&
         left.steerScaleG == right.steerScaleG &&
         left.steerScaleRightG == right.steerScaleRightG &&
         left.steerScaleLeftG == right.steerScaleLeftG &&
         left.dipScaleG == right.dipScaleG &&
         left.steerAsymmetryG == right.steerAsymmetryG &&
         left.calibrated == right.calibrated;
}

void reportBasis(uint32_t now) {
  const sf::input::TiltBasis& basis = gRouter.basis();
  if (gBasisReported && sameBasis(basis, gReportedBasis)) return;
  gReportedBasis = basis;
  gBasisReported = true;
  Serial.printf(
      "[BASIS] t=%lu s=%.3f,%.3f,%.3f f=%.3f,%.3f,%.3f "
      "steer=%.3f dip=%0.3f asym=%.3f gain=%.2f dipx=%.2f steerx=%.2f "
      "steerR=%.3f steerL=%.3f "
      "src=%s\n",
      static_cast<unsigned long>(now), basis.s.x, basis.s.y, basis.s.z,
      basis.f.x, basis.f.y, basis.f.z, basis.steerScaleG, basis.dipScaleG,
      basis.steerAsymmetryG, gRouter.tiltGain(),
      sf::input::effectiveDipFactor(basis),
      sf::input::effectiveSteerFactor(basis),
      basis.steerScaleRightG, basis.steerScaleLeftG,
      basis.calibrated ? "calib" : "identity");
}

void reportColumn(uint32_t now) {
  if (gApp.screen() == sf::Screen::Calibrating) {
    if (now - gLastColumnLogMs < kColumnLogMs) return;
    gLastColumnLogMs = now;
    const char* step = "idle";
    switch (gRouter.calibrationStep()) {
      case sf::input::CalibrationStep::Intro: step = "intro"; break;
      case sf::input::CalibrationStep::Still: step = "still"; break;
      case sf::input::CalibrationStep::TiltRight: step = "box_r"; break;
      case sf::input::CalibrationStep::TiltLeft: step = "box_l"; break;
      case sf::input::CalibrationStep::DipAway: step = "box_up"; break;
      case sf::input::CalibrationStep::DipToward: step = "box_down"; break;
      case sf::input::CalibrationStep::Maze: step = "maze"; break;
      case sf::input::CalibrationStep::Rotate: step = "rotate"; break;
      case sf::input::CalibrationStep::Done: step = "done"; break;
      case sf::input::CalibrationStep::Idle: break;
    }
    Serial.printf(
        "[CAL] t=%lu step=%s n=%u ppX=%.4f ppY=%.4f ppZ=%.4f "
        "bx=%d by=%d on=%u prog=%u stray=%u reachR=%.3f "
        "reachL=%.3f defl=%.3f best=%.3f win=%u rot=%u\n",
        static_cast<unsigned long>(now), step,
        static_cast<unsigned>(gRouter.stillCount()), gRouter.stillPpX(),
        gRouter.stillPpY(), gRouter.stillPpZ(),
        gRouter.mazeBallCol(), gRouter.mazeBallRow(),
        gRouter.mazeOnCourse() ? 1u : 0u,
        static_cast<unsigned>(gRouter.mazeProgress()),
        static_cast<unsigned>(gRouter.mazeStrays()),
        gRouter.mazeReachRightG(), gRouter.mazeReachLeftG(),
        gRouter.poseDeflectionG(), gRouter.poseBestG(),
        gRouter.poseWindowRunning() ? 1u : 0u,
        static_cast<unsigned>(gRouter.rotationCount()));
    if (gRouter.calibrationStep() != sf::input::CalibrationStep::Maze &&
        gRouter.calibrationStep() != sf::input::CalibrationStep::Rotate) {
      gRenderer.invalidate();
    }
    return;
  }
  if (gApp.screen() != sf::Screen::Playing || !gRouter.neutralReady() ||
      now - gLastColumnLogMs < kColumnLogMs) {
    return;
  }
  gLastColumnLogMs = now;
  Serial.printf(
      "[COL] t=%lu sig=%.3f col=%d lo=%.3f hi=%.3f piece=%d dip=%.3f "
      "x=%.3f y=%.3f z=%.3f\n",
      static_cast<unsigned long>(now), gRouter.tiltSignalG(),
      gRouter.selectedColumn(), gRouter.lowEdgeMagnitudeG(),
      gRouter.highEdgeMagnitudeG(), gRouter.pieceColumn(),
      gRouter.dipSignalG(), gRouter.rawXG(), gRouter.rawYG(),
      gRouter.rawZG());
}

void render(uint32_t now) {
  if (gApp.screen() == sf::Screen::Diagnostics) {
    refreshDiagnostics(now);
  }
  const sf::input::CalibrationStep calibrationStep =
      gRouter.calibrationStep();
  const uint32_t mazeElapsedMs = gRouter.mazeElapsedMs(now);
  const uint32_t elapsedS = mazeElapsedMs / 1000u;
  const uint16_t visibleElapsedS =
      elapsedS > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(elapsedS);
  const uint8_t practiceRotationCount =
      calibrationStep == sf::input::CalibrationStep::Maze
          ? static_cast<uint8_t>(visibleElapsedS > UINT8_MAX ? UINT8_MAX
                                                               : visibleElapsedS)
          : gRouter.rotationCount();
  const uint8_t practiceRotationState =
      calibrationStep == sf::input::CalibrationStep::Maze
          ? static_cast<uint8_t>((gRouter.mazeOnCourse() ? 0x80u : 0u) |
                                 (gApp.settings().tiltSensitivity < 5
                                      ? gApp.settings().tiltSensitivity
                                      : 4u))
          : gRouter.calibrationRotationState();
  // Fields 1-31 (through mazeArmed) are set positionally; everything below is
  // assigned BY NAME on purpose. The maze/factor tail of CalibratingView used to
  // be positional and had silently drifted out of order (mazeBallCol/Row and
  // mazeProgress landed in the steer-factor floats, onCourse in windowRunning,
  // and mazeOnCourse was never set) -- which broke the maze render: the ball drew
  // red, bounced left/right, and never reached the course. Name assignment keeps
  // this correct even if the struct gains a field.
  sf::ui::CalibratingView calibrating{
       calibrationStep, gRouter.stillCount(), gRouter.restless(),
       gRouter.stillPpY(), gRouter.stillPpZ(), gRouter.poseDeflectionG(),
       gRouter.poseTooSmall(), gRouter.poseRemainingS(now),
       static_cast<int8_t>(gRouter.boxBallCol()),
       static_cast<int8_t>(gRouter.boxBallRow()),
       static_cast<int8_t>(gRouter.boxTargetCol()),
       static_cast<int8_t>(gRouter.boxTargetRow()),
       gRouter.boxReached(), gRouter.poseBestG(), gRouter.poseRetry(),
       gRouter.rightDefaulted(), gRouter.leftDefaulted(),
       gRouter.dipDefaulted(), gRouter.basis().steerScaleG,
       gRouter.basis().dipScaleG, gRouter.basis().steerAsymmetryG,
       gRouter.tiltGain(), gRouter.mazeReachRightG(),
       gRouter.mazeReachLeftG(), gRouter.mazeStrays(),
      visibleElapsedS,
      gRouter.mazeSolved(),
      static_cast<uint16_t>(gRouter.mazeOffCourseMs(now) / 1000u),
      practiceRotationCount, practiceRotationState, gRouter.mazeArmed()};
  calibrating.windowRunning = gRouter.poseWindowRunning();
  calibrating.dipFactor = sf::input::effectiveDipFactor(gRouter.basis());
  calibrating.steerFactor = sf::input::effectiveSteerFactor(gRouter.basis());
  calibrating.steerFactorRight =
      sf::input::effectiveSteerFactorRight(gRouter.basis());
  calibrating.steerFactorLeft =
      sf::input::effectiveSteerFactorLeft(gRouter.basis());
  calibrating.ballCol = static_cast<int8_t>(gRouter.mazeBallCol());
  calibrating.ballRow = static_cast<int8_t>(gRouter.mazeBallRow());
  calibrating.onCourse = gRouter.mazeOnCourse();
  calibrating.mazeProgress = gRouter.mazeProgress();
  gRenderer.render(gApp, gPanel, gDiagnostics, gPlan, calibrating, now,
                   SF_VERSION, SF_BUILD_ID);
}

void flushIfRequested(uint32_t now) {
  if (!gApp.wantsFlush(now)) return;
  gStore.save(gApp.settings());
  gStore.save(gApp.scores());
  gApp.markFlushed(now);
}

}  // namespace

void setup() {
  auto cfg = M5.config();
  cfg.internal_imu = true;
  cfg.internal_spk = true;
  M5.begin(cfg);
  M5.Power.setBatteryCharge(true);

  Serial.begin(115200);
  for (uint32_t waited = 0; !Serial && waited < 1500; waited += 50) {
    delay(50);
  }

  auto loaded = LoadedState{};
  gStore.begin();
  gStore.load(loaded.settings);
  gStore.load(loaded.scores);
  const bool panelReady = gPanel.begin();
  gAudio.begin();
  gImuPresent = gImu.begin();
  if (!gImuPresent) {
    loaded.settings.controlProfile =
        static_cast<uint8_t>(sf::ControlProfile::BUTTONS_ONLY);
  }

  const uint32_t startedMs = sf_hal::nowMs();
  gApp.begin(loaded.settings, loaded.scores, startedMs);
  gRouter.setProfile(effectiveProfile());
  gRouter.setRotateCcw(!gApp.settings().rotateCw);
  gRouter.setTiltSpeed(gApp.settings().tiltSensitivity);
  gLastPollMs = startedMs;
  gLastImuMs = startedMs;
  gLastAppMs = startedMs;
  gLastRenderMs = startedMs;
  gLastAccelLogMs = startedMs;
  gLastColumnLogMs = startedMs;
  gLastDiagLogMs = startedMs;
  gLastFpsMs = startedMs;
  gLastPowerMs = startedMs;
  gLastHeartbeatMs = startedMs;
  gLastPerfMs = startedMs;
  gLastInputMs = startedMs;
  gLastBlueEdgeMs = startedMs;
  gLastSideEdgeMs = startedMs;

  const int board = static_cast<int>(M5.getBoard());
  const uint32_t psram = sf_hal::psramBytes();
  assert(panelReady);
  assert(board == 26);
  assert(psram > 8000000u);
  Serial.printf(
      "[BOOT] fw=%s sha=%s board=%d psram=%lu heap=%lu w=%d h=%d rot=0 "
      "sprite=%s bytes=%lu imu=%s\n",
      SF_VERSION, SF_BUILD_ID, board, static_cast<unsigned long>(psram),
      static_cast<unsigned long>(sf_hal::freeHeap()), gPanel.width(),
      gPanel.height(), gPanel.spriteOk() ? "ok" : "fallback",
      static_cast<unsigned long>(gPanel.spriteBytes()),
      gImuPresent ? "bmi270" : "none");
  reportBasis(startedMs);
}

void loop() {
  const uint64_t loopStartedUs = sf_hal::nowUs();
  const uint32_t now = sf_hal::nowMs();
  bool pollInput = false;
  if (now - gLastPollMs >= kPollMs) {
    gLastPollMs = now;
    sf_hal::updateDevice();
    pollInput = true;
  }

  if (now - gLastImuMs >= kImuMs) {
    gLastImuMs = now;
    gAccel = gImu.poll(now);
    if (gAccel.fresh) {
      gDiagnostics.setAccel(gAccel.ax, gAccel.ay, gAccel.az);
    }
  }

  if (pollInput) {
    bool blueEdge = false;
    bool sideEdge = false;
    printPhysicalEdges(sf_hal::sampleButtons(now), now, blueEdge, sideEdge);
    seedOnFirstTitlePress(blueEdge, sideEdge);
    gRouter.setProfile(effectiveProfile());
    gRouter.setRotateCcw(!gApp.settings().rotateCw);
    gRouter.setTiltSpeed(gApp.settings().tiltSensitivity);
    gRouter.setActivePiece(
        gApp.game().active(),
        gApp.screen() == sf::Screen::Playing &&
            gApp.game().status() == sf::GameStatus::Playing);
    gRouter.update(gButtons.blue, gButtons.side, gAccel.ax, gAccel.ay,
                   gAccel.az, gAccel.fresh, gApp.screen(), now, gActions);
    gAccel.fresh = false;
    reportNeutralCapture();
    reportBasis(now);
    processActions(now);
  }

  uint8_t catchUp = 0;
  while (now - gLastAppMs >= kAppMs && catchUp < kMaxAppCatchUp) {
    gLastAppMs += kAppMs;
    const sf::Screen before = gApp.screen();
    gApp.step(gLastAppMs);
    if (before != gApp.screen()) gRenderer.invalidate();
    ++catchUp;
  }

  processNewEvents(now);
  if (gApp.screen() == sf::Screen::Playing || gScoreKnown) {
    printScoreIfChanged();
  }
  gAudio.update(now);
  flushIfRequested(now);

  if (gApp.screen() == sf::Screen::Diagnostics &&
      now - gLastAccelLogMs >= kAccelLogMs) {
    gLastAccelLogMs = now;
    Serial.printf("[G] t=%lu ax=%.2f ay=%.2f az=%.2f\n",
                  static_cast<unsigned long>(now), gAccel.ax, gAccel.ay,
                  gAccel.az);
  }

  if (gApp.screen() == sf::Screen::Diagnostics &&
      now - gLastDiagLogMs >= kDiagLogMs) {
    gLastDiagLogMs = now;
    refreshDiagnostics(now);
    char line[120]{};
    gDiagnostics.formatLine(line, sizeof(line));
    Serial.printf("%s\n", line);
  }

  reportColumn(now);

  if (now - gLastRenderMs >= kRenderMs) {
    gLastRenderMs = now;
    render(now);
  }

  if (now - gLastFpsMs >= kFpsMs) {
    const uint32_t elapsed = now - gLastFpsMs;
    const uint32_t frames = gRenderer.takeFrames();
    const uint32_t measured = elapsed == 0 ? 0 : frames * 1000u / elapsed;
    gFps = measured > UINT16_MAX ? UINT16_MAX
                                : static_cast<uint16_t>(measured);
    gLastFpsMs = now;
  }

  if (now - gLastPowerMs >= kPowerMs) {
    gLastPowerMs = now;
    updatePower(now);
  }

  if (now - gLastPerfMs >= kPerfMs) {
    gLastPerfMs = now;
    Serial.printf(
        "[PERF] t=%lu loop_p50=%lu loop_p95=%lu loop_max=%lu "
        "frame_p95=%lu heap_min=%lu\n",
        static_cast<unsigned long>(now),
        static_cast<unsigned long>(gDiagnostics.loopPercentile(50)),
        static_cast<unsigned long>(gDiagnostics.loopPercentile(95)),
        static_cast<unsigned long>(gDiagnostics.loopPercentile(100)),
        static_cast<unsigned long>(gDiagnostics.framePercentile(95)),
        static_cast<unsigned long>(sf_hal::minFreeHeap()));
  }

  if (now - gLastHeartbeatMs >= kHeartbeatMs) {
    gLastHeartbeatMs = now;
    ++gHeartbeat;
    Serial.printf("[HB] n=%lu up=%lu heap=%lu fps=%u\n",
                  static_cast<unsigned long>(gHeartbeat),
                  static_cast<unsigned long>(now),
                  static_cast<unsigned long>(sf_hal::freeHeap()),
                  static_cast<unsigned>(gFps));
  }

  const uint64_t loopElapsedUs = sf_hal::nowUs() - loopStartedUs;
  gDiagnostics.recordLoopUs(
      loopElapsedUs > UINT32_MAX ? UINT32_MAX
                                : static_cast<uint32_t>(loopElapsedUs));
}
