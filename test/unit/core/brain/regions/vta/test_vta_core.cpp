/**
 * @file test_vta_core.cpp
 * @brief Unit tests for VTA core functionality
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/vta/nimcp_vta.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class VTACoreTest : public ::testing::Test {
protected:
    nimcp_vta_system_t vta;

    void SetUp() override {
        nimcp_vta_error_t err = nimcp_vta_init(&vta, nullptr);
        ASSERT_EQ(err, VTA_OK);
    }

    void TearDown() override {
        nimcp_vta_shutdown(&vta);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(VTACoreTest, InitializesWithDefaults) {
    EXPECT_TRUE(vta.initialized);
    EXPECT_GT(vta.da_concentration, 0.0f);
    EXPECT_EQ(vta.mode, VTA_MODE_TONIC);
    EXPECT_EQ(vta.status, VTA_STATUS_NORMAL);
}

TEST_F(VTACoreTest, InitNullReturnsError) {
    nimcp_vta_error_t err = nimcp_vta_init(nullptr, nullptr);
    EXPECT_EQ(err, VTA_ERROR_NULL);
}

TEST_F(VTACoreTest, DoubleInitReturnsError) {
    nimcp_vta_error_t err = nimcp_vta_init(&vta, nullptr);
    EXPECT_EQ(err, VTA_ERROR_ALREADY_INITIALIZED);
}

TEST_F(VTACoreTest, ShutdownClearsState) {
    nimcp_vta_shutdown(&vta);
    EXPECT_FALSE(vta.initialized);
}

TEST_F(VTACoreTest, ResetRestoresDefaults) {
    vta.da_concentration = 200.0f;
    vta.mode = VTA_MODE_PHASIC_EXCITATION;

    nimcp_vta_reset(&vta);

    EXPECT_TRUE(vta.initialized);
    EXPECT_FLOAT_EQ(vta.da_concentration, VTA_DEFAULT_DA_BASELINE);
    EXPECT_EQ(vta.mode, VTA_MODE_TONIC);
}

TEST_F(VTACoreTest, CustomConfigApplied) {
    nimcp_vta_shutdown(&vta);

    nimcp_vta_config_t config = nimcp_vta_default_config();
    config.baseline_firing_rate = 8.0f;
    config.baseline_da = 100.0f;

    nimcp_vta_init(&vta, &config);

    EXPECT_FLOAT_EQ(vta.tonic_firing_rate, 8.0f);
    EXPECT_FLOAT_EQ(vta.da_concentration, 100.0f);
}

//=============================================================================
// Projection Tests
//=============================================================================

TEST_F(VTACoreTest, AddProjectionSucceeds) {
    uint32_t id;
    nimcp_vta_error_t err = nimcp_vta_add_projection(&vta, VTA_TARGET_NAC, "NAc", 0.9f, &id);
    EXPECT_EQ(err, VTA_OK);
    EXPECT_EQ(vta.num_projections, 1u);
}

TEST_F(VTACoreTest, GetProjectionReturnsValid) {
    uint32_t id;
    nimcp_vta_add_projection(&vta, VTA_TARGET_NAC, "NAc", 0.9f, &id);

    nimcp_vta_projection_t* proj = nimcp_vta_get_projection(&vta, id);
    ASSERT_NE(proj, nullptr);
    EXPECT_EQ(proj->target, VTA_TARGET_NAC);
    EXPECT_FLOAT_EQ(proj->weight, 0.9f);
}

TEST_F(VTACoreTest, GetProjectionByTargetWorks) {
    uint32_t id;
    nimcp_vta_add_projection(&vta, VTA_TARGET_PFC, "PFC", 0.7f, &id);

    nimcp_vta_projection_t* proj = nimcp_vta_get_projection_by_target(&vta, VTA_TARGET_PFC);
    ASSERT_NE(proj, nullptr);
    EXPECT_EQ(proj->target, VTA_TARGET_PFC);
}

TEST_F(VTACoreTest, ProjectionCapacityEnforced) {
    for (int i = 0; i < VTA_MAX_PROJECTIONS; i++) {
        uint32_t id;
        nimcp_vta_add_projection(&vta, VTA_TARGET_NAC, "Test", 0.5f, &id);
    }

    uint32_t id;
    nimcp_vta_error_t err = nimcp_vta_add_projection(&vta, VTA_TARGET_PFC, "Extra", 0.5f, &id);
    EXPECT_EQ(err, VTA_ERROR_CAPACITY);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(VTACoreTest, UpdateSucceeds) {
    nimcp_vta_error_t err = nimcp_vta_update(&vta, 10.0f);
    EXPECT_EQ(err, VTA_OK);
}

TEST_F(VTACoreTest, UpdateIncreasesTime) {
    float initial_time = vta.simulation_time;
    nimcp_vta_update(&vta, 10.0f);
    EXPECT_FLOAT_EQ(vta.simulation_time, initial_time + 10.0f);
}

TEST_F(VTACoreTest, UpdateInvalidDtReturnsError) {
    nimcp_vta_error_t err = nimcp_vta_update(&vta, -1.0f);
    EXPECT_EQ(err, VTA_ERROR_INVALID_PARAM);

    err = nimcp_vta_update(&vta, 0.0f);
    EXPECT_EQ(err, VTA_ERROR_INVALID_PARAM);
}

TEST_F(VTACoreTest, MultipleUpdatesStable) {
    for (int i = 0; i < 1000; i++) {
        nimcp_vta_update(&vta, 1.0f);
    }

    EXPECT_FALSE(std::isnan(vta.da_concentration));
    EXPECT_FALSE(std::isinf(vta.da_concentration));
    EXPECT_GE(vta.da_concentration, 0.0f);
}

//=============================================================================
// DA Control Tests
//=============================================================================

TEST_F(VTACoreTest, TonicFiringRateValid) {
    float rate;
    nimcp_vta_error_t err = nimcp_vta_get_firing_rate(&vta, &rate);
    EXPECT_EQ(err, VTA_OK);
    EXPECT_GT(rate, 0.0f);
    EXPECT_LT(rate, VTA_PHASIC_MAX_RATE);
}

TEST_F(VTACoreTest, ExcitationIncreasesRate) {
    float initial_rate;
    nimcp_vta_get_firing_rate(&vta, &initial_rate);

    for (int i = 0; i < 20; i++) {
        nimcp_vta_apply_excitation(&vta, 0.5f);
        nimcp_vta_update(&vta, 1.0f);
    }

    float final_rate;
    nimcp_vta_get_firing_rate(&vta, &final_rate);
    EXPECT_GT(final_rate, initial_rate);
}

TEST_F(VTACoreTest, InhibitionDecreasesRate) {
    /* First apply excitation */
    for (int i = 0; i < 20; i++) {
        nimcp_vta_apply_excitation(&vta, 0.5f);
        nimcp_vta_update(&vta, 1.0f);
    }

    float initial_rate;
    nimcp_vta_get_firing_rate(&vta, &initial_rate);

    for (int i = 0; i < 50; i++) {
        nimcp_vta_apply_inhibition(&vta, 0.8f);
        nimcp_vta_update(&vta, 1.0f);
    }

    float final_rate;
    nimcp_vta_get_firing_rate(&vta, &final_rate);
    EXPECT_LT(final_rate, initial_rate);
}

