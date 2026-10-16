/**
 * @file test_habenula_core.cpp
 * @brief Unit tests for Habenula core system - Aversive learning
 */

#include <gtest/gtest.h>
#include "test_helpers.h"

extern "C" {
#include "core/brain/regions/habenula/nimcp_habenula.h"
}

class HabenulaCoreTest : public ::testing::Test {
protected:
    nimcp_habenula_system_t habenula;

    void SetUp() override {
        memset(&habenula, 0, sizeof(habenula));
    }

    void TearDown() override {
        if (habenula.initialized) {
            nimcp_habenula_shutdown(&habenula);
        }
    }
};

/* ==========================================================================
 * Configuration Tests
 * ========================================================================== */

TEST_F(HabenulaCoreTest, DefaultConfigHasValidValues) {
    nimcp_habenula_config_t config;
    nimcp_habenula_default_config(&config);

    EXPECT_GT(config.baseline_firing_rate, 0.0f);
    EXPECT_GT(config.max_firing_rate, config.baseline_firing_rate);
    EXPECT_GE(config.lhb_weight, 0.0f);
    EXPECT_LE(config.lhb_weight, 1.0f);
    EXPECT_GE(config.mhb_weight, 0.0f);
    EXPECT_LE(config.mhb_weight, 1.0f);
    EXPECT_GT(config.vta_inhibition_gain, 0.0f);
    EXPECT_GT(config.raphe_modulation_gain, 0.0f);
}

TEST_F(HabenulaCoreTest, DefaultConfigWithNullDoesNotCrash) {
    nimcp_habenula_default_config(nullptr);
    /* Should not crash */
}

/* ==========================================================================
 * Lifecycle Tests
 * ========================================================================== */

TEST_F(HabenulaCoreTest, InitWithNullReturnsError) {
    EXPECT_EQ(nimcp_habenula_init(nullptr, nullptr), HABENULA_ERROR_NULL);
}

TEST_F(HabenulaCoreTest, InitWithDefaultConfigSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);
    EXPECT_TRUE(habenula.initialized);
}

TEST_F(HabenulaCoreTest, InitWithCustomConfigSucceeds) {
    nimcp_habenula_config_t config;
    nimcp_habenula_default_config(&config);
    config.baseline_firing_rate = 10.0f;

    EXPECT_EQ(nimcp_habenula_init(&habenula, &config), HABENULA_OK);
    EXPECT_FLOAT_EQ(habenula.config.baseline_firing_rate, 10.0f);
}

TEST_F(HabenulaCoreTest, InitSetsCorrectInitialState) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    EXPECT_GT(habenula.lhb.firing_rate, 0.0f);
    EXPECT_GT(habenula.mhb.firing_rate, 0.0f);
    EXPECT_EQ(habenula.lhb.disappointment, 0.0f);
    EXPECT_EQ(habenula.mhb.aversion_level, 0.0f);
    EXPECT_EQ(habenula.mode, HABENULA_MODE_BASELINE);
    EXPECT_FALSE(habenula.depression.is_depressed);
}

TEST_F(HabenulaCoreTest, DoubleInitReturnsError) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_ERROR_ALREADY_INITIALIZED);
}

TEST_F(HabenulaCoreTest, ShutdownSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);
    EXPECT_EQ(nimcp_habenula_shutdown(&habenula), HABENULA_OK);
    EXPECT_FALSE(habenula.initialized);
}

TEST_F(HabenulaCoreTest, ShutdownWithNullReturnsError) {
    EXPECT_EQ(nimcp_habenula_shutdown(nullptr), HABENULA_ERROR_NULL);
}

TEST_F(HabenulaCoreTest, ShutdownWithoutInitReturnsError) {
    EXPECT_EQ(nimcp_habenula_shutdown(&habenula), HABENULA_ERROR_NOT_INITIALIZED);
}

TEST_F(HabenulaCoreTest, ResetSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    /* Modify state */
    habenula.lhb.disappointment = 0.8f;
    habenula.mhb.aversion_level = 0.5f;

    EXPECT_EQ(nimcp_habenula_reset(&habenula), HABENULA_OK);
    EXPECT_EQ(habenula.lhb.disappointment, 0.0f);
    EXPECT_EQ(habenula.mhb.aversion_level, 0.0f);
}

