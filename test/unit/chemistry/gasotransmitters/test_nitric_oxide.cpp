/**
 * @file test_nitric_oxide.cpp
 * @brief Unit tests for Nitric Oxide Signaling module
 *
 * WHAT: Test suite for nimcp_nitric_oxide
 * WHY:  Verify NO production, diffusion, and retrograde signaling
 * HOW:  Unit tests for create, modify, and lifecycle operations
 *
 * @author NIMCP Development Team
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "chemistry/gasotransmitters/nimcp_nitric_oxide.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NitricOxideTest : public ::testing::Test {
protected:
    nimcp_no_system_t system;

    void SetUp() override {
        memset(&system, 0, sizeof(system));
        nimcp_no_error_t err = nimcp_no_init(&system, nullptr);
        ASSERT_EQ(err, NO_OK);
    }

    void TearDown() override {
        nimcp_no_shutdown(&system);
    }
};

//=============================================================================
// Initialization Tests
//=============================================================================

TEST(NOInitTest, InitWithNullConfig) {
    nimcp_no_system_t sys;
    memset(&sys, 0, sizeof(sys));
    nimcp_no_error_t err = nimcp_no_init(&sys, nullptr);
    EXPECT_EQ(err, NO_OK);
    EXPECT_TRUE(sys.initialized);
    nimcp_no_shutdown(&sys);
}

TEST(NOInitTest, InitWithCustomConfig) {
    nimcp_no_system_t sys;
    memset(&sys, 0, sizeof(sys));

    nimcp_no_config_t config = {
        .default_nos_type = NOS_TYPE_NNOS,
        .nos_km_arginine = 3.0f,
        .nos_km_calcium = 0.3f,
        .nos_vmax = 1.0f,
        .diffusion_coefficient = 3300.0f,
        .decay_rate = 1.0f,
        .effective_radius = 100.0f,
        .gc_sensitivity = 0.5f,
        .cgmp_decay_rate = 0.1f,
        .pde_activity = 0.5f,
        .potentiation_max = 2.0f,
        .potentiation_threshold = 10.0f,
        .vasodilation_sensitivity = 0.5f,
        .on_release = nullptr,
        .on_retrograde = nullptr,
        .callback_data = nullptr
    };

    nimcp_no_error_t err = nimcp_no_init(&sys, &config);
    EXPECT_EQ(err, NO_OK);
    nimcp_no_shutdown(&sys);
}

TEST(NOInitTest, InitNull) {
    nimcp_no_error_t err = nimcp_no_init(nullptr, nullptr);
    EXPECT_EQ(err, NO_ERR_NULL_PTR);
}

TEST(NOInitTest, DoubleInit) {
    nimcp_no_system_t sys;
    memset(&sys, 0, sizeof(sys));
    nimcp_no_init(&sys, nullptr);
    nimcp_no_error_t err = nimcp_no_init(&sys, nullptr);
    /* Implementation allows re-init (resets system) */
    EXPECT_EQ(err, NO_OK);
    nimcp_no_shutdown(&sys);
}

TEST(NOInitTest, ShutdownNull) {
    nimcp_no_error_t err = nimcp_no_shutdown(nullptr);
    EXPECT_EQ(err, NO_ERR_NULL_PTR);
}

TEST(NOInitTest, ShutdownNotInitialized) {
    /* Shutdown is safe to call on non-initialized systems */
    nimcp_no_system_t sys;
    memset(&sys, 0, sizeof(sys));
    nimcp_no_error_t err = nimcp_no_shutdown(&sys);
    EXPECT_EQ(err, NO_OK);  /* Safe shutdown, no error */
}

//=============================================================================
// Source Management Tests
//=============================================================================

TEST_F(NitricOxideTest, AddSource) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;

    nimcp_no_error_t err = nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    EXPECT_EQ(err, NO_OK);
    EXPECT_EQ(system.num_sources, 1u);
}

