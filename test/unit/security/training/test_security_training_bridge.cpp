/**
 * @file test_security_training_bridge.cpp
 * @brief Unit tests for security-training bridge
 * @version 1.0.0
 * @date 2026-01-09
 *
 * Comprehensive tests for security-training integration:
 * 1. Lifecycle tests: default_config, create/destroy
 * 2. Connection tests: connect functions
 * 3. Data source validation tests
 * 4. Poisoning detection tests
 * 5. Gradient sanitization tests
 * 6. Model integrity tests
 * 7. Checkpoint tests
 * 8. Concept drift tests
 * 9. Bidirectional update tests
 * 10. Stats tests
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "security/training/nimcp_security_training_bridge.h"
#include "utils/error/nimcp_error_codes.h"
}

/* =============================================================================
 * Test Fixtures
 * ============================================================================= */

class SecurityTrainingBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&config, 0, sizeof(config));
        bridge = nullptr;
    }

    void TearDown() override {
        if (bridge) {
            security_training_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    security_training_config_t config;
    security_training_bridge_t* bridge;
};

/* =============================================================================
 * Lifecycle Tests
 * ============================================================================= */

TEST_F(SecurityTrainingBridgeTest, DefaultConfigValid) {
    EXPECT_EQ(0, security_training_default_config(&config));

    /* Feature enables should default to true */
    EXPECT_TRUE(config.enable_data_validation);
    EXPECT_TRUE(config.enable_poisoning_detection);
    EXPECT_TRUE(config.enable_gradient_sanitization);
    EXPECT_TRUE(config.enable_model_verification);
    EXPECT_TRUE(config.enable_concept_drift_detection);
    EXPECT_TRUE(config.enable_secure_checkpointing);

    /* Gradient defaults */
    EXPECT_FLOAT_EQ(SECURITY_TRAINING_DEFAULT_GRAD_CLIP_NORM, config.gradient_clip_norm);
    EXPECT_FLOAT_EQ(SECURITY_TRAINING_DEFAULT_GRAD_CLIP_VALUE, config.gradient_clip_value);
    EXPECT_FLOAT_EQ(SECURITY_TRAINING_DEFAULT_GRAD_MIN_VALUE, config.gradient_min_bound);
    EXPECT_FLOAT_EQ(SECURITY_TRAINING_DEFAULT_GRAD_MAX_VALUE, config.gradient_max_bound);

    /* Poisoning detection defaults */
    EXPECT_FLOAT_EQ(SECURITY_TRAINING_DEFAULT_LABEL_FLIP_THRESHOLD, config.label_flip_threshold);
    EXPECT_FLOAT_EQ(SECURITY_TRAINING_DEFAULT_BACKDOOR_THRESHOLD, config.backdoor_threshold);
    EXPECT_FLOAT_EQ(SECURITY_TRAINING_DEFAULT_TROJAN_THRESHOLD, config.trojan_threshold);
    EXPECT_FLOAT_EQ(SECURITY_TRAINING_DEFAULT_GRADIENT_ANOMALY_THRESHOLD, config.gradient_anomaly_threshold);

    /* Concept drift defaults */
    EXPECT_EQ(SECURITY_TRAINING_DRIFT_WINDOW_SIZE, config.drift_window_size);
    EXPECT_FLOAT_EQ(SECURITY_TRAINING_DEFAULT_DRIFT_THRESHOLD, config.drift_threshold);
}

TEST_F(SecurityTrainingBridgeTest, DefaultConfigNullFails) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_default_config(nullptr));
}

TEST_F(SecurityTrainingBridgeTest, CreateWithDefaultConfig) {
    EXPECT_EQ(0, security_training_default_config(&config));
    bridge = security_training_bridge_create(&config);
    EXPECT_NE(nullptr, bridge);

    /* Verify initial state - bridge starts in MONITORING phase */
    EXPECT_EQ(SECURITY_TRAINING_PHASE_MONITORING, security_training_get_phase(bridge));
    EXPECT_FLOAT_EQ(0.0f, security_training_get_threat_level(bridge));
    EXPECT_FALSE(security_training_is_under_attack(bridge));
}

TEST_F(SecurityTrainingBridgeTest, CreateWithNullConfigUsesDefaults) {
    bridge = security_training_bridge_create(nullptr);
    EXPECT_NE(nullptr, bridge);
}

TEST_F(SecurityTrainingBridgeTest, DestroyNull) {
    /* Should not crash */
    security_training_bridge_destroy(nullptr);
}

TEST_F(SecurityTrainingBridgeTest, CreateDestroyMultiple) {
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(0, security_training_default_config(&config));
        bridge = security_training_bridge_create(&config);
        EXPECT_NE(nullptr, bridge);
        security_training_bridge_destroy(bridge);
        bridge = nullptr;
    }
}

/* =============================================================================
 * Connection Tests
 * ============================================================================= */

class SecurityTrainingConnectionTest : public SecurityTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_training_default_config(&config));
        bridge = security_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityTrainingConnectionTest, ConnectTrainingPipeline) {
    int dummy_training = 42;
    EXPECT_EQ(0, security_training_connect_training_pipeline(bridge, &dummy_training));

    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_TRUE(stats.training_connected);
}

TEST_F(SecurityTrainingConnectionTest, ConnectTrainingPipelineNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_connect_training_pipeline(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_connect_training_pipeline(bridge, nullptr));
}

TEST_F(SecurityTrainingConnectionTest, ConnectOptimizer) {
    nimcp_optimizer_context_t* dummy_optimizer = (nimcp_optimizer_context_t*)0x1234;
    EXPECT_EQ(0, security_training_connect_optimizer(bridge, dummy_optimizer));

    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_TRUE(stats.optimizer_connected);
}

TEST_F(SecurityTrainingConnectionTest, ConnectOptimizerNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_connect_optimizer(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_connect_optimizer(bridge, nullptr));
}

TEST_F(SecurityTrainingConnectionTest, ConnectBBB) {
    bbb_system_t dummy_bbb = (bbb_system_t)0x5678;
    EXPECT_EQ(0, security_training_connect_bbb(bridge, dummy_bbb));

    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_TRUE(stats.bbb_connected);
}

TEST_F(SecurityTrainingConnectionTest, ConnectBBBNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_connect_bbb(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_connect_bbb(bridge, nullptr));
}

TEST_F(SecurityTrainingConnectionTest, ConnectAnomalyDetector) {
    nimcp_anomaly_detector_t dummy_detector = (nimcp_anomaly_detector_t)0xABCD;
    EXPECT_EQ(0, security_training_connect_anomaly_detector(bridge, dummy_detector));

    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_TRUE(stats.anomaly_connected);
}

TEST_F(SecurityTrainingConnectionTest, ConnectAnomalyDetectorNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_connect_anomaly_detector(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_connect_anomaly_detector(bridge, nullptr));
}

TEST_F(SecurityTrainingConnectionTest, DisconnectTrainingPipeline) {
    int dummy_training = 42;
    EXPECT_EQ(0, security_training_connect_training_pipeline(bridge, &dummy_training));
    EXPECT_EQ(0, security_training_disconnect_training_pipeline(bridge));

    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_FALSE(stats.training_connected);
}

