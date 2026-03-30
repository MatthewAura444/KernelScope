/*
 * proc_maps.cpp — /proc/<pid>/maps parser for KernelScope.
 *
 * Reads the virtual memory mappings of a process so that raw user-space
 * instruction pointers can be mapped to the correct ELF binary and
 * file-relative offset for symbol lookup.
 */

#include "symbolization/proc_maps.hpp"

#include <cstdio>
#include <fstream>
#include <string>

namespace kscope {

ProcMaps::ProcMaps(uint32_t pid) : pid_(pid) {}

bool ProcMaps::load() {
    mappings_.clear();
    std::string path = "/proc/" + std::to_string(pid_) + "/maps";
    std::ifstream f(path);
    if (!f.is_open())
        return false;

    std::string line;
    while (std::getline(f, line)) {
        MemoryMapping m{};
        char perms[5] = {};
        char pathname[4096] = {};
        unsigned long start, end, offset;
        unsigned dev_major, dev_minor;
        unsigned long inode;

        int n = std::sscanf(line.c_str(), "%lx-%lx %4s %lx %x:%x %lu %4095s",
                            &start, &end, perms, &offset,
                            &dev_major, &dev_minor, &inode, pathname);
        if (n < 7)
            continue;

        if (perms[2] != 'x')
            continue;

        m.start    = start;
        m.end      = end;
        m.offset   = offset;
        m.pathname = (n >= 8) ? pathname : "";

        if (!m.pathname.empty() && m.pathname[0] == '[')
            continue;

        mappings_.push_back(std::move(m));
    }
    return !mappings_.empty();
}

const MemoryMapping* ProcMaps::find(uint64_t addr) const {
    for (const auto& m : mappings_) {
        if (addr >= m.start && addr < m.end)
            return &m;
    }
    return nullptr;
}

}
