#pragma once

#include <cstdint>
#include <type_traits>

#include "inline.hpp"
#include "random.hpp"

namespace xobf::vm {

enum : std::uint8_t {
    OP_XOR = 0,
    OP_ADD = 1,
    OP_SUB = 2,
    OP_ROL = 3,
    OP_MUL = 4,
    OP_OUT = 5
};

inline constexpr int VM_MAXLEN = 48;
inline constexpr std::uint64_t VM_SEED0 = 0x9e3779b97f4a7c15ull;

struct program {
    std::uint8_t ops[VM_MAXLEN];
    std::uint64_t args[VM_MAXLEN];
    int len;
};

constexpr std::uint64_t vm_stream(std::uint64_t K, int i) {
    return ::xobf::detail::splitmix64(
        K ^ (0x100000001b3ull * (static_cast<std::uint64_t>(i) + 1ull)));
}

constexpr program build(std::uint64_t V, std::uint64_t K) {
    program p{};
    ::xobf::detail::ctprng rng(K | 1ull);
    std::uint64_t acc = VM_SEED0;
    int n = 0;
    int steps = 10 + static_cast<int>(rng.bounded(10));
    for (int i = 0; i < steps; ++i) {
        std::uint64_t r = rng.next();
        unsigned c = static_cast<unsigned>(rng.bounded(5));
        if (c == 0) {
            acc ^= r;
            p.ops[n] = OP_XOR;
            p.args[n] = r;
        } else if (c == 1) {
            acc += r;
            p.ops[n] = OP_ADD;
            p.args[n] = r;
        } else if (c == 2) {
            acc -= r;
            p.ops[n] = OP_SUB;
            p.args[n] = r;
        } else if (c == 3) {
            unsigned s = static_cast<unsigned>(r & 63u);
            acc = ::xobf::detail::rotl<std::uint64_t>(acc, s);
            p.ops[n] = OP_ROL;
            p.args[n] = s;
        } else {
            std::uint64_t odd = r | 1ull;
            acc *= odd;
            p.ops[n] = OP_MUL;
            p.args[n] = odd;
        }
        ++n;
    }
    std::uint64_t fx = acc ^ V;
    p.ops[n] = OP_XOR;
    p.args[n] = fx;
    acc ^= fx;
    ++n;
    p.ops[n] = OP_OUT;
    p.args[n] = 0;
    ++n;
    p.len = n;
    for (int i = 0; i < n; ++i) {
        std::uint64_t ks = vm_stream(K, i);
        p.args[i] ^= ks;
        p.ops[i] = static_cast<std::uint8_t>(p.ops[i] ^
                                             static_cast<std::uint8_t>(ks & 0xFFu));
    }
    return p;
}

template <std::uint64_t V, std::uint64_t K>
struct prog {
    static constexpr program value = build(V, K);
};

template <std::uint64_t V, std::uint64_t K>
XOBF_NOINLINE std::uint64_t run() {
    const program& p = prog<V, K>::value;
    std::uint64_t acc = ::xobf::detail::opaque_seed;
    for (int i = 0; i < p.len; ++i) {
        std::uint64_t ks = vm_stream(K, i);
        std::uint8_t op = static_cast<std::uint8_t>(
            p.ops[i] ^ static_cast<std::uint8_t>(ks & 0xFFu));
        std::uint64_t arg = p.args[i] ^ ks;
        if (op == OP_XOR)
            acc ^= arg;
        else if (op == OP_ADD)
            acc += arg;
        else if (op == OP_SUB)
            acc -= arg;
        else if (op == OP_ROL)
            acc = ::xobf::detail::rotl<std::uint64_t>(
                acc, static_cast<unsigned>(arg & 63u));
        else if (op == OP_MUL)
            acc *= arg;
        else
            break;
    }
    return acc;
}

}  // namespace xobf::vm

#define XOBF_VM_INT(value)                                                 \
    ([]() {                                                                 \
        using xobf_t = std::decay_t<decltype(value)>;                      \
        using xobf_u = std::make_unsigned_t<xobf_t>;                       \
        constexpr std::uint64_t xobf_k = XOBF_KEY;                         \
        constexpr std::uint64_t xobf_v =                                   \
            static_cast<std::uint64_t>(static_cast<xobf_u>(value));        \
        return static_cast<xobf_t>(                                        \
            static_cast<xobf_u>(::xobf::vm::run<xobf_v, xobf_k>()));        \
    }())