TEST_F(VTACoreTest, GetDAReturnsValid) {
    float da;
    nimcp_vta_error_t err = nimcp_vta_get_da(&vta, &da);
    EXPECT_EQ(err, VTA_OK);
    EXPECT_GT(da, 0.0f);
}

//=============================================================================
// Mode Tests
//=============================================================================

TEST_F(VTACoreTest, InitialModeIsTonic) {
    nimcp_vta_mode_t mode;
    nimcp_vta_get_mode(&vta, &mode);
    EXPECT_EQ(mode, VTA_MODE_TONIC);
}

TEST_F(VTACoreTest, SetModeWorks) {
    nimcp_vta_set_mode(&vta, VTA_MODE_PHASIC_EXCITATION);
    EXPECT_EQ(vta.mode, VTA_MODE_PHASIC_EXCITATION);
}

TEST_F(VTACoreTest, TriggerBurstSetsPhasicMode) {
    nimcp_vta_trigger_burst(&vta, 1.0f, 100.0f);
    EXPECT_EQ(vta.mode, VTA_MODE_PHASIC_EXCITATION);
}

TEST_F(VTACoreTest, TriggerPauseSetsPauseMode) {
    nimcp_vta_trigger_pause(&vta, 1.0f, 100.0f);
    EXPECT_EQ(vta.mode, VTA_MODE_PHASIC_PAUSE);
}

TEST_F(VTACoreTest, PhasicModeIncreasesDA) {
    float initial_da;
    nimcp_vta_get_da(&vta, &initial_da);

    nimcp_vta_trigger_burst(&vta, 1.0f, 200.0f);
    for (int i = 0; i < 50; i++) {
        nimcp_vta_update(&vta, 10.0f);
    }

    float final_da;
    nimcp_vta_get_da(&vta, &final_da);
    EXPECT_GT(final_da, initial_da);
}

//=============================================================================
// Reward Processing Tests
//=============================================================================

TEST_F(VTACoreTest, SignalRewardSucceeds) {
    nimcp_vta_error_t err = nimcp_vta_signal_reward(&vta, 1.0f);
    EXPECT_EQ(err, VTA_OK);
}

TEST_F(VTACoreTest, SetExpectationSucceeds) {
    nimcp_vta_error_t err = nimcp_vta_set_expectation(&vta, 0.5f);
    EXPECT_EQ(err, VTA_OK);
    EXPECT_FLOAT_EQ(vta.reward.expected_reward, 0.5f);
}

TEST_F(VTACoreTest, PositiveRPETriggersBurst) {
    nimcp_vta_set_expectation(&vta, 0.0f);
    nimcp_vta_signal_reward(&vta, 1.0f);

    float rpe;
    nimcp_vta_get_rpe(&vta, &rpe);
    EXPECT_GT(rpe, 0.0f);
}

