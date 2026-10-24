#pragma once
#include <CHGame.h>

namespace mystic {
#if CHGAME_DEBUG
struct FramePerf {
    enum { Game, Wait, Expand, Chrome, Submit, Work, Interval, Count };
    uint32_t frames = 0, previous = 0, sum[Count] = {}, peak[Count] = {};
    void record(uint32_t start, uint32_t game, uint32_t wait, uint32_t expand,
                uint32_t chrome, uint32_t end) {
        const uint32_t values[Count] = {game-start, wait-game, expand-wait,
            chrome-expand, end-chrome, end-start, previous ? start-previous : 0};
        previous = start;
        ++frames;
        for (unsigned i=0; i<Count; ++i) {
            sum[i] += values[i];
            if (values[i] > peak[i]) peak[i] = values[i];
        }
    }
    void report() {
        char line[256];
        snprintf(line, sizeof line,
            "TIME n=%u game=%u gmax=%u wait=%u wmax=%u expand=%u emax=%u color=%u cmax=%u submit=%u work=%u max=%u interval=%u imax=%u\n",
            (unsigned)frames, avg(Game), top(Game), avg(Wait), top(Wait),
            avg(Expand), top(Expand), avg(Chrome), top(Chrome), avg(Submit),
            avg(Work), top(Work), avg(Interval), top(Interval));
        dbg::print(line);
        *this = FramePerf{};
    }
    unsigned avg(unsigned n) const {
        const unsigned count = n==Interval && frames ? frames-1 : frames;
        return count ? sum[n]/count : 0;
    }
    unsigned top(unsigned n) const { return peak[n]; }
};
static FramePerf framePerf;
#endif
}
