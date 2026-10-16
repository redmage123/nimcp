//=============================================================================
// test_cognitive_adapters_regression.cpp - Cognitive Adapters Regression Tests
//
// Regression tests to ensure:
// - Backward compatibility of cognitive adapter APIs
// - Configuration parameter stability
// - Behavioral consistency across updates
// - Performance regression detection
// - Memory usage stability
//=============================================================================

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "middleware/cognitive/nimcp_cognitive_adapters.h"
#include "middleware/cognitive/nimcp_working_memory_adapter.h"
#include "middleware/cognitive/nimcp_consolidation_adapter.h"
#include "middleware/cognitive/nimcp_attention_adapter.h"

//=============================================================================
// REGRESSION TEST FIXTURE
//=============================================================================

class CognitiveAdaptersRegressionTest : public ::testing::Test {
protected:
    // Performance baselines (microseconds)
    static constexpr double MAX_WM_ADD_TIME_US = 50.0;
    static constexpr double MAX_CONSOL_UPDATE_TIME_US = 30.0;
    static constexpr double MAX_ATTENTION_SET_TIME_US = 20.0;

    // Behavioral baselines
    static constexpr uint32_t DEFAULT_WM_CAPACITY = 9;
    static constexpr float WM_SALIENCE_TOLERANCE = 0.01f;

    void SetUp() override {
        // Regression test setup
    }

    void TearDown() override {
        // Regression test cleanup
    }

    // Utility: Measure execution time in microseconds
    template<typename Func>
    double measureTimeUs(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start).count();
    }
};

//=============================================================================
// 1. BACKWARD COMPATIBILITY - WORKING MEMORY
//=============================================================================

TEST_F(CognitiveAdaptersRegressionTest, WorkingMemoryDefaultConfigUnchanged) {
    // WHAT: Verify default WM configuration values
    // WHY: Code relying on defaults must not break

    wm_adapter_config_t config = wm_adapter_default_config();

    EXPECT_EQ(config.capacity, DEFAULT_WM_CAPACITY);
    EXPECT_EQ(config.mode, WM_MODE_SLIDING);
    EXPECT_FALSE(config.enable_decay);
    EXPECT_GT(config.window_size, 0);
}

TEST_F(CognitiveAdaptersRegressionTest, WorkingMemoryAPISurfaceStable) {
    // WHAT: Verify WM API signatures unchanged
    // WHY: Binary compatibility and existing code

    wm_adapter_t* adapter = wm_adapter_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    // All original APIs must still exist
    float data[2] = {1.0f, 2.0f};
    EXPECT_TRUE(wm_adapter_add_item(adapter, 1, data, 2, 0.8f));
    EXPECT_NE(wm_adapter_get_item(adapter, 1), nullptr);
    EXPECT_TRUE(wm_adapter_remove_item(adapter, 1));

    wm_adapter_stats_t stats;
    EXPECT_TRUE(wm_adapter_get_stats(adapter, &stats));

    wm_adapter_destroy(adapter);
}

TEST_F(CognitiveAdaptersRegressionTest, WorkingMemoryCapacityBehavior) {
    // WHAT: Verify capacity enforcement consistency
    // WHY: WM must maintain 7±2 limit consistently

    wm_adapter_config_t config = wm_adapter_default_config();
    config.capacity = 5;

    wm_adapter_t* adapter = wm_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Add exactly capacity items
    for (uint32_t i = 0; i < 5; i++) {
        float data[2] = {static_cast<float>(i), static_cast<float>(i + 1)};
        EXPECT_TRUE(wm_adapter_add_item(adapter, i, data, 2, 0.7f));
    }

    wm_adapter_stats_t stats;
    EXPECT_TRUE(wm_adapter_get_stats(adapter, &stats));
    EXPECT_EQ(stats.current_items, 5);

    // Add one more - should evict
    float data[2] = {10.0f, 11.0f};
    EXPECT_TRUE(wm_adapter_add_item(adapter, 10, data, 2, 0.7f));

    EXPECT_TRUE(wm_adapter_get_stats(adapter, &stats));
    EXPECT_EQ(stats.current_items, 5);  // Still at capacity
    EXPECT_EQ(stats.total_evicted, 1);

    wm_adapter_destroy(adapter);
}

