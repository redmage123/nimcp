/**
 * @file test_lc_core.cpp
 * @brief Unit tests for Locus Coeruleus core functionality
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/locus_coeruleus/nimcp_locus_coeruleus.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class LCCoreTest : public ::testing::Test {
protected:
    nimcp_lc_system_t lc;

    void SetUp() override {
        memset(&lc, 0, sizeof(lc));
        nimcp_lc_error_t err = nimcp_lc_init(&lc, nullptr);
        ASSERT_EQ(err, LC_OK);
    }

    void TearDown() override {
        nimcp_lc_shutdown(&lc);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(LCCoreTest, InitializesWithDefaults) {
    EXPECT_TRUE(lc.initialized);
    EXPECT_EQ(lc.mode, LC_MODE_TONIC);
    EXPECT_FLOAT_EQ(lc.arousal_level, 0.5f);
    EXPECT_GT(lc.tonic_firing_rate, 0.0f);
}

TEST_F(LCCoreTest, InitNullReturnsError) {
    nimcp_lc_error_t err = nimcp_lc_init(nullptr, nullptr);
    EXPECT_EQ(err, LC_ERR_NULL_PTR);
}

TEST_F(LCCoreTest, DoubleInitReturnsError) {
    nimcp_lc_error_t err = nimcp_lc_init(&lc, nullptr);
    EXPECT_EQ(err, LC_ERR_ALREADY_INITIALIZED);
}

TEST_F(LCCoreTest, ShutdownClearsState) {
    nimcp_lc_error_t err = nimcp_lc_shutdown(&lc);
    EXPECT_EQ(err, LC_OK);
    EXPECT_FALSE(lc.initialized);
}

TEST_F(LCCoreTest, ResetRestoresDefaults) {
    /* Modify state */
    lc.arousal_level = 0.9f;
    lc.mode = LC_MODE_PHASIC;

    nimcp_lc_error_t err = nimcp_lc_reset(&lc);
    EXPECT_EQ(err, LC_OK);

    EXPECT_TRUE(lc.initialized);
    EXPECT_EQ(lc.mode, LC_MODE_TONIC);
    EXPECT_FLOAT_EQ(lc.arousal_level, 0.5f);
}

TEST_F(LCCoreTest, CustomConfigApplied) {
    nimcp_lc_shutdown(&lc);

    nimcp_lc_config_t config = nimcp_lc_default_config();
    config.tonic_rate_hz = 5.0f;
    config.num_neurons = 500;

    nimcp_lc_error_t err = nimcp_lc_init(&lc, &config);
    EXPECT_EQ(err, LC_OK);
    EXPECT_FLOAT_EQ(lc.config.tonic_rate_hz, 5.0f);
    EXPECT_EQ(lc.config.num_neurons, 500u);
}

//=============================================================================
// Projection Tests
//=============================================================================

TEST_F(LCCoreTest, AddProjectionSucceeds) {
    uint32_t proj_id;
    nimcp_lc_error_t err = nimcp_lc_add_projection(&lc, LC_TARGET_CORTEX, "TestCortex", 0.8f, &proj_id);
    EXPECT_EQ(err, LC_OK);
    EXPECT_GE(proj_id, 0u);
}

TEST_F(LCCoreTest, GetProjectionReturnsValid) {
    uint32_t proj_id;
    nimcp_lc_add_projection(&lc, LC_TARGET_HIPPOCAMPUS, "HPC", 0.7f, &proj_id);

    nimcp_lc_projection_t* proj = nimcp_lc_get_projection(&lc, proj_id);
    ASSERT_NE(proj, nullptr);
    EXPECT_EQ(proj->target, LC_TARGET_HIPPOCAMPUS);
    EXPECT_FLOAT_EQ(proj->strength, 0.7f);
}

TEST_F(LCCoreTest, GetProjectionByTargetWorks) {
    uint32_t proj_id;
    nimcp_lc_add_projection(&lc, LC_TARGET_AMYGDALA, "AMY", 0.9f, &proj_id);

    nimcp_lc_projection_t* proj = nimcp_lc_get_projection_by_target(&lc, LC_TARGET_AMYGDALA);
    ASSERT_NE(proj, nullptr);
    EXPECT_EQ(proj->target, LC_TARGET_AMYGDALA);
}

