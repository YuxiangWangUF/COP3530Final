# =============================================================================
# IMDb Top-250 Recommender -- root CMake script
# =============================================================================
# Build:         cmake -B build -S .
#                cmake --build build --config Release
# Run tests:     ctest --test-dir build --output-on-failure
# Benchmarks:    cmake -B build -S . -DIMDB_BUILD_BENCHMARKS=ON
#                cmake --build build --target bench_heap bench_rbtree
#
# Layering
# --------
#   * foundation-and-data owns:  src/loader.cpp, Movie.h, Loader.h,
#                                data/sample.tsv, data/README.md,
#                                tests/test_loader.cpp, docs/foundation.md.
#   * core-algorithms owns:      include/imdb/MinHeap.h,
#                                include/imdb/RedBlackTree.h,
#                                tests/test_minheap.cpp,
#                                tests/test_rbtree.cpp.
#   * Shared (contract-locked, see docs/foundation.md): Movie.h, Loader.h,
#                                loader.cpp, this CMakeLists.txt.
#
# Tested toolchains
# -----------------
#   MSVC v19.50 (Visual Studio 18 BuildTools, x64)
#   GCC 11+ / Clang 14+ (Linux, macOS) -- CI matrix
# =============================================================================

cmake_minimum_required(VERSION 3.20)

project(imdb_top250
    VERSION 0.1.0
    DESCRIPTION "IMDb Top-250 recommender built from scratch in modern C++17"
    LANGUAGES CXX
)

# -----------------------------------------------------------------------------
# Compile settings -- C++17, strict warnings, -Werror.
# -----------------------------------------------------------------------------
set(IMDB_CXX_STANDARD 17 CACHE STRING "C++ standard")
set(CMAKE_CXX_STANDARD         ${IMDB_CXX_STANDARD} CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_REQUIRED ON  CACHE BOOL "" FORCE)
set(CMAKE_CXX_EXTENSIONS        OFF CACHE BOOL "" FORCE)

if(MSVC)
    # MSVC equivalents of -Wall -Wextra -Wpedantic -Werror:
    #   /W4          highest regular warning level (== -Wall -Wextra)
    #   /WX          warnings are errors             (== -Werror)
    #   /permissive- enforce standards conformance  (== -Wpedantic)
    set(IMDB_WARN_FLAGS /W4 /WX /permissive-)
    # Silence MSVC noise from STL headers inside third-party sources
    # (gtest / gbench) without weakening our own warnings.
    add_compile_options(/experimental:external /external:W0)
    # Source files are read as UTF-8 -- non-ASCII string literals would
    # otherwise be mis-interpreted under the active ANSI codepage.
    add_compile_options(/utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    set(IMDB_WARN_FLAGS -Wall -Wextra -Wpedantic -Werror)
endif()

# -----------------------------------------------------------------------------
# FetchContent -- GoogleTest + GoogleBenchmark.
# -----------------------------------------------------------------------------
option(IMDB_BUILD_TESTS      "Build unit tests"           ON)
option(IMDB_BUILD_BENCHMARKS "Build GoogleBench targets"  OFF)

include(FetchContent)

# GoogleTest -----------------------------------------------------------------
set(gtest_force_shared_crt    ON  CACHE BOOL "" FORCE)
set(INSTALL_GTEST            OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK               ON  CACHE BOOL "" FORCE)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# GoogleBenchmark ------------------------------------------------------------
set(BENCHMARK_ENABLE_TESTING      OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL       OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS  OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    googlebenchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.8.4.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

if(IMDB_BUILD_TESTS)
    FetchContent_MakeAvailable(googletest)
endif()
if(IMDB_BUILD_BENCHMARKS)
    FetchContent_MakeAvailable(googlebenchmark)
endif()

# -----------------------------------------------------------------------------
# Strip -Werror /WX from third-party targets. gtest and gbench emit
# warnings that are not our problem; we only enforce -Werror on code we own.
# Must run AFTER FetchContent_MakeAvailable so the targets exist.
# -----------------------------------------------------------------------------
function(imdb_relax_third_party_warnings)
    set(_third_party_targets
        gtest      gtest_main
        gmock      gmock_main
        benchmark  benchmark_main
    )
    foreach(tgt IN LISTS _third_party_targets)
        if(NOT TARGET ${tgt})
            continue()
        endif()
        get_target_property(_opts ${tgt} INTERFACE_COMPILE_OPTIONS)
        if(_opts)
            list(REMOVE_ITEM _opts "-Werror" "/WX")
            set_property(TARGET ${tgt} PROPERTY INTERFACE_COMPILE_OPTIONS "${_opts}")
        endif()
    endforeach()
endfunction()
imdb_relax_third_party_warnings()

# =============================================================================
# Core library
# =============================================================================
# IMDB_SOURCES is appended to by downstream tasks (core-algorithms adds
# src/min_heap.cpp / src/red_black_tree.cpp once they graduate out of
# header-only mode; benchmarks-and-cli adds src/cli.cpp).
set(IMDB_SOURCES
    src/loader.cpp
)

add_library(imdb_core STATIC ${IMDB_SOURCES})

target_include_directories(imdb_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(imdb_core PUBLIC cxx_std_${IMDB_CXX_STANDARD})
target_compile_options(imdb_core PRIVATE ${IMDB_WARN_FLAGS})

# =============================================================================
# Tests
# =============================================================================
if(IMDB_BUILD_TESTS)
    enable_testing()
    include(GoogleTest)

    # Foundation: loader.
    add_executable(test_loader tests/test_loader.cpp)
    target_link_libraries(test_loader PRIVATE imdb_core GTest::gtest_main)
    target_compile_options(test_loader PRIVATE ${IMDB_WARN_FLAGS})
    gtest_discover_tests(test_loader
        PROPERTIES
            ENVIRONMENT "IMDB_TEST_DATA=${CMAKE_CURRENT_SOURCE_DIR}/data"
    )

    # Core algorithms: MinHeap + RedBlackTree.
    add_executable(test_minheap tests/test_minheap.cpp)
    target_link_libraries(test_minheap PRIVATE imdb_core GTest::gtest_main)
    target_compile_options(test_minheap PRIVATE ${IMDB_WARN_FLAGS})
    gtest_discover_tests(test_minheap)

    add_executable(test_rbtree tests/test_rbtree.cpp)
    target_link_libraries(test_rbtree PRIVATE imdb_core GTest::gtest_main)
    target_compile_options(test_rbtree PRIVATE ${IMDB_WARN_FLAGS})
    gtest_discover_tests(test_rbtree)
endif()

# =============================================================================
# Benchmarks -- opt-in via -DIMDB_BUILD_BENCHMARKS=ON.
# =============================================================================
if(IMDB_BUILD_BENCHMARKS)
    add_executable(bench_heap benchmarks/bench_heap.cpp)
    target_link_libraries(bench_heap PRIVATE imdb_core benchmark::benchmark_main)

    add_executable(bench_rbtree benchmarks/bench_rbtree.cpp)
    target_link_libraries(bench_rbtree PRIVATE imdb_core benchmark::benchmark_main)
endif()

# =============================================================================
# Install
# =============================================================================
install(TARGETS imdb_core ARCHIVE DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

message(STATUS "============================================================")
message(STATUS " imdb_top250 ${PROJECT_VERSION}")
message(STATUS "   C++ standard         : ${IMDB_CXX_STANDARD}")
message(STATUS "   Build tests          : ${IMDB_BUILD_TESTS}")
message(STATUS "   Build benchmarks     : ${IMDB_BUILD_BENCHMARKS}")
message(STATUS "   Compiler             : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "============================================================")
