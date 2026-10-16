cmake_minimum_required(VERSION 3.10)
project(nimcp VERSION 2.6.2 LANGUAGES C CXX)

# Set C/C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

#==============================================================================
# Security and Hardening Options
#==============================================================================

# Auto-enable sanitizers for Debug builds (can be overridden with -DENABLE_ASAN=OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(ASAN_DEFAULT ON)
    set(UBSAN_DEFAULT ON)
else()
    set(ASAN_DEFAULT OFF)
    set(UBSAN_DEFAULT OFF)
endif()

# Sanitizer options (mutually exclusive: ASAN vs TSAN)
option(ENABLE_ASAN "Enable AddressSanitizer for memory error detection" ${ASAN_DEFAULT})
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" ${UBSAN_DEFAULT})
option(ENABLE_TSAN "Enable ThreadSanitizer for race condition detection" OFF)
option(ENABLE_ALL_SANITIZERS "Enable all compatible sanitizers (ASAN+UBSAN+LEAK)" OFF)
option(ENABLE_HARDENING "Enable security hardening compiler flags" ON)
option(ENABLE_FUZZING "Enable fuzzing targets (requires libFuzzer)" OFF)
option(ENABLE_COVERAGE "Enable code coverage instrumentation (gcov/lcov)" OFF)

# ENABLE_ALL_SANITIZERS convenience option
if(ENABLE_ALL_SANITIZERS)
    set(ENABLE_ASAN ON)
    set(ENABLE_UBSAN ON)
    message(STATUS "All compatible sanitizers enabled (ASAN+UBSAN+LEAK)")
endif()

# Validate sanitizer options (ASAN and TSAN are mutually exclusive)
if(ENABLE_ASAN AND ENABLE_TSAN)
    message(FATAL_ERROR "AddressSanitizer and ThreadSanitizer are mutually exclusive. Choose one.")
endif()

# AddressSanitizer - detects memory errors (use-after-free, buffer overflows, etc.)
# Includes LeakSanitizer by default
if(ENABLE_ASAN)
    set(ASAN_FLAGS "-fsanitize=address -fsanitize=leak -fno-omit-frame-pointer -g -O1")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ASAN_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fsanitize=leak")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address -fsanitize=leak")
    message(STATUS "AddressSanitizer: ENABLED (includes LeakSanitizer)")
    message(STATUS "  - Detects: buffer overflows, use-after-free, double-free, memory leaks")
    message(STATUS "  - Performance: ~2x slowdown, 3x memory overhead")
endif()

# UndefinedBehaviorSanitizer - detects undefined behavior
if(ENABLE_UBSAN)
    set(UBSAN_FLAGS "-fsanitize=undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${UBSAN_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${UBSAN_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=undefined")
    message(STATUS "UndefinedBehaviorSanitizer: ENABLED")
    message(STATUS "  - Detects: integer overflow, null pointer, alignment, division by zero")
    message(STATUS "  - Performance: ~20% slowdown")
endif()

# ThreadSanitizer - detects data races (mutually exclusive with ASAN)
if(ENABLE_TSAN)
    set(TSAN_FLAGS "-fsanitize=thread -g")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TSAN_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TSAN_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread")
    message(STATUS "ThreadSanitizer: ENABLED")
    message(STATUS "  - Detects: data races, deadlocks in multi-threaded code")
    message(STATUS "  - Performance: ~5-15x slowdown, 5-10x memory overhead")
    message(STATUS "  - Note: Mutually exclusive with AddressSanitizer")
endif()

