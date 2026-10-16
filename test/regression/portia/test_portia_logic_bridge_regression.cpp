/**
 * @file test_portia_logic_bridge_regression.cpp
 * @brief Regression tests for Portia-Logic Bridge
 *
 * TEST COVERAGE:
 * - Performance stability (4 tests)
 * - Memory leak detection (3 tests)
 * - Decision consistency (4 tests)
 * - Long-running operation (3 tests)
 * - Stress testing (3 tests)
 * - Edge case handling (3 tests)
 *
 * TOTAL: 20 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <vector>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_logic_bridge.h"
#include "portia/nimcp_portia.h"
#include "utils/logging/nimcp_logging.h"

class PortiaLogicRegressionTest : public ::testing::Test {
protected:
    portia_logic_bridge_t* bridge;
    portia_logic_config_t config;
    portia_context_t* portia;

    void SetUp() override {
        /* Initialize Portia */
        portia_config_t portia_cfg = portia_get_default_config();
        portia_cfg.enable_bio_async = false;
        portia_cfg.enable_metrics = true;
        portia_init(&portia_cfg);
        portia = portia_get_context();
        ASSERT_NE(portia, nullptr);

        /* Create bridge */
        portia_logic_bridge_get_default_config(&config);
        config.enable_bio_async = false;
        config.disable_auto_update = true;  /* Allow manual condition control in tests */

        bridge = portia_logic_bridge_create(&config, portia);
        ASSERT_NE(bridge, nullptr);

        portia_logic_bridge_start(bridge);
    }

    void TearDown() override {
        if (bridge) {
            portia_logic_bridge_stop(bridge);
            portia_logic_bridge_destroy(bridge);
        }
        portia_destroy();
    }

    /* Helper: Measure evaluation time */
    uint64_t measure_evaluation_time(
        std::function<void()> eval_func,
        int iterations)
    {
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; i++) {
            eval_func();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        return duration.count();
    }
};

/*=============================================================================
 * PERFORMANCE STABILITY TESTS (4 tests)
 *============================================================================*/

TEST_F(PortiaLogicRegressionTest, EvaluationPerformanceStability) {
    /* Set up conditions */
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);

    const int iterations = 1000;

    /* Measure evaluation time */
    uint64_t time_us = measure_evaluation_time([this]() {
        portia_logic_can_upgrade_tier(this->bridge, 0, 1);
    }, iterations);

    float avg_time_us = static_cast<float>(time_us) / iterations;

    /* Performance threshold: < 100us per evaluation */
    EXPECT_LT(avg_time_us, 100.0f);
    NIMCP_LOGGING_INFO("Average evaluation time: %.2f us", avg_time_us);
}

TEST_F(PortiaLogicRegressionTest, PerformanceConsistencyAcrossDecisionTypes) {
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);
    portia_logic_set_condition(bridge, "cpu_ok", true);

    const int iterations = 500;

    /* Measure different decision types */
    uint64_t upgrade_time = measure_evaluation_time([this]() {
        portia_logic_can_upgrade_tier(this->bridge, 0, 1);
    }, iterations);

    uint64_t downgrade_time = measure_evaluation_time([this]() {
        portia_logic_must_downgrade_tier(this->bridge, 2);
    }, iterations);

    uint64_t degradation_time = measure_evaluation_time([this]() {
        portia_logic_can_disable_feature(this->bridge, 100);
    }, iterations);

    uint64_t allocation_time = measure_evaluation_time([this]() {
        portia_logic_can_allocate_resource(this->bridge, 1, 0.5f);
    }, iterations);

    /* All should be within same order of magnitude */
    float max_time = std::max({upgrade_time, downgrade_time, degradation_time, allocation_time});
    float min_time = std::min({upgrade_time, downgrade_time, degradation_time, allocation_time});

    EXPECT_LT(max_time / min_time, 10.0f);  /* < 10x variance */
}

TEST_F(PortiaLogicRegressionTest, NoPerformanceDegradationOverTime) {
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);

    /* Measure early performance */
    uint64_t early_time = measure_evaluation_time([this]() {
        portia_logic_can_upgrade_tier(this->bridge, 0, 1);
    }, 100);

    /* Run many evaluations */
    for (int i = 0; i < 10000; i++) {
        portia_logic_can_upgrade_tier(bridge, 0, 1);
    }

    /* Measure late performance */
    uint64_t late_time = measure_evaluation_time([this]() {
        portia_logic_can_upgrade_tier(this->bridge, 0, 1);
    }, 100);

    /* Performance should not degrade > 20% */
    float ratio = static_cast<float>(late_time) / static_cast<float>(early_time);
    EXPECT_LT(ratio, 1.2f);
}

