/**
 * @file test_security_distributed_training_regression.cpp
 * @brief Regression tests for security-distributed training bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * Regression tests for security-distributed training:
 * 1. Performance regression tests
 * 2. Consistency tests
 * 3. Memory stability tests
 * 4. Edge case regression tests
 * 5. API contract verification
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <chrono>
#include <random>

extern "C" {
#include "security/distributed/nimcp_security_distributed_training_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

/* =============================================================================
 * Performance Constants
 * ============================================================================= */

/* Maximum allowed times in microseconds */
constexpr uint64_t MAX_CREATE_TIME_US = 5000;          /* 5ms */
constexpr uint64_t MAX_WORKER_REGISTER_TIME_US = 100;  /* 100us */
constexpr uint64_t MAX_GRADIENT_VALIDATE_TIME_US = 500; /* 500us */
constexpr uint64_t MAX_BYZANTINE_DETECT_TIME_US = 2000; /* 2ms */
constexpr uint64_t MAX_CHECKPOINT_TIME_US = 5000;      /* 5ms */

/* =============================================================================
 * Test Fixtures
 * ============================================================================= */

class SecurityDistributedRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        EXPECT_EQ(0, security_distributed_training_default_config(&config));
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            security_distributed_training_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    void SetUpWorkers(int count) {
        for (int i = 0; i < count; i++) {
            std::string worker_id = "worker_" + std::to_string(i);
            EXPECT_EQ(0, security_distributed_training_register_worker(
                bridge, worker_id.c_str(), nullptr));
            EXPECT_EQ(0, security_distributed_training_set_worker_trust(
                bridge, worker_id.c_str(), SECURITY_WORKER_TRUST_VERIFIED));
        }
    }

    std::vector<float> GenerateGradients(size_t size, float scale = 0.1f) {
        std::vector<float> gradients(size);
        std::mt19937 gen(42);
        std::normal_distribution<float> dist(0.0f, scale);
        for (size_t i = 0; i < size; i++) {
            gradients[i] = dist(gen);
        }
        return gradients;
    }

    uint64_t MeasureTimeUs(std::function<void()> func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }

    security_distributed_training_config_t config;
    security_distributed_training_bridge_t* bridge;
};

/* =============================================================================
 * Performance Regression Tests
 * ============================================================================= */

TEST_F(SecurityDistributedRegressionTest, CreatePerformance) {
    uint64_t total_time = 0;
    const int iterations = 10;

    for (int i = 0; i < iterations; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        bridge = security_distributed_training_bridge_create(&config);
        auto end = std::chrono::high_resolution_clock::now();

        ASSERT_NE(nullptr, bridge);

        uint64_t time_us = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start).count();
        total_time += time_us;

        security_distributed_training_bridge_destroy(bridge);
        bridge = nullptr;
    }

    uint64_t avg_time = total_time / iterations;
    EXPECT_LT(avg_time, MAX_CREATE_TIME_US)
        << "Bridge creation took " << avg_time << "us, expected < " << MAX_CREATE_TIME_US << "us";
}

TEST_F(SecurityDistributedRegressionTest, WorkerRegistrationPerformance) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const int num_workers = 100;
    uint64_t total_time = 0;

    for (int i = 0; i < num_workers; i++) {
        std::string worker_id = "worker_" + std::to_string(i);

        auto start = std::chrono::high_resolution_clock::now();
        int result = security_distributed_training_register_worker(
            bridge, worker_id.c_str(), nullptr);
        auto end = std::chrono::high_resolution_clock::now();

        EXPECT_EQ(0, result);

        total_time += std::chrono::duration_cast<std::chrono::microseconds>(
            end - start).count();
    }

    uint64_t avg_time = total_time / num_workers;
    EXPECT_LT(avg_time, MAX_WORKER_REGISTER_TIME_US)
        << "Worker registration took " << avg_time << "us, expected < "
        << MAX_WORKER_REGISTER_TIME_US << "us";
}

