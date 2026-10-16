/**
 * @file test_fep_neuromod.cpp
 * @brief Unit tests for FEP Neuromodulation module
 * @date 2025-12-12
 *
 * WHAT: Tests for neuromodulator precision weighting in FEP
 * WHY:  Verify biologically-realistic precision control via neuromodulators
 * HOW:  Test ACh, NE, DA, 5-HT dynamics and their effects on precision
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "cognitive/free_energy/nimcp_fep_neuromod.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class FEPNeuromodTest : public ::testing::Test {
protected:
    fep_neuromod_system_t* neuromod = nullptr;
    fep_neuromod_config_t config;
    fep_system_t* fep = nullptr;
    fep_config_t fep_config;

    static constexpr uint32_t OBS_DIM = 16;
    static constexpr uint32_t ACTION_DIM = 4;

    void SetUp() override {
        fep_neuromod_default_config(&config);
        fep_default_config(&fep_config);
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

    void createNeuromod() {
        neuromod = fep_neuromod_create(&config);
        ASSERT_NE(neuromod, nullptr);
    }

    void createFEP() {
        fep = fep_create(&fep_config, OBS_DIM, ACTION_DIM);
        ASSERT_NE(fep, nullptr);
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(FEPNeuromodTest, CreateDestroy) {
    // WHAT: Create and destroy neuromod system
    createNeuromod();
    ASSERT_NE(neuromod, nullptr);
}

TEST_F(FEPNeuromodTest, CreateWithNullConfig) {
    // WHAT: Create with NULL should use defaults
    neuromod = fep_neuromod_create(nullptr);
    ASSERT_NE(neuromod, nullptr);
}

TEST_F(FEPNeuromodTest, DestroyNullSafe) {
    // WHAT: Destroying NULL should not crash
    fep_neuromod_destroy(nullptr);
}

TEST_F(FEPNeuromodTest, DefaultConfig) {
    // WHAT: Verify default config has sensible values
    fep_neuromod_config_t cfg;
    int ret = fep_neuromod_default_config(&cfg);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(cfg.ach_baseline, 0.0f);
    EXPECT_LE(cfg.ach_baseline, 1.0f);
    EXPECT_GT(cfg.ne_baseline, 0.0f);
    EXPECT_GT(cfg.da_baseline, 0.0f);
    EXPECT_GT(cfg.serotonin_baseline, 0.0f);
}

TEST_F(FEPNeuromodTest, DefaultConfigNullFails) {
    // WHAT: Default config with NULL should fail
    int ret = fep_neuromod_default_config(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Level Query Tests
 * ============================================================================ */

TEST_F(FEPNeuromodTest, GetLevelACh) {
    // WHAT: Get ACh level
    createNeuromod();

    float level = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_ACH);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(FEPNeuromodTest, GetLevelNE) {
    // WHAT: Get NE level
    createNeuromod();

    float level = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_NE);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(FEPNeuromodTest, GetLevelDA) {
    // WHAT: Get DA level
    createNeuromod();

    float level = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_DA);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(FEPNeuromodTest, GetLevel5HT) {
    // WHAT: Get 5-HT level
    createNeuromod();

    float level = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_5HT);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(FEPNeuromodTest, GetLevelNullReturnsNegative) {
    // WHAT: Get level from NULL should return -1
    float level = fep_neuromod_get_level(nullptr, FEP_NEUROMOD_ACH);
    EXPECT_LT(level, 0.0f);
}

/* ============================================================================
 * Level Setting Tests
 * ============================================================================ */

TEST_F(FEPNeuromodTest, SetLevel) {
    // WHAT: Set neuromodulator level
    createNeuromod();

    int ret = fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, 0.8f);
    EXPECT_EQ(ret, 0);

    float level = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_DA);
    EXPECT_NEAR(level, 0.8f, 0.01f);
}

TEST_F(FEPNeuromodTest, SetLevelClamps) {
    // WHAT: Setting level outside [0,1] should clamp
    createNeuromod();

    // Set above 1.0
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, 2.0f);
    EXPECT_LE(fep_neuromod_get_level(neuromod, FEP_NEUROMOD_ACH), 1.0f);

    // Set below 0.0
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, -1.0f);
    EXPECT_GE(fep_neuromod_get_level(neuromod, FEP_NEUROMOD_ACH), 0.0f);
}

TEST_F(FEPNeuromodTest, SetLevelNullFails) {
    // WHAT: Set level on NULL should fail
    int ret = fep_neuromod_set_level(nullptr, FEP_NEUROMOD_DA, 0.5f);
    EXPECT_NE(ret, 0);
}

