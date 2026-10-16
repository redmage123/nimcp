//=============================================================================
// benchmark_complex_fft.c
//=============================================================================
/**
 * @file benchmark_complex_fft.c
 * @brief Performance benchmarks: Complex FFT operations
 *
 * OBJECTIVE: Verify FFT performance targets
 *
 * BENCHMARKS:
 * 1. FFT (N=1024): target <50µs
 * 2. Power spectrum (N=1024): target <60µs
 * 3. Hilbert transform (N=1024): target <80µs
 * 4. Comparison to FFTW (if available)
 * 5. Scalability testing (various N)
 *
 * TARGET: Performance comparable to optimized FFT libraries
 */

#include "utils/math/nimcp_complex_math.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

// Try to include FFTW for comparison (optional)
#ifdef HAVE_FFTW
#include <fftw3.h>
#endif

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
// Signal Generation
//=============================================================================

void generate_test_signal(neural_phasor_t* signal, uint32_t n, bool add_noise) {
    // Generate multi-frequency signal
    // f1 = 10 Hz, f2 = 25 Hz, f3 = 40 Hz
    float sample_rate = 1000.0f;

    for (uint32_t i = 0; i < n; i++) {
        float t = (float)i / sample_rate;

        // Sum of sinusoids
        float value = 0.0f;
        value += 1.0f * sinf(2.0f * M_PI * 10.0f * t);  // 10 Hz
        value += 0.5f * sinf(2.0f * M_PI * 25.0f * t);  // 25 Hz
        value += 0.3f * sinf(2.0f * M_PI * 40.0f * t);  // 40 Hz

        if (add_noise) {
            value += 0.1f * ((float)rand() / RAND_MAX - 0.5f);
        }

        signal[i] = phasor_from_cartesian(value, 0.0f);
    }
}

void generate_real_signal(float* signal, uint32_t n, bool add_noise) {
    float sample_rate = 1000.0f;

    for (uint32_t i = 0; i < n; i++) {
        float t = (float)i / sample_rate;

        float value = 0.0f;
        value += 1.0f * sinf(2.0f * M_PI * 10.0f * t);
        value += 0.5f * sinf(2.0f * M_PI * 25.0f * t);
        value += 0.3f * sinf(2.0f * M_PI * 40.0f * t);

        if (add_noise) {
            value += 0.1f * ((float)rand() / RAND_MAX - 0.5f);
        }

        signal[i] = value;
    }
}

//=============================================================================
// Benchmark 1: FFT Performance (Target: <50µs for N=1024)
//=============================================================================

void benchmark_fft(void) {
    printf("\n========================================\n");
    printf("BENCHMARK 1: FFT (N=1024)\n");
    printf("========================================\n");

    const uint32_t n = 1024;
    const uint32_t iterations = 5000;
    double* times = malloc(iterations * sizeof(double));
    benchmark_timer_t timer;

    // Prepare test data
    neural_phasor_t* input = malloc(n * sizeof(neural_phasor_t));
    neural_phasor_t* output = malloc(n * sizeof(neural_phasor_t));

    srand(42);
    generate_test_signal(input, n, true);

    // Test: Forward FFT
    printf("\n--- phasor_fft (forward) ---\n");
    for (uint32_t i = 0; i < iterations; i++) {
        timer_start(&timer);
        bool success = phasor_fft(input, output, n);
        times[i] = timer_end(&timer);

        if (!success) {
            printf("ERROR: FFT failed at iteration %u\n", i);
            break;
        }
    }

    benchmark_stats_t stats;
    compute_stats(times, iterations, &stats);
    stats.mean /= 1000.0;  // Convert to µs
    stats.min /= 1000.0;
    stats.max /= 1000.0;
    stats.stddev /= 1000.0;
    print_stats("Forward FFT", &stats, "µs", 50.0);

    // Test: Inverse FFT
    printf("\n--- phasor_ifft (inverse) ---\n");
    for (uint32_t i = 0; i < iterations; i++) {
        timer_start(&timer);
        bool success = phasor_ifft(output, input, n);
        times[i] = timer_end(&timer);

        if (!success) {
            printf("ERROR: IFFT failed at iteration %u\n", i);
            break;
        }
    }

    compute_stats(times, iterations, &stats);
    stats.mean /= 1000.0;
    stats.min /= 1000.0;
    stats.max /= 1000.0;
    stats.stddev /= 1000.0;
    print_stats("Inverse FFT", &stats, "µs", 50.0);

    free(input);
    free(output);
    free(times);
}

