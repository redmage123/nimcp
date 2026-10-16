/**
 * @file test_mood_regulation.cpp
 * @brief Unit tests for mood regulation system
 */

#include <gtest/gtest.h>

extern "C" {
#include "core/brain/regions/raphe/nimcp_mood_regulation.h"
}

class MoodRegulationTest : public ::testing::Test {
protected:
    nimcp_mood_system_t system;

    void SetUp() override {
        memset(&system, 0, sizeof(system));
    }

    void TearDown() override {
        if (system.initialized) {
            nimcp_mood_shutdown(&system);
        }
    }
};

/* ==========================================================================
 * Lifecycle Tests
 * ========================================================================== */

TEST_F(MoodRegulationTest, DefaultConfigHasValidValues) {
    nimcp_mood_config_t config = nimcp_mood_default_config();

    EXPECT_FLOAT_EQ(config.time_constant, MOOD_TIME_CONSTANT);
    EXPECT_FLOAT_EQ(config.stability_baseline, MOOD_DEFAULT_STABILITY);
    EXPECT_FLOAT_EQ(config.anxiety_baseline, ANXIETY_BASELINE);
    EXPECT_GT(config.stress_sensitivity, 0.0f);
    EXPECT_GT(config.reward_sensitivity, 0.0f);
}

TEST_F(MoodRegulationTest, InitWithNullReturnsError) {
    EXPECT_EQ(nimcp_mood_init(nullptr, nullptr), -1);
}

TEST_F(MoodRegulationTest, InitWithDefaultConfigSucceeds) {
    EXPECT_EQ(nimcp_mood_init(&system, nullptr), 0);
    EXPECT_TRUE(system.initialized);
}

TEST_F(MoodRegulationTest, InitWithCustomConfigSucceeds) {
    nimcp_mood_config_t config = nimcp_mood_default_config();
    config.stress_sensitivity = 0.8f;

    EXPECT_EQ(nimcp_mood_init(&system, &config), 0);
    EXPECT_FLOAT_EQ(system.config.stress_sensitivity, 0.8f);
}

TEST_F(MoodRegulationTest, InitSetsNeutralValence) {
    nimcp_mood_init(&system, nullptr);

    EXPECT_FLOAT_EQ(system.valence, MOOD_DEFAULT_NEUTRAL);
}

TEST_F(MoodRegulationTest, ShutdownSucceeds) {
    nimcp_mood_init(&system, nullptr);
    EXPECT_EQ(nimcp_mood_shutdown(&system), 0);
    EXPECT_FALSE(system.initialized);
}

TEST_F(MoodRegulationTest, ResetRestoresInitialState) {
    nimcp_mood_init(&system, nullptr);

    system.valence = 0.8f;
    system.anxiety = 0.9f;

    EXPECT_EQ(nimcp_mood_reset(&system), 0);
    EXPECT_FLOAT_EQ(system.valence, MOOD_DEFAULT_NEUTRAL);
}

/* ==========================================================================
 * Update Tests
 * ========================================================================== */

TEST_F(MoodRegulationTest, UpdateWithNullReturnsError) {
    EXPECT_EQ(nimcp_mood_update(nullptr, 20.0f, 10.0f), -1);
}

TEST_F(MoodRegulationTest, UpdateWithoutInitReturnsError) {
    EXPECT_EQ(nimcp_mood_update(&system, 20.0f, 10.0f), -1);
}

TEST_F(MoodRegulationTest, UpdateWithBaseline5HTMaintainsNeutralMood) {
    nimcp_mood_init(&system, nullptr);

    /* Run with baseline 5-HT */
    for (int i = 0; i < 100; i++) {
        nimcp_mood_update(&system, 20.0f, 100.0f);  /* 20nM baseline */
    }

    /* Mood should stay near neutral */
    EXPECT_NEAR(system.valence, 0.0f, 0.2f);
}

