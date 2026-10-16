/**
 * @file test_security_distributed_training_bridge.cpp
 * @brief Unit tests for security-distributed training bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * Comprehensive tests for security-distributed training integration:
 * 1. Lifecycle tests: default_config, create/destroy
 * 2. Connection tests: connect/disconnect functions
 * 3. Worker management tests: register, unregister, trust
 * 4. Byzantine detection tests: detect_byzantine, report_anomaly
 * 5. Gradient validation tests: validate, sanitize
 * 6. Worker trust scoring tests
 * 7. Secure checkpointing tests
 * 8. Bidirectional update tests
 * 9. Stats tests
 * 10. Null safety tests
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>

extern "C" {
#include "security/distributed/nimcp_security_distributed_training_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

/* =============================================================================
 * Test Fixtures
 * ============================================================================= */

class SecurityDistributedTrainingBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&config, 0, sizeof(config));
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            security_distributed_training_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    security_distributed_training_config_t config;
    security_distributed_training_bridge_t* bridge;
};

/* =============================================================================
 * Lifecycle Tests
 * ============================================================================= */

TEST_F(SecurityDistributedTrainingBridgeTest, DefaultConfigValid) {
    EXPECT_EQ(0, security_distributed_training_default_config(&config));

    /* Feature enables should default to true */
    EXPECT_TRUE(config.enable_byzantine_detection);
    EXPECT_TRUE(config.enable_gradient_validation);
    EXPECT_TRUE(config.enable_worker_trust_scoring);
    EXPECT_TRUE(config.enable_secure_checkpointing);
    EXPECT_FALSE(config.enable_secure_aggregation);

    /* Byzantine detection defaults */
    EXPECT_FLOAT_EQ(SECURITY_DISTRIBUTED_DEFAULT_BYZANTINE_THRESHOLD,
                    config.byzantine_threshold);
    EXPECT_FLOAT_EQ(SECURITY_DISTRIBUTED_DEFAULT_ANOMALY_THRESHOLD,
                    config.anomaly_threshold);
    EXPECT_EQ(3u, config.min_workers_for_detection);

    /* Gradient validation defaults */
    EXPECT_FLOAT_EQ(SECURITY_DISTRIBUTED_DEFAULT_GRAD_NORM_THRESHOLD,
                    config.gradient_norm_threshold);
    EXPECT_EQ(SECURITY_GRAD_AGG_TRIMMED_MEAN, config.aggregation_method);

    /* Trust scoring defaults */
    EXPECT_EQ(SECURITY_WORKER_TRUST_PROBATION, config.min_worker_trust);
    EXPECT_TRUE(config.auto_quarantine_byzantine);
}

TEST_F(SecurityDistributedTrainingBridgeTest, DefaultConfigNullFails) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_default_config(nullptr));
}

TEST_F(SecurityDistributedTrainingBridgeTest, CreateWithDefaultConfig) {
    EXPECT_EQ(0, security_distributed_training_default_config(&config));
    bridge = security_distributed_training_bridge_create(&config);
    EXPECT_NE(nullptr, bridge);

    /* Verify initial state - bridge starts in MONITORING phase */
    EXPECT_EQ(SECURITY_DISTRIBUTED_PHASE_MONITORING,
              security_distributed_training_get_phase(bridge));
    EXPECT_FLOAT_EQ(0.0f, security_distributed_training_get_threat_level(bridge));
    EXPECT_FALSE(security_distributed_training_is_under_attack(bridge));
}

TEST_F(SecurityDistributedTrainingBridgeTest, CreateWithNullConfigUsesDefaults) {
    bridge = security_distributed_training_bridge_create(nullptr);
    EXPECT_NE(nullptr, bridge);
}

TEST_F(SecurityDistributedTrainingBridgeTest, DestroyNull) {
    /* Should not crash */
    security_distributed_training_bridge_destroy(nullptr);
}

TEST_F(SecurityDistributedTrainingBridgeTest, CreateDestroyMultiple) {
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(0, security_distributed_training_default_config(&config));
        bridge = security_distributed_training_bridge_create(&config);
        EXPECT_NE(nullptr, bridge);
        security_distributed_training_bridge_destroy(bridge);
        bridge = nullptr;
    }
}

/* =============================================================================
 * Connection Tests
 * ============================================================================= */

class SecurityDistributedConnectionTest : public SecurityDistributedTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityDistributedTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_distributed_training_default_config(&config));
        bridge = security_distributed_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityDistributedConnectionTest, ConnectCoordinator) {
    distributed_coordinator_t dummy_coordinator = (distributed_coordinator_t)0x1234;
    EXPECT_EQ(0, security_distributed_training_connect_coordinator(bridge, dummy_coordinator));

    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_TRUE(stats.coordinator_connected);

    /* Phase should change to PROTECTING when coordinator connected */
    EXPECT_EQ(SECURITY_DISTRIBUTED_PHASE_PROTECTING,
              security_distributed_training_get_phase(bridge));
}

TEST_F(SecurityDistributedConnectionTest, ConnectCoordinatorNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_connect_coordinator(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_connect_coordinator(bridge, nullptr));
}

TEST_F(SecurityDistributedConnectionTest, ConnectBBB) {
    bbb_system_t dummy_bbb = (bbb_system_t)0x5678;
    EXPECT_EQ(0, security_distributed_training_connect_bbb(bridge, dummy_bbb));

    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_TRUE(stats.bbb_connected);
}

TEST_F(SecurityDistributedConnectionTest, ConnectBBBNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_connect_bbb(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_connect_bbb(bridge, nullptr));
}

TEST_F(SecurityDistributedConnectionTest, DisconnectCoordinator) {
    distributed_coordinator_t dummy_coordinator = (distributed_coordinator_t)0x1234;
    EXPECT_EQ(0, security_distributed_training_connect_coordinator(bridge, dummy_coordinator));
    EXPECT_EQ(0, security_distributed_training_disconnect_coordinator(bridge));

    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_FALSE(stats.coordinator_connected);
}

TEST_F(SecurityDistributedConnectionTest, DisconnectBBB) {
    bbb_system_t dummy_bbb = (bbb_system_t)0x5678;
    EXPECT_EQ(0, security_distributed_training_connect_bbb(bridge, dummy_bbb));
    EXPECT_EQ(0, security_distributed_training_disconnect_bbb(bridge));

    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_FALSE(stats.bbb_connected);
}

TEST_F(SecurityDistributedConnectionTest, DisconnectNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_disconnect_coordinator(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_disconnect_bbb(nullptr));
}

/* =============================================================================
 * Worker Management Tests
 * ============================================================================= */

