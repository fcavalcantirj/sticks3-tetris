// SPIKE S5 — charging enable, audio brownout on battery, and NVS persistence.
// Active only when -DSF_PROBE=6. Owned by probe_main.cpp's setup()/loop().
#if SF_PROBE == 6

#include <M5Unified.h>
#include <nvs_flash.h>
#include <nvs.h>

#include "hal/sticks3/audio.h"
#include "hal/sticks3/clock.h"
#include "hal/sticks3/panel.h"
#include "hal/sticks3/power.h"

namespace {

static constexpr uint32_t kPowerPeriodMs = 60000;      // one [PWR] sample per minute
static constexpr uint32_t kAudioPeriodMs = 2000;       // one cue every 2 s
static constexpr uint32_t kTestDurationMs = 10 * 60 * 1000; // 10 minutes
static constexpr uint32_t kSchemaVersion = 1;
static constexpr uint32_t kTestHiScore = 123456;

static uint32_t s_powerNextMs = 0;
static uint32_t s_audioNextMs = 0;
static uint32_t s_testEndMs = 0;
static uint32_t s_powerCount = 0;
static uint32_t s_audioCount = 0;
static bool s_audioRunning = true;
static sf_hal::Audio s_audio;

static bool saveHiScore()
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("stackfall", NVS_READWRITE, &h);
    if (err != ESP_OK) {
        Serial.printf("[NVS] op=save schema=%u bytes=4 ok=0 err=%d\n",
                      (unsigned)kSchemaVersion, (int)err);
        return false;
    }
    err = nvs_set_u32(h, "hiscore", kTestHiScore);
    esp_err_t commitErr = nvs_commit(h);
    nvs_close(h);
    bool ok = (err == ESP_OK) && (commitErr == ESP_OK);
    Serial.printf("[NVS] op=save schema=%u bytes=4 ok=%d\n",
                  (unsigned)kSchemaVersion, ok ? 1 : 0);
    return ok;
}

static bool loadHiScore()
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("stackfall", NVS_READONLY, &h);
    if (err != ESP_OK) {
        Serial.printf("[NVS] op=load schema=%u ok=0 err=%d\n",
                      (unsigned)kSchemaVersion, (int)err);
        return false;
    }
    uint32_t value = 0;
    err = nvs_get_u32(h, "hiscore", &value);
    nvs_close(h);
    bool ok = (err == ESP_OK);
    Serial.printf("[NVS] op=load schema=%u ok=%d hi=%lu\n",
                  (unsigned)kSchemaVersion, ok ? 1 : 0,
                  (unsigned long)value);
    return ok && (value == kTestHiScore);
}

static void samplePower(uint32_t nowMs)
{
    const sf::PowerIn in = sf_hal::readPower(nowMs);
    Serial.printf("[PWR] t=%u vbat=%u charging=%d vbus=%u\n",
                  (unsigned)nowMs, (unsigned)in.vbatMv,
                  in.charging ? 1 : 0, (unsigned)in.vbusMv);
    s_powerCount += 1;
}

static void playQuadTone(uint32_t nowMs)
{
    if (!s_audioRunning) return;
    // Exercise the production one-voice cue path at the battery-safe cap.
    s_audio.play(sf::SfxId::Quad, nowMs);
    s_audioCount += 1;
    s_audioNextMs = nowMs + kAudioPeriodMs;
}

} // namespace

void sfProbeSetup()
{
    sf_hal::probe::display().setRotation(0);
    sf_hal::probe::display().setBrightness(128);
    sf_hal::probe::display().fillScreen(TFT_BLACK);
    sf_hal::probe::display().setTextSize(1);
    sf_hal::probe::display().setCursor(4, 4);
    sf_hal::probe::display().print("S5 POWER");

    s_audio.begin();

    // Ensure NVS is initialized, then save and immediately load back the test value.
    esp_err_t nvsErr = nvs_flash_init();
    if (nvsErr != ESP_OK && nvsErr != ESP_ERR_NVS_NO_FREE_PAGES
        && nvsErr != ESP_ERR_NVS_NEW_VERSION_FOUND) {
        Serial.printf("[NVS] op=init ok=0 err=%d\n", (int)nvsErr);
    }
    saveHiScore();
    loadHiScore();

    uint32_t nowMs = sf_hal::nowMs();
    samplePower(nowMs);
    s_powerNextMs = nowMs + kPowerPeriodMs;
    s_audioNextMs = nowMs + kAudioPeriodMs;
    s_testEndMs = nowMs + kTestDurationMs;
}

void sfProbeReport()
{
    Serial.printf("[PROBE] s=5 power_samples=%u audio_tones=%u\n",
                  (unsigned)s_powerCount, (unsigned)s_audioCount);
}

void sfProbeLoop()
{
    uint32_t nowMs = sf_hal::nowMs();
    s_audio.update(nowMs);

    if (nowMs >= s_powerNextMs) {
        samplePower(nowMs);
        s_powerNextMs += kPowerPeriodMs;
    }

    if (s_audioRunning) {
        if (nowMs >= s_testEndMs) {
            s_audioRunning = false;
            Serial.printf("[AUDIO] end t=%u tones=%u\n",
                          (unsigned)nowMs, (unsigned)s_audioCount);
        } else if (nowMs >= s_audioNextMs) {
            playQuadTone(nowMs);
        }
    }
}

#endif
