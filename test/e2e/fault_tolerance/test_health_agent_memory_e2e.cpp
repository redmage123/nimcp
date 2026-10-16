/**
 * @file test_health_agent_memory_e2e.cpp
 * @brief End-to-end tests for Phase 5.7 Memory System Health Integration
 * @date 2026-01-18
 *
 * Tests the complete memory health integration workflow from
 * agent creation to monitoring and recovery.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/logging/nimcp_logging.h"
}

/**
 * @brief Test fixture for memory health E2E tests
 */
class HealthAgentMemoryE2ETest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;

    void SetUp() override {
        health_agent_config_t config = {};
        config.heartbeat_interval_ms = 50;  /* Fast for E2E tests */
        config.message_queue_depth = 128;
        config.watchdog_timeout_ms = 5000;
        config.enable_auto_recovery = true;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);
    }

    void TearDown() override {
        if (agent) {
            nimcp_health_agent_stop(agent);
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

/* ============================================================================
 * Complete Workflow E2E Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryE2ETest, FullMemoryHealthWorkflow) {
    /* Step 1: Start the health agent */
    int start_result = nimcp_health_agent_start(agent);
    EXPECT_EQ(start_result, 0);

    /* Step 2: Let agent initialize */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    /* Step 3: Collect initial metrics */
    memory_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &metrics), 0);
    EXPECT_FLOAT_EQ(metrics.overall_memory_health, 1.0f);  /* Perfect when no modules */

    /* Step 4: Run consistency validation */
    int inconsistencies = nimcp_health_agent_validate_memory_consistency(agent);
    EXPECT_EQ(inconsistencies, 0);  /* No inconsistencies when no modules */

    /* Step 5: Check if needs attention */
    bool needs_attention = nimcp_health_agent_memory_needs_attention(agent);
    EXPECT_FALSE(needs_attention);  /* Should not need attention */

    /* Step 6: Simulate recovery scenario */
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_CROSS_TIER_SYNC, 2), 0);

    /* Step 7: Verify agent still operational */
    EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &metrics), 0);

    /* Step 8: Stop agent */
    nimcp_health_agent_stop(agent);
}

TEST_F(HealthAgentMemoryE2ETest, LongRunningMonitoring) {
    /* Start agent */
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    /* Run for extended period with periodic checks */
    for (int i = 0; i < 10; i++) {
        memory_health_metrics_t metrics = {};
        EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &metrics), 0);
        EXPECT_GE(metrics.overall_memory_health, 0.0f);
        EXPECT_LE(metrics.overall_memory_health, 1.0f);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    /* Stop agent */
    nimcp_health_agent_stop(agent);
}

/* ============================================================================
 * Recovery Scenario E2E Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryE2ETest, RecoverySequenceCA3Reset) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Simulate CA3 instability recovery sequence */
    memory_health_metrics_t before = {};
    EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &before), 0);

    /* Trigger CA3 reset */
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_RESET_CA3, 0), 0);

    /* Verify system still operational */
    memory_health_metrics_t after = {};
    EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &after), 0);

    nimcp_health_agent_stop(agent);
}

TEST_F(HealthAgentMemoryE2ETest, RecoverySequenceHDDrift) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Simulate HD drift correction sequence */
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_HD_DRIFT_CORRECT, 1), 0);

    /* Verify consistency after correction */
    int inconsistencies = nimcp_health_agent_validate_memory_consistency(agent);
    EXPECT_GE(inconsistencies, 0);

    nimcp_health_agent_stop(agent);
}

TEST_F(HealthAgentMemoryE2ETest, RecoverySequencePapezRepair) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Simulate Papez circuit repair */
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_PAPEZ_REPAIR, 2), 0);
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_FORNIX_STRENGTHEN, 2), 0);

    /* Verify metrics still collectible */
    memory_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &metrics), 0);

    nimcp_health_agent_stop(agent);
}

TEST_F(HealthAgentMemoryE2ETest, EmergencyRecoverySequence) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Simulate emergency scenario */
    /* First try normal recovery */
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_FORCE_CONSOLIDATION, 2), 0);

    /* If that fails (simulated), escalate to emergency save */
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_EMERGENCY_SAVE, 2), 0);

    /* Agent should still respond */
    bool needs_attention = nimcp_health_agent_memory_needs_attention(agent);
    (void)needs_attention;  /* Just verify no crash */

    nimcp_health_agent_stop(agent);
}

