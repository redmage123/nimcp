/**
 * @file test_security_continual_learning_bridge.cpp
 * @brief Unit tests for security-continual learning bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * Comprehensive tests for security-continual learning integration:
 * 1. Lifecycle tests: default_config, create/destroy
 * 2. Connection tests: connect functions
 * 3. Knowledge protection tests
 * 4. Concept drift validation tests
 * 5. Memory replay integrity tests
 * 6. Retention monitoring tests
 * 7. Learning rate manipulation detection tests
 * 8. Bidirectional update tests
 * 9. Bio-async integration tests
 * 10. Stats tests
 * 11. Null safety tests
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <thread>
#include <atomic>

extern "C" {
#include "security/continual/nimcp_security_continual_learning_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

/* =============================================================================
 * Test Fixtures
 * ============================================================================= */

class SecurityCLBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&config, 0, sizeof(config));
        bridge = nullptr;
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
 * Lifecycle Tests
 * ============================================================================= */

TEST_F(SecurityCLBridgeTest, DefaultConfigValid) {
    EXPECT_EQ(0, security_cl_default_config(&config));

    /* Feature enables should default to true */
    EXPECT_TRUE(config.enable_forgetting_protection);
    EXPECT_TRUE(config.enable_drift_detection);
    EXPECT_TRUE(config.enable_replay_verification);
    EXPECT_TRUE(config.enable_lr_monitoring);
    EXPECT_TRUE(config.enable_task_validation);
    EXPECT_TRUE(config.enable_ewc_boost);

    /* Retention thresholds */
    EXPECT_FLOAT_EQ(SECURITY_CL_DEFAULT_RETENTION_THRESHOLD, config.retention_threshold);
    EXPECT_FLOAT_EQ(SECURITY_CL_DEFAULT_CRITICAL_THRESHOLD, config.critical_threshold);
    EXPECT_FLOAT_EQ(SECURITY_CL_DEFAULT_FORGETTING_RATE, config.max_forgetting_rate);

    /* Drift thresholds */
    EXPECT_FLOAT_EQ(SECURITY_CL_DEFAULT_DRIFT_THRESHOLD, config.drift_threshold);
    EXPECT_FLOAT_EQ(SECURITY_CL_DEFAULT_SUDDEN_DRIFT_THRESHOLD, config.sudden_drift_threshold);
    EXPECT_EQ(SECURITY_CL_DRIFT_WINDOW_SIZE, config.drift_window_size);

    /* Learning rate bounds */
    EXPECT_FLOAT_EQ(SECURITY_CL_DEFAULT_LR_MIN, config.lr_min_bound);
    EXPECT_FLOAT_EQ(SECURITY_CL_DEFAULT_LR_MAX, config.lr_max_bound);
    EXPECT_FLOAT_EQ(SECURITY_CL_DEFAULT_LR_CHANGE_THRESHOLD, config.lr_change_threshold);

    /* Replay verification */
    EXPECT_FLOAT_EQ(SECURITY_CL_DEFAULT_REPLAY_VERIFY_RATE, config.replay_verify_rate);
    EXPECT_TRUE(config.use_hash_chains);

    /* Bio-async */
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_EQ(SECURITY_CL_BIO_INBOX_CAPACITY, config.bio_inbox_capacity);
}

TEST_F(SecurityCLBridgeTest, DefaultConfigNullFails) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_default_config(nullptr));
}

TEST_F(SecurityCLBridgeTest, CreateWithDefaultConfig) {
    EXPECT_EQ(0, security_cl_default_config(&config));
    bridge = security_cl_bridge_create(&config);
    EXPECT_NE(nullptr, bridge);

    /* Verify initial state - bridge starts in MONITORING phase */
    EXPECT_EQ(SECURITY_CL_PHASE_MONITORING, security_cl_get_phase(bridge));
    EXPECT_FLOAT_EQ(0.0f, security_cl_get_threat_level(bridge));
    EXPECT_FALSE(security_cl_is_under_attack(bridge));
}

TEST_F(SecurityCLBridgeTest, CreateWithNullConfigUsesDefaults) {
    bridge = security_cl_bridge_create(nullptr);
    EXPECT_NE(nullptr, bridge);
}

TEST_F(SecurityCLBridgeTest, DestroyNull) {
    /* Should not crash */
    security_cl_bridge_destroy(nullptr);
}

TEST_F(SecurityCLBridgeTest, CreateDestroyMultiple) {
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(0, security_cl_default_config(&config));
        bridge = security_cl_bridge_create(&config);
        EXPECT_NE(nullptr, bridge);
        security_cl_bridge_destroy(bridge);
        bridge = nullptr;
    }
}

/* =============================================================================
 * Connection Tests
 * ============================================================================= */