class SecurityDistributedWorkerTest : public SecurityDistributedTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityDistributedTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_distributed_training_default_config(&config));
        bridge = security_distributed_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityDistributedWorkerTest, RegisterWorker) {
    const char* worker_id = "worker_0";
    uint8_t worker_key[64] = {0};

    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, worker_id, worker_key));

    security_worker_info_t info;
    EXPECT_EQ(0, security_distributed_training_get_worker_info(bridge, worker_id, &info));
    EXPECT_STREQ(worker_id, info.worker_id);
    EXPECT_EQ(SECURITY_WORKER_TRUST_UNTRUSTED, info.trust_level);
    EXPECT_TRUE(info.is_active);
    EXPECT_FALSE(info.is_quarantined);
}

TEST_F(SecurityDistributedWorkerTest, RegisterWorkerNullKey) {
    /* Should work with null key */
    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_1", nullptr));
}

TEST_F(SecurityDistributedWorkerTest, RegisterWorkerNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_register_worker(nullptr, "test", nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_register_worker(bridge, nullptr, nullptr));
}

TEST_F(SecurityDistributedWorkerTest, RegisterDuplicateWorker) {
    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));
    EXPECT_EQ(NIMCP_ERROR_ALREADY_EXISTS,
              security_distributed_training_register_worker(bridge, "worker_0", nullptr));
}

TEST_F(SecurityDistributedWorkerTest, RegisterMultipleWorkers) {
    for (int i = 0; i < 10; i++) {
        std::string worker_id = "worker_" + std::to_string(i);
        EXPECT_EQ(0, security_distributed_training_register_worker(
            bridge, worker_id.c_str(), nullptr));
    }

    security_worker_info_t workers[10];
    int count = security_distributed_training_list_workers(bridge, workers, 10);
    EXPECT_EQ(10, count);
}

TEST_F(SecurityDistributedWorkerTest, UnregisterWorker) {
    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));
    EXPECT_EQ(0, security_distributed_training_unregister_worker(bridge, "worker_0"));

    security_worker_info_t info;
    EXPECT_EQ(0, security_distributed_training_get_worker_info(bridge, "worker_0", &info));
    EXPECT_FALSE(info.is_active);
    EXPECT_TRUE(info.pending_removal);
}

TEST_F(SecurityDistributedWorkerTest, UnregisterWorkerNotFound) {
    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND,
              security_distributed_training_unregister_worker(bridge, "nonexistent"));
}

TEST_F(SecurityDistributedWorkerTest, UnregisterWorkerNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_unregister_worker(nullptr, "test"));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_unregister_worker(bridge, nullptr));
}

TEST_F(SecurityDistributedWorkerTest, GetWorkerInfoNotFound) {
    security_worker_info_t info;
    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND,
              security_distributed_training_get_worker_info(bridge, "nonexistent", &info));
}

TEST_F(SecurityDistributedWorkerTest, GetWorkerInfoNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_worker_info(nullptr, "test", nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_worker_info(bridge, nullptr, nullptr));
    security_worker_info_t info;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_worker_info(bridge, "test", nullptr));
}

TEST_F(SecurityDistributedWorkerTest, GetWorkerTrust) {
    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));
    EXPECT_EQ(SECURITY_WORKER_TRUST_UNTRUSTED,
              security_distributed_training_get_worker_trust(bridge, "worker_0"));
}

TEST_F(SecurityDistributedWorkerTest, GetWorkerTrustNotFound) {
    EXPECT_EQ(SECURITY_WORKER_TRUST_QUARANTINED,
              security_distributed_training_get_worker_trust(bridge, "nonexistent"));
}

TEST_F(SecurityDistributedWorkerTest, GetWorkerTrustNull) {
    EXPECT_EQ(SECURITY_WORKER_TRUST_QUARANTINED,
              security_distributed_training_get_worker_trust(nullptr, "test"));
    EXPECT_EQ(SECURITY_WORKER_TRUST_QUARANTINED,
              security_distributed_training_get_worker_trust(bridge, nullptr));
}

TEST_F(SecurityDistributedWorkerTest, SetWorkerTrust) {
    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));

    EXPECT_EQ(0, security_distributed_training_set_worker_trust(
        bridge, "worker_0", SECURITY_WORKER_TRUST_VERIFIED));
    EXPECT_EQ(SECURITY_WORKER_TRUST_VERIFIED,
              security_distributed_training_get_worker_trust(bridge, "worker_0"));

    EXPECT_EQ(0, security_distributed_training_set_worker_trust(
        bridge, "worker_0", SECURITY_WORKER_TRUST_TRUSTED));
    EXPECT_EQ(SECURITY_WORKER_TRUST_TRUSTED,
              security_distributed_training_get_worker_trust(bridge, "worker_0"));
}

TEST_F(SecurityDistributedWorkerTest, SetWorkerTrustNotFound) {
    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND,
              security_distributed_training_set_worker_trust(
                  bridge, "nonexistent", SECURITY_WORKER_TRUST_VERIFIED));
}

TEST_F(SecurityDistributedWorkerTest, SetWorkerTrustNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_set_worker_trust(
                  nullptr, "test", SECURITY_WORKER_TRUST_VERIFIED));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_set_worker_trust(
                  bridge, nullptr, SECURITY_WORKER_TRUST_VERIFIED));
}

TEST_F(SecurityDistributedWorkerTest, QuarantineWorker) {
    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));
    EXPECT_EQ(0, security_distributed_training_quarantine_worker(
        bridge, "worker_0", "Byzantine behavior detected"));

    security_worker_info_t info;
    EXPECT_EQ(0, security_distributed_training_get_worker_info(bridge, "worker_0", &info));
    EXPECT_TRUE(info.is_quarantined);
    EXPECT_EQ(SECURITY_WORKER_TRUST_QUARANTINED, info.trust_level);

    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.workers_quarantined);
}

TEST_F(SecurityDistributedWorkerTest, QuarantineWorkerNotFound) {
    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND,
              security_distributed_training_quarantine_worker(
                  bridge, "nonexistent", "reason"));
}

TEST_F(SecurityDistributedWorkerTest, QuarantineWorkerNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_quarantine_worker(nullptr, "test", "reason"));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_quarantine_worker(bridge, nullptr, "reason"));
}

TEST_F(SecurityDistributedWorkerTest, ReleaseWorker) {
    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));
    EXPECT_EQ(0, security_distributed_training_quarantine_worker(
        bridge, "worker_0", "reason"));
    EXPECT_EQ(0, security_distributed_training_release_worker(bridge, "worker_0"));

    security_worker_info_t info;
    EXPECT_EQ(0, security_distributed_training_get_worker_info(bridge, "worker_0", &info));
    EXPECT_FALSE(info.is_quarantined);
    EXPECT_EQ(SECURITY_WORKER_TRUST_PROBATION, info.trust_level);
}

