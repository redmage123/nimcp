# NIMCP Build System - Quick Reference Guide

Fast lookup for common build tasks and configurations.

---

## Quick Build Commands

### Basic Build
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Debug Build
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

### Build with AddressSanitizer
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
make -j$(nproc)
./bin/simple_demo  # Run with ASAN checking
```

### Build with ThreadSanitizer
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
make -j$(nproc)
```

### Build with Coverage
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
make -j$(nproc)
ctest -j$(nproc)
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

### Build with Fuzzing
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_FUZZING=ON
make -j$(nproc)
./bin/fuzz_test_*
```

### Install System-Wide
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
sudo make install
```

### Docker Build
```bash
docker build -t nimcp:2.6.2 .
docker run -p 8080:8080 -p 9090:9090 nimcp:2.6.2
```

---

## Common CMake Options

| Option | Values | Default | Purpose |
|--------|--------|---------|---------|
| `CMAKE_BUILD_TYPE` | Debug, Release, RelWithDebInfo, MinSizeRel | Debug | Optimization level |
| `CMAKE_INSTALL_PREFIX` | /path/to/prefix | /usr/local | Installation directory |
| `ENABLE_ASAN` | ON, OFF | OFF | Memory error detection |
| `ENABLE_UBSAN` | ON, OFF | OFF | Undefined behavior detection |
| `ENABLE_TSAN` | ON, OFF | OFF | Data race detection |
| `ENABLE_HARDENING` | ON, OFF | ON | Security hardening flags |
| `ENABLE_FUZZING` | ON, OFF | OFF | Fuzzing targets |
| `ENABLE_COVERAGE` | ON, OFF | OFF | Code coverage instrumentation |

---

## Test Commands

### Run All Tests
```bash
ctest -j$(nproc)
```

### Run Tests by Category
```bash
ctest -L unit -j$(nproc)           # Unit tests only
ctest -L integration -j$(nproc)    # Integration tests
ctest -L e2e                        # End-to-end tests
ctest -L regression                 # Regression tests
ctest -L fuzz                       # Fuzzing tests
```

### Verbose Test Output
```bash
ctest -V                            # All tests, verbose
ctest -R pattern -V                 # Specific tests
ctest --rerun-failed                # Rerun failed tests
```

### Test from Source Tree
```bash
# Run tests in parallel
cd build
ctest -j8

# Run specific test with full output
ctest -R test_brain -VV

# Show test names only
ctest --print-labels
```

---

## Cleaning Build

### Remove Build Artifacts
```bash
cd build
make clean                          # Remove .o files
rm -rf *                           # Remove all (start fresh)
```

### Full Clean
```bash
rm -rf build/
rm -rf build-fuzz/
rm -f bin/libnimcp.so*
mkdir build && cd build
cmake .. && make
```

---

## Installation Verification

### Verify Library Installation
```bash
# Check library exists
ls -la /usr/local/lib/libnimcp.so*

# Check headers
ls /usr/local/include/nimcp/

# Check pkg-config
pkg-config --cflags --libs nimcp
# Output: -I/usr/local/include -L/usr/local/lib -lnimcp_core
```

### Verify Python Module
```bash
python3 -c "import nimcp; print(nimcp.__version__)"

# Or using pkg-config
python3 -c "import nimcp; nimcp.Brain('test', 100, 100, 10, 5)"
```

### Verify CMake Discovery
```bash
# In a test project:
cmake .. -DCMAKE_PREFIX_PATH=/usr/local/lib/cmake
# Or:
find /usr/local/lib/cmake -name "NIMCP*"
```

---

## Dependency Installation

### Ubuntu/Debian
```bash
# All required dependencies
sudo apt-get install -y \
    build-essential \
    cmake \
    python3-dev \
    libjansson-dev \
    liblz4-dev \
    pkg-config

# Optional: CUDA support
sudo apt-get install -y nvidia-cuda-toolkit

# Optional: Encryption
sudo apt-get install -y libsodium-dev

# Optional: GTest (if not using FetchContent)
sudo apt-get install -y libgtest-dev
```

### CentOS/RHEL/Fedora
```bash
sudo yum install -y \
    gcc g++ make \
    cmake \
    python3-devel \
    jansson-devel \
    lz4-devel \
    pkgconfig

# Optional: CUDA
sudo yum install -y cuda-toolkit

# Optional: Encryption
sudo yum install -y libsodium-devel
```