TEST_F(PortiaLogicRegressionTest, ConcurrentEvaluationPerformance) {
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);

    const int num_threads = 8;
    const int iterations_per_thread = 100;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this, iterations_per_thread]() {
            for (int j = 0; j < iterations_per_thread; j++) {
                portia_logic_can_upgrade_tier(this->bridge, 0, 1);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float avg_time_us = static_cast<float>(duration.count()) / (num_threads * iterations_per_thread);

    /* Should not be much slower than single-threaded */
    EXPECT_LT(avg_time_us, 500.0f);  /* < 500us per eval with contention */
}

/*=============================================================================
 * MEMORY LEAK DETECTION TESTS (3 tests)
 *============================================================================*/

TEST_F(PortiaLogicRegressionTest, NoLeaksInRepeatedCreationDestruction) {
    /* Create and destroy multiple times */
    for (int i = 0; i < 100; i++) {
        portia_logic_bridge_t* temp_bridge = portia_logic_bridge_create(&config, portia);
        ASSERT_NE(temp_bridge, nullptr);
        portia_logic_bridge_destroy(temp_bridge);
    }

    /* If there were leaks, this would show up in valgrind/asan */
    SUCCEED();
}

TEST_F(PortiaLogicRegressionTest, NoLeaksInCustomGateCreation) {
    /* Add many custom gates */
    for (int i = 0; i < 50; i++) {
        uint32_t gate_id = 0;
        const char* expressions[] = {"A AND B", "A OR B", "NOT A", "A XOR B", "A IMPLIES B"};
        portia_logic_add_custom_gate(bridge, expressions[i % 5], &gate_id);
    }

    /* Destroy bridge - should clean up all gates */
    portia_logic_bridge_destroy(bridge);
    bridge = nullptr;  /* Prevent double-free in TearDown */

    SUCCEED();
}

TEST_F(PortiaLogicRegressionTest, NoLeaksInRepeatedEvaluations) {
    portia_logic_set_condition(bridge, "memory_ok", true);

    /* Perform many evaluations */
    for (int i = 0; i < 10000; i++) {
        portia_logic_can_upgrade_tier(bridge, 0, 1);
        portia_logic_must_downgrade_tier(bridge, 2);
        portia_logic_can_disable_feature(bridge, 100);
        portia_logic_can_allocate_resource(bridge, 1, 0.5f);
    }

    SUCCEED();
}

/*=============================================================================
 * DECISION CONSISTENCY TESTS (4 tests)
 *============================================================================*/

TEST_F(PortiaLogicRegressionTest, ConsistentUpgradeDecisions) {
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);

    /* Same conditions should yield same result */
    bool result1 = portia_logic_can_upgrade_tier(bridge, 0, 1);
    bool result2 = portia_logic_can_upgrade_tier(bridge, 0, 1);
    bool result3 = portia_logic_can_upgrade_tier(bridge, 0, 1);

    EXPECT_EQ(result1, result2);
    EXPECT_EQ(result2, result3);
    EXPECT_TRUE(result1);  /* All conditions OK */
}

TEST_F(PortiaLogicRegressionTest, ConsistentDowngradeDecisions) {
    portia_logic_set_condition(bridge, "memory_ok", false);
    portia_logic_set_condition(bridge, "thermal_ok", false);
    portia_logic_set_condition(bridge, "battery_ok", false);

    /* Same critical conditions should yield same result */
    bool result1 = portia_logic_must_downgrade_tier(bridge, 2);
    bool result2 = portia_logic_must_downgrade_tier(bridge, 2);
    bool result3 = portia_logic_must_downgrade_tier(bridge, 2);

    EXPECT_EQ(result1, result2);
    EXPECT_EQ(result2, result3);
    EXPECT_TRUE(result1);  /* All critical */
}

