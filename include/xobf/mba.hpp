// xobf/mba.hpp : Mixed Boolean-Arithmetic (MBA) substitution.
//
// Research basis: replace plain arithmetic with equivalent but more complex
// expressions mixing bitwise and arithmetic ops (e.g. binprotect's
// x + y == (x & y) + (x | y)). All identities below are exact for fixed-width
// two's-complement integers (wrapping), and are unit-tested in tests/.
#pragma once

#include <type_traits>

namespace xobf::mba {

// x + y  ==  (x ^ y) + 2*(x & y)   (full-adder identity)
template <typename T>
inline T add(T x, T y) {
    static_assert(std::is_integral_v<T>);
    using U = std::make_unsigned_t<T>;
    U ux = static_cast<U>(x), uy = static_cast<U>(y);
    return static_cast<T>((ux ^ uy) + 2u * (ux & uy));
}

// x - y  ==  add(x, -y)   (-y via two's complement: ~y + 1)
template <typename T>
inline T sub(T x, T y) {
    using U = std::make_unsigned_t<T>;
    return add<T>(x, static_cast<T>(static_cast<U>(~static_cast<U>(y)) + 1u));
}

// x ^ y  ==  (x | y) - (x & y)
template <typename T>
inline T xor_(T x, T y) {
    using U = std::make_unsigned_t<T>;
    U ux = static_cast<U>(x), uy = static_cast<U>(y);
    return static_cast<T>((ux | uy) - (ux & uy));
}

// x & y  ==  (x + y) - (x | y)
template <typename T>
inline T and_(T x, T y) {
    using U = std::make_unsigned_t<T>;
    U ux = static_cast<U>(x), uy = static_cast<U>(y);
    return static_cast<T>((ux + uy) - (ux | uy));
}

// x | y  ==  (x & y) + (x ^ y)
template <typename T>
inline T or_(T x, T y) {
    using U = std::make_unsigned_t<T>;
    U ux = static_cast<U>(x), uy = static_cast<U>(y);
    return static_cast<T>((ux & uy) + (ux ^ uy));
}

}  // namespace xobf::mba
