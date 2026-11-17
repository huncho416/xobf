#pragma once

#include <cstdint>
#include <type_traits>

#include "config.hpp"
#include "inline.hpp"
#include "mba.hpp"
#include "random.hpp"

namespace xobf {

namespace detail {

template <typename T, bool IsEnum = std::is_enum_v<T>>
struct base_int {
    using type = T;
};
template <typename T>
struct base_int<T, true> {
    using type = std::underlying_type_t<T>;
};
template <typename T>
using base_int_t = typename base_int<T>::type;

}  // namespace detail

template <typename T, std::uint64_t K>
class obf_const {
    static_assert(std::is_integral_v<T> || std::is_enum_v<T>);
    using U = std::make_unsigned_t<detail::base_int_t<T>>;

    static constexpr int slots =
        XOBF_VAULT_SLOTS < 1 ? 1 : XOBF_VAULT_SLOTS;
    static constexpr int real =
        static_cast<int>((K >> 11) % static_cast<std::uint64_t>(slots));

    volatile U vault_[slots];

    static constexpr U mask() {
        return static_cast<U>(detail::splitmix64(K ^ 0xa5a5a5a5a5a5a5a5ull));
    }

    static constexpr U decoy(int i) {
        return static_cast<U>(detail::splitmix64(
            K ^ (0x123456789abcdefull * static_cast<std::uint64_t>(i + 1))));
    }

    XOBF_NOINLINE U load() const {
        U e = static_cast<U>(vault_[real]);
        U v = static_cast<U>(e ^ mask());
        return ::xobf::mba::mix<K, XOBF_MBA_DEPTH, U>(v);
    }

public:
    explicit obf_const(T v) : vault_{} {
        for (int i = 0; i < slots; ++i)
            vault_[i] = decoy(i);
        vault_[real] = static_cast<U>(static_cast<U>(v) ^ mask());
    }

    T get() const { return static_cast<T>(load()); }
    operator T() const { return get(); }
};

}  // namespace xobf

#define XOBF_INT(value)                                  \
    ([&]() {                                             \
        constexpr std::uint64_t xobf_k = XOBF_KEY;       \
        using xobf_t = std::decay_t<decltype(value)>;    \
        return ::xobf::obf_const<xobf_t, xobf_k>(value); \
    }())