# Print sanitizer summary
message(STATUS "")
message(STATUS "========================================")
message(STATUS "Sanitizer Configuration Summary")
message(STATUS "========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
if(ENABLE_ASAN)
    message(STATUS "✓ AddressSanitizer + LeakSanitizer")
endif()
if(ENABLE_UBSAN)
    message(STATUS "✓ UndefinedBehaviorSanitizer")
endif()
if(ENABLE_TSAN)
    message(STATUS "✓ ThreadSanitizer")
endif()
if(NOT ENABLE_ASAN AND NOT ENABLE_UBSAN AND NOT ENABLE_TSAN)
    message(STATUS "✗ No sanitizers enabled")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(STATUS "  (Use -DENABLE_ASAN=OFF to keep disabled)")
    else()
        message(STATUS "  (Use -DENABLE_ASAN=ON to enable for ${CMAKE_BUILD_TYPE})")
    endif()
endif()
message(STATUS "========================================")
message(STATUS "")

# Security hardening flags (enabled by default in Release builds)
if(ENABLE_HARDENING)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        # Fortify source - compile-time and runtime buffer overflow detection
        add_definitions(-D_FORTIFY_SOURCE=2)

        # Stack protection - NOTE: -fPIC for shared libs, -fPIE for executables
        set(HARDENING_FLAGS
            -fstack-protector-strong      # Stack canary protection
            -Wformat                       # Format string vulnerability detection
            -Wformat-security              # Additional format string checks
            -fno-common                    # Prevent common symbol vulnerabilities
        )

        add_compile_options(${HARDENING_FLAGS})

        # Link-time hardening (executables only)
        if(NOT APPLE)  # Linux-specific hardening
            add_link_options(
                -Wl,-z,relro               # Read-only relocations
                -Wl,-z,now                 # Immediate binding (full RELRO)
                -Wl,-z,noexecstack         # Non-executable stack
            )
        endif()

        message(STATUS "Security hardening: ENABLED")
        message(STATUS "  - FORTIFY_SOURCE=2 (buffer overflow detection)")
        message(STATUS "  - Stack protector strong")
        message(STATUS "  - Position Independent Code (PIC/PIE)")
        message(STATUS "  - Full RELRO (Read-only relocations)")
    else()
        message(STATUS "Security hardening: PARTIAL (Debug build)")
    endif()
endif()

# Coverage instrumentation (gcov/lcov)
if(ENABLE_COVERAGE)
    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_C_COMPILER_ID MATCHES "GNU")
        set(COVERAGE_FLAGS "--coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")
        message(STATUS "Code Coverage: ENABLED")
        message(STATUS "  - Instrumentation: gcov")
        message(STATUS "  - Use lcov to generate HTML reports")
        message(STATUS "  - Run: lcov --capture --directory . --output-file coverage.info")
        message(STATUS "  - HTML: genhtml coverage.info --output-directory coverage_html")
    else()
        message(WARNING "Coverage only supported with GCC")
    endif()
endif()

# Compiler flags
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
endif()

# Find required packages
find_package(Python3 COMPONENTS Development REQUIRED)

# Find or fetch GTest for testing
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message(STATUS "GTest not found on system - fetching from GitHub")
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    # Create GTest::GTest and GTest::Main aliases
    add_library(GTest::GTest ALIAS gtest)
    add_library(GTest::Main ALIAS gtest_main)
    set(GTest_FOUND TRUE)
    message(STATUS "GTest fetched and configured successfully")
endif()

# Find LAPACK/BLAS (required for tensor decomposition)
find_package(LAPACK QUIET)
find_package(BLAS QUIET)

# If find_package failed, try manual search
if(NOT LAPACK_FOUND OR NOT BLAS_FOUND)
    find_library(LAPACK_LIBRARY NAMES lapack PATHS /usr/lib/x86_64-linux-gnu/lapack /usr/lib/x86_64-linux-gnu)
    find_library(BLAS_LIBRARY NAMES blas PATHS /usr/lib/x86_64-linux-gnu/blas /usr/lib/x86_64-linux-gnu)

    if(LAPACK_LIBRARY AND BLAS_LIBRARY)
        set(LAPACK_LIBRARIES ${LAPACK_LIBRARY})
        set(BLAS_LIBRARIES ${BLAS_LIBRARY})
        set(LAPACK_FOUND TRUE)
        set(BLAS_FOUND TRUE)
    endif()
endif()

if(LAPACK_FOUND AND BLAS_FOUND)
    set(LAPACK_BLAS_FOUND TRUE)
    add_definitions(-DNIMCP_ENABLE_LAPACK)
    message(STATUS "LAPACK/BLAS support ENABLED")
    message(STATUS "  LAPACK Libraries: ${LAPACK_LIBRARIES}")
    message(STATUS "  BLAS Libraries: ${BLAS_LIBRARIES}")
else()
    set(LAPACK_BLAS_FOUND FALSE)
    message(STATUS "LAPACK/BLAS not found - tensor decomposition will use simplified SVD")
    message(STATUS "  Install liblapack-dev and libblas-dev for optimal performance")
endif()

