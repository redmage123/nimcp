# NIMCP Build System - Architecture Diagrams

Visual representations of the build system structure, dependencies, and data flow.

---

## 1. CMake Configuration Hierarchy

```
CMakeLists.txt (root)
│
├─ Version: 2.6.2
├─ Languages: C (C11), CXX (C++17)
├─ Minimum CMake: 3.10
│
├─── DEPENDENCIES DETECTION
│    ├─ Python3 (REQUIRED)
│    ├─ GTest (REQUIRED - fetched if needed)
│    ├─ CUDA (OPTIONAL - auto-detect)
│    ├─ libsodium (OPTIONAL - auto-detect)
│    └─ External: lz4, jansson, pthread
│
├─── FEATURES & OPTIONS
│    ├─ ENABLE_ASAN ...................... OFF (default)
│    ├─ ENABLE_UBSAN ..................... OFF (default)
│    ├─ ENABLE_TSAN ..................... OFF (default)
│    ├─ ENABLE_HARDENING ................ ON (default, Release)
│    ├─ ENABLE_FUZZING .................. OFF (default)
│    └─ ENABLE_COVERAGE ................. OFF (default)
│
└─── SUBDIRECTORIES
     ├─ src/ (main library)
     │  ├─ lib/CMakeLists.txt ........... nimcp (SHARED library)
     │  └─ python/CMakeLists.txt ....... nimcp_python (MODULE)
     ├─ examples/CMakeLists.txt ........ 17+ demo executables
     └─ test/CMakeLists.txt ............ Auto-discovered test suite
```

---

## 2. Build Target Dependency Graph

```
External Libraries
├─ Python3::Python
├─ lz4
├─ jansson
├─ pthread
├─ CUDA::cudart (optional)
└─ libsodium (optional)
         │
         ↓
┌─────────────────────────────────────────┐
│   TARGET: nimcp (SHARED LIBRARY)        │
│   Output: libnimcp.so.2.6.2             │
│   SOVERSION: 2 (ABI compatibility)      │
│   Location: bin/                        │
├─────────────────────────────────────────┤
│ Components: 140+ source files           │
│ - Core brain (10 modules)               │
│ - Cognitive functions (20 modules)      │
│ - Plasticity mechanisms (9)             │
│ - Glial support (5)                     │
│ - Networking (5)                        │
│ - I/O & serialization (5)               │
│ - Utilities (30+)                       │
│ - GPU support (3 modules + 3 kernels)   │
└─────────────────────────────────────────┘
         │
         ├─────────────────────────────────┐
         │                                 │
         ↓                                 ↓
    ┌──────────────┐         ┌──────────────────────┐
    │ Python Module│         │ Test Executables     │
    │ nimcp_python │         │ unit_test_*          │
    │ (MODULE)     │         │ integration_test_*   │
    │ nimcp.so     │         │ e2e_test_*          │
    └──────────────┘         └──────────────────────┘
         │                         │
         ↓                         ↓
    lib/python/                build/
    nimcp.so                   - Unit tests
                               - Integration tests
                               - E2E tests
                               - Regression tests
                               - Fuzz tests
```

---

## 3. Library Versioning & Symlink Strategy

```
Version Number: 2.6.2
│
├─ MAJOR: 2 ............ ABI compatibility version (SOVERSION)
├─ MINOR: 6 ............ Feature additions (backward compatible)
└─ PATCH: 2 ............ Bug fixes (no API changes)

At Runtime:
┌─────────────────────────────────────────────┐
│  Symlink Chain (in /usr/local/lib/)         │
├─────────────────────────────────────────────┤
│  libnimcp.so ──→ libnimcp.so.2 ──→ libnimcp.so.2.6.2
│  (latest)      (ABI version)      (actual binary)
│                                    (3.0 MB)
└─────────────────────────────────────────────┘

Linking Rules:
- New code: links to libnimcp.so (latest)
- ABI-stable: links to libnimcp.so.2
- Apps compiled against 2.5.0 still work with 2.6.2
  (same SOVERSION = same ABI)
```