TEST_F(SecurityDistributedWorkerTest, ReleaseWorkerNotQuarantined) {
    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));
    EXPECT_EQ(NIMCP_ERROR_INVALID_STATE,
              security_distributed_training_release_worker(bridge, "worker_0"));
}

TEST_F(SecurityDistributedWorkerTest, ReleaseWorkerNotFound) {
    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND,
              security_distributed_training_release_worker(bridge, "nonexistent"));
}

TEST_F(SecurityDistributedWorkerTest, ReleaseWorkerNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_release_worker(nullptr, "test"));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_release_worker(bridge, nullptr));
}

TEST_F(SecurityDistributedWorkerTest, ListWorkers) {
    for (int i = 0; i < 5; i++) {
        std::string worker_id = "worker_" + std::to_string(i);
        EXPECT_EQ(0, security_distributed_training_register_worker(
            bridge, worker_id.c_str(), nullptr));
    }

    security_worker_info_t workers[10];
    int count = security_distributed_training_list_workers(bridge, workers, 10);
    EXPECT_EQ(5, count);

    /* Verify all workers are in list */
    for (int i = 0; i < 5; i++) {
        std::string expected_id = "worker_" + std::to_string(i);
        bool found = false;
        for (int j = 0; j < count; j++) {
            if (expected_id == workers[j].worker_id) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Worker " << expected_id << " not found";
    }
}

TEST_F(SecurityDistributedWorkerTest, ListWorkersNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_list_workers(nullptr, nullptr, 0));
}

TEST_F(SecurityDistributedWorkerTest, ScoreWorker) {
    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));

    float score = 0.0f;
    EXPECT_EQ(0, security_distributed_training_score_worker(bridge, "worker_0", &score));
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(SecurityDistributedWorkerTest, ScoreWorkerNotFound) {
    float score = 0.0f;
    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND,
              security_distributed_training_score_worker(bridge, "nonexistent", &score));
}

TEST_F(SecurityDistributedWorkerTest, ScoreWorkerNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_score_worker(nullptr, "test", nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_score_worker(bridge, nullptr, nullptr));
    float score;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_score_worker(bridge, "test", nullptr));
}

/* =============================================================================
 * Byzantine Detection Tests
 * ============================================================================= */

class SecurityDistributedByzantineTest : public SecurityDistributedTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityDistributedTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_distributed_training_default_config(&config));
        config.enable_byzantine_detection = true;
        config.min_workers_for_detection = 3;
        bridge = security_distributed_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);

        /* Register some workers for testing */
        for (int i = 0; i < 5; i++) {
            std::string worker_id = "worker_" + std::to_string(i);
            EXPECT_EQ(0, security_distributed_training_register_worker(
                bridge, worker_id.c_str(), nullptr));
        }
    }
};

TEST_F(SecurityDistributedByzantineTest, DetectByzantineNone) {
    security_byzantine_result_t result;
    EXPECT_EQ(0, security_distributed_training_detect_byzantine(bridge, &result));

    /* No Byzantine behavior without anomalies */
    EXPECT_FALSE(result.byzantine_detected);
    EXPECT_EQ(SECURITY_BYZANTINE_NONE, result.type);
}

TEST_F(SecurityDistributedByzantineTest, DetectByzantineWithAnomalies) {
    /* Report anomalies for a worker multiple times */
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(0, security_distributed_training_report_worker_anomaly(
            bridge, "worker_0", 0.9f, "Gradient manipulation"));
    }

    security_byzantine_result_t result;
    EXPECT_EQ(0, security_distributed_training_detect_byzantine(bridge, &result));

    /* After multiple anomalies, should detect Byzantine */
    EXPECT_TRUE(result.byzantine_detected);
    EXPECT_NE(SECURITY_BYZANTINE_NONE, result.type);
    EXPECT_GT(result.confidence, 0.0f);
    EXPECT_TRUE(result.quarantine_recommended);
}

TEST_F(SecurityDistributedByzantineTest, DetectByzantineInsufficientWorkers) {
    /* Create new bridge with high min_workers_for_detection */
    security_distributed_training_bridge_destroy(bridge);

    config.min_workers_for_detection = 10;
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    /* Register only 3 workers */
    for (int i = 0; i < 3; i++) {
        std::string worker_id = "worker_" + std::to_string(i);
        EXPECT_EQ(0, security_distributed_training_register_worker(
            bridge, worker_id.c_str(), nullptr));
    }

    security_byzantine_result_t result;
    EXPECT_EQ(0, security_distributed_training_detect_byzantine(bridge, &result));

    /* Should not detect with insufficient workers */
    EXPECT_FALSE(result.byzantine_detected);
}

TEST_F(SecurityDistributedByzantineTest, DetectByzantineDisabled) {
    security_distributed_training_bridge_destroy(bridge);

    config.enable_byzantine_detection = false;
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    security_byzantine_result_t result;
    EXPECT_EQ(0, security_distributed_training_detect_byzantine(bridge, &result));
    EXPECT_FALSE(result.byzantine_detected);
}

TEST_F(SecurityDistributedByzantineTest, DetectByzantineNull) {
    security_byzantine_result_t result;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_detect_byzantine(nullptr, &result));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_detect_byzantine(bridge, nullptr));
}

TEST_F(SecurityDistributedByzantineTest, ReportWorkerAnomaly) {
    EXPECT_EQ(0, security_distributed_training_report_worker_anomaly(
        bridge, "worker_0", 0.8f, "Suspicious gradient pattern"));

    security_worker_info_t info;
    EXPECT_EQ(0, security_distributed_training_get_worker_info(bridge, "worker_0", &info));
    EXPECT_EQ(1u, info.violations_count);
    EXPECT_GT(info.last_violation_ms, 0u);
}

TEST_F(SecurityDistributedByzantineTest, ReportWorkerAnomalyNotFound) {
    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND,
              security_distributed_training_report_worker_anomaly(
                  bridge, "nonexistent", 0.5f, "reason"));
}

TEST_F(SecurityDistributedByzantineTest, ReportWorkerAnomalyNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_report_worker_anomaly(
                  nullptr, "test", 0.5f, "reason"));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_report_worker_anomaly(
                  bridge, nullptr, 0.5f, "reason"));
}

TEST_F(SecurityDistributedByzantineTest, AutoQuarantineByzantine) {
    /* Enable auto quarantine */
    EXPECT_TRUE(bridge != nullptr);

    /* Report many anomalies to trigger auto-quarantine */
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(0, security_distributed_training_report_worker_anomaly(
            bridge, "worker_0", 0.95f, "Byzantine behavior"));
    }

    security_worker_info_t info;
    EXPECT_EQ(0, security_distributed_training_get_worker_info(bridge, "worker_0", &info));

    /* Should be auto-quarantined after multiple high anomalies */
    EXPECT_TRUE(info.is_quarantined);
    EXPECT_EQ(SECURITY_WORKER_TRUST_QUARANTINED, info.trust_level);
}

