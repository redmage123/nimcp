/**
 * @file test_swarm_fep_bridges.cpp
 * @brief Unit tests for all Swarm-FEP Bridge modules
 *
 * WHAT: Comprehensive tests for all swarm-FEP bidirectional integrations
 * WHY:  Ensure collective free energy minimization works across swarm subsystems
 * HOW:  Test lifecycle, effects, and bio-async for each bridge type
 *
 * NOTE: All swarm FEP bridge create functions take 3 parameters:
 *       (config, context_for_module, fep_system)
 *       The context parameter is the actual swarm module instance.
 *       Without the real module instances, bridges cannot be fully tested.
 *       These tests verify NULL handling and basic API contracts.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_consensus_fep_bridge.h"
#include "swarm/nimcp_swarm_emergence_fep_bridge.h"
#include "swarm/nimcp_swarm_flocking_fep_bridge.h"
#include "swarm/nimcp_swarm_immune_fep_bridge.h"
#include "swarm/nimcp_swarm_memory_fep_bridge.h"
#include "swarm/nimcp_swarm_pheromone_fep_bridge.h"
#include "swarm/nimcp_swarm_quorum_fep_bridge.h"
#include "swarm/nimcp_swarm_signal_fep_bridge.h"
#include "swarm/nimcp_collective_workspace_fep_bridge.h"
#include "swarm/nimcp_emotional_contagion_fep_bridge.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

class SwarmFepBridgesTestBase : public ::testing::Test {
protected:
    fep_system_t* fep = nullptr;

    void SetUp() override {
        fep_config_t fep_config;
        fep_default_config(&fep_config);
        fep = fep_create(&fep_config, 8, 4);
        ASSERT_NE(fep, nullptr);
    }

    void TearDown() override {
        if (fep) {
            fep_destroy(fep);
            fep = nullptr;
        }
    }
};

/* ============================================================================
 * Swarm Consensus FEP Bridge Tests
 * API: swarm_consensus_fep_create(config, consensus_ctx, fep_system)
 * ============================================================================ */

