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

constexpr std::uint32_t flat_tok(std::uint64_t K, std::uint32_t n) {
    std::uint32_t k = static_cast<std::uint32_t>(detail::splitmix64(K)) | 1u;
    std::uint32_t x = n ^ static_cast<std::uint32_t>(K >> 29);
    x *= k;
    x ^= x >> 15;
    x *= 0x2c1b3c6du;
    x ^= x >> 13;
    x ^= static_cast<std::uint32_t>(K >> 7);
    return x;
}

inline std::uint32_t flat_mask() {
    std::uint32_t x = static_cast<std::uint32_t>(detail::opaque_seed);
    return x ^ 0x5bd1e995u;
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

#define XOBF_DFLATTEN_BEGIN(entry)                                            \
    {                                                                         \
        constexpr std::uint64_t xobf_dk = XOBF_KEY;                           \
        const std::uint32_t xobf_dm = ::xobf::flat_mask();                    \
        volatile std::uint32_t xobf_dfsm =                                    \
            ::xobf::flat_tok(xobf_dk, static_cast<std::uint32_t>(entry)) ^    \
            xobf_dm;                                                          \
        for (;;) {                                                            \
            switch (static_cast<std::uint32_t>(xobf_dfsm) ^ xobf_dm) {

#define XOBF_DCASE(n) \
    case ::xobf::flat_tok(xobf_dk, static_cast<std::uint32_t>(n)):

#define XOBF_DGOTO(n)                                                         \
    {                                                                         \
        xobf_dfsm = static_cast<std::uint32_t>(::xobf::opaque_identity(       \
                        ::xobf::flat_tok(                                     \
                            xobf_dk, static_cast<std::uint32_t>(n)))) ^       \
                    xobf_dm;                                                  \
        break;                                                                \
    }

#define XOBF_DGOTO_IF(cond, n)                                                \
    if (cond) {                                                               \
        xobf_dfsm = static_cast<std::uint32_t>(::xobf::opaque_identity(       \
                        ::xobf::flat_tok(                                     \
                            xobf_dk, static_cast<std::uint32_t>(n)))) ^       \
                    xobf_dm;                                                  \
        break;                                                                \
    }

#define XOBF_DFLATTEN_EXIT_TO(label) goto label

#define XOBF_DFLATTEN_END(label) \
    default:                     \
        goto label;              \
        }                        \
        }                        \
        label:;                  \
    }
