/**
 * @file test_serotonin_release.cpp
 * @brief Unit tests for 5-HT (Serotonin) release dynamics
 */

#include <gtest/gtest.h>

extern "C" {
#include "core/brain/regions/raphe/nimcp_serotonin_release.h"
}

class SerotoninReleaseTest : public ::testing::Test {
protected:
    nimcp_ht_release_system_t system;

    void SetUp() override {
        memset(&system, 0, sizeof(system));
    }

    void TearDown() override {
        if (system.initialized) {
            nimcp_ht_release_shutdown(&system);
        }
    }
};

/* ==========================================================================
 * Lifecycle Tests
 * ========================================================================== */

TEST_F(SerotoninReleaseTest, DefaultConfigHasValidValues) {
    nimcp_ht_release_config_t config = nimcp_ht_release_default_config();

    EXPECT_FLOAT_EQ(config.release_probability, HT_DEFAULT_RELEASE_PROB);
    EXPECT_FLOAT_EQ(config.sert_vmax, HT_DEFAULT_SERT_VMAX);
    EXPECT_FLOAT_EQ(config.sert_km, HT_DEFAULT_SERT_KM);
    EXPECT_FLOAT_EQ(config.mao_rate, HT_DEFAULT_MAO_RATE);
    EXPECT_TRUE(config.enable_volume_transmission);
}

TEST_F(SerotoninReleaseTest, InitWithNullReturnsError) {
    EXPECT_EQ(nimcp_ht_release_init(nullptr, nullptr), -1);
}

TEST_F(SerotoninReleaseTest, InitWithDefaultConfigSucceeds) {
    EXPECT_EQ(nimcp_ht_release_init(&system, nullptr), 0);
    EXPECT_TRUE(system.initialized);
}

TEST_F(SerotoninReleaseTest, InitWithCustomConfigSucceeds) {
    nimcp_ht_release_config_t config = nimcp_ht_release_default_config();
    config.release_probability = 0.5f;

    EXPECT_EQ(nimcp_ht_release_init(&system, &config), 0);
    EXPECT_FLOAT_EQ(system.config.release_probability, 0.5f);
}

TEST_F(SerotoninReleaseTest, InitSetsCorrectCompartments) {
    EXPECT_EQ(nimcp_ht_release_init(&system, nullptr), 0);

    EXPECT_GT(system.concentrations.vesicular, 0.0f);
    EXPECT_GT(system.concentrations.cytosolic, 0.0f);
    EXPECT_FLOAT_EQ(system.concentrations.synaptic, HT_DEFAULT_BASELINE);
}

TEST_F(SerotoninReleaseTest, ShutdownSucceeds) {
    nimcp_ht_release_init(&system, nullptr);
    EXPECT_EQ(nimcp_ht_release_shutdown(&system), 0);
    EXPECT_FALSE(system.initialized);
}

TEST_F(SerotoninReleaseTest, ResetSucceeds) {
    nimcp_ht_release_init(&system, nullptr);

    /* Modify state */
    system.concentrations.synaptic = 100.0f;

    EXPECT_EQ(nimcp_ht_release_reset(&system), 0);
    EXPECT_FLOAT_EQ(system.concentrations.synaptic, HT_DEFAULT_BASELINE);
}

/* ==========================================================================
 * Update Tests
 * ========================================================================== */

TEST_F(SerotoninReleaseTest, UpdateWithNullReturnsError) {
    EXPECT_EQ(nimcp_ht_release_update(nullptr, 2.0f, 10.0f), -1);
}

TEST_F(SerotoninReleaseTest, UpdateWithoutInitReturnsError) {
    EXPECT_EQ(nimcp_ht_release_update(&system, 2.0f, 10.0f), -1);
}

TEST_F(SerotoninReleaseTest, UpdateWithFiringIncreasesRelease) {
    nimcp_ht_release_init(&system, nullptr);

    float initial_synaptic = system.concentrations.synaptic;

    /* High firing rate should cause release into synaptic space */
    nimcp_ht_release_update(&system, 10.0f, 100.0f);

    /* Synaptic concentration should increase with firing */
    EXPECT_GT(system.concentrations.synaptic, initial_synaptic);
}

TEST_F(SerotoninReleaseTest, UpdateWithZeroFiringDecreasesConcentration) {
    nimcp_ht_release_init(&system, nullptr);

    /* Set initial synaptic concentration above baseline */
    system.concentrations.synaptic = 50.0f;
    float initial = system.concentrations.synaptic;

    /* Zero firing + SERT should decrease concentration */
    for (int i = 0; i < 100; i++) {
        nimcp_ht_release_update(&system, 0.0f, 10.0f);
    }

    EXPECT_LT(system.concentrations.synaptic, initial);
}

