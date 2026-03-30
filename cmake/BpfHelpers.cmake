# BpfHelpers.cmake — CMake functions for BPF compilation and skeleton generation.
#
# Provides:
#   bpf_compile(SOURCE <file> OUTPUT <file> INCLUDE <dir>)
#   bpf_gen_skeleton(OBJECT <file> OUTPUT <file>)
#
# These wrap clang (BPF target) and bpftool to integrate eBPF into
# the CMake build pipeline.

# ── Detect target architecture for BPF ──────────────────────────────

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
    set(BPF_ARCH "x86")
    set(BPF_ARCH_DEFINE "__TARGET_ARCH_x86")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    set(BPF_ARCH "arm64")
    set(BPF_ARCH_DEFINE "__TARGET_ARCH_arm64")
else()
    message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# ── bpf_compile ─────────────────────────────────────────────────────

function(bpf_compile)
    cmake_parse_arguments(BPF "" "SOURCE;OUTPUT;INCLUDE" "" ${ARGN})

    if(NOT BPF_SOURCE OR NOT BPF_OUTPUT)
        message(FATAL_ERROR "bpf_compile: SOURCE and OUTPUT are required")
    endif()

    # Collect all BPF headers for dependency tracking
    file(GLOB BPF_HEADERS "${BPF_INCLUDE}/*.h")

    add_custom_command(
        OUTPUT  "${BPF_OUTPUT}"
        COMMAND "${CLANG}"
            -g -O2
            -target bpf
            -D${BPF_ARCH_DEFINE}
            -I "${BPF_INCLUDE}"
            -c "${BPF_SOURCE}"
            -o "${BPF_OUTPUT}"
        DEPENDS "${BPF_SOURCE}" ${BPF_HEADERS}
        COMMENT "Compiling BPF program: ${BPF_SOURCE}"
        VERBATIM
    )

    add_custom_target(bpf_object DEPENDS "${BPF_OUTPUT}")
endfunction()

# ── bpf_gen_skeleton ────────────────────────────────────────────────

function(bpf_gen_skeleton)
    cmake_parse_arguments(SKEL "" "OBJECT;OUTPUT" "" ${ARGN})

    if(NOT SKEL_OBJECT OR NOT SKEL_OUTPUT)
        message(FATAL_ERROR "bpf_gen_skeleton: OBJECT and OUTPUT are required")
    endif()

    add_custom_command(
        OUTPUT  "${SKEL_OUTPUT}"
        COMMAND "${BPFTOOL}" gen skeleton "${SKEL_OBJECT}" > "${SKEL_OUTPUT}"
        DEPENDS "${SKEL_OBJECT}" bpf_object
        COMMENT "Generating BPF skeleton: ${SKEL_OUTPUT}"
        VERBATIM
    )

    add_custom_target(bpf_skeleton DEPENDS "${SKEL_OUTPUT}")
endfunction()
