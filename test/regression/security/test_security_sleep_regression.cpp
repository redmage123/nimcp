/**
 * @file test_security_sleep_regression.cpp
 * @brief Regression tests for security sleep bridge modules
 * @version 1.0.0
 * @date 2025-12-18
 *
 * Tests stability and correctness of:
 * - BBB Sleep Bridge factor functions
 * - Anomaly Detector Sleep Bridge factor functions
 * - Rate Limiter Sleep Bridge factor functions
 * - Pattern DB Sleep Bridge factor functions
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "security/sleep/nimcp_bbb_sleep_bridge.h"
#include "security/sleep/nimcp_anomaly_detector_sleep_bridge.h"
#include "security/sleep/nimcp_rate_limiter_sleep_bridge.h"
#include "security/sleep/nimcp_pattern_db_sleep_bridge.h"

/* =============================================================================
 * BBB Sleep Bridge Regression Tests
 * ============================================================================= */

class BBBSleepRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(BBBSleepRegressionTest, DefaultConfigStability) {
    bbb_sleep_config_t config;
    EXPECT_EQ(0, bbb_sleep_default_config(&config));
    EXPECT_TRUE(config.enable_permeability_modulation);
    EXPECT_TRUE(config.enable_detection_modulation);
    EXPECT_TRUE(config.enable_response_modulation);
    EXPECT_FLOAT_EQ(1.0f, config.modulation_strength);
    EXPECT_TRUE(config.maintain_critical_protection);
}

TEST_F(BBBSleepRegressionTest, NullConfigHandled) {
    EXPECT_EQ(-1, bbb_sleep_default_config(nullptr));
}

