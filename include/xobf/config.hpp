#pragma once

#if defined(_WIN32) || defined(_WIN64)
#define XOBF_OS_WINDOWS 1
#elif defined(__APPLE__)
#define XOBF_OS_MACOS 1
#elif defined(__linux__)
#define XOBF_OS_LINUX 1
#else
#define XOBF_OS_UNKNOWN 1
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define XOBF_ARCH_X64 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#define XOBF_ARCH_ARM64 1
#else
#define XOBF_ARCH_UNKNOWN 1
#endif

#if defined(_MSC_VER)
#define XOBF_COMPILER_MSVC 1
#elif defined(__clang__)
#define XOBF_COMPILER_CLANG 1
#elif defined(__GNUC__)
#define XOBF_COMPILER_GCC 1
#endif

#ifndef XOBF_LEVEL
#define XOBF_LEVEL 2
#endif

#if XOBF_LEVEL < 0 || XOBF_LEVEL > 3
#error "XOBF_LEVEL must be between 0 and 3"
#endif

#if XOBF_LEVEL >= 1
#define XOBF_MBA_DEPTH 2
#define XOBF_STR_ROUNDS 1
#elif XOBF_LEVEL == 0
#define XOBF_MBA_DEPTH 0
#define XOBF_STR_ROUNDS 1
#endif

#if XOBF_LEVEL >= 2
#undef XOBF_MBA_DEPTH
#undef XOBF_STR_ROUNDS
#define XOBF_MBA_DEPTH 3
#define XOBF_STR_ROUNDS 2
#endif

#if XOBF_LEVEL >= 3
#undef XOBF_MBA_DEPTH
#undef XOBF_STR_ROUNDS
#define XOBF_MBA_DEPTH 5
#define XOBF_STR_ROUNDS 3
#endif

#ifndef XOBF_VAULT_SLOTS
#define XOBF_VAULT_SLOTS 4
#endif
