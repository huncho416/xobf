# xobf techniques and threat model

This document maps every xobf module to the concrete reverse-engineering attack it is designed
to raise the cost of, explains how it works, and states what it does **not** do. xobf targets
software self-protection (IP, licensing, embedded secrets, integrity of binaries you ship), not
evasion of legitimate malware analysis.

## Threat model

We consider an analyst with full access to the shipped binary and the following capabilities,
ordered by effort:

1. **Static string/constant scraping** — `strings`, `objdump`, grep over `.rodata`/`.data`/`.text`.
2. **Static pattern matching / decompilation** — IDA, Ghidra, Binary Ninja: CFG recovery,
   decompiler structuring, library/API identification, MBA rewrite rules.
3. **Automated algebraic simplification** — SMT/term-rewriting simplifiers over lifted IR.
4. **Symbolic / concolic execution** — angr, KLEE-style path exploration and constraint solving.
5. **Dynamic analysis** — live debugger, tracer, instrumentation, memory dumps at runtime.

xobf raises the cost of (1)–(4) substantially and adds friction to (5). It does not claim to
defeat a determined dynamic analyst; that is an explicit non-goal of source-level obfuscation.

## Design principles

- **Semantics-preserving.** Every transform is correctness-preserving and unit-tested; a
  transform that changes observable output is treated as a bug.
- **Polymorphic per site and per build.** Each site derives a unique compile-time key from
  `__COUNTER__`/`__LINE__` plus a `__DATE__`/`__TIME__` build seed (`XOBF_KEY`), so identical
  source produces divergent code and rebuilds differ.
- **Defeat tools, not only humans.** Favor constructs that break automated pipelines:
  diverse expression trees over fixed identities, runtime-seeded values over foldable constants,
  one-way/path-explosion predicates over tautologies a solver can invert.
- **Layering.** Techniques compose so that neutralizing one still leaves the others; no single
  silver bullet.

## Module-by-module

### `random.hpp` — compile-time key & PRNG primitives
Provides `fnv1a`, `splitmix64`, a `__DATE__`/`__TIME__` build seed, a `ctprng` counter PRNG, and
rotate/odd-inverse helpers, plus the `XOBF_KEY` per-site key macro and the `volatile opaque_seed`
used as a runtime anti-fold source. Everything else keys off these.

### `string.hpp` — string encryption (attack 1, 2, 5-patching)
Literals are consumed inside a `constexpr cipher`, so only ciphertext reaches the binary. A
multi-round, per-site keystream (XOR / add / rotate selected by the site key) means two identical
literals decrypt through different code. Two variants:
- `XOBF_STR` decrypts lazily into static storage, `.scrub()` re-encrypts.
- `XOBF_STR_STACK` decrypts into a stack buffer that is wiped on destruction; the ciphertext is
  read through a `volatile` barrier so the optimizer cannot constant-fold the decrypt and emit the
  plaintext as `mov` immediates.
A compile-time payload checksum is verified after decryption; a patched ciphertext yields zeroed
output instead of a usable plaintext.
**Does not** prevent reading the decrypted buffer from memory at runtime.

### `constant.hpp` — integer constant hiding (attack 1, 3)
`XOBF_INT` stores the value XOR-masked in a vault of same-width decoy slots; the real slot index is
key-derived. Recovery runs through `mba::mix` behind a `volatile` read, so the constant is neither a
visible immediate nor constant-foldable.

### `vm.hpp` — compile-time→runtime bytecode VM (attack 1, 2, 3)
At compile time a `constexpr` assembler emits a short encrypted bytecode program for a tiny
stack machine (XOR/ADD/SUB/ROL/MUL/OUT). Opcodes and operands are XOR-masked with a per-site
keystream, so the VM has no fixed byte signature across builds. At runtime a `NOINLINE`
interpreter, seeded from `opaque_seed`, reconstructs the value on the stack — it never appears
as an immediate. This is the strongest static-recovery barrier in the library for scalar values.
`XOBF_VMSTR` extends the same mechanism to strings: the literal is split into 8-byte blocks, each
compiled to its own encrypted VM program, and reconstructed block-by-block into a stack buffer at
runtime — neither plaintext nor an XOR-decryptable ciphertext of it is stored.
**Trade-off**: interpreter overhead per value/block and added build time plus per-block program
size; use for high-value constants and the most sensitive strings.

