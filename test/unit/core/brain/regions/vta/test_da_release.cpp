/**
 * @file test_da_release.cpp
 * @brief Unit tests for DA release dynamics
 * @date 2026-01-11
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/vta/nimcp_dopamine_release.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class DAReleaseTest : public ::testing::Test {
protected:
    nimcp_da_release_system_t system;

    void SetUp() override {
        int err = nimcp_da_release_init(&system, nullptr);
        ASSERT_EQ(err, 0);
    }

    void TearDown() override {
        nimcp_da_release_shutdown(&system);
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(DAReleaseTest, InitSucceeds) {
    EXPECT_TRUE(system.initialized);
}

TEST_F(DAReleaseTest, InitNullReturnsError) {
    int err = nimcp_da_release_init(nullptr, nullptr);
    EXPECT_EQ(err, -1);
}

TEST_F(DAReleaseTest, ShutdownClearsState) {
    nimcp_da_release_shutdown(&system);
    EXPECT_FALSE(system.initialized);
}

TEST_F(DAReleaseTest, ResetWorks) {
    system.concentrations.synaptic = 200.0f;
    int err = nimcp_da_release_reset(&system);
    EXPECT_EQ(err, 0);
    EXPECT_FLOAT_EQ(system.concentrations.synaptic, 50.0f);
}

TEST_F(DAReleaseTest, DefaultConfigValid) {
    nimcp_da_release_config_t config = nimcp_da_release_default_config();
    EXPECT_GT(config.release_probability, 0.0f);
    EXPECT_GT(config.dat_vmax, 0.0f);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(DAReleaseTest, UpdateSucceeds) {
    int err = nimcp_da_release_update(&system, 10.0f, 5.0f);
    EXPECT_EQ(err, 0);
}

TEST_F(DAReleaseTest, UpdateNullReturnsError) {
    int err = nimcp_da_release_update(nullptr, 10.0f, 5.0f);
    EXPECT_EQ(err, -1);
}

TEST_F(DAReleaseTest, UpdateInvalidDtReturnsError) {
    int err = nimcp_da_release_update(&system, -1.0f, 5.0f);
    EXPECT_EQ(err, -1);
}

TEST_F(DAReleaseTest, HighFiringRateIncreasesDA) {
    /* First deplete DA */
    for (int i = 0; i < 100; i++) {
        nimcp_da_release_update(&system, 10.0f, 0.0f);
    }

    float before;
    nimcp_da_get_concentration(&system, DA_COMPARTMENT_SYNAPTIC, &before);

    /* High firing with parameters that trigger spikes */
    for (int i = 0; i < 100; i++) {
        nimcp_da_release_update(&system, 100.0f, 100.0f);
    }

    float after;
    nimcp_da_get_concentration(&system, DA_COMPARTMENT_SYNAPTIC, &after);
    EXPECT_GT(after, before);
}

TEST_F(DAReleaseTest, ZeroFiringRateDecreasesDA) {
    /* First increase DA */
    for (int i = 0; i < 50; i++) {
        nimcp_da_release_update(&system, 100.0f, 100.0f);
    }

    float before;
    nimcp_da_get_concentration(&system, DA_COMPARTMENT_SYNAPTIC, &before);

    /* Then let it decay */
    for (int i = 0; i < 200; i++) {
        nimcp_da_release_update(&system, 10.0f, 0.0f);
    }

    float after;
    nimcp_da_get_concentration(&system, DA_COMPARTMENT_SYNAPTIC, &after);
    EXPECT_LT(after, before);
}

TEST_F(DAReleaseTest, LongTermStability) {
    for (int i = 0; i < 10000; i++) {
        nimcp_da_release_update(&system, 1.0f, 5.0f);
    }

    float conc;
    nimcp_da_get_concentration(&system, DA_COMPARTMENT_SYNAPTIC, &conc);
    EXPECT_FALSE(std::isnan(conc));
    EXPECT_FALSE(std::isinf(conc));
    EXPECT_GE(conc, 0.0f);
}

//=============================================================================
// Concentration Tests
//=============================================================================

TEST_F(DAReleaseTest, GetConcentrationSucceeds) {
    float conc;
    int err = nimcp_da_get_concentration(&system, DA_COMPARTMENT_SYNAPTIC, &conc);
    EXPECT_EQ(err, 0);
    EXPECT_GE(conc, 0.0f);
}

TEST_F(DAReleaseTest, SetConcentrationSucceeds) {
    int err = nimcp_da_set_concentration(&system, DA_COMPARTMENT_SYNAPTIC, 100.0f);
    EXPECT_EQ(err, 0);

    float conc;
    nimcp_da_get_concentration(&system, DA_COMPARTMENT_SYNAPTIC, &conc);
    EXPECT_FLOAT_EQ(conc, 100.0f);
}

TEST_F(DAReleaseTest, GetTotalExtracellularSucceeds) {
    float total;
    int err = nimcp_da_get_total_extracellular(&system, &total);
    EXPECT_EQ(err, 0);
    EXPECT_GT(total, 0.0f);
}

