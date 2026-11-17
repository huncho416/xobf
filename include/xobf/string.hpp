// xobf/string.hpp : compile-time string literal encryption.
//
// Research basis: "constant/string encryption" - the plaintext is encrypted at
// compile time (so it never appears in the binary) and decrypted on demand at
// runtime, then scrubbed to minimise the plaintext's lifetime in memory.
//
// Design: the plaintext literal is consumed entirely inside a `constexpr`
// context (the `cipher` object), which prevents the compiler from emitting it
// into .rodata. A separate runtime `secret` object holds only ciphertext and
// decrypts into its own buffer.
#pragma once

#include <cstddef>
#include <cstdint>

#include "random.hpp"

namespace xobf {

// Immutable, compile-time-encrypted blob. Constructed in a constexpr context.
template <std::size_t N, std::uint64_t K>
struct cipher {
    unsigned char data[N];

    constexpr cipher(const char (&s)[N]) : data{} {
        for (std::size_t i = 0; i < N; ++i)
            data[i] = static_cast<unsigned char>(s[i]) ^ detail::keybyte<K>(i);
    }
};

// Runtime holder: stores ciphertext, decrypts/encrypts in place on demand.
template <std::size_t N, std::uint64_t K>
class secret {
    char buf_[N];
    bool encrypted_ = true;

public:
    explicit secret(const cipher<N, K>& c) : buf_{} {
        for (std::size_t i = 0; i < N; ++i)
            buf_[i] = static_cast<char>(c.data[i]);
    }

    // Decrypt in place (idempotent) and return the plaintext pointer.
    char* decrypt() {
        if (encrypted_) {
            for (std::size_t i = 0; i < N; ++i)
                buf_[i] = static_cast<char>(static_cast<unsigned char>(buf_[i]) ^
                                            detail::keybyte<K>(i));
            encrypted_ = false;
        }
        return buf_;
    }

    // Re-encrypt in place to minimise how long plaintext stays in memory.
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
    constexpr std::size_t size() const { return N - 1; }  // excludes NUL
    bool is_encrypted() const { return encrypted_; }
};

template <std::uint64_t K, std::size_t N>
constexpr cipher<N, K> make_cipher(const char (&s)[N]) {
    return cipher<N, K>(s);
}

}  // namespace xobf

// XOBF_STR("literal") -> xobf::secret&  (call .decrypt()/.get() or use as const char*)
//
// The constexpr `xobf_c` forces compile-time encryption (keeping the plaintext
// out of the binary); the static `xobf_s` holds only ciphertext at runtime and
// decrypts lazily on first use. XOBF_KEY is expanded exactly once so the cipher
// and secret share the same key.
#define XOBF_STR(str)                                                       \
    ([]() -> auto& {                                                        \
        constexpr std::uint64_t xobf_k = XOBF_KEY;                          \
        constexpr std::size_t xobf_n = sizeof(str) / sizeof((str)[0]);      \
        constexpr ::xobf::cipher<xobf_n, xobf_k> xobf_c(str);               \
        static ::xobf::secret<xobf_n, xobf_k> xobf_s(xobf_c);               \
        return xobf_s;                                                      \
    }())