TEST_F(PortiaLogicRegressionTest, ConsistentDegradationDecisions) {
    portia_logic_set_condition(bridge, "memory_ok", false);
    portia_logic_set_condition(bridge, "thermal_ok", false);
    portia_logic_set_condition(bridge, "battery_ok", false);
    portia_logic_set_condition(bridge, "cpu_ok", false);

    /* Resource score = 0, should consistently allow degradation */
    bool result1 = portia_logic_can_disable_feature(bridge, 100);
    bool result2 = portia_logic_can_disable_feature(bridge, 100);
    bool result3 = portia_logic_can_disable_feature(bridge, 100);

    EXPECT_EQ(result1, result2);
    EXPECT_EQ(result2, result3);
    EXPECT_TRUE(result1);
}

TEST_F(PortiaLogicRegressionTest, DecisionTransitionsAreReversible) {
    /* Start OK */
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);

    bool can_upgrade_1 = portia_logic_can_upgrade_tier(bridge, 0, 1);
    EXPECT_TRUE(can_upgrade_1);

    /* Transition to critical */
    portia_logic_set_condition(bridge, "memory_ok", false);
    portia_logic_set_condition(bridge, "thermal_ok", false);

    bool can_upgrade_2 = portia_logic_can_upgrade_tier(bridge, 0, 1);
    EXPECT_FALSE(can_upgrade_2);

    /* Transition back to OK */
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);

    bool can_upgrade_3 = portia_logic_can_upgrade_tier(bridge, 0, 1);
    EXPECT_TRUE(can_upgrade_3);

    /* Should match original state */
    EXPECT_EQ(can_upgrade_1, can_upgrade_3);
}

/*=============================================================================
 * LONG-RUNNING OPERATION TESTS (3 tests)
 *============================================================================*/

