/*
 * event_decoder.cpp — Ring buffer event decoder for KernelScope.
 *
 * Converts the raw struct event (BPF wire format) into the C++
 * kscope::Event.  Also provides name-lookup tables for event types,
 * latency classes, and syscall IDs (x86_64).
 */

#include "core/event_decoder.hpp"

#include <cstring>
#include <string>

#include "events.h"

namespace kscope {

std::optional<Event> decode_event(std::span<const std::byte> raw) {
    if (raw.size() < sizeof(struct event))
        return std::nullopt;

    const auto* src = reinterpret_cast<const struct event*>(raw.data());

    Event ev{};
    ev.timestamp_ns   = src->timestamp_ns;
    ev.pid            = src->pid;
    ev.tid            = src->tid;
    std::memcpy(ev.comm.data(), src->comm, kCommLen);
    ev.cpu            = src->cpu;
    ev.type           = static_cast<EventType>(src->type);
    ev.classification = static_cast<LatencyClass>(src->classification);
    ev.duration_ns    = src->duration_ns;
    ev.syscall_id     = src->syscall_id;
    ev.user_stack_id  = src->user_stack_id;
    ev.kern_stack_id  = src->kern_stack_id;
    ev.flags          = src->flags;

    return ev;
}

std::string_view event_type_name(EventType t) {
    switch (t) {
        case EventType::None:        return "none";
        case EventType::SyscallSlow: return "syscall_slow";
        case EventType::OffcpuSlice: return "offcpu_slice";
        case EventType::LockWait:    return "lock_wait";
        case EventType::IoWait:      return "io_wait";
        case EventType::ProcExec:    return "proc_exec";
        case EventType::ProcExit:    return "proc_exit";
        case EventType::LostSamples: return "lost_samples";
        case EventType::Diag:        return "diag";
    }
    return "unknown";
}

std::string_view latency_class_name(LatencyClass c) {
    switch (c) {
        case LatencyClass::Unknown:        return "unknown";
        case LatencyClass::OnCpu:          return "on_cpu";
        case LatencyClass::OffCpu:         return "off_cpu";
        case LatencyClass::SyscallBlocked: return "syscall_blocked";
        case LatencyClass::IoWait:         return "io_wait";
        case LatencyClass::LockWait:       return "lock_wait";
        case LatencyClass::UnknownWait:    return "unknown_wait";
    }
    return "unknown";
}

std::string syscall_name(uint64_t id) {
    static constexpr const char* table[] = {
        "read", "write", "open", "close", "stat", "fstat", "lstat", "poll", "lseek",
        "mmap", "mprotect", "munmap", "brk", "rt_sigaction", "rt_sigprocmask",
        "rt_sigreturn", "ioctl", "pread64", "pwrite64", "readv", "writev", "access",
        "pipe", "select", "sched_yield", "mremap", "msync", "mincore", "madvise",
        "shmget", "shmat", "shmctl", "dup", "dup2", "pause", "nanosleep", "getitimer",
        "alarm", "setitimer", "getpid", "sendfile", "socket", "connect", "accept",
        "sendto", "recvfrom", "sendmsg", "recvmsg", "shutdown", "bind", "listen",
        "getsockname", "getpeername", "socketpair", "setsockopt", "getsockopt", "clone",
        "fork", "vfork", "execve", "exit", "wait4", "kill", "uname", "semget", "semop",
        "semctl", "shmdt", "msgget", "msgsnd", "msgrcv", "msgctl", "fcntl", "flock",
        "fsync", "fdatasync", "truncate", "ftruncate", "getdents", "getcwd", "chdir",
        "fchdir", "rename", "mkdir", "rmdir", "creat", "link", "unlink", "symlink",
        "readlink", "chmod", "fchmod", "chown", "fchown", "lchown", "umask",
        "gettimeofday", "getrlimit", "getrusage", "sysinfo", "times", "ptrace", "getuid",
        "syslog", "getgid", "setuid", "setgid", "geteuid", "getegid", "setpgid",
        "getppid", "getpgrp", "setsid", "setreuid", "setregid", "getgroups", "setgroups",
        "setresuid", "getresuid", "setresgid", "getresgid", "getpgid", "setfsuid",
        "setfsgid", "getsid", "capget", "capset", "rt_sigpending", "rt_sigtimedwait",
        "rt_sigqueueinfo", "rt_sigsuspend", "sigaltstack", "utime", "mknod", "uselib",
        "personality", "ustat", "statfs", "fstatfs", "sysfs", "getpriority",
        "setpriority", "sched_setparam", "sched_getparam", "sched_setscheduler",
        "sched_getscheduler", "sched_get_priority_max", "sched_get_priority_min",
        "sched_rr_get_interval", "mlock", "munlock", "mlockall", "munlockall", "vhangup",
        "modify_ldt", "pivot_root", "_sysctl", "prctl", "arch_prctl", "adjtimex",
        "setrlimit", "chroot", "sync", "acct", "settimeofday", "mount", "umount2",
        "swapon", "swapoff", "reboot", "sethostname", "setdomainname", "iopl", "ioperm",
        "create_module", "init_module", "delete_module", "get_kernel_syms",
        "query_module", "quotactl", "nfsservctl", "getpmsg", "putpmsg", "afs_syscall",
        "tuxcall", "security", "gettid", "readahead", "setxattr", "lsetxattr",
        "fsetxattr", "getxattr", "lgetxattr", "fgetxattr", "listxattr", "llistxattr",
        "flistxattr", "removexattr", "lremovexattr", "fremovexattr", "tkill", "time",
        "futex", "sched_setaffinity", "sched_getaffinity", "set_thread_area", "io_setup",
        "io_destroy", "io_getevents", "io_submit", "io_cancel", "get_thread_area",
        "lookup_dcookie", "epoll_create", "epoll_ctl_old", "epoll_wait_old",
        "remap_file_pages", "getdents64", "set_tid_address", "restart_syscall",
        "semtimedop", "fadvise64", "timer_create", "timer_settime", "timer_gettime",
        "timer_getoverrun", "timer_delete", "clock_settime", "clock_gettime",
        "clock_getres", "clock_nanosleep", "exit_group", "epoll_wait", "epoll_ctl",
        "tgkill", "utimes", "vserver", "mbind", "set_mempolicy", "get_mempolicy",
        "mq_open", "mq_unlink", "mq_timedsend", "mq_timedreceive", "mq_notify",
        "mq_getsetattr", "kexec_load", "waitid", "add_key", "request_key", "keyctl",
        "ioprio_set", "ioprio_get", "inotify_init", "inotify_add_watch",
        "inotify_rm_watch", "migrate_pages", "openat", "mkdirat", "mknodat", "fchownat",
        "futimesat", "newfstatat", "unlinkat", "renameat", "linkat", "symlinkat",
        "readlinkat", "fchmodat", "faccessat", "pselect6", "ppoll", "unshare",
        "set_robust_list", "get_robust_list", "splice", "tee", "sync_file_range",
        "vmsplice", "move_pages", "utimensat", "epoll_pwait", "signalfd",
        "timerfd_create", "eventfd", "fallocate", "timerfd_settime", "timerfd_gettime",
        "accept4", "signalfd4", "eventfd2", "epoll_create1", "dup3", "pipe2",
        "inotify_init1", "preadv", "pwritev", "rt_tgsigqueueinfo", "perf_event_open",
        "recvmmsg", "fanotify_init", "fanotify_mark", "prlimit64", "name_to_handle_at",
        "open_by_handle_at", "clock_adjtime", "syncfs", "sendmmsg", "setns", "getcpu",
        "process_vm_readv", "process_vm_writev", "kcmp", "finit_module", "sched_setattr",
        "sched_getattr", "renameat2", "seccomp", "getrandom", "memfd_create",
        "kexec_file_load", "bpf", "execveat", "userfaultfd", "membarrier", "mlock2",
        "copy_file_range", "preadv2", "pwritev2", "pkey_mprotect", "pkey_alloc",
        "pkey_free", "statx", "io_pgetevents", "rseq", nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, "pidfd_send_signal", "io_uring_setup",
        "io_uring_enter", "io_uring_register", "open_tree", "move_mount", "fsopen",
        "fsconfig", "fsmount", "fspick", "pidfd_open", "clone3", "close_range", "openat2",
        "pidfd_getfd", "faccessat2", "process_madvise", "epoll_pwait2", "mount_setattr",
        "quotactl_fd", "landlock_create_ruleset", "landlock_add_rule",
        "landlock_restrict_self", "memfd_secret", "process_mrelease", "futex_waitv",
        "set_mempolicy_home_node", "cachestat", "fchmodat2", "map_shadow_stack",
        "futex_wake", "futex_wait", "futex_requeue", "statmount", "listmount",
        "lsm_get_self_attr", "lsm_set_self_attr", "lsm_list_modules",
    };
    static constexpr size_t table_size = sizeof(table) / sizeof(table[0]);

    if (id < table_size && table[id] != nullptr) {
        return table[id];
    }
    return "sys_" + std::to_string(id);
}

}