TEST_F(NitricOxideTest, AddMultipleSources) {
    float pos1[3] = {0.0f, 0.0f, 0.0f};
    float pos2[3] = {10.0f, 0.0f, 0.0f};
    float pos3[3] = {0.0f, 10.0f, 0.0f};
    uint32_t id1, id2, id3;

    nimcp_no_add_source(&system, pos1, NOS_TYPE_NNOS, &id1);
    nimcp_no_add_source(&system, pos2, NOS_TYPE_INOS, &id2);
    nimcp_no_error_t err = nimcp_no_add_source(&system, pos3, NOS_TYPE_ENOS, &id3);

    EXPECT_EQ(err, NO_OK);
    EXPECT_EQ(system.num_sources, 3u);
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
}

TEST_F(NitricOxideTest, GetSource) {
    float position[3] = {5.0f, 10.0f, 15.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);

    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);
    ASSERT_NE(source, nullptr);
    EXPECT_EQ(source->nos_type, NOS_TYPE_NNOS);
    EXPECT_NEAR(source->position[0], 5.0f, 0.01f);
    EXPECT_NEAR(source->position[1], 10.0f, 0.01f);
    EXPECT_NEAR(source->position[2], 15.0f, 0.01f);
}

TEST_F(NitricOxideTest, GetSourceNotFound) {
    nimcp_no_source_t* source = nimcp_no_get_source(&system, 999);
    EXPECT_EQ(source, nullptr);
}

TEST_F(NitricOxideTest, RemoveSource) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);

    nimcp_no_error_t err = nimcp_no_remove_source(&system, source_id);
    EXPECT_EQ(err, NO_OK);

    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);
    EXPECT_EQ(source, nullptr);
}

TEST_F(NitricOxideTest, RemoveSourceNotFound) {
    nimcp_no_error_t err = nimcp_no_remove_source(&system, 999);
    EXPECT_EQ(err, NO_ERR_SOURCE_NOT_FOUND);
}

//=============================================================================
// Target Management Tests
//=============================================================================

TEST_F(NitricOxideTest, AddTarget) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    nimcp_no_error_t err = nimcp_no_add_target(source, 100, 50.0f, NO_RETROGRADE_PRESYNAPTIC_RELEASE);
    EXPECT_EQ(err, NO_OK);
    EXPECT_EQ(source->num_targets, 1u);
}

TEST_F(NitricOxideTest, AddMultipleTargets) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    nimcp_no_add_target(source, 100, 10.0f, NO_RETROGRADE_PRESYNAPTIC_RELEASE);
    nimcp_no_add_target(source, 101, 20.0f, NO_RETROGRADE_PRESYNAPTIC_INHIBIT);
    nimcp_no_error_t err = nimcp_no_add_target(source, 102, 30.0f, NO_RETROGRADE_BILATERAL);

    EXPECT_EQ(err, NO_OK);
    EXPECT_EQ(source->num_targets, 3u);
}

TEST_F(NitricOxideTest, RemoveTarget) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    nimcp_no_add_target(source, 100, 50.0f, NO_RETROGRADE_PRESYNAPTIC_RELEASE);

    nimcp_no_error_t err = nimcp_no_remove_target(source, 100);
    EXPECT_EQ(err, NO_OK);
    EXPECT_EQ(source->num_targets, 0u);
}

TEST_F(NitricOxideTest, GetTargetPotentiation) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    nimcp_no_add_target(source, 100, 50.0f, NO_RETROGRADE_PRESYNAPTIC_RELEASE);

    float potentiation;
    nimcp_no_error_t err = nimcp_no_get_target_potentiation(source, 100, &potentiation);
    EXPECT_EQ(err, NO_OK);
    EXPECT_GE(potentiation, 0.0f);
}

//=============================================================================
// NOS Activation Tests
//=============================================================================

