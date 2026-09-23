// SPIKE S2 — hand-rolled press/hold/chord poller over isPressed() only.
// Active only when -DSF_PROBE == 2. Owned by probe_main.cpp's setup()/loop().
//
// Grammar (docs/CONTROLS.md, rules a/b/c): press edge fires at once; hold at
// 500 ms of one key down; chord at 800 ms of both down; a SIDE press edge
// while BLUE is already down arms the chord and stands for no action of its
// own; while both are down neither per-key hold timer runs. The [K] stream
// records physical edges, not meaning, so that SIDE edge is still logged.
#if SF_PROBE == 2

#include <M5Unified.h>

#include "hal/sticks3/buttons.h"
#include "hal/sticks3/clock.h"
#include "hal/sticks3/panel.h"

namespace {

// Thresholds from the locked control map.
constexpr uint32_t kDebounceMs = 8;
constexpr uint32_t kHoldMs = 500;
constexpr uint32_t kChordMs = 800;
constexpr int64_t kSummaryUs = (int64_t)5000 * (int64_t)1000;
constexpr int kLatRingCap = 256;

struct KeyState {
    bool down = false;
    uint32_t downAtMs = 0;
    uint32_t lastChangeMs = 0;
    bool holdFired = false;
    bool holdBlocked = false; // other key was down at this key's 500 ms mark
    bool bothSeen = false; // other key was down during this press: no click
};

KeyState s_blue;
KeyState s_side;
bool s_chordArmed = false;
uint32_t s_bothDownAtMs = 0;
bool s_chordFired = false; // latched until both keys are back up

// Cumulative counters, never reset: drill deltas read off the last line.
uint32_t s_press = 0;
uint32_t s_release = 0;
uint32_t s_hold = 0;
uint32_t s_click = 0;
uint32_t s_chord = 0;

// Fixed ring of press-edge-to-action latency samples, microseconds, no heap.
uint32_t s_latRing[kLatRingCap] = {0};
int s_latIdx = 0;
uint32_t s_latCount = 0;

int64_t s_lastSummaryUs = 0;

void emitK(uint32_t nowMs, const char *ev)
{
    Serial.printf("[K] t=%lu blue=%d side=%d ev=%s\n",
                  (unsigned long)nowMs,
                  s_blue.down ? 1 : 0,
                  s_side.down ? 1 : 0,
                  ev);
}

void pushLat(int64_t t0us)
{
    int64_t d = static_cast<int64_t>(sf_hal::nowUs()) - t0us;
    if (d < 0)
    {
        d = 0;
    }
    s_latRing[s_latIdx] = (uint32_t)d;
    s_latIdx = (s_latIdx + 1) % kLatRingCap;
    if (s_latCount < (uint32_t)kLatRingCap)
    {
        ++s_latCount;
    }
}

void printSummary()
{
    uint32_t p50 = 0;
    uint32_t p95 = 0;
    uint32_t mx = 0;
    uint32_t n = s_latCount;
    if (n > 0)
    {
        uint32_t tmp[kLatRingCap];
        for (uint32_t i = 0; i < n; ++i)
        {
            tmp[i] = s_latRing[i];
        }
        for (uint32_t i = 1; i < n; ++i)
        {
            uint32_t v = tmp[i];
            uint32_t j = i;
            while (j > 0 && tmp[j - 1] > v)
            {
                tmp[j] = tmp[j - 1];
                --j;
            }
            tmp[j] = v;
        }
        p50 = tmp[n / 2] / 1000u;
        uint32_t i95 = (n * 95u) / 100u;
        if (i95 >= n)
        {
            i95 = n - 1;
        }
        p95 = tmp[i95] / 1000u;
        mx = tmp[n - 1] / 1000u;
    }
    Serial.printf("[PROBE] s=2 press=%lu release=%lu hold=%lu click=%lu chord=%lu lat_p50=%lu lat_p95=%lu lat_max=%lu\n",
                  (unsigned long)s_press,
                  (unsigned long)s_release,
                  (unsigned long)s_hold,
                  (unsigned long)s_click,
                  (unsigned long)s_chord,
                  (unsigned long)p50,
                  (unsigned long)p95,
                  (unsigned long)mx);
}

// One debounced poll step. t0us was taken at the top of the loop iteration;
// nowMs is the same clock in milliseconds (unsigned, so wrap-safe).
void update(bool blueRaw, bool sideRaw, uint32_t nowMs, int64_t t0us)
{
    KeyState *keys[2] = {&s_blue, &s_side};
    bool raws[2] = {blueRaw, sideRaw};
    for (int k = 0; k < 2; ++k)
    {
        KeyState &ks = *keys[k];
        bool raw = raws[k];
        if (raw != ks.down && (uint32_t)(nowMs - ks.lastChangeMs) >= kDebounceMs)
        {
            ks.down = raw;
            ks.lastChangeMs = nowMs;
            if (raw)
            {
                ks.downAtMs = nowMs;
                ks.holdFired = false;
                ks.holdBlocked = false;
                ks.bothSeen = false;
                ++s_press;
                emitK(nowMs, "press");
                pushLat(t0us);
            }
            else
            {
                ++s_release;
                emitK(nowMs, "release");
                pushLat(t0us);
                // Short press on a lone key. A press that shared its gesture
                // with the other key (pause modifier) never counts.
                if (!ks.holdFired && !ks.bothSeen && !s_chordFired)
                {
                    ++s_click;
                }
            }
        }
    }

    if (s_blue.down && s_side.down)
    {
        if (!s_chordArmed && !s_chordFired)
        {
            s_chordArmed = true;
            s_bothDownAtMs = nowMs;
        }
        s_blue.bothSeen = true;
        s_side.bothSeen = true;
    }
    else if (!s_blue.down && !s_side.down)
    {
        s_chordArmed = false;
        s_chordFired = false;
    }
    else if (!s_chordFired)
    {
        // Exactly one key down and no chord latched: the both-down window,
        // if any, ended without a chord.
        s_chordArmed = false;
    }

    // Per-key holds are suspended for as long as both keys are down, and a
    // hold whose 500 ms mark arrives while the other key is down never fires
    // at all (rule b) — nor anything after a chord in this gesture.
    if (!s_chordFired)
    {
        for (int k = 0; k < 2; ++k)
        {
            KeyState &ks = *keys[k];
            bool otherDown = (k == 0) ? s_side.down : s_blue.down;
            bool pastDue = ks.down &&
                (uint32_t)(nowMs - ks.downAtMs) >= kHoldMs;
            if (pastDue && !ks.holdFired && !ks.holdBlocked)
            {
                if (otherDown)
                {
                    // The 500 ms mark arrived with the other key down: this
                    // gesture gets no hold even if the other key lifts later.
                    ks.holdBlocked = true;
                }
                else
                {
                    ks.holdFired = true;
                    ++s_hold;
                    emitK(nowMs, "hold");
                    pushLat(t0us);
                }
            }
        }
    }

    if (s_chordArmed && !s_chordFired &&
        (uint32_t)(nowMs - s_bothDownAtMs) >= kChordMs)
    {
        s_chordFired = true;
        // The chord owns the gesture from here: neither key may hold nor
        // click on its way up.
        s_blue.holdFired = true;
        s_side.holdFired = true;
        ++s_chord;
        emitK(nowMs, "chord");
        pushLat(t0us);
    }
}

} // namespace

