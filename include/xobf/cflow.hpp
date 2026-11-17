#pragma once

#include <cstdint>

#include "config.hpp"
#include "predicate.hpp"
#include "random.hpp"

namespace xobf {

inline unsigned opaque_zero() {
    std::uint64_t x = detail::opaque_seed;
    return static_cast<unsigned>((x * (x + 1ull)) & 1ull);
}

inline unsigned opaque_one() { return opaque_zero() + 1u; }

inline std::uint64_t opaque_identity(std::uint64_t v) {
    std::uint64_t x = detail::opaque_seed;
    std::uint64_t z = (x * (x + 1ull)) & 1ull;
    return v + z - z;
}

}  // namespace xobf

#define XOBF_FLATTEN_BEGIN(entry)                                 \
    {                                                             \
        volatile unsigned xobf_fsm_state =                       \
            static_cast<unsigned>(entry);                        \
        for (;;) {                                               \
            switch (xobf_fsm_state) {

#define XOBF_CASE(n) case static_cast<unsigned>(n):

#define XOBF_GOTO(n)                                \
    {                                               \
        xobf_fsm_state = static_cast<unsigned>(n);  \
        break;                                      \
    }

#define XOBF_GOTO_IF(cond, n)                           \
    if (cond) {                                         \
        xobf_fsm_state = static_cast<unsigned>(n);      \
        break;                                          \
    }

#define XOBF_FLATTEN_EXIT_TO(label) goto label

#define XOBF_FLATTEN_END(label) \
    default:                    \
        goto label;             \
        }                       \
        }                       \
        label:;                 \
        }

#define XOBF_JUNK()                                            \
    do {                                                       \
        if (::xobf::opaque_false<XOBF_KEY>()) {                \
            volatile int xobf_j =                              \
                static_cast<int>(::xobf::detail::opaque_seed); \
            xobf_j = xobf_j * 7 + 13;                          \
            (void)xobf_j;                                      \
        }                                                      \
    } while (0)

#define XOBF_OPAQUE(v) (::xobf::opaque_identity(static_cast<std::uint64_t>(v)))
