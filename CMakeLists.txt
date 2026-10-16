cmake_minimum_required(VERSION 3.15)
project(NIMCP VERSION 0.9.0 LANGUAGES C CXX)

# Default to RelWithDebInfo so debug symbols are always available for GDB
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# Set prefix path for locally installed packages
set(CMAKE_PREFIX_PATH $ENV{HOME}/.local ${CMAKE_PREFIX_PATH})

# Set C and C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable AVX2 SIMD optimizations on x86_64 platforms
if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64)|(AMD64)")
    # Check if compiler supports AVX2
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)

    if(COMPILER_SUPPORTS_AVX2)
        message(STATUS "Enabling AVX2 SIMD optimizations")
        add_compile_options(-mavx2 -mfma)
    else()
        message(STATUS "AVX2 not supported by compiler")
    endif()
else()
    message(STATUS "Non-x86_64 platform: AVX2 disabled")
endif()

# Find Python3 for bindings
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# Find NumPy include directory
if(Python3_FOUND)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -c "import numpy; print(numpy.get_include())"
        OUTPUT_VARIABLE Python3_NumPy_INCLUDE_DIRS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(Python3_NumPy_INCLUDE_DIRS)
        message(STATUS "NumPy include directory: ${Python3_NumPy_INCLUDE_DIRS}")
    else()
        message(WARNING "NumPy not found - Python bindings may be limited")
    endif()
endif()

# Find GTest for testing
find_package(GTest)

# Find CUDA Toolkit for GPU acceleration (optional)
option(NIMCP_DISABLE_CUDA "Disable CUDA support even if toolkit is found" OFF)
if(NOT NIMCP_DISABLE_CUDA)
    find_package(CUDAToolkit QUIET)
    if(CUDAToolkit_FOUND)
        message(STATUS "CUDA Toolkit found: ${CUDAToolkit_VERSION}")
        enable_language(CUDA)
        add_compile_definitions(NIMCP_ENABLE_CUDA)
    else()
        message(STATUS "CUDA Toolkit not found - GPU kernels will be disabled")
    endif()
else()
    message(STATUS "CUDA support disabled by NIMCP_DISABLE_CUDA option")
endif()

# Enable testing
enable_testing()

#==============================================================================
# Static Analysis / Linting
#==============================================================================

# Option to enable linting during build (off by default - use targets instead)
option(NIMCP_ENABLE_LINT "Enable clang-tidy during build" OFF)
option(NIMCP_ENABLE_CPPCHECK "Enable cppcheck during build" OFF)

# Find clang-tidy
find_program(CLANG_TIDY_EXE NAMES clang-tidy clang-tidy-18 clang-tidy-17 clang-tidy-16 clang-tidy-15 clang-tidy-14)
if(CLANG_TIDY_EXE)
    message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")

    # Enable clang-tidy during compilation if requested
    if(NIMCP_ENABLE_LINT)
        set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}")
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
        message(STATUS "clang-tidy enabled for all builds")
    endif()
else()
    message(STATUS "clang-tidy not found - install with: sudo apt install clang-tidy")
endif()

# Find cppcheck
find_program(CPPCHECK_EXE NAMES cppcheck)
if(CPPCHECK_EXE)
    message(STATUS "Found cppcheck: ${CPPCHECK_EXE}")

    # Enable cppcheck during compilation if requested
    if(NIMCP_ENABLE_CPPCHECK)
        set(CMAKE_C_CPPCHECK "${CPPCHECK_EXE};--enable=warning,performance,portability;--suppress=missingIncludeSystem;--inline-suppr")
        set(CMAKE_CXX_CPPCHECK "${CPPCHECK_EXE};--enable=warning,performance,portability;--suppress=missingIncludeSystem;--inline-suppr")
        message(STATUS "cppcheck enabled for all builds")
    endif()
else()
    message(STATUS "cppcheck not found - install with: sudo apt install cppcheck")
endif()

# Collect source files for lint targets
file(GLOB_RECURSE NIMCP_ALL_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.c"
    "${CMAKE_SOURCE_DIR}/src/*.h"
)
file(GLOB_RECURSE NIMCP_ALL_HEADERS
    "${CMAKE_SOURCE_DIR}/include/*.h"
)

# Create 'lint' target for clang-tidy
if(CLANG_TIDY_EXE)
    add_custom_target(lint
        COMMAND ${CLANG_TIDY_EXE}
            -p ${CMAKE_BINARY_DIR}
            --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
            ${NIMCP_ALL_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy on all source files..."
        VERBATIM
    )

    # Create 'lint-fix' target to auto-fix issues
    add_custom_target(lint-fix
        COMMAND ${CLANG_TIDY_EXE}
            -p ${CMAKE_BINARY_DIR}
            --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
            --fix
            --fix-errors
            ${NIMCP_ALL_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy with auto-fix on all source files..."
        VERBATIM
    )
endif()

# Create 'cppcheck' target
if(CPPCHECK_EXE)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK_EXE}
            --enable=all
            --suppress=missingIncludeSystem
            --suppress=unusedFunction
            --suppress=unmatchedSuppression
            --inline-suppr
            --project=${CMAKE_BINARY_DIR}/compile_commands.json
            --error-exitcode=1
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running cppcheck on all source files..."
        VERBATIM
    )
endif()

# Create combined 'check' target that runs all available linters
add_custom_target(check
    COMMENT "Running all static analysis tools..."
)
if(CLANG_TIDY_EXE)
    add_dependencies(check lint)
endif()
if(CPPCHECK_EXE)
    add_dependencies(check cppcheck)
endif()

# Export compile commands for IDE and tool integration
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#==============================================================================
# End Static Analysis
#==============================================================================

# Add subdirectories
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(examples)

# Standalone-C lang/SNN tests (printf+exit, no GTest dep). Optional —
# only registers if tests/CMakeLists.txt exists. The 21 .c smoke tests
# under tests/unit/ are wired into ctest there. Run with:
#   ctest -R "test_lang_|test_bigram|test_snn_lang|test_bulk_lex"
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