TEST_F(DAReleaseTest, AllCompartmentsValid) {
    float conc;
    EXPECT_EQ(nimcp_da_get_concentration(&system, DA_COMPARTMENT_VESICULAR, &conc), 0);
    EXPECT_EQ(nimcp_da_get_concentration(&system, DA_COMPARTMENT_CYTOSOLIC, &conc), 0);
    EXPECT_EQ(nimcp_da_get_concentration(&system, DA_COMPARTMENT_SYNAPTIC, &conc), 0);
    EXPECT_EQ(nimcp_da_get_concentration(&system, DA_COMPARTMENT_EXTRASYNAPTIC, &conc), 0);
}

//=============================================================================
// Receptor Tests
//=============================================================================

TEST_F(DAReleaseTest, GetReceptorActivation) {
    float activation;
    int err = nimcp_da_get_receptor_activation(&system, DA_RECEPTOR_TYPE_D1, &activation);
    EXPECT_EQ(err, 0);
    EXPECT_GE(activation, 0.0f);
    EXPECT_LE(activation, 1.0f);
}

TEST_F(DAReleaseTest, ReceptorActivationIncreasesWithDA) {
    float before;
    nimcp_da_get_receptor_activation(&system, DA_RECEPTOR_TYPE_D1, &before);

    nimcp_da_set_concentration(&system, DA_COMPARTMENT_SYNAPTIC, 200.0f);
    for (int i = 0; i < 100; i++) {
        nimcp_da_release_update(&system, 1.0f, 0.0f);
    }

    float after;
    nimcp_da_get_receptor_activation(&system, DA_RECEPTOR_TYPE_D1, &after);
    EXPECT_GT(after, before);
}

TEST_F(DAReleaseTest, AllReceptorTypesWork) {
    float activation;
    EXPECT_EQ(nimcp_da_get_receptor_activation(&system, DA_RECEPTOR_TYPE_D1, &activation), 0);
    EXPECT_EQ(nimcp_da_get_receptor_activation(&system, DA_RECEPTOR_TYPE_D2, &activation), 0);
    EXPECT_EQ(nimcp_da_get_receptor_activation(&system, DA_RECEPTOR_TYPE_D3, &activation), 0);
}

TEST_F(DAReleaseTest, D1D2BalanceValid) {
    float balance;
    int err = nimcp_da_get_d1_d2_balance(&system, &balance);
    EXPECT_EQ(err, 0);
}

//=============================================================================
// Transporter Tests
//=============================================================================

TEST_F(DAReleaseTest, SetDATInhibitionSucceeds) {
    int err = nimcp_da_set_dat_inhibition(&system, 0.5f);
    EXPECT_EQ(err, 0);
    EXPECT_FLOAT_EQ(system.transporter.inhibition, 0.5f);
}

TEST_F(DAReleaseTest, DATInhibitionClamped) {
    nimcp_da_set_dat_inhibition(&system, 1.5f);
    EXPECT_FLOAT_EQ(system.transporter.inhibition, 1.0f);

    nimcp_da_set_dat_inhibition(&system, -0.5f);
    EXPECT_FLOAT_EQ(system.transporter.inhibition, 0.0f);
}

TEST_F(DAReleaseTest, GetUptakeRateSucceeds) {
    float rate;
    int err = nimcp_da_get_uptake_rate(&system, &rate);
    EXPECT_EQ(err, 0);
}

//=============================================================================
// Autoreceptor Tests
//=============================================================================

TEST_F(DAReleaseTest, AutoreceptorFeedbackInRange) {
    float feedback;
    int err = nimcp_da_get_autoreceptor_feedback(&system, &feedback);
    EXPECT_EQ(err, 0);
    EXPECT_GE(feedback, 0.0f);
    EXPECT_LE(feedback, 1.0f);
}

TEST_F(DAReleaseTest, HighDAIncreasesAutoreceptorFeedback) {
    float before;
    nimcp_da_get_autoreceptor_feedback(&system, &before);

    nimcp_da_set_concentration(&system, DA_COMPARTMENT_SYNAPTIC, 200.0f);
    for (int i = 0; i < 100; i++) {
        nimcp_da_release_update(&system, 1.0f, 0.0f);
    }

    float after;
    nimcp_da_get_autoreceptor_feedback(&system, &after);
    EXPECT_GT(after, before);
}

//=============================================================================
// Vesicle Pool Tests
//=============================================================================

TEST_F(DAReleaseTest, GetVesiclePoolSucceeds) {
    nimcp_da_vesicle_pool_t pool;
    int err = nimcp_da_get_vesicle_pool(&system, &pool);
    EXPECT_EQ(err, 0);
    EXPECT_GT(pool.readily_releasable, 0u);
}

TEST_F(DAReleaseTest, GetReleaseEfficacySucceeds) {
    float efficacy;
    int err = nimcp_da_get_release_efficacy(&system, &efficacy);
    EXPECT_EQ(err, 0);
    EXPECT_GT(efficacy, 0.0f);
    EXPECT_LE(efficacy, 1.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