TEST_F(SecurityDistributedRegressionTest, GradientValidationPerformance) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    SetUpWorkers(10);

    const size_t gradient_size = 1000;
    const int iterations = 100;
    uint64_t total_time = 0;

    for (int i = 0; i < iterations; i++) {
        std::string worker_id = "worker_" + std::to_string(i % 10);
        auto gradients = GenerateGradients(gradient_size);

        security_gradient_validation_result_t result;

        auto start = std::chrono::high_resolution_clock::now();
        int ret = security_distributed_training_validate_gradients(
            bridge, worker_id.c_str(), gradients.data(), gradients.size(), &result);
        auto end = std::chrono::high_resolution_clock::now();

        EXPECT_EQ(0, ret);

        total_time += std::chrono::duration_cast<std::chrono::microseconds>(
            end - start).count();
    }

    uint64_t avg_time = total_time / iterations;
    EXPECT_LT(avg_time, MAX_GRADIENT_VALIDATE_TIME_US)
        << "Gradient validation took " << avg_time << "us, expected < "
        << MAX_GRADIENT_VALIDATE_TIME_US << "us";
}

TEST_F(SecurityDistributedRegressionTest, ByzantineDetectionPerformance) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    SetUpWorkers(50);

    const int iterations = 20;
    uint64_t total_time = 0;

    for (int i = 0; i < iterations; i++) {
        security_byzantine_result_t result;

        auto start = std::chrono::high_resolution_clock::now();
        int ret = security_distributed_training_detect_byzantine(bridge, &result);
        auto end = std::chrono::high_resolution_clock::now();

        EXPECT_EQ(0, ret);

        total_time += std::chrono::duration_cast<std::chrono::microseconds>(
            end - start).count();
    }

    uint64_t avg_time = total_time / iterations;
    EXPECT_LT(avg_time, MAX_BYZANTINE_DETECT_TIME_US)
        << "Byzantine detection took " << avg_time << "us, expected < "
        << MAX_BYZANTINE_DETECT_TIME_US << "us";
}

TEST_F(SecurityDistributedRegressionTest, CheckpointPerformance) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const size_t model_size = 10000;
    std::vector<float> model(model_size, 0.5f);

    const int iterations = 10;
    uint64_t total_time = 0;

    for (int i = 0; i < iterations; i++) {
        std::string cp_name = "checkpoint_" + std::to_string(i);

        auto start = std::chrono::high_resolution_clock::now();
        int ret = security_distributed_training_secure_checkpoint(
            bridge, cp_name.c_str(), model.data(), model.size(), i);
        auto end = std::chrono::high_resolution_clock::now();

        EXPECT_EQ(0, ret);

        total_time += std::chrono::duration_cast<std::chrono::microseconds>(
            end - start).count();
    }

    uint64_t avg_time = total_time / iterations;
    EXPECT_LT(avg_time, MAX_CHECKPOINT_TIME_US)
        << "Checkpointing took " << avg_time << "us, expected < "
        << MAX_CHECKPOINT_TIME_US << "us";
}

/* =============================================================================
 * Consistency Tests
 * ============================================================================= */

TEST_F(SecurityDistributedRegressionTest, StatsConsistency) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const int num_workers = 10;
    SetUpWorkers(num_workers);

    /* Perform various operations */
    const size_t gradient_size = 100;
    for (int w = 0; w < num_workers; w++) {
        std::string worker_id = "worker_" + std::to_string(w);
        auto gradients = GenerateGradients(gradient_size);

        security_gradient_validation_result_t result;
        security_distributed_training_validate_gradients(
            bridge, worker_id.c_str(), gradients.data(), gradients.size(), &result);
    }

    /* Get stats multiple times - should be consistent */
    security_distributed_training_stats_t stats1, stats2;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats1));
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats2));

    EXPECT_EQ(stats1.total_workers_registered, stats2.total_workers_registered);
    EXPECT_EQ(stats1.gradients_validated, stats2.gradients_validated);
    EXPECT_EQ(stats1.current_active_workers, stats2.current_active_workers);
}

