/**
 * @file test_epigenetics.cpp
 * @brief Unit tests for Epigenetics module
 *
 * WHAT: Test suite for nimcp_epigenetics
 * WHY:  Verify methylation, histone, chromatin, and imprinting operations
 * HOW:  Unit tests for create, modify, and lifecycle operations
 *
 * @author NIMCP Development Team
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

extern "C" {
#include "biology/epigenetics/nimcp_epigenetics.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class EpigeneticsTest : public ::testing::Test {
protected:
    nimcp_epigenetics_t epi = nullptr;

    void SetUp() override {
        nimcp_epigenetics_config_t config = nimcp_epigenetics_default_config();
        epi = nimcp_epigenetics_create(&config);
        ASSERT_NE(epi, nullptr);
    }

    void TearDown() override {
        if (epi) {
            nimcp_epigenetics_destroy(epi);
            epi = nullptr;
        }
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST(EpigeneticsCreateTest, CreateWithDefaultConfig) {
    nimcp_epigenetics_t e = nimcp_epigenetics_create(nullptr);
    ASSERT_NE(e, nullptr);
    nimcp_epigenetics_destroy(e);
}

TEST(EpigeneticsCreateTest, CreateWithCustomConfig) {
    nimcp_epigenetics_config_t config = nimcp_epigenetics_default_config();
    config.max_neurons = 2048;
    config.methylation_stability = 0.98f;
    config.enable_environmental = true;

    nimcp_epigenetics_t e = nimcp_epigenetics_create(&config);
    ASSERT_NE(e, nullptr);
    nimcp_epigenetics_destroy(e);
}

TEST(EpigeneticsCreateTest, DestroyNull) {
    /* Should not crash */
    nimcp_epigenetics_destroy(nullptr);
}

TEST(EpigeneticsCreateTest, DefaultConfigValues) {
    nimcp_epigenetics_config_t config = nimcp_epigenetics_default_config();

    EXPECT_GT(config.max_neurons, 0u);
    EXPECT_GT(config.methylation_stability, 0.0f);
    EXPECT_LE(config.methylation_stability, 1.0f);
}

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(EpigeneticsTest, InitSuccess) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_init(epi, nullptr);
    EXPECT_EQ(err, EPIGENETICS_OK);
}

TEST_F(EpigeneticsTest, InitNull) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_init(nullptr, nullptr);
    EXPECT_EQ(err, EPIGENETICS_ERR_NULL_PTR);
}

TEST_F(EpigeneticsTest, DoubleInit) {
    nimcp_epigenetics_init(epi, nullptr);
    nimcp_epigenetics_error_t err = nimcp_epigenetics_init(epi, nullptr);
    EXPECT_EQ(err, EPIGENETICS_ERR_ALREADY_INITIALIZED);
}

TEST_F(EpigeneticsTest, ShutdownSuccess) {
    nimcp_epigenetics_init(epi, nullptr);
    nimcp_epigenetics_error_t err = nimcp_epigenetics_shutdown(epi);
    EXPECT_EQ(err, EPIGENETICS_OK);
}

TEST_F(EpigeneticsTest, ShutdownNotInitialized) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_shutdown(epi);
    EXPECT_EQ(err, EPIGENETICS_ERR_NOT_INITIALIZED);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(EpigeneticsTest, UpdateSuccess) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_update(epi, 0.1f);
    EXPECT_EQ(err, EPIGENETICS_OK);
}

TEST_F(EpigeneticsTest, UpdateNull) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_update(nullptr, 0.1f);
    EXPECT_EQ(err, EPIGENETICS_ERR_NULL_PTR);
}

TEST_F(EpigeneticsTest, MultipleUpdates) {
    for (int i = 0; i < 100; i++) {
        nimcp_epigenetics_error_t err = nimcp_epigenetics_update(epi, 0.01f);
        EXPECT_EQ(err, EPIGENETICS_OK);
    }
}

//=============================================================================
// State and Stats Tests
//=============================================================================

TEST_F(EpigeneticsTest, GetStateSuccess) {
    nimcp_epigenetics_state_t state;
    nimcp_epigenetics_error_t err = nimcp_epigenetics_get_state(epi, &state);
    EXPECT_EQ(err, EPIGENETICS_OK);
    EXPECT_GE(state.global_plasticity, 0.0f);
    EXPECT_LE(state.global_plasticity, 1.0f);
}

TEST_F(EpigeneticsTest, GetStateNull) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_get_state(epi, nullptr);
    EXPECT_EQ(err, EPIGENETICS_ERR_NULL_PTR);
}

