/**
 * @file nimcp_platform.h
 * @brief Cross-platform compatibility layer for NIMCP
 *
 * WHAT: Platform detection and abstraction for Windows, macOS, Linux
 * WHY:  Enable NIMCP to compile and run on Windows 11, macOS, and Linux
 * HOW:  Detect platform, provide unified API for OS-specific functions
 *
 * SUPPORTED PLATFORMS:
 * - Linux (GCC, Clang)
 * - macOS (Clang, Apple Silicon, Intel)
 * - Windows 11 (MSVC, MinGW, Clang)
 *
 * DESIGN PATTERN: Facade pattern - unified interface to platform-specific code
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_PLATFORM_H
#define NIMCP_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * PLATFORM DETECTION
 * ======================================================================== */

/**
 * WHAT: Detect operating system at compile time
 * WHY:  Different platforms need different headers and APIs
 * HOW:  Use compiler-defined macros
 */

/* Windows detection */
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #define NIMCP_PLATFORM_WINDOWS 1
    #define NIMCP_PLATFORM_NAME "Windows"

    /* Windows version detection */
    #if defined(_WIN64)
        #define NIMCP_PLATFORM_WIN64 1
    #else
        #define NIMCP_PLATFORM_WIN32 1
    #endif

    /* Compiler detection on Windows */
    #if defined(_MSC_VER)
        #define NIMCP_COMPILER_MSVC 1
        #define NIMCP_COMPILER_NAME "MSVC"
    #elif defined(__MINGW32__) || defined(__MINGW64__)
        #define NIMCP_COMPILER_MINGW 1
        #define NIMCP_COMPILER_NAME "MinGW"
    #elif defined(__clang__)
        #define NIMCP_COMPILER_CLANG 1
        #define NIMCP_COMPILER_NAME "Clang"
    #endif

/* macOS detection */
#elif defined(__APPLE__) && defined(__MACH__)
    #define NIMCP_PLATFORM_MACOS 1
    #define NIMCP_PLATFORM_NAME "macOS"
    #define NIMCP_PLATFORM_POSIX 1

    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #define NIMCP_PLATFORM_OSX 1
    #endif

    /* Architecture detection */
    #if defined(__arm64__) || defined(__aarch64__)
        #define NIMCP_ARCH_ARM64 1
        #define NIMCP_ARCH_NAME "Apple Silicon (ARM64)"
    #elif defined(__x86_64__) || defined(__amd64__)
        #define NIMCP_ARCH_X86_64 1
        #define NIMCP_ARCH_NAME "Intel x86_64"
    #endif

    #if defined(__clang__)
        #define NIMCP_COMPILER_CLANG 1
        #define NIMCP_COMPILER_NAME "Clang"
    #endif

/* Linux detection */
#elif defined(__linux__) || defined(__unix__) || defined(__unix)
    #ifndef NIMCP_PLATFORM_LINUX
    #define NIMCP_PLATFORM_LINUX 1
    #endif
    #define NIMCP_PLATFORM_NAME "Linux"
    #define NIMCP_PLATFORM_POSIX 1

    #if defined(__ANDROID__)
        #define NIMCP_PLATFORM_ANDROID 1
    #endif

    /* Compiler detection on Linux */
    #if defined(__clang__)
        #define NIMCP_COMPILER_CLANG 1
        #define NIMCP_COMPILER_NAME "Clang"
    #elif defined(__GNUC__)
        #define NIMCP_COMPILER_GCC 1
        #define NIMCP_COMPILER_NAME "GCC"
    #endif

/* FreeBSD/other Unix */
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    #define NIMCP_PLATFORM_BSD 1
    #define NIMCP_PLATFORM_NAME "BSD"
    #define NIMCP_PLATFORM_POSIX 1

#else
    #error "Unsupported platform - NIMCP requires Windows, macOS, or Linux"
#endif

/* ========================================================================
 * COMPILER ATTRIBUTES
 * ======================================================================== */

/**
 * WHAT: Compiler-specific attributes for optimization and warnings
 * WHY:  Different compilers use different syntax
 * HOW:  Detect compiler and use appropriate syntax
 */

#if defined(NIMCP_COMPILER_GCC) || defined(NIMCP_COMPILER_CLANG)
    #define NIMCP_INLINE inline __attribute__((always_inline))
    #define NIMCP_NOINLINE __attribute__((noinline))
    #define NIMCP_ALIGNED(x) __attribute__((aligned(x)))
    #define NIMCP_PACKED __attribute__((packed))
    #define NIMCP_UNUSED __attribute__((unused))
    #define NIMCP_LIKELY(x) __builtin_expect(!!(x), 1)
    #define NIMCP_UNLIKELY(x) __builtin_expect(!!(x), 0)
#elif defined(NIMCP_COMPILER_MSVC)
    #define NIMCP_INLINE __forceinline
    #define NIMCP_NOINLINE __declspec(noinline)
    #define NIMCP_ALIGNED(x) __declspec(align(x))
    #define NIMCP_PACKED
    #define NIMCP_UNUSED
    #define NIMCP_LIKELY(x) (x)
    #define NIMCP_UNLIKELY(x) (x)
#else
    #define NIMCP_INLINE inline
    #define NIMCP_NOINLINE
    #define NIMCP_ALIGNED(x)
    #define NIMCP_PACKED
    #define NIMCP_UNUSED
    #define NIMCP_LIKELY(x) (x)
    #define NIMCP_UNLIKELY(x) (x)
