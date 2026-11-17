# xobf — industrial-grade, header-only C++ obfuscation

A **zero-dependency, header-only** obfuscation library for C++17. You `#include <xobf/xobf.hpp>`
and compile with any standard compiler — no custom toolchain, no build plugin, no external
runtime. It layers many correctness-preserving transforms that compound on each other to make
a program slow and expensive to reverse-engineer.

xobf is built for **software self-protection**: protecting your own intellectual property,
license checks, embedded secrets, and integrity of binaries you ship. It is not designed to
hide malware from legitimate security analysis.

## Highlights

- **Polymorphic per site and per build.** Every expansion site derives a unique compile-time
  key from `__COUNTER__`/`__LINE__` and a `__DATE__`/`__TIME__` build seed, so two identical
  source lines generate different code and two builds of the same source differ.
- **Resists automated deobfuscation, not just humans.** Diverse MBA trees defeat pattern
  matchers; `volatile`-seeded constructs defeat constant folding; hash/path-explosion opaque
  predicates resist symbolic execution.
- **Layered.** String & constant encryption, a compile-time→runtime bytecode VM, MBA,
  control-flow flattening, opaque predicates, junk code, hidden calls, encrypted API
  resolution, anti-debug and anti-tamper — composable, not a single silver bullet.
- **Tunable.** One `XOBF_LEVEL` (0–3) trades build-time/runtime cost against strength.
- **Verified.** Correctness-preserving by design, unit-tested exhaustively, and checked for
  plaintext leakage under clang and g++ at `-O0/-O2/-O3`.

## Quick start

```cpp
#include <xobf/xobf.hpp>
#include <cstdio>

int main() {
    std::puts(XOBF_STR("secret api token").decrypt());   // plaintext never in .rodata
    int port = XOBF_INT(8080);                            // no plaintext immediate
    int vm   = XOBF_VM_INT(443);                          // reconstructed by a tiny VM
    int z    = xobf::mba::add(port, 1);                   // == port + 1, obfuscated
    XOBF_BOGUS(std::printf("%d %d %d\n", port, vm, z));   // opaque-predicate-guarded
}
```

Build: `clang++ -std=c++17 -O2 -Iinclude your.cpp` (also tested with `g++`).

## Technique matrix

| Module | Public API | Technique | Primary attack it resists | Cost |
|--------|-----------|-----------|---------------------------|------|
| `string.hpp` | `XOBF_STR`, `XOBF_STR_STACK` | multi-round polymorphic string cipher; stack-only decrypt; payload checksum | static strings/`strings`, patching | low |
| `constant.hpp` | `XOBF_INT` | decoy-vault + MBA-decoded integer constants | immediate scraping, constant folding | low |
| `vm.hpp` | `XOBF_VM_INT`, `XOBF_VMSTR` | compile-time→runtime bytecode VM, per-build opcode permutation; VM-reconstructed strings | static constant/string recovery, pattern matching | medium |
| `mba.hpp` | `xobf::mba::add/sub/xor_/and_/or_`, `*_poly<K>`, `mix<K,D>` | diverse Mixed Boolean-Arithmetic trees + reversible identity mixer | algebraic simplification, MBA pattern rules | low–med |
| `predicate.hpp` | `XOBF_BOGUS`, `XOBF_DEAD`, `opaque_true*/false*` | runtime-seeded, hash, and path-explosion opaque predicates | symbolic/concolic execution, path pruning | low |
| `cflow.hpp` | `XOBF_FLATTEN_*`, `XOBF_JUNK`, `XOBF_OPAQUE` | source-level control-flow flattening + junk insertion | CFG recovery, decompiler structuring | medium |
| `callhide.hpp` | `XOBF_HIDDEN` | encrypted function-pointer table with decoys, indirect dispatch | static call-graph recovery | low |
| `apihash.hpp` | `XOBF_API`, `XOBF_API_MAIN` | API resolved via the standard loader with compile-time-encrypted names | plaintext API name scraping | low |
| `antidebug.hpp` | `xobf::is_debugged`, `timing_tripped`, `under_analysis`, `XOBF_IF_DEBUGGED`, `XOBF_CRASH_IF_DEBUGGED` | TracerPid / `IsDebuggerPresent` / timing probes | live debugging | low |
| `antitamper.hpp` | `XOBF_CHECKSUM`, `XOBF_VERIFY_OR`, `code/region_checksum` | FNV-1a self-checksums over code/data ranges | binary patching | low |

See [`docs/TECHNIQUES.md`](docs/TECHNIQUES.md) for the full threat model and how each module maps
to a specific deobfuscation attack.

## API reference

### Data protection
- `XOBF_STR("s")` → `xobf::secret&`; `.decrypt()` / `.get()` / implicit `const char*`; `.scrub()`
  re-encrypts in place; `.verified()` reports payload integrity.