TEST_F(EpigeneticsTest, GetStatsSuccess) {
    nimcp_epigenetics_stats_t stats;
    nimcp_epigenetics_error_t err = nimcp_epigenetics_get_stats(epi, &stats);
    EXPECT_EQ(err, EPIGENETICS_OK);
}

TEST_F(EpigeneticsTest, ResetStatsSuccess) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_reset_stats(epi);
    EXPECT_EQ(err, EPIGENETICS_OK);
}

TEST_F(EpigeneticsTest, ResetStatsNull) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_reset_stats(nullptr);
    EXPECT_EQ(err, EPIGENETICS_ERR_NULL_PTR);
}

//=============================================================================
// Methylation Tests
//=============================================================================

TEST_F(EpigeneticsTest, AddMethylationSuccess) {
    nimcp_methylation_site_t site = {
        .site_id = 1,
        .neuron_id = 100,
        .synapse_id = 0,
        .initial_level = 0.5f,
        .stability = 0.95f
    };

    nimcp_epigenetics_error_t err = nimcp_epigenetics_add_methylation(epi, &site);
    EXPECT_EQ(err, EPIGENETICS_OK);
}

TEST_F(EpigeneticsTest, AddMethylationNull) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_add_methylation(epi, nullptr);
    EXPECT_EQ(err, EPIGENETICS_ERR_NULL_PTR);
}

TEST_F(EpigeneticsTest, RemoveMethylation) {
    nimcp_methylation_site_t site = {
        .site_id = 42,
        .neuron_id = 100,
        .synapse_id = 0,
        .initial_level = 0.5f,
        .stability = 0.95f
    };

    nimcp_epigenetics_add_methylation(epi, &site);
    nimcp_epigenetics_error_t err = nimcp_epigenetics_remove_methylation(epi, 42);
    EXPECT_EQ(err, EPIGENETICS_OK);
}

TEST_F(EpigeneticsTest, RemoveMethylationNotFound) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_remove_methylation(epi, 999);
    EXPECT_EQ(err, EPIGENETICS_ERR_SITE_NOT_FOUND);
}

TEST_F(EpigeneticsTest, GetMethylation) {
    nimcp_methylation_site_t site = {
        .site_id = 1,
        .neuron_id = 100,
        .synapse_id = 0,
        .initial_level = 0.7f,
        .stability = 0.95f
    };

    nimcp_epigenetics_add_methylation(epi, &site);

    float level;
    nimcp_methylation_state_t state;
    nimcp_epigenetics_error_t err = nimcp_epigenetics_get_methylation(epi, 100, &level, &state);
    EXPECT_EQ(err, EPIGENETICS_OK);
    EXPECT_NEAR(level, 0.7f, 0.01f);
    EXPECT_EQ(state, METHYL_STATE_METHYLATED);
}

TEST_F(EpigeneticsTest, MethylateSynapse) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_methylate_synapse(epi, 123, 0.8f);
    EXPECT_EQ(err, EPIGENETICS_OK);

    /* Verify it was added - need to call update to refresh state counts */
    nimcp_epigenetics_update(epi, 0.01f);
    nimcp_epigenetics_state_t state;
    nimcp_epigenetics_get_state(epi, &state);
    EXPECT_EQ(state.active_methylations, 1u);
}

TEST_F(EpigeneticsTest, DemethylateSynapse) {
    nimcp_epigenetics_methylate_synapse(epi, 123, 0.8f);
    nimcp_epigenetics_error_t err = nimcp_epigenetics_demethylate_synapse(epi, 123);
    EXPECT_EQ(err, EPIGENETICS_OK);
}

TEST_F(EpigeneticsTest, DemethylateSynapseNotFound) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_demethylate_synapse(epi, 999);
    EXPECT_EQ(err, EPIGENETICS_ERR_SITE_NOT_FOUND);
}

//=============================================================================
// Plasticity Tests
//=============================================================================

TEST_F(EpigeneticsTest, GetPlasticityDefault) {
    float plasticity;
    nimcp_epigenetics_error_t err = nimcp_epigenetics_get_plasticity(epi, 0, &plasticity);
    EXPECT_EQ(err, EPIGENETICS_OK);
    EXPECT_NEAR(plasticity, 1.0f, 0.01f);  /* Default should be high */
}

