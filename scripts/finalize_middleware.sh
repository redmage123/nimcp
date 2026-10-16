#!/bin/bash
set -e

echo "Finalizing middleware implementation..."

# Create CMakeLists.txt for middleware
cat > /home/bbrelin/nimcp/src/middleware/CMakeLists.txt << 'EOF'
# Middleware Subsystems
add_library(nimcp_middleware STATIC
    # Buffering
    buffering/nimcp_circular_buffer.c
    buffering/nimcp_sliding_window.c
    buffering/nimcp_temporal_accumulator.c
    buffering/nimcp_integration_buffer.c
    
    # Normalization
    normalization/nimcp_zscore_normalizer.c
    normalization/nimcp_min_max_normalizer.c
    normalization/nimcp_adaptive_normalizer.c
    normalization/nimcp_homeostatic_normalizer.c
)

target_include_directories(nimcp_middleware PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/middleware
)

target_link_libraries(nimcp_middleware PRIVATE m)

install(TARGETS nimcp_middleware ARCHIVE DESTINATION lib)
EOF

# Create main middleware header
cat > /home/bbrelin/nimcp/src/middleware/nimcp_middleware.h << 'EOF'
#ifndef NIMCP_MIDDLEWARE_H
#define NIMCP_MIDDLEWARE_H

#include "buffering/nimcp_circular_buffer.h"
#include "buffering/nimcp_sliding_window.h"
#include "buffering/nimcp_temporal_accumulator.h"
#include "buffering/nimcp_integration_buffer.h"
#include "normalization/nimcp_zscore_normalizer.h"
#include "normalization/nimcp_min_max_normalizer.h"
#include "normalization/nimcp_adaptive_normalizer.h"
#include "normalization/nimcp_homeostatic_normalizer.h"

#endif // NIMCP_MIDDLEWARE_H
EOF

# Create comprehensive test CMakeLists.txt
cat > /home/bbrelin/nimcp/test/unit/middleware/CMakeLists.txt << 'EOF'
# Middleware Unit Tests

# Buffering Tests
add_executable(test_circular_buffer buffering/test_circular_buffer.cpp)
target_link_libraries(test_circular_buffer 
    nimcp_middleware
    gtest
    gtest_main
    pthread
)
add_test(NAME CircularBuffer COMMAND test_circular_buffer)

# Integration Test
add_executable(test_middleware_integration test_middleware_integration.cpp)
target_link_libraries(test_middleware_integration
    nimcp_middleware
    gtest
    gtest_main
    pthread
)
add_test(NAME MiddlewareIntegration COMMAND test_middleware_integration)
EOF

# Create integration test
cat > /home/bbrelin/nimcp/test/unit/middleware/test_middleware_integration.cpp << 'EOF'
#include <gtest/gtest.h>
extern "C" {
#include "middleware/nimcp_middleware.h"
}

TEST(MiddlewareIntegration, BufferingPipeline) {
    // Create buffering components
    circular_buffer_t* cbuf = circular_buffer_create(sizeof(float), 100, OVERFLOW_OVERWRITE);
    sliding_window_t* window = sliding_window_create(50, 0);
    temporal_accumulator_t* acc = temporal_accumulator_create(3, 0.1f, INTEGRATION_EMA);
    integration_buffer_t* ibuf = integration_buffer_create(100, 50, 25, 3);
    
    ASSERT_NE(cbuf, nullptr);
    ASSERT_NE(window, nullptr);
    ASSERT_NE(acc, nullptr);
    ASSERT_NE(ibuf, nullptr);
    
    // Simulate neural signal
    for (int i = 0; i < 200; i++) {
        float signal = sinf(i * 0.1f) + 0.5f * cosf(i * 0.05f);
        
        // Buffer in circular buffer
        circular_buffer_push(cbuf, &signal);
        
        // Update sliding window
        sliding_window_add(window, signal);
        
        // Integrate in accumulator
        temporal_accumulator_update(acc, 0, signal, 0.01f);
        
        // Add to multi-timescale buffer
        integration_buffer_add(ibuf, 0, signal, i);
    }
    
    // Verify components have data
    EXPECT_GT(circular_buffer_size(cbuf), 0);
    EXPECT_TRUE(sliding_window_is_full(window));
    EXPECT_GT(temporal_accumulator_get_value(acc, 0), -100.0f);
    EXPECT_GT(integration_buffer_count(ibuf, TIMESCALE_FAST), 0);
    
    // Cleanup
    circular_buffer_destroy(cbuf);
    sliding_window_destroy(window);
    temporal_accumulator_destroy(acc);
    integration_buffer_destroy(ibuf);
}