TEST_F(SerotoninReleaseTest, UpdateActivatesReceptors) {
    nimcp_ht_release_init(&system, nullptr);

    /* Run with moderate firing */
    for (int i = 0; i < 50; i++) {
        nimcp_ht_release_update(&system, 5.0f, 10.0f);
    }

    /* Receptors should be activated */
    float activation;
    nimcp_ht_get_receptor_activation(&system, HT_RECEPTOR_TYPE_1A, &activation);
    EXPECT_GT(activation, 0.0f);
}

TEST_F(SerotoninReleaseTest, UpdateComputesAutoreceptorFeedback) {
    nimcp_ht_release_init(&system, nullptr);

    /* High 5-HT -> autoreceptor activation */
    system.concentrations.synaptic = 50.0f;
    system.concentrations.extrasynaptic = 30.0f;

    nimcp_ht_release_update(&system, 5.0f, 10.0f);

    EXPECT_GT(system.autoreceptor.activation_1a, 0.0f);
    EXPECT_GT(system.autoreceptor.feedback_strength, 0.0f);
}

/* ==========================================================================
 * Concentration API Tests
 * ========================================================================== */

TEST_F(SerotoninReleaseTest, GetConcentrationWithNullReturnsError) {
    nimcp_ht_release_init(&system, nullptr);

    float conc;
    EXPECT_EQ(nimcp_ht_get_concentration(nullptr, HT_COMPARTMENT_SYNAPTIC, &conc), -1);
    EXPECT_EQ(nimcp_ht_get_concentration(&system, HT_COMPARTMENT_SYNAPTIC, nullptr), -1);
}

TEST_F(SerotoninReleaseTest, GetConcentrationReturnsCorrectCompartments) {
    nimcp_ht_release_init(&system, nullptr);

    float vesicular, cytosolic, synaptic, extrasynaptic;

    EXPECT_EQ(nimcp_ht_get_concentration(&system, HT_COMPARTMENT_VESICULAR, &vesicular), 0);
    EXPECT_EQ(nimcp_ht_get_concentration(&system, HT_COMPARTMENT_CYTOSOLIC, &cytosolic), 0);
    EXPECT_EQ(nimcp_ht_get_concentration(&system, HT_COMPARTMENT_SYNAPTIC, &synaptic), 0);
    EXPECT_EQ(nimcp_ht_get_concentration(&system, HT_COMPARTMENT_EXTRASYNAPTIC, &extrasynaptic), 0);

    EXPECT_GT(vesicular, 0.0f);
    EXPECT_GT(cytosolic, 0.0f);
    EXPECT_GT(synaptic, 0.0f);
}

TEST_F(SerotoninReleaseTest, SetConcentrationSucceeds) {
    nimcp_ht_release_init(&system, nullptr);

    EXPECT_EQ(nimcp_ht_set_concentration(&system, HT_COMPARTMENT_SYNAPTIC, 50.0f), 0);
    EXPECT_FLOAT_EQ(system.concentrations.synaptic, 50.0f);
}

TEST_F(SerotoninReleaseTest, SetConcentrationRejectsNegative) {
    nimcp_ht_release_init(&system, nullptr);

    EXPECT_EQ(nimcp_ht_set_concentration(&system, HT_COMPARTMENT_SYNAPTIC, -10.0f), -1);
}

TEST_F(SerotoninReleaseTest, GetTotalExtracellularSucceeds) {
    nimcp_ht_release_init(&system, nullptr);

    system.concentrations.synaptic = 30.0f;
    system.concentrations.extrasynaptic = 20.0f;

    float total;
    EXPECT_EQ(nimcp_ht_get_total_extracellular(&system, &total), 0);
    EXPECT_FLOAT_EQ(total, 50.0f);
}

/* ==========================================================================
 * Receptor API Tests
 * ========================================================================== */

TEST_F(SerotoninReleaseTest, GetReceptorActivationWithNullReturnsError) {
    nimcp_ht_release_init(&system, nullptr);

    float activation;
    EXPECT_EQ(nimcp_ht_get_receptor_activation(nullptr, HT_RECEPTOR_TYPE_1A, &activation), -1);
    EXPECT_EQ(nimcp_ht_get_receptor_activation(&system, HT_RECEPTOR_TYPE_1A, nullptr), -1);
}