TEST_F(SecurityDistributedRegressionTest, TrustLevelConsistency) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));

    /* Set and get trust multiple times */
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(0, security_distributed_training_set_worker_trust(
            bridge, "worker_0", SECURITY_WORKER_TRUST_VERIFIED));
        EXPECT_EQ(SECURITY_WORKER_TRUST_VERIFIED,
                  security_distributed_training_get_worker_trust(bridge, "worker_0"));

        EXPECT_EQ(0, security_distributed_training_set_worker_trust(
            bridge, "worker_0", SECURITY_WORKER_TRUST_TRUSTED));
        EXPECT_EQ(SECURITY_WORKER_TRUST_TRUSTED,
                  security_distributed_training_get_worker_trust(bridge, "worker_0"));
    }
}

TEST_F(SecurityDistributedRegressionTest, CheckpointHashConsistency) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const size_t model_size = 100;
    std::vector<float> model(model_size);
    for (size_t i = 0; i < model_size; i++) {
        model[i] = (float)i / model_size;
    }

    /* Create checkpoint */
    EXPECT_EQ(0, security_distributed_training_secure_checkpoint(
        bridge, "consistent_cp", model.data(), model.size(), 0));

    /* Verify multiple times - should always succeed */
    for (int i = 0; i < 10; i++) {
        security_checkpoint_result_t result =
            security_distributed_training_verify_checkpoint(
                bridge, "consistent_cp", model.data(), model.size());
        EXPECT_EQ(SECURITY_CHECKPOINT_OK, result);
    }
}

TEST_F(SecurityDistributedRegressionTest, EffectsConsistency) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    /* Update effects */
    EXPECT_EQ(0, security_distributed_training_update_security_effects(bridge));

    /* Get effects multiple times - should be consistent */
    security_distributed_effects_t effects1, effects2;
    EXPECT_EQ(0, security_distributed_training_get_security_effects(bridge, &effects1));
    EXPECT_EQ(0, security_distributed_training_get_security_effects(bridge, &effects2));

    EXPECT_EQ(effects1.min_trust_level, effects2.min_trust_level);
    EXPECT_FLOAT_EQ(effects1.threat_level, effects2.threat_level);
    EXPECT_EQ(effects1.under_attack, effects2.under_attack);
}

/* =============================================================================
 * Memory Stability Tests
 * ============================================================================= */

TEST_F(SecurityDistributedRegressionTest, CreateDestroyStability) {
    /* Create and destroy many times */
    for (int i = 0; i < 100; i++) {
        bridge = security_distributed_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
        security_distributed_training_bridge_destroy(bridge);
        bridge = nullptr;
    }
}

TEST_F(SecurityDistributedRegressionTest, WorkerRegistrationStability) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    /* Register and unregister workers repeatedly */
    for (int round = 0; round < 10; round++) {
        for (int i = 0; i < 50; i++) {
            std::string worker_id = "worker_" + std::to_string(round) + "_" + std::to_string(i);
            EXPECT_EQ(0, security_distributed_training_register_worker(
                bridge, worker_id.c_str(), nullptr));
        }

        /* Unregister half */
        for (int i = 0; i < 25; i++) {
            std::string worker_id = "worker_" + std::to_string(round) + "_" + std::to_string(i);
            security_distributed_training_unregister_worker(bridge, worker_id.c_str());
        }
    }

    /* Should not leak memory or crash */
    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
}

TEST_F(SecurityDistributedRegressionTest, GradientValidationStability) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    SetUpWorkers(5);

    /* Validate many gradients */
    const size_t gradient_size = 500;
    for (int i = 0; i < 1000; i++) {
        std::string worker_id = "worker_" + std::to_string(i % 5);
        auto gradients = GenerateGradients(gradient_size);

        security_gradient_validation_result_t result;
        security_distributed_training_validate_gradients(
            bridge, worker_id.c_str(), gradients.data(), gradients.size(), &result);
    }

    /* Should not leak memory */
    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_EQ(1000u, stats.gradients_validated);
}

/* =============================================================================
 * API Contract Verification Tests
 * ============================================================================= */

TEST_F(SecurityDistributedRegressionTest, DefaultConfigAlwaysSucceeds) {
    security_distributed_training_config_t local_config;
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(0, security_distributed_training_default_config(&local_config));
    }
}