TEST_F(MoodRegulationTest, UpdateWithHigh5HTImprovesMood) {
    nimcp_mood_init(&system, nullptr);

    /* Run with high 5-HT */
    for (int i = 0; i < 200; i++) {
        nimcp_mood_update(&system, 40.0f, 100.0f);  /* Double baseline */
    }

    /* Mood should be positive */
    EXPECT_GT(system.valence, 0.0f);
}

TEST_F(MoodRegulationTest, UpdateWithLow5HTWorsensMood) {
    nimcp_mood_init(&system, nullptr);

    /* Run with low 5-HT */
    for (int i = 0; i < 200; i++) {
        nimcp_mood_update(&system, 10.0f, 100.0f);  /* Half baseline */
    }

    /* Mood should be negative */
    EXPECT_LT(system.valence, 0.0f);
}

TEST_F(MoodRegulationTest, UpdateRecordsHistory) {
    nimcp_mood_init(&system, nullptr);

    EXPECT_EQ(system.history_count, 0u);

    for (int i = 0; i < 10; i++) {
        nimcp_mood_update(&system, 20.0f, 100.0f);
    }

    EXPECT_EQ(system.history_count, 10u);
}

/* ==========================================================================
 * Input API Tests
 * ========================================================================== */

TEST_F(MoodRegulationTest, ApplyStressDecreasesValence) {
    nimcp_mood_init(&system, nullptr);

    nimcp_mood_apply_stress(&system, 0.5f);

    /* Run updates */
    for (int i = 0; i < 100; i++) {
        nimcp_mood_update(&system, 20.0f, 100.0f);
    }

    EXPECT_LT(system.valence, 0.0f);
}

TEST_F(MoodRegulationTest, ApplyRewardIncreasesValence) {
    nimcp_mood_init(&system, nullptr);

    nimcp_mood_apply_reward(&system, 0.5f);

    /* Run updates */
    for (int i = 0; i < 100; i++) {
        nimcp_mood_update(&system, 20.0f, 100.0f);
    }

    EXPECT_GT(system.valence, 0.0f);
}

TEST_F(MoodRegulationTest, ApplySocialAffectsValence) {
    nimcp_mood_init(&system, nullptr);

    /* Positive social interaction */
    nimcp_mood_apply_social(&system, 0.5f);

    for (int i = 0; i < 100; i++) {
        nimcp_mood_update(&system, 20.0f, 100.0f);
    }

    EXPECT_GT(system.valence, 0.0f);
}

TEST_F(MoodRegulationTest, SetCircadianPhaseSucceeds) {
    nimcp_mood_init(&system, nullptr);

    EXPECT_EQ(nimcp_mood_set_circadian_phase(&system, 0.5f), 0);
    EXPECT_FLOAT_EQ(system.circadian_phase, 0.5f);
}

TEST_F(MoodRegulationTest, SetCircadianPhaseWraps) {
    nimcp_mood_init(&system, nullptr);

    nimcp_mood_set_circadian_phase(&system, 1.5f);
    EXPECT_FLOAT_EQ(system.circadian_phase, 0.5f);

    nimcp_mood_set_circadian_phase(&system, -0.25f);
    EXPECT_FLOAT_EQ(system.circadian_phase, 0.75f);
}

TEST_F(MoodRegulationTest, StressClampsToValidRange) {
    nimcp_mood_init(&system, nullptr);

    nimcp_mood_apply_stress(&system, 2.0f);
    EXPECT_LE(system.stress_input, 1.0f);
}

TEST_F(MoodRegulationTest, RewardClampsToValidRange) {
    nimcp_mood_init(&system, nullptr);

    nimcp_mood_apply_reward(&system, 2.0f);
    EXPECT_LE(system.reward_input, 1.0f);
}

/* ==========================================================================
 * Query API Tests
 * ========================================================================== */

TEST_F(MoodRegulationTest, GetValenceWithNullReturnsError) {
    nimcp_mood_init(&system, nullptr);

    float valence;
    EXPECT_EQ(nimcp_mood_get_valence(nullptr, &valence), -1);
    EXPECT_EQ(nimcp_mood_get_valence(&system, nullptr), -1);
}

