/**
 * @file test_trained_immunity.cpp
 * @brief Unit tests for Trained Immunity Module
 * @date 2025-12-12
 *
 * Tests epigenetic reprogramming, metabolic shift, PRR sensitivity,
 * and cross-protection mechanisms.
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_trained_immunity.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class TrainedImmunityTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    trained_immunity_t* trained = nullptr;
    trained_immunity_config_t config;

    void SetUp() override {
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        trained_immunity_default_config(&config);
        trained = trained_immunity_create(&config, immune_system);
        ASSERT_NE(trained, nullptr);
    }

    void TearDown() override {
        if (trained) {
            trained_immunity_destroy(trained);
            trained = nullptr;
        }
        if (immune_system) {
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(TrainedImmunityTest, DefaultConfigIsValid) {
    trained_immunity_config_t cfg;
    int result = trained_immunity_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_GT(cfg.max_training_intensity, 0.0f);
    EXPECT_TRUE(cfg.enable_cross_protection);
}

TEST_F(TrainedImmunityTest, DefaultConfigNullFails) {
    int result = trained_immunity_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(TrainedImmunityTest, CreateWithNullConfig) {
    trained_immunity_t* sys = trained_immunity_create(nullptr, immune_system);
    ASSERT_NE(sys, nullptr);
    trained_immunity_destroy(sys);
}

TEST_F(TrainedImmunityTest, CreateWithNullImmuneSystem) {
    trained_immunity_t* sys = trained_immunity_create(&config, nullptr);
    // Should still create but with no integration
    if (sys) {
        trained_immunity_destroy(sys);
    }
}

TEST_F(TrainedImmunityTest, DestroyNullSafe) {
    trained_immunity_destroy(nullptr);
    // Should not crash
}

/* ============================================================================
 * Training Tests
 * ============================================================================ */

TEST_F(TrainedImmunityTest, TrainWithBCG) {
    int result = trained_immunity_train(trained, TRAINED_STIMULUS_BCG, 0.8f);
    EXPECT_EQ(result, 0);

    float enhancement = trained_immunity_get_enhancement_factor(trained);
    EXPECT_GT(enhancement, 1.0f);  // Should be enhanced
    EXPECT_LE(enhancement, TRAINED_BCG_ENHANCEMENT_PEAK);
}

TEST_F(TrainedImmunityTest, TrainWithBetaGlucan) {
    int result = trained_immunity_train(trained, TRAINED_STIMULUS_BETA_GLUCAN, 1.0f);
    EXPECT_EQ(result, 0);

    float enhancement = trained_immunity_get_enhancement_factor(trained);
    EXPECT_GT(enhancement, 1.0f);
    EXPECT_LE(enhancement, TRAINED_BETA_GLUCAN_ENHANCEMENT);
}

TEST_F(TrainedImmunityTest, TrainWithLPS) {
    int result = trained_immunity_train(trained, TRAINED_STIMULUS_LPS_LOW_DOSE, 0.5f);
    EXPECT_EQ(result, 0);

    float enhancement = trained_immunity_get_enhancement_factor(trained);
    EXPECT_GE(enhancement, 1.0f);
}

TEST_F(TrainedImmunityTest, TrainWithOxLDL) {
    int result = trained_immunity_train(trained, TRAINED_STIMULUS_OXIDIZED_LDL, 0.7f);
    EXPECT_EQ(result, 0);

    float enhancement = trained_immunity_get_enhancement_factor(trained);
    EXPECT_GE(enhancement, 1.0f);
}

TEST_F(TrainedImmunityTest, TrainNullSystemFails) {
    int result = trained_immunity_train(nullptr, TRAINED_STIMULUS_BCG, 0.5f);
    EXPECT_EQ(result, -1);
}

TEST_F(TrainedImmunityTest, TrainZeroIntensity) {
    int result = trained_immunity_train(trained, TRAINED_STIMULUS_BCG, 0.0f);
    // Zero intensity might be rejected or accepted with minimal effect
    float enhancement = trained_immunity_get_enhancement_factor(trained);
    EXPECT_GE(enhancement, 1.0f);  // At minimum, baseline
}

