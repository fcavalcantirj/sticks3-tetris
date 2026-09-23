// Probe dispatcher: owns setup()/loop(); exactly one spike is active per build.
// Every spike file defines sfProbeSetup/sfProbeLoop inside its own
// #if SF_PROBE == n guard, so all probe sources compile in every probe build
// with no duplicate symbols and no file shuffling between spikes.
#include <M5Unified.h>

#include "hal/sticks3/buttons.h"
#include "hal/sticks3/clock.h"
#include "hal/sticks3/panel.h"
#include "hal/sticks3/sysinfo.h"

void sfProbeSetup();
void sfProbeLoop();
void sfProbeReport();

#if SF_PROBE == 0
void sfProbeSetup()
{
    Serial.printf("[PROBE] s=0 none-selected\n");
}
void sfProbeLoop()
{
}
void sfProbeReport()
{
}
#endif

static uint32_t s_hbCount = 0;
static int64_t s_hbLastUs = 0;
static int64_t s_loopLastUs = 0;

static constexpr int kLoopPeriodRingSize = 1024;
static uint32_t s_loopPeriods[kLoopPeriodRingSize];
static int s_loopPeriodCount = 0;

static void sfPrintBoot()
{
    Serial.printf("[BOOT] fw=%s sha=%s probe=%d board=%d psram=%u heap=%u w=%d h=%d rot=%d\n",
                  SF_VERSION, SF_SHA, (int)SF_PROBE,
                  (int)M5.getBoard(),
                  (unsigned)sf_hal::psramBytes(),
                  (unsigned)sf_hal::freeHeap(),
                  sf_hal::probe::display().width(),
                  sf_hal::probe::display().height(),
                  sf_hal::probe::display().getRotation());
}

static uint32_t loopPeriodMedian(uint32_t* arr, int n)
{
    if (n <= 0) return 0;
    for (int i = 1; i < n; ++i) {
        uint32_t v = arr[i]; int j = i;
        while (j > 0 && arr[j-1] > v) { arr[j] = arr[j-1]; --j; }
        arr[j] = v;
    }
    return arr[n / 2];
}

void setup()
{
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200; // M5Unified default is 0, so M5.begin would never start Serial
    M5.begin(cfg);
    uint32_t t0 = sf_hal::nowMs();
    while (!Serial && sf_hal::nowMs() - t0 < 2000u) { yield(); }
    s_hbLastUs = static_cast<int64_t>(sf_hal::nowUs());
    s_loopLastUs = s_hbLastUs;
    sfProbeSetup();
    sfPrintBoot();
}

void loop()
{
    sf_hal::updateDevice();
    sfProbeLoop();
    int64_t nowUs = static_cast<int64_t>(sf_hal::nowUs());

    // Measure the loop period for the heartbeat telemetry.
    uint32_t period = (uint32_t)(nowUs - s_loopLastUs);
    s_loopLastUs = nowUs;
    if (s_loopPeriodCount < kLoopPeriodRingSize) {
        s_loopPeriods[s_loopPeriodCount++] = period;
    }

    if (nowUs - s_hbLastUs >= (int64_t)5000 * (int64_t)1000)
    {
        s_hbLastUs = nowUs;
        s_hbCount += 1;
        if (s_hbCount <= 3) { sfPrintBoot(); sfProbeReport(); }
        uint32_t upMs = (uint32_t)(nowUs / (int64_t)1000);
        uint32_t heap = sf_hal::freeHeap();
        uint32_t minUs = 0, maxUs = 0, medUs = 0;
        int n = s_loopPeriodCount;
        if (n > 0) {
            uint32_t tmp[kLoopPeriodRingSize];
            for (int i = 0; i < n; ++i) tmp[i] = s_loopPeriods[i];
            minUs = tmp[0]; maxUs = tmp[0];
            for (int i = 1; i < n; ++i) {
                if (tmp[i] < minUs) minUs = tmp[i];
                if (tmp[i] > maxUs) maxUs = tmp[i];
            }
            medUs = loopPeriodMedian(tmp, n);
        }
        Serial.printf("[HB] n=%lu up=%lu heap=%lu loop_us min=%lu med=%lu max=%lu n=%lu\n",
                      (unsigned long)s_hbCount,
                      (unsigned long)upMs,
                      (unsigned long)heap,
                      (unsigned long)minUs,
                      (unsigned long)medUs,
                      (unsigned long)maxUs,
                      (unsigned long)n);
        s_loopPeriodCount = 0;
    }
}