TEST_F(FEPNeuromodTest, SetLevelAllTypes) {
    // WHAT: Set all neuromodulator types
    createNeuromod();

    for (int type = FEP_NEUROMOD_ACH; type < FEP_NEUROMOD_COUNT; type++) {
        float target = 0.3f + 0.1f * type;
        int ret = fep_neuromod_set_level(neuromod, (fep_neuromod_type_t)type, target);
        EXPECT_EQ(ret, 0);
        float actual = fep_neuromod_get_level(neuromod, (fep_neuromod_type_t)type);
        EXPECT_NEAR(actual, target, 0.01f);
    }
}

/* ============================================================================
 * Release Tests
 * ============================================================================ */

TEST_F(FEPNeuromodTest, Release) {
    // WHAT: Release neuromodulator increases level
    createNeuromod();

    float before = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_NE);
    int ret = fep_neuromod_release(neuromod, FEP_NEUROMOD_NE, 0.1f);
    EXPECT_EQ(ret, 0);

    float after = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_NE);
    EXPECT_GT(after, before);
}

TEST_F(FEPNeuromodTest, ReleaseNullFails) {
    // WHAT: Release on NULL should fail
    int ret = fep_neuromod_release(nullptr, FEP_NEUROMOD_NE, 0.1f);
    EXPECT_NE(ret, 0);
}

TEST_F(FEPNeuromodTest, ReleaseClamps) {
    // WHAT: Release should not exceed maximum
    createNeuromod();

    // Set to high level
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, 0.95f);

    // Release more
    fep_neuromod_release(neuromod, FEP_NEUROMOD_ACH, 0.5f);

    // Should be clamped to max
    float level = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_ACH);
    EXPECT_LE(level, 1.0f);
}

TEST_F(FEPNeuromodTest, ReleaseNegativeIgnored) {
    // WHAT: Negative release amount should be handled
    createNeuromod();

    float before = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_DA);
    fep_neuromod_release(neuromod, FEP_NEUROMOD_DA, -0.1f);
    float after = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_DA);

    // Should not increase (might decrease or stay same)
    EXPECT_LE(after, before + 0.01f);
}

/* ============================================================================
 * Update/Decay Tests
 * ============================================================================ */

TEST_F(FEPNeuromodTest, Update) {
    // WHAT: Update applies decay
    createNeuromod();

    int ret = fep_neuromod_update(neuromod, 100);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPNeuromodTest, UpdateNullFails) {
    // WHAT: Update on NULL should fail
    int ret = fep_neuromod_update(nullptr, 100);
    EXPECT_NE(ret, 0);
}

TEST_F(FEPNeuromodTest, UpdateCausesDecay) {
    // WHAT: Update should cause levels to decay toward baseline
    createNeuromod();

    // Set high level
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_NE, 0.9f);
    float before = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_NE);

    // Update for some time
    fep_neuromod_update(neuromod, 1000);

    float after = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_NE);

    // Level should have decayed
    EXPECT_LT(after, before);
}

TEST_F(FEPNeuromodTest, UpdateZeroDeltaNoChange) {
    // WHAT: Update with zero delta should not change levels significantly
    createNeuromod();

    float before = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_ACH);
    fep_neuromod_update(neuromod, 0);
    float after = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_ACH);

    EXPECT_NEAR(after, before, 0.01f);
}

/* ============================================================================
 * Precision Computation Tests
 * ============================================================================ */

TEST_F(FEPNeuromodTest, ComputePrecision) {
    // WHAT: Compute precision from neuromod state
    createNeuromod();

    float modulated = fep_neuromod_compute_precision(neuromod, 1.0f);
    EXPECT_GT(modulated, 0.0f);
}

TEST_F(FEPNeuromodTest, ComputePrecisionNullReturnsBase) {
    // WHAT: Null neuromod should return base precision
    float modulated = fep_neuromod_compute_precision(nullptr, 1.0f);
    EXPECT_NEAR(modulated, 1.0f, 0.1f);
}

TEST_F(FEPNeuromodTest, PrecisionIncreasesWithACh) {
    // WHAT: Higher ACh should increase precision
    createNeuromod();

    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, 0.2f);
    float low_ach = fep_neuromod_compute_precision(neuromod, 1.0f);

    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, 0.8f);
    float high_ach = fep_neuromod_compute_precision(neuromod, 1.0f);

    EXPECT_GT(high_ach, low_ach);
}

TEST_F(FEPNeuromodTest, PrecisionIncreasesWithNE) {
    // WHAT: Higher NE should increase precision (salience)
    createNeuromod();

    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_NE, 0.1f);
    float low_ne = fep_neuromod_compute_precision(neuromod, 1.0f);

    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_NE, 0.9f);
    float high_ne = fep_neuromod_compute_precision(neuromod, 1.0f);

    EXPECT_GT(high_ne, low_ne);
}

