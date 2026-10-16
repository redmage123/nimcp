//=============================================================================
// test_training_logic_bridge_regression.cpp - Regression Tests
//=============================================================================

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_training_logic_bridge.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TrainingLogicRegressionTest : public ::testing::Test {
protected:
    training_logic_bridge_t* bridge;
    training_logic_config_t config;

    void SetUp() override {
        training_logic_default_config(&config);
        config.enable_bio_async = false;
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            training_logic_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Memory Leak Regression Tests (3 tests)
//=============================================================================

TEST_F(TrainingLogicRegressionTest, NoMemoryLeaksOnDestroy) {
    for (int i = 0; i < 100; i++) {
        bridge = training_logic_create(&config);
        ASSERT_NE(bridge, nullptr);
        training_logic_destroy(bridge);
        bridge = nullptr;
    }
}

TEST_F(TrainingLogicRegressionTest, LongRunningMemoryStability) {
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);
    training_logic_start(bridge);

    training_logic_decision_t decision;
    for (int i = 0; i < 10000; i++) {
        training_logic_update_metrics(bridge, 0.5f, 0.3f, 0.001f, i);
        training_logic_get_decision(bridge, &decision);
    }
}

TEST_F(TrainingLogicRegressionTest, HistoryBufferNoLeak) {
    config.history_size = 100;
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);
    training_logic_start(bridge);

    // Fill history buffer multiple times
    for (int cycle = 0; cycle < 5; cycle++) {
        for (uint32_t i = 0; i < config.history_size * 2; i++) {
            training_logic_update_metrics(bridge, 0.5f, 0.3f, 0.001f, cycle * 1000 + i);
        }
    }
}

//=============================================================================
// Performance Regression Tests (4 tests)
//=============================================================================

TEST_F(TrainingLogicRegressionTest, GateEvaluationPerformance) {
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);
    training_logic_start(bridge);

    // Create custom gates
    uint32_t gates[10];
    const char* expressions[] = {
        "A AND B", "A OR B", "A XOR B", "A IMPLIES B", "NOT A",
        "A AND (B OR C)", "(A OR B) AND C", "A IMPLIES (B AND C)",
        "(A XOR B) OR C", "NOT (A AND B)"
    };

    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(training_logic_add_custom_gate(bridge, expressions[i], &gates[i]), 0);
    }

    // Measure evaluation performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        for (int g = 0; g < 10; g++) {
            training_logic_evaluate_gate(bridge, gates[g]);
        }
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // 10,000 evaluations should complete quickly (< 5000ms, relaxed for CI/parallel test contention)
    EXPECT_LT(duration.count(), 5000000);
}

TEST_F(TrainingLogicRegressionTest, DecisionLatency) {
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);
    training_logic_start(bridge);

    std::vector<uint64_t> latencies;
    training_logic_decision_t decision;

    for (int i = 0; i < 100; i++) {
        training_logic_update_metrics(bridge, 0.5f, 0.3f, 0.001f, i);
        training_logic_get_decision(bridge, &decision);
        latencies.push_back(decision.evaluation_time_us);
    }

    // Check variance - latencies should be stable
    uint64_t min_latency = *std::min_element(latencies.begin(), latencies.end());
    uint64_t max_latency = *std::max_element(latencies.begin(), latencies.end());

    // Max should not be more than 100x min
    uint64_t min_for_comparison = std::max(min_latency, (uint64_t)1);
    EXPECT_LT(max_latency, min_for_comparison * 100);
}

