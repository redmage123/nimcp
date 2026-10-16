//=============================================================================
// benchmark_pattern_cow.c - Phase 1.4 CoW Benchmark
//=============================================================================

#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>

#include "middleware/patterns/nimcp_pattern_cow.h"
#include "utils/memory/nimcp_memory.h"

#define PATTERN_DIM 128
#define NUM_CLONES 10000
#define NUM_TRIALS 10

//=============================================================================
// Baseline: Deep Copy Pattern Cloning
//=============================================================================

typedef struct {
    float* data;
    uint32_t dimension;
    uint32_t refcount;  // Not atomic, just for structure similarity
} pattern_baseline_t;

static pattern_baseline_t* pattern_baseline_create(const float* data, uint32_t dim) {
    pattern_baseline_t* p = malloc(sizeof(pattern_baseline_t));
    p->data = malloc(dim * sizeof(float));
    memcpy(p->data, data, dim * sizeof(float));
    p->dimension = dim;
    p->refcount = 1;
    return p;
}

static pattern_baseline_t* pattern_baseline_clone_deep(pattern_baseline_t* p) {
    // Baseline: DEEP COPY every time (malloc + memcpy)
    pattern_baseline_t* clone = malloc(sizeof(pattern_baseline_t));
    clone->data = malloc(p->dimension * sizeof(float));
    memcpy(clone->data, p->data, p->dimension * sizeof(float));
    clone->dimension = p->dimension;
    clone->refcount = 1;
    return clone;
}

static void pattern_baseline_free(pattern_baseline_t* p) {
    if (!p) return;
    free(p->data);
    free(p);
}

//=============================================================================
// Benchmark: Baseline Deep Copy
//=============================================================================

static double benchmark_baseline_deep_copy(uint32_t dim, uint32_t num_clones) {
    float* test_data = malloc(dim * sizeof(float));
    for (uint32_t i = 0; i < dim; i++) {
        test_data[i] = (float)i;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Create original
    pattern_baseline_t* original = pattern_baseline_create(test_data, dim);

    // Clone many times (deep copy)
    pattern_baseline_t** clones = malloc(num_clones * sizeof(pattern_baseline_t*));
    for (uint32_t i = 0; i < num_clones; i++) {
        clones[i] = pattern_baseline_clone_deep(original);
    }

    // Free all
    for (uint32_t i = 0; i < num_clones; i++) {
        pattern_baseline_free(clones[i]);
    }
    pattern_baseline_free(original);
    free(clones);

    clock_gettime(CLOCK_MONOTONIC, &end);
    free(test_data);

    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    return elapsed;
}

//=============================================================================
// Benchmark: Phase 1.4 CoW (Zero-Copy Clone)
//=============================================================================

static double benchmark_cow_zero_copy(uint32_t dim, uint32_t num_clones) {
    float* test_data = malloc(dim * sizeof(float));
    for (uint32_t i = 0; i < dim; i++) {
        test_data[i] = (float)i;
    }

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Create original
    pattern_cow_t* original = pattern_cow_create(test_data, dim);

    // Clone many times (zero-copy, just refcount increment)
    pattern_cow_t** clones = malloc(num_clones * sizeof(pattern_cow_t*));
    for (uint32_t i = 0; i < num_clones; i++) {
        clones[i] = pattern_cow_clone(original);
    }

    // Release all
    for (uint32_t i = 0; i < num_clones; i++) {
        pattern_cow_release(clones[i]);
    }
    pattern_cow_release(original);
    free(clones);

    clock_gettime(CLOCK_MONOTONIC, &end);
    free(test_data);

    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    return elapsed;
}

//=============================================================================
// Main Benchmark
//=============================================================================

int main() {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║          Phase 1.4: Pattern CoW Benchmark                    ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    printf("Configuration:\n");
    printf("  Pattern dimension:  %u floats (%u bytes)\n", PATTERN_DIM, PATTERN_DIM * 4);
    printf("  Number of clones:   %u\n", NUM_CLONES);
    printf("  Trials:             %u\n\n", NUM_TRIALS);

    // Warmup
    benchmark_baseline_deep_copy(PATTERN_DIM, 100);
    benchmark_cow_zero_copy(PATTERN_DIM, 100);

    // Baseline: Deep Copy
    printf("Running baseline (deep copy)...\n");
    double baseline_total = 0.0;
    for (int i = 0; i < NUM_TRIALS; i++) {
        double t = benchmark_baseline_deep_copy(PATTERN_DIM, NUM_CLONES);
        baseline_total += t;
        printf("  Trial %d: %.6f seconds\n", i+1, t);
    }
    double baseline_avg = baseline_total / NUM_TRIALS;

    printf("\nRunning Phase 1.4 (CoW zero-copy)...\n");
    double cow_total = 0.0;
    for (int i = 0; i < NUM_TRIALS; i++) {
        double t = benchmark_cow_zero_copy(PATTERN_DIM, NUM_CLONES);
        cow_total += t;
        printf("  Trial %d: %.6f seconds\n", i+1, t);
    }
    double cow_avg = cow_total / NUM_TRIALS;

    // Results
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                        RESULTS                               ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║ Baseline (deep copy):        %.6f seconds                ║\n", baseline_avg);
    printf("║ Phase 1.4 (CoW):             %.6f seconds                ║\n", cow_avg);
    printf("║                                                              ║\n");
    printf("║ Speedup:                     %.2fx                         ║\n", baseline_avg / cow_avg);
    printf("║ Memory savings:              ~%.1f%% (shared data)          ║\n",
           (1.0 - (1.0 / NUM_CLONES)) * 100.0);
    printf("╚══════════════════════════════════════════════════════════════╝\n");

    double clones_per_sec_baseline = NUM_CLONES / baseline_avg;
    double clones_per_sec_cow = NUM_CLONES / cow_avg;

    printf("\nThroughput:\n");
    printf("  Baseline: %.0f clones/second\n", clones_per_sec_baseline);
    printf("  Phase 1.4: %.0f clones/second\n", clones_per_sec_cow);

    printf("\n✓ Phase 1.4 CoW benchmark complete\n\n");

    return 0;
}
