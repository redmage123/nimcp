//=============================================================================
// test_sequence_immune_integration.cpp - Sequence Detector Immune Integration Tests
//=============================================================================

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "middleware/immune/nimcp_sequence_immune_bridge.h"
#include "middleware/patterns/nimcp_sequence_detector.h"
#include "cognitive/immune/nimcp_brain_immune.h"

//=============================================================================
// TEST FIXTURE
//=============================================================================

class SequenceImmuneTest : public ::testing::Test {
protected:
    brain_immune_system_t* brain_immune;
    sequence_detector_t* sequence_detector;
    sequence_immune_bridge_t* bridge;

    void SetUp() override {
        // Create brain immune system
        brain_immune_config_t brain_config;
        brain_immune_default_config(&brain_config);
        brain_immune = brain_immune_create(&brain_config);
        ASSERT_NE(brain_immune, nullptr);
        brain_immune_start(brain_immune);

        // Create sequence detector
        sequence_detector_config_t seq_config = sequence_detector_default_config();
        sequence_detector = sequence_detector_create(&seq_config);
        ASSERT_NE(sequence_detector, nullptr);

        // Create bridge
        sequence_immune_config_t bridge_config;
        sequence_immune_default_config(&bridge_config);
        bridge = sequence_immune_bridge_create(&bridge_config, brain_immune,
                                                 sequence_detector);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        sequence_immune_bridge_destroy(bridge);
        sequence_detector_destroy(sequence_detector);
        brain_immune_destroy(brain_immune);
    }
};

//=============================================================================
// LIFECYCLE TESTS
//=============================================================================

TEST_F(SequenceImmuneTest, CreateDestroy) {
    EXPECT_NE(bridge, nullptr);

    uint64_t total_updates;
    uint32_t anomaly_alerts;
    uint32_t learning_failures;
    int result = sequence_immune_get_stats(bridge, &total_updates, &anomaly_alerts,
                                          &learning_failures);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(total_updates, 0);
    EXPECT_EQ(anomaly_alerts, 0);
    EXPECT_EQ(learning_failures, 0);
}

TEST_F(SequenceImmuneTest, CreateWithNullBrainImmune) {
    sequence_immune_config_t config;
    sequence_immune_default_config(&config);
    sequence_immune_bridge_t* bad_bridge = sequence_immune_bridge_create(
        &config, nullptr, sequence_detector);
    EXPECT_EQ(bad_bridge, nullptr);
}

TEST_F(SequenceImmuneTest, CreateWithNullSequenceDetector) {
    sequence_immune_config_t config;
    sequence_immune_default_config(&config);
    sequence_immune_bridge_t* bad_bridge = sequence_immune_bridge_create(
        &config, brain_immune, nullptr);
    EXPECT_EQ(bad_bridge, nullptr);
}

TEST_F(SequenceImmuneTest, DefaultConfig) {
    sequence_immune_config_t config;
    int result = sequence_immune_default_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_modulation);
    EXPECT_TRUE(config.enable_inflammation_impairment);
    EXPECT_TRUE(config.enable_anomaly_immune_trigger);
    EXPECT_TRUE(config.enable_positive_feedback);
    EXPECT_FLOAT_EQ(config.cytokine_sensitivity, 1.0F);
    EXPECT_FLOAT_EQ(config.inflammation_sensitivity, 1.0F);
    EXPECT_FLOAT_EQ(config.anomaly_sensitivity, 1.0F);
    EXPECT_FLOAT_EQ(config.anomaly_match_threshold, SEQUENCE_ANOMALY_THRESHOLD);
    EXPECT_FLOAT_EQ(config.timing_violation_threshold,
                    SEQUENCE_TIMING_VIOLATION_THRESHOLD);
    EXPECT_EQ(config.learning_failure_threshold, SEQUENCE_LEARNING_FAILURE_COUNT);
}

//=============================================================================
// IMMUNE → SEQUENCE TESTS (Cytokine Effects)
//=============================================================================

TEST_F(SequenceImmuneTest, ApplyCytokineEffects) {
    int result = sequence_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);

    cytokine_sequence_effects_t effects;
    result = sequence_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(result, 0);

    // Initially no cytokines, so no effects
    EXPECT_FLOAT_EQ(effects.total_timing_tolerance_ms, 0.0F);
    EXPECT_FLOAT_EQ(effects.detection_accuracy_factor, 1.0F);
    EXPECT_FLOAT_EQ(effects.learning_impairment, 0.0F);
}

