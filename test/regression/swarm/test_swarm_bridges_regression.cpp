/**
 * @file test_swarm_bridges_regression.cpp
 * @brief Regression tests for swarm sleep and immune bridge modules
 * @version 1.0.0
 * @date 2025-12-18
 *
 * Tests stability and correctness of:
 * - Swarm sleep bridges (10 modules) - static factor functions
 * - Swarm immune bridges (10 modules) - config and null handling
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
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
 * Sleep Bridge Factor Regression Tests
 *
 * Verify that factor functions return consistent values across runs.
 * ============================================================================= */

class SleepFactorRegressionTest : public ::testing::Test {
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

TEST_F(SleepFactorRegressionTest, SignalFactorsAreConsistent) {
    // Run factor computation multiple times, verify consistency
    for (int run = 0; run < 10; run++) {
        for (size_t i = 0; i < NUM_STATES; i++) {
            float power1 = swarm_signal_sleep_get_power_factor(ALL_STATES[i]);
            float power2 = swarm_signal_sleep_get_power_factor(ALL_STATES[i]);
            EXPECT_FLOAT_EQ(power1, power2) << "Run " << run << ", State " << i;

            float recv1 = swarm_signal_sleep_get_recv_factor(ALL_STATES[i]);
            float recv2 = swarm_signal_sleep_get_recv_factor(ALL_STATES[i]);
            EXPECT_FLOAT_EQ(recv1, recv2);
        }
    }
}

TEST_F(SleepFactorRegressionTest, ConsensusFactorsAreConsistent) {
    for (int run = 0; run < 10; run++) {
        for (size_t i = 0; i < NUM_STATES; i++) {
            float vote1 = swarm_consensus_sleep_get_vote_factor(ALL_STATES[i]);
            float vote2 = swarm_consensus_sleep_get_vote_factor(ALL_STATES[i]);
            EXPECT_FLOAT_EQ(vote1, vote2);

            float quorum1 = swarm_consensus_sleep_get_quorum_factor(ALL_STATES[i]);
            float quorum2 = swarm_consensus_sleep_get_quorum_factor(ALL_STATES[i]);
            EXPECT_FLOAT_EQ(quorum1, quorum2);
        }
    }
}

TEST_F(SleepFactorRegressionTest, MemoryFactorsAreConsistent) {
    for (int run = 0; run < 10; run++) {
        for (size_t i = 0; i < NUM_STATES; i++) {
            float consol1 = swarm_memory_sleep_get_consol_factor(ALL_STATES[i]);
            float consol2 = swarm_memory_sleep_get_consol_factor(ALL_STATES[i]);
            EXPECT_FLOAT_EQ(consol1, consol2);

            float replay1 = swarm_memory_sleep_get_replay_factor(ALL_STATES[i]);
            float replay2 = swarm_memory_sleep_get_replay_factor(ALL_STATES[i]);
            EXPECT_FLOAT_EQ(replay1, replay2);
        }
    }
}

TEST_F(SleepFactorRegressionTest, FlockingFactorsAreConsistent) {
    for (int run = 0; run < 10; run++) {
        for (size_t i = 0; i < NUM_STATES; i++) {
            float force1 = swarm_flocking_sleep_get_force_factor(ALL_STATES[i]);
            float force2 = swarm_flocking_sleep_get_force_factor(ALL_STATES[i]);
            EXPECT_FLOAT_EQ(force1, force2);
        }
    }
}

TEST_F(SleepFactorRegressionTest, EmergenceFactorsAreConsistent) {
    for (int run = 0; run < 10; run++) {
        for (size_t i = 0; i < NUM_STATES; i++) {
            float trans1 = swarm_emergence_sleep_get_trans_factor(ALL_STATES[i]);
            float trans2 = swarm_emergence_sleep_get_trans_factor(ALL_STATES[i]);
            EXPECT_FLOAT_EQ(trans1, trans2);
        }
    }
}

TEST_F(SleepFactorRegressionTest, ConsciousnessFactorsAreConsistent) {
    for (int run = 0; run < 10; run++) {
        for (size_t i = 0; i < NUM_STATES; i++) {
            float phi1 = swarm_consciousness_sleep_get_phi_factor(ALL_STATES[i]);
            float phi2 = swarm_consciousness_sleep_get_phi_factor(ALL_STATES[i]);
            EXPECT_FLOAT_EQ(phi1, phi2);
        }
    }
}

TEST_F(SleepFactorRegressionTest, PheromoneFactorsAreConsistent) {
    for (int run = 0; run < 10; run++) {
        for (size_t i = 0; i < NUM_STATES; i++) {
            float decay1 = swarm_pheromone_sleep_get_decay_factor(ALL_STATES[i]);
            float decay2 = swarm_pheromone_sleep_get_decay_factor(ALL_STATES[i]);
            EXPECT_FLOAT_EQ(decay1, decay2);
        }
    }
}

TEST_F(SleepFactorRegressionTest, QuorumFactorsAreConsistent) {
    for (int run = 0; run < 10; run++) {
        for (size_t i = 0; i < NUM_STATES; i++) {
            float thresh1 = swarm_quorum_sleep_get_thresh_factor(ALL_STATES[i]);
            float thresh2 = swarm_quorum_sleep_get_thresh_factor(ALL_STATES[i]);
            EXPECT_FLOAT_EQ(thresh1, thresh2);
        }
    }
}

TEST_F(SleepFactorRegressionTest, ImmuneFactorsAreConsistent) {
    for (int run = 0; run < 10; run++) {
        for (size_t i = 0; i < NUM_STATES; i++) {
            float detect1 = swarm_immune_sleep_get_detect_factor(ALL_STATES[i]);
            float detect2 = swarm_immune_sleep_get_detect_factor(ALL_STATES[i]);
            EXPECT_FLOAT_EQ(detect1, detect2);
        }
    }
}

TEST_F(SleepFactorRegressionTest, BrainFactorsAreConsistent) {
    for (int run = 0; run < 10; run++) {
        for (size_t i = 0; i < NUM_STATES; i++) {
            float coord1 = swarm_brain_sleep_get_coord_factor(ALL_STATES[i]);
            float coord2 = swarm_brain_sleep_get_coord_factor(ALL_STATES[i]);
            EXPECT_FLOAT_EQ(coord1, coord2);
        }
    }
}

/* =============================================================================
 * Sleep Factor Boundary Tests
 * ============================================================================= */

TEST_F(SleepFactorRegressionTest, AllFactorsNonNegative) {
    for (size_t i = 0; i < NUM_STATES; i++) {
        EXPECT_GT(swarm_signal_sleep_get_power_factor(ALL_STATES[i]), 0.0f);
        EXPECT_GT(swarm_signal_sleep_get_recv_factor(ALL_STATES[i]), 0.0f);
        // Vote and activity factors can be 0 during deep sleep (suspended)
        EXPECT_GE(swarm_consensus_sleep_get_vote_factor(ALL_STATES[i]), 0.0f);
        EXPECT_GT(swarm_memory_sleep_get_consol_factor(ALL_STATES[i]), 0.0f);
        EXPECT_GE(swarm_flocking_sleep_get_force_factor(ALL_STATES[i]), 0.0f);
        EXPECT_GE(swarm_emergence_sleep_get_trans_factor(ALL_STATES[i]), 0.0f);
        EXPECT_GE(swarm_consciousness_sleep_get_phi_factor(ALL_STATES[i]), 0.0f);
        EXPECT_GT(swarm_pheromone_sleep_get_decay_factor(ALL_STATES[i]), 0.0f);
        EXPECT_GT(swarm_quorum_sleep_get_thresh_factor(ALL_STATES[i]), 0.0f);
        EXPECT_GT(swarm_immune_sleep_get_detect_factor(ALL_STATES[i]), 0.0f);
        EXPECT_GT(swarm_brain_sleep_get_coord_factor(ALL_STATES[i]), 0.0f);
    }
}

TEST_F(SleepFactorRegressionTest, AwakeStateIsBaseline) {
    // All baseline factors should be 1.0 when awake
    EXPECT_FLOAT_EQ(1.0f, swarm_signal_sleep_get_power_factor(SLEEP_STATE_AWAKE));
    EXPECT_FLOAT_EQ(1.0f, swarm_signal_sleep_get_recv_factor(SLEEP_STATE_AWAKE));
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
 * Immune Bridge Config Regression Tests
 * ============================================================================= */

class ImmuneConfigRegressionTest : public ::testing::Test {};

TEST_F(ImmuneConfigRegressionTest, ConfigsAreConsistent) {
    // Run config initialization multiple times, verify consistency
    for (int run = 0; run < 10; run++) {
        swarm_signal_immune_config_t config1, config2;
        swarm_signal_immune_default_config(&config1);
        swarm_signal_immune_default_config(&config2);

        EXPECT_EQ(config1.enable_cytokine_effects, config2.enable_cytokine_effects);
        EXPECT_EQ(config1.enable_inflammation_effects, config2.enable_inflammation_effects);
        EXPECT_FLOAT_EQ(config1.cytokine_sensitivity, config2.cytokine_sensitivity);
    }
}

TEST_F(ImmuneConfigRegressionTest, AllConfigsHaveDefaults) {
    swarm_signal_immune_config_t signal;
    swarm_consensus_immune_config_t consensus;
    swarm_memory_immune_config_t memory;
    swarm_flocking_immune_config_t flocking;
    swarm_emergence_immune_config_t emergence;
    swarm_consciousness_immune_config_t consciousness;
    swarm_pheromone_immune_config_t pheromone;
    swarm_quorum_immune_config_t quorum;
    swarm_immune_immune_config_t immune;
    swarm_brain_immune_config_t brain;

    EXPECT_EQ(0, swarm_signal_immune_default_config(&signal));
    EXPECT_EQ(0, swarm_consensus_immune_default_config(&consensus));
    EXPECT_EQ(0, swarm_memory_immune_default_config(&memory));
    EXPECT_EQ(0, swarm_flocking_immune_default_config(&flocking));
    EXPECT_EQ(0, swarm_emergence_immune_default_config(&emergence));
    EXPECT_EQ(0, swarm_consciousness_immune_default_config(&consciousness));
    EXPECT_EQ(0, swarm_pheromone_immune_default_config(&pheromone));
    EXPECT_EQ(0, swarm_quorum_immune_default_config(&quorum));
    EXPECT_EQ(0, swarm_immune_immune_default_config(&immune));
    EXPECT_EQ(0, swarm_brain_immune_default_config(&brain));
}

/* =============================================================================
 * Null Safety Regression Tests
 * ============================================================================= */

class NullSafetyRegressionTest : public ::testing::Test {};

TEST_F(NullSafetyRegressionTest, SleepConfigNullHandling) {
    // All config functions should return -1 for null
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

TEST_F(NullSafetyRegressionTest, ImmuneConfigNullHandling) {
    EXPECT_EQ(-1, swarm_signal_immune_default_config(nullptr));
    EXPECT_EQ(-1, swarm_consensus_immune_default_config(nullptr));
    EXPECT_EQ(-1, swarm_memory_immune_default_config(nullptr));
    EXPECT_EQ(-1, swarm_flocking_immune_default_config(nullptr));
    EXPECT_EQ(-1, swarm_emergence_immune_default_config(nullptr));
    EXPECT_EQ(-1, swarm_consciousness_immune_default_config(nullptr));
    EXPECT_EQ(-1, swarm_pheromone_immune_default_config(nullptr));
    EXPECT_EQ(-1, swarm_quorum_immune_default_config(nullptr));
    EXPECT_EQ(-1, swarm_immune_immune_default_config(nullptr));
    EXPECT_EQ(-1, swarm_brain_immune_default_config(nullptr));
}

TEST_F(NullSafetyRegressionTest, ImmuneGetterNullHandling) {
    // All getter functions should return safe defaults for null bridge
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
 * Sleep Cycle Regression Tests
 * ============================================================================= */

TEST(SleepCycleRegression, FullCycleProducesValidFactors) {
    std::vector<sleep_state_t> cycle = {
        SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_REM,
        SLEEP_STATE_AWAKE
    };

    for (auto state : cycle) {
        // All factors must be in valid range
        float power = swarm_signal_sleep_get_power_factor(state);
        EXPECT_GE(power, 0.0f);
        EXPECT_LE(power, 2.0f);

        float consol = swarm_memory_sleep_get_consol_factor(state);
        EXPECT_GE(consol, 0.0f);
        EXPECT_LE(consol, 3.0f);  // Consolidation can be enhanced

        float phi = swarm_consciousness_sleep_get_phi_factor(state);
        EXPECT_GE(phi, 0.0f);
        EXPECT_LE(phi, 1.0f);
    }
}

TEST(SleepCycleRegression, DeepSleepHasMaxConsolidation) {
    std::vector<sleep_state_t> states = {
        SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_REM
    };

    float max_consol = 0.0f;
    sleep_state_t max_state = SLEEP_STATE_AWAKE;

    for (auto state : states) {
        float consol = swarm_memory_sleep_get_consol_factor(state);
        if (consol > max_consol) {
            max_consol = consol;
            max_state = state;
        }
    }

    EXPECT_EQ(SLEEP_STATE_DEEP_NREM, max_state);
}

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
