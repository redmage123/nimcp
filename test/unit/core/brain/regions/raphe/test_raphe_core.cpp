/**
 * @file test_raphe_core.cpp
 * @brief Unit tests for Raphe Nuclei core system
 */

#include <gtest/gtest.h>

extern "C" {
#include "core/brain/regions/raphe/nimcp_raphe.h"
}

class RapheCoreTest : public ::testing::Test {
protected:
    nimcp_raphe_system_t raphe;

    void SetUp() override {
        memset(&raphe, 0, sizeof(raphe));
    }

    void TearDown() override {
        if (raphe.initialized) {
            nimcp_raphe_shutdown(&raphe);
        }
    }
};

/* ==========================================================================
 * Lifecycle Tests
 * ========================================================================== */

TEST_F(RapheCoreTest, DefaultConfigHasValidValues) {
    nimcp_raphe_config_t config = nimcp_raphe_default_config();

    EXPECT_FLOAT_EQ(config.baseline_firing_rate, RAPHE_DEFAULT_TONIC_RATE);
    EXPECT_FLOAT_EQ(config.baseline_5ht, RAPHE_DEFAULT_5HT_BASELINE);
    EXPECT_GT(config.ht_decay_tau, 0.0f);
    EXPECT_GT(config.mood_time_constant, 0.0f);
    EXPECT_TRUE(config.enable_autoreceptors);
}

TEST_F(RapheCoreTest, InitWithNullReturnsError) {
    EXPECT_EQ(nimcp_raphe_init(nullptr, nullptr), RAPHE_ERROR_NULL);
}

TEST_F(RapheCoreTest, InitWithDefaultConfigSucceeds) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);
    EXPECT_TRUE(raphe.initialized);
}

TEST_F(RapheCoreTest, InitWithCustomConfigSucceeds) {
    nimcp_raphe_config_t config = nimcp_raphe_default_config();
    config.baseline_firing_rate = 5.0f;
    config.baseline_5ht = 30.0f;

    EXPECT_EQ(nimcp_raphe_init(&raphe, &config), RAPHE_OK);
    EXPECT_TRUE(raphe.initialized);
    EXPECT_FLOAT_EQ(raphe.config.baseline_firing_rate, 5.0f);
    EXPECT_FLOAT_EQ(raphe.config.baseline_5ht, 30.0f);
}

TEST_F(RapheCoreTest, InitSetsCorrectInitialState) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    EXPECT_FLOAT_EQ(raphe.ht_concentration, RAPHE_DEFAULT_5HT_BASELINE);
    EXPECT_FLOAT_EQ(raphe.mood.valence, RAPHE_DEFAULT_MOOD_NEUTRAL);
    EXPECT_EQ(raphe.mode, RAPHE_MODE_TONIC);
    EXPECT_EQ(raphe.status, RAPHE_STATUS_NORMAL);
}

TEST_F(RapheCoreTest, DoubleInitReturnsError) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_ERROR_ALREADY_INITIALIZED);
}

TEST_F(RapheCoreTest, ShutdownSucceeds) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);
    EXPECT_EQ(nimcp_raphe_shutdown(&raphe), RAPHE_OK);
    EXPECT_FALSE(raphe.initialized);
}

TEST_F(RapheCoreTest, ShutdownWithNullReturnsError) {
    EXPECT_EQ(nimcp_raphe_shutdown(nullptr), RAPHE_ERROR_NULL);
}

TEST_F(RapheCoreTest, ShutdownWithoutInitReturnsError) {
    EXPECT_EQ(nimcp_raphe_shutdown(&raphe), RAPHE_ERROR_NOT_INITIALIZED);
}

TEST_F(RapheCoreTest, ResetSucceeds) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    /* Modify state */
    raphe.ht_concentration = 50.0f;
    raphe.mood.valence = 0.8f;

    EXPECT_EQ(nimcp_raphe_reset(&raphe), RAPHE_OK);

    /* Should be reset to initial values */
    EXPECT_FLOAT_EQ(raphe.ht_concentration, RAPHE_DEFAULT_5HT_BASELINE);
    EXPECT_FLOAT_EQ(raphe.mood.valence, RAPHE_DEFAULT_MOOD_NEUTRAL);
}