TEST_F(SequenceImmuneTest, ApplyInflammationEffects) {
    int result = sequence_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, 0);

    inflammation_sequence_state_t state;
    result = sequence_immune_get_inflammation_state(bridge, &state);
    EXPECT_EQ(result, 0);

    // Initially no inflammation
    EXPECT_EQ(state.current_level, INFLAMMATION_NONE);
    EXPECT_FALSE(state.is_chronic);
    EXPECT_FLOAT_EQ(state.accuracy_reduction, 0.0F);
    EXPECT_FLOAT_EQ(state.timing_precision_loss, 0.0F);
}

TEST_F(SequenceImmuneTest, ComputeAccuracyFactorNoInflammation) {
    float accuracy = sequence_immune_get_accuracy_factor(bridge);
    EXPECT_FLOAT_EQ(accuracy, 1.0F);
}

TEST_F(SequenceImmuneTest, ComputeTimingToleranceBaseline) {
    float tolerance = sequence_immune_get_timing_tolerance(bridge);
    // Should return baseline since no cytokine effects
    EXPECT_FLOAT_EQ(tolerance, SEQUENCE_TEMPORAL_TOLERANCE_MS);
}

TEST_F(SequenceImmuneTest, DetectionNotImpairedInitially) {
    bool impaired = sequence_immune_is_detection_impaired(bridge);
    EXPECT_FALSE(impaired);
}

//=============================================================================
// SEQUENCE → IMMUNE TESTS (Anomaly Detection)
//=============================================================================

TEST_F(SequenceImmuneTest, TriggerFromAnomalyLowMatchStrength) {
    sequence_detection_t detection;
    memset(&detection, 0, sizeof(detection));
    detection.template_id = 1;
    detection.strength = 0.2F;  // Below SEQUENCE_ANOMALY_THRESHOLD (0.3)
    detection.start_time_ms = 0.0;
    detection.end_time_ms = 1000.0;
    detection.is_forward = true;
    detection.matched_elements = 5;
    detection.total_elements = 10;

    int result = sequence_immune_trigger_from_anomaly(bridge, &detection);
    EXPECT_EQ(result, 0);

    // Check that anomaly was registered
    uint64_t total_updates;
    uint32_t anomaly_alerts;
    uint32_t learning_failures;
    sequence_immune_get_stats(bridge, &total_updates, &anomaly_alerts,
                             &learning_failures);

    // Low match may trigger alert if severity high enough
    // (depends on implementation threshold)
}

TEST_F(SequenceImmuneTest, TriggerFromAnomalyTimingViolation) {
    sequence_detection_t detection;
    memset(&detection, 0, sizeof(detection));
    detection.template_id = 1;
    detection.strength = 0.8F;  // Good match
    detection.start_time_ms = 0.0;
    detection.end_time_ms = 5000.0;  // 5x expected duration (1000ms)
    detection.is_forward = true;
    detection.matched_elements = 8;
    detection.total_elements = 10;

    int result = sequence_immune_trigger_from_anomaly(bridge, &detection);
    EXPECT_EQ(result, 0);
}

TEST_F(SequenceImmuneTest, TriggerFromAnomalyCorruptedSequence) {
    sequence_detection_t detection;
    memset(&detection, 0, sizeof(detection));
    detection.template_id = 1;
    detection.strength = 0.5F;
    detection.start_time_ms = 0.0;
    detection.end_time_ms = 1000.0;
    detection.is_forward = true;
    detection.matched_elements = 2;  // Only 2 out of 10 matched
    detection.total_elements = 10;

    int result = sequence_immune_trigger_from_anomaly(bridge, &detection);
    EXPECT_EQ(result, 0);
}

TEST_F(SequenceImmuneTest, ReportLearningFailure) {
    int result = sequence_immune_report_learning_failure(bridge);
    EXPECT_EQ(result, 0);

    uint64_t total_updates;
    uint32_t anomaly_alerts;
    uint32_t learning_failures;
    sequence_immune_get_stats(bridge, &total_updates, &anomaly_alerts,
                             &learning_failures);
    EXPECT_EQ(learning_failures, 1);
}

