// demo.cpp : shows every xobf primitive and is used to prove (via `strings`)
// that the plaintext secrets do not appear in the compiled binary.
#include <cstdio>

#include "xobf/xobf.hpp"

XOBF_NOINLINE static void licensed_feature() {
    std::puts(XOBF_STR("PREMIUM_FEATURE_UNLOCKED_MARKER").decrypt());
}

int main() {
    // Encrypted string literals (plaintext never stored in the binary).
    std::printf("%s\n", XOBF_STR("SUPER_SECRET_API_TOKEN_MARKER").decrypt());

    // Encrypted integer constant (no plaintext immediate in the disassembly).
    int magic = XOBF_INT(1337);
    std::printf("magic=%d\n", magic);

    // MBA-substituted arithmetic.
    int a = magic, b = 42;
    std::printf("a+b=%d a-b=%d a^b=%d\n", xobf::mba::add(a, b),
                xobf::mba::sub(a, b), xobf::mba::xor_(a, b));

    // Opaque-predicate-guarded control flow.
    XOBF_BOGUS(licensed_feature());

    // Dead branch: never runs, but visible to naive static analysis.
    XOBF_DEAD(std::puts(XOBF_STR("THIS_SHOULD_NEVER_PRINT_MARKER").decrypt()));

    return 0;
}