TEST_F(SecurityDistributedRegressionTest, CreateWithDefaultConfigAlwaysSucceeds) {
    for (int i = 0; i < 20; i++) {
        bridge = security_distributed_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
        security_distributed_training_bridge_destroy(bridge);
        bridge = nullptr;
    }
}

TEST_F(SecurityDistributedRegressionTest, DestroyNullAlwaysSafe) {
    for (int i = 0; i < 100; i++) {
        security_distributed_training_bridge_destroy(nullptr);
    }
}

TEST_F(SecurityDistributedRegressionTest, GetPhaseNullReturnsInactive) {
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(SECURITY_DISTRIBUTED_PHASE_INACTIVE,
                  security_distributed_training_get_phase(nullptr));
    }
}

TEST_F(SecurityDistributedRegressionTest, GetThreatLevelNullReturnsZero) {
    for (int i = 0; i < 100; i++) {
        EXPECT_FLOAT_EQ(0.0f, security_distributed_training_get_threat_level(nullptr));
    }
}

TEST_F(SecurityDistributedRegressionTest, StringConversionAlwaysReturnsValidString) {
    /* Worker trust */
    for (int i = -10; i < 20; i++) {
        const char* str = security_worker_trust_to_string((security_worker_trust_t)i);
        EXPECT_NE(nullptr, str);
        EXPECT_GT(strlen(str), 0u);
    }

    /* Byzantine type */
    for (int i = -10; i < 20; i++) {
        const char* str = security_byzantine_type_to_string((security_byzantine_type_t)i);
        EXPECT_NE(nullptr, str);
        EXPECT_GT(strlen(str), 0u);
    }

    /* Aggregation method */
    for (int i = -10; i < 20; i++) {
        const char* str = security_grad_aggregation_to_string((security_grad_aggregation_t)i);
        EXPECT_NE(nullptr, str);
        EXPECT_GT(strlen(str), 0u);
    }

    /* Phase */
    for (int i = -10; i < 20; i++) {
        const char* str = security_distributed_phase_to_string((security_distributed_phase_t)i);
        EXPECT_NE(nullptr, str);
        EXPECT_GT(strlen(str), 0u);
    }

    /* Checkpoint result */
    for (int i = -10; i < 20; i++) {
        const char* str = security_checkpoint_result_to_string((security_checkpoint_result_t)i);
        EXPECT_NE(nullptr, str);
        EXPECT_GT(strlen(str), 0u);
    }
}

/* =============================================================================
 * Edge Case Regression Tests
 * ============================================================================= */

TEST_F(SecurityDistributedRegressionTest, MaxWorkersRegistration) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    /* Try to register up to max workers */
    int registered = 0;
    for (int i = 0; i < SECURITY_DISTRIBUTED_MAX_WORKERS + 10; i++) {
        std::string worker_id = "worker_" + std::to_string(i);
        int result = security_distributed_training_register_worker(
            bridge, worker_id.c_str(), nullptr);

        if (result == 0) {
            registered++;
        } else {
            EXPECT_EQ(NIMCP_ERROR_OUT_OF_RANGE, result);
        }
    }

    /* Should have registered exactly MAX_WORKERS */
    EXPECT_EQ(SECURITY_DISTRIBUTED_MAX_WORKERS, (uint32_t)registered);
}

TEST_F(SecurityDistributedRegressionTest, MaxCheckpointsHandled) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const size_t model_size = 10;
    std::vector<float> model(model_size, 0.5f);

    /* Create more than max checkpoints */
    for (int i = 0; i < SECURITY_DISTRIBUTED_MAX_CHECKPOINTS + 10; i++) {
        std::string cp_name = "checkpoint_" + std::to_string(i);

        /* Modify model slightly for each checkpoint */
        model[0] = (float)i;

        int result = security_distributed_training_secure_checkpoint(
            bridge, cp_name.c_str(), model.data(), model.size(), i);
        EXPECT_EQ(0, result);
    }

    /* Should have at most MAX_CHECKPOINTS */
    std::vector<security_distributed_checkpoint_t> checkpoints(
        SECURITY_DISTRIBUTED_MAX_CHECKPOINTS + 10);
    int count = security_distributed_training_list_checkpoints(
        bridge, checkpoints.data(), checkpoints.size());

    EXPECT_LE(count, (int)SECURITY_DISTRIBUTED_MAX_CHECKPOINTS);
}

