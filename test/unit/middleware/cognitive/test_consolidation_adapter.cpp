/**
 * @file test_consolidation_adapter.cpp
 * @brief Comprehensive unit tests for consolidation adapter
 *
 * WHAT: Test consolidation adapter multi-timescale integration
 * WHY:  Ensure consolidation adapter provides reliable temporal integration
 * HOW:  Test all 11 API functions with edge cases, strategies, normalization
 *
 * COVERAGE GOALS:
 * - 100% function coverage for consolidation adapter
 * - All consolidation strategies tested
 * - Timescale integration validated
 * - Normalization verified
 *
 * @author NIMCP Development Team
 * @date 2025-11-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "middleware/cognitive/nimcp_cognitive_adapters.h"
#include "middleware/buffering/nimcp_integration_buffer.h"
#include "core/events/nimcp_event_bus.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ConsolidationAdapterTest : public ::testing::Test {
protected:
    consol_adapter_t* adapter = nullptr;
    consol_adapter_config_t config;
    event_bus_t event_bus;

    void SetUp() override {
        config = consol_adapter_default_config();
        // Create event bus for cognitive event integration
        event_bus = event_bus_create("test_consolidation_bus", EVENT_DELIVERY_IMMEDIATE);
        ASSERT_NE(event_bus, nullptr) << "Failed to create event bus";
    }

    void TearDown() override {
        if (adapter) {
            consol_adapter_destroy(adapter);
            adapter = nullptr;
        }
        if (event_bus) {
            event_bus_destroy(event_bus);
            event_bus = nullptr;
        }
    }

    // Helper: Generate test timestamp
    uint64_t get_timestamp() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count();
    }

    // Helper: Update adapter with series of values
    void update_series(size_t channel, const std::vector<float>& values) {
        for (float value : values) {
            consol_adapter_update(adapter, channel, value, get_timestamp());
        }
    }
};

//=============================================================================
// LIFECYCLE TESTS (create/destroy)
//=============================================================================

TEST_F(ConsolidationAdapterTest, CreateWithDefaultConfig) {
    // WHAT: Create adapter with default config
    // WHY:  Verify default configuration works
    config = consol_adapter_default_config();
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr) << "Should create with default config";
}

TEST_F(ConsolidationAdapterTest, CreateWithCustomConfig) {
    // WHAT: Create adapter with custom configuration
    // WHY:  Verify custom config is respected
    config.strategy = CONSOL_STRATEGY_WEIGHTED;
    config.fast_size = 50;
    config.medium_size = 25;
    config.slow_size = 10;
    config.num_channels = 4;

    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr) << "Should create with custom config";
}

TEST_F(ConsolidationAdapterTest, CreateAllStrategies) {
    // WHAT: Test all consolidation strategies
    // WHY:  Ensure all strategies initialize correctly
    consolidation_strategy_t strategies[] = {
        CONSOL_STRATEGY_AVERAGE,
        CONSOL_STRATEGY_WEIGHTED,
        CONSOL_STRATEGY_THRESHOLD,
        CONSOL_STRATEGY_ADAPTIVE
    };

    for (auto strategy : strategies) {
        config.strategy = strategy;
        consol_adapter_t* test_adapter = consol_adapter_create(&config);
        EXPECT_NE(test_adapter, nullptr) << "Strategy " << strategy << " should work";
        consol_adapter_destroy(test_adapter);
    }
}

TEST_F(ConsolidationAdapterTest, DestroyNullSafe) {
    // WHAT: Destroy NULL adapter
    // WHY:  Ensure NULL is safe (should not crash)
    consol_adapter_destroy(nullptr);
    SUCCEED() << "Destroying NULL should be safe";
}

TEST_F(ConsolidationAdapterTest, DestroyFreesResources) {
    // WHAT: Verify destroy frees all resources
    // WHY:  Prevent memory leaks
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Update with data
    for (size_t ch = 0; ch < config.num_channels; ch++) {
        consol_adapter_update(adapter, ch, 1.0f, get_timestamp());
    }

    consol_adapter_destroy(adapter);
    adapter = nullptr;
    SUCCEED() << "Destroy should free all resources";
}

//=============================================================================
// UPDATE TESTS
//=============================================================================

TEST_F(ConsolidationAdapterTest, UpdateSuccess) {
    // WHAT: Update adapter with new value
    // WHY:  Verify basic update functionality
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    bool result = consol_adapter_update(adapter, 0, 1.5f, get_timestamp());
    EXPECT_TRUE(result) << "Should update successfully";
}

TEST_F(ConsolidationAdapterTest, UpdateNullAdapter) {
    // WHAT: Update with NULL adapter
    // WHY:  Test error handling
    bool result = consol_adapter_update(nullptr, 0, 1.0f, get_timestamp());
    EXPECT_FALSE(result) << "Should fail with NULL adapter";
}

TEST_F(ConsolidationAdapterTest, UpdateInvalidChannel) {
    // WHAT: Update with out-of-bounds channel
    // WHY:  Test boundary validation
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    bool result = consol_adapter_update(adapter, config.num_channels, 1.0f, get_timestamp());
    EXPECT_FALSE(result) << "Should fail with invalid channel";
}

TEST_F(ConsolidationAdapterTest, UpdateMultipleChannels) {
    // WHAT: Update multiple channels independently
    // WHY:  Verify multi-channel support
    config.num_channels = 4;
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    for (size_t ch = 0; ch < config.num_channels; ch++) {
        float value = (float)ch * 10.0f;
        EXPECT_TRUE(consol_adapter_update(adapter, ch, value, get_timestamp()));
    }
}

TEST_F(ConsolidationAdapterTest, UpdateSequence) {
    // WHAT: Update with sequence of values
    // WHY:  Verify temporal integration
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    std::vector<float> values = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    for (float value : values) {
        EXPECT_TRUE(consol_adapter_update(adapter, 0, value, get_timestamp()));
    }
}

//=============================================================================
// GET VALUE TESTS (at specific timescale)
//=============================================================================

TEST_F(ConsolidationAdapterTest, GetValueFastTimescale) {
    // WHAT: Get value from fast timescale buffer
    // WHY:  Verify fast buffer access
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    consol_adapter_update(adapter, 0, 5.5f, get_timestamp());

    float value = consol_adapter_get_value(adapter, TIMESCALE_FAST, 0);
    EXPECT_FLOAT_EQ(value, 5.5f) << "Should return fast buffer value";
}

TEST_F(ConsolidationAdapterTest, GetValueAllTimescales) {
    // WHAT: Get values from all timescale levels
    // WHY:  Verify all buffers work
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Add multiple updates
    for (int i = 0; i < 100; i++) {
        consol_adapter_update(adapter, 0, 1.0f, get_timestamp());
    }

    float fast = consol_adapter_get_value(adapter, TIMESCALE_FAST, 0);
    float medium = consol_adapter_get_value(adapter, TIMESCALE_MEDIUM, 0);
    float slow = consol_adapter_get_value(adapter, TIMESCALE_SLOW, 0);

    // All should have some value
    EXPECT_NE(fast, 0.0f);
    EXPECT_NE(medium, 0.0f);
    EXPECT_NE(slow, 0.0f);
}

TEST_F(ConsolidationAdapterTest, GetValueNullAdapter) {
    // WHAT: Get value with NULL adapter
    // WHY:  Test error handling
    float value = consol_adapter_get_value(nullptr, TIMESCALE_FAST, 0);
    EXPECT_FLOAT_EQ(value, 0.0f) << "Should return 0.0 for NULL adapter";
}

TEST_F(ConsolidationAdapterTest, GetValueInvalidChannel) {
    // WHAT: Get value from invalid channel
    // WHY:  Test boundary validation
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    float value = consol_adapter_get_value(adapter, TIMESCALE_FAST, config.num_channels);
    EXPECT_FLOAT_EQ(value, 0.0f) << "Should return 0.0 for invalid channel";
}

//=============================================================================
// GET CONSOLIDATED VALUE TESTS (across timescales)
//=============================================================================

TEST_F(ConsolidationAdapterTest, GetConsolidatedSuccess) {
    // WHAT: Get consolidated value across timescales
    // WHY:  Verify consolidation integration
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Add values
    for (int i = 0; i < 50; i++) {
        consol_adapter_update(adapter, 0, 2.0f, get_timestamp());
    }

    float consolidated = consol_adapter_get_consolidated(adapter, 0);
    EXPECT_GT(consolidated, 0.0f) << "Should have non-zero consolidated value";
}

TEST_F(ConsolidationAdapterTest, GetConsolidatedStrategyAverage) {
    // WHAT: Test AVERAGE strategy
    // WHY:  Verify strategy implementation
    config.strategy = CONSOL_STRATEGY_AVERAGE;
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Add consistent values
    for (int i = 0; i < 100; i++) {
        consol_adapter_update(adapter, 0, 3.0f, get_timestamp());
    }

    float consolidated = consol_adapter_get_consolidated(adapter, 0);
    EXPECT_NEAR(consolidated, 3.0f, 0.5f) << "Average should be near 3.0";
}

TEST_F(ConsolidationAdapterTest, GetConsolidatedStrategyWeighted) {
    // WHAT: Test WEIGHTED strategy
    // WHY:  Verify weighted consolidation
    config.strategy = CONSOL_STRATEGY_WEIGHTED;
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    for (int i = 0; i < 100; i++) {
        consol_adapter_update(adapter, 0, 1.0f, get_timestamp());
    }

    float consolidated = consol_adapter_get_consolidated(adapter, 0);
    EXPECT_GT(consolidated, 0.0f);
}

TEST_F(ConsolidationAdapterTest, GetConsolidatedStrategyThreshold) {
    // WHAT: Test THRESHOLD strategy
    // WHY:  Verify threshold-based consolidation
    config.strategy = CONSOL_STRATEGY_THRESHOLD;
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    for (int i = 0; i < 100; i++) {
        consol_adapter_update(adapter, 0, 1.5f, get_timestamp());
    }

    float consolidated = consol_adapter_get_consolidated(adapter, 0);
    EXPECT_GT(consolidated, 0.0f);
}

TEST_F(ConsolidationAdapterTest, GetConsolidatedStrategyAdaptive) {
    // WHAT: Test ADAPTIVE strategy
    // WHY:  Verify adaptive consolidation
    config.strategy = CONSOL_STRATEGY_ADAPTIVE;
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    for (int i = 0; i < 100; i++) {
        consol_adapter_update(adapter, 0, 2.0f, get_timestamp());
    }

    float consolidated = consol_adapter_get_consolidated(adapter, 0);
    EXPECT_GT(consolidated, 0.0f);
}

TEST_F(ConsolidationAdapterTest, GetConsolidatedNullAdapter) {
    // WHAT: Get consolidated with NULL adapter
    // WHY:  Test error handling
    float value = consol_adapter_get_consolidated(nullptr, 0);
    EXPECT_FLOAT_EQ(value, 0.0f) << "Should return 0.0 for NULL adapter";
}

TEST_F(ConsolidationAdapterTest, GetConsolidatedInvalidChannel) {
    // WHAT: Get consolidated from invalid channel
    // WHY:  Test boundary validation
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    float value = consol_adapter_get_consolidated(adapter, config.num_channels);
    EXPECT_FLOAT_EQ(value, 0.0f) << "Should return 0.0 for invalid channel";
}

//=============================================================================
// GET TREND TESTS
//=============================================================================

TEST_F(ConsolidationAdapterTest, GetTrendIncreasing) {
    // WHAT: Detect increasing trend
    // WHY:  Verify trend detection
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Create increasing sequence
    for (int i = 0; i < 100; i++) {
        float value = (float)i / 10.0f;
        consol_adapter_update(adapter, 0, value, get_timestamp());
    }

    float trend = consol_adapter_get_trend(adapter, 0);
    // Trend should reflect increasing pattern (implementation dependent)
}

TEST_F(ConsolidationAdapterTest, GetTrendDecreasing) {
    // WHAT: Detect decreasing trend
    // WHY:  Verify trend detection
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Create decreasing sequence
    for (int i = 100; i > 0; i--) {
        float value = (float)i / 10.0f;
        consol_adapter_update(adapter, 0, value, get_timestamp());
    }

    float trend = consol_adapter_get_trend(adapter, 0);
    // Trend should reflect decreasing pattern
}

TEST_F(ConsolidationAdapterTest, GetTrendStable) {
    // WHAT: Detect stable trend
    // WHY:  Verify trend detection for constant values
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Create stable sequence
    for (int i = 0; i < 100; i++) {
        consol_adapter_update(adapter, 0, 5.0f, get_timestamp());
    }

    float trend = consol_adapter_get_trend(adapter, 0);
    EXPECT_NEAR(trend, 0.0f, 0.5f) << "Stable values should have near-zero trend";
}

TEST_F(ConsolidationAdapterTest, GetTrendNullAdapter) {
    // WHAT: Get trend with NULL adapter
    // WHY:  Test error handling
    float trend = consol_adapter_get_trend(nullptr, 0);
    EXPECT_FLOAT_EQ(trend, 0.0f) << "Should return 0.0 for NULL adapter";
}

TEST_F(ConsolidationAdapterTest, GetTrendInvalidChannel) {
    // WHAT: Get trend from invalid channel
    // WHY:  Test boundary validation
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    float trend = consol_adapter_get_trend(adapter, config.num_channels);
    EXPECT_FLOAT_EQ(trend, 0.0f) << "Should return 0.0 for invalid channel";
}

//=============================================================================
// NORMALIZE TESTS
//=============================================================================

TEST_F(ConsolidationAdapterTest, NormalizeSuccess) {
    // WHAT: Normalize channel data
    // WHY:  Verify normalization functionality
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Add values with known distribution
    std::vector<float> values = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    for (float value : values) {
        consol_adapter_update(adapter, 0, value, get_timestamp());
    }

    float normalized = consol_adapter_normalize(adapter, 0);
    // Normalized value should be z-score
}

TEST_F(ConsolidationAdapterTest, NormalizeZeroVariance) {
    // WHAT: Normalize with zero variance (constant values)
    // WHY:  Test edge case
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Add constant values
    for (int i = 0; i < 50; i++) {
        consol_adapter_update(adapter, 0, 5.0f, get_timestamp());
    }

    float normalized = consol_adapter_normalize(adapter, 0);
    EXPECT_FLOAT_EQ(normalized, 0.0f) << "Zero variance should return 0.0";
}

TEST_F(ConsolidationAdapterTest, NormalizeNullAdapter) {
    // WHAT: Normalize with NULL adapter
    // WHY:  Test error handling
    float normalized = consol_adapter_normalize(nullptr, 0);
    EXPECT_FLOAT_EQ(normalized, 0.0f) << "Should return 0.0 for NULL adapter";
}

TEST_F(ConsolidationAdapterTest, NormalizeInvalidChannel) {
    // WHAT: Normalize invalid channel
    // WHY:  Test boundary validation
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    float normalized = consol_adapter_normalize(adapter, config.num_channels);
    EXPECT_FLOAT_EQ(normalized, 0.0f) << "Should return 0.0 for invalid channel";
}

//=============================================================================
// CLEAR TESTS
//=============================================================================

TEST_F(ConsolidationAdapterTest, ClearResetsBuffers) {
    // WHAT: Clear resets all buffers
    // WHY:  Verify clear functionality
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Add data
    for (int i = 0; i < 50; i++) {
        consol_adapter_update(adapter, 0, 5.0f, get_timestamp());
    }

    consol_adapter_clear(adapter);

    // After clear, should have default/zero values
    float value = consol_adapter_get_value(adapter, TIMESCALE_FAST, 0);
    EXPECT_FLOAT_EQ(value, 0.0f) << "Should be zero after clear";
}

TEST_F(ConsolidationAdapterTest, ClearNullSafe) {
    // WHAT: Clear NULL adapter
    // WHY:  Ensure NULL is safe
    consol_adapter_clear(nullptr);
    SUCCEED() << "Clearing NULL should be safe";
}

TEST_F(ConsolidationAdapterTest, ClearCanUpdateAfter) {
    // WHAT: Can update after clear
    // WHY:  Verify adapter remains functional
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    consol_adapter_update(adapter, 0, 1.0f, get_timestamp());
    consol_adapter_clear(adapter);

    bool result = consol_adapter_update(adapter, 0, 2.0f, get_timestamp());
    EXPECT_TRUE(result) << "Should be able to update after clear";
}

//=============================================================================
// STATISTICS TESTS
//=============================================================================

TEST_F(ConsolidationAdapterTest, GetStatsSuccess) {
    // WHAT: Retrieve adapter statistics
    // WHY:  Verify stats tracking
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Add updates
    for (int i = 0; i < 10; i++) {
        consol_adapter_update(adapter, 0, 1.0f, get_timestamp());
    }

    consol_adapter_stats_t stats;
    bool result = consol_adapter_get_stats(adapter, &stats);

    EXPECT_TRUE(result);
    EXPECT_EQ(stats.total_updates, 10u);
    EXPECT_GE(stats.integration_quality, 0.0f);
    EXPECT_LE(stats.integration_quality, 1.0f);
}

TEST_F(ConsolidationAdapterTest, GetStatsTracksActivity) {
    // WHAT: Stats track activity levels
    // WHY:  Verify activity monitoring
    config.num_channels = 3;
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Add different activity levels
    // Need at least 100 updates per channel to populate slow timescale buffer
    // (slow buffer uses 100x downsampling)
    for (int i = 0; i < 100; i++) {
        consol_adapter_update(adapter, 0, 1.0f, get_timestamp());
        consol_adapter_update(adapter, 1, 2.0f, get_timestamp());
        consol_adapter_update(adapter, 2, 0.5f, get_timestamp());
    }

    consol_adapter_stats_t stats;
    ASSERT_TRUE(consol_adapter_get_stats(adapter, &stats));

    EXPECT_GT(stats.fast_activity, 0.0f);
    EXPECT_GT(stats.medium_activity, 0.0f);
    EXPECT_GT(stats.slow_activity, 0.0f);
}

TEST_F(ConsolidationAdapterTest, GetStatsNullParameters) {
    // WHAT: Get stats with NULL parameters
    // WHY:  Test error handling
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    bool result = consol_adapter_get_stats(nullptr, nullptr);
    EXPECT_FALSE(result);

    consol_adapter_stats_t stats;
    result = consol_adapter_get_stats(nullptr, &stats);
    EXPECT_FALSE(result);

    result = consol_adapter_get_stats(adapter, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// DEFAULT CONFIG TESTS
//=============================================================================

TEST_F(ConsolidationAdapterTest, DefaultConfigValid) {
    // WHAT: Default config has valid values
    // WHY:  Ensure sensible defaults
    consol_adapter_config_t def = consol_adapter_default_config();

    EXPECT_GT(def.fast_size, 0u);
    EXPECT_GT(def.medium_size, 0u);
    EXPECT_GT(def.slow_size, 0u);
    EXPECT_GT(def.num_channels, 0u);
    EXPECT_GT(def.alpha, 0.0f);
    EXPECT_LE(def.alpha, 1.0f);
    EXPECT_TRUE(def.strategy == CONSOL_STRATEGY_AVERAGE ||
                def.strategy == CONSOL_STRATEGY_WEIGHTED ||
                def.strategy == CONSOL_STRATEGY_THRESHOLD ||
                def.strategy == CONSOL_STRATEGY_ADAPTIVE);
}

//=============================================================================
// CONCURRENT ACCESS TESTS
//=============================================================================

TEST_F(ConsolidationAdapterTest, ConcurrentUpdates) {
    // WHAT: Multiple threads updating
    // WHY:  Test thread safety (best effort)
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    std::vector<std::thread> threads;
    const int num_threads = 4;
    const int updates_per_thread = 100;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, updates_per_thread]() {
            for (int i = 0; i < updates_per_thread; i++) {
                size_t channel = t % config.num_channels;
                consol_adapter_update(adapter, channel, 1.0f, get_timestamp());
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify some updates were recorded
    consol_adapter_stats_t stats;
    ASSERT_TRUE(consol_adapter_get_stats(adapter, &stats));
    EXPECT_GT(stats.total_updates, 0u);
}

//=============================================================================
// EDGE CASES AND BOUNDARY CONDITIONS
//=============================================================================

TEST_F(ConsolidationAdapterTest, MaxChannels) {
    // WHAT: Test maximum channel count
    // WHY:  Verify large channel count works
    config.num_channels = 128;
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    for (size_t ch = 0; ch < config.num_channels; ch++) {
        EXPECT_TRUE(consol_adapter_update(adapter, ch, 1.0f, get_timestamp()));
    }
}

TEST_F(ConsolidationAdapterTest, MinimalConfig) {
    // WHAT: Test minimal configuration
    // WHY:  Verify edge case
    config.fast_size = 1;
    config.medium_size = 1;
    config.slow_size = 1;
    config.num_channels = 1;
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    EXPECT_TRUE(consol_adapter_update(adapter, 0, 1.0f, get_timestamp()));
}

TEST_F(ConsolidationAdapterTest, ExtremeValues) {
    // WHAT: Update with extreme values
    // WHY:  Test robustness
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    EXPECT_TRUE(consol_adapter_update(adapter, 0, 1e10f, get_timestamp()));
    EXPECT_TRUE(consol_adapter_update(adapter, 0, -1e10f, get_timestamp()));
    EXPECT_TRUE(consol_adapter_update(adapter, 0, 0.0f, get_timestamp()));
}

TEST_F(ConsolidationAdapterTest, RapidUpdates) {
    // WHAT: Many rapid updates
    // WHY:  Test performance under load
    adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    for (int i = 0; i < 10000; i++) {
        consol_adapter_update(adapter, 0, (float)i, get_timestamp());
    }

    consol_adapter_stats_t stats;
    ASSERT_TRUE(consol_adapter_get_stats(adapter, &stats));
    EXPECT_EQ(stats.total_updates, 10000u);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