TEST_F(MoodRegulationTest, GetValenceReturnsCurrentValue) {
    nimcp_mood_init(&system, nullptr);
    system.valence = 0.5f;

    float valence;
    EXPECT_EQ(nimcp_mood_get_valence(&system, &valence), 0);
    EXPECT_FLOAT_EQ(valence, 0.5f);
}

TEST_F(MoodRegulationTest, GetStabilitySucceeds) {
    nimcp_mood_init(&system, nullptr);

    float stability;
    EXPECT_EQ(nimcp_mood_get_stability(&system, &stability), 0);
    EXPECT_GE(stability, 0.0f);
    EXPECT_LE(stability, 1.0f);
}

TEST_F(MoodRegulationTest, GetAnxietySucceeds) {
    nimcp_mood_init(&system, nullptr);

    float anxiety;
    EXPECT_EQ(nimcp_mood_get_anxiety(&system, &anxiety), 0);
    EXPECT_GE(anxiety, 0.0f);
    EXPECT_LE(anxiety, 1.0f);
}

TEST_F(MoodRegulationTest, GetEnergySucceeds) {
    nimcp_mood_init(&system, nullptr);

    float energy;
    EXPECT_EQ(nimcp_mood_get_energy(&system, &energy), 0);
    EXPECT_GE(energy, 0.0f);
    EXPECT_LE(energy, 1.0f);
}

TEST_F(MoodRegulationTest, GetStateReturnsAllValues) {
    nimcp_mood_init(&system, nullptr);

    system.valence = 0.3f;
    system.stability = 0.6f;
    system.anxiety = 0.4f;

    nimcp_mood_snapshot_t state;
    EXPECT_EQ(nimcp_mood_get_state(&system, &state), 0);
    EXPECT_FLOAT_EQ(state.valence, 0.3f);
    EXPECT_FLOAT_EQ(state.stability, 0.6f);
    EXPECT_FLOAT_EQ(state.anxiety, 0.4f);
}

/* ==========================================================================
 * Analysis API Tests
 * ========================================================================== */

TEST_F(MoodRegulationTest, GetTrendWithNoHistoryReturnsZero) {
    nimcp_mood_init(&system, nullptr);

    float trend;
    EXPECT_EQ(nimcp_mood_get_trend(&system, &trend), 0);
    EXPECT_FLOAT_EQ(trend, 0.0f);
}

TEST_F(MoodRegulationTest, GetTrendShowsImprovingMood) {
    nimcp_mood_init(&system, nullptr);

    /* Start low, move to high */
    system.valence = -0.5f;
    for (int i = 0; i < MOOD_MAX_HISTORY; i++) {
        nimcp_mood_update(&system, 40.0f, 100.0f);  /* High 5-HT */
    }

    float trend;
    nimcp_mood_get_trend(&system, &trend);
    EXPECT_GT(trend, 0.0f);  /* Positive trend */
}

TEST_F(MoodRegulationTest, GetVariabilitySucceeds) {
    nimcp_mood_init(&system, nullptr);

    /* Create some variability */
    for (int i = 0; i < 100; i++) {
        float ht = (i % 2 == 0) ? 30.0f : 10.0f;  /* Alternate high/low */
        nimcp_mood_update(&system, ht, 100.0f);
    }

    float variability;
    EXPECT_EQ(nimcp_mood_get_variability(&system, &variability), 0);
    EXPECT_GE(variability, 0.0f);
}

TEST_F(MoodRegulationTest, IsDepressedDetectsLowMood) {
    nimcp_mood_init(&system, nullptr);

    /* Induce depression-like state */
    system.valence = -0.5f;
    system.energy = 0.2f;
    system.anxiety = 0.7f;
    system.time_depressed = 120.0f;  /* Over threshold */

    bool depressed;
    EXPECT_EQ(nimcp_mood_is_depressed(&system, &depressed), 0);
    EXPECT_TRUE(depressed);
}