- `XOBF_STR_STACK("s")` → `xobf::stack_secret`; decrypts into a stack buffer that is wiped on
  destruction and never kept as a static plaintext copy. Resistant to constant folding.
- `XOBF_INT(v)` → integer stored XOR-masked in a decoy vault and recovered through an MBA
  identity mixer behind a `volatile` barrier.
- `XOBF_VM_INT(v)` → value reconstructed at runtime by the stack bytecode VM; never present as
  an immediate.
- `XOBF_VMSTR("s")` → `xobf::vm::vm_secret`; each 8-byte block of the string is reconstructed at
  runtime by a per-block encrypted VM program into a stack buffer wiped on destruction. Plaintext
  is never stored; keep the returned object alive while using `.c_str()`.

### Arithmetic
- `xobf::mba::add/sub/xor_/and_/or_(a, b)` — exact MBA identities for two's-complement integers.
- `xobf::mba::add_poly<K>/sub_poly<K>/xor_poly<K>(a, b)` — per-key diverse expansions.
- `xobf::mba::mix<K, Depth>(x)` — reversible identity; forces an analyst/solver through a deep
  expression while returning `x` unchanged.

### Control flow
- `XOBF_FLATTEN_BEGIN(entry) … XOBF_CASE(n) … XOBF_GOTO(n) / XOBF_GOTO_IF(cond, n) …
  XOBF_FLATTEN_EXIT_TO(label) … XOBF_FLATTEN_END(label)` — dispatcher-driven state machine.
- `XOBF_OPAQUE(v)` — identity that launders `v` through the runtime seed.
- `XOBF_JUNK()` — data-dependent dead code behind an opaque-false predicate.
- `XOBF_BOGUS(stmt...)` / `XOBF_DEAD(stmt...)` — run `stmt` behind an always-true predicate with
  a bogus else / on a provably-dead branch.

### Symbol & call hiding
- `XOBF_HIDDEN(&fn)` → callable wrapper that stores the real pointer encrypted among decoys and
  dispatches indirectly.
- `XOBF_API(FnType, module, "name")` / `XOBF_API_MAIN(FnType, "name")` — resolve a function
  through `dlsym`/`GetProcAddress` with the name string encrypted at compile time.

### Self-defense
- `xobf::is_debugged()`, `xobf::timing_tripped(ns)`, `xobf::under_analysis()`.
- `XOBF_IF_DEBUGGED(stmt...)`, `XOBF_CRASH_IF_DEBUGGED()`.
- `XOBF_CHECKSUM(ptr, len)`, `XOBF_VERIFY_OR(ptr, len, expected, stmt...)`.

## Strength levels

`XOBF_LEVEL` (default `2`) scales MBA depth and string cipher rounds:

| Level | Intent | MBA depth | String rounds |
|-------|--------|-----------|---------------|
| 0 | debug / minimal overhead | 0 | 1 |
| 1 | light | 2 | 1 |
| 2 | **default**, balanced | 3 | 2 |
| 3 | maximum | 5 | 3 |

Set it per build: `clang++ -std=c++17 -O2 -DXOBF_LEVEL=3 -Iinclude your.cpp`, or via CMake
`-DXOBF_LEVEL=3`.

## Build & test

```bash
cmake -B build -DXOBF_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build

# or run the full verification harness (clang + g++, -O0/-O2/-O3, leak check):
bash scripts/verify.sh
```

`scripts/verify.sh` builds the test suite and demo across both compilers and every optimization
level, runs the correctness tests, and greps the demo binary for plaintext markers to prove no
secret leaks into the binary.

> Build/run on an exec-capable filesystem. Some mounts (network/fuse shares) forbid executing
> files; compile the test binary to a local path such as `/tmp` if a runner cannot launch it.

## Limitations (read these)

- This is **source-level** obfuscation. A determined analyst with a dynamic debugger can still
  observe decrypted strings, resolved API calls, and real control flow at runtime. The goal is to
  raise the cost, not to make reversing impossible.
- Optimizers can simplify some constructs. xobf uses `volatile` seeds and `NOINLINE` barriers to
  resist this, but the exact machine-code shape is not guaranteed across every compiler/flag.
- Anti-debug / anti-tamper here are best-effort self-checks, not a kernel-level protection scheme.
  They deter casual analysis and should be layered, never relied on alone.
- For machine-code-level control-flow obfuscation, combine with the LLVM pass in
  [`llvm-obfuscator/`](llvm-obfuscator).

## Repository layout

```
include/xobf/     header-only library (one header per technique + xobf.hpp umbrella)
examples/demo.cpp runnable showcase and leak-check target
tests/            correctness suite + leak_check.sh
scripts/verify.sh clang+g++ × -O0/-O2/-O3 build/test/leak harness
docs/             research report + TECHNIQUES.md threat model
llvm-obfuscator/  optional LLVM pass for machine-code-level obfuscation
```

## License

MIT — see [`LICENSE`](LICENSE).