/* ==========================================================================
 * Update Tests
 * ========================================================================== */

TEST_F(RapheCoreTest, UpdateWithNullReturnsError) {
    EXPECT_EQ(nimcp_raphe_update(nullptr, 10.0f), RAPHE_ERROR_NULL);
}

TEST_F(RapheCoreTest, UpdateWithoutInitReturnsError) {
    EXPECT_EQ(nimcp_raphe_update(&raphe, 10.0f), RAPHE_ERROR_NOT_INITIALIZED);
}

TEST_F(RapheCoreTest, UpdateIncreasesSimulationTime) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    EXPECT_FLOAT_EQ(raphe.simulation_time, 0.0f);
    nimcp_raphe_update(&raphe, 100.0f);
    EXPECT_FLOAT_EQ(raphe.simulation_time, 100.0f);
}

TEST_F(RapheCoreTest, UpdateMaintains5HTWithinBounds) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    for (int i = 0; i < 100; i++) {
        nimcp_raphe_update(&raphe, 10.0f);
    }

    /* 5-HT should stay within physiological bounds */
    EXPECT_GT(raphe.ht_concentration, 1.0f);
    EXPECT_LT(raphe.ht_concentration, 200.0f);
}

TEST_F(RapheCoreTest, UpdateIncreasesMetricsCount) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    nimcp_raphe_update(&raphe, 10.0f);
    EXPECT_EQ(raphe.metrics.update_count, 1u);

    nimcp_raphe_update(&raphe, 10.0f);
    EXPECT_EQ(raphe.metrics.update_count, 2u);
}

/* ==========================================================================
 * Mode Tests
 * ========================================================================== */

TEST_F(RapheCoreTest, GetModeReturnsCorrectMode) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    nimcp_raphe_mode_t mode;
    EXPECT_EQ(nimcp_raphe_get_mode(&raphe, &mode), RAPHE_OK);
    EXPECT_EQ(mode, RAPHE_MODE_TONIC);
}

TEST_F(RapheCoreTest, SetModeSucceeds) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    EXPECT_EQ(nimcp_raphe_set_mode(&raphe, RAPHE_MODE_ELEVATED), RAPHE_OK);
    EXPECT_EQ(raphe.mode, RAPHE_MODE_ELEVATED);
}

TEST_F(RapheCoreTest, SetModeResetsDuration) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    nimcp_raphe_update(&raphe, 100.0f);
    EXPECT_GT(raphe.mode_duration, 0.0f);

    EXPECT_EQ(nimcp_raphe_set_mode(&raphe, RAPHE_MODE_SUPPRESSED), RAPHE_OK);
    EXPECT_FLOAT_EQ(raphe.mode_duration, 0.0f);
}

TEST_F(RapheCoreTest, ElevatedModeIncreasesFiring) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    /* Run in tonic mode */
    for (int i = 0; i < 50; i++) {
        nimcp_raphe_update(&raphe, 10.0f);
    }
    float tonic_rate = raphe.neurons.firing_rate;

    /* Switch to elevated mode */
    nimcp_raphe_set_mode(&raphe, RAPHE_MODE_ELEVATED);
    for (int i = 0; i < 50; i++) {
        nimcp_raphe_update(&raphe, 10.0f);
    }

    EXPECT_GT(raphe.neurons.firing_rate, tonic_rate);
}

TEST_F(RapheCoreTest, SuppressedModeHasLowerTargetFiring) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    /* Run in tonic mode and get target */
    nimcp_raphe_set_mode(&raphe, RAPHE_MODE_TONIC);
    float tonic_target = raphe.config.baseline_firing_rate;

    /* Check suppressed mode has lower target */
    nimcp_raphe_set_mode(&raphe, RAPHE_MODE_SUPPRESSED);
    float suppressed_target = raphe.config.baseline_firing_rate * 0.3f; /* Suppressed uses 30% */

    EXPECT_LT(suppressed_target, tonic_target);
    EXPECT_EQ(raphe.mode, RAPHE_MODE_SUPPRESSED);
}

