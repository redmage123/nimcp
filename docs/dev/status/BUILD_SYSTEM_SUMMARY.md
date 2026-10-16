# NIMCP Build System - Executive Summary

**Prepared:** November 11, 2025  
**NIMCP Version:** 2.6.2  
**Build System:** CMake 3.10+

---

## Overview

NIMCP uses a sophisticated CMake-based build system with:
- **140+ C source files** comprising the core library
- **3 CUDA kernel files** for GPU acceleration (optional)
- **Modular architecture** organized by functional domain
- **Multi-platform support** (Linux primary, macOS/Windows experimental)
- **Production-ready features** (hardening, sanitizers, coverage)
- **Deployment pipelines** (Docker, systemd, shell scripts)

---

## Key Statistics

| Metric | Value |
|--------|-------|
| **CMake Version** | 3.10 minimum |
| **Source Files** | 140+ C files + 3 CUDA kernels |
| **Header Files** | 100+ distributed across modules |
| **Build Targets** | 1 core library + 17+ examples + 50+ tests |
| **Compiled Library Size** | ~3.0 MB (Release, stripped) |
| **Build Time** | 30-60 seconds (Release, -j8, fresh) |
| **Installation Size** | ~20 MB (library + headers + Python module) |
| **Test Count** | 50+ unit/integration/E2E tests |
| **Code Coverage** | Instrumentation available (gcov/lcov) |

---

## Architecture Summary

### Core Structure
```
src/
├── api/              Public API facade
├── core/             Brain components (neural networks, synapse types)
├── cognitive/        Ethics, knowledge, memory, learning (20+ modules)
├── plasticity/       STDP, BCM, attention, neuromodulation (9 modules)
├── glial/            Astrocytes, microglia, oligodendrocytes (5 modules)
├── networking/       P2P, distributed cognition (5 modules)
├── io/               Serialization, encryption (5 modules)
├── gpu/              CUDA kernels and GPU APIs (3 + kernels)
├── utils/            Memory, threading, logging, containers (30+ modules)
├── nlp/              Natural language processing (2 modules)
└── python/           Python C extension bindings (3 files)
```

### Build Targets
1. **libnimcp.so.2.6.2** - Core shared library (SOVERSION 2)
2. **nimcp.so** - Python C extension module
3. **17+ Example Programs** - Demonstrating features
4. **50+ Test Executables** - Unit, integration, E2E, regression, fuzz

---

## Dependencies

### Required
- **Python 3.7+** - C API headers for bindings
- **CMake 3.10+** - Build system
- **C11 Compiler** - GCC 9+ or Clang 10+
- **C++17 Compiler** - GCC 9+ or Clang 10+
- **libjansson** - JSON library
- **liblz4** - Compression library
- **pthread** - Threading (system)

### Optional
- **CUDA 11.0+** - GPU acceleration (auto-detected)
  - Architectures: 75, 80, 86, 89 (Turing, Ampere, Ada)
- **libsodium** - Encryption support (auto-detected)
- **Google Test** - Testing framework (fetched if unavailable)

---

## Build Configuration Options

| Feature | Default | Purpose |
|---------|---------|---------|
| `CMAKE_BUILD_TYPE` | Debug | Optimization level |
| `ENABLE_ASAN` | OFF | Memory error detection |
| `ENABLE_UBSAN` | OFF | Undefined behavior detection |
| `ENABLE_TSAN` | OFF | Data race detection |
| `ENABLE_HARDENING` | ON | Security flags (Release builds) |
| `ENABLE_COVERAGE` | OFF | Code coverage (gcov/lcov) |
| `ENABLE_FUZZING` | OFF | Fuzzing targets (libFuzzer) |

**Note:** ASAN and TSAN are mutually exclusive.

---

## Build Outputs

### Library Versioning
```
libnimcp.so         → latest version (symlink)
libnimcp.so.2       → SOVERSION 2 (ABI symlink)
libnimcp.so.2.6.2   → actual binary file
```

### Output Directories
- **Libraries:** `bin/` (build) → `/usr/local/lib/` (installed)
- **Executables:** `bin/` (examples and test binaries)
- **Python Module:** `lib/python/` (build) → `python3/dist-packages/` (installed)
- **Headers:** `src/` (build) → `/usr/local/include/nimcp/` (installed)

---

## Installation Structure

When installed to `/usr/local`:

```
/usr/local/
├── lib/
│   ├── libnimcp.so.2.6.2         Main library
│   ├── libnimcp.so.2              SOVERSION symlink
│   ├── libnimcp.so                Latest symlink
│   ├── python3/dist-packages/
│   │   └── nimcp.so               Python module
│   ├── pkgconfig/
│   │   └── nimcp.pc               pkg-config metadata
│   └── cmake/NIMCP/
│       ├── NIMCPConfig.cmake      CMake package config
│       ├── NIMCPConfigVersion.cmake
│       └── NIMCPTargets.cmake     Export targets
│
├── bin/
│   └── examples/
│       ├── brain_demo
│       ├── ethics_demo
│       └── ... (selected examples)
│
└── include/
    └── nimcp/
        ├── api/
        ├── core/
        ├── cognitive/
        └── ... (all 100+ headers)
```

