//=============================================================================
// benchmark_complex_vs_real_oscillations.c
//=============================================================================
/**
 * @file benchmark_complex_vs_real_oscillations.c
 * @brief Performance benchmarks: Complex vs Real-valued oscillation detection
 *
 * OBJECTIVE: Verify 2-5x speedup targets for complex phasor operations
 *
 * BENCHMARKS:
 * 1. Phasor operations: target <10ns per operation
 * 2. Phase difference: target <10ns
 * 3. Array coherence (N=1000): target <1µs
 * 4. Array synchrony (N=1000): target <1.5µs
 * 5. Comparison vs baseline real-valued implementation
 *
 * TARGET SPEEDUP: 2-5x for oscillation/PAC detection operations
 */

#include "utils/math/nimcp_complex_math.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>

//=============================================================================
// High-Resolution Timer
//=============================================================================

typedef struct {
    struct timespec start;
    struct timespec end;
} benchmark_timer_t;

static inline void timer_start(benchmark_timer_t* timer) {
    clock_gettime(CLOCK_MONOTONIC, &timer->start);
}

static inline double timer_end(benchmark_timer_t* timer) {
    clock_gettime(CLOCK_MONOTONIC, &timer->end);
    double start_ns = timer->start.tv_sec * 1e9 + timer->start.tv_nsec;
    double end_ns = timer->end.tv_sec * 1e9 + timer->end.tv_nsec;
    return end_ns - start_ns;
}

//=============================================================================
// Statistics
//=============================================================================

typedef struct {
    double mean;
    double min;
    double max;
    double stddev;
    uint32_t iterations;
} benchmark_stats_t;

void compute_stats(double* times, uint32_t n, benchmark_stats_t* stats) {
    stats->iterations = n;

    // Mean
    double sum = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        sum += times[i];
    }
    stats->mean = sum / n;

    // Min/Max
    stats->min = times[0];
    stats->max = times[0];
    for (uint32_t i = 1; i < n; i++) {
        if (times[i] < stats->min) stats->min = times[i];
        if (times[i] > stats->max) stats->max = times[i];
    }

    // Standard deviation
    double var_sum = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        double diff = times[i] - stats->mean;
        var_sum += diff * diff;
    }
    stats->stddev = sqrt(var_sum / n);
}

void print_stats(const char* name, benchmark_stats_t* stats, const char* unit, double target) {
    printf("\n%s:\n", name);
    printf("  Mean:   %.3f %s\n", stats->mean, unit);
    printf("  Min:    %.3f %s\n", stats->min, unit);
    printf("  Max:    %.3f %s\n", stats->max, unit);
    printf("  StdDev: %.3f %s\n", stats->stddev, unit);
    printf("  Iters:  %u\n", stats->iterations);

    if (target > 0.0) {
        printf("  Target: %.3f %s\n", target, unit);
        if (stats->mean <= target) {
            printf("  Status: PASS (%.1fx margin)\n", target / stats->mean);
        } else {
            printf("  Status: FAIL (%.1fx over target)\n", stats->mean / target);
        }
    }
}

//=============================================================================
// Baseline Real-Valued Implementations
//=============================================================================

// Baseline: Real-valued phase difference using atan2
static inline double baseline_phase_difference(float real1, float imag1,
                                                float real2, float imag2) {
    float phase1 = atan2f(imag1, real1);
    float phase2 = atan2f(imag2, real2);
    float diff = phase2 - phase1;

    // Wrap to [-π, π]
    while (diff > M_PI) diff -= 2.0f * M_PI;
    while (diff < -M_PI) diff += 2.0f * M_PI;

    return diff;
}

// Baseline: Real-valued coherence calculation
float baseline_array_coherence(const float* real_parts, const float* imag_parts, uint32_t n) {
    float sum_real = 0.0f;
    float sum_imag = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        // Normalize each phasor
        float mag = sqrtf(real_parts[i] * real_parts[i] + imag_parts[i] * imag_parts[i]);
        if (mag > 1e-10f) {
            sum_real += real_parts[i] / mag;
            sum_imag += imag_parts[i] / mag;
        }
    }

    float mean_real = sum_real / (float)n;
    float mean_imag = sum_imag / (float)n;

    return sqrtf(mean_real * mean_real + mean_imag * mean_imag);
}

