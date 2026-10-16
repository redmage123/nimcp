/**
 * @file test_swarm_sleep_integration.cpp
 * @brief Integration tests for swarm sleep bridge modules
 * @version 1.0.0
 * @date 2025-12-18
 *
 * Tests cross-module integration between swarm sleep bridges using the
 * static factor functions (which don't require a real sleep system).
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <algorithm>

// Headers have their own extern "C" guards
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

/* =============================================================================
 * Sleep State Factor Tests
 *
 * These tests verify the static factor functions that compute modulation
 * factors based on sleep state. These are pure functions that don't require
 * a real sleep system connection.
 * ============================================================================= */

class SwarmSleepFactorsTest : public ::testing::Test {
protected:
    static constexpr sleep_state_t ALL_STATES[] = {
        SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_REM
    };
    static constexpr size_t NUM_STATES = 5;
};

TEST_F(SwarmSleepFactorsTest, SignalFactorsVaryWithState) {
    float power_factors[NUM_STATES];
    float recv_factors[NUM_STATES];
    float latency_factors[NUM_STATES];

    for (size_t i = 0; i < NUM_STATES; i++) {
        power_factors[i] = swarm_signal_sleep_get_power_factor(ALL_STATES[i]);
        recv_factors[i] = swarm_signal_sleep_get_recv_factor(ALL_STATES[i]);
        latency_factors[i] = swarm_signal_sleep_get_latency_factor(ALL_STATES[i]);

        // All factors should be positive
        EXPECT_GT(power_factors[i], 0.0f) << "State " << i;
        EXPECT_GT(recv_factors[i], 0.0f) << "State " << i;
        EXPECT_GT(latency_factors[i], 0.0f) << "State " << i;
    }

    // Awake should have highest power and reception
    EXPECT_FLOAT_EQ(1.0f, power_factors[0]);  // AWAKE
    EXPECT_FLOAT_EQ(1.0f, recv_factors[0]);   // AWAKE

    // Deep NREM should have lowest power and reception
    EXPECT_LT(power_factors[3], power_factors[0]);  // DEEP_NREM < AWAKE
    EXPECT_LT(recv_factors[3], recv_factors[0]);    // DEEP_NREM < AWAKE

    // Latency tolerance should increase during sleep
    EXPECT_GT(latency_factors[3], latency_factors[0]);  // DEEP_NREM > AWAKE
}

TEST_F(SwarmSleepFactorsTest, ConsensusFactorsVaryWithState) {
    float vote_factors[NUM_STATES];
    float quorum_factors[NUM_STATES];

    for (size_t i = 0; i < NUM_STATES; i++) {
        vote_factors[i] = swarm_consensus_sleep_get_vote_factor(ALL_STATES[i]);
        quorum_factors[i] = swarm_consensus_sleep_get_quorum_factor(ALL_STATES[i]);

        // Vote factors can be 0 during deep sleep (activity suspended)
        EXPECT_GE(vote_factors[i], 0.0f) << "State " << i;
        EXPECT_GE(quorum_factors[i], 0.0f) << "State " << i;
    }

    // Awake should have normal factors
    EXPECT_FLOAT_EQ(1.0f, vote_factors[0]);
    EXPECT_FLOAT_EQ(1.0f, quorum_factors[0]);

    // Consensus should be harder during sleep (higher thresholds or lower voting)
    EXPECT_LE(vote_factors[3], vote_factors[0]);     // DEEP_NREM <= AWAKE
    EXPECT_GE(quorum_factors[3], quorum_factors[0]); // DEEP_NREM >= AWAKE
}

