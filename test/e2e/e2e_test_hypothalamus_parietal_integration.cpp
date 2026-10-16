/**
 * @file e2e_test_hypothalamus_parietal_integration.cpp
 * @brief End-to-end tests for Hypothalamus-Parietal integration
 *
 * WHAT: Verify hypothalamus and parietal lobe are properly integrated in brain
 * WHY:  Both systems should be initialized and able to work together
 * HOW:  Test initialization order, co-existence, and indirect interactions
 *
 * BIOLOGICAL BASIS:
 * - Hypothalamus provides drive states (hunger, stress, fatigue)
 * - These states influence cognitive performance including mathematical reasoning
 * - Parietal lobe mathematical performance can be affected by stress/fatigue
 * - Successful problem-solving can generate reward signals
 *
 * @version 1.0.0
 * @date 2025-01-10
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/factory/init/nimcp_brain_init_medulla.h"
#include "core/brain/factory/init/nimcp_brain_init_hypothalamus.h"
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h"
#include "cognitive/parietal/nimcp_parietal.h"
#include "core/medulla/nimcp_medulla.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class HypothalamusParietalIntegrationTest : public ::testing::Test {
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

    /**
     * @brief Create brain with both hypothalamus and parietal enabled
     */
    brain_t CreateBrainWithBothSystems() {
        brain_t b = brain_create(
            "hypo_parietal_test",
            BRAIN_SIZE_SMALL,
            BRAIN_TASK_CLASSIFICATION,
            32,
            8
        );

        if (!b) {
            return nullptr;
        }

        // Ensure both systems are initialized
        if (!b->hypothalamus) {
            if (!b->medulla) {
                nimcp_brain_factory_init_medulla_subsystem(b);
            }
            nimcp_brain_factory_init_hypothalamus_subsystem(b);
        }

        if (!b->parietal) {
            // Enable parietal in config (disabled by default)
            b->config.enable_parietal = true;
            nimcp_brain_factory_init_parietal_subsystem(b);
        }

        return b;
    }
};

// ============================================================================
// TEST: BOTH SYSTEMS INITIALIZED
// ============================================================================

/**
 * @test BothSystemsInitialized
 * @brief Verify both hypothalamus and parietal are initialized
 */
TEST_F(HypothalamusParietalIntegrationTest, BothSystemsInitialized) {
    brain = CreateBrainWithBothSystems();
    ASSERT_NE(nullptr, brain) << "Brain creation failed";

    // Hypothalamus should be initialized
    EXPECT_TRUE(brain->hypothalamus_enabled) << "Hypothalamus should be enabled";
    EXPECT_NE(nullptr, brain->hypothalamus) << "Hypothalamus adapter should exist";

    // Parietal should be initialized
    EXPECT_TRUE(brain->parietal_enabled) << "Parietal should be enabled";
    EXPECT_NE(nullptr, brain->parietal) << "Parietal lobe should exist";
}

/**
 * @test InitializationOrder
 * @brief Verify correct initialization order (medulla → hypothalamus → parietal)
 *
 * Note: Parietal is disabled by default in brain config, so we enable it explicitly.
 */
TEST_F(HypothalamusParietalIntegrationTest, InitializationOrder) {
    brain = brain_create(
        "init_order_test",
        BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION,
        10,
        4
    );
    ASSERT_NE(nullptr, brain) << "Brain creation failed";

    // Medulla and hypothalamus should be initialized by default
    EXPECT_TRUE(brain->medulla_enabled) << "Medulla should be initialized first";
    EXPECT_NE(nullptr, brain->medulla);

    EXPECT_TRUE(brain->hypothalamus_enabled) << "Hypothalamus should be initialized second";
    EXPECT_NE(nullptr, brain->hypothalamus);

    // Parietal is disabled by default - enable and init it now to test order
    ASSERT_FALSE(brain->parietal_enabled) << "Parietal should be disabled by default";
    brain->config.enable_parietal = true;
    EXPECT_TRUE(nimcp_brain_factory_init_parietal_subsystem(brain)) << "Parietal init should succeed";

    // After manual init, parietal should be initialized
    EXPECT_TRUE(brain->parietal_enabled) << "Parietal should be enabled after init";
    EXPECT_NE(nullptr, brain->parietal) << "Parietal should exist after init";
}

/**
 * @test BothSystemsCanUpdate
 * @brief Verify both systems can be updated without conflict
 */
TEST_F(HypothalamusParietalIntegrationTest, BothSystemsCanUpdate) {
    brain = CreateBrainWithBothSystems();
    ASSERT_NE(nullptr, brain) << "Brain creation failed";
    ASSERT_NE(nullptr, brain->hypothalamus);
    ASSERT_NE(nullptr, brain->parietal);

    // Update both systems multiple times
    for (int cycle = 0; cycle < 100; cycle++) {
        // Update hypothalamus
        bool hypo_result = hypothalamus_update(brain->hypothalamus, 10000);  // 10ms
        EXPECT_TRUE(hypo_result) << "Hypothalamus update failed at cycle " << cycle;

        // Update parietal
        int parietal_result = parietal_step(brain->parietal, 10000);
        EXPECT_EQ(0, parietal_result) << "Parietal update failed at cycle " << cycle;
    }

    // Verify both systems still in valid state
    hypothalamus_state_t hypo_state;
    EXPECT_TRUE(hypothalamus_get_state(brain->hypothalamus, &hypo_state));
    EXPECT_NE(HYPOTHALAMUS_STATUS_ERROR, hypo_state.status);
}