TEST_F(SecurityTrainingConnectionTest, DisconnectOptimizer) {
    nimcp_optimizer_context_t* dummy_optimizer = (nimcp_optimizer_context_t*)0x1234;
    EXPECT_EQ(0, security_training_connect_optimizer(bridge, dummy_optimizer));
    EXPECT_EQ(0, security_training_disconnect_optimizer(bridge));

    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_FALSE(stats.optimizer_connected);
}

TEST_F(SecurityTrainingConnectionTest, DisconnectBBB) {
    bbb_system_t dummy_bbb = (bbb_system_t)0x5678;
    EXPECT_EQ(0, security_training_connect_bbb(bridge, dummy_bbb));
    EXPECT_EQ(0, security_training_disconnect_bbb(bridge));

    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_FALSE(stats.bbb_connected);
}

TEST_F(SecurityTrainingConnectionTest, DisconnectAnomalyDetector) {
    nimcp_anomaly_detector_t dummy_detector = (nimcp_anomaly_detector_t)0xABCD;
    EXPECT_EQ(0, security_training_connect_anomaly_detector(bridge, dummy_detector));
    EXPECT_EQ(0, security_training_disconnect_anomaly_detector(bridge));

    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_FALSE(stats.anomaly_connected);
}

TEST_F(SecurityTrainingConnectionTest, DisconnectNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_disconnect_training_pipeline(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_disconnect_optimizer(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_disconnect_bbb(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_disconnect_anomaly_detector(nullptr));
}

/* =============================================================================
 * Data Source Validation Tests
 * ============================================================================= */

class SecurityTrainingDataSourceTest : public SecurityTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_training_default_config(&config));
        config.enable_data_validation = true;
        config.default_trust_level = SECURITY_TRUST_UNTRUSTED;
        config.require_source_verification = true;
        bridge = security_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityTrainingDataSourceTest, ValidateTrustedSourcePasses) {
    const char* source_name = "trusted_dataset";

    /* Set source to internal (highest trust) */
    EXPECT_EQ(0, security_training_set_source_trust(bridge, source_name, SECURITY_TRUST_INTERNAL));

    /* Should pass validation */
    EXPECT_TRUE(security_training_validate_data_source(bridge, source_name));
}

TEST_F(SecurityTrainingDataSourceTest, ValidateCertifiedSourcePasses) {
    const char* source_name = "certified_dataset";

    EXPECT_EQ(0, security_training_set_source_trust(bridge, source_name, SECURITY_TRUST_CERTIFIED));
    EXPECT_TRUE(security_training_validate_data_source(bridge, source_name));
}

TEST_F(SecurityTrainingDataSourceTest, ValidateVerifiedSourcePasses) {
    const char* source_name = "verified_dataset";

    EXPECT_EQ(0, security_training_set_source_trust(bridge, source_name, SECURITY_TRUST_VERIFIED));
    EXPECT_TRUE(security_training_validate_data_source(bridge, source_name));
}

TEST_F(SecurityTrainingDataSourceTest, ValidateUntrustedSourceBlocked) {
    const char* source_name = "untrusted_dataset";

    /* Source starts as untrusted (default) */
    EXPECT_EQ(0, security_training_set_source_trust(bridge, source_name, SECURITY_TRUST_UNTRUSTED));

    /* Should fail validation */
    EXPECT_FALSE(security_training_validate_data_source(bridge, source_name));
}

TEST_F(SecurityTrainingDataSourceTest, UnknownSourceDefaultsToUntrusted) {
    const char* source_name = "unknown_dataset";

    /* Unknown sources should default to UNTRUSTED and fail validation */
    EXPECT_EQ(SECURITY_TRUST_UNTRUSTED, security_training_get_source_trust(bridge, source_name));
    EXPECT_FALSE(security_training_validate_data_source(bridge, source_name));
}

TEST_F(SecurityTrainingDataSourceTest, BlockSource) {
    const char* source_name = "malicious_dataset";

    /* First set to trusted */
    EXPECT_EQ(0, security_training_set_source_trust(bridge, source_name, SECURITY_TRUST_INTERNAL));
    EXPECT_TRUE(security_training_validate_data_source(bridge, source_name));

    /* Then block */
    EXPECT_EQ(0, security_training_block_source(bridge, source_name));

    /* Should now fail validation */
    EXPECT_FALSE(security_training_validate_data_source(bridge, source_name));
}

TEST_F(SecurityTrainingDataSourceTest, GetSourceTrust) {
    const char* source_name = "test_dataset";

    /* Set different trust levels and verify */
    EXPECT_EQ(0, security_training_set_source_trust(bridge, source_name, SECURITY_TRUST_VERIFIED));
    EXPECT_EQ(SECURITY_TRUST_VERIFIED, security_training_get_source_trust(bridge, source_name));

    EXPECT_EQ(0, security_training_set_source_trust(bridge, source_name, SECURITY_TRUST_CERTIFIED));
    EXPECT_EQ(SECURITY_TRUST_CERTIFIED, security_training_get_source_trust(bridge, source_name));

    EXPECT_EQ(0, security_training_set_source_trust(bridge, source_name, SECURITY_TRUST_INTERNAL));
    EXPECT_EQ(SECURITY_TRUST_INTERNAL, security_training_get_source_trust(bridge, source_name));
}

TEST_F(SecurityTrainingDataSourceTest, ValidateNullSource) {
    EXPECT_FALSE(security_training_validate_data_source(bridge, nullptr));
    EXPECT_FALSE(security_training_validate_data_source(nullptr, "test"));
}

TEST_F(SecurityTrainingDataSourceTest, SetTrustNullParams) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_set_source_trust(nullptr, "test", SECURITY_TRUST_INTERNAL));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_set_source_trust(bridge, nullptr, SECURITY_TRUST_INTERNAL));
}

TEST_F(SecurityTrainingDataSourceTest, BlockSourceNullParams) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_block_source(nullptr, "test"));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_block_source(bridge, nullptr));
}

/* =============================================================================
 * Poisoning Detection Tests
 * ============================================================================= */

class SecurityTrainingPoisoningTest : public SecurityTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_training_default_config(&config));
        config.enable_poisoning_detection = true;
        config.label_flip_threshold = 0.1f;
        config.backdoor_threshold = 0.8f;
        bridge = security_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityTrainingPoisoningTest, DetectNormalDataPasses) {
    /* Create normal training data with varied values (not constant) to avoid low entropy detection */
    std::vector<float> data(1000);
    for (size_t i = 0; i < data.size(); i++) {
        data[i] = (float)(i % 256) / 255.0f;  /* Varied values */
    }
    std::vector<int32_t> labels = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1};  /* Balanced labels */

    security_poisoning_result_t result;
    memset(&result, 0, sizeof(result));

    EXPECT_EQ(0, security_training_detect_poisoning(
        bridge, data.data(), data.size() * sizeof(float),
        labels.data(), labels.size(), &result));

    /* Normal data should not trigger poisoning detection */
    EXPECT_FALSE(result.poisoning_detected);
    EXPECT_EQ(SECURITY_POISONING_NONE, result.type);
}

