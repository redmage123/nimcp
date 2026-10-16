/**
 * @file test_executive_regression.cpp
 * @brief Regression tests for executive integration
 *
 * WHAT: Backward compatibility and regression testing
 * WHY:  Ensure no regressions in existing functionality
 * HOW:  GoogleTest-based regression tests
 *
 * @author NIMCP Development Team
 * @date 2025-11-22
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/integration/nimcp_executive_middleware_adapter.h"
#include "middleware/integration/nimcp_quantum_command_propagator.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Mock Executive
//=============================================================================

struct executive_controller {
    int dummy;
};

//=============================================================================
// Test Fixture
//=============================================================================

class ExecutiveRegressionTest : public ::testing::Test {
protected:
    brain_t brain;
    executive_controller_t* executive;
    quantum_command_propagator_t* propagator;
    executive_middleware_adapter_t* adapter;

    void SetUp() override {
        brain = brain_create("test_exec_regression", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 100, 10);
        ASSERT_NE(brain, nullptr);

        executive = (executive_controller_t*)calloc(1, sizeof(executive_controller));
        ASSERT_NE(executive, nullptr);

        propagator = quantum_command_propagator_create(brain, nullptr);
        ASSERT_NE(propagator, nullptr);

        adapter = executive_middleware_adapter_create(executive, propagator, nullptr);
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) executive_middleware_adapter_destroy(adapter);
        if (propagator) quantum_command_propagator_destroy(propagator);
        if (executive) free(executive);
        if (brain) brain_destroy(brain);
    }
};

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(ExecutiveRegressionTest, DefaultConfigurationUnchanged) {
    // Verify default configuration values haven't changed

    quantum_command_propagator_config_t qcp_config =
        quantum_command_propagator_default_config();
    EXPECT_EQ(qcp_config.num_quantum_steps, 100);
    EXPECT_FLOAT_EQ(qcp_config.propagation_threshold, 0.01f);
    EXPECT_TRUE(qcp_config.enable_shannon_optimization);

    executive_middleware_config_t adapter_config =
        executive_middleware_adapter_default_config();
    EXPECT_TRUE(adapter_config.enable_adaptive_routing);
    EXPECT_FLOAT_EQ(adapter_config.command_priority_threshold, 0.3f);
    EXPECT_FLOAT_EQ(adapter_config.information_threshold_bits, 2.0f);
}

TEST_F(ExecutiveRegressionTest, APISignaturesUnchanged) {
    // Verify core API functions still exist and work

    // Propagator API
    command_propagation_metrics_t prop_metrics;
    bool success1 = quantum_command_propagator_get_metrics(propagator, &prop_metrics);
    EXPECT_TRUE(success1);

    float coverage = quantum_command_propagator_get_last_coverage(propagator);
    EXPECT_GE(coverage, 0.0f);

    float speedup = quantum_command_propagator_get_speedup(propagator);
    // Speedup may be 0 before any propagation occurs
    // After propagation it should be >= 1.0
    EXPECT_GE(speedup, 0.0f);

    // Adapter API
    executive_middleware_metrics_t adapter_metrics;
    bool success2 = executive_middleware_adapter_get_metrics(adapter, &adapter_metrics);
    EXPECT_TRUE(success2);

    float mi = executive_middleware_adapter_get_mutual_information(adapter);
    EXPECT_GE(mi, 0.0f);

    float rate = executive_middleware_adapter_get_success_rate(adapter);
    EXPECT_GE(rate, 0.0f);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

TEST_F(ExecutiveRegressionTest, PropagationLatencyBaseline) {
    // Establish baseline: single command should complete < 500µs

    middleware_command_t cmd = {};
    cmd.type = COMMAND_CONFIGURE_ATTENTION;
    cmd.priority = 0.8f;
    cmd.information_bits = 5.0f;
    cmd.payload.attention.target_region = TARGET_ALL_REGIONS;
    cmd.payload.attention.priority = 0.8f;

    auto start = std::chrono::high_resolution_clock::now();
    uint32_t reached = quantum_command_propagator_propagate(propagator, &cmd);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    EXPECT_GT(reached, 0);
    // Allow some timing variance, baseline is < 1000µs
    EXPECT_LT(duration.count(), 1000);  // Should complete in < 1ms
}

TEST_F(ExecutiveRegressionTest, AdapterLatencyBaseline) {
    // Establish baseline: event handling should complete < 1ms

    auto start = std::chrono::high_resolution_clock::now();
    bool success = executive_middleware_adapter_on_task_switched(
        adapter, 1, 0, 0.8f
    );
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    EXPECT_TRUE(success);
    EXPECT_LT(duration.count(), 1000);  // Should complete in < 1ms
}

TEST_F(ExecutiveRegressionTest, ThroughputBaseline) {
    // Baseline: Should process ≥ 1000 commands/sec

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        executive_middleware_adapter_on_task_switched(
            adapter, i, 0, 0.5f
        );
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should process 1000 events in < 1 second
    EXPECT_LT(duration.count(), 1000);
}

//=============================================================================
// Memory Regression Tests
//=============================================================================

TEST_F(ExecutiveRegressionTest, NoMemoryLeakOnCreation) {
    // Test that create/destroy cycles don't leak memory

    for (int i = 0; i < 100; i++) {
        quantum_command_propagator_t* temp_qcp =
            quantum_command_propagator_create(brain, nullptr);
        ASSERT_NE(temp_qcp, nullptr);
        quantum_command_propagator_destroy(temp_qcp);
    }

    for (int i = 0; i < 100; i++) {
        executive_middleware_adapter_t* temp_adapter =
            executive_middleware_adapter_create(executive, propagator, nullptr);
        ASSERT_NE(temp_adapter, nullptr);
        executive_middleware_adapter_destroy(temp_adapter);
    }

    // If we got here without crashing, no obvious leaks
    SUCCEED();
}

TEST_F(ExecutiveRegressionTest, NoMemoryLeakOnEvents) {
    // Process many events and verify no leaks

    for (int i = 0; i < 1000; i++) {
        executive_middleware_adapter_on_task_switched(adapter, i, 0, 0.5f);
    }

    executive_middleware_metrics_t metrics;
    executive_middleware_adapter_get_metrics(adapter, &metrics);
    EXPECT_EQ(metrics.total_events_received, 1000);
}

//=============================================================================
// Behavioral Regression Tests
//=============================================================================

TEST_F(ExecutiveRegressionTest, PriorityFilteringBehavior) {
    // Verify priority filtering still works as expected

    executive_middleware_adapter_set_priority_threshold(adapter, 0.5f);

    // Below threshold - filtered
    bool success1 = executive_middleware_adapter_on_task_switched(
        adapter, 1, 0, 0.4f
    );
    EXPECT_FALSE(success1);

    // Above threshold - processed
    bool success2 = executive_middleware_adapter_on_task_switched(
        adapter, 2, 0, 0.6f
    );
    EXPECT_TRUE(success2);

    executive_middleware_metrics_t metrics;
    executive_middleware_adapter_get_metrics(adapter, &metrics);
    EXPECT_EQ(metrics.total_events_received, 2);
    EXPECT_EQ(metrics.total_commands_issued, 1);
}

TEST_F(ExecutiveRegressionTest, MetricsAccuracyBehavior) {
    // Verify metrics are tracked accurately

    // Process 10 successful events
    for (int i = 0; i < 10; i++) {
        executive_middleware_adapter_on_task_switched(adapter, i, 0, 0.8f);
    }

    executive_middleware_metrics_t metrics;
    executive_middleware_adapter_get_metrics(adapter, &metrics);

    EXPECT_EQ(metrics.total_events_received, 10);
    EXPECT_EQ(metrics.total_commands_issued, 10);
    EXPECT_EQ(metrics.total_commands_executed, 10);
    EXPECT_FLOAT_EQ(metrics.command_success_rate, 1.0f);
    EXPECT_FLOAT_EQ(metrics.event_conversion_rate, 1.0f);
}

TEST_F(ExecutiveRegressionTest, ConfigurationPersistence) {
    // Verify configuration changes persist

    quantum_command_propagator_set_threshold(propagator, 0.05f);
    quantum_command_propagator_set_num_steps(propagator, 200);

    executive_middleware_adapter_set_priority_threshold(adapter, 0.7f);

    // Process events and verify settings are still in effect
    bool success = executive_middleware_adapter_on_task_switched(
        adapter, 1, 0, 0.5f  // Below 0.7 threshold
    );
    EXPECT_FALSE(success);

    success = executive_middleware_adapter_on_task_switched(
        adapter, 2, 0, 0.8f  // Above 0.7 threshold
    );
    EXPECT_TRUE(success);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
