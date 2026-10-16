# Tech Context

## Technologies Used
- **Language**: C (C11 standard)
- **Build System**: CMake
- **GPU**: CUDA for parallel acceleration
- **Testing**: CTest with custom test framework
- **Static Analysis**: clang-tidy, cppcheck
- **Fuzzing**: libFuzzer (build-fuzz directory)
- **Sanitizers**: ASan, LSan, TSan (build-asan directory)

## Hardware Support
| Hardware | Status | Notes |
|----------|--------|-------|
| CPU | ✓ Full | Baseline with SIMD vectorization |
| GPU (CUDA) | ✓ Full | Parallel neuron/synapse updates |
| Neuromorphic | Planned | Loihi, TrueNorth, SpiNNaker targets |
| FPGA | Planned | Custom neuron models |
| TPU | Planned | Accelerated plasticity |
| Quantum | ✓ Partial | Quantum statistics module |

## Development Setup
```bash
# Build directory
cd /home/bbrelin/nimcp/build
cmake ..
make nimcp -j4

# ASAN build (memory checking)
cd /home/bbrelin/nimcp/build-asan
cmake -DCMAKE_BUILD_TYPE=Debug -DSANITIZE_ADDRESS=ON ..
make -j4

# Fuzz testing
cd /home/bbrelin/nimcp/build-fuzz
cmake -DFUZZ_TESTING=ON ..
make -j4
```

## Key Dependencies
- CUDA Toolkit (GPU acceleration)
- pthreads (threading)
- OpenSSL (encryption/security)
- CMake 3.28+

## Project Scale
| Metric | Count |
|--------|-------|
| C source files | 2,202 |
| Header files | 2,369 |
| Lines of C code | ~451,000 |
| Test files | 1,000+ |

## Configuration Files
- `CMakeLists.txt` — Build configuration
- `.clang-format` — Code style
- `.clang-tidy` — Static analysis rules
- `.cppcheck` — Additional static analysis
- `valgrind.supp` — Valgrind suppressions
- `lsan.supp` — Leak sanitizer suppressions

## Documentation Structure
```
docs/
├── INDEX.md                    # Master navigation
├── EXTERNAL_API_GUIDE.md       # Public API reference
└── claude/                     # AI-readable docs
    ├── 00-overview.md          # Project vision
    ├── 01-build-test.md        # Build commands
    ├── 02-coding-standards.md  # Code standards
    ├── 03-api-patterns.md      # API conventions
    ├── 04-file-organization.md # Structure
    ├── 05-resource-optimization.md
    ├── 06-error-codes.md
    ├── 07-common-issues.md
    └── modules/                # Per-module docs
```

## Critical Constraints
- **Test protocol**: Must read headers before writing tests
- **Thread safety**: Use thread layer, not platform layer for mutexes
- **Memory**: Track all allocations; immune system monitors heap
- **Security**: BBB validation required for external inputs