TEST_F(NitricOxideTest, SetCalcium) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    nimcp_no_error_t err = nimcp_no_set_calcium(source, 1.0f);
    EXPECT_EQ(err, NO_OK);
    EXPECT_NEAR(source->calcium_level, 1.0f, 0.01f);
}

TEST_F(NitricOxideTest, SetNMDAActivation) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    nimcp_no_error_t err = nimcp_no_set_nmda_activation(source, 0.8f);
    EXPECT_EQ(err, NO_OK);
    EXPECT_NEAR(source->nmda_activation, 0.8f, 0.01f);
}

TEST_F(NitricOxideTest, SetSubstrate) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    nimcp_no_error_t err = nimcp_no_set_substrate(source, 100.0f, 0.95f, 0.9f);
    EXPECT_EQ(err, NO_OK);
    EXPECT_NEAR(source->arginine_level, 100.0f, 0.1f);
    EXPECT_NEAR(source->oxygen_level, 0.95f, 0.01f);
    EXPECT_NEAR(source->bh4_level, 0.9f, 0.01f);
}

TEST_F(NitricOxideTest, GetNOSActivity) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    float activity;
    nimcp_no_error_t err = nimcp_no_get_nos_activity(source, &activity);
    EXPECT_EQ(err, NO_OK);
    EXPECT_GE(activity, 0.0f);
    EXPECT_LE(activity, 1.0f);
}

TEST_F(NitricOxideTest, CalciumActivatesNOS) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* Set optimal substrate levels */
    nimcp_no_set_substrate(source, 100.0f, 1.0f, 1.0f);

    /* Low calcium */
    nimcp_no_set_calcium(source, 0.01f);
    nimcp_no_update(&system, 10.0f);
    float low_activity;
    nimcp_no_get_nos_activity(source, &low_activity);

    /* High calcium */
    nimcp_no_set_calcium(source, 10.0f);
    nimcp_no_update(&system, 10.0f);
    float high_activity;
    nimcp_no_get_nos_activity(source, &high_activity);

    EXPECT_GT(high_activity, low_activity);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(NitricOxideTest, UpdateSystem) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);

    nimcp_no_error_t err = nimcp_no_update(&system, 1.0f);
    EXPECT_EQ(err, NO_OK);
    EXPECT_EQ(system.update_count, 1u);
}

TEST_F(NitricOxideTest, UpdateMultiple) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);

    for (int i = 0; i < 100; i++) {
        nimcp_no_error_t err = nimcp_no_update(&system, 1.0f);
        EXPECT_EQ(err, NO_OK);
    }
    EXPECT_EQ(system.update_count, 100u);
}

TEST_F(NitricOxideTest, UpdateSource) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    nimcp_no_error_t err = nimcp_no_update_source(&system, source, 1.0f);
    EXPECT_EQ(err, NO_OK);
}

TEST_F(NitricOxideTest, Diffuse) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    /* Add targets at different distances */
    nimcp_no_add_target(source, 100, 10.0f, NO_RETROGRADE_PRESYNAPTIC_RELEASE);
    nimcp_no_add_target(source, 101, 50.0f, NO_RETROGRADE_PRESYNAPTIC_RELEASE);
    nimcp_no_add_target(source, 102, 90.0f, NO_RETROGRADE_PRESYNAPTIC_RELEASE);

    /* Activate source */
    nimcp_no_set_calcium(source, 5.0f);
    nimcp_no_set_substrate(source, 100.0f, 1.0f, 1.0f);
    source->no_concentration = 50.0f;

    nimcp_no_error_t err = nimcp_no_diffuse(&system, source);
    EXPECT_EQ(err, NO_OK);

    /* Closer target should have more NO */
    EXPECT_GT(source->targets[0].no_concentration, source->targets[2].no_concentration);
}

//=============================================================================
// Effects API Tests
//=============================================================================

TEST_F(NitricOxideTest, GetCGMP) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);

    float cgmp;
    nimcp_no_error_t err = nimcp_no_get_cgmp(&system, source_id, &cgmp);
    EXPECT_EQ(err, NO_OK);
    EXPECT_GE(cgmp, 0.0f);
}

