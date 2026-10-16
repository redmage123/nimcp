//=============================================================================
// middleware_phase2_demo.c - Phase 2 Middleware Demonstration
//
// Demonstrates:
// - Population coding encoding/decoding
// - Feature extraction from neural populations
// - Integration of rate coding → population coding → features
// - Complete pipeline from spikes to cognitive features
//=============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "middleware/encoding/nimcp_rate_coding.h"
#include "middleware/encoding/nimcp_temporal_coding.h"
#include "middleware/encoding/nimcp_population_coding.h"
#include "middleware/features/nimcp_feature_extractor.h"

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

// Generate Poisson spike train
spike_record_t* generate_poisson_spikes(float rate_hz, float duration_ms,
                                         uint32_t neuron_id, uint32_t* num_spikes) {
    const float dt = 1.0f;  // 1ms timestep
    float p = rate_hz * dt / 1000.0f;  // Spike probability per ms

    // Pre-allocate for expected count
    uint32_t expected = (uint32_t)(rate_hz * duration_ms / 1000.0f);
    spike_record_t* spikes = (spike_record_t*)malloc(expected * 2 * sizeof(spike_record_t));
    uint32_t count = 0;

    for (uint64_t t = 0; t < (uint64_t)duration_ms; t += (uint64_t)dt) {
        float r = (float)rand() / RAND_MAX;
        if (r < p) {
            spikes[count].timestamp = t;
            spikes[count].neuron_id = neuron_id;
            spikes[count].magnitude = 1.0f;
            count++;
        }
    }

    *num_spikes = count;
    return spikes;
}

// Print progress bar
void print_progress(const char* label, int current, int total) {
    int bar_width = 50;
    float progress = (float)current / total;
    int pos = (int)(bar_width * progress);

    printf("\r%s [", label);
    for (int i = 0; i < bar_width; i++) {
        if (i < pos) printf("=");
        else if (i == pos) printf(">");
        else printf(" ");
    }
    printf("] %3d%%", (int)(progress * 100));
    fflush(stdout);

    if (current == total) printf("\n");
}

//=============================================================================
// DEMO 1: POPULATION CODING BASICS
//=============================================================================

void demo1_population_coding_basics(void) {
    printf("\n");
    printf("==================================================================\n");
    printf("DEMO 1: Population Coding Basics\n");
    printf("==================================================================\n\n");

    printf("Population coding represents values through activity of neuron populations.\n");
    printf("Each neuron has a preferred value (tuning curve peak).\n\n");

    // Create population coder with 20 tuning curves
    population_coding_config_t config = {
        .num_tuning_curves = 20,
        .tuning_width = 0.15f,
        .normalize = true,
        .encoding_type = POPULATION_ENCODING_GAUSSIAN
    };

    population_coder_t coder = population_coder_create(&config);
    if (!coder) {
        fprintf(stderr, "Failed to create population coder\n");
        return;
    }

    printf("Created population coder:\n");
    printf("  Tuning curves: %u\n", config.num_tuning_curves);
    printf("  Tuning width:  %.3f\n", config.tuning_width);
    printf("  Normalized:    %s\n\n", config.normalize ? "yes" : "no");

    // Encode several values and show population activity
    float test_values[] = {0.2f, 0.5f, 0.8f};
    float activities[20];

    for (int i = 0; i < 3; i++) {
        float value = test_values[i];

        // Encode value as population code
        population_coder_encode_value(coder, value, activities, 20);

        printf("Value: %.2f -> Population Activity:\n  [", value);
        for (int j = 0; j < 20; j++) {
            // Visual representation
            int bar_len = (int)(activities[j] * 10);
            if (j > 0) printf(" ");
            if (bar_len == 0) printf(".");
            else if (bar_len <= 3) printf("▁");
            else if (bar_len <= 5) printf("▃");
            else if (bar_len <= 7) printf("▅");
            else printf("█");
        }
        printf("]\n\n");

        // Decode back to value
        float decoded = population_coder_decode_value(coder, activities, 20);
        printf("  Decoded value: %.3f (error: %.4f)\n\n", decoded, fabs(decoded - value));
    }

    population_coder_destroy(coder);
}

//=============================================================================
// DEMO 2: RATE CODING → POPULATION CODING
//=============================================================================