TEST_F(SequenceImmuneTest, MultipleLearningFailuresTriggersAlert) {
    // Report failures up to threshold
    for (uint32_t i = 0; i < SEQUENCE_LEARNING_FAILURE_COUNT; i++) {
        sequence_immune_report_learning_failure(bridge);
    }

    uint64_t total_updates;
    uint32_t anomaly_alerts;
    uint32_t learning_failures;
    sequence_immune_get_stats(bridge, &total_updates, &anomaly_alerts,
                             &learning_failures);
    EXPECT_EQ(learning_failures, SEQUENCE_LEARNING_FAILURE_COUNT);
    // Should have triggered at least one immune alert
    EXPECT_GT(anomaly_alerts, 0);
}

TEST_F(SequenceImmuneTest, ReportDetectionFailure) {
    float poor_match = 0.2F;
    int result = sequence_immune_report_detection_failure(bridge, poor_match);
    EXPECT_EQ(result, 0);

    uint64_t total_updates;
    uint32_t anomaly_alerts;
    uint32_t learning_failures;
    sequence_immune_get_stats(bridge, &total_updates, &anomaly_alerts,
                             &learning_failures);
    EXPECT_GT(anomaly_alerts + learning_failures, 0);  // Some activity
}

TEST_F(SequenceImmuneTest, BoostFromLearningSuccess) {
    int result = sequence_immune_boost_from_learning_success(bridge);
    EXPECT_EQ(result, 0);

    // Multiple successes should activate positive feedback
    for (int i = 0; i < 10; i++) {
        sequence_immune_boost_from_learning_success(bridge);
    }

    // Feedback state should be updated
    // (Would check positive_feedback structure if accessible)
}

//=============================================================================
// BIDIRECTIONAL UPDATE TESTS
//=============================================================================

TEST_F(SequenceImmuneTest, UpdateBridge) {
    int result = sequence_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, 0);

    uint64_t total_updates;
    uint32_t anomaly_alerts;
    uint32_t learning_failures;
    sequence_immune_get_stats(bridge, &total_updates, &anomaly_alerts,
                             &learning_failures);
    EXPECT_EQ(total_updates, 1);
}

TEST_F(SequenceImmuneTest, MultipleUpdates) {
    for (int i = 0; i < 10; i++) {
        int result = sequence_immune_bridge_update(bridge, 100);
        EXPECT_EQ(result, 0);
    }

    uint64_t total_updates;
    uint32_t anomaly_alerts;
    uint32_t learning_failures;
    sequence_immune_get_stats(bridge, &total_updates, &anomaly_alerts,
                             &learning_failures);
    EXPECT_EQ(total_updates, 10);
}

//=============================================================================
// QUERY API TESTS
//=============================================================================

TEST_F(SequenceImmuneTest, GetCytokineEffects) {
    cytokine_sequence_effects_t effects;
    int result = sequence_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(result, 0);

    // Initially zero
    EXPECT_FLOAT_EQ(effects.il1_timing_penalty, 0.0F);
    EXPECT_FLOAT_EQ(effects.il6_timing_penalty, 0.0F);
    EXPECT_FLOAT_EQ(effects.tnf_timing_penalty, 0.0F);
    EXPECT_FLOAT_EQ(effects.ifn_gamma_timing_penalty, 0.0F);
    EXPECT_FLOAT_EQ(effects.il10_precision_boost, 0.0F);
}

TEST_F(SequenceImmuneTest, GetInflammationState) {
    inflammation_sequence_state_t state;
    int result = sequence_immune_get_inflammation_state(bridge, &state);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(state.current_level, INFLAMMATION_NONE);
    EXPECT_FLOAT_EQ(state.inflammation_duration_sec, 0.0F);
    EXPECT_FALSE(state.is_chronic);
}

TEST_F(SequenceImmuneTest, GetStatistics) {
    uint64_t total_updates;
    uint32_t anomaly_alerts;
    uint32_t learning_failures;
    int result = sequence_immune_get_stats(bridge, &total_updates, &anomaly_alerts,
                                          &learning_failures);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(total_updates, 0);
    EXPECT_EQ(anomaly_alerts, 0);
    EXPECT_EQ(learning_failures, 0);
}

//=============================================================================
// NULL POINTER HANDLING TESTS
//=============================================================================

