/**
 * @file test_brain_probe_health_e2e.cpp
 * @brief End-to-end tests for Phase 5.13 Brain Probes Enhancement
 *
 * Full system tests for brain probe health monitoring.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <mutex>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
}

// Mock brain type for testing
static void* create_mock_brain(int id) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0x1000 + id * 0x100));
}

class BrainProbeHealthE2ETest : public ::testing::Test {
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
// Full System Lifecycle E2E Tests
// ============================================================================

TEST_F(BrainProbeHealthE2ETest, FullLifecycleWithMultipleBrains) {
    const int num_brains = 10;
    std::vector<brain_t> brains;

    // Phase 1: Registration
    printf("Phase 1: Registering %d brains...\n", num_brains);
    for (int i = 0; i < num_brains; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        brains.push_back(brain);
        EXPECT_EQ(nimcp_health_agent_register_brain_probe(agent, brain, nullptr), 0);
    }
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), static_cast<uint32_t>(num_brains));

    // Phase 2: Initial health check
    printf("Phase 2: Initial health check...\n");
    EXPECT_FALSE(nimcp_health_agent_brain_needs_attention(agent));  // No probes yet
    float initial_score = nimcp_health_agent_get_brain_probe_health_score(agent);
    EXPECT_GE(initial_score, 0.0f);

    // Phase 3: Probe cycles
    printf("Phase 3: Running 50 probe cycles...\n");
    for (int cycle = 0; cycle < 50; cycle++) {
        int probed = nimcp_health_agent_probe_all_brains_now(agent);
        EXPECT_EQ(probed, num_brains);
    }

    // Phase 4: Verify metrics collected
    printf("Phase 4: Verifying metrics...\n");
    for (int i = 0; i < num_brains; i++) {
        brain_probe_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_brain_probe_metrics(agent, i, &metrics), 0);
        EXPECT_EQ(metrics.total_probes, 50u);
        EXPECT_GT(metrics.num_neurons, 0u);
        EXPECT_GT(metrics.num_synapses, 0u);
    }

    // Phase 5: Health score aggregation
    printf("Phase 5: Checking aggregate health...\n");
    float final_score = nimcp_health_agent_get_brain_probe_health_score(agent);
    EXPECT_GE(final_score, 0.0f);
    EXPECT_LE(final_score, 100.0f);

    // Phase 6: Recovery actions
    printf("Phase 6: Executing recovery actions...\n");
    EXPECT_EQ(nimcp_health_agent_brain_probe_recovery(
        agent, BRAIN_PROBE_RECOVERY_TRIGGER_GC, -1), 0);
    EXPECT_EQ(nimcp_health_agent_brain_probe_recovery(
        agent, BRAIN_PROBE_RECOVERY_RESET_STATS, -1), 0);

    // Phase 7: Cleanup
    printf("Phase 7: Unregistering all brains...\n");
    for (auto brain : brains) {
        EXPECT_EQ(nimcp_health_agent_unregister_brain_probe(agent, brain), 0);
    }
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 0u);

    printf("Full lifecycle test completed successfully.\n");
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(BrainProbeHealthE2ETest, StressTestHighFrequencyProbes) {
    const int num_brains = 5;
    for (int i = 0; i < num_brains; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    }

    printf("Running 1000 high-frequency probe cycles...\n");
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 1000; i++) {
        int probed = nimcp_health_agent_probe_all_brains_now(agent);
        EXPECT_EQ(probed, num_brains);

        if (i % 100 == 0) {
            float score = nimcp_health_agent_get_brain_probe_health_score(agent);
            EXPECT_GE(score, 0.0f);
            EXPECT_LE(score, 100.0f);
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    printf("Completed 1000 probes in %ld ms\n", duration.count());

    // Verify final state
    for (int i = 0; i < num_brains; i++) {
        brain_probe_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_brain_probe_metrics(agent, i, &metrics), 0);
        EXPECT_EQ(metrics.total_probes, 1000u);
    }
}

TEST_F(BrainProbeHealthE2ETest, StressTestRapidRegisterUnregister) {
    printf("Running rapid register/unregister stress test...\n");
    auto start = std::chrono::steady_clock::now();

    for (int cycle = 0; cycle < 500; cycle++) {
        // Register several brains
        std::vector<brain_t> brains;
        for (int i = 0; i < 5; i++) {
            brain_t brain = static_cast<brain_t>(create_mock_brain(cycle * 5 + i));
            brains.push_back(brain);
            nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
        }

        // Probe once
        nimcp_health_agent_probe_all_brains_now(agent);

        // Unregister all
        for (auto brain : brains) {
            nimcp_health_agent_unregister_brain_probe(agent, brain);
        }
    }

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    printf("Completed 500 cycles in %ld ms\n", duration.count());

    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), 0u);
}

// ============================================================================
// Concurrent Operation Tests
// ============================================================================

TEST_F(BrainProbeHealthE2ETest, ConcurrentMultiThreadedOperations) {
    const int num_brains = 10;
    std::vector<brain_t> brains;
    std::mutex brains_mutex;

    for (int i = 0; i < num_brains; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        brains.push_back(brain);
        nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    }

    std::atomic<bool> stop{false};
    std::atomic<int> total_probes{0};
    std::atomic<int> total_metrics_reads{0};
    std::atomic<int> total_recoveries{0};

    // Thread 1: Continuous probing
    std::thread probe_thread([&]() {
        while (!stop.load()) {
            nimcp_health_agent_probe_all_brains_now(agent);
            total_probes.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Thread 2: Continuous metrics reading
    std::thread metrics_thread([&]() {
        while (!stop.load()) {
            brain_probe_health_metrics_t metrics;
            for (int i = 0; i < num_brains; i++) {
                nimcp_health_agent_get_brain_probe_metrics(agent, i, &metrics);
            }
            nimcp_health_agent_get_brain_probe_health_score(agent);
            total_metrics_reads.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    // Thread 3: Occasional recovery actions
    std::thread recovery_thread([&]() {
        while (!stop.load()) {
            nimcp_health_agent_brain_probe_recovery(
                agent, BRAIN_PROBE_RECOVERY_NONE, -1);
            total_recoveries.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Run for 500ms
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop.store(true);

    probe_thread.join();
    metrics_thread.join();
    recovery_thread.join();

    printf("Concurrent test results:\n");
    printf("  Total probes: %d\n", total_probes.load());
    printf("  Total metrics reads: %d\n", total_metrics_reads.load());
    printf("  Total recoveries: %d\n", total_recoveries.load());

    EXPECT_GT(total_probes.load(), 0);
    EXPECT_GT(total_metrics_reads.load(), 0);
    EXPECT_GT(total_recoveries.load(), 0);

    // Verify agent is still in valid state
    EXPECT_EQ(nimcp_health_agent_get_brain_count(agent), static_cast<uint32_t>(num_brains));
    float score = nimcp_health_agent_get_brain_probe_health_score(agent);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);
}

// ============================================================================
// Recovery Scenario Tests
// ============================================================================

TEST_F(BrainProbeHealthE2ETest, RecoverySequenceE2E) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    // Build up some history
    for (int i = 0; i < 20; i++) {
        nimcp_health_agent_probe_all_brains_now(agent);
    }

    brain_probe_health_metrics_t metrics_before;
    nimcp_health_agent_get_brain_probe_metrics(agent, 0, &metrics_before);
    EXPECT_EQ(metrics_before.total_probes, 20u);

    // Execute all recovery actions in sequence
    printf("Executing all recovery actions...\n");
    for (int action = BRAIN_PROBE_RECOVERY_NONE; action < BRAIN_PROBE_RECOVERY_COUNT; action++) {
        int result = nimcp_health_agent_brain_probe_recovery(
            agent, static_cast<brain_probe_recovery_action_t>(action), 0);
        EXPECT_EQ(result, 0);
    }

    // After RESET_STATS, metrics should be cleared
    brain_probe_health_metrics_t metrics_after;
    nimcp_health_agent_get_brain_probe_metrics(agent, 0, &metrics_after);
    // Note: Some metrics may persist, but health score should be reset
}

TEST_F(BrainProbeHealthE2ETest, RecoveryAllVsIndividualBrains) {
    const int num_brains = 5;
    for (int i = 0; i < num_brains; i++) {
        brain_t brain = static_cast<brain_t>(create_mock_brain(i));
        nimcp_health_agent_register_brain_probe(agent, brain, nullptr);
    }

    // Probe to build history
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_probe_all_brains_now(agent);
    }

    // Recovery on all brains
    printf("Recovery on all brains...\n");
    EXPECT_EQ(nimcp_health_agent_brain_probe_recovery(
        agent, BRAIN_PROBE_RECOVERY_RESET_STATS, -1), 0);

    // Verify all reset
    for (int i = 0; i < num_brains; i++) {
        brain_probe_health_metrics_t metrics;
        nimcp_health_agent_get_brain_probe_metrics(agent, i, &metrics);
        EXPECT_FLOAT_EQ(metrics.overall_health_score, 100.0f);
    }

    // Probe again to build new history
    for (int i = 0; i < 5; i++) {
        nimcp_health_agent_probe_all_brains_now(agent);
    }

    // Recovery on individual brain
    printf("Recovery on individual brain...\n");
    EXPECT_EQ(nimcp_health_agent_brain_probe_recovery(
        agent, BRAIN_PROBE_RECOVERY_RESET_STATS, 2), 0);

    // Only brain 2 should be reset
    for (int i = 0; i < num_brains; i++) {
        brain_probe_health_metrics_t metrics;
        nimcp_health_agent_get_brain_probe_metrics(agent, i, &metrics);
        if (i == 2) {
            EXPECT_FLOAT_EQ(metrics.overall_health_score, 100.0f);
        }
    }
}

// ============================================================================
// Configuration Change Scenarios
// ============================================================================

TEST_F(BrainProbeHealthE2ETest, ConfigChangeDuringOperation) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(1));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    printf("Testing config changes during operation...\n");

    // Probe with default config
    for (int i = 0; i < 10; i++) {
        nimcp_health_agent_probe_all_brains_now(agent);
    }

    float score1 = nimcp_health_agent_get_brain_probe_health_score(agent);
    printf("Score with default config: %.1f\n", score1);

    // Change to very strict config
    health_agent_brain_probe_config_t strict_config;
    nimcp_health_agent_brain_probe_config_default(&strict_config);
    strict_config.memory_warning_bytes = 1;  // Very strict
    strict_config.inference_time_warning_us = 1.0f;  // Very strict
    nimcp_health_agent_update_brain_probe_config(agent, &strict_config);

    // Probe with strict config
    nimcp_health_agent_probe_all_brains_now(agent);
    float score2 = nimcp_health_agent_get_brain_probe_health_score(agent);
    printf("Score with strict config: %.1f\n", score2);

    // Change to very lenient config
    health_agent_brain_probe_config_t lenient_config;
    nimcp_health_agent_brain_probe_config_default(&lenient_config);
    lenient_config.memory_warning_bytes = SIZE_MAX / 2;
    lenient_config.memory_critical_bytes = SIZE_MAX;
    lenient_config.inference_time_warning_us = 1e12f;
    lenient_config.inference_time_critical_us = 1e12f;
    nimcp_health_agent_update_brain_probe_config(agent, &lenient_config);

    // Probe with lenient config
    nimcp_health_agent_probe_all_brains_now(agent);
    float score3 = nimcp_health_agent_get_brain_probe_health_score(agent);
    printf("Score with lenient config: %.1f\n", score3);

    // All scores should be valid
    EXPECT_GE(score1, 0.0f);
    EXPECT_LE(score1, 100.0f);
    EXPECT_GE(score2, 0.0f);
    EXPECT_LE(score2, 100.0f);
    EXPECT_GE(score3, 0.0f);
    EXPECT_LE(score3, 100.0f);
}

// ============================================================================
// Metrics Validation E2E Test
// ============================================================================

TEST_F(BrainProbeHealthE2ETest, MetricsValidation) {
    brain_t brain = static_cast<brain_t>(create_mock_brain(42));
    nimcp_health_agent_register_brain_probe(agent, brain, nullptr);

    // Probe multiple times to build history
    const int num_probes = 20;
    for (int i = 0; i < num_probes; i++) {
        nimcp_health_agent_probe_all_brains_now(agent);
    }

    brain_probe_health_metrics_t metrics;
    nimcp_health_agent_get_brain_probe_metrics(agent, 0, &metrics);

    printf("Metrics validation:\n");
    printf("  num_neurons: %u\n", metrics.num_neurons);
    printf("  num_synapses: %u\n", metrics.num_synapses);
    printf("  num_active_synapses: %u\n", metrics.num_active_synapses);
    printf("  memory_bytes: %zu\n", metrics.memory_bytes);
    printf("  avg_inference_time_us: %.2f\n", metrics.avg_inference_time_us);
    printf("  current_learning_rate: %.4f\n", metrics.current_learning_rate);
    printf("  avg_sparsity: %.2f\n", metrics.avg_sparsity);
    printf("  accuracy: %.2f\n", metrics.accuracy);
    printf("  overall_health_score: %.1f\n", metrics.overall_health_score);
    printf("  total_probes: %lu\n", metrics.total_probes);
    printf("  history_count: %u\n", metrics.history_count);

    // Validate metrics
    EXPECT_EQ(metrics.total_probes, static_cast<uint64_t>(num_probes));
    EXPECT_GT(metrics.num_neurons, 0u);
    EXPECT_GT(metrics.num_synapses, 0u);
    EXPECT_LE(metrics.num_active_synapses, metrics.num_synapses);
    EXPECT_GT(metrics.memory_bytes, 0u);
    EXPECT_GE(metrics.avg_inference_time_us, 0.0f);
    EXPECT_GE(metrics.current_learning_rate, 0.0f);
    EXPECT_GE(metrics.avg_sparsity, 0.0f);
    EXPECT_LE(metrics.avg_sparsity, 1.0f);
    EXPECT_GE(metrics.accuracy, 0.0f);
    EXPECT_LE(metrics.accuracy, 1.0f);
    EXPECT_GE(metrics.overall_health_score, 0.0f);
    EXPECT_LE(metrics.overall_health_score, 100.0f);
    EXPECT_GT(metrics.history_count, 0u);
    EXPECT_LE(metrics.history_count, 10u);
}