---

## Environment Setup

### Set LD_LIBRARY_PATH
```bash
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Or (for development):
export LD_LIBRARY_PATH=$(pwd)/bin:$LD_LIBRARY_PATH
```

### Set PYTHONPATH
```bash
export PYTHONPATH=/usr/local/lib/python3/dist-packages:$PYTHONPATH

# Or (for development):
export PYTHONPATH=$(pwd)/lib/python:$PYTHONPATH
```

### Using setup_env.sh
```bash
source setup_env.sh
# Automatically sets LD_LIBRARY_PATH and PYTHONPATH
```

---

## Building Examples

### Build Specific Example
```bash
# From build directory
make simple_demo

# Then run
./bin/simple_demo
```

### List All Available Examples
```bash
ls examples/*.c | sed 's/.*\///;s/\.c$//' | while read name; do
    [ -x "bin/$name" ] && echo "✓ $name" || echo "○ $name"
done
```

### Build & Run Example
```bash
cd build
make integrated_demo
./bin/integrated_demo
```

---

## IDE Integration

### CLion
```
File → Open → CMakeLists.txt
(CLion auto-detects CMake configuration)
```

### VS Code with CMake Tools
```json
// .vscode/settings.json
{
  "cmake.buildDirectory": "${workspaceFolder}/build",
  "cmake.sourceDirectory": "${workspaceFolder}",
  "cmake.preferredGenerator": "Unix Makefiles",
  "cmake.configureArgs": [
    "-DCMAKE_BUILD_TYPE=Debug"
  ]
}
```

### Command Line Build (Ninja)
```bash
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja

# Or with cmake itself
cmake --build build -j$(nproc)
```

---

## Static Analysis

### cppcheck
```bash
# Install
sudo apt-get install cppcheck

# Analyze
cppcheck --project=build/compile_commands.json src/
cppcheck --enable=all src/
```

### clang-tidy
```bash
# Install
sudo apt-get install clang-tools

# Analyze (from build directory)
clang-tidy -p build/compile_commands.json src/**/*.c src/**/*.cpp
```

---

## Memory Debugging

### Valgrind (Comprehensive Memory Check)
```bash
# Install
sudo apt-get install valgrind

# Run
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./build/bin/simple_demo
```

### AddressSanitizer (Fast, Compiler-integrated)
```bash
cmake .. -DENABLE_ASAN=ON
make
./build/bin/simple_demo  # Automatically detects memory errors
```

### GDB Debugging
```bash
# Build with debug symbols
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Debug
gdb ./build/bin/simple_demo
(gdb) run
(gdb) bt              # Backtrace
(gdb) print var       # Print variable
(gdb) quit
```

---

## Performance Profiling

### Build Release for Profiling
```bash
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
make
```

### Using perf (Linux)
```bash
# Install
sudo apt-get install linux-tools-generic

# Profile
perf record ./build/bin/integrated_demo
perf report

# Flamegraph
perf script | FlameGraph/stackcollapse-perf.pl | \
  FlameGraph/flamegraph.pl > flamegraph.svg
```

### Using time Command
```bash
time ./build/bin/integrated_demo
# Real: actual wall time
# User: CPU time in user space
# Sys: CPU time in kernel space
```

---

## Build Status Checking

### Check Build Configuration
```bash
# From build directory
cat CMakeCache.txt | grep -E "^(CMAKE_BUILD_TYPE|ENABLE_|Python3|CUDAToolkit)"
```

### List Compiled Targets
```bash
cmake --build build --target help
```

### Verbose Build Output
```bash
make VERBOSE=1          # Show compiler commands
cmake --build . --verbose
```

---

## Common Build Issues

### Issue: "Python3 not found"
```bash
# Solution: Install Python development headers
sudo apt-get install python3-dev

# Or specify Python path
cmake .. -DPython3_ROOT_DIR=/usr/bin/python3
```

### Issue: "GTest not found"
```bash
# Solution: Either install system GTest
sudo apt-get install libgtest-dev

# Or let CMake fetch it (automatic)
# Nothing needed, CMake will FetchContent
```