TEST_F(SecurityTrainingPoisoningTest, DetectLabelFlip) {
    /* Create data with varied values (not constant) to avoid low entropy detection
     * but with severely imbalanced labels (label flip attack).
     * The detection requires some labels with class > 0 to detect imbalance. */
    std::vector<float> data(1000);
    for (size_t i = 0; i < data.size(); i++) {
        data[i] = (float)(i % 256) / 255.0f;  /* Varied values for normal entropy */
    }
    std::vector<int32_t> labels(100);

    /* Create extremely imbalanced labels: 99 of class 0, 1 of class 5
     * This creates max_label=5, expected_per_class=100/6=16.67
     * deviation for each class > threshold */
    for (size_t i = 0; i < labels.size() - 1; i++) {
        labels[i] = 0;  /* 99 of class 0 */
    }
    labels[99] = 5;  /* 1 of class 5, introduces imbalance expectation across 6 classes */

    security_poisoning_result_t result;
    memset(&result, 0, sizeof(result));

    EXPECT_EQ(0, security_training_detect_poisoning(
        bridge, data.data(), data.size() * sizeof(float),
        labels.data(), labels.size(), &result));

    /* Should detect label flip attack due to severe class imbalance */
    EXPECT_TRUE(result.poisoning_detected);
    EXPECT_EQ(SECURITY_POISONING_LABEL_FLIP, result.type);
    EXPECT_GT(result.confidence, 0.0f);
    EXPECT_TRUE(result.quarantine_recommended);
}

TEST_F(SecurityTrainingPoisoningTest, DetectBackdoorTrigger) {
    /* Create data with potential backdoor pattern (unusual high values) */
    std::vector<float> data(1000);
    for (size_t i = 0; i < data.size(); i++) {
        if (i % 10 == 0) {
            /* Inject suspicious trigger pattern */
            data[i] = 999.9f;
        } else {
            data[i] = 0.5f;
        }
    }

    std::vector<int32_t> labels = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1};

    security_poisoning_result_t result;
    memset(&result, 0, sizeof(result));

    EXPECT_EQ(0, security_training_detect_poisoning(
        bridge, data.data(), data.size() * sizeof(float),
        labels.data(), labels.size(), &result));

    /* Should detect backdoor pattern */
    EXPECT_TRUE(result.poisoning_detected);
    EXPECT_EQ(SECURITY_POISONING_BACKDOOR, result.type);
    EXPECT_GT(result.confidence, 0.0f);
}

TEST_F(SecurityTrainingPoisoningTest, ReportSuspiciousSample) {
    uint32_t sample_index = 42;
    float suspicion_score = 0.85f;
    const char* reason = "Unusual activation pattern";

    EXPECT_EQ(0, security_training_report_suspicious_sample(
        bridge, sample_index, suspicion_score, reason));

    /* Verify stats updated */
    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.suspicious_samples_total);
}

TEST_F(SecurityTrainingPoisoningTest, ReportSuspiciousSampleNullParams) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_report_suspicious_sample(nullptr, 0, 0.5f, "reason"));
    /* Note: NULL reason is allowed - it's only used for logging */
    EXPECT_EQ(0, security_training_report_suspicious_sample(bridge, 0, 0.5f, nullptr));
}

TEST_F(SecurityTrainingPoisoningTest, QuarantineSamples) {
    std::vector<uint32_t> indices = {5, 10, 15, 20};

    EXPECT_EQ(0, security_training_quarantine_samples(
        bridge, indices.data(), indices.size()));

    /* Verify stats updated */
    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_EQ(4u, stats.samples_quarantined);
}

TEST_F(SecurityTrainingPoisoningTest, QuarantineSamplesNullParams) {
    std::vector<uint32_t> indices = {5, 10};
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_quarantine_samples(nullptr, indices.data(), 2));
    /* Note: NULL indices with count returns 0 (no-op) - implementation checks after BRIDGE_NULL_CHECK */
    EXPECT_EQ(0, security_training_quarantine_samples(bridge, nullptr, 2));
}

TEST_F(SecurityTrainingPoisoningTest, DetectPoisoningNullParams) {
    security_poisoning_result_t result;
    std::vector<float> data(100, 0.5f);
    std::vector<int32_t> labels = {0, 1, 0, 1, 0};

    /* NULL bridge returns error */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_detect_poisoning(nullptr, data.data(), 100, labels.data(), 5, &result));
    /* NULL data is allowed - returns 0 with no poisoning detected */
    EXPECT_EQ(0, security_training_detect_poisoning(bridge, nullptr, 100, labels.data(), 5, &result));
    EXPECT_FALSE(result.poisoning_detected);
    /* NULL result returns error */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_detect_poisoning(bridge, data.data(), 100, labels.data(), 5, nullptr));
}

/* =============================================================================
 * Gradient Sanitization Tests
 * ============================================================================= */

class SecurityTrainingGradientTest : public SecurityTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_training_default_config(&config));
        config.enable_gradient_sanitization = true;
        config.gradient_clip_norm = 1.0f;
        config.gradient_clip_value = 10.0f;
        config.gradient_min_bound = -100.0f;
        config.gradient_max_bound = 100.0f;
        bridge = security_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityTrainingGradientTest, SanitizeClipNorm) {
    /* Set clip norm mode */
    EXPECT_EQ(0, security_training_set_gradient_params(
        bridge, SECURITY_GRAD_SANITIZE_CLIP_NORM, 1.0f, 10.0f));

    /* Create gradient with large norm */
    std::vector<float> gradients = {10.0f, 10.0f, 10.0f, 10.0f};  /* Norm = 20 */
    uint32_t num_params = gradients.size();

    /* Calculate original norm */
    float original_norm = 0.0f;
    for (float g : gradients) {
        original_norm += g * g;
    }
    original_norm = sqrtf(original_norm);
    EXPECT_GT(original_norm, 1.0f);  /* Should be > clip_norm */

    /* Sanitize */
    EXPECT_EQ(0, security_training_sanitize_gradients(bridge, gradients.data(), num_params));

    /* Calculate new norm */
    float new_norm = 0.0f;
    for (float g : gradients) {
        new_norm += g * g;
    }
    new_norm = sqrtf(new_norm);

    /* Norm should be clipped to 1.0 */
    EXPECT_NEAR(1.0f, new_norm, 0.01f);

    /* Verify stats */
    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_GT(stats.gradients_sanitized, 0u);
    EXPECT_GT(stats.gradients_clipped_norm, 0u);
}

TEST_F(SecurityTrainingGradientTest, SanitizeClipValue) {
    /* Set clip value mode */
    EXPECT_EQ(0, security_training_set_gradient_params(
        bridge, SECURITY_GRAD_SANITIZE_CLIP_VALUE, 1.0f, 5.0f));

    /* Create gradient with large values */
    std::vector<float> gradients = {100.0f, -100.0f, 3.0f, -2.0f};
    uint32_t num_params = gradients.size();

    /* Sanitize */
    EXPECT_EQ(0, security_training_sanitize_gradients(bridge, gradients.data(), num_params));

    /* Values should be clipped to [-5, 5] */
    EXPECT_FLOAT_EQ(5.0f, gradients[0]);
    EXPECT_FLOAT_EQ(-5.0f, gradients[1]);
    EXPECT_FLOAT_EQ(3.0f, gradients[2]);   /* Within range, unchanged */
    EXPECT_FLOAT_EQ(-2.0f, gradients[3]);  /* Within range, unchanged */

    /* Verify stats */
    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_GT(stats.gradients_clipped_value, 0u);
}

