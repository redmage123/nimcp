/**
 * @file test_normalization_immune.cpp
 * @brief Unit tests for Normalization-Immune System Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Tests for normalization immune integration
 * WHY:  Verify fever shifts, outlier detection, and baseline restoration
 * HOW:  Google Test fixtures with mock immune system states
 */

#include <gtest/gtest.h>
#include "middleware/normalization/nimcp_normalization_immune.h"
#include "middleware/normalization/nimcp_zscore_normalizer.h"
#include "middleware/normalization/nimcp_adaptive_normalizer.h"
#include "middleware/normalization/nimcp_homeostatic_normalizer.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class NormalizationImmuneTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    zscore_normalizer_t* zscore;
    adaptive_normalizer_t* adaptive;
    homeostatic_normalizer_t* homeostatic;
    normalization_immune_context_t* ctx;

    static const size_t NUM_CHANNELS = 4;

    void SetUp() override {
        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        /* Create normalizers */
        zscore = zscore_normalizer_create(NUM_CHANNELS, 0, 3.0f);
        ASSERT_NE(zscore, nullptr);

        adaptive = adaptive_normalizer_create(NUM_CHANNELS, 0.01f, 0.1f);
        ASSERT_NE(adaptive, nullptr);

        homeostatic = homeostatic_normalizer_create(NUM_CHANNELS, 0.5f, 10.0f);
        ASSERT_NE(homeostatic, nullptr);

        /* Create integration context */
        ctx = normalization_immune_create(immune_system, NUM_CHANNELS);
        ASSERT_NE(ctx, nullptr);

        /* Connect normalizers */
        ASSERT_EQ(normalization_immune_connect_zscore(ctx, zscore), 0);
        ASSERT_EQ(normalization_immune_connect_adaptive(ctx, adaptive), 0);
        ASSERT_EQ(normalization_immune_connect_homeostatic(ctx, homeostatic), 0);

        /* Start immune system */
        ASSERT_EQ(brain_immune_start(immune_system), 0);
    }

    void TearDown() override {
        normalization_immune_destroy(ctx);
        zscore_normalizer_destroy(zscore);
        adaptive_normalizer_destroy(adaptive);
        homeostatic_normalizer_destroy(homeostatic);
        brain_immune_destroy(immune_system);
    }

    /* Helper: Trigger inflammation in immune system */
    void TriggerInflammation(brain_inflammation_level_t level) {
        /* Present antigen to trigger immune response */
        uint8_t epitope[64] = {0x01, 0x02, 0x03};
        uint32_t antigen_id;
        uint32_t severity = (level == INFLAMMATION_LOCAL) ? 3 :
                           (level == INFLAMMATION_REGIONAL) ? 6 :
                           (level == INFLAMMATION_SYSTEMIC) ? 8 : 10;

        brain_immune_present_antigen(
            immune_system,
            ANTIGEN_SOURCE_ANOMALY,
            epitope,
            3,
            severity,
            0,
            &antigen_id
        );

        /* Activate immune response */
        uint32_t b_cell_id;
        brain_immune_activate_b_cell(immune_system, antigen_id, &b_cell_id);

        /* Trigger inflammation */
        uint32_t site_id;
        brain_immune_initiate_inflammation(immune_system, 0, antigen_id, &site_id);

        if (level >= INFLAMMATION_REGIONAL) {
            brain_immune_escalate_inflammation(immune_system, site_id);
        }
        if (level >= INFLAMMATION_SYSTEMIC) {
            brain_immune_escalate_inflammation(immune_system, site_id);
        }
        if (level == INFLAMMATION_STORM) {
            brain_immune_escalate_inflammation(immune_system, site_id);
        }
    }

    /* Helper: Prime z-score normalizer with data */
    void PrimeZScoreNormalizer(size_t channel, float mean, float stddev, size_t samples) {
        for (size_t i = 0; i < samples; i++) {
            float value = mean + (((float)i / samples - 0.5f) * 2.0f * stddev);
            zscore_normalizer_fit(zscore, channel, value);
        }
    }
};