//=============================================================================
// Benchmark 2: Power Spectrum (Target: <60µs for N=1024)
//=============================================================================

void benchmark_power_spectrum(void) {
    printf("\n========================================\n");
    printf("BENCHMARK 2: Power Spectrum (N=1024)\n");
    printf("========================================\n");

    const uint32_t n = 1024;
    const uint32_t iterations = 5000;
    double* times = malloc(iterations * sizeof(double));
    benchmark_timer_t timer;

    // Prepare test data
    neural_phasor_t* input = malloc(n * sizeof(neural_phasor_t));
    float* power = malloc(n * sizeof(float));

    srand(42);
    generate_test_signal(input, n, true);

    // Test: Power spectrum
    printf("\n--- phasor_power_spectrum ---\n");
    for (uint32_t i = 0; i < iterations; i++) {
        timer_start(&timer);
        bool success = phasor_power_spectrum(input, power, n);
        times[i] = timer_end(&timer);

        if (!success) {
            printf("ERROR: Power spectrum failed at iteration %u\n", i);
            break;
        }
    }

    benchmark_stats_t stats;
    compute_stats(times, iterations, &stats);
    stats.mean /= 1000.0;  // Convert to µs
    stats.min /= 1000.0;
    stats.max /= 1000.0;
    stats.stddev /= 1000.0;
    print_stats("Power Spectrum", &stats, "µs", 60.0);

    // Verify spectrum has expected peaks
    printf("\nSpectrum analysis:\n");
    float max_power = 0.0f;
    uint32_t max_idx = 0;
    for (uint32_t i = 1; i < n/2; i++) {
        if (power[i] > max_power) {
            max_power = power[i];
            max_idx = i;
        }
    }
    float dominant_freq = (float)max_idx * 1000.0f / n;
    printf("  Dominant frequency: %.1f Hz (expected ~10 Hz)\n", dominant_freq);

    free(input);
    free(power);
    free(times);
}

//=============================================================================
// Benchmark 3: Hilbert Transform (Target: <80µs for N=1024)
//=============================================================================

void benchmark_hilbert_transform(void) {
    printf("\n========================================\n");
    printf("BENCHMARK 3: Hilbert Transform (N=1024)\n");
    printf("========================================\n");

    const uint32_t n = 1024;
    const uint32_t iterations = 3000;
    double* times = malloc(iterations * sizeof(double));
    benchmark_timer_t timer;

    // Prepare test data
    float* real_signal = malloc(n * sizeof(float));
    neural_phasor_t* analytic = malloc(n * sizeof(neural_phasor_t));

    srand(42);
    generate_real_signal(real_signal, n, true);

    // Test: Hilbert transform
    printf("\n--- phasor_hilbert_transform ---\n");
    for (uint32_t i = 0; i < iterations; i++) {
        timer_start(&timer);
        bool success = phasor_hilbert_transform(real_signal, analytic, n);
        times[i] = timer_end(&timer);

        if (!success) {
            printf("ERROR: Hilbert transform failed at iteration %u\n", i);
            break;
        }
    }

    benchmark_stats_t stats;
    compute_stats(times, iterations, &stats);
    stats.mean /= 1000.0;  // Convert to µs
    stats.min /= 1000.0;
    stats.max /= 1000.0;
    stats.stddev /= 1000.0;
    print_stats("Hilbert Transform", &stats, "µs", 80.0);

    // Verify analytic signal properties
    printf("\nAnalytic signal properties:\n");
    float mean_amplitude = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        mean_amplitude += phasor_amplitude(analytic[i]);
    }
    mean_amplitude /= n;
    printf("  Mean amplitude: %.3f (expected ~0.8-1.2)\n", mean_amplitude);

    free(real_signal);
    free(analytic);
    free(times);
}