TEST_F(EpigeneticsTest, GetPlasticityWithMethylation) {
    nimcp_methylation_site_t site = {
        .site_id = 1,
        .neuron_id = 100,
        .synapse_id = 0,
        .initial_level = 0.9f,
        .stability = 0.95f
    };

    nimcp_epigenetics_add_methylation(epi, &site);

    float plasticity;
    nimcp_epigenetics_get_plasticity(epi, 100, &plasticity);
    EXPECT_LT(plasticity, 1.0f);  /* Should be reduced by methylation */
}

TEST_F(EpigeneticsTest, GetPlasticityNull) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_get_plasticity(epi, 0, nullptr);
    EXPECT_EQ(err, EPIGENETICS_ERR_NULL_PTR);
}

//=============================================================================
// Histone Modification Tests
//=============================================================================

TEST_F(EpigeneticsTest, ModifyHistoneSuccess) {
    nimcp_histone_config_t config = {
        .type = HISTONE_MOD_ACETYLATION,
        .region_id = 1,
        .magnitude = 0.5f,
        .decay_rate = 0.01f,
        .is_activating = true
    };

    nimcp_epigenetics_error_t err = nimcp_epigenetics_modify_histone(epi, &config);
    EXPECT_EQ(err, EPIGENETICS_OK);
}

TEST_F(EpigeneticsTest, ModifyHistoneNull) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_modify_histone(epi, nullptr);
    EXPECT_EQ(err, EPIGENETICS_ERR_NULL_PTR);
}

TEST_F(EpigeneticsTest, GetHistoneState) {
    nimcp_histone_config_t config = {
        .type = HISTONE_MOD_ACETYLATION,
        .region_id = 1,
        .magnitude = 0.6f,
        .decay_rate = 0.01f,
        .is_activating = true
    };

    nimcp_epigenetics_modify_histone(epi, &config);

    float acet, meth;
    nimcp_epigenetics_error_t err = nimcp_epigenetics_get_histone_state(epi, 1, &acet, &meth);
    EXPECT_EQ(err, EPIGENETICS_OK);
    EXPECT_NEAR(acet, 0.6f, 0.01f);
}

TEST_F(EpigeneticsTest, ClearHistones) {
    nimcp_histone_config_t config = {
        .type = HISTONE_MOD_ACETYLATION,
        .region_id = 1,
        .magnitude = 0.5f,
        .decay_rate = 0.01f,
        .is_activating = true
    };

    nimcp_epigenetics_modify_histone(epi, &config);
    nimcp_epigenetics_error_t err = nimcp_epigenetics_clear_histones(epi, 1);
    EXPECT_EQ(err, EPIGENETICS_OK);
}

//=============================================================================
// Chromatin Region Tests
//=============================================================================

TEST_F(EpigeneticsTest, ConfigureRegionSuccess) {
    nimcp_chromatin_config_t config = {
        .region_id = 1,
        .start_neuron = 0,
        .end_neuron = 99,
        .initial_state = CHROMATIN_STATE_OPEN,
        .transition_threshold = 0.5f
    };

    nimcp_epigenetics_error_t err = nimcp_epigenetics_configure_region(epi, &config);
    EXPECT_EQ(err, EPIGENETICS_OK);
}

TEST_F(EpigeneticsTest, ConfigureRegionNull) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_configure_region(epi, nullptr);
    EXPECT_EQ(err, EPIGENETICS_ERR_NULL_PTR);
}

TEST_F(EpigeneticsTest, GetChromatinState) {
    nimcp_chromatin_config_t config = {
        .region_id = 1,
        .start_neuron = 0,
        .end_neuron = 99,
        .initial_state = CHROMATIN_STATE_POISED,
        .transition_threshold = 0.5f
    };

    nimcp_epigenetics_configure_region(epi, &config);

    nimcp_chromatin_state_t state;
    nimcp_epigenetics_error_t err = nimcp_epigenetics_get_chromatin_state(epi, 1, &state);
    EXPECT_EQ(err, EPIGENETICS_OK);
    EXPECT_EQ(state, CHROMATIN_STATE_POISED);
}

TEST_F(EpigeneticsTest, GetChromatinStateNotFound) {
    nimcp_chromatin_state_t state;
    nimcp_epigenetics_error_t err = nimcp_epigenetics_get_chromatin_state(epi, 999, &state);
    EXPECT_EQ(err, EPIGENETICS_ERR_SITE_NOT_FOUND);
}

