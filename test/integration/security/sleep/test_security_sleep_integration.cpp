/**
 * @file test_security_sleep_integration.cpp
 * @brief Integration tests for security sleep bridge modules
 * @version 1.0.0
 * @date 2025-12-18
 *
 * Tests cross-module integration between security sleep bridges:
 * - State factor functions for all sleep states
 * - Coordinated security relaxation during sleep
 * - Default configuration validation
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "security/sleep/nimcp_bbb_sleep_bridge.h"
#include "security/sleep/nimcp_anomaly_detector_sleep_bridge.h"
#include "security/sleep/nimcp_rate_limiter_sleep_bridge.h"
#include "security/sleep/nimcp_pattern_db_sleep_bridge.h"

/* =============================================================================
 * State Factor Integration Tests
 * Tests that all bridges have coordinated factor functions
 * ============================================================================= */

class SecuritySleepFactorIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize states to test
        states = {
            SLEEP_STATE_AWAKE,
            SLEEP_STATE_DROWSY,
            SLEEP_STATE_LIGHT_NREM,
            SLEEP_STATE_DEEP_NREM,
            SLEEP_STATE_REM
        };
    }

    std::vector<sleep_state_t> states;
};

TEST_F(SecuritySleepFactorIntegrationTest, AllBBBFactorsArePositive) {
    for (sleep_state_t state : states) {
        EXPECT_GT(bbb_sleep_permeability_for_state(state), 0.0f)
            << "Failed for state: " << static_cast<int>(state);
        EXPECT_GT(bbb_sleep_detection_for_state(state), 0.0f)
            << "Failed for state: " << static_cast<int>(state);
        EXPECT_GT(bbb_sleep_urgency_for_state(state), 0.0f)
            << "Failed for state: " << static_cast<int>(state);
    }
}

TEST_F(SecuritySleepFactorIntegrationTest, AllAnomalyFactorsArePositive) {
    for (sleep_state_t state : states) {
        EXPECT_GT(anomaly_sleep_get_thresh_factor(state), 0.0f)
            << "Failed for state: " << static_cast<int>(state);
        EXPECT_GE(anomaly_sleep_get_learn_factor(state), 0.0f)
            << "Failed for state: " << static_cast<int>(state);
        EXPECT_GT(anomaly_sleep_get_fp_factor(state), 0.0f)
            << "Failed for state: " << static_cast<int>(state);
    }
}

TEST_F(SecuritySleepFactorIntegrationTest, AllRateLimiterFactorsArePositive) {
    for (sleep_state_t state : states) {
        EXPECT_GT(rate_limiter_sleep_get_rate_factor(state), 0.0f)
            << "Failed for state: " << static_cast<int>(state);
        EXPECT_GT(rate_limiter_sleep_get_burst_factor(state), 0.0f)
            << "Failed for state: " << static_cast<int>(state);
    }
}

TEST_F(SecuritySleepFactorIntegrationTest, AllPatternDBFactorsArePositive) {
    for (sleep_state_t state : states) {
        EXPECT_GT(pattern_db_sleep_get_conf_factor(state), 0.0f)
            << "Failed for state: " << static_cast<int>(state);
        EXPECT_GT(pattern_db_sleep_get_prio_factor(state), 0.0f)
            << "Failed for state: " << static_cast<int>(state);
    }
}

TEST_F(SecuritySleepFactorIntegrationTest, FactorsHaveReasonableBounds) {
    for (sleep_state_t state : states) {
        // BBB permeability should be between 0.5 and 2.0
        float perm = bbb_sleep_permeability_for_state(state);
        EXPECT_GE(perm, 0.5f);
        EXPECT_LE(perm, 2.0f);

        // Detection factor increases during sleep (1.0 awake to 1.5 deep NREM)
        float detect = bbb_sleep_detection_for_state(state);
        EXPECT_GE(detect, 1.0f);
        EXPECT_LE(detect, 1.6f);

        // Rate factor should be between 0.5 and 2.0
        float rate = rate_limiter_sleep_get_rate_factor(state);
        EXPECT_GE(rate, 0.5f);
        EXPECT_LE(rate, 2.0f);
    }
}

/* =============================================================================
 * Coordinated State Behavior Tests
 * ============================================================================= */

class SecuritySleepCoordinationTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(SecuritySleepCoordinationTest, DeepNREMHasMaxPermeability) {
    float awake_perm = bbb_sleep_permeability_for_state(SLEEP_STATE_AWAKE);
    float drowsy_perm = bbb_sleep_permeability_for_state(SLEEP_STATE_DROWSY);
    float light_perm = bbb_sleep_permeability_for_state(SLEEP_STATE_LIGHT_NREM);
    float deep_perm = bbb_sleep_permeability_for_state(SLEEP_STATE_DEEP_NREM);
    float rem_perm = bbb_sleep_permeability_for_state(SLEEP_STATE_REM);

    // Deep NREM should have highest permeability
    EXPECT_GE(deep_perm, awake_perm);
    EXPECT_GE(deep_perm, drowsy_perm);
    EXPECT_GE(deep_perm, light_perm);
    EXPECT_GE(deep_perm, rem_perm);
}

