/**
 * @file test_brain_probe_health_integration.cpp
 * @brief Integration tests for Phase 5.13 Brain Probes Enhancement
 *
 * Tests brain probe health monitoring integration with health agent lifecycle.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
}

// Mock brain type for testing
static void* create_mock_brain(int id) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0x1000 + id * 0x100));
}

class BrainProbeHealthIntegrationTest : public ::testing::Test {
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
// Multi-Brain Registration Integration
// ============================================================================

TEST_F(BrainProbeHealthIntegrationTest, RegisterMultipleBrainsWithDifferentConfigs) {
    health_agent_brain_probe_config_t fast_config, slow_config;
    nimcp_health_agent_brain_probe_config_default(&fast_config);
    nimcp_health_agent_brain_probe_config_default(&slow_config);

    fast_config.probe_interval_ms = 100;
    slow_config.probe_interval_ms = 5000;

    brain_t brain1 = static_cast<brain_t>(create_mock_brain(1));
    brain_t brain2 = static_cast<brain_t>(create_mock_brain(2));

    EXPECT_EQ(nimcp_health_agent_register_brain_probe(agent, brain1, &fast_config), 0);
    EXPECT_EQ(nimcp_health_agent_register_brain_probe(agent, brain2, &slow_config), 0);
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 2u);
}

TEST_F(BrainProbeHealthIntegrationTest, RegisterUnregisterMixedOperations) {
    std::vector<brain_t> brains;
    for (int i = 0; i < 10; i++) {
        brains.push_back(static_cast<brain_t>(create_mock_brain(i)));
    }

    // Register all
    for (auto brain : brains) {
        EXPECT_EQ(nimcp_health_agent_register_brain_probe(agent, brain, nullptr), 0);
    }
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 10u);

    // Unregister odd indices
    for (size_t i = 1; i < brains.size(); i += 2) {
        EXPECT_EQ(nimcp_health_agent_unregister_brain_probe(agent, brains[i]), 0);
    }
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 5u);

    // Re-register some
    EXPECT_EQ(nimcp_health_agent_register_brain_probe(agent, brains[1], nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_register_brain_probe(agent, brains[3], nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 7u);
}

// ============================================================================
// Probe Cycle Integration
// ============================================================================

TEST_F(BrainProbeHealthIntegrationTest, ProbeCycleWithMultipleBrains) {
    const int num_brains = 5;
    for (int i = 0; i < num_brains; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    }

    // Perform multiple probe cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        int probed = nimcp_health_agent_probe_all_brains_now(agent);
        EXPECT_EQ(probed, num_brains);

        // Check that metrics are updated for each brain
        for (int i = 0; i < num_brains; i++) {
            brain_probe_health_metrics_t metrics;
            EXPECT_EQ(nimcp_health_agent_get_brain_probe_metrics(agent, i, &metrics), 0);
            EXPECT_EQ(metrics.total_probes, static_cast<uint64_t>(cycle + 1));
        }
    }
}

TEST_F(BrainProbeHealthIntegrationTest, ProbeWithConfigUpdates) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    // Probe with default config
    nimcp_health_agent_probe_all_brains_now(agent);

    // Update config
    health_agent_brain_probe_config_t new_config;
    nimcp_health_agent_brain_probe_config_default(&new_config);
    new_config.memory_warning_bytes = 1024;  // Very low threshold
    new_config.inference_time_warning_us = 1.0f;  // Very low threshold
    EXPECT_EQ(nimcp_health_agent_update_brain_probe_config(agent, &new_config), 0);

    // Probe with new config - may trigger warnings
    nimcp_health_agent_probe_all_brains_now(agent);

    brain_probe_health_metrics_t metrics;
    nimcp_health_agent_get_brain_probe_metrics(agent, 0, &metrics);
    EXPECT_EQ(metrics.total_probes, 2u);
}

// ============================================================================
// Health Score Aggregation
// ============================================================================

TEST_F(BrainProbeHealthIntegrationTest, HealthScoreAggregation) {
    const int num_brains = 5;
    for (int i = 0; i < num_brains; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    }

    nimcp_health_agent_probe_all_brains_now(agent);

    float aggregate_score = nimcp_health_agent_get_brain_probe_health_score(agent);

    // Manually compute expected average
    float sum = 0.0f;
    for (int i = 0; i < num_brains; i++) {
        brain_probe_health_metrics_t metrics;
        nimcp_health_agent_get_brain_probe_metrics(agent, i, &metrics);
        sum += metrics.overall_health_score;
    }
    float expected_avg = sum / num_brains;

    EXPECT_FLOAT_EQ(aggregate_score, expected_avg);
}

TEST_F(BrainProbeHealthIntegrationTest, HealthScoreAfterUnregister) {
    brain_t brain1 = static_cast<brain_t>(create_mock_brain(1));
    brain_t brain2 = static_cast<brain_t>(create_mock_brain(2));

    nimcp_health_agent_register_brain_probe(agent, brain1, nullptr);
    nimcp_health_agent_register_brain_probe(agent, brain2, nullptr);
    nimcp_health_agent_probe_all_brains_now(agent);

    float score_with_two = nimcp_health_agent_get_brain_probe_health_score(agent);

    nimcp_health_agent_unregister_brain_probe(agent, brain2);
    nimcp_health_agent_probe_all_brains_now(agent);

    float score_with_one = nimcp_health_agent_get_brain_probe_health_score(agent);

    // Both should be valid scores
    EXPECT_GE(score_with_two, 0.0f);
    EXPECT_LE(score_with_two, 100.0f);
    EXPECT_GE(score_with_one, 0.0f);
    EXPECT_LE(score_with_one, 100.0f);
}

// ============================================================================
// Recovery Sequence Integration
// ============================================================================

TEST_F(BrainProbeHealthIntegrationTest, RecoverySequenceMultipleActions) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    // Execute a sequence of recovery actions
    std::vector<brain_probe_recovery_action_t> actions = {
        BRAIN_PROBE_RECOVERY_TRIGGER_GC,
        BRAIN_PROBE_RECOVERY_REDUCE_LR,
        BRAIN_PROBE_RECOVERY_RESET_STATS,
        BRAIN_PROBE_RECOVERY_CHECKPOINT
    };

    for (auto action : actions) {
        EXPECT_EQ(nimcp_health_agent_brain_probe_recovery(agent, action, 0), 0);
    }
}

TEST_F(BrainProbeHealthIntegrationTest, RecoveryAllBrainsThenIndividual) {
    const int num_brains = 3;
    for (int i = 0; i < num_brains; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    }

    // Recovery on all brains
    EXPECT_EQ(nimcp_health_agent_brain_probe_recovery(
        agent, BRAIN_PROBE_RECOVERY_TRIGGER_GC, -1), 0);

    // Recovery on individual brains
    for (int i = 0; i < num_brains; i++) {
        EXPECT_EQ(nimcp_health_agent_brain_probe_recovery(
            agent, BRAIN_PROBE_RECOVERY_RESET_STATS, i), 0);
    }
}

// ============================================================================
// Concurrent Access Simulation
// ============================================================================

TEST_F(BrainProbeHealthIntegrationTest, ConcurrentProbeAndMetricsAccess) {
    const int num_brains = 5;
    for (int i = 0; i < num_brains; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    }

    std::atomic<bool> stop{false};
    std::atomic<int> probes_done{0};
    std::atomic<int> metrics_read{0};

    // Thread doing probes
    std::thread probe_thread([&]() {
        while (!stop.load()) {
            nimcp_health_agent_probe_all_brains_now(agent);
            probes_done.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Thread reading metrics
    std::thread metrics_thread([&]() {
        while (!stop.load()) {
            brain_probe_health_metrics_t metrics;
            for (int i = 0; i < num_brains; i++) {
                nimcp_health_agent_get_brain_probe_metrics(agent, i, &metrics);
            }
            metrics_read.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Run for a short duration
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop.store(true);

    probe_thread.join();
    metrics_thread.join();

    EXPECT_GT(probes_done.load(), 0);
    EXPECT_GT(metrics_read.load(), 0);
}

// ============================================================================
// Lifecycle Integration
// ============================================================================

TEST_F(BrainProbeHealthIntegrationTest, FullLifecycleTest) {
    // Phase 1: Register brains
    std::vector<brain_t> brains;
    for (int i = 0; i < 5; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        brains.push_back(brain);
        EXPECT_EQ(nimcp_health_agent_register_brain_probe(agent, brain, nullptr), 0);
    }
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 5u);

    // Phase 2: Initial probe
    EXPECT_EQ(nimcp_health_agent_probe_all_brains_now(agent), 5);

    // Phase 3: Check health
    float initial_health = nimcp_health_agent_get_brain_probe_health_score(agent);
    EXPECT_GE(initial_health, 0.0f);
    EXPECT_LE(initial_health, 100.0f);

    // Phase 4: Multiple probe cycles
    for (int i = 0; i < 5; i++) {
        nimcp_health_agent_probe_all_brains_now(agent);
    }

    // Phase 5: Recovery actions
    nimcp_health_agent_brain_probe_recovery(agent, BRAIN_PROBE_RECOVERY_RESET_STATS, -1);

    // Phase 6: Verify metrics reset
    for (size_t i = 0; i < brains.size(); i++) {
        brain_probe_health_metrics_t metrics;
        nimcp_health_agent_get_brain_probe_metrics(agent, i, &metrics);
        EXPECT_FLOAT_EQ(metrics.overall_health_score, 100.0f);
    }

    // Phase 7: Unregister all
    for (auto brain : brains) {
        EXPECT_EQ(nimcp_health_agent_unregister_brain_probe(agent, brain), 0);
    }
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 0u);

    // Phase 8: Health with no brains
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_brain_probe_health_score(agent), 100.0f);
}