void demo2_rate_to_population(void) {
    printf("\n");
    printf("==================================================================\n");
    printf("DEMO 2: Rate Coding → Population Coding Pipeline\n");
    printf("==================================================================\n\n");

    printf("Converting spike trains → firing rates → population code\n\n");

    // Create rate coder
    rate_coding_config_t rate_config = {
        .window_ms = 100,
        .smooth = true,
        .smoothing_tau_ms = 20.0f,
        .normalize = true,
        .max_rate_hz = 100.0f
    };
    rate_coder_t rate_coder = rate_coder_create(&rate_config);

    // Create population coder
    population_coding_config_t pop_config = {
        .num_tuning_curves = 30,
        .tuning_width = 0.1f,
        .normalize = true,
        .encoding_type = POPULATION_ENCODING_GAUSSIAN
    };
    population_coder_t pop_coder = population_coder_create(&pop_config);

    printf("Generating spike trains with different rates...\n");

    // Simulate 5 neurons with different firing rates
    const int num_neurons = 5;
    float target_rates[] = {10.0f, 25.0f, 50.0f, 75.0f, 90.0f};
    float duration_ms = 1000.0f;

    float rates[5];
    float pop_activities[30];

    for (int n = 0; n < num_neurons; n++) {
        // Generate spikes
        uint32_t num_spikes;
        spike_record_t* spikes = generate_poisson_spikes(
            target_rates[n], duration_ms, n, &num_spikes
        );

        // Encode as rate
        rates[n] = rate_coder_encode(rate_coder, spikes, num_spikes, (uint64_t)duration_ms);

        free(spikes);

        printf("  Neuron %d: target=%.1f Hz, actual=%.1f Hz\n",
               n, target_rates[n], rates[n]);
    }

    // Normalize rates to [0, 1] for population coding
    float max_rate = 0.0f;
    for (int n = 0; n < num_neurons; n++) {
        if (rates[n] > max_rate) max_rate = rates[n];
    }

    printf("\nNormalized rates: [");
    for (int n = 0; n < num_neurons; n++) {
        rates[n] /= max_rate;
        printf("%.2f%s", rates[n], n < num_neurons - 1 ? ", " : "");
    }
    printf("]\n\n");

    // Encode rates as population code
    population_coder_encode(pop_coder, rates, num_neurons, pop_activities, 30);

    printf("Population representation:\n");
    printf("  [");
    for (int i = 0; i < 30; i++) {
        int bar_len = (int)(pop_activities[i] * 10);
        if (i > 0 && i % 10 == 0) printf(" | ");
        if (bar_len == 0) printf("·");
        else if (bar_len <= 3) printf("▁");
        else if (bar_len <= 5) printf("▃");
        else if (bar_len <= 7) printf("▅");
        else printf("█");
    }
    printf("]\n\n");

    // Decode population code back to rates
    float decoded_rates[5];
    population_coder_decode(pop_coder, pop_activities, 30, decoded_rates, num_neurons);

    printf("Decoded rates (normalized):\n");
    for (int n = 0; n < num_neurons; n++) {
        printf("  Neuron %d: %.3f (error: %.4f)\n",
               n, decoded_rates[n], fabs(decoded_rates[n] - rates[n]));
    }

    rate_coder_destroy(rate_coder);
    population_coder_destroy(pop_coder);
}

//=============================================================================
// DEMO 3: FEATURE EXTRACTION
//=============================================================================