/* =============================================================================
 * Gradient Validation Tests
 * ============================================================================= */

class SecurityDistributedGradientTest : public SecurityDistributedTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityDistributedTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_distributed_training_default_config(&config));
        config.enable_gradient_validation = true;
        config.gradient_norm_threshold = 10.0f;
        bridge = security_distributed_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);

        /* Register and set trust for test worker */
        EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));
        EXPECT_EQ(0, security_distributed_training_set_worker_trust(
            bridge, "worker_0", SECURITY_WORKER_TRUST_VERIFIED));
    }
};

TEST_F(SecurityDistributedGradientTest, ValidateNormalGradients) {
    std::vector<float> gradients = {0.1f, -0.2f, 0.3f, -0.1f, 0.05f};
    security_gradient_validation_result_t result;

    EXPECT_EQ(0, security_distributed_training_validate_gradients(
        bridge, "worker_0", gradients.data(), gradients.size(), &result));

    EXPECT_TRUE(result.is_valid);
    EXPECT_FALSE(result.is_suspicious);
    EXPECT_FALSE(result.nan_inf_detected);
    EXPECT_FALSE(result.norm_exceeded);
    EXPECT_FALSE(result.reject_recommended);
}

TEST_F(SecurityDistributedGradientTest, ValidateLargeNormGradients) {
    /* Create gradients with large norm */
    std::vector<float> gradients(100, 10.0f);  /* Norm will exceed threshold */
    security_gradient_validation_result_t result;

    EXPECT_EQ(0, security_distributed_training_validate_gradients(
        bridge, "worker_0", gradients.data(), gradients.size(), &result));

    EXPECT_TRUE(result.norm_exceeded);
    EXPECT_TRUE(result.is_suspicious);
}

TEST_F(SecurityDistributedGradientTest, ValidateNaNGradients) {
    std::vector<float> gradients = {0.1f, NAN, 0.3f};
    security_gradient_validation_result_t result;

    EXPECT_EQ(0, security_distributed_training_validate_gradients(
        bridge, "worker_0", gradients.data(), gradients.size(), &result));

    EXPECT_FALSE(result.is_valid);
    EXPECT_TRUE(result.nan_inf_detected);
    EXPECT_TRUE(result.reject_recommended);
}

TEST_F(SecurityDistributedGradientTest, ValidateInfGradients) {
    std::vector<float> gradients = {0.1f, INFINITY, 0.3f};
    security_gradient_validation_result_t result;

    EXPECT_EQ(0, security_distributed_training_validate_gradients(
        bridge, "worker_0", gradients.data(), gradients.size(), &result));

    EXPECT_FALSE(result.is_valid);
    EXPECT_TRUE(result.nan_inf_detected);
    EXPECT_TRUE(result.reject_recommended);
}

TEST_F(SecurityDistributedGradientTest, ValidateQuarantinedWorker) {
    EXPECT_EQ(0, security_distributed_training_quarantine_worker(
        bridge, "worker_0", "test"));

    std::vector<float> gradients = {0.1f, 0.2f};
    security_gradient_validation_result_t result;

    EXPECT_EQ(0, security_distributed_training_validate_gradients(
        bridge, "worker_0", gradients.data(), gradients.size(), &result));

    EXPECT_FALSE(result.is_valid);
    EXPECT_TRUE(result.reject_recommended);
}

TEST_F(SecurityDistributedGradientTest, ValidateGradientsWorkerNotFound) {
    std::vector<float> gradients = {0.1f, 0.2f};
    security_gradient_validation_result_t result;

    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND, security_distributed_training_validate_gradients(
        bridge, "nonexistent", gradients.data(), gradients.size(), &result));
}

TEST_F(SecurityDistributedGradientTest, ValidateGradientsNull) {
    std::vector<float> gradients = {0.1f, 0.2f};
    security_gradient_validation_result_t result;

    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_validate_gradients(
                  nullptr, "worker_0", gradients.data(), 2, &result));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_validate_gradients(
                  bridge, nullptr, gradients.data(), 2, &result));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_validate_gradients(
                  bridge, "worker_0", gradients.data(), 2, nullptr));
}

TEST_F(SecurityDistributedGradientTest, ValidateGradientsNullData) {
    security_gradient_validation_result_t result;

    EXPECT_EQ(0, security_distributed_training_validate_gradients(
        bridge, "worker_0", nullptr, 100, &result));

    EXPECT_FALSE(result.is_valid);
    EXPECT_TRUE(result.reject_recommended);
}

TEST_F(SecurityDistributedGradientTest, ValidateGradientsDisabled) {
    security_distributed_training_bridge_destroy(bridge);

    config.enable_gradient_validation = false;
    bridge = security_distributed_training_bridge_create(&config);
    ASSERT_NE(nullptr, bridge);

    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));

    std::vector<float> gradients = {100.0f, 200.0f};  /* Would normally fail */
    security_gradient_validation_result_t result;

    EXPECT_EQ(0, security_distributed_training_validate_gradients(
        bridge, "worker_0", gradients.data(), gradients.size(), &result));

    EXPECT_TRUE(result.is_valid);
}

TEST_F(SecurityDistributedGradientTest, ValidateAggregatedGradients) {
    std::vector<float> aggregated = {0.1f, 0.2f, 0.3f, 0.4f};
    float anomaly_score = 0.0f;

    bool anomaly = security_distributed_training_validate_aggregated(
        bridge, aggregated.data(), aggregated.size(), &anomaly_score);

    EXPECT_FALSE(anomaly);
    EXPECT_LT(anomaly_score, 0.5f);
}

TEST_F(SecurityDistributedGradientTest, ValidateAggregatedGradientsAnomaly) {
    std::vector<float> aggregated(100, 100.0f);  /* Large values */
    float anomaly_score = 0.0f;

    bool anomaly = security_distributed_training_validate_aggregated(
        bridge, aggregated.data(), aggregated.size(), &anomaly_score);

    EXPECT_GT(anomaly_score, 0.5f);
}

TEST_F(SecurityDistributedGradientTest, ValidateAggregatedNull) {
    float score;
    EXPECT_FALSE(security_distributed_training_validate_aggregated(
        nullptr, nullptr, 0, &score));
    EXPECT_FALSE(security_distributed_training_validate_aggregated(
        bridge, nullptr, 0, &score));
}

TEST_F(SecurityDistributedGradientTest, GetAggregationMethod) {
    EXPECT_EQ(SECURITY_GRAD_AGG_TRIMMED_MEAN,
              security_distributed_training_get_aggregation_method(bridge));
}

TEST_F(SecurityDistributedGradientTest, GetAggregationMethodNull) {
    EXPECT_EQ(SECURITY_GRAD_AGG_SIMPLE_AVERAGE,
              security_distributed_training_get_aggregation_method(nullptr));
}

