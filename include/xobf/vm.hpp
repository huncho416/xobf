#pragma once

#include <cstddef>
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

#ifndef XOBF_VM_STR_MAXLEN
#define XOBF_VM_STR_MAXLEN 16
#endif

inline constexpr int VM_STR_MAXLEN = XOBF_VM_STR_MAXLEN;
inline constexpr int VM_STR_MINSTEPS = 6;
inline constexpr int VM_STR_STEPSPAN = 6;

template <int MAXLEN>
struct program_t {
    std::uint8_t ops[MAXLEN];
    std::uint64_t args[MAXLEN];
    int len;
};

using program = program_t<VM_MAXLEN>;
using cprogram = program_t<VM_STR_MAXLEN>;

constexpr std::uint64_t vm_stream(std::uint64_t K, int i) {
    return ::xobf::detail::splitmix64(
        K ^ (0x100000001b3ull * (static_cast<std::uint64_t>(i) + 1ull)));
}

template <int MAXLEN>
constexpr program_t<MAXLEN> build_n(std::uint64_t V, std::uint64_t K,
                                    int minsteps, int stepspan) {
    program_t<MAXLEN> p{};
    ::xobf::detail::ctprng rng(K | 1ull);
    std::uint64_t acc = VM_SEED0;
    int n = 0;
    int steps =
        minsteps + (stepspan > 0
                        ? static_cast<int>(rng.bounded(
                              static_cast<std::uint64_t>(stepspan)))
                        : 0);
    int cap = MAXLEN - 2;
    if (steps > cap)
        steps = cap;
    if (steps < 0)
        steps = 0;
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

constexpr program build(std::uint64_t V, std::uint64_t K) {
    return build_n<VM_MAXLEN>(V, K, 10, 10);
}

constexpr cprogram build_str(std::uint64_t V, std::uint64_t K) {
    return build_n<VM_STR_MAXLEN>(V, K, VM_STR_MINSTEPS, VM_STR_STEPSPAN);
}

template <std::uint64_t V, std::uint64_t K>
struct prog {
    static constexpr program value = build(V, K);
};

template <typename P>
XOBF_NOINLINE std::uint64_t interpret(const P& p, std::uint64_t K) {
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

template <std::uint64_t V, std::uint64_t K>
XOBF_NOINLINE std::uint64_t run() {
    return interpret(prog<V, K>::value, K);
}

constexpr std::uint64_t vmstr_key(std::uint64_t K, std::size_t b) {
    return ::xobf::detail::splitmix64(
        K ^ (0x9e3779b97f4a7c15ull * (static_cast<std::uint64_t>(b) + 1ull)));
}

template <std::size_t N, std::uint64_t K>
struct vm_cipher {
    static constexpr std::size_t NB = (N + 7) / 8;
    cprogram progs[NB];
    std::uint64_t keys[NB];

    constexpr vm_cipher(const char (&s)[N]) : progs{}, keys{} {
        for (std::size_t b = 0; b < NB; ++b) {
            std::uint64_t pv = 0;
            for (std::size_t j = 0; j < 8; ++j) {
                std::size_t idx = b * 8 + j;
                std::uint64_t c =
                    idx < N ? static_cast<std::uint64_t>(
                                  static_cast<unsigned char>(s[idx]))
                            : 0ull;
                pv |= c << (8 * j);
            }
            std::uint64_t kb = vmstr_key(K, b);
            keys[b] = kb;
            cprogram pr = build_str(pv, kb);
            for (int i = 0; i < VM_STR_MAXLEN; ++i) {
                progs[b].ops[i] = pr.ops[i];
                progs[b].args[i] = pr.args[i];
            }
            progs[b].len = pr.len;
        }
    }
};

template <std::size_t N, std::uint64_t K>
class vm_secret {
    char buf_[N];

public:
    explicit vm_secret(const vm_cipher<N, K>& c) : buf_{} {
        for (std::size_t b = 0; b < vm_cipher<N, K>::NB; ++b) {
            std::uint64_t pv = interpret(c.progs[b], c.keys[b]);
            for (std::size_t j = 0; j < 8; ++j) {
                std::size_t idx = b * 8 + j;
                if (idx < N)
                    buf_[idx] =
                        static_cast<char>((pv >> (8 * j)) & 0xFFull);
            }
        }
    }

    ~vm_secret() {
        volatile char* p = buf_;
        for (std::size_t i = 0; i < N; ++i)
            p[i] = 0;
    }

    const char* c_str() const { return buf_; }
    operator const char*() const { return buf_; }
    constexpr std::size_t size() const { return N - 1; }
};

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

#define XOBF_VMSTR(str)                                                     \
    ([]() {                                                                 \
        constexpr std::uint64_t xobf_k = XOBF_KEY;                         \
        constexpr std::size_t xobf_n = sizeof(str) / sizeof((str)[0]);     \
        constexpr ::xobf::vm::vm_cipher<xobf_n, xobf_k> xobf_c(str);       \
        return ::xobf::vm::vm_secret<xobf_n, xobf_k>(xobf_c);              \
    }())
