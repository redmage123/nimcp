/**
 * @file test_security_sleep_bridges.cpp
 * @brief Unit tests for security sleep bridge modules
 * @version 1.0.0
 * @date 2025-12-18
 *
 * Tests 4 security sleep bridge modules:
 * 1. BBB Sleep Bridge (nimcp_bbb_sleep_bridge.h)
 * 2. Anomaly Detector Sleep Bridge (nimcp_anomaly_detector_sleep_bridge.h)
 * 3. Rate Limiter Sleep Bridge (nimcp_rate_limiter_sleep_bridge.h)
 * 4. Pattern DB Sleep Bridge (nimcp_pattern_db_sleep_bridge.h)
 *
 * Note: These tests focus on configuration and constant functions.
 * Full lifecycle tests require the complete sleep system infrastructure.
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "security/sleep/nimcp_bbb_sleep_bridge.h"
#include "security/sleep/nimcp_anomaly_detector_sleep_bridge.h"
#include "security/sleep/nimcp_rate_limiter_sleep_bridge.h"
#include "security/sleep/nimcp_pattern_db_sleep_bridge.h"

/* =============================================================================
 * BBB Sleep Bridge Tests
 * ============================================================================= */

class BBBSleepBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&config, 0, sizeof(config));
    }

    bbb_sleep_config_t config;
};

TEST_F(BBBSleepBridgeTest, DefaultConfigValid) {
    EXPECT_EQ(0, bbb_sleep_default_config(&config));
    EXPECT_TRUE(config.enable_permeability_modulation);
    EXPECT_TRUE(config.enable_detection_modulation);
    EXPECT_TRUE(config.enable_response_modulation);
    EXPECT_FLOAT_EQ(1.0f, config.modulation_strength);
    EXPECT_TRUE(config.maintain_critical_protection);
}

TEST_F(BBBSleepBridgeTest, DefaultConfigNullFails) {
    EXPECT_EQ(-1, bbb_sleep_default_config(nullptr));
}

TEST_F(BBBSleepBridgeTest, CreateFailsWithNullConfig) {
    bbb_sleep_bridge_t b = bbb_sleep_bridge_create(nullptr, nullptr);
    EXPECT_EQ(nullptr, b);
}

