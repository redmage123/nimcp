#include <gtest/gtest.h>
#include <cmath>
// Headers have their own extern "C" guards
#include "middleware/nimcp_middleware.h"

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