TEST(MiddlewareIntegration, NormalizationPipeline) {
    // Create normalizers
    zscore_normalizer_t* zscore = zscore_normalizer_create(3, 0, 3.0f);
    min_max_normalizer_t* minmax = minmax_normalizer_create(3, 0.0f, 1.0f, false);
    adaptive_normalizer_t* adaptive = adaptive_normalizer_create(3, 0.01f, 0.001f);
    homeostatic_normalizer_t* homeo = homeostatic_normalizer_create(3, 0.5f, 10.0f);
    
    ASSERT_NE(zscore, nullptr);
    ASSERT_NE(minmax, nullptr);
    ASSERT_NE(adaptive, nullptr);
    ASSERT_NE(homeo, nullptr);
    
    // Process signal through all normalizers
    for (int i = 0; i < 100; i++) {
        float signal = (float)i * 0.1f;
        
        float z = zscore_normalizer_fit_transform(zscore, 0, signal);
        float mm = minmax_normalizer_fit_transform(minmax, 0, signal);
        float ad = adaptive_normalizer_fit_transform(adaptive, 0, signal);
        
        homeostatic_normalizer_update(homeo, 0, signal, 0.1f);
        float ho = homeostatic_normalizer_apply(homeo, 0, signal);
        
        // Verify bounds
        EXPECT_LE(fabsf(z), 10.0f);  // Z-score should be reasonable
        EXPECT_GE(mm, -0.1f);         // Min-max in range
        EXPECT_LE(mm, 1.1f);
    }
    
    // Cleanup
    zscore_normalizer_destroy(zscore);
    minmax_normalizer_destroy(minmax);
    adaptive_normalizer_destroy(adaptive);
    homeostatic_normalizer_destroy(homeo);
}

TEST(MiddlewareIntegration, BufferingAndNormalization) {
    // Combined buffering and normalization
    sliding_window_t* window = sliding_window_create(100, 0);
    zscore_normalizer_t* normalizer = zscore_normalizer_create(1, 100, 3.0f);
    
    ASSERT_NE(window, nullptr);
    ASSERT_NE(normalizer, nullptr);
    
    // Process noisy signal
    for (int i = 0; i < 500; i++) {
        float noisy_signal = sinf(i * 0.05f) + ((rand() % 100) / 100.0f - 0.5f);
        
        // Buffer and normalize
        sliding_window_add(window, noisy_signal);
        float normalized = zscore_normalizer_fit_transform(normalizer, 0, noisy_signal);
        
        if (i > 100) {  // After warmup
            // Normalized signal should have reasonable bounds
            EXPECT_LE(fabsf(normalized), 5.0f);
        }
    }
    
    // Window should be full
    EXPECT_TRUE(sliding_window_is_full(window));
    
    // Statistics should be reasonable
    float mean = sliding_window_mean(window);
    float stddev = sliding_window_stddev(window);
    
    EXPECT_GT(stddev, 0.0f);
    EXPECT_LT(fabsf(mean), 2.0f);  // Mean should be near zero
    
    sliding_window_destroy(window);
    zscore_normalizer_destroy(normalizer);
}

TEST(MiddlewareIntegration, MultiTimescaleProcessing) {
    integration_buffer_t* ibuf = integration_buffer_create(1000, 100, 10, 1);
    temporal_accumulator_t* fast_acc = temporal_accumulator_create(1, 0.5f, INTEGRATION_EMA);
    temporal_accumulator_t* slow_acc = temporal_accumulator_create(1, 0.01f, INTEGRATION_LEAKY);
    
    ASSERT_NE(ibuf, nullptr);
    ASSERT_NE(fast_acc, nullptr);
    ASSERT_NE(slow_acc, nullptr);
    
    // Simulate multi-scale signal
    for (uint64_t t = 0; t < 1000; t++) {
        float fast_component = sinf(t * 0.5f);
        float slow_component = 0.5f * sinf(t * 0.01f);
        float signal = fast_component + slow_component;
        
        integration_buffer_add(ibuf, 0, signal, t);
        temporal_accumulator_update(fast_acc, 0, signal, 1.0f);
        temporal_accumulator_update(slow_acc, 0, signal, 1.0f);
    }
    
    // Verify different timescales captured different dynamics
    float fast_value = temporal_accumulator_get_value(fast_acc, 0);
    float slow_value = temporal_accumulator_get_value(slow_acc, 0);
    
    // Fast accumulator should track recent changes more
    float fast_mean = integration_buffer_mean(ibuf, TIMESCALE_FAST, 0);
    float slow_mean = integration_buffer_mean(ibuf, TIMESCALE_SLOW, 0);
    
    EXPECT_NE(fast_mean, slow_mean);  // Different timescales capture different dynamics
    
    integration_buffer_destroy(ibuf);
    temporal_accumulator_destroy(fast_acc);
    temporal_accumulator_destroy(slow_acc);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
EOF

echo "Integration tests created"

# Create demonstration program
cat > /home/bbrelin/nimcp/examples/middleware_demo.c << 'EOF'
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
EOF

echo "Middleware finalization complete!"

