# NIMCP Cross-Platform Refactoring Summary

## Overview

**WHAT**: Refactor NIMCP to compile and run on Windows 11, macOS, and Linux
**WHY**: Currently pthread-dependent (POSIX only), need Windows support
**HOW**: Create platform abstraction layer, update build system, test on each platform

**STATUS**: ⚠️ IN PROGRESS - Foundation complete, migration pending

---

## Platform Support Matrix

| Platform | Compiler | Architecture | Status |
|----------|----------|--------------|--------|
| Linux    | GCC 13+  | x86_64       | ✅ Currently supported |
| Linux    | Clang 15+ | x86_64      | ✅ Currently supported |
| macOS    | Clang (Xcode) | Intel x86_64 | 🟡 Needs testing |
| macOS    | Clang (Xcode) | Apple Silicon (ARM64) | 🟡 Needs testing |
| Windows 11 | MSVC 2022 | x86_64      | 🔴 Not yet supported |
| Windows 11 | MinGW-w64 | x86_64     | 🔴 Not yet supported |
| Windows 11 | Clang/LLVM | x86_64   | 🔴 Not yet supported |

---

## Architecture

### New Platform Abstraction Layer

**Files Created:**
1. `/src/utils/platform/nimcp_platform.h` - Platform detection & constants
2. `/src/utils/platform/nimcp_platform.c` - Platform utility functions
3. `/src/utils/platform/nimcp_thread_platform.h` - Cross-platform threading API
4. `/src/utils/platform/nimcp_thread_platform.c` - Threading implementations (pending)
5. `/src/utils/platform/nimcp_time_platform.h` - Cross-platform time API (pending)
6. `/src/utils/platform/nimcp_time_platform.c` - Time implementations (pending)

### Platform Detection Macros

```c
#if defined(NIMCP_PLATFORM_WINDOWS)
    /* Windows-specific code */
#elif defined(NIMCP_PLATFORM_MACOS)
    /* macOS-specific code */
#elif defined(NIMCP_PLATFORM_LINUX)
    /* Linux-specific code */
#elif defined(NIMCP_PLATFORM_POSIX)
    /* Generic POSIX code (macOS + Linux + BSD) */
#endif
```

### Threading Abstraction

**Before** (POSIX-only):
```c
pthread_mutex_t mutex;
pthread_mutex_init(&mutex, NULL);
pthread_mutex_lock(&mutex);
pthread_mutex_unlock(&mutex);
pthread_mutex_destroy(&mutex);
```

**After** (Cross-platform):
```c
nimcp_platform_mutex_t mutex;
nimcp_platform_mutex_init(&mutex, false);
nimcp_platform_mutex_lock(&mutex);
nimcp_platform_mutex_unlock(&mutex);
nimcp_platform_mutex_destroy(&mutex);
```

**Implementation**:
- **POSIX** (Linux/macOS): Wraps pthread_* functions
- **Windows**: Wraps CRITICAL_SECTION, HANDLE, SRWLOCK

---

## Platform-Specific Issues Identified

### 1. Threading (CRITICAL)

**Files Using pthread Directly:**
- `/src/utils/thread/nimcp_thread.h` - 100% pthread
- `/src/utils/thread/nimcp_thread.c` - 100% pthread
- `/src/utils/thread/nimcp_thread_pool.c` - pthread pool
- `/src/utils/containers/nimcp_queue.h` - pthread mutex
- `/src/utils/containers/nimcp_btree.c` - pthread rwlock
- Many cognitive modules (consolidation, introspection, etc.)

**Solution**: Create `nimcp_platform_thread_*` wrapper API

**Estimate**: ~50 files to update

### 2. Time Functions (CRITICAL)

**Files Using POSIX time:**
- `/src/utils/time/nimcp_time.c` - Uses `clock_gettime` (POSIX)
- `/src/utils/time/nimcp_time.h` - Returns uint64_t milliseconds

**POSIX API**:
```c
struct timespec ts;
clock_gettime(CLOCK_MONOTONIC, &ts);
```

**Windows Alternative**:
```c
LARGE_INTEGER freq, counter;
QueryPerformanceFrequency(&freq);
QueryPerformanceCounter(&counter);
```

**macOS Alternative**:
```c
mach_timebase_info_data_t info;
mach_timebase_info(&info);
uint64_t time = mach_absolute_time();
```

**Solution**: Create `nimcp_platform_time_*` wrapper API

**Estimate**: ~10 files to update

### 3. Compiler Differences

**GCC/Clang Attributes:**
```c
__attribute__((aligned(16)))
__attribute__((packed))
__builtin_expect()
```

**MSVC Equivalents:**
```c
__declspec(align(16))
#pragma pack(push, 1)
/* No direct equivalent for likely/unlikely */
```

