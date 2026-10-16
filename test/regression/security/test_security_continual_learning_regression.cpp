/**
 * @file test_security_continual_learning_regression.cpp
 * @brief Regression tests for Security-Continual Learning Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * REGRESSION SCENARIOS:
 * 1. API stability tests (default configs)
 * 2. Performance baseline tests
 * 3. Backward compatibility tests
 * 4. Mathematical correctness tests
 * 5. Thread safety regression tests
 * 6. Memory leak prevention tests
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <thread>
#include <chrono>
#include <numeric>

extern "C" {
#include "security/continual/nimcp_security_continual_learning_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

namespace {

/* =============================================================================
 * Test Fixture
 * ============================================================================= */

class SecurityCLRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        EXPECT_EQ(0, security_cl_default_config(&config));
        bridge = security_cl_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }

    void TearDown() override {
        if (bridge) {
            security_cl_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    security_cl_config_t config;
    security_cl_bridge_t* bridge;
};

/* =============================================================================
 * API Stability Tests - Default Configuration Values
 * ============================================================================= */

TEST(SecurityCLAPIStabilityTest, DefaultConfigRetentionThresholdsStable) {
    security_cl_config_t config;
    EXPECT_EQ(0, security_cl_default_config(&config));

    /* These values should remain stable across versions */
    EXPECT_FLOAT_EQ(0.8f, config.retention_threshold);
    EXPECT_FLOAT_EQ(0.6f, config.critical_threshold);
    EXPECT_FLOAT_EQ(0.1f, config.max_forgetting_rate);
}

TEST(SecurityCLAPIStabilityTest, DefaultConfigDriftThresholdsStable) {
    security_cl_config_t config;
    EXPECT_EQ(0, security_cl_default_config(&config));

    /* Drift detection thresholds */
    EXPECT_FLOAT_EQ(0.3f, config.drift_threshold);
    EXPECT_FLOAT_EQ(0.5f, config.sudden_drift_threshold);
    EXPECT_EQ(100u, config.drift_window_size);
}

TEST(SecurityCLAPIStabilityTest, DefaultConfigLRBoundsStable) {
    security_cl_config_t config;
    EXPECT_EQ(0, security_cl_default_config(&config));

    /* Learning rate bounds */
    EXPECT_FLOAT_EQ(1e-6f, config.lr_min_bound);
    EXPECT_FLOAT_EQ(0.1f, config.lr_max_bound);
    EXPECT_FLOAT_EQ(0.5f, config.lr_change_threshold);
}

TEST(SecurityCLAPIStabilityTest, DefaultConfigFeaturesEnabledStable) {
    security_cl_config_t config;
    EXPECT_EQ(0, security_cl_default_config(&config));

    /* All security features should be enabled by default */
    EXPECT_TRUE(config.enable_forgetting_protection);
    EXPECT_TRUE(config.enable_drift_detection);
    EXPECT_TRUE(config.enable_replay_verification);
    EXPECT_TRUE(config.enable_lr_monitoring);
    EXPECT_TRUE(config.enable_task_validation);
    EXPECT_TRUE(config.enable_ewc_boost);
    EXPECT_TRUE(config.use_hash_chains);
}

TEST(SecurityCLAPIStabilityTest, ConstantsStable) {
    /* Hash and buffer sizes */
    EXPECT_EQ(32u, SECURITY_CL_HASH_SIZE);
    EXPECT_EQ(16u, SECURITY_CL_MAX_REPLAY_BUFFERS);
    EXPECT_EQ(256u, SECURITY_CL_MAX_TASKS);
    EXPECT_EQ(64u, SECURITY_CL_TASK_FINGERPRINT_SIZE);
    EXPECT_EQ(64u, SECURITY_CL_BIO_INBOX_CAPACITY);
    EXPECT_EQ(100u, SECURITY_CL_LR_HISTORY_SIZE);
}

/* =============================================================================
 * String Conversion Stability Tests
 * ============================================================================= */

TEST(SecurityCLAPIStabilityTest, ForgettingTypeStringsStable) {
    EXPECT_STREQ("none", security_cl_forgetting_type_to_string(SECURITY_CL_FORGETTING_NONE));
    EXPECT_STREQ("gradient_flood", security_cl_forgetting_type_to_string(SECURITY_CL_FORGETTING_GRADIENT_FLOOD));
    EXPECT_STREQ("weight_erasure", security_cl_forgetting_type_to_string(SECURITY_CL_FORGETTING_WEIGHT_ERASURE));
    EXPECT_STREQ("task_overwrite", security_cl_forgetting_type_to_string(SECURITY_CL_FORGETTING_TASK_OVERWRITE));
    EXPECT_STREQ("replay_poison", security_cl_forgetting_type_to_string(SECURITY_CL_FORGETTING_REPLAY_POISON));
    EXPECT_STREQ("lr_spike", security_cl_forgetting_type_to_string(SECURITY_CL_FORGETTING_LR_SPIKE));
}

TEST(SecurityCLAPIStabilityTest, DriftTypeStringsStable) {
    EXPECT_STREQ("none", security_cl_drift_type_to_string(SECURITY_CL_DRIFT_NONE));
    EXPECT_STREQ("natural", security_cl_drift_type_to_string(SECURITY_CL_DRIFT_NATURAL));
    EXPECT_STREQ("task_switch", security_cl_drift_type_to_string(SECURITY_CL_DRIFT_TASK_SWITCH));
    EXPECT_STREQ("adversarial", security_cl_drift_type_to_string(SECURITY_CL_DRIFT_ADVERSARIAL));
    EXPECT_STREQ("manipulation", security_cl_drift_type_to_string(SECURITY_CL_DRIFT_MANIPULATION));
}

TEST(SecurityCLAPIStabilityTest, RetentionLevelStringsStable) {
    EXPECT_STREQ("healthy", security_cl_retention_level_to_string(SECURITY_CL_RETENTION_HEALTHY));
    EXPECT_STREQ("degrading", security_cl_retention_level_to_string(SECURITY_CL_RETENTION_DEGRADING));
    EXPECT_STREQ("critical", security_cl_retention_level_to_string(SECURITY_CL_RETENTION_CRITICAL));
    EXPECT_STREQ("compromised", security_cl_retention_level_to_string(SECURITY_CL_RETENTION_COMPROMISED));
}

TEST(SecurityCLAPIStabilityTest, PhaseStringsStable) {
    EXPECT_STREQ("inactive", security_cl_phase_to_string(SECURITY_CL_PHASE_INACTIVE));
    EXPECT_STREQ("monitoring", security_cl_phase_to_string(SECURITY_CL_PHASE_MONITORING));
    EXPECT_STREQ("protecting", security_cl_phase_to_string(SECURITY_CL_PHASE_PROTECTING));
    EXPECT_STREQ("defending", security_cl_phase_to_string(SECURITY_CL_PHASE_DEFENDING));
    EXPECT_STREQ("recovery", security_cl_phase_to_string(SECURITY_CL_PHASE_RECOVERY));
}

TEST(SecurityCLAPIStabilityTest, ReplayStatusStringsStable) {
    EXPECT_STREQ("ok", security_cl_replay_status_to_string(SECURITY_CL_REPLAY_OK));
    EXPECT_STREQ("hash_mismatch", security_cl_replay_status_to_string(SECURITY_CL_REPLAY_HASH_MISMATCH));
    EXPECT_STREQ("poison_detected", security_cl_replay_status_to_string(SECURITY_CL_REPLAY_POISON_DETECTED));
    EXPECT_STREQ("distribution_shift", security_cl_replay_status_to_string(SECURITY_CL_REPLAY_DISTRIBUTION_SHIFT));
    EXPECT_STREQ("tampered", security_cl_replay_status_to_string(SECURITY_CL_REPLAY_TAMPERED));
}

/* =============================================================================
 * Initial State Tests
 * ============================================================================= */

TEST_F(SecurityCLRegressionTest, InitialStateConsistent) {
    /* Phase should be MONITORING on creation */
    EXPECT_EQ(SECURITY_CL_PHASE_MONITORING, security_cl_get_phase(bridge));

    /* Threat level should be 0 */
    EXPECT_FLOAT_EQ(0.0f, security_cl_get_threat_level(bridge));

    /* Should not be under attack */
    EXPECT_FALSE(security_cl_is_under_attack(bridge));

    /* Retention should not be compromised */
    EXPECT_FALSE(security_cl_is_retention_compromised(bridge));

    /* Initial stats should be zero */
    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_EQ(0u, stats.knowledge_protections);
    EXPECT_EQ(0u, stats.drift_checks);
    EXPECT_EQ(0u, stats.replay_verifications);
    EXPECT_EQ(0u, stats.lr_checks);
    EXPECT_EQ(0u, stats.retention_checks);
    EXPECT_EQ(0u, stats.tasks_registered);
    EXPECT_EQ(0u, stats.forgetting_attacks_detected);
}

TEST_F(SecurityCLRegressionTest, InitialEffectsConsistent) {
    security_cl_effects_t sec_effects;
    EXPECT_EQ(0, security_cl_get_security_effects(bridge, &sec_effects));

    /* Initial security effects */
    EXPECT_TRUE(sec_effects.valid);
    EXPECT_FLOAT_EQ(1.0f, sec_effects.ewc_lambda_boost);
    EXPECT_FLOAT_EQ(1.0f, sec_effects.si_importance_boost);
    EXPECT_FLOAT_EQ(1.0f, sec_effects.lr_scale_factor);
    EXPECT_FLOAT_EQ(0.0f, sec_effects.threat_level);
    EXPECT_FALSE(sec_effects.under_attack);
    EXPECT_FALSE(sec_effects.knowledge_lock_active);
    EXPECT_FALSE(sec_effects.replay_suspended);
    EXPECT_FALSE(sec_effects.task_switch_blocked);
    EXPECT_FALSE(sec_effects.new_task_blocked);
    EXPECT_EQ(SECURITY_CL_RETENTION_HEALTHY, sec_effects.retention_status);
}

/* =============================================================================
 * Performance Baseline Tests
 * ============================================================================= */

TEST_F(SecurityCLRegressionTest, KnowledgeProtectionPerformance) {
    const int iterations = 100;
    std::vector<float> weights(1000, 0.1f);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        EXPECT_EQ(0, security_cl_protect_knowledge(bridge, weights.data(), weights.size(), i));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float avg_us = (float)duration.count() / iterations;

    /* Protection should complete in reasonable time (< 1ms per call) */
    EXPECT_LT(avg_us, 1000.0f);

    /* Log for baseline tracking */
    std::cout << "[PERF] Knowledge protection: " << avg_us << " us/call" << std::endl;
}

TEST_F(SecurityCLRegressionTest, DriftValidationPerformance) {
    const int iterations = 1000;
    std::vector<float> baseline(100, 0.5f);
    EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, baseline.data(), baseline.size()));

    std::vector<float> current(100, 0.55f);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        security_cl_drift_type_t drift_type;
        float drift_score;
        security_cl_validate_drift(bridge, current.data(), current.size(), &drift_type, &drift_score);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float avg_us = (float)duration.count() / iterations;

    /* Drift validation should be fast (< 100us per call) */
    EXPECT_LT(avg_us, 100.0f);

    std::cout << "[PERF] Drift validation: " << avg_us << " us/call" << std::endl;
}