TEST_F(SecurityTrainingGradientTest, SanitizeDifferentialPrivacy) {
    /* Set differential privacy mode */
    EXPECT_EQ(0, security_training_set_gradient_params(
        bridge, SECURITY_GRAD_SANITIZE_DIFFERENTIAL, 1.0f, 10.0f));

    /* Create deterministic gradient */
    std::vector<float> gradients = {1.0f, 2.0f, 3.0f, 4.0f};
    std::vector<float> original_gradients = gradients;
    uint32_t num_params = gradients.size();

    /* Sanitize (adds noise) */
    EXPECT_EQ(0, security_training_sanitize_gradients(bridge, gradients.data(), num_params));

    /* Gradients should be modified by noise */
    bool any_different = false;
    for (size_t i = 0; i < gradients.size(); i++) {
        if (fabsf(gradients[i] - original_gradients[i]) > 0.0001f) {
            any_different = true;
            break;
        }
    }
    EXPECT_TRUE(any_different);
}

TEST_F(SecurityTrainingGradientTest, SanitizeNoMode) {
    /* Set no sanitization mode */
    EXPECT_EQ(0, security_training_set_gradient_params(
        bridge, SECURITY_GRAD_SANITIZE_NONE, 1.0f, 10.0f));

    /* Create gradient */
    std::vector<float> gradients = {100.0f, -100.0f, 50.0f};
    std::vector<float> original_gradients = gradients;
    uint32_t num_params = gradients.size();

    /* Sanitize (should not modify) */
    EXPECT_EQ(0, security_training_sanitize_gradients(bridge, gradients.data(), num_params));

    /* Gradients should be unchanged */
    for (size_t i = 0; i < gradients.size(); i++) {
        EXPECT_FLOAT_EQ(original_gradients[i], gradients[i]);
    }
}

TEST_F(SecurityTrainingGradientTest, CheckGradientAnomaly) {
    std::vector<float> normal_gradients = {0.1f, -0.2f, 0.3f, -0.1f};
    float anomaly_score = 0.0f;

    /* Normal gradients should not trigger anomaly */
    EXPECT_FALSE(security_training_check_gradient_anomaly(
        bridge, normal_gradients.data(), normal_gradients.size(), &anomaly_score));
    EXPECT_LT(anomaly_score, 0.5f);

    /* Extreme gradients should trigger anomaly */
    std::vector<float> extreme_gradients = {1000.0f, -1000.0f, 500.0f, -500.0f};
    EXPECT_TRUE(security_training_check_gradient_anomaly(
        bridge, extreme_gradients.data(), extreme_gradients.size(), &anomaly_score));
    EXPECT_GT(anomaly_score, 0.5f);
}

TEST_F(SecurityTrainingGradientTest, SanitizeGradientsNullParams) {
    std::vector<float> gradients = {1.0f, 2.0f};
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_sanitize_gradients(nullptr, gradients.data(), 2));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_sanitize_gradients(bridge, nullptr, 2));
}

TEST_F(SecurityTrainingGradientTest, SetGradientParamsNullBridge) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_set_gradient_params(
        nullptr, SECURITY_GRAD_SANITIZE_CLIP_NORM, 1.0f, 10.0f));
}

/* =============================================================================
 * Model Integrity Tests
 * ============================================================================= */

class SecurityTrainingIntegrityTest : public SecurityTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_training_default_config(&config));
        config.enable_model_verification = true;
        config.enable_secure_checkpointing = true;
        bridge = security_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityTrainingIntegrityTest, VerifyModelIntegrityNoCheckpoint) {
    std::vector<float> weights = {0.1f, 0.2f, 0.3f, 0.4f};

    /* No checkpoint exists yet */
    security_integrity_result_t result = security_training_verify_model_integrity(
        bridge, weights.data(), weights.size());

    EXPECT_EQ(SECURITY_INTEGRITY_CHECKPOINT_MISSING, result);
}

TEST_F(SecurityTrainingIntegrityTest, VerifyModelIntegrityOK) {
    std::vector<float> weights = {0.1f, 0.2f, 0.3f, 0.4f};

    /* Create checkpoint */
    EXPECT_EQ(0, security_training_checkpoint_model(
        bridge, "checkpoint_1", weights.data(), weights.size(), 100));

    /* Verify with same weights */
    security_integrity_result_t result = security_training_verify_model_integrity(
        bridge, weights.data(), weights.size());

    EXPECT_EQ(SECURITY_INTEGRITY_OK, result);

    /* Verify stats */
    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_GT(stats.integrity_checks, 0u);
    EXPECT_EQ(0u, stats.integrity_failures);
}

TEST_F(SecurityTrainingIntegrityTest, VerifyModelIntegrityTampered) {
    std::vector<float> weights = {0.1f, 0.2f, 0.3f, 0.4f};

    /* Create checkpoint */
    EXPECT_EQ(0, security_training_checkpoint_model(
        bridge, "checkpoint_1", weights.data(), weights.size(), 100));

    /* Modify weights (simulate tampering) */
    weights[0] = 999.9f;

    /* Verify with modified weights */
    security_integrity_result_t result = security_training_verify_model_integrity(
        bridge, weights.data(), weights.size());

    EXPECT_EQ(SECURITY_INTEGRITY_HASH_MISMATCH, result);

    /* Verify stats */
    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_GT(stats.integrity_failures, 0u);
}

TEST_F(SecurityTrainingIntegrityTest, VerifyModelIntegrityNullParams) {
    std::vector<float> weights = {0.1f, 0.2f};

    /* Null bridge should return error result */
    security_integrity_result_t result = security_training_verify_model_integrity(
        nullptr, weights.data(), weights.size());
    EXPECT_NE(SECURITY_INTEGRITY_OK, result);

    /* Null weights should return error result */
    result = security_training_verify_model_integrity(bridge, nullptr, 2);
    EXPECT_NE(SECURITY_INTEGRITY_OK, result);
}

/* =============================================================================
 * Checkpoint Tests
 * ============================================================================= */

TEST_F(SecurityTrainingIntegrityTest, CheckpointModel) {
    std::vector<float> weights = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    EXPECT_EQ(0, security_training_checkpoint_model(
        bridge, "test_checkpoint", weights.data(), weights.size(), 1000));

    /* Verify stats */
    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.checkpoints_created);
}

TEST_F(SecurityTrainingIntegrityTest, CheckpointModelNullParams) {
    std::vector<float> weights = {0.1f, 0.2f};

    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_checkpoint_model(nullptr, "test", weights.data(), 2, 0));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_checkpoint_model(bridge, nullptr, weights.data(), 2, 0));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_checkpoint_model(bridge, "test", nullptr, 2, 0));
}