/* ==========================================================================
 * Update Tests
 * ========================================================================== */

TEST_F(HabenulaCoreTest, UpdateWithNullReturnsError) {
    EXPECT_EQ(nimcp_habenula_update(nullptr, 10.0f), HABENULA_ERROR_NULL);
}

TEST_F(HabenulaCoreTest, UpdateWithoutInitReturnsError) {
    EXPECT_EQ(nimcp_habenula_update(&habenula, 10.0f), HABENULA_ERROR_NOT_INITIALIZED);
}

TEST_F(HabenulaCoreTest, UpdateIncreasesSimulationTime) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float initial_time = habenula.simulation_time;
    nimcp_habenula_update(&habenula, 100.0f);

    EXPECT_GT(habenula.simulation_time, initial_time);
}

TEST_F(HabenulaCoreTest, UpdateMaintainsFiringWithinBounds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    for (int i = 0; i < 100; i++) {
        nimcp_habenula_update(&habenula, 10.0f);
    }

    EXPECT_GE(habenula.neurons.combined_firing_rate, 0.0f);
    EXPECT_LE(habenula.neurons.combined_firing_rate, habenula.config.max_firing_rate);
}

TEST_F(HabenulaCoreTest, UpdateDecaysDisappointment) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    /* Set high disappointment */
    habenula.lhb.disappointment = 0.9f;
    float initial_disappointment = habenula.lhb.disappointment;

    for (int i = 0; i < 50; i++) {
        nimcp_habenula_update(&habenula, 100.0f);
    }

    EXPECT_LT(habenula.lhb.disappointment, initial_disappointment);
}

/* ==========================================================================
 * Negative RPE Tests
 * ========================================================================== */

TEST_F(HabenulaCoreTest, ComputeNegativeRPEWithNull) {
    EXPECT_EQ(nimcp_habenula_compute_negative_rpe(nullptr, 0.5f, 0.3f, nullptr),
              HABENULA_ERROR_NULL);
}

TEST_F(HabenulaCoreTest, ComputeNegativeRPEForDisappointment) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float neg_rpe;
    /* Expected 0.8, received 0.3 -> disappointment */
    EXPECT_EQ(nimcp_habenula_compute_negative_rpe(&habenula, 0.8f, 0.3f, &neg_rpe),
              HABENULA_OK);

    EXPECT_GT(neg_rpe, 0.0f);
    EXPECT_NEAR(neg_rpe, 0.5f, 0.01f);
}

TEST_F(HabenulaCoreTest, ComputeNegativeRPEZeroForPositive) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float neg_rpe;
    /* Expected 0.3, received 0.8 -> positive RPE */
    EXPECT_EQ(nimcp_habenula_compute_negative_rpe(&habenula, 0.3f, 0.8f, &neg_rpe),
              HABENULA_OK);

    EXPECT_EQ(neg_rpe, 0.0f);
}

TEST_F(HabenulaCoreTest, ProcessOutcomeIncreasesDisappointment) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float initial_disappointment = habenula.lhb.disappointment;

    /* Disappointing outcome */
    EXPECT_EQ(nimcp_habenula_process_outcome(&habenula, 1.0f, 0.2f), HABENULA_OK);

    EXPECT_GT(habenula.lhb.disappointment, initial_disappointment);
}

TEST_F(HabenulaCoreTest, ProcessOutcomeDecreasesDisappointmentOnSuccess) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    /* Set initial disappointment */
    habenula.lhb.disappointment = 0.6f;
    float initial = habenula.lhb.disappointment;

    /* Positive outcome */
    EXPECT_EQ(nimcp_habenula_process_outcome(&habenula, 0.3f, 0.8f), HABENULA_OK);

    EXPECT_LT(habenula.lhb.disappointment, initial);
}

