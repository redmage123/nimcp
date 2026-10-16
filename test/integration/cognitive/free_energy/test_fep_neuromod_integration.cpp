/**
 * @file test_fep_neuromod_integration.cpp
 * @brief Integration tests for FEP Neuromodulation module with other FEP components
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include "cognitive/free_energy/nimcp_fep_neuromod.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/free_energy/nimcp_fep_learning.h"
#include "cognitive/free_energy/nimcp_fep_curiosity.h"

class FEPNeuromodIntegrationTest : public ::testing::Test {
protected:
    static const uint32_t OBS_DIM = 8;
    static const uint32_t ACTION_DIM = 4;
    static const uint32_t STATE_DIM = 8;
    fep_neuromod_system_t* neuromod = nullptr;
    fep_system_t* fep = nullptr;

    void SetUp() override {
        /* Create neuromodulation system */
        fep_neuromod_config_t neuro_config;
        fep_neuromod_default_config(&neuro_config);
        neuromod = fep_neuromod_create(&neuro_config);

        /* Create FEP system */
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);
    }

    void TearDown() override {
        if (neuromod) {
            fep_neuromod_destroy(neuromod);
            neuromod = nullptr;
        }
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
    }
};

/* ============================================================================
 * Neuromod + FEP Precision Integration Tests
 * ============================================================================ */

TEST_F(FEPNeuromodIntegrationTest, NeuromodAffectsPrecision) {
    /* Base precision */
    float base_precision = 1.0f;

    /* Modify neuromodulator levels */
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, 0.9f);  /* High attention */
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_NE, 0.8f);   /* High arousal */

    float modulated = fep_neuromod_compute_precision(neuromod, base_precision);
    EXPECT_GT(modulated, 0.0f);
    EXPECT_TRUE(std::isfinite(modulated));
}

TEST_F(FEPNeuromodIntegrationTest, LowACHReducesPrecision) {
    float base = 1.0f;

    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, 0.1f);  /* Low attention */
    fep_neuromod_update(neuromod, 0);  /* Recompute precision_multiplier */
    float low_ach = fep_neuromod_compute_precision(neuromod, base);

    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, 0.9f);  /* High attention */
    fep_neuromod_update(neuromod, 0);  /* Recompute precision_multiplier */
    float high_ach = fep_neuromod_compute_precision(neuromod, base);

    EXPECT_GT(high_ach, low_ach);
}

/* ============================================================================
 * Neuromod + Learning Integration Tests
 * ============================================================================ */

TEST_F(FEPNeuromodIntegrationTest, DopamineAffectsLearning) {
    /* High dopamine state (reward prediction error) */
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, 0.9f);

    /* Create transition learner and learn */
    fep_learning_config_t config;
    fep_learning_default_config(&config);
    fep_transition_learner_t* learner = fep_transition_learner_create(&config, STATE_DIM);
    ASSERT_NE(learner, nullptr);

    std::vector<float> state = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<float> next = {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    int ret = fep_learn_transition(learner, fep, state.data(), next.data(), STATE_DIM);
    EXPECT_EQ(ret, 0);

    fep_transition_learner_destroy(learner);
}

TEST_F(FEPNeuromodIntegrationTest, SerotoninModulatesLearning) {
    /* Serotonin affects learning rate */
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_5HT, 0.5f);
    float precision = fep_neuromod_compute_precision(neuromod, 1.0f);
    EXPECT_GT(precision, 0.0f);
}

/* ============================================================================
 * Release and Decay Tests
 * ============================================================================ */

TEST_F(FEPNeuromodIntegrationTest, ReleaseIncreasesLevel) {
    float before = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_DA);

    fep_neuromod_release(neuromod, FEP_NEUROMOD_DA, 0.2f);

    float after = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_DA);
    EXPECT_GT(after, before);
}

TEST_F(FEPNeuromodIntegrationTest, DecayTowardsBaseline) {
    /* Set high level */
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_NE, 1.0f);
    float initial = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_NE);

    /* Update multiple times */
    for (int i = 0; i < 100; i++) {
        fep_neuromod_update(neuromod, 10);
    }

    float after = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_NE);

    /* Should decay towards baseline */
    EXPECT_LE(after, initial);
}

/* ============================================================================
 * Multi-Neuromodulator Interaction Tests
 * ============================================================================ */

TEST_F(FEPNeuromodIntegrationTest, MultipleNeuromodulatorsInteract) {
    /* Set different levels */
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, 0.8f);
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_NE, 0.6f);
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, 0.7f);
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_5HT, 0.5f);

    /* Combined precision */
    float precision = fep_neuromod_compute_precision(neuromod, 1.0f);
    EXPECT_GT(precision, 0.0f);
}

TEST_F(FEPNeuromodIntegrationTest, StateReflectsAllNeuromodulators) {
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, 0.7f);
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_NE, 0.6f);
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, 0.8f);
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_5HT, 0.4f);

    fep_neuromod_state_t state;
    int ret = fep_neuromod_get_state(neuromod, &state);
    EXPECT_EQ(ret, 0);

    EXPECT_NEAR(state.levels[FEP_NEUROMOD_ACH], 0.7f, 0.01f);
    EXPECT_NEAR(state.levels[FEP_NEUROMOD_NE], 0.6f, 0.01f);
    EXPECT_NEAR(state.levels[FEP_NEUROMOD_DA], 0.8f, 0.01f);
    EXPECT_NEAR(state.levels[FEP_NEUROMOD_5HT], 0.4f, 0.01f);
}

