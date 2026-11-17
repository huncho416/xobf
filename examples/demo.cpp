#include <cstdio>

#include "xobf/xobf.hpp"

XOBF_NOINLINE static void licensed_feature() {
    std::puts(XOBF_STR("PREMIUM_FEATURE_UNLOCKED_MARKER").decrypt());
}

static int checked_add(int a, int b) { return a + b; }

static long flat_triangle(int n) {
    long acc = 0;
    int i = 1;
    XOBF_FLATTEN_BEGIN(1)
    XOBF_CASE(1)
    XOBF_GOTO_IF(i > n, 3)
    XOBF_GOTO(2)
    XOBF_CASE(2)
    acc += static_cast<long>(XOBF_OPAQUE(i));
    XOBF_JUNK();
    ++i;
    XOBF_GOTO(1)
    XOBF_CASE(3)
    XOBF_FLATTEN_EXIT_TO(flat_triangle_done);
    XOBF_FLATTEN_END(flat_triangle_done)
    return acc;
}

static long dflat_factorial(int n) {
    long acc = 1;
    int i = 1;
    XOBF_DFLATTEN_BEGIN(1)
    XOBF_DCASE(1)
    XOBF_DGOTO_IF(i > n, 3)
    XOBF_DGOTO(2)
    XOBF_DCASE(2)
    acc *= static_cast<long>(XOBF_OPAQUE(i));
    XOBF_JUNK();
    ++i;
    XOBF_DGOTO(1)
    XOBF_DCASE(3)
    XOBF_DFLATTEN_EXIT_TO(dflat_factorial_done);
    XOBF_DFLATTEN_END(dflat_factorial_done)
    return acc;
}

int main() {
    std::printf("%s\n", XOBF_STR("SUPER_SECRET_API_TOKEN_MARKER").decrypt());

    int magic = XOBF_INT(1337);
    std::printf("magic=%d\n", magic);

    int vm_port = XOBF_VM_INT(443);
    std::printf("vm_port=%d\n", vm_port);

    {
        auto stacked = XOBF_STR_STACK("STACK_ONLY_SECRET_MARKER");
        std::printf("%s\n", stacked.c_str());
    }

    {
        auto shortstk = XOBF_STR_STACK("SHORTMRK");
        std::printf("%s\n", shortstk.c_str());
    }

    {
        auto vmstr = XOBF_VMSTR("VM_STRING_SECRET_MARKER");
        std::printf("%s\n", vmstr.c_str());
    }

    int a = magic, b = 42;
    std::printf("a+b=%d a-b=%d a^b=%d\n", xobf::mba::add(a, b),
                xobf::mba::sub(a, b), xobf::mba::xor_(a, b));

    {
        auto hidden = XOBF_HIDDEN(&checked_add);
        std::printf("hidden_call=%d\n", hidden(magic, 63));
    }

    std::printf("triangle(10)=%ld\n", flat_triangle(10));
    std::printf("factorial(6)=%ld\n", dflat_factorial(6));

    {
        static const unsigned char region[] = {0x10, 0x20, 0x30, 0x40};
        std::printf("checksum_ok=%d\n",
                    XOBF_CHECKSUM(region, sizeof(region)) ==
                        XOBF_CHECKSUM(region, sizeof(region)));
    }

    std::printf("under_analysis=%d\n", xobf::under_analysis() ? 1 : 0);

    XOBF_BOGUS(licensed_feature());

    XOBF_DEAD(std::puts(XOBF_STR("THIS_SHOULD_NEVER_PRINT_MARKER").decrypt()));

    return 0;
}
