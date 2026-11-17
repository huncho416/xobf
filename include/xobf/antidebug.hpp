#pragma once

#include <cstdint>

#include "config.hpp"
#include "predicate.hpp"
#include "string.hpp"

#if defined(XOBF_OS_LINUX) || defined(XOBF_OS_MACOS)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif

#if defined(XOBF_OS_WINDOWS)
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent(void);
#endif

#include <chrono>

namespace xobf {

inline bool is_debugged() {
#if defined(XOBF_OS_LINUX)
    const char* path = XOBF_STR("/proc/self/status");
    std::FILE* f = std::fopen(path, "r");
    if (!f)
        return false;
    char line[256];
    const char* key = XOBF_STR("TracerPid:");
    bool traced = false;
    while (std::fgets(line, sizeof(line), f)) {
        if (std::strncmp(line, key, 10) == 0) {
            traced = std::atoi(line + 10) != 0;
            break;
        }
    }
    std::fclose(f);
    return traced;
#elif defined(XOBF_OS_WINDOWS)
    return IsDebuggerPresent() != 0;
#else
    return false;
#endif
}

inline bool timing_tripped(std::uint64_t threshold_ns = 50000000ull) {
    auto t0 = std::chrono::steady_clock::now();
    volatile std::uint64_t acc = detail::opaque_seed;
    for (int i = 0; i < 64; ++i)
        acc = acc * 6364136223846793005ull + 1442695040888963407ull;
    auto t1 = std::chrono::steady_clock::now();
    (void)acc;
    auto ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return static_cast<std::uint64_t>(ns) > threshold_ns;
}

inline bool under_analysis() { return is_debugged() || timing_tripped(); }

}  // namespace xobf

#define XOBF_IF_DEBUGGED(...)       \
    do {                            \
        if (::xobf::is_debugged()) { \
            __VA_ARGS__;            \
        }                           \
    } while (0)

#define XOBF_CRASH_IF_DEBUGGED()                                      \
    do {                                                             \
        if (::xobf::is_debugged()) {                                 \
            volatile int* xobf_bad = reinterpret_cast<int*>(0);      \
            *xobf_bad = 0;                                           \
        }                                                            \
    } while (0)
