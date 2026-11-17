#pragma once

#include <cstddef>
#include <cstdint>

#include "config.hpp"
#include "inline.hpp"
#include "predicate.hpp"

namespace xobf {

XOBF_NOINLINE inline std::uint64_t code_checksum(const void* p,
                                                 std::size_t n) {
    const unsigned char* b = static_cast<const unsigned char*>(p);
    std::uint64_t h = 0xcbf29ce484222325ull;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= static_cast<std::uint64_t>(b[i]);
        h *= 0x100000001b3ull;
    }
    return h;
}

inline bool verify_checksum(const void* p, std::size_t n,
                            std::uint64_t expected) {
    std::uint64_t h = code_checksum(p, n);
    std::uint64_t s = detail::opaque_seed;
    std::uint64_t z = (s * (s + 1ull)) & 1ull;
    return (h + z - z) == expected;
}

XOBF_NOINLINE inline std::uint64_t region_checksum(const void* begin,
                                                   const void* end) {
    const unsigned char* a = static_cast<const unsigned char*>(begin);
    const unsigned char* b = static_cast<const unsigned char*>(end);
    if (b <= a)
        return 0ull;
    return code_checksum(a, static_cast<std::size_t>(b - a));
}

}  // namespace xobf

#define XOBF_CHECKSUM(ptr, len) (::xobf::code_checksum((ptr), (len)))

#define XOBF_VERIFY_OR(ptr, len, expected, ...)        \
    do {                                               \
        if (!::xobf::verify_checksum((ptr), (len),     \
                                     (expected))) {     \
            __VA_ARGS__;                               \
        }                                              \
    } while (0)
