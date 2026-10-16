/**
 * @file test_swarm_immune_integration.cpp
 * @brief Integration tests for swarm immune bridge modules
 * @version 1.0.0
 * @date 2025-12-18
 *
 * Tests configuration and null handling for all swarm immune bridges.
 * Note: Full integration tests with mock immune systems require separate
 * mock library linkage to avoid symbol conflicts.
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
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
 * Config Tests
 *
 * Test default configuration functions for all immune bridges.
 * These are pure functions that don't require mock implementations.
 * ============================================================================= */

class SwarmImmuneConfigTest : public ::testing::Test {};

TEST_F(SwarmImmuneConfigTest, SignalImmuneDefaultConfig) {
    swarm_signal_immune_config_t config;
    EXPECT_EQ(0, swarm_signal_immune_default_config(&config));

    // Verify defaults are sensible
    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_inflammation_effects);
    EXPECT_GT(config.cytokine_sensitivity, 0.0f);
}

TEST_F(SwarmImmuneConfigTest, ConsensusImmuneDefaultConfig) {
    swarm_consensus_immune_config_t config;
    EXPECT_EQ(0, swarm_consensus_immune_default_config(&config));

    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

TEST_F(SwarmImmuneConfigTest, MemoryImmuneDefaultConfig) {
    swarm_memory_immune_config_t config;
    EXPECT_EQ(0, swarm_memory_immune_default_config(&config));

    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

TEST_F(SwarmImmuneConfigTest, FlockingImmuneDefaultConfig) {
    swarm_flocking_immune_config_t config;
    EXPECT_EQ(0, swarm_flocking_immune_default_config(&config));

    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

TEST_F(SwarmImmuneConfigTest, EmergenceImmuneDefaultConfig) {
    swarm_emergence_immune_config_t config;
    EXPECT_EQ(0, swarm_emergence_immune_default_config(&config));

    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

TEST_F(SwarmImmuneConfigTest, ConsciousnessImmuneDefaultConfig) {
    swarm_consciousness_immune_config_t config;
    EXPECT_EQ(0, swarm_consciousness_immune_default_config(&config));

    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

TEST_F(SwarmImmuneConfigTest, PheromoneImmuneDefaultConfig) {
    swarm_pheromone_immune_config_t config;
    EXPECT_EQ(0, swarm_pheromone_immune_default_config(&config));

    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

TEST_F(SwarmImmuneConfigTest, QuorumImmuneDefaultConfig) {
    swarm_quorum_immune_config_t config;
    EXPECT_EQ(0, swarm_quorum_immune_default_config(&config));

    EXPECT_TRUE(config.enable_cytokine_effects);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

TEST_F(SwarmImmuneConfigTest, ImmuneImmuneDefaultConfig) {
    swarm_immune_immune_config_t config;
    EXPECT_EQ(0, swarm_immune_immune_default_config(&config));

    EXPECT_TRUE(config.enable_brain_to_swarm);
    EXPECT_TRUE(config.enable_swarm_to_brain);
}

TEST_F(SwarmImmuneConfigTest, BrainImmuneDefaultConfig) {
    swarm_brain_immune_config_t config;
    EXPECT_EQ(0, swarm_brain_immune_default_config(&config));

    EXPECT_TRUE(config.enable_cytokine_impairment);
    EXPECT_TRUE(config.enable_inflammation_effects);
}

/* =============================================================================
 * Null Config Tests
 * ============================================================================= */

TEST_F(SwarmImmuneConfigTest, AllNullConfigsReturnError) {
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

/* =============================================================================
 * Null Bridge Getter Tests
 *
 * Test that getter functions handle null bridges gracefully.
 * ============================================================================= */

class SwarmImmuneNullBridgeTest : public ::testing::Test {};

TEST_F(SwarmImmuneNullBridgeTest, SignalImmuneNullReturnsDefault) {
    // Null bridge should return safe default values (typically 1.0f)
    float quality = swarm_signal_immune_get_quality_factor(nullptr);
    float packet_loss = swarm_signal_immune_get_packet_loss(nullptr);

    EXPECT_FLOAT_EQ(1.0f, quality);
    EXPECT_FLOAT_EQ(0.0f, packet_loss);
}

TEST_F(SwarmImmuneNullBridgeTest, ConsensusImmuneNullReturnsDefault) {
    float delay = swarm_consensus_immune_get_voting_delay(nullptr);
    float quorum = swarm_consensus_immune_get_quorum_factor(nullptr);

    // Returns 1.0f as safe default (no extra delay)
    EXPECT_FLOAT_EQ(1.0f, delay);
    EXPECT_FLOAT_EQ(1.0f, quorum);
}

TEST_F(SwarmImmuneNullBridgeTest, MemoryImmuneNullReturnsDefault) {
    float capacity = swarm_memory_immune_get_capacity_factor(nullptr);
    float efficiency = swarm_memory_immune_get_consolidation_efficiency(nullptr);

    EXPECT_FLOAT_EQ(1.0f, capacity);
    EXPECT_FLOAT_EQ(1.0f, efficiency);
}

TEST_F(SwarmImmuneNullBridgeTest, FlockingImmuneNullReturnsDefault) {
    float alignment = swarm_flocking_immune_get_alignment_factor(nullptr);
    float cohesion = swarm_flocking_immune_get_cohesion_factor(nullptr);
    float separation = swarm_flocking_immune_get_separation_factor(nullptr);

    EXPECT_FLOAT_EQ(1.0f, alignment);
    EXPECT_FLOAT_EQ(1.0f, cohesion);
    EXPECT_FLOAT_EQ(1.0f, separation);
}

TEST_F(SwarmImmuneNullBridgeTest, EmergenceImmuneNullReturnsDefault) {
    float factor = swarm_emergence_immune_get_emergence_factor(nullptr);
    int penalty = swarm_emergence_immune_get_tier_penalty(nullptr);
    bool blocked = swarm_emergence_immune_is_advancement_blocked(nullptr);

    EXPECT_FLOAT_EQ(1.0f, factor);
    EXPECT_EQ(0, penalty);
    EXPECT_FALSE(blocked);
}

TEST_F(SwarmImmuneNullBridgeTest, ConsciousnessImmuneNullReturnsDefault) {
    float phi = swarm_consciousness_immune_get_phi_factor(nullptr);
    float integration = swarm_consciousness_immune_get_integration_factor(nullptr);

    EXPECT_FLOAT_EQ(1.0f, phi);
    EXPECT_FLOAT_EQ(1.0f, integration);
}

TEST_F(SwarmImmuneNullBridgeTest, PheromoneImmuneNullReturnsDefault) {
    float sensing = swarm_pheromone_immune_get_sensing_factor(nullptr);
    float evaporation = swarm_pheromone_immune_get_evaporation_factor(nullptr);
    bool impaired = swarm_pheromone_immune_is_gradient_impaired(nullptr);

    EXPECT_FLOAT_EQ(1.0f, sensing);
    EXPECT_FLOAT_EQ(1.0f, evaporation);
    EXPECT_FALSE(impaired);
}

TEST_F(SwarmImmuneNullBridgeTest, QuorumImmuneNullReturnsDefault) {
    float threshold = swarm_quorum_immune_get_threshold_factor(nullptr);
    float integration = swarm_quorum_immune_get_integration_factor(nullptr);
    bool impaired = swarm_quorum_immune_is_impaired(nullptr);

    EXPECT_FLOAT_EQ(1.0f, threshold);
    EXPECT_FLOAT_EQ(1.0f, integration);
    EXPECT_FALSE(impaired);
}

TEST_F(SwarmImmuneNullBridgeTest, ImmuneImmuneNullReturnsDefault) {
    float detection = swarm_immune_immune_get_detection_factor(nullptr);
    bool coordinated = swarm_immune_immune_is_coordinated(nullptr);

    EXPECT_FLOAT_EQ(1.0f, detection);
    EXPECT_FALSE(coordinated);
}

TEST_F(SwarmImmuneNullBridgeTest, BrainImmuneNullReturnsDefault) {
    float coherence = swarm_brain_immune_get_coherence_factor(nullptr);

    EXPECT_FLOAT_EQ(1.0f, coherence);
}

/* =============================================================================
 * Bio-Async Connection Tests (Null Bridge)
 * ============================================================================= */

TEST(SwarmImmuneBioAsync, NullBridgeConnectionReturnsError) {
    EXPECT_EQ(-1, swarm_signal_immune_connect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_consensus_immune_connect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_memory_immune_connect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_flocking_immune_connect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_emergence_immune_connect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_consciousness_immune_connect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_pheromone_immune_connect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_quorum_immune_connect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_immune_immune_connect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_brain_immune_connect_bio_async(nullptr));
}

TEST(SwarmImmuneBioAsync, NullBridgeDisconnectionReturnsError) {
    EXPECT_EQ(-1, swarm_signal_immune_disconnect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_consensus_immune_disconnect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_memory_immune_disconnect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_flocking_immune_disconnect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_emergence_immune_disconnect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_consciousness_immune_disconnect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_pheromone_immune_disconnect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_quorum_immune_disconnect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_immune_immune_disconnect_bio_async(nullptr));
    EXPECT_EQ(-1, swarm_brain_immune_disconnect_bio_async(nullptr));
}

TEST(SwarmImmuneBioAsync, NullBridgeConnectionStatusReturnsFalse) {
    EXPECT_FALSE(swarm_signal_immune_is_bio_async_connected(nullptr));
    EXPECT_FALSE(swarm_consensus_immune_is_bio_async_connected(nullptr));
    EXPECT_FALSE(swarm_memory_immune_is_bio_async_connected(nullptr));
    EXPECT_FALSE(swarm_flocking_immune_is_bio_async_connected(nullptr));
    EXPECT_FALSE(swarm_emergence_immune_is_bio_async_connected(nullptr));
    EXPECT_FALSE(swarm_consciousness_immune_is_bio_async_connected(nullptr));
    EXPECT_FALSE(swarm_pheromone_immune_is_bio_async_connected(nullptr));
    EXPECT_FALSE(swarm_quorum_immune_is_bio_async_connected(nullptr));
    EXPECT_FALSE(swarm_immune_immune_is_bio_async_connected(nullptr));
    EXPECT_FALSE(swarm_brain_immune_is_bio_async_connected(nullptr));
}

/* =============================================================================
 * Config Consistency Tests
 * ============================================================================= */

TEST(SwarmImmuneConfigConsistency, AllModulesHaveCytokineAndInflammationFlags) {
    // All bridges should have consistent config patterns
    swarm_signal_immune_config_t signal_config;
    swarm_consensus_immune_config_t consensus_config;
    swarm_memory_immune_config_t memory_config;
    swarm_flocking_immune_config_t flocking_config;
    swarm_emergence_immune_config_t emergence_config;
    swarm_consciousness_immune_config_t consciousness_config;
    swarm_pheromone_immune_config_t pheromone_config;
    swarm_quorum_immune_config_t quorum_config;

    swarm_signal_immune_default_config(&signal_config);
    swarm_consensus_immune_default_config(&consensus_config);
    swarm_memory_immune_default_config(&memory_config);
    swarm_flocking_immune_default_config(&flocking_config);
    swarm_emergence_immune_default_config(&emergence_config);
    swarm_consciousness_immune_default_config(&consciousness_config);
    swarm_pheromone_immune_default_config(&pheromone_config);
    swarm_quorum_immune_default_config(&quorum_config);

    // All should have cytokine and inflammation effects enabled by default
    EXPECT_TRUE(signal_config.enable_cytokine_effects);
    EXPECT_TRUE(signal_config.enable_inflammation_effects);

    EXPECT_TRUE(consensus_config.enable_cytokine_effects);
    EXPECT_TRUE(consensus_config.enable_inflammation_effects);

    EXPECT_TRUE(memory_config.enable_cytokine_effects);
    EXPECT_TRUE(memory_config.enable_inflammation_effects);

    EXPECT_TRUE(flocking_config.enable_cytokine_effects);
    EXPECT_TRUE(flocking_config.enable_inflammation_effects);

    EXPECT_TRUE(emergence_config.enable_cytokine_effects);
    EXPECT_TRUE(emergence_config.enable_inflammation_effects);

    EXPECT_TRUE(consciousness_config.enable_cytokine_effects);
    EXPECT_TRUE(consciousness_config.enable_inflammation_effects);

    EXPECT_TRUE(pheromone_config.enable_cytokine_effects);
    EXPECT_TRUE(pheromone_config.enable_inflammation_effects);

    EXPECT_TRUE(quorum_config.enable_cytokine_effects);
    EXPECT_TRUE(quorum_config.enable_inflammation_effects);
}

/* =============================================================================
 * Main
 * ============================================================================= */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
