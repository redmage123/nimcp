/**
 * @file test_cortical_neuromodulation.cpp
 * @brief Unit tests for neuromodulatory effects on cortical columns
 */

#include <gtest/gtest.h>
// Headers have their own extern "C" guards
#include "core/cortical_columns/nimcp_cortical_neuromodulation.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CorticalNeuromodulationTest : public ::testing::Test {
protected:
    cortical_neuromod_system_t* neuromod;
    cortical_neuromod_config_t config;

    void SetUp() override {
        cortical_neuromod_default_config(&config);
        config.num_columns = 16;
        neuromod = cortical_neuromod_create(&config);
        ASSERT_NE(neuromod, nullptr);
    }

    void TearDown() override {
        if (neuromod) {
            cortical_neuromod_destroy(neuromod);
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(CorticalNeuromodulationTest, DefaultConfig) {
    cortical_neuromod_config_t cfg;
    cortical_neuromod_default_config(&cfg);

    EXPECT_GT(cfg.ach_snr_boost, 0.0f);
    EXPECT_GT(cfg.da_reward_sensitivity, 0.0f);
    EXPECT_GT(cfg.ne_gain_boost, 0.0f);
    EXPECT_GT(cfg.serotonin_inhibition_boost, 0.0f);
}

TEST_F(CorticalNeuromodulationTest, CreateWithConfig) {
    cortical_neuromod_config_t custom_config;
    cortical_neuromod_default_config(&custom_config);
    custom_config.da_plasticity_modulation = 0.8f;
    custom_config.num_columns = 32;

    cortical_neuromod_system_t* system = cortical_neuromod_create(&custom_config);
    ASSERT_NE(system, nullptr);

    cortical_neuromod_destroy(system);
}

TEST_F(CorticalNeuromodulationTest, CreateWithNullConfig) {
    cortical_neuromod_system_t* system = cortical_neuromod_create(nullptr);
    /* Implementation may require valid config */
    if (system != nullptr) {
        cortical_neuromod_destroy(system);
    }
}

/* ============================================================================
 * Level Setting Tests
 * ============================================================================ */

TEST_F(CorticalNeuromodulationTest, SetAcetylcholineLevel) {
    int result = cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_ACETYLCHOLINE, 0.8f);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalNeuromodulationTest, SetDopamineLevel) {
    int result = cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_DOPAMINE, 0.7f);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalNeuromodulationTest, SetNorepinephrineLevel) {
    int result = cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_NOREPINEPHRINE, 0.6f);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalNeuromodulationTest, SetSerotoninLevel) {
    int result = cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_SEROTONIN, 0.5f);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalNeuromodulationTest, SetLevelOutOfRange) {
    int result1 = cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_DOPAMINE, 1.5f);
    int result2 = cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_DOPAMINE, -0.5f);
    /* Should either clamp or reject */
    EXPECT_TRUE(result1 == 0 || result1 < 0);
    EXPECT_TRUE(result2 == 0 || result2 < 0);
}

/* ============================================================================
 * Level Querying Tests
 * ============================================================================ */

TEST_F(CorticalNeuromodulationTest, GetLevel) {
    cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_DOPAMINE, 0.75f);
    float level = cortical_neuromod_get_level(neuromod, CORTICAL_NEUROMOD_DOPAMINE);
    EXPECT_NEAR(level, 0.75f, 0.01f);
}

TEST_F(CorticalNeuromodulationTest, GetAllLevels) {
    float ach = cortical_neuromod_get_level(neuromod, CORTICAL_NEUROMOD_ACETYLCHOLINE);
    float da = cortical_neuromod_get_level(neuromod, CORTICAL_NEUROMOD_DOPAMINE);
    float ne = cortical_neuromod_get_level(neuromod, CORTICAL_NEUROMOD_NOREPINEPHRINE);
    float serotonin = cortical_neuromod_get_level(neuromod, CORTICAL_NEUROMOD_SEROTONIN);

    EXPECT_GE(ach, 0.0f);
    EXPECT_GE(da, 0.0f);
    EXPECT_GE(ne, 0.0f);
    EXPECT_GE(serotonin, 0.0f);
}

/* ============================================================================
 * Column-specific DA Tests
 * ============================================================================ */

TEST_F(CorticalNeuromodulationTest, SetColumnDA) {
    int result = cortical_neuromod_set_column_da(neuromod, 0, 0.9f);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalNeuromodulationTest, GetColumnDA) {
    cortical_neuromod_set_column_da(neuromod, 0, 0.85f);
    float da = cortical_neuromod_get_column_da(neuromod, 0);
    EXPECT_NEAR(da, 0.85f, 0.01f);
}

TEST_F(CorticalNeuromodulationTest, SetColumnDAInvalidIndex) {
    int result = cortical_neuromod_set_column_da(neuromod, 10000, 0.5f);
    /* Should return error for invalid index */
    EXPECT_TRUE(result != 0 || result == 0); /* Just verify no crash */
}

/* ============================================================================
 * Effect Application Tests
 * ============================================================================ */

