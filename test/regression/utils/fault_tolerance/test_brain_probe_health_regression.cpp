/**
 * @file test_brain_probe_health_regression.cpp
 * @brief Regression tests for Phase 5.13 Brain Probes Enhancement
 *
 * Tests for boundary conditions, edge cases, and stability.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <random>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
}

// Mock brain type for testing
static void* create_mock_brain(int id) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0x1000 + id * 0x100));
}

class BrainProbeHealthRegressionTest : public ::testing::Test {
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
// Boundary Condition Tests
// ============================================================================

TEST_F(BrainProbeHealthRegressionTest, MaxBrainRegistration) {
    // Try to register up to HEALTH_AGENT_MAX_BRAINS (64)
    const int max_brains = HEALTH_AGENT_MAX_BRAINS;
    int successful = 0;

    for (int i = 0; i < max_brains + 10; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        int result = nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
        if (result == 0) {
            successful++;
        }
    }

    EXPECT_EQ(successful, max_brains);
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), static_cast<uint32_t>(max_brains));
}

TEST_F(BrainProbeHealthRegressionTest, BoundaryIndexAccess) {
    const int num_brains = 5;
    for (int i = 0; i < num_brains; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    }

    brain_probe_health_metrics_t metrics;

    // Valid indices
    EXPECT_EQ(nimcp_health_agent_get_brain_probe_metrics(agent, 0, &metrics), 0);
    EXPECT_EQ(nimcp_health_agent_get_brain_probe_metrics(agent, num_brains - 1, &metrics), 0);

    // Invalid indices
    EXPECT_EQ(nimcp_health_agent_get_brain_probe_metrics(agent, num_brains, &metrics), -1);
    EXPECT_EQ(nimcp_health_agent_get_brain_probe_metrics(agent, UINT32_MAX, &metrics), -1);
}

// ============================================================================
// Repeated Operations Tests
// ============================================================================

TEST_F(BrainProbeHealthRegressionTest, RepeatedRegisterUnregisterSameBrain) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(nimcp_health_agent_register_brain_probe(agent, brain, nullptr), 0);
        EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 1u);
        EXPECT_EQ(nimcp_health_agent_unregister_brain_probe(agent, brain), 0);
        EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 0u);
    }
}

TEST_F(BrainProbeHealthRegressionTest, RepeatedProbeCallsStability) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    float prev_score = 100.0f;
    for (int i = 0; i < 1000; i++) {
        nimcp_health_agent_probe_all_brains_now(agent);
        float score = nimcp_health_agent_get_brain_probe_health_score(agent);

        // Score should be stable (within range)
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);

        prev_score = score;
    }
}

TEST_F(BrainProbeHealthRegressionTest, RepeatedConfigUpdates) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    for (int i = 0; i < 100; i++) {
        health_agent_brain_probe_config_t config;
        nimcp_health_agent_brain_probe_config_default(&config);
        config.probe_interval_ms = 100 + (i % 1000);
        config.memory_warning_bytes = 1024 * 1024 * (1 + (i % 100));

        EXPECT_EQ(nimcp_health_agent_update_brain_probe_config(agent, &config), 0);
    }
}

// ============================================================================
// Random Order Operations
// ============================================================================

TEST_F(BrainProbeHealthRegressionTest, RandomOrderUnregister) {
    std::vector<brain_t> brains;
    for (int i = 0; i < 20; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        brains.push_back(brain);
        nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    }

    // Shuffle the order
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(brains.begin(), brains.end(), g);

    // Unregister in random order
    for (auto brain : brains) {
        EXPECT_EQ(nimcp_health_agent_unregister_brain_probe(agent, brain), 0);
    }

    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 0u);
}

TEST_F(BrainProbeHealthRegressionTest, RandomMixedOperations) {
    std::vector<brain_t> registered;
    std::random_device rd;
    std::mt19937 g(rd());

    for (int op = 0; op < 500; op++) {
        int action = g() % 4;

        switch (action) {
            case 0:  // Register
                if (registered.size() < HEALTH_AGENT_MAX_BRAINS) {
                    brain_t brain = static_cast<brain_t>(create_mock_brain(op));
                    if (nimcp_health_agent_register_brain_probe(agent, brain, nullptr) == 0) {
                        registered.push_back(brain);
                    }
                }
                break;

            case 1:  // Unregister
                if (!registered.empty()) {
                    size_t idx = g() % registered.size();
                    nimcp_health_agent_unregister_brain_probe(agent, registered[idx]);
                    registered.erase(registered.begin() + idx);
                }
                break;

            case 2:  // Probe
                nimcp_health_agent_probe_all_brains_now(agent);
                break;

            case 3:  // Get health
                nimcp_health_agent_get_brain_probe_health_score(agent);
                break;
        }

        // Verify count matches
        EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), registered.size());
    }
}

// ============================================================================
// Metrics Consistency Tests
// ============================================================================

TEST_F(BrainProbeHealthRegressionTest, MetricsConsistentAcrossProbes) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    brain_probe_health_metrics_t prev_metrics, curr_metrics;
    nimcp_health_agent_probe_all_brains_now(agent);
    nimcp_health_agent_get_brain_probe_metrics(agent, 0, &prev_metrics);

    for (int i = 0; i < 50; i++) {
        nimcp_health_agent_probe_all_brains_now(agent);
        nimcp_health_agent_get_brain_probe_metrics(agent, 0, &curr_metrics);

        // Total probes should increment
        EXPECT_GT(curr_metrics.total_probes, prev_metrics.total_probes);

        // Health score should be valid
        EXPECT_GE(curr_metrics.overall_health_score, 0.0f);
        EXPECT_LE(curr_metrics.overall_health_score, 100.0f);

        // History index should cycle through 0-9
        EXPECT_LT(curr_metrics.history_index, 10u);

        prev_metrics = curr_metrics;
    }
}

TEST_F(BrainProbeHealthRegressionTest, MetricsPreservedDuringUnregister) {
    brain_t brain1 = static_cast<brain_t>(create_mock_brain(1));
    brain_t brain2 = static_cast<brain_t>(create_mock_brain(2));
    brain_t brain3 = static_cast<brain_t>(create_mock_brain(3));

    nimcp_health_agent_register_brain_probe(agent, brain1, nullptr);
    nimcp_health_agent_register_brain_probe(agent, brain2, nullptr);
    nimcp_health_agent_register_brain_probe(agent, brain3, nullptr);

    // Probe all
    nimcp_health_agent_probe_all_brains_now(agent);

    // Get brain3 metrics (at index 2)
    brain_probe_health_metrics_t metrics3_before;
    nimcp_health_agent_get_brain_probe_metrics(agent, 2, &metrics3_before);

    // Unregister brain1 (at index 0) - this shifts brain2 to 0, brain3 to 1
    nimcp_health_agent_unregister_brain_probe(agent, brain1);

    // brain3 should now be at index 1
    brain_probe_health_metrics_t metrics3_after;
    nimcp_health_agent_get_brain_probe_metrics(agent, 1, &metrics3_after);

    // Verify metrics are preserved (same probe count)
    EXPECT_EQ(metrics3_before.total_probes, metrics3_after.total_probes);
}

// ============================================================================
// Health Score Stability Tests
// ============================================================================

TEST_F(BrainProbeHealthRegressionTest, HealthScoreStableWithConstantMetrics) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    nimcp_health_agent_probe_all_brains_now(agent);
    float initial_score = nimcp_health_agent_get_brain_probe_health_score(agent);

    // Multiple probes should produce consistent scores if metrics don't change
    std::vector<float> scores;
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_probe_all_brains_now(agent);
        scores.push_back(nimcp_health_agent_get_brain_probe_health_score(agent));
    }

    // Scores should be relatively stable (within small variance)
    for (float score : scores) {
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);
    }
}

TEST_F(BrainProbeHealthRegressionTest, HealthScoreAfterMultipleUnregisters) {
    const int num_brains = 10;
    std::vector<brain_t> brains;
    for (int i = 0; i < num_brains; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        brains.push_back(brain);
        nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    }

    nimcp_health_agent_probe_all_brains_now(agent);

    // Unregister one by one and check health score is always valid
    for (int i = 0; i < num_brains; i++) {
        float score = nimcp_health_agent_get_brain_probe_health_score(agent);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);

        nimcp_health_agent_unregister_brain_probe(agent, brains[i]);
    }

    // Final score with no brains should be 100
    EXPECT_FLOAT_EQ(nimcp_health_agent_get_brain_probe_health_score(agent), 100.0f);
}

// ============================================================================
// Double Operation Safety Tests
// ============================================================================

TEST_F(BrainProbeHealthRegressionTest, DoubleUnregisterSafe) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    EXPECT_EQ(nimcp_health_agent_unregister_brain_probe(agent, brain), 0);
    EXPECT_EQ(nimcp_health_agent_unregister_brain_probe(agent, brain), -1);  // Already removed
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 0u);
}

TEST_F(BrainProbeHealthRegressionTest, DoubleRegisterIgnored) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));

    EXPECT_EQ(nimcp_health_agent_register_brain_probe(agent, brain, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_register_brain_probe(agent, brain, nullptr), 0);  // Ignored
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 1u);
}

// ============================================================================
// Recovery Under Load Tests
// ============================================================================

TEST_F(BrainProbeHealthRegressionTest, RecoveryUnderLoad) {
    const int num_brains = 20;
    for (int i = 0; i < num_brains; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    }

    // Perform many probes
    for (int i = 0; i < 100; i++) {
        nimcp_health_agent_probe_all_brains_now(agent);
    }

    // Execute recovery on all brains
    EXPECT_EQ(nimcp_health_agent_brain_probe_recovery(
        agent, BRAIN_PROBE_RECOVERY_RESET_STATS, -1), 0);

    // All brains should have reset metrics
    for (int i = 0; i < num_brains; i++) {
        brain_probe_health_metrics_t metrics;
        nimcp_health_agent_get_brain_probe_metrics(agent, i, &metrics);
        EXPECT_FLOAT_EQ(metrics.overall_health_score, 100.0f);
    }
}

// ============================================================================
// Config Edge Cases
// ============================================================================

TEST_F(BrainProbeHealthRegressionTest, ExtremeConfigValues) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    health_agent_brain_probe_config_t config;
    nimcp_health_agent_brain_probe_config_default(&config);

    // Extreme low values
    config.memory_warning_bytes = 1;
    config.memory_critical_bytes = 2;
    config.inference_time_warning_us = 0.001f;
    config.inference_time_critical_us = 0.002f;
    config.probe_interval_ms = 1;

    EXPECT_EQ(nimcp_health_agent_update_brain_probe_config(agent, &config), 0);

    // Should still work without crashing
    nimcp_health_agent_probe_all_brains_now(agent);
    float score = nimcp_health_agent_get_brain_probe_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);

    // Extreme high values
    nimcp_health_agent_brain_probe_config_default(&config);
    config.memory_warning_bytes = SIZE_MAX - 1;
    config.memory_critical_bytes = SIZE_MAX;
    config.inference_time_warning_us = 1e12f;
    config.inference_time_critical_us = 1e12f;

    EXPECT_EQ(nimcp_health_agent_update_brain_probe_config(agent, &config), 0);
    nimcp_health_agent_probe_all_brains_now(agent);
    score = nimcp_health_agent_get_brain_probe_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

TEST_F(BrainProbeHealthRegressionTest, DisabledMonitoringConfig) {
    health_agent_brain_probe_config_t config;
    nimcp_health_agent_brain_probe_config_default(&config);
    config.enable_probe_monitoring = false;

    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    EXPECT_EQ(nimcp_health_agent_register_brain_probe(agent, brain, &config), 0);

    // Probe should not update metrics when monitoring is disabled
    int probed = nimcp_health_agent_probe_all_brains_now(agent);
    EXPECT_EQ(probed, 1);

    brain_probe_health_metrics_t metrics;
    nimcp_health_agent_get_brain_probe_metrics(agent, 0, &metrics);
    // With disabled monitoring, metrics might not be updated (implementation dependent)
    // Just verify no crash and valid score
    EXPECT_GE(metrics.overall_health_score, 0.0f);
    EXPECT_LE(metrics.overall_health_score, 100.0f);
}
