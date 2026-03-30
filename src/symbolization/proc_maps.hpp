/*
 * proc_maps.hpp — /proc/<pid>/maps parser for KernelScope.
 *
 * Reads and caches the memory mappings of target processes so that
 * raw user-space addresses can be mapped to the correct ELF binary
 * and offset for symbolization.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace kscope {

struct MemoryMapping {
    uint64_t start;
    uint64_t end;
    uint64_t offset;
    std::string pathname;
};

class ProcMaps {
public:
    explicit ProcMaps(uint32_t pid);

    bool load();
    const MemoryMapping* find(uint64_t addr) const;
    const std::vector<MemoryMapping>& mappings() const { return mappings_; }

private:
    uint32_t pid_;
    std::vector<MemoryMapping> mappings_;
};

}
