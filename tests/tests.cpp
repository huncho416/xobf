// Self-contained correctness tests for xobf. Exits non-zero on first failure.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

#include "xobf/xobf.hpp"

static int g_failures = 0;
#define CHECK(cond)                                                      \
    do {                                                                 \
        if (!(cond)) {                                                   \
            std::printf("FAIL: %s  (line %d)\n", #cond, __LINE__);       \
            ++g_failures;                                                \
        }                                                                \
    } while (0)

template <typename T>
static void test_mba_type() {
    const T vals[] = {T(0),
                      T(1),
                      T(2),
                      T(37),
                      T(255),
                      T(-1),
                      T(12345),
                      std::numeric_limits<T>::max(),
                      std::numeric_limits<T>::min()};
    for (T a : vals)
        for (T b : vals) {
            using U = std::make_unsigned_t<T>;
            CHECK(xobf::mba::add(a, b) == (T)(U(a) + U(b)));
            CHECK(xobf::mba::sub(a, b) == (T)(U(a) - U(b)));
            CHECK(xobf::mba::xor_(a, b) == (T)(a ^ b));
            CHECK(xobf::mba::and_(a, b) == (T)(a & b));
            CHECK(xobf::mba::or_(a, b) == (T)(a | b));
        }
}

int main() {
    // --- String encryption round-trips ---
    auto& s1 = XOBF_STR("hello world");
    CHECK(s1.is_encrypted());                 // not yet decrypted
    CHECK(std::strcmp(s1.decrypt(), "hello world") == 0);
    CHECK(!s1.is_encrypted());
    s1.scrub();
    CHECK(s1.is_encrypted());
    CHECK(std::strcmp(s1.decrypt(), "hello world") == 0);  // idempotent redecrypt

    auto& s2 = XOBF_STR("another DISTINCT secret 1234");
    CHECK(std::strcmp(s2.decrypt(), "another DISTINCT secret 1234") == 0);
    CHECK(s1.size() == 11);

    const char* as_ptr = XOBF_STR("implicit");  // operator const char*
    CHECK(std::strcmp(as_ptr, "implicit") == 0);

    // --- Constant encryption ---
    int port = XOBF_INT(8080);
    CHECK(port == 8080);
    std::uint64_t big = XOBF_INT((std::uint64_t)0xDEADBEEFCAFEull);
    CHECK(big == 0xDEADBEEFCAFEull);
    char c = XOBF_INT((char)'Z');
    CHECK(c == 'Z');

    // --- MBA substitution correctness across widths ---
    test_mba_type<std::int32_t>();
    test_mba_type<std::uint32_t>();
    test_mba_type<std::int64_t>();
    test_mba_type<std::uint64_t>();
    test_mba_type<std::int16_t>();
    test_mba_type<std::uint8_t>();

    // --- Opaque predicates ---
    CHECK(xobf::opaque_true() == true);
    CHECK(xobf::opaque_false() == false);
    int reached = 0;
    XOBF_BOGUS(reached = 1);
    CHECK(reached == 1);
    int dead = 0;
    XOBF_DEAD(dead = 1);
    CHECK(dead == 0);

    if (g_failures == 0) {
        std::puts("ALL TESTS PASSED");
        return 0;
    }
    std::printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}
