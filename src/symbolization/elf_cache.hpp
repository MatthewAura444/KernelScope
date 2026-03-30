/*
 * elf_cache.hpp — ELF symbol table cache for KernelScope.
 *
 * Caches parsed ELF symbol tables to avoid repeated disk I/O during
 * stack symbolization.  Keyed by file path.  For PIE/shared objects,
 * also caches the ELF base virtual address (lowest PT_LOAD p_vaddr)
 * so callers can compute correct symbol offsets.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace kscope {

struct ElfSymbol {
    std::string name;
    uint64_t addr;
    uint64_t size;
};

struct ElfInfo {
    std::vector<ElfSymbol> symbols;
    uint64_t load_base;
    bool is_pie;
};

class ElfCache {
public:
    explicit ElfCache(size_t max_entries = 10000);

    std::optional<ElfSymbol> resolve(const std::string& path, uint64_t addr_offset);
    const ElfInfo* get_info(const std::string& path);
    void clear();
    size_t size() const { return cache_.size(); }

private:
    const ElfInfo& ensure_loaded(const std::string& path);

    size_t max_entries_;
    std::unordered_map<std::string, ElfInfo> cache_;
};

}
