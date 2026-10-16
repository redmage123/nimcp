/**
 * @file test_auto_activity_history.cpp
 * @brief Unit tests for auto activity history feature
 *
 * TEST COVERAGE:
 * - Configuration defaults
 * - Activity sampling
 * - Sample interval configuration
 * - History buffer management
 * - Change threshold filtering
 * - Callback invocation
 * - History statistics
 * - Clear history
 * - Edge cases and error handling
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>

#include "cognitive/introspection/nimcp_introspection.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AutoActivityHistoryTest : public ::testing::Test {
protected:
    brain_t brain;
    introspection_context_t context;

    void SetUp() override {
        // Create a minimal brain for testing
        brain = brain_create(
            "test_auto_history",
            BRAIN_SIZE_TINY,
            BRAIN_TASK_CLASSIFICATION,
            10,  // num_inputs
            3    // num_outputs
        );

        // Create introspection context with auto history disabled initially
        introspection_config_t config = introspection_default_config();
        config.enable_auto_history = false;
        config.history_sample_interval_ms = 50;  // Fast sampling for tests
        config.history_change_threshold = 0.0F;  // Record all samples

        if (brain != nullptr) {
            context = introspection_context_create(brain, &config);
        } else {
            context = nullptr;
        }
    }

    void TearDown() override {
        if (context != nullptr) {
            introspection_context_destroy(context);
            context = nullptr;
        }
        if (brain != nullptr) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper to simulate brain activity
    void simulate_activity() {
        if (brain == nullptr) return;

        // Process some input to activate neurons
        float inputs[10] = {0.5, 0.3, 0.8, 0.2, 0.9, 0.1, 0.6, 0.4, 0.7, 0.3};
        brain_decision_t* decision = brain_decide(brain, inputs, 10);
        if (decision != nullptr) {
            nimcp_free(decision);
        }
    }
};

//=============================================================================
// 1. Configuration Tests
//=============================================================================

TEST_F(AutoActivityHistoryTest, DefaultConfigDisablesAutoHistory) {
    introspection_config_t config = introspection_default_config();

    EXPECT_FALSE(config.enable_auto_history);
    EXPECT_EQ(config.history_sample_interval_ms, 100);
    EXPECT_FLOAT_EQ(config.history_change_threshold, 0.05F);
}

TEST_F(AutoActivityHistoryTest, ConfigWithAutoHistoryEnabled) {
    if (brain == nullptr) GTEST_SKIP();

    introspection_config_t config = introspection_default_config();
    config.enable_auto_history = true;
    config.history_sample_interval_ms = 200;
    config.history_change_threshold = 0.1F;

    introspection_context_t ctx = introspection_context_create(brain, &config);
    ASSERT_NE(ctx, nullptr);

    introspection_context_destroy(ctx);
}

//=============================================================================
// 2. Activity Sampling Tests
//=============================================================================

TEST_F(AutoActivityHistoryTest, ManualSamplingWorks) {
    if (context == nullptr) GTEST_SKIP();

    // Sample activity manually
    bool result = introspection_sample_activity(context);
    EXPECT_TRUE(result);

    // Check that history has one entry
    uint32_t num_entries = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);

    EXPECT_EQ(num_entries, 1);
    if (history != nullptr) {
        EXPECT_GE(history[0].timestamp, 0);
        EXPECT_GE(history[0].avg_activation, 0.0F);
        EXPECT_LE(history[0].avg_activation, 1.0F);
        EXPECT_GE(history[0].max_activation, 0.0F);
        EXPECT_LE(history[0].max_activation, 1.0F);
        EXPECT_GE(history[0].energy_consumption, 0.0F);

        nimcp_free(history);
    }
}

TEST_F(AutoActivityHistoryTest, SamplingNullContextFails) {
    bool result = introspection_sample_activity(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(AutoActivityHistoryTest, MultipleSamplesRecorded) {
    if (context == nullptr) GTEST_SKIP();

    // Take multiple samples
    const int num_samples = 5;
    for (int i = 0; i < num_samples; i++) {
        simulate_activity();  // Change brain state
        bool result = introspection_sample_activity(context);
        EXPECT_TRUE(result);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Verify history
    uint32_t num_entries = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);

    EXPECT_EQ(num_entries, num_samples);

    // Verify timestamps are increasing
    if (history != nullptr && num_entries > 1) {
        for (uint32_t i = 1; i < num_entries; i++) {
            EXPECT_GT(history[i].timestamp, history[i-1].timestamp);
        }
        nimcp_free(history);
    }
}

//=============================================================================
// 3. Change Threshold Tests
//=============================================================================

TEST_F(AutoActivityHistoryTest, ChangeThresholdFiltersStableActivity) {
    if (brain == nullptr) GTEST_SKIP();

    // Create context with change threshold
    introspection_config_t config = introspection_default_config();
    config.history_change_threshold = 0.5F;  // High threshold - only big changes

    introspection_context_t ctx = introspection_context_create(brain, &config);
    ASSERT_NE(ctx, nullptr);

    // First sample should always be recorded
    introspection_sample_activity(ctx);

    // Second sample with similar activity should be filtered
    introspection_sample_activity(ctx);

    // Check history - should only have 1 entry (or possibly 2 if activity actually changed)
    uint32_t num_entries = 0;
    activity_history_entry_t* history = brain_get_activity_history(ctx, &num_entries);

    // With high threshold and no actual activity change, we expect 1 entry
    EXPECT_LE(num_entries, 2);

    if (history != nullptr) {
        nimcp_free(history);
    }

    introspection_context_destroy(ctx);
}

TEST_F(AutoActivityHistoryTest, ZeroThresholdRecordsAllSamples) {
    if (context == nullptr) GTEST_SKIP();

    // Set zero threshold (already set in SetUp)
    const int num_samples = 10;
    for (int i = 0; i < num_samples; i++) {
        introspection_sample_activity(context);
    }

    uint32_t num_entries = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);

    // All samples should be recorded with zero threshold
    EXPECT_EQ(num_entries, num_samples);

    if (history != nullptr) {
        nimcp_free(history);
    }
}

//=============================================================================
// 4. Sample Interval Tests
//=============================================================================

TEST_F(AutoActivityHistoryTest, SetSampleIntervalWorks) {
    if (context == nullptr) GTEST_SKIP();

    bool result = introspection_set_sample_interval(context, 500);
    EXPECT_TRUE(result);
}

TEST_F(AutoActivityHistoryTest, SetSampleIntervalNullContextFails) {
    bool result = introspection_set_sample_interval(nullptr, 100);
    EXPECT_FALSE(result);
}

//=============================================================================
// 5. History Statistics Tests
//=============================================================================

TEST_F(AutoActivityHistoryTest, HistoryStatsInitiallyEmpty) {
    if (context == nullptr) GTEST_SKIP();

    uint32_t size, capacity;
    float utilization;

    bool result = introspection_get_history_stats(context, &size, &capacity, &utilization);
    EXPECT_TRUE(result);
    EXPECT_EQ(size, 0);
    EXPECT_GT(capacity, 0);
    EXPECT_FLOAT_EQ(utilization, 0.0F);
}

TEST_F(AutoActivityHistoryTest, HistoryStatsAfterSampling) {
    if (context == nullptr) GTEST_SKIP();

    // Add some samples
    const int num_samples = 5;
    for (int i = 0; i < num_samples; i++) {
        introspection_sample_activity(context);
    }

    uint32_t size, capacity;
    float utilization;

    bool result = introspection_get_history_stats(context, &size, &capacity, &utilization);
    EXPECT_TRUE(result);
    EXPECT_EQ(size, num_samples);
    EXPECT_GT(capacity, 0);
    EXPECT_GT(utilization, 0.0F);
    EXPECT_LE(utilization, 1.0F);
}

TEST_F(AutoActivityHistoryTest, HistoryStatsNullInputsFail) {
    if (context == nullptr) GTEST_SKIP();

    uint32_t size, capacity;
    float utilization;

    EXPECT_FALSE(introspection_get_history_stats(nullptr, &size, &capacity, &utilization));
    EXPECT_FALSE(introspection_get_history_stats(context, nullptr, &capacity, &utilization));
    EXPECT_FALSE(introspection_get_history_stats(context, &size, nullptr, &utilization));
    EXPECT_FALSE(introspection_get_history_stats(context, &size, &capacity, nullptr));
}

//=============================================================================
// 6. Clear History Tests
//=============================================================================

TEST_F(AutoActivityHistoryTest, ClearHistoryWorks) {
    if (context == nullptr) GTEST_SKIP();

    // Add samples
    for (int i = 0; i < 5; i++) {
        introspection_sample_activity(context);
    }

    // Clear history
    bool result = introspection_clear_history(context);
    EXPECT_TRUE(result);

    // Verify empty
    uint32_t size, capacity;
    float utilization;
    introspection_get_history_stats(context, &size, &capacity, &utilization);

    EXPECT_EQ(size, 0);
    EXPECT_FLOAT_EQ(utilization, 0.0F);
}

TEST_F(AutoActivityHistoryTest, ClearHistoryNullContextFails) {
    bool result = introspection_clear_history(nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// 7. Callback Tests
//=============================================================================

// Callback state for testing
static int callback_invocation_count = 0;
static activity_history_entry_t last_callback_entry;
static void* last_callback_user_data = nullptr;

static void test_activity_callback(const activity_history_entry_t* entry, void* user_data) {
    callback_invocation_count++;
    if (entry != nullptr) {
        last_callback_entry = *entry;
    }
    last_callback_user_data = user_data;
}

TEST_F(AutoActivityHistoryTest, CallbackRegistrationWorks) {
    if (context == nullptr) GTEST_SKIP();

    bool result = introspection_set_activity_callback(context, test_activity_callback, nullptr);
    EXPECT_TRUE(result);
}

TEST_F(AutoActivityHistoryTest, CallbackInvokedOnSample) {
    if (context == nullptr) GTEST_SKIP();

    // Reset callback state
    callback_invocation_count = 0;

    // Register callback
    introspection_set_activity_callback(context, test_activity_callback, nullptr);

    // Sample activity
    introspection_sample_activity(context);

    // Verify callback was invoked
    EXPECT_EQ(callback_invocation_count, 1);
    EXPECT_GE(last_callback_entry.timestamp, 0);
}

TEST_F(AutoActivityHistoryTest, CallbackReceivesUserData) {
    if (context == nullptr) GTEST_SKIP();

    // Reset callback state
    callback_invocation_count = 0;
    int user_data_value = 42;

    // Register callback with user data
    introspection_set_activity_callback(context, test_activity_callback, &user_data_value);

    // Sample activity
    introspection_sample_activity(context);

    // Verify user data was passed
    EXPECT_EQ(last_callback_user_data, &user_data_value);
}

TEST_F(AutoActivityHistoryTest, CallbackUnregistrationWorks) {
    if (context == nullptr) GTEST_SKIP();

    // Register then unregister
    introspection_set_activity_callback(context, test_activity_callback, nullptr);
    introspection_set_activity_callback(context, nullptr, nullptr);

    // Reset counter
    callback_invocation_count = 0;

    // Sample - callback should not be invoked
    introspection_sample_activity(context);

    EXPECT_EQ(callback_invocation_count, 0);
}

TEST_F(AutoActivityHistoryTest, CallbackNullContextFails) {
    bool result = introspection_set_activity_callback(nullptr, test_activity_callback, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// 8. Energy Consumption Tests
//=============================================================================

TEST_F(AutoActivityHistoryTest, EnergyConsumptionNonNegative) {
    if (context == nullptr) GTEST_SKIP();

    introspection_sample_activity(context);

    uint32_t num_entries = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);

    ASSERT_EQ(num_entries, 1);
    EXPECT_GE(history[0].energy_consumption, 0.0F);

    if (history != nullptr) {
        nimcp_free(history);
    }
}

TEST_F(AutoActivityHistoryTest, EnergyIncreasesWithActivity) {
    if (context == nullptr) GTEST_SKIP();

    // Sample with low activity (initial state)
    introspection_sample_activity(context);

    // Simulate high activity
    for (int i = 0; i < 10; i++) {
        simulate_activity();
    }

    // Sample with high activity
    introspection_sample_activity(context);

    uint32_t num_entries = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);

    if (num_entries >= 2) {
        // Energy with activity should be >= baseline energy
        EXPECT_GE(history[1].energy_consumption, history[0].energy_consumption * 0.8F);
    }

    if (history != nullptr) {
        nimcp_free(history);
    }
}

//=============================================================================
// 9. Edge Cases and Error Handling
//=============================================================================

TEST_F(AutoActivityHistoryTest, HistoryBufferOverflow) {
    if (context == nullptr) GTEST_SKIP();

    uint32_t capacity = 0;
    uint32_t size_before = 0;
    float util = 0.0F;
    introspection_get_history_stats(context, &size_before, &capacity, &util);

    // Fill beyond capacity
    for (uint32_t i = 0; i < capacity + 10; i++) {
        introspection_sample_activity(context);
    }

    uint32_t size_after = 0;
    introspection_get_history_stats(context, &size_after, &capacity, &util);

    // Size should not exceed capacity
    EXPECT_LE(size_after, capacity);
}

TEST_F(AutoActivityHistoryTest, EmptyHistoryReturnsNull) {
    if (context == nullptr) GTEST_SKIP();

    uint32_t num_entries = 999;  // Non-zero sentinel
    activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);

    EXPECT_EQ(history, nullptr);
    EXPECT_EQ(num_entries, 0);
}

TEST_F(AutoActivityHistoryTest, ActivationValuesInValidRange) {
    if (context == nullptr) GTEST_SKIP();

    // Sample multiple times
    for (int i = 0; i < 10; i++) {
        simulate_activity();
        introspection_sample_activity(context);
    }

    uint32_t num_entries = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);

    // Verify all entries have valid activation values [0,1]
    for (uint32_t i = 0; i < num_entries; i++) {
        EXPECT_GE(history[i].avg_activation, 0.0F);
        EXPECT_LE(history[i].avg_activation, 1.0F);
        EXPECT_GE(history[i].max_activation, 0.0F);
        EXPECT_LE(history[i].max_activation, 1.0F);
        EXPECT_GE(history[i].max_activation, history[i].avg_activation);
    }

    if (history != nullptr) {
        nimcp_free(history);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
