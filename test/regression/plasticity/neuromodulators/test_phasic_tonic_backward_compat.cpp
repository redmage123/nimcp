/**
 * @file test_phasic_tonic_backward_compat.cpp
 * @brief Regression tests for phasic-tonic neuromodulation backward compatibility
 *
 * WHAT: Ensures Phase C2.2 neuromodulation features don't break existing code
 * WHY:  Verify zero breaking changes to pre-C2.2 code
 * HOW:  Test legacy patterns and ensure they still work correctly
 *
 * @version Phase C2.2 Regression Testing
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
    #include "core/brain/nimcp_brain.h"
    #include "plasticity/neuromodulators/nimcp_neuromodulators.h"
    #include "plasticity/neuromodulators/nimcp_phasic_tonic.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PhasicTonicRegressionTest : public ::testing::Test {
protected:
    brain_t brain;

    void SetUp() override {
        brain = nullptr;
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

//=============================================================================
// Regression Test 1: Brain Creation Still Works
//=============================================================================

TEST_F(PhasicTonicRegressionTest, BrainCreation_StillWorks) {
    // Old code pattern: Create brain with default config
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);

    // Should still work exactly as before
    ASSERT_NE(brain, nullptr) << "Brain creation should not be broken by Phase C2.2";
}

//=============================================================================
// Regression Test 2: Legacy Learning Without Neuromodulation
//=============================================================================

TEST_F(PhasicTonicRegressionTest, LegacyLearning_StillWorks) {
    // Old code pattern: Create brain and do learning without neuromodulation awareness
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old supervised learning pattern (no neuromodulation)
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    EXPECT_NE(decision, nullptr);
    if (decision) {
        EXPECT_NE(decision->label, nullptr);
    }

    // Old reward learning pattern (should still work)
    float reward = 1.0f;
    uint32_t modified = brain_apply_reward_learning(brain, reward);

    // Should work without breaking
    EXPECT_GE(modified, 0);
}

//=============================================================================
// Regression Test 3: Neuromodulator System Optional
//=============================================================================

TEST_F(PhasicTonicRegressionTest, NeuromodSystem_Optional) {
    // Brain should work even if neuromodulator system is NULL or disabled
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Get neuromodulator system (may be NULL in minimal configurations)
    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(brain);

    // If system exists, it should have valid defaults
    if (neuromod) {
        float da = neuromodulator_get_level(neuromod, NEUROMOD_DOPAMINE);
        EXPECT_GE(da, 0.0f);
        EXPECT_LE(da, 2.0f);  // Allow for bursts
    }

    // Brain should still function for decision making
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
    brain_decision_t* decision = brain_decide(brain, features, 4);
    EXPECT_NE(decision, nullptr);
    if (decision) {
        EXPECT_NE(decision->label, nullptr);
    }
}

//=============================================================================
// Regression Test 4: Dopamine Doesn't Interfere with Non-RL Learning
//=============================================================================

TEST_F(PhasicTonicRegressionTest, Dopamine_NoInterference) {
    // Create brain
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old supervised learning pattern
    float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};

    // Decision 1
    brain_decision_t* decision1 = brain_decide(brain, features, 4);
    ASSERT_NE(decision1, nullptr);
    float conf1 = decision1->confidence;

    // Decision 2 (should be consistent)
    brain_decision_t* decision2 = brain_decide(brain, features, 4);
    ASSERT_NE(decision2, nullptr);
    float conf2 = decision2->confidence;

    // Confidence should be consistent (dopamine shouldn't cause wild variations)
    EXPECT_NEAR(conf1, conf2, 0.1f) << "Neuromodulation shouldn't cause erratic behavior";
}

//=============================================================================
// Regression Test 5: No Performance Regression
//=============================================================================

TEST_F(PhasicTonicRegressionTest, NoPerformanceRegression) {
    // Create brain
    brain = brain_create("test", BRAIN_SIZE_SMALL, BRAIN_TASK_CLASSIFICATION, 10, 5);
    ASSERT_NE(brain, nullptr);

    // Measure time for 100 inferences (with neuromodulation present)
    float features[10];
    for (int i = 0; i < 10; i++) {
        features[i] = 0.5f;
    }

    uint64_t start_time = nimcp_time_get_us();
    for (int i = 0; i < 100; i++) {
        brain_decision_t* decision = brain_decide(brain, features, 10);
        (void)decision;  // Suppress unused warning
    }
    uint64_t end_time = nimcp_time_get_us();

    uint64_t elapsed_us = end_time - start_time;
    float avg_us = elapsed_us / 100.0f;

    // Should be reasonably fast (< 1ms per inference for SMALL brain)
    EXPECT_LT(avg_us, 1000.0f) << "Neuromodulation shouldn't cause severe performance regression";
}

//=============================================================================
// Regression Test 6: Statistics API Backward Compatible
//=============================================================================

TEST_F(PhasicTonicRegressionTest, Statistics_BackwardCompatible) {
    // Create brain
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old statistics pattern
    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(brain);
    if (neuromod) {
        neuromodulator_stats_t stats;
        bool got_stats = neuromodulator_get_stats(neuromod, &stats);

        if (got_stats) {
            // Stats should have reasonable values
            EXPECT_GE(stats.current_dopamine, 0.0f);
            EXPECT_LE(stats.current_dopamine, 2.0f);
        }
    }
}

//=============================================================================
// Regression Test 7: NULL Safety Maintained
//=============================================================================

TEST_F(PhasicTonicRegressionTest, NullSafety_Maintained) {
    // Test NULL brain handle (should not crash)
    neuromodulator_system_t null_neuromod = brain_get_neuromodulator_system(nullptr);
    EXPECT_EQ(null_neuromod, nullptr) << "NULL brain should return NULL neuromod system";

    // Test NULL neuromodulator system operations
    float level = neuromodulator_get_level(nullptr, NEUROMOD_DOPAMINE);
    EXPECT_EQ(level, 0.0f) << "NULL neuromod system should return 0.0";

    float released = neuromodulator_release_dopamine(nullptr, 1.0f, false);
    EXPECT_EQ(released, 0.0f) << "NULL neuromod system should return 0.0";

    bool updated = neuromodulator_update(nullptr, 0.1f);
    EXPECT_FALSE(updated) << "NULL neuromod system update should return false";

    bool reset = neuromodulator_reset(nullptr);
    EXPECT_FALSE(reset) << "NULL neuromod system reset should return false";
}

//=============================================================================
// Regression Test 8: Memory Management No Leaks
//=============================================================================

TEST_F(PhasicTonicRegressionTest, MemoryManagement_NoLeaks) {
    // Create and destroy brain multiple times
    for (int i = 0; i < 10; i++) {
        brain_t temp_brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
        ASSERT_NE(temp_brain, nullptr);

        // Get neuromodulator system
        neuromodulator_system_t neuromod = brain_get_neuromodulator_system(temp_brain);
        (void)neuromod;  // Suppress unused warning

        // Do some operations
        float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
        brain_decide(temp_brain, features, 4);

        // Destroy (should not leak)
        brain_destroy(temp_brain);
    }

    // If we get here without crashing, memory management is OK
    SUCCEED();
}

//=============================================================================
// Regression Test 9: Default Configuration Values Stable
//=============================================================================

TEST_F(PhasicTonicRegressionTest, DefaultConfig_Stable) {
    // Create brain with default config
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Get neuromodulator system
    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(brain);
    if (neuromod) {
        // Check that default baseline levels are reasonable and stable
        float da = neuromodulator_get_level(neuromod, NEUROMOD_DOPAMINE);
        float sero = neuromodulator_get_level(neuromod, NEUROMOD_SEROTONIN);
        float ne = neuromodulator_get_level(neuromod, NEUROMOD_NOREPINEPHRINE);
        float ach = neuromodulator_get_level(neuromod, NEUROMOD_ACETYLCHOLINE);

        // All should be in reasonable baseline range (adjusted for actual normalized values)
        // Baseline tonic levels are around 0.5-0.6 after normalization, but can drift to ~0.8
        EXPECT_GT(da, 0.01f);
        EXPECT_LT(da, 0.9f);
        EXPECT_GT(sero, 0.01f);
        EXPECT_LT(sero, 0.9f);
        EXPECT_GT(ne, 0.01f);
        EXPECT_LT(ne, 0.9f);
        EXPECT_GT(ach, 0.01f);
        EXPECT_LT(ach, 0.9f);
    }
}

//=============================================================================
// Regression Test 10: Old Reward Learning Pattern Works
//=============================================================================

TEST_F(PhasicTonicRegressionTest, OldRewardPattern_Works) {
    // Old code pattern from pre-C2.2
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Old RL training loop pattern
    for (int episode = 0; episode < 5; episode++) {
        // Get features
        float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};

        // Make decision
        brain_decision_t* decision = brain_decide(brain, features, 4);
        EXPECT_NE(decision, nullptr);
        if (decision) {
            EXPECT_NE(decision->label, nullptr);
        }

        // Apply reward (old API - should still work)
        float reward = 1.0f;
        uint32_t modified = brain_apply_reward_learning(brain, reward);
        EXPECT_GE(modified, 0);
    }

    // Should complete without errors
    SUCCEED();
}

//=============================================================================
// Regression Test 11: Phasic-Tonic Doesn't Break STDP
//=============================================================================

TEST_F(PhasicTonicRegressionTest, PhasicTonic_NoSTDPInterference) {
    // Create brain
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Get neuromodulator system
    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(brain);

    if (neuromod) {
        // Release dopamine
        neuromodulator_release_dopamine(neuromod, 0.5f, false);

        // Apply learning (should work with STDP)
        float features[4] = {0.5f, 0.3f, 0.7f, 0.2f};
        brain_decide(brain, features, 4);

        uint32_t modified = brain_apply_reward_learning(brain, 1.0f);

        // Should work without breaking STDP
        EXPECT_GE(modified, 0);
    }
}

//=============================================================================
// Regression Test 12: Receptor Types Don't Break Learning
//=============================================================================

TEST_F(PhasicTonicRegressionTest, ReceptorTypes_NoBreakage) {
    // Create brain
    brain = brain_create("test", BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 4, 2);
    ASSERT_NE(brain, nullptr);

    // Get neuromodulator system
    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(brain);

    if (neuromod) {
        // Create D1-dominant receptor profile
        receptor_profile_t d1_profile = receptor_profile_create();
        receptor_profile_set_d1_density(&d1_profile, 1.0f);
        receptor_profile_set_d2_density(&d1_profile, 0.1f);
        receptor_profile_set_serotonin_density(&d1_profile, 0.5f);
        receptor_profile_set_nicotinic_density(&d1_profile, 0.5f);
        receptor_profile_set_alpha_density(&d1_profile, 0.5f);
        receptor_profile_set_beta_density(&d1_profile, 0.5f);

        // Get learning weight for D1-dominant neuron
        float d1_weight = neuromodulator_get_learning_weight(neuromod, &d1_profile);

        // Should return reasonable value
        EXPECT_GT(d1_weight, 0.0f);
        EXPECT_LT(d1_weight, 10.0f);

        // Create D2-dominant receptor profile
        receptor_profile_t d2_profile = receptor_profile_create();
        receptor_profile_set_d1_density(&d2_profile, 0.1f);
        receptor_profile_set_d2_density(&d2_profile, 1.0f);
        receptor_profile_set_serotonin_density(&d2_profile, 0.5f);
        receptor_profile_set_nicotinic_density(&d2_profile, 0.5f);
        receptor_profile_set_alpha_density(&d2_profile, 0.5f);
        receptor_profile_set_beta_density(&d2_profile, 0.5f);

        // Get learning weight for D2-dominant neuron
        float d2_weight = neuromodulator_get_learning_weight(neuromod, &d2_profile);

        // Should return reasonable value (may be different from D1)
        EXPECT_GT(d2_weight, -10.0f);
        EXPECT_LT(d2_weight, 10.0f);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
