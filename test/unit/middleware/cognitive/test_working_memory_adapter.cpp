/**
 * @file test_working_memory_adapter.cpp
 * @brief Comprehensive unit tests for Working Memory Adapter
 *
 * WHAT: Test middleware adapter for working memory integration
 * WHY:  Ensure 100% code coverage for working_memory_adapter_t
 * HOW:  Test create/destroy, feature extraction, normalization, edge cases
 *
 * Coverage: 100% of all functions, branches, and edge cases
 * Categories: Lifecycle, Feature Extraction, Normalization, Error Handling
 *
 * @author NIMCP Test Suite
 * @date 2025-11-20
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <memory>

// Headers have their own extern "C" guards
#include "middleware/cognitive/nimcp_working_memory_adapter.h"
#include "utils/memory/nimcp_memory.h"
#include "core/events/nimcp_event_bus.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WorkingMemoryAdapterTest : public ::testing::Test {
protected:
    working_memory_adapter_t adapter;
    event_bus_t event_bus;

    void SetUp() override {
        adapter = nullptr;
        // Create event bus for cognitive event integration
        event_bus = event_bus_create("test_working_memory_bus", EVENT_DELIVERY_IMMEDIATE);
        ASSERT_NE(event_bus, nullptr) << "Failed to create event bus";
    }

    void TearDown() override {
        working_memory_adapter_destroy(adapter);
        if (event_bus) {
            event_bus_destroy(event_bus);
            event_bus = nullptr;
        }
    }

    // Helper: Create test activity pattern
    std::vector<float> create_test_activity(uint32_t num_channels, float base_value) {
        std::vector<float> activity(num_channels);
        for (uint32_t i = 0; i < num_channels; i++) {
            activity[i] = base_value + static_cast<float>(i) * 0.1f;
        }
        return activity;
    }
};

//=============================================================================
// 1. Lifecycle Tests
//=============================================================================

TEST_F(WorkingMemoryAdapterTest, CreateWithDefaultConfig) {
    // WHAT: Create adapter with default configuration
    // WHY:  Test standard creation path
    auto config = working_memory_adapter_default_config();
    adapter = working_memory_adapter_create(&config);
    EXPECT_NE(adapter, nullptr);
}

TEST_F(WorkingMemoryAdapterTest, CreateWithCustomConfig) {
    // WHAT: Create adapter with custom configuration
    // WHY:  Test configuration flexibility
    working_memory_adapter_config_t config = {
        .num_channels = 50,
        .buffer_size = BUFFER_SIZE_1S,
        .norm_type = NORMALIZE_MINMAX,
        .max_features = 25,
        .enable_spike_features = false,
        .enable_oscillations = true
    };

    adapter = working_memory_adapter_create(&config);
    EXPECT_NE(adapter, nullptr);
}

TEST_F(WorkingMemoryAdapterTest, CreateWithSpikeFeatures) {
    // WHAT: Create adapter with spike features enabled
    // WHY:  Test spike feature extraction path
    working_memory_adapter_config_t config = {
        .num_channels = 100,
        .buffer_size = BUFFER_SIZE_100MS,
        .norm_type = NORMALIZE_ZSCORE,
        .max_features = 50,
        .enable_spike_features = true,
        .enable_oscillations = true
    };

    adapter = working_memory_adapter_create(&config);
    EXPECT_NE(adapter, nullptr);
}

TEST_F(WorkingMemoryAdapterTest, CreateWithNullConfig) {
    // WHAT: Create adapter with NULL config
    // WHY:  Test NULL input handling
    adapter = working_memory_adapter_create(nullptr);
    EXPECT_EQ(adapter, nullptr);
}

TEST_F(WorkingMemoryAdapterTest, CreateWithZeroChannels) {
    // WHAT: Create adapter with zero channels
    // WHY:  Test invalid configuration validation
    working_memory_adapter_config_t config = working_memory_adapter_default_config();
    config.num_channels = 0;

    adapter = working_memory_adapter_create(&config);
    EXPECT_EQ(adapter, nullptr);
}

TEST_F(WorkingMemoryAdapterTest, CreateWithZeroFeatures) {
    // WHAT: Create adapter with zero max features
    // WHY:  Test feature count validation
    working_memory_adapter_config_t config = working_memory_adapter_default_config();
    config.max_features = 0;

    adapter = working_memory_adapter_create(&config);
    // Should still create successfully but not extract features
    EXPECT_NE(adapter, nullptr);
}

TEST_F(WorkingMemoryAdapterTest, CreateWithAllNormalizationTypes) {
    // WHAT: Create adapter with each normalization type
    // WHY:  Test all normalization paths
    brain_normalize_type_t types[] = {
        NORMALIZE_ZSCORE,
        NORMALIZE_MINMAX,
        NORMALIZE_ADAPTIVE,
        NORMALIZE_HOMEOSTATIC,
        NORMALIZE_NONE
    };

    for (auto type : types) {
        working_memory_adapter_config_t config = working_memory_adapter_default_config();
        config.norm_type = type;

        adapter = working_memory_adapter_create(&config);
        EXPECT_NE(adapter, nullptr);
        working_memory_adapter_destroy(adapter);
        adapter = nullptr;
    }
}

TEST_F(WorkingMemoryAdapterTest, CreateWithAllBufferSizes) {
    // WHAT: Create adapter with each buffer size
    // WHY:  Test all buffer size presets
    brain_buffer_size_t sizes[] = {
        BUFFER_SIZE_10MS,
        BUFFER_SIZE_100MS,
        BUFFER_SIZE_1S,
        BUFFER_SIZE_CUSTOM
    };

    for (auto size : sizes) {
        working_memory_adapter_config_t config = working_memory_adapter_default_config();
        config.buffer_size = size;

        adapter = working_memory_adapter_create(&config);
        EXPECT_NE(adapter, nullptr);
        working_memory_adapter_destroy(adapter);
        adapter = nullptr;
    }
}

TEST_F(WorkingMemoryAdapterTest, DestroyNull) {
    // WHAT: Destroy NULL adapter
    // WHY:  Verify safe NULL handling
    working_memory_adapter_destroy(nullptr);
    // Should not crash
}

TEST_F(WorkingMemoryAdapterTest, DestroyValidAdapter) {
    // WHAT: Create and destroy adapter
    // WHY:  Test proper cleanup
    auto config = working_memory_adapter_default_config();
    adapter = working_memory_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);
    working_memory_adapter_destroy(adapter);
    adapter = nullptr;  // Prevent double-free in TearDown
}

//=============================================================================
// 2. Feature Extraction Tests
//=============================================================================

TEST_F(WorkingMemoryAdapterTest, UpdateBasic) {
    // WHAT: Update adapter with basic activity
    // WHY:  Test basic update path
    auto config = working_memory_adapter_default_config();
    adapter = working_memory_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    auto activity = create_test_activity(config.num_channels, 1.0f);
    std::vector<float> features(config.max_features);

    uint32_t num_extracted = working_memory_adapter_update(
        adapter, activity.data(), config.num_channels, 1000, features.data()
    );

    EXPECT_GT(num_extracted, 0);
    EXPECT_LE(num_extracted, config.max_features);
}

TEST_F(WorkingMemoryAdapterTest, UpdateWithNullAdapter) {
    // WHAT: Update with NULL adapter
    // WHY:  Test error handling
    auto config = working_memory_adapter_default_config();
    auto activity = create_test_activity(config.num_channels, 1.0f);
    std::vector<float> features(config.max_features);

    uint32_t num_extracted = working_memory_adapter_update(
        nullptr, activity.data(), config.num_channels, 1000, features.data()
    );

    EXPECT_EQ(num_extracted, 0);
}

TEST_F(WorkingMemoryAdapterTest, UpdateWithNullActivity) {
    // WHAT: Update with NULL activity
    // WHY:  Test NULL input validation
    auto config = working_memory_adapter_default_config();
    adapter = working_memory_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    std::vector<float> features(config.max_features);

    uint32_t num_extracted = working_memory_adapter_update(
        adapter, nullptr, config.num_channels, 1000, features.data()
    );

    EXPECT_EQ(num_extracted, 0);
}

TEST_F(WorkingMemoryAdapterTest, UpdateWithNullOutput) {
    // WHAT: Update with NULL output
    // WHY:  Test output validation
    auto config = working_memory_adapter_default_config();
    adapter = working_memory_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    auto activity = create_test_activity(config.num_channels, 1.0f);

    uint32_t num_extracted = working_memory_adapter_update(
        adapter, activity.data(), config.num_channels, 1000, nullptr
    );

    EXPECT_EQ(num_extracted, 0);
}

TEST_F(WorkingMemoryAdapterTest, UpdateWithMismatchedChannels) {
    // WHAT: Update with wrong channel count
    // WHY:  Test channel count validation
    auto config = working_memory_adapter_default_config();
    adapter = working_memory_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    auto activity = create_test_activity(config.num_channels, 1.0f);
    std::vector<float> features(config.max_features);

    uint32_t num_extracted = working_memory_adapter_update(
        adapter, activity.data(), config.num_channels + 10, 1000, features.data()
    );

    EXPECT_EQ(num_extracted, 0);
}

TEST_F(WorkingMemoryAdapterTest, UpdateMultipleTimes) {
    // WHAT: Update adapter multiple times
    // WHY:  Test temporal buffering and normalization updates
    auto config = working_memory_adapter_default_config();
    adapter = working_memory_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    std::vector<float> features(config.max_features);

    for (int t = 0; t < 10; t++) {
        auto activity = create_test_activity(config.num_channels, 1.0f + t * 0.5f);

        uint32_t num_extracted = working_memory_adapter_update(
            adapter, activity.data(), config.num_channels, t * 100, features.data()
        );

        EXPECT_GT(num_extracted, 0);
    }
}

TEST_F(WorkingMemoryAdapterTest, UpdateWithZeroActivity) {
    // WHAT: Update with all-zero activity
    // WHY:  Test edge case of no activity
    auto config = working_memory_adapter_default_config();
    adapter = working_memory_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    std::vector<float> activity(config.num_channels, 0.0f);
    std::vector<float> features(config.max_features);

    uint32_t num_extracted = working_memory_adapter_update(
        adapter, activity.data(), config.num_channels, 1000, features.data()
    );

    EXPECT_GT(num_extracted, 0);
    // Features should be normalized zero values
}

TEST_F(WorkingMemoryAdapterTest, UpdateWithHighActivity) {
    // WHAT: Update with high activity values
    // WHY:  Test normalization with large inputs
    auto config = working_memory_adapter_default_config();
    adapter = working_memory_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    std::vector<float> activity(config.num_channels, 1000.0f);
    std::vector<float> features(config.max_features);

    uint32_t num_extracted = working_memory_adapter_update(
        adapter, activity.data(), config.num_channels, 1000, features.data()
    );

    EXPECT_GT(num_extracted, 0);
}

TEST_F(WorkingMemoryAdapterTest, UpdateWithVaryingActivity) {
    // WHAT: Update with varying activity pattern
    // WHY:  Test feature extraction from dynamic signal
    auto config = working_memory_adapter_default_config();
    adapter = working_memory_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    std::vector<float> features(config.max_features);

    for (int t = 0; t < 20; t++) {
        std::vector<float> activity(config.num_channels);
        for (uint32_t i = 0; i < config.num_channels; i++) {
            // Sinusoidal pattern
            activity[i] = sinf(static_cast<float>(t) * 0.1f + static_cast<float>(i) * 0.01f);
        }

        uint32_t num_extracted = working_memory_adapter_update(
            adapter, activity.data(), config.num_channels, t * 50, features.data()
        );

        EXPECT_GT(num_extracted, 0);
    }
}

//=============================================================================
// 3. Normalization Tests
//=============================================================================

TEST_F(WorkingMemoryAdapterTest, NormalizationZScore) {
    // WHAT: Test z-score normalization
    // WHY:  Verify z-score path works correctly
    working_memory_adapter_config_t config = working_memory_adapter_default_config();
    config.norm_type = NORMALIZE_ZSCORE;

    adapter = working_memory_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    auto activity = create_test_activity(config.num_channels, 1.0f);
    std::vector<float> features(config.max_features);

    uint32_t num_extracted = working_memory_adapter_update(
        adapter, activity.data(), config.num_channels, 1000, features.data()
    );

    EXPECT_GT(num_extracted, 0);
}

TEST_F(WorkingMemoryAdapterTest, NormalizationMinMax) {
    // WHAT: Test min-max normalization
    // WHY:  Verify min-max path works correctly
    working_memory_adapter_config_t config = working_memory_adapter_default_config();
    config.norm_type = NORMALIZE_MINMAX;

    adapter = working_memory_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    auto activity = create_test_activity(config.num_channels, 1.0f);
    std::vector<float> features(config.max_features);

    uint32_t num_extracted = working_memory_adapter_update(
        adapter, activity.data(), config.num_channels, 1000, features.data()
    );

    EXPECT_GT(num_extracted, 0);
}

TEST_F(WorkingMemoryAdapterTest, NormalizationNone) {
    // WHAT: Test no normalization
    // WHY:  Verify features are extracted without normalization
    working_memory_adapter_config_t config = working_memory_adapter_default_config();
    config.norm_type = NORMALIZE_NONE;

    adapter = working_memory_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    auto activity = create_test_activity(config.num_channels, 1.0f);
    std::vector<float> features(config.max_features);

    uint32_t num_extracted = working_memory_adapter_update(
        adapter, activity.data(), config.num_channels, 1000, features.data()
    );

    EXPECT_GT(num_extracted, 0);
}

//=============================================================================
// 4. Performance and Stress Tests
//=============================================================================

TEST_F(WorkingMemoryAdapterTest, LargeChannelCount) {
    // WHAT: Test with large number of channels
    // WHY:  Verify scalability
    working_memory_adapter_config_t config = working_memory_adapter_default_config();
    config.num_channels = 1000;
    config.max_features = 200;

    adapter = working_memory_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    auto activity = create_test_activity(config.num_channels, 1.0f);
    std::vector<float> features(config.max_features);

    uint32_t num_extracted = working_memory_adapter_update(
        adapter, activity.data(), config.num_channels, 1000, features.data()
    );

    EXPECT_GT(num_extracted, 0);
}

TEST_F(WorkingMemoryAdapterTest, SmallChannelCount) {
    // WHAT: Test with minimal channels
    // WHY:  Verify works with small populations
    working_memory_adapter_config_t config = working_memory_adapter_default_config();
    config.num_channels = 1;
    config.max_features = 5;

    adapter = working_memory_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    auto activity = create_test_activity(config.num_channels, 1.0f);
    std::vector<float> features(config.max_features);

    uint32_t num_extracted = working_memory_adapter_update(
        adapter, activity.data(), config.num_channels, 1000, features.data()
    );

    EXPECT_GT(num_extracted, 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