---

## 4. Source Code Organization & Compilation Flow

```
src/
├─ api/
│  └─ nimcp.c ..................... Public API facade
│
├─ core/
│  ├─ brain/
│  │  ├─ nimcp_brain.c ........... Brain entity
│  │  └─ nimcp_distributed_cow.c  Copy-on-write cloning
│  ├─ neuralnet/
│  ├─ neuron_types/
│  ├─ neuron_models/
│  ├─ synapse_compute/
│  ├─ synapse_types/
│  ├─ topology/
│  └─ brain_oscillations/
│
├─ cognitive/ (20+ modules)
│  ├─ ethics/
│  ├─ knowledge/
│  ├─ working_memory/
│  ├─ executive/
│  └─ ...
│
├─ plasticity/ (9 modules)
│  ├─ stdp/
│  ├─ stp/
│  ├─ attention/
│  └─ ...
│
├─ glial/ (5 modules)
├─ networking/ (5 modules)
├─ io/ (5 modules)
├─ gpu/ (3 modules + 3 .cu kernels)
├─ utils/ (30+ modules)
├─ nlp/ (2 modules)
├─ python/ (3 files - bindings)
│
└─ lib/CMakeLists.txt
   │
   └─ Compilation Steps:
      1. Collect all .c files
      2. Collect .cu files (if CUDA available)
      3. Set compile flags
      4. Link external libraries
      5. Generate: libnimcp.so.2.6.2
```

---

## 5. CUDA Conditional Compilation

```
CMake Configuration Phase
│
├─ check_language(CUDA)
│  │
│  └─ CUDA compiler available?
│     │
│     ├─ YES ──→ find_package(CUDAToolkit)
│     │           │
│     │           ├─ add_definitions(-DNIMCP_ENABLE_CUDA)
│     │           ├─ Set architectures: 75, 80, 86, 89
│     │           └─ Enable CUDA as language
│     │
│     └─ NO ──→ (CUDA disabled, CPU fallback)
│
└─ Compilation Phase
   │
   ├─ C/C++ Files: Compiled normally with GCC/Clang
   │
   └─ CUDA Files: (Only if CUDAToolkit_FOUND)
      ├─ nimcp_gpu_kernels.cu
      ├─ nimcp_synapse_compute_gpu.cu
      └─ nimcp_neural_logic_kernels.cu
         │
         └─ Compiled with NVCC
            - CUDA_SEPARABLE_COMPILATION: ON
            - CUDA_STANDARD: 14
            - Incompatible GCC flags filtered
            - Per-file compile options: --expt-relaxed-constexpr

Result:
├─ CUDA available: Full GPU acceleration
│  - GPU neuron computations
│  - GPU synapse calculations
│  - GPU neural logic gates
│
└─ CUDA unavailable: CPU-only fallback
   - GPU API present, but executes on CPU
   - Identical API, degraded performance
```

---

## 6. Test Framework Architecture