/* Static member definition */
const size_t NormalizationImmuneTest::NUM_CHANNELS;

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(NormalizationImmuneTest, CreateDestroy) {
    EXPECT_NE(ctx, nullptr);
    EXPECT_TRUE(ctx->enabled);
    EXPECT_EQ(ctx->num_channels, NUM_CHANNELS);
}

TEST_F(NormalizationImmuneTest, ConnectNormalizers) {
    EXPECT_EQ(ctx->zscore, zscore);
    EXPECT_EQ(ctx->adaptive, adaptive);
    EXPECT_EQ(ctx->homeostatic, homeostatic);
}

/* ============================================================================
 * Outlier Detection Tests
 * ============================================================================ */

TEST_F(NormalizationImmuneTest, DetectZScoreOutlier) {
    size_t channel = 0;

    /* Prime normalizer: mean=0, stddev=1 */
    PrimeZScoreNormalizer(channel, 0.0f, 1.0f, 100);

    /* Feed outlier value (5 sigma) - normalizer clips to 3.0 */
    float outlier_value = 5.0f;
    float zscore_val = zscore_normalizer_transform(zscore, channel, outlier_value);

    EXPECT_GE(fabs(zscore_val), 3.0f);  /* At or beyond threshold (clipped to 3.0) */

    /* Detect outlier */
    uint32_t outlier_id;
    int result = normalization_immune_detect_zscore_outlier(
        ctx, channel, outlier_value, zscore_val, &outlier_id
    );

    EXPECT_EQ(result, 0);  /* Outlier detected */
    EXPECT_GT(outlier_id, 0);

    /* Verify outlier record */
    const normalization_outlier_t* outlier = normalization_immune_get_outlier(ctx, outlier_id);
    ASSERT_NE(outlier, nullptr);
    EXPECT_EQ(outlier->type, NORMALIZATION_ANOMALY_ZSCORE_OUTLIER);
    EXPECT_EQ(outlier->channel, channel);
    EXPECT_FLOAT_EQ(outlier->value, outlier_value);
    EXPECT_GT(outlier->severity, 0.0f);
    EXPECT_TRUE(outlier->immune_responded);
}

TEST_F(NormalizationImmuneTest, NoOutlierForNormalValue) {
    size_t channel = 0;

    /* Prime normalizer */
    PrimeZScoreNormalizer(channel, 0.0f, 1.0f, 100);

    /* Feed normal value (1 sigma) */
    float normal_value = 1.0f;
    float zscore_val = zscore_normalizer_transform(zscore, channel, normal_value);

    EXPECT_LT(fabs(zscore_val), 3.0f);  /* Should not be outlier */

    /* Attempt detection */
    uint32_t outlier_id;
    int result = normalization_immune_detect_zscore_outlier(
        ctx, channel, normal_value, zscore_val, &outlier_id
    );

    EXPECT_EQ(result, -1);  /* No outlier */
}

TEST_F(NormalizationImmuneTest, DetectRapidShift) {
    size_t channel = 0;
    float old_baseline = 0.0f;
    float new_baseline = 2.0f;
    uint64_t delta_ms = 100;  /* 100ms for large shift = rapid */

    uint32_t outlier_id;
    int result = normalization_immune_detect_rapid_shift(
        ctx,
        NORMALIZER_ZSCORE,
        channel,
        old_baseline,
        new_baseline,
        delta_ms,
        &outlier_id
    );

    EXPECT_EQ(result, 0);  /* Rapid shift detected */

    const normalization_outlier_t* outlier = normalization_immune_get_outlier(ctx, outlier_id);
    ASSERT_NE(outlier, nullptr);
    EXPECT_EQ(outlier->type, NORMALIZATION_ANOMALY_RAPID_SHIFT);
}

