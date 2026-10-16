/**
 * @file test_training_diagnosis.cpp
 * @brief Comprehensive unit tests for Abductive Training Diagnosis
 *
 * TEST COVERAGE:
 * - Lifecycle (CreateDestroy)
 * - Empty state (NoObservationsNoDiagnosis)
 * - Gradient explosion detection & diagnosis
 * - Gradient vanishing detection & diagnosis
 * - Loss NaN detection & diagnosis
 * - Loss oscillation detection & diagnosis
 * - Loss plateau detection & diagnosis
 * - High inflammation detection & diagnosis
 * - Multiple simultaneous observations
 * - Recommended actions correctness
 * - Reset between steps
 * - NULL safety
 * - Observation name coverage
 * - Plausibility bounds
 * - LR factor bounds
 *
 * TOTAL: 15 tests
 *
 * @version 1.0.0
 * @date 2026-02-26
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <cstdio>

extern "C" {
#include "middleware/training/nimcp_training_diagnosis.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class TrainingDiagnosisTest : public ::testing::Test {
protected:
    training_diagnoser_t* diag;

    void SetUp() override {
        diag = training_diagnoser_create();
        ASSERT_NE(diag, nullptr);
    }

    void TearDown() override {
        if (diag) {
            training_diagnoser_destroy(diag);
            diag = nullptr;
        }
    }

    /* Helper: observe with "normal" metrics (no anomalies) */
    void observe_normal() {
        training_diagnoser_observe_from_metrics(
            diag,
            /* loss_current */ 0.3f, /* loss_previous */ 0.31f,
            /* grad_norm */ 0.5f, /* grad_norm_previous */ 0.48f,
            /* loss_volatility */ 0.05f, /* gradient_variance */ 0.1f,
            /* learning_rate */ 0.001f, /* batch_size */ 32.0f,
            /* arousal */ 0.5f, /* inflammation */ 0.1f,
            /* resource_pressure */ 0.3f
        );
    }
};

/*=============================================================================
 * 1. LIFECYCLE: CreateDestroy
 *===========================================================================*/

TEST_F(TrainingDiagnosisTest, CreateDestroy) {
    /* diag created in SetUp, verify non-null */
    EXPECT_NE(diag, nullptr);

    /* Destroy and set to null to avoid double-free in TearDown */
    training_diagnoser_destroy(diag);
    diag = nullptr;

    /* Verify NULL-safe destroy */
    training_diagnoser_destroy(nullptr);
}

/*=============================================================================
 * 2. EMPTY STATE: NoObservationsNoDiagnosis
 *===========================================================================*/

TEST_F(TrainingDiagnosisTest, NoObservationsNoDiagnosis) {
    training_diagnosis_t result;
    memset(&result, 0xFF, sizeof(result));

    int rc = training_diagnoser_diagnose(diag, &result);
    EXPECT_EQ(rc, 0);

    /* Should have sensible defaults */
    EXPECT_EQ(result.num_observations, 0u);
    EXPECT_FLOAT_EQ(result.primary_plausibility, 0.0f);
    EXPECT_STREQ(result.primary_cause, "no anomalies detected");
    EXPECT_FALSE(result.recommend_reduce_lr);
    EXPECT_FALSE(result.recommend_increase_batch);
    EXPECT_FALSE(result.recommend_tighter_clip);
    EXPECT_FALSE(result.recommend_pause);
    EXPECT_FALSE(result.recommend_rollback);
}

/*=============================================================================
 * 3. GRADIENT EXPLOSION: diagnosed when grad_norm doubles
 *===========================================================================*/