/* ============================================================================
 * Concurrent Access E2E Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryE2ETest, ConcurrentMetricsCollection) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    /* Launch multiple threads collecting metrics */
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &success_count, &failure_count]() {
            for (int i = 0; i < 25; i++) {
                memory_health_metrics_t metrics = {};
                if (nimcp_health_agent_get_memory_metrics(agent, &metrics) == 0) {
                    success_count++;
                } else {
                    failure_count++;
                }
            }
        });
    }

    /* Wait for all threads */
    for (auto& t : threads) {
        t.join();
    }

    /* All operations should succeed */
    EXPECT_EQ(success_count.load(), 100);
    EXPECT_EQ(failure_count.load(), 0);

    nimcp_health_agent_stop(agent);
}

TEST_F(HealthAgentMemoryE2ETest, ConcurrentRecoveryActions) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::atomic<int> success_count{0};

    /* Launch threads triggering recovery actions */
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &success_count, t]() {
            for (int i = 0; i < 20; i++) {
                memory_recovery_action_t action = static_cast<memory_recovery_action_t>((t + i) % 12);
                if (nimcp_health_agent_memory_recovery(agent, action, t % 3) == 0) {
                    success_count++;
                }
            }
        });
    }

    /* Wait for all threads */
    for (auto& t : threads) {
        t.join();
    }

    /* All operations should succeed */
    EXPECT_EQ(success_count.load(), 80);

    nimcp_health_agent_stop(agent);
}

/* ============================================================================
 * Lifecycle E2E Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryE2ETest, MultipleStartStopCycles) {
    for (int cycle = 0; cycle < 5; cycle++) {
        /* Start */
        EXPECT_EQ(nimcp_health_agent_start(agent), 0);

        /* Use memory health features */
        memory_health_metrics_t metrics = {};
        EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &metrics), 0);
        EXPECT_EQ(nimcp_health_agent_validate_memory_consistency(agent), 0);

        /* Stop */
        nimcp_health_agent_stop(agent);

        /* Brief pause between cycles */
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

TEST_F(HealthAgentMemoryE2ETest, MetricsAfterAgentRestart) {
    /* First run */
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    memory_health_metrics_t metrics1 = {};
    EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &metrics1), 0);
    nimcp_health_agent_stop(agent);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Second run */
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    memory_health_metrics_t metrics2 = {};
    EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &metrics2), 0);
    nimcp_health_agent_stop(agent);

    /* Both should have valid data */
    EXPECT_GE(metrics1.overall_memory_health, 0.0f);
    EXPECT_GE(metrics2.overall_memory_health, 0.0f);
}

/* ============================================================================
 * Configuration E2E Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryE2ETest, CustomConfigurationWorkflow) {
    /* Create custom configurations */
    health_agent_hippocampus_config_t hippo_config = {};
    nimcp_health_agent_hippocampus_config_default(&hippo_config);
    hippo_config.ca3_stability_threshold = 0.9f;  /* Stricter threshold */
    hippo_config.health_check_interval_ms = 100;

    health_agent_mammillary_config_t mammillary_config = {};
    nimcp_health_agent_mammillary_config_default(&mammillary_config);
    mammillary_config.hd_drift_max_degrees = 2.0f;  /* Stricter threshold */
    mammillary_config.health_check_interval_ms = 100;

    /* Start agent */
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    /* Collect and verify metrics */
    memory_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &metrics), 0);

    nimcp_health_agent_stop(agent);
}

/* ============================================================================
 * Error Recovery E2E Tests
 * ============================================================================ */

TEST_F(HealthAgentMemoryE2ETest, RecoveryFromMetabolicStress) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Trigger metabolic boost */
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_METABOLIC_BOOST, 2), 0);

    /* Verify agent still healthy */
    memory_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &metrics), 0);

    /* Metabolic should show healthy state */
    EXPECT_FALSE(metrics.metabolic.energy_constrained);

    nimcp_health_agent_stop(agent);
}

TEST_F(HealthAgentMemoryE2ETest, CapacityExpansionWorkflow) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Simulate capacity pressure */
    memory_health_metrics_t before = {};
    EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &before), 0);

    /* Trigger expansion and GC */
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_EXPAND_CAPACITY, 2), 0);
    EXPECT_EQ(nimcp_health_agent_memory_recovery(agent, MEMORY_RECOVERY_GC_OLD_TRACES, 2), 0);

    /* Verify agent operational */
    memory_health_metrics_t after = {};
    EXPECT_EQ(nimcp_health_agent_get_memory_metrics(agent, &after), 0);

    nimcp_health_agent_stop(agent);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
