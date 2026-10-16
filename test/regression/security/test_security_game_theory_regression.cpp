/**
 * @file test_security_game_theory_regression.cpp
 * @brief Regression tests for Security-Game Theory Bridge
 *
 * WHAT: Regression tests for security-game theory bidirectional bridge
 * WHY:  Verify performance characteristics and prevent regressions in
 *       decision latency, throughput, and resource usage
 * HOW:  Measure timing for all key operations, verify against thresholds,
 *       ensure consistent behavior across updates
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <vector>
#include <numeric>

extern "C" {
#include "security/game_theory/nimcp_security_game_theory_bridge.h"
#include "cognitive/game_theory/nimcp_game_theory.h"
#include "utils/error/nimcp_error_codes.h"
}

// ============================================================================
// Timing Utilities
// ============================================================================

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<double, std::micro>;

static double measure_time_us(std::function<void()> fn) {
    auto start = Clock::now();
    fn();
    auto end = Clock::now();
    return std::chrono::duration_cast<Duration>(end - start).count();
}

// ============================================================================
// Performance Thresholds (microseconds)
// ============================================================================

/* These thresholds should be achievable on typical hardware */
static constexpr double THRESHOLD_BRIDGE_CREATE_US = 1000.0;      /* 1ms */
static constexpr double THRESHOLD_BRIDGE_DESTROY_US = 500.0;       /* 0.5ms */
static constexpr double THRESHOLD_PAYOFF_VALIDATE_US = 100.0;      /* 100us for 4x4 */
static constexpr double THRESHOLD_COALITION_MONITOR_US = 50.0;     /* 50us */
static constexpr double THRESHOLD_MANIPULATION_DETECT_US = 100.0;  /* 100us */
static constexpr double THRESHOLD_THREAT_ANALYSIS_US = 500.0;      /* 500us for 3x3 */
static constexpr double THRESHOLD_BRIDGE_UPDATE_US = 50.0;         /* 50us */
static constexpr double THRESHOLD_GET_STATE_US = 20.0;             /* 20us */
static constexpr double THRESHOLD_GET_STATS_US = 20.0;             /* 20us */

/* Throughput thresholds (operations per second) */
static constexpr double THROUGHPUT_PAYOFF_VALIDATE_OPS = 10000.0;  /* 10K ops/s */
static constexpr double THROUGHPUT_COALITION_MONITOR_OPS = 20000.0; /* 20K ops/s */
static constexpr double THROUGHPUT_BRIDGE_UPDATE_OPS = 50000.0;    /* 50K ops/s */

// ============================================================================
// Test Fixture
// ============================================================================

class SecurityGameTheoryRegressionTest : public ::testing::Test {
protected:
    security_game_theory_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            security_gt_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    void CreateBridge() {
        security_game_theory_config_t config;
        security_gt_default_config(&config);
        bridge = security_gt_bridge_create(&config);
    }

    void CreateBridgeWithConfig(const security_game_theory_config_t* config) {
        bridge = security_gt_bridge_create(config);
    }

    /* Helper to compute average time */
    double average_time(const std::vector<double>& times) {
        if (times.empty()) return 0.0;
        return std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    }

    /* Helper to compute median time */
    double median_time(std::vector<double> times) {
        if (times.empty()) return 0.0;
        std::sort(times.begin(), times.end());
        size_t n = times.size();
        if (n % 2 == 0) {
            return (times[n/2 - 1] + times[n/2]) / 2.0;
        }
        return times[n/2];
    }

    /* Helper to compute 95th percentile */
    double percentile_95(std::vector<double> times) {
        if (times.empty()) return 0.0;
        std::sort(times.begin(), times.end());
        size_t idx = static_cast<size_t>(times.size() * 0.95);
        if (idx >= times.size()) idx = times.size() - 1;
        return times[idx];
    }
};

// ============================================================================
// Lifecycle Performance Tests
// ============================================================================