TEST_F(EpigeneticsTest, OpenRegion) {
    nimcp_chromatin_config_t config = {
        .region_id = 1,
        .start_neuron = 0,
        .end_neuron = 99,
        .initial_state = CHROMATIN_STATE_CLOSED,
        .transition_threshold = 0.5f
    };

    nimcp_epigenetics_configure_region(epi, &config);
    nimcp_epigenetics_error_t err = nimcp_epigenetics_open_region(epi, 1);
    EXPECT_EQ(err, EPIGENETICS_OK);

    nimcp_chromatin_state_t state;
    nimcp_epigenetics_get_chromatin_state(epi, 1, &state);
    EXPECT_EQ(state, CHROMATIN_STATE_OPEN);
}

TEST_F(EpigeneticsTest, CloseRegion) {
    nimcp_chromatin_config_t config = {
        .region_id = 1,
        .start_neuron = 0,
        .end_neuron = 99,
        .initial_state = CHROMATIN_STATE_OPEN,
        .transition_threshold = 0.5f
    };

    nimcp_epigenetics_configure_region(epi, &config);
    nimcp_epigenetics_error_t err = nimcp_epigenetics_close_region(epi, 1);
    EXPECT_EQ(err, EPIGENETICS_OK);

    nimcp_chromatin_state_t state;
    nimcp_epigenetics_get_chromatin_state(epi, 1, &state);
    EXPECT_EQ(state, CHROMATIN_STATE_CLOSED);
}

//=============================================================================
// Imprinting Tests
//=============================================================================

TEST_F(EpigeneticsTest, CreateImprintSuccess) {
    nimcp_imprint_config_t config = {
        .type = IMPRINT_TYPE_POSITIVE,
        .target_region = 1,
        .strength = 0.8f,
        .duration_ms = 1000.0f,
        .trigger_time = 0
    };

    uint32_t imprint_id;
    nimcp_epigenetics_error_t err = nimcp_epigenetics_create_imprint(epi, &config, &imprint_id);
    EXPECT_EQ(err, EPIGENETICS_OK);
    EXPECT_GT(imprint_id, 0u);
}

TEST_F(EpigeneticsTest, CreateImprintNull) {
    uint32_t imprint_id;
    nimcp_epigenetics_error_t err = nimcp_epigenetics_create_imprint(epi, nullptr, &imprint_id);
    EXPECT_EQ(err, EPIGENETICS_ERR_NULL_PTR);
}

TEST_F(EpigeneticsTest, GetImprint) {
    nimcp_imprint_config_t config = {
        .type = IMPRINT_TYPE_POSITIVE,
        .target_region = 1,
        .strength = 0.8f,
        .duration_ms = 1000.0f,
        .trigger_time = 0
    };

    uint32_t imprint_id;
    nimcp_epigenetics_create_imprint(epi, &config, &imprint_id);

    float strength, remaining;
    nimcp_epigenetics_error_t err = nimcp_epigenetics_get_imprint(epi, imprint_id, &strength, &remaining);
    EXPECT_EQ(err, EPIGENETICS_OK);
    EXPECT_NEAR(strength, 0.8f, 0.01f);
}

TEST_F(EpigeneticsTest, RemoveImprint) {
    nimcp_imprint_config_t config = {
        .type = IMPRINT_TYPE_POSITIVE,
        .target_region = 1,
        .strength = 0.8f,
        .duration_ms = 1000.0f,
        .trigger_time = 0
    };

    uint32_t imprint_id;
    nimcp_epigenetics_create_imprint(epi, &config, &imprint_id);

    nimcp_epigenetics_error_t err = nimcp_epigenetics_remove_imprint(epi, imprint_id);
    EXPECT_EQ(err, EPIGENETICS_OK);
}

TEST_F(EpigeneticsTest, RemoveImprintNotFound) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_remove_imprint(epi, 999);
    EXPECT_EQ(err, EPIGENETICS_ERR_SITE_NOT_FOUND);
}

//=============================================================================
// Critical Period Tests
//=============================================================================

TEST_F(EpigeneticsTest, StartCriticalPeriod) {
    /* Must create region first */
    nimcp_chromatin_config_t config = {
        .region_id = 1,
        .start_neuron = 0,
        .end_neuron = 99,
        .initial_state = CHROMATIN_STATE_POISED,
        .transition_threshold = 0.5f
    };

    nimcp_epigenetics_configure_region(epi, &config);
    nimcp_epigenetics_error_t err = nimcp_epigenetics_start_critical_period(epi, 1, 1000.0f);
    EXPECT_EQ(err, EPIGENETICS_OK);
}

