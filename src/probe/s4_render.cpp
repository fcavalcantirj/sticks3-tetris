// SPIKE S4 — render budget measured against the SPI floor.
// Active only when -DSF_PROBE=5. Owned by probe_main.cpp's setup()/loop().
#if SF_PROBE == 5

#include <M5Unified.h>

#include "hal/sticks3/clock.h"
#include "hal/sticks3/panel.h"
#include "hal/sticks3/sysinfo.h"

namespace {

static constexpr int kSamples = 200;

// SPI floor values in microseconds, computed from w*h*16/40e6.
static constexpr uint32_t kFloorP1 = 12960; // 135x240
static constexpr uint32_t kFloorP2 = 8000;  // 100x200
static constexpr uint32_t kFloorP3 = 2640;  // 33x200
static constexpr uint32_t kFloorP4 = 972;   // 135x18
static constexpr uint32_t kFloorP5 = 1188;  // 135x22
static constexpr uint32_t kFloorP6 = 40;    // 10x10

struct Primitive {
    const char* name;
    int w;
    int h;
    uint32_t floorUs;
    uint32_t samples[kSamples];
};

static Primitive s_prims[] = {
    { "full_push",    135, 240, kFloorP1, {} },
    { "playfield",    100, 200, kFloorP2, {} },
    { "sidebar",       33, 200, kFloorP3, {} },
    { "topbar",       135,  18, kFloorP4, {} },
    { "footer",       135,  22, kFloorP5, {} },
    { "cell",          10,  10, kFloorP6, {} },
    { "fill_sprite",  135, 240, 0,        {} },
    { "draw_string",  135,  18, 0,        {} },
};
static constexpr int kPrimCount = sizeof(s_prims) / sizeof(s_prims[0]);

static uint32_t s_spriteUsed = 0;
static bool s_spriteOk = false;

static void insertionSort(uint32_t* arr, int n)
{
    for (int i = 1; i < n; ++i) {
        uint32_t v = arr[i];
        int j = i;
        while (j > 0 && arr[j - 1] > v) {
            arr[j] = arr[j - 1];
            --j;
        }
        arr[j] = v;
    }
}

static uint32_t percentile(uint32_t* sorted, int n, int pct)
{
    if (n <= 0) return 0;
    int idx = (n * pct) / 100;
    if (idx >= n) idx = n - 1;
    return sorted[idx];
}

static uint32_t timePush(lgfx::LGFX_Sprite& sprite, int w, int h)
{
    (void)w;
    (void)h;
    uint64_t t0 = sf_hal::nowUs();
    sprite.pushSprite(0, 0);
    return static_cast<uint32_t>(sf_hal::nowUs() - t0);
}

static void runBenchmark()
{
    // Main full-screen canvas, allocated once.
    uint32_t heapBeforeMain = sf_hal::freeHeap();
    lgfx::LGFX_Sprite mainCanvas(&sf_hal::probe::display());
    void* mainBuf = mainCanvas.createSprite(135, 240);
    uint32_t heapAfterMain = sf_hal::freeHeap();
    uint32_t usedMain = (heapBeforeMain >= heapAfterMain) ? (heapBeforeMain - heapAfterMain) : 0;
    s_spriteOk = (mainBuf != nullptr);
    s_spriteUsed = usedMain;

    mainCanvas.fillSprite(TFT_BLACK);

    // P1: full-frame push 135x240.
    for (int i = 0; i < kSamples; ++i) {
        s_prims[0].samples[i] = timePush(mainCanvas, 135, 240);
    }

    // P2: playfield push 100x200.
    {
        lgfx::LGFX_Sprite sp(&sf_hal::probe::display());
        sp.createSprite(100, 200);
        sp.fillSprite(TFT_BLACK);
        for (int i = 0; i < kSamples; ++i) {
            s_prims[1].samples[i] = timePush(sp, 100, 200);
        }
    }

    // P3: sidebar push 33x200.
    {
        lgfx::LGFX_Sprite sp(&sf_hal::probe::display());
        sp.createSprite(33, 200);
        sp.fillSprite(TFT_BLACK);
        for (int i = 0; i < kSamples; ++i) {
            s_prims[2].samples[i] = timePush(sp, 33, 200);
        }
    }

    // P4: top-bar push 135x18.
    {
        lgfx::LGFX_Sprite sp(&sf_hal::probe::display());
        sp.createSprite(135, 18);
        sp.fillSprite(TFT_BLACK);
        for (int i = 0; i < kSamples; ++i) {
            s_prims[3].samples[i] = timePush(sp, 135, 18);
        }
    }

    // P5: footer push 135x22.
    {
        lgfx::LGFX_Sprite sp(&sf_hal::probe::display());
        sp.createSprite(135, 22);
        sp.fillSprite(TFT_BLACK);
        for (int i = 0; i < kSamples; ++i) {
            s_prims[4].samples[i] = timePush(sp, 135, 22);
        }
    }

    // P6: one cell 10x10.
    {
        lgfx::LGFX_Sprite sp(&sf_hal::probe::display());
        sp.createSprite(10, 10);
        sp.fillSprite(TFT_BLACK);
        for (int i = 0; i < kSamples; ++i) {
            s_prims[5].samples[i] = timePush(sp, 10, 10);
        }
    }

    // P7: fillSprite over the whole 135x240 canvas.
    {
        for (int i = 0; i < kSamples; ++i) {
            uint64_t t0 = sf_hal::nowUs();
            mainCanvas.fillSprite(TFT_BLACK);
            s_prims[6].samples[i] =
                static_cast<uint32_t>(sf_hal::nowUs() - t0);
        }
    }

    // P8: drawString("SCORE 1234567") at text size 2.
    {
        mainCanvas.setTextSize(2);
        for (int i = 0; i < kSamples; ++i) {
            mainCanvas.fillSprite(TFT_BLACK);
            uint64_t t0 = sf_hal::nowUs();
            mainCanvas.drawString("SCORE 1234567", 0, 0);
            s_prims[7].samples[i] =
                static_cast<uint32_t>(sf_hal::nowUs() - t0);
        }
    }

    mainCanvas.deleteSprite();

    // Compute and emit percentiles.
    for (int p = 0; p < kPrimCount; ++p) {
        insertionSort(s_prims[p].samples, kSamples);
        uint32_t p50 = percentile(s_prims[p].samples, kSamples, 50);
        uint32_t p95 = percentile(s_prims[p].samples, kSamples, 95);
        uint32_t mx  = s_prims[p].samples[kSamples - 1];
        Serial.printf("[PERF] p=%d name=%s w=%d h=%d floor=%lu p50=%lu p95=%lu max=%lu",
                      p + 1,
                      s_prims[p].name,
                      s_prims[p].w,
                      s_prims[p].h,
                      (unsigned long)s_prims[p].floorUs,
                      (unsigned long)p50,
                      (unsigned long)p95,
                      (unsigned long)mx);
        if (s_prims[p].floorUs > 0) {
            // Ratio as integer thousandths to avoid float formatting.
            uint32_t ratio = (p95 * 1000) / s_prims[p].floorUs;
            Serial.printf(" ratio=%lu.%03lu",
                          (unsigned long)(ratio / 1000),
                          (unsigned long)(ratio % 1000));
        }
        Serial.printf("\n");
    }
}

} // namespace

void sfProbeSetup()
{
    sf_hal::probe::display().setRotation(0);
    sf_hal::probe::display().fillScreen(TFT_BLACK);
    sf_hal::probe::display().setTextSize(1);
    sf_hal::probe::display().setCursor(4, 4);
    sf_hal::probe::display().print("S4 RENDER");

    runBenchmark();
}

void sfProbeReport()
{
    Serial.printf("[PROBE] s=4 sprite=%s dram_used=%u\n",
                  s_spriteOk ? "ok" : "fail",
                  (unsigned)s_spriteUsed);
}

void sfProbeLoop()
{
}

#endif