TEST_F(TrainedImmunityTest, MultipleTrainingEvents) {
    trained_immunity_train(trained, TRAINED_STIMULUS_BCG, 0.5f);
    trained_immunity_train(trained, TRAINED_STIMULUS_BETA_GLUCAN, 0.5f);

    size_t count = trained_immunity_get_history_count(trained);
    EXPECT_GE(count, 2u);
}

/* ============================================================================
 * PRR Sensitivity Tests
 * ============================================================================ */

TEST_F(TrainedImmunityTest, PRRSensitivityIncreasesWithTraining) {
    float baseline = trained_immunity_get_prr_sensitivity(trained);
    EXPECT_EQ(baseline, TRAINED_PRR_BASE_SENSITIVITY);

    trained_immunity_train(trained, TRAINED_STIMULUS_BCG, 1.0f);

    float enhanced = trained_immunity_get_prr_sensitivity(trained);
    EXPECT_GT(enhanced, baseline);
}

TEST_F(TrainedImmunityTest, PRRSensitivityCapped) {
    trained_immunity_train(trained, TRAINED_STIMULUS_BCG, 1.0f);
    trained_immunity_train(trained, TRAINED_STIMULUS_BETA_GLUCAN, 1.0f);
    trained_immunity_train(trained, TRAINED_STIMULUS_LPS_LOW_DOSE, 1.0f);

    float sensitivity = trained_immunity_get_prr_sensitivity(trained);
    EXPECT_LE(sensitivity, TRAINED_PRR_MAX_SENSITIVITY);
}

/* ============================================================================
 * Metabolic State Tests
 * ============================================================================ */

TEST_F(TrainedImmunityTest, InitialMetabolicStateOxidative) {
    metabolic_state_t state = trained_immunity_get_metabolic_state(trained);
    EXPECT_EQ(state, METABOLIC_STATE_OXIDATIVE);
}

TEST_F(TrainedImmunityTest, MetabolicShiftWithTraining) {
    trained_immunity_train(trained, TRAINED_STIMULUS_BCG, 1.0f);

    metabolic_state_t state = trained_immunity_get_metabolic_state(trained);
    EXPECT_NE(state, METABOLIC_STATE_OXIDATIVE);
}

TEST_F(TrainedImmunityTest, GetMetabolicReprogramming) {
    trained_immunity_train(trained, TRAINED_STIMULUS_BCG, 1.0f);

    metabolic_reprogramming_t metabolic;
    int result = trained_immunity_get_metabolic_reprogramming(trained, &metabolic);
    EXPECT_EQ(result, 0);

    EXPECT_GT(metabolic.glycolysis_rate, 0.0f);
    EXPECT_GT(metabolic.mtor_activation, 0.0f);
}

/* ============================================================================
 * Epigenetic State Tests
 * ============================================================================ */

TEST_F(TrainedImmunityTest, GetEpigeneticState) {
    trained_immunity_train(trained, TRAINED_STIMULUS_BCG, 1.0f);

    epigenetic_state_t epigenetic;
    int result = trained_immunity_get_epigenetic_state(trained, &epigenetic);
    EXPECT_EQ(result, 0);

    EXPECT_GT(epigenetic.h3k4me3_level, 0.0f);
    EXPECT_GT(epigenetic.chromatin_openness, 0.0f);
}

TEST_F(TrainedImmunityTest, EpigeneticStateNullFails) {
    int result = trained_immunity_get_epigenetic_state(trained, nullptr);
    EXPECT_EQ(result, -1);
}

/* ============================================================================
 * Decay Tests
 * ============================================================================ */

TEST_F(TrainedImmunityTest, DecayReducesEnhancement) {
    trained_immunity_train(trained, TRAINED_STIMULUS_BETA_GLUCAN, 1.0f);
    float initial = trained_immunity_get_enhancement_factor(trained);

    // Simulate time passing (beta-glucan has short half-life)
    uint64_t current_time = trained->system_start_time + TRAINED_BETA_GLUCAN_HALF_LIFE;
    trained_immunity_decay(trained, current_time);

    float after_decay = trained_immunity_get_enhancement_factor(trained);
    EXPECT_LT(after_decay, initial);
}