TEST_F(EpigeneticsTest, EndCriticalPeriod) {
    nimcp_chromatin_config_t config = {
        .region_id = 1,
        .start_neuron = 0,
        .end_neuron = 99,
        .initial_state = CHROMATIN_STATE_POISED,
        .transition_threshold = 0.5f
    };

    nimcp_epigenetics_configure_region(epi, &config);
    nimcp_epigenetics_start_critical_period(epi, 1, 1000.0f);
    nimcp_epigenetics_error_t err = nimcp_epigenetics_end_critical_period(epi, 1);
    EXPECT_EQ(err, EPIGENETICS_OK);
}

TEST_F(EpigeneticsTest, IsCriticalPeriod) {
    nimcp_chromatin_config_t config = {
        .region_id = 1,
        .start_neuron = 0,
        .end_neuron = 99,
        .initial_state = CHROMATIN_STATE_POISED,
        .transition_threshold = 0.5f
    };

    nimcp_epigenetics_configure_region(epi, &config);
    nimcp_epigenetics_start_critical_period(epi, 1, 1000.0f);

    bool is_critical;
    nimcp_epigenetics_error_t err = nimcp_epigenetics_is_critical_period(epi, 1, &is_critical);
    EXPECT_EQ(err, EPIGENETICS_OK);
    EXPECT_TRUE(is_critical);
}

//=============================================================================
// Environmental Factor Tests
//=============================================================================

TEST_F(EpigeneticsTest, ApplyEnvironment) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_apply_environment(epi, 0.3f, 0.7f);
    EXPECT_EQ(err, EPIGENETICS_OK);
}

TEST_F(EpigeneticsTest, ApplyEnvironmentNull) {
    nimcp_epigenetics_error_t err = nimcp_epigenetics_apply_environment(nullptr, 0.3f, 0.7f);
    EXPECT_EQ(err, EPIGENETICS_ERR_NULL_PTR);
}

TEST_F(EpigeneticsTest, GetEnvironmentEffect) {
    nimcp_epigenetics_apply_environment(epi, 0.0f, 0.5f);
    nimcp_epigenetics_update(epi, 0.1f);

    float modifier;
    nimcp_epigenetics_error_t err = nimcp_epigenetics_get_environment_effect(epi, &modifier);
    EXPECT_EQ(err, EPIGENETICS_OK);
    EXPECT_GT(modifier, 0.0f);
}

TEST_F(EpigeneticsTest, EnvironmentAffectsPlasticity) {
    /* High stress should reduce plasticity */
    nimcp_epigenetics_apply_environment(epi, 0.9f, 0.0f);
    nimcp_epigenetics_update(epi, 0.1f);

    nimcp_epigenetics_state_t state;
    nimcp_epigenetics_get_state(epi, &state);
    EXPECT_LT(state.global_plasticity, 1.0f);

    /* High enrichment should increase plasticity */
    nimcp_epigenetics_apply_environment(epi, 0.0f, 0.9f);
    nimcp_epigenetics_update(epi, 0.1f);

    nimcp_epigenetics_get_state(epi, &state);
    EXPECT_GT(state.global_plasticity, 0.5f);
}

//=============================================================================
// Error String Tests
//=============================================================================

TEST(ErrorStringTest, AllErrorsHaveStrings) {
    EXPECT_NE(nimcp_epigenetics_error_string(EPIGENETICS_OK), nullptr);
    EXPECT_NE(nimcp_epigenetics_error_string(EPIGENETICS_ERR_NULL_PTR), nullptr);
    EXPECT_NE(nimcp_epigenetics_error_string(EPIGENETICS_ERR_INVALID_PARAM), nullptr);
    EXPECT_NE(nimcp_epigenetics_error_string(EPIGENETICS_ERR_NOT_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_epigenetics_error_string(EPIGENETICS_ERR_ALREADY_INITIALIZED), nullptr);
    EXPECT_NE(nimcp_epigenetics_error_string(EPIGENETICS_ERR_NO_MEMORY), nullptr);
    EXPECT_NE(nimcp_epigenetics_error_string(EPIGENETICS_ERR_SITE_NOT_FOUND), nullptr);
    EXPECT_NE(nimcp_epigenetics_error_string(EPIGENETICS_ERR_REGION_FULL), nullptr);
    EXPECT_NE(nimcp_epigenetics_error_string(EPIGENETICS_ERR_WINDOW_CLOSED), nullptr);
}

TEST(ErrorStringTest, UnknownErrorCode) {
    const char* str = nimcp_epigenetics_error_string((nimcp_epigenetics_error_t)-999);
    EXPECT_NE(str, nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