TEST_F(HabenulaCoreTest, GetDisappointmentSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    habenula.lhb.disappointment = 0.4f;

    float disappointment;
    EXPECT_EQ(nimcp_habenula_get_disappointment(&habenula, &disappointment), HABENULA_OK);
    EXPECT_FLOAT_EQ(disappointment, 0.4f);
}

TEST_F(HabenulaCoreTest, ApplyAversiveIncreasesAversion) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float initial_aversion = habenula.mhb.aversion_level;

    EXPECT_EQ(nimcp_habenula_apply_aversive(&habenula, 0.7f), HABENULA_OK);

    EXPECT_GT(habenula.mhb.aversion_level, initial_aversion);
}

/* ==========================================================================
 * VTA Inhibition Tests
 * ========================================================================== */

TEST_F(HabenulaCoreTest, GetVTAInhibitionSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float inhibition;
    EXPECT_EQ(nimcp_habenula_get_vta_inhibition(&habenula, &inhibition), HABENULA_OK);
    EXPECT_GE(inhibition, 0.0f);
    EXPECT_LE(inhibition, 1.0f);
}

TEST_F(HabenulaCoreTest, HighDisappointmentIncreasesVTAInhibition) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float low_inhibition;
    nimcp_habenula_get_vta_inhibition(&habenula, &low_inhibition);

    /* Induce high disappointment */
    nimcp_habenula_process_outcome(&habenula, 1.0f, 0.0f);
    nimcp_habenula_update(&habenula, 100.0f);

    float high_inhibition;
    nimcp_habenula_get_vta_inhibition(&habenula, &high_inhibition);

    EXPECT_GT(high_inhibition, low_inhibition);
}

TEST_F(HabenulaCoreTest, ApplyVTAFeedbackSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    EXPECT_EQ(nimcp_habenula_apply_vta_feedback(&habenula, 50.0f), HABENULA_OK);
}

/* ==========================================================================
 * Raphe Modulation Tests
 * ========================================================================== */

TEST_F(HabenulaCoreTest, GetRapheModulationSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float modulation;
    EXPECT_EQ(nimcp_habenula_get_raphe_modulation(&habenula, &modulation), HABENULA_OK);
    EXPECT_GE(modulation, 0.0f);
}

/* ==========================================================================
 * Avoidance Learning Tests
 * ========================================================================== */

TEST_F(HabenulaCoreTest, GetAvoidanceSignalSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float avoidance;
    EXPECT_EQ(nimcp_habenula_get_avoidance_signal(&habenula, &avoidance), HABENULA_OK);
    EXPECT_GE(avoidance, 0.0f);
    EXPECT_LE(avoidance, 1.0f);
}

TEST_F(HabenulaCoreTest, HighDisappointmentIncreasesAvoidance) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float initial_avoidance;
    nimcp_habenula_get_avoidance_signal(&habenula, &initial_avoidance);

    /* Induce high disappointment and aversion */
    nimcp_habenula_apply_aversive(&habenula, 0.9f);
    nimcp_habenula_process_outcome(&habenula, 1.0f, 0.0f);

    float high_avoidance;
    nimcp_habenula_get_avoidance_signal(&habenula, &high_avoidance);

    EXPECT_GT(high_avoidance, initial_avoidance);
}

TEST_F(HabenulaCoreTest, ShouldAvoidWithHighAversion) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    /* Induce high aversion */
    nimcp_habenula_apply_aversive(&habenula, 0.9f);

    bool should_avoid;
    EXPECT_EQ(nimcp_habenula_should_avoid(&habenula, 0.0f, &should_avoid), HABENULA_OK);
    EXPECT_TRUE(should_avoid);
}

TEST_F(HabenulaCoreTest, ShouldNotAvoidWhenCalm) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    bool should_avoid;
    EXPECT_EQ(nimcp_habenula_should_avoid(&habenula, 0.5f, &should_avoid), HABENULA_OK);
    EXPECT_FALSE(should_avoid);
}

/* ==========================================================================
 * Depression Model Tests
 * ========================================================================== */

