/*
 * errors.hpp — Error types and result wrapper for KernelScope.
 *
 * Uses std::expected (C++23) for error propagation without exceptions.
 * Defines KernelScope-specific error codes covering BPF, system, I/O,
 * and user-input failures.
 */

#pragma once

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <expected>
#include <string>
#include <system_error>

namespace kscope {

enum class ErrorCode : uint8_t {
    Ok = 0,
    BpfLoadFailed,
    BpfAttachFailed,
    BpfMapError,
    KernelTooOld,
    NoBtf,
    PermissionDenied,
    InvalidArgument,
    IoError,
    SymbolizationFailed,
    RecordFormatError,
    ConfigError,
    Unsupported,
    Internal,
};

struct Error {
    ErrorCode code;
    std::string message;

    Error(ErrorCode c, std::string msg)
        : code(c), message(std::move(msg)) {}

    explicit operator bool() const { return code != ErrorCode::Ok; }
};

template <typename T>
using Result = std::expected<T, Error>;

using Status = std::expected<void, Error>;

inline Error make_error(ErrorCode code, std::string msg) {
    return Error{code, std::move(msg)};
}

inline Error errno_error(ErrorCode code, const std::string& prefix) {
    return Error{code, prefix + ": " + std::string(std::strerror(errno))};
}

}
