/**
 * @file e2e_test_bridge_pipelines.cpp
 * @brief End-to-end tests for swarm sleep and immune bridge pipelines
 * @version 1.0.0
 * @date 2025-12-18
 *
 * Tests the complete pipeline behavior of sleep and immune bridges
 * using the static factor functions (no mock conflicts).
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>

// Headers have their own extern "C" guards
// Sleep bridges
#include "swarm/sleep/nimcp_swarm_signal_sleep_bridge.h"
#include "swarm/sleep/nimcp_swarm_consensus_sleep_bridge.h"
#include "swarm/sleep/nimcp_swarm_memory_sleep_bridge.h"
#include "swarm/sleep/nimcp_swarm_flocking_sleep_bridge.h"
#include "swarm/sleep/nimcp_swarm_emergence_sleep_bridge.h"
#include "swarm/sleep/nimcp_swarm_consciousness_sleep_bridge.h"
#include "swarm/sleep/nimcp_swarm_pheromone_sleep_bridge.h"
#include "swarm/sleep/nimcp_swarm_quorum_sleep_bridge.h"
#include "swarm/sleep/nimcp_swarm_immune_sleep_bridge.h"
#include "swarm/sleep/nimcp_swarm_brain_sleep_bridge.h"

// Immune bridges
#include "swarm/immune/nimcp_swarm_signal_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_consensus_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_memory_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_flocking_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_emergence_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_consciousness_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_pheromone_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_quorum_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_immune_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_brain_immune_bridge.h"

/* =============================================================================
 * E2E Sleep Cycle Tests
 *
 * Test complete sleep cycles using static factor functions.
 * ============================================================================= */

class E2ESleepCycleTest : public ::testing::Test {
protected:
    std::vector<sleep_state_t> get_standard_sleep_cycle() {
        return {
            SLEEP_STATE_AWAKE,
            SLEEP_STATE_DROWSY,
            SLEEP_STATE_LIGHT_NREM,
            SLEEP_STATE_DEEP_NREM,
            SLEEP_STATE_LIGHT_NREM,
            SLEEP_STATE_REM,
            SLEEP_STATE_LIGHT_NREM,
            SLEEP_STATE_DEEP_NREM,
            SLEEP_STATE_LIGHT_NREM,
            SLEEP_STATE_REM,
            SLEEP_STATE_AWAKE
        };
    }
};

TEST_F(E2ESleepCycleTest, CompleteCycleProducesExpectedPattern) {
    auto cycle = get_standard_sleep_cycle();

    // Track factors through the cycle
    std::vector<float> signal_factors;
    std::vector<float> memory_factors;
    std::vector<float> consciousness_factors;

    for (auto state : cycle) {
        signal_factors.push_back(swarm_signal_sleep_get_power_factor(state));
        memory_factors.push_back(swarm_memory_sleep_get_consol_factor(state));
        consciousness_factors.push_back(swarm_consciousness_sleep_get_phi_factor(state));
    }

    // Start and end awake with baseline values
    EXPECT_FLOAT_EQ(1.0f, signal_factors.front());
    EXPECT_FLOAT_EQ(1.0f, signal_factors.back());

    // Memory consolidation should be enhanced somewhere in cycle
    float max_memory = *std::max_element(memory_factors.begin(), memory_factors.end());
    EXPECT_GT(max_memory, 1.0f);

    // Consciousness should dip during deep NREM
    float min_consciousness = *std::min_element(consciousness_factors.begin(),
                                                 consciousness_factors.end());
    EXPECT_LT(min_consciousness, 1.0f);
}

