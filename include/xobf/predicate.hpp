// xobf/predicate.hpp : opaque predicates and bogus control flow.
//
// Research basis: an opaque predicate always evaluates to a known value but is
// hard for a decompiler to fold. binprotect seeds its predicate from *runtime*
// values (rsp/rip) so it cannot be constant-folded; es3n1n notes that
// statically-foldable predicates are useless. We follow binprotect: seed from a
// `volatile` global the optimiser cannot prove constant.
#pragma once

#include <cstdint>

namespace xobf::detail {

// Volatile seed: the compiler cannot assume its value, so predicates built on
// it are not folded away at compile time.
inline volatile std::uint64_t opaque_seed = 0x9e3779b97f4a7c15ull;

}  // namespace xobf

namespace xobf {

// Always true: x*(x+1) is a product of two consecutive integers, hence even,
// so its least significant bit is always 0. Uses the runtime volatile seed.
inline bool opaque_true() {
    std::uint64_t x = detail::opaque_seed;
    return ((x * (x + 1)) & 1ull) == 0ull;
}

// Always false.
inline bool opaque_false() {
    std::uint64_t x = detail::opaque_seed;
    // 7*y*y - 1 is never a multiple of... simpler: (x*x + x) is even -> &1 == 1
    // is impossible -> always false.
    return ((x * (x + 1)) & 1ull) == 1ull;
}

}  // namespace xobf

// XOBF_BOGUS(stmt): execute stmt, but wrapped behind an opaque-true predicate
// with an unreachable bogus branch, confusing control-flow recovery.
#define XOBF_BOGUS(...)                       \
    do {                                      \
        if (::xobf::opaque_true()) {          \
            __VA_ARGS__;                      \
        } else {                              \
            volatile int xobf_junk = 0;       \
            xobf_junk = xobf_junk * 7 + 13;   \
            (void)xobf_junk;                  \
        }                                     \
    } while (0)

// XOBF_DEAD(stmt): stmt placed on a provably-dead (opaque_false) branch. Never
// executes at runtime, but appears live to naive static analysis.
#define XOBF_DEAD(...)              \
    do {                           \
        if (::xobf::opaque_false()) { \
            __VA_ARGS__;           \
        }                          \
    } while (0)
