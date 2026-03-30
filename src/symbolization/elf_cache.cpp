/*
 * elf_cache.cpp — ELF symbol table cache for KernelScope.
 *
 * Parses ELF binaries to extract .symtab / .dynsym symbol tables and
 * caches the results keyed by file path.  Uses libelf for parsing.
 * Symbols are sorted by address for binary-search resolution.
 *
 * For PIE executables and shared libraries (ET_DYN), determines the
 * ELF base virtual address from PT_LOAD headers so callers can correctly
 * translate runtime addresses to symbol-relative offsets.
 */

#include "symbolization/elf_cache.hpp"

#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <gelf.h>
#include <libelf.h>
#include <unistd.h>

namespace kscope {

ElfCache::ElfCache(size_t max_entries) : max_entries_(max_entries) {}

static ElfInfo parse_elf(const std::string& path) {
    ElfInfo info{{}, 0, false};

    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return info;

    elf_version(EV_CURRENT);
    Elf* elf = elf_begin(fd, ELF_C_READ, nullptr);
    if (!elf) { close(fd); return info; }

    GElf_Ehdr ehdr;
    if (!gelf_getehdr(elf, &ehdr)) { elf_end(elf); close(fd); return info; }

    info.is_pie = (ehdr.e_type == ET_DYN);

    uint64_t min_vaddr = UINT64_MAX;
    size_t phnum = 0;
    elf_getphdrnum(elf, &phnum);
    for (size_t i = 0; i < phnum; i++) {
        GElf_Phdr phdr;
        if (!gelf_getphdr(elf, static_cast<int>(i), &phdr))
            continue;
        if (phdr.p_type == PT_LOAD && phdr.p_vaddr < min_vaddr)
            min_vaddr = phdr.p_vaddr;
    }
    info.load_base = (min_vaddr == UINT64_MAX) ? 0 : min_vaddr;

    Elf_Scn* scn = nullptr;
    while ((scn = elf_nextscn(elf, scn)) != nullptr) {
        GElf_Shdr shdr;
        if (!gelf_getshdr(scn, &shdr))
            continue;
        if (shdr.sh_type != SHT_SYMTAB && shdr.sh_type != SHT_DYNSYM)
            continue;

        Elf_Data* data = elf_getdata(scn, nullptr);
        if (!data) continue;

        size_t count = shdr.sh_size / shdr.sh_entsize;
        for (size_t i = 0; i < count; i++) {
            GElf_Sym sym;
            if (!gelf_getsym(data, static_cast<int>(i), &sym))
                continue;
            if (GELF_ST_TYPE(sym.st_info) != STT_FUNC)
                continue;
            if (sym.st_value == 0)
                continue;

            const char* name = elf_strptr(elf, shdr.sh_link, sym.st_name);
            if (!name || name[0] == '\0')
                continue;

            uint64_t size = sym.st_size;
            if (size == 0) size = 1;

            info.symbols.push_back({name, sym.st_value, size});
        }
    }

    elf_end(elf);
    close(fd);

    std::sort(info.symbols.begin(), info.symbols.end(),
              [](const ElfSymbol& a, const ElfSymbol& b) {
                  return a.addr < b.addr;
              });
    return info;
}

const ElfInfo& ElfCache::ensure_loaded(const std::string& path) {
    auto it = cache_.find(path);
    if (it != cache_.end())
        return it->second;

    auto info = parse_elf(path);
    auto [ins, _] = cache_.emplace(path, std::move(info));
    return ins->second;
}

std::optional<ElfSymbol> ElfCache::resolve(const std::string& path, uint64_t addr_offset) {
    if (cache_.size() >= max_entries_ && cache_.find(path) == cache_.end())
        return std::nullopt;

    const auto& info = ensure_loaded(path);
    const auto& syms = info.symbols;
    if (syms.empty())
        return std::nullopt;

    auto it = std::upper_bound(syms.begin(), syms.end(), addr_offset,
        [](uint64_t a, const ElfSymbol& s) { return a < s.addr; });

    if (it != syms.begin()) {
        --it;
        if (addr_offset >= it->addr && addr_offset < it->addr + it->size)
            return *it;
        if (addr_offset >= it->addr && addr_offset - it->addr < 0x10000)
            return ElfSymbol{it->name, it->addr, it->size};
    }

    return std::nullopt;
}

const ElfInfo* ElfCache::get_info(const std::string& path) {
    if (cache_.size() >= max_entries_ && cache_.find(path) == cache_.end())
        return nullptr;
    return &ensure_loaded(path);
}

void ElfCache::clear() {
    cache_.clear();
}

}