void sfProbeSetup()
{
    sf_hal::probe::display().setRotation(0);
    sf_hal::probe::display().fillScreen(TFT_BLACK);
    sf_hal::probe::display().setTextSize(2);
    sf_hal::probe::display().setCursor(4, 4);
    sf_hal::probe::display().print("S2 KEYS");
    sf_hal::probe::display().setTextSize(1);
    sf_hal::probe::display().setCursor(4, 30);
    sf_hal::probe::display().print("see serial [K]/[PROBE]");
    // One-shot lines stay out of setup: the USB-CDC host attaches later.
    // The first summary goes out via sfProbeReport() instead.
    s_lastSummaryUs = static_cast<int64_t>(sf_hal::nowUs());
}

void sfProbeReport()
{
    printSummary();
}

void sfProbeLoop()
{
    int64_t t0us = static_cast<int64_t>(sf_hal::nowUs());
    uint32_t nowMs = (uint32_t)(t0us / (int64_t)1000);
    const sf_hal::RawButtons buttons = sf_hal::sampleButtons(nowMs);
    update(buttons.blue, buttons.side, buttons.tMs, t0us);
    if (t0us - s_lastSummaryUs >= kSummaryUs)
    {
        s_lastSummaryUs = t0us;
        printSummary();
    }
}

#endif