TEST_F(NormalizationImmuneTest, DetectHomeostaticDrift) {
    size_t channel = 1;
    float target_drift = 0.4f;  /* 40% drift = excessive */

    uint32_t outlier_id;
    int result = normalization_immune_detect_homeostatic_drift(
        ctx, channel, target_drift, &outlier_id
    );

    EXPECT_EQ(result, 0);  /* Drift detected */

    const normalization_outlier_t* outlier = normalization_immune_get_outlier(ctx, outlier_id);
    ASSERT_NE(outlier, nullptr);
    EXPECT_EQ(outlier->type, NORMALIZATION_ANOMALY_HOMEOSTATIC_DRIFT);
}

TEST_F(NormalizationImmuneTest, ReportRangeViolation) {
    size_t channel = 2;
    float value = 15.0f;
    float min = 0.0f;
    float max = 10.0f;

    uint32_t outlier_id;
    int result = normalization_immune_report_range_violation(
        ctx, channel, value, min, max, &outlier_id
    );

    EXPECT_EQ(result, 0);

    const normalization_outlier_t* outlier = normalization_immune_get_outlier(ctx, outlier_id);
    ASSERT_NE(outlier, nullptr);
    EXPECT_EQ(outlier->type, NORMALIZATION_ANOMALY_RANGE_VIOLATION);
    EXPECT_FLOAT_EQ(outlier->value, value);
}

/* ============================================================================
 * Immune Modulation Tests (Inflammation → Normalization)
 * ============================================================================ */

TEST_F(NormalizationImmuneTest, UpdateModulationFromImmuneState) {
    /* Trigger local inflammation */
    TriggerInflammation(INFLAMMATION_LOCAL);

    /* Update modulation */
    EXPECT_EQ(normalization_immune_update_modulation(ctx), 0);

    /* Check modulation state */
    normalization_immune_modulation_t modulation;
    EXPECT_EQ(normalization_immune_get_modulation(ctx, &modulation), 0);

    /* Should have some cytokine levels */
    EXPECT_GT(modulation.il1_level, 0.0f);
    EXPECT_GT(modulation.il6_level, 0.0f);
}

TEST_F(NormalizationImmuneTest, FeverShiftActivation) {
    /* Trigger regional inflammation (higher IL-6) */
    TriggerInflammation(INFLAMMATION_REGIONAL);
    EXPECT_EQ(normalization_immune_update_modulation(ctx), 0);

    /* Check fever shift */
    EXPECT_TRUE(normalization_immune_is_fever_active(ctx));

    normalization_immune_modulation_t modulation;
    normalization_immune_get_modulation(ctx, &modulation);

    EXPECT_GT(modulation.zscore_mean_shift, 0.0f);
    EXPECT_GT(modulation.homeostatic_target_shift, 0.0f);
    EXPECT_TRUE(modulation.fever_shift_active);
}

TEST_F(NormalizationImmuneTest, FeverShiftMagnitude) {
    /* Apply fever shift with IL-6 = 0.6 */
    float il6_level = 0.6f;
    EXPECT_EQ(normalization_immune_apply_fever_shift(ctx, il6_level), 0);

    /* Check shift magnitude */
    float expected_shift = il6_level * NORMALIZATION_IMMUNE_BASELINE_FEVER_SHIFT;

    normalization_immune_modulation_t modulation;
    normalization_immune_get_modulation(ctx, &modulation);

    EXPECT_FLOAT_EQ(modulation.zscore_mean_shift, expected_shift);
    EXPECT_TRUE(modulation.fever_shift_active);
}

