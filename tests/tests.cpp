#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

#include "xobf/xobf.hpp"

static int g_failures = 0;
#define CHECK(cond)                                                \
    do {                                                           \
        if (!(cond)) {                                             \
            std::printf("FAIL: %s  (line %d)\n", #cond, __LINE__); \
            ++g_failures;                                          \
        }                                                          \
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
            CHECK((xobf::mba::add_poly<0x11, T>(a, b)) == (T)(U(a) + U(b)));
            CHECK((xobf::mba::add_poly<0x12, T>(a, b)) == (T)(U(a) + U(b)));
            CHECK((xobf::mba::add_poly<0x13, T>(a, b)) == (T)(U(a) + U(b)));
            CHECK((xobf::mba::sub_poly<0x21, T>(a, b)) == (T)(U(a) - U(b)));
            CHECK((xobf::mba::sub_poly<0x22, T>(a, b)) == (T)(U(a) - U(b)));
            CHECK((xobf::mba::sub_poly<0x23, T>(a, b)) == (T)(U(a) - U(b)));
            CHECK((xobf::mba::xor_poly<0x31, T>(a, b)) == (T)(a ^ b));
            CHECK((xobf::mba::xor_poly<0x32, T>(a, b)) == (T)(a ^ b));
        }
}

template <typename T>
static void test_mix_type() {
    const T vals[] = {T(0),
                      T(1),
                      T(7),
                      T(255),
                      T(-1),
                      T(4242),
                      std::numeric_limits<T>::max(),
                      std::numeric_limits<T>::min()};
    for (T a : vals) {
        CHECK((xobf::mba::mix<0x1, 0, T>(a)) == a);
        CHECK((xobf::mba::mix<0x2, 1, T>(a)) == a);
        CHECK((xobf::mba::mix<0x3, 3, T>(a)) == a);
        CHECK((xobf::mba::mix<0xdeadbeef, 5, T>(a)) == a);
        CHECK((xobf::mba::mix<0xcafef00d, 7, T>(a)) == a);
    }
}

int main() {
    auto& s1 = XOBF_STR("hello world");
    CHECK(s1.is_encrypted());
    CHECK(std::strcmp(s1.decrypt(), "hello world") == 0);
    CHECK(!s1.is_encrypted());
    s1.scrub();
    CHECK(s1.is_encrypted());
    CHECK(std::strcmp(s1.decrypt(), "hello world") == 0);

    auto& s2 = XOBF_STR("another DISTINCT secret 1234");
    CHECK(std::strcmp(s2.decrypt(), "another DISTINCT secret 1234") == 0);
    CHECK(s1.size() == 11);

    const char* as_ptr = XOBF_STR("implicit");
    CHECK(std::strcmp(as_ptr, "implicit") == 0);

    int port = XOBF_INT(8080);
    CHECK(port == 8080);
    std::uint64_t big = XOBF_INT((std::uint64_t)0xDEADBEEFCAFEull);
    CHECK(big == 0xDEADBEEFCAFEull);
    char c = XOBF_INT((char)'Z');
    CHECK(c == 'Z');
    int neg = XOBF_INT(-31337);
    CHECK(neg == -31337);
    unsigned u = XOBF_INT((unsigned)0xFFFFFFFFu);
    CHECK(u == 0xFFFFFFFFu);

    test_mba_type<std::int32_t>();
    test_mba_type<std::uint32_t>();
    test_mba_type<std::int64_t>();
    test_mba_type<std::uint64_t>();
    test_mba_type<std::int16_t>();
    test_mba_type<std::uint8_t>();

    test_mix_type<std::int32_t>();
    test_mix_type<std::uint32_t>();
    test_mix_type<std::int64_t>();
    test_mix_type<std::uint64_t>();
    test_mix_type<std::uint8_t>();

    CHECK(xobf::opaque_true() == true);
    CHECK(xobf::opaque_false() == false);
    CHECK((xobf::opaque_true<0>()) == true);
    CHECK((xobf::opaque_true<1>()) == true);
    CHECK((xobf::opaque_true<2>()) == true);
    CHECK((xobf::opaque_true<3>()) == true);
    CHECK((xobf::opaque_false<0>()) == false);
    CHECK((xobf::opaque_false<1>()) == false);
    CHECK((xobf::opaque_false<2>()) == false);
    CHECK((xobf::opaque_false<3>()) == false);
    CHECK((xobf::opaque_true_hashed<0x55>()) == true);
    CHECK((xobf::opaque_true_explode<0x77>()) == true);

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
