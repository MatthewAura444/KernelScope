/*
 * symbolizer.cpp — Full address-to-symbol resolution for KernelScope.
 *
 * Resolves user-space addresses via /proc/pid/maps + ELF symbols,
 * and kernel addresses via /proc/kallsyms.  Results are cached
 * aggressively.  On failure, returns raw hex addresses (graceful
 * degradation).
 *
 * PIE binary handling: modern Linux executables are ET_DYN (position-
 * independent).  For these, the runtime virtual address must be
 * translated to the ELF virtual address using:
 *   elf_vaddr = runtime_addr - mapping_start + mapping_offset + elf_load_base
 * where elf_load_base is the lowest PT_LOAD p_vaddr from the ELF headers.
 */

#include "symbolization/symbolizer.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

namespace kscope {

Symbolizer::Symbolizer() : elf_cache_(10000) {}

ProcMaps& Symbolizer::get_proc_maps(uint32_t pid) {
    auto it = proc_cache_.find(pid);
    if (it == proc_cache_.end()) {
        auto [ins, _] = proc_cache_.emplace(pid, ProcMaps(pid));
        ins->second.load();
        return ins->second;
    }
    return it->second;
}

void Symbolizer::load_kallsyms() {
    if (kallsyms_loaded_) return;
    kallsyms_loaded_ = true;

    std::ifstream f("/proc/kallsyms");
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line)) {
        uint64_t addr;
        char type;
        char name[256] = {};
        if (std::sscanf(line.c_str(), "%lx %c %255s", &addr, &type, name) >= 3) {
            if (addr != 0 && (type == 'T' || type == 't'))
                kallsyms_.push_back({addr, name});
        }
    }
    std::sort(kallsyms_.begin(), kallsyms_.end(),
              [](const KernelSymbol& a, const KernelSymbol& b) {
                  return a.addr < b.addr;
              });
}

ResolvedFrame Symbolizer::resolve_user(uint32_t pid, uint64_t addr) {
    if (addr == 0)
        return {0, "[unknown]", "", 0, false};

    auto& maps = get_proc_maps(pid);
    const auto* mapping = maps.find(addr);
    if (!mapping)
        return {addr, std::format("0x{:x}", addr), "", 0, false};

    const auto* info = elf_cache_.get_info(mapping->pathname);
    uint64_t elf_vaddr;
    if (info) {
        elf_vaddr = addr - mapping->start + mapping->offset + info->load_base;
    } else {
        elf_vaddr = addr - mapping->start + mapping->offset;
    }

    auto sym = elf_cache_.resolve(mapping->pathname, elf_vaddr);

    if (sym) {
        uint64_t off = elf_vaddr - sym->addr;
        std::string basename = std::filesystem::path(mapping->pathname).filename().string();
        return {addr, sym->name, basename, off, true};
    }

    std::string basename = std::filesystem::path(mapping->pathname).filename().string();
    return {addr, std::format("0x{:x}", elf_vaddr), basename, elf_vaddr, false};
}

ResolvedFrame Symbolizer::resolve_kernel(uint64_t addr) {
    if (addr == 0)
        return {0, "[unknown]", "[kernel]", 0, false};

    load_kallsyms();

    if (kallsyms_.empty())
        return {addr, std::format("0x{:x}", addr), "[kernel]", 0, false};

    auto it = std::upper_bound(
        kallsyms_.begin(), kallsyms_.end(), addr,
        [](uint64_t a, const KernelSymbol& s) { return a < s.addr; });

    if (it != kallsyms_.begin()) {
        --it;
        uint64_t off = addr - it->addr;
        return {addr, it->name, "[kernel]", off, true};
    }

    return {addr, std::format("0x{:x}", addr), "[kernel]", 0, false};
}

std::vector<ResolvedFrame> Symbolizer::resolve_stack(
        uint32_t pid, const std::vector<uint64_t>& addrs, bool is_kernel) {
    std::vector<ResolvedFrame> frames;
    frames.reserve(addrs.size());
    for (auto addr : addrs)
        frames.push_back(is_kernel ? resolve_kernel(addr) : resolve_user(pid, addr));
    return frames;
}

void Symbolizer::clear_cache() {
    elf_cache_.clear();
    proc_cache_.clear();
    kallsyms_.clear();
    kallsyms_loaded_ = false;
}

}