### Issue: "CUDA compiler not found"
```bash
# Solution: Install CUDA Toolkit
sudo apt-get install nvidia-cuda-toolkit

# Or use CPU fallback (automatic)
# Build continues without GPU support
```

### Issue: "Out of memory during build"
```bash
# Solution: Reduce parallel jobs
make -j2  # Use 2 jobs instead of nproc

# Or:
cmake .. -DCMAKE_BUILD_PARALLEL_LEVEL=2
make
```

### Issue: "Linker errors"
```bash
# Solution: Check library installation
pkg-config --cflags --libs lz4
pkg-config --cflags --libs jansson

# Reinstall if missing
sudo apt-get install --reinstall libjansson-dev liblz4-dev
```

---

## Build Performance Tips

### Parallel Compilation
```bash
make -j$(nproc)        # Use all CPU cores
make -j8               # Use 8 cores specifically
```

### Ccache (Compiler Cache)
```bash
# Install
sudo apt-get install ccache

# Use with CMake
cmake .. -DCMAKE_C_COMPILER_LAUNCHER=ccache \
         -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

# Check stats
ccache -s
```

### Ninja Build System (Faster than Make)
```bash
# Install
sudo apt-get install ninja-build

# Configure with Ninja
cmake .. -G Ninja

# Build
ninja

# Parallel jobs
ninja -j8
```

---

## Release Build Checklist

```bash
# 1. Update version if needed
# Edit: CMakeLists.txt (line 2)
# project(nimcp VERSION X.Y.Z LANGUAGES C CXX)

# 2. Clean build
rm -rf build && mkdir build && cd build

# 3. Configure Release
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_INSTALL_PREFIX=/usr/local

# 4. Build
make -j$(nproc)

# 5. Run tests
ctest -j$(nproc)

# 6. Check coverage (optional)
# (Rebuild with ENABLE_COVERAGE if needed)

# 7. Run static analysis
cppcheck --enable=all ../src/

# 8. Install locally first
make install

# 9. Verify installation
pkg-config --cflags --libs nimcp
python3 -c "import nimcp; print(nimcp.__version__)"

# 10. Create distribution archive (if needed)
cd ..
tar czf nimcp-2.6.2.tar.gz src/ examples/ CMakeLists.txt ...
```

---

## Documentation Build

### Generate Doxygen Docs (if configured)
```bash
# Install Doxygen
sudo apt-get install doxygen graphviz

# Generate
doxygen Doxyfile  # if exists

# Or create simple one
doxygen -g Doxyfile
doxygen Doxyfile
```

---

## Troubleshooting Workflow

```
Problem occurs
    │
    ├─ Check CMake output
    │  cmake --debug-output
    │
    ├─ Check compiler output
    │  make VERBOSE=1
    │
    ├─ Check dependencies
    │  pkg-config --cflags --libs <library>
    │
    ├─ Check environment
    │  echo $LD_LIBRARY_PATH
    │  echo $PYTHONPATH
    │
    └─ Reset and rebuild
       rm -rf build/
       mkdir build && cd build
       cmake ..
       make -j$(nproc)
```

---

## Key Files Reference

| File | Purpose |
|------|---------|
| `/home/bbrelin/nimcp/CMakeLists.txt` | Root CMake configuration |
| `/home/bbrelin/nimcp/src/lib/CMakeLists.txt` | Core library build |
| `/home/bbrelin/nimcp/src/python/CMakeLists.txt` | Python bindings build |
| `/home/bbrelin/nimcp/test/CMakeLists.txt` | Test framework |
| `/home/bbrelin/nimcp/examples/CMakeLists.txt` | Examples build |
| `/home/bbrelin/nimcp/NIMCPConfig.cmake.in` | CMake package template |
| `/home/bbrelin/nimcp/nimcp.pc.in` | pkg-config template |
| `/home/bbrelin/nimcp/Dockerfile` | Docker build |
| `/home/bbrelin/nimcp/install.sh` | Installation script |

---

## Version Quick Facts

- **Current Version:** 2.6.2
- **SOVERSION (ABI):** 2
- **C Standard:** C11
- **C++ Standard:** C++17
- **CMake Minimum:** 3.10
- **Python Supported:** 3.7-3.11
- **CUDA Support:** 11.0+ (auto-detect)
- **License:** (Check LICENSE file)

---

*Keep this reference handy for development and deployment tasks!*
