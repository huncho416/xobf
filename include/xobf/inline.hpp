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