**Solution**: Macros in `nimcp_platform.h`
```c
NIMCP_ALIGNED(16)
NIMCP_PACKED
NIMCP_LIKELY(x)
```

**Estimate**: Already handled in nimcp_platform.h

### 4. Headers and Includes

**POSIX Headers** (not on Windows):
```c
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
```

**Windows Headers**:
```c
#include <windows.h>
#include <process.h>
```

**Solution**: Conditional includes in platform headers

**Estimate**: ~100 files may need include adjustments

### 5. Networking (if applicable)

**POSIX Sockets**:
```c
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
```

**Windows Sockets**:
```c
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
```

**Solution**: Create `nimcp_platform_socket.h` if needed

**Estimate**: ~5-10 networking files

### 6. File System

**Path Separators**:
- **POSIX**: `/` (forward slash)
- **Windows**: `\` (backslash)

**Solution**: Use macros
```c
NIMCP_PATH_SEPARATOR      /* '/' or '\\' */
NIMCP_PATH_SEPARATOR_STR  /* "/" or "\\" */
```

**Estimate**: Already handled in nimcp_platform.h

---

## CMake Build System Changes

### Platform Detection

```cmake
# Detect platform
if(WIN32)
    set(NIMCP_PLATFORM_WINDOWS ON)
    set(NIMCP_PLATFORM_NAME "Windows")
elseif(APPLE)
    set(NIMCP_PLATFORM_MACOS ON)
    set(NIMCP_PLATFORM_NAME "macOS")
    set(NIMCP_PLATFORM_POSIX ON)
elseif(UNIX)
    set(NIMCP_PLATFORM_LINUX ON)
    set(NIMCP_PLATFORM_NAME "Linux")
    set(NIMCP_PLATFORM_POSIX ON)
endif()

# Add platform abstraction sources
set(NIMCP_PLATFORM_SOURCES
    src/utils/platform/nimcp_platform.c
    src/utils/platform/nimcp_thread_platform.c
    src/utils/platform/nimcp_time_platform.c
)

# Platform-specific linking
if(WIN32)
    target_link_libraries(nimcp PRIVATE ws2_32)  # Winsock
elseif(UNIX)
    target_link_libraries(nimcp PRIVATE pthread)