TEST_F(SecurityTrainingIntegrityTest, RollbackModel) {
    std::vector<float> original_weights = {0.1f, 0.2f, 0.3f, 0.4f};

    /* Create checkpoint with original weights */
    EXPECT_EQ(0, security_training_checkpoint_model(
        bridge, "safe_checkpoint", original_weights.data(), original_weights.size(), 100));

    /* Rollback to checkpoint - the implementation verifies that provided weights match checkpoint
     * Note: The implementation doesn't actually restore weights from storage, it verifies
     * that the caller-provided weights match the checkpoint hash. The caller is responsible
     * for keeping a copy of weights and providing them for verification. */
    EXPECT_EQ(0, security_training_rollback_model(
        bridge, "safe_checkpoint", original_weights.data(), original_weights.size()));

    /* Verify stats */
    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_EQ(1u, stats.rollbacks_performed);
}

TEST_F(SecurityTrainingIntegrityTest, RollbackModelNonexistent) {
    std::vector<float> weights(4, 0.0f);

    EXPECT_EQ(NIMCP_ERROR_NOT_FOUND, security_training_rollback_model(
        bridge, "nonexistent_checkpoint", weights.data(), weights.size()));
}

TEST_F(SecurityTrainingIntegrityTest, RollbackModelNullParams) {
    std::vector<float> weights(4, 0.0f);

    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_rollback_model(nullptr, "test", weights.data(), 4));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_rollback_model(bridge, nullptr, weights.data(), 4));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_rollback_model(bridge, "test", nullptr, 4));
}

TEST_F(SecurityTrainingIntegrityTest, GetCheckpointInfo) {
    std::vector<float> weights = {0.1f, 0.2f, 0.3f};

    EXPECT_EQ(0, security_training_checkpoint_model(
        bridge, "info_test", weights.data(), weights.size(), 500));

    security_checkpoint_info_t info;
    memset(&info, 0, sizeof(info));

    EXPECT_EQ(0, security_training_get_checkpoint_info(bridge, "info_test", &info));

    EXPECT_STREQ("info_test", info.name);
    EXPECT_EQ(500u, info.step);
    EXPECT_GT(info.timestamp_ms, 0u);
}

TEST_F(SecurityTrainingIntegrityTest, GetCheckpointInfoNullParams) {
    security_checkpoint_info_t info;

    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_get_checkpoint_info(nullptr, "test", &info));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_get_checkpoint_info(bridge, nullptr, &info));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_get_checkpoint_info(bridge, "test", nullptr));
}

TEST_F(SecurityTrainingIntegrityTest, ListCheckpoints) {
    std::vector<float> weights = {0.1f, 0.2f};

    /* Create multiple checkpoints */
    EXPECT_EQ(0, security_training_checkpoint_model(bridge, "cp1", weights.data(), 2, 100));
    EXPECT_EQ(0, security_training_checkpoint_model(bridge, "cp2", weights.data(), 2, 200));
    EXPECT_EQ(0, security_training_checkpoint_model(bridge, "cp3", weights.data(), 2, 300));

    std::vector<security_checkpoint_info_t> checkpoints(10);
    int count = security_training_list_checkpoints(bridge, checkpoints.data(), 10);

    EXPECT_EQ(3, count);
}

TEST_F(SecurityTrainingIntegrityTest, ListCheckpointsNullParams) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_list_checkpoints(nullptr, nullptr, 0));
}

/* =============================================================================
 * Concept Drift Tests
 * ============================================================================= */

class SecurityTrainingDriftTest : public SecurityTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_training_default_config(&config));
        config.enable_concept_drift_detection = true;
        config.drift_threshold = 0.3f;
        config.drift_window_size = 100;
        bridge = security_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityTrainingDriftTest, DetectConceptDriftNoDrift) {
    /* Establish baseline */
    std::vector<float> baseline_features = {0.5f, 0.5f, 0.5f, 0.5f};
    EXPECT_EQ(0, security_training_update_drift_baseline(
        bridge, baseline_features.data(), baseline_features.size()));

    /* Check with similar features (no drift) */
    std::vector<float> current_features = {0.51f, 0.49f, 0.52f, 0.48f};
    float drift_score = 0.0f;

    EXPECT_FALSE(security_training_detect_concept_drift(
        bridge, current_features.data(), current_features.size(), &drift_score));
    EXPECT_LT(drift_score, 0.3f);
}

TEST_F(SecurityTrainingDriftTest, DetectConceptDriftSignificant) {
    /* Establish baseline */
    std::vector<float> baseline_features = {0.1f, 0.2f, 0.3f, 0.4f};
    EXPECT_EQ(0, security_training_update_drift_baseline(
        bridge, baseline_features.data(), baseline_features.size()));

    /* Check with significantly different features */
    std::vector<float> drifted_features = {0.9f, 0.8f, 0.7f, 0.6f};
    float drift_score = 0.0f;

    EXPECT_TRUE(security_training_detect_concept_drift(
        bridge, drifted_features.data(), drifted_features.size(), &drift_score));
    EXPECT_GT(drift_score, 0.3f);

    /* Verify stats */
    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_GT(stats.drift_detections, 0u);
}

TEST_F(SecurityTrainingDriftTest, UpdateDriftBaseline) {
    std::vector<float> features = {0.1f, 0.2f, 0.3f, 0.4f};

    EXPECT_EQ(0, security_training_update_drift_baseline(
        bridge, features.data(), features.size()));

    /* Second update should also succeed */
    std::vector<float> features2 = {0.2f, 0.3f, 0.4f, 0.5f};
    EXPECT_EQ(0, security_training_update_drift_baseline(
        bridge, features2.data(), features2.size()));
}

TEST_F(SecurityTrainingDriftTest, ResetDriftBaseline) {
    std::vector<float> features = {0.1f, 0.2f, 0.3f, 0.4f};

    EXPECT_EQ(0, security_training_update_drift_baseline(
        bridge, features.data(), features.size()));

    EXPECT_EQ(0, security_training_reset_drift_baseline(bridge));

    /* After reset, drift detection should need new baseline */
    float drift_score = 0.0f;
    /* This might return false or true depending on implementation,
       but should not crash */
    security_training_detect_concept_drift(
        bridge, features.data(), features.size(), &drift_score);
}

TEST_F(SecurityTrainingDriftTest, DetectConceptDriftNullParams) {
    std::vector<float> features = {0.1f, 0.2f};
    float drift_score = 0.0f;

    EXPECT_FALSE(security_training_detect_concept_drift(nullptr, features.data(), 2, &drift_score));
    EXPECT_FALSE(security_training_detect_concept_drift(bridge, nullptr, 2, &drift_score));
    EXPECT_FALSE(security_training_detect_concept_drift(bridge, features.data(), 2, nullptr));
}

TEST_F(SecurityTrainingDriftTest, UpdateDriftBaselineNullParams) {
    std::vector<float> features = {0.1f, 0.2f};

    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_update_drift_baseline(nullptr, features.data(), 2));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_update_drift_baseline(bridge, nullptr, 2));
}

TEST_F(SecurityTrainingDriftTest, ResetDriftBaselineNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_reset_drift_baseline(nullptr));
}

/* =============================================================================
 * Bidirectional Update Tests
 * ============================================================================= */