TEST_F(SecurityCLRegressionTest, RetentionMonitoringPerformance) {
    /* Register task first */
    EXPECT_EQ(0, security_cl_register_task(bridge, 0, "perf_task", 0.95f));

    const int iterations = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        float retention;
        security_cl_monitor_retention(bridge, 0, 0.90f, &retention);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float avg_us = (float)duration.count() / iterations;

    /* Retention monitoring should be fast (< 50us per call) */
    EXPECT_LT(avg_us, 50.0f);

    std::cout << "[PERF] Retention monitoring: " << avg_us << " us/call" << std::endl;
}

TEST_F(SecurityCLRegressionTest, LRDetectionPerformance) {
    const int iterations = 1000;

    /* Build up some history first */
    for (int i = 0; i < 50; i++) {
        security_cl_record_lr(bridge, 0.001f);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        security_cl_detect_lr_manipulation(bridge, 0.001f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float avg_us = (float)duration.count() / iterations;

    /* LR detection should be very fast (< 10us per call) */
    EXPECT_LT(avg_us, 10.0f);

    std::cout << "[PERF] LR detection: " << avg_us << " us/call" << std::endl;
}

TEST_F(SecurityCLRegressionTest, ReplayVerificationPerformance) {
    std::vector<float> buffer(10000, 0.5f);

    uint32_t buffer_id = security_cl_register_replay_buffer(
        bridge, "perf_buffer", buffer.data(), buffer.size() * sizeof(float), 100);
    EXPECT_GT(buffer_id, 0u);

    const int iterations = 100;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        security_cl_replay_status_t status;
        security_cl_verify_replay(bridge, buffer.data(), buffer.size() * sizeof(float), 100, &status);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float avg_us = (float)duration.count() / iterations;

    /* Replay verification should complete in reasonable time (< 5ms for 40KB buffer) */
    EXPECT_LT(avg_us, 5000.0f);

    std::cout << "[PERF] Replay verification (40KB): " << avg_us << " us/call" << std::endl;
}

/* =============================================================================
 * Mathematical Correctness Tests
 * ============================================================================= */

TEST_F(SecurityCLRegressionTest, RetentionRateCalculation) {
    EXPECT_EQ(0, security_cl_register_task(bridge, 0, "math_task", 0.95f));

    /* Test various retention scenarios */
    struct {
        float current_accuracy;
        float expected_retention;
        security_cl_retention_level_t expected_level;
    } test_cases[] = {
        {0.95f, 1.0f, SECURITY_CL_RETENTION_HEALTHY},           /* Perfect retention */
        {0.855f, 0.9f, SECURITY_CL_RETENTION_HEALTHY},          /* 90% retention (0.855/0.95) */
        {0.76f, 0.8f, SECURITY_CL_RETENTION_HEALTHY},           /* 80% threshold boundary */
        {0.665f, 0.7f, SECURITY_CL_RETENTION_DEGRADING},        /* Degrading */
        {0.57f, 0.6f, SECURITY_CL_RETENTION_DEGRADING},         /* At critical threshold boundary (>= 0.6) */
        {0.475f, 0.5f, SECURITY_CL_RETENTION_CRITICAL},         /* Below critical */
    };

    for (const auto& tc : test_cases) {
        float retention;
        security_cl_retention_level_t level = security_cl_monitor_retention(
            bridge, 0, tc.current_accuracy, &retention);

        EXPECT_NEAR(tc.expected_retention, retention, 0.01f)
            << "For current_accuracy=" << tc.current_accuracy;
        EXPECT_EQ(tc.expected_level, level)
            << "For current_accuracy=" << tc.current_accuracy;
    }
}

TEST_F(SecurityCLRegressionTest, ProtectionPenaltyCalculation) {
    /* Set up known baseline */
    std::vector<float> baseline = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    EXPECT_EQ(0, security_cl_protect_knowledge(bridge, baseline.data(), baseline.size(), 0));

    /* Test penalty increases with distance */
    float prev_penalty = 0.0f;
    for (float offset = 0.1f; offset <= 1.0f; offset += 0.1f) {
        std::vector<float> modified(baseline);
        for (auto& w : modified) {
            w += offset;
        }

        float penalty;
        EXPECT_EQ(0, security_cl_compute_protection_penalty(
            bridge, modified.data(), modified.size(), &penalty));

        /* Penalty should increase monotonically */
        EXPECT_GE(penalty, prev_penalty);
        prev_penalty = penalty;
    }
}

TEST_F(SecurityCLRegressionTest, DriftScoreCalculation) {
    /* Establish baseline */
    std::vector<float> baseline = {0.5f, 0.5f, 0.5f, 0.5f};
    EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, baseline.data(), baseline.size()));

    /* Test drift score increases with distance */
    float prev_score = 0.0f;
    for (float offset = 0.0f; offset <= 0.4f; offset += 0.1f) {
        std::vector<float> current(baseline);
        for (auto& f : current) {
            f += offset;
        }

        security_cl_drift_type_t drift_type;
        float drift_score;
        security_cl_validate_drift(bridge, current.data(), current.size(), &drift_type, &drift_score);

        /* Drift score should increase with offset */
        EXPECT_GE(drift_score, prev_score - 0.01f);  /* Allow small tolerance */
        prev_score = drift_score;
    }
}

