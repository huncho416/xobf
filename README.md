# xobf — compile-time C++ obfuscation library

A **zero-dependency, header-only** obfuscator you drop into any C++17 project. It
implements, in portable standard C++, the source-level techniques from the research
report (`cpp-obfuscation-research.md`): string encryption, constant encryption,
Mixed Boolean-Arithmetic substitution, and runtime-seeded opaque predicates / bogus
control flow.

Unlike a bin2bin or LLVM obfuscator, this needs no special toolchain — just
`#include` it and compile with any standard compiler.

## Quick start

```cpp
#include <xobf/xobf.hpp>
#include <cstdio>

int main() {
    // Encrypted string: plaintext is NOT present in the binary.
    std::puts(XOBF_STR("secret api token").decrypt());

    // Encrypted integer constant (no plaintext immediate in disassembly).
    int port = XOBF_INT(8080);

    // MBA-substituted arithmetic.
    int z = xobf::mba::add(port, 1);   // == port + 1, but obfuscated

    // Opaque-predicate-guarded control flow.
    XOBF_BOGUS(std::printf("port=%d z=%d\n", port, z));
}
```

Build: `g++ -std=c++17 -O2 -Iinclude your.cpp` (also tested with clang++).

## API

| Primitive | Usage | Effect |
|-----------|-------|--------|
| `XOBF_STR("s")` | returns `xobf::secret&`; call `.decrypt()`/`.get()` or use as `const char*` | string encrypted at compile time, decrypted lazily, `.scrub()` re-encrypts |
| `XOBF_INT(v)` | returns an `obf_const` that decays to the value | integer stored XOR-masked, recovered via a `volatile` to defeat constant folding |
| `xobf::mba::add/sub/xor_/and_/or_` | `xobf::mba::add(a,b)` | exact MBA identities, e.g. `a+b == (a^b)+2*(a&b)` |
| `XOBF_BOGUS(stmt...)` | statement wrapper | runs `stmt` behind an always-true opaque predicate with a bogus else-branch |
| `XOBF_DEAD(stmt...)` | statement wrapper | `stmt` on a provably-dead branch (never runs; confuses naive analysis) |
| `XOBF_FORCEINLINE` / `XOBF_NOINLINE` | function attributes | cross-compiler inlining control |

## How it maps to the research

- **String/constant encryption (§3.3 of the report).** The plaintext literal is
  consumed entirely inside a `constexpr cipher` object, so the compiler never emits
  it to `.rodata`; only ciphertext reaches the binary. Keys are derived at compile
  time (`splitmix64` over a `__DATE__`/`__TIME__` build seed), so no key material is
  embedded and every build differs. **Verified**: `strings` finds no plaintext
  markers in the demo binary (g++/clang, -O2/-O3).
- **MBA substitution (§3.1–3.2).** Uses the same identities as binprotect/OLLVM,
  proven exact for wrapping two's-complement integers and unit-tested exhaustively
  over edge values and every integer width.
- **Opaque predicates (§3.4).** Follows binprotect's principle of seeding from a
  *runtime* value (here a `volatile` global) so the predicate can't be constant-folded,
  rather than es3n1n's statically-foldable z3 tautologies.

## Limitations (be honest about these)

- This is **source-level** obfuscation. A determined reverse engineer with a dynamic
  debugger can still observe decrypted strings and real control flow at runtime.
- An optimising compiler may simplify some MBA/opaque constructs; the `volatile`
  seeds resist this but do not guarantee the shape survives aggressive optimisation.
- `XOBF_STR` uses a `static` per call site, so the decrypted buffer, once decrypted,
  stays in memory until `.scrub()` is called.
- Not a substitute for real anti-tamper/anti-debug; combine with the LLVM pass
  (`../llvm-obfuscator`) for machine-code-level control-flow obfuscation.

## Build & test

```bash
cmake -B build -DXOBF_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build        # runs correctness tests

# or directly:
g++ -std=c++17 -O2 -Iinclude tests/tests.cpp -o tests && ./tests
```

> Note: build/run on a normal (exec-capable) filesystem. Some mounts (e.g. network/
> fuse share drives) forbid executing files; compile the test binary to a local path
> such as `/tmp` if `ctest` reports it cannot launch the test.