TEST_F(LCCoreTest, SetProjectionParamsWorks) {
    uint32_t proj_id;
    nimcp_lc_add_projection(&lc, LC_TARGET_THALAMUS, "THL", 0.5f, &proj_id);

    nimcp_lc_projection_t* proj = nimcp_lc_get_projection(&lc, proj_id);
    nimcp_lc_error_t err = nimcp_lc_set_projection_params(proj, 2.0f, 100.0f, 0.05f);

    EXPECT_EQ(err, LC_OK);
    EXPECT_FLOAT_EQ(proj->ne_sensitivity, 2.0f);
    EXPECT_FLOAT_EQ(proj->conduction_delay_ms, 100.0f);
}

TEST_F(LCCoreTest, ProjectionCapacityEnforced) {
    /* Add max projections */
    for (uint32_t i = 0; i < LC_MAX_PROJECTIONS; i++) {
        uint32_t proj_id;
        nimcp_lc_add_projection(&lc, LC_TARGET_CORTEX, nullptr, 0.5f, &proj_id);
    }

    uint32_t proj_id;
    nimcp_lc_error_t err = nimcp_lc_add_projection(&lc, LC_TARGET_CORTEX, nullptr, 0.5f, &proj_id);
    EXPECT_EQ(err, LC_ERR_CAPACITY_EXCEEDED);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(LCCoreTest, UpdateSucceeds) {
    nimcp_lc_error_t err = nimcp_lc_update(&lc, 10.0f);
    EXPECT_EQ(err, LC_OK);
}

TEST_F(LCCoreTest, UpdateIncreasesTime) {
    float initial_time = lc.current_time;
    nimcp_lc_update(&lc, 10.0f);
    EXPECT_GT(lc.current_time, initial_time);
}

TEST_F(LCCoreTest, UpdateInvalidDtReturnsError) {
    nimcp_lc_error_t err = nimcp_lc_update(&lc, -1.0f);
    EXPECT_EQ(err, LC_ERR_INVALID_PARAM);

    err = nimcp_lc_update(&lc, 0.0f);
    EXPECT_EQ(err, LC_ERR_INVALID_PARAM);
}

TEST_F(LCCoreTest, MultipleUpdatesStable) {
    for (int i = 0; i < 1000; i++) {
        nimcp_lc_error_t err = nimcp_lc_update(&lc, 1.0f);
        EXPECT_EQ(err, LC_OK);
    }

    EXPECT_FALSE(std::isnan(lc.ne_concentration));
    EXPECT_FALSE(std::isinf(lc.ne_concentration));
    EXPECT_GE(lc.ne_concentration, 0.0f);
}

//=============================================================================
// Firing Rate Tests
//=============================================================================

TEST_F(LCCoreTest, TonicFiringRateValid) {
    float rate;
    nimcp_lc_error_t err = nimcp_lc_get_firing_rate(&lc, &rate);
    EXPECT_EQ(err, LC_OK);
    EXPECT_GT(rate, 0.0f);
    EXPECT_LT(rate, LC_PHASIC_MAX_HZ);
}

TEST_F(LCCoreTest, ExcitationIncreasesRate) {
    float initial_rate;
    nimcp_lc_get_firing_rate(&lc, &initial_rate);

    /* Apply sustained excitation */
    for (int i = 0; i < 20; i++) {
        nimcp_lc_apply_excitation(&lc, 0.5f);
        nimcp_lc_update(&lc, 1.0f);
    }

    float final_rate;
    nimcp_lc_get_firing_rate(&lc, &final_rate);
    EXPECT_GT(final_rate, initial_rate);
}

TEST_F(LCCoreTest, InhibitionDecreasesRate) {
    /* First apply some excitation to have room to decrease */
    nimcp_lc_apply_excitation(&lc, 0.5f);
    for (int i = 0; i < 50; i++) {
        nimcp_lc_update(&lc, 1.0f);
    }

    float initial_rate;
    nimcp_lc_get_firing_rate(&lc, &initial_rate);

    nimcp_lc_apply_inhibition(&lc, 0.8f);
    for (int i = 0; i < 100; i++) {
        nimcp_lc_update(&lc, 1.0f);
    }

    float final_rate;
    nimcp_lc_get_firing_rate(&lc, &final_rate);
    EXPECT_LT(final_rate, initial_rate);
}

//=============================================================================
// Mode Tests
//=============================================================================

TEST_F(LCCoreTest, InitialModeIsTonic) {
    nimcp_lc_mode_t mode = nimcp_lc_get_mode(&lc);
    EXPECT_EQ(mode, LC_MODE_TONIC);
}

TEST_F(LCCoreTest, SetModeWorks) {
    nimcp_lc_error_t err = nimcp_lc_set_mode(&lc, LC_MODE_PHASIC);
    EXPECT_EQ(err, LC_OK);
    EXPECT_EQ(nimcp_lc_get_mode(&lc), LC_MODE_PHASIC);
}

TEST_F(LCCoreTest, TriggerBurstSetsPhasicMode) {
    nimcp_lc_error_t err = nimcp_lc_trigger_burst(&lc, 0.8f, 200.0f);
    EXPECT_EQ(err, LC_OK);
    EXPECT_EQ(lc.mode, LC_MODE_PHASIC);
    EXPECT_GT(lc.phasic_firing_rate, 0.0f);
}

TEST_F(LCCoreTest, PhasicModeIncreasesNE) {
    float baseline_ne = lc.ne_concentration;

    nimcp_lc_trigger_burst(&lc, 1.0f, 100.0f);
    for (int i = 0; i < 50; i++) {
        nimcp_lc_update(&lc, 2.0f);
    }

    EXPECT_GT(lc.ne_concentration, baseline_ne);
}

//=============================================================================
// State and Status Tests
//=============================================================================

TEST_F(LCCoreTest, GetStateReturnsValid) {
    nimcp_lc_state_t state = nimcp_lc_get_state(&lc);
    EXPECT_GE(state, LC_STATE_IDLE);
}

TEST_F(LCCoreTest, GetStatusReturnsNormal) {
    nimcp_lc_status_t status = nimcp_lc_get_status(&lc);
    EXPECT_EQ(status, LC_STATUS_NORMAL);
}

TEST_F(LCCoreTest, HighNECausesHyperactive) {
    /* Force high NE */
    lc.ne_concentration = lc.config.ne_baseline_nm * 5.0f;
    nimcp_lc_update(&lc, 1.0f);

    nimcp_lc_status_t status = nimcp_lc_get_status(&lc);
    EXPECT_NE(status, LC_STATUS_NORMAL);
}

//=============================================================================
// Metrics Tests
//=============================================================================

TEST_F(LCCoreTest, MetricsInitiallyZero) {
    nimcp_lc_metrics_t metrics;
    nimcp_lc_error_t err = nimcp_lc_get_metrics(&lc, &metrics);
    EXPECT_EQ(err, LC_OK);
    EXPECT_EQ(metrics.update_count, 0u);
}

TEST_F(LCCoreTest, MetricsAccumulate) {
    for (int i = 0; i < 100; i++) {
        nimcp_lc_update(&lc, 10.0f);
    }

    nimcp_lc_metrics_t metrics;
    nimcp_lc_get_metrics(&lc, &metrics);
    EXPECT_EQ(metrics.update_count, 100u);
    EXPECT_GT(metrics.total_simulation_time, 0.0f);
}

TEST_F(LCCoreTest, ResetMetricsWorks) {
    for (int i = 0; i < 50; i++) {
        nimcp_lc_update(&lc, 10.0f);
    }

    nimcp_lc_reset_metrics(&lc);

    nimcp_lc_metrics_t metrics;
    nimcp_lc_get_metrics(&lc, &metrics);
    EXPECT_EQ(metrics.update_count, 0u);
}

//=============================================================================
// Error String Test
//=============================================================================

TEST_F(LCCoreTest, ErrorStringsValid) {
    EXPECT_STREQ(nimcp_lc_error_string(LC_OK), "OK");
    EXPECT_STREQ(nimcp_lc_error_string(LC_ERR_NULL_PTR), "Null pointer");
    EXPECT_STREQ(nimcp_lc_error_string(LC_ERR_INVALID_PARAM), "Invalid parameter");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