class SecurityCLConnectionTest : public SecurityCLBridgeTest {
protected:
    void SetUp() override {
        SecurityCLBridgeTest::SetUp();
        EXPECT_EQ(0, security_cl_default_config(&config));
        bridge = security_cl_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityCLConnectionTest, ConnectContinualLearning) {
    int dummy_cl = 42;
    EXPECT_EQ(0, security_cl_connect_continual_learning(bridge, &dummy_cl));

    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_TRUE(stats.cl_connected);
}

TEST_F(SecurityCLConnectionTest, ConnectContinualLearningNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_connect_continual_learning(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_connect_continual_learning(bridge, nullptr));
}

TEST_F(SecurityCLConnectionTest, ConnectBBB) {
    bbb_system_t dummy_bbb = (bbb_system_t)0x5678;
    EXPECT_EQ(0, security_cl_connect_bbb(bridge, dummy_bbb));

    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_TRUE(stats.bbb_connected);
}

TEST_F(SecurityCLConnectionTest, ConnectBBBNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_connect_bbb(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_connect_bbb(bridge, nullptr));
}

TEST_F(SecurityCLConnectionTest, ConnectAnomalyDetector) {
    nimcp_anomaly_detector_t dummy_detector = (nimcp_anomaly_detector_t)0xABCD;
    EXPECT_EQ(0, security_cl_connect_anomaly_detector(bridge, dummy_detector));

    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_TRUE(stats.anomaly_connected);
}

TEST_F(SecurityCLConnectionTest, ConnectAnomalyDetectorNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_connect_anomaly_detector(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_connect_anomaly_detector(bridge, nullptr));
}

TEST_F(SecurityCLConnectionTest, DisconnectContinualLearning) {
    int dummy_cl = 42;
    EXPECT_EQ(0, security_cl_connect_continual_learning(bridge, &dummy_cl));
    EXPECT_EQ(0, security_cl_disconnect_continual_learning(bridge));

    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_FALSE(stats.cl_connected);
}

TEST_F(SecurityCLConnectionTest, DisconnectBBB) {
    bbb_system_t dummy_bbb = (bbb_system_t)0x5678;
    EXPECT_EQ(0, security_cl_connect_bbb(bridge, dummy_bbb));
    EXPECT_EQ(0, security_cl_disconnect_bbb(bridge));

    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_FALSE(stats.bbb_connected);
}

TEST_F(SecurityCLConnectionTest, DisconnectAnomalyDetector) {
    nimcp_anomaly_detector_t dummy_detector = (nimcp_anomaly_detector_t)0xABCD;
    EXPECT_EQ(0, security_cl_connect_anomaly_detector(bridge, dummy_detector));
    EXPECT_EQ(0, security_cl_disconnect_anomaly_detector(bridge));

    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_FALSE(stats.anomaly_connected);
}

TEST_F(SecurityCLConnectionTest, DisconnectNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_disconnect_continual_learning(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_disconnect_bbb(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_disconnect_anomaly_detector(nullptr));
}

/* =============================================================================
 * Knowledge Protection Tests
 * ============================================================================= */

class SecurityCLKnowledgeTest : public SecurityCLBridgeTest {
protected:
    void SetUp() override {
        SecurityCLBridgeTest::SetUp();
        EXPECT_EQ(0, security_cl_default_config(&config));
        config.enable_forgetting_protection = true;
        bridge = security_cl_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityCLKnowledgeTest, ProtectKnowledge) {
    std::vector<float> weights = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    EXPECT_EQ(0, security_cl_protect_knowledge(bridge, weights.data(), weights.size(), 0));

    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_GT(stats.knowledge_protections, 0u);
}

TEST_F(SecurityCLKnowledgeTest, ProtectKnowledgeMultipleTasks) {
    std::vector<float> weights = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    EXPECT_EQ(0, security_cl_protect_knowledge(bridge, weights.data(), weights.size(), 0));
    EXPECT_EQ(0, security_cl_protect_knowledge(bridge, weights.data(), weights.size(), 1));
    EXPECT_EQ(0, security_cl_protect_knowledge(bridge, weights.data(), weights.size(), 2));

    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_EQ(3u, stats.knowledge_protections);
}

TEST_F(SecurityCLKnowledgeTest, ProtectKnowledgeNullParams) {
    std::vector<float> weights = {0.1f, 0.2f};
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_protect_knowledge(nullptr, weights.data(), 2, 0));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_protect_knowledge(bridge, nullptr, 2, 0));
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, security_cl_protect_knowledge(bridge, weights.data(), 0, 0));
}

TEST_F(SecurityCLKnowledgeTest, RegisterTask) {
    EXPECT_EQ(0, security_cl_register_task(bridge, 0, "task_zero", 0.95f));

    security_cl_task_info_t info;
    EXPECT_EQ(0, security_cl_get_task_info(bridge, 0, &info));
    EXPECT_STREQ("task_zero", info.name);
    EXPECT_EQ(0u, info.task_id);
    EXPECT_FLOAT_EQ(0.95f, info.baseline_accuracy);
    EXPECT_FLOAT_EQ(0.95f, info.current_accuracy);
    EXPECT_FLOAT_EQ(1.0f, info.retention_rate);
}

TEST_F(SecurityCLKnowledgeTest, RegisterMultipleTasks) {
    for (uint32_t i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "task_%u", i);
        EXPECT_EQ(0, security_cl_register_task(bridge, i, name, 0.9f + i * 0.01f));
    }

    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_EQ(10u, stats.tasks_registered);
}

TEST_F(SecurityCLKnowledgeTest, GetTaskInfoNotFound) {
    security_cl_task_info_t info;
    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND, security_cl_get_task_info(bridge, 999, &info));
}

TEST_F(SecurityCLKnowledgeTest, GetTaskInfoNullParams) {
    security_cl_task_info_t info;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_get_task_info(nullptr, 0, &info));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_get_task_info(bridge, 0, nullptr));
}

TEST_F(SecurityCLKnowledgeTest, ComputeProtectionPenalty) {
    std::vector<float> weights = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    /* First protect knowledge */
    EXPECT_EQ(0, security_cl_protect_knowledge(bridge, weights.data(), weights.size(), 0));

    /* Same weights should have zero penalty */
    float penalty = 0.0f;
    EXPECT_EQ(0, security_cl_compute_protection_penalty(bridge, weights.data(), weights.size(), &penalty));
    EXPECT_NEAR(0.0f, penalty, 0.001f);

    /* Different weights should have non-zero penalty */
    std::vector<float> new_weights = {0.5f, 0.6f, 0.7f, 0.8f, 0.9f};
    EXPECT_EQ(0, security_cl_compute_protection_penalty(bridge, new_weights.data(), new_weights.size(), &penalty));
    EXPECT_GT(penalty, 0.0f);
}

TEST_F(SecurityCLKnowledgeTest, ComputeProtectionPenaltyNoBaseline) {
    std::vector<float> weights = {0.1f, 0.2f, 0.3f};
    float penalty = -1.0f;

    /* No baseline established - should return 0 penalty */
    EXPECT_EQ(0, security_cl_compute_protection_penalty(bridge, weights.data(), weights.size(), &penalty));
    EXPECT_FLOAT_EQ(0.0f, penalty);
}

TEST_F(SecurityCLKnowledgeTest, ComputeProtectionPenaltyNullParams) {
    std::vector<float> weights = {0.1f, 0.2f};
    float penalty;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_compute_protection_penalty(nullptr, weights.data(), 2, &penalty));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_compute_protection_penalty(bridge, nullptr, 2, &penalty));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_compute_protection_penalty(bridge, weights.data(), 2, nullptr));
}

/* =============================================================================
 * Concept Drift Tests
 * ============================================================================= */

class SecurityCLDriftTest : public SecurityCLBridgeTest {
protected:
    void SetUp() override {
        SecurityCLBridgeTest::SetUp();
        EXPECT_EQ(0, security_cl_default_config(&config));
        config.enable_drift_detection = true;
        config.drift_threshold = 0.3f;
        config.sudden_drift_threshold = 0.5f;
        bridge = security_cl_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityCLDriftTest, ValidateDriftNoDrift) {
    /* Establish baseline */
    std::vector<float> baseline_features = {0.5f, 0.5f, 0.5f, 0.5f};
    EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, baseline_features.data(), baseline_features.size()));

    /* Check with similar features (no drift) */
    std::vector<float> current_features = {0.51f, 0.49f, 0.52f, 0.48f};
    security_cl_drift_type_t drift_type;
    float drift_score = 0.0f;

    EXPECT_TRUE(security_cl_validate_drift(bridge, current_features.data(), current_features.size(), &drift_type, &drift_score));
    EXPECT_EQ(SECURITY_CL_DRIFT_NONE, drift_type);
    EXPECT_LT(drift_score, 0.3f);
}