//=============================================================================
// Benchmark 4: FFT Scalability
//=============================================================================

void benchmark_fft_scalability(void) {
    printf("\n========================================\n");
    printf("BENCHMARK 4: FFT Scalability\n");
    printf("========================================\n");

    uint32_t sizes[] = {64, 128, 256, 512, 1024, 2048, 4096};
    uint32_t num_sizes = sizeof(sizes) / sizeof(uint32_t);

    printf("\n  N    | FFT (µs) | Throughput (Msamples/s)\n");
    printf("-------|----------|-------------------------\n");

    for (uint32_t s = 0; s < num_sizes; s++) {
        uint32_t n = sizes[s];
        // Scale iterations: more for small N, fewer for large N
        // Avoid division by zero for n < 256
        uint32_t divisor = (n / 256);
        if (divisor == 0) divisor = 1;  // Prevent division by zero
        uint32_t iterations = 10000 / divisor;
        if (iterations < 100) iterations = 100;

        neural_phasor_t* input = malloc(n * sizeof(neural_phasor_t));
        neural_phasor_t* output = malloc(n * sizeof(neural_phasor_t));

        srand(42);
        generate_test_signal(input, n, false);

        // Measure FFT
        benchmark_timer_t timer;
        double total_time = 0.0;
        for (uint32_t i = 0; i < iterations; i++) {
            timer_start(&timer);
            phasor_fft(input, output, n);
            total_time += timer_end(&timer);
        }

        double mean_time = total_time / iterations / 1000.0;  // µs
        double throughput = (n / mean_time);  // Msamples/s

        printf(" %5u |  %7.2f  |          %.2f\n", n, mean_time, throughput);

        free(input);
        free(output);
    }
}

//=============================================================================
// Benchmark 5: Accuracy Verification
//=============================================================================

void benchmark_fft_accuracy(void) {
    printf("\n========================================\n");
    printf("BENCHMARK 5: FFT Accuracy\n");
    printf("========================================\n");

    const uint32_t n = 1024;

    // Create known signal: single frequency at 10 Hz
    neural_phasor_t* input = malloc(n * sizeof(neural_phasor_t));
    neural_phasor_t* output = malloc(n * sizeof(neural_phasor_t));
    neural_phasor_t* reconstructed = malloc(n * sizeof(neural_phasor_t));

    float sample_rate = 1000.0f;
    float test_freq = 10.0f;

    for (uint32_t i = 0; i < n; i++) {
        float t = (float)i / sample_rate;
        float value = sinf(2.0f * M_PI * test_freq * t);
        input[i] = phasor_from_cartesian(value, 0.0f);
    }

    // Forward FFT
    phasor_fft(input, output, n);

    // Find peak in spectrum
    float max_magnitude = 0.0f;
    uint32_t peak_idx = 0;
    for (uint32_t i = 1; i < n/2; i++) {
        float mag = phasor_amplitude(output[i]);
        if (mag > max_magnitude) {
            max_magnitude = mag;
            peak_idx = i;
        }
    }

    float detected_freq = (float)peak_idx * sample_rate / n;
    printf("\nFrequency detection:\n");
    printf("  Input frequency:    %.2f Hz\n", test_freq);
    printf("  Detected frequency: %.2f Hz\n", detected_freq);
    printf("  Error:              %.3f Hz\n", fabs(detected_freq - test_freq));

    // Inverse FFT and reconstruction error
    phasor_ifft(output, reconstructed, n);

    float max_error = 0.0f;
    float mean_error = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        float error = fabs(reconstructed[i].real - input[i].real);
        mean_error += error;
        if (error > max_error) {
            max_error = error;
        }
    }
    mean_error /= n;

    printf("\nReconstruction accuracy:\n");
    printf("  Mean error: %.6f\n", mean_error);
    printf("  Max error:  %.6f\n", max_error);
    if (mean_error < 1e-5 && max_error < 1e-4) {
        printf("  Status: PASS (high accuracy)\n");
    } else if (mean_error < 1e-3 && max_error < 1e-2) {
        printf("  Status: PASS (acceptable accuracy)\n");
    } else {
        printf("  Status: FAIL (accuracy issues)\n");
    }

    free(input);
    free(output);
    free(reconstructed);
}