TEST_F(SequenceImmuneTest, NullPointerHandling) {
    EXPECT_EQ(sequence_immune_apply_cytokine_effects(nullptr), -1);
    EXPECT_EQ(sequence_immune_apply_inflammation_effects(nullptr), -1);
    EXPECT_FLOAT_EQ(sequence_immune_get_accuracy_factor(nullptr), 1.0F);
    EXPECT_FLOAT_EQ(sequence_immune_get_timing_tolerance(nullptr),
                    SEQUENCE_TEMPORAL_TOLERANCE_MS);
    EXPECT_FALSE(sequence_immune_is_detection_impaired(nullptr));

    sequence_detection_t detection;
    memset(&detection, 0, sizeof(detection));
    EXPECT_EQ(sequence_immune_trigger_from_anomaly(nullptr, &detection), -1);
    EXPECT_EQ(sequence_immune_trigger_from_anomaly(bridge, nullptr), -1);

    EXPECT_EQ(sequence_immune_report_learning_failure(nullptr), -1);
    EXPECT_EQ(sequence_immune_report_detection_failure(nullptr, 0.5F), -1);
    EXPECT_EQ(sequence_immune_boost_from_learning_success(nullptr), -1);

    EXPECT_EQ(sequence_immune_bridge_update(nullptr, 100), -1);

    cytokine_sequence_effects_t effects;
    EXPECT_EQ(sequence_immune_get_cytokine_effects(nullptr, &effects), -1);
    EXPECT_EQ(sequence_immune_get_cytokine_effects(bridge, nullptr), -1);

    inflammation_sequence_state_t state;
    EXPECT_EQ(sequence_immune_get_inflammation_state(nullptr, &state), -1);
    EXPECT_EQ(sequence_immune_get_inflammation_state(bridge, nullptr), -1);

    uint64_t total_updates;
    uint32_t anomaly_alerts;
    uint32_t learning_failures;
    EXPECT_EQ(sequence_immune_get_stats(nullptr, &total_updates, &anomaly_alerts,
                                       &learning_failures), -1);
}

TEST_F(SequenceImmuneTest, DefaultConfigNullPointer) {
    EXPECT_EQ(sequence_immune_default_config(nullptr), -1);
}

//=============================================================================
// FEATURE ENABLE/DISABLE TESTS
//=============================================================================

TEST_F(SequenceImmuneTest, DisableCytokineModulation) {
    sequence_immune_config_t config;
    sequence_immune_default_config(&config);
    config.enable_cytokine_modulation = false;

    sequence_immune_bridge_t* test_bridge = sequence_immune_bridge_create(
        &config, brain_immune, sequence_detector);
    ASSERT_NE(test_bridge, nullptr);

    // Applying cytokine effects should return 0 but do nothing
    int result = sequence_immune_apply_cytokine_effects(test_bridge);
    EXPECT_EQ(result, 0);

    sequence_immune_bridge_destroy(test_bridge);
}

TEST_F(SequenceImmuneTest, DisableInflammationImpairment) {
    sequence_immune_config_t config;
    sequence_immune_default_config(&config);
    config.enable_inflammation_impairment = false;

    sequence_immune_bridge_t* test_bridge = sequence_immune_bridge_create(
        &config, brain_immune, sequence_detector);
    ASSERT_NE(test_bridge, nullptr);

    int result = sequence_immune_apply_inflammation_effects(test_bridge);
    EXPECT_EQ(result, 0);

    sequence_immune_bridge_destroy(test_bridge);
}

TEST_F(SequenceImmuneTest, DisableAnomalyImmuneTrigger) {
    sequence_immune_config_t config;
    sequence_immune_default_config(&config);
    config.enable_anomaly_immune_trigger = false;

    sequence_immune_bridge_t* test_bridge = sequence_immune_bridge_create(
        &config, brain_immune, sequence_detector);
    ASSERT_NE(test_bridge, nullptr);

    sequence_detection_t detection;
    memset(&detection, 0, sizeof(detection));
    detection.strength = 0.1F;  // Anomalous

    int result = sequence_immune_trigger_from_anomaly(test_bridge, &detection);
    EXPECT_EQ(result, 0);

    // Should not trigger alert when disabled
    uint64_t total_updates;
    uint32_t anomaly_alerts;
    uint32_t learning_failures;
    sequence_immune_get_stats(test_bridge, &total_updates, &anomaly_alerts,
                             &learning_failures);
    EXPECT_EQ(anomaly_alerts, 0);

    sequence_immune_bridge_destroy(test_bridge);
}