TEST_F(SecurityCLRegressionTest, LRBoundsEnforcement) {
    /* Set specific bounds */
    EXPECT_EQ(0, security_cl_set_lr_bounds(bridge, 0.0001f, 0.01f));

    security_cl_effects_t effects;
    EXPECT_EQ(0, security_cl_get_security_effects(bridge, &effects));

    EXPECT_FLOAT_EQ(0.0001f, effects.lr_min_allowed);
    EXPECT_FLOAT_EQ(0.01f, effects.lr_max_allowed);

    /* Test out of bounds detection */
    EXPECT_TRUE(security_cl_detect_lr_manipulation(bridge, 0.00001f));  /* Below min */
    EXPECT_TRUE(security_cl_detect_lr_manipulation(bridge, 0.1f));       /* Above max */
    EXPECT_FALSE(security_cl_detect_lr_manipulation(bridge, 0.001f));    /* Within bounds */
}

/* =============================================================================
 * Thread Safety Regression Tests
 * ============================================================================= */

TEST_F(SecurityCLRegressionTest, ConcurrentBridgeAccess) {
    const int num_threads = 8;
    const int iterations = 50;
    std::atomic<int> errors{0};

    auto worker = [this, &errors, iterations](int thread_id) {
        for (int i = 0; i < iterations; i++) {
            /* Mix of read and write operations */
            switch (i % 5) {
                case 0: {
                    std::vector<float> w(100, 0.1f * thread_id);
                    if (security_cl_protect_knowledge(bridge, w.data(), w.size(), thread_id * 1000 + i) != 0) {
                        errors++;
                    }
                    break;
                }
                case 1: {
                    float retention;
                    security_cl_monitor_retention(bridge, thread_id % 10, 0.9f, &retention);
                    break;
                }
                case 2: {
                    security_cl_detect_lr_manipulation(bridge, 0.001f);
                    break;
                }
                case 3: {
                    security_cl_stats_t stats;
                    if (security_cl_get_stats(bridge, &stats) != 0) {
                        errors++;
                    }
                    break;
                }
                case 4: {
                    security_cl_update_security_effects(bridge);
                    break;
                }
            }
        }
    };

    /* Register some tasks first */
    for (int t = 0; t < 10; t++) {
        char name[32];
        snprintf(name, sizeof(name), "task_%d", t);
        security_cl_register_task(bridge, t, name, 0.9f);
    }

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back(worker, t);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(0, errors.load());
}