### `mba.hpp` — Mixed Boolean-Arithmetic (attack 2, 3)
Exact identities for two's-complement integers (e.g. `a+b == (a^b)+2*(a&b)`), plus `*_poly<K>`
diverse per-key expansions and a reversible `mix<K, Depth>` identity mixer. Diversity matters:
a handful of fixed MBA identities fall to a small set of rewrite rules, whereas per-site-varied
trees keep simplifiers working. **Trade-off**: aggressive optimizers can simplify shallow MBA;
raise `XOBF_LEVEL` for deeper, harder trees.

### `predicate.hpp` — opaque predicates (attack 2, 4)
Families selected per site:
- Runtime-seeded even/odd identities (`x*(x+1)` is always even) that cannot be folded because the
  seed is `volatile`.
- Hash predicates gating on `splitmix64(seed)` properties — a one-way construction a constraint
  solver cannot invert to force the other branch.
- Path-explosion predicates (`opaque_true_explode`) that iterate a seeded mixer to multiply the
  symbolic paths an executor must track.
`XOBF_BOGUS` runs real code behind an always-true predicate with a bogus else; `XOBF_DEAD` hangs
code off a provably-false branch.

### `cflow.hpp` — control-flow flattening (attack 2)
`XOBF_FLATTEN_*` rewrites straight-line/branching code into a `switch`-dispatched state machine
driven by a `volatile` state variable, collapsing the natural CFG into a dispatcher that frustrates
decompiler structuring. `XOBF_DFLATTEN_*` is a deeper variant: each logical state is mapped through
a per-site bijective scramble (`flat_tok`) to a non-sequential 32-bit dispatcher token, and the live
state variable is XOR-masked with a runtime-seeded value (`flat_mask`). In the shallow dispatcher an
analyst reads the state numbering directly from `cmp $0x1/$0x2/$0x3`; in the deep dispatcher the same
machine compares against scrambled tokens (e.g. `cmp $0x2b320167`) that the compiler cannot thread
back together because the mask is only known at runtime. `XOBF_OPAQUE` launders values through the
runtime seed and `XOBF_JUNK` inserts data-dependent dead code. **Trade-off**: dispatcher overhead on
hot paths, slightly higher for the deep variant; flatten security-relevant routines, not tight loops.

### `callhide.hpp` — call-graph hiding (attack 2)
`XOBF_HIDDEN(&fn)` stores the real function pointer XOR-masked at a key-derived index among decoy
pointers and dispatches through a `NOINLINE` resolver, producing an indirect `call` instead of a
direct one — hiding the program's **own** call edges from static call-graph recovery.
**Does not** hide the target once observed dynamically.

### `apihash.hpp` — API name hiding (attack 1)
`XOBF_API`/`XOBF_API_MAIN` resolve functions through the **standard loader** (`dlsym` /
`GetProcAddress`) while the API name string is compile-time encrypted (`XOBF_STR_STACK`) and
decrypted on the stack only for the resolution call, so names are not stored as plaintext. This
deliberately keeps the normal, documented import mechanism — it is a string-hiding convenience, not
a manual loader-walk import hider, and it does not try to make the binary look import-free.

### `antidebug.hpp` — debugger detection (attack 5)
`is_debugged()` reads `/proc/self/status` `TracerPid` on Linux and `IsDebuggerPresent` on Windows;
`timing_tripped()` flags when a short mixer loop takes implausibly long (single-stepping). All
helper strings go through `XOBF_STR`; checks sit behind diverse opaque predicates. `XOBF_IF_DEBUGGED`
and `XOBF_CRASH_IF_DEBUGGED` act on detection. **Best-effort**: anti-debug is defeatable by a
prepared analyst and should be one layer among many.

### `antitamper.hpp` — integrity self-check (attack 5-patching)
`code_checksum`/`region_checksum` compute FNV-1a over a byte range; `XOBF_VERIFY_OR` runs a
response when a region's checksum deviates from an expected value. Use to detect patched code or
data at runtime. **Best-effort**: an analyst who also patches the checker can bypass it; combine
with the other layers.

## Composition guidance

- Encrypt every embedded secret with `XOBF_STR`/`XOBF_STR_STACK` and every sensitive literal with
  `XOBF_INT`/`XOBF_VM_INT`.
- Flatten and `XOBF_BOGUS`/`XOBF_JUNK` the routines that implement license and integrity logic,
  not performance-critical inner loops. Use `XOBF_DFLATTEN_*` for the most sensitive routines.
- Gate critical paths on `under_analysis()` and verify integrity with `XOBF_VERIFY_OR` at points
  spread through execution rather than one up-front check.
- Raise `XOBF_LEVEL` for release builds; keep it low for debugging.

## Non-goals

- Defeating a determined dynamic analyst with full runtime control.
- Hiding malware from legitimate security analysis or AV/EDR.
- Guaranteeing a specific machine-code shape across all compilers and flags.
