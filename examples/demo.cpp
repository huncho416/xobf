#include <cstdio>

#include "xobf/xobf.hpp"

XOBF_NOINLINE static void licensed_feature() {
    std::puts(XOBF_STR("PREMIUM_FEATURE_UNLOCKED_MARKER").decrypt());
}

int main() {
    std::printf("%s\n", XOBF_STR("SUPER_SECRET_API_TOKEN_MARKER").decrypt());

    int magic = XOBF_INT(1337);
    std::printf("magic=%d\n", magic);

    int vm_port = XOBF_VM_INT(443);
    std::printf("vm_port=%d\n", vm_port);

    int a = magic, b = 42;
    std::printf("a+b=%d a-b=%d a^b=%d\n", xobf::mba::add(a, b),
                xobf::mba::sub(a, b), xobf::mba::xor_(a, b));

    XOBF_BOGUS(licensed_feature());

    XOBF_DEAD(std::puts(XOBF_STR("THIS_SHOULD_NEVER_PRINT_MARKER").decrypt()));

    return 0;
}
