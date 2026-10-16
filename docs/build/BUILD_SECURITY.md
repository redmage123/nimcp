# NIMCP Security Hardening Build Guide

This document provides comprehensive instructions for building NIMCP with various security features enabled.

## Table of Contents

- [Quick Start](#quick-start)
- [Security Features](#security-features)
- [Build Configurations](#build-configurations)
- [Sanitizers](#sanitizers)
- [Fuzzing](#fuzzing)
- [Error Injection Testing](#error-injection-testing)
- [CI/CD Integration](#cicd-integration)
- [Troubleshooting](#troubleshooting)

---

## Quick Start

### Production Build (Maximum Security)
```bash
mkdir build-release && cd build-release
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_HARDENING=ON
make -j$(nproc)
```

### Development Build with Sanitizers
```bash
mkdir build-dev && cd build-dev
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_ASAN=ON \
  -DENABLE_UBSAN=ON
make -j$(nproc)
./src/tests/nimcp_tests
```

---

## Security Features

### 1. Compiler Hardening Flags

**Enabled by default in Release builds** (`-DENABLE_HARDENING=ON`)

| Flag | Purpose | Protection Against |
|------|---------|-------------------|
| `-D_FORTIFY_SOURCE=2` | Buffer overflow detection | strcpy, memcpy, sprintf overflows |
| `-fstack-protector-strong` | Stack canaries | Stack buffer overflows, ROP |
| `-fPIE -pie` | Position Independent Executable | Return-to-libc attacks (requires ASLR) |
| `-Wformat-security` | Format string checking | printf-family vulnerabilities |
| `-Werror=format-security` | Treat format warnings as errors | Ensures no format bugs slip through |
| `-Wl,-z,relro` | Read-only relocations | GOT overwrite attacks |
| `-Wl,-z,now` | Immediate binding (Full RELRO) | Lazy binding exploits |
| `-Wl,-z,noexecstack` | Non-executable stack | Code injection via stack |
| `-fno-common` | No common symbols | Symbol interposition attacks |

**Verification:**
```bash
# Check if PIE is enabled
readelf -h build-release/src/lib/libnimcp.so | grep "Type:"
# Should show: Type: DYN (Shared object file)

# Check for stack canaries
objdump -d build-release/src/lib/libnimcp.so | grep stack_chk_fail
# Should show multiple references if enabled

# Check RELRO
readelf -l build-release/src/lib/libnimcp.so | grep GNU_RELRO
# Should show: GNU_RELRO
```

### 2. Unsafe Function Elimination

**Status:** ✅ Complete - All unsafe functions removed

| Unsafe Function | Safe Replacement | Count Fixed |
|----------------|------------------|-------------|
| `strcpy()` | `snprintf()` | 4 |
| `strcat()` | `strncat()` with bounds | 1 |
| `sprintf()` | `snprintf()` | 0 (already safe) |
| `gets()` | `fgets()` | 0 (not used) |

**Audit:**
```bash
./scripts/audit-unsafe-functions.sh
# Should output: ✓ No unsafe function usage detected!
```

### 3. Input Validation

**API boundary validation macros** in `src/include/nimcp_internal.h`:

```c
// Example usage:
int my_function(void *data, size_t size) {
    NIMCP_CHECK_NULL(data, -1);
    NIMCP_CHECK_RANGE(size, 1, MAX_SIZE, -1);
    // ... rest of function
}
```

---

## Build Configurations

### Configuration Matrix

| Configuration | Use Case | Sanitizers | Hardening | Performance |
|--------------|----------|-----------|-----------|-------------|
| **Release** | Production | None | Full | Maximum |
| **Debug** | Development | Optional | Partial | Medium |
| **ASAN** | Memory testing | ASAN+UBSAN | Partial | Low |
| **TSAN** | Concurrency testing | TSAN | Partial | Low |
| **Fuzzing** | Security testing | ASAN+Fuzzer | Partial | Low |

### 1. Release Build (Production)

```bash
mkdir build-release && cd build-release
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_HARDENING=ON
make -j$(nproc)
make install  # Optional
```

**Features:**
- Full hardening flags enabled
- Optimized code (`-O3`)
- No debug symbols (unless RelWithDebInfo)
- PIE, RELRO, stack protection, FORTIFY_SOURCE

**Use for:** Production deployments, performance benchmarks

### 2. Debug Build

```bash
mkdir build-debug && cd build-debug
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

**Features:**
- Debug symbols (`-g`)
- No optimization
- Partial hardening (stack protection only)
- Coverage instrumentation enabled

**Use for:** Development, debugging, code coverage

---

## Sanitizers

### AddressSanitizer (ASAN)

**Detects:**
- Buffer overflows (heap, stack, global)
- Use-after-free
- Use-after-return
- Use-after-scope
- Double-free
- Memory leaks

**Build:**
```bash
mkdir build-asan && cd build-asan
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_ASAN=ON \
  -DENABLE_UBSAN=ON
make -j$(nproc)
```

**Run:**
```bash
export ASAN_OPTIONS="detect_leaks=1:check_initialization_order=1:strict_init_order=1"
./src/tests/nimcp_tests
```

**Example Output:**
```
=================================================================
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...
```

### UndefinedBehaviorSanitizer (UBSAN)

**Detects:**
- Integer overflow
- Division by zero
- Null pointer dereference
- Misaligned memory access
- Invalid enum values
- Invalid bool values

**Build:** (Combined with ASAN above)

**Run:**
```bash
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=0"
./src/tests/nimcp_tests
```

### ThreadSanitizer (TSAN)

**Detects:**
- Data races
- Deadlocks
- Use of destroyed mutex

**Build:**
```bash
mkdir build-tsan && cd build-tsan
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_TSAN=ON
make -j$(nproc)
```

**Run:**
```bash
export TSAN_OPTIONS="halt_on_error=1:history_size=7:second_deadlock_stack=1"
./src/tests/nimcp_tests --gtest_filter="ThreadSafety*:Concurrent*"
```

**Note:** TSAN and ASAN are mutually exclusive.

---

## Fuzzing

### Setup

**Requirements:**
- Clang compiler with libFuzzer support
- AddressSanitizer

**Build:**
```bash
mkdir build-fuzz && cd build-fuzz
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_FUZZING=ON \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++
make -j$(nproc)
```

### Available Fuzz Targets

| Target | API Tested | Purpose |
|--------|-----------|---------|
| `fuzz_neuralnet` | Neural network creation | Config parsing, memory allocation |
| `fuzz_protocol` | Protocol serialization | Message parsing, buffer handling |
| `fuzz_queue_manager` | Queue operations | Concurrent operations, bounds |
| `fuzz_validate` | Input validation | Validation logic completeness |

### Running Fuzzers

**Quick fuzz (5 minutes):**
```bash
cd src/fuzz
./fuzz_neuralnet -max_total_time=300
```

**Continuous fuzzing with corpus:**
```bash
mkdir corpus_neuralnet
./fuzz_neuralnet corpus_neuralnet/ -max_total_time=3600 -workers=4
```

**Reproduce a crash:**
```bash
./fuzz_neuralnet crash-xxxx
```

### Fuzzer Options

| Option | Description | Example |
|--------|-------------|---------|
| `-max_total_time=N` | Run for N seconds | `-max_total_time=300` |
| `-workers=N` | Use N parallel workers | `-workers=4` |
| `-dict=file` | Use dictionary file | `-dict=proto.dict` |
| `-max_len=N` | Max input size | `-max_len=4096` |
| `-print_final_stats=1` | Show statistics | `-print_final_stats=1` |

### Integration with OSS-Fuzz

For continuous fuzzing integration:

1. Create `oss-fuzz-build.sh`:
```bash
#!/bin/bash
cd build-fuzz
make fuzz_neuralnet fuzz_protocol fuzz_queue_manager
cp src/fuzz/fuzz_* $OUT/
```

2. Submit to [OSS-Fuzz](https://github.com/google/oss-fuzz)

---

## Error Injection Testing

### Purpose

Systematically test error handling paths:
- Memory allocation failures
- NULL pointer handling
- Invalid parameters
- Resource exhaustion

### Running Error Injection Tests

```bash
./src/tests/nimcp_tests --gtest_filter="ErrorInjection*"
```

**Note:** Some tests are disabled by default as they require special build flags or affect system state.

### Enable Malloc Injection

Build with wrapper functions:
```bash
cmake .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_MALLOC_INJECTION=ON \
  -DCMAKE_EXE_LINKER_FLAGS="-Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc"
make
```

---

## CI/CD Integration

### GitHub Actions

The project includes 3 security-focused CI jobs:

1. **AddressSanitizer + UBSanitizer**
   ```yaml
   - name: Build with ASAN + UBSAN
     run: cmake .. -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
   ```

2. **ThreadSanitizer**
   ```yaml
   - name: Build with TSAN
     run: cmake .. -DENABLE_TSAN=ON
   ```

3. **Memory Leak Testing (Valgrind)**
   ```yaml
   - name: Run valgrind
     run: valgrind --leak-check=full ./src/tests/nimcp_tests
   ```

### Local Pre-Commit Testing

```bash
# Run all security checks before commit
./scripts/lint.sh
cd build-asan && make && ./src/tests/nimcp_tests
cd ../build-tsan && make && ./src/tests/nimcp_tests
```

---

## Troubleshooting

### Common Issues

#### 1. Sanitizer Runtime Not Found

**Error:**
```
./nimcp_tests: error while loading shared libraries: libasan.so.6
```

**Solution:**
```bash
sudo apt-get install libasan6 libubsan1 libtsan0
# Or find library path:
export LD_LIBRARY_PATH=/usr/lib/gcc/x86_64-linux-gnu/11:$LD_LIBRARY_PATH
```

#### 2. TSAN and ASAN Conflict

**Error:**
```
FATAL: ThreadSanitizer and AddressSanitizer are mutually exclusive
```

**Solution:**
Build separate configurations:
```bash
mkdir build-asan build-tsan
cd build-asan && cmake .. -DENABLE_ASAN=ON && make
cd ../build-tsan && cmake .. -DENABLE_TSAN=ON && make
```

#### 3. libFuzzer Not Found

**Error:**
```
clang: error: unsupported option '-fsanitize=fuzzer'
```

**Solution:**
```bash
# Use clang instead of gcc
cmake .. \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DENABLE_FUZZING=ON
```

#### 4. False Positive Leaks

**Error:**
```
Direct leak of 1024 byte(s) in 1 object(s) allocated from: ...PyMalloc...
```

**Solution:**
Use suppression file:
```bash
export LSAN_OPTIONS="suppressions=valgrind.supp"
./src/tests/nimcp_tests
```

---

## Performance Impact

| Configuration | Relative Speed | Memory Overhead | Use Case |
|--------------|----------------|-----------------|----------|
| Release | 1.0x (baseline) | 1.0x | Production |
| Debug | 0.3x | 1.1x | Development |
| ASAN | 0.5x | 2-3x | Memory testing |
| TSAN | 0.2-0.4x | 5-10x | Concurrency testing |
| Fuzzing | 0.6x | 2x | Security testing |

---

## Security Checklist

Before deploying to production:

- [ ] Build with `-DCMAKE_BUILD_TYPE=Release`
- [ ] Verify hardening flags enabled: `cmake .. -DENABLE_HARDENING=ON`
- [ ] Run full test suite: `make test`
- [ ] Run with ASAN: Tests pass without memory errors
- [ ] Run with TSAN: No data races detected
- [ ] Run with Valgrind: Zero definite leaks
- [ ] Run fuzzer for ≥1 hour: No crashes
- [ ] Audit unsafe functions: `./scripts/audit-unsafe-functions.sh` shows 0
- [ ] Static analysis clean: `./scripts/lint.sh` passes
- [ ] Coverage ≥70%: `./scripts/coverage.sh`

---

## Additional Resources

- [SECURITY.md](SECURITY.md) - Security policy and vulnerability reporting
- [QUICK_REFERENCE.md](QUICK_REFERENCE.md) - Daily workflow commands
- [scripts/README.md](scripts/README.md) - Detailed script documentation
- [Google Sanitizers](https://github.com/google/sanitizers)
- [libFuzzer Tutorial](https://llvm.org/docs/LibFuzzer.html)
- [OWASP Top 10](https://owasp.org/www-project-top-ten/)

---

**Last Updated:** 2025-11-01
**Version:** 1.0
