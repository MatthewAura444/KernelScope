# KernelScope — eBPF latency profiler for Linux processes.
#
# Top-level CMake build.  Orchestrates:
#   1. BPF program compilation  (clang → .bpf.o)
#   2. Skeleton generation      (bpftool gen skeleton → .skel.h)
#   3. C++23 user-space binary  (kscope)
#
# Usage:
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build -j$(nproc)

cmake_minimum_required(VERSION 3.22)

project(KernelScope
    VERSION   1.0.0
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── CMake modules ───────────────────────────────────────────────────

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(BpfHelpers)

# ── Find dependencies ──────────────────────────────────────────────

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBBPF REQUIRED libbpf>=1.0)
pkg_check_modules(LIBELF REQUIRED libelf)
pkg_check_modules(ZLIB   REQUIRED zlib)

find_program(BPFTOOL bpftool REQUIRED)
find_program(CLANG   clang   REQUIRED)

# ── Fetch header-only dependencies ─────────────────────────────────

include(FetchContent)

FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.2
)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v5.0.0
)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)

FetchContent_MakeAvailable(cli11 tomlplusplus nlohmann_json ftxui googletest)

# ── BPF compilation ────────────────────────────────────────────────

set(BPF_SOURCE "${CMAKE_SOURCE_DIR}/bpf/kscope.bpf.c")
set(BPF_OBJECT "${CMAKE_BINARY_DIR}/kscope.bpf.o")
set(BPF_SKEL   "${CMAKE_BINARY_DIR}/kscope.skel.h")

bpf_compile(
    SOURCE  "${BPF_SOURCE}"
    OUTPUT  "${BPF_OBJECT}"
    INCLUDE "${CMAKE_SOURCE_DIR}/bpf"
)

bpf_gen_skeleton(
    OBJECT  "${BPF_OBJECT}"
    OUTPUT  "${BPF_SKEL}"
)

# ── User-space binary ──────────────────────────────────────────────

set(KSCOPE_SOURCES
    src/main.cpp
    src/cli/parse_args.cpp
    src/cli/commands.cpp
    src/bpf/loader.cpp
    src/bpf/skeleton_bridge.cpp
    src/bpf/collector.cpp
    src/core/event_decoder.cpp
    src/core/aggregator.cpp
    src/core/classifications.cpp
    src/core/windows.cpp
    src/core/histograms.cpp
    src/symbolization/proc_maps.cpp
    src/symbolization/elf_cache.cpp
    src/symbolization/symbolizer.cpp
    src/symbolization/stack_formatter.cpp
    src/tui/app.cpp
    src/tui/screens.cpp
    src/tui/tables.cpp
    src/export/json_export.cpp
    src/export/otel_export.cpp
    src/export/prometheus_export.cpp
    src/record/recorder.cpp
    src/record/replay.cpp
    src/record/format.cpp
    src/util/logging.cpp
    src/util/errors.cpp
    src/util/time.cpp
    src/util/pidns.cpp
)

add_executable(kscope ${KSCOPE_SOURCES})

target_include_directories(kscope PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/bpf"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_BINARY_DIR}"          # for kscope.skel.h
    ${LIBBPF_INCLUDE_DIRS}
)

target_link_libraries(kscope PRIVATE
    ${LIBBPF_LIBRARIES}
    ${LIBELF_LIBRARIES}
    ${ZLIB_LIBRARIES}
    CLI11::CLI11
    tomlplusplus::tomlplusplus
    nlohmann_json::nlohmann_json
    ftxui::screen
    ftxui::dom
    ftxui::component
    pthread
)

target_link_directories(kscope PRIVATE
    ${LIBBPF_LIBRARY_DIRS}
    ${LIBELF_LIBRARY_DIRS}
    ${ZLIB_LIBRARY_DIRS}
)

# Skeleton header must be generated before compiling user-space
add_dependencies(kscope bpf_skeleton)

target_compile_options(kscope PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

# ── Tests ──────────────────────────────────────────────────────────

enable_testing()

file(GLOB_RECURSE TEST_SOURCES "test/unit/*.cpp")
if(TEST_SOURCES)
    add_executable(kscope_tests ${TEST_SOURCES}
        src/core/event_decoder.cpp
        src/core/classifications.cpp
        src/core/aggregator.cpp
        src/core/windows.cpp
        src/core/histograms.cpp
        src/record/format.cpp
        src/util/logging.cpp
        src/util/errors.cpp
        src/util/time.cpp
    )

    target_include_directories(kscope_tests PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/bpf"
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_BINARY_DIR}"
    )

    target_link_libraries(kscope_tests PRIVATE
        GTest::gtest_main
        nlohmann_json::nlohmann_json
        tomlplusplus::tomlplusplus
    )

    include(GoogleTest)
    gtest_discover_tests(kscope_tests)
endif()

# ── Install ────────────────────────────────────────────────────────

install(TARGETS kscope RUNTIME DESTINATION bin)