```
test/ (Code Surgeon Integration)
│
├─ CMakeLists.txt ..................... Master test config
│  │
│  ├─ discover_category_tests(CATEGORY)
│  │  └─ For each test_*.cpp file:
│  │     └─ Create {CATEGORY}_{TESTNAME} executable
│  │
│  └─ Enable CTest integration
│
├─ unit/ ............................ Per-module testing
│  ├─ test_brain.cpp ───→ unit_test_brain
│  ├─ test_neuron.cpp ──→ unit_test_neuron
│  └─ test_*.cpp
│
├─ integration/ ..................... Cross-module testing
│  ├─ test_*.cpp ───→ integration_test_*
│
├─ e2e/ ............................. End-to-end testing
│  ├─ test_*.cpp ───→ e2e_test_*
│
├─ regression/ ....................... Bug reproduction
│  ├─ test_*.cpp ───→ regression_test_*
│
└─ fuzz/ ............................ Property-based
   ├─ test_*.cpp ───→ fuzz_test_*

Test Execution:
│
ctest [options]
├─ ctest -j$(nproc) ........... Run all tests in parallel
├─ ctest -L unit -j$(nproc) .. Run only unit tests
├─ ctest -L integration ....... Run only integration
├─ ctest -L e2e ............... Run only E2E
├─ ctest -V ................... Verbose output
└─ ctest -R pattern ........... Run tests matching pattern

Test Configuration:
├─ Each test links:
│  ├─ nimcp (core library)
│  ├─ GTest::GTest
│  ├─ GTest::Main
│  ├─ Python3::Python
│  └─ pthread
│
└─ Each test enables:
   ├─ NIMCP_TESTING compile flag
   ├─ C++20 standard
   └─ PYTHONPATH environment variable
```

---

## 7. Installation & Deployment Flow

```
Build Phase
│
├─ cmake .. -DCMAKE_BUILD_TYPE=Release \
│           -DCMAKE_INSTALL_PREFIX=/usr/local
│
└─ make -j$(nproc)
   │
   ├─ Builds: libnimcp.so.2.6.2
   ├─ Builds: nimcp.so (Python module)
   ├─ Builds: examples (17+ demos)
   └─ Builds: tests
   
Install Phase
│
└─ make install (or sudo make install)
   │
   ├─ Install Library
   │  └─ libnimcp.so.2.6.2 → /usr/local/lib/
   │     ├─ symlink: libnimcp.so.2
   │     └─ symlink: libnimcp.so
   │
   ├─ Install Headers
   │  └─ src/**/*.h → /usr/local/include/nimcp/
   │
   ├─ Install Python Module
   │  └─ nimcp.so → /usr/local/lib/python3/dist-packages/
   │
   ├─ Install Examples
   │  └─ demo programs → /usr/local/bin/examples/
   │
   ├─ Install Pkg-config
   │  └─ nimcp.pc → /usr/local/lib/pkgconfig/
   │
   └─ Install CMake Configs
      ├─ NIMCPConfig.cmake
      ├─ NIMCPConfigVersion.cmake
      └─ NIMCPTargets.cmake
         └─ /usr/local/lib/cmake/NIMCP/

Discovery by Downstream Projects
│
├─ pkg-config:
│  └─ pkg-config --cflags --libs nimcp
│     → -I/usr/local/include -L/usr/local/lib -lnimcp
│
└─ CMake:
   └─ find_package(NIMCP 2.6)
      └─ NIMCP::core target available
```

---

## 8. Deployment Architecture

```
Development Machine
│
├─ Source Code (git repo)
└─ Build System (CMake)
   │
   ├─ Option 1: Manual Install
   │  └─ chmod +x install.sh && ./install.sh
   │     └─ Creates:
   │        ├─ /usr/local/lib/libnimcp.so*
   │        ├─ /usr/local/include/nimcp/
   │        ├─ setup_env.sh
   │        └─ systemd service files
   │
   ├─ Option 2: Docker Build
   │  └─ docker build -t nimcp:2.6.2 .
   │     │
   │     ├─ Stage 1: Builder
   │     │  └─ Ubuntu 22.04 + build tools
   │     │     └─ Compiles NIMCP
   │     │        └─ Runs tests
   │     │           └─ Artifacts: libnimcp.so, nimcp.so
   │     │
   │     └─ Stage 2: Runtime
   │        └─ Ubuntu 22.04 + runtime only
   │           └─ Minimal image (300MB)
   │              └─ Non-root user
   │                 └─ Health check enabled
   │
   ├─ Option 3: Systemd Services
   │  └─ Create nimcp-backend.service
   │     └─ Create nimcp-frontend.service
   │        └─ sudo systemctl start nimcp-*
   │
   └─ Option 4: Production Nginx
      └─ nginx (reverse proxy)
         ├─ Frontend: React build (port 80)
         └─ Backend: Flask (port 5000)

Runtime Environment
│
├─ LD_LIBRARY_PATH=/usr/local/lib
│  └─ (for finding libnimcp.so)
│
├─ PYTHONPATH=/usr/local/lib/python3/dist-packages
│  └─ (for finding nimcp.so)
│
├─ NIMCP_HOME=/usr/local/nimcp
├─ NIMCP_DATA=/var/lib/nimcp
└─ NIMCP_LOG=/var/log/nimcp
```