TEST_F(SecurityDistributedRegressionTest, LongWorkerIds) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    /* Create worker ID at exactly max length - 1 */
    std::string long_id(SECURITY_DISTRIBUTED_WORKER_ID_MAX - 1, 'w');
    EXPECT_EQ(0, security_distributed_training_register_worker(
        bridge, long_id.c_str(), nullptr));

    security_worker_info_t info;
    EXPECT_EQ(0, security_distributed_training_get_worker_info(
        bridge, long_id.c_str(), &info));
}

TEST_F(SecurityDistributedRegressionTest, LongCheckpointNames) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    const size_t model_size = 10;
    std::vector<float> model(model_size, 0.5f);

    /* Create checkpoint name at exactly max length - 1 */
    std::string long_name(SECURITY_DISTRIBUTED_CHECKPOINT_NAME_MAX - 1, 'c');
    EXPECT_EQ(0, security_distributed_training_secure_checkpoint(
        bridge, long_name.c_str(), model.data(), model.size(), 0));

    security_distributed_checkpoint_t info;
    EXPECT_EQ(0, security_distributed_training_get_checkpoint_info(
        bridge, long_name.c_str(), &info));
}

TEST_F(SecurityDistributedRegressionTest, ZeroSizeGradients) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));
    EXPECT_EQ(0, security_distributed_training_set_worker_trust(
        bridge, "worker_0", SECURITY_WORKER_TRUST_VERIFIED));

    std::vector<float> gradients;
    security_gradient_validation_result_t result;

    /* Zero size should be handled gracefully */
    EXPECT_EQ(0, security_distributed_training_validate_gradients(
        bridge, "worker_0", gradients.data(), 0, &result));
    EXPECT_FALSE(result.is_valid);
}

TEST_F(SecurityDistributedRegressionTest, VeryLargeGradients) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));
    EXPECT_EQ(0, security_distributed_training_set_worker_trust(
        bridge, "worker_0", SECURITY_WORKER_TRUST_VERIFIED));

    /* Create very large gradient array */
    const size_t large_size = 100000;
    std::vector<float> gradients(large_size, 0.01f);

    security_gradient_validation_result_t result;
    EXPECT_EQ(0, security_distributed_training_validate_gradients(
        bridge, "worker_0", gradients.data(), gradients.size(), &result));

    /* Should handle large gradients */
    EXPECT_TRUE(result.is_valid);
}

/* =============================================================================
 * Numerical Stability Tests
 * ============================================================================= */

TEST_F(SecurityDistributedRegressionTest, GradientNormStability) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));
    EXPECT_EQ(0, security_distributed_training_set_worker_trust(
        bridge, "worker_0", SECURITY_WORKER_TRUST_VERIFIED));

    /* Test with very small values */
    std::vector<float> small_gradients = {1e-30f, 1e-30f, 1e-30f};
    security_gradient_validation_result_t result;
    EXPECT_EQ(0, security_distributed_training_validate_gradients(
        bridge, "worker_0", small_gradients.data(), small_gradients.size(), &result));
    EXPECT_GE(result.gradient_norm, 0.0f);
    EXPECT_FALSE(std::isnan(result.gradient_norm));
    EXPECT_FALSE(std::isinf(result.gradient_norm));

    /* Test with mixed values */
    std::vector<float> mixed_gradients = {1e10f, 1e-10f, 1.0f, -1e5f};
    EXPECT_EQ(0, security_distributed_training_validate_gradients(
        bridge, "worker_0", mixed_gradients.data(), mixed_gradients.size(), &result));
    EXPECT_GE(result.gradient_norm, 0.0f);
    EXPECT_FALSE(std::isnan(result.gradient_norm));
}