TEST_F(SecurityGameTheoryRegressionTest, BridgeCreateLatency) {
    const int iterations = 100;
    std::vector<double> times;

    for (int i = 0; i < iterations; i++) {
        double time = measure_time_us([&]() {
            security_game_theory_config_t config;
            security_gt_default_config(&config);
            security_game_theory_bridge_t* b = security_gt_bridge_create(&config);
            if (b) security_gt_bridge_destroy(b);
        });
        times.push_back(time);
    }

    double avg = average_time(times);
    double p95 = percentile_95(times);

    EXPECT_LT(avg, THRESHOLD_BRIDGE_CREATE_US * 2)
        << "Average bridge create time: " << avg << " us";
    EXPECT_LT(p95, THRESHOLD_BRIDGE_CREATE_US * 3)
        << "P95 bridge create time: " << p95 << " us";
}

TEST_F(SecurityGameTheoryRegressionTest, BridgeDestroyLatency) {
    const int iterations = 100;
    std::vector<double> times;

    for (int i = 0; i < iterations; i++) {
        security_game_theory_config_t config;
        security_gt_default_config(&config);
        security_game_theory_bridge_t* b = security_gt_bridge_create(&config);
        if (!b) continue;

        double time = measure_time_us([&]() {
            security_gt_bridge_destroy(b);
        });
        times.push_back(time);
    }

    double avg = average_time(times);
    double p95 = percentile_95(times);

    EXPECT_LT(avg, THRESHOLD_BRIDGE_DESTROY_US)
        << "Average bridge destroy time: " << avg << " us";
    EXPECT_LT(p95, THRESHOLD_BRIDGE_DESTROY_US * 2)
        << "P95 bridge destroy time: " << p95 << " us";
}

// ============================================================================
// Payoff Validation Performance Tests
// ============================================================================

TEST_F(SecurityGameTheoryRegressionTest, PayoffValidationLatency) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int iterations = 1000;
    std::vector<double> times;
    float payoffs[16];
    for (int i = 0; i < 16; i++) payoffs[i] = (float)i;

    for (int i = 0; i < iterations; i++) {
        security_payoff_result_t result;
        double time = measure_time_us([&]() {
            security_gt_validate_payoff_matrix(bridge, payoffs, 4, 4, &result);
        });
        times.push_back(time);
    }

    double avg = average_time(times);
    double median = median_time(times);
    double p95 = percentile_95(times);

    EXPECT_LT(avg, THRESHOLD_PAYOFF_VALIDATE_US)
        << "Average payoff validation time: " << avg << " us";
    EXPECT_LT(p95, THRESHOLD_PAYOFF_VALIDATE_US * 3)
        << "P95 payoff validation time: " << p95 << " us";

    /* Log performance */
    std::cout << "Payoff validation (4x4): avg=" << avg << "us, median=" << median
              << "us, p95=" << p95 << "us" << std::endl;
}

TEST_F(SecurityGameTheoryRegressionTest, PayoffValidationThroughput) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int total_operations = 10000;
    float payoffs[16];
    for (int i = 0; i < 16; i++) payoffs[i] = (float)i;

    auto start = Clock::now();
    for (int i = 0; i < total_operations; i++) {
        security_payoff_result_t result;
        security_gt_validate_payoff_matrix(bridge, payoffs, 4, 4, &result);
    }
    auto end = Clock::now();

    double duration_s = std::chrono::duration<double>(end - start).count();
    double ops_per_second = total_operations / duration_s;

    EXPECT_GT(ops_per_second, THROUGHPUT_PAYOFF_VALIDATE_OPS)
        << "Payoff validation throughput: " << ops_per_second << " ops/s";

    std::cout << "Payoff validation throughput: " << ops_per_second << " ops/s" << std::endl;
}