TEST_F(CognitiveAdaptersRegressionTest, WorkingMemorySalienceOrdering) {
    // WHAT: Verify salience-based eviction order
    // WHY: Eviction policy must be deterministic

    wm_adapter_config_t config = wm_adapter_default_config();
    config.capacity = 3;

    wm_adapter_t* adapter = wm_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Add items with known saliences
    float data[2] = {1.0f, 2.0f};
    EXPECT_TRUE(wm_adapter_add_item(adapter, 1, data, 2, 0.3f));  // Low
    EXPECT_TRUE(wm_adapter_add_item(adapter, 2, data, 2, 0.9f));  // High
    EXPECT_TRUE(wm_adapter_add_item(adapter, 3, data, 2, 0.6f));  // Medium

    // Add high salience item - should evict item 1
    EXPECT_TRUE(wm_adapter_add_item(adapter, 4, data, 2, 0.95f));

    // Item 1 should be evicted, others should remain
    EXPECT_EQ(wm_adapter_get_item(adapter, 1), nullptr);
    EXPECT_NE(wm_adapter_get_item(adapter, 2), nullptr);
    EXPECT_NE(wm_adapter_get_item(adapter, 3), nullptr);
    EXPECT_NE(wm_adapter_get_item(adapter, 4), nullptr);

    wm_adapter_destroy(adapter);
}

TEST_F(CognitiveAdaptersRegressionTest, WorkingMemoryDecayFormula) {
    // WHAT: Verify temporal decay formula unchanged
    // WHY: Decay behavior must be consistent

    wm_adapter_config_t config = wm_adapter_default_config();
    config.enable_decay = true;
    config.decay_tau_ms = 100.0f;

    wm_adapter_t* adapter = wm_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    float data[2] = {1.0f, 2.0f};
    EXPECT_TRUE(wm_adapter_add_item(adapter, 1, data, 2, 1.0f));

    const wm_item_t* item1 = wm_adapter_get_item(adapter, 1);
    float initial_salience = item1->salience;

    // Apply one tau worth of decay
    wm_adapter_update_decay(adapter, 100000);  // 100ms

    const wm_item_t* item2 = wm_adapter_get_item(adapter, 1);
    float decayed_salience = item2->salience;

    // Should decay to approximately 1/e ≈ 0.368
    float expected = initial_salience * std::exp(-1.0f);
    EXPECT_NEAR(decayed_salience, expected, 0.05f);

    wm_adapter_destroy(adapter);
}

TEST_F(CognitiveAdaptersRegressionTest, WorkingMemoryPerformance) {
    // WHAT: Verify WM operations meet performance baseline
    // WHY: No performance regressions

    wm_adapter_t* adapter = wm_adapter_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    // Measure add performance
    double add_time = measureTimeUs([&]() {
        for (int i = 0; i < 100; i++) {
            wm_adapter_add_item(adapter, i, data, 4, 0.7f);
        }
    });

    double avg_add_time = add_time / 100.0;
    EXPECT_LT(avg_add_time, MAX_WM_ADD_TIME_US);

    wm_adapter_destroy(adapter);
}

//=============================================================================
// 2. BACKWARD COMPATIBILITY - CONSOLIDATION
//=============================================================================

TEST_F(CognitiveAdaptersRegressionTest, ConsolidationDefaultConfigUnchanged) {
    // WHAT: Verify default consolidation configuration
    // WHY: Existing code relies on defaults

    consol_adapter_config_t config = consol_adapter_default_config();

    EXPECT_EQ(config.strategy, CONSOL_STRATEGY_AVERAGE);
    EXPECT_GT(config.fast_size, 0);
    EXPECT_GT(config.medium_size, config.fast_size);
    EXPECT_GT(config.slow_size, config.medium_size);
    EXPECT_FALSE(config.enable_normalization);
}

TEST_F(CognitiveAdaptersRegressionTest, ConsolidationAPISurfaceStable) {
    // WHAT: Verify consolidation API unchanged
    // WHY: Binary compatibility

    consol_adapter_config_t config = consol_adapter_default_config();
    config.num_channels = 4;

    consol_adapter_t* adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // All original APIs must exist
    EXPECT_TRUE(consol_adapter_update(adapter, 0, 1.0f, 0));
    EXPECT_FALSE(std::isnan(consol_adapter_get_value(adapter, TIMESCALE_FAST, 0)));
    EXPECT_FALSE(std::isnan(consol_adapter_get_consolidated(adapter, 0)));
    EXPECT_FALSE(std::isnan(consol_adapter_get_trend(adapter, 0)));

    consol_adapter_stats_t stats;
    EXPECT_TRUE(consol_adapter_get_stats(adapter, &stats));

    consol_adapter_destroy(adapter);
}