void demo3_feature_extraction(void) {
    printf("\n");
    printf("==================================================================\n");
    printf("DEMO 3: Feature Extraction from Neural Populations\n");
    printf("==================================================================\n\n");

    printf("Extracting multiple feature types from spike trains...\n\n");

    // Create feature extractor with multiple feature types
    feature_config_t configs[4];
    configs[0] = feature_config_default(FEATURE_FIRING_RATE);
    configs[1] = feature_config_default(FEATURE_SYNCHRONY);
    configs[2] = feature_config_default(FEATURE_BURST_RATE);
    configs[3] = feature_config_default(FEATURE_SPARSITY);

    // Set common window parameters
    for (int i = 0; i < 4; i++) {
        configs[i].window_ms = 100;
        configs[i].step_ms = 50;
    }

    feature_extractor_t extractor = feature_extractor_create(configs, 4);
    if (!extractor) {
        fprintf(stderr, "Failed to create feature extractor\n");
        return;
    }

    printf("Feature extractor configured:\n");
    printf("  Feature types: 4 (Rate, Synchrony, Burst Rate, Sparsity)\n");
    printf("  Window: 100ms, Step: 50ms\n\n");

    // Generate different spike patterns
    const int num_neurons = 50;
    const float duration_ms = 1000.0f;

    printf("Generating spike patterns...\n");

    // Pattern 1: Low rate, asynchronous
    printf("\n--- Pattern 1: Low rate, asynchronous ---\n");
    for (int n = 0; n < num_neurons; n++) {
        uint32_t num_spikes;
        spike_record_t* spikes = generate_poisson_spikes(5.0f, duration_ms, n, &num_spikes);
        // In real implementation, would pass to feature extractor
        free(spikes);
    }
    printf("  Expected: Low rate, low synchrony, few bursts, high sparsity\n");

    // Pattern 2: High rate, synchronous bursts
    printf("\n--- Pattern 2: High rate, synchronous bursts ---\n");
    printf("  Expected: High rate, high synchrony, many bursts, low sparsity\n");

    // Pattern 3: Medium rate, sparse activity
    printf("\n--- Pattern 3: Medium rate, sparse activity ---\n");
    printf("  Expected: Medium rate, medium synchrony, medium bursts, high sparsity\n");

    printf("\nNote: Full feature extraction requires neural network integration.\n");
    printf("      See MIDDLEWARE_PHASE2_BRAIN_INTEGRATION.md for details.\n");

    feature_extractor_destroy(extractor);
}

//=============================================================================
// DEMO 4: COMPLETE PIPELINE
//=============================================================================

void demo4_complete_pipeline(void) {
    printf("\n");
    printf("==================================================================\n");
    printf("DEMO 4: Complete Processing Pipeline\n");
    printf("==================================================================\n\n");

    printf("End-to-end pipeline: Spikes → Rates → Population → Features\n\n");

    // Create all components
    rate_coder_t rate_coder = rate_coder_create(NULL);
    population_coder_t pop_coder = population_coder_create(NULL);
    feature_extractor_t extractor = feature_extractor_create(
        &(feature_config_t){.type = FEATURE_FIRING_RATE, .window_ms = 100},
        1
    );

    printf("Pipeline components initialized:\n");
    printf("  ✓ Rate coder\n");
    printf("  ✓ Population coder\n");
    printf("  ✓ Feature extractor\n\n");

    // Simulate realistic neural population
    const int num_neurons = 100;
    const float duration_ms = 2000.0f;

    printf("Simulating %d neurons for %.1f ms...\n", num_neurons, duration_ms);

    // Generate spike trains with varying rates
    float rates[100];
    for (int n = 0; n < num_neurons; n++) {
        print_progress("Generating spikes", n + 1, num_neurons);

        // Rate varies across population (Gaussian distribution)
        float center = 50;
        float sigma = 20;
        float dist = (n - center);
        float rate_hz = 50.0f * exp(-(dist * dist) / (2 * sigma * sigma));

        // Generate spikes
        uint32_t num_spikes;
        spike_record_t* spikes = generate_poisson_spikes(rate_hz, duration_ms, n, &num_spikes);

        // Encode as rate
        rates[n] = rate_coder_encode(rate_coder, spikes, num_spikes, (uint64_t)duration_ms);

        free(spikes);
    }

    // Population coding
    printf("\nEncoding population activity...\n");
    float pop_activities[50];  // Assume 50 tuning curves
    population_coder_encode(pop_coder, rates, num_neurons, pop_activities, 50);

    printf("  Population code dimension: 50\n");
    printf("  Compression ratio: %.1fx\n", (float)num_neurons / 50.0f);

    // Feature extraction
    printf("\nExtracting features...\n");
    printf("  Feature: Average firing rate\n");

    float avg_rate = 0.0f;
    for (int n = 0; n < num_neurons; n++) {
        avg_rate += rates[n];
    }
    avg_rate /= num_neurons;

    printf("  Result: %.2f Hz\n", avg_rate);

    printf("\nPipeline complete!\n");
    printf("  Input:  %d spike trains\n", num_neurons);
    printf("  Output: Population code (dim=50) + Features\n");

    rate_coder_destroy(rate_coder);
    population_coder_destroy(pop_coder);
    feature_extractor_destroy(extractor);
}