TEST_F(SecuritySleepCoordinationTest, AwakeHasLowestDetectionFactor) {
    float awake_detect = bbb_sleep_detection_for_state(SLEEP_STATE_AWAKE);
    float deep_detect = bbb_sleep_detection_for_state(SLEEP_STATE_DEEP_NREM);

    // During sleep, detection factor increases (relaxed monitoring)
    // Awake has lowest detection factor (tightest security)
    EXPECT_LE(awake_detect, deep_detect);
}

TEST_F(SecuritySleepCoordinationTest, SleepRelaxesRateLimits) {
    float awake_rate = rate_limiter_sleep_get_rate_factor(SLEEP_STATE_AWAKE);
    float deep_rate = rate_limiter_sleep_get_rate_factor(SLEEP_STATE_DEEP_NREM);

    // Deep NREM should have relaxed (higher) rate limits
    EXPECT_GE(deep_rate, awake_rate);
}

TEST_F(SecuritySleepCoordinationTest, SleepRaisesAnomalyThreshold) {
    float awake_thresh = anomaly_sleep_get_thresh_factor(SLEEP_STATE_AWAKE);
    float deep_thresh = anomaly_sleep_get_thresh_factor(SLEEP_STATE_DEEP_NREM);

    // Deep NREM should have higher threshold (less sensitive)
    EXPECT_GE(deep_thresh, awake_thresh);
}

TEST_F(SecuritySleepCoordinationTest, DeepNREMDisablesLearning) {
    float awake_learn = anomaly_sleep_get_learn_factor(SLEEP_STATE_AWAKE);
    float deep_learn = anomaly_sleep_get_learn_factor(SLEEP_STATE_DEEP_NREM);

    // Awake should have learning enabled
    EXPECT_GT(awake_learn, 0.0f);

    // Deep NREM should have learning disabled or reduced
    EXPECT_LT(deep_learn, awake_learn);
}

/* =============================================================================
 * Configuration Tests
 * ============================================================================= */

class SecuritySleepConfigTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(SecuritySleepConfigTest, BBBDefaultConfigValid) {
    bbb_sleep_config_t config;
    int result = bbb_sleep_default_config(&config);
    EXPECT_EQ(0, result);
    EXPECT_TRUE(config.enable_permeability_modulation);
}

TEST_F(SecuritySleepConfigTest, AnomalyDetectorDefaultConfigValid) {
    anomaly_detector_sleep_config_t config;
    int result = anomaly_detector_sleep_default_config(&config);
    EXPECT_EQ(0, result);
    EXPECT_TRUE(config.enable_threshold_modulation);
}

TEST_F(SecuritySleepConfigTest, RateLimiterDefaultConfigValid) {
    rate_limiter_sleep_config_t config;
    int result = rate_limiter_sleep_default_config(&config);
    EXPECT_EQ(0, result);
    EXPECT_TRUE(config.enable_rate_modulation);
}

TEST_F(SecuritySleepConfigTest, PatternDBDefaultConfigValid) {
    pattern_db_sleep_config_t config;
    int result = pattern_db_sleep_default_config(&config);
    EXPECT_EQ(0, result);
    EXPECT_TRUE(config.enable_confidence_modulation);
}

TEST_F(SecuritySleepConfigTest, NullConfigHandled) {
    EXPECT_EQ(-1, bbb_sleep_default_config(nullptr));
    EXPECT_EQ(-1, anomaly_detector_sleep_default_config(nullptr));
    EXPECT_EQ(-1, rate_limiter_sleep_default_config(nullptr));
    EXPECT_EQ(-1, pattern_db_sleep_default_config(nullptr));
}

/* =============================================================================
 * Sleep State Progression Tests
 * ============================================================================= */

class SecuritySleepProgressionTest : public ::testing::Test {
protected:
    void SetUp() override {}
};

TEST_F(SecuritySleepProgressionTest, PermeabilityProgressesCorrectly) {
    float awake = bbb_sleep_permeability_for_state(SLEEP_STATE_AWAKE);
    float drowsy = bbb_sleep_permeability_for_state(SLEEP_STATE_DROWSY);
    float light = bbb_sleep_permeability_for_state(SLEEP_STATE_LIGHT_NREM);
    float deep = bbb_sleep_permeability_for_state(SLEEP_STATE_DEEP_NREM);

    // Permeability should increase as sleep deepens
    EXPECT_LE(awake, drowsy);
    EXPECT_LE(drowsy, light);
    EXPECT_LE(light, deep);
}

TEST_F(SecuritySleepProgressionTest, DetectionFactorIncreasesIntoSleep) {
    float awake = bbb_sleep_detection_for_state(SLEEP_STATE_AWAKE);
    float deep = bbb_sleep_detection_for_state(SLEEP_STATE_DEEP_NREM);

    // Detection factor increases during sleep (relaxed threshold)
    EXPECT_LE(awake, deep);
}

TEST_F(SecuritySleepProgressionTest, REMHasIntermediateValues) {
    float awake = bbb_sleep_permeability_for_state(SLEEP_STATE_AWAKE);
    float deep = bbb_sleep_permeability_for_state(SLEEP_STATE_DEEP_NREM);
    float rem = bbb_sleep_permeability_for_state(SLEEP_STATE_REM);

    // REM should be between awake and deep NREM
    EXPECT_GE(rem, awake);
    EXPECT_LE(rem, deep);
}

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
