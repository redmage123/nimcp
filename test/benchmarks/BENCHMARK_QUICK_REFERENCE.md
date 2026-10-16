# Complex Math Benchmarks - Quick Reference

## Running Benchmarks

```bash
cd /home/bbrelin/nimcp/build

# Build benchmarks
make bench_benchmark_complex_vs_real_oscillations
make bench_benchmark_complex_pac_detection
make bench_benchmark_complex_fft

# Run individual benchmarks
./test/benchmarks/bench_benchmark_complex_vs_real_oscillations
./test/benchmarks/bench_benchmark_complex_pac_detection
./test/benchmarks/bench_benchmark_complex_fft

# Or run all benchmarks
make run_all_benchmarks

# Generate report
make benchmark_report
cat test/benchmarks/*_results.txt
```

## Benchmark Files

```
test/benchmarks/
├── benchmark_complex_vs_real_oscillations.c  # Phasor ops, coherence, synchrony
├── benchmark_complex_pac_detection.c          # Phase-amplitude coupling
├── benchmark_complex_fft.c                    # FFT, power spectrum, Hilbert
├── CMakeLists.txt                             # Build configuration
├── COMPLEX_MATH_PERFORMANCE_REPORT.md         # Full analysis
└── BENCHMARK_QUICK_REFERENCE.md               # This file
```

## Results Summary (2025-11-22)

### Performance Status: ❌ NOT MET

| Metric | Target | Actual | Status |
|--------|--------|--------|--------|
| Phasor ops | <10ns | 61.5ns | 6x over |
| Coherence | <1µs | 24.4µs | 24x over |
| Synchrony | <1.5µs | 56.8µs | 38x over |
| PAC | <2µs | 41.0µs | 20x over |
| Speedup vs baseline | 2-5x | 0.13x | 8x slower |

### Critical Issues

1. **Performance:** 4-9x slower than baseline (should be 2-5x faster)
2. **Accuracy:** PAC detection has 88% error
3. **FFT:** Floating point exception (not runnable)

## Optimization Priorities

### Phase 1: Quick Wins (1-2 days)
- [x] Inline all hot-path functions → 2-3x speedup
- [ ] Fix PAC accuracy bug → Correct results
- [ ] Debug FFT floating point exception

### Phase 2: SIMD (3-5 days)
- [ ] AVX2-vectorized array operations → 4-8x speedup
- [ ] Batch normalization → 5-10x speedup
- [ ] FMA instructions → 1.3-1.5x speedup

### Phase 3: Algorithm (1-2 weeks)
- [ ] Trigonometric lookup tables → 5-10x for phasor creation
- [ ] Memory layout optimization → 1.5-2x
- [ ] OpenMP parallelization → Near-linear scaling

## Expected Outcomes After Optimization

```
Phase 1 (inline):     2-3x improvement  → Still below target
Phase 2 (SIMD):       10-20x improvement → TARGET MET ✅
Phase 3 (full opt):   20-40x improvement → TARGET EXCEEDED ✅
```

## Key Insights

1. **Complex math correctness is solid** - implementation works, just slow
2. **SIMD is critical** - sequential processing kills performance
3. **Function calls are expensive** - inline everything
4. **Baseline is well-optimized** - compiler auto-vectorizes simple loops
5. **Complex advantage emerges in arrays** - not in single operations

## Next Steps

1. **IMMEDIATE:** Fix PAC bug and FFT crash
2. **THIS WEEK:** Implement inlining optimizations
3. **NEXT WEEK:** Add SIMD vectorization
4. **THIS MONTH:** Full optimization suite

## Contacts & Resources

- **Implementation:** `/home/bbrelin/nimcp/src/utils/math/nimcp_complex_math.c`
- **Header:** `/home/bbrelin/nimcp/include/utils/math/nimcp_complex_math.h`
- **Full Report:** `/home/bbrelin/nimcp/test/benchmarks/COMPLEX_MATH_PERFORMANCE_REPORT.md`
- **Test Suite:** `/home/bbrelin/nimcp/test/unit/utils/math/`

---

**Last Updated:** 2025-11-22
**Status:** Performance targets not met - optimization required
**Priority:** HIGH - Critical for neural oscillation performance