TEST_F(VTACoreTest, NegativeRPETriggersPause) {
    nimcp_vta_set_expectation(&vta, 1.0f);
    nimcp_vta_signal_reward(&vta, 0.0f);

    float rpe;
    nimcp_vta_get_rpe(&vta, &rpe);
    EXPECT_LT(rpe, 0.0f);
}

TEST_F(VTACoreTest, ComputeRPEWorks) {
    nimcp_vta_set_expectation(&vta, 0.5f);

    float rpe;
    nimcp_vta_compute_rpe(&vta, 0.8f, &rpe);
    EXPECT_FLOAT_EQ(rpe, 0.3f);
}

//=============================================================================
// Motivation Tests
//=============================================================================

TEST_F(VTACoreTest, GetWantingInRange) {
    float wanting;
    nimcp_vta_get_wanting(&vta, &wanting);
    EXPECT_GE(wanting, 0.0f);
    EXPECT_LE(wanting, 1.0f);
}

TEST_F(VTACoreTest, GetLikingInRange) {
    float liking;
    nimcp_vta_get_liking(&vta, &liking);
    EXPECT_GE(liking, 0.0f);
    EXPECT_LE(liking, 1.0f);
}

TEST_F(VTACoreTest, ModulateMotivationSucceeds) {
    float motivation;
    nimcp_vta_error_t err = nimcp_vta_modulate_motivation(&vta, 1.0f, &motivation);
    EXPECT_EQ(err, VTA_OK);
    EXPECT_GE(motivation, 0.0f);
    EXPECT_LE(motivation, 1.0f);
}

TEST_F(VTACoreTest, ComputeEffortUtilityWorks) {
    float utility;
    nimcp_vta_error_t err = nimcp_vta_compute_effort_utility(&vta, 1.0f, 0.5f, &utility);
    EXPECT_EQ(err, VTA_OK);
}

//=============================================================================
// Status Tests
//=============================================================================

TEST_F(VTACoreTest, GetStateReturnsValid) {
    float da, rpe, wanting;
    nimcp_vta_mode_t mode;

    nimcp_vta_error_t err = nimcp_vta_get_state(&vta, &da, &rpe, &wanting, &mode);
    EXPECT_EQ(err, VTA_OK);
    EXPECT_GT(da, 0.0f);
}

TEST_F(VTACoreTest, GetStatusReturnsNormal) {
    nimcp_vta_status_t status;
    nimcp_vta_error_t err = nimcp_vta_get_status(&vta, &status);
    EXPECT_EQ(err, VTA_OK);
    EXPECT_EQ(status, VTA_STATUS_NORMAL);
}

TEST_F(VTACoreTest, LowDADetected) {
    /* Apply strong inhibition to reduce firing and DA release */
    for (int i = 0; i < 50; i++) {
        nimcp_vta_apply_inhibition(&vta, 0.9f);
        nimcp_vta_update(&vta, 10.0f);
    }

    /* Now set DA very low and continue with inhibition */
    vta.da_concentration = 5.0f;  /* Very low - below 25nM threshold */
    for (int i = 0; i < 20; i++) {
        nimcp_vta_apply_inhibition(&vta, 0.9f);  /* Keep inhibition high to prevent DA recovery */
        nimcp_vta_update(&vta, 1.0f);  /* Small timestep to limit DA release */
    }

    nimcp_vta_status_t status;
    nimcp_vta_get_status(&vta, &status);
    EXPECT_EQ(status, VTA_STATUS_HYPODOPAMINERGIC);
}

//=============================================================================
// Metrics Tests
//=============================================================================

TEST_F(VTACoreTest, MetricsInitiallyZero) {
    nimcp_vta_metrics_t metrics;
    nimcp_vta_get_metrics(&vta, &metrics);
    EXPECT_EQ(metrics.update_count, 0u);
    EXPECT_EQ(metrics.total_spikes, 0u);
}

TEST_F(VTACoreTest, MetricsAccumulate) {
    for (int i = 0; i < 100; i++) {
        nimcp_vta_update(&vta, 10.0f);
    }

    nimcp_vta_metrics_t metrics;
    nimcp_vta_get_metrics(&vta, &metrics);
    EXPECT_EQ(metrics.update_count, 100u);
    EXPECT_GT(metrics.total_simulation_time, 0.0f);
}

TEST_F(VTACoreTest, ResetMetricsWorks) {
    for (int i = 0; i < 50; i++) {
        nimcp_vta_update(&vta, 10.0f);
    }

    nimcp_vta_reset_metrics(&vta);

    nimcp_vta_metrics_t metrics;
    nimcp_vta_get_metrics(&vta, &metrics);
    EXPECT_EQ(metrics.update_count, 0u);
}

//=============================================================================
// Error String Tests
//=============================================================================

TEST_F(VTACoreTest, ErrorStringsValid) {
    EXPECT_STREQ(nimcp_vta_error_string(VTA_OK), "OK");
    EXPECT_STREQ(nimcp_vta_error_string(VTA_ERROR_NULL), "Null pointer");
    EXPECT_STREQ(nimcp_vta_error_string(VTA_ERROR_NOT_INITIALIZED), "Not initialized");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
