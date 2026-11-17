#pragma once

#include <cstddef>
#include <cstdint>

#include "config.hpp"
#include "random.hpp"

#if defined(XOBF_OS_LINUX)
#include <link.h>
#endif

namespace xobf::api {

#if defined(XOBF_OS_LINUX)

namespace detail {

struct resolve_ctx {
    std::uint64_t target;
    void* result;
};

inline std::uint64_t name_hash(const char* s) {
    std::uint64_t h = 0xcbf29ce484222325ull;
    while (*s) {
        h ^= static_cast<std::uint64_t>(static_cast<unsigned char>(*s));
        h *= 0x100000001b3ull;
        ++s;
    }
    return h;
}

inline std::uint32_t gnu_hash_symcount(const std::uint32_t* gh) {
    std::uint32_t nbuckets = gh[0];
    std::uint32_t symoffset = gh[1];
    std::uint32_t bloom_size = gh[2];
    const ElfW(Addr)* bloom = reinterpret_cast<const ElfW(Addr)*>(&gh[4]);
    const std::uint32_t* buckets =
        reinterpret_cast<const std::uint32_t*>(&bloom[bloom_size]);
    const std::uint32_t* chain = &buckets[nbuckets];
    std::uint32_t last = 0;
    for (std::uint32_t i = 0; i < nbuckets; ++i)
        if (buckets[i] > last)
            last = buckets[i];
    if (last < symoffset)
        return symoffset;
    while (!(chain[last - symoffset] & 1u))
        ++last;
    return last + 1;
}

inline int phdr_cb(struct dl_phdr_info* info, std::size_t, void* data) {
    resolve_ctx* ctx = static_cast<resolve_ctx*>(data);
    const ElfW(Dyn)* dyn = nullptr;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        if (info->dlpi_phdr[i].p_type == PT_DYNAMIC) {
            dyn = reinterpret_cast<const ElfW(Dyn)*>(info->dlpi_addr +
                                                     info->dlpi_phdr[i].p_vaddr);
            break;
        }
    }
    if (!dyn)
        return 0;

    const char* strtab = nullptr;
    const ElfW(Sym)* symtab = nullptr;
    const std::uint32_t* gnu_hash = nullptr;
    const std::uint32_t* elf_hash = nullptr;

    for (const ElfW(Dyn)* d = dyn; d->d_tag != DT_NULL; ++d) {
        ElfW(Addr) p = d->d_un.d_ptr;
        if (p && p < info->dlpi_addr)
            p += info->dlpi_addr;
        if (d->d_tag == DT_STRTAB)
            strtab = reinterpret_cast<const char*>(p);
        else if (d->d_tag == DT_SYMTAB)
            symtab = reinterpret_cast<const ElfW(Sym)*>(p);
        else if (d->d_tag == DT_GNU_HASH)
            gnu_hash = reinterpret_cast<const std::uint32_t*>(p);
        else if (d->d_tag == DT_HASH)
            elf_hash = reinterpret_cast<const std::uint32_t*>(p);
    }
    if (!strtab || !symtab)
        return 0;

    std::uint32_t count = 0;
    if (elf_hash)
        count = elf_hash[1];
    else if (gnu_hash)
        count = gnu_hash_symcount(gnu_hash);
    else
        return 0;

    for (std::uint32_t i = 0; i < count; ++i) {
        const ElfW(Sym)& sym = symtab[i];
        if (sym.st_name == 0 || sym.st_value == 0)
            continue;
        if (sym.st_shndx == SHN_UNDEF)
            continue;
        if (name_hash(strtab + sym.st_name) == ctx->target) {
            ctx->result =
                reinterpret_cast<void*>(info->dlpi_addr + sym.st_value);
            return 1;
        }
    }
    return 0;
}

}  // namespace detail

inline void* resolve(std::uint64_t name_hash) {
    detail::resolve_ctx ctx{name_hash, nullptr};
    dl_iterate_phdr(&detail::phdr_cb, &ctx);
    return ctx.result;
}

#elif defined(XOBF_OS_WINDOWS)

extern "C" unsigned __int64 __readgsqword(unsigned long);

namespace detail {

inline std::uint64_t name_hash(const char* s) {
    std::uint64_t h = 0xcbf29ce484222325ull;
    while (*s) {
        h ^= static_cast<std::uint64_t>(static_cast<unsigned char>(*s));
        h *= 0x100000001b3ull;
        ++s;
    }
    return h;
}

inline void* resolve_in_module(unsigned char* base, std::uint64_t target) {
    std::uint32_t pe = *reinterpret_cast<std::uint32_t*>(base + 0x3C);
    unsigned char* nt = base + pe;
    std::uint32_t dir =
        *reinterpret_cast<std::uint32_t*>(nt + 0x18 + 0x70);
    if (dir == 0)
        return nullptr;
    unsigned char* exp = base + dir;
    std::uint32_t nnames = *reinterpret_cast<std::uint32_t*>(exp + 0x18);
    std::uint32_t names_rva = *reinterpret_cast<std::uint32_t*>(exp + 0x20);
    std::uint32_t ords_rva = *reinterpret_cast<std::uint32_t*>(exp + 0x24);
    std::uint32_t funcs_rva = *reinterpret_cast<std::uint32_t*>(exp + 0x1C);
    std::uint32_t* names = reinterpret_cast<std::uint32_t*>(base + names_rva);
    std::uint16_t* ords = reinterpret_cast<std::uint16_t*>(base + ords_rva);
    std::uint32_t* funcs = reinterpret_cast<std::uint32_t*>(base + funcs_rva);
    for (std::uint32_t i = 0; i < nnames; ++i) {
        const char* nm = reinterpret_cast<const char*>(base + names[i]);
        if (name_hash(nm) == target)
            return reinterpret_cast<void*>(base + funcs[ords[i]]);
    }
    return nullptr;
}

}  // namespace detail

inline void* resolve(std::uint64_t name_hash) {
    unsigned char* peb =
        reinterpret_cast<unsigned char*>(__readgsqword(0x60));
    unsigned char* ldr = *reinterpret_cast<unsigned char**>(peb + 0x18);
    unsigned char* head = ldr + 0x10;
    unsigned char* cur = *reinterpret_cast<unsigned char**>(head);
    while (cur != head) {
        unsigned char* entry = cur - 0x10;
        unsigned char* base =
            *reinterpret_cast<unsigned char**>(entry + 0x30);
        if (base) {
            void* r = detail::resolve_in_module(base, name_hash);
            if (r)
                return r;
        }
        cur = *reinterpret_cast<unsigned char**>(cur);
    }
    return nullptr;
}

#else

inline void* resolve(std::uint64_t) { return nullptr; }

#endif

template <typename Fn>
inline Fn resolve_as(std::uint64_t name_hash) {
    return reinterpret_cast<Fn>(resolve(name_hash));
}

}  // namespace xobf::api

#define XOBF_API_HASH(s) (::xobf::detail::fnv1a(s))

#define XOBF_RESOLVE(s)                                         \
    ([]() {                                                     \
        constexpr std::uint64_t xobf_h = ::xobf::detail::fnv1a(s); \
        return ::xobf::api::resolve(xobf_h);                   \
    }())

#define XOBF_RESOLVE_AS(Fn, s)                                 \
    ([]() {                                                     \
        constexpr std::uint64_t xobf_h = ::xobf::detail::fnv1a(s); \
        return ::xobf::api::resolve_as<Fn>(xobf_h);            \
    }())