// Baseline: Real-valued synchrony calculation
float baseline_array_synchrony(const float* real1, const float* imag1,
                               const float* real2, const float* imag2,
                               uint32_t n) {
    float sum_real = 0.0f;
    float sum_imag = 0.0f;

    for (uint32_t i = 0; i < n; i++) {
        // Normalize both phasors
        float mag1 = sqrtf(real1[i] * real1[i] + imag1[i] * imag1[i]);
        float mag2 = sqrtf(real2[i] * real2[i] + imag2[i] * imag2[i]);

        if (mag1 > 1e-10f && mag2 > 1e-10f) {
            float norm_real1 = real1[i] / mag1;
            float norm_imag1 = imag1[i] / mag1;
            float norm_real2 = real2[i] / mag2;
            float norm_imag2 = imag2[i] / mag2;

            // Complex multiply: (a+bi) * (c-di) = (ac+bd) + (bc-ad)i
            float diff_real = norm_real1 * norm_real2 + norm_imag1 * norm_imag2;
            float diff_imag = norm_imag1 * norm_real2 - norm_real1 * norm_imag2;

            sum_real += diff_real;
            sum_imag += diff_imag;
        }
    }

    float mean_real = sum_real / (float)n;
    float mean_imag = sum_imag / (float)n;

    return sqrtf(mean_real * mean_real + mean_imag * mean_imag);
}

//=============================================================================
// Benchmark 1: Phasor Creation (Target: <10ns)
//=============================================================================

void benchmark_phasor_creation(void) {
    printf("\n========================================\n");
    printf("BENCHMARK 1: Phasor Creation\n");
    printf("========================================\n");

    const uint32_t iterations = 10000000;  // 10M iterations for nano-second precision
    double* times = malloc(iterations * sizeof(double));
    benchmark_timer_t timer;

    // Test: phasor_from_polar
    printf("\n--- phasor_from_polar ---\n");
    for (uint32_t i = 0; i < iterations; i++) {
        float amplitude = 1.0f + (i % 100) / 100.0f;
        float phase = (i % 360) * M_PI / 180.0f;

        timer_start(&timer);
        neural_phasor_t z = phasor_from_polar(amplitude, phase);
        times[i] = timer_end(&timer);

        // Prevent optimization
        volatile float dummy = z.real + z.imag;
        (void)dummy;
    }

    benchmark_stats_t stats;
    compute_stats(times, iterations, &stats);
    print_stats("phasor_from_polar", &stats, "ns", 10.0);

    // Test: phasor_from_cartesian
    printf("\n--- phasor_from_cartesian ---\n");
    for (uint32_t i = 0; i < iterations; i++) {
        float real = cosf((i % 360) * M_PI / 180.0f);
        float imag = sinf((i % 360) * M_PI / 180.0f);

        timer_start(&timer);
        neural_phasor_t z = phasor_from_cartesian(real, imag);
        times[i] = timer_end(&timer);

        volatile float dummy = z.real + z.imag;
        (void)dummy;
    }

    compute_stats(times, iterations, &stats);
    print_stats("phasor_from_cartesian", &stats, "ns", 5.0);

    free(times);
}

//=============================================================================
// Benchmark 2: Phase Difference (Target: <10ns)
//=============================================================================