TEST_F(SecurityGameTheoryRegressionTest, PayoffValidationScaling) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Test scaling with matrix size */
    std::vector<uint32_t> sizes = {2, 4, 8, 16, 32};
    std::vector<double> avg_times;

    for (uint32_t size : sizes) {
        std::vector<float> payoffs(size * size);
        for (uint32_t i = 0; i < size * size; i++) {
            payoffs[i] = (float)i * 0.01f;
        }

        std::vector<double> times;
        for (int iter = 0; iter < 100; iter++) {
            security_payoff_result_t result;
            double time = measure_time_us([&]() {
                security_gt_validate_payoff_matrix(bridge, payoffs.data(), size, size, &result);
            });
            times.push_back(time);
        }

        double avg = average_time(times);
        avg_times.push_back(avg);
        std::cout << "Payoff validation " << size << "x" << size << ": " << avg << " us" << std::endl;
    }

    /* Verify roughly linear scaling */
    if (avg_times.size() >= 2) {
        double ratio = avg_times.back() / avg_times.front();
        uint32_t size_ratio = sizes.back() * sizes.back() / (sizes.front() * sizes.front());
        /* Allow 10x overhead for constant factors */
        EXPECT_LT(ratio, size_ratio * 10)
            << "Payoff validation scaling ratio: " << ratio
            << " for size ratio: " << size_ratio;
    }
}

// ============================================================================
// Coalition Monitoring Performance Tests
// ============================================================================

TEST_F(SecurityGameTheoryRegressionTest, CoalitionMonitoringLatency) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int iterations = 1000;
    std::vector<double> times;
    uint32_t players[4] = {0, 1, 2, 3};

    for (int i = 0; i < iterations; i++) {
        security_coalition_result_t result;
        double time = measure_time_us([&]() {
            security_gt_monitor_coalition(bridge, 0xF, players, 4, &result);
        });
        times.push_back(time);
    }

    double avg = average_time(times);
    double p95 = percentile_95(times);

    EXPECT_LT(avg, THRESHOLD_COALITION_MONITOR_US)
        << "Average coalition monitoring time: " << avg << " us";
    EXPECT_LT(p95, THRESHOLD_COALITION_MONITOR_US * 3)
        << "P95 coalition monitoring time: " << p95 << " us";

    std::cout << "Coalition monitoring: avg=" << avg << "us, p95=" << p95 << "us" << std::endl;
}

TEST_F(SecurityGameTheoryRegressionTest, CoalitionMonitoringThroughput) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int total_operations = 10000;
    uint32_t players[4] = {0, 1, 2, 3};

    auto start = Clock::now();
    for (int i = 0; i < total_operations; i++) {
        security_coalition_result_t result;
        security_gt_monitor_coalition(bridge, 0xF, players, 4, &result);
    }
    auto end = Clock::now();

    double duration_s = std::chrono::duration<double>(end - start).count();
    double ops_per_second = total_operations / duration_s;

    EXPECT_GT(ops_per_second, THROUGHPUT_COALITION_MONITOR_OPS)
        << "Coalition monitoring throughput: " << ops_per_second << " ops/s";

    std::cout << "Coalition monitoring throughput: " << ops_per_second << " ops/s" << std::endl;
}

// ============================================================================
// Manipulation Detection Performance Tests
// ============================================================================

TEST_F(SecurityGameTheoryRegressionTest, ManipulationDetectionLatency) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int iterations = 1000;
    std::vector<double> times;
    uint32_t actions[16] = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};

    for (int i = 0; i < iterations; i++) {
        security_manipulation_result_t result;
        double time = measure_time_us([&]() {
            security_gt_detect_manipulation(bridge, 0, actions, 16, &result);
        });
        times.push_back(time);
    }

    double avg = average_time(times);
    double p95 = percentile_95(times);

    EXPECT_LT(avg, THRESHOLD_MANIPULATION_DETECT_US)
        << "Average manipulation detection time: " << avg << " us";
    EXPECT_LT(p95, THRESHOLD_MANIPULATION_DETECT_US * 3)
        << "P95 manipulation detection time: " << p95 << " us";

    std::cout << "Manipulation detection: avg=" << avg << "us, p95=" << p95 << "us" << std::endl;
}

// ============================================================================
// Threat Analysis Performance Tests
// ============================================================================