TEST_F(SecurityDistributedRegressionTest, ThreatLevelBounds) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    /* Perform many operations that might affect threat level */
    SetUpWorkers(20);

    for (int i = 0; i < 10; i++) {
        std::string worker_id = "worker_" + std::to_string(i);
        for (int j = 0; j < 10; j++) {
            security_distributed_training_report_worker_anomaly(
                bridge, worker_id.c_str(), 0.9f, "test");
        }
    }

    security_distributed_training_update_security_effects(bridge);

    /* Threat level should always be in [0, 1] */
    float threat = security_distributed_training_get_threat_level(bridge);
    EXPECT_GE(threat, 0.0f);
    EXPECT_LE(threat, 1.0f);
    EXPECT_FALSE(std::isnan(threat));
    EXPECT_FALSE(std::isinf(threat));
}

TEST_F(SecurityDistributedRegressionTest, TrustScoreBounds) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));

    /* Many trust updates */
    for (int i = 0; i < 100; i++) {
        float score;
        EXPECT_EQ(0, security_distributed_training_score_worker(bridge, "worker_0", &score));
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 1.0f);
        EXPECT_FALSE(std::isnan(score));
        EXPECT_FALSE(std::isinf(score));

        /* Report anomaly to modify score */
        security_distributed_training_report_worker_anomaly(
            bridge, "worker_0", 0.5f, "test");
    }
}

/* =============================================================================
 * State Transition Tests
 * ============================================================================= */

TEST_F(SecurityDistributedRegressionTest, PhaseTransitions) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    /* Initial phase */
    EXPECT_EQ(SECURITY_DISTRIBUTED_PHASE_MONITORING,
              security_distributed_training_get_phase(bridge));

    /* Connect coordinator - should transition to PROTECTING */
    distributed_coordinator_t dummy = (distributed_coordinator_t)0x1234;
    security_distributed_training_connect_coordinator(bridge, dummy);
    EXPECT_EQ(SECURITY_DISTRIBUTED_PHASE_PROTECTING,
              security_distributed_training_get_phase(bridge));

    /* High threat should transition to RESPONDING */
    SetUpWorkers(20);
    for (int i = 0; i < 15; i++) {
        std::string worker_id = "worker_" + std::to_string(i);
        security_distributed_training_quarantine_worker(bridge, worker_id.c_str(), "test");
    }
    security_distributed_training_update_security_effects(bridge);

    security_distributed_phase_t phase = security_distributed_training_get_phase(bridge);
    /* Phase should be RESPONDING or PROTECTING based on threat level */
    EXPECT_NE(SECURITY_DISTRIBUTED_PHASE_INACTIVE, phase);
}

TEST_F(SecurityDistributedRegressionTest, WorkerStateTransitions) {
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));

    /* UNTRUSTED -> PROBATION -> VERIFIED -> TRUSTED */
    EXPECT_EQ(SECURITY_WORKER_TRUST_UNTRUSTED,
              security_distributed_training_get_worker_trust(bridge, "worker_0"));

    security_distributed_training_set_worker_trust(bridge, "worker_0", SECURITY_WORKER_TRUST_PROBATION);
    EXPECT_EQ(SECURITY_WORKER_TRUST_PROBATION,
              security_distributed_training_get_worker_trust(bridge, "worker_0"));

    security_distributed_training_set_worker_trust(bridge, "worker_0", SECURITY_WORKER_TRUST_VERIFIED);
    EXPECT_EQ(SECURITY_WORKER_TRUST_VERIFIED,
              security_distributed_training_get_worker_trust(bridge, "worker_0"));

    security_distributed_training_set_worker_trust(bridge, "worker_0", SECURITY_WORKER_TRUST_TRUSTED);
    EXPECT_EQ(SECURITY_WORKER_TRUST_TRUSTED,
              security_distributed_training_get_worker_trust(bridge, "worker_0"));

    /* TRUSTED -> QUARANTINED via quarantine_worker */
    security_distributed_training_quarantine_worker(bridge, "worker_0", "test");
    EXPECT_EQ(SECURITY_WORKER_TRUST_QUARANTINED,
              security_distributed_training_get_worker_trust(bridge, "worker_0"));

    /* QUARANTINED -> PROBATION via release_worker */
    security_distributed_training_release_worker(bridge, "worker_0");
    EXPECT_EQ(SECURITY_WORKER_TRUST_PROBATION,
              security_distributed_training_get_worker_trust(bridge, "worker_0"));
}

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