TEST_F(CognitiveAdaptersRegressionTest, ConsolidationTimescaleRelations) {
    // WHAT: Verify timescale hierarchy maintained
    // WHY: Fast < Medium < Slow relationship critical

    consol_adapter_config_t config = consol_adapter_default_config();
    config.num_channels = 1;
    config.fast_size = 10;
    config.medium_size = 100;
    config.slow_size = 1000;

    consol_adapter_t* adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Add step function
    uint64_t timestamp = 0;
    for (int t = 0; t < 200; t++) {
        float value = (t < 100) ? 0.0f : 1.0f;
        EXPECT_TRUE(consol_adapter_update(adapter, 0, value, timestamp));
        timestamp += 1000;
    }

    // Fast should react quickly, slow should lag
    float fast = consol_adapter_get_value(adapter, TIMESCALE_FAST, 0);
    float slow = consol_adapter_get_value(adapter, TIMESCALE_SLOW, 0);

    // After step change, fast should be closer to 1.0
    EXPECT_GT(fast, slow);

    consol_adapter_destroy(adapter);
}

TEST_F(CognitiveAdaptersRegressionTest, ConsolidationStrategyConsistency) {
    // WHAT: Verify consolidation strategies produce expected results
    // WHY: Strategy behavior must be stable

    consolidation_strategy_t strategies[] = {
        CONSOL_STRATEGY_AVERAGE,
        CONSOL_STRATEGY_WEIGHTED,
        CONSOL_STRATEGY_THRESHOLD,
        CONSOL_STRATEGY_ADAPTIVE
    };

    for (auto strategy : strategies) {
        consol_adapter_config_t config = consol_adapter_default_config();
        config.strategy = strategy;
        config.num_channels = 1;

        consol_adapter_t* adapter = consol_adapter_create(&config);
        ASSERT_NE(adapter, nullptr);

        // Add known values
        uint64_t timestamp = 0;
        for (int t = 0; t < 10; t++) {
            EXPECT_TRUE(consol_adapter_update(adapter, 0, 1.0f, timestamp));
            timestamp += 1000;
        }

        // All strategies should produce valid output
        float consolidated = consol_adapter_get_consolidated(adapter, 0);
        EXPECT_FALSE(std::isnan(consolidated));
        EXPECT_GE(consolidated, 0.0f);
        EXPECT_LE(consolidated, 2.0f);  // Reasonable range

        consol_adapter_destroy(adapter);
    }
}

TEST_F(CognitiveAdaptersRegressionTest, ConsolidationPerformance) {
    // WHAT: Verify consolidation meets performance baseline
    // WHY: No performance regressions

    consol_adapter_config_t config = consol_adapter_default_config();
    config.num_channels = 4;

    consol_adapter_t* adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Measure update performance
    double update_time = measureTimeUs([&]() {
        uint64_t timestamp = 0;
        for (int t = 0; t < 100; t++) {
            for (size_t ch = 0; ch < 4; ch++) {
                consol_adapter_update(adapter, ch, 1.0f, timestamp);
            }
            timestamp += 1000;
        }
    });

    double avg_update_time = update_time / 400.0;  // 100 * 4 channels
    EXPECT_LT(avg_update_time, MAX_CONSOL_UPDATE_TIME_US);

    consol_adapter_destroy(adapter);
}

TEST_F(CognitiveAdaptersRegressionTest, ConsolidationTrendCalculation) {
    // WHAT: Verify trend calculation formula
    // WHY: Trend detection must be consistent

    consol_adapter_config_t config = consol_adapter_default_config();
    config.num_channels = 1;

    consol_adapter_t* adapter = consol_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Create increasing trend
    uint64_t timestamp = 0;
    for (int t = 0; t < 50; t++) {
        float value = static_cast<float>(t);
        EXPECT_TRUE(consol_adapter_update(adapter, 0, value, timestamp));
        timestamp += 1000;
    }

    float trend = consol_adapter_get_trend(adapter, 0);
    EXPECT_GT(trend, 0.0f);  // Positive trend

    // Create decreasing trend
    consol_adapter_clear(adapter);
    timestamp = 0;
    for (int t = 0; t < 50; t++) {
        float value = 50.0f - static_cast<float>(t);
        EXPECT_TRUE(consol_adapter_update(adapter, 0, value, timestamp));
        timestamp += 1000;
    }

    trend = consol_adapter_get_trend(adapter, 0);
    EXPECT_LT(trend, 0.0f);  // Negative trend

    consol_adapter_destroy(adapter);
}