---

## 9. Compilation Process - Detailed View

```
Input: CMakeLists.txt + Sources
│
Step 1: CMake Configuration
├─ Detect compiler (GCC 11)
├─ Detect dependencies (Python3, CUDA, libsodium)
├─ Validate options (sanitizers)
├─ Check security hardening
├─ Generate build files (Makefiles or Ninja)
└─ Create compile_commands.json
   
Step 2: Pre-compilation
├─ Set compile flags:
│  ├─ C flags: -Wall -Wextra -Wpedantic
│  ├─ C++ flags: -Wall -Wextra -Wpedantic
│  └─ Hardening (Release): -fstack-protector-strong, FORTIFY_SOURCE=2
│
├─ Set include paths:
│  ├─ src/ (main headers)
│  ├─ src/include/ (public API)
│  ├─ python3/include/ (Python C API)
│  └─ cuda/include/ (if CUDA available)
│
└─ Set library paths:
   ├─ python3/lib
   ├─ cuda/lib
   └─ /usr/lib (system libraries)

Step 3: Compilation
├─ C Files (140+)
│  ├─ nimcp_brain.c ──→ brain.o
│  ├─ nimcp_ethics.c ─→ ethics.o
│  └─ ... ────────────→ ...
│
└─ CUDA Files (3, if CUDA available)
   ├─ nimcp_gpu_kernels.cu ──→ gpu_kernels.o
   ├─ (compiled with NVCC)
   └─ (architecture-specific code)

Step 4: Linking
├─ Link all .o files
├─ Link external libraries:
│  ├─ Python3::Python
│  ├─ lz4
│  ├─ jansson
│  ├─ CUDA::cudart (if compiled)
│  └─ libsodium (if found)
│
└─ Create shared library:
   └─ libnimcp.so.2.6.2

Step 5: Post-build
├─ Create symlinks:
│  ├─ libnimcp.so.2 → libnimcp.so.2.6.2
│  └─ libnimcp.so ──→ libnimcp.so.2
│
└─ Copy to output directory:
   └─ bin/libnimcp.so*

Final Output:
bin/
├─ libnimcp.so.2.6.2 (3.0 MB)
├─ libnimcp.so.2 ────→ libnimcp.so.2.6.2
└─ libnimcp.so ──────→ libnimcp.so.2
```

---

## 10. Feature Enablement Decision Tree