/* =============================================================================
 * Secure Checkpointing Tests
 * ============================================================================= */

class SecurityDistributedCheckpointTest : public SecurityDistributedTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityDistributedTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_distributed_training_default_config(&config));
        config.enable_secure_checkpointing = true;
        bridge = security_distributed_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityDistributedCheckpointTest, SecureCheckpoint) {
    std::vector<float> weights = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    EXPECT_EQ(0, security_distributed_training_secure_checkpoint(
        bridge, "checkpoint_1", weights.data(), weights.size(), 1000));

    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.checkpoints_created);
}

TEST_F(SecurityDistributedCheckpointTest, SecureCheckpointNull) {
    std::vector<float> weights = {0.1f, 0.2f};

    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_secure_checkpoint(
                  nullptr, "test", weights.data(), 2, 0));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_secure_checkpoint(
                  bridge, nullptr, weights.data(), 2, 0));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_secure_checkpoint(
                  bridge, "test", nullptr, 2, 0));
}

TEST_F(SecurityDistributedCheckpointTest, SecureCheckpointEmpty) {
    std::vector<float> weights = {0.1f};

    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM,
              security_distributed_training_secure_checkpoint(
                  bridge, "test", weights.data(), 0, 0));
}

TEST_F(SecurityDistributedCheckpointTest, VerifyCheckpoint) {
    std::vector<float> weights = {0.1f, 0.2f, 0.3f, 0.4f};

    EXPECT_EQ(0, security_distributed_training_secure_checkpoint(
        bridge, "checkpoint_1", weights.data(), weights.size(), 100));

    security_checkpoint_result_t result = security_distributed_training_verify_checkpoint(
        bridge, "checkpoint_1", weights.data(), weights.size());

    EXPECT_EQ(SECURITY_CHECKPOINT_OK, result);
}

TEST_F(SecurityDistributedCheckpointTest, VerifyCheckpointTampered) {
    std::vector<float> weights = {0.1f, 0.2f, 0.3f, 0.4f};

    EXPECT_EQ(0, security_distributed_training_secure_checkpoint(
        bridge, "checkpoint_1", weights.data(), weights.size(), 100));

    /* Tamper with weights */
    weights[0] = 999.9f;

    security_checkpoint_result_t result = security_distributed_training_verify_checkpoint(
        bridge, "checkpoint_1", weights.data(), weights.size());

    EXPECT_EQ(SECURITY_CHECKPOINT_HASH_MISMATCH, result);
}

TEST_F(SecurityDistributedCheckpointTest, VerifyCheckpointNotFound) {
    std::vector<float> weights = {0.1f, 0.2f};

    security_checkpoint_result_t result = security_distributed_training_verify_checkpoint(
        bridge, "nonexistent", weights.data(), weights.size());

    EXPECT_EQ(SECURITY_CHECKPOINT_HASH_MISMATCH, result);
}

TEST_F(SecurityDistributedCheckpointTest, VerifyCheckpointNull) {
    EXPECT_EQ(SECURITY_CHECKPOINT_TAMPERED,
              security_distributed_training_verify_checkpoint(nullptr, "test", nullptr, 0));
}

TEST_F(SecurityDistributedCheckpointTest, GetCheckpointInfo) {
    std::vector<float> weights = {0.1f, 0.2f, 0.3f};

    EXPECT_EQ(0, security_distributed_training_secure_checkpoint(
        bridge, "info_test", weights.data(), weights.size(), 500));

    security_distributed_checkpoint_t info;
    EXPECT_EQ(0, security_distributed_training_get_checkpoint_info(
        bridge, "info_test", &info));

    EXPECT_STREQ("info_test", info.name);
    EXPECT_EQ(500u, info.round);
    EXPECT_GT(info.timestamp_ms, 0u);
    EXPECT_TRUE(info.consensus_reached);
}

TEST_F(SecurityDistributedCheckpointTest, GetCheckpointInfoNotFound) {
    security_distributed_checkpoint_t info;
    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND,
              security_distributed_training_get_checkpoint_info(
                  bridge, "nonexistent", &info));
}

TEST_F(SecurityDistributedCheckpointTest, GetCheckpointInfoNull) {
    security_distributed_checkpoint_t info;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_checkpoint_info(nullptr, "test", &info));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_checkpoint_info(bridge, nullptr, &info));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_checkpoint_info(bridge, "test", nullptr));
}

TEST_F(SecurityDistributedCheckpointTest, ListCheckpoints) {
    std::vector<float> weights = {0.1f, 0.2f};

    /* Create multiple checkpoints */
    EXPECT_EQ(0, security_distributed_training_secure_checkpoint(
        bridge, "cp1", weights.data(), 2, 100));
    EXPECT_EQ(0, security_distributed_training_secure_checkpoint(
        bridge, "cp2", weights.data(), 2, 200));
    EXPECT_EQ(0, security_distributed_training_secure_checkpoint(
        bridge, "cp3", weights.data(), 2, 300));

    std::vector<security_distributed_checkpoint_t> checkpoints(10);
    int count = security_distributed_training_list_checkpoints(
        bridge, checkpoints.data(), 10);

    EXPECT_EQ(3, count);
}

TEST_F(SecurityDistributedCheckpointTest, ListCheckpointsNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_list_checkpoints(nullptr, nullptr, 0));
}

/* =============================================================================
 * Bidirectional Update Tests
 * ============================================================================= */

class SecurityDistributedBidirectionalTest : public SecurityDistributedTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityDistributedTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_distributed_training_default_config(&config));
        bridge = security_distributed_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityDistributedBidirectionalTest, UpdateSecurityEffects) {
    EXPECT_EQ(0, security_distributed_training_update_security_effects(bridge));

    security_distributed_effects_t effects;
    EXPECT_EQ(0, security_distributed_training_get_security_effects(bridge, &effects));
    EXPECT_TRUE(effects.valid);
    EXPECT_GT(effects.last_update_ms, 0u);
}

TEST_F(SecurityDistributedBidirectionalTest, UpdateTrainingEffects) {
    uint64_t round = 1000;
    float loss = 0.5f;
    uint32_t active_workers = 10;

    EXPECT_EQ(0, security_distributed_training_update_training_effects(
        bridge, round, loss, active_workers));

    distributed_security_effects_t effects;
    EXPECT_EQ(0, security_distributed_training_get_training_effects(bridge, &effects));
    EXPECT_TRUE(effects.valid);
    EXPECT_EQ(round, effects.current_round);
    EXPECT_FLOAT_EQ(loss, effects.current_loss);
    EXPECT_EQ(active_workers, effects.active_workers);
}

