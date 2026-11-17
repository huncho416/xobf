#pragma once

#include <cstddef>
#include <cstdint>

#include "random.hpp"

namespace xobf {

template <std::size_t N, std::uint64_t K>
struct cipher {
    unsigned char data[N];

    constexpr cipher(const char (&s)[N]) : data{} {
        for (std::size_t i = 0; i < N; ++i)
            data[i] = static_cast<unsigned char>(s[i]) ^ detail::keybyte<K>(i);
    }
};

template <std::size_t N, std::uint64_t K>
class secret {
    char buf_[N];
    bool encrypted_ = true;

public:
    explicit secret(const cipher<N, K>& c) : buf_{} {
        for (std::size_t i = 0; i < N; ++i)
            buf_[i] = static_cast<char>(c.data[i]);
    }

    char* decrypt() {
        if (encrypted_) {
            for (std::size_t i = 0; i < N; ++i)
                buf_[i] = static_cast<char>(static_cast<unsigned char>(buf_[i]) ^
                                            detail::keybyte<K>(i));
            encrypted_ = false;
        }
        return buf_;
    }

    void scrub() {
        if (!encrypted_) {
            for (std::size_t i = 0; i < N; ++i)
                buf_[i] = static_cast<char>(static_cast<unsigned char>(buf_[i]) ^
                                            detail::keybyte<K>(i));
            encrypted_ = true;
        }
    }

    const char* get() { return decrypt(); }
    operator const char*() { return decrypt(); }
    constexpr std::size_t size() const { return N - 1; }
    bool is_encrypted() const { return encrypted_; }
};

template <std::uint64_t K, std::size_t N>
constexpr cipher<N, K> make_cipher(const char (&s)[N]) {
    return cipher<N, K>(s);
}

}  // namespace xobf

#define XOBF_STR(str)                                                  \
    ([]() -> auto& {                                                   \
        constexpr std::uint64_t xobf_k = XOBF_KEY;                     \
        constexpr std::size_t xobf_n = sizeof(str) / sizeof((str)[0]); \
        constexpr ::xobf::cipher<xobf_n, xobf_k> xobf_c(str);          \
        static ::xobf::secret<xobf_n, xobf_k> xobf_s(xobf_c);          \
        return xobf_s;                                                 \
    }())
