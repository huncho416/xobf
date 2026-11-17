// xobf - compile-time obfuscation library
// random.hpp : compile-time seeding and key derivation
//
// Techniques here are derived from the research report: constants/keys are
// generated at *compile time* so no plaintext key material is embedded, and a
// strong mixing function (splitmix64) spreads each per-site key.
#pragma once

#include <cstddef>
#include <cstdint>

namespace xobf::detail {

// FNV-1a over a C string, usable in constant expressions. Used to turn the
// compiler's __DATE__/__TIME__ macros into a build-unique 64-bit seed so that
// two builds of the same source produce different ciphertext.
constexpr std::uint64_t fnv1a(const char* s,
                              std::uint64_t h = 0xcbf29ce484222325ull) {
    return (*s == '\0')
               ? h
               : fnv1a(s + 1,
                       (h ^ static_cast<std::uint64_t>(
                                static_cast<unsigned char>(*s))) *
                           0x100000001b3ull);
}

// Per-translation-unit build seed (changes every rebuild).
constexpr std::uint64_t build_seed() {
    return fnv1a(__DATE__, fnv1a(__TIME__));
}

// splitmix64 - a fast, well-distributed finalizer used for all key expansion.
constexpr std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

// Derive a 64-bit key for a call site from a caller-supplied unique tag mixed
// with the build seed.
constexpr std::uint64_t key64(std::uint64_t tag) {
    return splitmix64(build_seed() ^ splitmix64(tag));
}

// Per-index keystream byte for a given 64-bit key. Each byte position gets a
// distinct, non-linear value (resists simple XOR-key recovery).
template <std::uint64_t K>
constexpr unsigned char keybyte(std::size_t i) {
    return static_cast<unsigned char>(
        splitmix64(K ^ (0x9e3779b97f4a7c15ull * (static_cast<std::uint64_t>(i) + 1))) &
        0xFFull);
}

}  // namespace xobf::detail

// A fresh, call-site-unique compile-time key. __COUNTER__ guarantees a new key
// for every expansion; __LINE__ adds further entropy.
#define XOBF_KEY                                                              \
    (::xobf::detail::key64(((static_cast<std::uint64_t>(__COUNTER__)) << 1) ^ \
                           ((static_cast<std::uint64_t>(__LINE__)) << 17) ^   \
                           0x5bd1e9955bd1e995ull))
