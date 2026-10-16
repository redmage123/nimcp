/**
 * @file test_swarm_immune_bridges.cpp
 * @brief Unit tests for Swarm Immune Bridge default configs
 * @version 1.0.0
 * @date 2025-12-18
 *
 * Tests the default config functions for all 9 Swarm Immune Bridges.
 *
 * @author NIMCP Development Team
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_brain_immune.h"
#include "swarm/immune/nimcp_swarm_brain_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_consensus_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_signal_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_memory_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_flocking_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_emergence_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_consciousness_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_pheromone_immune_bridge.h"
#include "swarm/immune/nimcp_swarm_quorum_immune_bridge.h"

//=============================================================================
// Swarm Brain Immune Bridge Tests
//=============================================================================

class SwarmBrainImmuneBridgeTest : public ::testing::Test {};

TEST_F(SwarmBrainImmuneBridgeTest, DefaultConfigInitializesCorrectly) {
    swarm_brain_immune_config_t config;
    int result = swarm_brain_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_impairment);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

TEST_F(SwarmBrainImmuneBridgeTest, DefaultConfigNullReturnsError) {
    int result = swarm_brain_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Swarm Consensus Immune Bridge Tests
//=============================================================================

class SwarmConsensusImmuneBridgeTest : public ::testing::Test {};

TEST_F(SwarmConsensusImmuneBridgeTest, DefaultConfigInitializesCorrectly) {
    swarm_consensus_immune_config_t config;
    int result = swarm_consensus_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

TEST_F(SwarmConsensusImmuneBridgeTest, DefaultConfigNullReturnsError) {
    int result = swarm_consensus_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Swarm Signal Immune Bridge Tests
//=============================================================================

class SwarmSignalImmuneBridgeTest : public ::testing::Test {};

TEST_F(SwarmSignalImmuneBridgeTest, DefaultConfigInitializesCorrectly) {
    swarm_signal_immune_config_t config;
    int result = swarm_signal_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

TEST_F(SwarmSignalImmuneBridgeTest, DefaultConfigNullReturnsError) {
    int result = swarm_signal_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Swarm Memory Immune Bridge Tests
//=============================================================================

class SwarmMemoryImmuneBridgeTest : public ::testing::Test {};

TEST_F(SwarmMemoryImmuneBridgeTest, DefaultConfigInitializesCorrectly) {
    swarm_memory_immune_config_t config;
    int result = swarm_memory_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

TEST_F(SwarmMemoryImmuneBridgeTest, DefaultConfigNullReturnsError) {
    int result = swarm_memory_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Swarm Flocking Immune Bridge Tests
//=============================================================================

class SwarmFlockingImmuneBridgeTest : public ::testing::Test {};

TEST_F(SwarmFlockingImmuneBridgeTest, DefaultConfigInitializesCorrectly) {
    swarm_flocking_immune_config_t config;
    int result = swarm_flocking_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

TEST_F(SwarmFlockingImmuneBridgeTest, DefaultConfigNullReturnsError) {
    int result = swarm_flocking_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Swarm Emergence Immune Bridge Tests
//=============================================================================

class SwarmEmergenceImmuneBridgeTest : public ::testing::Test {};

TEST_F(SwarmEmergenceImmuneBridgeTest, DefaultConfigInitializesCorrectly) {
    swarm_emergence_immune_config_t config;
    int result = swarm_emergence_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

TEST_F(SwarmEmergenceImmuneBridgeTest, DefaultConfigNullReturnsError) {
    int result = swarm_emergence_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Swarm Consciousness Immune Bridge Tests
//=============================================================================

class SwarmConsciousnessImmuneBridgeTest : public ::testing::Test {};

TEST_F(SwarmConsciousnessImmuneBridgeTest, DefaultConfigInitializesCorrectly) {
    swarm_consciousness_immune_config_t config;
    int result = swarm_consciousness_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

TEST_F(SwarmConsciousnessImmuneBridgeTest, DefaultConfigNullReturnsError) {
    int result = swarm_consciousness_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Swarm Pheromone Immune Bridge Tests
//=============================================================================

class SwarmPheromoneImmuneBridgeTest : public ::testing::Test {};

TEST_F(SwarmPheromoneImmuneBridgeTest, DefaultConfigInitializesCorrectly) {
    swarm_pheromone_immune_config_t config;
    int result = swarm_pheromone_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

TEST_F(SwarmPheromoneImmuneBridgeTest, DefaultConfigNullReturnsError) {
    int result = swarm_pheromone_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Swarm Quorum Immune Bridge Tests
//=============================================================================

class SwarmQuorumImmuneBridgeTest : public ::testing::Test {};

TEST_F(SwarmQuorumImmuneBridgeTest, DefaultConfigInitializesCorrectly) {
    swarm_quorum_immune_config_t config;
    int result = swarm_quorum_immune_default_config(&config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

TEST_F(SwarmQuorumImmuneBridgeTest, DefaultConfigNullReturnsError) {
    int result = swarm_quorum_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Cross-Bridge Tests
//=============================================================================

class SwarmImmuneBridgeCrossIntegrationTest : public ::testing::Test {};

TEST_F(SwarmImmuneBridgeCrossIntegrationTest, AllDefaultConfigsSucceed) {
    swarm_brain_immune_config_t brain_cfg;
    swarm_consensus_immune_config_t consensus_cfg;
    swarm_signal_immune_config_t signal_cfg;
    swarm_memory_immune_config_t memory_cfg;
    swarm_flocking_immune_config_t flocking_cfg;
    swarm_emergence_immune_config_t emergence_cfg;
    swarm_consciousness_immune_config_t consciousness_cfg;
    swarm_pheromone_immune_config_t pheromone_cfg;
    swarm_quorum_immune_config_t quorum_cfg;

    EXPECT_EQ(swarm_brain_immune_default_config(&brain_cfg), 0);
    EXPECT_EQ(swarm_consensus_immune_default_config(&consensus_cfg), 0);
    EXPECT_EQ(swarm_signal_immune_default_config(&signal_cfg), 0);
    EXPECT_EQ(swarm_memory_immune_default_config(&memory_cfg), 0);
    EXPECT_EQ(swarm_flocking_immune_default_config(&flocking_cfg), 0);
    EXPECT_EQ(swarm_emergence_immune_default_config(&emergence_cfg), 0);
    EXPECT_EQ(swarm_consciousness_immune_default_config(&consciousness_cfg), 0);
    EXPECT_EQ(swarm_pheromone_immune_default_config(&pheromone_cfg), 0);
    EXPECT_EQ(swarm_quorum_immune_default_config(&quorum_cfg), 0);
}

TEST_F(SwarmImmuneBridgeCrossIntegrationTest, AllDefaultConfigsRejectNull) {
    EXPECT_EQ(swarm_brain_immune_default_config(nullptr), -1);
    EXPECT_EQ(swarm_consensus_immune_default_config(nullptr), -1);
    EXPECT_EQ(swarm_signal_immune_default_config(nullptr), -1);
    EXPECT_EQ(swarm_memory_immune_default_config(nullptr), -1);
    EXPECT_EQ(swarm_flocking_immune_default_config(nullptr), -1);
    EXPECT_EQ(swarm_emergence_immune_default_config(nullptr), -1);
    EXPECT_EQ(swarm_consciousness_immune_default_config(nullptr), -1);
    EXPECT_EQ(swarm_pheromone_immune_default_config(nullptr), -1);
    EXPECT_EQ(swarm_quorum_immune_default_config(nullptr), -1);
}

// Total: 20 tests covering all 9 bridges + cross-integration
