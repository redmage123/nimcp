//=============================================================================
// test_dragonfly_parameters.cpp - Parameter Regression Tests
//=============================================================================
/**
 * @file test_dragonfly_parameters.cpp
 * @brief Regression tests ensuring biological parameters remain stable
 *
 * WHAT: Tests that biological tuning parameters have expected values
 * WHY:  Parameters are tuned to match dragonfly neuroscience data
 * HOW:  Check default config values against known biological values
 *
 * BIOLOGICAL REFERENCES:
 * - Mischiati et al. (2015): Dragonfly internal models for prey interception
 * - Wiederman & O'Carroll (2013): STMD neurons and size selectivity
 * - Gonzalez-Bellido et al. (2011): TSDN population vector encoding
 *
 * IMPORTANT: These tests guard against accidental parameter changes that
 * would degrade biological fidelity. Changes require neuroscience justification.
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "dragonfly/nimcp_dragonfly.h"
#include "dragonfly/nimcp_dragonfly_tsdn.h"
#include "dragonfly/nimcp_dragonfly_tracking.h"
#include "dragonfly/nimcp_dragonfly_prediction.h"
#include "dragonfly/nimcp_dragonfly_intercept.h"

//=============================================================================
// TSDN Parameter Regression Tests
//=============================================================================

class TSDNParameterRegressionTest : public ::testing::Test {
protected:
    tsdn_config_t config;

    void SetUp() override {
        tsdn_config_default(&config);
    }
};

TEST_F(TSDNParameterRegressionTest, TuningWidthBiologicallyCorrect) {
    // TSDN neurons have ~45-60 degree tuning width (Gonzalez-Bellido 2011)
    // 45 degrees = 0.785 radians, 60 degrees = 1.047 radians
    EXPECT_GE(config.tuning_width, 0.5f) << "Tuning width should be >= 0.5 rad (~29 deg)";
    EXPECT_LE(config.tuning_width, 1.2f) << "Tuning width should be <= 1.2 rad (~69 deg)";
}

TEST_F(TSDNParameterRegressionTest, BaselineNoiseBiological) {
    // Biological neurons have ~5-15% baseline noise
    EXPECT_GE(config.baseline_noise, 0.01f) << "Baseline noise should be >= 1%";
    EXPECT_LE(config.baseline_noise, 0.20f) << "Baseline noise should be <= 20%";
}

TEST_F(TSDNParameterRegressionTest, AdaptationRateBiological) {
    // Neural adaptation typically 1-50 Hz
    EXPECT_GE(config.adaptation_rate, 0.5f) << "Adaptation rate should be >= 0.5 Hz";
    EXPECT_LE(config.adaptation_rate, 100.0f) << "Adaptation rate should be <= 100 Hz";
}

TEST_F(TSDNParameterRegressionTest, GainMultiplierReasonable) {
    // Gain should be positive and not extreme
    EXPECT_GE(config.gain, 0.5f) << "Gain should be >= 0.5";
    EXPECT_LE(config.gain, 5.0f) << "Gain should be <= 5.0";
}

//=============================================================================
// Tracking Parameter Regression Tests
//=============================================================================

class TrackingParameterRegressionTest : public ::testing::Test {
protected:
    tracking_config_t config;

    void SetUp() override {
        config = tracking_default_config();
    }
};

TEST_F(TrackingParameterRegressionTest, LockThresholdReasonable) {
    // Lock threshold should be conservative (high confidence required)
    EXPECT_GE(config.lock_threshold, 0.5f) << "Lock threshold should be >= 0.5";
    EXPECT_LE(config.lock_threshold, 0.95f) << "Lock threshold should be <= 0.95";
}

TEST_F(TrackingParameterRegressionTest, AcquisitionTimeRealistic) {
    // Dragonflies acquire targets in 10-50ms
    EXPECT_GE(config.acquisition_time_ms, 5.0f) << "Acquisition time should be >= 5ms";
    EXPECT_LE(config.acquisition_time_ms, 100.0f) << "Acquisition time should be <= 100ms";
}

TEST_F(TrackingParameterRegressionTest, PredictionHorizonReasonable) {
    // Prediction horizon 50-300ms is biologically realistic
    EXPECT_GE(config.prediction_horizon_ms, 20.0f) << "Prediction horizon should be >= 20ms";
    EXPECT_LE(config.prediction_horizon_ms, 500.0f) << "Prediction horizon should be <= 500ms";
}

TEST_F(TrackingParameterRegressionTest, SizeSelectivityMatchesBiology) {
    // Dragonfly STMD neurons tuned to 1-10 degree targets
    // At 1m distance: 1 deg = 0.0175 rad, 10 deg = 0.175 rad
    EXPECT_GE(config.min_target_size, 0.005f) << "Min size should be >= 0.005 rad";
    EXPECT_LE(config.max_target_size, 0.3f) << "Max size should be <= 0.3 rad";
    EXPECT_LT(config.min_target_size, config.max_target_size) << "Min < Max";
}

TEST_F(TrackingParameterRegressionTest, DistractorSuppressionReasonable) {
    // Suppression should be significant but not complete
    EXPECT_GE(config.distractor_suppression, 0.3f) << "Suppression should be >= 0.3";
    EXPECT_LE(config.distractor_suppression, 0.95f) << "Suppression should be <= 0.95";
}

//=============================================================================
// Prediction Parameter Regression Tests
//=============================================================================

class PredictionParameterRegressionTest : public ::testing::Test {
protected:
    prediction_config_t config;

    void SetUp() override {
        config = prediction_default_config();
    }
};

TEST_F(PredictionParameterRegressionTest, MaxPredictionTimeReasonable) {
    // Dragonflies predict 100-300ms ahead (Mischiati 2015)
    EXPECT_GE(config.max_prediction_ms, 50.0f) << "Max prediction should be >= 50ms";
    EXPECT_LE(config.max_prediction_ms, 500.0f) << "Max prediction should be <= 500ms";
}

TEST_F(PredictionParameterRegressionTest, PredictionStepsReasonable) {
    // Discrete prediction steps for trajectory planning
    EXPECT_GE(config.prediction_steps, 3) << "Should have >= 3 prediction steps";
    EXPECT_LE(config.prediction_steps, 50) << "Should have <= 50 prediction steps";
}

TEST_F(PredictionParameterRegressionTest, ProcessNoiseReasonable) {
    // Kalman filter process noise (motion model uncertainty)
    EXPECT_GT(config.process_noise, 0.0f) << "Process noise should be > 0";
    EXPECT_LT(config.process_noise, 10.0f) << "Process noise should be < 10";
}

TEST_F(PredictionParameterRegressionTest, MeasurementNoiseReasonable) {
    // Kalman filter measurement noise (observation uncertainty)
    EXPECT_GT(config.measurement_noise, 0.0f) << "Measurement noise should be > 0";
    EXPECT_LT(config.measurement_noise, 10.0f) << "Measurement noise should be < 10";
}

//=============================================================================
// Interception Parameter Regression Tests
//=============================================================================

class InterceptParameterRegressionTest : public ::testing::Test {
protected:
    intercept_config_t config;

    void SetUp() override {
        config = intercept_default_config();
    }
};

TEST_F(InterceptParameterRegressionTest, PNGainMatchesMissileGuidance) {
    // Proportional Navigation gain N typically 3-5 for effective interception
    // Dragonflies use similar gain (Mischiati 2015)
    EXPECT_GE(config.pn_gain, 2.0f) << "PN gain should be >= 2.0";
    EXPECT_LE(config.pn_gain, 6.0f) << "PN gain should be <= 6.0";
}

TEST_F(InterceptParameterRegressionTest, LeadTimeFactorReasonable) {
    // Lead time factor for interception planning
    EXPECT_GE(config.lead_time_factor, 0.5f) << "Lead time factor should be >= 0.5";
    EXPECT_LE(config.lead_time_factor, 3.0f) << "Lead time factor should be <= 3.0";
}

TEST_F(InterceptParameterRegressionTest, SafetyMarginPositive) {
    // Safety margin for constraint satisfaction
    EXPECT_GT(config.safety_margin, 0.0f) << "Safety margin should be > 0";
    EXPECT_LT(config.safety_margin, 2.0f) << "Safety margin should be < 2.0";
}

TEST_F(InterceptParameterRegressionTest, MinInterceptTimePositive) {
    // Minimum time to plan intercept
    EXPECT_GT(config.min_intercept_time_s, 0.0f) << "Min intercept time should be > 0";
    EXPECT_LT(config.min_intercept_time_s, 1.0f) << "Min intercept time should be < 1s";
}

//=============================================================================
// System Configuration Regression Tests
//=============================================================================

class SystemConfigRegressionTest : public ::testing::Test {
protected:
    dragonfly_config_t config;

    void SetUp() override {
        config = dragonfly_default_config();
    }
};

TEST_F(SystemConfigRegressionTest, MinTargetSizePositive) {
    EXPECT_GT(config.min_target_size, 0.0f) << "Min target size should be positive";
}

TEST_F(SystemConfigRegressionTest, MaxTargetDistanceReasonable) {
    // Dragonflies engage prey at 1-30 meters
    EXPECT_GE(config.max_target_distance, 5.0f) << "Max distance should be >= 5m";
    EXPECT_LE(config.max_target_distance, 100.0f) << "Max distance should be <= 100m";
}

TEST_F(SystemConfigRegressionTest, PursuitTimeoutReasonable) {
    // Dragonfly pursuits typically 0.5-5 seconds
    EXPECT_GE(config.pursuit_timeout_s, 0.5f) << "Pursuit timeout should be >= 0.5s";
    EXPECT_LE(config.pursuit_timeout_s, 30.0f) << "Pursuit timeout should be <= 30s";
}

TEST_F(SystemConfigRegressionTest, LockThresholdHigherThanPursue) {
    EXPECT_GE(config.lock_threshold, config.pursue_threshold)
        << "Lock threshold should be >= pursue threshold";
}

TEST_F(SystemConfigRegressionTest, AbortThresholdLowerThanLock) {
    EXPECT_LE(config.abort_threshold, config.lock_threshold)
        << "Abort threshold should be <= lock threshold";
}

//=============================================================================
// Cross-Module Consistency Tests
//=============================================================================

TEST(CrossModuleConsistencyTest, TSDNConfigMatchesSystemConfig) {
    dragonfly_config_t sys_config = dragonfly_default_config();
    tsdn_config_t tsdn_config = sys_config.tsdn_config;

    // TSDN config should be consistent with system expectations
    EXPECT_GT(tsdn_config.tuning_width, 0.0f);
    EXPECT_GT(tsdn_config.gain, 0.0f);
}

TEST(CrossModuleConsistencyTest, TrackingConfigMatchesSystemConfig) {
    dragonfly_config_t sys_config = dragonfly_default_config();
    tracking_config_t track_config = sys_config.tracker_config;

    // Tracking size selectivity should match system min/max
    EXPECT_LE(track_config.min_target_size, sys_config.min_target_size * 2.0f);
}

TEST(CrossModuleConsistencyTest, PredictionConfigMatchesSystemConfig) {
    dragonfly_config_t sys_config = dragonfly_default_config();
    prediction_config_t pred_config = sys_config.prediction_config;

    // Prediction horizon should be reasonable for pursuit timeout
    float max_prediction_s = pred_config.max_prediction_ms / 1000.0f;
    EXPECT_LE(max_prediction_s, sys_config.pursuit_timeout_s);
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST(BackwardCompatibilityTest, DefaultConfigReturnsValidConfig) {
    dragonfly_config_t config = dragonfly_default_config();
    EXPECT_TRUE(dragonfly_validate_config(&config))
        << "Default config must be valid";
}

TEST(BackwardCompatibilityTest, DefaultTSDNConfigValid) {
    tsdn_config_t config;
    tsdn_config_default(&config);
    EXPECT_EQ(tsdn_config_validate(&config), 0)
        << "Default TSDN config must be valid";
}

TEST(BackwardCompatibilityTest, DefaultTrackingConfigValid) {
    tracking_config_t config = tracking_default_config();
    EXPECT_TRUE(tracking_validate_config(&config))
        << "Default tracking config must be valid";
}

TEST(BackwardCompatibilityTest, DefaultPredictionConfigValid) {
    prediction_config_t config = prediction_default_config();
    EXPECT_TRUE(prediction_validate_config(&config))
        << "Default prediction config must be valid";
}

TEST(BackwardCompatibilityTest, DefaultInterceptConfigValid) {
    intercept_config_t config = intercept_default_config();
    EXPECT_TRUE(intercept_validate_config(&config))
        << "Default intercept config must be valid";
}

TEST(BackwardCompatibilityTest, IMMConfigValid) {
    prediction_config_t config = prediction_imm_config();
    EXPECT_TRUE(prediction_validate_config(&config))
        << "IMM config must be valid";
    EXPECT_TRUE(config.enable_imm) << "IMM config should enable IMM";
}