TEST_F(SecurityCLRegressionTest, ConcurrentStatsAccess) {
    const int num_threads = 4;
    const int iterations = 200;
    std::atomic<int> success_count{0};

    auto stats_reader = [this, &success_count, iterations]() {
        for (int i = 0; i < iterations; i++) {
            security_cl_stats_t stats;
            if (security_cl_get_stats(bridge, &stats) == 0) {
                success_count++;
            }
        }
    };

    auto effects_reader = [this, &success_count, iterations]() {
        for (int i = 0; i < iterations; i++) {
            security_cl_effects_t effects;
            if (security_cl_get_security_effects(bridge, &effects) == 0) {
                success_count++;
            }
        }
    };

    auto updater = [this, &success_count, iterations]() {
        for (int i = 0; i < iterations; i++) {
            if (security_cl_update_security_effects(bridge) == 0) {
                success_count++;
            }
        }
    };

    std::thread t1(stats_reader);
    std::thread t2(stats_reader);
    std::thread t3(effects_reader);
    std::thread t4(updater);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    /* All operations should succeed */
    EXPECT_EQ(num_threads * iterations, success_count.load());
}

/* =============================================================================
 * Memory Management Tests
 * ============================================================================= */

TEST(SecurityCLMemoryTest, CreateDestroyNoLeak) {
    const int iterations = 100;

    for (int i = 0; i < iterations; i++) {
        security_cl_config_t config;
        EXPECT_EQ(0, security_cl_default_config(&config));

        security_cl_bridge_t* bridge = security_cl_bridge_create(&config);
        EXPECT_NE(nullptr, bridge);

        /* Perform some operations */
        std::vector<float> weights(100, 0.1f);
        security_cl_protect_knowledge(bridge, weights.data(), weights.size(), 0);

        std::vector<float> features(50, 0.5f);
        security_cl_update_drift_baseline(bridge, features.data(), features.size());

        security_cl_bridge_destroy(bridge);
    }

    /* If we get here without crashing or memory errors, test passes */
}