TEST_F(SecurityGameTheoryRegressionTest, ThreatAnalysisLatency) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int iterations = 500;
    std::vector<double> times;

    float attacker[9] = {3.0f, 0.0f, 1.0f, 5.0f, 1.0f, 2.0f, 2.0f, 4.0f, 3.0f};
    float defender[9] = {-3.0f, 0.0f, -1.0f, -5.0f, -1.0f, -2.0f, -2.0f, -4.0f, -3.0f};
    float defense[3];
    float payoff;

    for (int i = 0; i < iterations; i++) {
        double time = measure_time_us([&]() {
            security_gt_analyze_threat_game(bridge, attacker, defender, 3, 3, defense, &payoff);
        });
        times.push_back(time);
    }

    double avg = average_time(times);
    double p95 = percentile_95(times);

    EXPECT_LT(avg, THRESHOLD_THREAT_ANALYSIS_US)
        << "Average threat analysis time: " << avg << " us";
    EXPECT_LT(p95, THRESHOLD_THREAT_ANALYSIS_US * 3)
        << "P95 threat analysis time: " << p95 << " us";

    std::cout << "Threat analysis (3x3): avg=" << avg << "us, p95=" << p95 << "us" << std::endl;
}

// ============================================================================
// Bridge Update Performance Tests
// ============================================================================

TEST_F(SecurityGameTheoryRegressionTest, BridgeUpdateLatency) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int iterations = 1000;
    std::vector<double> times;

    for (int i = 0; i < iterations; i++) {
        double time = measure_time_us([&]() {
            security_gt_bridge_update(bridge, 16);
        });
        times.push_back(time);
    }

    double avg = average_time(times);
    double p95 = percentile_95(times);

    EXPECT_LT(avg, THRESHOLD_BRIDGE_UPDATE_US)
        << "Average bridge update time: " << avg << " us";
    EXPECT_LT(p95, THRESHOLD_BRIDGE_UPDATE_US * 3)
        << "P95 bridge update time: " << p95 << " us";

    std::cout << "Bridge update: avg=" << avg << "us, p95=" << p95 << "us" << std::endl;
}

TEST_F(SecurityGameTheoryRegressionTest, BridgeUpdateThroughput) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int total_operations = 50000;

    auto start = Clock::now();
    for (int i = 0; i < total_operations; i++) {
        security_gt_bridge_update(bridge, 1);
    }
    auto end = Clock::now();

    double duration_s = std::chrono::duration<double>(end - start).count();
    double ops_per_second = total_operations / duration_s;

    EXPECT_GT(ops_per_second, THROUGHPUT_BRIDGE_UPDATE_OPS)
        << "Bridge update throughput: " << ops_per_second << " ops/s";

    std::cout << "Bridge update throughput: " << ops_per_second << " ops/s" << std::endl;
}

// ============================================================================
// Query Performance Tests
// ============================================================================

TEST_F(SecurityGameTheoryRegressionTest, GetStateLatency) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int iterations = 1000;
    std::vector<double> times;

    for (int i = 0; i < iterations; i++) {
        security_game_theory_state_t state;
        double time = measure_time_us([&]() {
            security_gt_bridge_get_state(bridge, &state);
        });
        times.push_back(time);
    }

    double avg = average_time(times);
    double p95 = percentile_95(times);

    EXPECT_LT(avg, THRESHOLD_GET_STATE_US)
        << "Average get state time: " << avg << " us";
    EXPECT_LT(p95, THRESHOLD_GET_STATE_US * 3)
        << "P95 get state time: " << p95 << " us";

    std::cout << "Get state: avg=" << avg << "us, p95=" << p95 << "us" << std::endl;
}