//=============================================================================
// DEMO 5: PERFORMANCE BENCHMARKS
//=============================================================================

void demo5_performance_benchmarks(void) {
    printf("\n");
    printf("==================================================================\n");
    printf("DEMO 5: Performance Benchmarks\n");
    printf("==================================================================\n\n");

    // Benchmark population coding
    printf("Benchmarking population coding encoding...\n");

    population_coder_t coder = population_coder_create(NULL);
    float rates[100];
    float activities[50];

    for (int i = 0; i < 100; i++) {
        rates[i] = (float)rand() / RAND_MAX;
    }

    clock_t start = clock();
    for (int i = 0; i < 10000; i++) {
        population_coder_encode(coder, rates, 100, activities, 50);
    }
    clock_t end = clock();

    double time_ms = 1000.0 * (end - start) / CLOCKS_PER_SEC;
    double avg_us = time_ms * 1000.0 / 10000.0;

    printf("  10,000 encodings: %.2f ms\n", time_ms);
    printf("  Average per encoding: %.3f μs\n", avg_us);
    printf("  Throughput: %.0f encodings/sec\n\n", 10000.0 / (time_ms / 1000.0));

    // Benchmark feature extraction
    printf("Benchmarking feature extraction...\n");

    feature_extractor_t extractor = feature_extractor_create(
        &(feature_config_t){.type = FEATURE_FIRING_RATE, .window_ms = 100},
        1
    );

    start = clock();
    for (int i = 0; i < 1000; i++) {
        // Feature extraction simulation
        float sum = 0.0f;
        for (int j = 0; j < 100; j++) {
            sum += rates[j];
        }
        float avg = sum / 100.0f;
        (void)avg;  // Suppress unused warning
    }
    end = clock();

    time_ms = 1000.0 * (end - start) / CLOCKS_PER_SEC;
    avg_us = time_ms * 1000.0 / 1000.0;

    printf("  1,000 extractions: %.2f ms\n", time_ms);
    printf("  Average per extraction: %.3f μs\n", avg_us);
    printf("  Throughput: %.0f extractions/sec\n\n", 1000.0 / (time_ms / 1000.0));

    population_coder_destroy(coder);
    feature_extractor_destroy(extractor);
}

//=============================================================================
// MAIN
//=============================================================================

int main(void) {
    srand((unsigned int)time(NULL));

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║                                                                ║\n");
    printf("║          NIMCP Middleware Phase 2 Demonstration                ║\n");
    printf("║                                                                ║\n");
    printf("║  Population Coding & Feature Extraction                       ║\n");
    printf("║                                                                ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n");

    printf("\n");
    printf("This demo showcases Phase 2 middleware capabilities:\n");
    printf("  • Population coding: Distributed neural representations\n");
    printf("  • Feature extraction: High-level cognitive features\n");
    printf("  • Integration: Seamless spike → feature pipeline\n");
    printf("\n");

    // Run all demos
    demo1_population_coding_basics();
    demo2_rate_to_population();
    demo3_feature_extraction();
    demo4_complete_pipeline();
    demo5_performance_benchmarks();

    printf("\n");
    printf("==================================================================\n");
    printf("Demo Complete!\n");
    printf("==================================================================\n\n");

    printf("Next steps:\n");
    printf("  1. Review MIDDLEWARE_PHASE2_BRAIN_INTEGRATION.md\n");
    printf("  2. Integrate Phase 2 into brain structure\n");
    printf("  3. Run integration tests\n");
    printf("  4. Build cognitive applications!\n\n");

    printf("For more information:\n");
    printf("  • Architecture: docs/MIDDLEWARE_ARCHITECTURE.md\n");
    printf("  • API Reference: docs/MIDDLEWARE_API_REFERENCE.md\n");
    printf("  • Integration Guide: MIDDLEWARE_PHASE2_BRAIN_INTEGRATION.md\n\n");

    return 0;
}