TEST_F(SwarmSleepFactorsTest, MemoryFactorsVaryWithState) {
    float consol_factors[NUM_STATES];
    float replay_factors[NUM_STATES];
    float forget_factors[NUM_STATES];

    for (size_t i = 0; i < NUM_STATES; i++) {
        consol_factors[i] = swarm_memory_sleep_get_consol_factor(ALL_STATES[i]);
        replay_factors[i] = swarm_memory_sleep_get_replay_factor(ALL_STATES[i]);
        forget_factors[i] = swarm_memory_sleep_get_forget_factor(ALL_STATES[i]);

        EXPECT_GT(consol_factors[i], 0.0f) << "State " << i;
        EXPECT_GT(replay_factors[i], 0.0f) << "State " << i;
        EXPECT_GT(forget_factors[i], 0.0f) << "State " << i;
    }

    // Memory consolidation should be enhanced during sleep
    EXPECT_GT(consol_factors[3], consol_factors[0]);  // DEEP_NREM > AWAKE
    EXPECT_GT(replay_factors[3], replay_factors[0]);  // DEEP_NREM > AWAKE
}

TEST_F(SwarmSleepFactorsTest, FlockingFactorsVaryWithState) {
    float force_factors[NUM_STATES];
    float update_factors[NUM_STATES];

    for (size_t i = 0; i < NUM_STATES; i++) {
        force_factors[i] = swarm_flocking_sleep_get_force_factor(ALL_STATES[i]);
        update_factors[i] = swarm_flocking_sleep_get_update_factor(ALL_STATES[i]);

        // Force factors can be 0 during deep sleep (movement suspended)
        EXPECT_GE(force_factors[i], 0.0f) << "State " << i;
        EXPECT_GE(update_factors[i], 0.0f) << "State " << i;
    }

    // Awake should have full flocking behavior
    EXPECT_FLOAT_EQ(1.0f, force_factors[0]);
    EXPECT_FLOAT_EQ(1.0f, update_factors[0]);

    // Flocking should be reduced during sleep
    EXPECT_LE(force_factors[3], force_factors[0]);   // DEEP_NREM <= AWAKE
    EXPECT_LE(update_factors[3], update_factors[0]); // DEEP_NREM <= AWAKE
}

TEST_F(SwarmSleepFactorsTest, EmergenceFactorsVaryWithState) {
    float trans_factors[NUM_STATES];
    float cap_factors[NUM_STATES];

    for (size_t i = 0; i < NUM_STATES; i++) {
        trans_factors[i] = swarm_emergence_sleep_get_trans_factor(ALL_STATES[i]);
        cap_factors[i] = swarm_emergence_sleep_get_cap_factor(ALL_STATES[i]);

        // Transition factors can be 0 during deep sleep (advancement suspended)
        EXPECT_GE(trans_factors[i], 0.0f) << "State " << i;
        EXPECT_GE(cap_factors[i], 0.0f) << "State " << i;
    }

    // Emergence should be suppressed during sleep
    EXPECT_LE(trans_factors[3], trans_factors[0]);  // DEEP_NREM <= AWAKE
    EXPECT_GE(cap_factors[3], cap_factors[0]);      // DEEP_NREM >= AWAKE (harder threshold)
}

TEST_F(SwarmSleepFactorsTest, ConsciousnessFactorsVaryWithState) {
    float phi_factors[NUM_STATES];
    float integration_factors[NUM_STATES];
    float coherence_factors[NUM_STATES];

    for (size_t i = 0; i < NUM_STATES; i++) {
        phi_factors[i] = swarm_consciousness_sleep_get_phi_factor(ALL_STATES[i]);
        integration_factors[i] = swarm_consciousness_sleep_get_integration_factor(ALL_STATES[i]);
        coherence_factors[i] = swarm_consciousness_sleep_get_coherence_factor(ALL_STATES[i]);

        EXPECT_GE(phi_factors[i], 0.0f) << "State " << i;
        EXPECT_GE(integration_factors[i], 0.0f) << "State " << i;
        EXPECT_GE(coherence_factors[i], 0.0f) << "State " << i;
    }

    // Consciousness should be highest when awake
    EXPECT_FLOAT_EQ(1.0f, phi_factors[0]);
    EXPECT_FLOAT_EQ(1.0f, integration_factors[0]);

    // Consciousness should be reduced during deep sleep
    EXPECT_LT(phi_factors[3], phi_factors[0]);  // DEEP_NREM < AWAKE

    // REM should have partial consciousness (dreaming)
    EXPECT_GT(phi_factors[4], 0.0f);  // REM > 0
}

