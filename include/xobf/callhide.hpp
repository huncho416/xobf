#pragma once

#include <cstdint>
#include <utility>

#include "config.hpp"
#include "inline.hpp"
#include "random.hpp"

namespace xobf {

template <typename Fn, std::uint64_t K>
class hidden_fn;

template <typename R, typename... A, std::uint64_t K>
class hidden_fn<R (*)(A...), K> {
    using fn_t = R (*)(A...);

    static constexpr int slots = XOBF_VAULT_SLOTS < 2 ? 2 : XOBF_VAULT_SLOTS;
    static constexpr int real =
        static_cast<int>((K >> 7) % static_cast<std::uint64_t>(slots));

    volatile std::uintptr_t table_[slots];

    static constexpr std::uintptr_t mask() {
        std::uint64_t m = detail::splitmix64(K ^ 0x5151baff5151baffull);
        if (sizeof(std::uintptr_t) > 4)
            m ^= detail::splitmix64(m) << 1;
        return static_cast<std::uintptr_t>(m);
    }

    static constexpr std::uintptr_t decoy(int i) {
        return static_cast<std::uintptr_t>(detail::splitmix64(
            K ^ (0x27d4eb2f165667c5ull * static_cast<std::uint64_t>(i + 3))));
    }

    XOBF_NOINLINE fn_t resolve() const {
        std::uintptr_t e = table_[real];
        std::uintptr_t p = e ^ mask();
        return reinterpret_cast<fn_t>(p);
    }

public:
    explicit hidden_fn(fn_t f) : table_{} {
        for (int i = 0; i < slots; ++i)
            table_[i] = decoy(i);
        table_[real] = reinterpret_cast<std::uintptr_t>(f) ^ mask();
    }

    template <typename... Args>
    R operator()(Args&&... args) const {
        return resolve()(std::forward<Args>(args)...);
    }

    fn_t get() const { return resolve(); }
};

template <std::uint64_t K, typename R, typename... A>
hidden_fn<R (*)(A...), K> make_hidden(R (*f)(A...)) {
    return hidden_fn<R (*)(A...), K>(f);
}

}  // namespace xobf

#define XOBF_HIDDEN(fn)                               \
    ([&]() {                                          \
        constexpr std::uint64_t xobf_k = XOBF_KEY;    \
        return ::xobf::make_hidden<xobf_k>(fn);       \
    }())
