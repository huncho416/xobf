# C++ Obfuscators — deliverables

Built from the research in `cpp-obfuscation-research.md`. There are three ways to
obfuscate C++ (per the report): source-level, compiler/IR-level, and bin2bin. Two of
the three are delivered here as **working, tested** tools; the third is scoped below.

## 1. `xobf/` — compile-time source-level obfuscation library ✅ complete & verified
Zero-dependency, header-only C++17. String encryption, constant encryption, MBA
substitution, opaque predicates / bogus control flow.
- Works with any standard compiler; nothing special needed.
- **Verified here**: all correctness tests pass (g++ 13 and clang 18, -O0/-O2), and
  `strings` confirms plaintext secrets are absent from the compiled binary.
- See `xobf/README.md`.

## 2. `llvm-obfuscator/` — OLLVM-style LLVM pass plugin ✅ complete & verified
Middle-end IR obfuscator as an out-of-tree plugin (no LLVM fork). `substitution`
(MBA) and `bogus-cf` (opaque-predicate control-flow inflation) passes.
- **Verified here** against LLVM 18: builds to `libLLVMObfuscator.so`, runs via both
  `opt` and `clang -fpass-plugin`, inflates the IR/CFG, and the obfuscated program's
  output is byte-identical to the unobfuscated reference.
- Works for any LLVM frontend (C/C++/Rust/Swift/…) and target.
- See `llvm-obfuscator/README.md`.

## 3. bin2bin PE obfuscator (like binprotect / es3n1n) — scoped, not built
This is the largest effort: a full binary rewriter (PE parse → disassemble → basic
blocks → jump-table recovery → RVA tracking → transforms → relink). I can write the
source, but to **validate** it I would need:
- **A sample target**: a real Windows x64 `.exe` plus its `.pdb` or `.map` (several
  transforms require symbols, per the report).
- **A Windows (or wine) runtime** to execute and confirm the obfuscated PE still runs
  — this sandbox is Linux-only, so runtime verification of PEs isn't possible here.
- Dependencies (`Zydis`/`zasm`, `linux-pe`) are open source and fetchable.

If you want this path, provide a sample `.exe`+`.pdb` and a Windows/wine test target,
and it can be built incrementally (starting from PE loader + disassembler).

---

## What was needed from you
Short answer to "what would I need to give you access to": **nothing** for
deliverables 1 and 2 — this environment already has network access plus a full LLVM 18
dev toolchain (`llvm-18-dev`, `clang-18`, `opt-18`), which was installed and used to
build and test both. Only the bin2bin PE tool (#3) needs you to supply a Windows test
runtime and a sample binary with symbols.

## Reproduce everything
```bash
# 1) compile-time library
cd xobf && g++ -std=c++17 -O2 -Iinclude tests/tests.cpp -o /tmp/t && /tmp/t
g++ -std=c++17 -O2 -Iinclude examples/demo.cpp -o /tmp/demo
strings /tmp/demo | grep -c MARKER   # -> 0 (no plaintext leaks)

# 2) LLVM pass plugin
cd ../llvm-obfuscator
cmake -B build -DLLVM_DIR=$(llvm-config-18 --cmakedir) . && cmake --build build
clang-18 -O2 -fpass-plugin=build/libLLVMObfuscator.so test/sample.c -o /tmp/obf && /tmp/obf
```