TEST_F(NitricOxideTest, GetVasodilation) {
    float factor;
    nimcp_no_error_t err = nimcp_no_get_vasodilation(&system, &factor);
    EXPECT_EQ(err, NO_OK);
    EXPECT_GE(factor, 0.0f);
}

TEST_F(NitricOxideTest, GetPlasticityModifier) {
    float modifier;
    nimcp_no_error_t err = nimcp_no_get_plasticity_modifier(&system, &modifier);
    EXPECT_EQ(err, NO_OK);
    EXPECT_GE(modifier, 0.0f);
}

TEST_F(NitricOxideTest, NOIncreasesVasodilation) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_ENOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    float baseline_vaso;
    nimcp_no_get_vasodilation(&system, &baseline_vaso);

    /* High NO production */
    nimcp_no_set_calcium(source, 10.0f);
    nimcp_no_set_substrate(source, 100.0f, 1.0f, 1.0f);
    for (int i = 0; i < 100; i++) {
        nimcp_no_update(&system, 10.0f);
    }

    float active_vaso;
    nimcp_no_get_vasodilation(&system, &active_vaso);

    EXPECT_GT(active_vaso, baseline_vaso);
}

//=============================================================================
// State and Metrics Tests
//=============================================================================

TEST_F(NitricOxideTest, GetState) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_source_t* source = nimcp_no_get_source(&system, source_id);

    nimcp_no_state_t state = nimcp_no_get_state(source);
    EXPECT_GE(state, NO_STATE_INACTIVE);
    EXPECT_LE(state, NO_STATE_PATHOLOGICAL);
}

TEST_F(NitricOxideTest, GetMetrics) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_update(&system, 10.0f);

    nimcp_no_metrics_t metrics;
    nimcp_no_error_t err = nimcp_no_get_metrics(&system, &metrics);
    EXPECT_EQ(err, NO_OK);
    EXPECT_EQ(metrics.total_sources, 1u);
    EXPECT_GT(metrics.total_simulation_time, 0.0f);
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(NitricOxideTest, Reset) {
    float position[3] = {0.0f, 0.0f, 0.0f};
    uint32_t source_id;
    nimcp_no_add_source(&system, position, NOS_TYPE_NNOS, &source_id);
    nimcp_no_update(&system, 100.0f);

    nimcp_no_error_t err = nimcp_no_reset(&system);
    EXPECT_EQ(err, NO_OK);
    EXPECT_EQ(system.update_count, 0u);
}

//=============================================================================
// Error String Tests
//=============================================================================

TEST(NOErrorTest, AllErrorsHaveStrings) {
    EXPECT_NE(nimcp_no_error_string(NO_OK), nullptr);
    EXPECT_NE(nimcp_no_error_string(NO_ERR_NULL_PTR), nullptr);
    EXPECT_NE(nimcp_no_error_string(NO_ERR_INVALID_PARAM), nullptr);
    EXPECT_NE(nimcp_no_error_string(NO_ERR_NOT_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_no_error_string(NO_ERR_ALREADY_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_no_error_string(NO_ERR_NO_MEMORY), nullptr);
    EXPECT_NE(nimcp_no_error_string(NO_ERR_SOURCE_NOT_FOUND), nullptr);
    EXPECT_NE(nimcp_no_error_string(NO_ERR_CAPACITY_EXCEEDED), nullptr);
    EXPECT_NE(nimcp_no_error_string(NO_ERR_NOS_INACTIVE), nullptr);
    EXPECT_NE(nimcp_no_error_string(NO_ERR_SUBSTRATE_DEPLETED), nullptr);
}

TEST(NOErrorTest, UnknownErrorCode) {
    const char* str = nimcp_no_error_string((nimcp_no_error_t)-999);
    EXPECT_NE(str, nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