---

## Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| Linux x86_64 | ✓ Primary | Fully tested, recommended |
| Linux ARM64 | ✓ Supported | Tested, identical features |
| macOS | ~ Partial | CUDA disabled, basic support |
| Windows | ○ Experimental | MinGW/MSVC, limited testing |

---

## GPU/CUDA Support

**Automatic Detection:**
- CMake checks for CUDA compiler
- If found: Compiles GPU kernels
- If not found: CPU fallback (identical API)

**Supported Architectures:**
- 75 (Turing: RTX 2080/3080/4080)
- 80 (Ampere: A100)
- 86 (Ada Lovelace variant: L40)
- 89 (Ada Lovelace: H100)

**GPU Kernel Files:**
- `nimcp_gpu_kernels.cu` - Neuron computations
- `nimcp_synapse_compute_gpu.cu` - Synapse calculations
- `nimcp_neural_logic_kernels.cu` - Logic gates

---

## Testing Framework

**Framework:** Google Test v1.14.0 (fetched or system)  
**Test Categories:**
- Unit tests (per-module)
- Integration tests (cross-module)
- End-to-end tests (full system)
- Regression tests (bug reproduction)
- Fuzz tests (property-based)

**Test Execution:**
```bash
ctest -j$(nproc)           # All tests, parallel
ctest -L unit -j$(nproc)   # Unit tests only
ctest -L integration       # Integration tests
```

**Code Surgeon Integration:**
Tests automatically discovered and executed via Code Surgeon tools.

---

## Deployment Options

### 1. Manual Installation
```bash
./install.sh                          # Automated setup
source setup_env.sh                   # Configure environment
./src/bindings/web-demo/start_demo.sh # Run application
```

### 2. Docker Containerization
```bash
docker build -t nimcp:2.6.2 .
docker run -p 8080:8080 -p 9090:9090 nimcp:2.6.2
```

### 3. Systemd Services
```bash
sudo cp systemd/*.service /etc/systemd/system/
sudo systemctl enable nimcp-backend nimcp-frontend
sudo systemctl start nimcp-backend nimcp-frontend
```

### 4. Production Nginx Proxy
- React frontend (port 80)
- Flask backend (port 5000, proxied)
- HTTPS with Let's Encrypt

---

## Security Features

### Compiler Hardening (Release Builds)
- `-fstack-protector-strong` - Stack canary
- `-D_FORTIFY_SOURCE=2` - Buffer overflow detection
- `-Wformat -Wformat-security` - Format string protection
- `-fno-common` - Symbol collision prevention
- `-fPIC/-fPIE` - Position independent code

### Linker Hardening (Linux)
- `-Wl,-z,relro` - Read-only relocations
- `-Wl,-z,now` - Immediate binding (full RELRO)
- `-Wl,-z,noexecstack` - Non-executable stack

### Runtime Sanitizers
- **AddressSanitizer** - Memory errors (use-after-free, overflows)
- **ThreadSanitizer** - Race conditions
- **UndefinedBehaviorSanitizer** - Undefined behavior detection

---

## Performance Considerations

### Build Time
- **Fresh build (Release, -j8):** 30-60 seconds
- **Incremental (single file):** 2-10 seconds
- **CUDA kernel build:** 20-40 seconds each

### Runtime Performance
- **Library Size:** 3.0 MB (Release, stripped)
- **Memory Overhead:** ~100 MB base (brain-size dependent)
- **GPU Acceleration:** Available for compute-intensive operations
- **Thread Safety:** Full thread safety with optional sanitizers

### Optimization Tips
- Use `-DCMAKE_BUILD_TYPE=Release` for production
- Enable CUDA if GPU available for compute-bound workloads
- Use ccache for faster incremental builds
- Parallel builds with `make -j$(nproc)` or ninja

---

## Common Workflows

### Development
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
make -j$(nproc)
ctest -j$(nproc)
```

### Production Release
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
sudo make install
pkg-config --cflags --libs nimcp  # Verify
```