TEST_F(SerotoninReleaseTest, GetReceptorActivationReturnsValidRange) {
    nimcp_ht_release_init(&system, nullptr);

    /* Run update to activate receptors */
    system.concentrations.synaptic = 30.0f;
    nimcp_ht_release_update(&system, 5.0f, 10.0f);

    float activation;
    for (int i = 0; i < HT_RECEPTOR_TYPE_COUNT; i++) {
        EXPECT_EQ(nimcp_ht_get_receptor_activation(&system, (nimcp_ht_receptor_type_t)i, &activation), 0);
        EXPECT_GE(activation, 0.0f);
        EXPECT_LE(activation, 1.0f);
    }
}

TEST_F(SerotoninReleaseTest, Get1A2ABalanceSucceeds) {
    nimcp_ht_release_init(&system, nullptr);

    /* Run updates */
    for (int i = 0; i < 20; i++) {
        nimcp_ht_release_update(&system, 5.0f, 10.0f);
    }

    float balance;
    EXPECT_EQ(nimcp_ht_get_1a_2a_balance(&system, &balance), 0);
    /* Balance is 2A - 1A, can be positive or negative */
    EXPECT_GE(balance, -1.0f);
    EXPECT_LE(balance, 1.0f);
}

/* ==========================================================================
 * Transporter API Tests
 * ========================================================================== */

TEST_F(SerotoninReleaseTest, SetSERTInhibitionSucceeds) {
    nimcp_ht_release_init(&system, nullptr);

    EXPECT_EQ(nimcp_ht_set_sert_inhibition(&system, 0.5f), 0);
    EXPECT_FLOAT_EQ(system.transporter.inhibition, 0.5f);
}

TEST_F(SerotoninReleaseTest, SetSERTInhibitionClampsValue) {
    nimcp_ht_release_init(&system, nullptr);

    EXPECT_EQ(nimcp_ht_set_sert_inhibition(&system, 1.5f), 0);
    EXPECT_FLOAT_EQ(system.transporter.inhibition, 1.0f);

    EXPECT_EQ(nimcp_ht_set_sert_inhibition(&system, -0.5f), 0);
    EXPECT_FLOAT_EQ(system.transporter.inhibition, 0.0f);
}

TEST_F(SerotoninReleaseTest, SERTInhibitionReducesReuptake) {
    nimcp_ht_release_init(&system, nullptr);

    /* Set high synaptic concentration */
    system.concentrations.synaptic = 50.0f;

    /* Run without inhibition */
    float rate_without;
    nimcp_ht_release_update(&system, 0.0f, 10.0f);
    nimcp_ht_get_uptake_rate(&system, &rate_without);

    /* Reset and run with inhibition */
    system.concentrations.synaptic = 50.0f;
    nimcp_ht_set_sert_inhibition(&system, 0.8f);
    float rate_with;
    nimcp_ht_get_uptake_rate(&system, &rate_with);

    EXPECT_LT(rate_with, rate_without);
}

TEST_F(SerotoninReleaseTest, GetUptakeRateSucceeds) {
    nimcp_ht_release_init(&system, nullptr);

    float rate;
    EXPECT_EQ(nimcp_ht_get_uptake_rate(&system, &rate), 0);
    EXPECT_GE(rate, 0.0f);
}

/* ==========================================================================
 * Autoreceptor API Tests
 * ========================================================================== */

TEST_F(SerotoninReleaseTest, GetAutoreceptorFeedbackSucceeds) {
    nimcp_ht_release_init(&system, nullptr);

    /* High 5-HT -> autoreceptor activation */
    system.concentrations.synaptic = 50.0f;
    nimcp_ht_release_update(&system, 5.0f, 10.0f);

    float feedback;
    EXPECT_EQ(nimcp_ht_get_autoreceptor_feedback(&system, &feedback), 0);
    EXPECT_GE(feedback, 0.0f);
}

TEST_F(SerotoninReleaseTest, HighHTIncreasesAutoreceptorFeedback) {
    nimcp_ht_release_init(&system, nullptr);

    /* Low 5-HT */
    system.concentrations.synaptic = 5.0f;
    system.concentrations.extrasynaptic = 2.0f;
    nimcp_ht_release_update(&system, 5.0f, 10.0f);
    float low_feedback = system.autoreceptor.feedback_strength;

    /* High 5-HT */
    system.concentrations.synaptic = 80.0f;
    system.concentrations.extrasynaptic = 40.0f;
    nimcp_ht_release_update(&system, 5.0f, 10.0f);
    float high_feedback = system.autoreceptor.feedback_strength;

    EXPECT_GT(high_feedback, low_feedback);
}

