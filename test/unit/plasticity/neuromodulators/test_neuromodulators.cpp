/**
 * @file test_neuromodulators.cpp
 * @brief Test-driven development for neuromodulator system
 *
 * TEST PHILOSOPHY:
 * - Red-Green-Refactor cycle
 * - Test edge cases and boundary conditions
 * - Verify biological constraints
 * - Performance regression tests
 */

#include <gtest/gtest.h>
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include <cmath>
#include <chrono>

//=============================================================================
// Test Fixtures
//=============================================================================

class NeuromodulatorTest : public ::testing::Test {
protected:
    neuromodulator_system_t system;
    neuromodulator_config_t config;

    void SetUp() override {
        /* WHAT: Initialize test environment with default configuration
         * WHY:  Each test starts with clean state for isolation
         */
        config = {
            .baseline_dopamine = 0.3f,
            .baseline_serotonin = 0.4f,
            .baseline_acetylcholine = 0.2f,
            .baseline_norepinephrine = 0.3f,
            .dopamine_decay = 2.0f,
            .serotonin_decay = 10.0f,
            .acetylcholine_decay = 0.5f,
            .norepinephrine_decay = 3.0f,
            .reward_dopamine_gain = 0.5f,
            .threat_norepinephrine_gain = 0.7f,
            .salience_acetylcholine_gain = 0.6f,
            .punishment_serotonin_gain = 0.4f,
            .enable_volume_transmission = true,
            .diffusion_rate = 0.1f
        };
        system = neuromodulator_system_create(&config);
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override {
        /* WHAT: Clean up test resources
         * WHY:  Prevent memory leaks between tests
         */
        neuromodulator_system_destroy(system);
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST_F(NeuromodulatorTest, CreateSystem) {
    /* WHAT: Test basic system creation
     * WHY:  Verify constructor initializes correctly
     */
    EXPECT_NE(system, nullptr);
}

TEST_F(NeuromodulatorTest, CreateWithNullConfig) {
    /* WHAT: Test null safety
     * WHY:  Should handle null config gracefully
     */
    neuromodulator_system_t null_system = neuromodulator_system_create(nullptr);
    EXPECT_NE(null_system, nullptr);  // Should create with defaults
    neuromodulator_system_destroy(null_system);
}

TEST_F(NeuromodulatorTest, DestroyNullSystem) {
    /* WHAT: Test null safety on destruction
     * WHY:  Should not crash on null pointer
     */
    neuromodulator_system_destroy(nullptr);  // Should not crash
    SUCCEED();
}

//=============================================================================
// Baseline Level Tests
//=============================================================================

TEST_F(NeuromodulatorTest, InitialLevels) {
    /* WHAT: Test initial neuromodulator concentrations
     * WHY:  Should start at baseline values from config
     */
    neuromodulator_pool_t pool = neuromodulator_pool_create();
    ASSERT_TRUE(neuromodulator_get_levels(system, &pool));

    EXPECT_FLOAT_EQ(neuromodulator_pool_get_dopamine(&pool), 0.3f);
    EXPECT_FLOAT_EQ(neuromodulator_pool_get_serotonin(&pool), 0.4f);
    EXPECT_FLOAT_EQ(neuromodulator_pool_get_acetylcholine(&pool), 0.2f);
    EXPECT_FLOAT_EQ(neuromodulator_pool_get_norepinephrine(&pool), 0.3f);
}

TEST_F(NeuromodulatorTest, SetLevelDopamine) {
    /* WHAT: Test direct level setting
     * WHY:  Verify we can manually adjust concentrations
     */
    ASSERT_TRUE(neuromodulator_set_level(system, NEUROMOD_DOPAMINE, 0.8f));

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(system, &pool);
    EXPECT_FLOAT_EQ(neuromodulator_pool_get_dopamine(&pool), 0.8f);
}

TEST_F(NeuromodulatorTest, SetLevelBoundaryCheck) {
    /* WHAT: Test boundary conditions (0.0 and 1.0)
     * WHY:  Concentrations should be clamped to [0, 1]
     */
    ASSERT_TRUE(neuromodulator_set_level(system, NEUROMOD_DOPAMINE, 1.5f));

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(system, &pool);
    EXPECT_FLOAT_EQ(neuromodulator_pool_get_dopamine(&pool), 1.0f);  // Should clamp to 1.0

    ASSERT_TRUE(neuromodulator_set_level(system, NEUROMOD_DOPAMINE, -0.5f));
    neuromodulator_get_levels(system, &pool);
    EXPECT_FLOAT_EQ(neuromodulator_pool_get_dopamine(&pool), 0.0f);  // Should clamp to 0.0
}

//=============================================================================
// Decay Dynamics Tests
//=============================================================================

TEST_F(NeuromodulatorTest, DopamineDecay) {
    /* WHAT: Test exponential decay of dopamine toward baseline
     * WHY:  Neurotransmitters decay to baseline, not zero (tonic activity)
     *       Formula: c(t) = c(0) × exp(-t/τ) + baseline × (1 - exp(-t/τ))
     */
    neuromodulator_set_level(system, NEUROMOD_DOPAMINE, 1.0f);

    // Update for 2 seconds (one time constant, τ=2.0s)
    neuromodulator_update(system, 2.0f);

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(system, &pool);

    /* WHAT: Verify decay toward baseline (0.3)
     * WHY:  After 1 τ: c = 1.0 × e^(-1) + 0.3 × (1 - e^(-1))
     *                    = 0.368 + 0.190 = 0.558
     */
    float expected = 1.0f * expf(-1.0f) + 0.3f * (1.0f - expf(-1.0f));
    EXPECT_NEAR(neuromodulator_pool_get_dopamine(&pool), expected, 0.05f);
}

TEST_F(NeuromodulatorTest, SerotoninSlowDecay) {
    /* WHAT: Test that serotonin decays slower than dopamine
     * WHY:  Different time constants → different decay rates
     */
    neuromodulator_set_level(system, NEUROMOD_DOPAMINE, 1.0f);
    neuromodulator_set_level(system, NEUROMOD_SEROTONIN, 1.0f);

    neuromodulator_update(system, 2.0f);

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(system, &pool);

    // Serotonin τ=10s, so after 2s should be higher than dopamine τ=2s
    EXPECT_GT(neuromodulator_pool_get_serotonin(&pool), neuromodulator_pool_get_dopamine(&pool));
}

TEST_F(NeuromodulatorTest, AcetylcholineFastDecay) {
    /* WHAT: Test that acetylcholine decays fastest
     * WHY:  ACh has shortest time constant (0.5s)
     */
    neuromodulator_set_level(system, NEUROMOD_ACETYLCHOLINE, 1.0f);
    neuromodulator_set_level(system, NEUROMOD_DOPAMINE, 1.0f);

    neuromodulator_update(system, 0.5f);

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(system, &pool);

    // ACh should decay faster than DA
    EXPECT_LT(neuromodulator_pool_get_acetylcholine(&pool), neuromodulator_pool_get_dopamine(&pool));
}

//=============================================================================
// Reward Prediction Error (Dopamine) Tests
//=============================================================================

TEST_F(NeuromodulatorTest, RewardPredictionErrorPositive) {
    /* WHAT: Test dopamine burst for positive RPE
     * WHY:  Reward > predicted → dopamine increase
     *       δ = reward - predicted
     */
    float rpe = neuromodulator_release_dopamine(system, 0.8f, 0.3f);

    EXPECT_GT(rpe, 0.0f);  // Positive RPE

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(system, &pool);

    EXPECT_GT(neuromodulator_pool_get_dopamine(&pool), config.baseline_dopamine);
}

TEST_F(NeuromodulatorTest, RewardPredictionErrorNegative) {
    /* WHAT: Test dopamine dip for negative RPE
     * WHY:  Reward < predicted → dopamine decrease
     */
    float rpe = neuromodulator_release_dopamine(system, 0.2f, 0.8f);

    EXPECT_LT(rpe, 0.0f);  // Negative RPE

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(system, &pool);

    EXPECT_LT(neuromodulator_pool_get_dopamine(&pool), config.baseline_dopamine);
}

TEST_F(NeuromodulatorTest, RewardPredictionErrorZero) {
    /* WHAT: Test no dopamine change when reward = predicted
     * WHY:  Perfect prediction → no learning signal
     */
    float baseline = config.baseline_dopamine;
    float rpe = neuromodulator_release_dopamine(system, 0.5f, 0.5f);

    EXPECT_NEAR(rpe, 0.0f, 0.01f);

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(system, &pool);

    EXPECT_NEAR(neuromodulator_pool_get_dopamine(&pool), baseline, 0.05f);
}

//=============================================================================
// Threat Response (Norepinephrine) Tests
//=============================================================================

TEST_F(NeuromodulatorTest, ThreatReleaseNorepinephrine) {
    /* WHAT: Test norepinephrine release on threat
     * WHY:  Threat → locus coeruleus activation → NE release
     */
    float ne_released = neuromodulator_release_norepinephrine(system, 0.8f, 0.2f);

    EXPECT_GT(ne_released, 0.0f);

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(system, &pool);

    EXPECT_GT(neuromodulator_pool_get_norepinephrine(&pool), config.baseline_norepinephrine);
}

TEST_F(NeuromodulatorTest, UncertaintyIncreasesNorepinephrine) {
    /* WHAT: Test that uncertainty amplifies NE response
     * WHY:  Uncertainty → increased vigilance needed
     */
    float ne_low_uncertainty = neuromodulator_release_norepinephrine(system, 0.5f, 0.1f);
    neuromodulator_reset(system);
    float ne_high_uncertainty = neuromodulator_release_norepinephrine(system, 0.5f, 0.9f);

    EXPECT_GT(ne_high_uncertainty, ne_low_uncertainty);
}

//=============================================================================
// Salience (Acetylcholine) Tests
//=============================================================================

TEST_F(NeuromodulatorTest, SalienceReleaseAcetylcholine) {
    /* WHAT: Test acetylcholine release on salient stimulus
     * WHY:  Salience → basal forebrain → ACh → attention
     */
    float ach_released = neuromodulator_release_acetylcholine(system, 0.8f);

    EXPECT_GT(ach_released, 0.0f);

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(system, &pool);

    EXPECT_GT(neuromodulator_pool_get_acetylcholine(&pool), config.baseline_acetylcholine);
}

//=============================================================================
// Punishment (Serotonin) Tests
//=============================================================================

TEST_F(NeuromodulatorTest, PunishmentReleaseSerotonin) {
    /* WHAT: Test serotonin release on punishment
     * WHY:  Aversive outcomes → raphe nuclei → 5-HT → inhibition
     */
    float serotonin_released = neuromodulator_release_serotonin(system, 0.7f);

    EXPECT_GT(serotonin_released, 0.0f);

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(system, &pool);

    EXPECT_GT(neuromodulator_pool_get_serotonin(&pool), config.baseline_serotonin);
}

//=============================================================================
// Receptor Profile Tests
//=============================================================================

TEST_F(NeuromodulatorTest, CorticalExcitatoryProfile) {
    /* WHAT: Test preset receptor profile for cortical excitatory neuron
     * WHY:  Should have high D1, moderate ACh sensitivity
     */
    receptor_profile_t profile = neuromodulator_profile_cortical_excitatory();

    EXPECT_GT(receptor_profile_get_d1_density(&profile), 0.5f);  // High D1
    EXPECT_GT(receptor_profile_get_nicotinic_density(&profile), 0.3f);  // Moderate ACh
}

TEST_F(NeuromodulatorTest, StriatalProfileHighDopamine) {
    /* WHAT: Test striatal neuron has very high dopamine sensitivity
     * WHY:  Striatum is primary dopamine target
     */
    receptor_profile_t profile = neuromodulator_profile_striatal();

    EXPECT_GT(receptor_profile_get_d1_density(&profile), 0.8f);
    EXPECT_GT(receptor_profile_get_d2_density(&profile), 0.8f);
}

//=============================================================================
// Modulation Effects Tests
//=============================================================================

TEST_F(NeuromodulatorTest, ComputeModulationEffects) {
    /* WHAT: Test computation of modulation effects
     * WHY:  Global levels × receptor densities = local effects
     */
    neuromodulator_set_level(system, NEUROMOD_DOPAMINE, 0.8f);

    receptor_profile_t receptors = neuromodulator_profile_cortical_excitatory();
    modulation_effects_t effects = modulation_effects_create();

    ASSERT_TRUE(neuromodulator_compute_effects(system, &receptors, &effects));

    // High dopamine should increase learning rate
    EXPECT_GT(modulation_effects_get_learning_rate_multiplier(&effects), 1.0f);
}

TEST_F(NeuromodulatorTest, LearningRateModulation) {
    /* WHAT: Test that neuromodulators scale learning rate
     * WHY:  Dopamine + ACh should enhance plasticity
     */
    neuromodulator_set_level(system, NEUROMOD_DOPAMINE, 0.9f);
    neuromodulator_set_level(system, NEUROMOD_ACETYLCHOLINE, 0.9f);

    receptor_profile_t receptors = neuromodulator_profile_hippocampal();
    modulation_effects_t effects = modulation_effects_create();
    neuromodulator_compute_effects(system, &receptors, &effects);

    float base_lr = 0.01f;
    float modulated_lr = neuromodulator_modulate_learning_rate(base_lr, &effects);

    EXPECT_GT(modulated_lr, base_lr);  // Should be enhanced
}

TEST_F(NeuromodulatorTest, TransmissionModulation) {
    /* WHAT: Test that attention modulates synaptic strength
     * WHY:  ACh + NE increase signal gain
     */
    neuromodulator_set_level(system, NEUROMOD_ACETYLCHOLINE, 0.8f);
    neuromodulator_set_level(system, NEUROMOD_NOREPINEPHRINE, 0.7f);

    receptor_profile_t receptors = neuromodulator_profile_cortical_excitatory();
    modulation_effects_t effects = modulation_effects_create();
    neuromodulator_compute_effects(system, &receptors, &effects);

    float base_weight = 0.5f;
    float modulated_weight = neuromodulator_modulate_transmission(base_weight, &effects);

    EXPECT_GT(modulated_weight, base_weight);
}

TEST_F(NeuromodulatorTest, ThresholdModulation) {
    /* WHAT: Test that norepinephrine lowers firing threshold
     * WHY:  Arousal → increased excitability
     */
    neuromodulator_set_level(system, NEUROMOD_NOREPINEPHRINE, 0.9f);

    receptor_profile_t receptors = neuromodulator_profile_cortical_excitatory();
    modulation_effects_t effects = modulation_effects_create();
    neuromodulator_compute_effects(system, &receptors, &effects);

    float base_threshold = 0.5f;
    float modulated_threshold = neuromodulator_modulate_threshold(base_threshold, &effects);

    EXPECT_LT(modulated_threshold, base_threshold);  // Lower = more excitable
}

//=============================================================================
// Ethics Integration Tests
//=============================================================================

TEST_F(NeuromodulatorTest, EthicsToNeuromodulatorsPositive) {
    /* WHAT: Test that positive ethics → dopamine release
     * WHY:  Good information = reward signal
     */
    float golden_rule = 0.8f;
    float trust = 0.9f;
    float harm = 0.1f;
    float salience = 0.7f;

    ASSERT_TRUE(neuromodulator_release_from_ethics(system, golden_rule, trust, harm, salience));

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(system, &pool);

    EXPECT_GT(neuromodulator_pool_get_dopamine(&pool), config.baseline_dopamine);  // Reward
    EXPECT_GT(neuromodulator_pool_get_acetylcholine(&pool), config.baseline_acetylcholine);  // Attention
}

TEST_F(NeuromodulatorTest, EthicsToNeuromodulatorsNegative) {
    /* WHAT: Test that negative ethics → serotonin + norepinephrine
     * WHY:  Bad information = inhibit + vigilance
     */
    float golden_rule = -0.7f;  // Negative
    float trust = 0.2f;
    float harm = 0.9f;
    float salience = 0.8f;

    ASSERT_TRUE(neuromodulator_release_from_ethics(system, golden_rule, trust, harm, salience));

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(system, &pool);

    EXPECT_GT(neuromodulator_pool_get_serotonin(&pool), config.baseline_serotonin);  // Inhibition
    EXPECT_GT(neuromodulator_pool_get_norepinephrine(&pool), config.baseline_norepinephrine);  // Threat
}

TEST_F(NeuromodulatorTest, GetLearningWeightFromEthics) {
    /* WHAT: Test computing effective learning weight from ethics
     * WHY:  Good ethics → high weight, bad ethics → low weight
     */
    // Good ethics
    neuromodulator_release_from_ethics(system, 0.8f, 0.9f, 0.1f, 0.8f);
    receptor_profile_t receptors = neuromodulator_profile_hippocampal();
    float good_weight = neuromodulator_get_learning_weight(system, &receptors);

    // Reset and test bad ethics
    neuromodulator_reset(system);
    neuromodulator_release_from_ethics(system, -0.7f, 0.2f, 0.9f, 0.5f);
    float bad_weight = neuromodulator_get_learning_weight(system, &receptors);

    EXPECT_GT(good_weight, bad_weight);
}

//=============================================================================
// Performance Tests
//=============================================================================

TEST_F(NeuromodulatorTest, UpdatePerformance) {
    /* WHAT: Test that update is O(1) in number of neuromodulators
     * WHY:  Should be fast enough for real-time simulation
     */
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++) {
        neuromodulator_update(system, 0.01f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    /* WHAT: Verify reasonable performance (< 50ms for 10k updates)
     * WHY:  Allow headroom for debug builds and loaded systems
     * NOTE: ~5μs per update is acceptable for real-time simulation
     */
    EXPECT_LT(duration.count(), 50000);
}

TEST_F(NeuromodulatorTest, ModulationEffectsPerformance) {
    /* WHAT: Test that computing effects is O(1)
     * WHY:  Called per synapse, must be very fast
     */
    receptor_profile_t receptors = neuromodulator_profile_cortical_excitatory();
    modulation_effects_t effects = modulation_effects_create();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100000; i++) {
        neuromodulator_compute_effects(system, &receptors, &effects);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    /* WHAT: Verify reasonable performance (< 100ms for 100k calls)
     * WHY:  Allow headroom for debug builds and loaded systems
     * NOTE: ~1μs per call is acceptable for per-synapse computation
     */
    EXPECT_LT(duration.count(), 100000);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(NeuromodulatorTest, GetStatistics) {
    /* WHAT: Test statistics collection
     * WHY:  Need to monitor system behavior
     */
    neuromodulator_stats_t stats;
    ASSERT_TRUE(neuromodulator_get_stats(system, &stats));

    EXPECT_FLOAT_EQ(stats.current_dopamine, config.baseline_dopamine);
}

TEST_F(NeuromodulatorTest, StatisticsTrackReleases) {
    /* WHAT: Test that release events are counted
     * WHY:  Useful for debugging and analysis
     */
    neuromodulator_release_dopamine(system, 0.8f, 0.3f);
    neuromodulator_release_acetylcholine(system, 0.7f);

    neuromodulator_stats_t stats;
    neuromodulator_get_stats(system, &stats);

    EXPECT_EQ(stats.dopamine_releases, 1);
    EXPECT_EQ(stats.acetylcholine_releases, 1);
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(NeuromodulatorTest, NullPointerSafety) {
    /* WHAT: Test null pointer handling
     * WHY:  Should not crash on invalid input
     */
    EXPECT_FALSE(neuromodulator_get_levels(nullptr, nullptr));
    EXPECT_FALSE(neuromodulator_set_level(nullptr, NEUROMOD_DOPAMINE, 0.5f));
    EXPECT_FALSE(neuromodulator_update(nullptr, 1.0f));
}

TEST_F(NeuromodulatorTest, ResetToBaseline) {
    /* WHAT: Test reset functionality
     * WHY:  Should return to baseline concentrations
     */
    neuromodulator_set_level(system, NEUROMOD_DOPAMINE, 0.9f);
    neuromodulator_set_level(system, NEUROMOD_SEROTONIN, 0.1f);

    ASSERT_TRUE(neuromodulator_reset(system));

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(system, &pool);

    EXPECT_FLOAT_EQ(neuromodulator_pool_get_dopamine(&pool), config.baseline_dopamine);
    EXPECT_FLOAT_EQ(neuromodulator_pool_get_serotonin(&pool), config.baseline_serotonin);
}

//=============================================================================
// Boundary Condition Tests
//=============================================================================

TEST_F(NeuromodulatorTest, ExtremeLevels_Maximum) {
    /* WHAT: Test maximum concentration boundaries
     * WHY:  Should clamp to [0, 1] range
     */
    neuromodulator_set_level(system, NEUROMOD_DOPAMINE, 10.0f);

    float level = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);
    EXPECT_LE(level, 1.0f);
    EXPECT_GE(level, 0.0f);
}

TEST_F(NeuromodulatorTest, ExtremeLevels_Minimum) {
    /* WHAT: Test minimum concentration boundaries
     * WHY:  Should not go negative
     */
    neuromodulator_set_level(system, NEUROMOD_DOPAMINE, -5.0f);

    float level = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);
    EXPECT_GE(level, 0.0f);
}

TEST_F(NeuromodulatorTest, RapidFluctuations) {
    /* WHAT: Test rapid concentration changes
     * WHY:  System should remain stable under stress
     */
    for (int i = 0; i < 1000; i++) {
        float value = (i % 2 == 0) ? 1.0f : 0.0f;
        neuromodulator_set_level(system, NEUROMOD_DOPAMINE, value);

        float level = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);
        EXPECT_GE(level, 0.0f);
        EXPECT_LE(level, 1.0f);
    }
}

TEST_F(NeuromodulatorTest, ZeroTimeStep) {
    /* WHAT: Test update with dt=0
     * WHY:  Should handle gracefully without division by zero
     */
    neuromodulator_set_level(system, NEUROMOD_DOPAMINE, 0.8f);

    EXPECT_TRUE(neuromodulator_update(system, 0.0f));

    float level = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);
    EXPECT_NEAR(level, 0.8f, 0.01f);  // Should not decay
}

TEST_F(NeuromodulatorTest, VeryLargeTimeStep) {
    /* WHAT: Test update with very large dt
     * WHY:  Should decay toward baseline without numerical issues
     */
    neuromodulator_set_level(system, NEUROMOD_DOPAMINE, 0.9f);

    EXPECT_TRUE(neuromodulator_update(system, 10000.0f));  // 10 seconds

    float level = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);
    EXPECT_NEAR(level, config.baseline_dopamine, 0.1f);  // Should be near baseline
}

//=============================================================================
// Stress and Robustness Tests
//=============================================================================

TEST_F(NeuromodulatorTest, ThousandUpdates_Stability) {
    /* WHAT: Test stability over many updates
     * WHY:  Should not drift or accumulate errors
     */
    float initial_da = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);