# Find CUDA (optional for GPU support)
include(CheckLanguage)
check_language(CUDA)
if(CMAKE_CUDA_COMPILER)
    # Set CUDA architectures before enabling language
    set(CMAKE_CUDA_ARCHITECTURES "75;80;86;89")  # Turing, Ampere, Ada Lovelace
    enable_language(CUDA)
    find_package(CUDAToolkit QUIET)
    if(CUDAToolkit_FOUND)
        add_definitions(-DNIMCP_ENABLE_CUDA)
        message(STATUS "CUDA support ENABLED")
        message(STATUS "  CUDA Toolkit: ${CUDAToolkit_VERSION}")
        message(STATUS "  CUDA Include: ${CUDAToolkit_INCLUDE_DIRS}")
        message(STATUS "  CUDA Libraries: ${CUDAToolkit_LIBRARY_DIR}")
        message(STATUS "  CUDA Compiler: ${CMAKE_CUDA_COMPILER}")
    else()
        message(STATUS "CUDA compiler found but toolkit incomplete - GPU support disabled")
    endif()
else()
    message(STATUS "CUDA not available - GPU support disabled")
endif()

# Find libsodium (optional for encryption support)
find_library(SODIUM_LIBRARY NAMES sodium libsodium)
find_path(SODIUM_INCLUDE_DIR NAMES sodium.h)
if(SODIUM_LIBRARY AND SODIUM_INCLUDE_DIR)
    set(SODIUM_FOUND TRUE)
    add_definitions(-DNIMCP_ENABLE_ENCRYPTION)
    message(STATUS "libsodium found: ${SODIUM_LIBRARY}")
    message(STATUS "  Sodium Include: ${SODIUM_INCLUDE_DIR}")
    message(STATUS "  Encryption support: ENABLED")
else()
    set(SODIUM_FOUND FALSE)
    message(STATUS "libsodium not found - encryption support disabled")
    message(STATUS "  Install libsodium-dev to enable encryption")
endif()

# Define project-wide variables
# With new hierarchical structure, headers are colocated with sources
set(NIMCP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(NIMCP_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/lib)
set(NIMCP_PYTHON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/python)
set(NIMCP_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)

# Enable testing (must be in top-level CMakeLists.txt BEFORE add_subdirectory)
enable_testing()

# Add subdirectories
add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(test)

# Add fuzzing targets if enabled
if(ENABLE_FUZZING)
    add_subdirectory(src/fuzz)
endif()

# Install rules for headers
include(GNUInstallDirs)
install(DIRECTORY ${NIMCP_INCLUDE_DIR}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nimcp
        FILES_MATCHING PATTERN "*.h")

# Generate and install pkg-config file
configure_file(nimcp.pc.in nimcp.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/nimcp.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# Export targets
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/NIMCPConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/NIMCPConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/NIMCPConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NIMCP
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/NIMCPConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/NIMCPConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NIMCP
)

install(EXPORT nimcp-targets
    FILE NIMCPTargets.cmake
    NAMESPACE NIMCP::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NIMCP
)

# Configuration summary
message(STATUS "")
message(STATUS "===============================================================================")
message(STATUS "NIMCP ${PROJECT_VERSION} - Configuration Summary")
message(STATUS "===============================================================================")
message(STATUS "Build Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
message(STATUS "Optional Features:")
message(STATUS "  CUDA Support: ${CUDAToolkit_FOUND}")
message(STATUS "  LAPACK/BLAS Support: ${LAPACK_BLAS_FOUND}")
message(STATUS "  Encryption Support: ${SODIUM_FOUND}")
message(STATUS "")
message(STATUS "Security Features:")
message(STATUS "  AddressSanitizer (ASAN): ${ENABLE_ASAN}")
message(STATUS "  UndefinedBehaviorSanitizer (UBSAN): ${ENABLE_UBSAN}")
message(STATUS "  ThreadSanitizer (TSAN): ${ENABLE_TSAN}")
message(STATUS "  Security Hardening: ${ENABLE_HARDENING}")
message(STATUS "  Fuzzing Targets: ${ENABLE_FUZZING}")
message(STATUS "")
message(STATUS "Installation:")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Library: ${CMAKE_INSTALL_LIBDIR}/libnimcp.so")
message(STATUS "  Headers: ${CMAKE_INSTALL_INCLUDEDIR}/nimcp/")
message(STATUS "  Pkg-config: ${CMAKE_INSTALL_LIBDIR}/pkgconfig/nimcp.pc")
message(STATUS "===============================================================================")
message(STATUS "")

# ============================================================================
# Benchmarks
# ============================================================================

add_executable(benchmark_quantum_walk benchmark_quantum_walk.cpp)
target_include_directories(benchmark_quantum_walk PRIVATE
    ${NIMCP_INCLUDE_DIR}
    ${Python3_INCLUDE_DIRS}
)
target_link_libraries(benchmark_quantum_walk PRIVATE
    nimcp
    Python3::Python
    pthread
)
set_target_properties(benchmark_quantum_walk PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