TEST_F(BBBSleepRegressionTest, PermeabilityFactorsStable) {
    // Verify expected values haven't changed
    EXPECT_FLOAT_EQ(BBB_SLEEP_PERMEABILITY_AWAKE,
                    bbb_sleep_permeability_for_state(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(BBB_SLEEP_PERMEABILITY_DROWSY,
                    bbb_sleep_permeability_for_state(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(BBB_SLEEP_PERMEABILITY_LIGHT_NREM,
                    bbb_sleep_permeability_for_state(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(BBB_SLEEP_PERMEABILITY_DEEP_NREM,
                    bbb_sleep_permeability_for_state(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(BBB_SLEEP_PERMEABILITY_REM,
                    bbb_sleep_permeability_for_state(SLEEP_STATE_REM));
}

TEST_F(BBBSleepRegressionTest, DetectionFactorsStable) {
    EXPECT_FLOAT_EQ(BBB_SLEEP_DETECTION_AWAKE,
                    bbb_sleep_detection_for_state(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(BBB_SLEEP_DETECTION_DROWSY,
                    bbb_sleep_detection_for_state(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(BBB_SLEEP_DETECTION_LIGHT_NREM,
                    bbb_sleep_detection_for_state(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(BBB_SLEEP_DETECTION_DEEP_NREM,
                    bbb_sleep_detection_for_state(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(BBB_SLEEP_DETECTION_REM,
                    bbb_sleep_detection_for_state(SLEEP_STATE_REM));
}

TEST_F(BBBSleepRegressionTest, UrgencyFactorsStable) {
    EXPECT_FLOAT_EQ(BBB_SLEEP_URGENCY_AWAKE,
                    bbb_sleep_urgency_for_state(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(BBB_SLEEP_URGENCY_DROWSY,
                    bbb_sleep_urgency_for_state(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(BBB_SLEEP_URGENCY_LIGHT_NREM,
                    bbb_sleep_urgency_for_state(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(BBB_SLEEP_URGENCY_DEEP_NREM,
                    bbb_sleep_urgency_for_state(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(BBB_SLEEP_URGENCY_REM,
                    bbb_sleep_urgency_for_state(SLEEP_STATE_REM));
}

TEST_F(BBBSleepRegressionTest, AllFactorsPositive) {
    for (int i = 0; i < 5; i++) {
        sleep_state_t state = static_cast<sleep_state_t>(i);
        EXPECT_GT(bbb_sleep_permeability_for_state(state), 0.0f);
        EXPECT_GT(bbb_sleep_detection_for_state(state), 0.0f);
        EXPECT_GT(bbb_sleep_urgency_for_state(state), 0.0f);
    }
}

/* =============================================================================
 * Anomaly Detector Sleep Bridge Regression Tests
 * ============================================================================= */

class AnomalySleepRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(AnomalySleepRegressionTest, DefaultConfigStability) {
    anomaly_detector_sleep_config_t config;
    EXPECT_EQ(0, anomaly_detector_sleep_default_config(&config));
    EXPECT_TRUE(config.enable_threshold_modulation);
}

TEST_F(AnomalySleepRegressionTest, NullConfigHandled) {
    EXPECT_EQ(-1, anomaly_detector_sleep_default_config(nullptr));
}

TEST_F(AnomalySleepRegressionTest, ThresholdFactorsStable) {
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_THRESH_AWAKE,
                    anomaly_sleep_get_thresh_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_THRESH_DROWSY,
                    anomaly_sleep_get_thresh_factor(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_THRESH_LIGHT_NREM,
                    anomaly_sleep_get_thresh_factor(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_THRESH_DEEP_NREM,
                    anomaly_sleep_get_thresh_factor(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_THRESH_REM,
                    anomaly_sleep_get_thresh_factor(SLEEP_STATE_REM));
}

TEST_F(AnomalySleepRegressionTest, LearningFactorsStable) {
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_LEARN_AWAKE,
                    anomaly_sleep_get_learn_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_LEARN_DROWSY,
                    anomaly_sleep_get_learn_factor(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_LEARN_LIGHT_NREM,
                    anomaly_sleep_get_learn_factor(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_LEARN_DEEP_NREM,
                    anomaly_sleep_get_learn_factor(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(ANOMALY_SLEEP_LEARN_REM,
                    anomaly_sleep_get_learn_factor(SLEEP_STATE_REM));
}

TEST_F(AnomalySleepRegressionTest, AllFactorsNonNegative) {
    for (int i = 0; i < 5; i++) {
        sleep_state_t state = static_cast<sleep_state_t>(i);
        EXPECT_GT(anomaly_sleep_get_thresh_factor(state), 0.0f);
        EXPECT_GE(anomaly_sleep_get_learn_factor(state), 0.0f);
        EXPECT_GT(anomaly_sleep_get_fp_factor(state), 0.0f);
    }
}

/* =============================================================================
 * Rate Limiter Sleep Bridge Regression Tests
 * ============================================================================= */

class RateLimiterSleepRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(RateLimiterSleepRegressionTest, DefaultConfigStability) {
    rate_limiter_sleep_config_t config;
    EXPECT_EQ(0, rate_limiter_sleep_default_config(&config));
    EXPECT_TRUE(config.enable_rate_modulation);
}

TEST_F(RateLimiterSleepRegressionTest, NullConfigHandled) {
    EXPECT_EQ(-1, rate_limiter_sleep_default_config(nullptr));
}

TEST_F(RateLimiterSleepRegressionTest, RateFactorsStable) {
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_RATE_AWAKE,
                    rate_limiter_sleep_get_rate_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_RATE_DROWSY,
                    rate_limiter_sleep_get_rate_factor(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_RATE_LIGHT_NREM,
                    rate_limiter_sleep_get_rate_factor(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_RATE_DEEP_NREM,
                    rate_limiter_sleep_get_rate_factor(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_RATE_REM,
                    rate_limiter_sleep_get_rate_factor(SLEEP_STATE_REM));
}

TEST_F(RateLimiterSleepRegressionTest, BurstFactorsStable) {
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_BURST_AWAKE,
                    rate_limiter_sleep_get_burst_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_BURST_DROWSY,
                    rate_limiter_sleep_get_burst_factor(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_BURST_LIGHT_NREM,
                    rate_limiter_sleep_get_burst_factor(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_BURST_DEEP_NREM,
                    rate_limiter_sleep_get_burst_factor(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(RATE_LIMITER_SLEEP_BURST_REM,
                    rate_limiter_sleep_get_burst_factor(SLEEP_STATE_REM));
}

TEST_F(RateLimiterSleepRegressionTest, AllFactorsPositive) {
    for (int i = 0; i < 5; i++) {
        sleep_state_t state = static_cast<sleep_state_t>(i);
        EXPECT_GT(rate_limiter_sleep_get_rate_factor(state), 0.0f);
        EXPECT_GT(rate_limiter_sleep_get_burst_factor(state), 0.0f);
    }
}

/* =============================================================================
 * Pattern DB Sleep Bridge Regression Tests
 * ============================================================================= */

class PatternDBSleepRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(PatternDBSleepRegressionTest, DefaultConfigStability) {
    pattern_db_sleep_config_t config;
    EXPECT_EQ(0, pattern_db_sleep_default_config(&config));
    EXPECT_TRUE(config.enable_confidence_modulation);
}

TEST_F(PatternDBSleepRegressionTest, NullConfigHandled) {
    EXPECT_EQ(-1, pattern_db_sleep_default_config(nullptr));
}

TEST_F(PatternDBSleepRegressionTest, ConfidenceFactorsStable) {
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_CONF_AWAKE,
                    pattern_db_sleep_get_conf_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_CONF_DROWSY,
                    pattern_db_sleep_get_conf_factor(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_CONF_LIGHT_NREM,
                    pattern_db_sleep_get_conf_factor(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_CONF_DEEP_NREM,
                    pattern_db_sleep_get_conf_factor(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_CONF_REM,
                    pattern_db_sleep_get_conf_factor(SLEEP_STATE_REM));
}

TEST_F(PatternDBSleepRegressionTest, PriorityFactorsStable) {
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_PRIO_AWAKE,
                    pattern_db_sleep_get_prio_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_PRIO_DROWSY,
                    pattern_db_sleep_get_prio_factor(SLEEP_STATE_DROWSY));
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_PRIO_LIGHT_NREM,
                    pattern_db_sleep_get_prio_factor(SLEEP_STATE_LIGHT_NREM));
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_PRIO_DEEP_NREM,
                    pattern_db_sleep_get_prio_factor(SLEEP_STATE_DEEP_NREM));
    EXPECT_FLOAT_EQ(PATTERN_DB_SLEEP_PRIO_REM,
                    pattern_db_sleep_get_prio_factor(SLEEP_STATE_REM));
}

TEST_F(PatternDBSleepRegressionTest, AllFactorsPositive) {
    for (int i = 0; i < 5; i++) {
        sleep_state_t state = static_cast<sleep_state_t>(i);
        EXPECT_GT(pattern_db_sleep_get_conf_factor(state), 0.0f);
        EXPECT_GT(pattern_db_sleep_get_prio_factor(state), 0.0f);
    }
}

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
