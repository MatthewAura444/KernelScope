/*
 * symbolizer.hpp — Address-to-symbol resolution for KernelScope.
 *
 * Orchestrates proc_maps, elf_cache, and /proc/kallsyms to resolve
 * raw instruction pointers into human-readable function names.
 * Caches proc maps per-PID and kernel symbols globally.
 */

#pragma once

#include "symbolization/elf_cache.hpp"
#include "symbolization/proc_maps.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace kscope {

struct ResolvedFrame {
    uint64_t addr;
    std::string symbol;
    std::string binary;
    uint64_t offset;
    bool resolved;
};

struct KernelSymbol {
    uint64_t addr;
    std::string name;
};

class Symbolizer {
public:
    Symbolizer();

    ResolvedFrame resolve_user(uint32_t pid, uint64_t addr);
    ResolvedFrame resolve_kernel(uint64_t addr);

    std::vector<ResolvedFrame> resolve_stack(
        uint32_t pid, const std::vector<uint64_t>& addrs, bool is_kernel);

    void clear_cache();

private:
    ProcMaps& get_proc_maps(uint32_t pid);
    void load_kallsyms();

    ElfCache elf_cache_;
    std::unordered_map<uint32_t, ProcMaps> proc_cache_;
    std::vector<KernelSymbol> kallsyms_;
    bool kallsyms_loaded_ = false;
};

}
