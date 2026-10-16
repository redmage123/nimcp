/**
 * @file test_ne_release.cpp
 * @brief Unit tests for Norepinephrine Release Dynamics
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/locus_coeruleus/nimcp_norepinephrine_release.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class NEReleaseTest : public ::testing::Test {
protected:
    nimcp_ne_release_system_t system;

    void SetUp() override {
        memset(&system, 0, sizeof(system));
        int err = nimcp_ne_release_init(&system, nullptr);
        ASSERT_EQ(err, 0);
    }

    void TearDown() override {
        nimcp_ne_release_shutdown(&system);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(NEReleaseTest, InitializesSuccessfully) {
    EXPECT_TRUE(system.initialized);
}

TEST_F(NEReleaseTest, InitNullReturnsError) {
    int err = nimcp_ne_release_init(nullptr, nullptr);
    EXPECT_EQ(err, -1);
}

TEST_F(NEReleaseTest, ShutdownClearsState) {
    int err = nimcp_ne_release_shutdown(&system);
    EXPECT_EQ(err, 0);
    EXPECT_FALSE(system.initialized);
}

TEST_F(NEReleaseTest, ResetRestoresBaseline) {
    /* Modify state */
    system.concentrations.synaptic = 100.0f;

    int err = nimcp_ne_release_reset(&system);
    EXPECT_EQ(err, 0);
    EXPECT_LT(system.concentrations.synaptic, 100.0f);
}

TEST_F(NEReleaseTest, CustomConfigApplied) {
    nimcp_ne_release_shutdown(&system);

    nimcp_ne_release_config_t config = nimcp_ne_release_default_config();
    config.initial_vesicles = 500;
    config.release_probability = 0.5f;

    int err = nimcp_ne_release_init(&system, &config);
    EXPECT_EQ(err, 0);
    EXPECT_EQ(system.vesicles.total_vesicles, 500u);
}

//=============================================================================
// Concentration Tests
//=============================================================================

TEST_F(NEReleaseTest, GetConcentrationVesicular) {
    float conc;
    int err = nimcp_ne_get_concentration(&system, NE_COMPARTMENT_VESICLE, &conc);
    EXPECT_EQ(err, 0);
    EXPECT_GT(conc, 0.0f);
}

TEST_F(NEReleaseTest, GetConcentrationSynaptic) {
    float conc;
    int err = nimcp_ne_get_concentration(&system, NE_COMPARTMENT_SYNAPTIC, &conc);
    EXPECT_EQ(err, 0);
    EXPECT_GE(conc, 0.0f);
}

TEST_F(NEReleaseTest, GetConcentrationAllCompartments) {
    float conc;
    for (int i = 0; i < NE_COMPARTMENT_COUNT; i++) {
        int err = nimcp_ne_get_concentration(&system, (nimcp_ne_compartment_t)i, &conc);
        EXPECT_EQ(err, 0);
        EXPECT_FALSE(std::isnan(conc));
    }
}

//=============================================================================
// Release Trigger Tests
//=============================================================================

TEST_F(NEReleaseTest, TriggerReleaseIncreasesSynapticNE) {
    float before;
    nimcp_ne_get_concentration(&system, NE_COMPARTMENT_SYNAPTIC, &before);

    nimcp_ne_release_trigger(&system, 10);

    float after;
    nimcp_ne_get_concentration(&system, NE_COMPARTMENT_SYNAPTIC, &after);
    EXPECT_GE(after, before);
}

TEST_F(NEReleaseTest, ZeroSpikesNoRelease) {
    float before;
    nimcp_ne_get_concentration(&system, NE_COMPARTMENT_SYNAPTIC, &before);

    nimcp_ne_release_trigger(&system, 0);

    float after;
    nimcp_ne_get_concentration(&system, NE_COMPARTMENT_SYNAPTIC, &after);
    EXPECT_FLOAT_EQ(after, before);
}

