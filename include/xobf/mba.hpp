#pragma once

#include <cstdint>
#include <type_traits>

#include "random.hpp"

namespace xobf::mba {

template <typename T>
inline T add(T x, T y) {
    static_assert(std::is_integral_v<T>);
    using U = std::make_unsigned_t<T>;
    U ux = static_cast<U>(x), uy = static_cast<U>(y);
    return static_cast<T>((ux ^ uy) + 2u * (ux & uy));
}

template <typename T>
inline T sub(T x, T y) {
    using U = std::make_unsigned_t<T>;
    return add<T>(x, static_cast<T>(static_cast<U>(~static_cast<U>(y)) + 1u));
}

template <typename T>
inline T xor_(T x, T y) {
    using U = std::make_unsigned_t<T>;
    U ux = static_cast<U>(x), uy = static_cast<U>(y);
    return static_cast<T>((ux | uy) - (ux & uy));
}

template <typename T>
inline T and_(T x, T y) {
    using U = std::make_unsigned_t<T>;
    U ux = static_cast<U>(x), uy = static_cast<U>(y);
    return static_cast<T>((ux + uy) - (ux | uy));
}

template <typename T>
inline T or_(T x, T y) {
    using U = std::make_unsigned_t<T>;
    U ux = static_cast<U>(x), uy = static_cast<U>(y);
    return static_cast<T>((ux & uy) + (ux ^ uy));
}

namespace detail {

template <typename T>
inline std::make_unsigned_t<T> junk_key(std::uint64_t k) {
    using U = std::make_unsigned_t<T>;
    std::uint64_t s =
        ::xobf::detail::opaque_seed ^ ::xobf::detail::splitmix64(k);
    return static_cast<U>(s);
}

}  // namespace detail

template <std::uint64_t K, int D, typename T>
inline T mix(T x) {
    static_assert(std::is_integral_v<T>);
    if constexpr (D <= 0) {
        return x;
    } else {
        using U = std::make_unsigned_t<T>;
        constexpr std::uint64_t nk = ::xobf::detail::splitmix64(K);
        constexpr unsigned choice = static_cast<unsigned>(K & 3u);
        if constexpr (choice == 0) {
            U k = detail::junk_key<T>(K);
            x = static_cast<T>(static_cast<U>(x) ^ k);
            x = mix<nk, D - 1, T>(x);
            x = static_cast<T>(static_cast<U>(x) ^ k);
            return x;
        } else if constexpr (choice == 1) {
            U k = detail::junk_key<T>(K);
            x = static_cast<T>(static_cast<U>(x) + k);
            x = mix<nk, D - 1, T>(x);
            x = static_cast<T>(static_cast<U>(x) - k);
            return x;
        } else if constexpr (choice == 2) {
            constexpr unsigned r =
                static_cast<unsigned>((K >> 2) % (sizeof(T) * 8));
            x = ::xobf::detail::rotl<T>(x, r);
            x = mix<nk, D - 1, T>(x);
            x = ::xobf::detail::rotr<T>(x, r);
            return x;
        } else {
            x = static_cast<T>(~static_cast<U>(x));
            x = mix<nk, D - 1, T>(x);
            x = static_cast<T>(~static_cast<U>(x));
            return x;
        }
    }
}

template <std::uint64_t K, typename T>
inline T add_poly(T x, T y) {
    using U = std::make_unsigned_t<T>;
    U ux = static_cast<U>(x), uy = static_cast<U>(y);
    constexpr unsigned v = static_cast<unsigned>(K % 3u);
    if constexpr (v == 0)
        return static_cast<T>((ux ^ uy) + 2u * (ux & uy));
    else if constexpr (v == 1)
        return static_cast<T>((ux | uy) + (ux & uy));
    else
        return static_cast<T>(ux - static_cast<U>(~uy) - 1u);
}

template <std::uint64_t K, typename T>
inline T sub_poly(T x, T y) {
    using U = std::make_unsigned_t<T>;
    U ux = static_cast<U>(x), uy = static_cast<U>(y);
    constexpr unsigned v = static_cast<unsigned>(K % 3u);
    if constexpr (v == 0)
        return static_cast<T>(ux + static_cast<U>(~uy) + 1u);
    else if constexpr (v == 1)
        return static_cast<T>((ux ^ uy) - 2u * (static_cast<U>(~ux) & uy));
    else
        return static_cast<T>((ux & static_cast<U>(~uy)) -
                              (static_cast<U>(~ux) & uy));
}

template <std::uint64_t K, typename T>
inline T xor_poly(T x, T y) {
    using U = std::make_unsigned_t<T>;
    U ux = static_cast<U>(x), uy = static_cast<U>(y);
    constexpr unsigned v = static_cast<unsigned>(K % 2u);
    if constexpr (v == 0)
        return static_cast<T>((ux | uy) - (ux & uy));
    else
        return static_cast<T>((ux | uy) & static_cast<U>(~(ux & uy)));
}

}  // namespace xobf::mba
