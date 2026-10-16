//=============================================================================
// test_portia_swarm_logic_regression.cpp - Regression Tests
//=============================================================================

#include <gtest/gtest.h>
#include <thread>
#include <vector>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_swarm_logic_bridge.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PortiaSwarmLogicRegressionTest : public ::testing::Test {
protected:
    portia_swarm_logic_bridge_t* bridge;
    portia_swarm_logic_config_t config;

    void SetUp() override {
        portia_swarm_logic_default_config(&config);
        config.enable_bio_async = false;
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            portia_swarm_logic_destroy(bridge);
            bridge = nullptr;
        }
    }
};

//=============================================================================
// Memory Leak Regression Tests (3 tests)
//=============================================================================

TEST_F(PortiaSwarmLogicRegressionTest, NoLeakCreateDestroyCycles) {
    for (int i = 0; i < 100; i++) {
        bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
        portia_swarm_logic_destroy(bridge);
        bridge = nullptr;
    }
}

TEST_F(PortiaSwarmLogicRegressionTest, NoLeakDecisionCycles) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;
    for (int i = 0; i < 1000; i++) {
        portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
        portia_swarm_logic_decide_degradation(bridge, i % 100, &result);
        portia_swarm_logic_decide_resource_allocation(bridge, i, 0.5f, &result);
    }
}

TEST_F(PortiaSwarmLogicRegressionTest, NoLeakGateCreation) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < 100; i++) {
        uint32_t gate_id;
        const char* expr = (i % 2 == 0) ? "A AND B" : "A OR B";
        portia_swarm_logic_add_unified_gate(bridge, expr, &gate_id);
    }
}

//=============================================================================
// Performance Stability Regression Tests (4 tests)
//=============================================================================

TEST_F(PortiaSwarmLogicRegressionTest, StableDecisionLatency) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_logic_start(bridge);

    std::vector<uint64_t> latencies;
    unified_decision_result_t result;

    for (int i = 0; i < 100; i++) {
        portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
        latencies.push_back(result.decision_time_us);
    }

    // Check variance - latencies should be relatively stable
    uint64_t min_latency = *std::min_element(latencies.begin(), latencies.end());
    uint64_t max_latency = *std::max_element(latencies.begin(), latencies.end());

    // Max should not be more than 100x min (very loose bound)
    // Use max(min_latency, 1) to avoid edge case when min_latency is 0
    uint64_t min_for_comparison = std::max(min_latency, (uint64_t)1);
    EXPECT_LT(max_latency, min_for_comparison * 100);
}

TEST_F(PortiaSwarmLogicRegressionTest, NoPerformanceDegradationOverTime) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;

    // Measure first 100 decisions
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);

    // Measure next 100 decisions after 900 more
    for (int i = 0; i < 900; i++) {
        portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
    }

    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);

    // Later measurements should not be significantly slower (< 2x)
    EXPECT_LT(duration2.count(), duration1.count() * 2);
}

TEST_F(PortiaSwarmLogicRegressionTest, ConsistentStatisticsAccumulation) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;
    portia_swarm_logic_stats_t stats;

    // Make 50 decisions and check stats
    for (int i = 0; i < 50; i++) {
        portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
    }

    portia_swarm_logic_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decisions, 50);

    // Make 50 more
    for (int i = 0; i < 50; i++) {
        portia_swarm_logic_decide_degradation(bridge, i, &result);
    }

    portia_swarm_logic_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decisions, 100);

    // Reset and verify
    portia_swarm_logic_reset_stats(bridge);
    portia_swarm_logic_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decisions, 0);
}

TEST_F(PortiaSwarmLogicRegressionTest, StableUnderHighLoad) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;

    // High volume of decisions
    for (int i = 0; i < 10000; i++) {
        int decision_type = i % 4;
        switch (decision_type) {
            case 0:
                portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
                break;
            case 1:
                portia_swarm_logic_decide_degradation(bridge, i, &result);
                break;
            case 2:
                portia_swarm_logic_decide_resource_allocation(bridge, i, 0.5f, &result);
                break;
            case 3:
                portia_swarm_logic_decide_emergency_mode(bridge, &result);
                break;
        }
    }

    portia_swarm_logic_stats_t stats;
    portia_swarm_logic_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decisions, 10000);
}

