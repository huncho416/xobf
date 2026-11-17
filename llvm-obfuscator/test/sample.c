#include <stdio.h>

__attribute__((noinline)) int compute(int a, int b) {
    int s = a + b;      // -> MBA
    int d = a - b;      // -> MBA
    int x = a ^ b;      // -> MBA
    int m = (a & b) | (s ^ d);
    for (int i = 0; i < b; ++i)   // loop -> bogus control flow on blocks
        m += (i * 3) - 1;
    return s + d + x + m;
}

int main(void) {
    int acc = 0;
    for (int i = 1; i <= 10; ++i)
        acc += compute(i * 7, i + 2);
    printf("result=%d\n", acc);
    return 0;
}
