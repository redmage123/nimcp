//=============================================================================
// benchmark_complex_pac_detection.c
//=============================================================================
/**
 * @file benchmark_complex_pac_detection.c
 * @brief Performance benchmarks: Complex-based PAC detection
 *
 * OBJECTIVE: Verify 2-5x speedup for phase-amplitude coupling detection
 *
 * BENCHMARKS:
 * 1. PAC modulation index (N=1000): target <2µs
 * 2. Accuracy comparison: complex vs baseline
 * 3. Speedup measurement across frequency bands
 * 4. Various signal characteristics (SNR, coupling strength)
 *
 * TARGET: 2-5x speedup over baseline real-valued implementation
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

    double sum = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        sum += times[i];
    }
    stats->mean = sum / n;

    stats->min = times[0];
    stats->max = times[0];
    for (uint32_t i = 1; i < n; i++) {
        if (times[i] < stats->min) stats->min = times[i];
        if (times[i] > stats->max) stats->max = times[i];
    }

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
// Baseline Real-Valued PAC Implementation
//=============================================================================

// Baseline PAC using histogram method
float baseline_pac_modulation_index(const float* theta_phase_angles,
                                    const float* gamma_amplitude,
                                    uint32_t n) {
    #define NUM_BINS 18
    float amplitude_by_phase[NUM_BINS] = {0};
    uint32_t counts[NUM_BINS] = {0};

    // Bin gamma amplitude by theta phase
    for (uint32_t i = 0; i < n; i++) {
        // BUG FIX: Wrap phase to [-π, π] using atan2 before normalization
        // This ensures consistency with the complex implementation which uses atan2
        float phase = atan2f(sinf(theta_phase_angles[i]), cosf(theta_phase_angles[i]));

        float phase_normalized = (phase + M_PI) / (2.0f * M_PI);
        uint32_t bin = (uint32_t)(phase_normalized * NUM_BINS);
        if (bin >= NUM_BINS) bin = NUM_BINS - 1;

        amplitude_by_phase[bin] += gamma_amplitude[i];
        counts[bin]++;
    }

    // Compute mean amplitude per bin
    float total_amplitude = 0.0f;
    for (uint32_t i = 0; i < NUM_BINS; i++) {
        if (counts[i] > 0) {
            amplitude_by_phase[i] /= counts[i];
            total_amplitude += amplitude_by_phase[i];
        }
    }

    if (total_amplitude < 1e-10f) {
        return 0.0f;
    }

    // Normalize to probability distribution
    for (uint32_t i = 0; i < NUM_BINS; i++) {
        amplitude_by_phase[i] /= total_amplitude;
    }

    // Compute Shannon entropy
    float entropy = 0.0f;
    for (uint32_t i = 0; i < NUM_BINS; i++) {
        if (amplitude_by_phase[i] > 1e-10f) {
            entropy -= amplitude_by_phase[i] * log2f(amplitude_by_phase[i]);
        }
    }

    // Modulation index
    float max_entropy = log2f((float)NUM_BINS);
    float modulation_index = (max_entropy - entropy) / max_entropy;

    #undef NUM_BINS
    return modulation_index;
}

//=============================================================================
// Signal Generation
//=============================================================================

typedef struct {
    neural_phasor_t* theta_phasors;
    float* theta_phase_angles;
    float* gamma_amplitude;
    uint32_t n;
    float true_coupling;  // Ground truth coupling strength
} pac_test_signal_t;

pac_test_signal_t* generate_pac_signal(uint32_t n, float coupling_strength, float noise_level) {
    pac_test_signal_t* signal = malloc(sizeof(pac_test_signal_t));
    signal->n = n;
    signal->true_coupling = coupling_strength;
    signal->theta_phasors = malloc(n * sizeof(neural_phasor_t));
    signal->theta_phase_angles = malloc(n * sizeof(float));
    signal->gamma_amplitude = malloc(n * sizeof(float));

    // Generate theta oscillation (6 Hz)
    float theta_freq = 6.0f;
    float sample_rate = 1000.0f;

    for (uint32_t i = 0; i < n; i++) {
        float t = (float)i / sample_rate;
        float theta_phase = 2.0f * M_PI * theta_freq * t;

        // Add some phase jitter
        theta_phase += noise_level * 0.5f * ((float)rand() / RAND_MAX - 0.5f);

        signal->theta_phase_angles[i] = theta_phase;
        signal->theta_phasors[i] = phasor_from_polar(1.0f, theta_phase);

        // Gamma amplitude modulated by theta phase
        // Peak amplitude at theta phase = 0 (trough of theta)
        float modulation = 1.0f + coupling_strength * cosf(theta_phase);
        float base_amplitude = 0.5f;
        float gamma_amp = base_amplitude * modulation;

        // Add noise
        gamma_amp += noise_level * ((float)rand() / RAND_MAX);

        signal->gamma_amplitude[i] = gamma_amp;
    }

    return signal;
}

void free_pac_signal(pac_test_signal_t* signal) {
    if (signal) {
        free(signal->theta_phasors);
        free(signal->theta_phase_angles);
        free(signal->gamma_amplitude);
        free(signal);
    }
}

//=============================================================================
// Benchmark 1: PAC Modulation Index (Target: <2µs for N=1000)
//=============================================================================

void benchmark_pac_modulation_index(void) {
    printf("\n========================================\n");
    printf("BENCHMARK 1: PAC Modulation Index (N=1000)\n");
    printf("========================================\n");

    const uint32_t n = 1000;
    const uint32_t iterations = 5000;
    double* times_complex = malloc(iterations * sizeof(double));
    double* times_baseline = malloc(iterations * sizeof(double));
    benchmark_timer_t timer;

    // Generate test signal with moderate coupling
    srand(42);  // Reproducible results
    pac_test_signal_t* signal = generate_pac_signal(n, 0.6f, 0.1f);

    printf("\nSignal characteristics:\n");
    printf("  N = %u samples\n", n);
    printf("  True coupling strength = %.2f\n", signal->true_coupling);

    // Test: Complex implementation
    printf("\n--- Complex phasor_pac_modulation_index ---\n");
    float complex_result = 0.0f;
    for (uint32_t i = 0; i < iterations; i++) {
        timer_start(&timer);
        complex_result = phasor_pac_modulation_index(signal->theta_phasors,
                                                     signal->gamma_amplitude,
                                                     n);
        times_complex[i] = timer_end(&timer);
    }

    benchmark_stats_t stats_complex;
    compute_stats(times_complex, iterations, &stats_complex);
    stats_complex.mean /= 1000.0;  // Convert to µs
    stats_complex.min /= 1000.0;
    stats_complex.max /= 1000.0;
    stats_complex.stddev /= 1000.0;
    print_stats("Complex PAC", &stats_complex, "µs", 2.0);
    printf("  Detected MI: %.4f\n", complex_result);

    // Test: Baseline implementation
    printf("\n--- Baseline PAC (phase angles) ---\n");
    float baseline_result = 0.0f;
    for (uint32_t i = 0; i < iterations; i++) {
        timer_start(&timer);
        baseline_result = baseline_pac_modulation_index(signal->theta_phase_angles,
                                                        signal->gamma_amplitude,
                                                        n);
        times_baseline[i] = timer_end(&timer);
    }

    benchmark_stats_t stats_baseline;
    compute_stats(times_baseline, iterations, &stats_baseline);
    stats_baseline.mean /= 1000.0;
    stats_baseline.min /= 1000.0;
    stats_baseline.max /= 1000.0;
    stats_baseline.stddev /= 1000.0;
    print_stats("Baseline PAC", &stats_baseline, "µs", 0.0);
    printf("  Detected MI: %.4f\n", baseline_result);

    // Accuracy comparison
    printf("\nAccuracy:\n");
    printf("  Complex MI:  %.4f\n", complex_result);
    printf("  Baseline MI: %.4f\n", baseline_result);
    printf("  Difference:  %.4f (%.1f%%)\n",
           fabs(complex_result - baseline_result),
           100.0f * fabs(complex_result - baseline_result) / baseline_result);

    // Speedup
    double speedup = stats_baseline.mean / stats_complex.mean;
    printf("\nSpeedup: %.2fx\n", speedup);
    if (speedup >= 2.0 && speedup <= 5.0) {
        printf("Target speedup (2-5x): ACHIEVED\n");
    } else if (speedup > 5.0) {
        printf("Target speedup (2-5x): EXCEEDED\n");
    } else {
        printf("Target speedup (2-5x): NOT MET\n");
    }

    free_pac_signal(signal);
    free(times_complex);
    free(times_baseline);
}

//=============================================================================
// Benchmark 2: PAC Across Coupling Strengths
//=============================================================================

void benchmark_pac_coupling_strength(void) {
    printf("\n========================================\n");
    printf("BENCHMARK 2: PAC Across Coupling Strengths\n");
    printf("========================================\n");

    const uint32_t n = 1000;
    const uint32_t iterations = 1000;
    float coupling_strengths[] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    uint32_t num_strengths = sizeof(coupling_strengths) / sizeof(float);

    printf("\nCoupling | Complex MI | Baseline MI | Speedup\n");
    printf("---------|------------|-------------|--------\n");

    for (uint32_t s = 0; s < num_strengths; s++) {
        float coupling = coupling_strengths[s];
        srand(42);
        pac_test_signal_t* signal = generate_pac_signal(n, coupling, 0.1f);

        // Measure complex implementation
        benchmark_timer_t timer;
        double complex_total = 0.0;
        float complex_mi = 0.0f;
        for (uint32_t i = 0; i < iterations; i++) {
            timer_start(&timer);
            complex_mi = phasor_pac_modulation_index(signal->theta_phasors,
                                                     signal->gamma_amplitude,
                                                     n);
            complex_total += timer_end(&timer);
        }
        double complex_mean = complex_total / iterations / 1000.0;  // µs

        // Measure baseline implementation
        double baseline_total = 0.0;
        float baseline_mi = 0.0f;
        for (uint32_t i = 0; i < iterations; i++) {
            timer_start(&timer);
            baseline_mi = baseline_pac_modulation_index(signal->theta_phase_angles,
                                                        signal->gamma_amplitude,
                                                        n);
            baseline_total += timer_end(&timer);
        }
        double baseline_mean = baseline_total / iterations / 1000.0;  // µs

        double speedup = baseline_mean / complex_mean;

        printf("  %.2f   |   %.4f    |   %.4f     | %.2fx\n",
               coupling, complex_mi, baseline_mi, speedup);

        free_pac_signal(signal);
    }
}

//=============================================================================
// Benchmark 3: PAC with Various Noise Levels
//=============================================================================

void benchmark_pac_noise_robustness(void) {
    printf("\n========================================\n");
    printf("BENCHMARK 3: PAC Noise Robustness\n");
    printf("========================================\n");

    const uint32_t n = 1000;
    const uint32_t iterations = 1000;
    float noise_levels[] = {0.0f, 0.1f, 0.2f, 0.3f, 0.5f};
    uint32_t num_noise = sizeof(noise_levels) / sizeof(float);

    printf("\nNoise  | Complex MI | Baseline MI | Accuracy Loss\n");
    printf("-------|------------|-------------|--------------\n");

    for (uint32_t s = 0; s < num_noise; s++) {
        float noise = noise_levels[s];
        srand(42);
        pac_test_signal_t* signal = generate_pac_signal(n, 0.6f, noise);

        // Measure complex implementation
        float complex_mi = 0.0f;
        for (uint32_t i = 0; i < iterations; i++) {
            complex_mi += phasor_pac_modulation_index(signal->theta_phasors,
                                                      signal->gamma_amplitude,
                                                      n);
        }
        complex_mi /= iterations;

        // Measure baseline implementation
        float baseline_mi = 0.0f;
        for (uint32_t i = 0; i < iterations; i++) {
            baseline_mi += baseline_pac_modulation_index(signal->theta_phase_angles,
                                                         signal->gamma_amplitude,
                                                         n);
        }
        baseline_mi /= iterations;

        float accuracy_diff = fabs(complex_mi - baseline_mi);

        printf(" %.2f   |   %.4f    |   %.4f     |   %.4f\n",
               noise, complex_mi, baseline_mi, accuracy_diff);

        free_pac_signal(signal);
    }
}

//=============================================================================
// Benchmark 4: PAC for Different Signal Lengths
//=============================================================================

void benchmark_pac_signal_length(void) {
    printf("\n========================================\n");
    printf("BENCHMARK 4: PAC Performance vs Signal Length\n");
    printf("========================================\n");

    uint32_t lengths[] = {100, 250, 500, 1000, 2000, 5000};
    uint32_t num_lengths = sizeof(lengths) / sizeof(uint32_t);
    const uint32_t iterations = 1000;

    printf("\n  N    | Complex (µs) | Baseline (µs) | Speedup\n");
    printf("-------|--------------|---------------|---------\n");

    for (uint32_t l = 0; l < num_lengths; l++) {
        uint32_t n = lengths[l];
        srand(42);
        pac_test_signal_t* signal = generate_pac_signal(n, 0.6f, 0.1f);

        // Measure complex implementation
        benchmark_timer_t timer;
        double complex_total = 0.0;
        for (uint32_t i = 0; i < iterations; i++) {
            timer_start(&timer);
            float mi = phasor_pac_modulation_index(signal->theta_phasors,
                                                   signal->gamma_amplitude,
                                                   n);
            complex_total += timer_end(&timer);
            (void)mi;
        }
        double complex_mean = complex_total / iterations / 1000.0;

        // Measure baseline implementation
        double baseline_total = 0.0;
        for (uint32_t i = 0; i < iterations; i++) {
            timer_start(&timer);
            float mi = baseline_pac_modulation_index(signal->theta_phase_angles,
                                                     signal->gamma_amplitude,
                                                     n);
            baseline_total += timer_end(&timer);
            (void)mi;
        }
        double baseline_mean = baseline_total / iterations / 1000.0;

        double speedup = baseline_mean / complex_mean;

        printf(" %5u |    %7.3f   |    %7.3f    |  %.2fx\n",
               n, complex_mean, baseline_mean, speedup);

        free_pac_signal(signal);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("Complex PAC Detection Benchmarks\n");
    printf("========================================\n");
    printf("Target: 2-5x speedup for PAC detection\n");
    printf("Target: <2µs for N=1000 PAC computation\n");

    // Initialize complex math
    complex_math_init(NULL);

    // Run benchmarks
    benchmark_pac_modulation_index();
    benchmark_pac_coupling_strength();
    benchmark_pac_noise_robustness();
    benchmark_pac_signal_length();

    printf("\n========================================\n");
    printf("Benchmark Complete\n");
    printf("========================================\n");

    complex_math_cleanup();
    return 0;
}