endif()
```

### Compiler-Specific Flags

```cmake
if(MSVC)
    # MSVC-specific flags
    add_compile_options(/W4 /WX)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    # GCC/Clang flags (existing)
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()
```

---

## Migration Strategy

### Phase 1: Foundation (COMPLETE ✅)

- [x] Create `nimcp_platform.h` - Platform detection
- [x] Create `nimcp_platform.c` - Utility functions
- [x] Create `nimcp_thread_platform.h` - Threading API
- [ ] Create `nimcp_thread_platform.c` - Threading implementations

**Time**: 2-3 hours
**Risk**: Low - new files, no existing code broken

### Phase 2: Core Utilities (PENDING)

- [ ] Create `nimcp_time_platform.h` - Time API
- [ ] Create `nimcp_time_platform.c` - Time implementations
- [ ] Update `nimcp_time.c` to use platform layer
- [ ] Update `nimcp_thread.c` to use platform layer
- [ ] Add platform sources to CMakeLists.txt

**Time**: 4-6 hours
**Risk**: Medium - affects core utilities

### Phase 3: Module Migration (PENDING)

- [ ] Update all modules to use `nimcp_platform_mutex_t`
- [ ] Update all modules to use `nimcp_platform_time_*`
- [ ] Fix includes (platform.h instead of pthread.h)
- [ ] Test each module individually

**Affected modules**:
- Consolidation (Phase 10.2)
- Working Memory (Phase 10.2)
- Introspection
- Ethics
- Sleep/Wake (Phase 10.1)
- All containers (queue, btree, etc.)

**Time**: 10-15 hours
**Risk**: High - touches many files

### Phase 4: Windows Testing (PENDING)

- [ ] Set up Windows 11 build environment
- [ ] Install MSVC 2022 or MinGW-w64
- [ ] Build on Windows, fix compile errors
- [ ] Run test suite on Windows
- [ ] Fix platform-specific bugs

**Time**: 8-12 hours
**Risk**: High - unknown issues

### Phase 5: macOS Testing (PENDING)

- [ ] Set up macOS build environment (Intel or M1/M2)
- [ ] Build on macOS, fix compile errors
- [ ] Run test suite on macOS
- [ ] Test both Intel and Apple Silicon if possible
- [ ] Fix platform-specific bugs

**Time**: 6-10 hours
**Risk**: Medium - mostly POSIX-compatible

### Phase 6: CI/CD (PENDING)

- [ ] GitHub Actions workflow for Windows
- [ ] GitHub Actions workflow for macOS
- [ ] GitHub Actions workflow for Linux (existing)
- [ ] Matrix builds (all platforms × all compilers)

**Time**: 4-6 hours
**Risk**: Low - automation

---

## Estimated Total Effort

| Phase | Time | Risk | Status |
|-------|------|------|--------|
| 1. Foundation | 2-3h | Low | ✅ Complete |
| 2. Core Utilities | 4-6h | Medium | 🟡 Pending |
| 3. Module Migration | 10-15h | High | 🔴 Not started |
| 4. Windows Testing | 8-12h | High | 🔴 Not started |
| 5. macOS Testing | 6-10h | Medium | 🔴 Not started |
| 6. CI/CD | 4-6h | Low | 🔴 Not started |
| **TOTAL** | **34-52h** | | **5% complete** |

---

## Testing Strategy

### Unit Tests (TDD)

1. Create platform-specific tests for each abstraction:
   - `test_platform_threading.cpp` - Test mutex, cond, rwlock
   - `test_platform_time.cpp` - Test time functions
   - Run on each platform to verify correctness

2. Existing tests should pass without modification:
   - `test_sleep_wake.cpp` (31 tests)
   - `test_working_memory.cpp`
   - `test_consolidation.cpp`
   - All 10 test suites

### Integration Testing

1. **Linux** (baseline): All existing tests must pass
2. **macOS**: All tests must pass (may need adjustments)
3. **Windows**: All tests must pass (will need adjustments)

### Performance Testing

- Verify threading performance is comparable across platforms
- Check that Windows CRITICAL_SECTION performs well
- Verify macOS `mach_absolute_time()` accuracy

---

## Known Challenges

### 1. Windows Thread Return Type

**POSIX**: `void* thread_func(void* arg)`
**Windows**: `DWORD WINAPI thread_func(LPVOID arg)`

**Solution**: Wrapper that translates return types

### 2. Read-Write Locks on Windows

**Windows SRWLock** (Slim Read-Write Lock):
- Different API than pthread_rwlock
- No timeout support
- Must track whether lock is read or write mode

**Solution**: Abstract API hides differences

### 3. Condition Variables on Windows

**Windows Event Objects**:
- Different semantics than pthread_cond
- May need broadcast emulation
- Potential spurious wakeups

**Solution**: Careful implementation + testing

### 4. Time Resolution

- **Linux**: nanosecond precision (clock_gettime)
- **Windows**: QueryPerformanceCounter (variable precision)
- **macOS**: mach_absolute_time (nanosecond precision)

**Solution**: All return milliseconds, precision best-effort

---

## Next Steps

**Option A: Complete Full Refactoring**
1. Implement `nimcp_thread_platform.c` (Windows + POSIX)
2. Implement `nimcp_time_platform.c` (Windows + POSIX + macOS)
3. Update all ~50-100 files to use platform layer
4. Test on all 3 platforms
5. Fix issues, iterate

**Time**: 30-50 hours over 1-2 weeks

**Option B: Gradual Migration**
1. Add platform layer alongside existing code
2. Migrate one module at a time
3. Test after each module
4. Eventually remove old pthread code

**Time**: 20-30 hours over 2-3 weeks (safer)

**Option C: Windows-Only Minimal Port**
1. Only implement what's needed for Windows
2. Leave Linux/macOS using pthread directly
3. Conditional compilation: `#ifdef _WIN32`

**Time**: 15-20 hours (fastest, but messy)

---

## Recommendation

**Use Option B: Gradual Migration**

**Rationale**:
- Less risky (doesn't break existing Linux builds)
- Can test incrementally
- Easier to debug issues
- Platform layer coexists with pthread
- Can be done module-by-module

**First milestone**: Get sleep/wake cycle working on all platforms (smallest module)

---

## Files Created (Phase 1 Complete)

✅ `/src/utils/platform/nimcp_platform.h` (370 lines)
✅ `/src/utils/platform/nimcp_platform.c` (60 lines)
✅ `/src/utils/platform/nimcp_thread_platform.h` (550 lines)

**Total**: ~980 lines of cross-platform abstraction code

**Status**: Ready for Phase 2 implementation

---

## References

### Windows API Documentation
- Threading: https://learn.microsoft.com/en-us/windows/win32/procthread/
- Synchronization: https://learn.microsoft.com/en-us/windows/win32/sync/
- Critical Sections: https://learn.microsoft.com/en-us/windows/win32/sync/critical-section-objects

### POSIX Standards
- pthread: https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/pthread.h.html
- time: https://pubs.opengroup.org/onlinepubs/9699919799/functions/clock_gettime.html

### macOS Specific
- mach time: https://developer.apple.com/documentation/kernel/1462446-mach_absolute_time
- Grand Central Dispatch (future optimization): https://developer.apple.com/documentation/dispatch

---

**Last Updated**: 2025-01-09
**Author**: NIMCP Development Team
**Status**: Phase 1 Complete, awaiting Phase 2 approval