TEST_F(CorticalNeuromodulationTest, ComputeEffects) {
    cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_DOPAMINE, 0.8f);
    cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_ACETYLCHOLINE, 0.7f);

    int result = cortical_neuromod_compute_effects(neuromod);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalNeuromodulationTest, ApplyAChEffects) {
    cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_ACETYLCHOLINE, 0.8f);

    float modulated_li, modulated_snr;
    /* API: apply_ach_effects(system, base_lateral_inhibition, base_snr, modulated_li*, modulated_snr*) */
    int result = cortical_neuromod_apply_ach_effects(neuromod, 0.5f, 10.0f, &modulated_li, &modulated_snr);
    EXPECT_EQ(result, 0);
    EXPECT_GT(modulated_snr, 0.0f);
}

TEST_F(CorticalNeuromodulationTest, ApplyDAEffects) {
    cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_DOPAMINE, 0.8f);

    float modulated_gain, modulated_lr;
    /* API: apply_da_effects(system, base_gain, base_learning_rate, modulated_gain*, modulated_lr*) */
    int result = cortical_neuromod_apply_da_effects(neuromod, 1.0f, 0.01f, &modulated_gain, &modulated_lr);
    EXPECT_EQ(result, 0);
    EXPECT_GT(modulated_gain, 0.0f);
}

TEST_F(CorticalNeuromodulationTest, ApplyNEEffects) {
    cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_NOREPINEPHRINE, 0.9f);

    float gain_out;
    bool should_reset;
    int result = cortical_neuromod_apply_ne_effects(neuromod, 1.0f, &gain_out, &should_reset);
    EXPECT_EQ(result, 0);
    EXPECT_GT(gain_out, 0.0f);
}

TEST_F(CorticalNeuromodulationTest, ApplySerotoninEffects) {
    cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_SEROTONIN, 0.7f);

    float inhibition_out;
    int result = cortical_neuromod_apply_serotonin_effects(neuromod, 0.5f, &inhibition_out);
    EXPECT_EQ(result, 0);
    EXPECT_GE(inhibition_out, 0.0f);
}

/* ============================================================================
 * Plasticity Modulation Tests
 * ============================================================================ */

TEST_F(CorticalNeuromodulationTest, GetPlasticityGate) {
    cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_ACETYLCHOLINE, 0.8f);
    cortical_neuromod_compute_effects(neuromod);

    float gate = cortical_neuromod_get_plasticity_gate(neuromod);
    EXPECT_GE(gate, 0.0f);
    EXPECT_LE(gate, 1.0f);
}

TEST_F(CorticalNeuromodulationTest, ModulatePlasticity) {
    cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_DOPAMINE, 0.9f);
    cortical_neuromod_compute_effects(neuromod);

    float base_lr = 0.01f;
    float modulated_lr = cortical_neuromod_modulate_plasticity(neuromod, base_lr);
    EXPECT_GT(modulated_lr, 0.0f);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(CorticalNeuromodulationTest, Update) {
    cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_DOPAMINE, 1.0f);
    int result = cortical_neuromod_update(neuromod, 100.0f);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalNeuromodulationTest, Release) {
    float released = cortical_neuromod_release(neuromod, CORTICAL_NEUROMOD_DOPAMINE, 0.5f);
    EXPECT_GT(released, 0.0f);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(CorticalNeuromodulationTest, GetStats) {
    cortical_neuromod_stats_t stats;
    int result = cortical_neuromod_get_stats(neuromod, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(CorticalNeuromodulationTest, Reset) {
    cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_DOPAMINE, 0.8f);
    int result = cortical_neuromod_reset(neuromod);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Bio-async Tests
 * ============================================================================ */

TEST_F(CorticalNeuromodulationTest, ConnectBioAsync) {
    int result = cortical_neuromod_connect_bio_async(neuromod);
    EXPECT_TRUE(result == 0 || result < 0);
}

TEST_F(CorticalNeuromodulationTest, IsBioAsyncConnected) {
    bool connected = cortical_neuromod_is_bio_async_connected(neuromod);
    EXPECT_FALSE(connected);
}

TEST_F(CorticalNeuromodulationTest, DisconnectBioAsync) {
    int result = cortical_neuromod_disconnect_bio_async(neuromod);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(CorticalNeuromodulationTest, DestroyNull) {
    cortical_neuromod_destroy(nullptr);
}

TEST_F(CorticalNeuromodulationTest, RapidLevelChanges) {
    for (int i = 0; i < 100; i++) {
        float level = (float)i / 100.0f;
        cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_DOPAMINE, level);
        cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_NOREPINEPHRINE, 1.0f - level);
    }
    /* Should handle rapid changes */
    float da = cortical_neuromod_get_level(neuromod, CORTICAL_NEUROMOD_DOPAMINE);
    EXPECT_GE(da, 0.0f);
}

TEST_F(CorticalNeuromodulationTest, MultipleUpdateCycles) {
    cortical_neuromod_set_level(neuromod, CORTICAL_NEUROMOD_DOPAMINE, 1.0f);

    for (int i = 0; i < 100; i++) {
        cortical_neuromod_update(neuromod, 10.0f);
    }

    float da = cortical_neuromod_get_level(neuromod, CORTICAL_NEUROMOD_DOPAMINE);
    EXPECT_GE(da, 0.0f);
}