class SecurityTrainingBidirectionalTest : public SecurityTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_training_default_config(&config));
        bridge = security_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityTrainingBidirectionalTest, UpdateSecurityEffects) {
    EXPECT_EQ(0, security_training_update_security_effects(bridge));

    security_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    EXPECT_EQ(0, security_training_get_security_effects(bridge, &effects));
    EXPECT_TRUE(effects.valid);
    EXPECT_GT(effects.last_update_ms, 0u);
}

TEST_F(SecurityTrainingBidirectionalTest, UpdateTrainingEffects) {
    float loss = 0.5f;
    float gradient_norm = 1.2f;
    uint64_t step = 1000;

    EXPECT_EQ(0, security_training_update_training_effects(bridge, loss, gradient_norm, step));

    training_security_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    EXPECT_EQ(0, security_training_get_training_effects(bridge, &effects));
    EXPECT_TRUE(effects.valid);
    EXPECT_FLOAT_EQ(loss, effects.current_loss);
    EXPECT_FLOAT_EQ(gradient_norm, effects.current_gradient_norm);
    EXPECT_EQ(step, effects.current_step);
}

TEST_F(SecurityTrainingBidirectionalTest, GetSecurityEffects) {
    security_training_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    EXPECT_EQ(0, security_training_get_security_effects(bridge, &effects));

    /* Default values should be reasonable */
    EXPECT_GE(effects.threat_level, 0.0f);
    EXPECT_LE(effects.threat_level, 1.0f);
}

TEST_F(SecurityTrainingBidirectionalTest, GetTrainingEffects) {
    training_security_effects_t effects;
    memset(&effects, 0, sizeof(effects));

    EXPECT_EQ(0, security_training_get_training_effects(bridge, &effects));

    /* Default values should be reasonable */
    EXPECT_GE(effects.gradient_anomaly_score, 0.0f);
    EXPECT_LE(effects.gradient_anomaly_score, 1.0f);
}

TEST_F(SecurityTrainingBidirectionalTest, UpdateSecurityEffectsNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_update_security_effects(nullptr));
}

TEST_F(SecurityTrainingBidirectionalTest, UpdateTrainingEffectsNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_update_training_effects(nullptr, 0.0f, 0.0f, 0));
}

TEST_F(SecurityTrainingBidirectionalTest, GetSecurityEffectsNullParams) {
    security_training_effects_t effects;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_get_security_effects(nullptr, &effects));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_get_security_effects(bridge, nullptr));
}

TEST_F(SecurityTrainingBidirectionalTest, GetTrainingEffectsNullParams) {
    training_security_effects_t effects;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_get_training_effects(nullptr, &effects));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_get_training_effects(bridge, nullptr));
}

TEST_F(SecurityTrainingBidirectionalTest, DetectLossNaN) {
    float nan_loss = NAN;
    float gradient_norm = 1.0f;

    EXPECT_EQ(0, security_training_update_training_effects(bridge, nan_loss, gradient_norm, 100));

    training_security_effects_t effects;
    EXPECT_EQ(0, security_training_get_training_effects(bridge, &effects));
    EXPECT_TRUE(effects.loss_nan_detected);
}

TEST_F(SecurityTrainingBidirectionalTest, DetectLossInf) {
    float inf_loss = INFINITY;
    float gradient_norm = 1.0f;

    EXPECT_EQ(0, security_training_update_training_effects(bridge, inf_loss, gradient_norm, 100));

    training_security_effects_t effects;
    EXPECT_EQ(0, security_training_get_training_effects(bridge, &effects));
    EXPECT_TRUE(effects.loss_inf_detected);
}

TEST_F(SecurityTrainingBidirectionalTest, DetectGradientExplosion) {
    float loss = 0.5f;
    float huge_gradient_norm = 1e10f;

    EXPECT_EQ(0, security_training_update_training_effects(bridge, loss, huge_gradient_norm, 100));

    training_security_effects_t effects;
    EXPECT_EQ(0, security_training_get_training_effects(bridge, &effects));
    EXPECT_TRUE(effects.gradient_explosion);
}

TEST_F(SecurityTrainingBidirectionalTest, DetectGradientVanishing) {
    float loss = 0.5f;
    float tiny_gradient_norm = 1e-12f;

    EXPECT_EQ(0, security_training_update_training_effects(bridge, loss, tiny_gradient_norm, 100));

    training_security_effects_t effects;
    EXPECT_EQ(0, security_training_get_training_effects(bridge, &effects));
    EXPECT_TRUE(effects.gradient_vanishing);
}

/* =============================================================================
 * Stats Tests
 * ============================================================================= */

class SecurityTrainingStatsTest : public SecurityTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_training_default_config(&config));
        bridge = security_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityTrainingStatsTest, GetStatsInitial) {
    security_training_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));  /* Fill with garbage */

    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));

    /* Initial stats should be zero/false */
    EXPECT_EQ(0u, stats.total_validations);
    EXPECT_EQ(0u, stats.data_sources_validated);
    EXPECT_EQ(0u, stats.poisoning_scans);
    EXPECT_EQ(0u, stats.gradients_sanitized);
    EXPECT_EQ(0u, stats.integrity_checks);
    EXPECT_EQ(0u, stats.drift_checks);
    /* Bridge starts in MONITORING phase after creation */
    EXPECT_EQ(SECURITY_TRAINING_PHASE_MONITORING, stats.current_phase);
}

TEST_F(SecurityTrainingStatsTest, GetStatsAfterOperations) {
    /* Perform some operations */
    std::vector<float> data(100);
    for (size_t i = 0; i < data.size(); i++) {
        data[i] = (float)(i % 256) / 255.0f;  /* Varied values */
    }
    std::vector<int32_t> labels = {0, 1, 0, 1, 0};
    security_poisoning_result_t result;

    /* Poisoning scan */
    security_training_detect_poisoning(bridge, data.data(), 100, labels.data(), 5, &result);

    /* Gradient sanitization */
    std::vector<float> gradients = {1.0f, 2.0f, 3.0f};
    security_training_sanitize_gradients(bridge, gradients.data(), 3);

    /* Data source validation - calling validate_data_source on a new source registers it
     * and increments data_sources_validated */
    security_training_validate_data_source(bridge, "new_test_source");

    /* Get stats */
    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));

    EXPECT_GT(stats.poisoning_scans, 0u);
    EXPECT_GT(stats.gradients_sanitized, 0u);
    /* Note: data_sources_validated increments when new sources are registered during validation */
    EXPECT_GT(stats.data_sources_validated, 0u);
}

TEST_F(SecurityTrainingStatsTest, ResetStats) {
    /* Perform operations to increment stats */
    std::vector<float> gradients = {1.0f, 2.0f, 3.0f};
    security_training_sanitize_gradients(bridge, gradients.data(), 3);

    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_GT(stats.gradients_sanitized, 0u);

    /* Reset stats */
    EXPECT_EQ(0, security_training_reset_stats(bridge));

    /* Stats should be reset */
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_EQ(0u, stats.gradients_sanitized);
}

TEST_F(SecurityTrainingStatsTest, GetStatsNullParams) {
    security_training_stats_t stats;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_get_stats(nullptr, &stats));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_get_stats(bridge, nullptr));
}