void benchmark_phase_difference(void) {
    printf("\n========================================\n");
    printf("BENCHMARK 2: Phase Difference\n");
    printf("========================================\n");

    const uint32_t iterations = 10000000;
    double* times_complex = malloc(iterations * sizeof(double));
    double* times_baseline = malloc(iterations * sizeof(double));
    benchmark_timer_t timer;

    // Prepare test data
    neural_phasor_t* phasors1 = malloc(iterations * sizeof(neural_phasor_t));
    neural_phasor_t* phasors2 = malloc(iterations * sizeof(neural_phasor_t));

    for (uint32_t i = 0; i < iterations; i++) {
        float phase1 = (i % 360) * M_PI / 180.0f;
        float phase2 = ((i + 45) % 360) * M_PI / 180.0f;
        phasors1[i] = phasor_from_polar(1.0f, phase1);
        phasors2[i] = phasor_from_polar(1.0f, phase2);
    }

    // Test: Complex implementation
    printf("\n--- Complex phasor_phase_difference ---\n");
    for (uint32_t i = 0; i < iterations; i++) {
        timer_start(&timer);
        float diff = phasor_phase_difference(phasors1[i], phasors2[i]);
        times_complex[i] = timer_end(&timer);

        volatile float dummy = diff;
        (void)dummy;
    }

    benchmark_stats_t stats_complex;
    compute_stats(times_complex, iterations, &stats_complex);
    print_stats("Complex phase_difference", &stats_complex, "ns", 10.0);

    // Test: Baseline implementation
    printf("\n--- Baseline (real/imag + atan2) ---\n");
    for (uint32_t i = 0; i < iterations; i++) {
        timer_start(&timer);
        float diff = baseline_phase_difference(phasors1[i].real, phasors1[i].imag,
                                               phasors2[i].real, phasors2[i].imag);
        times_baseline[i] = timer_end(&timer);

        volatile float dummy = diff;
        (void)dummy;
    }

    benchmark_stats_t stats_baseline;
    compute_stats(times_baseline, iterations, &stats_baseline);
    print_stats("Baseline phase_difference", &stats_baseline, "ns", 0.0);

    // Speedup
    double speedup = stats_baseline.mean / stats_complex.mean;
    printf("\nSpeedup: %.2fx\n", speedup);

    free(phasors1);
    free(phasors2);
    free(times_complex);
    free(times_baseline);
}

//=============================================================================
// Benchmark 3: Array Coherence (Target: <1µs for N=1000)
//=============================================================================

void benchmark_array_coherence(void) {
    printf("\n========================================\n");
    printf("BENCHMARK 3: Array Coherence (N=1000)\n");
    printf("========================================\n");

    const uint32_t n = 1000;
    const uint32_t iterations = 10000;
    double* times_complex = malloc(iterations * sizeof(double));
    double* times_baseline = malloc(iterations * sizeof(double));
    benchmark_timer_t timer;

    // Prepare test data
    neural_phasor_t* phasors = malloc(n * sizeof(neural_phasor_t));
    float* real_parts = malloc(n * sizeof(float));
    float* imag_parts = malloc(n * sizeof(float));

    for (uint32_t i = 0; i < n; i++) {
        float phase = (i % 360) * M_PI / 180.0f;
        float amplitude = 0.8f + 0.4f * (i % 10) / 10.0f;
        phasors[i] = phasor_from_polar(amplitude, phase);
        real_parts[i] = phasors[i].real;
        imag_parts[i] = phasors[i].imag;
    }

    // Test: Complex implementation
    printf("\n--- Complex phasor_array_coherence ---\n");
    for (uint32_t i = 0; i < iterations; i++) {
        timer_start(&timer);
        float coherence = phasor_array_coherence(phasors, n);
        times_complex[i] = timer_end(&timer);

        volatile float dummy = coherence;
        (void)dummy;
    }

    benchmark_stats_t stats_complex;
    compute_stats(times_complex, iterations, &stats_complex);
    stats_complex.mean /= 1000.0;  // Convert to µs
    stats_complex.min /= 1000.0;
    stats_complex.max /= 1000.0;
    stats_complex.stddev /= 1000.0;
    print_stats("Complex array_coherence", &stats_complex, "µs", 1.0);

    // Test: Baseline implementation
    printf("\n--- Baseline (separate arrays) ---\n");
    for (uint32_t i = 0; i < iterations; i++) {
        timer_start(&timer);
        float coherence = baseline_array_coherence(real_parts, imag_parts, n);
        times_baseline[i] = timer_end(&timer);

        volatile float dummy = coherence;
        (void)dummy;
    }

    benchmark_stats_t stats_baseline;
    compute_stats(times_baseline, iterations, &stats_baseline);
    stats_baseline.mean /= 1000.0;
    stats_baseline.min /= 1000.0;
    stats_baseline.max /= 1000.0;
    stats_baseline.stddev /= 1000.0;
    print_stats("Baseline array_coherence", &stats_baseline, "µs", 0.0);

    // Speedup
    double speedup = stats_baseline.mean / stats_complex.mean;
    printf("\nSpeedup: %.2fx\n", speedup);

    free(phasors);
    free(real_parts);
    free(imag_parts);
    free(times_complex);
    free(times_baseline);
}

