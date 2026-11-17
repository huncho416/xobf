// xobf/constant.hpp : integer constant encryption.
//
// Research basis: "constant encryption" - store the value XORed with a
// compile-time key and recover it at runtime. A `volatile` store of the
// ciphertext defeats the compiler's constant folding so the cleartext value is
// not visible in the disassembly as an immediate.
#pragma once

#include <cstdint>
#include <type_traits>

#include "random.hpp"

namespace xobf {

namespace detail {
// base_int<T> = T for integrals, or the enum's underlying type for enums.
// Written so underlying_type is only instantiated when T really is an enum.
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

    volatile U enc_;

    static constexpr U mask() {
        return static_cast<U>(detail::splitmix64(K));
    }

public:
    explicit obf_const(T v) : enc_(static_cast<U>(v) ^ mask()) {}

    T get() const { return static_cast<T>(static_cast<U>(enc_) ^ mask()); }
    operator T() const { return get(); }
};

}  // namespace xobf

// XOBF_INT(value): an encrypted integer constant. Decays to its value via
// operator T(). Key is expanded once.
#define XOBF_INT(value)                                                     \
    ([&]() {                                                                \
        constexpr std::uint64_t xobf_k = XOBF_KEY;                          \
        using xobf_t = std::decay_t<decltype(value)>;                       \
        return ::xobf::obf_const<xobf_t, xobf_k>(value);                    \
    }())