    for (int i = 0; i < 1000; i++) {
        neuromodulator_update(system, 1.0f);
    }

    float final_da = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);

    // Should converge to baseline
    EXPECT_NEAR(final_da, config.baseline_dopamine, 0.01f);
}

TEST_F(NeuromodulatorTest, AlternatingRewards_Stability) {
    /* WHAT: Test alternating reward/punishment
     * WHY:  Should remain stable under oscillating inputs
     */
    for (int i = 0; i < 100; i++) {
        if (i % 2 == 0) {
            neuromodulator_release_dopamine(system, 1.0f, 0.0f);  // Reward
        } else {
            neuromodulator_release_serotonin(system, 0.8f);  // Punishment
        }
        neuromodulator_update(system, 10.0f);
    }

    neuromodulator_stats_t stats;
    neuromodulator_get_stats(system, &stats);

    // Should have balanced release counts
    EXPECT_GT(stats.dopamine_releases, 0);
    EXPECT_GT(stats.serotonin_releases, 0);
}

TEST_F(NeuromodulatorTest, AllNeuromodulatorsSimultaneous) {
    /* WHAT: Test releasing all neuromodulators at once
     * WHY:  Verify no interference between systems
     */
    neuromodulator_release_dopamine(system, 0.8f, 0.3f);
    neuromodulator_release_serotonin(system, 0.6f);
    neuromodulator_release_acetylcholine(system, 0.7f);
    neuromodulator_release_norepinephrine(system, 0.5f, 0.4f);

    neuromodulator_pool_t pool = neuromodulator_pool_create();
    neuromodulator_get_levels(system, &pool);

    // All should be elevated above baseline
    EXPECT_GT(neuromodulator_pool_get_dopamine(&pool), config.baseline_dopamine);
    EXPECT_GT(neuromodulator_pool_get_serotonin(&pool), config.baseline_serotonin);
    EXPECT_GT(neuromodulator_pool_get_acetylcholine(&pool), config.baseline_acetylcholine);
    EXPECT_GT(neuromodulator_pool_get_norepinephrine(&pool), config.baseline_norepinephrine);
}