TEST(SecurityCLMemoryTest, WeightReallocationNoLeak) {
    security_cl_config_t config;
    EXPECT_EQ(0, security_cl_default_config(&config));

    security_cl_bridge_t* bridge = security_cl_bridge_create(&config);
    EXPECT_NE(nullptr, bridge);

    /* Repeatedly protect with different sizes */
    for (int size = 100; size <= 1000; size += 100) {
        std::vector<float> weights(size, 0.1f);
        EXPECT_EQ(0, security_cl_protect_knowledge(bridge, weights.data(), weights.size(), 0));
    }

    security_cl_bridge_destroy(bridge);
}

TEST(SecurityCLMemoryTest, DriftBaselineReallocationNoLeak) {
    security_cl_config_t config;
    EXPECT_EQ(0, security_cl_default_config(&config));

    security_cl_bridge_t* bridge = security_cl_bridge_create(&config);
    EXPECT_NE(nullptr, bridge);

    /* Repeatedly update baseline */
    for (int i = 0; i < 100; i++) {
        std::vector<float> features(config.drift_window_size, 0.5f + i * 0.001f);
        EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, features.data(), features.size()));
    }

    /* Reset baseline */
    EXPECT_EQ(0, security_cl_reset_drift_baseline(bridge));

    /* Update again */
    std::vector<float> features(config.drift_window_size, 0.5f);
    EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, features.data(), features.size()));

    security_cl_bridge_destroy(bridge);
}