TEST_F(BBBSleepBridgeTest, PermeabilityForStates) {
    EXPECT_FLOAT_EQ(BBB_SLEEP_PERMEABILITY_AWAKE, bbb_sleep_permeability_for_state(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(BBB_SLEEP_PERMEABILITY_DROWSY, bbb_sleep_permeability_for_state(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(BBB_SLEEP_PERMEABILITY_LIGHT_NREM, bbb_sleep_permeability_for_state(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(BBB_SLEEP_PERMEABILITY_DEEP_NREM, bbb_sleep_permeability_for_state(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(BBB_SLEEP_PERMEABILITY_REM, bbb_sleep_permeability_for_state(SLEEP_STATE_REM));
}

TEST_F(BBBSleepBridgeTest, DetectionForStates) {
    EXPECT_FLOAT_EQ(BBB_SLEEP_DETECTION_AWAKE, bbb_sleep_detection_for_state(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(BBB_SLEEP_DETECTION_DROWSY, bbb_sleep_detection_for_state(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(BBB_SLEEP_DETECTION_LIGHT_NREM, bbb_sleep_detection_for_state(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(BBB_SLEEP_DETECTION_DEEP_NREM, bbb_sleep_detection_for_state(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(BBB_SLEEP_DETECTION_REM, bbb_sleep_detection_for_state(SLEEP_STATE_REM));
}

TEST_F(BBBSleepBridgeTest, UrgencyForStates) {
    EXPECT_FLOAT_EQ(BBB_SLEEP_URGENCY_AWAKE, bbb_sleep_urgency_for_state(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(BBB_SLEEP_URGENCY_DROWSY, bbb_sleep_urgency_for_state(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(BBB_SLEEP_URGENCY_LIGHT_NREM, bbb_sleep_urgency_for_state(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(BBB_SLEEP_URGENCY_DEEP_NREM, bbb_sleep_urgency_for_state(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(BBB_SLEEP_URGENCY_REM, bbb_sleep_urgency_for_state(SLEEP_STATE_REM));
}

TEST_F(BBBSleepBridgeTest, PermeabilityMonotonicity) {
    // Permeability should generally increase from awake to sleep
    EXPECT_LE(BBB_SLEEP_PERMEABILITY_AWAKE, BBB_SLEEP_PERMEABILITY_DROWSY);
    EXPECT_LE(BBB_SLEEP_PERMEABILITY_DROWSY, BBB_SLEEP_PERMEABILITY_LIGHT_NREM);
    // Deep NREM has highest permeability for glymphatic clearance
    EXPECT_GE(BBB_SLEEP_PERMEABILITY_DEEP_NREM, BBB_SLEEP_PERMEABILITY_LIGHT_NREM);
}

TEST_F(BBBSleepBridgeTest, DetectionMonotonicity) {
    // Detection threshold should increase during sleep (less sensitive)
    EXPECT_LE(BBB_SLEEP_DETECTION_AWAKE, BBB_SLEEP_DETECTION_DROWSY);
    EXPECT_LE(BBB_SLEEP_DETECTION_DROWSY, BBB_SLEEP_DETECTION_LIGHT_NREM);
    EXPECT_LE(BBB_SLEEP_DETECTION_LIGHT_NREM, BBB_SLEEP_DETECTION_DEEP_NREM);
}

/* =============================================================================
 * Anomaly Detector Sleep Bridge Tests
 * ============================================================================= */

class AnomalyDetectorSleepBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&config, 0, sizeof(config));
    }

    anomaly_detector_sleep_config_t config;
};

TEST_F(AnomalyDetectorSleepBridgeTest, DefaultConfigValid) {
    EXPECT_EQ(0, anomaly_detector_sleep_default_config(&config));
    EXPECT_TRUE(config.enable_threshold_modulation);
    EXPECT_TRUE(config.enable_learning_modulation);
    EXPECT_TRUE(config.enable_fp_tolerance_modulation);
    EXPECT_FLOAT_EQ(1.0f, config.modulation_strength);
}

TEST_F(AnomalyDetectorSleepBridgeTest, DefaultConfigNullFails) {
    EXPECT_EQ(-1, anomaly_detector_sleep_default_config(nullptr));
}

TEST_F(AnomalyDetectorSleepBridgeTest, CreateFailsWithNullConfig) {
    anomaly_detector_sleep_bridge_t b = anomaly_detector_sleep_bridge_create(nullptr, nullptr);
    EXPECT_EQ(nullptr, b);
}

TEST_F(AnomalyDetectorSleepBridgeTest, ThresholdFactorForStates) {
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_THRESH_AWAKE, anomaly_sleep_get_thresh_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_THRESH_DROWSY, anomaly_sleep_get_thresh_factor(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_THRESH_LIGHT_NREM, anomaly_sleep_get_thresh_factor(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_THRESH_DEEP_NREM, anomaly_sleep_get_thresh_factor(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_THRESH_REM, anomaly_sleep_get_thresh_factor(SLEEP_STATE_REM));
}

TEST_F(AnomalyDetectorSleepBridgeTest, LearningFactorForStates) {
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_LEARN_AWAKE, anomaly_sleep_get_learn_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_LEARN_DROWSY, anomaly_sleep_get_learn_factor(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_LEARN_LIGHT_NREM, anomaly_sleep_get_learn_factor(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_LEARN_DEEP_NREM, anomaly_sleep_get_learn_factor(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_LEARN_REM, anomaly_sleep_get_learn_factor(SLEEP_STATE_REM));
}

TEST_F(AnomalyDetectorSleepBridgeTest, FPFactorForStates) {
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_FP_AWAKE, anomaly_sleep_get_fp_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_FP_DROWSY, anomaly_sleep_get_fp_factor(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_FP_LIGHT_NREM, anomaly_sleep_get_fp_factor(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_FP_DEEP_NREM, anomaly_sleep_get_fp_factor(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_FP_REM, anomaly_sleep_get_fp_factor(SLEEP_STATE_REM));
}

TEST_F(AnomalyDetectorSleepBridgeTest, ThresholdIncreaseDuringSleep) {
    // Threshold should increase during sleep (less sensitive)
    EXPECT_LE(ANOMALY_SLEEP_THRESH_AWAKE, ANOMALY_SLEEP_THRESH_DROWSY);
    EXPECT_LE(ANOMALY_SLEEP_THRESH_DROWSY, ANOMALY_SLEEP_THRESH_LIGHT_NREM);
    EXPECT_LE(ANOMALY_SLEEP_THRESH_LIGHT_NREM, ANOMALY_SLEEP_THRESH_DEEP_NREM);
}

TEST_F(AnomalyDetectorSleepBridgeTest, LearningDecreasesDuringSleep) {
    // Learning rate should decrease during sleep
    EXPECT_GE(ANOMALY_SLEEP_LEARN_AWAKE, ANOMALY_SLEEP_LEARN_DROWSY);
    EXPECT_GE(ANOMALY_SLEEP_LEARN_DROWSY, ANOMALY_SLEEP_LEARN_LIGHT_NREM);
    EXPECT_GE(ANOMALY_SLEEP_LEARN_LIGHT_NREM, ANOMALY_SLEEP_LEARN_DEEP_NREM);
}

/* =============================================================================
 * Rate Limiter Sleep Bridge Tests
 * ============================================================================= */

class RateLimiterSleepBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&config, 0, sizeof(config));
    }

    rate_limiter_sleep_config_t config;
};

TEST_F(RateLimiterSleepBridgeTest, DefaultConfigValid) {
    EXPECT_EQ(0, rate_limiter_sleep_default_config(&config));
    EXPECT_TRUE(config.enable_rate_modulation);
    EXPECT_TRUE(config.enable_burst_modulation);
    EXPECT_FLOAT_EQ(1.0f, config.modulation_strength);
}

TEST_F(RateLimiterSleepBridgeTest, DefaultConfigNullFails) {
    EXPECT_EQ(-1, rate_limiter_sleep_default_config(nullptr));
}

TEST_F(RateLimiterSleepBridgeTest, CreateFailsWithNullConfig) {
    rate_limiter_sleep_bridge_t b = rate_limiter_sleep_bridge_create(nullptr, nullptr);
    EXPECT_EQ(nullptr, b);
}

TEST_F(RateLimiterSleepBridgeTest, RateFactorForStates) {
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_RATE_AWAKE, rate_limiter_sleep_get_rate_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_RATE_DROWSY, rate_limiter_sleep_get_rate_factor(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_RATE_LIGHT_NREM, rate_limiter_sleep_get_rate_factor(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_RATE_DEEP_NREM, rate_limiter_sleep_get_rate_factor(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_RATE_REM, rate_limiter_sleep_get_rate_factor(SLEEP_STATE_REM));
}

TEST_F(RateLimiterSleepBridgeTest, BurstFactorForStates) {
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_BURST_AWAKE, rate_limiter_sleep_get_burst_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_BURST_DROWSY, rate_limiter_sleep_get_burst_factor(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_BURST_LIGHT_NREM, rate_limiter_sleep_get_burst_factor(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_BURST_DEEP_NREM, rate_limiter_sleep_get_burst_factor(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_BURST_REM, rate_limiter_sleep_get_burst_factor(SLEEP_STATE_REM));
}

TEST_F(RateLimiterSleepBridgeTest, RateLimitRelaxesDuringSleep) {
    // Rate limits should relax (increase) during sleep
    EXPECT_LE(RATE_LIMITER_SLEEP_RATE_AWAKE, RATE_LIMITER_SLEEP_RATE_DROWSY);
    EXPECT_LE(RATE_LIMITER_SLEEP_RATE_DROWSY, RATE_LIMITER_SLEEP_RATE_LIGHT_NREM);
    EXPECT_LE(RATE_LIMITER_SLEEP_RATE_LIGHT_NREM, RATE_LIMITER_SLEEP_RATE_DEEP_NREM);
}

TEST_F(RateLimiterSleepBridgeTest, BurstCapacityIncreasesDuringSleep) {
    // Burst capacity should increase during sleep
    EXPECT_LE(RATE_LIMITER_SLEEP_BURST_AWAKE, RATE_LIMITER_SLEEP_BURST_DROWSY);
    EXPECT_LE(RATE_LIMITER_SLEEP_BURST_DROWSY, RATE_LIMITER_SLEEP_BURST_LIGHT_NREM);
    EXPECT_LE(RATE_LIMITER_SLEEP_BURST_LIGHT_NREM, RATE_LIMITER_SLEEP_BURST_DEEP_NREM);
}

/* =============================================================================
 * Pattern DB Sleep Bridge Tests
 * ============================================================================= */

class PatternDBSleepBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&config, 0, sizeof(config));
    }

    pattern_db_sleep_config_t config;
};

TEST_F(PatternDBSleepBridgeTest, DefaultConfigValid) {
    EXPECT_EQ(0, pattern_db_sleep_default_config(&config));
    EXPECT_TRUE(config.enable_confidence_modulation);
    EXPECT_TRUE(config.enable_priority_modulation);
    EXPECT_FLOAT_EQ(1.0f, config.modulation_strength);
}

TEST_F(PatternDBSleepBridgeTest, DefaultConfigNullFails) {
    EXPECT_EQ(-1, pattern_db_sleep_default_config(nullptr));
}

TEST_F(PatternDBSleepBridgeTest, CreateFailsWithNullConfig) {
    pattern_db_sleep_bridge_t b = pattern_db_sleep_bridge_create(nullptr, nullptr);
    EXPECT_EQ(nullptr, b);
}

TEST_F(PatternDBSleepBridgeTest, ConfidenceFactorForStates) {
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_CONF_AWAKE, pattern_db_sleep_get_conf_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_CONF_DROWSY, pattern_db_sleep_get_conf_factor(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_CONF_LIGHT_NREM, pattern_db_sleep_get_conf_factor(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_CONF_DEEP_NREM, pattern_db_sleep_get_conf_factor(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_CONF_REM, pattern_db_sleep_get_conf_factor(SLEEP_STATE_REM));
}

TEST_F(PatternDBSleepBridgeTest, PriorityFactorForStates) {
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_PRIO_AWAKE, pattern_db_sleep_get_prio_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_PRIO_DROWSY, pattern_db_sleep_get_prio_factor(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_PRIO_LIGHT_NREM, pattern_db_sleep_get_prio_factor(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_PRIO_DEEP_NREM, pattern_db_sleep_get_prio_factor(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_PRIO_REM, pattern_db_sleep_get_prio_factor(SLEEP_STATE_REM));
}

TEST_F(PatternDBSleepBridgeTest, ConfidenceDecreasesDuringSleep) {
    // Confidence requirements should decrease during sleep
    EXPECT_GE(PATTERN_DB_SLEEP_CONF_AWAKE, PATTERN_DB_SLEEP_CONF_DROWSY);
    EXPECT_GE(PATTERN_DB_SLEEP_CONF_DROWSY, PATTERN_DB_SLEEP_CONF_LIGHT_NREM);
    EXPECT_GE(PATTERN_DB_SLEEP_CONF_LIGHT_NREM, PATTERN_DB_SLEEP_CONF_DEEP_NREM);
}

TEST_F(PatternDBSleepBridgeTest, PriorityIncreasesDuringSleep) {
    // Priority threshold should increase during sleep (focus on high-priority)
    EXPECT_LE(PATTERN_DB_SLEEP_PRIO_AWAKE, PATTERN_DB_SLEEP_PRIO_DROWSY);
    EXPECT_LE(PATTERN_DB_SLEEP_PRIO_DROWSY, PATTERN_DB_SLEEP_PRIO_LIGHT_NREM);
    EXPECT_LE(PATTERN_DB_SLEEP_PRIO_LIGHT_NREM, PATTERN_DB_SLEEP_PRIO_DEEP_NREM);
}

/* =============================================================================
 * Null Safety Tests
 * ============================================================================= */

TEST(SecuritySleepNullSafetyTest, BBBBridgeNullHandling) {
    EXPECT_EQ(-1, bbb_sleep_default_config(nullptr));
    EXPECT_EQ(nullptr, bbb_sleep_bridge_create(nullptr, nullptr));
    bbb_sleep_bridge_destroy(nullptr);  // Should not crash
    EXPECT_EQ(-1, bbb_sleep_update(nullptr));
    EXPECT_EQ(-1, bbb_sleep_get_effects(nullptr, nullptr));
    EXPECT_FLOAT_EQ(1.0f, bbb_sleep_get_permeability(nullptr));
    EXPECT_FALSE(bbb_sleep_is_glymphatic_active(nullptr));
}

TEST(SecuritySleepNullSafetyTest, AnomalyDetectorBridgeNullHandling) {
    EXPECT_EQ(-1, anomaly_detector_sleep_default_config(nullptr));
    EXPECT_EQ(nullptr, anomaly_detector_sleep_bridge_create(nullptr, nullptr));
    anomaly_detector_sleep_bridge_destroy(nullptr);  // Should not crash
    EXPECT_EQ(-1, anomaly_detector_sleep_update(nullptr));
    EXPECT_EQ(-1, anomaly_detector_sleep_get_effects(nullptr, nullptr));
    // Returns true as safe default (don't block learning)
    EXPECT_TRUE(anomaly_detector_sleep_is_learning_enabled(nullptr));
}

TEST(SecuritySleepNullSafetyTest, RateLimiterBridgeNullHandling) {
    EXPECT_EQ(-1, rate_limiter_sleep_default_config(nullptr));
    EXPECT_EQ(nullptr, rate_limiter_sleep_bridge_create(nullptr, nullptr));
    rate_limiter_sleep_bridge_destroy(nullptr);  // Should not crash
    EXPECT_EQ(-1, rate_limiter_sleep_update(nullptr));
    EXPECT_EQ(-1, rate_limiter_sleep_get_effects(nullptr, nullptr));
    EXPECT_FALSE(rate_limiter_sleep_is_relaxed(nullptr));
}

TEST(SecuritySleepNullSafetyTest, PatternDBBridgeNullHandling) {
    EXPECT_EQ(-1, pattern_db_sleep_default_config(nullptr));
    EXPECT_EQ(nullptr, pattern_db_sleep_bridge_create(nullptr, nullptr));
    pattern_db_sleep_bridge_destroy(nullptr);  // Should not crash
    EXPECT_EQ(-1, pattern_db_sleep_update(nullptr));
    EXPECT_EQ(-1, pattern_db_sleep_get_effects(nullptr, nullptr));
    EXPECT_FALSE(pattern_db_sleep_is_consolidating(nullptr));
    EXPECT_TRUE(pattern_db_sleep_allow_updates(nullptr));  // Safe default
}

/* =============================================================================
 * Constant Value Validation Tests
 * ============================================================================= */

TEST(SecuritySleepConstantsTest, BBBPermeabilityValuesReasonable) {
    // All values should be positive
    EXPECT_GT(BBB_SLEEP_PERMEABILITY_AWAKE, 0.0f);
    EXPECT_GT(BBB_SLEEP_PERMEABILITY_DROWSY, 0.0f);
    EXPECT_GT(BBB_SLEEP_PERMEABILITY_LIGHT_NREM, 0.0f);
    EXPECT_GT(BBB_SLEEP_PERMEABILITY_DEEP_NREM, 0.0f);
    EXPECT_GT(BBB_SLEEP_PERMEABILITY_REM, 0.0f);

    // Awake should be baseline (1.0)
    EXPECT_FLOAT_EQ(1.0f, BBB_SLEEP_PERMEABILITY_AWAKE);
}

TEST(SecuritySleepConstantsTest, AnomalyThresholdValuesReasonable) {
    // All values should be positive
    EXPECT_GT(ANOMALY_SLEEP_THRESH_AWAKE, 0.0f);
    EXPECT_GT(ANOMALY_SLEEP_THRESH_DROWSY, 0.0f);
    EXPECT_GT(ANOMALY_SLEEP_THRESH_LIGHT_NREM, 0.0f);
    EXPECT_GT(ANOMALY_SLEEP_THRESH_DEEP_NREM, 0.0f);
    EXPECT_GT(ANOMALY_SLEEP_THRESH_REM, 0.0f);

    // Awake should be baseline (1.0)
    EXPECT_FLOAT_EQ(1.0f, ANOMALY_SLEEP_THRESH_AWAKE);
}

TEST(SecuritySleepConstantsTest, RateLimiterValuesReasonable) {
    // All values should be positive
    EXPECT_GT(RATE_LIMITER_SLEEP_RATE_AWAKE, 0.0f);
    EXPECT_GT(RATE_LIMITER_SLEEP_RATE_DROWSY, 0.0f);
    EXPECT_GT(RATE_LIMITER_SLEEP_RATE_LIGHT_NREM, 0.0f);
    EXPECT_GT(RATE_LIMITER_SLEEP_RATE_DEEP_NREM, 0.0f);
    EXPECT_GT(RATE_LIMITER_SLEEP_RATE_REM, 0.0f);

    // Awake should be baseline (1.0)
    EXPECT_FLOAT_EQ(1.0f, RATE_LIMITER_SLEEP_RATE_AWAKE);
}

TEST(SecuritySleepConstantsTest, PatternDBValuesReasonable) {
    // All values should be positive
    EXPECT_GT(PATTERN_DB_SLEEP_CONF_AWAKE, 0.0f);
    EXPECT_GT(PATTERN_DB_SLEEP_CONF_DROWSY, 0.0f);
    EXPECT_GT(PATTERN_DB_SLEEP_CONF_LIGHT_NREM, 0.0f);
    EXPECT_GT(PATTERN_DB_SLEEP_CONF_DEEP_NREM, 0.0f);
    EXPECT_GT(PATTERN_DB_SLEEP_CONF_REM, 0.0f);

    // Awake should be baseline (1.0)
    EXPECT_FLOAT_EQ(1.0f, PATTERN_DB_SLEEP_CONF_AWAKE);
}

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