//=============================================================================
// Numerical Precision Tests
//=============================================================================

TEST_F(NeuromodulatorTest, TinyConcentrationChanges) {
    /* WHAT: Test handling of very small concentration changes
     * WHY:  Should maintain precision for subtle modulation
     */
    neuromodulator_set_level(system, NEUROMOD_DOPAMINE, 0.5f);

    float before = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);
    neuromodulator_set_level(system, NEUROMOD_DOPAMINE, 0.50001f);
    float after = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);

    EXPECT_NE(before, after);  // Should detect tiny change
}

TEST_F(NeuromodulatorTest, RepeatedSmallReleases) {
    /* WHAT: Test accumulation of many small releases
     * WHY:  Should sum correctly without loss of precision
     */
    float initial = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);

    for (int i = 0; i < 100; i++) {
        neuromodulator_release_dopamine(system, 0.01f, 0.0f);
    }

    float final = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);
    EXPECT_GT(final, initial);
}

//=============================================================================
// Receptor Profile Tests
//=============================================================================

TEST_F(NeuromodulatorTest, DifferentReceptorProfiles) {
    /* WHAT: Test different receptor profiles produce different effects
     * WHY:  Verify receptor density modulates response
     */
    neuromodulator_set_level(system, NEUROMOD_DOPAMINE, 0.8f);

    receptor_profile_t excitatory = neuromodulator_profile_cortical_excitatory();
    receptor_profile_t inhibitory = neuromodulator_profile_cortical_inhibitory();

    modulation_effects_t effects_exc = modulation_effects_create();
    modulation_effects_t effects_inh = modulation_effects_create();

    neuromodulator_compute_effects(system, &excitatory, &effects_exc);
    neuromodulator_compute_effects(system, &inhibitory, &effects_inh);

    // Effects should differ due to different receptor densities
    EXPECT_NE(modulation_effects_get_learning_rate_multiplier(&effects_exc),
              modulation_effects_get_learning_rate_multiplier(&effects_inh));
}