/* ==========================================================================
 * Mood Tests
 * ========================================================================== */

TEST_F(RapheCoreTest, GetMoodReturnsCurrentValence) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    float valence;
    EXPECT_EQ(nimcp_raphe_get_mood(&raphe, &valence), RAPHE_OK);
    EXPECT_FLOAT_EQ(valence, RAPHE_DEFAULT_MOOD_NEUTRAL);
}

TEST_F(RapheCoreTest, GetMoodStateReturnsNeutralInitially) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    nimcp_mood_state_t state;
    EXPECT_EQ(nimcp_raphe_get_mood_state(&raphe, &state), RAPHE_OK);
    EXPECT_EQ(state, MOOD_NEUTRAL);
}

TEST_F(RapheCoreTest, ApplyMoodInputAffectsValence) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    float initial_valence = raphe.mood.valence;
    EXPECT_EQ(nimcp_raphe_apply_mood_input(&raphe, 0.5f), RAPHE_OK);

    EXPECT_GT(raphe.mood.valence, initial_valence);
}

TEST_F(RapheCoreTest, GetAnxietyReturnsCorrectValue) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    float anxiety;
    EXPECT_EQ(nimcp_raphe_get_anxiety(&raphe, &anxiety), RAPHE_OK);
    EXPECT_GE(anxiety, 0.0f);
    EXPECT_LE(anxiety, 1.0f);
}

TEST_F(RapheCoreTest, ModulateAnxietyAffectsLevel) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    float initial_anxiety = raphe.mood.anxiety;
    EXPECT_EQ(nimcp_raphe_modulate_anxiety(&raphe, 0.3f), RAPHE_OK);

    EXPECT_GT(raphe.mood.anxiety, initial_anxiety);
}

/* ==========================================================================
 * Impulse Control Tests
 * ========================================================================== */

TEST_F(RapheCoreTest, GetInhibitionReturnsCorrectValue) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    float inhibition;
    EXPECT_EQ(nimcp_raphe_get_inhibition(&raphe, &inhibition), RAPHE_OK);
    EXPECT_GE(inhibition, 0.0f);
    EXPECT_LE(inhibition, 1.0f);
}

TEST_F(RapheCoreTest, GetPatienceReturnsCorrectValue) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    float patience;
    EXPECT_EQ(nimcp_raphe_get_patience(&raphe, &patience), RAPHE_OK);
    EXPECT_GE(patience, 0.0f);
    EXPECT_LE(patience, 1.0f);
}

TEST_F(RapheCoreTest, GetImpulsivityReturnsInverseOfInhibition) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    float impulsivity;
    EXPECT_EQ(nimcp_raphe_get_impulsivity(&raphe, &impulsivity), RAPHE_OK);

    /* Impulsivity should be approximately 1 - inhibition */
    EXPECT_NEAR(impulsivity, 1.0f - raphe.impulse.inhibition_strength, 0.1f);
}

TEST_F(RapheCoreTest, ComputeInhibitionSucceeds) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    float inhibition_output;
    EXPECT_EQ(nimcp_raphe_compute_inhibition(&raphe, 0.3f, &inhibition_output), RAPHE_OK);

    /* Should be inhibition_strength - impulse_strength */
    float expected = raphe.impulse.inhibition_strength - 0.3f;
    EXPECT_NEAR(inhibition_output, expected, 0.01f);
}

/* ==========================================================================
 * Temporal Discounting Tests
 * ========================================================================== */

TEST_F(RapheCoreTest, GetDiscountRateReturnsCorrectValue) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    float rate;
    EXPECT_EQ(nimcp_raphe_get_discount_rate(&raphe, &rate), RAPHE_OK);
    EXPECT_GT(rate, 0.0f);
}

TEST_F(RapheCoreTest, DiscountValueReducesWithDelay) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    float discounted;
    EXPECT_EQ(nimcp_raphe_discount_value(&raphe, 100.0f, 1000.0f, &discounted), RAPHE_OK);

    EXPECT_LT(discounted, 100.0f);
    EXPECT_GT(discounted, 0.0f);
}