TEST_F(SecurityGameTheoryRegressionTest, GetStatsLatency) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int iterations = 1000;
    std::vector<double> times;

    for (int i = 0; i < iterations; i++) {
        security_game_theory_stats_t stats;
        double time = measure_time_us([&]() {
            security_gt_bridge_get_stats(bridge, &stats);
        });
        times.push_back(time);
    }

    double avg = average_time(times);
    double p95 = percentile_95(times);

    EXPECT_LT(avg, THRESHOLD_GET_STATS_US)
        << "Average get stats time: " << avg << " us";
    EXPECT_LT(p95, THRESHOLD_GET_STATS_US * 3)
        << "P95 get stats time: " << p95 << " us";

    std::cout << "Get stats: avg=" << avg << "us, p95=" << p95 << "us" << std::endl;
}

// ============================================================================
// Memory Performance Tests
// ============================================================================

TEST_F(SecurityGameTheoryRegressionTest, MemoryStability) {
    /* Test that repeated create/destroy doesn't leak memory */
    /* Note: This is a basic test; proper memory leak detection requires
       tools like valgrind or AddressSanitizer */

    const int iterations = 1000;

    for (int i = 0; i < iterations; i++) {
        security_game_theory_config_t config;
        security_gt_default_config(&config);
        security_game_theory_bridge_t* b = security_gt_bridge_create(&config);
        if (b) {
            /* Perform some operations */
            float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
            security_payoff_result_t result;
            security_gt_validate_payoff_matrix(b, payoffs, 2, 2, &result);

            security_gt_bridge_destroy(b);
        }
    }

    /* If we get here without crashing, basic memory stability is OK */
    SUCCEED();
}

TEST_F(SecurityGameTheoryRegressionTest, StatsResetStability) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int iterations = 100;

    for (int i = 0; i < iterations; i++) {
        /* Generate stats */
        float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};
        security_payoff_result_t result;
        security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);

        /* Reset stats */
        security_gt_bridge_reset_stats(bridge);

        /* Verify stats are reset */
        security_game_theory_stats_t stats;
        security_gt_bridge_get_stats(bridge, &stats);
        EXPECT_EQ(stats.total_payoff_validations, 0u);
    }
}

// ============================================================================
// Consistency Tests
// ============================================================================

TEST_F(SecurityGameTheoryRegressionTest, ResultConsistency) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Same input should produce same output */
    float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    security_payoff_result_t result1, result2;
    security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result1);
    security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result2);

    EXPECT_EQ(result1.is_valid, result2.is_valid);
    EXPECT_EQ(result1.status, result2.status);
    EXPECT_EQ(result1.nan_count, result2.nan_count);
    EXPECT_EQ(result1.inf_count, result2.inf_count);
    EXPECT_EQ(result1.min_value, result2.min_value);
    EXPECT_EQ(result1.max_value, result2.max_value);
}

TEST_F(SecurityGameTheoryRegressionTest, StatsAccumulation) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int num_ops = 100;

    for (int i = 0; i < num_ops; i++) {
        float payoffs[4] = {(float)i, (float)i+1, (float)i+2, (float)i+3};
        security_payoff_result_t result;
        security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
    }

    security_game_theory_stats_t stats;
    security_gt_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_payoff_validations, (uint64_t)num_ops)
        << "Stats should accurately count operations";
    EXPECT_EQ(stats.payoff_valid_count, (uint64_t)num_ops)
        << "All validations should succeed for valid input";
}

// ============================================================================
// Decision Latency Tests (End-to-End)
// ============================================================================