//=============================================================================
// 3. BACKWARD COMPATIBILITY - ATTENTION
//=============================================================================

TEST_F(CognitiveAdaptersRegressionTest, AttentionDefaultConfigUnchanged) {
    // WHAT: Verify default attention configuration
    // WHY: Existing code relies on defaults

    attention_adapter_config_t config = attention_adapter_default_config();

    EXPECT_EQ(config.mode, ATTENTION_CONTROL_TOPDOWN);
    EXPECT_GT(config.max_targets, 0);
    EXPECT_EQ(config.spotlight_size, COGNITIVE_ATTENTION_SPOTLIGHT_SIZE);
    EXPECT_FALSE(config.enable_wta);
    EXPECT_FALSE(config.enable_pattern_detection);
}

TEST_F(CognitiveAdaptersRegressionTest, AttentionAPISurfaceStable) {
    // WHAT: Verify attention API unchanged
    // WHY: Binary compatibility

    attention_adapter_t* adapter = attention_adapter_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    // All original APIs must exist
    EXPECT_TRUE(attention_adapter_set_weight(adapter, 0, 1, 0.8f));
    EXPECT_TRUE(attention_adapter_update_salience(adapter, 1, 0.7f));

    uint32_t winner;
    attention_adapter_apply_wta(adapter, &winner);

    uint32_t spotlight[8];
    uint32_t num;
    attention_adapter_update_spotlight(adapter, spotlight, &num);

    float signal_in[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float signal_out[4];
    EXPECT_TRUE(attention_adapter_route_signal(adapter, 1,
                                               signal_in, signal_out, 4));

    attention_adapter_stats_t stats;
    EXPECT_TRUE(attention_adapter_get_stats(adapter, &stats));

    attention_adapter_destroy(adapter);
}

TEST_F(CognitiveAdaptersRegressionTest, AttentionModeSwitching) {
    // WHAT: Verify attention mode switching behavior
    // WHY: Mode changes must work consistently

    attention_control_mode_t modes[] = {
        ATTENTION_CONTROL_TOPDOWN,
        ATTENTION_CONTROL_BOTTOMUP,
        ATTENTION_CONTROL_MIXED
    };

    for (auto mode : modes) {
        attention_adapter_config_t config = attention_adapter_default_config();
        config.mode = mode;

        attention_adapter_t* adapter = attention_adapter_create(&config);
        ASSERT_NE(adapter, nullptr);

        // All modes should support basic operations
        EXPECT_TRUE(attention_adapter_set_weight(adapter, 0, 1, 0.8f));
        EXPECT_TRUE(attention_adapter_update_salience(adapter, 1, 0.6f));

        attention_adapter_destroy(adapter);
    }
}

TEST_F(CognitiveAdaptersRegressionTest, AttentionWTABehavior) {
    // WHAT: Verify winner-take-all behavior consistency
    // WHY: WTA must select highest weight deterministically

    attention_adapter_config_t config = attention_adapter_default_config();
    config.enable_wta = true;

    attention_adapter_t* adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Set multiple weights
    EXPECT_TRUE(attention_adapter_set_weight(adapter, 0, 1, 0.5f));
    EXPECT_TRUE(attention_adapter_set_weight(adapter, 0, 2, 0.9f));
    EXPECT_TRUE(attention_adapter_set_weight(adapter, 0, 3, 0.3f));

    // Apply WTA
    uint32_t winner = 0;
    EXPECT_TRUE(attention_adapter_apply_wta(adapter, &winner));

    // Winner should be target with highest weight
    EXPECT_EQ(winner, 2);

    attention_adapter_destroy(adapter);
}

TEST_F(CognitiveAdaptersRegressionTest, AttentionSpotlightSize) {
    // WHAT: Verify spotlight maintains configured size
    // WHY: Spotlight size constraint must be enforced

    attention_adapter_config_t config = attention_adapter_default_config();
    config.spotlight_size = 3;

    attention_adapter_t* adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Set many targets
    for (uint32_t i = 0; i < 10; i++) {
        float weight = static_cast<float>(i) / 10.0f;
        EXPECT_TRUE(attention_adapter_set_weight(adapter, 0, i, weight));
    }

    // Update spotlight
    uint32_t spotlight[3];
    uint32_t num_in_spotlight = 0;
    EXPECT_TRUE(attention_adapter_update_spotlight(adapter,
                                                   spotlight,
                                                   &num_in_spotlight));

    // Should have exactly spotlight_size targets
    EXPECT_EQ(num_in_spotlight, 3);

    attention_adapter_destroy(adapter);
}

TEST_F(CognitiveAdaptersRegressionTest, AttentionSignalModulation) {
    // WHAT: Verify signal routing formula unchanged
    // WHY: Attention modulation must be consistent

    attention_adapter_t* adapter = attention_adapter_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    // Set attention weight
    float attention_weight = 0.5f;
    EXPECT_TRUE(attention_adapter_set_weight(adapter, 0, 1, attention_weight));

    // Route signal
    float signal_in[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float signal_out[4];
    EXPECT_TRUE(attention_adapter_route_signal(adapter, 1,
                                               signal_in, signal_out, 4));

    // Output should be scaled by attention weight
    for (int i = 0; i < 4; i++) {
        EXPECT_NEAR(signal_out[i], signal_in[i] * attention_weight, 0.01f);
    }

    attention_adapter_destroy(adapter);
}

TEST_F(CognitiveAdaptersRegressionTest, AttentionPerformance) {
    // WHAT: Verify attention operations meet performance baseline
    // WHY: No performance regressions

    attention_adapter_t* adapter = attention_adapter_create(nullptr);
    ASSERT_NE(adapter, nullptr);

    // Measure set weight performance
    double set_time = measureTimeUs([&]() {
        for (int i = 0; i < 100; i++) {
            attention_adapter_set_weight(adapter, 0, i % 10, 0.7f);
        }
    });

    double avg_set_time = set_time / 100.0;
    EXPECT_LT(avg_set_time, MAX_ATTENTION_SET_TIME_US);

    attention_adapter_destroy(adapter);
}

//=============================================================================
// 4. CROSS-ADAPTER BEHAVIORAL CONSISTENCY
//=============================================================================

TEST_F(CognitiveAdaptersRegressionTest, TimestampHandlingConsistency) {
    // WHAT: Verify all adapters handle timestamps consistently
    // WHY: Time handling must be uniform

    wm_adapter_t* wm = wm_adapter_create(nullptr);
    consol_adapter_t* consol = consol_adapter_create(nullptr);
    attention_adapter_t* attn = attention_adapter_create(nullptr);

    ASSERT_NE(wm, nullptr);
    ASSERT_NE(consol, nullptr);
    ASSERT_NE(attn, nullptr);

    // All should accept zero timestamp
    float data[2] = {1.0f, 2.0f};
    EXPECT_TRUE(wm_adapter_add_item(wm, 1, data, 2, 0.7f));
    EXPECT_TRUE(consol_adapter_update(consol, 0, 1.0f, 0));
    EXPECT_TRUE(attention_adapter_set_weight(attn, 0, 1, 0.8f));

    // All should accept large timestamps
    uint64_t large_ts = 0xFFFFFFFFFFFFULL;
    EXPECT_TRUE(consol_adapter_update(consol, 0, 1.0f, large_ts));

    wm_adapter_destroy(wm);
    consol_adapter_destroy(consol);
    attention_adapter_destroy(attn);
}

TEST_F(CognitiveAdaptersRegressionTest, ErrorHandlingConsistency) {
    // WHAT: Verify consistent error handling across adapters
    // WHY: API should handle errors uniformly

    wm_adapter_t* wm = wm_adapter_create(nullptr);
    consol_adapter_t* consol = consol_adapter_create(nullptr);
    attention_adapter_t* attn = attention_adapter_create(nullptr);

    // NULL pointer handling
    EXPECT_FALSE(wm_adapter_add_item(wm, 1, nullptr, 2, 0.7f));
    EXPECT_EQ(wm_adapter_get_item(wm, 999), nullptr);  // Non-existent item

    // Invalid channel
    EXPECT_FALSE(consol_adapter_update(consol, 9999, 1.0f, 0));

    // Invalid weights
    EXPECT_TRUE(attention_adapter_set_weight(attn, 0, 1, 1.5f));  // Clamped to 1.0
    EXPECT_TRUE(attention_adapter_set_weight(attn, 0, 2, -0.5f)); // Clamped to 0.0

    wm_adapter_destroy(wm);
    consol_adapter_destroy(consol);
    attention_adapter_destroy(attn);
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