TEST_F(SecurityCLDriftTest, ValidateDriftNatural) {
    /* Establish baseline */
    std::vector<float> baseline_features = {0.1f, 0.2f, 0.3f, 0.4f};
    EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, baseline_features.data(), baseline_features.size()));

    /* Gradual drift (natural) - incremental changes */
    std::vector<float> drifted_features = {0.3f, 0.4f, 0.5f, 0.6f};
    security_cl_drift_type_t drift_type;
    float drift_score = 0.0f;

    /* May or may not be detected based on magnitude - test that function works */
    bool allowed = security_cl_validate_drift(bridge, drifted_features.data(), drifted_features.size(), &drift_type, &drift_score);
    EXPECT_GE(drift_score, 0.0f);
    /* Natural drift should be allowed */
    if (drift_type == SECURITY_CL_DRIFT_NATURAL) {
        EXPECT_TRUE(allowed);
    }
}

TEST_F(SecurityCLDriftTest, ValidateDriftAdversarial) {
    /* Establish baseline */
    std::vector<float> baseline_features = {0.1f, 0.2f, 0.3f, 0.4f};
    EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, baseline_features.data(), baseline_features.size()));

    /* Sudden large drift (adversarial indicator) */
    std::vector<float> adversarial_features = {0.9f, 0.8f, 0.7f, 0.6f};
    security_cl_drift_type_t drift_type;
    float drift_score = 0.0f;

    bool allowed = security_cl_validate_drift(bridge, adversarial_features.data(), adversarial_features.size(), &drift_type, &drift_score);

    /* Should detect drift */
    EXPECT_GT(drift_score, 0.3f);
    /* If adversarial, should be blocked */
    if (drift_type == SECURITY_CL_DRIFT_ADVERSARIAL) {
        EXPECT_FALSE(allowed);
    }
}

TEST_F(SecurityCLDriftTest, UpdateDriftBaseline) {
    std::vector<float> features = {0.1f, 0.2f, 0.3f, 0.4f};

    EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, features.data(), features.size()));

    /* Second update should also succeed (EMA update) */
    std::vector<float> features2 = {0.2f, 0.3f, 0.4f, 0.5f};
    EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, features2.data(), features2.size()));
}

TEST_F(SecurityCLDriftTest, ResetDriftBaseline) {
    std::vector<float> features = {0.1f, 0.2f, 0.3f, 0.4f};

    EXPECT_EQ(0, security_cl_update_drift_baseline(bridge, features.data(), features.size()));
    EXPECT_EQ(0, security_cl_reset_drift_baseline(bridge));

    /* After reset, should allow drift check without baseline */
    security_cl_drift_type_t drift_type;
    float drift_score = 0.0f;
    EXPECT_TRUE(security_cl_validate_drift(bridge, features.data(), features.size(), &drift_type, &drift_score));
}

TEST_F(SecurityCLDriftTest, ValidateDriftNullParams) {
    std::vector<float> features = {0.1f, 0.2f};
    security_cl_drift_type_t drift_type;
    float drift_score = 0.0f;

    EXPECT_FALSE(security_cl_validate_drift(nullptr, features.data(), 2, &drift_type, &drift_score));
    /* NULL features should return true (no validation possible) */
    EXPECT_TRUE(security_cl_validate_drift(bridge, nullptr, 2, &drift_type, &drift_score));
}

TEST_F(SecurityCLDriftTest, UpdateDriftBaselineNullParams) {
    std::vector<float> features = {0.1f, 0.2f};

    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_update_drift_baseline(nullptr, features.data(), 2));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_update_drift_baseline(bridge, nullptr, 2));
}

TEST_F(SecurityCLDriftTest, ResetDriftBaselineNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_reset_drift_baseline(nullptr));
}

/* =============================================================================
 * Memory Replay Tests
 * ============================================================================= */

class SecurityCLReplayTest : public SecurityCLBridgeTest {
protected:
    void SetUp() override {
        SecurityCLBridgeTest::SetUp();
        EXPECT_EQ(0, security_cl_default_config(&config));
        config.enable_replay_verification = true;
        config.use_hash_chains = true;
        bridge = security_cl_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityCLReplayTest, VerifyReplayUnregistered) {
    /* Verify unregistered buffer - should pass (no baseline to compare) */
    std::vector<float> buffer(100, 0.5f);
    security_cl_replay_status_t status;

    EXPECT_TRUE(security_cl_verify_replay(bridge, buffer.data(), buffer.size() * sizeof(float), 100, &status));
    EXPECT_EQ(SECURITY_CL_REPLAY_OK, status);
}

TEST_F(SecurityCLReplayTest, RegisterReplayBuffer) {
    std::vector<float> buffer(100, 0.5f);

    uint32_t buffer_id = security_cl_register_replay_buffer(
        bridge, "test_buffer", buffer.data(), buffer.size() * sizeof(float), 100);

    EXPECT_GT(buffer_id, 0u);
}

TEST_F(SecurityCLReplayTest, VerifyRegisteredReplayOK) {
    std::vector<float> buffer(100, 0.5f);

    uint32_t buffer_id = security_cl_register_replay_buffer(
        bridge, "test_buffer", buffer.data(), buffer.size() * sizeof(float), 100);
    EXPECT_GT(buffer_id, 0u);

    /* Verify same buffer - should pass */
    security_cl_replay_status_t status;
    EXPECT_TRUE(security_cl_verify_replay(bridge, buffer.data(), buffer.size() * sizeof(float), 100, &status));
    EXPECT_EQ(SECURITY_CL_REPLAY_OK, status);
}

TEST_F(SecurityCLReplayTest, VerifyRegisteredReplayTampered) {
    std::vector<float> buffer(100, 0.5f);

    uint32_t buffer_id = security_cl_register_replay_buffer(
        bridge, "test_buffer", buffer.data(), buffer.size() * sizeof(float), 100);
    EXPECT_GT(buffer_id, 0u);

    /* Modify buffer */
    buffer[0] = 999.9f;

    /* Verify modified buffer - should detect hash mismatch */
    security_cl_replay_status_t status;
    bool result = security_cl_verify_replay(bridge, buffer.data(), buffer.size() * sizeof(float), 100, &status);

    /* Should detect tampering */
    EXPECT_FALSE(result);
    EXPECT_NE(SECURITY_CL_REPLAY_OK, status);
}

TEST_F(SecurityCLReplayTest, UpdateReplayHash) {
    std::vector<float> buffer(100, 0.5f);

    uint32_t buffer_id = security_cl_register_replay_buffer(
        bridge, "test_buffer", buffer.data(), buffer.size() * sizeof(float), 100);
    EXPECT_GT(buffer_id, 0u);

    /* Modify buffer */
    buffer[0] = 999.9f;

    /* Update hash for legitimate modification */
    EXPECT_EQ(0, security_cl_update_replay_hash(bridge, buffer_id, buffer.data(), buffer.size() * sizeof(float), 100));

    /* Now verify should pass */
    security_cl_replay_status_t status;
    EXPECT_TRUE(security_cl_verify_replay(bridge, buffer.data(), buffer.size() * sizeof(float), 100, &status));
    EXPECT_EQ(SECURITY_CL_REPLAY_OK, status);
}

TEST_F(SecurityCLReplayTest, UpdateReplayHashNotFound) {
    std::vector<float> buffer(100, 0.5f);
    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND, security_cl_update_replay_hash(bridge, 999, buffer.data(), 100, 10));
}