TEST_F(SwarmSleepFactorsTest, PheromoneFactorsVaryWithState) {
    float decay_factors[NUM_STATES];
    float diff_factors[NUM_STATES];
    float detect_factors[NUM_STATES];

    for (size_t i = 0; i < NUM_STATES; i++) {
        decay_factors[i] = swarm_pheromone_sleep_get_decay_factor(ALL_STATES[i]);
        diff_factors[i] = swarm_pheromone_sleep_get_diff_factor(ALL_STATES[i]);
        detect_factors[i] = swarm_pheromone_sleep_get_detect_factor(ALL_STATES[i]);

        EXPECT_GT(decay_factors[i], 0.0f) << "State " << i;
        EXPECT_GT(diff_factors[i], 0.0f) << "State " << i;
        EXPECT_GT(detect_factors[i], 0.0f) << "State " << i;
    }

    // During sleep, pheromone decay should decrease (trails persist longer)
    EXPECT_LT(decay_factors[3], decay_factors[0]);  // DEEP_NREM < AWAKE

    // Detection threshold should increase (harder to sense)
    EXPECT_GT(detect_factors[3], detect_factors[0]);  // DEEP_NREM > AWAKE
}

TEST_F(SwarmSleepFactorsTest, QuorumFactorsVaryWithState) {
    float thresh_factors[NUM_STATES];
    float decay_factors[NUM_STATES];
    float commit_factors[NUM_STATES];

    for (size_t i = 0; i < NUM_STATES; i++) {
        thresh_factors[i] = swarm_quorum_sleep_get_thresh_factor(ALL_STATES[i]);
        decay_factors[i] = swarm_quorum_sleep_get_decay_factor(ALL_STATES[i]);
        commit_factors[i] = swarm_quorum_sleep_get_commit_factor(ALL_STATES[i]);

        EXPECT_GT(thresh_factors[i], 0.0f) << "State " << i;
        EXPECT_GT(decay_factors[i], 0.0f) << "State " << i;
        EXPECT_GT(commit_factors[i], 0.0f) << "State " << i;
    }

    // Quorum threshold should increase during sleep (harder to decide)
    EXPECT_GT(thresh_factors[3], thresh_factors[0]);  // DEEP_NREM > AWAKE

    // Commitment rate should decrease
    EXPECT_LT(commit_factors[3], commit_factors[0]);  // DEEP_NREM < AWAKE
}

TEST_F(SwarmSleepFactorsTest, ImmuneFactorsVaryWithState) {
    float detect_factors[NUM_STATES];
    float response_factors[NUM_STATES];
    float memory_factors[NUM_STATES];

    for (size_t i = 0; i < NUM_STATES; i++) {
        detect_factors[i] = swarm_immune_sleep_get_detect_factor(ALL_STATES[i]);
        response_factors[i] = swarm_immune_sleep_get_response_factor(ALL_STATES[i]);
        memory_factors[i] = swarm_immune_sleep_get_memory_factor(ALL_STATES[i]);

        EXPECT_GT(detect_factors[i], 0.0f) << "State " << i;
        EXPECT_GT(response_factors[i], 0.0f) << "State " << i;
        EXPECT_GT(memory_factors[i], 0.0f) << "State " << i;
    }

    // During sleep, immune sensitivity may be reduced (conservation)
    EXPECT_LE(detect_factors[3], detect_factors[0]);  // DEEP_NREM <= AWAKE

    // But immune memory consolidation should be enhanced
    EXPECT_GT(memory_factors[3], memory_factors[0]);  // DEEP_NREM > AWAKE
}