#endif

/* ========================================================================
 * DLL EXPORT/IMPORT (Windows)
 * ======================================================================== */

#if defined(NIMCP_PLATFORM_WINDOWS)
    #if defined(NIMCP_BUILD_SHARED)
        #define NIMCP_API __declspec(dllexport)
    #elif defined(NIMCP_USE_SHARED)
        #define NIMCP_API __declspec(dllimport)
    #else
        #define NIMCP_API
    #endif
#else
    #if defined(__GNUC__) && __GNUC__ >= 4
        #define NIMCP_API __attribute__((visibility("default")))
    #else
        #define NIMCP_API
    #endif
#endif

/* ========================================================================
 * STANDARD HEADERS (Platform-specific)
 * ======================================================================== */

#if defined(NIMCP_PLATFORM_WINDOWS)
    /* Windows headers */
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX  /* Prevent min/max macro conflicts */
    #endif

    #include <windows.h>
    #include <process.h>  /* _beginthread */

    /* Windows doesn't have these POSIX headers */
    /* We'll provide alternatives in platform-specific wrappers */

#elif defined(NIMCP_PLATFORM_POSIX)
    /* POSIX headers (macOS, Linux, BSD) */
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <errno.h>

    #if defined(NIMCP_PLATFORM_MACOS)
        #include <mach/mach_time.h>  /* High-resolution timing on macOS */
    #endif
#endif

/* ========================================================================
 * BASIC TYPES (Cross-platform)
 * ======================================================================== */

/**
 * WHAT: Platform-agnostic basic types
 * WHY:  Ensure consistent sizes across platforms
 * HOW:  Use stdint.h types
 */

/* These are already cross-platform from stdint.h */
typedef int8_t   nimcp_i8;
typedef int16_t  nimcp_i16;
typedef int32_t  nimcp_i32;
typedef int64_t  nimcp_i64;
typedef uint8_t  nimcp_u8;
typedef uint16_t nimcp_u16;
typedef uint32_t nimcp_u32;
typedef uint64_t nimcp_u64;
typedef float    nimcp_f32;
typedef double   nimcp_f64;

/* ========================================================================
 * PATH SEPARATORS
 * ======================================================================== */

#if defined(NIMCP_PLATFORM_WINDOWS)
    #define NIMCP_PATH_SEPARATOR '\\'
    #define NIMCP_PATH_SEPARATOR_STR "\\"
    #define NIMCP_ALT_PATH_SEPARATOR '/'
    #define NIMCP_PATH_LIST_SEPARATOR ';'
#else
    #define NIMCP_PATH_SEPARATOR '/'
    #define NIMCP_PATH_SEPARATOR_STR "/"
    #define NIMCP_ALT_PATH_SEPARATOR '\\'  /* Not used on POSIX */
    #define NIMCP_PATH_LIST_SEPARATOR ':'
#endif

#define NIMCP_MAX_PATH 4096  /* Maximum path length (conservative) */

/* ========================================================================
 * UTILITY FUNCTIONS
 * ======================================================================== */

/**
 * WHAT: Get platform name at runtime
 * WHY:  Useful for logging and diagnostics
 * HOW:  Return compile-time detected platform
 *
 * @return Platform name string (e.g., "Windows", "macOS", "Linux")
 */
const char* nimcp_platform_name(void);

/**
 * WHAT: Get compiler name at runtime
 * WHY:  Useful for bug reports
 * HOW:  Return compile-time detected compiler
 *
 * @return Compiler name string (e.g., "MSVC", "GCC", "Clang")
 */
const char* nimcp_compiler_name(void);

/**
 * WHAT: Get architecture name at runtime
 * WHY:  Useful for optimization detection
 * HOW:  Return compile-time detected architecture
 *
 * @return Architecture name string (e.g., "x86_64", "ARM64")
 */
const char* nimcp_architecture_name(void);

/**
 * WHAT: Check if running on Windows
 * WHY:  Runtime platform checks for conditional logic
 * HOW:  Return compile-time constant
 *
 * @return true if Windows, false otherwise
 */
static inline bool nimcp_is_windows(void) {
#if defined(NIMCP_PLATFORM_WINDOWS)
    return true;
#else
    return false;
#endif
}

/**
 * WHAT: Check if running on macOS
 * WHY:  Runtime platform checks for conditional logic
 * HOW:  Return compile-time constant
 *
 * @return true if macOS, false otherwise
 */
static inline bool nimcp_is_macos(void) {
#if defined(NIMCP_PLATFORM_MACOS)
    return true;
#else
    return false;
#endif
}

/**
 * WHAT: Check if running on Linux
 * WHY:  Runtime platform checks for conditional logic
 * HOW:  Return compile-time constant
 *
 * @return true if Linux, false otherwise
 */
static inline bool nimcp_is_linux(void) {
#if defined(NIMCP_PLATFORM_LINUX)
    return true;
#else
    return false;
#endif
}

/**
 * WHAT: Check if platform is POSIX-compliant
 * WHY:  Many APIs can use POSIX functions together
 * HOW:  Return compile-time constant
 *
 * @return true if POSIX (macOS/Linux/BSD), false otherwise
 */
static inline bool nimcp_is_posix(void) {
#if defined(NIMCP_PLATFORM_POSIX)
    return true;
#else
    return false;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PLATFORM_H */