### GPU Acceleration Testing
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release  # CUDA auto-detected
make -j$(nproc)
./bin/gpu_integration_test
```

### Code Coverage Analysis
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
make -j$(nproc)
ctest
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

---

## Known Limitations & Workarounds

| Issue | Workaround |
|-------|-----------|
| ASAN ↔ TSAN conflict | Choose one sanitizer, not both |
| CUDA on non-NVIDIA | CPU fallback automatic |
| Windows support limited | Use WSL2 with Linux image |
| macOS missing CUDA | No GPU, CPU-only (functional) |
| Old GTest version | CMake fetches v1.14.0 automatically |

---

## File Locations Reference

| Component | Path |
|-----------|------|
| Root CMakeLists.txt | `/home/bbrelin/nimcp/CMakeLists.txt` |
| Core library config | `/home/bbrelin/nimcp/src/lib/CMakeLists.txt` |
| Python bindings config | `/home/bbrelin/nimcp/src/python/CMakeLists.txt` |
| Test configuration | `/home/bbrelin/nimcp/test/CMakeLists.txt` |
| Examples config | `/home/bbrelin/nimcp/examples/CMakeLists.txt` |
| Package template | `/home/bbrelin/nimcp/NIMCPConfig.cmake.in` |
| pkg-config template | `/home/bbrelin/nimcp/nimcp.pc.in` |
| Docker build | `/home/bbrelin/nimcp/Dockerfile` |
| Installation script | `/home/bbrelin/nimcp/install.sh` |
| Setup env script | `/home/bbrelin/nimcp/setup_env.sh` (generated) |

---

## Quick Command Reference

```bash
# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
make -j$(nproc)

# Test
ctest -j$(nproc)

# Install
sudo make install

# Verify
pkg-config --cflags --libs nimcp
python3 -c "import nimcp"

# Run example
./bin/simple_demo

# Clean
make clean
rm -rf build/  # Full clean
```

---

## Documentation Files Generated

1. **BUILD_SYSTEM_ANALYSIS.md** - Comprehensive technical documentation
   - CMake configuration details
   - Build targets and dependencies
   - Installation structure
   - Platform support
   - Build options
   - Testing framework
   - Deployment procedures

2. **BUILD_ARCHITECTURE_DIAGRAMS.md** - Visual architecture representations
   - CMake hierarchy
   - Build dependency graph
   - Library versioning
   - Source organization
   - CUDA compilation flow
   - Test framework structure
   - Deployment architecture
   - Compilation process flow
   - Feature enablement decision tree
   - Platform compatibility matrix

3. **BUILD_QUICK_REFERENCE.md** - Fast lookup guide
   - Common build commands
   - CMake options
   - Test execution
   - Dependency installation
   - Environment setup
   - Example building
   - IDE integration
   - Performance profiling
   - Troubleshooting

4. **BUILD_SYSTEM_SUMMARY.md** - This document
   - Executive overview
   - Key statistics
   - Architecture summary
   - Deployment options
   - Security features
   - Common workflows

---

## Next Steps

### For Developers
1. Read **BUILD_QUICK_REFERENCE.md** for common tasks
2. Refer to **BUILD_SYSTEM_ANALYSIS.md** for technical details
3. Use **BUILD_ARCHITECTURE_DIAGRAMS.md** for visual understanding

### For DevOps/Deployment
1. Study **Dockerfile** for containerization
2. Review **install.sh** for automated installation
3. Check systemd service files for production deployment
4. Refer to DEPLOYMENT.md (generated by install.sh)

### For Contributors
1. Ensure builds pass: `cmake .. && make && ctest`
2. Run sanitizers in development: `-DENABLE_ASAN=ON`
3. Check coverage: `-DENABLE_COVERAGE=ON`
4. Verify on multiple platforms if possible

---

## Support & Troubleshooting

**Build Issues?**
- Check CMAKE output: `cmake --debug-output`
- Verify dependencies: `pkg-config --cflags --libs <lib>`
- Review compiler output: `make VERBOSE=1`

**Test Failures?**
- Run verbose: `ctest -V --rerun-failed`
- Check environment: `echo $LD_LIBRARY_PATH`
- Review test logs: `ctest -O test_output.log`

**Installation Problems?**
- Verify libraries: `ldconfig -p | grep nimcp`
- Check pkg-config: `pkg-config --cflags --libs nimcp`
- Test Python: `python3 -c "import nimcp"`

---

## Resources

- **CMake Documentation:** https://cmake.org/documentation/
- **Google Test:** https://github.com/google/googletest
- **CUDA Development:** https://developer.nvidia.com/cuda-toolkit
- **pkg-config:** https://www.freedesktop.org/wiki/Software/pkg-config/
- **GNU Install Dirs:** https://www.gnu.org/prep/standards/html_node/Directory-Variables.html

---

**Version:** NIMCP 2.6.2  
**Build System:** CMake 3.10+  
**Last Updated:** November 11, 2025

For detailed technical information, see **BUILD_SYSTEM_ANALYSIS.md**  
For visual representations, see **BUILD_ARCHITECTURE_DIAGRAMS.md**  
For quick commands, see **BUILD_QUICK_REFERENCE.md**