/* =============================================================================
 * Backward Compatibility Tests
 * ============================================================================= */

TEST_F(SecurityCLRegressionTest, NullConfigCreatesValidBridge) {
    security_cl_bridge_destroy(bridge);
    bridge = nullptr;

    /* NULL config should use defaults */
    bridge = security_cl_bridge_create(nullptr);
    EXPECT_NE(nullptr, bridge);

    /* Should have valid initial state */
    EXPECT_EQ(SECURITY_CL_PHASE_MONITORING, security_cl_get_phase(bridge));
}

TEST_F(SecurityCLRegressionTest, EmptyOperationsSucceed) {
    /* Operations with zero counts should succeed */
    EXPECT_EQ(0, security_cl_quarantine_replay_samples(bridge, nullptr, 0));
    EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, nullptr, 0));  /* NULL with 0 */
}

TEST_F(SecurityCLRegressionTest, GetTaskInfoForUnregisteredReturnsNotFound) {
    security_cl_task_info_t info;
    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND, security_cl_get_task_info(bridge, 999, &info));
}

TEST_F(SecurityCLRegressionTest, UpdateReplayHashForUnregisteredReturnsNotFound) {
    std::vector<float> buffer(100, 0.5f);
    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND, security_cl_update_replay_hash(
        bridge, 999, buffer.data(), buffer.size() * sizeof(float), 10));
}

/* =============================================================================
 * Enum Range Tests
 * ============================================================================= */

TEST(SecurityCLEnumRangeTest, ForgettingTypeEnumComplete) {
    EXPECT_EQ(0, SECURITY_CL_FORGETTING_NONE);
    EXPECT_EQ(6, SECURITY_CL_FORGETTING_COUNT);

    /* All types should have string representations */
    for (int i = 0; i < SECURITY_CL_FORGETTING_COUNT; i++) {
        const char* str = security_cl_forgetting_type_to_string((security_cl_forgetting_type_t)i);
        EXPECT_NE(nullptr, str);
        EXPECT_STRNE("unknown", str);
    }
}

