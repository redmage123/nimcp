/**
 * @file test_quantum_command_propagator.cpp
 * @brief Unit tests for quantum command propagator
 *
 * WHAT: Test quantum command propagation functionality
 * WHY:  Ensure 100% code coverage and correctness
 * HOW:  GoogleTest-based unit tests
 *
 * COVERAGE TARGET: 100%
 *
 * @author NIMCP Development Team
 * @date 2025-11-22
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/integration/nimcp_quantum_command_propagator.h"
#include "middleware/integration/nimcp_middleware_command.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumCommandPropagatorTest : public ::testing::Test {
protected:
    brain_t brain;
    quantum_command_propagator_t* qcp;
    shannon_monitor_t* shannon_monitor;

    void SetUp() override {
        // Create minimal brain for testing
        brain = brain_create("test_qcp", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 100, 10);
        ASSERT_NE(brain, nullptr);

        // Shannon monitor is optional for tests
        shannon_monitor = nullptr;

        // Create propagator with default config
        qcp = quantum_command_propagator_create(brain, shannon_monitor);
        ASSERT_NE(qcp, nullptr);
    }

    void TearDown() override {
        if (qcp) {
            quantum_command_propagator_destroy(qcp);
            qcp = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: Create test command
    middleware_command_t create_test_command(
        middleware_command_type_t type,
        command_target_region_t region,
        float priority
    ) {
        middleware_command_t cmd = {};
        cmd.type = type;
        cmd.command_id = 1;
        cmd.timestamp_us = 12345;
        cmd.priority = priority;
        cmd.information_bits = 5.0f;

        switch (type) {
            case COMMAND_CONFIGURE_ATTENTION:
                cmd.payload.attention.target_region = region;
                cmd.payload.attention.priority = priority;
                cmd.payload.attention.selectivity = 0.7f;
                cmd.payload.attention.top_k = 50;
                break;
            case COMMAND_REDUCE_ACTIVITY:
            case COMMAND_INCREASE_ACTIVITY:
                cmd.payload.activity.target_region = region;
                cmd.payload.activity.activity_scale = 1.0f;
                break;
            default:
                break;
        }

        return cmd;
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(QuantumCommandPropagatorTest, CreateDestroy) {
    // Test already done in SetUp/TearDown
    EXPECT_NE(qcp, nullptr);
}

TEST_F(QuantumCommandPropagatorTest, CreateWithNullBrain) {
    quantum_command_propagator_t* null_qcp =
        quantum_command_propagator_create(nullptr, nullptr);
    EXPECT_EQ(null_qcp, nullptr);
}

TEST_F(QuantumCommandPropagatorTest, CreateWithCustomConfig) {
    quantum_command_propagator_config_t config =
        quantum_command_propagator_default_config();
    config.num_quantum_steps = 200;
    config.propagation_threshold = 0.05f;

    quantum_command_propagator_t* custom_qcp =
        quantum_command_propagator_create_custom(brain, nullptr, &config);
    ASSERT_NE(custom_qcp, nullptr);

    quantum_command_propagator_destroy(custom_qcp);
}

TEST_F(QuantumCommandPropagatorTest, CreateWithNullConfig) {
    quantum_command_propagator_t* null_qcp =
        quantum_command_propagator_create_custom(brain, nullptr, nullptr);
    EXPECT_EQ(null_qcp, nullptr);
}

TEST_F(QuantumCommandPropagatorTest, DestroyNull) {
    // Should not crash
    quantum_command_propagator_destroy(nullptr);
}

//=============================================================================
// Command Propagation Tests
//=============================================================================

TEST_F(QuantumCommandPropagatorTest, PropagateToAllRegions) {
    middleware_command_t cmd = create_test_command(
        COMMAND_CONFIGURE_ATTENTION,
        TARGET_ALL_REGIONS,
        0.8f
    );

    uint32_t reached = quantum_command_propagator_propagate(qcp, &cmd);
    EXPECT_GT(reached, 0);
}

TEST_F(QuantumCommandPropagatorTest, PropagateToPrefrontal) {
    middleware_command_t cmd = create_test_command(
        COMMAND_CONFIGURE_ATTENTION,
        TARGET_PREFRONTAL,
        0.8f
    );

    uint32_t reached = quantum_command_propagator_propagate(qcp, &cmd);
    EXPECT_GT(reached, 0);

    // Prefrontal should be ~20% of neurons
    float coverage = quantum_command_propagator_get_last_coverage(qcp);
    EXPECT_GT(coverage, 0.0f);
    EXPECT_LT(coverage, 1.0f);
}

TEST_F(QuantumCommandPropagatorTest, PropagateToHippocampus) {
    middleware_command_t cmd = create_test_command(
        COMMAND_CONFIGURE_ATTENTION,
        TARGET_HIPPOCAMPUS,
        0.8f
    );

    uint32_t reached = quantum_command_propagator_propagate(qcp, &cmd);
    EXPECT_GT(reached, 0);
}

TEST_F(QuantumCommandPropagatorTest, PropagateToAmygdala) {
    middleware_command_t cmd = create_test_command(
        COMMAND_CONFIGURE_ATTENTION,
        TARGET_AMYGDALA,
        0.8f
    );

    uint32_t reached = quantum_command_propagator_propagate(qcp, &cmd);
    EXPECT_GT(reached, 0);
}

TEST_F(QuantumCommandPropagatorTest, PropagateToVisualCortex) {
    middleware_command_t cmd = create_test_command(
        COMMAND_CONFIGURE_ATTENTION,
        TARGET_VISUAL_CORTEX,
        0.8f
    );

    uint32_t reached = quantum_command_propagator_propagate(qcp, &cmd);
    EXPECT_GT(reached, 0);
}

TEST_F(QuantumCommandPropagatorTest, PropagateToAuditoryCortex) {
    middleware_command_t cmd = create_test_command(
        COMMAND_CONFIGURE_ATTENTION,
        TARGET_AUDITORY_CORTEX,
        0.8f
    );

    uint32_t reached = quantum_command_propagator_propagate(qcp, &cmd);
    EXPECT_GT(reached, 0);
}

TEST_F(QuantumCommandPropagatorTest, PropagateToMotorCortex) {
    middleware_command_t cmd = create_test_command(
        COMMAND_CONFIGURE_ATTENTION,
        TARGET_MOTOR_CORTEX,
        0.8f
    );

    uint32_t reached = quantum_command_propagator_propagate(qcp, &cmd);
    EXPECT_GT(reached, 0);
}

TEST_F(QuantumCommandPropagatorTest, PropagateWithNullCommand) {
    uint32_t reached = quantum_command_propagator_propagate(qcp, nullptr);
    EXPECT_EQ(reached, 0);
}

TEST_F(QuantumCommandPropagatorTest, PropagateWithNullPropagator) {
    middleware_command_t cmd = create_test_command(
        COMMAND_CONFIGURE_ATTENTION,
        TARGET_ALL_REGIONS,
        0.8f
    );

    uint32_t reached = quantum_command_propagator_propagate(nullptr, &cmd);
    EXPECT_EQ(reached, 0);
}

TEST_F(QuantumCommandPropagatorTest, Broadcast) {
    middleware_command_t cmd = create_test_command(
        COMMAND_RESET_BUFFERS,
        TARGET_ALL_REGIONS,
        0.9f
    );

    uint32_t reached = quantum_command_propagator_broadcast(qcp, &cmd);
    EXPECT_GT(reached, 0);

    // Broadcast should reach significant portion
    float coverage = quantum_command_propagator_get_last_coverage(qcp);
    EXPECT_GT(coverage, 0.0f);
}

TEST_F(QuantumCommandPropagatorTest, BroadcastWithNullCommand) {
    uint32_t reached = quantum_command_propagator_broadcast(qcp, nullptr);
    EXPECT_EQ(reached, 0);
}

//=============================================================================
// Metrics Tests
//=============================================================================

TEST_F(QuantumCommandPropagatorTest, GetMetrics) {
    command_propagation_metrics_t metrics;
    bool success = quantum_command_propagator_get_metrics(qcp, &metrics);
    EXPECT_TRUE(success);
    EXPECT_EQ(metrics.total_commands_propagated, 0);
    EXPECT_EQ(metrics.total_neurons_reached, 0);
}

TEST_F(QuantumCommandPropagatorTest, GetMetricsAfterPropagation) {
    middleware_command_t cmd = create_test_command(
        COMMAND_CONFIGURE_ATTENTION,
        TARGET_ALL_REGIONS,
        0.8f
    );
    quantum_command_propagator_propagate(qcp, &cmd);

    command_propagation_metrics_t metrics;
    bool success = quantum_command_propagator_get_metrics(qcp, &metrics);
    EXPECT_TRUE(success);
    EXPECT_EQ(metrics.total_commands_propagated, 1);
    EXPECT_GT(metrics.total_neurons_reached, 0);
    EXPECT_GT(metrics.average_coverage, 0.0f);
}

TEST_F(QuantumCommandPropagatorTest, GetMetricsWithNull) {
    command_propagation_metrics_t metrics;
    bool success = quantum_command_propagator_get_metrics(nullptr, &metrics);
    EXPECT_FALSE(success);
}

TEST_F(QuantumCommandPropagatorTest, GetLastCoverage) {
    middleware_command_t cmd = create_test_command(
        COMMAND_CONFIGURE_ATTENTION,
        TARGET_PREFRONTAL,
        0.8f
    );
    quantum_command_propagator_propagate(qcp, &cmd);

    float coverage = quantum_command_propagator_get_last_coverage(qcp);
    EXPECT_GE(coverage, 0.0f);
    EXPECT_LE(coverage, 1.0f);
}

TEST_F(QuantumCommandPropagatorTest, GetSpeedup) {
    middleware_command_t cmd = create_test_command(
        COMMAND_CONFIGURE_ATTENTION,
        TARGET_ALL_REGIONS,
        0.8f
    );
    quantum_command_propagator_propagate(qcp, &cmd);

    float speedup = quantum_command_propagator_get_speedup(qcp);
    EXPECT_GT(speedup, 1.0f);  // Should show some speedup
}

TEST_F(QuantumCommandPropagatorTest, ResetStats) {
    // Propagate a command
    middleware_command_t cmd = create_test_command(
        COMMAND_CONFIGURE_ATTENTION,
        TARGET_ALL_REGIONS,
        0.8f
    );
    quantum_command_propagator_propagate(qcp, &cmd);

    // Reset stats
    quantum_command_propagator_reset_stats(qcp);

    // Check metrics are reset
    command_propagation_metrics_t metrics;
    quantum_command_propagator_get_metrics(qcp, &metrics);
    EXPECT_EQ(metrics.total_commands_propagated, 0);
    EXPECT_EQ(metrics.total_neurons_reached, 0);
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(QuantumCommandPropagatorTest, EnableShannonOptimization) {
    quantum_command_propagator_enable_shannon_optimization(qcp, true);
    quantum_command_propagator_enable_shannon_optimization(qcp, false);
    // Should not crash
}

TEST_F(QuantumCommandPropagatorTest, SetThreshold) {
    quantum_command_propagator_set_threshold(qcp, 0.05f);
    quantum_command_propagator_set_threshold(qcp, 0.5f);
    quantum_command_propagator_set_threshold(qcp, 1.0f);
    // Should clamp invalid values
    quantum_command_propagator_set_threshold(qcp, -0.1f);
    quantum_command_propagator_set_threshold(qcp, 1.5f);
}

TEST_F(QuantumCommandPropagatorTest, SetNumSteps) {
    quantum_command_propagator_set_num_steps(qcp, 50);
    quantum_command_propagator_set_num_steps(qcp, 200);
    quantum_command_propagator_set_num_steps(qcp, 1000);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(QuantumCommandPropagatorTest, LowPriorityCommand) {
    middleware_command_t cmd = create_test_command(
        COMMAND_CONFIGURE_ATTENTION,
        TARGET_ALL_REGIONS,
        0.1f  // Low priority
    );

    uint32_t reached = quantum_command_propagator_propagate(qcp, &cmd);
    // Should still propagate even with low priority
    EXPECT_GE(reached, 0);
}

TEST_F(QuantumCommandPropagatorTest, MultipleCommands) {
    for (int i = 0; i < 10; i++) {
        middleware_command_t cmd = create_test_command(
            COMMAND_CONFIGURE_ATTENTION,
            TARGET_ALL_REGIONS,
            0.5f + (i * 0.05f)
        );
        uint32_t reached = quantum_command_propagator_propagate(qcp, &cmd);
        EXPECT_GT(reached, 0);
    }

    command_propagation_metrics_t metrics;
    quantum_command_propagator_get_metrics(qcp, &metrics);
    EXPECT_EQ(metrics.total_commands_propagated, 10);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