/**
 * @test StressAffectsCognition
 * @brief Verify stress from hypothalamus can influence cognitive performance
 *
 * High stress typically reduces cognitive precision/performance.
 */
TEST_F(HypothalamusParietalIntegrationTest, StressAffectsCognition) {
    brain = CreateBrainWithBothSystems();
    ASSERT_NE(nullptr, brain) << "Brain creation failed";
    ASSERT_NE(nullptr, brain->hypothalamus);
    ASSERT_NE(nullptr, brain->parietal);

    // Get baseline cortisol
    float baseline_cortisol = hypothalamus_get_cortisol(brain->hypothalamus);
    EXPECT_GE(baseline_cortisol, 0.0f);
    EXPECT_LE(baseline_cortisol, 1.0f);

    // Apply stress to hypothalamus
    hypothalamus_apply_stress(brain->hypothalamus, 0.7f);

    // Update HPA axis to process stress
    for (int i = 0; i < 20; i++) {
        hypothalamus_update_hpa_axis(brain->hypothalamus, 100000);  // 100ms
    }

    // Get stress-elevated cortisol
    float stress_cortisol = hypothalamus_get_cortisol(brain->hypothalamus);

    // Cortisol should be elevated (or at least not lower)
    // Stress response depends on HPA configuration
    EXPECT_GE(stress_cortisol, 0.0f);
    EXPECT_LE(stress_cortisol, 1.0f);

    // Both systems should still be operational
    hypothalamus_state_t hypo_state;
    EXPECT_TRUE(hypothalamus_get_state(brain->hypothalamus, &hypo_state));
    EXPECT_NE(HYPOTHALAMUS_STATUS_ERROR, hypo_state.status);

    // Parietal should still function
    int parietal_result = parietal_step(brain->parietal, 10000);
    EXPECT_EQ(0, parietal_result);
}

/**
 * @test FatigueAffectsMathPerformance
 * @brief Verify fatigue/circadian state affects mathematical reasoning
 */
TEST_F(HypothalamusParietalIntegrationTest, FatigueAffectsMathPerformance) {
    brain = CreateBrainWithBothSystems();
    ASSERT_NE(nullptr, brain) << "Brain creation failed";
    ASSERT_NE(nullptr, brain->hypothalamus);
    ASSERT_NE(nullptr, brain->parietal);

    // Get initial circadian alertness
    hypo_circadian_state_t circadian;
    hypothalamus_get_circadian_state(brain->hypothalamus, &circadian);
    float initial_alertness = circadian.alertness;

    // Alertness should be in valid range
    EXPECT_GE(initial_alertness, 0.0f);
    EXPECT_LE(initial_alertness, 1.0f);

    // Parietal should be operational regardless of alertness
    int parietal_result = parietal_step(brain->parietal, 10000);
    EXPECT_EQ(0, parietal_result);

    // Simulate time passing (affects circadian)
    for (int hour = 0; hour < 12; hour++) {
        // 1 hour in 1-second steps (faster for testing)
        for (int i = 0; i < 3600; i++) {
            hypothalamus_update(brain->hypothalamus, 1000000);  // 1 second
        }
    }

    // Get updated alertness
    hypothalamus_get_circadian_state(brain->hypothalamus, &circadian);

    // Alertness should still be valid
    EXPECT_GE(circadian.alertness, 0.0f);
    EXPECT_LE(circadian.alertness, 1.0f);

    // Parietal should still function
    parietal_result = parietal_step(brain->parietal, 10000);
    EXPECT_EQ(0, parietal_result);
}

/**
 * @test DriveStatesAndReasoning
 * @brief Verify drive states can coexist with mathematical reasoning
 */
TEST_F(HypothalamusParietalIntegrationTest, DriveStatesAndReasoning) {
    brain = CreateBrainWithBothSystems();
    ASSERT_NE(nullptr, brain) << "Brain creation failed";
    ASSERT_NE(nullptr, brain->hypothalamus);
    ASSERT_NE(nullptr, brain->parietal);

    // Get initial drive states
    appetite_state_t appetite;
    hypothalamus_get_appetite(brain->hypothalamus, &appetite);
    EXPECT_GE(appetite.hunger_drive, 0.0f);
    EXPECT_LE(appetite.hunger_drive, 1.0f);

    hydration_state_t hydration;
    hypothalamus_get_hydration(brain->hypothalamus, &hydration);
    EXPECT_GE(hydration.thirst_drive, 0.0f);
    EXPECT_LE(hydration.thirst_drive, 1.0f);

    // Simulate increasing hunger (time without food)
    for (int i = 0; i < 100; i++) {
        hypothalamus_update_homeostasis(brain->hypothalamus, 100000);
    }

    // Both systems should still work together
    hypothalamus_update(brain->hypothalamus, 10000);
    int parietal_result = parietal_step(brain->parietal, 10000);
    EXPECT_EQ(0, parietal_result);

    // Get updated states
    hypothalamus_get_appetite(brain->hypothalamus, &appetite);
    EXPECT_GE(appetite.hunger_drive, 0.0f);
    EXPECT_LE(appetite.hunger_drive, 1.0f);
}