TEST_F(SecurityDistributedBidirectionalTest, GetSecurityEffects) {
    security_distributed_effects_t effects;
    EXPECT_EQ(0, security_distributed_training_get_security_effects(bridge, &effects));

    EXPECT_GE(effects.threat_level, 0.0f);
    EXPECT_LE(effects.threat_level, 1.0f);
}

TEST_F(SecurityDistributedBidirectionalTest, GetTrainingEffects) {
    distributed_security_effects_t effects;
    EXPECT_EQ(0, security_distributed_training_get_training_effects(bridge, &effects));
}

TEST_F(SecurityDistributedBidirectionalTest, UpdateSecurityEffectsNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_update_security_effects(nullptr));
}

TEST_F(SecurityDistributedBidirectionalTest, UpdateTrainingEffectsNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_update_training_effects(nullptr, 0, 0.0f, 0));
}

TEST_F(SecurityDistributedBidirectionalTest, GetSecurityEffectsNull) {
    security_distributed_effects_t effects;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_security_effects(nullptr, &effects));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_security_effects(bridge, nullptr));
}

TEST_F(SecurityDistributedBidirectionalTest, GetTrainingEffectsNull) {
    distributed_security_effects_t effects;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_training_effects(nullptr, &effects));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_training_effects(bridge, nullptr));
}

TEST_F(SecurityDistributedBidirectionalTest, ThreatLevelFromQuarantinedWorkers) {
    /* Register and quarantine workers */
    for (int i = 0; i < 5; i++) {
        std::string worker_id = "worker_" + std::to_string(i);
        EXPECT_EQ(0, security_distributed_training_register_worker(
            bridge, worker_id.c_str(), nullptr));
    }

    for (int i = 0; i < 3; i++) {
        std::string worker_id = "worker_" + std::to_string(i);
        EXPECT_EQ(0, security_distributed_training_quarantine_worker(
            bridge, worker_id.c_str(), "test"));
    }

    EXPECT_EQ(0, security_distributed_training_update_security_effects(bridge));

    security_distributed_effects_t effects;
    EXPECT_EQ(0, security_distributed_training_get_security_effects(bridge, &effects));

    EXPECT_GT(effects.threat_level, 0.0f);
    EXPECT_EQ(3u, effects.quarantined_worker_count);
}

/* =============================================================================
 * Stats Tests
 * ============================================================================= */

class SecurityDistributedStatsTest : public SecurityDistributedTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityDistributedTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_distributed_training_default_config(&config));
        bridge = security_distributed_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityDistributedStatsTest, GetStatsInitial) {
    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));

    EXPECT_EQ(0u, stats.total_workers_registered);
    EXPECT_EQ(0u, stats.workers_quarantined);
    EXPECT_EQ(0u, stats.byzantine_checks);
    EXPECT_EQ(0u, stats.gradients_validated);
    EXPECT_EQ(0u, stats.checkpoints_created);
    EXPECT_EQ(SECURITY_DISTRIBUTED_PHASE_MONITORING, stats.current_phase);
}

TEST_F(SecurityDistributedStatsTest, GetStatsAfterOperations) {
    /* Register workers */
    for (int i = 0; i < 5; i++) {
        std::string worker_id = "worker_" + std::to_string(i);
        EXPECT_EQ(0, security_distributed_training_register_worker(
            bridge, worker_id.c_str(), nullptr));
        EXPECT_EQ(0, security_distributed_training_set_worker_trust(
            bridge, worker_id.c_str(), SECURITY_WORKER_TRUST_VERIFIED));
    }

    /* Validate gradients */
    std::vector<float> gradients = {0.1f, 0.2f, 0.3f};
    security_gradient_validation_result_t result;
    EXPECT_EQ(0, security_distributed_training_validate_gradients(
        bridge, "worker_0", gradients.data(), gradients.size(), &result));

    /* Byzantine check */
    security_byzantine_result_t byz_result;
    EXPECT_EQ(0, security_distributed_training_detect_byzantine(bridge, &byz_result));

    /* Create checkpoint */
    EXPECT_EQ(0, security_distributed_training_secure_checkpoint(
        bridge, "cp1", gradients.data(), gradients.size(), 1));

    /* Get stats */
    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));

    EXPECT_EQ(5u, stats.total_workers_registered);
    EXPECT_GT(stats.gradients_validated, 0u);
    EXPECT_GT(stats.byzantine_checks, 0u);
    EXPECT_GT(stats.checkpoints_created, 0u);
}

TEST_F(SecurityDistributedStatsTest, ResetStats) {
    /* Register worker and validate gradients */
    EXPECT_EQ(0, security_distributed_training_register_worker(bridge, "worker_0", nullptr));
    EXPECT_EQ(0, security_distributed_training_set_worker_trust(
        bridge, "worker_0", SECURITY_WORKER_TRUST_VERIFIED));

    std::vector<float> gradients = {0.1f, 0.2f};
    security_gradient_validation_result_t result;
    EXPECT_EQ(0, security_distributed_training_validate_gradients(
        bridge, "worker_0", gradients.data(), gradients.size(), &result));

    security_distributed_training_stats_t stats;
    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_GT(stats.gradients_validated, 0u);

    /* Reset stats */
    EXPECT_EQ(0, security_distributed_training_reset_stats(bridge));

    EXPECT_EQ(0, security_distributed_training_get_stats(bridge, &stats));
    EXPECT_EQ(0u, stats.gradients_validated);
}

TEST_F(SecurityDistributedStatsTest, GetStatsNull) {
    security_distributed_training_stats_t stats;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_stats(nullptr, &stats));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_stats(bridge, nullptr));
}

TEST_F(SecurityDistributedStatsTest, ResetStatsNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_reset_stats(nullptr));
}

/* =============================================================================
 * Query Tests
 * ============================================================================= */

TEST_F(SecurityDistributedStatsTest, GetPhase) {
    EXPECT_EQ(SECURITY_DISTRIBUTED_PHASE_MONITORING,
              security_distributed_training_get_phase(bridge));
}

TEST_F(SecurityDistributedStatsTest, GetPhaseNull) {
    EXPECT_EQ(SECURITY_DISTRIBUTED_PHASE_INACTIVE,
              security_distributed_training_get_phase(nullptr));
}

TEST_F(SecurityDistributedStatsTest, GetThreatLevel) {
    float threat = security_distributed_training_get_threat_level(bridge);
    EXPECT_GE(threat, 0.0f);
    EXPECT_LE(threat, 1.0f);
}

TEST_F(SecurityDistributedStatsTest, GetThreatLevelNull) {
    EXPECT_FLOAT_EQ(0.0f, security_distributed_training_get_threat_level(nullptr));
}

TEST_F(SecurityDistributedStatsTest, GetByzantineRatio) {
    float ratio = security_distributed_training_get_byzantine_ratio(bridge);
    EXPECT_GE(ratio, 0.0f);
    EXPECT_LE(ratio, 1.0f);
}

