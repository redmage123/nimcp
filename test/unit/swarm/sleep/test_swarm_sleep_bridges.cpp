/**
 * @file test_swarm_sleep_bridges.cpp
 * @brief Unit tests for all 10 Swarm Sleep Bridge modules
 * @version 1.0.0
 * @date 2025-12-18
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/nimcp_sleep_wake.h"
#include "utils/platform/nimcp_platform.h"
#include "utils/thread/nimcp_thread.h"

//=============================================================================
// Mock Sleep System Implementation
//=============================================================================

typedef struct mock_sleep_system {
    sleep_state_t current_state;
    float sleep_pressure;
    sleep_state_callback_t callback;
    void* callback_data;
} mock_sleep_system_t;

static mock_sleep_system_t g_mock_sleep = {SLEEP_STATE_AWAKE, 0.0f, nullptr, nullptr};

// Headers have their own extern "C" guards

sleep_state_t sleep_get_current_state(sleep_system_t sys) {
    (void)sys;
    return g_mock_sleep.current_state;
}

float sleep_get_pressure(sleep_system_t sys) {
    (void)sys;
    return g_mock_sleep.sleep_pressure;
}

bool sleep_register_state_callback(sleep_system_t sys, sleep_state_callback_t cb, void* data) {
    (void)sys;
    g_mock_sleep.callback = cb;
    g_mock_sleep.callback_data = data;
    return true;
}

bool sleep_unregister_state_callback(sleep_system_t sys, sleep_state_callback_t cb, void* data) {
    (void)sys; (void)cb; (void)data;
    g_mock_sleep.callback = nullptr;
    g_mock_sleep.callback_data = nullptr;
    return true;
}


static void trigger_sleep_state_change(sleep_state_t state) {
    g_mock_sleep.current_state = state;
    if (g_mock_sleep.callback) {
        g_mock_sleep.callback(state, g_mock_sleep.callback_data);
    }
}

//=============================================================================
// Swarm Brain Sleep Bridge Tests
//=============================================================================

// Headers have their own extern "C" guards
#include "swarm/sleep/nimcp_swarm_brain_sleep_bridge.h"

class SwarmBrainSleepBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_sleep.current_state = SLEEP_STATE_AWAKE;
        g_mock_sleep.callback = nullptr;
    }
};

TEST_F(SwarmBrainSleepBridgeTest, DefaultConfigInitializesCorrectly) {
    swarm_brain_sleep_config_t config;
    int result = swarm_brain_sleep_default_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_coord_modulation);
}

TEST_F(SwarmBrainSleepBridgeTest, CreateAndDestroy) {
    // Re-enabled: mock sleep system provides adequate infrastructure
    swarm_brain_sleep_config_t config;
    swarm_brain_sleep_default_config(&config);
    swarm_brain_sleep_bridge_t bridge = swarm_brain_sleep_bridge_create(&config, (sleep_system_t)1);
    ASSERT_NE(bridge, nullptr);
    swarm_brain_sleep_bridge_destroy(bridge);
}

TEST_F(SwarmBrainSleepBridgeTest, CoordFactorVariesWithSleep) {
    float awake = swarm_brain_sleep_get_coord_factor(SLEEP_STATE_AWAKE);
    float deep = swarm_brain_sleep_get_coord_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(awake, SWARM_BRAIN_SLEEP_COORD_AWAKE);
    EXPECT_LT(deep, awake);
}

TEST_F(SwarmBrainSleepBridgeTest, GetEffectsWorks) {
    swarm_brain_sleep_config_t config;
    swarm_brain_sleep_default_config(&config);
    swarm_brain_sleep_bridge_t bridge = swarm_brain_sleep_bridge_create(&config, (sleep_system_t)1);
    ASSERT_NE(bridge, nullptr);

    swarm_brain_sleep_effects_t effects;
    int result = swarm_brain_sleep_get_effects(bridge, &effects);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(effects.current_state, SLEEP_STATE_AWAKE);

    swarm_brain_sleep_bridge_destroy(bridge);
}

//=============================================================================
// Swarm Consciousness Sleep Bridge Tests
//=============================================================================

// Headers have their own extern "C" guards
#include "swarm/sleep/nimcp_swarm_consciousness_sleep_bridge.h"

class SwarmConsciousnessSleepBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_sleep.current_state = SLEEP_STATE_AWAKE;
        g_mock_sleep.callback = nullptr;
    }
};

TEST_F(SwarmConsciousnessSleepBridgeTest, CreateAndDestroy) {
    swarm_consciousness_sleep_config_t config;
    swarm_consciousness_sleep_default_config(&config);
    swarm_consciousness_sleep_bridge_t bridge = swarm_consciousness_sleep_bridge_create(&config, (sleep_system_t)1);
    EXPECT_NE(bridge, nullptr);
    swarm_consciousness_sleep_bridge_destroy(bridge);
}

TEST_F(SwarmConsciousnessSleepBridgeTest, PhiFactorVariesWithSleep) {
    float awake = swarm_consciousness_sleep_get_phi_factor(SLEEP_STATE_AWAKE);
    float deep = swarm_consciousness_sleep_get_phi_factor(SLEEP_STATE_DEEP_NREM);
    float rem = swarm_consciousness_sleep_get_phi_factor(SLEEP_STATE_REM);
    EXPECT_FLOAT_EQ(awake, SWARM_CONSCIOUSNESS_SLEEP_PHI_AWAKE);
    EXPECT_LT(deep, awake);
    EXPECT_GT(rem, deep);  // REM has more consciousness than deep sleep
}

//=============================================================================
// Bidirectional Consciousness → Sleep Tests
//=============================================================================

TEST_F(SwarmConsciousnessSleepBridgeTest, ConnectAndDisconnectConsciousness) {
    swarm_consciousness_sleep_config_t config;
    swarm_consciousness_sleep_default_config(&config);
    swarm_consciousness_sleep_bridge_t bridge = swarm_consciousness_sleep_bridge_create(&config, (sleep_system_t)1);
    ASSERT_NE(bridge, nullptr);

    // Connect consciousness (using NULL as mock - function handles it)
    int result = swarm_consciousness_sleep_connect_consciousness(bridge, nullptr);
    EXPECT_EQ(result, 0);

    // Disconnect
    swarm_consciousness_sleep_disconnect_consciousness(bridge);

    swarm_consciousness_sleep_bridge_destroy(bridge);
}

TEST_F(SwarmConsciousnessSleepBridgeTest, PressureModifierVariesWithConsciousnessState) {
    // DORMANT state increases sleep pressure
    float dormant_mod = swarm_consciousness_sleep_get_pressure_modifier(0);  // DORMANT
    EXPECT_FLOAT_EQ(dormant_mod, SWARM_CONSCIOUSNESS_TO_SLEEP_DORMANT);
    EXPECT_GT(dormant_mod, 1.0f);  // Increases pressure

    // EMERGING state is neutral
    float emerging_mod = swarm_consciousness_sleep_get_pressure_modifier(1);  // EMERGING
    EXPECT_FLOAT_EQ(emerging_mod, SWARM_CONSCIOUSNESS_TO_SLEEP_EMERGING);
    EXPECT_FLOAT_EQ(emerging_mod, 1.0f);  // Neutral

    // UNIFIED state reduces sleep pressure
    float unified_mod = swarm_consciousness_sleep_get_pressure_modifier(2);  // UNIFIED
    EXPECT_FLOAT_EQ(unified_mod, SWARM_CONSCIOUSNESS_TO_SLEEP_UNIFIED);
    EXPECT_LT(unified_mod, 1.0f);  // Reduces pressure

    // TRANSCENDENT state strongly reduces sleep pressure
    float transcendent_mod = swarm_consciousness_sleep_get_pressure_modifier(3);  // TRANSCENDENT
    EXPECT_FLOAT_EQ(transcendent_mod, SWARM_CONSCIOUSNESS_TO_SLEEP_TRANSCENDENT);
    EXPECT_LT(transcendent_mod, unified_mod);  // Even more reduction
}

TEST_F(SwarmConsciousnessSleepBridgeTest, ConsciousnessChangeUpdatesModulation) {
    swarm_consciousness_sleep_config_t config;
    swarm_consciousness_sleep_default_config(&config);
    swarm_consciousness_sleep_bridge_t bridge = swarm_consciousness_sleep_bridge_create(&config, (sleep_system_t)1);
    ASSERT_NE(bridge, nullptr);

    // Simulate consciousness state change
    int result = swarm_consciousness_sleep_on_consciousness_change(bridge, 3, 8.0f);  // TRANSCENDENT, high phi
    EXPECT_EQ(result, 0);

    // Get modulation values
    swarm_sleep_consciousness_modulation_t modulation;
    result = swarm_consciousness_sleep_get_consciousness_modulation(bridge, &modulation);
    EXPECT_EQ(result, 0);

    // Verify modulation was applied
    EXPECT_EQ(modulation.consciousness_state, 3u);  // TRANSCENDENT
    EXPECT_FLOAT_EQ(modulation.collective_phi, 8.0f);
    EXPECT_FLOAT_EQ(modulation.sleep_pressure_modifier, SWARM_CONSCIOUSNESS_TO_SLEEP_TRANSCENDENT);
    EXPECT_TRUE(modulation.suppress_sleep_transition);  // Transcendent blocks sleep

    swarm_consciousness_sleep_bridge_destroy(bridge);
}

TEST_F(SwarmConsciousnessSleepBridgeTest, TranscendentConsciousnessBlocksSleepTransition) {
    swarm_consciousness_sleep_config_t config;
    swarm_consciousness_sleep_default_config(&config);
    swarm_consciousness_sleep_bridge_t bridge = swarm_consciousness_sleep_bridge_create(&config, (sleep_system_t)1);
    ASSERT_NE(bridge, nullptr);

    // Initially should not block
    bool blocks = swarm_consciousness_sleep_blocks_transition(bridge);
    EXPECT_FALSE(blocks);

    // Set to TRANSCENDENT state
    swarm_consciousness_sleep_on_consciousness_change(bridge, 3, 10.0f);

    // Now should block
    blocks = swarm_consciousness_sleep_blocks_transition(bridge);
    EXPECT_TRUE(blocks);

    // Set back to DORMANT
    swarm_consciousness_sleep_on_consciousness_change(bridge, 0, 0.1f);

    // Should no longer block
    blocks = swarm_consciousness_sleep_blocks_transition(bridge);
    EXPECT_FALSE(blocks);

    swarm_consciousness_sleep_bridge_destroy(bridge);
}

TEST_F(SwarmConsciousnessSleepBridgeTest, WakefulnessBoostScalesWithPhi) {
    swarm_consciousness_sleep_config_t config;
    swarm_consciousness_sleep_default_config(&config);
    swarm_consciousness_sleep_bridge_t bridge = swarm_consciousness_sleep_bridge_create(&config, (sleep_system_t)1);
    ASSERT_NE(bridge, nullptr);

    // Low phi should have low wakefulness boost
    swarm_consciousness_sleep_on_consciousness_change(bridge, 2, 1.0f);  // UNIFIED, low phi
    swarm_sleep_consciousness_modulation_t mod_low;
    swarm_consciousness_sleep_get_consciousness_modulation(bridge, &mod_low);

    // High phi should have higher wakefulness boost
    swarm_consciousness_sleep_on_consciousness_change(bridge, 2, 10.0f);  // UNIFIED, high phi
    swarm_sleep_consciousness_modulation_t mod_high;
    swarm_consciousness_sleep_get_consciousness_modulation(bridge, &mod_high);

    EXPECT_GT(mod_high.wakefulness_boost, mod_low.wakefulness_boost);

    swarm_consciousness_sleep_bridge_destroy(bridge);
}

TEST_F(SwarmConsciousnessSleepBridgeTest, NullBridgeHandledGracefully) {
    // All bidirectional functions should handle null gracefully
    EXPECT_EQ(swarm_consciousness_sleep_connect_consciousness(nullptr, nullptr), -1);
    swarm_consciousness_sleep_disconnect_consciousness(nullptr);  // Should not crash
    EXPECT_EQ(swarm_consciousness_sleep_on_consciousness_change(nullptr, 0, 0.0f), -1);
    swarm_sleep_consciousness_modulation_t mod;
    EXPECT_EQ(swarm_consciousness_sleep_get_consciousness_modulation(nullptr, &mod), -1);
    EXPECT_FALSE(swarm_consciousness_sleep_blocks_transition(nullptr));
}

//=============================================================================
// Swarm Consensus Sleep Bridge Tests
//=============================================================================

// Headers have their own extern "C" guards
#include "swarm/sleep/nimcp_swarm_consensus_sleep_bridge.h"

class SwarmConsensusSleepBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_sleep.current_state = SLEEP_STATE_AWAKE;
        g_mock_sleep.callback = nullptr;
    }
};

TEST_F(SwarmConsensusSleepBridgeTest, CreateAndDestroy) {
    swarm_consensus_sleep_config_t config;
    swarm_consensus_sleep_default_config(&config);
    swarm_consensus_sleep_bridge_t bridge = swarm_consensus_sleep_bridge_create(&config, (sleep_system_t)1);
    EXPECT_NE(bridge, nullptr);
    swarm_consensus_sleep_bridge_destroy(bridge);
}

TEST_F(SwarmConsensusSleepBridgeTest, VoteFactorVariesWithSleep) {
    float awake = swarm_consensus_sleep_get_vote_factor(SLEEP_STATE_AWAKE);
    float deep = swarm_consensus_sleep_get_vote_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_LT(deep, awake);
}

//=============================================================================
// Swarm Emergence Sleep Bridge Tests
//=============================================================================

// Headers have their own extern "C" guards
#include "swarm/sleep/nimcp_swarm_emergence_sleep_bridge.h"

class SwarmEmergenceSleepBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_sleep.current_state = SLEEP_STATE_AWAKE;
        g_mock_sleep.callback = nullptr;
    }
};

TEST_F(SwarmEmergenceSleepBridgeTest, CreateAndDestroy) {
    swarm_emergence_sleep_config_t config;
    swarm_emergence_sleep_default_config(&config);
    swarm_emergence_sleep_bridge_t bridge = swarm_emergence_sleep_bridge_create(&config, (sleep_system_t)1);
    EXPECT_NE(bridge, nullptr);
    swarm_emergence_sleep_bridge_destroy(bridge);
}

TEST_F(SwarmEmergenceSleepBridgeTest, TransFactorVariesWithSleep) {
    float awake = swarm_emergence_sleep_get_trans_factor(SLEEP_STATE_AWAKE);
    float deep = swarm_emergence_sleep_get_trans_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_LT(deep, awake);
}

//=============================================================================
// Swarm Flocking Sleep Bridge Tests
//=============================================================================

// Headers have their own extern "C" guards
#include "swarm/sleep/nimcp_swarm_flocking_sleep_bridge.h"

class SwarmFlockingSleepBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_sleep.current_state = SLEEP_STATE_AWAKE;
        g_mock_sleep.callback = nullptr;
    }
};

TEST_F(SwarmFlockingSleepBridgeTest, CreateAndDestroy) {
    swarm_flocking_sleep_config_t config;
    swarm_flocking_sleep_default_config(&config);
    swarm_flocking_sleep_bridge_t bridge = swarm_flocking_sleep_bridge_create(&config, (sleep_system_t)1);
    EXPECT_NE(bridge, nullptr);
    swarm_flocking_sleep_bridge_destroy(bridge);
}

TEST_F(SwarmFlockingSleepBridgeTest, ForceFactorVariesWithSleep) {
    float awake = swarm_flocking_sleep_get_force_factor(SLEEP_STATE_AWAKE);
    float deep = swarm_flocking_sleep_get_force_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(awake, SWARM_FLOCKING_SLEEP_FORCE_AWAKE);
    EXPECT_LT(deep, awake);
}

//=============================================================================
// Swarm Immune Sleep Bridge Tests
//=============================================================================

// Headers have their own extern "C" guards
#include "swarm/sleep/nimcp_swarm_immune_sleep_bridge.h"

class SwarmImmuneSleepBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_sleep.current_state = SLEEP_STATE_AWAKE;
        g_mock_sleep.callback = nullptr;
    }
};

TEST_F(SwarmImmuneSleepBridgeTest, CreateAndDestroy) {
    swarm_immune_sleep_config_t config;
    swarm_immune_sleep_default_config(&config);
    swarm_immune_sleep_bridge_t bridge = swarm_immune_sleep_bridge_create(&config, (sleep_system_t)1);
    EXPECT_NE(bridge, nullptr);
    swarm_immune_sleep_bridge_destroy(bridge);
}

TEST_F(SwarmImmuneSleepBridgeTest, DetectFactorVariesWithSleep) {
    float awake = swarm_immune_sleep_get_detect_factor(SLEEP_STATE_AWAKE);
    float deep = swarm_immune_sleep_get_detect_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_LT(deep, awake);
}

TEST_F(SwarmImmuneSleepBridgeTest, MemoryFactorEnhancedDuringSleep) {
    float awake = swarm_immune_sleep_get_memory_factor(SLEEP_STATE_AWAKE);
    float deep = swarm_immune_sleep_get_memory_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_GT(deep, awake);  // Memory consolidation enhanced during sleep
}

//=============================================================================
// Swarm Memory Sleep Bridge Tests
//=============================================================================

// Headers have their own extern "C" guards
#include "swarm/sleep/nimcp_swarm_memory_sleep_bridge.h"

class SwarmMemorySleepBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_sleep.current_state = SLEEP_STATE_AWAKE;
        g_mock_sleep.callback = nullptr;
    }
};

TEST_F(SwarmMemorySleepBridgeTest, CreateAndDestroy) {
    swarm_memory_sleep_config_t config;
    swarm_memory_sleep_default_config(&config);
    swarm_memory_sleep_bridge_t bridge = swarm_memory_sleep_bridge_create(&config, (sleep_system_t)1);
    EXPECT_NE(bridge, nullptr);
    swarm_memory_sleep_bridge_destroy(bridge);
}

TEST_F(SwarmMemorySleepBridgeTest, ConsolFactorEnhancedDuringSleep) {
    float awake = swarm_memory_sleep_get_consol_factor(SLEEP_STATE_AWAKE);
    float deep = swarm_memory_sleep_get_consol_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_GT(deep, awake);  // Consolidation peaks during deep sleep
}

TEST_F(SwarmMemorySleepBridgeTest, ReplayFactorHighDuringREM) {
    float rem = swarm_memory_sleep_get_replay_factor(SLEEP_STATE_REM);
    EXPECT_GE(rem, SWARM_MEMORY_SLEEP_REPLAY_AWAKE);
}

//=============================================================================
// Swarm Pheromone Sleep Bridge Tests
//=============================================================================

// Headers have their own extern "C" guards
#include "swarm/sleep/nimcp_swarm_pheromone_sleep_bridge.h"

class SwarmPheromoneSleepBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_sleep.current_state = SLEEP_STATE_AWAKE;
        g_mock_sleep.callback = nullptr;
    }
};

TEST_F(SwarmPheromoneSleepBridgeTest, CreateAndDestroy) {
    swarm_pheromone_sleep_config_t config;
    swarm_pheromone_sleep_default_config(&config);
    swarm_pheromone_sleep_bridge_t bridge = swarm_pheromone_sleep_bridge_create(&config, (sleep_system_t)1);
    EXPECT_NE(bridge, nullptr);
    swarm_pheromone_sleep_bridge_destroy(bridge);
}

TEST_F(SwarmPheromoneSleepBridgeTest, DecayFactorVariesWithSleep) {
    float awake = swarm_pheromone_sleep_get_decay_factor(SLEEP_STATE_AWAKE);
    float deep = swarm_pheromone_sleep_get_decay_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_NE(awake, deep);
}

//=============================================================================
// Swarm Quorum Sleep Bridge Tests
//=============================================================================

// Headers have their own extern "C" guards
#include "swarm/sleep/nimcp_swarm_quorum_sleep_bridge.h"

class SwarmQuorumSleepBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_sleep.current_state = SLEEP_STATE_AWAKE;
        g_mock_sleep.callback = nullptr;
    }
};

TEST_F(SwarmQuorumSleepBridgeTest, CreateAndDestroy) {
    swarm_quorum_sleep_config_t config;
    swarm_quorum_sleep_default_config(&config);
    swarm_quorum_sleep_bridge_t bridge = swarm_quorum_sleep_bridge_create(&config, (sleep_system_t)1);
    EXPECT_NE(bridge, nullptr);
    swarm_quorum_sleep_bridge_destroy(bridge);
}

TEST_F(SwarmQuorumSleepBridgeTest, ThreshFactorPresent) {
    float awake = swarm_quorum_sleep_get_thresh_factor(SLEEP_STATE_AWAKE);
    EXPECT_FLOAT_EQ(awake, SWARM_QUORUM_SLEEP_THRESH_AWAKE);
}

TEST_F(SwarmQuorumSleepBridgeTest, CommitFactorReducedDuringSleep) {
    float awake = swarm_quorum_sleep_get_commit_factor(SLEEP_STATE_AWAKE);
    float deep = swarm_quorum_sleep_get_commit_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_LT(deep, awake);
}

//=============================================================================
// Swarm Signal Sleep Bridge Tests
//=============================================================================

// Headers have their own extern "C" guards
#include "swarm/sleep/nimcp_swarm_signal_sleep_bridge.h"

class SwarmSignalSleepBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_sleep.current_state = SLEEP_STATE_AWAKE;
        g_mock_sleep.callback = nullptr;
    }
};

TEST_F(SwarmSignalSleepBridgeTest, CreateAndDestroy) {
    swarm_signal_sleep_config_t config;
    swarm_signal_sleep_default_config(&config);
    swarm_signal_sleep_bridge_t bridge = swarm_signal_sleep_bridge_create(&config, (sleep_system_t)1);
    EXPECT_NE(bridge, nullptr);
    swarm_signal_sleep_bridge_destroy(bridge);
}

TEST_F(SwarmSignalSleepBridgeTest, PowerFactorVariesWithSleep) {
    float awake = swarm_signal_sleep_get_power_factor(SLEEP_STATE_AWAKE);
    float deep = swarm_signal_sleep_get_power_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_FLOAT_EQ(awake, SWARM_SIGNAL_SLEEP_POWER_AWAKE);
    EXPECT_LT(deep, awake);
}

TEST_F(SwarmSignalSleepBridgeTest, RecvFactorReducedDuringSleep) {
    float awake = swarm_signal_sleep_get_recv_factor(SLEEP_STATE_AWAKE);
    float light = swarm_signal_sleep_get_recv_factor(SLEEP_STATE_LIGHT_NREM);
    EXPECT_LT(light, awake);
}

TEST_F(SwarmSignalSleepBridgeTest, LatencyFactorIncreasedDuringSleep) {
    float awake = swarm_signal_sleep_get_latency_factor(SLEEP_STATE_AWAKE);
    float deep = swarm_signal_sleep_get_latency_factor(SLEEP_STATE_DEEP_NREM);
    EXPECT_GT(deep, awake);  // More latency tolerance during sleep
}

//=============================================================================
// Cross-Bridge Integration Tests
//=============================================================================

class SwarmSleepBridgeCrossIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_sleep.current_state = SLEEP_STATE_AWAKE;
        g_mock_sleep.callback = nullptr;
    }
};

TEST_F(SwarmSleepBridgeCrossIntegrationTest, AllBridgesCanBeCreated) {
    swarm_brain_sleep_config_t brain_cfg;
    swarm_brain_sleep_default_config(&brain_cfg);
    swarm_brain_sleep_bridge_t brain = swarm_brain_sleep_bridge_create(&brain_cfg, (sleep_system_t)1);

    swarm_consciousness_sleep_config_t cons_cfg;
    swarm_consciousness_sleep_default_config(&cons_cfg);
    swarm_consciousness_sleep_bridge_t cons = swarm_consciousness_sleep_bridge_create(&cons_cfg, (sleep_system_t)1);

    swarm_memory_sleep_config_t mem_cfg;
    swarm_memory_sleep_default_config(&mem_cfg);
    swarm_memory_sleep_bridge_t mem = swarm_memory_sleep_bridge_create(&mem_cfg, (sleep_system_t)1);

    EXPECT_NE(brain, nullptr);
    EXPECT_NE(cons, nullptr);
    EXPECT_NE(mem, nullptr);

    swarm_brain_sleep_bridge_destroy(brain);
    swarm_consciousness_sleep_bridge_destroy(cons);
    swarm_memory_sleep_bridge_destroy(mem);
}

TEST_F(SwarmSleepBridgeCrossIntegrationTest, StateTransitionsUpdateEffects) {
    swarm_memory_sleep_config_t config;
    swarm_memory_sleep_default_config(&config);
    swarm_memory_sleep_bridge_t bridge = swarm_memory_sleep_bridge_create(&config, (sleep_system_t)1);
    ASSERT_NE(bridge, nullptr);

    swarm_memory_sleep_effects_t effects;

    // Test full sleep cycle
    sleep_state_t cycle[] = {
        SLEEP_STATE_AWAKE,
        SLEEP_STATE_DROWSY,
        SLEEP_STATE_LIGHT_NREM,
        SLEEP_STATE_DEEP_NREM,
        SLEEP_STATE_REM,
        SLEEP_STATE_AWAKE
    };

    for (int i = 0; i < 6; i++) {
        trigger_sleep_state_change(cycle[i]);
        swarm_memory_sleep_update(bridge);
        swarm_memory_sleep_get_effects(bridge, &effects);
        EXPECT_EQ(effects.current_state, cycle[i]);
    }

    swarm_memory_sleep_bridge_destroy(bridge);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