```
CMake Configuration Phase
│
├─ CUDA Support?
│  ├─ check_language(CUDA)
│  │  ├─ YES → find_package(CUDAToolkit)
│  │  │        ├─ YES → ENABLE_CUDA=1
│  │  │        │        ├─ Compile .cu files
│  │  │        │        ├─ Link CUDA::cudart
│  │  │        │        └─ Define NIMCP_ENABLE_CUDA
│  │  │        └─ NO  → ENABLE_CUDA=0 (fallback)
│  │  └─ NO  → ENABLE_CUDA=0
│  │
│  └─ Result: GPU acceleration (or CPU fallback)
│
├─ Encryption Support?
│  ├─ find_library(SODIUM_LIBRARY)
│  │  ├─ YES → SODIUM_FOUND=1
│  │  │        ├─ Compile nimcp_encryption.c
│  │  │        ├─ Link libsodium
│  │  │        └─ Define NIMCP_ENABLE_ENCRYPTION
│  │  └─ NO  → SODIUM_FOUND=0
│  │
│  └─ Result: Encrypted I/O (or plaintext)
│
├─ Security Hardening?
│  ├─ CMAKE_BUILD_TYPE == Release?
│  │  ├─ YES → Apply hardening flags
│  │  │        ├─ -fstack-protector-strong
│  │  │        ├─ -D_FORTIFY_SOURCE=2
│  │  │        └─ -Wl,-z,relro -Wl,-z,now
│  │  └─ NO  → Partial hardening (debug)
│  │
│  └─ Result: Secure binary (prod vs dev)
│
├─ Testing?
│  ├─ find_package(GTest)
│  │  ├─ YES → Use system GTest
│  │  └─ NO  → FetchContent googletest v1.14.0
│  │
│  └─ Result: Enable ctest
│
├─ Fuzzing? (-DENABLE_FUZZING=ON)
│  ├─ YES → Compile src/fuzz/
│  └─ NO  → Skip fuzzing targets
│
├─ Coverage? (-DENABLE_COVERAGE=ON)
│  ├─ YES → Add --coverage flags
│  │        ├─ -fprofile-arcs
│  │        └─ -ftest-coverage
│  └─ NO  → Normal compilation
│
└─ Sanitizers? (mutually exclusive)
   ├─ ENABLE_ASAN=ON
   │  └─ -fsanitize=address (memory errors)
   ├─ ENABLE_TSAN=ON
   │  └─ -fsanitize=thread (data races)
   ├─ ENABLE_UBSAN=ON
   │  └─ -fsanitize=undefined (UB detection)
   └─ Note: ASAN ↔ TSAN are mutually exclusive
```

---

## 11. Platform Compatibility Matrix

```
                  Linux (x86_64)   Linux (ARM64)   macOS    Windows
                  ──────────────   ─────────────   ─────    ───────
Build System      CMake 3.10+      CMake 3.10+     CMake    CMake
C Compiler        GCC 9+ / Clang   GCC 9+ / Clang  Clang    MSVC/Clang
C++ Compiler      GCC 9+ / Clang   GCC 9+ / Clang  Clang    MSVC/Clang
C Standard        C11              C11             C11      C11
C++ Standard      C++17            C++17           C++17    C++17
Python Support    3.7-3.11         3.7-3.11        3.7-3.11 3.7-3.11
CUDA Support      Yes              Partial         No       Partial
libsodium         Yes              Yes             Yes      Yes
Hardening Flags   Full (Release)   Full (Release)  Partial  No
Sanitizers        ASAN/TSAN/UBSAN  ASAN/TSAN/UBSAN Limited  Limited
Docker            Yes              Yes             Yes      Yes
systemd Services  Yes              Yes             No       No

Status:
✓ Primary platform (fully tested)
~ Supported platform (tested, minor issues possible)
○ Experimental platform (limited testing)
```

---

## 12. Build Time Analysis

```
Typical Build Times (Release, -j8):
└─ Fresh build

├─ CMake configuration: 2-5 seconds
│  └─ Dependency detection
│
├─ Compilation: 30-60 seconds
│  ├─ 140+ C files
│  ├─ 3 CUDA kernels (if CUDA available)
│  │  └─ NVCC is slower (single-threaded per kernel)
│  └─ Parallel: -j$(nproc)
│
├─ Linking: 5-15 seconds
│  └─ Large library with external deps
│
├─ Testing: 10-60 seconds
│  └─ Depends on test complexity
│     └─ ctest -j$(nproc) for parallel
│
└─ Installation: <5 seconds
   └─ Copy files to system directories

Incremental Build Times:
├─ No changes: 0 seconds (up to date)
├─ Single file change: 2-10 seconds
├─ Header change: 5-20 seconds (rebuilds dependents)
└─ CUDA change: 20-40 seconds (re-kernelizes)
```

---

*This diagram document provides visual architecture understanding for NIMCP build system.*
