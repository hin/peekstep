// Peekstep - optional startup timing (--timing), to find slow steps.
#pragma once

#include <chrono>
#include <cstdio>

namespace peekstep::timing {

inline bool& enabled()
{
    static bool on = false;
    return on;
}

inline std::chrono::steady_clock::time_point& lastMark()
{
    static std::chrono::steady_clock::time_point t = std::chrono::steady_clock::now();
    return t;
}

// Call once, as early as possible, so later times count from program start.
inline void start(bool on)
{
    enabled() = on;
    lastMark() = std::chrono::steady_clock::now();
}

// Prints the time since the previous mark (and since start) for a step that just finished.
inline void mark(const char* step)
{
    if (!enabled())
        return;
    static const auto begin = lastMark();
    const auto now = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(now - lastMark()).count();
    const double total = std::chrono::duration<double, std::milli>(now - begin).count();
    lastMark() = now;
    std::fprintf(stderr, "timing: %8.1f ms  (total %8.1f ms)  %s\n", ms, total, step);
    std::fflush(stderr);
}

} // namespace peekstep::timing