TEST_F(E2ESleepCycleTest, AllModulesRespondToSleepStates) {
    auto cycle = get_standard_sleep_cycle();

    for (auto state : cycle) {
        // All modules should produce valid factors
        EXPECT_GE(swarm_signal_sleep_get_power_factor(state), 0.0f);
        EXPECT_LE(swarm_signal_sleep_get_power_factor(state), 2.0f);

        EXPECT_GE(swarm_consensus_sleep_get_vote_factor(state), 0.0f);
        EXPECT_LE(swarm_consensus_sleep_get_vote_factor(state), 2.0f);

        EXPECT_GE(swarm_memory_sleep_get_consol_factor(state), 0.0f);
        EXPECT_LE(swarm_memory_sleep_get_consol_factor(state), 3.0f);

        EXPECT_GE(swarm_flocking_sleep_get_force_factor(state), 0.0f);
        EXPECT_LE(swarm_flocking_sleep_get_force_factor(state), 2.0f);

        EXPECT_GE(swarm_emergence_sleep_get_trans_factor(state), 0.0f);
        EXPECT_LE(swarm_emergence_sleep_get_trans_factor(state), 2.0f);

        EXPECT_GE(swarm_consciousness_sleep_get_phi_factor(state), 0.0f);
        EXPECT_LE(swarm_consciousness_sleep_get_phi_factor(state), 1.0f);

        EXPECT_GE(swarm_pheromone_sleep_get_decay_factor(state), 0.0f);
        EXPECT_LE(swarm_pheromone_sleep_get_decay_factor(state), 3.0f);

        EXPECT_GE(swarm_quorum_sleep_get_thresh_factor(state), 0.0f);
        EXPECT_LE(swarm_quorum_sleep_get_thresh_factor(state), 3.0f);

        EXPECT_GE(swarm_immune_sleep_get_detect_factor(state), 0.0f);
        EXPECT_LE(swarm_immune_sleep_get_detect_factor(state), 2.0f);

        EXPECT_GE(swarm_brain_sleep_get_coord_factor(state), 0.0f);
        EXPECT_LE(swarm_brain_sleep_get_coord_factor(state), 2.0f);
    }
}

TEST_F(E2ESleepCycleTest, REMStateProducesIntermediateValues) {
    // REM should be a unique state with intermediate characteristics
    float rem_signal = swarm_signal_sleep_get_power_factor(SLEEP_STATE_REM);
    float rem_memory = swarm_memory_sleep_get_replay_factor(SLEEP_STATE_REM);
    float rem_phi = swarm_consciousness_sleep_get_phi_factor(SLEEP_STATE_REM);

    float deep_signal = swarm_signal_sleep_get_power_factor(SLEEP_STATE_DEEP_NREM);
    float deep_memory = swarm_memory_sleep_get_replay_factor(SLEEP_STATE_DEEP_NREM);
    float deep_phi = swarm_consciousness_sleep_get_phi_factor(SLEEP_STATE_DEEP_NREM);

    // REM should have more activity than deep NREM
    EXPECT_GE(rem_signal, deep_signal);
    EXPECT_GE(rem_phi, deep_phi);

    // Memory replay should be active in both REM and deep NREM
    EXPECT_GT(rem_memory, 1.0f);
    EXPECT_GT(deep_memory, 1.0f);
}

/* =============================================================================
 * E2E Module Coordination Tests
 * ============================================================================= */

class E2EModuleCoordinationTest : public ::testing::Test {};

TEST_F(E2EModuleCoordinationTest, CommunicationModulesCoordinate) {
    // All communication-related modules should reduce together during sleep
    float awake_signal = swarm_signal_sleep_get_power_factor(SLEEP_STATE_AWAKE);
    float awake_brain = swarm_brain_sleep_get_coord_factor(SLEEP_STATE_AWAKE);
    float awake_consensus = swarm_consensus_sleep_get_vote_factor(SLEEP_STATE_AWAKE);

    float deep_signal = swarm_signal_sleep_get_power_factor(SLEEP_STATE_DEEP_NREM);
    float deep_brain = swarm_brain_sleep_get_coord_factor(SLEEP_STATE_DEEP_NREM);
    float deep_consensus = swarm_consensus_sleep_get_vote_factor(SLEEP_STATE_DEEP_NREM);

    // All should be reduced during deep sleep
    EXPECT_LT(deep_signal, awake_signal);
    EXPECT_LT(deep_brain, awake_brain);
    EXPECT_LT(deep_consensus, awake_consensus);
}

TEST_F(E2EModuleCoordinationTest, MemoryAndImmuneCoordinate) {
    // Memory and immune systems both consolidate during sleep
    float awake_memory = swarm_memory_sleep_get_consol_factor(SLEEP_STATE_AWAKE);
    float awake_immune = swarm_immune_sleep_get_memory_factor(SLEEP_STATE_AWAKE);

    float deep_memory = swarm_memory_sleep_get_consol_factor(SLEEP_STATE_DEEP_NREM);
    float deep_immune = swarm_immune_sleep_get_memory_factor(SLEEP_STATE_DEEP_NREM);

    // Both should be enhanced during deep sleep
    EXPECT_GT(deep_memory, awake_memory);
    EXPECT_GT(deep_immune, awake_immune);
}

