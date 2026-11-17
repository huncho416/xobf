#pragma once

#include <cstdint>

#include "config.hpp"
#include "random.hpp"

namespace xobf {

template <std::uint64_t K>
inline bool opaque_true() {
    std::uint64_t x = detail::opaque_seed;
    constexpr unsigned c = static_cast<unsigned>(K & 3u);
    if constexpr (c == 0)
        return ((x * (x + 1ull)) & 1ull) == 0ull;
    else if constexpr (c == 1)
        return (((x | 1ull) * (x | 1ull)) & 1ull) == 1ull;
    else if constexpr (c == 2)
        return ((x & 1ull) * ((x + 1ull) & 1ull)) == 0ull;
    else
        return (((x * x) - x) & 1ull) == 0ull;
}

template <std::uint64_t K>
inline bool opaque_false() {
    std::uint64_t x = detail::opaque_seed;
    constexpr unsigned c = static_cast<unsigned>(K & 3u);
    if constexpr (c == 0)
        return ((x * (x + 1ull)) & 1ull) == 1ull;
    else if constexpr (c == 1)
        return (((x | 1ull) * (x | 1ull)) & 1ull) == 0ull;
    else if constexpr (c == 2)
        return ((x & 1ull) * ((x + 1ull) & 1ull)) == 1ull;
    else
        return (((x * x) - x) & 1ull) == 1ull;
}

inline bool opaque_true() { return opaque_true<0x9e3779b9ull>(); }
inline bool opaque_false() { return opaque_false<0x9e3779b9ull>(); }

template <std::uint64_t K>
inline bool opaque_true_hashed() {
    std::uint64_t x = detail::opaque_seed;
    std::uint64_t h = detail::splitmix64(x ^ K);
    std::uint64_t g = detail::splitmix64(h);
    return (((h * (h + 1ull)) & 1ull) == 0ull) &&
           (((g * (g + 1ull)) & 1ull) == 0ull);
}

template <std::uint64_t K>
inline bool opaque_true_explode() {
    std::uint64_t x = detail::opaque_seed | 1ull;
    std::uint64_t acc = 0ull;
    for (unsigned i = 0; i < 8u; ++i) {
        x = detail::splitmix64(x ^ (K + i));
        acc ^= (x * (x + 1ull));
    }
    return (acc & 1ull) == 0ull;
}

}  // namespace xobf

#define XOBF_BOGUS(...)                                \
    do {                                               \
        if (::xobf::opaque_true<XOBF_KEY>()) {         \
            __VA_ARGS__;                               \
        } else {                                       \
            volatile int xobf_junk = 0;                \
            xobf_junk = xobf_junk * 7 + 13;            \
            (void)xobf_junk;                           \
        }                                              \
    } while (0)

#define XOBF_DEAD(...)                         \
    do {                                       \
        if (::xobf::opaque_false<XOBF_KEY>()) { \
            __VA_ARGS__;                        \
        }                                      \
    } while (0)