/* ==========================================================================
 * Vesicle API Tests
 * ========================================================================== */

TEST_F(SerotoninReleaseTest, GetVesiclePoolSucceeds) {
    nimcp_ht_release_init(&system, nullptr);

    nimcp_ht_vesicle_pool_t pool;
    EXPECT_EQ(nimcp_ht_get_vesicle_pool(&system, &pool), 0);
    EXPECT_GT(pool.readily_releasable, 0u);
    EXPECT_GT(pool.reserve, 0u);
}

TEST_F(SerotoninReleaseTest, GetReleaseEfficacySucceeds) {
    nimcp_ht_release_init(&system, nullptr);

    /* Initially should be high */
    float efficacy;
    EXPECT_EQ(nimcp_ht_get_release_efficacy(&system, &efficacy), 0);
    EXPECT_NEAR(efficacy, 1.0f, 0.1f);
}

TEST_F(SerotoninReleaseTest, ReleaseEfficacyDecreasesWithAutoreceptorActivation) {
    nimcp_ht_release_init(&system, nullptr);

    /* High 5-HT activates autoreceptors which reduce release efficacy */
    system.concentrations.synaptic = 80.0f;
    system.concentrations.extrasynaptic = 40.0f;

    for (int i = 0; i < 20; i++) {
        nimcp_ht_release_update(&system, 5.0f, 10.0f);
    }

    float efficacy;
    nimcp_ht_get_release_efficacy(&system, &efficacy);
    EXPECT_LT(efficacy, 1.0f);
}

/* ==========================================================================
 * Volume Transmission Tests
 * ========================================================================== */

TEST_F(SerotoninReleaseTest, VolumeTransmissionDiffuses5HT) {
    nimcp_ht_release_config_t config = nimcp_ht_release_default_config();
    config.enable_volume_transmission = true;
    nimcp_ht_release_init(&system, &config);

    /* Set high synaptic, low extrasynaptic */
    system.concentrations.synaptic = 50.0f;
    system.concentrations.extrasynaptic = 10.0f;

    /* Run updates */
    for (int i = 0; i < 100; i++) {
        nimcp_ht_release_update(&system, 2.0f, 10.0f);
    }

    /* Extrasynaptic should have increased */
    EXPECT_GT(system.concentrations.extrasynaptic, 10.0f);
}

TEST_F(SerotoninReleaseTest, DisabledVolumeTransmissionNoExtrasynaptic) {
    nimcp_ht_release_config_t config = nimcp_ht_release_default_config();
    config.enable_volume_transmission = false;
    nimcp_ht_release_init(&system, &config);

    float initial_extra = system.concentrations.extrasynaptic;

    /* Set high synaptic */
    system.concentrations.synaptic = 100.0f;

    /* Run updates - extrasynaptic shouldn't change much from diffusion */
    for (int i = 0; i < 10; i++) {
        nimcp_ht_release_update(&system, 2.0f, 10.0f);
    }

    /* Difference should be minimal without volume transmission */
    /* (Note: SERT still affects extrasynaptic) */
    EXPECT_NEAR(system.concentrations.extrasynaptic, initial_extra, 5.0f);
}

/* ==========================================================================
 * Compartment Bounds Tests
 * ========================================================================== */

TEST_F(SerotoninReleaseTest, SynapticConcentrationClamped) {
    nimcp_ht_release_init(&system, nullptr);

    /* Try to exceed max */
    system.concentrations.synaptic = 300.0f;
    nimcp_ht_release_update(&system, 0.0f, 10.0f);

    EXPECT_LE(system.concentrations.synaptic, HT_MAX_CONCENTRATION);
}

TEST_F(SerotoninReleaseTest, ConcentrationsStayNonNegative) {
    nimcp_ht_release_init(&system, nullptr);

    /* Set very low values */
    system.concentrations.synaptic = 0.1f;
    system.concentrations.cytosolic = 0.1f;

    /* Aggressive reuptake */
    for (int i = 0; i < 1000; i++) {
        nimcp_ht_release_update(&system, 0.0f, 10.0f);
    }

    EXPECT_GE(system.concentrations.synaptic, 0.0f);
    EXPECT_GE(system.concentrations.cytosolic, 0.0f);
    EXPECT_GE(system.concentrations.vesicular, 0.0f);
}
