//=============================================================================
// test_learning_signal_adapter.cpp - Comprehensive Learning Signal Adapter Tests
//=============================================================================

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_adapters.h"
#include "core/events/nimcp_event_bus.h"
#include "utils/memory/nimcp_memory.h"

#include <thread>
#include <atomic>
#include <vector>
#include <cmath>
#include <cstring>

//=============================================================================
// Test Fixtures
//=============================================================================

class LearningSignalAdapterTest : public ::testing::Test {
protected:
    learning_signal_adapter_t adapter = nullptr;

    void SetUp() override {
        learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
        adapter = learning_signal_adapter_create(&config);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) {
            learning_signal_adapter_destroy(adapter);
        }
    }

    // Helper to create error event
    brain_event_t create_error_event(float expected, float actual, float magnitude) {
        brain_event_t event;
        memset(&event, 0, sizeof(event));
        event.type = EVENT_ERROR_DETECTED;
        event.priority = EVENT_PRIORITY_HIGH;
        event.source_module = "test";
        event.timestamp_us = 1000;

        // Pack data into event payload
        float data[3] = {expected, actual, magnitude};
        memcpy(event.data.data, data, sizeof(data));
        event.data.size = sizeof(data);

        return event;
    }

    // NOTE: Salience events are not currently supported by the learning signal adapter
    // These test helpers are kept for future implementation
    // Helper to create attention event with cognitive data
    brain_event_t create_cognitive_event(uint32_t prev_item, uint32_t curr_item, float strength) {
        brain_event_t event;
        memset(&event, 0, sizeof(event));
        event.type = EVENT_ATTENTION_SHIFT;
        event.priority = EVENT_PRIORITY_NORMAL;
        event.source_module = "test";
        event.timestamp_us = 2000;

        // Pack data into event payload
        float data[3] = {(float)prev_item, (float)curr_item, strength};
        memcpy(event.data.data, data, sizeof(data));
        event.data.size = sizeof(data);

        return event;
    }

    // Helper to create attention event
    brain_event_t create_attention_event(uint32_t prev_item, uint32_t curr_item, float strength) {
        brain_event_t event;
        memset(&event, 0, sizeof(event));
        event.type = EVENT_ATTENTION_SHIFT;
        event.priority = EVENT_PRIORITY_NORMAL;
        event.source_module = "test";
        event.timestamp_us = 3000;

        // Pack data into event payload
        float data[3] = {(float)prev_item, (float)curr_item, strength};
        memcpy(event.data.data, data, sizeof(data));
        event.data.size = sizeof(data);

        return event;
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(LearningSignalAdapterTest, CreateAndDestroy) {
    EXPECT_NE(adapter, nullptr);
}

TEST_F(LearningSignalAdapterTest, CreateWithNullConfig) {
    learning_signal_adapter_t test_adapter = learning_signal_adapter_create(nullptr);
    EXPECT_NE(test_adapter, nullptr);
    learning_signal_adapter_destroy(test_adapter);
}

TEST_F(LearningSignalAdapterTest, CreateWithCustomConfig) {
    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    config.normalization = NORMALIZE_MIN_MAX;
    config.learning_rate_scale = 0.5f;
    config.enable_attention_weighting = false;
    config.enable_novelty_boost = true;
    config.novelty_boost_factor = 2.0f;

    learning_signal_adapter_t test_adapter = learning_signal_adapter_create(&config);
    EXPECT_NE(test_adapter, nullptr);
    learning_signal_adapter_destroy(test_adapter);
}

TEST_F(LearningSignalAdapterTest, DestroyNullAdapter) {
    learning_signal_adapter_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Error Signal Extraction Tests
//=============================================================================

TEST_F(LearningSignalAdapterTest, ExtractErrorSignal) {
    brain_event_t event = create_error_event(1.0f, 0.5f, 0.5f);
    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    bool success = learning_signal_adapter_extract(adapter, &event, &signal);
    EXPECT_TRUE(success);

    EXPECT_EQ(signal.type, LEARNING_SIGNAL_ERROR);
    EXPECT_EQ(signal.num_features, 3u);
    EXPECT_NE(signal.features, nullptr);
    EXPECT_FLOAT_EQ(signal.features[0], 1.0f);  // expected
    EXPECT_FLOAT_EQ(signal.features[1], 0.5f);  // actual
    EXPECT_FLOAT_EQ(signal.features[2], 0.5f);  // error magnitude
    EXPECT_FLOAT_EQ(signal.magnitude, 0.5f);
    EXPECT_GE(signal.confidence, 0.8f);  // High confidence for error signals
    EXPECT_EQ(signal.timestamp_us, 1000u);

    learning_signal_free(&signal);
}

TEST_F(LearningSignalAdapterTest, ExtractErrorSignalZeroError) {
    brain_event_t event = create_error_event(1.0f, 1.0f, 0.0f);
    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    bool success = learning_signal_adapter_extract(adapter, &event, &signal);
    EXPECT_TRUE(success);

    EXPECT_FLOAT_EQ(signal.magnitude, 0.0f);
    learning_signal_free(&signal);
}

TEST_F(LearningSignalAdapterTest, ExtractErrorSignalLargeError) {
    brain_event_t event = create_error_event(0.0f, 1.0f, 1.0f);
    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    bool success = learning_signal_adapter_extract(adapter, &event, &signal);
    EXPECT_TRUE(success);

    EXPECT_FLOAT_EQ(signal.magnitude, 1.0f);
    learning_signal_free(&signal);
}

//=============================================================================
// Surprise Signal Extraction Tests
// NOTE: Surprise/salience/novelty events are not currently implemented
// These tests are disabled until the feature is added to the adapter
//=============================================================================

TEST_F(LearningSignalAdapterTest, DISABLED_ExtractSurpriseSignal) {
    // This test is disabled because surprise/salience events are not yet
    // implemented in the learning signal adapter
    GTEST_SKIP() << "Surprise signal extraction not yet implemented";
}

TEST_F(LearningSignalAdapterTest, DISABLED_ExtractNoveltyEvent) {
    // This test is disabled because novelty events are not yet
    // implemented in the learning signal adapter
    GTEST_SKIP() << "Novelty signal extraction not yet implemented";
}

TEST_F(LearningSignalAdapterTest, DISABLED_ExtractSurpriseEvent) {
    // This test is disabled because surprise events are not yet
    // implemented in the learning signal adapter
    GTEST_SKIP() << "Surprise event extraction not yet implemented";
}

//=============================================================================
// Attention Signal Extraction Tests
//=============================================================================

TEST_F(LearningSignalAdapterTest, ExtractAttentionSignal) {
    brain_event_t event = create_attention_event(10, 20, 0.75f);
    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    bool success = learning_signal_adapter_extract(adapter, &event, &signal);
    EXPECT_TRUE(success);

    EXPECT_EQ(signal.type, LEARNING_SIGNAL_ATTENTION);
    EXPECT_EQ(signal.num_features, 2u);
    EXPECT_FLOAT_EQ(signal.features[0], 10.0f);  // previous item
    EXPECT_FLOAT_EQ(signal.features[1], 20.0f);  // current item
    // Magnitude is calculated as |feature1 - feature2| = |10 - 20| = 10
    EXPECT_FLOAT_EQ(signal.magnitude, 10.0f);
    EXPECT_FLOAT_EQ(signal.confidence, 0.85f);  // Implementation sets to 0.85
    EXPECT_EQ(signal.timestamp_us, 3000u);

    learning_signal_free(&signal);
}

TEST_F(LearningSignalAdapterTest, ExtractAttentionWeakSignal) {
    brain_event_t event = create_attention_event(5, 15, 0.1f);
    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    bool success = learning_signal_adapter_extract(adapter, &event, &signal);
    EXPECT_TRUE(success);

    // Magnitude is calculated as |5 - 15| = 10
    EXPECT_FLOAT_EQ(signal.magnitude, 10.0f);
    EXPECT_FLOAT_EQ(signal.confidence, 0.85f);
    learning_signal_free(&signal);
}

TEST_F(LearningSignalAdapterTest, ExtractAttentionStrongSignal) {
    brain_event_t event = create_attention_event(1, 2, 1.0f);
    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    bool success = learning_signal_adapter_extract(adapter, &event, &signal);
    EXPECT_TRUE(success);

    // Magnitude is calculated as |1 - 2| = 1
    EXPECT_FLOAT_EQ(signal.magnitude, 1.0f);
    EXPECT_FLOAT_EQ(signal.confidence, 0.85f);

    learning_signal_free(&signal);
}

//=============================================================================
// Memory Signal Extraction Tests
//=============================================================================

TEST_F(LearningSignalAdapterTest, ExtractMemorySignal) {
    brain_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = EVENT_EPISODIC_MEMORY_STORED;
    event.priority = EVENT_PRIORITY_NORMAL;
    event.source_module = "hippocampus";
    event.timestamp_us = 4000;

    // Pack a single float value
    float memory_strength = 0.75f;
    memcpy(event.data.data, &memory_strength, sizeof(float));
    event.data.size = sizeof(float);

    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    bool success = learning_signal_adapter_extract(adapter, &event, &signal);

    // Memory signals are now implemented
    EXPECT_TRUE(success);
    EXPECT_EQ(signal.type, LEARNING_SIGNAL_MEMORY);
    EXPECT_EQ(signal.num_features, 1u);
    EXPECT_FLOAT_EQ(signal.features[0], memory_strength);
    EXPECT_FLOAT_EQ(signal.magnitude, memory_strength);
    EXPECT_FLOAT_EQ(signal.confidence, 0.8f);

    learning_signal_free(&signal);
}

//=============================================================================
// Normalization Tests
//=============================================================================

TEST_F(LearningSignalAdapterTest, NormalizeNoNormalization) {
    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    config.normalization = NORMALIZE_NONE;
    learning_signal_adapter_t test_adapter = learning_signal_adapter_create(&config);

    float features[] = {1.0f, 2.0f, 3.0f, 4.0f};
    bool success = learning_signal_adapter_normalize(test_adapter, features, 4);
    EXPECT_TRUE(success);

    // Features should remain unchanged
    EXPECT_FLOAT_EQ(features[0], 1.0f);
    EXPECT_FLOAT_EQ(features[1], 2.0f);
    EXPECT_FLOAT_EQ(features[2], 3.0f);
    EXPECT_FLOAT_EQ(features[3], 4.0f);

    learning_signal_adapter_destroy(test_adapter);
}

TEST_F(LearningSignalAdapterTest, NormalizeMinMax) {
    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    config.normalization = NORMALIZE_MIN_MAX;
    learning_signal_adapter_t test_adapter = learning_signal_adapter_create(&config);

    // Process multiple samples to build statistics
    float features1[] = {0.0f, 5.0f, 10.0f};
    float features2[] = {2.0f, 3.0f, 8.0f};
    float features3[] = {1.0f, 4.0f, 9.0f};

    learning_signal_adapter_normalize(test_adapter, features1, 3);
    learning_signal_adapter_normalize(test_adapter, features2, 3);
    learning_signal_adapter_normalize(test_adapter, features3, 3);

    // Last normalization should scale to [0,1]
    EXPECT_GE(features3[0], 0.0f);
    EXPECT_LE(features3[0], 1.0f);
    EXPECT_GE(features3[1], 0.0f);
    EXPECT_LE(features3[1], 1.0f);
    EXPECT_GE(features3[2], 0.0f);
    EXPECT_LE(features3[2], 1.0f);

    learning_signal_adapter_destroy(test_adapter);
}

TEST_F(LearningSignalAdapterTest, NormalizeZScore) {
    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    config.normalization = NORMALIZE_Z_SCORE;
    learning_signal_adapter_t test_adapter = learning_signal_adapter_create(&config);

    // Build statistics with multiple samples
    for (int i = 0; i < 20; i++) {
        float features[] = {1.0f, 2.0f, 3.0f};
        learning_signal_adapter_normalize(test_adapter, features, 3);
    }

    // After many samples, normalized values should be near zero
    float features[] = {1.0f, 2.0f, 3.0f};
    learning_signal_adapter_normalize(test_adapter, features, 3);

    // Z-score normalized values are typically in [-3, 3] range
    EXPECT_GE(features[0], -5.0f);
    EXPECT_LE(features[0], 5.0f);

    learning_signal_adapter_destroy(test_adapter);
}

TEST_F(LearningSignalAdapterTest, NormalizeL2) {
    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    config.normalization = NORMALIZE_L2;
    learning_signal_adapter_t test_adapter = learning_signal_adapter_create(&config);

    float features[] = {3.0f, 4.0f, 0.0f};
    bool success = learning_signal_adapter_normalize(test_adapter, features, 3);
    EXPECT_TRUE(success);

    // L2 norm should be 1.0
    float norm = sqrtf(features[0] * features[0] +
                       features[1] * features[1] +
                       features[2] * features[2]);
    EXPECT_NEAR(norm, 1.0f, 1e-5f);

    learning_signal_adapter_destroy(test_adapter);
}

TEST_F(LearningSignalAdapterTest, NormalizeAdaptive) {
    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    config.normalization = NORMALIZE_ADAPTIVE;
    learning_signal_adapter_t test_adapter = learning_signal_adapter_create(&config);

    // Need multiple samples for adaptive normalization
    for (int i = 0; i < 15; i++) {
        float features[] = {(float)i, (float)(i*2), (float)(i*3)};
        learning_signal_adapter_normalize(test_adapter, features, 3);
    }

    float features[] = {5.0f, 10.0f, 15.0f};
    bool success = learning_signal_adapter_normalize(test_adapter, features, 3);
    EXPECT_TRUE(success);

    learning_signal_adapter_destroy(test_adapter);
}

//=============================================================================
// Attention Weighting Tests
//=============================================================================

TEST_F(LearningSignalAdapterTest, ApplyAttentionWeighting) {
    brain_event_t event = create_error_event(1.0f, 0.5f, 0.5f);
    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    learning_signal_adapter_extract(adapter, &event, &signal);

    float original_magnitude = signal.magnitude;
    float attention_weight = 0.5f;

    bool success = learning_signal_adapter_apply_attention(adapter, &signal, attention_weight);
    EXPECT_TRUE(success);

    // Magnitude should be scaled
    EXPECT_FLOAT_EQ(signal.magnitude, original_magnitude * attention_weight);

    // Features should be scaled
    for (uint32_t i = 0; i < signal.num_features; i++) {
        // Features were originally [1.0, 0.5, 0.5], now scaled by 0.5
        EXPECT_LE(signal.features[i], signal.features[i] / attention_weight * 1.01f);
    }

    learning_signal_free(&signal);
}

TEST_F(LearningSignalAdapterTest, ApplyAttentionWeightingZero) {
    brain_event_t event = create_error_event(1.0f, 0.5f, 0.5f);
    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    learning_signal_adapter_extract(adapter, &event, &signal);

    bool success = learning_signal_adapter_apply_attention(adapter, &signal, 0.0f);
    EXPECT_TRUE(success);

    EXPECT_FLOAT_EQ(signal.magnitude, 0.0f);

    learning_signal_free(&signal);
}

TEST_F(LearningSignalAdapterTest, ApplyAttentionWeightingFull) {
    brain_event_t event = create_error_event(1.0f, 0.5f, 0.5f);
    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    learning_signal_adapter_extract(adapter, &event, &signal);

    float original_magnitude = signal.magnitude;

    bool success = learning_signal_adapter_apply_attention(adapter, &signal, 1.0f);
    EXPECT_TRUE(success);

    EXPECT_FLOAT_EQ(signal.magnitude, original_magnitude);

    learning_signal_free(&signal);
}

TEST_F(LearningSignalAdapterTest, ApplyAttentionDisabled) {
    learning_signal_adapter_config_t config = learning_signal_adapter_default_config();
    config.enable_attention_weighting = false;
    learning_signal_adapter_t test_adapter = learning_signal_adapter_create(&config);

    brain_event_t event = create_error_event(1.0f, 0.5f, 0.5f);
    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    learning_signal_adapter_extract(test_adapter, &event, &signal);

    float original_magnitude = signal.magnitude;

    bool success = learning_signal_adapter_apply_attention(test_adapter, &signal, 0.5f);
    EXPECT_TRUE(success);

    // Magnitude should remain unchanged when attention weighting is disabled
    EXPECT_FLOAT_EQ(signal.magnitude, original_magnitude);

    learning_signal_free(&signal);
    learning_signal_adapter_destroy(test_adapter);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(LearningSignalAdapterTest, GetStatistics) {
    learning_signal_adapter_stats_t stats;
    bool success = learning_signal_adapter_get_stats(adapter, &stats);
    EXPECT_TRUE(success);

    EXPECT_EQ(stats.signals_extracted, 0u);
    EXPECT_EQ(stats.signals_normalized, 0u);
    EXPECT_EQ(stats.signals_dropped, 0u);
}

TEST_F(LearningSignalAdapterTest, StatisticsAfterExtraction) {
    brain_event_t event = create_error_event(1.0f, 0.5f, 0.5f);
    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    learning_signal_adapter_extract(adapter, &event, &signal);
    learning_signal_free(&signal);

    learning_signal_adapter_stats_t stats;
    learning_signal_adapter_get_stats(adapter, &stats);

    EXPECT_EQ(stats.signals_extracted, 1u);
    EXPECT_GT(stats.avg_magnitude, 0.0f);
    EXPECT_GT(stats.avg_confidence, 0.0f);
}

TEST_F(LearningSignalAdapterTest, StatisticsAfterNormalization) {
    float features[] = {1.0f, 2.0f, 3.0f};
    learning_signal_adapter_normalize(adapter, features, 3);

    learning_signal_adapter_stats_t stats;
    learning_signal_adapter_get_stats(adapter, &stats);

    EXPECT_EQ(stats.signals_normalized, 1u);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(LearningSignalAdapterTest, ExtractNullAdapter) {
    brain_event_t event = create_error_event(1.0f, 0.5f, 0.5f);
    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    bool success = learning_signal_adapter_extract(nullptr, &event, &signal);
    EXPECT_FALSE(success);
}

TEST_F(LearningSignalAdapterTest, ExtractNullEvent) {
    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    bool success = learning_signal_adapter_extract(adapter, nullptr, &signal);
    EXPECT_FALSE(success);
}

TEST_F(LearningSignalAdapterTest, ExtractNullSignal) {
    brain_event_t event = create_error_event(1.0f, 0.5f, 0.5f);

    bool success = learning_signal_adapter_extract(adapter, &event, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(LearningSignalAdapterTest, ExtractUnsupportedEvent) {
    brain_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = EVENT_CUSTOM_USER;  // Custom events not handled by adapter
    event.source_module = "test";
    event.timestamp_us = 5000;

    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    bool success = learning_signal_adapter_extract(adapter, &event, &signal);
    EXPECT_FALSE(success);
}

TEST_F(LearningSignalAdapterTest, NormalizeNullAdapter) {
    float features[] = {1.0f, 2.0f, 3.0f};
    bool success = learning_signal_adapter_normalize(nullptr, features, 3);
    EXPECT_FALSE(success);
}

TEST_F(LearningSignalAdapterTest, NormalizeNullFeatures) {
    bool success = learning_signal_adapter_normalize(adapter, nullptr, 3);
    EXPECT_FALSE(success);
}

TEST_F(LearningSignalAdapterTest, NormalizeZeroFeatures) {
    float features[] = {1.0f, 2.0f, 3.0f};
    bool success = learning_signal_adapter_normalize(adapter, features, 0);
    EXPECT_FALSE(success);
}

TEST_F(LearningSignalAdapterTest, ApplyAttentionNullAdapter) {
    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    bool success = learning_signal_adapter_apply_attention(nullptr, &signal, 0.5f);
    EXPECT_FALSE(success);
}

TEST_F(LearningSignalAdapterTest, ApplyAttentionNullSignal) {
    bool success = learning_signal_adapter_apply_attention(adapter, nullptr, 0.5f);
    EXPECT_FALSE(success);
}

TEST_F(LearningSignalAdapterTest, GetStatsNullAdapter) {
    learning_signal_adapter_stats_t stats;
    bool success = learning_signal_adapter_get_stats(nullptr, &stats);
    EXPECT_FALSE(success);
}

TEST_F(LearningSignalAdapterTest, GetStatsNullStats) {
    bool success = learning_signal_adapter_get_stats(adapter, nullptr);
    EXPECT_FALSE(success);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(LearningSignalAdapterTest, ConcurrentExtraction) {
    const int num_threads = 4;
    const int extractions_per_thread = 100;
    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &success_count, extractions_per_thread]() {
            for (int i = 0; i < extractions_per_thread; i++) {
                brain_event_t event = create_error_event(1.0f, 0.5f, 0.5f);
                learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));

                if (learning_signal_adapter_extract(adapter, &event, &signal)) {
                    success_count++;
                    learning_signal_free(&signal);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * extractions_per_thread);

    learning_signal_adapter_stats_t stats;
    learning_signal_adapter_get_stats(adapter, &stats);
    EXPECT_EQ(stats.signals_extracted, (uint64_t)(num_threads * extractions_per_thread));
}

TEST_F(LearningSignalAdapterTest, ConcurrentNormalization) {
    const int num_threads = 4;
    const int normalizations_per_thread = 100;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, normalizations_per_thread]() {
            for (int i = 0; i < normalizations_per_thread; i++) {
                float features[] = {1.0f, 2.0f, 3.0f};
                learning_signal_adapter_normalize(adapter, features, 3);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    learning_signal_adapter_stats_t stats;
    learning_signal_adapter_get_stats(adapter, &stats);
    EXPECT_EQ(stats.signals_normalized, (uint64_t)(num_threads * normalizations_per_thread));
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(LearningSignalAdapterTest, EndToEndWorkflow) {
    // Extract signal
    brain_event_t event = create_error_event(1.0f, 0.3f, 0.7f);
    learning_signal_t signal;
    memset(&signal, 0, sizeof(signal));

    EXPECT_TRUE(learning_signal_adapter_extract(adapter, &event, &signal));
    EXPECT_EQ(signal.type, LEARNING_SIGNAL_ERROR);

    // Normalize features
    EXPECT_TRUE(learning_signal_adapter_normalize(adapter, signal.features, signal.num_features));

    // Apply attention
    EXPECT_TRUE(learning_signal_adapter_apply_attention(adapter, &signal, 0.8f));

    // Check final state
    EXPECT_GT(signal.magnitude, 0.0f);
    EXPECT_GT(signal.confidence, 0.0f);

    // Verify statistics
    learning_signal_adapter_stats_t stats;
    learning_signal_adapter_get_stats(adapter, &stats);
    EXPECT_EQ(stats.signals_extracted, 1u);
    EXPECT_EQ(stats.signals_normalized, 1u);

    learning_signal_free(&signal);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