TEST_F(HabenulaCoreTest, GetHelplessnessSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float helplessness;
    EXPECT_EQ(nimcp_habenula_get_helplessness(&habenula, &helplessness), HABENULA_OK);
    EXPECT_GE(helplessness, 0.0f);
    EXPECT_LE(helplessness, 1.0f);
}

TEST_F(HabenulaCoreTest, GetAnhedoniaSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float anhedonia;
    EXPECT_EQ(nimcp_habenula_get_anhedonia(&habenula, &anhedonia), HABENULA_OK);
    EXPECT_GE(anhedonia, 0.0f);
}

TEST_F(HabenulaCoreTest, IsDepressedInitiallyFalse) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    bool is_depressed;
    EXPECT_EQ(nimcp_habenula_is_depressed(&habenula, &is_depressed), HABENULA_OK);
    EXPECT_FALSE(is_depressed);
}

TEST_F(HabenulaCoreTest, RecordCopingFailureIncreasesHelplessness) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float initial_helplessness = habenula.depression.helplessness_index;

    EXPECT_EQ(nimcp_habenula_record_coping_failure(&habenula), HABENULA_OK);

    EXPECT_GT(habenula.depression.helplessness_index, initial_helplessness);
}

TEST_F(HabenulaCoreTest, RecordCopingSuccessDecreasesHelplessness) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    /* Set some helplessness */
    habenula.depression.helplessness_index = 0.5f;
    float initial = habenula.depression.helplessness_index;

    EXPECT_EQ(nimcp_habenula_record_coping_success(&habenula), HABENULA_OK);

    EXPECT_LT(habenula.depression.helplessness_index, initial);
}

/* ==========================================================================
 * Mode Tests
 * ========================================================================== */

TEST_F(HabenulaCoreTest, GetModeSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    nimcp_habenula_mode_t mode;
    EXPECT_EQ(nimcp_habenula_get_mode(&habenula, &mode), HABENULA_OK);
    EXPECT_EQ(mode, HABENULA_MODE_BASELINE);
}

TEST_F(HabenulaCoreTest, SetModeSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    EXPECT_EQ(nimcp_habenula_set_mode(&habenula, HABENULA_MODE_DISAPPOINTED), HABENULA_OK);
    EXPECT_EQ(habenula.mode, HABENULA_MODE_DISAPPOINTED);
}

/* ==========================================================================
 * Input Tests
 * ========================================================================== */

TEST_F(HabenulaCoreTest, ApplyExcitationSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float initial_input = habenula.neurons.excitatory_input;

    EXPECT_EQ(nimcp_habenula_apply_excitation(&habenula, 0.5f), HABENULA_OK);

    EXPECT_GT(habenula.neurons.excitatory_input, initial_input);
}

TEST_F(HabenulaCoreTest, ApplyInhibitionSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float initial_input = habenula.neurons.inhibitory_input;

    EXPECT_EQ(nimcp_habenula_apply_inhibition(&habenula, 0.5f), HABENULA_OK);

    EXPECT_GT(habenula.neurons.inhibitory_input, initial_input);
}

/* ==========================================================================
 * Firing Rate Tests
 * ========================================================================== */

TEST_F(HabenulaCoreTest, GetFiringRateSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float rate;
    EXPECT_EQ(nimcp_habenula_get_firing_rate(&habenula, &rate), HABENULA_OK);
    EXPECT_GT(rate, 0.0f);
}

TEST_F(HabenulaCoreTest, GetRegionFiringRateSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float lhb_rate, mhb_rate;
    EXPECT_EQ(nimcp_habenula_get_region_firing_rate(&habenula, HABENULA_REGION_LHB, &lhb_rate),
              HABENULA_OK);
    EXPECT_EQ(nimcp_habenula_get_region_firing_rate(&habenula, HABENULA_REGION_MHB, &mhb_rate),
              HABENULA_OK);

    EXPECT_GT(lhb_rate, 0.0f);
    EXPECT_GT(mhb_rate, 0.0f);
    /* LHb typically has higher baseline activity */
    EXPECT_GT(lhb_rate, mhb_rate);
}

/* ==========================================================================
 * Projection Tests
 * ========================================================================== */