TEST_F(MoodRegulationTest, IsDepressedNegativeWhenHealthy) {
    nimcp_mood_init(&system, nullptr);

    /* Healthy state */
    system.valence = 0.3f;
    system.energy = 0.6f;
    system.anxiety = 0.2f;

    bool depressed;
    nimcp_mood_is_depressed(&system, &depressed);
    EXPECT_FALSE(depressed);
}

/* ==========================================================================
 * 5-HT Effects on Secondary States
 * ========================================================================== */

TEST_F(MoodRegulationTest, High5HTIncreasesStability) {
    nimcp_mood_init(&system, nullptr);

    float initial_stability = system.stability;

    /* High 5-HT should increase stability */
    for (int i = 0; i < 200; i++) {
        nimcp_mood_update(&system, 40.0f, 100.0f);
    }

    EXPECT_GT(system.stability, initial_stability);
}

TEST_F(MoodRegulationTest, Low5HTIncreasesAnxiety) {
    nimcp_mood_init(&system, nullptr);

    float initial_anxiety = system.anxiety;

    /* Low 5-HT should increase anxiety */
    for (int i = 0; i < 200; i++) {
        nimcp_mood_update(&system, 10.0f, 100.0f);
    }

    EXPECT_GT(system.anxiety, initial_anxiety);
}

TEST_F(MoodRegulationTest, Low5HTIncreasesIrritability) {
    nimcp_mood_init(&system, nullptr);

    float initial_irritability = system.irritability;

    /* Low 5-HT should increase irritability */
    for (int i = 0; i < 200; i++) {
        nimcp_mood_update(&system, 10.0f, 100.0f);
    }

    EXPECT_GT(system.irritability, initial_irritability);
}

TEST_F(MoodRegulationTest, PositiveMoodIncreasesEnergy) {
    nimcp_mood_init(&system, nullptr);

    float initial_energy = system.energy;

    /* Positive mood should increase energy */
    for (int i = 0; i < 200; i++) {
        nimcp_mood_update(&system, 35.0f, 100.0f);
    }

    /* If mood improved, energy should follow */
    if (system.valence > 0.0f) {
        EXPECT_GT(system.energy, initial_energy);
    }
}

/* ==========================================================================
 * Input Decay Tests
 * ========================================================================== */

TEST_F(MoodRegulationTest, StressInputDecaysOverTime) {
    nimcp_mood_init(&system, nullptr);

    nimcp_mood_apply_stress(&system, 0.8f);
    float initial_stress = system.stress_input;

    for (int i = 0; i < 100; i++) {
        nimcp_mood_update(&system, 20.0f, 100.0f);
    }

    EXPECT_LT(system.stress_input, initial_stress);
}

TEST_F(MoodRegulationTest, RewardInputDecaysOverTime) {
    nimcp_mood_init(&system, nullptr);

    nimcp_mood_apply_reward(&system, 0.8f);
    float initial_reward = system.reward_input;

    for (int i = 0; i < 100; i++) {
        nimcp_mood_update(&system, 20.0f, 100.0f);
    }

    EXPECT_LT(system.reward_input, initial_reward);
}

/* ==========================================================================
 * Statistics Tests
 * ========================================================================== */

TEST_F(MoodRegulationTest, TimeDepressedTracked) {
    nimcp_mood_init(&system, nullptr);

    /* Force low valence */
    system.valence = -0.5f;

    for (int i = 0; i < 100; i++) {
        nimcp_mood_update(&system, 10.0f, 100.0f);
    }

    EXPECT_GT(system.time_depressed, 0.0f);
}

TEST_F(MoodRegulationTest, TimePositiveTracked) {
    nimcp_mood_init(&system, nullptr);

    /* Force positive valence */
    system.valence = 0.5f;

    for (int i = 0; i < 100; i++) {
        nimcp_mood_update(&system, 35.0f, 100.0f);
    }

    EXPECT_GT(system.time_positive, 0.0f);
}