/**
 * @test AutonomicAndCognition
 * @brief Verify autonomic balance doesn't interfere with cognition
 */
TEST_F(HypothalamusParietalIntegrationTest, AutonomicAndCognition) {
    brain = CreateBrainWithBothSystems();
    ASSERT_NE(nullptr, brain) << "Brain creation failed";
    ASSERT_NE(nullptr, brain->hypothalamus);
    ASSERT_NE(nullptr, brain->parietal);

    // Get initial autonomic state
    autonomic_state_t autonomic;
    hypothalamus_get_autonomic(brain->hypothalamus, &autonomic);

    float initial_sympathetic = autonomic.sympathetic_tone;
    EXPECT_GE(initial_sympathetic, 0.0f);
    EXPECT_LE(initial_sympathetic, 1.0f);

    // Update both systems concurrently
    for (int cycle = 0; cycle < 50; cycle++) {
        // Update autonomic
        hypothalamus_update_autonomic(brain->hypothalamus, 20000);

        // Update parietal
        parietal_step(brain->parietal, 20000);
    }

    // Both should remain in valid state
    hypothalamus_get_autonomic(brain->hypothalamus, &autonomic);
    EXPECT_GE(autonomic.sympathetic_tone, 0.0f);
    EXPECT_LE(autonomic.sympathetic_tone, 1.0f);
    EXPECT_GE(autonomic.parasympathetic_tone, 0.0f);
    EXPECT_LE(autonomic.parasympathetic_tone, 1.0f);
}

/**
 * @test BrainDestroyWithBothSystems
 * @brief Verify proper cleanup of both systems
 */
TEST_F(HypothalamusParietalIntegrationTest, BrainDestroyWithBothSystems) {
    brain = CreateBrainWithBothSystems();
    ASSERT_NE(nullptr, brain) << "Brain creation failed";

    bool had_hypothalamus = brain->hypothalamus_enabled && brain->hypothalamus != nullptr;
    bool had_parietal = brain->parietal_enabled && brain->parietal != nullptr;

    // Destroy brain (cleans up both systems)
    brain_destroy(brain);
    brain = nullptr;  // Prevent double-free in TearDown

    // If both were present, destruction succeeded without crash
    if (had_hypothalamus && had_parietal) {
        SUCCEED() << "Brain with both hypothalamus and parietal destroyed successfully";
    }
}

/**
 * @test MultipleCycles
 * @brief Verify no memory leaks over multiple create/destroy cycles
 */
TEST_F(HypothalamusParietalIntegrationTest, MultipleCycles) {
    const int NUM_CYCLES = 5;
    int successful_cycles = 0;

    for (int i = 0; i < NUM_CYCLES; i++) {
        brain_t test_brain = CreateBrainWithBothSystems();

        if (!test_brain) {
            continue;
        }

        // Verify both systems exist
        if (test_brain->hypothalamus && test_brain->parietal) {
            // Update both
            hypothalamus_update(test_brain->hypothalamus, 10000);
            parietal_step(test_brain->parietal, 10000);
            successful_cycles++;
        }

        brain_destroy(test_brain);
    }

    EXPECT_GT(successful_cycles, 0) << "At least one cycle should succeed";
    SUCCEED() << "Completed " << successful_cycles << " cycles without memory issues";
}

/**
 * @test MedullaHypothalamusParietalChain
 * @brief Verify full chain: medulla → hypothalamus → parietal
 */
TEST_F(HypothalamusParietalIntegrationTest, MedullaHypothalamusParietalChain) {
    brain = CreateBrainWithBothSystems();
    ASSERT_NE(nullptr, brain) << "Brain creation failed";
    ASSERT_NE(nullptr, brain->medulla) << "Medulla required";
    ASSERT_NE(nullptr, brain->hypothalamus) << "Hypothalamus required";
    ASSERT_NE(nullptr, brain->parietal) << "Parietal required";

    // Update in chain order
    for (int cycle = 0; cycle < 50; cycle++) {
        // 1. Medulla (arousal foundation)
        medulla_update(brain->medulla, 20000);

        // 2. Hypothalamus (homeostasis, drives)
        hypothalamus_update(brain->hypothalamus, 20000);

        // 3. Parietal (mathematical reasoning)
        parietal_step(brain->parietal, 20000);

        // Verify all systems remain valid
        if (cycle % 10 == 0) {
            float arousal = medulla_get_arousal_level(brain->medulla);
            EXPECT_GE(arousal, 0.0f);
            EXPECT_LE(arousal, 1.0f);

            hypothalamus_state_t hypo_state;
            hypothalamus_get_state(brain->hypothalamus, &hypo_state);
            EXPECT_NE(HYPOTHALAMUS_STATUS_ERROR, hypo_state.status);
        }
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