//=============================================================================
// Benchmark 4: Array Synchrony (Target: <1.5µs for N=1000)
//=============================================================================

void benchmark_array_synchrony(void) {
    printf("\n========================================\n");
    printf("BENCHMARK 4: Array Synchrony (N=1000)\n");
    printf("========================================\n");

    const uint32_t n = 1000;
    const uint32_t iterations = 10000;
    double* times_complex = malloc(iterations * sizeof(double));
    double* times_baseline = malloc(iterations * sizeof(double));
    benchmark_timer_t timer;

    // Prepare test data
    neural_phasor_t* phasors1 = malloc(n * sizeof(neural_phasor_t));
    neural_phasor_t* phasors2 = malloc(n * sizeof(neural_phasor_t));
    float* real1 = malloc(n * sizeof(float));
    float* imag1 = malloc(n * sizeof(float));
    float* real2 = malloc(n * sizeof(float));
    float* imag2 = malloc(n * sizeof(float));

    for (uint32_t i = 0; i < n; i++) {
        float phase1 = (i % 360) * M_PI / 180.0f;
        float phase2 = ((i + 30) % 360) * M_PI / 180.0f;
        phasors1[i] = phasor_from_polar(1.0f, phase1);
        phasors2[i] = phasor_from_polar(1.0f, phase2);
        real1[i] = phasors1[i].real;
        imag1[i] = phasors1[i].imag;
        real2[i] = phasors2[i].real;
        imag2[i] = phasors2[i].imag;
    }

    // Test: Complex implementation
    printf("\n--- Complex phasor_array_synchrony ---\n");
    for (uint32_t i = 0; i < iterations; i++) {
        timer_start(&timer);
        float synchrony = phasor_array_synchrony(phasors1, phasors2, n);
        times_complex[i] = timer_end(&timer);

        volatile float dummy = synchrony;
        (void)dummy;
    }

    benchmark_stats_t stats_complex;
    compute_stats(times_complex, iterations, &stats_complex);
    stats_complex.mean /= 1000.0;  // Convert to µs
    stats_complex.min /= 1000.0;
    stats_complex.max /= 1000.0;
    stats_complex.stddev /= 1000.0;
    print_stats("Complex array_synchrony", &stats_complex, "µs", 1.5);

    // Test: Baseline implementation
    printf("\n--- Baseline (separate arrays) ---\n");
    for (uint32_t i = 0; i < iterations; i++) {
        timer_start(&timer);
        float synchrony = baseline_array_synchrony(real1, imag1, real2, imag2, n);
        times_baseline[i] = timer_end(&timer);

        volatile float dummy = synchrony;
        (void)dummy;
    }

    benchmark_stats_t stats_baseline;
    compute_stats(times_baseline, iterations, &stats_baseline);
    stats_baseline.mean /= 1000.0;
    stats_baseline.min /= 1000.0;
    stats_baseline.max /= 1000.0;
    stats_baseline.stddev /= 1000.0;
    print_stats("Baseline array_synchrony", &stats_baseline, "µs", 0.0);

    // Speedup
    double speedup = stats_baseline.mean / stats_complex.mean;
    printf("\nSpeedup: %.2fx\n", speedup);

    free(phasors1);
    free(phasors2);
    free(real1);
    free(imag1);
    free(real2);
    free(imag2);
    free(times_complex);
    free(times_baseline);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("Complex vs Real-Valued Oscillation Benchmarks\n");
    printf("========================================\n");
    printf("Target: 2-5x speedup for oscillation detection\n");
    printf("Platform: %s\n",
#ifdef __x86_64__
           "x86_64"
#elif __aarch64__
           "aarch64"
#else
           "unknown"
#endif
    );

    // Initialize complex math
    complex_math_init(NULL);

    // Run benchmarks
    benchmark_phasor_creation();
    benchmark_phase_difference();
    benchmark_array_coherence();
    benchmark_array_synchrony();

    printf("\n========================================\n");
    printf("Benchmark Complete\n");
    printf("========================================\n");

    complex_math_cleanup();
    return 0;
}