TEST_F(SwarmFepBridgesTestBase, ConsensusNullConfig) {
    swarm_consensus_fep_bridge_t* br = swarm_consensus_fep_create(nullptr, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, ConsensusNullFep) {
    swarm_consensus_fep_config_t config;
    swarm_consensus_fep_default_config(&config);
    swarm_consensus_fep_bridge_t* br = swarm_consensus_fep_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, ConsensusDestroyNull) {
    swarm_consensus_fep_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Swarm Emergence FEP Bridge Tests
 * API: swarm_emergence_fep_create(config, emergence_ctx, fep_system)
 * ============================================================================ */

TEST_F(SwarmFepBridgesTestBase, EmergenceNullConfig) {
    swarm_emergence_fep_bridge_t* br = swarm_emergence_fep_create(nullptr, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, EmergenceNullFep) {
    swarm_emergence_fep_config_t config;
    swarm_emergence_fep_default_config(&config);
    swarm_emergence_fep_bridge_t* br = swarm_emergence_fep_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, EmergenceDestroyNull) {
    swarm_emergence_fep_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Swarm Flocking FEP Bridge Tests
 * API: swarm_flocking_fep_create(config, flocking_engine, fep_system)
 * ============================================================================ */

TEST_F(SwarmFepBridgesTestBase, FlockingNullConfig) {
    swarm_flocking_fep_bridge_t* br = swarm_flocking_fep_create(nullptr, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, FlockingNullFep) {
    swarm_flocking_fep_config_t config;
    swarm_flocking_fep_default_config(&config);
    swarm_flocking_fep_bridge_t* br = swarm_flocking_fep_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, FlockingDestroyNull) {
    swarm_flocking_fep_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Swarm Immune FEP Bridge Tests
 * API: swarm_immune_fep_create(config, immune_system, fep_system)
 * ============================================================================ */

TEST_F(SwarmFepBridgesTestBase, ImmuneNullConfig) {
    swarm_immune_fep_bridge_t* br = swarm_immune_fep_create(nullptr, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, ImmuneNullFep) {
    swarm_immune_fep_config_t config;
    swarm_immune_fep_default_config(&config);
    swarm_immune_fep_bridge_t* br = swarm_immune_fep_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, ImmuneDestroyNull) {
    swarm_immune_fep_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Swarm Memory FEP Bridge Tests
 * API: swarm_memory_fep_create(config, memory_system, fep_system)
 * ============================================================================ */

TEST_F(SwarmFepBridgesTestBase, MemoryNullConfig) {
    swarm_memory_fep_bridge_t* br = swarm_memory_fep_create(nullptr, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, MemoryNullFep) {
    swarm_memory_fep_config_t config;
    swarm_memory_fep_default_config(&config);
    swarm_memory_fep_bridge_t* br = swarm_memory_fep_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, MemoryDestroyNull) {
    swarm_memory_fep_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Swarm Pheromone FEP Bridge Tests
 * API: swarm_pheromone_fep_create(config, pheromone_system, fep_system)
 * ============================================================================ */

TEST_F(SwarmFepBridgesTestBase, PheromoneNullConfig) {
    swarm_pheromone_fep_bridge_t* br = swarm_pheromone_fep_create(nullptr, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, PheromoneNullFep) {
    swarm_pheromone_fep_config_t config;
    swarm_pheromone_fep_default_config(&config);
    swarm_pheromone_fep_bridge_t* br = swarm_pheromone_fep_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, PheromoneDestroyNull) {
    swarm_pheromone_fep_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Swarm Quorum FEP Bridge Tests
 * API: swarm_quorum_fep_create(config, quorum_system, fep_system)
 * ============================================================================ */

TEST_F(SwarmFepBridgesTestBase, QuorumNullConfig) {
    swarm_quorum_fep_bridge_t* br = swarm_quorum_fep_create(nullptr, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, QuorumNullFep) {
    swarm_quorum_fep_config_t config;
    swarm_quorum_fep_default_config(&config);
    swarm_quorum_fep_bridge_t* br = swarm_quorum_fep_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, QuorumDestroyNull) {
    swarm_quorum_fep_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Swarm Signal FEP Bridge Tests
 * API: swarm_signal_fep_create(config, signal_processor, fep_system)
 * ============================================================================ */

TEST_F(SwarmFepBridgesTestBase, SignalNullConfig) {
    swarm_signal_fep_bridge_t* br = swarm_signal_fep_create(nullptr, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, SignalNullFep) {
    swarm_signal_fep_config_t config;
    swarm_signal_fep_default_config(&config);
    swarm_signal_fep_bridge_t* br = swarm_signal_fep_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, SignalDestroyNull) {
    swarm_signal_fep_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Collective Workspace FEP Bridge Tests
 * API: collective_workspace_fep_create(config, workspace, fep_system)
 * ============================================================================ */

TEST_F(SwarmFepBridgesTestBase, WorkspaceNullConfig) {
    collective_workspace_fep_bridge_t* br = collective_workspace_fep_create(nullptr, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, WorkspaceNullFep) {
    collective_workspace_fep_config_t config;
    collective_workspace_fep_default_config(&config);
    collective_workspace_fep_bridge_t* br = collective_workspace_fep_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, WorkspaceDestroyNull) {
    collective_workspace_fep_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Emotional Contagion FEP Bridge Tests
 * API: emotional_contagion_fep_create(config, contagion_system, fep_system)
 * ============================================================================ */

TEST_F(SwarmFepBridgesTestBase, EmotionalNullConfig) {
    emotional_contagion_fep_bridge_t* br = emotional_contagion_fep_create(nullptr, nullptr, fep);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, EmotionalNullFep) {
    emotional_contagion_fep_config_t config;
    emotional_contagion_fep_default_config(&config);
    emotional_contagion_fep_bridge_t* br = emotional_contagion_fep_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(SwarmFepBridgesTestBase, EmotionalDestroyNull) {
    emotional_contagion_fep_destroy(nullptr);
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