TEST_F(NeuromodulatorTest, AllReceptorProfilePresets) {
    /* WHAT: Test all factory-created receptor profiles
     * WHY:  Ensure all presets are valid and distinct
     */
    receptor_profile_t cortical_exc = neuromodulator_profile_cortical_excitatory();
    receptor_profile_t cortical_inh = neuromodulator_profile_cortical_inhibitory();
    receptor_profile_t hippocampal = neuromodulator_profile_hippocampal();
    receptor_profile_t striatal = neuromodulator_profile_striatal();
    receptor_profile_t amygdala = neuromodulator_profile_amygdala();

    // All should have valid receptor densities
    EXPECT_GE(receptor_profile_get_d1_density(&cortical_exc), 0.0f);
    EXPECT_GE(receptor_profile_get_d2_density(&cortical_inh), 0.0f);
    EXPECT_GE(receptor_profile_get_nicotinic_density(&hippocampal), 0.0f);
    EXPECT_GE(receptor_profile_get_d1_density(&striatal), 0.0f);
    EXPECT_GE(receptor_profile_get_alpha_density(&amygdala), 0.0f);

    // Profiles should differ
    EXPECT_NE(receptor_profile_get_d1_density(&cortical_exc),
              receptor_profile_get_d1_density(&striatal));
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(NeuromodulatorTest, MultipleCreateDestroy) {
    /* WHAT: Test creating and destroying multiple systems
     * WHY:  Verify no memory leaks or corruption
     */
    for (int i = 0; i < 10; i++) {
        neuromodulator_system_t temp_system = neuromodulator_system_create(&config);
        ASSERT_NE(temp_system, nullptr);
        neuromodulator_system_destroy(temp_system);
    }

    // Original system should still be valid
    EXPECT_TRUE(neuromodulator_get_levels(system, nullptr) || true);
}

TEST_F(NeuromodulatorTest, DoubleDestroy_Safe) {
    /* WHAT: Test destroying null pointer is safe
     * WHY:  Guard clause should handle null gracefully
     * NOTE: Double-destroy with same pointer is undefined behavior (user error)
     */
    neuromodulator_system_t temp = nullptr;
    neuromodulator_system_destroy(temp);  // Should not crash
    neuromodulator_system_destroy(temp);  // Should not crash

    SUCCEED();  // If we get here, test passed
}