/* ============================================================================
 * Event Handler Tests
 * ============================================================================ */

TEST_F(FEPNeuromodTest, OnPredictionError) {
    // WHAT: Prediction error affects ACh
    createNeuromod();

    int ret = fep_neuromod_on_prediction_error(neuromod, 0.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPNeuromodTest, OnPredictionErrorNullFails) {
    int ret = fep_neuromod_on_prediction_error(nullptr, 0.5f);
    EXPECT_NE(ret, 0);
}

TEST_F(FEPNeuromodTest, OnSurprise) {
    // WHAT: Surprise increases NE
    createNeuromod();

    float before = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_NE);
    fep_neuromod_on_surprise(neuromod, 0.8f);
    float after = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_NE);

    EXPECT_GE(after, before);
}

TEST_F(FEPNeuromodTest, OnSurpriseNullFails) {
    int ret = fep_neuromod_on_surprise(nullptr, 0.8f);
    EXPECT_NE(ret, 0);
}

TEST_F(FEPNeuromodTest, OnReward) {
    // WHAT: Reward affects DA
    createNeuromod();

    int ret = fep_neuromod_on_reward(neuromod, 1.0f);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPNeuromodTest, OnRewardNullFails) {
    int ret = fep_neuromod_on_reward(nullptr, 1.0f);
    EXPECT_NE(ret, 0);
}

TEST_F(FEPNeuromodTest, OnUncertainty) {
    // WHAT: Uncertainty affects ACh
    createNeuromod();

    int ret = fep_neuromod_on_uncertainty(neuromod, 0.5f);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPNeuromodTest, OnUncertaintyNullFails) {
    int ret = fep_neuromod_on_uncertainty(nullptr, 0.5f);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * State Query Tests
 * ============================================================================ */

TEST_F(FEPNeuromodTest, GetState) {
    // WHAT: Get complete state
    createNeuromod();

    fep_neuromod_state_t state;
    int ret = fep_neuromod_get_state(neuromod, &state);

    EXPECT_EQ(ret, 0);
    EXPECT_GE(state.precision_multiplier, 0.0f);
}

TEST_F(FEPNeuromodTest, GetStateNullChecks) {
    // WHAT: Null checks for get state
    createNeuromod();

    fep_neuromod_state_t state;
    EXPECT_NE(fep_neuromod_get_state(nullptr, &state), 0);
    EXPECT_NE(fep_neuromod_get_state(neuromod, nullptr), 0);
}

TEST_F(FEPNeuromodTest, StateReflectsLevels) {
    // WHAT: State should reflect set levels
    createNeuromod();

    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, 0.7f);
    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_DA, 0.6f);

    fep_neuromod_state_t state;
    fep_neuromod_get_state(neuromod, &state);

    EXPECT_NEAR(state.levels[FEP_NEUROMOD_ACH], 0.7f, 0.01f);
    EXPECT_NEAR(state.levels[FEP_NEUROMOD_DA], 0.6f, 0.01f);
}

/* ============================================================================
 * FEP Integration Tests
 * ============================================================================ */