TEST_F(SecurityGameTheoryRegressionTest, EndToEndDecisionLatency) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    const int iterations = 100;
    std::vector<double> times;

    /* Complete decision cycle: validate, monitor, detect, analyze, update */
    float payoffs[9] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
    uint32_t players[3] = {0, 1, 2};
    uint32_t actions[8] = {0, 1, 2, 0, 1, 2, 0, 1};
    float attacker[4] = {3.0f, 1.0f, 2.0f, 4.0f};
    float defender[4] = {-3.0f, -1.0f, -2.0f, -4.0f};
    float defense[2];
    float expected_payoff;

    for (int i = 0; i < iterations; i++) {
        double time = measure_time_us([&]() {
            security_payoff_result_t payoff_result;
            security_gt_validate_payoff_matrix(bridge, payoffs, 3, 3, &payoff_result);

            security_coalition_result_t coal_result;
            security_gt_monitor_coalition(bridge, 0x7, players, 3, &coal_result);

            security_manipulation_result_t manip_result;
            security_gt_detect_manipulation(bridge, 0, actions, 8, &manip_result);

            security_gt_analyze_threat_game(bridge, attacker, defender, 2, 2, defense, &expected_payoff);

            security_gt_bridge_update(bridge, 16);
            security_gt_apply_security_effects(bridge);
            security_gt_apply_gt_effects(bridge);
        });
        times.push_back(time);
    }

    double avg = average_time(times);
    double p95 = percentile_95(times);

    /* End-to-end should be sum of individual operations plus some overhead */
    double expected_max = THRESHOLD_PAYOFF_VALIDATE_US +
                          THRESHOLD_COALITION_MONITOR_US +
                          THRESHOLD_MANIPULATION_DETECT_US +
                          THRESHOLD_THREAT_ANALYSIS_US +
                          THRESHOLD_BRIDGE_UPDATE_US * 3 +
                          1000.0;  /* 1ms overhead */

    EXPECT_LT(avg, expected_max)
        << "Average end-to-end decision time: " << avg << " us";

    std::cout << "End-to-end decision: avg=" << avg << "us, p95=" << p95 << "us" << std::endl;
}

// ============================================================================
// Regression-Specific Tests
// ============================================================================

TEST_F(SecurityGameTheoryRegressionTest, NoPerformanceDegradationUnderLoad) {
    CreateBridge();
    if (!bridge) GTEST_SKIP();

    /* Measure baseline */
    std::vector<double> baseline_times;
    float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    for (int i = 0; i < 100; i++) {
        security_payoff_result_t result;
        double time = measure_time_us([&]() {
            security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
        });
        baseline_times.push_back(time);
    }
    double baseline_avg = average_time(baseline_times);

    /* Generate load by performing many operations */
    for (int i = 0; i < 10000; i++) {
        float p[4] = {(float)i, (float)i+1, (float)i+2, (float)i+3};
        security_payoff_result_t r;
        security_gt_validate_payoff_matrix(bridge, p, 2, 2, &r);

        security_gt_bridge_update(bridge, 1);
    }

    /* Measure under load */
    std::vector<double> loaded_times;
    for (int i = 0; i < 100; i++) {
        security_payoff_result_t result;
        double time = measure_time_us([&]() {
            security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
        });
        loaded_times.push_back(time);
    }
    double loaded_avg = average_time(loaded_times);

    /* Performance should not degrade more than 2x under load */
    EXPECT_LT(loaded_avg, baseline_avg * 2)
        << "Performance degradation under load: baseline=" << baseline_avg
        << "us, loaded=" << loaded_avg << "us";

    std::cout << "Baseline: " << baseline_avg << "us, Under load: " << loaded_avg << "us" << std::endl;
}

TEST_F(SecurityGameTheoryRegressionTest, VerifyStatisticsOverhead) {
    /* Compare performance with stats enabled vs disabled */
    security_game_theory_config_t config;
    security_gt_default_config(&config);

    /* Test with default config (stats enabled via operations) */
    bridge = security_gt_bridge_create(&config);
    if (!bridge) GTEST_SKIP();

    std::vector<double> enabled_times;
    float payoffs[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    for (int i = 0; i < 500; i++) {
        security_payoff_result_t result;
        double time = measure_time_us([&]() {
            security_gt_validate_payoff_matrix(bridge, payoffs, 2, 2, &result);
        });
        enabled_times.push_back(time);
    }

    double enabled_avg = average_time(enabled_times);

    /* Statistics overhead should be minimal */
    /* Just verify operations complete in reasonable time */
    EXPECT_LT(enabled_avg, THRESHOLD_PAYOFF_VALIDATE_US * 2)
        << "Stats overhead too high: " << enabled_avg << "us";

    std::cout << "With stats: " << enabled_avg << "us" << std::endl;
}