TEST_F(TrainingDiagnosisTest, GradientExplosion) {
    /* grad_norm = 10.0, previous = 5.0 -> 2x increase > 1.5x threshold */
    int rc = training_diagnoser_observe_from_metrics(
        diag,
        0.5f, 0.45f,         /* loss current/previous */
        10.0f, 5.0f,          /* grad_norm current/previous (doubling) */
        0.05f, 0.1f,          /* volatility, variance */
        0.01f, 32.0f,         /* lr, batch */
        0.5f, 0.1f, 0.3f     /* arousal, inflammation, resource */
    );
    EXPECT_EQ(rc, 0);

    training_diagnosis_t result;
    rc = training_diagnoser_diagnose(diag, &result);
    EXPECT_EQ(rc, 0);

    /* Should have detected at least the gradient increasing observation */
    EXPECT_GT(result.num_observations, 0u);
    EXPECT_GT(result.num_hypotheses, 0u);

    /* Primary cause should mention gradient or learning rate */
    EXPECT_GT(strlen(result.primary_cause), 0u);
    EXPECT_GT(result.primary_plausibility, 0.0f);

    /* Should recommend reducing learning rate (from observation-based fallback) */
    EXPECT_TRUE(result.recommend_reduce_lr);
}

/*=============================================================================
 * 4. GRADIENT VANISHING: near-zero grad_norm
 *===========================================================================*/

TEST_F(TrainingDiagnosisTest, GradientVanishing) {
    int rc = training_diagnoser_observe_from_metrics(
        diag,
        0.8f, 0.8f,          /* loss stalled */
        0.0001f, 0.001f,     /* grad_norm near zero */
        0.005f, 0.01f,       /* low volatility, low variance */
        0.001f, 32.0f,
        0.5f, 0.1f, 0.3f
    );
    EXPECT_EQ(rc, 0);

    training_diagnosis_t result;
    rc = training_diagnoser_diagnose(diag, &result);
    EXPECT_EQ(rc, 0);

    EXPECT_GT(result.num_observations, 0u);
    EXPECT_GT(strlen(result.primary_cause), 0u);
}

/*=============================================================================
 * 5. LOSS NAN: severe numerical instability
 *===========================================================================*/

TEST_F(TrainingDiagnosisTest, LossNaN) {
    int rc = training_diagnoser_observe_from_metrics(
        diag,
        NAN, 0.5f,           /* NaN loss */
        5.0f, 3.0f,
        0.1f, 0.2f,
        0.01f, 32.0f,
        0.5f, 0.1f, 0.3f
    );
    EXPECT_EQ(rc, 0);

    training_diagnosis_t result;
    rc = training_diagnoser_diagnose(diag, &result);
    EXPECT_EQ(rc, 0);

    EXPECT_GT(result.num_observations, 0u);

    /* NaN should trigger rollback recommendation (from observation-based fallback) */
    EXPECT_TRUE(result.recommend_rollback);
}

/*=============================================================================
 * 6. LOSS OSCILLATION: volatile loss
 *===========================================================================*/

TEST_F(TrainingDiagnosisTest, LossOscillation) {
    int rc = training_diagnoser_observe_from_metrics(
        diag,
        0.6f, 0.4f,          /* loss jumping around */
        1.0f, 0.9f,
        0.5f, 0.2f,          /* high volatility (0.5 > 0.3 threshold) */
        0.01f, 16.0f,
        0.5f, 0.1f, 0.3f
    );
    EXPECT_EQ(rc, 0);

    training_diagnosis_t result;
    rc = training_diagnoser_diagnose(diag, &result);
    EXPECT_EQ(rc, 0);

    EXPECT_GT(result.num_observations, 0u);

    /* Oscillation should recommend increasing batch size (from fallback) */
    EXPECT_TRUE(result.recommend_increase_batch);
}

/*=============================================================================
 * 7. LOSS PLATEAU: stalled training
 *===========================================================================*/

TEST_F(TrainingDiagnosisTest, LossPlateau) {
    int rc = training_diagnoser_observe_from_metrics(
        diag,
        0.7f, 0.7f,          /* loss stuck */
        0.01f, 0.01f,        /* small gradients (also vanishing) */
        0.005f, 0.01f,       /* very low volatility + high loss */
        0.001f, 32.0f,
        0.5f, 0.1f, 0.3f
    );
    EXPECT_EQ(rc, 0);

    training_diagnosis_t result;
    rc = training_diagnoser_diagnose(diag, &result);
    EXPECT_EQ(rc, 0);

    /* Should have detected plateau (and possibly vanishing gradient) */
    EXPECT_GT(result.num_observations, 0u);
    EXPECT_GT(strlen(result.primary_cause), 0u);
}