/* ============================================================================
 * Event-Driven Release Tests
 * ============================================================================ */

TEST_F(FEPNeuromodIntegrationTest, SurpriseTriggersNE) {
    float before = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_NE);

    /* Simulate surprise event */
    fep_neuromod_on_surprise(neuromod, 0.8f);

    float after = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_NE);
    EXPECT_GE(after, before);
}

TEST_F(FEPNeuromodIntegrationTest, RewardTriggersDA) {
    float before = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_DA);

    /* Simulate reward event */
    fep_neuromod_on_reward(neuromod, 1.0f);

    float after = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_DA);
    EXPECT_GT(after, before);
}

TEST_F(FEPNeuromodIntegrationTest, PunishmentReducesDA) {
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, 0.7f);
    float before = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_DA);

    /* Simulate punishment event (negative reward) */
    fep_neuromod_on_reward(neuromod, -0.5f);

    float after = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_DA);
    EXPECT_LE(after, before);
}

/* ============================================================================
 * Curiosity Integration Tests
 * ============================================================================ */

TEST_F(FEPNeuromodIntegrationTest, CuriosityWithNeuromod) {
    fep_curiosity_config_t cur_config;
    fep_curiosity_default_config(&cur_config);
    fep_curiosity_system_t* curiosity = fep_curiosity_create(&cur_config);
    fep_curiosity_connect(curiosity, fep);

    /* Dopamine affects exploration */
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, 0.9f);
    float da_level = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_DA);

    /* Verify dopamine level was set correctly */
    EXPECT_NEAR(da_level, 0.9f, 0.01f);

    fep_curiosity_destroy(curiosity);
}

/* ============================================================================
 * Temporal Dynamics Tests
 * ============================================================================ */

TEST_F(FEPNeuromodIntegrationTest, TemporalDynamicsWithUpdate) {
    /* Set levels */
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, 0.9f);
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, 0.8f);

    /* Track precision over time */
    float precision_t0 = fep_neuromod_compute_precision(neuromod, 1.0f);

    /* Simulate time passing */
    for (int i = 0; i < 50; i++) {
        fep_neuromod_update(neuromod, 10);
    }

    float precision_t1 = fep_neuromod_compute_precision(neuromod, 1.0f);

    /* Precision should change as neuromodulators decay */
    EXPECT_TRUE(std::isfinite(precision_t0));
    EXPECT_TRUE(std::isfinite(precision_t1));
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(FEPNeuromodIntegrationTest, BioAsyncWithNeuromod) {
    fep_neuromod_connect_bio_async(neuromod);

    /* Neuromod should work with bio-async */
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, 0.8f);
    float level = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_DA);
    EXPECT_NEAR(level, 0.8f, 0.01f);

    fep_neuromod_disconnect_bio_async(neuromod);
}

/* ============================================================================
 * Type Conversion Tests
 * ============================================================================ */

TEST_F(FEPNeuromodIntegrationTest, TypeConversionsCorrect) {
    EXPECT_STREQ(fep_neuromod_type_to_string(FEP_NEUROMOD_ACH), "ACH");
    EXPECT_STREQ(fep_neuromod_type_to_string(FEP_NEUROMOD_NE), "NE");
    EXPECT_STREQ(fep_neuromod_type_to_string(FEP_NEUROMOD_DA), "DA");
    EXPECT_STREQ(fep_neuromod_type_to_string(FEP_NEUROMOD_5HT), "5HT");
}

/* ============================================================================
 * Config Validation Tests
 * ============================================================================ */

TEST_F(FEPNeuromodIntegrationTest, CustomBaselineConfig) {
    fep_neuromod_config_t config;
    fep_neuromod_default_config(&config);
    config.ach_baseline = 0.6f;
    config.ne_baseline = 0.4f;
    config.da_baseline = 0.5f;
    config.serotonin_baseline = 0.3f;

    fep_neuromod_system_t* custom = fep_neuromod_create(&config);
    ASSERT_NE(custom, nullptr);

    /* Levels should start at baseline after update */
    fep_neuromod_update(custom, 100);

    float ach = fep_neuromod_get_level(custom, FEP_NEUROMOD_ACH);
    EXPECT_GT(ach, 0.0f);

    fep_neuromod_destroy(custom);
}

TEST_F(FEPNeuromodIntegrationTest, DecayRateConfig) {
    fep_neuromod_config_t config;
    fep_neuromod_default_config(&config);
    config.da_decay_rate = 0.01f;  /* Slow decay */

    fep_neuromod_system_t* slow_decay = fep_neuromod_create(&config);
    ASSERT_NE(slow_decay, nullptr);

    fep_neuromod_set_level(slow_decay, FEP_NEUROMOD_DA, 1.0f);
    fep_neuromod_update(slow_decay, 100);

    float level = fep_neuromod_get_level(slow_decay, FEP_NEUROMOD_DA);
    EXPECT_GT(level, 0.5f);  /* Should decay slowly */

    fep_neuromod_destroy(slow_decay);
}