TEST_F(SecurityTrainingStatsTest, ResetStatsNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_reset_stats(nullptr));
}

TEST_F(SecurityTrainingStatsTest, StatsConnectionStatus) {
    security_training_stats_t stats;
    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));

    /* Initially no connections */
    EXPECT_FALSE(stats.training_connected);
    EXPECT_FALSE(stats.optimizer_connected);
    EXPECT_FALSE(stats.bbb_connected);
    EXPECT_FALSE(stats.anomaly_connected);

    /* Connect training */
    int dummy = 42;
    security_training_connect_training_pipeline(bridge, &dummy);

    EXPECT_EQ(0, security_training_get_stats(bridge, &stats));
    EXPECT_TRUE(stats.training_connected);
}

/* =============================================================================
 * Query Tests
 * ============================================================================= */

TEST_F(SecurityTrainingStatsTest, GetPhase) {
    /* Bridge starts in MONITORING phase after creation */
    EXPECT_EQ(SECURITY_TRAINING_PHASE_MONITORING, security_training_get_phase(bridge));
}

TEST_F(SecurityTrainingStatsTest, GetPhaseNull) {
    EXPECT_EQ(SECURITY_TRAINING_PHASE_INACTIVE, security_training_get_phase(nullptr));
}

TEST_F(SecurityTrainingStatsTest, GetThreatLevel) {
    float threat = security_training_get_threat_level(bridge);
    EXPECT_GE(threat, 0.0f);
    EXPECT_LE(threat, 1.0f);
}

TEST_F(SecurityTrainingStatsTest, GetThreatLevelNull) {
    EXPECT_FLOAT_EQ(0.0f, security_training_get_threat_level(nullptr));
}

TEST_F(SecurityTrainingStatsTest, IsUnderAttack) {
    EXPECT_FALSE(security_training_is_under_attack(bridge));
}

TEST_F(SecurityTrainingStatsTest, IsUnderAttackNull) {
    EXPECT_FALSE(security_training_is_under_attack(nullptr));
}

/* =============================================================================
 * String Conversion Tests
 * ============================================================================= */

TEST(SecurityTrainingStringConversionTest, PoisoningTypeToString) {
    EXPECT_STREQ("none", security_poisoning_type_to_string(SECURITY_POISONING_NONE));
    EXPECT_STREQ("label_flip", security_poisoning_type_to_string(SECURITY_POISONING_LABEL_FLIP));
    EXPECT_STREQ("backdoor", security_poisoning_type_to_string(SECURITY_POISONING_BACKDOOR));
    EXPECT_STREQ("trojan", security_poisoning_type_to_string(SECURITY_POISONING_TROJAN));
    EXPECT_STREQ("gradient_manipulation", security_poisoning_type_to_string(SECURITY_POISONING_GRADIENT_MANIPULATION));
    EXPECT_STREQ("data_injection", security_poisoning_type_to_string(SECURITY_POISONING_DATA_INJECTION));
    EXPECT_STREQ("feature_collision", security_poisoning_type_to_string(SECURITY_POISONING_FEATURE_COLLISION));
    EXPECT_STREQ("unknown", security_poisoning_type_to_string((security_poisoning_type_t)999));
}

TEST(SecurityTrainingStringConversionTest, TrustLevelToString) {
    EXPECT_STREQ("untrusted", security_trust_level_to_string(SECURITY_TRUST_UNTRUSTED));
    EXPECT_STREQ("verified", security_trust_level_to_string(SECURITY_TRUST_VERIFIED));
    EXPECT_STREQ("certified", security_trust_level_to_string(SECURITY_TRUST_CERTIFIED));
    EXPECT_STREQ("internal", security_trust_level_to_string(SECURITY_TRUST_INTERNAL));
    EXPECT_STREQ("unknown", security_trust_level_to_string((security_data_trust_t)999));
}

TEST(SecurityTrainingStringConversionTest, PhaseToString) {
    EXPECT_STREQ("inactive", security_training_phase_to_string(SECURITY_TRAINING_PHASE_INACTIVE));
    EXPECT_STREQ("monitoring", security_training_phase_to_string(SECURITY_TRAINING_PHASE_MONITORING));
    EXPECT_STREQ("protecting", security_training_phase_to_string(SECURITY_TRAINING_PHASE_PROTECTING));
    EXPECT_STREQ("responding", security_training_phase_to_string(SECURITY_TRAINING_PHASE_RESPONDING));
    EXPECT_STREQ("recovery", security_training_phase_to_string(SECURITY_TRAINING_PHASE_RECOVERY));
    EXPECT_STREQ("unknown", security_training_phase_to_string((security_training_phase_t)999));
}

TEST(SecurityTrainingStringConversionTest, IntegrityResultToString) {
    EXPECT_STREQ("ok", security_integrity_result_to_string(SECURITY_INTEGRITY_OK));
    EXPECT_STREQ("hash_mismatch", security_integrity_result_to_string(SECURITY_INTEGRITY_HASH_MISMATCH));
    EXPECT_STREQ("signature_invalid", security_integrity_result_to_string(SECURITY_INTEGRITY_SIGNATURE_INVALID));
    EXPECT_STREQ("checkpoint_missing", security_integrity_result_to_string(SECURITY_INTEGRITY_CHECKPOINT_MISSING));
    EXPECT_STREQ("tampered", security_integrity_result_to_string(SECURITY_INTEGRITY_TAMPERED));
    EXPECT_STREQ("unknown", security_integrity_result_to_string((security_integrity_result_t)999));
}

TEST(SecurityTrainingStringConversionTest, GradSanitizeModeToString) {
    EXPECT_STREQ("none", security_grad_sanitize_mode_to_string(SECURITY_GRAD_SANITIZE_NONE));
    EXPECT_STREQ("clip_norm", security_grad_sanitize_mode_to_string(SECURITY_GRAD_SANITIZE_CLIP_NORM));
    EXPECT_STREQ("clip_value", security_grad_sanitize_mode_to_string(SECURITY_GRAD_SANITIZE_CLIP_VALUE));
    EXPECT_STREQ("clip_both", security_grad_sanitize_mode_to_string(SECURITY_GRAD_SANITIZE_CLIP_BOTH));
    EXPECT_STREQ("bound", security_grad_sanitize_mode_to_string(SECURITY_GRAD_SANITIZE_BOUND));
    EXPECT_STREQ("differential", security_grad_sanitize_mode_to_string(SECURITY_GRAD_SANITIZE_DIFFERENTIAL));
    EXPECT_STREQ("unknown", security_grad_sanitize_mode_to_string((security_grad_sanitize_mode_t)999));
}

/* =============================================================================
 * Bio-Async Tests
 * ============================================================================= */

class SecurityTrainingBioAsyncTest : public SecurityTrainingBridgeTest {
protected:
    void SetUp() override {
        SecurityTrainingBridgeTest::SetUp();
        EXPECT_EQ(0, security_training_default_config(&config));
        config.enable_bio_async = true;
        config.bio_inbox_capacity = SECURITY_TRAINING_BIO_INBOX_CAPACITY;
        bridge = security_training_bridge_create(&config);
        ASSERT_NE(nullptr, bridge);
    }
};

