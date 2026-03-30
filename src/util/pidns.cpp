/*
 * pidns.cpp — PID namespace translation for KernelScope.
 *
 * Implements kernel ↔ namespace PID translation by parsing
 * /proc/<pid>/status NStgid lines.
 */

#include "util/pidns.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include "util/logging.hpp"

namespace kscope::pidns {

static bool parse_nstgid(uint32_t pid, uint32_t& kernel_pid_out) {
    std::string path = "/proc/" + std::to_string(pid) + "/status";
    std::ifstream f(path);
    if (!f.is_open())
        return false;

    std::string line;
    while (std::getline(f, line)) {
        if (line.compare(0, 7, "NStgid:") == 0) {
            uint32_t first_pid = 0;
            if (std::sscanf(line.c_str() + 7, " %u", &first_pid) == 1) {
                kernel_pid_out = first_pid;
                return true;
            }
        }
    }
    return false;
}

uint32_t to_kernel_pid(uint32_t ns_pid) {
    uint32_t kpid = 0;
    if (parse_nstgid(ns_pid, kpid) && kpid != ns_pid) {
        log::info("PID namespace detected: ns_pid={} → kernel_pid={}", ns_pid, kpid);
        return kpid;
    }
    return ns_pid;
}

uint32_t to_ns_pid(uint32_t kernel_pid) {
    if (std::filesystem::exists("/proc/" + std::to_string(kernel_pid) + "/maps"))
        return kernel_pid;

    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        if (!entry.is_directory())
            continue;

        const auto name = entry.path().filename().string();
        uint32_t pid = 0;
        if (std::sscanf(name.c_str(), "%u", &pid) != 1)
            continue;

        uint32_t kpid = 0;
        if (parse_nstgid(pid, kpid) && kpid == kernel_pid)
            return pid;
    }

    return 0;
}

bool has_pid_namespace() {
    std::ifstream f("/proc/self/status");
    if (!f.is_open())
        return false;

    std::string line;
    while (std::getline(f, line)) {
        if (line.compare(0, 7, "NStgid:") == 0) {
            int count = 0;
            const char* p = line.c_str() + 7;
            while (*p) {
                while (*p == ' ' || *p == '\t') p++;
                if (*p >= '0' && *p <= '9') {
                    count++;
                    while (*p >= '0' && *p <= '9') p++;
                } else {
                    break;
                }
            }
            return count > 1;
        }
    }
    return false;
}

}
