#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "middleware/nimcp_middleware.h"

int main(void) {
    printf("NIMCP Middleware Demonstration\n");
    printf("==============================\n\n");
    
    // Demo 1: Buffering neural spike train
    printf("1. Circular Buffer - Neural Spike Train\n");
    circular_buffer_t* spike_buf = circular_buffer_create(sizeof(float), 100, OVERFLOW_OVERWRITE);
    
    for (int i = 0; i < 50; i++) {
        float spike_time = i * 0.01f + ((rand() % 100) / 10000.0f);
        circular_buffer_push(spike_buf, &spike_time);
    }
    
    printf("   Buffered %zu spikes, utilization: %.1f%%\n",
           circular_buffer_size(spike_buf),
           circular_buffer_utilization(spike_buf));
    
    circular_buffer_destroy(spike_buf);
    
    // Demo 2: Sliding window feature extraction
    printf("\n2. Sliding Window - Online Statistics\n");
    sliding_window_t* window = sliding_window_create(100, 50);
    
    for (int i = 0; i < 200; i++) {
        float signal = sinf(i * 0.1f) + ((rand() % 100) / 100.0f - 0.5f);
        sliding_window_add(window, signal);
    }
    
    printf("   Mean: %.3f, Stddev: %.3f, Range: %.3f\n",
           sliding_window_mean(window),
           sliding_window_stddev(window),
           sliding_window_range(window));
    
    sliding_window_destroy(window);
    
    // Demo 3: Temporal accumulation
    printf("\n3. Temporal Accumulator - Leaky Integration\n");
    temporal_accumulator_t* acc = temporal_accumulator_create(3, 0.1f, INTEGRATION_LEAKY);
    
    for (int i = 0; i < 100; i++) {
        float input = (i % 10 == 0) ? 1.0f : 0.0f;  // Pulses every 10 steps
        temporal_accumulator_update(acc, 0, input, 0.1f);
    }
    
    printf("   Accumulated value: %.3f\n",
           temporal_accumulator_get_value(acc, 0));
    
    temporal_accumulator_destroy(acc);
    
    // Demo 4: Multi-timescale integration
    printf("\n4. Integration Buffer - Multi-scale Processing\n");
    integration_buffer_t* ibuf = integration_buffer_create(100, 50, 25, 1);
    
    for (uint64_t t = 0; t < 300; t++) {
        float signal = sinf(t * 0.05f);
        integration_buffer_add(ibuf, 0, signal, t);
    }
    
    printf("   Fast mean: %.3f, Medium mean: %.3f, Slow mean: %.3f\n",
           integration_buffer_mean(ibuf, TIMESCALE_FAST, 0),
           integration_buffer_mean(ibuf, TIMESCALE_MEDIUM, 0),
           integration_buffer_mean(ibuf, TIMESCALE_SLOW, 0));
    
    integration_buffer_destroy(ibuf);
    
    // Demo 5: Z-score normalization
    printf("\n5. Z-Score Normalizer - Statistical Normalization\n");
    zscore_normalizer_t* zscore = zscore_normalizer_create(1, 0, 3.0f);
    
    for (int i = 0; i < 100; i++) {
        float value = i * 0.5f + ((rand() % 100) / 50.0f);
        zscore_normalizer_fit(zscore, 0, value);
    }
    
    printf("   Mean: %.3f, Stddev: %.3f\n",
           zscore_normalizer_mean(zscore, 0),
           zscore_normalizer_stddev(zscore, 0));
    
    float test_val = 50.0f;
    float normalized = zscore_normalizer_transform(zscore, 0, test_val);
    printf("   Transform %.1f -> %.3f (z-score)\n", test_val, normalized);
    
    zscore_normalizer_destroy(zscore);
    
    // Demo 6: Homeostatic normalization
    printf("\n6. Homeostatic Normalizer - Activity Regulation\n");
    homeostatic_normalizer_t* homeo = homeostatic_normalizer_create(1, 0.5f, 10.0f);
    
    for (int i = 0; i < 100; i++) {
        float activity = 0.8f + ((rand() % 100) / 500.0f);  // Too high
        homeostatic_normalizer_update(homeo, 0, activity, 0.1f);
    }
    
    printf("   Scaling factor: %.3f (reduces high activity)\n",
           homeostatic_normalizer_get_scaling(homeo, 0));
    
    homeostatic_normalizer_destroy(homeo);
    
    printf("\nMiddleware demonstration complete!\n");
    return 0;
}