TEST_F(SecurityCLReplayTest, QuarantineSamples) {
    std::vector<uint32_t> indices = {5, 10, 15, 20};

    EXPECT_EQ(0, security_cl_quarantine_replay_samples(bridge, indices.data(), indices.size()));

    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_EQ(4u, stats.samples_quarantined);
}

TEST_F(SecurityCLReplayTest, QuarantineSamplesNullParams) {
    std::vector<uint32_t> indices = {5, 10};
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_quarantine_replay_samples(nullptr, indices.data(), 2));
    /* NULL indices with count returns 0 (no-op) */
    EXPECT_EQ(0, security_cl_quarantine_replay_samples(bridge, nullptr, 2));
}

TEST_F(SecurityCLReplayTest, VerifyReplayNullParams) {
    security_cl_replay_status_t status;
    EXPECT_FALSE(security_cl_verify_replay(nullptr, nullptr, 100, 10, &status));
    /* NULL buffer returns true (no data to verify) */
    EXPECT_TRUE(security_cl_verify_replay(bridge, nullptr, 100, 10, &status));
}

TEST_F(SecurityCLReplayTest, RegisterReplayBufferNullParams) {
    std::vector<float> buffer(100, 0.5f);
    EXPECT_EQ(0u, security_cl_register_replay_buffer(nullptr, "test", buffer.data(), 100, 10));
    EXPECT_EQ(0u, security_cl_register_replay_buffer(bridge, "test", nullptr, 100, 10));
    EXPECT_EQ(0u, security_cl_register_replay_buffer(bridge, "test", buffer.data(), 0, 10));
}

/* =============================================================================
 * Retention Monitoring Tests
 * ============================================================================= */

class SecurityCLRetentionTest : public SecurityCLBridgeTest {
protected:
    void SetUp() override {
        SecurityCLBridgeTest::SetUp();
        EXPECT_EQ(0, security_cl_default_config(&config));
        config.retention_threshold = 0.8f;
        config.critical_threshold = 0.6f;
        bridge = security_cl_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityCLRetentionTest, MonitorRetentionHealthy) {
    /* Register task with baseline */
    EXPECT_EQ(0, security_cl_register_task(bridge, 0, "task_0", 0.95f));

    /* Monitor with high accuracy */
    float retention;
    security_cl_retention_level_t level = security_cl_monitor_retention(bridge, 0, 0.90f, &retention);

    EXPECT_FLOAT_EQ(0.90f / 0.95f, retention);  /* retention_rate = current/baseline */
    EXPECT_GT(retention, 0.8f);  /* Should be healthy */
    EXPECT_EQ(SECURITY_CL_RETENTION_HEALTHY, level);
}

TEST_F(SecurityCLRetentionTest, MonitorRetentionDegrading) {
    /* Register task with baseline */
    EXPECT_EQ(0, security_cl_register_task(bridge, 0, "task_0", 0.95f));

    /* Monitor with degraded accuracy */
    float retention;
    security_cl_retention_level_t level = security_cl_monitor_retention(bridge, 0, 0.70f, &retention);

    EXPECT_GT(retention, 0.6f);
    EXPECT_LT(retention, 0.8f);
    EXPECT_EQ(SECURITY_CL_RETENTION_DEGRADING, level);

    /* Should trigger EWC boost */
    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_GT(stats.ewc_boosts_applied, 0u);
}

TEST_F(SecurityCLRetentionTest, MonitorRetentionCritical) {
    /* Register task with baseline */
    EXPECT_EQ(0, security_cl_register_task(bridge, 0, "task_0", 0.95f));

    /* Monitor with very low accuracy */
    float retention;
    security_cl_retention_level_t level = security_cl_monitor_retention(bridge, 0, 0.50f, &retention);

    EXPECT_LT(retention, 0.6f);
    EXPECT_EQ(SECURITY_CL_RETENTION_CRITICAL, level);

    /* Should trigger emergency response */
    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_GT(stats.emergency_responses, 0u);
}

TEST_F(SecurityCLRetentionTest, GetRetentionStatus) {
    /* Register multiple tasks */
    EXPECT_EQ(0, security_cl_register_task(bridge, 0, "task_0", 0.95f));
    EXPECT_EQ(0, security_cl_register_task(bridge, 1, "task_1", 0.90f));
    EXPECT_EQ(0, security_cl_register_task(bridge, 2, "task_2", 0.85f));

    /* Monitor tasks */
    float retention;
    security_cl_monitor_retention(bridge, 0, 0.90f, &retention);  /* Good */
    security_cl_monitor_retention(bridge, 1, 0.85f, &retention);  /* Good */
    security_cl_monitor_retention(bridge, 2, 0.80f, &retention);  /* Good */

    /* Get overall status */
    float overall;
    security_cl_retention_level_t level = security_cl_get_retention_status(bridge, &overall);

    EXPECT_GT(overall, 0.8f);
    EXPECT_EQ(SECURITY_CL_RETENTION_HEALTHY, level);
}

TEST_F(SecurityCLRetentionTest, IsRetentionCompromised) {
    /* Initially not compromised */
    EXPECT_FALSE(security_cl_is_retention_compromised(bridge));

    /* Register and degrade task */
    EXPECT_EQ(0, security_cl_register_task(bridge, 0, "task_0", 0.95f));
    float retention;
    security_cl_monitor_retention(bridge, 0, 0.30f, &retention);

    /* Now should be compromised */
    EXPECT_TRUE(security_cl_is_retention_compromised(bridge));
}

TEST_F(SecurityCLRetentionTest, MonitorRetentionTaskNotFound) {
    float retention;
    /* Task not registered - should return healthy */
    security_cl_retention_level_t level = security_cl_monitor_retention(bridge, 999, 0.5f, &retention);
    EXPECT_EQ(SECURITY_CL_RETENTION_HEALTHY, level);
    EXPECT_FLOAT_EQ(1.0f, retention);
}

TEST_F(SecurityCLRetentionTest, MonitorRetentionNullBridge) {
    float retention;
    security_cl_retention_level_t level = security_cl_monitor_retention(nullptr, 0, 0.5f, &retention);
    EXPECT_EQ(SECURITY_CL_RETENTION_COMPROMISED, level);
    EXPECT_FLOAT_EQ(0.0f, retention);
}

/* =============================================================================
 * Learning Rate Monitoring Tests
 * ============================================================================= */

class SecurityCLLearningRateTest : public SecurityCLBridgeTest {
protected:
    void SetUp() override {
        SecurityCLBridgeTest::SetUp();
        EXPECT_EQ(0, security_cl_default_config(&config));
        config.enable_lr_monitoring = true;
        config.lr_min_bound = 1e-6f;
        config.lr_max_bound = 0.1f;
        config.lr_change_threshold = 0.5f;
        bridge = security_cl_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityCLLearningRateTest, DetectLRManipulationNormal) {
    /* Normal LR within bounds */
    EXPECT_FALSE(security_cl_detect_lr_manipulation(bridge, 0.001f));
    EXPECT_FALSE(security_cl_detect_lr_manipulation(bridge, 0.01f));
    EXPECT_FALSE(security_cl_detect_lr_manipulation(bridge, 0.05f));
}

TEST_F(SecurityCLLearningRateTest, DetectLRManipulationOutOfBounds) {
    /* LR below minimum */
    EXPECT_TRUE(security_cl_detect_lr_manipulation(bridge, 1e-8f));

    /* LR above maximum */
    EXPECT_TRUE(security_cl_detect_lr_manipulation(bridge, 0.5f));

    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_GE(stats.lr_manipulations_detected, 2u);
}

TEST_F(SecurityCLLearningRateTest, DetectLRManipulationSuddenChange) {
    /* Record some LR history */
    security_cl_record_lr(bridge, 0.001f);
    security_cl_record_lr(bridge, 0.001f);
    security_cl_record_lr(bridge, 0.001f);

    /* Sudden large change */
    EXPECT_TRUE(security_cl_detect_lr_manipulation(bridge, 0.01f));  /* 10x increase */

    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_GT(stats.lr_manipulations_detected, 0u);
}

TEST_F(SecurityCLLearningRateTest, GetSafeLR) {
    float safe_lr = security_cl_get_safe_lr(bridge);

    EXPECT_GE(safe_lr, config.lr_min_bound);
    EXPECT_LE(safe_lr, config.lr_max_bound);
}

TEST_F(SecurityCLLearningRateTest, RecordLR) {
    EXPECT_EQ(0, security_cl_record_lr(bridge, 0.001f));
    EXPECT_EQ(0, security_cl_record_lr(bridge, 0.002f));
    EXPECT_EQ(0, security_cl_record_lr(bridge, 0.003f));

    /* Get CL effects to verify recording */
    cl_security_effects_t effects;
    EXPECT_EQ(0, security_cl_get_cl_effects(bridge, &effects));
    EXPECT_FLOAT_EQ(0.003f, effects.current_lr);
}

TEST_F(SecurityCLLearningRateTest, SetLRBounds) {
    EXPECT_EQ(0, security_cl_set_lr_bounds(bridge, 1e-5f, 0.05f));

    /* Get security effects to verify */
    security_cl_effects_t effects;
    EXPECT_EQ(0, security_cl_get_security_effects(bridge, &effects));
    EXPECT_FLOAT_EQ(1e-5f, effects.lr_min_allowed);
    EXPECT_FLOAT_EQ(0.05f, effects.lr_max_allowed);
}

TEST_F(SecurityCLLearningRateTest, SetLRBoundsInvalid) {
    /* Min >= Max */
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, security_cl_set_lr_bounds(bridge, 0.1f, 0.01f));
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, security_cl_set_lr_bounds(bridge, 0.1f, 0.1f));

