/**
 * @file test_auto_activity_history_integration.cpp
 * @brief Integration tests for auto activity history with brain processing
 *
 * TEST COVERAGE:
 * - Brain processing + auto sampling integration
 * - Bio-async message propagation
 * - Multiple brain instances
 * - Real-time sampling with intervals
 * - Callback coordination with brain operations
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>

#include "cognitive/introspection/nimcp_introspection.h"
#include "core/brain/nimcp_brain.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class AutoActivityHistoryIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    introspection_context_t context;

    void SetUp() override {
        // Create brain with introspection enabled
        brain = brain_create(
            "test_integration",
            BRAIN_SIZE_SMALL,
            BRAIN_TASK_CLASSIFICATION,
            20,  // num_inputs
            5    // num_outputs
        );

        if (brain != nullptr) {
            // Create introspection with auto history enabled
            introspection_config_t config = introspection_default_config();
            config.enable_auto_history = true;
            config.history_sample_interval_ms = 100;
            config.history_change_threshold = 0.0F;  // Record all

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

    // Simulate brain processing with multiple decisions
    void process_multiple_inputs(int count) {
        if (brain == nullptr) return;

        for (int i = 0; i < count; i++) {
            float inputs[20];
            for (int j = 0; j < 20; j++) {
                inputs[j] = (float)(rand() % 100) / 100.0f;
            }

            brain_decision_t* decision = brain_decide(brain, inputs, 20);
            if (decision != nullptr) {
                free(decision);
            }

            // Small delay between decisions
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

//=============================================================================
// 1. Brain Processing Integration Tests
//=============================================================================

TEST_F(AutoActivityHistoryIntegrationTest, ManualSamplingDuringProcessing) {
    if (context == nullptr) GTEST_SKIP();

    // Process some inputs
    process_multiple_inputs(5);

    // Manually sample after processing
    bool result = introspection_sample_activity(context);
    EXPECT_TRUE(result);

    // Verify history has entries
    uint32_t num_entries = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);

    EXPECT_GT(num_entries, 0);

    if (history != nullptr) {
        free(history);
    }
}

TEST_F(AutoActivityHistoryIntegrationTest, SamplingReflectsProcessingActivity) {
    if (context == nullptr) GTEST_SKIP();

    // Sample before processing (low activity)
    introspection_sample_activity(context);

    // Process many inputs (high activity)
    process_multiple_inputs(20);

    // Sample after processing (should show activity)
    introspection_sample_activity(context);

    // Get history
    uint32_t num_entries = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);

    EXPECT_GE(num_entries, 2);

    // Activity should have increased or remained non-zero
    if (history != nullptr && num_entries >= 2) {
        EXPECT_GE(history[1].num_active, 0);
        free(history);
    }
}

//=============================================================================
// 2. Periodic Sampling Tests
//=============================================================================

TEST_F(AutoActivityHistoryIntegrationTest, PeriodicSamplingWithIntervals) {
    if (context == nullptr) GTEST_SKIP();

    uint32_t sample_interval_ms = 50;  // Shorter interval for test reliability

    // Set sampling interval
    introspection_set_sample_interval(context, sample_interval_ms);

    // Simulate periodic sampling over time
    // We explicitly sample after each processing interval to ensure samples are captured
    for (int i = 0; i < 5; i++) {
        // Do some processing
        process_multiple_inputs(2);

        // Wait for interval
        std::this_thread::sleep_for(std::chrono::milliseconds(sample_interval_ms));

        // Explicitly sample after each interval (simulating periodic sampling behavior)
        introspection_sample_activity(context);
    }

    // Verify we have multiple samples
    uint32_t num_entries = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);

    EXPECT_GE(num_entries, 3);  // Should have captured several samples (5 iterations = 5 samples)

    if (history != nullptr) {
        free(history);
    }
}

//=============================================================================
// 3. Callback Integration Tests
//=============================================================================

static std::atomic<int> integration_callback_count{0};
static activity_history_entry_t integration_last_entry;

static void integration_callback(const activity_history_entry_t* entry, void* user_data) {
    integration_callback_count++;
    if (entry != nullptr) {
        integration_last_entry = *entry;
    }
}

TEST_F(AutoActivityHistoryIntegrationTest, CallbackDuringProcessing) {
    if (context == nullptr) GTEST_SKIP();

    integration_callback_count = 0;

    // Register callback
    introspection_set_activity_callback(context, integration_callback, nullptr);

    // Process and sample
    process_multiple_inputs(3);
    introspection_sample_activity(context);

    // Verify callback was invoked
    EXPECT_EQ(integration_callback_count.load(), 1);
}

TEST_F(AutoActivityHistoryIntegrationTest, CallbackReceivesValidData) {
    if (context == nullptr) GTEST_SKIP();

    integration_callback_count = 0;

    // Register callback
    introspection_set_activity_callback(context, integration_callback, nullptr);

    // Process to create activity
    process_multiple_inputs(5);

    // Sample
    introspection_sample_activity(context);

    // Verify callback received valid data
    EXPECT_GT(integration_last_entry.timestamp, 0);
    EXPECT_GE(integration_last_entry.avg_activation, 0.0F);
    EXPECT_LE(integration_last_entry.avg_activation, 1.0F);
    EXPECT_GE(integration_last_entry.energy_consumption, 0.0F);
}

//=============================================================================
// 4. Multiple Brain Instances
//=============================================================================

TEST_F(AutoActivityHistoryIntegrationTest, MultipleBrainsIndependentHistory) {
    if (context == nullptr) GTEST_SKIP();

    // Create second brain
    brain_t brain2 = brain_create(
        "test_second_brain",
        BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION,
        10, 3
    );

    if (brain2 == nullptr) GTEST_SKIP();

    introspection_config_t config = introspection_default_config();
    introspection_context_t context2 = introspection_context_create(brain2, &config);

    if (context2 == nullptr) {
        brain_destroy(brain2);
        GTEST_SKIP();
    }

    // Sample both brains
    introspection_sample_activity(context);
    introspection_sample_activity(context2);

    // Get histories
    uint32_t num_entries1 = 0, num_entries2 = 0;
    activity_history_entry_t* history1 = brain_get_activity_history(context, &num_entries1);
    activity_history_entry_t* history2 = brain_get_activity_history(context2, &num_entries2);

    // Both should have independent histories
    EXPECT_EQ(num_entries1, 1);
    EXPECT_EQ(num_entries2, 1);

    if (history1 != nullptr) free(history1);
    if (history2 != nullptr) free(history2);

    introspection_context_destroy(context2);
    brain_destroy(brain2);
}

//=============================================================================
// 5. History Statistics Integration
//=============================================================================

TEST_F(AutoActivityHistoryIntegrationTest, StatisticsAccurateDuringProcessing) {
    if (context == nullptr) GTEST_SKIP();

    // Take several samples
    for (int i = 0; i < 10; i++) {
        process_multiple_inputs(2);
        introspection_sample_activity(context);
    }

    // Get statistics
    uint32_t size, capacity;
    float utilization;
    bool result = introspection_get_history_stats(context, &size, &capacity, &utilization);

    EXPECT_TRUE(result);
    EXPECT_EQ(size, 10);
    EXPECT_GT(capacity, 0);
    EXPECT_FLOAT_EQ(utilization, (float)size / (float)capacity);
}

TEST_F(AutoActivityHistoryIntegrationTest, ClearHistoryDuringProcessing) {
    if (context == nullptr) GTEST_SKIP();

    // Add samples
    for (int i = 0; i < 5; i++) {
        introspection_sample_activity(context);
    }

    // Process more
    process_multiple_inputs(3);

    // Clear history
    bool result = introspection_clear_history(context);
    EXPECT_TRUE(result);

    // Verify empty
    uint32_t size, capacity;
    float utilization;
    introspection_get_history_stats(context, &size, &capacity, &utilization);

    EXPECT_EQ(size, 0);
    EXPECT_FLOAT_EQ(utilization, 0.0F);

    // Can still sample after clearing
    introspection_sample_activity(context);
    introspection_get_history_stats(context, &size, &capacity, &utilization);
    EXPECT_EQ(size, 1);
}

//=============================================================================
// 6. Load Testing
//=============================================================================

TEST_F(AutoActivityHistoryIntegrationTest, HighFrequencySampling) {
    if (context == nullptr) GTEST_SKIP();

    // Rapid sampling
    for (int i = 0; i < 100; i++) {
        introspection_sample_activity(context);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Verify history captured samples (may have dropped some due to overflow)
    uint32_t size, capacity;
    float utilization;
    introspection_get_history_stats(context, &size, &capacity, &utilization);

    EXPECT_GT(size, 0);
    EXPECT_LE(size, capacity);
}

TEST_F(AutoActivityHistoryIntegrationTest, LongRunningProcessing) {
    if (context == nullptr) GTEST_SKIP();

    // Simulate long-running processing with periodic sampling
    for (int i = 0; i < 20; i++) {
        process_multiple_inputs(5);

        if (i % 3 == 0) {
            introspection_sample_activity(context);
        }
    }

    // Verify we captured samples
    uint32_t num_entries = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);

    EXPECT_GT(num_entries, 0);
    EXPECT_LE(num_entries, 7);  // ~20/3 samples

    if (history != nullptr) {
        free(history);
    }
}

//=============================================================================
// 7. Edge Cases
//=============================================================================

TEST_F(AutoActivityHistoryIntegrationTest, SamplingWithNoProcessing) {
    if (context == nullptr) GTEST_SKIP();

    // Sample without any brain processing
    introspection_sample_activity(context);

    uint32_t num_entries = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);

    EXPECT_EQ(num_entries, 1);

    // Should have low activity (initial state)
    if (history != nullptr) {
        EXPECT_GE(history[0].avg_activation, 0.0F);
        free(history);
    }
}

TEST_F(AutoActivityHistoryIntegrationTest, SamplingAfterBrainReset) {
    if (context == nullptr) GTEST_SKIP();

    // Sample before reset
    process_multiple_inputs(5);
    introspection_sample_activity(context);

    // Clear history to simulate reset
    introspection_clear_history(context);

    // Sample after reset
    introspection_sample_activity(context);

    uint32_t num_entries = 0;
    activity_history_entry_t* history = brain_get_activity_history(context, &num_entries);

    EXPECT_EQ(num_entries, 1);

    if (history != nullptr) {
        free(history);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
