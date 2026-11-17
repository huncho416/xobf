#pragma once

#include <cstdint>
#include <cstring>

#include "config.hpp"
#include "random.hpp"
#include "string.hpp"

#if defined(XOBF_OS_LINUX) || defined(XOBF_OS_MACOS)
#include <dlfcn.h>
#endif

#if defined(XOBF_OS_WINDOWS)
extern "C" __declspec(dllimport) void* __stdcall GetModuleHandleA(const char*);
extern "C" __declspec(dllimport) void* __stdcall LoadLibraryA(const char*);
extern "C" __declspec(dllimport) void* __stdcall GetProcAddress(void*,
                                                                const char*);
#endif

namespace xobf {

using module_t = void*;

inline module_t main_module() {
#if defined(XOBF_OS_LINUX) || defined(XOBF_OS_MACOS)
    return RTLD_DEFAULT;
#elif defined(XOBF_OS_WINDOWS)
    return GetModuleHandleA(nullptr);
#else
    return nullptr;
#endif
}

inline module_t open_module(const char* name) {
#if defined(XOBF_OS_LINUX) || defined(XOBF_OS_MACOS)
    return ::dlopen(name, RTLD_NOW | RTLD_GLOBAL);
#elif defined(XOBF_OS_WINDOWS)
    return LoadLibraryA(name);
#else
    (void)name;
    return nullptr;
#endif
}

inline void* raw_resolve(module_t module, const char* name) {
#if defined(XOBF_OS_LINUX) || defined(XOBF_OS_MACOS)
    return ::dlsym(module, name);
#elif defined(XOBF_OS_WINDOWS)
    return GetProcAddress(module, name);
#else
    (void)module;
    (void)name;
    return nullptr;
#endif
}

template <typename Fn>
inline Fn resolve_api(module_t module, const char* name) {
    void* sym = raw_resolve(module, name);
    Fn fn{};
    std::memcpy(&fn, &sym, sizeof(fn) <= sizeof(sym) ? sizeof(fn) : sizeof(sym));
    return fn;
}

constexpr std::uint64_t api_hash(const char* s) { return detail::fnv1a(s); }

}  // namespace xobf

#define XOBF_API(FnType, module, name)                        \
    ([&]() -> FnType {                                        \
        auto xobf_nm = XOBF_STR_STACK(name);                 \
        return ::xobf::resolve_api<FnType>((module),         \
                                           xobf_nm.c_str());  \
    }())

#define XOBF_API_MAIN(FnType, name) \
    XOBF_API(FnType, ::xobf::main_module(), name)
