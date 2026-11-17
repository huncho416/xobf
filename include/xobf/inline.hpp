// xobf/inline.hpp : cross-compiler inlining control.
// Forcing/ preventing inlining changes the shape of the generated code, which
// is useful when combining with the other passes.
#pragma once

#if defined(_MSC_VER)
#define XOBF_FORCEINLINE __forceinline
#define XOBF_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define XOBF_FORCEINLINE inline __attribute__((always_inline))
#define XOBF_NOINLINE __attribute__((noinline))
#else
#define XOBF_FORCEINLINE inline
#define XOBF_NOINLINE
#endif