TEST_F(NEReleaseTest, VesiclePoolDepletesWithRelease) {
    uint32_t initial_ready = system.vesicles.ready_pool;

    /* Many release events */
    for (int i = 0; i < 100; i++) {
        nimcp_ne_release_trigger(&system, 5);
    }

    /* Some vesicles should be depleted */
    EXPECT_GT(system.vesicles.depleted, 0u);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(NEReleaseTest, UpdateSucceeds) {
    int err = nimcp_ne_release_update(&system, 10.0f, 2.0f);
    EXPECT_EQ(err, 0);
}

TEST_F(NEReleaseTest, UpdateInvalidDtReturnsError) {
    int err = nimcp_ne_release_update(&system, -1.0f, 2.0f);
    EXPECT_EQ(err, -1);
}

TEST_F(NEReleaseTest, HighFiringRateIncreasesNE) {
    /* First deplete NE to start from low baseline */
    for (int i = 0; i < 100; i++) {
        nimcp_ne_release_update(&system, 10.0f, 0.0f);  /* No firing - decay */
    }

    float before;
    nimcp_ne_get_concentration(&system, NE_COMPARTMENT_SYNAPTIC, &before);

    /* Now apply high firing rate with large enough dt to trigger spikes
     * Need firing_rate * dt / 1000 >= 1 for spike, so dt >= 1000/firing_rate
     * With 100 Hz, need dt >= 10ms. Use 100ms steps with 100 Hz. */
    for (int i = 0; i < 100; i++) {
        nimcp_ne_release_update(&system, 100.0f, 100.0f);  /* 100 Hz, 100ms = 10 spikes/update */
    }

    float after;
    nimcp_ne_get_concentration(&system, NE_COMPARTMENT_SYNAPTIC, &after);
    EXPECT_GT(after, before);
}

TEST_F(NEReleaseTest, ZeroFiringRateDecreasesNE) {
    /* First increase NE with parameters that trigger spikes */
    for (int i = 0; i < 50; i++) {
        nimcp_ne_release_update(&system, 100.0f, 100.0f);  /* 10 spikes/update */
    }

    float before;
    nimcp_ne_get_concentration(&system, NE_COMPARTMENT_SYNAPTIC, &before);

    /* Then let it decay */
    for (int i = 0; i < 200; i++) {
        nimcp_ne_release_update(&system, 10.0f, 0.0f);
    }

    float after;
    nimcp_ne_get_concentration(&system, NE_COMPARTMENT_SYNAPTIC, &after);
    EXPECT_LT(after, before);
}

TEST_F(NEReleaseTest, LongTermStability) {
    for (int i = 0; i < 10000; i++) {
        nimcp_ne_release_update(&system, 1.0f, 3.0f);
    }

    float conc;
    nimcp_ne_get_concentration(&system, NE_COMPARTMENT_SYNAPTIC, &conc);
    EXPECT_FALSE(std::isnan(conc));
    EXPECT_FALSE(std::isinf(conc));
    EXPECT_GE(conc, 0.0f);
}

//=============================================================================
// Receptor Tests
//=============================================================================

TEST_F(NEReleaseTest, GetReceptorActivationAlpha1) {
    float activation;
    int err = nimcp_ne_get_receptor_activation(&system, NE_RECEPTOR_ALPHA1, &activation);
    EXPECT_EQ(err, 0);
    EXPECT_GE(activation, 0.0f);
    EXPECT_LE(activation, 1.0f);
}

TEST_F(NEReleaseTest, ReceptorActivationIncreasesWithNE) {
    float before;
    nimcp_ne_get_receptor_activation(&system, NE_RECEPTOR_ALPHA1, &before);

    /* Increase NE */
    for (int i = 0; i < 100; i++) {
        nimcp_ne_release_update(&system, 10.0f, 20.0f);
    }

    float after;
    nimcp_ne_get_receptor_activation(&system, NE_RECEPTOR_ALPHA1, &after);
    EXPECT_GT(after, before);
}

TEST_F(NEReleaseTest, AllReceptorTypesWork) {
    for (int r = 0; r < NE_RECEPTOR_COUNT; r++) {
        float activation;
        int err = nimcp_ne_get_receptor_activation(&system, (nimcp_ne_receptor_t)r, &activation);
        EXPECT_EQ(err, 0);
        EXPECT_GE(activation, 0.0f);
        EXPECT_LE(activation, 1.0f);
    }
}

//=============================================================================
// Transporter Tests
//=============================================================================

TEST_F(NEReleaseTest, TransporterInhibitionReducesUptake) {
    /* Increase synaptic NE */
    for (int i = 0; i < 50; i++) {
        nimcp_ne_release_update(&system, 10.0f, 20.0f);
    }

    float ne_normal;
    nimcp_ne_get_concentration(&system, NE_COMPARTMENT_SYNAPTIC, &ne_normal);

    /* Reset and try with inhibited transporter */
    nimcp_ne_release_reset(&system);
    nimcp_ne_apply_net_inhibition(&system, 0.9f);

    for (int i = 0; i < 50; i++) {
        nimcp_ne_release_update(&system, 10.0f, 20.0f);
    }

    float ne_inhibited;
    nimcp_ne_get_concentration(&system, NE_COMPARTMENT_SYNAPTIC, &ne_inhibited);

    /* With inhibited transporter, more NE should remain */
    EXPECT_GT(ne_inhibited, ne_normal);
}

//=============================================================================
// Autoreceptor Tests
//=============================================================================

TEST_F(NEReleaseTest, AutoreceptorFeedbackInRange) {
    float feedback;
    int err = nimcp_ne_get_autoreceptor_feedback(&system, &feedback);
    EXPECT_EQ(err, 0);
    EXPECT_GE(feedback, 0.0f);
    EXPECT_LE(feedback, 1.0f);
}

TEST_F(NEReleaseTest, HighNEIncreasesAutoreceptorFeedback) {
    float before;
    nimcp_ne_get_autoreceptor_feedback(&system, &before);

    /* Increase NE a lot */
    for (int i = 0; i < 200; i++) {
        nimcp_ne_release_update(&system, 10.0f, 20.0f);
    }

    float after;
    nimcp_ne_get_autoreceptor_feedback(&system, &after);
    EXPECT_GT(after, before);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
