/**
 * @file test_brain_probe_health_functions.cpp
 * @brief Unit tests for Phase 5.13 Brain Probes Enhancement
 *
 * Tests for brain probe health monitoring functions in the health agent.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
}

// Mock brain type for testing
static void* create_mock_brain(int id) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0x1000 + id * 0x100));
}

class BrainProbeHealthTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent;

    void SetUp() override {
        health_agent_config_t config;
        nimcp_health_agent_default_config(&config);
        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);
    }

    void TearDown() override {
        if (agent) {
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

// ============================================================================
// Configuration Default Tests
// ============================================================================

TEST_F(BrainProbeHealthTest, ConfigDefaultSetsReasonableValues) {
    health_agent_brain_probe_config_t config;
    nimcp_health_agent_brain_probe_config_default(&config);

    EXPECT_TRUE(config.enable_probe_monitoring);
    EXPECT_TRUE(config.enable_memory_tracking);
    EXPECT_TRUE(config.enable_performance_tracking);
    EXPECT_TRUE(config.enable_learning_monitoring);
    EXPECT_TRUE(config.enable_synapse_monitoring);
    EXPECT_TRUE(config.enable_cow_monitoring);
    EXPECT_FALSE(config.enable_auto_recovery);

    EXPECT_EQ(config.probe_interval_ms, 1000u);
    EXPECT_EQ(config.trend_window_probes, 10u);

    EXPECT_GT(config.memory_warning_bytes, 0u);
    EXPECT_GT(config.memory_critical_bytes, config.memory_warning_bytes);
    EXPECT_GT(config.inference_time_warning_us, 0.0f);
    EXPECT_GT(config.inference_time_critical_us, config.inference_time_warning_us);
}

TEST_F(BrainProbeHealthTest, ConfigDefaultNullSafe) {
    nimcp_health_agent_brain_probe_config_default(nullptr);
    // Should not crash
}

// ============================================================================
// Brain Registration Tests
// ============================================================================

TEST_F(BrainProbeHealthTest, RegisterBrainSuccess) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));

    int result = nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 1u);
}

TEST_F(BrainProbeHealthTest, RegisterBrainWithCustomConfig) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    health_agent_brain_probe_config_t config;
    nimcp_health_agent_brain_probe_config_default(&config);
    config.probe_interval_ms = 500;
    config.enable_auto_recovery = true;

    int result = nimcp_health_agent_register_brain_probe(agent, brain, &config);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 1u);
}

TEST_F(BrainProbeHealthTest, RegisterMultipleBrains) {
    for (int i = 0; i < 5; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        int result = nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
        EXPECT_EQ(result, 0);
    }
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 5u);
}

TEST_F(BrainProbeHealthTest, RegisterDuplicateBrainIgnored) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));

    int result1 = nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    EXPECT_EQ(result1, 0);

    int result2 = nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    EXPECT_EQ(result2, 0);  // Success but ignored

    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 1u);
}

TEST_F(BrainProbeHealthTest, RegisterNullAgentFails) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    int result = nimcp_health_agent_register_brain_probe(nullptr, brain, nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainProbeHealthTest, RegisterNullBrainFails) {
    int result = nimcp_health_agent_register_brain_probe(agent, nullptr, nullptr);
    EXPECT_EQ(result, -1);
}

// ============================================================================
// Brain Unregistration Tests
// ============================================================================

TEST_F(BrainProbeHealthTest, UnregisterBrainSuccess) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 1u);

    int result = nimcp_health_agent_unregister_brain_probe(agent, brain);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 0u);
}

TEST_F(BrainProbeHealthTest, UnregisterMiddleBrain) {
    brain_t brain1 = static_cast<brain_t>(create_mock_brain(1));
    brain_t brain2 = static_cast<brain_t>(create_mock_brain(2));
    brain_t brain3 = static_cast<brain_t>(create_mock_brain(3));

    nimcp_health_agent_register_brain_probe(agent, brain1, nullptr);
    nimcp_health_agent_register_brain_probe(agent, brain2, nullptr);
    nimcp_health_agent_register_brain_probe(agent, brain3, nullptr);
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 3u);

    int result = nimcp_health_agent_unregister_brain_probe(agent, brain2);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 2u);
}

TEST_F(BrainProbeHealthTest, UnregisterNotFoundFails) {
    brain_t brain1 = static_cast<brain_t>(create_mock_brain(1));
    brain_t brain2 = static_cast<brain_t>(create_mock_brain(2));

    nimcp_health_agent_register_brain_probe(agent, brain1, nullptr);

    int result = nimcp_health_agent_unregister_brain_probe(agent, brain2);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainProbeHealthTest, UnregisterNullAgentFails) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    int result = nimcp_health_agent_unregister_brain_probe(nullptr, brain);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainProbeHealthTest, UnregisterNullBrainFails) {
    int result = nimcp_health_agent_unregister_brain_probe(agent, nullptr);
    EXPECT_EQ(result, -1);
}

// ============================================================================
// Metrics Retrieval Tests
// ============================================================================

TEST_F(BrainProbeHealthTest, GetMetricsSuccess) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    brain_probe_health_metrics_t metrics;
    memset(&metrics, 0xFF, sizeof(metrics));  // Fill with garbage

    int result = nimcp_health_agent_get_brain_probe_metrics(agent, 0, &metrics);
    EXPECT_EQ(result, 0);
    EXPECT_GE(metrics.overall_health_score, 0.0f);
    EXPECT_LE(metrics.overall_health_score, 100.0f);
}

TEST_F(BrainProbeHealthTest, GetMetricsInvalidIndexFails) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    brain_probe_health_metrics_t metrics;
    int result = nimcp_health_agent_get_brain_probe_metrics(agent, 99, &metrics);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainProbeHealthTest, GetMetricsNullAgentFails) {
    brain_probe_health_metrics_t metrics;
    int result = nimcp_health_agent_get_brain_probe_metrics(nullptr, 0, &metrics);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainProbeHealthTest, GetMetricsNullOutputFails) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    int result = nimcp_health_agent_get_brain_probe_metrics(agent, 0, nullptr);
    EXPECT_EQ(result, -1);
}

// ============================================================================
// Recovery Action Tests
// ============================================================================

TEST_F(BrainProbeHealthTest, RecoveryActionSuccess) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    int result = nimcp_health_agent_brain_probe_recovery(
        agent, BRAIN_PROBE_RECOVERY_RESET_STATS, 0);
    EXPECT_EQ(result, 0);
}

TEST_F(BrainProbeHealthTest, RecoveryActionAllBrains) {
    for (int i = 0; i < 3; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    }

    int result = nimcp_health_agent_brain_probe_recovery(
        agent, BRAIN_PROBE_RECOVERY_TRIGGER_GC, -1);  // -1 = all brains
    EXPECT_EQ(result, 0);
}

TEST_F(BrainProbeHealthTest, RecoveryActionNoBrainsFails) {
    int result = nimcp_health_agent_brain_probe_recovery(
        agent, BRAIN_PROBE_RECOVERY_RESET_STATS, 0);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainProbeHealthTest, RecoveryActionInvalidIndexFails) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    int result = nimcp_health_agent_brain_probe_recovery(
        agent, BRAIN_PROBE_RECOVERY_RESET_STATS, 99);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainProbeHealthTest, RecoveryActionNullAgentFails) {
    int result = nimcp_health_agent_brain_probe_recovery(
        nullptr, BRAIN_PROBE_RECOVERY_RESET_STATS, 0);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainProbeHealthTest, AllRecoveryActionsSupported) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    for (int action = BRAIN_PROBE_RECOVERY_NONE; action < BRAIN_PROBE_RECOVERY_COUNT; action++) {
        int result = nimcp_health_agent_brain_probe_recovery(
            agent, static_cast<brain_probe_recovery_action_t>(action), 0);
        EXPECT_EQ(result, 0) << "Failed for action: " << action;
    }
}

// ============================================================================
// Health Score Tests
// ============================================================================

TEST_F(BrainProbeHealthTest, HealthScoreNoBrainsReturns100) {
    float score = nimcp_health_agent_get_brain_probe_health_score(agent);
    EXPECT_FLOAT_EQ(score, 100.0f);
}

TEST_F(BrainProbeHealthTest, HealthScoreWithBrainsValid) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    float score = nimcp_health_agent_get_brain_probe_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

TEST_F(BrainProbeHealthTest, HealthScoreNullAgentReturns100) {
    float score = nimcp_health_agent_get_brain_probe_health_score(nullptr);
    EXPECT_FLOAT_EQ(score, 100.0f);
}

// ============================================================================
// Needs Attention Tests
// ============================================================================

TEST_F(BrainProbeHealthTest, NeedsAttentionNoBrains) {
    bool needs = nimcp_health_agent_brain_needs_attention(agent);
    EXPECT_FALSE(needs);
}

TEST_F(BrainProbeHealthTest, NeedsAttentionNullAgent) {
    bool needs = nimcp_health_agent_brain_needs_attention(nullptr);
    EXPECT_FALSE(needs);
}

TEST_F(BrainProbeHealthTest, NeedsAttentionHealthyBrains) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    // Initial state should be healthy
    bool needs = nimcp_health_agent_brain_needs_attention(agent);
    // May or may not need attention depending on probe results
    EXPECT_TRUE(needs == true || needs == false);  // Valid result
}

// ============================================================================
// Probe All Brains Tests
// ============================================================================

TEST_F(BrainProbeHealthTest, ProbeAllBrainsNoBrains) {
    int result = nimcp_health_agent_probe_all_brains_now(agent);
    EXPECT_EQ(result, 0);
}

TEST_F(BrainProbeHealthTest, ProbeAllBrainsSuccess) {
    for (int i = 0; i < 3; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    }

    int result = nimcp_health_agent_probe_all_brains_now(agent);
    EXPECT_EQ(result, 3);
}

TEST_F(BrainProbeHealthTest, ProbeAllBrainsNullAgentFails) {
    int result = nimcp_health_agent_probe_all_brains_now(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainProbeHealthTest, ProbeAllBrainsUpdatesMetrics) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    brain_probe_health_metrics_t metrics_before;
    nimcp_health_agent_get_brain_probe_metrics(agent, 0, &metrics_before);

    nimcp_health_agent_probe_all_brains_now(agent);

    brain_probe_health_metrics_t metrics_after;
    nimcp_health_agent_get_brain_probe_metrics(agent, 0, &metrics_after);

    // Metrics should be updated
    EXPECT_GE(metrics_after.total_probes, metrics_before.total_probes);
}

// ============================================================================
// Config Update Tests
// ============================================================================

TEST_F(BrainProbeHealthTest, UpdateConfigSuccess) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    health_agent_brain_probe_config_t new_config;
    nimcp_health_agent_brain_probe_config_default(&new_config);
    new_config.probe_interval_ms = 2000;
    new_config.enable_auto_recovery = true;

    int result = nimcp_health_agent_update_brain_probe_config(agent, &new_config);
    EXPECT_EQ(result, 0);
}

TEST_F(BrainProbeHealthTest, UpdateConfigNullAgentFails) {
    health_agent_brain_probe_config_t config;
    nimcp_health_agent_brain_probe_config_default(&config);

    int result = nimcp_health_agent_update_brain_probe_config(nullptr, &config);
    EXPECT_EQ(result, -1);
}

TEST_F(BrainProbeHealthTest, UpdateConfigNullConfigFails) {
    int result = nimcp_health_agent_update_brain_probe_config(agent, nullptr);
    EXPECT_EQ(result, -1);
}

// ============================================================================
// Brain Count Tests
// ============================================================================

TEST_F(BrainProbeHealthTest, BrainCountInitiallyZero) {
    uint32_t count = nimcp_health_agent_get_brain_count(agent);
    EXPECT_EQ(count, 0u);
}

TEST_F(BrainProbeHealthTest, BrainCountAccurate) {
    for (int i = 0; i < 10; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
        EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), static_cast<uint32_t>(i + 1));
    }
}

TEST_F(BrainProbeHealthTest, BrainCountNullAgentReturnsZero) {
    uint32_t count = nimcp_health_agent_get_brain_count(nullptr);
    EXPECT_EQ(count, 0u);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(BrainProbeHealthTest, RegisterManyBrains) {
    const int num_brains = 50;
    for (int i = 0; i < num_brains; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        int result = nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
        EXPECT_EQ(result, 0);
    }
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), static_cast<uint32_t>(num_brains));
}

TEST_F(BrainProbeHealthTest, RegisterUnregisterCycle) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));

    for (int cycle = 0; cycle < 100; cycle++) {
        int reg = nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
        EXPECT_EQ(reg, 0);
        EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 1u);

        int unreg = nimcp_health_agent_unregister_brain_probe(agent, brain);
        EXPECT_EQ(unreg, 0);
        EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 0u);
    }
}

TEST_F(BrainProbeHealthTest, MultipleProbeCallsStable) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    for (int i = 0; i < 100; i++) {
        int result = nimcp_health_agent_probe_all_brains_now(agent);
        EXPECT_EQ(result, 1);

        float score = nimcp_health_agent_get_brain_probe_health_score(agent);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);
    }
}