//=============================================================================
// Thread Safety Regression Tests (3 tests)
//=============================================================================

TEST_F(PortiaSwarmLogicRegressionTest, ConcurrentAccessStability) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_logic_start(bridge);

    const int num_threads = 8;
    const int iterations = 100;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, iterations]() {
            unified_decision_result_t result;
            for (int i = 0; i < iterations; i++) {
                portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    portia_swarm_logic_stats_t stats;
    portia_swarm_logic_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decisions, num_threads * iterations);
}

TEST_F(PortiaSwarmLogicRegressionTest, ConcurrentMixedOperations) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_logic_start(bridge);

    std::thread t1([this]() {
        unified_decision_result_t result;
        for (int i = 0; i < 200; i++) {
            portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
        }
    });

    std::thread t2([this]() {
        unified_decision_result_t result;
        for (int i = 0; i < 200; i++) {
            portia_swarm_logic_decide_degradation(bridge, i, &result);
        }
    });

    std::thread t3([this]() {
        portia_swarm_logic_stats_t stats;
        for (int i = 0; i < 200; i++) {
            portia_swarm_logic_get_stats(bridge, &stats);
        }
    });

    t1.join();
    t2.join();
    t3.join();

    portia_swarm_logic_stats_t stats;
    portia_swarm_logic_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decisions, 400);
}

TEST_F(PortiaSwarmLogicRegressionTest, NoDeadlockUnderContention) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_logic_start(bridge);

    const int num_threads = 16;
    std::vector<std::thread> threads;

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t]() {
            unified_decision_result_t result;
            for (int i = 0; i < 50; i++) {
                portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result);
                portia_swarm_logic_connect_brain(bridge, nullptr);
                portia_swarm_logic_reset_stats(bridge);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

    // Should complete in reasonable time (< 10 seconds)
    EXPECT_LT(duration.count(), 10);
}

//=============================================================================
// Edge Case Regression Tests (5 tests)
//=============================================================================

TEST_F(PortiaSwarmLogicRegressionTest, ZeroWeightConfiguration) {
    config.local_weight = 0.0f;
    config.collective_weight = 0.0f;

    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result), 0);
    // Should not crash with zero weights
}

TEST_F(PortiaSwarmLogicRegressionTest, ExtremeConfidenceThresholds) {
    // Test with 0.0 threshold
    config.confidence_threshold = 0.0f;
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result), 0);

    portia_swarm_logic_destroy(bridge);

    // Test with 1.0 threshold
    config.confidence_threshold = 1.0f;
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_logic_start(bridge);

    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 0, 1, &result), 0);
}

TEST_F(PortiaSwarmLogicRegressionTest, VeryLongDecisionChain) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;

    // Chain of 1000 decisions
    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, i % 4, (i + 1) % 4, &result), 0);
    }

    portia_swarm_logic_stats_t stats;
    portia_swarm_logic_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_decisions, 1000);
}

TEST_F(PortiaSwarmLogicRegressionTest, RapidStartStopCycles) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < 50; i++) {
        EXPECT_EQ(portia_swarm_logic_start(bridge), 0);
        EXPECT_EQ(portia_swarm_logic_stop(bridge), 0);
    }
}

TEST_F(PortiaSwarmLogicRegressionTest, InvalidParameterRobustness) {
    bridge = portia_swarm_logic_create(&config, nullptr, nullptr, nullptr);
    ASSERT_NE(bridge, nullptr);
    portia_swarm_logic_start(bridge);

    unified_decision_result_t result;

    // Invalid resource amounts
    EXPECT_NE(portia_swarm_logic_decide_resource_allocation(bridge, 0, -0.5f, &result), 0);
    EXPECT_NE(portia_swarm_logic_decide_resource_allocation(bridge, 0, 2.0f, &result), 0);

    // Extreme tier values (should still work)
    EXPECT_EQ(portia_swarm_logic_decide_tier_change(bridge, 255, 0, &result), 0);

    // Extreme feature IDs (should still work)
    EXPECT_EQ(portia_swarm_logic_decide_degradation(bridge, UINT32_MAX, &result), 0);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