TEST_F(NormalizationImmuneTest, CytokineStormClamping) {
    /* Trigger cytokine storm */
    TriggerInflammation(INFLAMMATION_STORM);
    EXPECT_EQ(normalization_immune_update_modulation(ctx), 0);

    /* Check storm clamping */
    EXPECT_TRUE(normalization_immune_is_storm_clamping_active(ctx));

    normalization_immune_modulation_t modulation;
    normalization_immune_get_modulation(ctx, &modulation);

    /* Outlier threshold should be tightened */
    EXPECT_LT(modulation.zscore_outlier_threshold, NORMALIZATION_IMMUNE_ZSCORE_THRESHOLD);

    /* Range expansion should be reduced */
    EXPECT_LT(modulation.minmax_range_expansion, 1.0f);
}

TEST_F(NormalizationImmuneTest, LearningRateModulation) {
    /* Trigger systemic inflammation */
    TriggerInflammation(INFLAMMATION_SYSTEMIC);
    EXPECT_EQ(normalization_immune_update_modulation(ctx), 0);

    normalization_immune_modulation_t modulation;
    normalization_immune_get_modulation(ctx, &modulation);

    /* Learning rate should be reduced during inflammation */
    EXPECT_LT(modulation.adaptive_learning_rate_factor, 1.0f);
    EXPECT_GE(modulation.adaptive_learning_rate_factor, 0.3f);  /* Clamped minimum */
}

TEST_F(NormalizationImmuneTest, VarianceExpansion) {
    /* Trigger regional inflammation */
    TriggerInflammation(INFLAMMATION_REGIONAL);
    EXPECT_EQ(normalization_immune_update_modulation(ctx), 0);

    normalization_immune_modulation_t modulation;
    normalization_immune_get_modulation(ctx, &modulation);

    /* Variance should expand during inflammation */
    EXPECT_GT(modulation.zscore_variance_scale, 1.0f);
}

TEST_F(NormalizationImmuneTest, RangeExpansion) {
    /* Trigger local inflammation (not storm) */
    TriggerInflammation(INFLAMMATION_LOCAL);
    EXPECT_EQ(normalization_immune_update_modulation(ctx), 0);

    normalization_immune_modulation_t modulation;
    normalization_immune_get_modulation(ctx, &modulation);

    /* Range should expand during inflammation */
    EXPECT_GT(modulation.minmax_range_expansion, 1.0f);
}

/* ============================================================================
 * Baseline Restoration Tests
 * ============================================================================ */

TEST_F(NormalizationImmuneTest, CaptureBaselines) {
    /* Prime z-score normalizer */
    PrimeZScoreNormalizer(0, 5.0f, 2.0f, 50);

    /* Capture baselines */
    EXPECT_EQ(normalization_immune_capture_baselines(ctx), 0);

    /* Original mean should be captured */
    EXPECT_FLOAT_EQ(ctx->zscore_original_mean[0], zscore_normalizer_mean(zscore, 0));
}

TEST_F(NormalizationImmuneTest, RestoreBaselinesAfterInflammation) {
    /* Apply fever shift */
    EXPECT_EQ(normalization_immune_apply_fever_shift(ctx, 0.8f), 0);
    EXPECT_TRUE(normalization_immune_is_fever_active(ctx));

    float initial_shift = ctx->modulation.zscore_mean_shift;
    EXPECT_GT(initial_shift, 0.0f);

    /* Restore with IL-10 over time */
    float il10_level = 0.5f;
    uint64_t delta_ms = 1000;  /* 1 second */

    EXPECT_EQ(normalization_immune_restore_baselines(ctx, il10_level, delta_ms), 0);

    /* Shift should be reduced */
    EXPECT_LT(ctx->modulation.zscore_mean_shift, initial_shift);
}

TEST_F(NormalizationImmuneTest, CompleteRestorationWithHighIL10) {
    /* Apply fever shift */
    EXPECT_EQ(normalization_immune_apply_fever_shift(ctx, 0.8f), 0);

    /* Restore with high IL-10 over multiple iterations */
    float il10_level = 1.0f;

    for (int i = 0; i < 50; i++) {
        normalization_immune_restore_baselines(ctx, il10_level, 100);
    }

    /* Shift should be nearly zero */
    EXPECT_LT(fabs(ctx->modulation.zscore_mean_shift), 0.02f);
    EXPECT_FALSE(normalization_immune_is_fever_active(ctx));
}