TEST(SecurityCLEnumRangeTest, InvalidEnumsReturnUnknown) {
    EXPECT_STREQ("unknown", security_cl_forgetting_type_to_string((security_cl_forgetting_type_t)999));
    EXPECT_STREQ("unknown", security_cl_drift_type_to_string((security_cl_drift_type_t)999));
    EXPECT_STREQ("unknown", security_cl_retention_level_to_string((security_cl_retention_level_t)999));
    EXPECT_STREQ("unknown", security_cl_phase_to_string((security_cl_phase_t)999));
    EXPECT_STREQ("unknown", security_cl_replay_status_to_string((security_cl_replay_status_t)999));
}

/* =============================================================================
 * Edge Case Tests
 * ============================================================================= */

TEST_F(SecurityCLRegressionTest, MaxTasksRegistration) {
    /* Register up to max tasks */
    for (uint32_t t = 0; t < SECURITY_CL_MAX_TASKS; t++) {
        char name[32];
        snprintf(name, sizeof(name), "task_%u", t);
        int result = security_cl_register_task(bridge, t, name, 0.9f);

        if (t < SECURITY_CL_MAX_TASKS) {
            EXPECT_EQ(0, result) << "Failed at task " << t;
        }
    }

    /* One more should fail */
    EXPECT_EQ(NIMCP_ERROR_OUT_OF_RANGE, security_cl_register_task(
        bridge, SECURITY_CL_MAX_TASKS, "overflow", 0.9f));
}

TEST_F(SecurityCLRegressionTest, MaxReplayBuffersRegistration) {
    std::vector<float> buffer(100, 0.5f);

    /* Register up to max buffers */
    for (uint32_t b = 0; b < SECURITY_CL_MAX_REPLAY_BUFFERS; b++) {
        char name[32];
        snprintf(name, sizeof(name), "buffer_%u", b);
        uint32_t id = security_cl_register_replay_buffer(
            bridge, name, buffer.data(), buffer.size() * sizeof(float), 10);

        EXPECT_GT(id, 0u) << "Failed at buffer " << b;
    }

    /* One more should fail */
    uint32_t id = security_cl_register_replay_buffer(
        bridge, "overflow", buffer.data(), buffer.size() * sizeof(float), 10);
    EXPECT_EQ(0u, id);
}

TEST_F(SecurityCLRegressionTest, ZeroSizeOperationsHandled) {
    /* Zero size weight array */
    std::vector<float> weights = {0.1f};
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, security_cl_protect_knowledge(
        bridge, weights.data(), 0, 0));

    /* Zero size replay buffer */
    EXPECT_EQ(0u, security_cl_register_replay_buffer(
        bridge, "empty", weights.data(), 0, 0));
}

TEST_F(SecurityCLRegressionTest, ExtremeLRValues) {
    /* Very small LR - should be below min bound */
    EXPECT_TRUE(security_cl_detect_lr_manipulation(bridge, 1e-15f));

    /* Very large LR - should be above max bound */
    EXPECT_TRUE(security_cl_detect_lr_manipulation(bridge, 1e10f));

    /* Zero LR - should be below min bound */
    EXPECT_TRUE(security_cl_detect_lr_manipulation(bridge, 0.0f));
}

TEST_F(SecurityCLRegressionTest, ExtremeRetentionValues) {
    EXPECT_EQ(0, security_cl_register_task(bridge, 0, "task", 0.95f));

    float retention;

    /* Zero accuracy */
    security_cl_retention_level_t level = security_cl_monitor_retention(bridge, 0, 0.0f, &retention);
    EXPECT_FLOAT_EQ(0.0f, retention);
    EXPECT_EQ(SECURITY_CL_RETENTION_CRITICAL, level);

    /* Above baseline accuracy (impossible in practice) */
    level = security_cl_monitor_retention(bridge, 0, 1.0f, &retention);
    EXPECT_LE(retention, 1.0f);  /* Should be clamped */
}

}  /* namespace */

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
