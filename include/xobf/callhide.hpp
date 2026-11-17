#pragma once

#include <cstdint>
#include <type_traits>

#include "config.hpp"
#include "inline.hpp"
#include "random.hpp"

namespace xobf {

template <typename Fn, std::uint64_t K>
class hidden_fn {
    static_assert(std::is_pointer_v<Fn>);

    static constexpr int slots = XOBF_VAULT_SLOTS < 1 ? 1 : XOBF_VAULT_SLOTS;
    static constexpr int real =
        static_cast<int>((K >> 7) % static_cast<std::uint64_t>(slots));

    volatile std::uintptr_t tbl_[slots];

    static std::uintptr_t mask() {
        return static_cast<std::uintptr_t>(
            detail::splitmix64(K ^ 0x5151515151515151ull));
    }

    static constexpr std::uintptr_t decoy(int i) {
        return static_cast<std::uintptr_t>(detail::splitmix64(
            K ^ (0x9999999999999999ull * static_cast<std::uint64_t>(i + 1))));
    }

public:
    explicit hidden_fn(Fn f) : tbl_{} {
        for (int i = 0; i < slots; ++i)
            tbl_[i] = decoy(i);
        tbl_[real] = reinterpret_cast<std::uintptr_t>(f) ^ mask();
    }

    XOBF_NOINLINE Fn get() const {
        std::uintptr_t p = static_cast<std::uintptr_t>(tbl_[real]) ^ mask();
        std::uint64_t s = detail::opaque_seed;
        std::uintptr_t z = static_cast<std::uintptr_t>((s * (s + 1ull)) & 1ull);
        return reinterpret_cast<Fn>(p + z - z);
    }

    template <typename... A>
    auto operator()(A... a) const {
        return get()(a...);
    }
};

}  // namespace xobf

#define XOBF_HIDE_FN(f)                               \
    ([&]() {                                          \
        constexpr std::uint64_t xobf_k = XOBF_KEY;    \
        using xobf_fn = std::decay_t<decltype(f)>;    \
        return ::xobf::hidden_fn<xobf_fn, xobf_k>(f); \
    }())