TEST_F(SwarmSleepFactorsTest, BrainFactorsVaryWithState) {
    float coord_factors[NUM_STATES];
    float heartbeat_factors[NUM_STATES];
    float coherence_factors[NUM_STATES];

    for (size_t i = 0; i < NUM_STATES; i++) {
        coord_factors[i] = swarm_brain_sleep_get_coord_factor(ALL_STATES[i]);
        heartbeat_factors[i] = swarm_brain_sleep_get_heartbeat_factor(ALL_STATES[i]);
        coherence_factors[i] = swarm_brain_sleep_get_coherence_factor(ALL_STATES[i]);

        EXPECT_GT(coord_factors[i], 0.0f) << "State " << i;
        EXPECT_GT(heartbeat_factors[i], 0.0f) << "State " << i;
        EXPECT_GT(coherence_factors[i], 0.0f) << "State " << i;
    }

    // Coordination should be reduced during sleep
    EXPECT_LT(coord_factors[3], coord_factors[0]);  // DEEP_NREM < AWAKE

    // Heartbeat interval should increase (slower heartbeat)
    EXPECT_GT(heartbeat_factors[3], heartbeat_factors[0]);  // DEEP_NREM > AWAKE
}

/* =============================================================================
 * Complete Sleep Cycle Tests
 * ============================================================================= */

class SwarmSleepCycleTest : public ::testing::Test {
protected:
    std::vector<sleep_state_t> get_sleep_cycle() {
        return {
            SLEEP_STATE_AWAKE,
            SLEEP_STATE_DROWSY,
            SLEEP_STATE_LIGHT_NREM,
            SLEEP_STATE_DEEP_NREM,
            SLEEP_STATE_LIGHT_NREM,
            SLEEP_STATE_REM,
            SLEEP_STATE_AWAKE
        };
    }
};

TEST_F(SwarmSleepCycleTest, MemoryConsolidationPeaksDuringDeepSleep) {
    auto cycle = get_sleep_cycle();
    float max_consolidation = 0.0f;
    sleep_state_t max_state = SLEEP_STATE_AWAKE;

    for (auto state : cycle) {
        float consol = swarm_memory_sleep_get_consol_factor(state);
        if (consol > max_consolidation) {
            max_consolidation = consol;
            max_state = state;
        }
    }

    // Memory consolidation should peak during deep NREM
    EXPECT_EQ(SLEEP_STATE_DEEP_NREM, max_state);
    EXPECT_GT(max_consolidation, 1.0f);
}

TEST_F(SwarmSleepCycleTest, ConsciousnessFadesAndReturns) {
    auto cycle = get_sleep_cycle();

    // Start awake with full consciousness
    float start_phi = swarm_consciousness_sleep_get_phi_factor(cycle.front());
    EXPECT_FLOAT_EQ(1.0f, start_phi);

    // Find minimum consciousness during cycle
    float min_phi = 1.0f;
    for (auto state : cycle) {
        float phi = swarm_consciousness_sleep_get_phi_factor(state);
        min_phi = std::min(min_phi, phi);
    }

    // Should drop below awake level
    EXPECT_LT(min_phi, start_phi);

    // End awake with full consciousness
    float end_phi = swarm_consciousness_sleep_get_phi_factor(cycle.back());
    EXPECT_FLOAT_EQ(1.0f, end_phi);
}

