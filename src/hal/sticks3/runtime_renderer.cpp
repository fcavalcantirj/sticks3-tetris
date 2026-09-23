#include "hal/sticks3/runtime_renderer.h"

#include <cstdint>

#include "hal/sticks3/clock.h"
#include "hal/sticks3/diagnostics.h"
#include "hal/sticks3/panel.h"
#include "stackfall/app/app.h"
#include "stackfall/ui/plan.h"

namespace sf_hal {
namespace {

constexpr uint32_t kDiagRenderMs = 250;

}  // namespace

void RuntimeRenderer::pushFrame(Panel& panel, Diagnostics& diagnostics,
                                 const sf::ui::DrawPlan& plan) {
  const uint64_t startedUs = nowUs();
  panel.pushFrame(plan);
  const uint64_t elapsedUs = nowUs() - startedUs;
  diagnostics.recordFrameUs(
      elapsedUs > UINT32_MAX ? UINT32_MAX
                             : static_cast<uint32_t>(elapsedUs));
}

void RuntimeRenderer::renderPlayingModel(Panel& panel,
                                          Diagnostics& diagnostics,
                                          sf::ui::DrawPlan& plan,
                                          const sf::ui::PlayingModel& model) {
  if (!havePreviousPlaying_ || needsFullRedraw_) {
    plan.full = true;
  } else {
    plan = sf::ui::diff(previousPlaying_, model);
  }
  previousPlaying_ = model;
  havePreviousPlaying_ = true;
  needsFullRedraw_ = false;
  if (plan.full || plan.count > 0) {
    panel.drawPlaying(model, plan);
    pushFrame(panel, diagnostics, plan);
    ++frames_;
  }
}

void RuntimeRenderer::render(sf::App& app, Panel& panel,
                             Diagnostics& diagnostics,
                             sf::ui::DrawPlan& plan,
                             const sf::ui::CalibratingView& calibrating,
                             uint32_t nowMs,
                             const char* version, const char* buildId) {
  plan = sf::ui::DrawPlan{};
  const sf::Screen screen = app.screen();
  if (screen == sf::Screen::Playing) {
    sf::ui::PlayingModel next = sf::ui::buildPlaying(app.game(), nowMs);
    if (reZeroBannerActive_) {
      const uint32_t age = nowMs - reZeroBannerStartedMs_;
      if (age < sf::ui::kBannerDurationMs) {
        next.banner = "RE-ZERO";
        next.bannerMsLeft =
            static_cast<uint16_t>(sf::ui::kBannerDurationMs - age);
      } else {
        reZeroBannerActive_ = false;
      }
    }
    renderPlayingModel(panel, diagnostics, plan, next);
    return;
  }

  if (screen == sf::Screen::Calibrating &&
      (calibrating.step == sf::input::CalibrationStep::Maze ||
       calibrating.step == sf::input::CalibrationStep::Rotate ||
       calibrating.step == sf::input::CalibrationStep::TiltRight ||
       calibrating.step == sf::input::CalibrationStep::TiltLeft ||
       calibrating.step == sf::input::CalibrationStep::DipAway ||
       calibrating.step == sf::input::CalibrationStep::DipToward)) {
    const sf::ui::PracticeView practice{
        calibrating.step,
        calibrating.ballCol,
        calibrating.ballRow,
        calibrating.onCourse,
        calibrating.mazeProgress,
        calibrating.elapsedS,
        calibrating.tiltGain,
        calibrating.rotationCount,
        calibrating.rotationState,
        calibrating.boxBallCol,
        calibrating.boxBallRow,
        calibrating.boxTargetCol,
        calibrating.boxTargetRow,
        calibrating.boxReached,
        calibrating.poseTooSmall,
        calibrating.poseBestG,
        calibrating.mazeArmed,
        calibrating.windowRunning};
    renderPlayingModel(panel, diagnostics, plan,
                       sf::ui::buildCalibratePractice(practice));
    return;
  }

  havePreviousPlaying_ = false;
  if (screen == sf::Screen::Diagnostics) {
    if (!needsFullRedraw_ && nowMs - lastDiagRenderMs_ < kDiagRenderMs) {
      return;
    }
    lastDiagRenderMs_ = nowMs;
    plan.full = true;
    diagnostics.draw(panel, version, buildId);
    pushFrame(panel, diagnostics, plan);
    ++frames_;
    needsFullRedraw_ = false;
    return;
  }
  if (!needsFullRedraw_) return;

  plan.full = true;
  switch (screen) {
    case sf::Screen::Title: panel.drawTitle(sf::ui::buildTitle(app)); break;
    case sf::Screen::Paused:
      panel.drawPaused(sf::ui::buildPaused(app, nowMs));
      break;
    case sf::Screen::GameOver:
      panel.drawGameOver(sf::ui::buildGameOver(app));
      break;
    case sf::Screen::HighScores:
      panel.drawHighScores(sf::ui::buildHighScores(app));
      break;
    case sf::Screen::Settings:
      panel.drawSettings(sf::ui::buildSettings(app));
      break;
    case sf::Screen::Instructions:
      panel.drawInstructions(sf::ui::buildInstructions(sf::Screen::Playing));
      break;
    case sf::Screen::Calibrating:
      panel.drawTextPage("CALIBRATE", sf::ui::buildCalibrating(calibrating));
      break;
    case sf::Screen::Diagnostics:
    case sf::Screen::Playing:
      break;
  }
  pushFrame(panel, diagnostics, plan);
  ++frames_;
  needsFullRedraw_ = false;
}

}  // namespace sf_hal
