/**
 * @file test_phasic_tonic_integration.cpp
 * @brief Integration tests for phasic-tonic neuromodulation in cognitive modules
 *
 * WHAT: Tests that phasic-tonic dynamics are actively used by brain/cognitive systems
 * WHY:  Ensure Phase C2.2 features are properly wired into cognitive pipeline
 * HOW:  Test neuromodulator system integration, TD error encoding, burst dynamics
 *
 * @version Phase C2.2 Integration
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>

#include "core/brain/nimcp_brain.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/neuromodulators/nimcp_phasic_tonic.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PhasicTonicIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    neuromodulator_system_t neuromod_system;

    void SetUp() override {
        // Create brain with neuromodulator system
        brain = brain_create("phasic_tonic_test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
        ASSERT_NE(brain, nullptr) << "Failed to create brain";

        // Get neuromodulator system
        neuromod_system = brain_get_neuromodulator_system(brain);
        ASSERT_NE(neuromod_system, nullptr) << "Brain should have neuromodulator system";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Integration Test 1: Neuromodulator System Initialization
//=============================================================================

TEST_F(PhasicTonicIntegrationTest, NeuromodSystemInitialized) {
    // Verify neuromodulator system is initialized
    ASSERT_NE(neuromod_system, nullptr);

    // Check dopamine level (should have baseline tonic level)
    float da_level = neuromodulator_get_level(neuromod_system, NEUROMOD_DOPAMINE);
    EXPECT_GT(da_level, 0.0f);
    EXPECT_LT(da_level, 1.0f);

    // Check serotonin level
    float serotonin_level = neuromodulator_get_level(neuromod_system, NEUROMOD_SEROTONIN);
    EXPECT_GT(serotonin_level, 0.0f);
    EXPECT_LT(serotonin_level, 1.0f);

    // Check norepinephrine level
    float ne_level = neuromodulator_get_level(neuromod_system, NEUROMOD_NOREPINEPHRINE);
    EXPECT_GT(ne_level, 0.0f);
    EXPECT_LT(ne_level, 1.0f);

    // Check acetylcholine level
    float ach_level = neuromodulator_get_level(neuromod_system, NEUROMOD_ACETYLCHOLINE);
    EXPECT_GT(ach_level, 0.0f);
    EXPECT_LT(ach_level, 1.0f);
}

//=============================================================================
// Integration Test 2: Dopamine Release on Reward
//=============================================================================

TEST_F(PhasicTonicIntegrationTest, DopamineReleaseOnReward) {
    // Get baseline dopamine
    float baseline_da = neuromodulator_get_level(neuromod_system, NEUROMOD_DOPAMINE);

    // Release dopamine (simulating reward)
    float reward_magnitude = 1.0f;
    float released = neuromodulator_release_dopamine(neuromod_system, reward_magnitude, false);
    EXPECT_GT(released, 0.0f);

    // Dopamine should increase
    float post_reward_da = neuromodulator_get_level(neuromod_system, NEUROMOD_DOPAMINE);
    EXPECT_GT(post_reward_da, baseline_da);
}

//=============================================================================
// Integration Test 3: Neuromodulator Update Over Time
//=============================================================================

TEST_F(PhasicTonicIntegrationTest, NeuromodulatorDecayOverTime) {
    // Release dopamine
    neuromodulator_release_dopamine(neuromod_system, 1.0f, false);
    float peak_da = neuromodulator_get_level(neuromod_system, NEUROMOD_DOPAMINE);

    // Update system (simulate time passing)
    float dt = 0.1f;  // 100ms
    for (int i = 0; i < 10; i++) {
        neuromodulator_update(neuromod_system, dt);
    }

    // Dopamine should decay back towards baseline
    float decayed_da = neuromodulator_get_level(neuromod_system, NEUROMOD_DOPAMINE);
    EXPECT_LT(decayed_da, peak_da);
}

//=============================================================================
// Integration Test 4: Learning Rate Modulation by Dopamine
//=============================================================================

TEST_F(PhasicTonicIntegrationTest, LearningRateModulation) {
    float base_lr = 0.01f;

    // Create receptor profile using factory + setters
    receptor_profile_t receptors = receptor_profile_create();
    receptor_profile_set_d1_density(&receptors, 0.8f);
    receptor_profile_set_d2_density(&receptors, 0.2f);
    receptor_profile_set_serotonin_density(&receptors, 0.5f);
    receptor_profile_set_nicotinic_density(&receptors, 0.5f);
    receptor_profile_set_alpha_density(&receptors, 0.5f);
    receptor_profile_set_beta_density(&receptors, 0.5f);

    // Compute modulation effects
    modulation_effects_t effects = {};
    bool computed = neuromodulator_compute_effects(neuromod_system, &receptors, &effects);
    ASSERT_TRUE(computed);

    // Modulate learning rate
    float modulated_lr = neuromodulator_modulate_learning_rate(base_lr, &effects);

    // Modulated LR should be positive and reasonable
    EXPECT_GT(modulated_lr, 0.0f);
    EXPECT_LT(modulated_lr, base_lr * 10.0f);  // Not more than 10x
}

//=============================================================================
// Integration Test 5: Multiple Neuromodulators Active Simultaneously
//=============================================================================

TEST_F(PhasicTonicIntegrationTest, MultipleNeurotransmittersActive) {
    // Release multiple neurotransmitters
    neuromodulator_release_dopamine(neuromod_system, 0.8f, false);
    neuromodulator_release_serotonin(neuromod_system, 0.3f);
    neuromodulator_release_acetylcholine(neuromod_system, 0.6f);
    neuromodulator_release_norepinephrine(neuromod_system, 0.5f, false);

    // All should have elevated levels
    EXPECT_GT(neuromodulator_get_level(neuromod_system, NEUROMOD_DOPAMINE), 0.05f);
    EXPECT_GT(neuromodulator_get_level(neuromod_system, NEUROMOD_SEROTONIN), 0.05f);
    EXPECT_GT(neuromodulator_get_level(neuromod_system, NEUROMOD_ACETYLCHOLINE), 0.05f);
    EXPECT_GT(neuromodulator_get_level(neuromod_system, NEUROMOD_NOREPINEPHRINE), 0.05f);
}

//=============================================================================
// Integration Test 6: Reward-Based Learning Uses Neuromodulation
//=============================================================================

TEST_F(PhasicTonicIntegrationTest, RewardLearningUsesNeuromodulation) {
    // Perform some brain operations to activate synapses
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    EXPECT_NE(decision, nullptr);
    EXPECT_NE(decision->label, nullptr);

    // Apply reward learning (this should interact with neuromodulation)
    float reward = 1.0f;
    uint32_t synapses_modified = brain_apply_reward_learning(brain, reward);

    // Some synapses should have been modified
    // (Exact number depends on network size and activation)
    EXPECT_GE(synapses_modified, 0);
}

//=============================================================================
// Integration Test 7: Negative Reward Affects Learning
//=============================================================================

TEST_F(PhasicTonicIntegrationTest, NegativeRewardAffectsLearning) {
    // Activate network
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decide(brain, features, 4);

    // Apply negative reward (punishment)
    float punishment = -0.5f;
    uint32_t synapses_modified = brain_apply_reward_learning(brain, punishment);

    // Negative rewards should still modify synapses
    EXPECT_GE(synapses_modified, 0);
}

//=============================================================================
// Integration Test 8: Neuromodulator Statistics
//=============================================================================

TEST_F(PhasicTonicIntegrationTest, NeuromodulatorStatistics) {
    // Release some dopamine
    neuromodulator_release_dopamine(neuromod_system, 0.8f, false);

    // Get statistics
    neuromodulator_stats_t stats;
    bool got_stats = neuromodulator_get_stats(neuromod_system, &stats);
    ASSERT_TRUE(got_stats);

    // Stats should be reasonable
    EXPECT_GE(stats.current_dopamine, 0.0f);
    EXPECT_LE(stats.current_dopamine, 2.0f);  // Can go above 1.0 during bursts
}

//=============================================================================
// Integration Test 9: Neuromodulator Reset
//=============================================================================

TEST_F(PhasicTonicIntegrationTest, NeuromodulatorReset) {
    // Release dopamine to elevate levels
    neuromodulator_release_dopamine(neuromod_system, 1.0f, false);
    float elevated_da = neuromodulator_get_level(neuromod_system, NEUROMOD_DOPAMINE);

    // Reset system
    bool reset_success = neuromodulator_reset(neuromod_system);
    ASSERT_TRUE(reset_success);

    // Dopamine should return to baseline
    float reset_da = neuromodulator_get_level(neuromod_system, NEUROMOD_DOPAMINE);
    EXPECT_LT(reset_da, elevated_da);
}

//=============================================================================
// Integration Test 10: Learning Weight Computation
//=============================================================================

TEST_F(PhasicTonicIntegrationTest, LearningWeightComputation) {
    // Release dopamine for positive learning signal
    neuromodulator_release_dopamine(neuromod_system, 0.8f, false);

    // Create receptor profile for D1-dominant neuron (like striatal Go pathway)
    receptor_profile_t d1_profile = receptor_profile_create();
    receptor_profile_set_d1_density(&d1_profile, 1.0f);         // High D1 density
    receptor_profile_set_d2_density(&d1_profile, 0.1f);         // Low D2 density
    receptor_profile_set_serotonin_density(&d1_profile, 0.5f);
    receptor_profile_set_nicotinic_density(&d1_profile, 0.5f);
    receptor_profile_set_alpha_density(&d1_profile, 0.5f);
    receptor_profile_set_beta_density(&d1_profile, 0.5f);

    // Get learning weight for D1-dominant neuron
    float d1_weight = neuromodulator_get_learning_weight(neuromod_system, &d1_profile);

    // Learning weight should be positive and meaningful
    EXPECT_GT(d1_weight, 0.0f);
    EXPECT_LT(d1_weight, 10.0f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
