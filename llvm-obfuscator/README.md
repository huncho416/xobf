# llvm-obfuscator — an OLLVM-style LLVM pass plugin

A compiler-level (middle-end) obfuscator implemented as a **modern, out-of-tree LLVM
pass plugin** — no LLVM fork required. It transforms LLVM IR, so it works for any
LLVM-based frontend (C, C++, Rust, Swift, …) and any target LLVM supports.

This is the "serious" obfuscator path from the research report: because it runs
mid-compilation it has full semantic information, making transforms simpler and safer
than a bin2bin rewriter.

## Transforms implemented

| Pass name | What it does | Research basis |
|-----------|--------------|----------------|
| `substitution` | Rewrites integer `add/sub/and/or/xor` into equivalent Mixed Boolean-Arithmetic expressions | OLLVM `-sub`, §3.1–3.2 |
| `bogus-cf` | Splits each basic-block body and guards it with a runtime-seeded opaque predicate plus a bogus (harmless) edge, inflating the CFG | OLLVM `-bcf`, §3.4 |

Both are **correctness-preserving**: substitution uses exact integer identities, and
the bogus false-edge falls through to the real body, so either path is safe.

## Build

Requires an installed LLVM with dev headers (built & tested against **LLVM 18**).

```bash
cmake -B build -DLLVM_DIR=$(llvm-config --cmakedir) .
cmake --build build
# produces build/libLLVMObfuscator.so
```

## Usage

**Via `opt` (apply to pre-generated IR):**
```bash
clang -O1 -S -emit-llvm input.c -o input.ll
opt -load-pass-plugin=build/libLLVMObfuscator.so \
    -passes="substitution,bogus-cf" -S input.ll -o obf.ll
clang -O2 obf.ll -o program
```

**Via `clang` (auto-runs at the optimizer-last insertion point):**
```bash
clang -O2 -fpass-plugin=build/libLLVMObfuscator.so input.c -o program
```

## Verified behaviour (see `test/sample.c`)

- `opt` and `clang -fpass-plugin` both run the passes cleanly on LLVM 18.
- On the sample, IR grew from 38 → 104 lines, with 6 bogus `.junk` blocks and the
  `xobf_opaque_seed` global inserted.
- The obfuscated program's output is **identical** to the unobfuscated reference,
  confirming semantics are preserved (both via `opt` and `-fpass-plugin`).

## Extending it

Add a pass by writing a `PassInfoMixin` struct with a `run(Function&, …)` method and
registering its name in `registerPipelineParsingCallback`. Natural next passes
(described in the report): **control-flow flattening** (`-fla`), **string/global
encryption** (encrypt constant initialisers, insert a decrypt stub in a global ctor),
and **instruction-level constant encryption**. Production forks also add deterministic
seeding and an IR-growth budget to prevent runaway bloat.

## Limitations

- These middle-end transforms can be partially undone by aggressive later
  optimisation and by modern deobfuscation (SMT-based MBA simplification, symbolic
  execution) — see the report's confidence-and-gaps section.
- Opaque predicates rely on the `volatile` seed load not being proven constant; this
  holds in practice but is not a formal guarantee.
- No anti-debug/anti-tamper here; obfuscation ≠ encryption.