TEST_F(SecurityTrainingBioAsyncTest, IsBioAsyncConnectedInitially) {
#ifndef NIMCP_BIO_ASYNC_ENABLED
    GTEST_SKIP() << "Bio-async router not available";
#endif
    EXPECT_FALSE(security_training_is_bio_async_connected(bridge));
}

TEST_F(SecurityTrainingBioAsyncTest, IsBioAsyncConnectedNull) {
    EXPECT_FALSE(security_training_is_bio_async_connected(nullptr));
}

TEST_F(SecurityTrainingBioAsyncTest, ConnectBioAsyncNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_connect_bio_async(nullptr));
}

TEST_F(SecurityTrainingBioAsyncTest, DisconnectBioAsyncNull) {
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_disconnect_bio_async(nullptr));
}

/* =============================================================================
 * Null Safety Tests
 * ============================================================================= */

TEST(SecurityTrainingNullSafetyTest, AllFunctionsHandleNull) {
    /* Lifecycle */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_default_config(nullptr));
    security_training_bridge_destroy(nullptr);  /* Should not crash */

    /* Connections */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_connect_training_pipeline(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_connect_optimizer(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_connect_bbb(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_connect_anomaly_detector(nullptr, nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_disconnect_training_pipeline(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_disconnect_optimizer(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_disconnect_bbb(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_disconnect_anomaly_detector(nullptr));

    /* Data sources */
    EXPECT_FALSE(security_training_validate_data_source(nullptr, "test"));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_set_source_trust(nullptr, "test", SECURITY_TRUST_INTERNAL));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_block_source(nullptr, "test"));
    EXPECT_EQ(SECURITY_TRUST_UNTRUSTED, security_training_get_source_trust(nullptr, "test"));

    /* Poisoning */
    security_poisoning_result_t poisoning_result;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_detect_poisoning(nullptr, nullptr, 0, nullptr, 0, &poisoning_result));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_report_suspicious_sample(nullptr, 0, 0.0f, "test"));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_quarantine_samples(nullptr, nullptr, 0));

    /* Gradients */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_sanitize_gradients(nullptr, nullptr, 0));
    float score;
    EXPECT_FALSE(security_training_check_gradient_anomaly(nullptr, nullptr, 0, &score));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_set_gradient_params(nullptr, SECURITY_GRAD_SANITIZE_NONE, 0, 0));

    /* Model integrity */
    EXPECT_NE(SECURITY_INTEGRITY_OK, security_training_verify_model_integrity(nullptr, nullptr, 0));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_checkpoint_model(nullptr, "test", nullptr, 0, 0));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_rollback_model(nullptr, "test", nullptr, 0));
    security_checkpoint_info_t cp_info;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_get_checkpoint_info(nullptr, "test", &cp_info));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_list_checkpoints(nullptr, nullptr, 0));

    /* Drift */
    float drift;
    EXPECT_FALSE(security_training_detect_concept_drift(nullptr, nullptr, 0, &drift));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_update_drift_baseline(nullptr, nullptr, 0));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_reset_drift_baseline(nullptr));

    /* Bidirectional */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_update_security_effects(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_update_training_effects(nullptr, 0, 0, 0));
    security_training_effects_t sec_effects;
    training_security_effects_t train_effects;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_get_security_effects(nullptr, &sec_effects));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_get_training_effects(nullptr, &train_effects));

    /* Bio-async */
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_connect_bio_async(nullptr));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_disconnect_bio_async(nullptr));
    EXPECT_FALSE(security_training_is_bio_async_connected(nullptr));

    /* Query */
    EXPECT_EQ(SECURITY_TRAINING_PHASE_INACTIVE, security_training_get_phase(nullptr));
    EXPECT_FLOAT_EQ(0.0f, security_training_get_threat_level(nullptr));
    EXPECT_FALSE(security_training_is_under_attack(nullptr));

    /* Stats */
    security_training_stats_t stats;
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_get_stats(nullptr, &stats));
    EXPECT_EQ(NIMCP_ERROR_NULL_POINTER, security_training_reset_stats(nullptr));
}

/* =============================================================================
 * Constants Validation Tests
 * ============================================================================= */

TEST(SecurityTrainingConstantsTest, GradientDefaultsReasonable) {
    EXPECT_GT(SECURITY_TRAINING_DEFAULT_GRAD_CLIP_NORM, 0.0f);
    EXPECT_GT(SECURITY_TRAINING_DEFAULT_GRAD_CLIP_VALUE, 0.0f);
    EXPECT_LT(SECURITY_TRAINING_DEFAULT_GRAD_MIN_VALUE, 0.0f);
    EXPECT_GT(SECURITY_TRAINING_DEFAULT_GRAD_MAX_VALUE, 0.0f);
    EXPECT_LT(SECURITY_TRAINING_DEFAULT_GRAD_MIN_VALUE, SECURITY_TRAINING_DEFAULT_GRAD_MAX_VALUE);
}

TEST(SecurityTrainingConstantsTest, PoisoningThresholdsReasonable) {
    EXPECT_GT(SECURITY_TRAINING_DEFAULT_LABEL_FLIP_THRESHOLD, 0.0f);
    EXPECT_LE(SECURITY_TRAINING_DEFAULT_LABEL_FLIP_THRESHOLD, 1.0f);

    EXPECT_GT(SECURITY_TRAINING_DEFAULT_BACKDOOR_THRESHOLD, 0.0f);
    EXPECT_LE(SECURITY_TRAINING_DEFAULT_BACKDOOR_THRESHOLD, 1.0f);

    EXPECT_GT(SECURITY_TRAINING_DEFAULT_TROJAN_THRESHOLD, 0.0f);
    EXPECT_LE(SECURITY_TRAINING_DEFAULT_TROJAN_THRESHOLD, 1.0f);

    EXPECT_GT(SECURITY_TRAINING_DEFAULT_GRADIENT_ANOMALY_THRESHOLD, 0.0f);
    EXPECT_LE(SECURITY_TRAINING_DEFAULT_GRADIENT_ANOMALY_THRESHOLD, 1.0f);
}

TEST(SecurityTrainingConstantsTest, DriftConstantsReasonable) {
    EXPECT_GT(SECURITY_TRAINING_DRIFT_WINDOW_SIZE, 0u);
    EXPECT_GT(SECURITY_TRAINING_DEFAULT_DRIFT_THRESHOLD, 0.0f);
    EXPECT_LE(SECURITY_TRAINING_DEFAULT_DRIFT_THRESHOLD, 1.0f);
}

TEST(SecurityTrainingConstantsTest, LimitsReasonable) {
    EXPECT_GT(SECURITY_TRAINING_HASH_SIZE, 0u);
    EXPECT_GT(SECURITY_TRAINING_MAX_CHECKPOINTS, 0u);
    EXPECT_GT(SECURITY_TRAINING_CHECKPOINT_NAME_MAX, 0u);
    EXPECT_GT(SECURITY_TRAINING_BIO_INBOX_CAPACITY, 0u);
    EXPECT_GT(SECURITY_TRAINING_MAX_SUSPICIOUS_SAMPLES, 0u);
    EXPECT_GT(SECURITY_TRAINING_MAX_DATA_SOURCES, 0u);
}

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