/*=============================================================================
 * 8. HIGH INFLAMMATION: immune system interference
 *===========================================================================*/

TEST_F(TrainingDiagnosisTest, HighInflammation) {
    int rc = training_diagnoser_observe_from_metrics(
        diag,
        0.5f, 0.48f,
        0.5f, 0.48f,
        0.05f, 0.1f,
        0.001f, 32.0f,
        0.5f, 0.8f, 0.3f    /* high inflammation (0.8 > 0.5) */
    );
    EXPECT_EQ(rc, 0);

    training_diagnosis_t result;
    rc = training_diagnoser_diagnose(diag, &result);
    EXPECT_EQ(rc, 0);

    EXPECT_GT(result.num_observations, 0u);
    EXPECT_GT(strlen(result.primary_cause), 0u);
}

/*=============================================================================
 * 9. MULTIPLE OBSERVATIONS: several simultaneous symptoms
 *===========================================================================*/

TEST_F(TrainingDiagnosisTest, MultipleObservations) {
    /* Engineer metrics that trigger multiple observations at once:
     * - gradient increasing (10.0 > 5.0 * 1.5)
     * - loss increasing (0.8 > 0.5 * 1.1)
     * - high variance (0.8 > 0.5)
     * - high inflammation (0.7 > 0.5)
     * - high resource pressure (0.9 > 0.7)
     */
    int rc = training_diagnoser_observe_from_metrics(
        diag,
        0.8f, 0.5f,          /* loss increasing */
        10.0f, 5.0f,         /* gradient increasing */
        0.1f, 0.8f,          /* high gradient variance */
        0.01f, 32.0f,
        0.5f, 0.7f, 0.9f    /* high inflammation + resource pressure */
    );
    EXPECT_EQ(rc, 0);

    training_diagnosis_t result;
    rc = training_diagnoser_diagnose(diag, &result);
    EXPECT_EQ(rc, 0);

    /* Should have detected at least 4 observations */
    EXPECT_GE(result.num_observations, 4u);
    EXPECT_GT(result.num_hypotheses, 0u);
    EXPECT_GT(strlen(result.primary_cause), 0u);
    EXPECT_GT(result.primary_plausibility, 0.0f);
}

/*=============================================================================
 * 10. RECOMMENDED ACTIONS: verify correct actions for each diagnosis type
 *===========================================================================*/

TEST_F(TrainingDiagnosisTest, RecommendedActions) {
    /* NaN loss -> should recommend rollback */
    int rc = training_diagnoser_observe_from_metrics(
        diag,
        INFINITY, 0.5f,      /* Inf loss */
        100.0f, 5.0f,        /* massive gradient */
        0.8f, 0.9f,          /* high volatility + variance */
        0.1f, 32.0f,
        0.5f, 0.1f, 0.3f
    );
    EXPECT_EQ(rc, 0);

    training_diagnosis_t result;
    rc = training_diagnoser_diagnose(diag, &result);
    EXPECT_EQ(rc, 0);

    /* NaN/Inf -> rollback */
    EXPECT_TRUE(result.recommend_rollback);

    /* Gradient explosion -> reduce LR */
    EXPECT_TRUE(result.recommend_reduce_lr);
    EXPECT_LT(result.recommended_lr_factor, 1.0f);
}

/*=============================================================================
 * 11. RESET CLEARS STATE: reset between training steps
 *===========================================================================*/

TEST_F(TrainingDiagnosisTest, ResetClearsState) {
    /* First: observe some anomalies */
    training_diagnoser_observe_from_metrics(
        diag,
        NAN, 0.5f,
        10.0f, 5.0f,
        0.5f, 0.8f,
        0.01f, 32.0f,
        0.5f, 0.8f, 0.9f
    );

    training_diagnosis_t result1;
    training_diagnoser_diagnose(diag, &result1);
    EXPECT_GT(result1.num_observations, 0u);

    /* Reset */
    int rc = training_diagnoser_reset(diag);
    EXPECT_EQ(rc, 0);

    /* Diagnose again with no new observations */
    training_diagnosis_t result2;
    rc = training_diagnoser_diagnose(diag, &result2);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(result2.num_observations, 0u);
    EXPECT_STREQ(result2.primary_cause, "no anomalies detected");
}