TEST_F(E2EModuleCoordinationTest, EmergenceAndConsciousnessCoordinate) {
    // Both should reduce during deep sleep
    float awake_emergence = swarm_emergence_sleep_get_trans_factor(SLEEP_STATE_AWAKE);
    float awake_phi = swarm_consciousness_sleep_get_phi_factor(SLEEP_STATE_AWAKE);

    float deep_emergence = swarm_emergence_sleep_get_trans_factor(SLEEP_STATE_DEEP_NREM);
    float deep_phi = swarm_consciousness_sleep_get_phi_factor(SLEEP_STATE_DEEP_NREM);

    EXPECT_LT(deep_emergence, awake_emergence);
    EXPECT_LT(deep_phi, awake_phi);
}

/* =============================================================================
 * E2E Immune Bridge Config Pipeline Tests
 * ============================================================================= */

class E2EImmuneConfigTest : public ::testing::Test {};

TEST_F(E2EImmuneConfigTest, AllModulesConfigureCorrectly) {
    // All immune bridge configs should initialize with sensible defaults
    swarm_signal_immune_config_t signal_config;
    swarm_consensus_immune_config_t consensus_config;
    swarm_memory_immune_config_t memory_config;
    swarm_flocking_immune_config_t flocking_config;
    swarm_emergence_immune_config_t emergence_config;
    swarm_consciousness_immune_config_t consciousness_config;
    swarm_pheromone_immune_config_t pheromone_config;
    swarm_quorum_immune_config_t quorum_config;
    swarm_immune_immune_config_t immune_config;
    swarm_brain_immune_config_t brain_config;

    EXPECT_EQ(0, swarm_signal_immune_default_config(&signal_config));
    EXPECT_EQ(0, swarm_consensus_immune_default_config(&consensus_config));
    EXPECT_EQ(0, swarm_memory_immune_default_config(&memory_config));
    EXPECT_EQ(0, swarm_flocking_immune_default_config(&flocking_config));
    EXPECT_EQ(0, swarm_emergence_immune_default_config(&emergence_config));
    EXPECT_EQ(0, swarm_consciousness_immune_default_config(&consciousness_config));
    EXPECT_EQ(0, swarm_pheromone_immune_default_config(&pheromone_config));
    EXPECT_EQ(0, swarm_quorum_immune_default_config(&quorum_config));
    EXPECT_EQ(0, swarm_immune_immune_default_config(&immune_config));
    EXPECT_EQ(0, swarm_brain_immune_default_config(&brain_config));

    // All configs should enable effects by default
    EXPECT_TRUE(signal_config.enable_cytokine_effects);
    EXPECT_TRUE(consensus_config.enable_cytokine_effects);
    EXPECT_TRUE(memory_config.enable_cytokine_effects);
    EXPECT_TRUE(flocking_config.enable_cytokine_effects);
    EXPECT_TRUE(emergence_config.enable_cytokine_effects);
    EXPECT_TRUE(consciousness_config.enable_cytokine_effects);
    EXPECT_TRUE(pheromone_config.enable_cytokine_effects);
    EXPECT_TRUE(quorum_config.enable_cytokine_effects);
}

TEST_F(E2EImmuneConfigTest, NullBridgesReturnSafeDefaults) {
    // All getters should return safe defaults for null bridges
    EXPECT_FLOAT_EQ(1.0f, swarm_signal_immune_get_quality_factor(nullptr));
    EXPECT_FLOAT_EQ(1.0f, swarm_consensus_immune_get_quorum_factor(nullptr));
    EXPECT_FLOAT_EQ(1.0f, swarm_memory_immune_get_capacity_factor(nullptr));
    EXPECT_FLOAT_EQ(1.0f, swarm_flocking_immune_get_cohesion_factor(nullptr));
    EXPECT_FLOAT_EQ(1.0f, swarm_emergence_immune_get_emergence_factor(nullptr));
    EXPECT_FLOAT_EQ(1.0f, swarm_consciousness_immune_get_phi_factor(nullptr));
    EXPECT_FLOAT_EQ(1.0f, swarm_pheromone_immune_get_sensing_factor(nullptr));
    EXPECT_FLOAT_EQ(1.0f, swarm_quorum_immune_get_threshold_factor(nullptr));
    EXPECT_FLOAT_EQ(1.0f, swarm_immune_immune_get_detection_factor(nullptr));
    EXPECT_FLOAT_EQ(1.0f, swarm_brain_immune_get_coherence_factor(nullptr));
}