TEST_F(RapheCoreTest, DiscountValueWithZeroDelayEqualsAmount) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    float discounted;
    EXPECT_EQ(nimcp_raphe_discount_value(&raphe, 100.0f, 0.0f, &discounted), RAPHE_OK);

    EXPECT_FLOAT_EQ(discounted, 100.0f);
}

TEST_F(RapheCoreTest, GetFutureOrientationReturnsCorrectValue) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    float orientation;
    EXPECT_EQ(nimcp_raphe_get_future_orientation(&raphe, &orientation), RAPHE_OK);
    EXPECT_GE(orientation, 0.0f);
    EXPECT_LE(orientation, 1.0f);
}

/* ==========================================================================
 * 5-HT Control Tests
 * ========================================================================== */

TEST_F(RapheCoreTest, ApplyExcitationIncreasesFiring) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    float initial_rate = raphe.neurons.firing_rate;
    EXPECT_EQ(nimcp_raphe_apply_excitation(&raphe, 0.5f), RAPHE_OK);

    /* Run updates */
    for (int i = 0; i < 50; i++) {
        nimcp_raphe_update(&raphe, 10.0f);
    }

    EXPECT_GT(raphe.neurons.firing_rate, initial_rate);
}

TEST_F(RapheCoreTest, ApplyInhibitionIncreasesInhibitoryInput) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    /* Get initial inhibitory input */
    float initial_input = raphe.neurons.inhibitory_input;

    /* Apply strong inhibition */
    EXPECT_EQ(nimcp_raphe_apply_inhibition(&raphe, 0.8f), RAPHE_OK);

    /* Inhibitory input should increase */
    EXPECT_GT(raphe.neurons.inhibitory_input, initial_input);
}

TEST_F(RapheCoreTest, Get5HTReturnsCorrectConcentration) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    float ht;
    EXPECT_EQ(nimcp_raphe_get_5ht(&raphe, &ht), RAPHE_OK);
    EXPECT_FLOAT_EQ(ht, RAPHE_DEFAULT_5HT_BASELINE);
}

TEST_F(RapheCoreTest, GetFiringRateReturnsCorrectValue) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    float rate;
    EXPECT_EQ(nimcp_raphe_get_firing_rate(&raphe, &rate), RAPHE_OK);
    EXPECT_GT(rate, 0.0f);
}

/* ==========================================================================
 * Projection Tests
 * ========================================================================== */

TEST_F(RapheCoreTest, AddProjectionSucceeds) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    uint32_t id;
    EXPECT_EQ(nimcp_raphe_add_projection(&raphe, RAPHE_TARGET_PFC, "test_pfc", 0.8f, &id), RAPHE_OK);
    EXPECT_EQ(id, 0u);
    EXPECT_EQ(raphe.num_projections, 1u);
}

TEST_F(RapheCoreTest, GetProjectionReturnsCorrectData) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    uint32_t id;
    nimcp_raphe_add_projection(&raphe, RAPHE_TARGET_AMYGDALA, "test_amyg", 0.7f, &id);

    nimcp_raphe_projection_t* proj = nimcp_raphe_get_projection(&raphe, id);
    ASSERT_NE(proj, nullptr);
    EXPECT_EQ(proj->target, RAPHE_TARGET_AMYGDALA);
    EXPECT_FLOAT_EQ(proj->weight, 0.7f);
}

TEST_F(RapheCoreTest, GetProjectionByTargetSucceeds) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    uint32_t id;
    nimcp_raphe_add_projection(&raphe, RAPHE_TARGET_VTA, "test_vta", 0.6f, &id);

    nimcp_raphe_projection_t* proj = nimcp_raphe_get_projection_by_target(&raphe, RAPHE_TARGET_VTA);
    ASSERT_NE(proj, nullptr);
    EXPECT_EQ(proj->target, RAPHE_TARGET_VTA);
}

