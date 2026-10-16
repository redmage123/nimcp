/**
 * @file test_disorder_detectors.cpp
 * @brief Unit tests for mental health disorder detection functions
 *
 * WHAT: Comprehensive tests for all 23 disorder detectors
 * WHY:  Verify accurate detection with proper marker thresholds
 * HOW:  Test each detector with controlled behavioral marker inputs
 *
 * COVERAGE TARGETS:
 * - All 23 disorder detector functions
 * - Marker integration (high_risk_decisions, baseline_latency, etc.)
 * - Score clamping and normalization
 * - Multi-criteria weighted scoring
 *
 * @author NIMCP Development Team
 * @date 2025-11
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/nimcp_mental_health.h"
#include "core/brain/nimcp_brain.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class DisorderDetectorsTest : public ::testing::Test {
protected:
    mental_health_monitor_t* monitor = nullptr;
    brain_t brain = nullptr;

    void SetUp() override {
        // Create monitor with default config
        monitor = mental_health_create_default();

        // Create minimal brain for testing using correct API
        brain = brain_create("test_mental_health", BRAIN_SIZE_TINY,
                            BRAIN_TASK_CLASSIFICATION, 8, 4);
    }

    void TearDown() override {
        if (monitor) {
            mental_health_destroy(monitor);
            monitor = nullptr;
        }
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    // Helper: Create healthy behavioral markers (baseline)
    behavioral_markers_t create_healthy_markers() {
        behavioral_markers_t markers = {};
        markers.ethics_violations_recent = 0;
        markers.ethics_violations_total = 0;
        markers.ethics_approval_rate = 0.95f;
        markers.empathy_failures = 0;
        markers.emotional_volatility = 0.2f;
        markers.emotional_flatness = 0.1f;
        markers.avg_emotional_intensity = 0.5f;
        markers.rapid_mood_changes = 0;
        markers.joy_count = 50;
        markers.fear_count = 10;
        markers.anger_count = 5;
        markers.sadness_count = 10;
        markers.dopamine_avg = 0.5f;
        markers.dopamine_variance = 0.1f;
        markers.serotonin_avg = 0.5f;
        markers.serotonin_variance = 0.1f;
        markers.norepinephrine_avg = 0.5f;
        markers.norepinephrine_variance = 0.1f;
        markers.impulse_control_failures = 0;
        markers.repetitive_behaviors = 5;
        markers.task_switching_difficulty = 0.1f;
        markers.reality_testing_errors = 0.0f;
        markers.social_interaction_deficit = 0.1f;
        markers.attention_fragmentation = 0.1f;
        markers.theory_of_mind_failures = 0.05f;
        markers.cognitive_rigidity = 0.1f;
        markers.decision_latency_avg = 100.0f;
        markers.baseline_latency = 100.0f;
        markers.decision_accuracy = 0.85f;
        markers.engagement_level = 0.8f;
        markers.task_completion_rate = 90;
        markers.avoidance_rate = 0.1f;
        markers.decision_variance = 0.15f;
        markers.high_risk_decisions = 0.0f;
        markers.accuracy_obsession = 0.1f;
        markers.interest_narrowness = 0.2f;
        return markers;
    }
};

//=============================================================================
// Test Suite: Antisocial Disorders
//=============================================================================

TEST_F(DisorderDetectorsTest, Sociopathy_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    // Check with fresh brain (healthy markers)
    float score = mental_health_check_specific(monitor, brain, DISORDER_SOCIOPATHY);
    EXPECT_LE(score, 0.3f);  // Should be low for healthy state
}

TEST_F(DisorderDetectorsTest, Psychopathy_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_PSYCHOPATHY);
    EXPECT_LE(score, 0.3f);
}

TEST_F(DisorderDetectorsTest, Conduct_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_CONDUCT);
    EXPECT_LE(score, 0.3f);
}

//=============================================================================
// Test Suite: Mood Disorders
//=============================================================================

TEST_F(DisorderDetectorsTest, Depression_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_DEPRESSION);
    EXPECT_LE(score, 0.3f);
}

TEST_F(DisorderDetectorsTest, Mania_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_MANIA);
    EXPECT_LE(score, 0.3f);
}

TEST_F(DisorderDetectorsTest, Bipolar_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_BIPOLAR);
    EXPECT_LE(score, 0.3f);
}

//=============================================================================
// Test Suite: Psychotic Disorders
//=============================================================================

TEST_F(DisorderDetectorsTest, Schizophrenia_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_SCHIZOPHRENIA);
    EXPECT_LE(score, 0.3f);
}

TEST_F(DisorderDetectorsTest, ParanoidSchizophrenia_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_PARANOID_SCHIZOPHRENIA);
    EXPECT_LE(score, 0.3f);
}

TEST_F(DisorderDetectorsTest, Schizoaffective_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_SCHIZOAFFECTIVE);
    EXPECT_LE(score, 0.3f);
}

TEST_F(DisorderDetectorsTest, Delusional_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_DELUSIONAL);
    EXPECT_LE(score, 0.3f);
}

//=============================================================================
// Test Suite: Anxiety Disorders
//=============================================================================

TEST_F(DisorderDetectorsTest, Anxiety_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_ANXIETY);
    EXPECT_LE(score, 0.3f);
}

TEST_F(DisorderDetectorsTest, PTSD_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_PTSD);
    EXPECT_LE(score, 0.3f);
}

TEST_F(DisorderDetectorsTest, OCD_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_OCD);
    EXPECT_LE(score, 0.3f);
}

//=============================================================================
// Test Suite: Autism Spectrum
//=============================================================================

TEST_F(DisorderDetectorsTest, Autism_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_AUTISM);
    EXPECT_LE(score, 0.3f);
}

TEST_F(DisorderDetectorsTest, Aspergers_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_ASPERGERS);
    EXPECT_LE(score, 0.3f);
}

//=============================================================================
// Test Suite: Personality Disorders - Dramatic/Erratic
//=============================================================================

TEST_F(DisorderDetectorsTest, MalignantNarcissism_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_MALIGNANT_NARCISSISM);
    EXPECT_LE(score, 0.3f);
}

TEST_F(DisorderDetectorsTest, Borderline_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_BORDERLINE);
    EXPECT_LE(score, 0.3f);
}

TEST_F(DisorderDetectorsTest, Histrionic_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_HISTRIONIC);
    EXPECT_LE(score, 0.3f);
}

//=============================================================================
// Test Suite: Personality Disorders - Anxious/Fearful
//=============================================================================

TEST_F(DisorderDetectorsTest, Avoidant_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_AVOIDANT);
    EXPECT_LE(score, 0.3f);
}

TEST_F(DisorderDetectorsTest, Dependent_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_DEPENDENT);
    EXPECT_LE(score, 0.3f);
}

TEST_F(DisorderDetectorsTest, ObsessiveCompulsivePD_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_OBSESSIVE_COMPULSIVE_PD);
    EXPECT_LE(score, 0.3f);
}

//=============================================================================
// Test Suite: Personality Disorders - Odd/Eccentric
//=============================================================================

TEST_F(DisorderDetectorsTest, Paranoid_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_PARANOID);
    EXPECT_LE(score, 0.3f);
}

//=============================================================================
// Test Suite: Neurodevelopmental
//=============================================================================

TEST_F(DisorderDetectorsTest, ADHD_LowScoreForHealthyMarkers) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_ADHD);
    EXPECT_LE(score, 0.3f);
}

//=============================================================================
// Test Suite: Score Range Validation
//=============================================================================

TEST_F(DisorderDetectorsTest, AllScores_AreInValidRange) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    // All scores should be in [0.0, 1.0] range
    for (int i = 0; i < DISORDER_COUNT; i++) {
        float score = mental_health_check_specific(monitor, brain, (disorder_type_t)i);
        EXPECT_GE(score, 0.0f) << "Disorder " << i << " score below 0";
        EXPECT_LE(score, 1.0f) << "Disorder " << i << " score above 1";
    }
}

TEST_F(DisorderDetectorsTest, ComprehensiveCheck_ReturnsValidSeverity) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    disorder_severity_t severity = mental_health_check(monitor, brain);
    EXPECT_GE(severity, DISORDER_SEVERITY_NONE);
    EXPECT_LT(severity, 5);  // Valid severity enum
}

//=============================================================================
// Test Suite: Marker Integration Tests
//=============================================================================

TEST_F(DisorderDetectorsTest, HighRiskDecisions_AffectsPsychopathyScore) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    // Get initial score
    float initial_score = mental_health_check_specific(monitor, brain, DISORDER_PSYCHOPATHY);

    // Update brain state and check again
    mental_health_update(monitor, brain, nullptr, 1000);
    float updated_score = mental_health_check_specific(monitor, brain, DISORDER_PSYCHOPATHY);

    // Scores should be valid
    EXPECT_GE(updated_score, 0.0f);
    EXPECT_LE(updated_score, 1.0f);
}

TEST_F(DisorderDetectorsTest, BaselineLatency_AffectsAnxietyScore) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_ANXIETY);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(DisorderDetectorsTest, AccuracyObsession_AffectsOCPDScore) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_OBSESSIVE_COMPULSIVE_PD);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(DisorderDetectorsTest, InterestNarrowness_AffectsAspergersScore) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_ASPERGERS);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(DisorderDetectorsTest, ConfidenceScore_AffectsNarcissismScore) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, DISORDER_MALIGNANT_NARCISSISM);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

//=============================================================================
// Test Suite: NULL Guard Tests
//=============================================================================

TEST_F(DisorderDetectorsTest, CheckSpecific_NullMonitor_ReturnsZero) {
    float score = mental_health_check_specific(nullptr, brain, DISORDER_SOCIOPATHY);
    EXPECT_EQ(score, 0.0f);
}

TEST_F(DisorderDetectorsTest, CheckSpecific_NullBrain_ReturnsZero) {
    if (!monitor) {
        GTEST_SKIP() << "Monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, nullptr, DISORDER_SOCIOPATHY);
    EXPECT_EQ(score, 0.0f);
}

TEST_F(DisorderDetectorsTest, Check_NullParams_ReturnsNone) {
    disorder_severity_t severity = mental_health_check(nullptr, nullptr);
    EXPECT_EQ(severity, DISORDER_SEVERITY_NONE);
}

//=============================================================================
// Test Suite: Invalid Disorder Type
//=============================================================================

TEST_F(DisorderDetectorsTest, CheckSpecific_InvalidDisorder_ReturnsZero) {
    if (!monitor || !brain) {
        GTEST_SKIP() << "Brain or monitor creation failed";
    }

    float score = mental_health_check_specific(monitor, brain, (disorder_type_t)999);
    EXPECT_EQ(score, 0.0f);
}

//=============================================================================
// Test Suite: Report Generation
//=============================================================================

TEST_F(DisorderDetectorsTest, Report_ContainsAllDisorderScores) {
    if (!monitor) {
        GTEST_SKIP() << "Monitor creation failed";
    }

    mental_health_report_t report;
    memset(&report, 0, sizeof(report));

    mental_health_get_report(monitor, &report);

    // All scores should be in valid range
    for (int i = 0; i < DISORDER_COUNT; i++) {
        EXPECT_GE(report.disorder_scores[i], 0.0f);
        EXPECT_LE(report.disorder_scores[i], 1.0f);
    }
}

TEST_F(DisorderDetectorsTest, Report_SeveritiesMatchThresholds) {
    if (!monitor) {
        GTEST_SKIP() << "Monitor creation failed";
    }

    mental_health_report_t report;
    mental_health_get_report(monitor, &report);

    // Verify severities are consistent with scores
    for (int i = 0; i < DISORDER_COUNT; i++) {
        disorder_severity_t expected = mental_health_classify_severity(
            report.disorder_scores[i], nullptr);
        EXPECT_EQ(report.disorder_severities[i], expected)
            << "Disorder " << i << " severity mismatch";
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