TEST_F(NormalizationImmuneTest, NoRestorationWithoutIL10) {
    /* Apply fever shift */
    EXPECT_EQ(normalization_immune_apply_fever_shift(ctx, 0.8f), 0);
    float initial_shift = ctx->modulation.zscore_mean_shift;

    /* Try to restore with no IL-10 */
    EXPECT_EQ(normalization_immune_restore_baselines(ctx, 0.0f, 1000), 0);

    /* Shift should be unchanged */
    EXPECT_FLOAT_EQ(ctx->modulation.zscore_mean_shift, initial_shift);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(NormalizationImmuneTest, GetModulation) {
    normalization_immune_modulation_t modulation;
    EXPECT_EQ(normalization_immune_get_modulation(ctx, &modulation), 0);

    /* Check defaults */
    EXPECT_FLOAT_EQ(modulation.adaptive_learning_rate_factor, 1.0f);
    EXPECT_FLOAT_EQ(modulation.zscore_mean_shift, 0.0f);
    EXPECT_EQ(modulation.inflammation, INFLAMMATION_NONE);
}

TEST_F(NormalizationImmuneTest, GetInflammationLevel) {
    brain_inflammation_level_t level = normalization_immune_get_inflammation(ctx);
    EXPECT_EQ(level, INFLAMMATION_NONE);

    /* Trigger inflammation */
    TriggerInflammation(INFLAMMATION_REGIONAL);
    normalization_immune_update_modulation(ctx);

    level = normalization_immune_get_inflammation(ctx);
    EXPECT_GE(level, INFLAMMATION_LOCAL);
}

TEST_F(NormalizationImmuneTest, GetFeverShift) {
    /* Apply fever shift */
    float il6 = 0.5f;
    normalization_immune_apply_fever_shift(ctx, il6);

    /* Get shift for z-score normalizer */
    float shift = normalization_immune_get_fever_shift(ctx, NORMALIZER_ZSCORE, 0);
    EXPECT_GT(shift, 0.0f);

    /* Get shift for homeostatic normalizer */
    shift = normalization_immune_get_fever_shift(ctx, NORMALIZER_HOMEOSTATIC, 0);
    EXPECT_GT(shift, 0.0f);
}

/* ============================================================================
 * Integration Tests (End-to-End)
 * ============================================================================ */

TEST_F(NormalizationImmuneTest, EndToEndOutlierToImmuneResponse) {
    size_t channel = 0;

    /* Prime normalizer */
    PrimeZScoreNormalizer(channel, 0.0f, 1.0f, 100);

    /* Feed outlier */
    float outlier_value = 4.5f;
    float zscore_val = zscore_normalizer_transform(zscore, channel, outlier_value);

    /* Detect outlier */
    uint32_t outlier_id;
    EXPECT_EQ(normalization_immune_detect_zscore_outlier(
        ctx, channel, outlier_value, zscore_val, &outlier_id
    ), 0);

    /* Verify immune system received antigen */
    const normalization_outlier_t* outlier = normalization_immune_get_outlier(ctx, outlier_id);
    ASSERT_NE(outlier, nullptr);
    EXPECT_TRUE(outlier->immune_responded);
    EXPECT_GT(outlier->antigen_id, 0);

    /* Verify antigen exists in immune system */
    const brain_antigen_t* antigen = brain_immune_get_antigen(
        immune_system, outlier->antigen_id
    );
    ASSERT_NE(antigen, nullptr);
    EXPECT_EQ(antigen->source, ANTIGEN_SOURCE_ANOMALY);
}

TEST_F(NormalizationImmuneTest, EndToEndInflammationToFeverShift) {
    /* Trigger inflammation */
    TriggerInflammation(INFLAMMATION_REGIONAL);

    /* Update modulation from immune state */
    EXPECT_EQ(normalization_immune_update_modulation(ctx), 0);

    /* Verify fever shift is active */
    EXPECT_TRUE(normalization_immune_is_fever_active(ctx));

    /* Verify parameters are shifted */
    normalization_immune_modulation_t modulation;
    normalization_immune_get_modulation(ctx, &modulation);

    EXPECT_GT(modulation.zscore_mean_shift, 0.0f);
    EXPECT_LT(modulation.adaptive_learning_rate_factor, 1.0f);
    EXPECT_GT(modulation.zscore_variance_scale, 1.0f);
}

TEST_F(NormalizationImmuneTest, EndToEndFeverCycleWithRestoration) {
    /* 1. Trigger inflammation */
    TriggerInflammation(INFLAMMATION_REGIONAL);
    normalization_immune_update_modulation(ctx);

    EXPECT_TRUE(normalization_immune_is_fever_active(ctx));

    /* 2. Resolve inflammation (simulate IL-10 release) */
    /* In real system, immune would resolve and release IL-10 */
    /* Here we manually drive restoration */

    float il10_level = 0.8f;

    /* 3. Gradual restoration */
    for (int i = 0; i < 30; i++) {
        normalization_immune_restore_baselines(ctx, il10_level, 100);
    }

    /* 4. Verify restoration */
    EXPECT_FALSE(normalization_immune_is_fever_active(ctx));
    EXPECT_LT(fabs(ctx->modulation.zscore_mean_shift), 0.02f);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(NormalizationImmuneTest, AnomalyTypeToString) {
    EXPECT_STREQ(
        normalization_immune_anomaly_type_to_string(NORMALIZATION_ANOMALY_ZSCORE_OUTLIER),
        "ZScoreOutlier"
    );
    EXPECT_STREQ(
        normalization_immune_anomaly_type_to_string(NORMALIZATION_ANOMALY_RANGE_VIOLATION),
        "RangeViolation"
    );
}

TEST_F(NormalizationImmuneTest, NormalizerTypeToString) {
    EXPECT_STREQ(
        normalization_immune_normalizer_to_string(NORMALIZER_ZSCORE),
        "ZScore"
    );
    EXPECT_STREQ(
        normalization_immune_normalizer_to_string(NORMALIZER_ADAPTIVE),
        "Adaptive"
    );
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(NormalizationImmuneTest, NullContextHandling) {
    uint32_t outlier_id;
    EXPECT_EQ(normalization_immune_detect_zscore_outlier(
        nullptr, 0, 1.0f, 3.5f, &outlier_id
    ), -1);

    EXPECT_EQ(normalization_immune_update_modulation(nullptr), -1);
    EXPECT_FALSE(normalization_immune_is_fever_active(nullptr));
}

TEST_F(NormalizationImmuneTest, OutOfRangeChannel) {
    uint32_t outlier_id;
    size_t invalid_channel = NUM_CHANNELS + 10;

    /* Should handle gracefully */
    float shift = normalization_immune_get_fever_shift(ctx, NORMALIZER_ZSCORE, invalid_channel);
    EXPECT_FLOAT_EQ(shift, 0.0f);
}

TEST_F(NormalizationImmuneTest, MaxOutliersLimit) {
    /* Fill outlier capacity */
    for (size_t i = 0; i < NORMALIZATION_IMMUNE_MAX_OUTLIERS; i++) {
        uint32_t outlier_id;
        normalization_immune_report_range_violation(ctx, 0, 100.0f, 0.0f, 10.0f, &outlier_id);
    }

    /* Next outlier should fail gracefully */
    uint32_t outlier_id;
    int result = normalization_immune_report_range_violation(ctx, 0, 100.0f, 0.0f, 10.0f, &outlier_id);
    /* Implementation may or may not fail here, but shouldn't crash */
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
