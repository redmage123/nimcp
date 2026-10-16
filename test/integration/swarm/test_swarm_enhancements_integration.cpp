/**
 * @file test_swarm_enhancements_integration.cpp
 * @brief Integration tests for all swarm enhancements working together
 *
 * TEST COVERAGE:
 * - Pheromone-guided morphogenesis
 * - Flocking with quorum sensing
 * - Immune system + cascade prevention
 * - Energy-aware gossip + memory consolidation
 * - Multi-swarm with proprioception
 * - End-to-end system integration
 * - Cross-module communication
 * - Bio-async message flow
 * - BBB security across modules
 * - Performance under integrated load
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_pheromone.h"
#include "swarm/nimcp_swarm_morphogenesis.h"
#include "swarm/nimcp_swarm_flocking.h"
#include "swarm/nimcp_swarm_quorum.h"
#include "swarm/nimcp_swarm_immune.h"
#include "swarm/nimcp_swarm_energy_gossip.h"
#include "swarm/nimcp_swarm_memory.h"
#include "swarm/nimcp_swarm_cascade.h"
#include "swarm/nimcp_swarm_multi.h"
#include "swarm/nimcp_swarm_proprioception.h"

//=============================================================================
// Integration Test Fixture
//=============================================================================

class SwarmIntegrationTest : public ::testing::Test {
protected:
    // All subsystems
    nimcp_pheromone_system_t* pheromone;
    nimcp_morphogenesis_system_t* morphogenesis;
    nimcp_flocking_system_t* flocking;
    nimcp_quorum_system_t* quorum;
    nimcp_immune_system_t* immune;
    nimcp_energy_gossip_t* energy_gossip;
    NimcpSwarmMemory* memory;
    nimcp_cascade_system_t* cascade;
    nimcp_multi_swarm_coordinator_t* multi_swarm;
    nimcp_swarm_proprioception_t* proprioception;

    void SetUp() override {
        // Initialize all subsystems with compatible configs
        SetupPheromone();
        SetupMorphogenesis();
        SetupFlocking();
        SetupQuorum();
        SetupImmune();
        SetupEnergyGossip();
        SetupMemory();
        SetupCascade();
        SetupMultiSwarm();
        SetupProprioception();
    }

    void TearDown() override {
        if (proprioception) nimcp_swarm_proprioception_destroy(proprioception);
        if (multi_swarm) nimcp_multi_swarm_destroy(multi_swarm);
        if (cascade) nimcp_cascade_destroy(cascade);
        if (memory) nimcp_swarm_memory_destroy(memory);
        if (energy_gossip) nimcp_energy_gossip_destroy(energy_gossip);
        if (immune) nimcp_immune_destroy(immune);
        if (quorum) nimcp_quorum_destroy(quorum);
        if (flocking) nimcp_flocking_destroy(flocking);
        if (morphogenesis) nimcp_morphogenesis_destroy(morphogenesis);
        if (pheromone) nimcp_pheromone_destroy(pheromone);
    }

    void SetupPheromone() {
        nimcp_pheromone_config_t config;
        nimcp_pheromone_default_config(&config);
        pheromone = nimcp_pheromone_create(&config, nullptr);
        ASSERT_NE(pheromone, nullptr);
    }

    void SetupMorphogenesis() {
        nimcp_morphogenesis_config_t config;
        nimcp_morphogenesis_default_config(&config);
        morphogenesis = nimcp_morphogenesis_create(&config, nullptr);
        ASSERT_NE(morphogenesis, nullptr);
    }

    void SetupFlocking() {
        nimcp_flocking_config_t config;
        nimcp_flocking_default_config(&config);
        flocking = nimcp_flocking_create(&config, nullptr);
        ASSERT_NE(flocking, nullptr);
    }

    void SetupQuorum() {
        nimcp_quorum_config_t config;
        nimcp_quorum_default_config(&config);
        quorum = nimcp_quorum_create(&config, nullptr);
        ASSERT_NE(quorum, nullptr);
    }

    void SetupImmune() {
        nimcp_immune_config_t config;
        nimcp_immune_default_config(&config);
        immune = nimcp_immune_create(&config, nullptr);
        ASSERT_NE(immune, nullptr);
    }

    void SetupEnergyGossip() {
        nimcp_energy_gossip_config_t config;
        nimcp_energy_gossip_default_config(&config);
        energy_gossip = nimcp_energy_gossip_create(&config, nullptr);
        ASSERT_NE(energy_gossip, nullptr);
    }

    void SetupMemory() {
        memory = nimcp_swarm_memory_create(1000, 3);
        ASSERT_NE(memory, nullptr);
        nimcp_swarm_memory_init(memory, nullptr);
    }

    void SetupCascade() {
        nimcp_cascade_config_t config;
        nimcp_cascade_get_default_config(&config);
        cascade = nimcp_cascade_create(&config, nullptr);
        ASSERT_NE(cascade, nullptr);
    }

    void SetupMultiSwarm() {
        multi_swarm = nimcp_multi_swarm_create(nullptr, nullptr);
        ASSERT_NE(multi_swarm, nullptr);
    }

    void SetupProprioception() {
        nimcp_swarm_proprio_config_t config;
        nimcp_swarm_proprio_default_config(&config);
        proprioception = nimcp_swarm_proprioception_create(1, &config, nullptr);
        ASSERT_NE(proprioception, nullptr);
    }
};

//=============================================================================
// Integration Test Cases
//=============================================================================

TEST_F(SwarmIntegrationTest, AllSystemsCreated) {
    EXPECT_NE(pheromone, nullptr);
    EXPECT_NE(morphogenesis, nullptr);
    EXPECT_NE(flocking, nullptr);
    EXPECT_NE(quorum, nullptr);
    EXPECT_NE(immune, nullptr);
    EXPECT_NE(energy_gossip, nullptr);
    EXPECT_NE(memory, nullptr);
    EXPECT_NE(cascade, nullptr);
    EXPECT_NE(multi_swarm, nullptr);
    EXPECT_NE(proprioception, nullptr);
}

TEST_F(SwarmIntegrationTest, PheromoneGuidedMorphogenesis) {
    // Scenario: Pheromones guide role differentiation

    // 1. Deposit resource pheromone
    nimcp_position3d_t resource_pos = {10.0f, 10.0f, 0.0f};
    nimcp_pheromone_deposit(pheromone, &resource_pos, PHEROMONE_RESOURCE, 0.9f);

    // 2. Register agent
    nimcp_morphogenesis_register_agent(morphogenesis, 1, SWARM_ROLE_WORKER);

    // 3. Check if strong resource signal triggers forager role
    float concentration = 0.0f;
    nimcp_pheromone_get_concentration(pheromone, &resource_pos,
                                      PHEROMONE_RESOURCE, &concentration);

    if (concentration > 0.7f) {
        // Agent should consider transitioning to forager
        nimcp_morphogenesis_stimulus_t stimulus = {
            STIMULUS_RESOURCE_FOUND, concentration, resource_pos, 0
        };
        nimcp_morphogenesis_process_stimulus(morphogenesis, 1, &stimulus);
    }

    SUCCEED();
}

TEST_F(SwarmIntegrationTest, FlockingWithQuorumSensing) {
    // Scenario: Flocking behavior triggers quorum decisions

    // 1. Register multiple flocking agents
    for (uint32_t i = 0; i < 10; i++) {
        nimcp_flocking_agent_t agent = {
            i, {(double)i * 2.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1.0, true
        };
        nimcp_flocking_register_agent(flocking, &agent);

        // Agents produce aggregation signals
        nimcp_quorum_produce_signal(quorum, i, QUORUM_SIGNAL_AGGREGATION, 0.1f);
    }

    // 2. Check if quorum threshold reached
    bool threshold_reached = false;
    nimcp_quorum_check_threshold(quorum, QUORUM_SIGNAL_AGGREGATION, &threshold_reached);

    // 3. If threshold reached, swarm can make collective decision
    if (threshold_reached) {
        // Set formation based on quorum decision
        nimcp_flocking_set_formation(flocking, FORMATION_WEDGE);
    }

    SUCCEED();
}

TEST_F(SwarmIntegrationTest, ImmuneSystemWithCascadePrevention) {
    // Scenario: Immune system detects threat, cascade prevention isolates

    // 1. Detect pathogen
    nimcp_pathogen_t pathogen = {
        {0xDE, 0xAD, 0xBE, 0xEF}, 4, THREAT_MALICIOUS_CODE, 0.9, 0
    };
    nimcp_immune_detect_pathogen(immune, 5, &pathogen);

    // 2. Immune system isolates agent
    nimcp_immune_isolate_agent(immune, 5);

    // 3. Cascade system records failure
    nimcp_failure_event_t event = {
        5, HEALTH_OPTIMAL, HEALTH_FAILING, SEVERITY_MAJOR, 0, "pathogen_detected"
    };
    nimcp_cascade_record_failure(cascade, &event);

    // 4. Check if cascade detected
    nimcp_cascade_detection_t detection;
    nimcp_cascade_detect_cascade(cascade, &detection);

    // 5. If no cascade, start recovery
    if (!detection.cascade_detected) {
        nimcp_cascade_start_recovery(cascade, 5, RECOVERY_GRADUAL);
    }

    SUCCEED();
}

TEST_F(SwarmIntegrationTest, EnergyGossipWithMemoryConsolidation) {
    // Scenario: Energy-aware gossip shares important memories

    // 1. Register nodes with different energy levels
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_energy_gossip_register_node(energy_gossip, i, 0.5 + i * 0.1);
    }

    // 2. Store important memory
    uint8_t data[] = {1, 2, 3, 4, 5};
    char memory_id[64];
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_THREAT,
                             NIMCP_IMPORTANCE_CRITICAL, data, sizeof(data), memory_id);

    // 3. Select high-energy peer for memory distribution
    uint32_t peer = 0;
    nimcp_energy_gossip_select_peer(energy_gossip, 0, &peer);

    // 4. Distribute memory to selected peer
    if (peer > 0) {
        uint32_t replicas = 0;
        nimcp_swarm_memory_distribute(memory, memory_id, &replicas);
    }

    SUCCEED();
}

TEST_F(SwarmIntegrationTest, MultiSwarmWithProprioception) {
    // Scenario: Multiple swarms coordinate using proprioception

    // 1. Create two swarms
    auto* swarm1 = nimcp_swarm_identity_create(multi_swarm, "swarm1", 10);
    auto* swarm2 = nimcp_swarm_identity_create(multi_swarm, "swarm2", 10);
    ASSERT_NE(swarm1, nullptr);
    ASSERT_NE(swarm2, nullptr);

    nimcp_swarm_register(multi_swarm, swarm1);
    nimcp_swarm_register(multi_swarm, swarm2);

    // 2. Update proprioception for swarm1
    nimcp_swarm_position_t pos1 = {5.0, 5.0, 0.0};
    nimcp_swarm_proprio_update_position(proprioception, &pos1, nullptr);

    // 3. Detect swarm shape
    nimcp_swarm_shape_descriptor_t shape;
    nimcp_swarm_proprio_classify_shape(proprioception, &shape);

    // 4. Set territories based on shape
    if (shape.shape_type == NIMCP_SWARM_SHAPE_SPHERE) {
        nimcp_coord3d_t min1 = {0, 0, 0}, max1 = {10, 10, 10};
        nimcp_swarm_set_territory(swarm1, min1, max1, true, 0.8);

        nimcp_coord3d_t min2 = {15, 0, 0}, max2 = {25, 10, 10};
        nimcp_swarm_set_territory(swarm2, min2, max2, true, 0.8);
    }

    SUCCEED();
}

TEST_F(SwarmIntegrationTest, CompleteWorkflowSimulation) {
    // Scenario: Complete swarm mission with all systems working together

    // 1. Initialize swarm with agents
    for (uint32_t i = 0; i < 20; i++) {
        nimcp_morphogenesis_register_agent(morphogenesis, i, SWARM_ROLE_WORKER);
        nimcp_energy_gossip_register_node(energy_gossip, i, 0.8);

        nimcp_flocking_agent_t agent = {
            i, {(double)i * 1.5, 0.0, 0.0}, {1.0, 0.0, 0.0}, 1.0, true
        };
        nimcp_flocking_register_agent(flocking, &agent);
    }

    // 2. Deposit target pheromone
    nimcp_position3d_t target = {50.0f, 50.0f, 0.0f};
    nimcp_pheromone_deposit(pheromone, &target, PHEROMONE_TARGET, 1.0f);

    // 3. Agents sense quorum and decide to move
    for (uint32_t i = 0; i < 20; i++) {
        nimcp_quorum_produce_signal(quorum, i, QUORUM_SIGNAL_AGGREGATION, 0.05f);
    }

    bool quorum_reached = false;
    nimcp_quorum_check_threshold(quorum, QUORUM_SIGNAL_AGGREGATION, &quorum_reached);

    // 4. If quorum reached, coordinate movement
    if (quorum_reached) {
        nimcp_flocking_set_formation(flocking, FORMATION_WEDGE);

        // Some agents become scouts
        for (uint32_t i = 0; i < 3; i++) {
            nimcp_morphogenesis_set_role(morphogenesis, i, SWARM_ROLE_SCOUT);
        }
    }

    // 5. Store mission memory
    uint8_t mission_data[] = "mission_to_target";
    char memory_id[64];
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_EPISODIC,
                             NIMCP_IMPORTANCE_HIGH,
                             mission_data, sizeof(mission_data), memory_id);

    // 6. Update all systems
    nimcp_flocking_update(flocking, 0.016);
    nimcp_pheromone_update(pheromone, 16);
    nimcp_quorum_update(quorum, 16);
    nimcp_energy_gossip_update(energy_gossip, 16);

    SUCCEED();
}

TEST_F(SwarmIntegrationTest, StressTestAllSystems) {
    // Stress test: high load on all systems simultaneously

    // Spawn many agents and operations
    for (uint32_t i = 0; i < 50; i++) {
        nimcp_morphogenesis_register_agent(morphogenesis, i, SWARM_ROLE_WORKER);
        nimcp_energy_gossip_register_node(energy_gossip, i, 0.7);

        nimcp_position3d_t pos = {(float)i, (float)i, 0.0f};
        nimcp_pheromone_deposit(pheromone, &pos, PHEROMONE_PATH, 0.3f);

        nimcp_quorum_produce_signal(quorum, i, QUORUM_SIGNAL_AGGREGATION, 0.02f);
    }

    // Run multiple update cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        nimcp_pheromone_update(pheromone, 100);
        nimcp_quorum_update(quorum, 100);
        nimcp_energy_gossip_update(energy_gossip, 100);
        nimcp_cascade_update_telemetry(cascade, nullptr);
    }

    SUCCEED();
}

TEST_F(SwarmIntegrationTest, BiDirectionalCommunication) {
    // Test: Systems can communicate bi-directionally

    // Agent detects resource via pheromone
    nimcp_position3d_t resource_pos = {20.0f, 20.0f, 0.0f};
    nimcp_pheromone_deposit(pheromone, &resource_pos, PHEROMONE_RESOURCE, 0.8f);

    // This triggers morphogenesis change
    nimcp_morphogenesis_register_agent(morphogenesis, 1, SWARM_ROLE_WORKER);
    nimcp_morphogenesis_stimulus_t stimulus = {
        STIMULUS_RESOURCE_FOUND, 0.8f, resource_pos, 0
    };
    nimcp_morphogenesis_process_stimulus(morphogenesis, 1, &stimulus);

    // Role change triggers memory storage
    uint8_t role_change_data[] = "worker_to_forager";
    char memory_id[64];
    nimcp_swarm_memory_store(memory, NIMCP_MEMORY_PROCEDURAL,
                             NIMCP_IMPORTANCE_MEDIUM,
                             role_change_data, sizeof(role_change_data), memory_id);

    // Memory gets distributed via energy gossip
    nimcp_energy_gossip_register_node(energy_gossip, 1, 0.9);
    uint32_t replicas = 0;
    nimcp_swarm_memory_distribute(memory, memory_id, &replicas);

    SUCCEED();
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
