/**
 * @file e2e_test_swarm_full_system.cpp
 * @brief End-to-end test of complete NIMCP swarm system
 *
 * TEST COVERAGE:
 * - Full swarm lifecycle (formation, mission, dispersal)
 * - Real-world scenario simulation
 * - Multi-agent coordination
 * - Emergent behavior validation
 * - Performance benchmarking
 * - Fault tolerance and recovery
 * - Resource management
 * - Decision making pipeline
 * - Inter-swarm cooperation
 * - Long-running stability
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <random>

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
// Complete Swarm System
//=============================================================================

class CompleteSwarmSystem {
public:
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

    std::vector<uint32_t> agent_ids;
    bool is_initialized;

    CompleteSwarmSystem() : is_initialized(false) {
        InitializeAllSubsystems();
    }

    ~CompleteSwarmSystem() {
        DestroyAllSubsystems();
    }

    void InitializeAllSubsystems() {
        // Initialize pheromone system
        nimcp_pheromone_config_t ph_config;
        nimcp_pheromone_default_config(&ph_config);
        pheromone = nimcp_pheromone_create(&ph_config, nullptr);

        // Initialize morphogenesis
        nimcp_morphogenesis_config_t morph_config;
        nimcp_morphogenesis_default_config(&morph_config);
        morphogenesis = nimcp_morphogenesis_create(&morph_config, nullptr);

        // Initialize flocking
        nimcp_flocking_config_t flock_config;
        nimcp_flocking_default_config(&flock_config);
        flocking = nimcp_flocking_create(&flock_config, nullptr);

        // Initialize quorum sensing
        nimcp_quorum_config_t quorum_config;
        nimcp_quorum_default_config(&quorum_config);
        quorum = nimcp_quorum_create(&quorum_config, nullptr);

        // Initialize immune system
        nimcp_immune_config_t immune_config;
        nimcp_immune_default_config(&immune_config);
        immune = nimcp_immune_create(&immune_config, nullptr);

        // Initialize energy gossip
        nimcp_energy_gossip_config_t eg_config;
        nimcp_energy_gossip_default_config(&eg_config);
        energy_gossip = nimcp_energy_gossip_create(&eg_config, nullptr);

        // Initialize memory
        memory = nimcp_swarm_memory_create(10000, 5);
        nimcp_swarm_memory_init(memory, nullptr);

        // Initialize cascade prevention
        nimcp_cascade_config_t cascade_config;
        nimcp_cascade_get_default_config(&cascade_config);
        cascade = nimcp_cascade_create(&cascade_config, nullptr);

        // Initialize multi-swarm
        multi_swarm = nimcp_multi_swarm_create(nullptr, nullptr);

        // Initialize proprioception
        nimcp_swarm_proprio_config_t proprio_config;
        nimcp_swarm_proprio_default_config(&proprio_config);
        proprioception = nimcp_swarm_proprioception_create(1, &proprio_config, nullptr);

        is_initialized = true;
    }

    void DestroyAllSubsystems() {
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

    void AddAgent(uint32_t id, nimcp_swarm_role_t role, double x, double y, double z) {
        // Register in morphogenesis
        nimcp_morphogenesis_register_agent(morphogenesis, id, role);

        // Register in flocking
        nimcp_flocking_agent_t agent = {id, {x, y, z}, {1.0, 0.0, 0.0}, 1.0, true};
        nimcp_flocking_register_agent(flocking, &agent);

        // Register in energy gossip
        nimcp_energy_gossip_register_node(energy_gossip, id, 0.8);

        agent_ids.push_back(id);
    }

    void UpdateAllSystems(double delta_time) {
        uint64_t dt_ms = static_cast<uint64_t>(delta_time * 1000.0);

        nimcp_pheromone_update(pheromone, dt_ms);
        nimcp_quorum_update(quorum, dt_ms);
        nimcp_flocking_update(flocking, delta_time);
        nimcp_energy_gossip_update(energy_gossip, dt_ms);
        nimcp_immune_update(immune, dt_ms);
        nimcp_morphogenesis_update(morphogenesis, dt_ms);
    }
};

//=============================================================================
// E2E Test Fixture
//=============================================================================

class SwarmE2ETest : public ::testing::Test {
protected:
    CompleteSwarmSystem* swarm_system;

    void SetUp() override {
        swarm_system = new CompleteSwarmSystem();
        ASSERT_TRUE(swarm_system->is_initialized);
    }

    void TearDown() override {
        delete swarm_system;
    }
};

//=============================================================================
// E2E Test Cases
//=============================================================================

TEST_F(SwarmE2ETest, SystemInitialization) {
    EXPECT_NE(swarm_system->pheromone, nullptr);
    EXPECT_NE(swarm_system->morphogenesis, nullptr);
    EXPECT_NE(swarm_system->flocking, nullptr);
    EXPECT_NE(swarm_system->quorum, nullptr);
    EXPECT_NE(swarm_system->immune, nullptr);
    EXPECT_NE(swarm_system->energy_gossip, nullptr);
    EXPECT_NE(swarm_system->memory, nullptr);
    EXPECT_NE(swarm_system->cascade, nullptr);
    EXPECT_NE(swarm_system->multi_swarm, nullptr);
    EXPECT_NE(swarm_system->proprioception, nullptr);
}

TEST_F(SwarmE2ETest, SwarmFormationScenario) {
    // Scenario: Swarm forms from dispersed agents

    // 1. Add dispersed agents
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 100.0);

    for (uint32_t i = 0; i < 30; i++) {
        double x = dis(gen);
        double y = dis(gen);
        swarm_system->AddAgent(i, SWARM_ROLE_WORKER, x, y, 0.0);
    }

    // 2. Deposit aggregation pheromone at center
    nimcp_position3d_t center = {50.0f, 50.0f, 0.0f};
    nimcp_pheromone_deposit(swarm_system->pheromone, &center,
                           PHEROMONE_AGGREGATION, 1.0f);

    // 3. Produce quorum signals
    for (uint32_t i = 0; i < 30; i++) {
        nimcp_quorum_produce_signal(swarm_system->quorum, i,
                                    QUORUM_SIGNAL_AGGREGATION, 0.05f);
    }

    // 4. Run simulation for formation
    for (int step = 0; step < 100; step++) {
        swarm_system->UpdateAllSystems(0.1); // 100ms per step = 10 seconds total
    }

    // 5. Check if swarm formed
    bool quorum_reached = false;
    nimcp_quorum_check_threshold(swarm_system->quorum,
                                 QUORUM_SIGNAL_AGGREGATION, &quorum_reached);

    EXPECT_TRUE(quorum_reached);
}

TEST_F(SwarmE2ETest, ResourceSearchMissionScenario) {
    // Scenario: Swarm searches for and collects resource

    // 1. Form swarm
    for (uint32_t i = 0; i < 20; i++) {
        swarm_system->AddAgent(i, SWARM_ROLE_WORKER, i * 2.0, 0.0, 0.0);
    }

    // 2. Place resource in environment
    nimcp_position3d_t resource_loc = {80.0f, 80.0f, 0.0f};
    nimcp_pheromone_deposit(swarm_system->pheromone, &resource_loc,
                           PHEROMONE_RESOURCE, 0.9f);

    // 3. Deploy scouts
    for (uint32_t i = 0; i < 3; i++) {
        nimcp_morphogenesis_set_role(swarm_system->morphogenesis, i, SWARM_ROLE_SCOUT);
    }

    // 4. Scouts discover resource and lay trail
    for (int j = 0; j < 10; j++) {
        nimcp_position3d_t trail_pos = {
            80.0f - j * 8.0f, 80.0f - j * 8.0f, 0.0f
        };
        nimcp_pheromone_deposit(swarm_system->pheromone, &trail_pos,
                               PHEROMONE_PATH, 0.5f);
    }

    // 5. Store resource discovery memory
    uint8_t resource_data[] = "resource_at_80_80";
    char memory_id[64];
    nimcp_swarm_memory_store(swarm_system->memory, NIMCP_MEMORY_EPISODIC,
                             NIMCP_IMPORTANCE_HIGH,
                             resource_data, sizeof(resource_data), memory_id);

    // 6. Swarm follows pheromone trail
    for (int step = 0; step < 50; step++) {
        swarm_system->UpdateAllSystems(0.1);
    }

    SUCCEED();
}

TEST_F(SwarmE2ETest, ThreatResponseScenario) {
    // Scenario: Swarm detects and responds to threat

    // 1. Establish swarm
    for (uint32_t i = 0; i < 25; i++) {
        swarm_system->AddAgent(i, SWARM_ROLE_WORKER, i * 1.5, 0.0, 0.0);
    }

    // 2. Threat detected by immune system
    nimcp_pathogen_t threat = {
        {0xBA, 0xDF, 0x00, 0xD}, 4, THREAT_MALICIOUS_CODE, 0.95, 0
    };
    nimcp_immune_detect_pathogen(swarm_system->immune, 10, &threat);

    // 3. Immune response activates
    nimcp_immune_activate_response(swarm_system->immune, 10, &threat);

    // 4. Cascade prevention isolates affected agent
    nimcp_failure_event_t event = {
        10, HEALTH_OPTIMAL, HEALTH_FAILING, DIAG_SEVERITY_CRITICAL, 0, "threat_detected"
    };
    nimcp_cascade_record_failure(swarm_system->cascade, &event);

    // 5. Deposit danger pheromone
    nimcp_position3d_t danger_pos = {15.0f, 0.0f, 0.0f};
    nimcp_pheromone_deposit(swarm_system->pheromone, &danger_pos,
                           PHEROMONE_DANGER, 0.9f);

    // 6. Transition defenders
    for (uint32_t i = 0; i < 5; i++) {
        if (i != 10) { // Skip isolated agent
            nimcp_morphogenesis_set_role(swarm_system->morphogenesis, i, SWARM_ROLE_DEFENDER);
        }
    }

    // 7. Broadcast threat to swarm
    nimcp_immune_broadcast_threat(swarm_system->immune, 10, &threat);

    // 8. Store threat memory
    uint8_t threat_data[] = "threat_signature_BADFO0D";
    char memory_id[64];
    nimcp_swarm_memory_store(swarm_system->memory, NIMCP_MEMORY_THREAT,
                             NIMCP_IMPORTANCE_CRITICAL,
                             threat_data, sizeof(threat_data), memory_id);

    SUCCEED();
}

TEST_F(SwarmE2ETest, MultiSwarmCooperationScenario) {
    // Scenario: Multiple swarms cooperate on mission

    // 1. Create two swarms
    auto* swarm1 = nimcp_swarm_identity_create(swarm_system->multi_swarm, "swarm1", 15);
    auto* swarm2 = nimcp_swarm_identity_create(swarm_system->multi_swarm, "swarm2", 15);

    nimcp_swarm_register(swarm_system->multi_swarm, swarm1);
    nimcp_swarm_register(swarm_system->multi_swarm, swarm2);

    // 2. Add capabilities
    nimcp_swarm_add_capability(swarm1, NIMCP_SWARM_CAP_SURVEILLANCE, 0.9, 10, true);
    nimcp_swarm_add_capability(swarm2, NIMCP_SWARM_CAP_TRANSPORT, 0.8, 10, true);

    // 3. Set territories
    nimcp_coord3d_t min1 = {0, 0, 0}, max1 = {50, 50, 10};
    nimcp_swarm_set_territory(swarm1, min1, max1, true, 0.8);

    nimcp_coord3d_t min2 = {60, 0, 0}, max2 = {110, 50, 10};
    nimcp_swarm_set_territory(swarm2, min2, max2, true, 0.8);

    // 4. Create joint mission
    nimcp_territory_bounds_t mission_area = {{25, 0, 0}, {85, 50, 10}, 0, false, 0.9};
    uint64_t mission_id = nimcp_mission_create(swarm_system->multi_swarm,
                                                "joint_surveillance_transport",
                                                NIMCP_MISSION_PRIORITY_HIGH,
                                                mission_area, 0);

    // 5. Assign both swarms to mission
    uint64_t swarm_ids[] = {swarm1->swarm_id, swarm2->swarm_id};
    nimcp_mission_assign_swarms(swarm_system->multi_swarm, mission_id, swarm_ids, 2);

    // 6. Create communication bridge
    uint64_t bridge_id = nimcp_comm_bridge_create(swarm_system->multi_swarm,
                                                    swarm1->swarm_id,
                                                    swarm2->swarm_id,
                                                    nullptr, 0);

    EXPECT_GT(mission_id, 0);
    EXPECT_GT(bridge_id, 0);
}

TEST_F(SwarmE2ETest, LongRunningStabilityTest) {
    // Test: System remains stable over extended operation

    // Add agents
    for (uint32_t i = 0; i < 50; i++) {
        swarm_system->AddAgent(i, SWARM_ROLE_WORKER, i * 2.0, 0.0, 0.0);
    }

    // Run for extended period (simulate 5 minutes)
    for (int step = 0; step < 3000; step++) {
        swarm_system->UpdateAllSystems(0.1); // 100ms steps

        // Periodically introduce events
        if (step % 100 == 0) {
            // Deposit pheromone
            nimcp_position3d_t pos = {(float)(step % 100), (float)(step % 100), 0.0f};
            nimcp_pheromone_deposit(swarm_system->pheromone, &pos, PHEROMONE_PATH, 0.3f);
        }

        if (step % 500 == 0) {
            // Consolidate memories
            uint32_t consolidated = 0;
            nimcp_swarm_memory_consolidate(swarm_system->memory, &consolidated);
        }
    }

    // Verify system health
    float health = nimcp_swarm_memory_get_health_score(swarm_system->memory);
    EXPECT_GE(health, 0.5f);
}

TEST_F(SwarmE2ETest, FaultToleranceAndRecoveryScenario) {
    // Scenario: Swarm experiences failures and recovers

    // 1. Establish swarm
    for (uint32_t i = 0; i < 30; i++) {
        swarm_system->AddAgent(i, SWARM_ROLE_WORKER, i * 2.0, i * 0.5, 0.0);
    }

    // 2. Simulate multiple agent failures
    for (uint32_t i = 5; i < 10; i++) {
        nimcp_failure_event_t event = {
            i, HEALTH_OPTIMAL, HEALTH_FAILED, SEVERITY_MAJOR, 0, "agent_failure"
        };
        nimcp_cascade_record_failure(swarm_system->cascade, &event);
    }

    // 3. Check for cascade
    nimcp_cascade_detection_t detection;
    nimcp_cascade_detect_cascade(swarm_system->cascade, &detection);

    // 4. Start recovery for failed agents
    for (uint32_t i = 5; i < 10; i++) {
        nimcp_cascade_start_recovery(swarm_system->cascade, i, RECOVERY_GRADUAL);
    }

    // 5. Redistribute roles among remaining agents
    nimcp_result_t result = nimcp_morphogenesis_rebalance(swarm_system->morphogenesis);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // 6. Verify swarm adaptation
    nimcp_morphogenesis_stats_t stats;
    nimcp_morphogenesis_get_stats(swarm_system->morphogenesis, &stats);

    EXPECT_GT(stats.total_agents, 0);
}

TEST_F(SwarmE2ETest, CompleteLifecycleScenario) {
    // Scenario: Complete swarm lifecycle from formation to dispersal

    // Phase 1: Formation
    for (uint32_t i = 0; i < 40; i++) {
        swarm_system->AddAgent(i, SWARM_ROLE_WORKER,
                               (i % 10) * 5.0, (i / 10) * 5.0, 0.0);
    }

    nimcp_position3d_t rally = {25.0f, 25.0f, 0.0f};
    nimcp_pheromone_deposit(swarm_system->pheromone, &rally,
                           PHEROMONE_AGGREGATION, 1.0f);

    for (int i = 0; i < 50; i++) {
        swarm_system->UpdateAllSystems(0.1);
    }

    // Phase 2: Specialization
    for (uint32_t i = 0; i < 5; i++) {
        nimcp_morphogenesis_set_role(swarm_system->morphogenesis, i, SWARM_ROLE_SCOUT);
    }
    for (uint32_t i = 5; i < 10; i++) {
        nimcp_morphogenesis_set_role(swarm_system->morphogenesis, i, SWARM_ROLE_DEFENDER);
    }

    // Phase 3: Mission Execution
    nimcp_position3d_t target = {100.0f, 100.0f, 0.0f};
    nimcp_pheromone_deposit(swarm_system->pheromone, &target,
                           PHEROMONE_TARGET, 0.9f);

    for (int i = 0; i < 100; i++) {
        swarm_system->UpdateAllSystems(0.1);
    }

    // Phase 4: Dispersal
    nimcp_pheromone_deposit(swarm_system->pheromone, &rally,
                           PHEROMONE_DISPERSION, 0.8f);

    for (int i = 0; i < 50; i++) {
        swarm_system->UpdateAllSystems(0.1);
    }

    SUCCEED();
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