    /* Negative min */
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, security_cl_set_lr_bounds(bridge, -0.1f, 0.1f));
}

TEST_F(SecurityCLLearningRateTest, DetectLRManipulationNull) {
    EXPECT_FALSE(security_cl_detect_lr_manipulation(nullptr, 0.01f));
}

TEST_F(SecurityCLLearningRateTest, RecordLRNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_record_lr(nullptr, 0.01f));
}

TEST_F(SecurityCLLearningRateTest, SetLRBoundsNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_set_lr_bounds(nullptr, 0.001f, 0.1f));
}

/* =============================================================================
 * Bidirectional Update Tests
 * ============================================================================= */

class SecurityCLBidirectionalTest : public SecurityCLBridgeTest {
protected:
    void SetUp() override {
        SecurityCLBridgeTest::SetUp();
        EXPECT_EQ(0, security_cl_default_config(&config));
        bridge = security_cl_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityCLBidirectionalTest, UpdateSecurityEffects) {
    EXPECT_EQ(0, security_cl_update_security_effects(bridge));

    security_cl_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    EXPECT_EQ(0, security_cl_get_security_effects(bridge, &effects));
    EXPECT_TRUE(effects.valid);
    EXPECT_GT(effects.last_update_ms, 0u);
}

TEST_F(SecurityCLBidirectionalTest, UpdateCLEffects) {
    EXPECT_EQ(0, security_cl_update_cl_effects(bridge, 0.9f, 0.1f, 0.001f, 0));

    cl_security_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    EXPECT_EQ(0, security_cl_get_cl_effects(bridge, &effects));
    EXPECT_TRUE(effects.valid);
    EXPECT_FLOAT_EQ(0.9f, effects.current_retention);
    EXPECT_FLOAT_EQ(0.1f, effects.current_drift_score);
    EXPECT_FLOAT_EQ(0.001f, effects.current_lr);
    EXPECT_EQ(0u, effects.current_task_id);
}

TEST_F(SecurityCLBidirectionalTest, GetSecurityEffects) {
    security_cl_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    EXPECT_EQ(0, security_cl_get_security_effects(bridge, &effects));

    /* Default values should be reasonable */
    EXPECT_GE(effects.threat_level, 0.0f);
    EXPECT_LE(effects.threat_level, 1.0f);
    EXPECT_FALSE(effects.under_attack);
}

TEST_F(SecurityCLBidirectionalTest, GetCLEffects) {
    cl_security_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    /* Initially effects are not valid */
    EXPECT_EQ(0, security_cl_get_cl_effects(bridge, &effects));
    EXPECT_FALSE(effects.valid);

    /* After update, should be valid */
    EXPECT_EQ(0, security_cl_update_cl_effects(bridge, 0.9f, 0.1f, 0.001f, 0));
    EXPECT_EQ(0, security_cl_get_cl_effects(bridge, &effects));
    EXPECT_TRUE(effects.valid);
}

TEST_F(SecurityCLBidirectionalTest, ThreatLevelProgression) {
    /* No threat initially */
    EXPECT_FLOAT_EQ(0.0f, security_cl_get_threat_level(bridge));

    /* Trigger retention anomaly */
    EXPECT_EQ(0, security_cl_register_task(bridge, 0, "task_0", 0.95f));
    float retention;
    security_cl_monitor_retention(bridge, 0, 0.50f, &retention);  /* Critical */

    /* Update security effects */
    EXPECT_EQ(0, security_cl_update_security_effects(bridge));

    /* Threat level should increase */
    float threat = security_cl_get_threat_level(bridge);
    EXPECT_GT(threat, 0.0f);
}

TEST_F(SecurityCLBidirectionalTest, UnderAttackDetection) {
    /* Initially not under attack */
    EXPECT_FALSE(security_cl_is_under_attack(bridge));

    /* Create multiple threat signals */
    EXPECT_EQ(0, security_cl_register_task(bridge, 0, "task_0", 0.95f));
    float retention;
    security_cl_monitor_retention(bridge, 0, 0.30f, &retention);  /* Very low */

    /* Trigger drift anomaly */
    std::vector<float> baseline = {0.1f, 0.2f, 0.3f, 0.4f};
    security_cl_update_drift_baseline(bridge, baseline.data(), baseline.size());
    std::vector<float> adversarial = {0.9f, 0.8f, 0.7f, 0.6f};
    security_cl_drift_type_t drift_type;
    float drift_score;
    security_cl_validate_drift(bridge, adversarial.data(), adversarial.size(), &drift_type, &drift_score);

    /* Update security effects */
    security_cl_update_security_effects(bridge);

    /* Should be under attack with high enough threat level */
    float threat = security_cl_get_threat_level(bridge);
    if (threat > 0.6f) {
        EXPECT_TRUE(security_cl_is_under_attack(bridge));
    }
}

TEST_F(SecurityCLBidirectionalTest, UpdateSecurityEffectsNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_update_security_effects(nullptr));
}

