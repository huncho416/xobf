# Building a C++ Obfuscator: Research Report

A practitioner-oriented survey of how C++ obfuscators work and how to build your
own, grounded in two reference projects (noahware/binprotect [3] and
es3n1n/obfuscator [1][2]) plus the LLVM-based ecosystem (OLLVM [4] and successors
[5][6][7][8][9]).

**How to read the citations.** Every non-obvious claim carries an inline marker like
[3] or [1 §2.4] that points to a numbered, URL-bearing entry in the **References**
section at the end. Where a specific sub-section of a source supports the claim, the
marker names it so the claim can be traced to that passage.

## Sub-questions covered
1. What architectural approaches exist for obfuscating C++ (where in the toolchain do you intervene)?
2. What is the internal pipeline of a bin2bin obfuscator (the hard engineering problem)?
3. What obfuscation transforms are actually implemented, and how?
4. How do you build the compiler-level (LLVM) alternative?
5. A recommended build path.
6. Contested points: where sources disagree, and which I find more credible.
7. Confidence and gaps.

---

## 1. Two architectural approaches

There are fundamentally two places to intervene, and the choice dictates almost
everything else about the project.

**bin2bin (binary-to-binary).** You feed an already-compiled binary (plus optional
debug info such as MAP/PDB) to the obfuscator and get an obfuscated binary back; the
author cites VMProtect and Themida as commercial examples [1 §"Binary to binary"].
This is the approach of both reference repos: binprotect states it "takes an already
compiled executable binary and reproduces it with the obfuscation passes applied,"
which can protect an application "without having access to the original source code,"
and it supports x64 PE only for now [3 §1].

**Compiler/linker level.** You hook into the compiler — es3n1n notes it is "pretty
common to create an LLVM fork and directly modify the LLVM transforms, so that as
soon as your function is compiled, but not linked, you can apply the obfuscation,"
citing OLLVM as the example [1 §"Linker level"]. OLLVM itself is described by its
authors as a set of transformations implemented as **middle-end** LLVM passes, which
makes them "language-agnostic and independent of the target architecture" because
they operate on LLVM IR [4 §II.A].

The practical trade-off: the compiler-level approach is language- and
target-agnostic because it works on IR [4 §II.A][9], but you must control the build.
bin2bin works on any shipped binary but must reconstruct information the compiler
discarded [1 §"Portable executable"].

---

## 2. The bin2bin pipeline (the real engineering)

For a bin2bin tool, most of the work is *not* obfuscation — it is taking a binary
apart and reassembling it without breaking it.

### 2.1 Inputs and symbol information
Both projects accept optional PDB and MAP symbol files to aid analysis; binprotect
states that some features — specifically exceptions support and control-flow
flattening — *require* a symbol file [3 §1, §2.3.1.1, §3.3]. es3n1n's `func_parser`
module parses function start/end RVAs from MAP and PDB; its PDB parser supports only
the common v7 format and its MAP parser only the MSVC variant [1 §"Function parser"].

### 2.2 PE understanding
A bin2bin author must understand the executable format: sections (.text =
read/execute code, .data = read/write globals, .rdata = read-only data),
relocations for ASLR, RIP-relative addressing as the common x64 way to reference data
without relocations, and the imports table [1 §"Portable executable"]. es3n1n uses
`linux-pe` for parsing and `zasm` (a Zydis/asmjit-style wrapper) for
disassembly/assembly [1 §"Implementation"].

### 2.3 Relative-address tracking — the core invariant
binprotect's distinguishing choice is inserting obfuscated code *into the original
sections* rather than appending a new section (which it says every other bin2bin tool
does); this forces tracking of every RVA so references stay valid when bytes shift,
and it updates every tracked RVA whenever bytes are inserted or removed [3 §1, §2.1].
(This claimed stealth benefit is a contested point — see §6.1.)