TEST_F(HabenulaCoreTest, AddProjectionSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    EXPECT_EQ(nimcp_habenula_add_projection(&habenula, HABENULA_TARGET_VTA,
              HABENULA_REGION_LHB, 0.8f, true), HABENULA_OK);

    EXPECT_EQ(habenula.projection_count, 1u);
}

TEST_F(HabenulaCoreTest, GetProjectionSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    nimcp_habenula_add_projection(&habenula, HABENULA_TARGET_VTA,
                                   HABENULA_REGION_LHB, 0.8f, true);

    nimcp_habenula_projection_t proj;
    EXPECT_EQ(nimcp_habenula_get_projection(&habenula, 0, &proj), HABENULA_OK);

    EXPECT_EQ(proj.target, HABENULA_TARGET_VTA);
    EXPECT_EQ(proj.source, HABENULA_REGION_LHB);
    EXPECT_FLOAT_EQ(proj.weight, 0.8f);
    EXPECT_TRUE(proj.is_inhibitory);
}

TEST_F(HabenulaCoreTest, GetOutputToTargetSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float vta_output;
    EXPECT_EQ(nimcp_habenula_get_output_to_target(&habenula, HABENULA_TARGET_VTA,
              &vta_output), HABENULA_OK);
    /* VTA output should be negative (inhibitory) */
}

/* ==========================================================================
 * State Tests
 * ========================================================================== */

TEST_F(HabenulaCoreTest, GetStateSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    float firing_rate, disappointment, aversion, vta_inhibition;
    EXPECT_EQ(nimcp_habenula_get_state(&habenula, &firing_rate, &disappointment,
              &aversion, &vta_inhibition), HABENULA_OK);

    EXPECT_GT(firing_rate, 0.0f);
    EXPECT_GE(disappointment, 0.0f);
    EXPECT_GE(aversion, 0.0f);
}

TEST_F(HabenulaCoreTest, GetStatusReturnsNormalInitially) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    nimcp_habenula_status_t status;
    EXPECT_EQ(nimcp_habenula_get_status(&habenula, &status), HABENULA_OK);
    EXPECT_EQ(status, HABENULA_STATUS_NORMAL);
}

/* ==========================================================================
 * Metrics Tests
 * ========================================================================== */

TEST_F(HabenulaCoreTest, GetMetricsSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    nimcp_habenula_metrics_t metrics;
    EXPECT_EQ(nimcp_habenula_get_metrics(&habenula, &metrics), HABENULA_OK);
}

TEST_F(HabenulaCoreTest, UpdateIncreasesMetricsCount) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    for (int i = 0; i < 10; i++) {
        nimcp_habenula_update(&habenula, 10.0f);
    }

    nimcp_habenula_metrics_t metrics;
    nimcp_habenula_get_metrics(&habenula, &metrics);
    EXPECT_EQ(metrics.update_count, 10u);
}

TEST_F(HabenulaCoreTest, ResetMetricsSucceeds) {
    EXPECT_EQ(nimcp_habenula_init(&habenula, nullptr), HABENULA_OK);

    /* Generate some metrics */
    nimcp_habenula_update(&habenula, 10.0f);
    nimcp_habenula_process_outcome(&habenula, 1.0f, 0.0f);

    EXPECT_EQ(nimcp_habenula_reset_metrics(&habenula), HABENULA_OK);

    nimcp_habenula_metrics_t metrics;
    nimcp_habenula_get_metrics(&habenula, &metrics);
    EXPECT_EQ(metrics.update_count, 0u);
}

/* ==========================================================================
 * Error String Tests
 * ========================================================================== */

TEST_F(HabenulaCoreTest, ErrorStringReturnsCorrectStrings) {
    EXPECT_STREQ(nimcp_habenula_error_string(HABENULA_OK), "OK");
    EXPECT_STREQ(nimcp_habenula_error_string(HABENULA_ERROR_NULL), "Null pointer");
    EXPECT_STREQ(nimcp_habenula_error_string(HABENULA_ERROR_NOT_INITIALIZED), "Not initialized");
}