TEST_F(SwarmSleepCycleTest, AllModulesReturnToBaselineOnWake) {
    // All factors should return to 1.0 (baseline) when awake
    EXPECT_FLOAT_EQ(1.0f, swarm_signal_sleep_get_power_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(1.0f, swarm_consensus_sleep_get_vote_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(1.0f, swarm_memory_sleep_get_consol_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(1.0f, swarm_flocking_sleep_get_force_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(1.0f, swarm_emergence_sleep_get_trans_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(1.0f, swarm_consciousness_sleep_get_phi_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(1.0f, swarm_pheromone_sleep_get_decay_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(1.0f, swarm_quorum_sleep_get_thresh_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(1.0f, swarm_immune_sleep_get_detect_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(1.0f, swarm_brain_sleep_get_coord_factor(SLEEP_STATE_AWAKE));
}

/* =============================================================================
 * Cross-Module Coordination Tests
 * ============================================================================= */

TEST(SwarmSleepCoordination, CommunicationReductionDuringSleep) {
    // All communication-related modules should reduce during deep sleep
    float signal = swarm_signal_sleep_get_power_factor(SLEEP_STATE_DEEP_NREM);
    float consensus = swarm_consensus_sleep_get_vote_factor(SLEEP_STATE_DEEP_NREM);
    float pheromone_decay = swarm_pheromone_sleep_get_decay_factor(SLEEP_STATE_DEEP_NREM);
    float brain_coord = swarm_brain_sleep_get_coord_factor(SLEEP_STATE_DEEP_NREM);

    // Signal power reduced
    EXPECT_LT(signal, 1.0f);

    // Consensus voting reduced or suspended
    EXPECT_LE(consensus, 1.0f);

    // Pheromone decay decreased (trails persist longer during sleep)
    EXPECT_LT(pheromone_decay, 1.0f);

    // Brain coordination reduced
    EXPECT_LT(brain_coord, 1.0f);
}

TEST(SwarmSleepCoordination, ConsolidationEnhancementDuringSleep) {
    // All consolidation/memory modules should enhance during deep sleep
    float memory = swarm_memory_sleep_get_consol_factor(SLEEP_STATE_DEEP_NREM);
    float immune_memory = swarm_immune_sleep_get_memory_factor(SLEEP_STATE_DEEP_NREM);

    // Memory consolidation enhanced
    EXPECT_GT(memory, 1.0f);

    // Immune memory consolidation enhanced
    EXPECT_GT(immune_memory, 1.0f);
}

TEST(SwarmSleepCoordination, EmergenceSuppressionDuringSleep) {
    // Emergence and collective intelligence should be suppressed during sleep
    float emergence = swarm_emergence_sleep_get_trans_factor(SLEEP_STATE_DEEP_NREM);
    float flocking = swarm_flocking_sleep_get_force_factor(SLEEP_STATE_DEEP_NREM);
    float quorum = swarm_quorum_sleep_get_commit_factor(SLEEP_STATE_DEEP_NREM);

    // All should be reduced
    EXPECT_LT(emergence, 1.0f);
    EXPECT_LT(flocking, 1.0f);
    EXPECT_LT(quorum, 1.0f);
}

TEST(SwarmSleepCoordination, REMStateIntermediate) {
    // REM should have intermediate values (dreaming state)
    float consciousness = swarm_consciousness_sleep_get_phi_factor(SLEEP_STATE_REM);
    float memory = swarm_memory_sleep_get_replay_factor(SLEEP_STATE_REM);
    float signal = swarm_signal_sleep_get_power_factor(SLEEP_STATE_REM);

    // Consciousness partially active (dreaming)
    EXPECT_GT(consciousness, 0.0f);
    EXPECT_LT(consciousness, 1.0f);

    // Memory replay active
    EXPECT_GT(memory, 1.0f);

    // Signal partially reduced
    EXPECT_LT(signal, 1.0f);
    EXPECT_GT(signal, 0.3f);  // Not as reduced as deep NREM
}

/* =============================================================================
 * Config Tests
 * ============================================================================= */

class SwarmSleepConfigTest : public ::testing::Test {};

TEST_F(SwarmSleepConfigTest, AllDefaultConfigsSucceed) {
    swarm_signal_sleep_config_t signal_config;
    swarm_consensus_sleep_config_t consensus_config;
    swarm_memory_sleep_config_t memory_config;
    swarm_flocking_sleep_config_t flocking_config;
    swarm_emergence_sleep_config_t emergence_config;
    swarm_consciousness_sleep_config_t consciousness_config;
    swarm_pheromone_sleep_config_t pheromone_config;
    swarm_quorum_sleep_config_t quorum_config;
    swarm_immune_sleep_config_t immune_config;
    swarm_brain_sleep_config_t brain_config;

    EXPECT_EQ(0, swarm_signal_sleep_default_config(&signal_config));
    EXPECT_EQ(0, swarm_consensus_sleep_default_config(&consensus_config));
    EXPECT_EQ(0, swarm_memory_sleep_default_config(&memory_config));
    EXPECT_EQ(0, swarm_flocking_sleep_default_config(&flocking_config));
    EXPECT_EQ(0, swarm_emergence_sleep_default_config(&emergence_config));
    EXPECT_EQ(0, swarm_consciousness_sleep_default_config(&consciousness_config));
    EXPECT_EQ(0, swarm_pheromone_sleep_default_config(&pheromone_config));
    EXPECT_EQ(0, swarm_quorum_sleep_default_config(&quorum_config));
    EXPECT_EQ(0, swarm_immune_sleep_default_config(&immune_config));
    EXPECT_EQ(0, swarm_brain_sleep_default_config(&brain_config));
}

TEST_F(SwarmSleepConfigTest, NullConfigReturnsError) {
    EXPECT_EQ(-1, swarm_signal_sleep_default_config(nullptr));
    EXPECT_EQ(-1, swarm_consensus_sleep_default_config(nullptr));
    EXPECT_EQ(-1, swarm_memory_sleep_default_config(nullptr));
    EXPECT_EQ(-1, swarm_flocking_sleep_default_config(nullptr));
    EXPECT_EQ(-1, swarm_emergence_sleep_default_config(nullptr));
    EXPECT_EQ(-1, swarm_consciousness_sleep_default_config(nullptr));
    EXPECT_EQ(-1, swarm_pheromone_sleep_default_config(nullptr));
    EXPECT_EQ(-1, swarm_quorum_sleep_default_config(nullptr));
    EXPECT_EQ(-1, swarm_immune_sleep_default_config(nullptr));
    EXPECT_EQ(-1, swarm_brain_sleep_default_config(nullptr));
}

/* =============================================================================
 * Bidirectional Consciousness <-> Sleep Integration Tests
 *
 * These tests verify the bidirectional integration where:
 * - Sleep affects consciousness (phi, integration, coherence factors)
 * - Consciousness affects sleep (pressure modulation, wakefulness boost)
 * ============================================================================= */

TEST(SwarmBidirectionalIntegration, ConsciousnessStateAffectsSleepPressure) {
    // DORMANT state should increase sleep pressure (promotes sleep)
    float dormant = swarm_consciousness_sleep_get_pressure_modifier(0);
    EXPECT_GT(dormant, 1.0f);  // > 1.0 means increased sleep pressure

    // EMERGING state should be neutral
    float emerging = swarm_consciousness_sleep_get_pressure_modifier(1);
    EXPECT_FLOAT_EQ(1.0f, emerging);

    // UNIFIED state should reduce sleep pressure (promotes wakefulness)
    float unified = swarm_consciousness_sleep_get_pressure_modifier(2);
    EXPECT_LT(unified, 1.0f);  // < 1.0 means reduced sleep pressure

    // TRANSCENDENT state should strongly reduce sleep pressure
    float transcendent = swarm_consciousness_sleep_get_pressure_modifier(3);
    EXPECT_LT(transcendent, unified);  // Even more wakefulness-promoting
}

TEST(SwarmBidirectionalIntegration, HighConsciousnessPromotesWakefulness) {
    // Higher consciousness states should progressively reduce sleep pressure
    float dormant_pressure = swarm_consciousness_sleep_get_pressure_modifier(0);
    float emerging_pressure = swarm_consciousness_sleep_get_pressure_modifier(1);
    float unified_pressure = swarm_consciousness_sleep_get_pressure_modifier(2);
    float transcendent_pressure = swarm_consciousness_sleep_get_pressure_modifier(3);

    // Should form a decreasing sequence
    EXPECT_GE(dormant_pressure, emerging_pressure);
    EXPECT_GE(emerging_pressure, unified_pressure);
    EXPECT_GE(unified_pressure, transcendent_pressure);
}

TEST(SwarmBidirectionalIntegration, SleepStateMatchesConsciousnessExpectation) {
    // Deep sleep should have minimal consciousness
    float deep_phi = swarm_consciousness_sleep_get_phi_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_LT(deep_phi, 0.1f);  // Near-zero consciousness

    // Awake should have full consciousness
    float awake_phi = swarm_consciousness_sleep_get_phi_factor(SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(1.0f, awake_phi);

    // The ratio should be significant (at least 10:1)
    EXPECT_GT(awake_phi / deep_phi, 10.0f);
}

TEST(SwarmBidirectionalIntegration, BidirectionalConsistency) {
    // When consciousness is low (DORMANT), sleep pressure should be high
    float dormant_sleep_mod = swarm_consciousness_sleep_get_pressure_modifier(0);
    EXPECT_GT(dormant_sleep_mod, 1.0f);  // Promotes sleep

    // When in deep sleep, consciousness should be low
    float deep_sleep_phi = swarm_consciousness_sleep_get_phi_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_LT(deep_sleep_phi, 0.1f);  // Very low consciousness

    // This creates a consistent feedback loop:
    // Low consciousness -> high sleep pressure -> deep sleep -> low consciousness
}

TEST(SwarmBidirectionalIntegration, TranscendentStateBlocksSleep) {
    // The transcendent consciousness state should have the strongest
    // wakefulness-promoting effect
    float transcendent_mod = swarm_consciousness_sleep_get_pressure_modifier(3);
    EXPECT_LE(transcendent_mod, 0.5f);  // Strong reduction in sleep pressure
}

/* =============================================================================
 * Regression Tests for Collective Consciousness Integration
 *
 * These tests ensure the collective consciousness module properly integrates
 * with all its connected modules through defined interfaces.
 * ============================================================================= */

TEST(SwarmConsciousnessRegression, PhiFactorRange) {
    // All phi factors should be in valid range [0, 1]
    for (int state = 0; state < 5; state++) {
        float phi = swarm_consciousness_sleep_get_phi_factor((sleep_state_t)state);
        EXPECT_GE(phi, 0.0f) << "State " << state;
        EXPECT_LE(phi, 1.0f) << "State " << state;
    }
}

TEST(SwarmConsciousnessRegression, IntegrationFactorRange) {
    // All integration factors should be in valid range [0, 1]
    for (int state = 0; state < 5; state++) {
        float factor = swarm_consciousness_sleep_get_integration_factor((sleep_state_t)state);
        EXPECT_GE(factor, 0.0f) << "State " << state;
        EXPECT_LE(factor, 1.0f) << "State " << state;
    }
}

TEST(SwarmConsciousnessRegression, CoherenceFactorRange) {
    // All coherence factors should be in valid range [0, 1]
    for (int state = 0; state < 5; state++) {
        float factor = swarm_consciousness_sleep_get_coherence_factor((sleep_state_t)state);
        EXPECT_GE(factor, 0.0f) << "State " << state;
        EXPECT_LE(factor, 1.0f) << "State " << state;
    }
}

TEST(SwarmConsciousnessRegression, PressureModifierRange) {
    // Pressure modifiers should be positive (> 0)
    // Range varies by consciousness state
    for (int state = 0; state <= 3; state++) {
        float mod = swarm_consciousness_sleep_get_pressure_modifier((uint32_t)state);
        EXPECT_GT(mod, 0.0f) << "State " << state;
        EXPECT_LT(mod, 10.0f) << "State " << state;  // Reasonable upper bound
    }
}

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
