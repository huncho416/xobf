#pragma once

#include <cstddef>
#include <cstdint>

#include "config.hpp"
#include "random.hpp"

namespace xobf {

namespace detail {

constexpr unsigned char rotl8(unsigned char v, unsigned s) {
    s &= 7u;
    return s == 0 ? v
                  : static_cast<unsigned char>((v << s) | (v >> (8u - s)));
}

constexpr unsigned char rotr8(unsigned char v, unsigned s) {
    s &= 7u;
    return s == 0 ? v
                  : static_cast<unsigned char>((v >> s) | (v << (8u - s)));
}

constexpr unsigned char strksb(std::uint64_t K, std::size_t i, int r) {
    return static_cast<unsigned char>(
        splitmix64(K ^ (0x9e3779b97f4a7c15ull *
                        (static_cast<std::uint64_t>(i) + 1ull)) ^
                   (0x100000001b3ull * static_cast<std::uint64_t>(r + 1))) &
        0xFFull);
}

constexpr unsigned stropsel(std::uint64_t K, int r) {
    return static_cast<unsigned>(
        splitmix64(K ^ (0xc2b2ae3d27d4eb4full *
                        static_cast<std::uint64_t>(r + 1))) %
        3ull);
}

constexpr unsigned char str_enc(unsigned char b, std::uint64_t K,
                                std::size_t i, int r, unsigned op) {
    unsigned char k = strksb(K, i, r);
    if (op == 0)
        return static_cast<unsigned char>(b ^ k);
    else if (op == 1)
        return static_cast<unsigned char>((b + k) & 0xFF);
    else
        return rotl8(b, k & 7u);
}

constexpr unsigned char str_dec(unsigned char b, std::uint64_t K,
                                std::size_t i, int r, unsigned op) {
    unsigned char k = strksb(K, i, r);
    if (op == 0)
        return static_cast<unsigned char>(b ^ k);
    else if (op == 1)
        return static_cast<unsigned char>((b - k) & 0xFF);
    else
        return rotr8(b, k & 7u);
}

constexpr std::uint32_t str_summask(std::uint64_t K) {
    return static_cast<std::uint32_t>(splitmix64(K ^ 0x5a5a5a5a5a5a5a5aull));
}

}  // namespace detail

template <std::size_t N, std::uint64_t K, int R>
struct cipher {
    unsigned char data[N];
    std::uint32_t sum;

    constexpr cipher(const char (&s)[N]) : data{}, sum(0) {
        std::uint32_t h = 2166136261u;
        for (std::size_t i = 0; i < N; ++i) {
            unsigned char b = static_cast<unsigned char>(s[i]);
            h = (h ^ b) * 16777619u;
            data[i] = b;
        }
        sum = h ^ detail::str_summask(K);
        for (int r = 0; r < R; ++r) {
            unsigned op = detail::stropsel(K, r);
            for (std::size_t i = 0; i < N; ++i)
                data[i] = detail::str_enc(data[i], K, i, r, op);
        }
    }
};

template <std::size_t N, std::uint64_t K, int R>
class secret {
    char buf_[N];
    bool encrypted_ = true;
    bool tampered_ = false;
    std::uint32_t sum_;

public:
    explicit secret(const cipher<N, K, R>& c) : buf_{}, sum_(c.sum) {
        for (std::size_t i = 0; i < N; ++i)
            buf_[i] = static_cast<char>(c.data[i]);
    }

    char* decrypt() {
        if (encrypted_) {
            for (int r = R - 1; r >= 0; --r) {
                unsigned op = detail::stropsel(K, r);
                for (std::size_t i = 0; i < N; ++i)
                    buf_[i] = static_cast<char>(detail::str_dec(
                        static_cast<unsigned char>(buf_[i]), K, i, r, op));
            }
            encrypted_ = false;
            std::uint32_t h = 2166136261u;
            for (std::size_t i = 0; i < N; ++i)
                h = (h ^ static_cast<unsigned char>(buf_[i])) * 16777619u;
            if ((h ^ detail::str_summask(K)) != sum_) {
                tampered_ = true;
                for (std::size_t i = 0; i < N; ++i)
                    buf_[i] = 0;
            }
        }
        return buf_;
    }

    void scrub() {
        if (!encrypted_) {
            for (int r = 0; r < R; ++r) {
                unsigned op = detail::stropsel(K, r);
                for (std::size_t i = 0; i < N; ++i)
                    buf_[i] = static_cast<char>(detail::str_enc(
                        static_cast<unsigned char>(buf_[i]), K, i, r, op));
            }
            encrypted_ = true;
        }
    }

    const char* get() { return decrypt(); }
    operator const char*() { return decrypt(); }
    constexpr std::size_t size() const { return N - 1; }
    bool is_encrypted() const { return encrypted_; }
    bool verified() const { return !tampered_; }
};

template <std::size_t N, std::uint64_t K, int R>
class stack_secret {
    char buf_[N];
    bool tampered_ = false;

public:
    explicit stack_secret(const cipher<N, K, R>& c) : buf_{} {
        const volatile unsigned char* src = c.data;
        for (std::size_t i = 0; i < N; ++i)
            buf_[i] = static_cast<char>(src[i]);
        for (int r = R - 1; r >= 0; --r) {
            unsigned op = detail::stropsel(K, r);
            for (std::size_t i = 0; i < N; ++i)
                buf_[i] = static_cast<char>(detail::str_dec(
                    static_cast<unsigned char>(buf_[i]), K, i, r, op));
        }
        std::uint32_t h = 2166136261u;
        for (std::size_t i = 0; i < N; ++i)
            h = (h ^ static_cast<unsigned char>(buf_[i])) * 16777619u;
        if ((h ^ detail::str_summask(K)) != c.sum) {
            tampered_ = true;
            for (std::size_t i = 0; i < N; ++i)
                buf_[i] = 0;
        }
    }

    ~stack_secret() {
        volatile char* p = buf_;
        for (std::size_t i = 0; i < N; ++i)
            p[i] = 0;
    }

    const char* c_str() const { return buf_; }
    operator const char*() const { return buf_; }
    constexpr std::size_t size() const { return N - 1; }
    bool verified() const { return !tampered_; }
};

template <std::uint64_t K, int R, std::size_t N>
constexpr cipher<N, K, R> make_cipher(const char (&s)[N]) {
    return cipher<N, K, R>(s);
}

}  // namespace xobf

#define XOBF_STR(str)                                                  \
    ([]() -> auto& {                                                   \
        constexpr std::uint64_t xobf_k = XOBF_KEY;                     \
        constexpr std::size_t xobf_n = sizeof(str) / sizeof((str)[0]); \
        constexpr int xobf_r = XOBF_STR_ROUNDS;                        \
        constexpr ::xobf::cipher<xobf_n, xobf_k, xobf_r> xobf_c(str);  \
        static ::xobf::secret<xobf_n, xobf_k, xobf_r> xobf_s(xobf_c);  \
        return xobf_s;                                                 \
    }())

#define XOBF_STR_STACK(str)                                            \
    ([]() {                                                            \
        constexpr std::uint64_t xobf_k = XOBF_KEY;                     \
        constexpr std::size_t xobf_n = sizeof(str) / sizeof((str)[0]); \
        constexpr int xobf_r = XOBF_STR_ROUNDS;                        \
        constexpr ::xobf::cipher<xobf_n, xobf_k, xobf_r> xobf_c(str);  \
        return ::xobf::stack_secret<xobf_n, xobf_k, xobf_r>(xobf_c);   \
    }())