TEST_F(FEPNeuromodTest, ConnectToFEP) {
    // WHAT: Connect neuromod to FEP system
    createNeuromod();
    createFEP();

    int ret = fep_neuromod_connect(neuromod, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPNeuromodTest, ConnectNullChecks) {
    // WHAT: Null checks for connect
    createNeuromod();
    createFEP();

    EXPECT_NE(fep_neuromod_connect(nullptr, fep), 0);
    EXPECT_NE(fep_neuromod_connect(neuromod, nullptr), 0);
}

TEST_F(FEPNeuromodTest, ApplyToFEP) {
    // WHAT: Apply neuromod state to FEP
    createNeuromod();
    createFEP();

    fep_neuromod_connect(neuromod, fep);
    int ret = fep_neuromod_apply_to_fep(neuromod, fep);
    EXPECT_EQ(ret, 0);
}

TEST_F(FEPNeuromodTest, ApplyToFEPNullChecks) {
    // WHAT: Null checks for apply
    createNeuromod();
    createFEP();

    EXPECT_NE(fep_neuromod_apply_to_fep(nullptr, fep), 0);
    EXPECT_NE(fep_neuromod_apply_to_fep(neuromod, nullptr), 0);
}

/* ============================================================================
 * Bio-Async Tests
 * ============================================================================ */

TEST_F(FEPNeuromodTest, BioAsyncConnectDisconnect) {
    // WHAT: Connect and disconnect bio-async
    createNeuromod();

    EXPECT_FALSE(fep_neuromod_is_bio_async_connected(neuromod));

    int ret = fep_neuromod_connect_bio_async(neuromod);
    EXPECT_EQ(ret, 0);

    ret = fep_neuromod_disconnect_bio_async(neuromod);
    EXPECT_EQ(ret, 0);

    EXPECT_FALSE(fep_neuromod_is_bio_async_connected(neuromod));
}

TEST_F(FEPNeuromodTest, BioAsyncNullChecks) {
    // WHAT: Null checks for bio-async
    EXPECT_NE(fep_neuromod_connect_bio_async(nullptr), 0);
    EXPECT_NE(fep_neuromod_disconnect_bio_async(nullptr), 0);
    EXPECT_FALSE(fep_neuromod_is_bio_async_connected(nullptr));
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(FEPNeuromodTest, TypeToString) {
    // WHAT: Convert neuromod types to strings
    EXPECT_STREQ(fep_neuromod_type_to_string(FEP_NEUROMOD_ACH), "ACH");
    EXPECT_STREQ(fep_neuromod_type_to_string(FEP_NEUROMOD_NE), "NE");
    EXPECT_STREQ(fep_neuromod_type_to_string(FEP_NEUROMOD_DA), "DA");
    EXPECT_STREQ(fep_neuromod_type_to_string(FEP_NEUROMOD_5HT), "5HT");
}

TEST_F(FEPNeuromodTest, TypeToStringInvalid) {
    // WHAT: Invalid type should return "Unknown"
    const char* str = fep_neuromod_type_to_string((fep_neuromod_type_t)99);
    EXPECT_NE(str, nullptr);
}

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(FEPNeuromodTest, CustomBaselines) {
    // WHAT: Custom baseline configuration
    config.ach_baseline = 0.7f;
    config.ne_baseline = 0.3f;
    createNeuromod();

    // Levels should start near baselines
    float ach = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_ACH);
    float ne = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_NE);

    EXPECT_NEAR(ach, 0.7f, 0.2f);
    EXPECT_NEAR(ne, 0.3f, 0.2f);
}

TEST_F(FEPNeuromodTest, CustomDecayRates) {
    // WHAT: Custom decay rate configuration
    config.ach_decay_rate = 0.9f;  // Fast decay
    createNeuromod();

    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, 0.9f);

    // Fast decay should reduce level quickly
    fep_neuromod_update(neuromod, 500);

    float level = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_ACH);
    EXPECT_LT(level, 0.9f);
}

TEST_F(FEPNeuromodTest, CustomPrecisionGains) {
    // WHAT: Custom precision gain configuration
    config.precision_gain_ach = 2.0f;  // Strong effect
    createNeuromod();

    fep_neuromod_set_level(neuromod, FEP_NEUROMOD_ACH, 0.8f);
    float precision = fep_neuromod_compute_precision(neuromod, 1.0f);

    // High gain should result in high precision
    EXPECT_GT(precision, 1.5f);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(FEPNeuromodTest, MultipleUpdates) {
    // WHAT: Multiple sequential updates
    createNeuromod();

    for (int i = 0; i < 100; i++) {
        int ret = fep_neuromod_update(neuromod, 10);
        EXPECT_EQ(ret, 0);
    }

    // All levels should still be valid
    for (int type = FEP_NEUROMOD_ACH; type < FEP_NEUROMOD_COUNT; type++) {
        float level = fep_neuromod_get_level(neuromod, (fep_neuromod_type_t)type);
        EXPECT_GE(level, 0.0f);
        EXPECT_LE(level, 1.0f);
    }
}

TEST_F(FEPNeuromodTest, RapidReleaseAndDecay) {
    // WHAT: Rapid release followed by decay
    createNeuromod();

    // Rapid release
    for (int i = 0; i < 10; i++) {
        fep_neuromod_release(neuromod, FEP_NEUROMOD_NE, 0.1f);
    }

    // Rapid decay
    for (int i = 0; i < 10; i++) {
        fep_neuromod_update(neuromod, 100);
    }

    // Level should be valid
    float level = fep_neuromod_get_level(neuromod, FEP_NEUROMOD_NE);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

TEST_F(FEPNeuromodTest, AllEventsCombined) {
    // WHAT: Multiple event types in sequence
    createNeuromod();

    fep_neuromod_on_prediction_error(neuromod, 0.5f);
    fep_neuromod_on_surprise(neuromod, 0.7f);
    fep_neuromod_on_reward(neuromod, 0.9f);
    fep_neuromod_on_uncertainty(neuromod, 0.3f);
    fep_neuromod_update(neuromod, 50);

    // All levels should still be valid
    fep_neuromod_state_t state;
    fep_neuromod_get_state(neuromod, &state);

    for (int i = 0; i < FEP_NEUROMOD_COUNT; i++) {
        EXPECT_GE(state.levels[i], 0.0f);
        EXPECT_LE(state.levels[i], 1.0f);
    }
}
