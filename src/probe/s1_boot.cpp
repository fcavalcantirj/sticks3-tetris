// SPIKE S1 — board identity, PSRAM, portrait rotation, first light.
// Active only when -DSF_PROBE=1. Owned by probe_main.cpp's setup()/loop().
#if SF_PROBE == 1

#include <M5Unified.h>

#include "hal/sticks3/panel.h"
#include "hal/sticks3/sysinfo.h"

static bool s_s1Done = false;
static bool s_spriteOk = false;
static uint32_t s_spriteUsed = 0;

void sfProbeSetup()
{
    sf_hal::probe::display().setRotation(0);

    // NOTE: boot identity is printed by the shared printer in probe_main.cpp
    // (at boot and repeated with the first three heartbeats), not here.

    // First-light pattern: proves geometry, not just backlight.
    sf_hal::probe::display().fillScreen(TFT_BLACK);
    sf_hal::probe::display().drawRect(0, 0, 135, 240, TFT_WHITE);
    // Playfield rect x 1..100 / y 18..217: 10x10 squares at its four corners.
    sf_hal::probe::display().fillRect(1, 18, 10, 10, TFT_RED);
    sf_hal::probe::display().fillRect(91, 18, 10, 10, TFT_RED);
    sf_hal::probe::display().fillRect(1, 208, 10, 10, TFT_RED);
    sf_hal::probe::display().fillRect(91, 208, 10, 10, TFT_RED);
    sf_hal::probe::display().setTextSize(1);
    sf_hal::probe::display().setCursor(4, 4);
    sf_hal::probe::display().print("STACKFALL S1");

    uint32_t heapBefore = sf_hal::freeHeap();
    lgfx::LGFX_Sprite sprite(&sf_hal::probe::display());
    void *buf = sprite.createSprite(135, 240);
    uint32_t heapAfter = sf_hal::freeHeap();
    uint32_t used = (heapBefore >= heapAfter) ? (heapBefore - heapAfter) : 0;
    s_spriteOk = (buf != nullptr);
    s_spriteUsed = used;
    sprite.deleteSprite();
    s_s1Done = true;
    (void)s_s1Done;
}

void sfProbeReport()
{
    Serial.printf("[PROBE] s=1 sprite=%s dram_used=%u\n",
                  s_spriteOk ? "ok" : "fail",
                  (unsigned)s_spriteUsed);
}

void sfProbeLoop()
{
}

#endif
