#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace xobf::detail {

constexpr std::uint64_t fnv1a(const char* s,
                              std::uint64_t h = 0xcbf29ce484222325ull) {
    return (*s == '\0')
               ? h
               : fnv1a(s + 1,
                       (h ^ static_cast<std::uint64_t>(
                                static_cast<unsigned char>(*s))) *
                           0x100000001b3ull);
}

constexpr std::uint64_t build_seed() {
    return fnv1a(__DATE__, fnv1a(__TIME__));
}

inline volatile std::uint64_t opaque_seed = 0x9e3779b97f4a7c15ull;

constexpr std::uint64_t splitmix64(std::uint64_t x) {
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

constexpr std::uint64_t mix2(std::uint64_t a, std::uint64_t b) {
    return splitmix64(splitmix64(a) ^ (b + 0x9e3779b97f4a7c15ull));
}

constexpr std::uint64_t key64(std::uint64_t tag) {
    return splitmix64(build_seed() ^ splitmix64(tag));
}

template <typename T>
constexpr T rotl(T v, unsigned r) {
    using U = std::make_unsigned_t<T>;
    constexpr unsigned bits = sizeof(T) * 8;
    r &= (bits - 1);
    U u = static_cast<U>(v);
    return static_cast<T>(r == 0 ? u
                                 : static_cast<U>((u << r) | (u >> (bits - r))));
}

template <typename T>
constexpr T rotr(T v, unsigned r) {
    using U = std::make_unsigned_t<T>;
    constexpr unsigned bits = sizeof(T) * 8;
    r &= (bits - 1);
    U u = static_cast<U>(v);
    return static_cast<T>(r == 0 ? u
                                 : static_cast<U>((u >> r) | (u << (bits - r))));
}

struct ctprng {
    std::uint64_t state;

    constexpr explicit ctprng(std::uint64_t seed) : state(seed ? seed : 1ull) {}

    constexpr std::uint64_t next() {
        state = splitmix64(state);
        return state;
    }

    constexpr std::uint64_t bounded(std::uint64_t n) {
        return n == 0 ? 0 : (next() % n);
    }
};

constexpr std::uint64_t odd_from(std::uint64_t k) {
    return splitmix64(k) | 1ull;
}

constexpr std::uint64_t mod_inverse_pow2(std::uint64_t a) {
    std::uint64_t x = a;
    for (int i = 0; i < 6; ++i)
        x *= 2ull - a * x;
    return x;
}

template <std::uint64_t K>
constexpr unsigned char keybyte(std::size_t i) {
    return static_cast<unsigned char>(
        splitmix64(K ^ (0x9e3779b97f4a7c15ull *
                        (static_cast<std::uint64_t>(i) + 1))) &
        0xFFull);
}

}  // namespace xobf::detail

#define XOBF_KEY                                                              \
    (::xobf::detail::key64(((static_cast<std::uint64_t>(__COUNTER__)) << 1) ^ \
                           ((static_cast<std::uint64_t>(__LINE__)) << 17) ^   \
                           0x5bd1e9955bd1e995ull))