TEST_F(TrainingLogicRegressionTest, HighFrequencyUpdates) {
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);
    training_logic_start(bridge);

    auto start = std::chrono::high_resolution_clock::now();

    // Simulate high-frequency metric updates
    for (int i = 0; i < 10000; i++) {
        training_logic_update_metrics(bridge, 0.5f, 0.3f, 0.001f, i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 10,000 updates should complete quickly (< 1 second)
    EXPECT_LT(duration.count(), 1000);
}

TEST_F(TrainingLogicRegressionTest, LargeHistoryPerformance) {
    config.history_size = 1000;  // Large history
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);
    training_logic_start(bridge);

    auto start = std::chrono::high_resolution_clock::now();

    // Fill large history multiple times
    for (uint32_t i = 0; i < config.history_size * 3; i++) {
        training_logic_update_metrics(bridge, 0.5f, 0.3f, 0.001f, i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should handle large history efficiently (< 500ms for 3000 updates)
    EXPECT_LT(duration.count(), 500);
}

//=============================================================================
// Thread Safety Regression Tests (4 tests)
//=============================================================================

TEST_F(TrainingLogicRegressionTest, ConcurrentMetricUpdates) {
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);
    training_logic_start(bridge);

    const int num_threads = 8;
    const int iterations = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, iterations, t]() {
            for (int i = 0; i < iterations; i++) {
                training_logic_update_metrics(bridge, 0.5f, 0.3f, 0.001f, t * 1000 + i);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify no crashes
    training_logic_stats_t stats;
    EXPECT_EQ(training_logic_get_stats(bridge, &stats), 0);
}

TEST_F(TrainingLogicRegressionTest, ConcurrentDecisionQueries) {
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);
    training_logic_start(bridge);

    const int num_threads = 8;
    const int iterations = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, iterations]() {
            training_logic_decision_t decision;
            for (int i = 0; i < iterations; i++) {
                training_logic_get_decision(bridge, &decision);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    training_logic_stats_t stats;
    EXPECT_EQ(training_logic_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_decisions, num_threads * iterations);
}

TEST_F(TrainingLogicRegressionTest, ConcurrentConditionChanges) {
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);
    training_logic_start(bridge);

    std::thread t1([this]() {
        for (int i = 0; i < 200; i++) {
            training_logic_set_condition(bridge, TRAINING_COND_LOSS_STABLE, i % 2 == 0);
        }
    });

    std::thread t2([this]() {
        for (int i = 0; i < 200; i++) {
            training_logic_set_condition(bridge, TRAINING_COND_GRAD_STABLE, i % 2 == 0);
        }
    });

    std::thread t3([this]() {
        training_logic_decision_t decision;
        for (int i = 0; i < 200; i++) {
            training_logic_get_decision(bridge, &decision);
        }
    });

    t1.join();
    t2.join();
    t3.join();

    training_logic_stats_t stats;
    EXPECT_EQ(training_logic_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_decisions, 200);
}

TEST_F(TrainingLogicRegressionTest, RaceConditionPrevention) {
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);
    training_logic_start(bridge);

    const int num_threads = 16;
    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t]() {
            training_logic_decision_t decision;
            for (int i = 0; i < 50; i++) {
                training_logic_update_metrics(bridge, 0.5f, 0.3f, 0.001f, t * 100 + i);
                training_logic_get_decision(bridge, &decision);
                training_logic_set_condition(bridge, TRAINING_COND_LOSS_STABLE, i % 2 == 0);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

    // Should complete without deadlock (< 10 seconds)
    EXPECT_LT(duration.count(), 10);
}

//=============================================================================
// State Consistency Regression Tests (4 tests)
//=============================================================================

TEST_F(TrainingLogicRegressionTest, ConsistencyAfterRestart) {
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Start and stop multiple times
    for (int i = 0; i < 50; i++) {
        EXPECT_EQ(training_logic_start(bridge), 0);

        training_logic_decision_t decision;
        EXPECT_EQ(training_logic_get_decision(bridge, &decision), 0);

        EXPECT_EQ(training_logic_stop(bridge), 0);
    }

    // State should remain consistent
    training_logic_stats_t stats;
    EXPECT_EQ(training_logic_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_decisions, 50);
}

TEST_F(TrainingLogicRegressionTest, ConsistencyUnderLoad) {
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);
    training_logic_start(bridge);

    training_logic_decision_t decision;

    // High volume of mixed operations
    for (int i = 0; i < 1000; i++) {
        training_logic_update_metrics(bridge, 0.5f, 0.3f, 0.001f, i);
        training_logic_get_decision(bridge, &decision);
        training_logic_check_stability(bridge);
        training_logic_needs_intervention(bridge);
        training_logic_can_increase_lr(bridge);
    }

    // Verify stats consistency
    // Note: stability_checks is 2000 because both get_decision and check_stability
    // each increment the counter (1000 calls each)
    training_logic_stats_t stats;
    EXPECT_EQ(training_logic_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_decisions, 1000);
    EXPECT_EQ(stats.stability_checks, 2000);
}

TEST_F(TrainingLogicRegressionTest, ConsistencyWithIntegrations) {
    config.enable_immune_integration = true;
    config.enable_portia_integration = true;
    config.enable_swarm_integration = true;

    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);

    // Connect all integrations
    EXPECT_EQ(training_logic_connect_training_immune(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_connect_portia_logic(bridge, nullptr), 0);
    EXPECT_EQ(training_logic_connect_swarm_logic(bridge, nullptr), 0);

    training_logic_start(bridge);

    // Make many decisions with all integrations
    training_logic_decision_t decision;
    for (int i = 0; i < 500; i++) {
        training_logic_update_metrics(bridge, 0.5f, 0.3f, 0.001f, i);
        training_logic_get_decision(bridge, &decision);
    }

    training_logic_stats_t stats;
    EXPECT_EQ(training_logic_get_stats(bridge, &stats), 0);
    EXPECT_EQ(stats.total_decisions, 500);
}

TEST_F(TrainingLogicRegressionTest, ConditionStateConsistency) {
    bridge = training_logic_create(&config);
    ASSERT_NE(bridge, nullptr);
    training_logic_start(bridge);

    // Set all conditions and verify they persist
    for (int cond = 0; cond < TRAINING_COND_COUNT; cond++) {
        EXPECT_EQ(training_logic_set_condition(bridge, (training_logic_condition_t)cond, true), 0);
    }

    // Get conditions and verify
    training_logic_conditions_t conditions;
    EXPECT_EQ(training_logic_get_conditions(bridge, &conditions), 0);

    EXPECT_TRUE(conditions.loss_stable);
    EXPECT_TRUE(conditions.grad_stable);
    EXPECT_TRUE(conditions.lr_reasonable);

    // Update metrics and verify conditions update
    EXPECT_EQ(training_logic_update_metrics(bridge, 0.5f, 0.3f, 0.001f, 100), 0);

    EXPECT_EQ(training_logic_get_conditions(bridge, &conditions), 0);
    EXPECT_EQ(conditions.current_step, 100);
    EXPECT_FLOAT_EQ(conditions.loss_current, 0.5f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
