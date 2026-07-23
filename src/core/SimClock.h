#ifndef ASHB_SIM_CLOCK_H
#define ASHB_SIM_CLOCK_H

#include <cstdint>

// ── The one simulation clock ─────────────────────────────────────────────────
// Before this existed, "day" meant three different things in three places:
// a render-frame counter (main loop), a tick index (civ engine cadence), and
// a calendar day (environment). Aging used an 8-tick "year" while harvest luck
// used a 365-FRAME "year" (~6 ticks), so famine cycles and lifespans never
// lined up. All time questions now go through this struct.
//
// Time model (deliberately compressed for generational turnover):
//   1 tick  = FRAMES_PER_TICK render frames = 1 in-world day
//   1 year  = DAYS_PER_YEAR in-world days  (agents age +1 per year)
//   civ/economy/social-order systems run every TICKS_PER_CIV_TICK days
struct SimClock {
    static constexpr int FRAMES_PER_TICK   = 60;
    static constexpr int TICKS_PER_DAY     = 1;
    static constexpr int DAYS_PER_YEAR     = 16;
    static constexpr int TICKS_PER_CIV_TICK = 5;

    // Raw frame counter; the main loop assigns this every step.
    uint64_t frame = 0;

    uint64_t tick() const { return frame / FRAMES_PER_TICK; }
    uint64_t day()  const { return tick() / TICKS_PER_DAY; }
    uint64_t year() const { return day() / DAYS_PER_YEAR; }

    // Sub-day time-of-day, derived from position within the current tick
    // (matches the legacy (day % 60) * 24 / 60 formula).
    int hourOfDay() const {
        return (int)((frame % FRAMES_PER_TICK) * 24 / FRAMES_PER_TICK);
    }
    int dayOfWeek() const { return (int)(day() % 7); }

    // True on ticks where a new in-world year begins (birthday/aging cadence).
    bool isYearStartTick() const { return tick() % DAYS_PER_YEAR == 0; }
    // True on ticks where the macro (civilization) layer should run.
    bool isCivTick() const { return tick() % TICKS_PER_CIV_TICK == 0; }
};

// Global clock instance (C++17 inline variable; set from the main loop).
inline SimClock g_clock;

#endif // ASHB_SIM_CLOCK_H