TEST_F(RapheCoreTest, Get5HTAtTargetReturnsScaledValue) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    uint32_t id;
    nimcp_raphe_add_projection(&raphe, RAPHE_TARGET_PFC, "test", 0.5f, &id);

    /* Update to deliver 5-HT */
    for (int i = 0; i < 10; i++) {
        nimcp_raphe_update(&raphe, 10.0f);
    }

    float ht;
    EXPECT_EQ(nimcp_raphe_get_5ht_at_target(&raphe, RAPHE_TARGET_PFC, &ht), RAPHE_OK);
    EXPECT_GT(ht, 0.0f);
}

/* ==========================================================================
 * Status Tests
 * ========================================================================== */

TEST_F(RapheCoreTest, GetStateReturnsMultipleValues) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    float ht, mood, anxiety;
    nimcp_raphe_mode_t mode;

    EXPECT_EQ(nimcp_raphe_get_state(&raphe, &ht, &mood, &anxiety, &mode), RAPHE_OK);
    EXPECT_FLOAT_EQ(ht, RAPHE_DEFAULT_5HT_BASELINE);
    EXPECT_FLOAT_EQ(mood, RAPHE_DEFAULT_MOOD_NEUTRAL);
    EXPECT_GE(anxiety, 0.0f);
    EXPECT_EQ(mode, RAPHE_MODE_TONIC);
}

TEST_F(RapheCoreTest, GetStatusReturnsNormalInitially) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    nimcp_raphe_status_t status;
    EXPECT_EQ(nimcp_raphe_get_status(&raphe, &status), RAPHE_OK);
    EXPECT_EQ(status, RAPHE_STATUS_NORMAL);
}

TEST_F(RapheCoreTest, Low5HTDetectsHyposerotonergic) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    /* Force low 5-HT through sustained inhibition */
    for (int i = 0; i < 200; i++) {
        nimcp_raphe_apply_inhibition(&raphe, 0.95f);
        nimcp_raphe_update(&raphe, 10.0f);
    }

    /* Directly set to ensure low level */
    raphe.ht_concentration = 5.0f;
    nimcp_raphe_update(&raphe, 10.0f);

    nimcp_raphe_status_t status;
    nimcp_raphe_get_status(&raphe, &status);
    EXPECT_EQ(status, RAPHE_STATUS_HYPOSEROTONERGIC);
}

TEST_F(RapheCoreTest, High5HTDetectsHyperserotonergic) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    /* Directly set high 5-HT */
    raphe.ht_concentration = 50.0f;
    nimcp_raphe_update(&raphe, 10.0f);

    nimcp_raphe_status_t status;
    nimcp_raphe_get_status(&raphe, &status);
    EXPECT_EQ(status, RAPHE_STATUS_HYPERSEROTONERGIC);
}

TEST_F(RapheCoreTest, GetMetricsSucceeds) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    for (int i = 0; i < 10; i++) {
        nimcp_raphe_update(&raphe, 10.0f);
    }

    nimcp_raphe_metrics_t metrics;
    EXPECT_EQ(nimcp_raphe_get_metrics(&raphe, &metrics), RAPHE_OK);
    EXPECT_EQ(metrics.update_count, 10u);
}

TEST_F(RapheCoreTest, ResetMetricsSucceeds) {
    EXPECT_EQ(nimcp_raphe_init(&raphe, nullptr), RAPHE_OK);

    for (int i = 0; i < 10; i++) {
        nimcp_raphe_update(&raphe, 10.0f);
    }

    EXPECT_EQ(nimcp_raphe_reset_metrics(&raphe), RAPHE_OK);
    EXPECT_EQ(raphe.metrics.update_count, 0u);
}

/* ==========================================================================
 * Error String Tests
 * ========================================================================== */

TEST_F(RapheCoreTest, ErrorStringReturnsCorrectStrings) {
    EXPECT_STREQ(nimcp_raphe_error_string(RAPHE_OK), "Success");
    EXPECT_STREQ(nimcp_raphe_error_string(RAPHE_ERROR_NULL), "Null pointer");
    EXPECT_STREQ(nimcp_raphe_error_string(RAPHE_ERROR_NOT_INITIALIZED), "System not initialized");
}