### 2.4 Disassembly and basic-block decomposition
Both tools build a recursive disassembler that decomposes code into **basic blocks**.
binprotect defines a basic block as "a group of instructions with no branches" that
terminates at control-flow instructions (jump, return, int) but not at calls, since
calls usually return [3 §2.2]. es3n1n defines BBs identically and decomposes by
disassembling instruction-by-instruction until a JCC/JMP/RET is hit [1 §"Basic Block
decomposition"]. A recurring subtlety both document is **basic-block splitting**:
naive linear disassembly duplicates shared instructions (e.g. a loop body or a shared
`ret`), so overlapping blocks must be split [3 §2.2.1][1 §"Phase 3: BB splitting"].

### 2.5 Indirect control flow / jump tables
Switch statements compile to jump tables, and recovering them is among the hardest
parts. binprotect handles **bounded** tables (a `cmp`/`ja` bounds-check reveals the
entry count) and **unbounded** tables whose entries must be brute-forced by scanning
the table base for valid RVAs — which it explicitly calls less safe [3 §2.2.2.1.1,
§2.2.2.1.2]. It supports MSVC (including multi-level), LLVM/Clang, and GCC jump-table
formats, which differ (MSVC = arrays of RVAs; LLVM = offsets relative to the table
base; GCC = DIR64 relocations) [3 §2.2.2.1.3]. es3n1n similarly collects jump tables,
estimates size via bounds-check or brute force, then expands them into chained
compare/jump sequences [1 §"Phase 2", "Phase 4-1"].

### 2.6 "No-return" calls and risky references
Functions like `_CxxThrowException` never return, so disassembly runs into whatever
bytes follow. binprotect notes compilers insert INT3/UD2 padding that helps terminate
blocks but that detection is not infallible, adding countermeasures such as splitting
a block when jump-table entries are found inside it [3 §2.2.4]. es3n1n documents the
same category around `lea rax,[rip+X]; ret` continuation patterns, where the target
may be code, a string, or a jump table and must be validated before being queued as
code [3 §2.2.2.2].

### 2.7 Exceptions, unwinding, and RTTI
binprotect goes furthest here. Because its transforms allocate stack space, it
inserts a frame pointer (rbp) into every runtime function that lacks one, pushing rbp
twice (the second push realigns the stack to 16 bytes), emitting matching unwind
codes, and adjusting every stack reference past the local allocation by 16 bytes
[3 §2.3.1.1]. It parses SEH/C_SCOPE_TABLE, C++ FuncInfo3/FuncInfo4 descriptors, and
RTTI/ThrowInfo structures so the RVAs inside them stay tracked [3 §2.3.2, §2.3.3]. It
supports MSVC and Clang/LLVM C++ exceptions but *not* GCC, which uses a different
format [3 §2.3.2.2].

### 2.8 Linking and rebuilding the PE
es3n1n estimates total obfuscated code size, creates a new read/execute section,
links the functions (zasm emits code and the relocation list), then rebuilds the PE:
erase/reassemble relocations, re-init the header, copy sections, recompute checksum
[1 §"Producing output binary"]. (binprotect instead resizes original sections — §2.3.)

---

## 3. Obfuscation transforms

Both projects converge on a similar catalogue — the techniques OLLVM popularized [4].

### 3.1 Instruction/arithmetic substitution
Replace an operation with an equivalent, more complex one. es3n1n's examples:
`x + y → x - (-y)`, `x - y → x + (-y)`, `x & y → ((x ^ ~y) & x)`, runnable repeatedly
[1 §"Substitution"]. OLLVM's substitution pass (`-sub`) covers integer add/sub and
boolean AND/OR/XOR, with tunable density to limit code size impact [4 §II.C].

### 3.2 Mixed Boolean Arithmetic (MBA)
A stronger substitution using Boolean-arithmetic identities, e.g.
`x + y → (x & y) + (x | y)`, applied recursively by binprotect so complexity grows
exponentially [3 §3.5]. A correctness detail binprotect documents: splitting an
instruction changes flag behaviour, so it emulates the original flags (`test` for
and/or/xor, `cmp` for sub, a manual CF/AF/OF stub for add) but only when flags are
actually read, since the stub itself leaks the original instruction [3 §3.5].

### 3.3 Constant encryption
Encrypt immediates with a math expression at obfuscation time and decrypt at runtime.
es3n1n built a `mathop` generator/emulator that generates a random expression,
emulates it on the constant, then lifts the reverse operations into native code
[1 §"Constant encryption", §"Mathop"]. Because IDA and Binary Ninja use symbolic
execution to simplify these, es3n1n deliberately defeats those optimizations: `xchg`
load/store via the stack to break IDA [1 §"Breaking IDA symbolic execution"], and an
`ADC`-plus-forced-carry trick that exploits a Binary Ninja bug where it emits
`a+b+0` regardless of the carry value, producing incorrect decompilation
[1 §"Breaking BInja symbolic execution"].

### 3.4 Opaque predicates / bogus control flow
Insert an always-known branch condition leading to a fake block. The two projects
choose different predicates — and implicitly disagree on how strong a predicate needs
to be (see §6.2):
- **binprotect** uses **Fermat's Last Theorem** (`aⁿ + bⁿ ≠ cⁿ` for n > 2), with a,
  b, c, n drawn from runtime values (rsp and rip under ASLR) so an attacker cannot
  pre-solve it; it also duplicates the real block with randomized operands and
  shuffles physical block order [3 §3.2].
- **es3n1n** uses **z3 to generate tautologies** on uint32 values, and explicitly
  admits these mostly discard bits and are therefore easily optimized away by
  decompilers [1 §"Bogus control flow"].

### 3.5 Control-flow flattening (CFF)
binprotect groups all basic blocks except the prologue, assigns each a unique ID, and
builds a dispatcher switch; direct branches are replaced with jumps to the dispatcher
with the correct ID, and physical block order is shuffled [3 §3.3]. In OLLVM this is
the `-fla` pass [4 §II.B][7].

### 3.6 Linear substitution (binprotect's term)
Hide immediates/displacements by computing them at runtime: pick random R, encode
`R+N`, subtract R back so the effective value is `((R+N) - R)`; for stack operands a
displacement accounts for the pushes it inserts to save flags and a scratch register
[3 §3.4].

### 3.7 Virtual machine (code virtualization) — the strongest technique
binprotect virtualizes x86-64 instructions into a custom virtual CPU so a reverse
engineer must first understand the invented architecture [3 §3.1]. The implementation
is generic rather than one hand-written handler per instruction: on entry it pushes
all GPRs (except rsp) and rflags as randomly ordered virtual registers; operands are
obfuscated (random 16-bit XOR + ones-complement) across handlers; flags are
loaded/stored around the real instruction so behaviour matches; virtualization stops
when an instruction uses rsp; and it adds unwind info so exceptions propagate out of
the VM [3 §3.1, §3.3.1].

### 3.8 Decompiler-breaking (a distinct goal from obfuscation)
es3n1n documents deliberately crashing decompilers, hidden behind opaque predicates
so they never run on bare metal: `ENTER 0xFFFF, 0xFF` triggers an IDA microcode
exception, and a dereference of address `-1` triggers a Ghidra "extends beyond the
end of the address space" error [1 §"Breaking decompilers"]. These exploit specific
tool bugs, so they are fragile across versions.

---

## 4. The compiler-level (LLVM) approach

If you control the build, an LLVM-based obfuscator is far less work.

### 4.1 Origin: Obfuscator-LLVM (OLLVM)
OLLVM, from HEIG-VD, implements obfuscating transformations as middle-end LLVM
passes; its authors argue the middle-end placement makes them language-agnostic and
target-independent, while noting some protections (e.g. anti-debugging) cannot be
done there [4 §II.A]. Its classic passes are substitution (`-sub`), bogus control
flow (`-bcf`), control-flow flattening (`-fla`), and basic-block splitting
(`-split`), invoked via Clang `-mllvm` flags, e.g.
`clang -mllvm -sub -mllvm -sub_loop=3 -mllvm -split -mllvm -split_num=3 -mllvm -fla -mllvm -bcf -mllvm -bcf_loop=3 -mllvm -bcf_prob=40 main.c` [7].

### 4.2 The maintenance problem and modern forks
The original OLLVM is old; a maintained successor notes its last version was built on
LLVM 4.0, released years ago, which is why a long tail of forks tracks newer versions
(ollvm-12.x, -13, -16, and community forks up to LLVM 21/22) [6][8]. eShard revived it
as an **out-of-tree pass plugin** (`libLLVMObfuscator.so`) usable with a stock LLVM
via `-fpass-plugin=`/`-load-pass-plugin`, avoiding a full fork [5]. Because the same
IR-level passes apply to any LLVM frontend, one fork lists compatibility with Ada, C,
C++, D, Fortran, Rust, Swift and others, across many target architectures [9].

### 4.3 How to build one
Two integration models:
- **Full fork:** place passes in `llvm/lib/Transforms/Obfuscation/`, register via
  `add_subdirectory(Obfuscation)`, hook pass construction into the pass manager, and
  build the whole LLVM/Clang tree [7][8].
- **Out-of-tree plugin:** build only your pass against an installed LLVM's cmake
  config, producing a `.so`/`.dll` loaded into clang/opt — no changes to the LLVM
  install, much faster iteration [5][9].

A minimal custom pass exports `llvmGetPassPluginInfo`, registers a named pass, and
implements a `run` method that walks functions/blocks/instructions and rewrites IR
[5]. Well-engineered modern forks add deterministic seeding, IR-growth budgeting to
avoid runaway bloat, and annotation-driven per-function control via
`__attribute__((annotate("...")))` [8].

### 4.4 A transferable design lesson: the transform framework
es3n1n's project is explicitly meant as a learnable, scalable base rather than a
feature leader [1 §"Why this project is better"]. Its most reusable idea is a clean
transform framework: transforms derive from `BBTransform`/`FunctionTransform`/node/
instruction base classes, implement `run_on_*` callbacks, declare a `FeaturesSet` so
the scheduler skips irrelevant callbacks, register in a scheduler, and expose
global/per-function/shared config variables [1 §"Transforms design", §"Transform
scheduling"]. Supporting infra (`LRUReg`/`VarAlloc`) finds registers unused by a
function and allocates scratch variables, saving/restoring flags around them
[1 §"Pass 3: LRUReg"].

---

## 5. Recommended path to building your own

Ordered by dependency rather than schedule:

1. **Decide the layer.** Control the source/build → start with an LLVM pass plugin
   (fastest, cross-language) [5][9]. Must protect third-party binaries → commit to
   bin2bin and its much larger analysis effort [1 §"Portable executable"].
2. **For LLVM:** stand up an out-of-tree plugin against a stock LLVM; implement
   substitution first, then bogus control flow, then flattening [5][7].
3. **For bin2bin:** reuse mature libraries — a PE parser (linux-pe) and an assemble/
   disassemble engine (zasm/Zydis) [1 §"Implementation"]. Build in order: PE loader →
   symbol parser → recursive disassembler + BB decomposition → block splitting →
   jump-table recovery → RVA tracking → linker/PE rebuilder [1][3].
4. **Build the transform framework before the transforms** [1 §"Transforms design"].
5. **Implement transforms cheapest-first:** substitution → MBA → constant encryption
   → opaque predicates → control-flow flattening → virtualization [1][3].
6. **Handle correctness early:** flag emulation for substituted arithmetic [3 §3.5]
   and stack-unwind/exception support if you touch the stack [3 §2.3.1.1].
7. **Test against decompilers** and keep the random seed reproducible — both reference
   tools log/fix their seed for debugging [2][8].

---

## 6. Contested points: where sources disagree, and which I find more credible

This section adjudicates genuine factual disputes (not mere design-taste
differences), with a reasoned credibility judgement for each.

### 6.1 Does avoiding a new PE section actually make a binary harder to analyse?
**The dispute.** binprotect makes an affirmative empirical claim: inserting
obfuscated code into the original sections is "less suspicious to malware analysis as
there are no additional executable sections" and makes analysis "harder" because a
reverse engineer cannot separate obfuscated from unobfuscated code by section [3 §1].
The entire rest of the bin2bin field takes the opposite design position — binprotect
itself concedes that "all of the well known bin2bin obfuscators insert a section"
because it keeps RVAs valid and is "much easier to manage" [3 §1]. es3n1n's tool is
in that mainstream camp, creating "a new READ/EXECUTE PE section" for obfuscated code
[1 §"Linking"].

**Judgement.** binprotect's stealth claim is plausible but is an unverified,
self-reported assertion; the README explicitly flags that it "was tested purely for
educational and research purposes" [3 §1], and offers no detection-rate data against
real AV/analysis tooling. The opposing position — that appending a section is
preferable — rests on a concrete, verifiable engineering fact (RVAs stay valid, so
the rewrite is far less error-prone) that binprotect does not dispute and in fact
affirms [3 §1]. I therefore find the mainstream "append a section" position **more
credible on robustness grounds**, and treat binprotect's stealth advantage as an
unproven hypothesis rather than an established result. The honest summary is a
trade-off: binprotect buys (claimed, unmeasured) stealth at the cost of (documented,
certain) implementation complexity and fragility.

### 6.2 How strong does an opaque predicate have to be to survive a modern decompiler?
**The dispute.** The sources make conflicting *factual* claims about whether simple
opaque predicates work. The original OLLVM paper presents opaque predicates and
substitution as meaningful "software protection" [4 title, §II], and lists increasing
the number of opaque predicates as a path to greater diversity [4 §IV]. es3n1n
directly contradicts the idea that simple predicates are effective, reporting that its
z3-generated tautologies "mostly discard bits" and that "most of the decompilers
could easily optimize these expressions" away [1 §"Bogus control flow"] — and more
broadly that by 2023 "all decompilers have a symbolic execution and optimizations"
that defeat naive constant/predicate tricks [1 §"Constant encryption"]. binprotect's
design implicitly sides with es3n1n: it deliberately avoids a statically solvable
predicate by using Fermat's Last Theorem seeded with runtime-only values (rsp, rip)
so the predicate cannot be folded at analysis time [3 §3.2].

**Judgement.** I find es3n1n and binprotect **more credible than the unqualified
OLLVM framing** on this specific point, for two reasons. First, recency: the OLLVM
paper predates the symbolic-execution capabilities now standard in IDA, Ghidra and
Binary Ninja, whereas the practitioner sources are explicitly reasoning about 2023-era
decompilers [1]. Second, the OLLVM authors themselves undercut their strength claim by
noting that OLLVM being open-source "clearly make[s] the life easier to an adversary"
[4 §V-adjacent]. The reconciling conclusion: opaque predicates are not worthless, but
a predicate that can be constant-folded is; effectiveness hinges on using values not
known until runtime, which is exactly binprotect's design and exactly the weakness
es3n1n flags in its own simpler implementation.

### 6.3 Is the "virtual machine" transform categorically stronger, or just costlier?
**The dispute.** binprotect presents virtualization as the headline protection, "much
harder to analyse" because the analyst must first reverse the invented CPU [3 §3.1].
No source in this set offers a quantitative comparison against the lighter transforms,
and es3n1n does not implement a VM at all. **Judgement.** The qualitative claim is
well-reasoned and widely accepted (it is the basis of VMProtect/Themida [1]), so I
accept it directionally, but I note it is **unquantified** — none of these sources
measures the runtime/size cost of virtualization versus the analysis effort it adds,
so "strongest" here is an informed design assertion, not a measured result.

---

## 7. Confidence and gaps

- **High confidence** on the pipeline structure and transform catalogue: the two
  reference projects independently document nearly identical architectures (disasm →
  basic blocks → jump-table recovery → transforms → relink), matching the OLLVM paper
  [1][3][4]. Independent convergence is strong evidence.
- **Medium confidence** on relative-difficulty claims (e.g. "jump tables are among the
  hardest parts"): these are credible primary accounts [1][3] but subjective and not
  independently benchmarked.
- **Author-admitted weaknesses:** es3n1n's z3 opaque predicates are easily optimized
  away [1 §"Bogus control flow"], and its decompiler-crashing tricks exploit specific
  tool versions and are fragile [1 §"Breaking decompilers"]. Treat these as
  illustrative, not durable.
- **Gaps not covered by these sources:**
  - *Empirical strength and overhead.* Neither project measures how much
    reverse-engineering effort each transform adds, nor runtime/size overhead
    (see §6.3). Quantifying this needs deobfuscation research I did not retrieve.
  - *Deobfuscation resistance.* Modern SMT-based MBA simplification and symbolic
    execution actively defeat naive substitution and opaque predicates [1]; a serious
    project must assume the attacker runs these.
  - *Architectures beyond x86-64 PE.* binprotect is x64-PE only, with ELF "planned"
    [3 §1]; es3n1n covers x86/x64 PE [1 §"Architecture"]. ARM/ARM64 and ELF specifics
    are out of scope in both.
  - *Legal/ethical context.* binprotect explicitly frames its anti-analysis
    properties as educational/research only [3 §1] — relevant because these techniques
    overlap with malware packing.

---

## References

1. es3n1n, "Obfuscating native code for fun: part 1 — introduction."
   https://blog.es3n1n.eu/posts/obfuscator-pt-1/ (primary author write-up; section
   names in citations match the article's headings).
2. es3n1n/obfuscator, project README and source.
   https://github.com/es3n1n/obfuscator
3. noahware/binprotect, project README (technical document) and source; section
   numbers in citations match the README's numbered contents.
   https://github.com/noahware/binprotect
4. P. Junod, J. Rinaldini, J. Wehrli, J. Michielin, "Obfuscator-LLVM — Software
   Protection for the Masses," 2015 (original OLLVM paper).
   https://crypto.junod.info/spro15.pdf
5. eshard/obfuscator-llvm, out-of-tree LLVM obfuscation pass plugin.
   https://github.com/eshard/obfuscator-llvm
6. eShard, "Obfuscation of Rust code with LLVM" (OLLVM history and plugin usage).
   https://www.eshard.com/blog/rust-code-obfuscation
7. wwh1004/ollvm-16, OLLVM fork for LLVM 16 (classic -mllvm flags and build steps).
   https://github.com/wwh1004/ollvm-16
8. und3ath/ollvm, modern OLLVM fork (LLVM 21/22; deterministic seeding, IR budgeting,
   annotation-driven config). https://github.com/und3ath/ollvm
9. MaxXor/obfuscator-llvm, OLLVM as an out-of-tree extension (language/target
   compatibility list). https://github.com/MaxXor/obfuscator-llvm