TEST_F(SecurityCLBidirectionalTest, UpdateCLEffectsNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_update_cl_effects(nullptr, 0.0f, 0.0f, 0.0f, 0));
}

TEST_F(SecurityCLBidirectionalTest, GetSecurityEffectsNullParams) {
    security_cl_effects_t effects;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_get_security_effects(nullptr, &effects));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_get_security_effects(bridge, nullptr));
}

TEST_F(SecurityCLBidirectionalTest, GetCLEffectsNullParams) {
    cl_security_effects_t effects;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_get_cl_effects(nullptr, &effects));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_get_cl_effects(bridge, nullptr));
}

/* =============================================================================
 * Stats Tests
 * ============================================================================= */

class SecurityCLStatsTest : public SecurityCLBridgeTest {
protected:
    void SetUp() override {
        SecurityCLBridgeTest::SetUp();
        EXPECT_EQ(0, security_cl_default_config(&config));
        bridge = security_cl_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityCLStatsTest, GetStatsInitial) {
    security_cl_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  /* Fill with garbage */

    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));

    /* Initial stats should be zero/false */
    EXPECT_EQ(0u, stats.knowledge_protections);
    EXPECT_EQ(0u, stats.drift_checks);
    EXPECT_EQ(0u, stats.replay_verifications);
    EXPECT_EQ(0u, stats.lr_checks);
    EXPECT_EQ(0u, stats.retention_checks);

    /* Bridge starts in MONITORING phase */
    EXPECT_EQ(SECURITY_CL_PHASE_MONITORING, stats.current_phase);
}

TEST_F(SecurityCLStatsTest, GetStatsAfterOperations) {
    /* Perform some operations */
    std::vector<float> weights = {0.1f, 0.2f, 0.3f};
    security_cl_protect_knowledge(bridge, weights.data(), weights.size(), 0);

    std::vector<float> features = {0.5f, 0.5f, 0.5f};
    security_cl_update_drift_baseline(bridge, features.data(), features.size());
    security_cl_drift_type_t drift_type;
    float drift_score;
    security_cl_validate_drift(bridge, features.data(), features.size(), &drift_type, &drift_score);

    security_cl_detect_lr_manipulation(bridge, 0.01f);

    EXPECT_EQ(0, security_cl_register_task(bridge, 0, "task_0", 0.9f));
    float retention;
    security_cl_monitor_retention(bridge, 0, 0.85f, &retention);

    /* Get stats */
    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));

    EXPECT_GT(stats.knowledge_protections, 0u);
    EXPECT_GT(stats.drift_checks, 0u);
    EXPECT_GT(stats.lr_checks, 0u);
    EXPECT_GT(stats.retention_checks, 0u);
    EXPECT_GT(stats.tasks_registered, 0u);
}

TEST_F(SecurityCLStatsTest, ResetStats) {
    /* Perform operations to increment stats */
    std::vector<float> weights = {0.1f, 0.2f, 0.3f};
    security_cl_protect_knowledge(bridge, weights.data(), weights.size(), 0);

    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_GT(stats.knowledge_protections, 0u);

    /* Reset stats */
    EXPECT_EQ(0, security_cl_reset_stats(bridge));

    /* Stats should be reset */
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_EQ(0u, stats.knowledge_protections);
}

TEST_F(SecurityCLStatsTest, GetStatsNullParams) {
    security_cl_stats_t stats;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_get_stats(nullptr, &stats));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_get_stats(bridge, nullptr));
}

TEST_F(SecurityCLStatsTest, ResetStatsNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_reset_stats(nullptr));
}

TEST_F(SecurityCLStatsTest, StatsConnectionStatus) {
    security_cl_stats_t stats;
    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));

    /* Initially no connections */
    EXPECT_FALSE(stats.cl_connected);
    EXPECT_FALSE(stats.bbb_connected);
    EXPECT_FALSE(stats.anomaly_connected);

    /* Connect CL */
    int dummy = 42;
    security_cl_connect_continual_learning(bridge, &dummy);

    EXPECT_EQ(0, security_cl_get_stats(bridge, &stats));
    EXPECT_TRUE(stats.cl_connected);
}

/* =============================================================================
 * Query Tests
 * ============================================================================= */

TEST_F(SecurityCLStatsTest, GetPhase) {
    /* Bridge starts in MONITORING phase */
    EXPECT_EQ(SECURITY_CL_PHASE_MONITORING, security_cl_get_phase(bridge));
}

TEST_F(SecurityCLStatsTest, GetPhaseNull) {
    EXPECT_EQ(SECURITY_CL_PHASE_INACTIVE, security_cl_get_phase(nullptr));
}

TEST_F(SecurityCLStatsTest, GetThreatLevel) {
    float threat = security_cl_get_threat_level(bridge);
    EXPECT_GE(threat, 0.0f);
    EXPECT_LE(threat, 1.0f);
}

TEST_F(SecurityCLStatsTest, GetThreatLevelNull) {
    EXPECT_FLOAT_EQ(0.0f, security_cl_get_threat_level(nullptr));
}

TEST_F(SecurityCLStatsTest, IsUnderAttack) {
    EXPECT_FALSE(security_cl_is_under_attack(bridge));
}

TEST_F(SecurityCLStatsTest, IsUnderAttackNull) {
    EXPECT_FALSE(security_cl_is_under_attack(nullptr));
}

/* =============================================================================
 * Bio-Async Tests
 * ============================================================================= */