TEST_F(PortiaLogicRegressionTest, ExtendedOperationStability) {
    portia_logic_set_condition(bridge, "memory_ok", true);

    /* Run for 10 seconds */
    auto start = std::chrono::steady_clock::now();
    auto end = start + std::chrono::seconds(10);

    int evaluation_count = 0;
    while (std::chrono::steady_clock::now() < end) {
        portia_logic_can_upgrade_tier(bridge, 0, 1);
        evaluation_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_GT(evaluation_count, 5000);  /* Should complete many evaluations */
}

TEST_F(PortiaLogicRegressionTest, StatisticsAccuracyLongTerm) {
    portia_logic_reset_stats(bridge);

    const int target_evals = 1000;

    /* Perform exactly 1000 evaluations */
    for (int i = 0; i < target_evals; i++) {
        portia_logic_can_upgrade_tier(bridge, 0, 1);
    }

    portia_logic_stats_t stats;
    portia_logic_get_stats(bridge, &stats);

    /* Stats should be accurate */
    EXPECT_EQ(stats.tier_upgrade_decisions, target_evals);
    EXPECT_GE(stats.total_evaluations, target_evals);
}

TEST_F(PortiaLogicRegressionTest, NoStatisticsOverflow) {
    portia_logic_reset_stats(bridge);

    /* Perform many evaluations to test counter overflow resistance */
    for (int i = 0; i < 100000; i++) {
        if (i % 4 == 0) {
            portia_logic_can_upgrade_tier(bridge, 0, 1);
        } else if (i % 4 == 1) {
            portia_logic_must_downgrade_tier(bridge, 2);
        } else if (i % 4 == 2) {
            portia_logic_can_disable_feature(bridge, 100);
        } else {
            portia_logic_can_allocate_resource(bridge, 1, 0.5f);
        }
    }

    portia_logic_stats_t stats;
    portia_logic_get_stats(bridge, &stats);

    /* Counters should not overflow (uint64_t can handle this) */
    EXPECT_GT(stats.total_evaluations, 0u);
    EXPECT_LE(stats.total_evaluations, 100000u);
}

/*=============================================================================
 * STRESS TESTING (3 tests)
 *============================================================================*/

TEST_F(PortiaLogicRegressionTest, RapidConditionChanges) {
    /* Rapidly toggle conditions */
    for (int i = 0; i < 1000; i++) {
        bool value = (i % 2 == 0);
        portia_logic_set_condition(bridge, "memory_ok", value);
        portia_logic_set_condition(bridge, "thermal_ok", !value);
        portia_logic_set_condition(bridge, "battery_ok", value);

        /* Evaluate after each change */
        portia_logic_can_upgrade_tier(bridge, 0, 1);
    }

    SUCCEED();
}

TEST_F(PortiaLogicRegressionTest, MaximumCustomGates) {
    /* Add maximum allowed custom gates */
    int max_gates = config.max_custom_rules;
    int created = 0;

    for (int i = 0; i < max_gates; i++) {
        uint32_t gate_id = 0;
        const char* expressions[] = {"A AND B", "A OR B", "NOT A", "A XOR B", "A IMPLIES B"};
        int result = portia_logic_add_custom_gate(bridge, expressions[i % 5], &gate_id);
        if (result == NIMCP_SUCCESS) {
            created++;
        }
    }

    EXPECT_GT(created, 0);

    /* Verify gate count */
    uint32_t count = portia_logic_get_gate_count(bridge);
    EXPECT_GT(count, 5u);  /* At least pre-built gates */
}

TEST_F(PortiaLogicRegressionTest, HighConcurrencyStress) {
    const int num_threads = 32;
    const int iterations_per_thread = 1000;

    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back([this, i, iterations_per_thread]() {
            for (int j = 0; j < iterations_per_thread; j++) {
                /* Different threads do different operations */
                if (i % 4 == 0) {
                    portia_logic_can_upgrade_tier(this->bridge, 0, 1);
                } else if (i % 4 == 1) {
                    portia_logic_must_downgrade_tier(this->bridge, 2);
                } else if (i % 4 == 2) {
                    portia_logic_can_disable_feature(this->bridge, 100);
                } else {
                    portia_logic_can_allocate_resource(this->bridge, 1, 0.5f);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    /* Verify stats */
    portia_logic_stats_t stats;
    portia_logic_get_stats(bridge, &stats);

    EXPECT_GT(stats.total_evaluations, 0u);
    EXPECT_LE(stats.total_evaluations, num_threads * iterations_per_thread);
}

/*=============================================================================
 * EDGE CASE HANDLING (3 tests)
 *============================================================================*/

TEST_F(PortiaLogicRegressionTest, ResourceScoreBoundaries) {
    /* Test resource score at boundaries */

    /* All OK: score = 1.0 */
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);
    portia_logic_set_condition(bridge, "cpu_ok", true);

    portia_resource_condition_t conditions;
    portia_logic_get_conditions(bridge, &conditions);
    EXPECT_FLOAT_EQ(conditions.resource_score, 1.0f);

    /* All critical: score = 0.0 */
    portia_logic_set_condition(bridge, "memory_ok", false);
    portia_logic_set_condition(bridge, "thermal_ok", false);
    portia_logic_set_condition(bridge, "battery_ok", false);
    portia_logic_set_condition(bridge, "cpu_ok", false);

    portia_logic_get_conditions(bridge, &conditions);
    EXPECT_FLOAT_EQ(conditions.resource_score, 0.0f);

    /* Exactly half: score = 0.5 */
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", false);
    portia_logic_set_condition(bridge, "cpu_ok", false);

    portia_logic_get_conditions(bridge, &conditions);
    EXPECT_FLOAT_EQ(conditions.resource_score, 0.5f);
}

TEST_F(PortiaLogicRegressionTest, AllocationAmountBoundaries) {
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);
    portia_logic_set_condition(bridge, "cpu_ok", true);

    /* Test boundary allocations */
    bool can_allocate_zero = portia_logic_can_allocate_resource(bridge, 1, 0.0f);
    EXPECT_TRUE(can_allocate_zero);

    bool can_allocate_full = portia_logic_can_allocate_resource(bridge, 1, 1.0f);
    EXPECT_TRUE(can_allocate_full);

    bool can_allocate_over = portia_logic_can_allocate_resource(bridge, 1, 1.1f);
    EXPECT_FALSE(can_allocate_over);  /* Over limit */

    bool can_allocate_negative = portia_logic_can_allocate_resource(bridge, 1, -0.1f);
    EXPECT_FALSE(can_allocate_negative);  /* Invalid */
}

TEST_F(PortiaLogicRegressionTest, TierTransitionBoundaries) {
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);

    /* Test tier boundaries */
    bool can_upgrade_same = portia_logic_can_upgrade_tier(bridge, 2, 2);
    EXPECT_FALSE(can_upgrade_same);  /* Same tier */

    bool can_upgrade_down = portia_logic_can_upgrade_tier(bridge, 3, 1);
    EXPECT_FALSE(can_upgrade_down);  /* Downgrade, not upgrade */

    bool can_upgrade_valid = portia_logic_can_upgrade_tier(bridge, 0, 255);
    EXPECT_TRUE(can_upgrade_valid);  /* Large jump but valid */
}

/*=============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