TEST_F(SecurityDistributedStatsTest, GetByzantineRatioNull) {
    EXPECT_FLOAT_EQ(0.0f, security_distributed_training_get_byzantine_ratio(nullptr));
}

TEST_F(SecurityDistributedStatsTest, IsUnderAttack) {
    EXPECT_FALSE(security_distributed_training_is_under_attack(bridge));
}

TEST_F(SecurityDistributedStatsTest, IsUnderAttackNull) {
    EXPECT_FALSE(security_distributed_training_is_under_attack(nullptr));
}

/* =============================================================================
 * String Conversion Tests
 * ============================================================================= */

TEST(SecurityDistributedStringConversionTest, WorkerTrustToString) {
    EXPECT_STREQ("quarantined",
                 security_worker_trust_to_string(SECURITY_WORKER_TRUST_QUARANTINED));
    EXPECT_STREQ("untrusted",
                 security_worker_trust_to_string(SECURITY_WORKER_TRUST_UNTRUSTED));
    EXPECT_STREQ("probation",
                 security_worker_trust_to_string(SECURITY_WORKER_TRUST_PROBATION));
    EXPECT_STREQ("verified",
                 security_worker_trust_to_string(SECURITY_WORKER_TRUST_VERIFIED));
    EXPECT_STREQ("trusted",
                 security_worker_trust_to_string(SECURITY_WORKER_TRUST_TRUSTED));
    EXPECT_STREQ("unknown",
                 security_worker_trust_to_string((security_worker_trust_t)999));
}

TEST(SecurityDistributedStringConversionTest, ByzantineTypeToString) {
    EXPECT_STREQ("none",
                 security_byzantine_type_to_string(SECURITY_BYZANTINE_NONE));
    EXPECT_STREQ("gradient_attack",
                 security_byzantine_type_to_string(SECURITY_BYZANTINE_GRADIENT_ATTACK));
    EXPECT_STREQ("free_rider",
                 security_byzantine_type_to_string(SECURITY_BYZANTINE_FREE_RIDER));
    EXPECT_STREQ("sybil",
                 security_byzantine_type_to_string(SECURITY_BYZANTINE_SYBIL));
    EXPECT_STREQ("collusion",
                 security_byzantine_type_to_string(SECURITY_BYZANTINE_COLLUSION));
    EXPECT_STREQ("model_poisoning",
                 security_byzantine_type_to_string(SECURITY_BYZANTINE_MODEL_POISONING));
    EXPECT_STREQ("data_leakage",
                 security_byzantine_type_to_string(SECURITY_BYZANTINE_DATA_LEAKAGE));
    EXPECT_STREQ("unknown",
                 security_byzantine_type_to_string((security_byzantine_type_t)999));
}

TEST(SecurityDistributedStringConversionTest, GradAggregationToString) {
    EXPECT_STREQ("simple_average",
                 security_grad_aggregation_to_string(SECURITY_GRAD_AGG_SIMPLE_AVERAGE));
    EXPECT_STREQ("median",
                 security_grad_aggregation_to_string(SECURITY_GRAD_AGG_MEDIAN));
    EXPECT_STREQ("trimmed_mean",
                 security_grad_aggregation_to_string(SECURITY_GRAD_AGG_TRIMMED_MEAN));
    EXPECT_STREQ("krum",
                 security_grad_aggregation_to_string(SECURITY_GRAD_AGG_KRUM));
    EXPECT_STREQ("bulyan",
                 security_grad_aggregation_to_string(SECURITY_GRAD_AGG_BULYAN));
    EXPECT_STREQ("secure",
                 security_grad_aggregation_to_string(SECURITY_GRAD_AGG_SECURE));
    EXPECT_STREQ("multi_krum",
                 security_grad_aggregation_to_string(SECURITY_GRAD_AGG_MULTI_KRUM));
    EXPECT_STREQ("unknown",
                 security_grad_aggregation_to_string((security_grad_aggregation_t)999));
}

TEST(SecurityDistributedStringConversionTest, PhaseToString) {
    EXPECT_STREQ("inactive",
                 security_distributed_phase_to_string(SECURITY_DISTRIBUTED_PHASE_INACTIVE));
    EXPECT_STREQ("monitoring",
                 security_distributed_phase_to_string(SECURITY_DISTRIBUTED_PHASE_MONITORING));
    EXPECT_STREQ("protecting",
                 security_distributed_phase_to_string(SECURITY_DISTRIBUTED_PHASE_PROTECTING));
    EXPECT_STREQ("responding",
                 security_distributed_phase_to_string(SECURITY_DISTRIBUTED_PHASE_RESPONDING));
    EXPECT_STREQ("recovery",
                 security_distributed_phase_to_string(SECURITY_DISTRIBUTED_PHASE_RECOVERY));
    EXPECT_STREQ("unknown",
                 security_distributed_phase_to_string((security_distributed_phase_t)999));
}

TEST(SecurityDistributedStringConversionTest, CheckpointResultToString) {
    EXPECT_STREQ("ok",
                 security_checkpoint_result_to_string(SECURITY_CHECKPOINT_OK));
    EXPECT_STREQ("hash_mismatch",
                 security_checkpoint_result_to_string(SECURITY_CHECKPOINT_HASH_MISMATCH));
    EXPECT_STREQ("no_consensus",
                 security_checkpoint_result_to_string(SECURITY_CHECKPOINT_NO_CONSENSUS));
    EXPECT_STREQ("insufficient_nodes",
                 security_checkpoint_result_to_string(SECURITY_CHECKPOINT_INSUFFICIENT_NODES));
    EXPECT_STREQ("tampered",
                 security_checkpoint_result_to_string(SECURITY_CHECKPOINT_TAMPERED));
    EXPECT_STREQ("unknown",
                 security_checkpoint_result_to_string((security_checkpoint_result_t)999));
}

/* =============================================================================
 * Bio-Async Tests
 * ============================================================================= */

class SecurityDistributedBioAsyncTest : public SecurityDistributedTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityDistributedTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_distributed_training_default_config(&config));
        config.enable_bio_async = true;
        bridge = security_distributed_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityDistributedBioAsyncTest, IsBioAsyncConnectedInitially) {
#ifndef NIMCP_BIO_ASYNC_ENABLED
    GTEST_SKIP() << "Bio-async router not available";
#endif
    /* May or may not be connected depending on router availability */
    (void)security_distributed_training_is_bio_async_connected(bridge);
}

TEST_F(SecurityDistributedBioAsyncTest, IsBioAsyncConnectedNull) {
    EXPECT_FALSE(security_distributed_training_is_bio_async_connected(nullptr));
}

TEST_F(SecurityDistributedBioAsyncTest, ConnectBioAsyncNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_connect_bio_async(nullptr));
}