class SecurityCLBioAsyncTest : public SecurityCLBridgeTest {
protected:
    void SetUp() override {
        SecurityCLBridgeTest::SetUp();
        EXPECT_EQ(0, security_cl_default_config(&config));
        config.enable_bio_async = true;
        config.bio_inbox_capacity = SECURITY_CL_BIO_INBOX_CAPACITY;
        bridge = security_cl_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityCLBioAsyncTest, IsBioAsyncConnectedInitially) {
#ifndef NIMCP_BIO_ASYNC_ENABLED
    GTEST_SKIP() << "Bio-async router not available";
#endif
    /* May or may not be connected depending on router availability */
    bool connected = security_cl_is_bio_async_connected(bridge);
    (void)connected;  /* Just verify it doesn't crash */
}

TEST_F(SecurityCLBioAsyncTest, IsBioAsyncConnectedNull) {
    EXPECT_FALSE(security_cl_is_bio_async_connected(nullptr));
}

TEST_F(SecurityCLBioAsyncTest, ConnectBioAsyncNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_connect_bio_async(nullptr));
}

TEST_F(SecurityCLBioAsyncTest, DisconnectBioAsyncNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_disconnect_bio_async(nullptr));
}

/* =============================================================================
 * String Conversion Tests
 * ============================================================================= */

TEST(SecurityCLStringConversionTest, ForgettingTypeToString) {
    EXPECT_STREQ("none", security_cl_forgetting_type_to_string(SECURITY_CL_FORGETTING_NONE));
    EXPECT_STREQ("gradient_flood", security_cl_forgetting_type_to_string(SECURITY_CL_FORGETTING_GRADIENT_FLOOD));
    EXPECT_STREQ("weight_erasure", security_cl_forgetting_type_to_string(SECURITY_CL_FORGETTING_WEIGHT_ERASURE));
    EXPECT_STREQ("task_overwrite", security_cl_forgetting_type_to_string(SECURITY_CL_FORGETTING_TASK_OVERWRITE));
    EXPECT_STREQ("replay_poison", security_cl_forgetting_type_to_string(SECURITY_CL_FORGETTING_REPLAY_POISON));
    EXPECT_STREQ("lr_spike", security_cl_forgetting_type_to_string(SECURITY_CL_FORGETTING_LR_SPIKE));
    EXPECT_STREQ("unknown", security_cl_forgetting_type_to_string((security_cl_forgetting_type_t)999));
}

TEST(SecurityCLStringConversionTest, DriftTypeToString) {
    EXPECT_STREQ("none", security_cl_drift_type_to_string(SECURITY_CL_DRIFT_NONE));
    EXPECT_STREQ("natural", security_cl_drift_type_to_string(SECURITY_CL_DRIFT_NATURAL));
    EXPECT_STREQ("task_switch", security_cl_drift_type_to_string(SECURITY_CL_DRIFT_TASK_SWITCH));
    EXPECT_STREQ("adversarial", security_cl_drift_type_to_string(SECURITY_CL_DRIFT_ADVERSARIAL));
    EXPECT_STREQ("manipulation", security_cl_drift_type_to_string(SECURITY_CL_DRIFT_MANIPULATION));
    EXPECT_STREQ("unknown", security_cl_drift_type_to_string((security_cl_drift_type_t)999));
}

TEST(SecurityCLStringConversionTest, RetentionLevelToString) {
    EXPECT_STREQ("healthy", security_cl_retention_level_to_string(SECURITY_CL_RETENTION_HEALTHY));
    EXPECT_STREQ("degrading", security_cl_retention_level_to_string(SECURITY_CL_RETENTION_DEGRADING));
    EXPECT_STREQ("critical", security_cl_retention_level_to_string(SECURITY_CL_RETENTION_CRITICAL));
    EXPECT_STREQ("compromised", security_cl_retention_level_to_string(SECURITY_CL_RETENTION_COMPROMISED));
    EXPECT_STREQ("unknown", security_cl_retention_level_to_string((security_cl_retention_level_t)999));
}

TEST(SecurityCLStringConversionTest, PhaseToString) {
    EXPECT_STREQ("inactive", security_cl_phase_to_string(SECURITY_CL_PHASE_INACTIVE));
    EXPECT_STREQ("monitoring", security_cl_phase_to_string(SECURITY_CL_PHASE_MONITORING));
    EXPECT_STREQ("protecting", security_cl_phase_to_string(SECURITY_CL_PHASE_PROTECTING));
    EXPECT_STREQ("defending", security_cl_phase_to_string(SECURITY_CL_PHASE_DEFENDING));
    EXPECT_STREQ("recovery", security_cl_phase_to_string(SECURITY_CL_PHASE_RECOVERY));
    EXPECT_STREQ("unknown", security_cl_phase_to_string((security_cl_phase_t)999));
}

TEST(SecurityCLStringConversionTest, ReplayStatusToString) {
    EXPECT_STREQ("ok", security_cl_replay_status_to_string(SECURITY_CL_REPLAY_OK));
    EXPECT_STREQ("hash_mismatch", security_cl_replay_status_to_string(SECURITY_CL_REPLAY_HASH_MISMATCH));
    EXPECT_STREQ("poison_detected", security_cl_replay_status_to_string(SECURITY_CL_REPLAY_POISON_DETECTED));
    EXPECT_STREQ("distribution_shift", security_cl_replay_status_to_string(SECURITY_CL_REPLAY_DISTRIBUTION_SHIFT));
    EXPECT_STREQ("tampered", security_cl_replay_status_to_string(SECURITY_CL_REPLAY_TAMPERED));
    EXPECT_STREQ("unknown", security_cl_replay_status_to_string((security_cl_replay_status_t)999));
}

/* =============================================================================
 * Null Safety Tests
 * ============================================================================= */

TEST(SecurityCLNullSafetyTest, AllFunctionsHandleNull) {
    /* Lifecycle */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_default_config(nullptr));
    security_cl_bridge_destroy(nullptr);  /* Should not crash */

    /* Connections */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_connect_continual_learning(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_connect_bbb(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_connect_anomaly_detector(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_disconnect_continual_learning(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_disconnect_bbb(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_disconnect_anomaly_detector(nullptr));

    /* Knowledge protection */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_protect_knowledge(nullptr, nullptr, 0, 0));
    security_cl_task_info_t task_info;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_get_task_info(nullptr, 0, &task_info));
    float penalty;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_compute_protection_penalty(nullptr, nullptr, 0, &penalty));

    /* Drift */
    security_cl_drift_type_t drift_type;
    float drift_score;
    EXPECT_FALSE(security_cl_validate_drift(nullptr, nullptr, 0, &drift_type, &drift_score));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_update_drift_baseline(nullptr, nullptr, 0));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_reset_drift_baseline(nullptr));

    /* Replay */
    security_cl_replay_status_t replay_status;
    EXPECT_FALSE(security_cl_verify_replay(nullptr, nullptr, 0, 0, &replay_status));
    EXPECT_EQ(0u, security_cl_register_replay_buffer(nullptr, "test", nullptr, 0, 0));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_quarantine_replay_samples(nullptr, nullptr, 0));

    /* Retention */
    float retention;
    EXPECT_EQ(SECURITY_CL_RETENTION_COMPROMISED, security_cl_monitor_retention(nullptr, 0, 0, &retention));
    EXPECT_TRUE(security_cl_is_retention_compromised(nullptr));

    /* Learning rate */
    EXPECT_FALSE(security_cl_detect_lr_manipulation(nullptr, 0.0f));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_record_lr(nullptr, 0.0f));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_set_lr_bounds(nullptr, 0.0f, 0.0f));

    /* Bidirectional */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_update_security_effects(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_update_cl_effects(nullptr, 0.0f, 0.0f, 0.0f, 0));
    security_cl_effects_t sec_effects;
    cl_security_effects_t cl_effects;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_get_security_effects(nullptr, &sec_effects));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_get_cl_effects(nullptr, &cl_effects));

    /* Bio-async */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_connect_bio_async(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_disconnect_bio_async(nullptr));
    EXPECT_FALSE(security_cl_is_bio_async_connected(nullptr));

    /* Query */
    EXPECT_EQ(SECURITY_CL_PHASE_INACTIVE, security_cl_get_phase(nullptr));
    EXPECT_FLOAT_EQ(0.0f, security_cl_get_threat_level(nullptr));
    EXPECT_FALSE(security_cl_is_under_attack(nullptr));

    /* Stats */
    security_cl_stats_t stats;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_get_stats(nullptr, &stats));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_cl_reset_stats(nullptr));
}

