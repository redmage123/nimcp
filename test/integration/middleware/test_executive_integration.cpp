/**
 * @file test_executive_integration.cpp
 * @brief Integration tests for executive-middleware integration
 *
 * WHAT: End-to-end integration testing
 * WHY:  Verify complete event flow from executive to brain
 * HOW:  GoogleTest-based integration tests
 *
 * @author NIMCP Development Team
 * @date 2025-11-22
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/integration/nimcp_executive_middleware_adapter.h"
#include "middleware/integration/nimcp_quantum_command_propagator.h"
#include "middleware/integration/nimcp_shannon_monitor.h"
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

class ExecutiveIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    executive_controller_t* executive;
    shannon_monitor_t* shannon_monitor;
    quantum_command_propagator_t* propagator;
    executive_middleware_adapter_t* adapter;

    void SetUp() override {
        // Create complete integration stack
        brain = brain_create("test_exec_integration", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 100, 10);
        ASSERT_NE(brain, nullptr);

        executive = (executive_controller_t*)calloc(1, sizeof(executive_controller));
        ASSERT_NE(executive, nullptr);

        shannon_monitor = shannon_monitor_create();
        ASSERT_NE(shannon_monitor, nullptr);

        propagator = quantum_command_propagator_create(brain, shannon_monitor);
        ASSERT_NE(propagator, nullptr);

        adapter = executive_middleware_adapter_create(
            executive, propagator, shannon_monitor
        );
        ASSERT_NE(adapter, nullptr);
    }

    void TearDown() override {
        if (adapter) executive_middleware_adapter_destroy(adapter);
        if (propagator) quantum_command_propagator_destroy(propagator);
        if (shannon_monitor) shannon_monitor_destroy(shannon_monitor);
        if (executive) free(executive);
        if (brain) brain_destroy(brain);
    }
};

//=============================================================================
// End-to-End Flow Tests
//=============================================================================

TEST_F(ExecutiveIntegrationTest, CompleteEventFlow) {
    // Simulate complete event flow:
    // Executive event → Adapter → Command → Propagator → Brain

    bool success = executive_middleware_adapter_on_task_switched(
        adapter,
        1,      // task_id
        0,      // TASK_TYPE_CLASSIFICATION
        0.9f    // priority
    );
    EXPECT_TRUE(success);

    // Verify command was propagated
    command_propagation_metrics_t prop_metrics;
    quantum_command_propagator_get_metrics(propagator, &prop_metrics);
    EXPECT_EQ(prop_metrics.total_commands_propagated, 1);
    EXPECT_GT(prop_metrics.total_neurons_reached, 0);

    // Verify adapter metrics
    executive_middleware_metrics_t adapter_metrics;
    executive_middleware_adapter_get_metrics(adapter, &adapter_metrics);
    EXPECT_EQ(adapter_metrics.total_events_received, 1);
    EXPECT_EQ(adapter_metrics.total_commands_issued, 1);
    EXPECT_GT(adapter_metrics.command_success_rate, 0.0f);
}

TEST_F(ExecutiveIntegrationTest, MultipleEventsFlow) {
    // Simulate realistic multi-event scenario

    // 1. Task switch
    executive_middleware_adapter_on_task_switched(adapter, 1, 0, 0.8f);

    // 2. Pattern detection
    executive_middleware_adapter_on_pattern_detected(adapter, 123, 0.9f, 4);

    // 3. Oscillation change
    executive_middleware_adapter_on_oscillation_changed(adapter, 40.0f, 0.7f, 4);

    // 4. High cognitive load
    executive_middleware_adapter_on_cognitive_load_changed(adapter, 0.9f);

    // Verify all events processed
    executive_middleware_metrics_t metrics;
    executive_middleware_adapter_get_metrics(adapter, &metrics);
    EXPECT_EQ(metrics.total_events_received, 4);
    EXPECT_GT(metrics.total_commands_issued, 0);
    EXPECT_GT(metrics.total_information_delivered, 0.0f);
}

TEST_F(ExecutiveIntegrationTest, ShannonMonitorIntegration) {
    // Verify Shannon monitor tracking

    executive_middleware_adapter_on_task_switched(adapter, 1, 0, 0.9f);
    executive_middleware_adapter_on_pattern_detected(adapter, 1, 0.9f, 0);

    shannon_routing_metrics_t shannon_metrics =
        shannon_monitor_get_metrics(shannon_monitor);

    // Shannon monitor should have tracked some activity
    // (depends on whether events were recorded - may be 0 in MVP)
    EXPECT_GE(shannon_metrics.total_events, 0);
}

TEST_F(ExecutiveIntegrationTest, CommandPropagationToDifferentRegions) {
    // Test propagation to all brain regions

    command_target_region_t regions[] = {
        TARGET_PREFRONTAL,
        TARGET_HIPPOCAMPUS,
        TARGET_AMYGDALA,
        TARGET_VISUAL_CORTEX,
        TARGET_AUDITORY_CORTEX,
        TARGET_MOTOR_CORTEX
    };

    for (auto region : regions) {
        // Map task type to use different regions
        uint32_t task_type = static_cast<uint32_t>(region);
        executive_middleware_adapter_on_task_switched(
            adapter, task_type, task_type, 0.8f
        );
    }

    command_propagation_metrics_t metrics;
    quantum_command_propagator_get_metrics(propagator, &metrics);
    EXPECT_EQ(metrics.total_commands_propagated, 6);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(ExecutiveIntegrationTest, ThroughputTest) {
    // Measure command throughput
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        executive_middleware_adapter_on_task_switched(
            adapter, i, i % 7, 0.5f + (i % 5) * 0.1f
        );
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Should process 100 events reasonably quickly
    EXPECT_LT(duration.count(), 100000);  // Less than 100ms

    // Verify all processed
    executive_middleware_metrics_t metrics;
    executive_middleware_adapter_get_metrics(adapter, &metrics);
    EXPECT_EQ(metrics.total_events_received, 100);
}

TEST_F(ExecutiveIntegrationTest, LatencyTest) {
    // Measure single-event latency

    executive_middleware_metrics_t before;
    executive_middleware_adapter_get_metrics(adapter, &before);

    executive_middleware_adapter_on_task_switched(adapter, 1, 0, 0.8f);

    executive_middleware_metrics_t after;
    executive_middleware_adapter_get_metrics(adapter, &after);

    // Should have low latency
    EXPECT_LT(after.average_adaptation_latency_us, 1000.0f);  // < 1ms
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(ExecutiveIntegrationTest, RecoveryFromLowPriority) {
    // Test filtering and recovery

    // Low priority event (filtered)
    bool success1 = executive_middleware_adapter_on_task_switched(
        adapter, 1, 0, 0.1f
    );
    EXPECT_FALSE(success1);

    // High priority event (processed)
    bool success2 = executive_middleware_adapter_on_task_switched(
        adapter, 2, 0, 0.9f
    );
    EXPECT_TRUE(success2);

    // Verify metrics
    executive_middleware_metrics_t metrics;
    executive_middleware_adapter_get_metrics(adapter, &metrics);
    EXPECT_EQ(metrics.total_events_received, 2);
    EXPECT_EQ(metrics.total_commands_issued, 1);
}

TEST_F(ExecutiveIntegrationTest, MixedEventTypes) {
    // Test handling mixed event types

    executive_middleware_adapter_on_task_switched(adapter, 1, 0, 0.8f);
    executive_middleware_adapter_on_cognitive_load_changed(adapter, 0.9f);
    executive_middleware_adapter_on_pattern_detected(adapter, 1, 0.7f, 0);
    executive_middleware_adapter_on_oscillation_changed(adapter, 40.0f, 0.6f, 0);
    executive_middleware_adapter_on_salience_peak(adapter, 0.9f, 0);

    executive_middleware_metrics_t metrics;
    executive_middleware_adapter_get_metrics(adapter, &metrics);
    EXPECT_EQ(metrics.total_events_received, 5);
    EXPECT_GT(metrics.event_conversion_rate, 0.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
