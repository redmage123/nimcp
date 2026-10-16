# NIMCP Build System - Complete Analysis

**Project:** NIMCP (Neural Inference for Massive Concurrent Processing)  
**Version:** 2.6.2  
**Build System:** CMake 3.10+  
**Languages:** C (C11), C++ (C++17)

---

## Table of Contents

1. [CMake Configuration](#cmake-configuration)
2. [Build Targets](#build-targets)
3. [Dependencies](#dependencies)
4. [Build Outputs](#build-outputs)
5. [Installation Structure](#installation-structure)
6. [Platform Support](#platform-support)
7. [Build Configuration Options](#build-configuration-options)
8. [Testing Framework](#testing-framework)
9. [Deployment](#deployment)
10. [Developer Workflows](#developer-workflows)

---

## 1. CMake Configuration

### Root CMakeLists.txt Location
**File:** `/home/bbrelin/nimcp/CMakeLists.txt`

### Project Definition
```cmake
project(nimcp VERSION 2.6.2 LANGUAGES C CXX)

# C/C++ Standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

### Key Features
- **Minimum CMake Version:** 3.10
- **Default Build Type:** Debug (can be overridden with `-DCMAKE_BUILD_TYPE=Release`)
- **Project Structure:** Hierarchical with colocated headers
  - `/src/lib/` - Core library
  - `/src/python/` - Python bindings
  - `/src/tests/` - Test framework
  - `/test/` - Code Surgeon integrated tests
  - `/examples/` - Demo applications

### Include Directories
```cmake
set(NIMCP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(NIMCP_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/lib)
set(NIMCP_PYTHON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/python)
set(NIMCP_TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)
```

---

## 2. CMake Build Targets

### 2.1 Core Library Target

**Target Name:** `nimcp` (SHARED library)  
**Output:** `libnimcp.so.2.6.2` → `libnimcp.so.2` → `libnimcp.so`  
**Location:** `/home/bbrelin/nimcp/bin/`

#### Library Properties
```cmake
set_target_properties(nimcp PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    ENABLE_EXPORTS ON
    VERSION ${PROJECT_VERSION}      # 2.6.2
    SOVERSION 2                      # ABI version
    OUTPUT_NAME nimcp
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)
```

#### Soversion Scheme
- **Full Version:** 2.6.2 (patch updates)
- **SOVERSION:** 2 (ABI compatibility)
- **Symlinks:** `libnimcp.so.2` → `libnimcp.so.2.6.2` → `libnimcp.so`

### 2.2 Python Module Target

**Target Name:** `nimcp_python` (MODULE)  
**Type:** Python C extension  
**Output:** `nimcp.so` (no lib prefix)  
**Location:** `${CMAKE_BINARY_DIR}/lib/python/`

#### Python Module Properties
```cmake
Python3_add_library(nimcp_python MODULE nimcp_module.c)
set_target_properties(nimcp_python PROPERTIES
    PREFIX ""                       # No lib prefix for .so
    OUTPUT_NAME "nimcp"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/python"
    POSITION_INDEPENDENT_CODE ON
    ENABLE_EXPORTS ON
)
```

### 2.3 Example Targets

Built in `/home/bbrelin/nimcp/examples/CMakeLists.txt`

| Target | Purpose | Status |
|--------|---------|--------|
| `simple_demo` | Primary reference implementation | Active |
| `brain_demo` | Brain API demonstration | Active |
| `ethics_demo` | Ethics engine showcase | Active |
| `infant_demo` | Infant learning concepts | Active |
| `integrated_demo` | Full system integration | Active |
| `event_demo` | Event processing | Active |
| `brain_probe_demo` | Brain inspection API | Active |
| `izhikevich_demo` | Neuron model demo | Active |
| `nlp_integration_test` | NLP + synapses + attention | Active |
| `fractal_network_demo` | Scale-free topology | Active |
| `multimodal_integration_demo` | Visual + audio + speech | Active |
| `speech_cortex_demo` | Speech processing | Active |
| `gpu_integration_test` | GPU/CPU hybrid | Active |
| `cow_inference_server` | Copy-on-Write efficiency | Active |
| `cow_snapshot_learning` | COW checkpointing | Active |
| `cow_ab_testing` | A/B testing with COW | Active |
| `distributed_cow_demo` | Cross-node brain sharing | Active |
| `programmable_synapses_demo` | Synapses API | Disabled (API mismatch) |

### 2.4 Test Targets

Built via `test/CMakeLists.txt` with automatic discovery

#### Test Categories
```
test/
├── unit/            - Unit tests (per-module)
├── integration/     - Cross-module tests
├── e2e/            - End-to-end tests
├── regression/     - Bug reproduction
└── fuzz/           - Property-based fuzzing
```

#### Test Target Naming
Format: `{category}_{testname}`
- Example: `unit_test_brain.cpp` → target `unit_test_brain`
- All tests link with: `nimcp`, `GTest::GTest`, `GTest::Main`, `Python3::Python`, `pthread`

---

## 3. Dependencies

### 3.1 Core Dependencies (Required)

#### Python 3
```cmake
find_package(Python3 COMPONENTS Development REQUIRED)
```
- **Purpose:** Python C API for bindings
- **Versions Supported:** 3.7+
- **Files:** `python.h` from Python3::Python

#### CMake Modules
- `CheckLanguage` - CUDA language detection
- `GNUInstallDirs` - Standard install directories
- `CMakePackageConfigHelpers` - Package config generation

### 3.2 Optional Dependencies (Auto-Detected)

#### CUDA Toolkit (Optional)
```cmake
check_language(CUDA)
find_package(CUDAToolkit QUIET)

if(CUDAToolkit_FOUND)
    add_definitions(-DNIMCP_ENABLE_CUDA)
    set(CMAKE_CUDA_ARCHITECTURES "75;80;86;89")  # Turing, Ampere, Ada
endif()
```

**Features When Available:**
- GPU kernel compilation (`.cu` files)
- CUDA runtime linking (`CUDA::cudart`)
- Device memory management

**CUDA Source Files:**
- `/src/gpu/neuron/nimcp_gpu_kernels.cu`
- `/src/gpu/synapse_compute/nimcp_synapse_compute_gpu.cu`
- `/src/core/neuron_types/nimcp_neural_logic_kernels.cu`

**Architectures Supported:**
- 75 (Turing - RTX 20/30 series)
- 80 (Ampere - RTX 40 series, A100)
- 86 (Ada Lovelace - RTX 40 series, L40/H100)
- 89 (Ada Lovelace variant)

#### libsodium (Optional - Encryption)
```cmake
find_library(SODIUM_LIBRARY NAMES sodium libsodium)
find_path(SODIUM_INCLUDE_DIR NAMES sodium.h)

if(SODIUM_FOUND)
    add_definitions(-DNIMCP_ENABLE_ENCRYPTION)
    # Enables: nimcp_encryption.c
endif()
```

**Features When Available:**
- Encryption/decryption support
- Defined in: `-DNIMCP_ENABLE_ENCRYPTION`

#### Google Test (Testing)
```cmake
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googletest)
endif()
```

**Behavior:**
- Uses system GTest if available
- Otherwise fetches v1.14.0 from GitHub
- Creates aliases: `GTest::GTest`, `GTest::Main`

### 3.3 External Libraries (Linked)

| Library | Version | Purpose | Link Type |
|---------|---------|---------|-----------|
| `lz4` | system | Compression | PUBLIC |
| `jansson` | system | JSON parsing | PUBLIC |
| `Python3::Python` | 3.7+ | Python C API | PUBLIC |
| `CUDA::cudart` | 11.0+ | GPU runtime | PUBLIC (if CUDA) |
| `libsodium` | system | Encryption | PUBLIC (if found) |
| `pthread` | system | Threading | PRIVATE (tests) |

### 3.4 System Dependencies for Build

#### Debian/Ubuntu
```bash
build-essential
cmake >= 3.10
git
python3
python3-pip
python3-dev
libjansson-dev       # JSON
liblz4-dev          # Compression
pkg-config
curl wget            # Utilities
```

#### RHEL/CentOS/Fedora
```bash
gcc g++             # build-essential equiv
make cmake >= 3.10
git
python3 python3-devel
jansson-devel
lz4-devel
pkgconfig
curl wget
```

#### Optional (GPU Support)
```
NVIDIA CUDA Toolkit >= 11.0
cuDNN (for optimized kernels)
nvidia-driver >= 450
```

---

## 4. Build Outputs

### 4.1 Output Directory Structure

```
${CMAKE_BINARY_DIR}/
├── bin/
│   ├── libnimcp.so.2.6.2          # Main library (shared)
│   ├── libnimcp.so.2               # SOVERSION symlink
│   ├── libnimcp.so                 # Latest version symlink
│   ├── simple_demo                 # Examples (executables)
│   ├── brain_demo
│   ├── ethics_demo
│   ├── integrated_demo
│   └── ... (other examples)
│
├── lib/
│   ├── libgtest.a                  # Google Test library
│   ├── libgtest_main.a
│   └── python/
│       └── nimcp.so                # Python C extension
│
├── _deps/                          # FetchContent downloads
│   └── googletest-src/
│
└── CTestTestfile.cmake             # Test registry
```

### 4.2 Library Versioning

```
libnimcp.so.2.6.2 (actual file)
   ↑
   └─ libnimcp.so.2 (symlink for SOVERSION compatibility)
       ↑
       └─ libnimcp.so (symlink for latest version)
```

**Current State:**
```bash
lrwxrwxrwx  1 ... libnimcp.so -> libnimcp.so.2
lrwxrwxrwx  1 ... libnimcp.so.2 -> libnimcp.so.2.6.2
-rwxrwxr-x  1 ... libnimcp.so.2.6.2 (3MB - actual binary)
```

### 4.3 Compiled Sources in Library

**Total Source Count:** 140+ C files + 3 CUDA kernels

#### By Category

**Brain Components (10):**
- `nimcp_brain.c` - Main brain API
- `nimcp_distributed_cow.c` - Copy-on-write cloning
- `nimcp_neuralnet.c` - Neural network
- `nimcp_brain_regions.c` - Brain region abstractions
- `nimcp_multimodal_integration.c` - Multi-modal fusion
- `nimcp_brain_oscillations.c` - Oscillation patterns
- Plus neuron models, synapse computation

**Cognitive Modules (20):**
- Ethics engine
- Knowledge management
- Curiosity/introspection
- Wellbeing monitoring
- Memory consolidation
- Emotional tagging
- Executive functions
- Theory of mind
- Meta-learning
- Predictive processing

**Plasticity (9):**
- STDP (spike-timing-dependent)
- BCM (Bienenstock-Cooper-Munro)
- STP (short-term)
- Attention mechanisms
- Pink noise
- Eligibility traces
- Neuromodulators

**Glial Support (5):**
- Astrocytes
- Oligodendrocytes
- Microglia
- Glial integration

**Networking (5):**
- P2P nodes
- Protocol handling
- Event systems
- Replication
- Distributed cognition

**I/O & Serialization (5):**
- Data I/O
- Streaming
- Serialization
- Network serialization
- Encryption (with libsodium)

**Utilities (30+):**
- Memory management
- Thread management
- Thread pools
- Platform abstraction (mutex, thread, condition, rwlock, once, time)
- Logging
- Signal handling
- Error codes
- Dynamic configuration
- Containers (vector, hash table, btree, queue, graph, heap)
- JSON parsing
- Metrics
- Caching
- FFT/spectral analysis

**GPU Support (3):**
- `nimcp_execution_mode.c` - CPU/GPU selection
- `nimcp_spike_event.c` - Event handling
- `nimcp_gpu_neuron.c` - GPU neuron API
- `nimcp_gpu_kernels.cu` - CUDA kernels (compiled only if CUDA available)
- `nimcp_synapse_compute_gpu.cu` - GPU synapse computation
- `nimcp_neural_logic_kernels.cu` - GPU neural logic gates

**API & Python (3):**
- `nimcp.c` - Public API facade
- `nimcp_module.c` - Python module initialization
- `nimcp_types.c` - Python type definitions

---

## 5. Installation Structure

### 5.1 Install Rules (CMake)

```cmake
# Headers
install(DIRECTORY ${NIMCP_INCLUDE_DIR}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nimcp
        FILES_MATCHING PATTERN "*.h")

# Library
install(TARGETS nimcp
    EXPORT nimcp-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Pkg-config
install(FILES ${CMAKE_BINARY_DIR}/nimcp.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# CMake config files
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/NIMCPConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/NIMCPConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NIMCP)

# Export targets
install(EXPORT nimcp-targets
    FILE NIMCPTargets.cmake
    NAMESPACE NIMCP::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NIMCP)

# Examples (selected ones)
install(TARGETS event_demo brain_demo ethics_demo ... 
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples)

# Python module
install(TARGETS nimcp_python
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/python3/dist-packages)
```

### 5.2 Standard Install Directories

Using `GNUInstallDirs`:

| Component | Default Path | Override |
|-----------|--------------|----------|
| Libraries | `${PREFIX}/lib` | `CMAKE_INSTALL_LIBDIR` |
| Headers | `${PREFIX}/include` | `CMAKE_INSTALL_INCLUDEDIR` |
| Binaries | `${PREFIX}/bin` | `CMAKE_INSTALL_BINDIR` |
| Pkg-config | `${PREFIX}/lib/pkgconfig` | (derives from LIBDIR) |
| CMake configs | `${PREFIX}/lib/cmake/NIMCP` | (derives from LIBDIR) |

### 5.3 Typical Install Paths

**When installed with `-DCMAKE_INSTALL_PREFIX=/usr/local`:**

```
/usr/local/
├── lib/
│   ├── libnimcp.so.2.6.2          # Library
│   ├── libnimcp.so.2
│   ├── libnimcp.so
│   ├── pkgconfig/
│   │   └── nimcp.pc                # Pkg-config file
│   ├── cmake/NIMCP/
│   │   ├── NIMCPConfig.cmake       # CMake config
│   │   ├── NIMCPConfigVersion.cmake
│   │   └── NIMCPTargets.cmake      # Export targets
│   └── python3/dist-packages/
│       └── nimcp.so                # Python module
│
├── bin/
│   ├── examples/
│   │   ├── brain_demo
│   │   ├── ethics_demo
│   │   └── ... (selected examples)
│
└── include/
    └── nimcp/                       # All .h files (100+)
        ├── api/
        ├── core/
        ├── cognitive/
        └── ...
```

### 5.4 Pkg-config File

**File:** `/home/bbrelin/nimcp/nimcp.pc` (generated)

```ini
prefix=/usr/local
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: NIMCP
Description: Neural Inference for Massive Concurrent Processing with Golden Rule Ethics
Version: 2.6.2
Requires:
Libs: -L${libdir} -lnimcp_core
Cflags: -I${includedir}
```

**Usage:**
```bash
pkg-config --cflags --libs nimcp
# Output: -I/usr/local/include -L/usr/local/lib -lnimcp_core
```

### 5.5 CMake Config File

**Primary File:** `/home/bbrelin/nimcp/NIMCPConfig.cmake` (generated from template)

```cmake
@PACKAGE_INIT@

set(NIMCP_VERSION "2.6.2")

# Import targets
include("${CMAKE_CURRENT_LIST_DIR}/NIMCPTargets.cmake")

# Verify target existence
if(NOT TARGET NIMCP::core)
    message(FATAL_ERROR "Expected target NIMCP::core not found")
endif()

# Backward compatibility variables
set(NIMCP_LIBRARIES NIMCP::core)
set(NIMCP_INCLUDE_DIRS "@CMAKE_INSTALL_PREFIX@/include")

check_required_components(NIMCP)
```

**Usage in downstream projects:**
```cmake
find_package(NIMCP 2.6 REQUIRED)

add_executable(myapp myapp.c)
target_link_libraries(myapp NIMCP::core)
```

---

## 6. Platform Support

### 6.1 Operating System Support

| Platform | Status | Notes |
|----------|--------|-------|
| **Linux (x86_64)** | Primary | Ubuntu 20.04+ (primary testing), Debian 11+ |
| **Linux (arm64)** | Supported | ARM-based systems |
| **macOS** | Partial | CUDA disabled, basic support |
| **Windows** | Experimental | CUDA support, CMake generation needed |

### 6.2 GPU/CUDA Support

**Detection:**
```cmake
check_language(CUDA)
find_package(CUDAToolkit QUIET)
```

**Architectures:**
```
Compute Capability 7.5+ (RTX 2080, 3080, 4080, A100, H100)
75 - Turing (RTX 20/30 series)
80 - Ampere (RTX 40 series, A100)
86 - Ada variant (L40)
89 - Ada Lovelace (H100)
```

**Conditional Compilation:**
```cmake
# Only compile CUDA kernels if CUDAToolkit_FOUND
$<$<BOOL:${CUDAToolkit_FOUND}>:${CUDA_KERNEL_FILES}>
```

**Compiler Settings for CUDA:**
```
CUDA_SEPARABLE_COMPILATION ON
CUDA_STANDARD 14
CUDA_ARCHITECTURES "75;80;86;89"
```

**NVCC Compatibility:**
- Cannot use some GCC flags
- Per-file compile options: `--expt-relaxed-constexpr`
- Incompatible flags filtered out for CUDA sources

### 6.3 Compiler Support

**C Compiler:**
- GCC 9.0+
- Clang 10.0+
- C11 standard required

**C++ Compiler:**
- GCC 9.0+
- Clang 10.0+
- C++17 standard required

**Compiler Warnings:**
```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
```

### 6.4 Python Version Support

Supported versions: **3.7, 3.8, 3.9, 3.10, 3.11**

**Note:** Tests use Python 3.10 specifically (libpython3.10-dev)

---

## 7. Build Configuration Options

### 7.1 Build Type Options

```bash
-DCMAKE_BUILD_TYPE=Debug       # Default: full debug info
-DCMAKE_BUILD_TYPE=Release     # Optimized, no debug
-DCMAKE_BUILD_TYPE=RelWithDebInfo  # Optimized + debug
-DCMAKE_BUILD_TYPE=MinSizeRel  # Minimal size
```

### 7.2 Security & Sanitizer Options

| Option | Default | Purpose |
|--------|---------|---------|
| `ENABLE_ASAN` | OFF | AddressSanitizer (memory errors) |
| `ENABLE_UBSAN` | OFF | UndefinedBehaviorSanitizer |
| `ENABLE_TSAN` | OFF | ThreadSanitizer (race conditions) |
| `ENABLE_HARDENING` | ON | Security hardening flags |
| `ENABLE_FUZZING` | OFF | libFuzzer targets |
| `ENABLE_COVERAGE` | OFF | Code coverage (gcov/lcov) |

**Note:** ASAN and TSAN are **mutually exclusive**

### 7.3 Hardening Flags (When Enabled)

**Compile-time:**
- `-D_FORTIFY_SOURCE=2` - Buffer overflow detection
- `-fstack-protector-strong` - Stack canary
- `-Wformat -Wformat-security` - Format string checks
- `-fno-common` - Prevent symbol vulnerabilities
- `-fPIC` / `-fPIE` - Position independent code

**Link-time (Linux only, Release build):**
- `-Wl,-z,relro` - Read-only relocations
- `-Wl,-z,now` - Immediate binding (full RELRO)
- `-Wl,-z,noexecstack` - Non-executable stack

**Status:**
```
- FULL hardening: Release builds
- PARTIAL hardening: Debug builds
- Can be disabled: -DENABLE_HARDENING=OFF
```

### 7.4 Coverage Configuration

**When enabled:** `-DENABLE_COVERAGE=ON`

```bash
# Compile with coverage instrumentation
--coverage -fprofile-arcs -ftest-coverage

# Link with coverage support
--coverage
```

**Requires:** GCC compiler

**Generate reports:**
```bash
# Capture coverage data
lcov --capture --directory . --output-file coverage.info

# Generate HTML report
genhtml coverage.info --output-directory coverage_html
```

### 7.5 Example Build Commands

```bash
# Release build with hardening (default)
cmake .. -DCMAKE_BUILD_TYPE=Release

# Debug build with AddressSanitizer
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON

# Release with thread safety checking
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_TSAN=ON

# Coverage analysis
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
make
ctest
lcov --capture --directory . --output-file coverage.info

# Fuzzing targets
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_FUZZING=ON

# GPU support (CUDA auto-detected)
cmake .. -DCMAKE_BUILD_TYPE=Release
# CUDA support will be enabled if toolkit is found
```

---

## 8. Testing Framework

### 8.1 Test Framework Architecture

**Location:** `/home/bbrelin/nimcp/test/`

**Test Runner:** CTest (CMake's testing facility)

**Test Framework:** Google Test v1.14.0

**Structure:**
```
test/
├── CMakeLists.txt              # Master test configuration
├── unit/                       # Unit tests
│   ├── test_*.cpp             # Individual test files
│   └── ...
├── integration/                # Cross-module tests
│   ├── test_*.cpp
│   └── ...
├── e2e/                        # End-to-end tests
├── regression/                 # Bug reproduction
└── fuzz/                        # Property-based fuzzing
```

### 8.2 Test Discovery

**Automatic discovery mechanism:**

```cmake
function(discover_category_tests CATEGORY)
    file(GLOB TEST_FILES "${CATEGORY}/*.cpp")
    foreach(TEST_FILE ${TEST_FILES})
        # Create: ${CATEGORY}_${TEST_NAME} target
        add_test_binary(${TARGET_NAME} ${TEST_FILE} ${CATEGORY})
    endforeach()
endfunction()

# Discover all categories
discover_category_tests(unit)
discover_category_tests(integration)
discover_category_tests(e2e)
discover_category_tests(regression)
discover_category_tests(fuzz)
```

### 8.3 Running Tests

```bash
# All tests (parallel)
ctest -j$(nproc)

# By category
ctest -L unit -j$(nproc)           # Unit tests
ctest -L integration -j$(nproc)    # Integration
ctest -L e2e                       # E2E only
ctest -L regression                # Regression

# Verbose output
ctest -V

# Specific test
ctest -R unit_test_brain -V
```

### 8.4 Code Surgeon Integration

Tests are integrated with Code Surgeon for automated testing:

```bash
./tools/code_surgeon/code_surgeon.py --mode test-only
./tools/code_surgeon/code_surgeon.py --mode full
```

### 8.5 Test Configuration

**Common includes:**
```cmake
${NIMCP_INCLUDE_DIR}
${NIMCP_TEST_DIR}
${NIMCP_TEST_DIR}/utils
${NIMCP_LIB_DIR}
${GTEST_INCLUDE_DIRS}
${Python3_INCLUDE_DIRS}
```

**Common libraries:**
```cmake
nimcp
GTest::GTest
GTest::Main
Python3::Python
pthread
```

**Environment for tests:**
```
PYTHONPATH=${CMAKE_BINARY_DIR}/lib/python
```

**Test-specific definitions:**
```
NIMCP_TESTING=1  # Enable test-only APIs
```

---

## 9. Deployment

### 9.1 Docker Support

**File:** `/home/bbrelin/nimcp/Dockerfile`

**Multi-stage build:**

**Stage 1: Builder**
- Base: Ubuntu 22.04
- Installs all build dependencies
- Builds NIMCP from source
- Runs tests to verify build
- Artifacts: compiled binaries

**Stage 2: Runtime**
- Base: Ubuntu 22.04 (minimal)
- Only runtime dependencies
- Non-root user: `nimcp` (uid 1000)
- Binaries from builder
- Python modules from builder
- Example configs included

**Exposed ports:**
- 8080 - API server
- 9090 - Monitoring

**Health check:**
- Runs `/usr/local/bin/healthcheck.sh`
- Interval: 30s
- Timeout: 3s
- Retries: 3

**Default command:**
```bash
/opt/nimcp/bin/integrated_demo
```

### 9.2 Installation Script

**File:** `/home/bbrelin/nimcp/install.sh`

**Supported OS:**
- Ubuntu/Debian
- CentOS/RHEL/Fedora
- Amazon Linux 2

**Steps:**
1. Detect OS
2. Install system dependencies
3. Install Node.js (for web demo)
4. Build NIMCP core library
5. Install Python bindings
6. Install web demo dependencies
7. Create environment setup script
8. Create startup scripts
9. Create systemd service files
10. Create deployment README

**Outputs:**
```
setup_env.sh                          # Environment configuration
src/bindings/web-demo/start_demo.sh  # Combined startup
src/bindings/web-demo/start_backend.sh
src/bindings/web-demo/start_frontend.sh
systemd/nimcp-backend.service        # Production services
systemd/nimcp-frontend.service
DEPLOYMENT.md                         # Deployment guide
```

### 9.3 Deployment Methods

#### Method 1: Manual Installation
```bash
chmod +x install.sh
./install.sh
source setup_env.sh
./src/bindings/web-demo/start_demo.sh
```

#### Method 2: Production with Systemd
```bash
sudo cp systemd/*.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable nimcp-backend nimcp-frontend
sudo systemctl start nimcp-backend nimcp-frontend
```

#### Method 3: Docker
```bash
docker build -t nimcp:2.6.2 .
docker run -p 8080:8080 -p 9090:9090 nimcp:2.6.2
```

#### Method 4: Production with Nginx
- React build for frontend
- Nginx reverse proxy
- Flask backend on port 5000
- HTTPS with Let's Encrypt

### 9.4 Environment Variables

**For runtime:**
```bash
LD_LIBRARY_PATH=/opt/nimcp/bin:$LD_LIBRARY_PATH
PYTHONPATH=/opt/nimcp/lib/python:$PYTHONPATH
NIMCP_HOME=/opt/nimcp
NIMCP_DATA=/var/lib/nimcp
NIMCP_LOG=/var/log/nimcp
```

**For Flask backend:**
```bash
FLASK_ENV=production
LD_LIBRARY_PATH=<path-to-lib>
```

**For React frontend:**
```bash
REACT_APP_API_URL=http://localhost:5000
HOST=localhost
```

---

## 10. Developer Workflows

### 10.1 Local Development Build

```bash
# Clone and navigate
git clone <repo>
cd nimcp

# Configure (debug mode recommended)
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build
make -j$(nproc)

# Set up environment
cd ..
source setup_env.sh

# Run tests
ctest -j$(nproc)

# Try an example
./build/bin/simple_demo
```

### 10.2 IDE Integration

**CMake with IDEs:**
```bash
# VS Code, CLion, Qt Creator support CMake natively
# Open CMakeLists.txt as project file

# Or use CMake presets (if available)
cmake --list-presets
cmake --preset=<preset-name>
```

### 10.3 Common Build Tasks

**Clean build:**
```bash
cd build
make clean
make -j$(nproc)
```

**Rebuild specific target:**
```bash
make nimcp              # Rebuild core library
make simple_demo        # Rebuild example
make unit_test_brain    # Rebuild specific test
```

**Static analysis:**
```bash
# cppcheck
cppcheck --project=build/compile_commands.json src/

# clang-tidy
clang-tidy -p build src/**/*.c src/**/*.cpp
```

**Memory checking:**
```bash
# AddressSanitizer
cmake .. -DENABLE_ASAN=ON
make
./build/bin/<test>

# Valgrind
valgrind --leak-check=full ./build/bin/<test>
```

### 10.4 Release Build Process

```bash
# Configure release
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_INSTALL_PREFIX=/usr/local

# Build
make -j$(nproc)

# Test
ctest

# Install
sudo make install

# Verify installation
pkg-config --cflags --libs nimcp
python3 -c "import nimcp; print(nimcp.__version__)"
```

### 10.5 Contributing Build Guidelines

**Before submitting PR:**
1. Build with all sanitizers: `-DENABLE_ASAN=ON`, `-DENABLE_UBSAN=ON`
2. Run full test suite: `ctest`
3. Check code coverage: `-DENABLE_COVERAGE=ON`
4. Run static analysis: cppcheck, clang-tidy
5. Test on multiple platforms if possible

---

## Summary: Build System at a Glance

| Aspect | Details |
|--------|---------|
| **Build System** | CMake 3.10+ |
| **Languages** | C (C11), C++ (C++17) |
| **Core Library** | `libnimcp.so.2.6.2` |
| **Python Binding** | `nimcp.so` (Python module) |
| **Test Framework** | Google Test v1.14.0 |
| **GPU Support** | CUDA 11.0+ (optional) |
| **Security** | Hardening flags, sanitizers |
| **Installation** | Standard FHS paths via CMake |
| **Deployment** | Docker, Systemd, Nginx |
| **Platforms** | Linux (primary), macOS (partial), Windows (experimental) |
| **Package Config** | pkg-config, CMake find modules |

---

## Configuration Summary Message

When running CMake, you'll see:

```
===============================================================================
NIMCP 2.6.2 - Configuration Summary
===============================================================================
Build Configuration:
  Build type: Release
  C Compiler: GCC 11
  C++ Compiler: GCC 11

Optional Features:
  CUDA Support: TRUE        (if CUDA Toolkit found)
  Encryption Support: TRUE  (if libsodium found)

Security Features:
  AddressSanitizer (ASAN): FALSE
  ThreadSanitizer (TSAN): FALSE
  Security Hardening: ON

Installation:
  Install prefix: /usr/local
  Library: /usr/local/lib/libnimcp.so
  Headers: /usr/local/include/nimcp/
  Pkg-config: /usr/local/lib/pkgconfig/nimcp.pc
===============================================================================
```

---

*Last Updated: November 11, 2025*  
*NIMCP Version: 2.6.2*