/*=============================================================================
 * 12. NULL SAFETY: NULL params handled gracefully
 *===========================================================================*/

TEST_F(TrainingDiagnosisTest, NullSafety) {
    /* NULL diagnoser */
    EXPECT_EQ(training_diagnoser_observe_from_metrics(
        nullptr, 0.5f, 0.5f, 0.5f, 0.5f, 0.1f, 0.1f,
        0.001f, 32.0f, 0.5f, 0.1f, 0.3f), -1);

    training_diagnosis_t result;
    EXPECT_EQ(training_diagnoser_diagnose(nullptr, &result), -1);
    EXPECT_EQ(training_diagnoser_diagnose(diag, nullptr), -1);
    EXPECT_EQ(training_diagnoser_reset(nullptr), -1);

    /* NULL-safe destroy */
    training_diagnoser_destroy(nullptr);
}

/*=============================================================================
 * 13. OBSERVATION NAMES: all names are non-NULL
 *===========================================================================*/

TEST_F(TrainingDiagnosisTest, ObservationNames) {
    for (int i = 0; i < TRAIN_OBS_COUNT; i++) {
        const char* name = training_diagnosis_observation_name(
            (training_observation_type_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }

    /* Out-of-range should return "unknown" */
    const char* unknown = training_diagnosis_observation_name(TRAIN_OBS_COUNT);
    EXPECT_NE(unknown, nullptr);
    EXPECT_STREQ(unknown, "unknown");

    const char* unknown2 = training_diagnosis_observation_name(
        (training_observation_type_t)999);
    EXPECT_STREQ(unknown2, "unknown");
}

/*=============================================================================
 * 14. PLAUSIBILITY BOUNDED: plausibility in [0,1]
 *===========================================================================*/

TEST_F(TrainingDiagnosisTest, PlausibilityBounded) {
    /* Observe some anomalies to get a real diagnosis */
    training_diagnoser_observe_from_metrics(
        diag,
        0.8f, 0.5f,
        10.0f, 5.0f,
        0.5f, 0.8f,
        0.01f, 32.0f,
        0.9f, 0.8f, 0.9f
    );

    training_diagnosis_t result;
    int rc = training_diagnoser_diagnose(diag, &result);
    EXPECT_EQ(rc, 0);

    EXPECT_GE(result.primary_plausibility, 0.0f);
    EXPECT_LE(result.primary_plausibility, 1.0f);
    EXPECT_GE(result.secondary_plausibility, 0.0f);
    EXPECT_LE(result.secondary_plausibility, 1.0f);
}

/*=============================================================================
 * 15. LR FACTOR REASONABLE: recommended lr_factor in [0.01, 1.0]
 *===========================================================================*/

TEST_F(TrainingDiagnosisTest, LrFactorReasonable) {
    /* Trigger gradient explosion for LR reduction recommendation */
    training_diagnoser_observe_from_metrics(
        diag,
        0.8f, 0.5f,
        100.0f, 5.0f,         /* massive gradient explosion */
        0.5f, 0.9f,
        0.1f, 32.0f,
        0.5f, 0.1f, 0.3f
    );

    training_diagnosis_t result;
    int rc = training_diagnoser_diagnose(diag, &result);
    EXPECT_EQ(rc, 0);

    /* LR factor should be in [0.01, 1.0] */
    EXPECT_GE(result.recommended_lr_factor, 0.01f);
    EXPECT_LE(result.recommended_lr_factor, 1.0f);

    /* Reset and test with no anomalies: lr_factor should be 1.0 (no change) */
    training_diagnoser_reset(diag);
    observe_normal();

    training_diagnosis_t result_normal;
    rc = training_diagnoser_diagnose(diag, &result_normal);
    EXPECT_EQ(rc, 0);

    /* With normal metrics, lr_factor should remain at 1.0 */
    EXPECT_FLOAT_EQ(result_normal.recommended_lr_factor, 1.0f);
}