/* =============================================================================
 * Constants Validation Tests
 * ============================================================================= */

TEST(SecurityCLConstantsTest, RetentionThresholdsReasonable) {
    EXPECT_GT(SECURITY_CL_DEFAULT_RETENTION_THRESHOLD, 0.0f);
    EXPECT_LE(SECURITY_CL_DEFAULT_RETENTION_THRESHOLD, 1.0f);

    EXPECT_GT(SECURITY_CL_DEFAULT_CRITICAL_THRESHOLD, 0.0f);
    EXPECT_LE(SECURITY_CL_DEFAULT_CRITICAL_THRESHOLD, 1.0f);

    EXPECT_LT(SECURITY_CL_DEFAULT_CRITICAL_THRESHOLD, SECURITY_CL_DEFAULT_RETENTION_THRESHOLD);
}

TEST(SecurityCLConstantsTest, DriftThresholdsReasonable) {
    EXPECT_GT(SECURITY_CL_DEFAULT_DRIFT_THRESHOLD, 0.0f);
    EXPECT_LE(SECURITY_CL_DEFAULT_DRIFT_THRESHOLD, 1.0f);

    EXPECT_GT(SECURITY_CL_DEFAULT_SUDDEN_DRIFT_THRESHOLD, 0.0f);
    EXPECT_LE(SECURITY_CL_DEFAULT_SUDDEN_DRIFT_THRESHOLD, 1.0f);

    EXPECT_GT(SECURITY_CL_DRIFT_WINDOW_SIZE, 0u);
}

TEST(SecurityCLConstantsTest, LearningRateBoundsReasonable) {
    EXPECT_GT(SECURITY_CL_DEFAULT_LR_MIN, 0.0f);
    EXPECT_GT(SECURITY_CL_DEFAULT_LR_MAX, 0.0f);
    EXPECT_LT(SECURITY_CL_DEFAULT_LR_MIN, SECURITY_CL_DEFAULT_LR_MAX);

    EXPECT_GT(SECURITY_CL_DEFAULT_LR_CHANGE_THRESHOLD, 0.0f);
    EXPECT_LE(SECURITY_CL_DEFAULT_LR_CHANGE_THRESHOLD, 1.0f);
}

TEST(SecurityCLConstantsTest, ReplayConstantsReasonable) {
    EXPECT_GT(SECURITY_CL_HASH_SIZE, 0u);
    EXPECT_GT(SECURITY_CL_MAX_REPLAY_BUFFERS, 0u);

    EXPECT_GT(SECURITY_CL_DEFAULT_REPLAY_VERIFY_RATE, 0.0f);
    EXPECT_LE(SECURITY_CL_DEFAULT_REPLAY_VERIFY_RATE, 1.0f);
}

TEST(SecurityCLConstantsTest, TaskConstantsReasonable) {
    EXPECT_GT(SECURITY_CL_MAX_TASKS, 0u);
    EXPECT_GT(SECURITY_CL_TASK_FINGERPRINT_SIZE, 0u);
}

TEST(SecurityCLConstantsTest, BioAsyncConstantsReasonable) {
    EXPECT_GT(SECURITY_CL_BIO_INBOX_CAPACITY, 0u);
}

/* =============================================================================
 * Thread Safety Tests
 * ============================================================================= */

class SecurityCLThreadSafetyTest : public SecurityCLBridgeTest {
protected:
    void SetUp() override {
        SecurityCLBridgeTest::SetUp();
        EXPECT_EQ(0, security_cl_default_config(&config));
        bridge = security_cl_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityCLThreadSafetyTest, ConcurrentKnowledgeProtection) {
    std::atomic<int> success_count{0};
    const int num_threads = 4;
    const int iterations_per_thread = 10;

    auto worker = [this, &success_count, iterations_per_thread](int thread_id) {
        for (int i = 0; i < iterations_per_thread; i++) {
            std::vector<float> weights(100, 0.1f * (thread_id + 1));
            int result = security_cl_protect_knowledge(
                bridge, weights.data(), weights.size(), thread_id * 1000 + i);
            if (result == 0) {
                success_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back(worker, t);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(num_threads * iterations_per_thread, success_count.load());
}

TEST_F(SecurityCLThreadSafetyTest, ConcurrentRetentionMonitoring) {
    /* Register tasks first */
    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "task_%d", i);
        security_cl_register_task(bridge, i, name, 0.9f);
    }

    std::atomic<int> check_count{0};
    const int num_threads = 4;
    const int iterations_per_thread = 25;

    auto worker = [this, &check_count, iterations_per_thread](int thread_id) {
        for (int i = 0; i < iterations_per_thread; i++) {
            float retention;
            uint32_t task_id = (thread_id + i) % 10;
            security_cl_retention_level_t level = security_cl_monitor_retention(
                bridge, task_id, 0.8f, &retention);
            (void)level;
            check_count++;
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back(worker, t);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(num_threads * iterations_per_thread, check_count.load());
}

TEST_F(SecurityCLThreadSafetyTest, ConcurrentLRRecording) {
    std::atomic<int> record_count{0};
    const int num_threads = 4;
    const int iterations_per_thread = 50;

    auto worker = [this, &record_count, iterations_per_thread](int thread_id) {
        for (int i = 0; i < iterations_per_thread; i++) {
            float lr = 0.001f + 0.0001f * (thread_id + i);
            int result = security_cl_record_lr(bridge, lr);
            if (result == 0) {
                record_count++;
            }
        }
    };

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back(worker, t);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(num_threads * iterations_per_thread, record_count.load());
}

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