TEST_F(SecurityDistributedBioAsyncTest, DisconnectBioAsyncNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_disconnect_bio_async(nullptr));
}

/* =============================================================================
 * Null Safety Tests
 * ============================================================================= */

TEST(SecurityDistributedNullSafetyTest, AllFunctionsHandleNull) {
    /* Lifecycle */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_default_config(nullptr));
    security_distributed_training_bridge_destroy(nullptr);  /* Should not crash */

    /* Connections */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_connect_coordinator(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_connect_bbb(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_disconnect_coordinator(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_disconnect_bbb(nullptr));

    /* Worker management */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_register_worker(nullptr, "test", nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_unregister_worker(nullptr, "test"));
    security_worker_info_t info;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_worker_info(nullptr, "test", &info));
    EXPECT_EQ(SECURITY_WORKER_TRUST_QUARANTINED,
              security_distributed_training_get_worker_trust(nullptr, "test"));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_set_worker_trust(
                  nullptr, "test", SECURITY_WORKER_TRUST_VERIFIED));
    float score;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_score_worker(nullptr, "test", &score));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_quarantine_worker(nullptr, "test", "reason"));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_release_worker(nullptr, "test"));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_list_workers(nullptr, nullptr, 0));

    /* Byzantine detection */
    security_byzantine_result_t byz_result;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_detect_byzantine(nullptr, &byz_result));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_report_worker_anomaly(
                  nullptr, "test", 0.5f, "reason"));

    /* Gradient validation */
    security_gradient_validation_result_t grad_result;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_validate_gradients(
                  nullptr, "test", nullptr, 0, &grad_result));
    float anomaly;
    EXPECT_FALSE(security_distributed_training_validate_aggregated(
        nullptr, nullptr, 0, &anomaly));
    EXPECT_EQ(SECURITY_GRAD_AGG_SIMPLE_AVERAGE,
              security_distributed_training_get_aggregation_method(nullptr));

    /* Checkpointing */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_secure_checkpoint(
                  nullptr, "test", nullptr, 0, 0));
    EXPECT_EQ(SECURITY_CHECKPOINT_TAMPERED,
              security_distributed_training_verify_checkpoint(nullptr, "test", nullptr, 0));
    security_distributed_checkpoint_t cp_info;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_checkpoint_info(nullptr, "test", &cp_info));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_list_checkpoints(nullptr, nullptr, 0));

    /* Bidirectional */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_update_security_effects(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_update_training_effects(nullptr, 0, 0.0f, 0));
    security_distributed_effects_t sec_effects;
    distributed_security_effects_t train_effects;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_security_effects(nullptr, &sec_effects));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_training_effects(nullptr, &train_effects));

    /* Bio-async */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_connect_bio_async(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_disconnect_bio_async(nullptr));
    EXPECT_FALSE(security_distributed_training_is_bio_async_connected(nullptr));

    /* Query */
    EXPECT_EQ(SECURITY_DISTRIBUTED_PHASE_INACTIVE,
              security_distributed_training_get_phase(nullptr));
    EXPECT_FLOAT_EQ(0.0f, security_distributed_training_get_threat_level(nullptr));
    EXPECT_FLOAT_EQ(0.0f, security_distributed_training_get_byzantine_ratio(nullptr));
    EXPECT_FALSE(security_distributed_training_is_under_attack(nullptr));

    /* Stats */
    security_distributed_training_stats_t stats;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_get_stats(nullptr, &stats));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER,
              security_distributed_training_reset_stats(nullptr));
}

/* =============================================================================
 * Constants Validation Tests
 * ============================================================================= */

TEST(SecurityDistributedConstantsTest, ThresholdsReasonable) {
    EXPECT_GT(SECURITY_DISTRIBUTED_DEFAULT_BYZANTINE_THRESHOLD, 0.0f);
    EXPECT_LE(SECURITY_DISTRIBUTED_DEFAULT_BYZANTINE_THRESHOLD, 1.0f);

    EXPECT_GT(SECURITY_DISTRIBUTED_DEFAULT_ANOMALY_THRESHOLD, 0.0f);
    EXPECT_LE(SECURITY_DISTRIBUTED_DEFAULT_ANOMALY_THRESHOLD, 1.0f);

    EXPECT_GT(SECURITY_DISTRIBUTED_DEFAULT_CONSISTENCY_THRESHOLD, 0.0f);
    EXPECT_LE(SECURITY_DISTRIBUTED_DEFAULT_CONSISTENCY_THRESHOLD, 1.0f);
}

TEST(SecurityDistributedConstantsTest, GradientThresholdsReasonable) {
    EXPECT_GT(SECURITY_DISTRIBUTED_DEFAULT_GRAD_NORM_THRESHOLD, 0.0f);
    EXPECT_GT(SECURITY_DISTRIBUTED_DEFAULT_GRAD_VARIANCE_THRESHOLD, 0.0f);
    EXPECT_GT(SECURITY_DISTRIBUTED_DEFAULT_GRAD_OUTLIER_THRESHOLD, 0.0f);
}

TEST(SecurityDistributedConstantsTest, TrustRatesReasonable) {
    EXPECT_GT(SECURITY_DISTRIBUTED_DEFAULT_TRUST_DECAY_RATE, 0.0f);
    EXPECT_LT(SECURITY_DISTRIBUTED_DEFAULT_TRUST_DECAY_RATE, 1.0f);

    EXPECT_GT(SECURITY_DISTRIBUTED_DEFAULT_TRUST_RECOVERY_RATE, 0.0f);
    EXPECT_LT(SECURITY_DISTRIBUTED_DEFAULT_TRUST_RECOVERY_RATE, 1.0f);

    EXPECT_GT(SECURITY_DISTRIBUTED_DEFAULT_TRUST_VIOLATION_PENALTY, 0.0f);
    EXPECT_LT(SECURITY_DISTRIBUTED_DEFAULT_TRUST_VIOLATION_PENALTY, 1.0f);
}

TEST(SecurityDistributedConstantsTest, LimitsReasonable) {
    EXPECT_GT(SECURITY_DISTRIBUTED_MAX_WORKERS, 0u);
    EXPECT_GT(SECURITY_DISTRIBUTED_WORKER_ID_MAX, 0u);
    EXPECT_GT(SECURITY_DISTRIBUTED_WORKER_KEY_SIZE, 0u);
    EXPECT_GT(SECURITY_DISTRIBUTED_CHECKPOINT_NAME_MAX, 0u);
    EXPECT_GT(SECURITY_DISTRIBUTED_MAX_CHECKPOINTS, 0u);
    EXPECT_GT(SECURITY_DISTRIBUTED_HASH_SIZE, 0u);
    EXPECT_GT(SECURITY_DISTRIBUTED_BIO_INBOX_CAPACITY, 0u);
}

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
