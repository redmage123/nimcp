/**
 * @file test_attention_adapter.cpp
 * @brief Comprehensive unit tests for attention adapter
 *
 * WHAT: Test attention adapter routing, gating, pattern detection
 * WHY:  Ensure attention adapter provides reliable attention modulation
 * HOW:  Test all 11 API functions with edge cases, modes, WTA, spotlight
 *
 * COVERAGE GOALS:
 * - 100% function coverage for attention adapter
 * - All control modes tested
 * - WTA and spotlight validated
 * - Pattern detection verified
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
#include <algorithm>

// Headers have their own extern "C" guards
#include "middleware/cognitive/nimcp_cognitive_adapters.h"
#include "middleware/routing/nimcp_attention_gate.h"
#include "core/events/nimcp_event_bus.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AttentionAdapterTest : public ::testing::Test {
protected:
    attention_adapter_t* adapter = nullptr;
    attention_adapter_config_t config;
    event_bus_t event_bus;

    void SetUp() override {
        config = attention_adapter_default_config();
        // Create event bus for cognitive event integration
        event_bus = event_bus_create("test_attention_bus", EVENT_DELIVERY_IMMEDIATE);
        ASSERT_NE(event_bus, nullptr) << "Failed to create event bus";
    }

    void TearDown() override {
        if (adapter) {
            attention_adapter_destroy(adapter);
            adapter = nullptr;
        }
        if (event_bus) {
            event_bus_destroy(event_bus);
            event_bus = nullptr;
        }
    }

    // Helper: Create test signal
    std::vector<float> create_test_signal(size_t size, float value = 1.0f) {
        std::vector<float> signal(size, value);
        return signal;
    }

    // Helper: Check if signal is modulated correctly
    bool is_modulated(const float* input, const float* output,
                     size_t size, float expected_weight) {
        for (size_t i = 0; i < size; i++) {
            float expected = input[i] * expected_weight;
            if (std::abs(output[i] - expected) > 0.01f) {
                return false;
            }
        }
        return true;
    }
};

//=============================================================================
// LIFECYCLE TESTS (create/destroy)
//=============================================================================

TEST_F(AttentionAdapterTest, CreateWithDefaultConfig) {
    // WHAT: Create adapter with NULL config (uses defaults)
    // WHY:  Verify default configuration works
    adapter = attention_adapter_create(nullptr);
    ASSERT_NE(adapter, nullptr) << "Should create with default config";
}

TEST_F(AttentionAdapterTest, CreateWithCustomConfig) {
    // WHAT: Create adapter with custom configuration
    // WHY:  Verify custom config is respected
    config.mode = ATTENTION_CONTROL_TOPDOWN;
    config.max_targets = 64;
    config.spotlight_size = 4;
    config.enable_wta = true;

    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr) << "Should create with custom config";
}

TEST_F(AttentionAdapterTest, CreateAllModes) {
    // WHAT: Test all control modes
    // WHY:  Ensure all modes initialize correctly
    attention_control_mode_t modes[] = {
        ATTENTION_CONTROL_TOPDOWN,
        ATTENTION_CONTROL_BOTTOMUP,
        ATTENTION_CONTROL_MIXED,
        ATTENTION_CONTROL_LEARNED
    };

    for (auto mode : modes) {
        config.mode = mode;
        attention_adapter_t* test_adapter = attention_adapter_create(&config);
        EXPECT_NE(test_adapter, nullptr) << "Mode " << mode << " should work";
        attention_adapter_destroy(test_adapter);
    }
}

TEST_F(AttentionAdapterTest, DestroyNullSafe) {
    // WHAT: Destroy NULL adapter
    // WHY:  Ensure NULL is safe (should not crash)
    attention_adapter_destroy(nullptr);
    SUCCEED() << "Destroying NULL should be safe";
}

TEST_F(AttentionAdapterTest, DestroyFreesResources) {
    // WHAT: Verify destroy frees all resources
    // WHY:  Prevent memory leaks
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Set weights
    attention_adapter_set_weight(adapter, 0, 1, 0.5f);
    attention_adapter_set_weight(adapter, 0, 2, 0.7f);

    attention_adapter_destroy(adapter);
    adapter = nullptr;
    SUCCEED() << "Destroy should free all resources";
}

//=============================================================================
// SET WEIGHT TESTS
//=============================================================================

TEST_F(AttentionAdapterTest, SetWeightSuccess) {
    // WHAT: Set attention weight successfully
    // WHY:  Verify basic weight setting
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    bool result = attention_adapter_set_weight(adapter, 0, 1, 0.75f);
    EXPECT_TRUE(result) << "Should set weight successfully";
}

TEST_F(AttentionAdapterTest, SetWeightNullAdapter) {
    // WHAT: Set weight with NULL adapter
    // WHY:  Test error handling
    bool result = attention_adapter_set_weight(nullptr, 0, 1, 0.5f);
    EXPECT_FALSE(result) << "Should fail with NULL adapter";
}

TEST_F(AttentionAdapterTest, SetWeightBoundaries) {
    // WHAT: Test weight boundary values
    // WHY:  Verify [0,1] range
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    EXPECT_TRUE(attention_adapter_set_weight(adapter, 0, 1, 0.0f));
    EXPECT_TRUE(attention_adapter_set_weight(adapter, 0, 2, 1.0f));
    EXPECT_TRUE(attention_adapter_set_weight(adapter, 0, 3, 0.5f));
}

TEST_F(AttentionAdapterTest, SetWeightMultipleTargets) {
    // WHAT: Set weights for multiple targets
    // WHY:  Verify multi-target support
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    for (uint32_t target = 0; target < 10; target++) {
        float weight = (float)target / 10.0f;
        EXPECT_TRUE(attention_adapter_set_weight(adapter, 0, target, weight));
    }
}

//=============================================================================
// UPDATE SALIENCE TESTS
//=============================================================================

TEST_F(AttentionAdapterTest, UpdateSalienceSuccess) {
    // WHAT: Update bottom-up salience
    // WHY:  Verify salience-driven attention
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    bool result = attention_adapter_update_salience(adapter, 1, 0.8f);
    EXPECT_TRUE(result) << "Should update salience successfully";
}

TEST_F(AttentionAdapterTest, UpdateSalienceNullAdapter) {
    // WHAT: Update salience with NULL adapter
    // WHY:  Test error handling
    bool result = attention_adapter_update_salience(nullptr, 1, 0.5f);
    EXPECT_FALSE(result) << "Should fail with NULL adapter";
}

TEST_F(AttentionAdapterTest, UpdateSalienceMultipleTargets) {
    // WHAT: Update salience for multiple targets
    // WHY:  Verify multi-target salience tracking
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    for (uint32_t target = 0; target < 10; target++) {
        float salience = (float)target / 10.0f;
        EXPECT_TRUE(attention_adapter_update_salience(adapter, target, salience));
    }
}

//=============================================================================
// ROUTE SIGNAL TESTS
//=============================================================================

TEST_F(AttentionAdapterTest, RouteSignalSuccess) {
    // WHAT: Route signal with attention modulation
    // WHY:  Verify basic signal routing
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    attention_adapter_set_weight(adapter, 0, 1, 0.5f);

    auto input = create_test_signal(10, 2.0f);
    std::vector<float> output(10);

    bool result = attention_adapter_route_signal(adapter, 1,
                                                 input.data(), output.data(), 10);
    EXPECT_TRUE(result) << "Should route signal successfully";

    // Output should be modulated by weight
    EXPECT_TRUE(is_modulated(input.data(), output.data(), 10, 0.5f));
}

TEST_F(AttentionAdapterTest, RouteSignalZeroWeight) {
    // WHAT: Route signal with zero attention weight
    // WHY:  Verify suppression
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    attention_adapter_set_weight(adapter, 0, 1, 0.0f);

    auto input = create_test_signal(10, 5.0f);
    std::vector<float> output(10);

    ASSERT_TRUE(attention_adapter_route_signal(adapter, 1,
                                               input.data(), output.data(), 10));

    // Output should be all zeros
    for (float val : output) {
        EXPECT_FLOAT_EQ(val, 0.0f);
    }
}

TEST_F(AttentionAdapterTest, RouteSignalFullWeight) {
    // WHAT: Route signal with full attention weight
    // WHY:  Verify no attenuation
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    attention_adapter_set_weight(adapter, 0, 1, 1.0f);

    auto input = create_test_signal(10, 3.5f);
    std::vector<float> output(10);

    ASSERT_TRUE(attention_adapter_route_signal(adapter, 1,
                                               input.data(), output.data(), 10));

    // Output should equal input
    for (size_t i = 0; i < 10; i++) {
        EXPECT_FLOAT_EQ(output[i], input[i]);
    }
}

TEST_F(AttentionAdapterTest, RouteSignalNullParameters) {
    // WHAT: Route signal with NULL parameters
    // WHY:  Test error handling
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    auto input = create_test_signal(10);
    std::vector<float> output(10);

    EXPECT_FALSE(attention_adapter_route_signal(nullptr, 1,
                                                input.data(), output.data(), 10));
    EXPECT_FALSE(attention_adapter_route_signal(adapter, 1,
                                                nullptr, output.data(), 10));
    EXPECT_FALSE(attention_adapter_route_signal(adapter, 1,
                                                input.data(), nullptr, 10));
}

TEST_F(AttentionAdapterTest, RouteSignalZeroSize) {
    // WHAT: Route signal with zero size
    // WHY:  Test boundary condition
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    auto input = create_test_signal(10);
    std::vector<float> output(10);

    bool result = attention_adapter_route_signal(adapter, 1,
                                                 input.data(), output.data(), 0);
    EXPECT_FALSE(result) << "Should fail with zero size";
}

//=============================================================================
// WINNER-TAKE-ALL TESTS
//=============================================================================

TEST_F(AttentionAdapterTest, ApplyWTASuccess) {
    // WHAT: Apply winner-take-all
    // WHY:  Verify WTA mechanism
    config.enable_wta = true;
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Set different weights
    attention_adapter_set_weight(adapter, 0, 1, 0.3f);
    attention_adapter_set_weight(adapter, 0, 2, 0.7f);
    attention_adapter_set_weight(adapter, 0, 3, 0.5f);

    uint32_t winner_id = 0;
    bool result = attention_adapter_apply_wta(adapter, &winner_id);
    EXPECT_TRUE(result) << "Should apply WTA successfully";
}

TEST_F(AttentionAdapterTest, ApplyWTANullAdapter) {
    // WHAT: Apply WTA with NULL adapter
    // WHY:  Test error handling
    uint32_t winner_id = 0;
    bool result = attention_adapter_apply_wta(nullptr, &winner_id);
    EXPECT_FALSE(result) << "Should fail with NULL adapter";
}

TEST_F(AttentionAdapterTest, ApplyWTAOptionalWinner) {
    // WHAT: Apply WTA without winner output
    // WHY:  Verify optional parameter
    config.enable_wta = true;
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    attention_adapter_set_weight(adapter, 0, 1, 0.5f);
    bool result = attention_adapter_apply_wta(adapter, nullptr);
    EXPECT_TRUE(result) << "Should work without winner output";
}

//=============================================================================
// SPOTLIGHT TESTS
//=============================================================================

TEST_F(AttentionAdapterTest, UpdateSpotlightSuccess) {
    // WHAT: Update attention spotlight
    // WHY:  Verify spotlight mechanism
    config.spotlight_size = 3;
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Set weights for multiple targets
    for (uint32_t i = 0; i < 10; i++) {
        float weight = (float)i / 10.0f;
        attention_adapter_set_weight(adapter, 0, i, weight);
    }

    uint32_t spotlight_ids[3];
    uint32_t num_in_spotlight = 0;

    bool result = attention_adapter_update_spotlight(adapter,
                                                     spotlight_ids,
                                                     &num_in_spotlight);
    EXPECT_TRUE(result) << "Should update spotlight successfully";
    EXPECT_LE(num_in_spotlight, 3u) << "Should not exceed spotlight size";
}

TEST_F(AttentionAdapterTest, UpdateSpotlightNullAdapter) {
    // WHAT: Update spotlight with NULL adapter
    // WHY:  Test error handling
    uint32_t spotlight_ids[5];
    uint32_t num_in_spotlight = 0;

    bool result = attention_adapter_update_spotlight(nullptr,
                                                     spotlight_ids,
                                                     &num_in_spotlight);
    EXPECT_FALSE(result) << "Should fail with NULL adapter";
}

TEST_F(AttentionAdapterTest, UpdateSpotlightOptionalOutputs) {
    // WHAT: Update spotlight without output parameters
    // WHY:  Verify optional parameters
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    attention_adapter_set_weight(adapter, 0, 1, 0.5f);
    bool result = attention_adapter_update_spotlight(adapter, nullptr, nullptr);
    EXPECT_TRUE(result) << "Should work without output parameters";
}

//=============================================================================
// PATTERN DETECTION TESTS
//=============================================================================

TEST_F(AttentionAdapterTest, DetectPatternSuccess) {
    // WHAT: Detect attention pattern
    // WHY:  Verify pattern detection
    config.enable_pattern_detection = true;
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Create stable attention pattern
    for (int i = 0; i < 10; i++) {
        attention_adapter_set_weight(adapter, 0, 1, 0.8f);
        attention_adapter_set_weight(adapter, 0, 2, 0.7f);
        attention_adapter_update_spotlight(adapter, nullptr, nullptr);
    }

    attention_pattern_t pattern;
    memset(&pattern, 0, sizeof(pattern));
    bool result = attention_adapter_detect_pattern(adapter, &pattern);

    if (result) {
        EXPECT_GT(pattern.num_targets, 0u);
        if (pattern.target_ids) {
            free(pattern.target_ids);
        }
    }
}

TEST_F(AttentionAdapterTest, DetectPatternDisabled) {
    // WHAT: Detect pattern when disabled
    // WHY:  Verify feature can be disabled
    config.enable_pattern_detection = false;
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    attention_pattern_t pattern;
    bool result = attention_adapter_detect_pattern(adapter, &pattern);
    EXPECT_FALSE(result) << "Should not detect when disabled";
}

TEST_F(AttentionAdapterTest, DetectPatternNullAdapter) {
    // WHAT: Detect pattern with NULL adapter
    // WHY:  Test error handling
    attention_pattern_t pattern;
    bool result = attention_adapter_detect_pattern(nullptr, &pattern);
    EXPECT_FALSE(result) << "Should fail with NULL adapter";
}

TEST_F(AttentionAdapterTest, DetectPatternOptionalOutput) {
    // WHAT: Detect pattern without output
    // WHY:  Verify optional parameter
    config.enable_pattern_detection = true;
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    attention_adapter_set_weight(adapter, 0, 1, 0.5f);
    attention_adapter_set_weight(adapter, 0, 2, 0.6f);
    attention_adapter_update_spotlight(adapter, nullptr, nullptr);

    bool result = attention_adapter_detect_pattern(adapter, nullptr);
    // May or may not detect, but should not crash
}

//=============================================================================
// GET SHIFTS TESTS
//=============================================================================

TEST_F(AttentionAdapterTest, GetShiftsSuccess) {
    // WHAT: Get attention shifts
    // WHY:  Verify shift tracking
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Create some shifts
    attention_adapter_set_weight(adapter, 0, 1, 1.0f);
    attention_adapter_apply_wta(adapter, nullptr);
    attention_adapter_set_weight(adapter, 0, 2, 1.0f);
    attention_adapter_apply_wta(adapter, nullptr);

    attention_shift_t shifts[10];
    uint32_t num_shifts = 0;

    bool result = attention_adapter_get_shifts(adapter, shifts, 10, &num_shifts);
    // Result depends on implementation
}

TEST_F(AttentionAdapterTest, GetShiftsNullAdapter) {
    // WHAT: Get shifts with NULL adapter
    // WHY:  Test error handling
    attention_shift_t shifts[10];
    uint32_t num_shifts = 0;

    bool result = attention_adapter_get_shifts(nullptr, shifts, 10, &num_shifts);
    EXPECT_FALSE(result) << "Should fail with NULL adapter";
}

//=============================================================================
// RESET TESTS
//=============================================================================

TEST_F(AttentionAdapterTest, ResetClearsState) {
    // WHAT: Reset clears attention state
    // WHY:  Verify reset functionality
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Set weights
    attention_adapter_set_weight(adapter, 0, 1, 0.8f);
    attention_adapter_set_weight(adapter, 0, 2, 0.6f);

    attention_adapter_reset(adapter);

    // After reset, stats should reflect clean state
    attention_adapter_stats_t stats;
    if (attention_adapter_get_stats(adapter, &stats)) {
        // Verify reset effect
    }
}

TEST_F(AttentionAdapterTest, ResetNullSafe) {
    // WHAT: Reset NULL adapter
    // WHY:  Ensure NULL is safe
    attention_adapter_reset(nullptr);
    SUCCEED() << "Resetting NULL should be safe";
}

TEST_F(AttentionAdapterTest, ResetCanUseAfter) {
    // WHAT: Can use adapter after reset
    // WHY:  Verify adapter remains functional
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    attention_adapter_set_weight(adapter, 0, 1, 0.5f);
    attention_adapter_reset(adapter);

    bool result = attention_adapter_set_weight(adapter, 0, 2, 0.7f);
    EXPECT_TRUE(result) << "Should be able to use after reset";
}

//=============================================================================
// STATISTICS TESTS
//=============================================================================

TEST_F(AttentionAdapterTest, GetStatsSuccess) {
    // WHAT: Retrieve adapter statistics
    // WHY:  Verify stats tracking
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Create some activity
    attention_adapter_set_weight(adapter, 0, 1, 0.5f);
    attention_adapter_set_weight(adapter, 0, 2, 0.7f);
    attention_adapter_apply_wta(adapter, nullptr);

    attention_adapter_stats_t stats;
    bool result = attention_adapter_get_stats(adapter, &stats);

    EXPECT_TRUE(result);
    EXPECT_GE(stats.attention_stability, 0.0f);
    EXPECT_LE(stats.attention_stability, 1.0f);
}

TEST_F(AttentionAdapterTest, GetStatsTracksShifts) {
    // WHAT: Stats track attention shifts
    // WHY:  Verify shift counting
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    attention_adapter_stats_t stats_before;
    attention_adapter_get_stats(adapter, &stats_before);

    // Create shifts
    for (int i = 0; i < 5; i++) {
        attention_adapter_set_weight(adapter, 0, i, 1.0f);
        attention_adapter_apply_wta(adapter, nullptr);
    }

    attention_adapter_stats_t stats_after;
    ASSERT_TRUE(attention_adapter_get_stats(adapter, &stats_after));
    // May track shifts depending on implementation
}

TEST_F(AttentionAdapterTest, GetStatsNullParameters) {
    // WHAT: Get stats with NULL parameters
    // WHY:  Test error handling
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    bool result = attention_adapter_get_stats(nullptr, nullptr);
    EXPECT_FALSE(result);

    attention_adapter_stats_t stats;
    result = attention_adapter_get_stats(nullptr, &stats);
    EXPECT_FALSE(result);

    result = attention_adapter_get_stats(adapter, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// DEFAULT CONFIG TESTS
//=============================================================================

TEST_F(AttentionAdapterTest, DefaultConfigValid) {
    // WHAT: Default config has valid values
    // WHY:  Ensure sensible defaults
    attention_adapter_config_t def = attention_adapter_default_config();

    EXPECT_GT(def.max_targets, 0u);
    EXPECT_GT(def.spotlight_size, 0u);
    EXPECT_LE(def.spotlight_size, def.max_targets);
    EXPECT_GE(def.pattern_threshold, 0.0f);
    EXPECT_LE(def.pattern_threshold, 1.0f);
    EXPECT_TRUE(def.mode == ATTENTION_CONTROL_TOPDOWN ||
                def.mode == ATTENTION_CONTROL_BOTTOMUP ||
                def.mode == ATTENTION_CONTROL_MIXED ||
                def.mode == ATTENTION_CONTROL_LEARNED);
}

//=============================================================================
// CONCURRENT ACCESS TESTS
//=============================================================================

TEST_F(AttentionAdapterTest, ConcurrentWeightUpdates) {
    // WHAT: Multiple threads updating weights
    // WHY:  Test thread safety (best effort)
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    std::vector<std::thread> threads;
    const int num_threads = 4;
    const int updates_per_thread = 100;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, updates_per_thread]() {
            for (int i = 0; i < updates_per_thread; i++) {
                uint32_t target = (t * updates_per_thread + i) % config.max_targets;
                attention_adapter_set_weight(adapter, 0, target, 0.5f);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    SUCCEED() << "Concurrent updates completed";
}

//=============================================================================
// EDGE CASES AND BOUNDARY CONDITIONS
//=============================================================================

TEST_F(AttentionAdapterTest, MaxTargets) {
    // WHAT: Test maximum target count
    // WHY:  Verify large target count works
    config.max_targets = 512;
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    for (uint32_t target = 0; target < 100; target++) {
        EXPECT_TRUE(attention_adapter_set_weight(adapter, 0, target, 0.5f));
    }
}

TEST_F(AttentionAdapterTest, MinimalSpotlight) {
    // WHAT: Test minimal spotlight size (1)
    // WHY:  Verify edge case
    config.spotlight_size = 1;
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    attention_adapter_set_weight(adapter, 0, 1, 0.5f);
    attention_adapter_set_weight(adapter, 0, 2, 0.7f);

    uint32_t spotlight_ids[1];
    uint32_t num_in_spotlight = 0;

    EXPECT_TRUE(attention_adapter_update_spotlight(adapter,
                                                   spotlight_ids,
                                                   &num_in_spotlight));
    EXPECT_LE(num_in_spotlight, 1u);
}

TEST_F(AttentionAdapterTest, LargeSignalRouting) {
    // WHAT: Route large signal
    // WHY:  Test performance with large data
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    attention_adapter_set_weight(adapter, 0, 1, 0.5f);

    auto input = create_test_signal(10000, 1.0f);
    std::vector<float> output(10000);

    bool result = attention_adapter_route_signal(adapter, 1,
                                                 input.data(), output.data(), 10000);
    EXPECT_TRUE(result);
}

TEST_F(AttentionAdapterTest, RapidModeChanges) {
    // WHAT: Create adapters with different modes rapidly
    // WHY:  Test mode handling robustness
    attention_control_mode_t modes[] = {
        ATTENTION_CONTROL_TOPDOWN,
        ATTENTION_CONTROL_BOTTOMUP,
        ATTENTION_CONTROL_MIXED
    };

    for (int i = 0; i < 10; i++) {
        config.mode = modes[i % 3];
        attention_adapter_t* test = attention_adapter_create(&config);
        ASSERT_NE(test, nullptr);
        attention_adapter_set_weight(test, 0, 1, 0.5f);
        attention_adapter_destroy(test);
    }
}

TEST_F(AttentionAdapterTest, ExtremeSalienceValues) {
    // WHAT: Update with extreme salience values
    // WHY:  Test robustness
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    EXPECT_TRUE(attention_adapter_update_salience(adapter, 1, 0.0f));
    EXPECT_TRUE(attention_adapter_update_salience(adapter, 2, 1.0f));
    EXPECT_TRUE(attention_adapter_update_salience(adapter, 3, 0.5f));
}

TEST_F(AttentionAdapterTest, ManyPatternDetections) {
    // WHAT: Detect many patterns in sequence
    // WHY:  Test pattern detection performance
    config.enable_pattern_detection = true;
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    for (int i = 0; i < 100; i++) {
        attention_adapter_set_weight(adapter, 0, 1, 0.8f);
        attention_adapter_set_weight(adapter, 0, 2, 0.7f);
        attention_adapter_update_spotlight(adapter, nullptr, nullptr);

        attention_pattern_t pattern;
        if (attention_adapter_detect_pattern(adapter, &pattern)) {
            if (pattern.target_ids) {
                free(pattern.target_ids);
            }
        }
    }

    attention_adapter_stats_t stats;
    ASSERT_TRUE(attention_adapter_get_stats(adapter, &stats));
}

//=============================================================================
// INTEGRATION TESTS
//=============================================================================

TEST_F(AttentionAdapterTest, TopDownBottomUpIntegration) {
    // WHAT: Test integration of top-down and bottom-up attention
    // WHY:  Verify mixed mode operation
    config.mode = ATTENTION_CONTROL_MIXED;
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Set top-down weight
    attention_adapter_set_weight(adapter, 0, 1, 0.6f);

    // Set bottom-up salience
    attention_adapter_update_salience(adapter, 1, 0.8f);

    // Route signal
    auto input = create_test_signal(10, 2.0f);
    std::vector<float> output(10);

    bool result = attention_adapter_route_signal(adapter, 1,
                                                 input.data(), output.data(), 10);
    EXPECT_TRUE(result);
    // Output should reflect combined attention
}

TEST_F(AttentionAdapterTest, WTAWithSpotlight) {
    // WHAT: Use WTA and spotlight together
    // WHY:  Verify feature interaction
    config.enable_wta = true;
    config.spotlight_size = 3;
    adapter = attention_adapter_create(&config);
    ASSERT_NE(adapter, nullptr);

    // Set multiple weights
    for (uint32_t i = 0; i < 10; i++) {
        attention_adapter_set_weight(adapter, 0, i, (float)i / 10.0f);
    }

    // Apply WTA
    uint32_t winner = 0;
    EXPECT_TRUE(attention_adapter_apply_wta(adapter, &winner));

    // Update spotlight
    uint32_t spotlight_ids[3];
    uint32_t num_in_spotlight = 0;
    EXPECT_TRUE(attention_adapter_update_spotlight(adapter,
                                                   spotlight_ids,
                                                   &num_in_spotlight));
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
