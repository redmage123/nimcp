# NIMCP Complex Math Performance Benchmarks

This directory contains comprehensive performance benchmarks for the complex number mathematics implementation used in neural oscillation detection and phase-amplitude coupling analysis.

## Overview

The NIMCP complex math library provides:
- **Phasor operations** - Complex number representation of neural oscillations
- **Phase analysis** - Phase difference, coherence, synchrony
- **PAC detection** - Phase-amplitude coupling for cross-frequency analysis
- **FFT operations** - Spectral analysis and Hilbert transforms

**Target:** 2-5x speedup over baseline real-valued implementations

## Benchmark Suite

### 1. `benchmark_complex_vs_real_oscillations.c`

Tests core phasor operations and array-based oscillation metrics.

**Benchmarks:**
- Phasor creation (polar/cartesian) - Target: <10ns
- Phase difference computation - Target: <10ns
- Array coherence (N=1000) - Target: <1µs
- Array synchrony (N=1000) - Target: <1.5µs

**Comparison:** Complex implementation vs baseline real-valued

### 2. `benchmark_complex_pac_detection.c`

Tests phase-amplitude coupling detection across various conditions.

**Benchmarks:**
- PAC modulation index (N=1000) - Target: <2µs
- Coupling strength sensitivity
- Noise robustness analysis
- Signal length scaling

**Comparison:** Accuracy and performance vs baseline

### 3. `benchmark_complex_fft.c`

Tests FFT-based spectral analysis operations.

**Benchmarks:**
- Forward/inverse FFT (N=1024) - Target: <50µs
- Power spectrum (N=1024) - Target: <60µs
- Hilbert transform (N=1024) - Target: <80µs
- Scalability testing (N=64 to 4096)
- FFTW comparison (if available)

## Building & Running

### Prerequisites

```bash
cmake >= 3.18
gcc with -O3 support
Optional: FFTW3 library (for comparison benchmarks)
```

### Build

```bash
cd /home/bbrelin/nimcp/build

# Configure (done once)
cmake ..

# Build all benchmarks
make bench_benchmark_complex_vs_real_oscillations
make bench_benchmark_complex_pac_detection
make bench_benchmark_complex_fft

# Or build all at once
make -j4
```

### Run

```bash
# Run individual benchmarks
./test/benchmarks/bench_benchmark_complex_vs_real_oscillations
./test/benchmarks/bench_benchmark_complex_pac_detection
./test/benchmarks/bench_benchmark_complex_fft

# Run all benchmarks (custom target)
make run_all_benchmarks

# Generate report files
make benchmark_report
cat test/benchmarks/*_results.txt
```

### Output

Each benchmark produces:
- **Console output** - Detailed performance statistics
- **Results file** - `*_results.txt` (when using `make benchmark_report`)
- **Statistics** - Mean, Min, Max, StdDev for each metric

## Current Results (2025-11-22)

### Status: ❌ TARGETS NOT MET

| Benchmark | Target | Actual | Gap | Status |
|-----------|--------|--------|-----|--------|
| Phasor ops | <10ns | 61ns | 6x | ❌ |
| Phase diff | <10ns | 89ns | 9x | ❌ |
| Coherence | <1µs | 24µs | 24x | ❌ |
| Synchrony | <1.5µs | 57µs | 38x | ❌ |
| PAC | <2µs | 41µs | 20x | ❌ |
| **Speedup** | **2-5x** | **0.13x** | **8x slower** | **❌** |

### Critical Issues

1. **Performance Regression:** 4-9x slower than baseline (should be 2-5x faster)
2. **PAC Accuracy:** 88% error in modulation index detection
3. **FFT Crash:** Floating point exception (requires debugging)

See [COMPLEX_MATH_PERFORMANCE_REPORT.md](COMPLEX_MATH_PERFORMANCE_REPORT.md) for detailed analysis.

## Optimization Roadmap

### Phase 1: Foundation (1-2 days) ⏳

- [ ] Inline all hot-path functions (`static inline __attribute__((always_inline))`)
- [ ] Fix PAC accuracy bug
- [ ] Debug FFT floating point exception
- [ ] Add performance regression tests

**Expected:** 2-3x improvement (still below target)

### Phase 2: SIMD Vectorization (3-5 days)

- [ ] AVX2-optimized array operations (4-8 phasors per iteration)
- [ ] Batch normalization using reciprocal sqrt approximation
- [ ] FMA instructions for complex multiply-add
- [ ] SIMD-friendly memory layouts

**Expected:** 10-20x improvement - **TARGETS MET** ✅

### Phase 3: Advanced Optimization (1-2 weeks)

- [ ] Trigonometric lookup tables (phase quantization)
- [ ] Cache-aligned memory allocation
- [ ] OpenMP parallelization for large arrays
- [ ] Hardware-specific tuning (AVX-512, ARM NEON)

**Expected:** 20-40x improvement - **TARGETS EXCEEDED** ✅

## Benchmark Methodology

### Timer

- **Function:** `clock_gettime(CLOCK_MONOTONIC)`
- **Resolution:** Nanosecond
- **Warmup:** Each benchmark warms cache before measurement

### Statistics

For each benchmark:
- **Iterations:** 1,000 - 10,000,000 (varies by operation)
- **Mean:** Average time across all iterations
- **Min:** Best-case performance
- **Max:** Worst-case (including outliers)
- **StdDev:** Variability measure

### Comparison

All benchmarks compare:
1. **Complex implementation** - Current neural_phasor_t based code
2. **Baseline implementation** - Real-valued reference (separate real/imag arrays)
3. **Speedup factor** - Baseline / Complex (target: 2-5x)

## Adding New Benchmarks

### Template

```c
void benchmark_new_operation(void) {
    const uint32_t iterations = 10000;
    double* times = malloc(iterations * sizeof(double));
    benchmark_timer_t timer;

    // Setup test data
    // ...

    // Benchmark loop
    for (uint32_t i = 0; i < iterations; i++) {
        timer_start(&timer);
        // Operation to benchmark
        times[i] = timer_end(&timer);
    }

    // Compute and print statistics
    benchmark_stats_t stats;
    compute_stats(times, iterations, &stats);
    print_stats("Operation Name", &stats, "unit", target_value);

    free(times);
}
```

### Integration

Add to `CMakeLists.txt`:
```cmake
set(BENCHMARK_SOURCES
    ${BENCHMARK_SOURCES}
    benchmark_new_feature.c
)
```

## References

### Implementation
- **Source:** `/home/bbrelin/nimcp/src/utils/math/nimcp_complex_math.c`
- **Header:** `/home/bbrelin/nimcp/include/utils/math/nimcp_complex_math.h`
- **Tests:** `/home/bbrelin/nimcp/test/unit/utils/math/`

### Documentation
- **Performance Report:** [COMPLEX_MATH_PERFORMANCE_REPORT.md](COMPLEX_MATH_PERFORMANCE_REPORT.md)
- **Quick Reference:** [BENCHMARK_QUICK_REFERENCE.md](BENCHMARK_QUICK_REFERENCE.md)
- **Complex Math Design:** `/home/bbrelin/nimcp/include/utils/math/nimcp_complex_math.h` (header comments)

### Scientific Background
- Cohen, M. X. (2014). "Analyzing Neural Time Series Data"
- Tort et al. (2010). "Measuring Phase-Amplitude Coupling" - J. Neurophysiol.
- Agner Fog's Optimization Manuals: https://www.agner.org/optimize/

---

## Contact

**Team:** NIMCP Development Team
**Date:** 2025-11-22
**Status:** Performance optimization in progress
**Priority:** HIGH - Critical for neural oscillation performance