//=============================================================================
// Benchmark 6: FFTW Comparison (if available)
//=============================================================================

#ifdef HAVE_FFTW
void benchmark_fftw_comparison(void) {
    printf("\n========================================\n");
    printf("BENCHMARK 6: FFTW Comparison\n");
    printf("========================================\n");

    const uint32_t n = 1024;
    const uint32_t iterations = 5000;

    // Allocate FFTW arrays
    fftwf_complex* fftw_in = fftwf_alloc_complex(n);
    fftwf_complex* fftw_out = fftwf_alloc_complex(n);
    fftwf_plan plan = fftwf_plan_dft_1d(n, fftw_in, fftw_out, FFTW_FORWARD, FFTW_MEASURE);

    // Allocate our arrays
    neural_phasor_t* our_in = malloc(n * sizeof(neural_phasor_t));
    neural_phasor_t* our_out = malloc(n * sizeof(neural_phasor_t));

    // Generate test signal
    srand(42);
    for (uint32_t i = 0; i < n; i++) {
        float t = (float)i / 1000.0f;
        float value = sinf(2.0f * M_PI * 10.0f * t);
        fftw_in[i][0] = value;
        fftw_in[i][1] = 0.0f;
        our_in[i] = phasor_from_cartesian(value, 0.0f);
    }

    // Benchmark FFTW
    benchmark_timer_t timer;
    double fftw_total = 0.0;
    for (uint32_t i = 0; i < iterations; i++) {
        timer_start(&timer);
        fftwf_execute(plan);
        fftw_total += timer_end(&timer);
    }
    double fftw_mean = fftw_total / iterations / 1000.0;  // µs

    // Benchmark our FFT
    double our_total = 0.0;
    for (uint32_t i = 0; i < iterations; i++) {
        timer_start(&timer);
        phasor_fft(our_in, our_out, n);
        our_total += timer_end(&timer);
    }
    double our_mean = our_total / iterations / 1000.0;  // µs

    printf("\nFFTW vs Our FFT (N=%u):\n", n);
    printf("  FFTW:    %.3f µs\n", fftw_mean);
    printf("  Our FFT: %.3f µs\n", our_mean);
    printf("  Ratio:   %.2fx\n", our_mean / fftw_mean);

    if (our_mean / fftw_mean < 2.0) {
        printf("  Status: EXCELLENT (within 2x of FFTW)\n");
    } else if (our_mean / fftw_mean < 5.0) {
        printf("  Status: GOOD (within 5x of FFTW)\n");
    } else {
        printf("  Status: NEEDS OPTIMIZATION (>5x slower than FFTW)\n");
    }

    fftwf_destroy_plan(plan);
    fftwf_free(fftw_in);
    fftwf_free(fftw_out);
    free(our_in);
    free(our_out);
}
#endif

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    printf("========================================\n");
    printf("Complex FFT Benchmarks\n");
    printf("========================================\n");
    printf("Targets:\n");
    printf("  - FFT (N=1024): <50µs\n");
    printf("  - Power spectrum (N=1024): <60µs\n");
    printf("  - Hilbert transform (N=1024): <80µs\n");

    // Initialize complex math
    complex_math_init(NULL);

    // Run benchmarks
    benchmark_fft();
    benchmark_power_spectrum();
    benchmark_hilbert_transform();
    benchmark_fft_scalability();
    benchmark_fft_accuracy();

#ifdef HAVE_FFTW
    benchmark_fftw_comparison();
#else
    printf("\n========================================\n");
    printf("FFTW comparison skipped (not available)\n");
    printf("========================================\n");
#endif

    printf("\n========================================\n");
    printf("Benchmark Complete\n");
    printf("========================================\n");

    complex_math_cleanup();
    return 0;
}