/* =============================================================================
 * E2E Combined Pipeline Tests
 * ============================================================================= */

TEST(E2ECombinedPipeline, SleepAndImmuneModulesCoexist) {
    // Sleep configs
    swarm_signal_sleep_config_t sleep_config;
    swarm_signal_sleep_default_config(&sleep_config);

    // Immune configs
    swarm_signal_immune_config_t immune_config;
    swarm_signal_immune_default_config(&immune_config);

    // Both should configure correctly
    EXPECT_TRUE(sleep_config.enable_power_modulation);
    EXPECT_TRUE(immune_config.enable_cytokine_effects);

    // Sleep factors should work
    EXPECT_FLOAT_EQ(1.0f, swarm_signal_sleep_get_power_factor(SLEEP_STATE_AWAKE));
    EXPECT_LT(swarm_signal_sleep_get_power_factor(SLEEP_STATE_DEEP_NREM), 1.0f);

    // Immune null handling should work
    EXPECT_FLOAT_EQ(1.0f, swarm_signal_immune_get_quality_factor(nullptr));
}

TEST(E2ECombinedPipeline, AllTenSleepModulesHaveFactorFunctions) {
    // Verify all 10 sleep bridge modules have working factor functions
    sleep_state_t test_state = SLEEP_STATE_DEEP_NREM;

    // All should return non-baseline values for deep NREM
    EXPECT_NE(1.0f, swarm_signal_sleep_get_power_factor(test_state));
    EXPECT_NE(1.0f, swarm_consensus_sleep_get_vote_factor(test_state));
    EXPECT_NE(1.0f, swarm_memory_sleep_get_consol_factor(test_state));
    EXPECT_NE(1.0f, swarm_flocking_sleep_get_force_factor(test_state));
    EXPECT_NE(1.0f, swarm_emergence_sleep_get_trans_factor(test_state));
    EXPECT_NE(1.0f, swarm_consciousness_sleep_get_phi_factor(test_state));
    EXPECT_NE(1.0f, swarm_pheromone_sleep_get_decay_factor(test_state));
    EXPECT_NE(1.0f, swarm_quorum_sleep_get_thresh_factor(test_state));
    EXPECT_NE(1.0f, swarm_immune_sleep_get_detect_factor(test_state));
    EXPECT_NE(1.0f, swarm_brain_sleep_get_coord_factor(test_state));
}

TEST(E2ECombinedPipeline, AllTenImmuneModulesHaveConfigs) {
    // Verify all 10 immune bridge modules have working config functions
    swarm_signal_immune_config_t c1;
    swarm_consensus_immune_config_t c2;
    swarm_memory_immune_config_t c3;
    swarm_flocking_immune_config_t c4;
    swarm_emergence_immune_config_t c5;
    swarm_consciousness_immune_config_t c6;
    swarm_pheromone_immune_config_t c7;
    swarm_quorum_immune_config_t c8;
    swarm_immune_immune_config_t c9;
    swarm_brain_immune_config_t c10;

    // All should succeed
    EXPECT_EQ(0, swarm_signal_immune_default_config(&c1));
    EXPECT_EQ(0, swarm_consensus_immune_default_config(&c2));
    EXPECT_EQ(0, swarm_memory_immune_default_config(&c3));
    EXPECT_EQ(0, swarm_flocking_immune_default_config(&c4));
    EXPECT_EQ(0, swarm_emergence_immune_default_config(&c5));
    EXPECT_EQ(0, swarm_consciousness_immune_default_config(&c6));
    EXPECT_EQ(0, swarm_pheromone_immune_default_config(&c7));
    EXPECT_EQ(0, swarm_quorum_immune_default_config(&c8));
    EXPECT_EQ(0, swarm_immune_immune_default_config(&c9));
    EXPECT_EQ(0, swarm_brain_immune_default_config(&c10));
}

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