TEST_F(TrainedImmunityTest, BCGDecaysSlowerThanBetaGlucan) {
    // Train two separate systems with different stimuli
    trained_immunity_t* bcg_trained = trained_immunity_create(&config, immune_system);
    trained_immunity_t* beta_trained = trained_immunity_create(&config, immune_system);

    trained_immunity_train(bcg_trained, TRAINED_STIMULUS_BCG, 1.0f);
    trained_immunity_train(beta_trained, TRAINED_STIMULUS_BETA_GLUCAN, 1.0f);

    // Apply same decay time
    uint64_t decay_time = 86400000; // 1 day
    uint64_t current = bcg_trained->system_start_time + decay_time;

    trained_immunity_decay(bcg_trained, current);
    trained_immunity_decay(beta_trained, current);

    float bcg_remaining = trained_immunity_get_enhancement_factor(bcg_trained);
    float beta_remaining = trained_immunity_get_enhancement_factor(beta_trained);

    // BCG should retain more enhancement (longer half-life)
    EXPECT_GT(bcg_remaining, beta_remaining);

    trained_immunity_destroy(bcg_trained);
    trained_immunity_destroy(beta_trained);
}

/* ============================================================================
 * Cross-Protection Tests
 * ============================================================================ */

TEST_F(TrainedImmunityTest, CrossProtectionWithSufficientTraining) {
    trained_immunity_train(trained, TRAINED_STIMULUS_BCG, 1.0f);

    brain_antigen_t antigen = {0};
    antigen.id = 1;
    antigen.severity = 5;

    bool has_protection = trained_immunity_check_cross_protection(trained, &antigen);
    EXPECT_TRUE(has_protection);
}

TEST_F(TrainedImmunityTest, NoCrossProtectionWithoutTraining) {
    brain_antigen_t antigen = {0};
    antigen.id = 1;
    antigen.severity = 5;

    bool has_protection = trained_immunity_check_cross_protection(trained, &antigen);
    EXPECT_FALSE(has_protection);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(TrainedImmunityTest, IsActiveAfterTraining) {
    EXPECT_FALSE(trained_immunity_is_active(trained));

    trained_immunity_train(trained, TRAINED_STIMULUS_BCG, 0.5f);

    EXPECT_TRUE(trained_immunity_is_active(trained));
}

TEST_F(TrainedImmunityTest, TimeSinceTraining) {
    trained_immunity_train(trained, TRAINED_STIMULUS_BCG, 0.5f);

    uint64_t current = trained->last_training_time + 1000;
    uint64_t elapsed = trained_immunity_time_since_training(trained, current);

    EXPECT_EQ(elapsed, 1000u);
}

TEST_F(TrainedImmunityTest, GetPRRState) {
    trained_immunity_train(trained, TRAINED_STIMULUS_BCG, 1.0f);

    prr_sensitivity_t prr;
    int result = trained_immunity_get_prr_state(trained, &prr);
    EXPECT_EQ(result, 0);

    EXPECT_GT(prr.tlr_expression, 1.0f);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(TrainedImmunityTest, StimulusToString) {
    EXPECT_STREQ(trained_immunity_stimulus_to_string(TRAINED_STIMULUS_BCG), "BCG");
    EXPECT_STREQ(trained_immunity_stimulus_to_string(TRAINED_STIMULUS_BETA_GLUCAN), "BETA_GLUCAN");
    EXPECT_STREQ(trained_immunity_stimulus_to_string(TRAINED_STIMULUS_NONE), "NONE");
}

TEST_F(TrainedImmunityTest, MetabolicStateToString) {
    EXPECT_STREQ(trained_immunity_metabolic_state_to_string(METABOLIC_STATE_OXIDATIVE), "OXIDATIVE");
    EXPECT_STREQ(trained_immunity_metabolic_state_to_string(METABOLIC_STATE_GLYCOLYTIC), "GLYCOLYTIC");
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(TrainedImmunityTest, TrainMaxHistory) {
    // Fill up training history
    for (int i = 0; i < TRAINED_IMMUNITY_MAX_HISTORY + 5; i++) {
        trained_immunity_train(trained, TRAINED_STIMULUS_BCG, 0.1f);
    }

    size_t count = trained_immunity_get_history_count(trained);
    EXPECT_LE(count, TRAINED_IMMUNITY_MAX_HISTORY);
}

TEST_F(TrainedImmunityTest, InvalidStimulusType) {
    int result = trained_immunity_train(trained, (training_stimulus_t)999, 0.5f);
    // Should handle gracefully
}