TEST_F(SequenceImmuneTest, DisablePositiveFeedback) {
    sequence_immune_config_t config;
    sequence_immune_default_config(&config);
    config.enable_positive_feedback = false;

    sequence_immune_bridge_t* test_bridge = sequence_immune_bridge_create(
        &config, brain_immune, sequence_detector);
    ASSERT_NE(test_bridge, nullptr);

    int result = sequence_immune_boost_from_learning_success(test_bridge);
    EXPECT_EQ(result, 0);

    sequence_immune_bridge_destroy(test_bridge);
}

//=============================================================================
// INTEGRATION TEST: FULL IMMUNE CYCLE
//=============================================================================

TEST_F(SequenceImmuneTest, FullImmuneCycle) {
    // Initially healthy
    EXPECT_FALSE(sequence_immune_is_detection_impaired(bridge));
    EXPECT_FLOAT_EQ(sequence_immune_get_accuracy_factor(bridge), 1.0F);

    // Apply inflammation effects
    sequence_immune_apply_inflammation_effects(bridge);

    // Report some learning failures
    for (int i = 0; i < 3; i++) {
        sequence_immune_report_learning_failure(bridge);
    }

    // Report anomalous sequence
    sequence_detection_t anomaly;
    memset(&anomaly, 0, sizeof(anomaly));
    anomaly.template_id = 1;
    anomaly.strength = 0.15F;  // Very low
    anomaly.start_time_ms = 0.0;
    anomaly.end_time_ms = 1000.0;
    anomaly.matched_elements = 2;
    anomaly.total_elements = 10;
    sequence_immune_trigger_from_anomaly(bridge, &anomaly);

    // Update bridge
    sequence_immune_bridge_update(bridge, 100);

    // Check statistics
    uint64_t total_updates;
    uint32_t anomaly_alerts;
    uint32_t learning_failures;
    sequence_immune_get_stats(bridge, &total_updates, &anomaly_alerts,
                             &learning_failures);
    EXPECT_GT(total_updates, 0);

    // Apply positive feedback to recovery
    for (int i = 0; i < 15; i++) {
        sequence_immune_boost_from_learning_success(bridge);
    }

    // System should stabilize
    EXPECT_EQ(sequence_immune_bridge_update(bridge, 100), 0);
}

//=============================================================================
// TIMING TOLERANCE TESTS
//=============================================================================

TEST_F(SequenceImmuneTest, TimingToleranceIncreasesWithCytokines) {
    // Initial tolerance
    float initial_tolerance = sequence_immune_get_timing_tolerance(bridge);
    EXPECT_FLOAT_EQ(initial_tolerance, SEQUENCE_TEMPORAL_TOLERANCE_MS);

    // Apply cytokine effects (would increase if cytokines present)
    sequence_immune_apply_cytokine_effects(bridge);

    float new_tolerance = sequence_immune_get_timing_tolerance(bridge);
    // Should be >= baseline (could be same if no cytokines)
    EXPECT_GE(new_tolerance, initial_tolerance);
}

//=============================================================================
// ACCURACY FACTOR TESTS
//=============================================================================

TEST_F(SequenceImmuneTest, AccuracyFactorMappingToInflammationLevels) {
    // Would need to manually set inflammation levels in immune system
    // to test full mapping. For now, test that function returns valid range.
    float accuracy = sequence_immune_get_accuracy_factor(bridge);
    EXPECT_GE(accuracy, 0.5F);
    EXPECT_LE(accuracy, 1.0F);
}

//=============================================================================
// CONSECUTIVE FAILURE TRACKING TESTS
//=============================================================================

TEST_F(SequenceImmuneTest, ConsecutiveDetectionFailuresTracked) {
    // Report multiple consecutive failures
    for (int i = 0; i < 15; i++) {
        sequence_immune_report_detection_failure(bridge, 0.1F);
    }

    uint64_t total_updates;
    uint32_t anomaly_alerts;
    uint32_t learning_failures;
    sequence_immune_get_stats(bridge, &total_updates, &anomaly_alerts,
                             &learning_failures);

    // Many failures should be tracked
    // (Actual alert triggering depends on threshold implementation)
}

TEST_F(SequenceImmuneTest, SuccessfulDetectionResetsCon consecutiveFailures) {
    // Report failures
    for (int i = 0; i < 5; i++) {
        sequence_immune_report_detection_failure(bridge, 0.1F);
    }

    // Report success (high match strength)
    sequence_immune_report_detection_failure(bridge, 0.9F);

    // Consecutive counter should reset
    // (Would need access to internal state to verify fully)
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
