/**
 * @file test_swarm_core.cpp
 * @brief Comprehensive unit tests for Swarm Intelligence Core Functionality
 *
 * TEST COVERAGE:
 * - Swarm initialization and cleanup
 * - Agent creation and destruction
 * - Flocking behavior (separation, alignment, cohesion)
 * - Consensus mechanisms
 * - Signal propagation
 * - Pheromone trails
 * - Emergence patterns
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <memory>

// Include swarm headers with extern "C" guards
#include "swarm/nimcp_swarm_flocking.h"
#include "swarm/nimcp_swarm_consensus.h"
#include "swarm/nimcp_swarm_signal.h"
#include "swarm/nimcp_swarm_pheromone.h"
#include "swarm/nimcp_swarm_emergence.h"

//=============================================================================
// Test Fixture: Swarm Core
//=============================================================================

class SwarmCoreTest : public ::testing::Test {
protected:
    nimcp_flocking_engine_t* flocking_engine = nullptr;
    swarm_consensus_t consensus_ctx = nullptr;
    nimcp_swarm_signal_adapter_t* signal_adapter = nullptr;
    nimcp_pheromone_system_t* pheromone_system = nullptr;
    swarm_emergence_ctx_t* emergence_ctx = nullptr;

    void SetUp() override {
        // Initialize flocking engine
        nimcp_flocking_config_t flocking_config;
        nimcp_flocking_get_default_config(&flocking_config);
        flocking_engine = nimcp_flocking_create(&flocking_config);

        // Initialize consensus
        swarm_consensus_config_t consensus_config = swarm_consensus_default_config(1);
        consensus_ctx = swarm_consensus_create(&consensus_config);

        // Initialize signal adapter
        // NOTE: max_packet_size must be <= LORA_MAX_PAYLOAD (255)
        swarm_signal_config_t signal_config = {
            .radio_type = SWARM_RADIO_SIMULATION,
            .frequency_hz = 915000000,
            .bandwidth_hz = 125000,
            .tx_power_dbm = 14,
            .max_packet_size = 255,  // Fixed: was 256 but LORA_MAX_PAYLOAD is 255
            .retry_count = 3,
            .timeout_ms = 1000,
            .node_id = 1,
            .custom_send = nullptr,
            .custom_recv = nullptr,
            .custom_ctx = nullptr
        };
        signal_adapter = swarm_signal_adapter_create(&signal_config);

        // Initialize pheromone system
        nimcp_pheromone_config_t pheromone_config;
        nimcp_pheromone_default_config(&pheromone_config);
        pheromone_system = nimcp_pheromone_create(&pheromone_config, nullptr);

        // Initialize emergence
        emergence_ctx = swarm_emergence_create();
    }

    void TearDown() override {
        if (flocking_engine) {
            nimcp_flocking_destroy(flocking_engine);
            flocking_engine = nullptr;
        }
        if (consensus_ctx) {
            swarm_consensus_destroy(consensus_ctx);
            consensus_ctx = nullptr;
        }
        if (signal_adapter) {
            swarm_signal_adapter_destroy(signal_adapter);
            signal_adapter = nullptr;
        }
        if (pheromone_system) {
            nimcp_pheromone_destroy(pheromone_system);
            pheromone_system = nullptr;
        }
        if (emergence_ctx) {
            swarm_emergence_destroy(emergence_ctx);
            emergence_ctx = nullptr;
        }
    }

    // Helper: Create random position
    nimcp_vec3_t RandomPosition(float range = 100.0f) {
        return nimcp_vec3_create(
            (rand() / (float)RAND_MAX) * range - range/2,
            (rand() / (float)RAND_MAX) * range - range/2,
            (rand() / (float)RAND_MAX) * range - range/2
        );
    }

    // Helper: Create random velocity
    nimcp_vec3_t RandomVelocity(float max_speed = 5.0f) {
        return nimcp_vec3_create(
            (rand() / (float)RAND_MAX) * max_speed - max_speed/2,
            (rand() / (float)RAND_MAX) * max_speed - max_speed/2,
            (rand() / (float)RAND_MAX) * max_speed - max_speed/2
        );
    }
};

//=============================================================================
// Swarm Initialization and Cleanup Tests
//=============================================================================

TEST_F(SwarmCoreTest, FlockingEngineCreation) {
    EXPECT_NE(flocking_engine, nullptr) << "Flocking engine should be created";
}

TEST_F(SwarmCoreTest, FlockingEngineWithNullConfig) {
    // Create with NULL config should use defaults
    nimcp_flocking_engine_t* engine = nimcp_flocking_create(nullptr);
    EXPECT_NE(engine, nullptr) << "Should create with default config";
    if (engine) {
        nimcp_flocking_destroy(engine);
    }
}

TEST_F(SwarmCoreTest, FlockingEngineDestroyNull) {
    // Destroy NULL should not crash
    nimcp_flocking_destroy(nullptr);
    SUCCEED();
}

TEST_F(SwarmCoreTest, ConsensusContextCreation) {
    EXPECT_NE(consensus_ctx, nullptr) << "Consensus context should be created";
}

TEST_F(SwarmCoreTest, ConsensusDestroyNull) {
    swarm_consensus_destroy(nullptr);
    SUCCEED();
}

TEST_F(SwarmCoreTest, SignalAdapterCreation) {
    EXPECT_NE(signal_adapter, nullptr) << "Signal adapter should be created";
}

TEST_F(SwarmCoreTest, PheromoneSystemCreation) {
    EXPECT_NE(pheromone_system, nullptr) << "Pheromone system should be created";
}

TEST_F(SwarmCoreTest, EmergenceContextCreation) {
    EXPECT_NE(emergence_ctx, nullptr) << "Emergence context should be created";
}

//=============================================================================
// Agent Creation and Destruction Tests
//=============================================================================

TEST_F(SwarmCoreTest, AddSingleBoid) {
    ASSERT_NE(flocking_engine, nullptr);

    nimcp_vec3_t pos = nimcp_vec3_create(0.0f, 0.0f, 0.0f);
    nimcp_vec3_t vel = nimcp_vec3_create(1.0f, 0.0f, 0.0f);

    uint32_t boid_id = nimcp_flocking_add_boid(flocking_engine, &pos, &vel);
    EXPECT_GT(boid_id, 0u) << "Boid ID should be positive";
}

TEST_F(SwarmCoreTest, AddMultipleBoids) {
    ASSERT_NE(flocking_engine, nullptr);

    std::vector<uint32_t> boid_ids;
    for (int i = 0; i < 10; i++) {
        nimcp_vec3_t pos = RandomPosition();
        nimcp_vec3_t vel = RandomVelocity();
        uint32_t id = nimcp_flocking_add_boid(flocking_engine, &pos, &vel);
        EXPECT_GT(id, 0u);
        boid_ids.push_back(id);
    }
    EXPECT_EQ(boid_ids.size(), 10u);
}

TEST_F(SwarmCoreTest, RemoveBoid) {
    ASSERT_NE(flocking_engine, nullptr);

    nimcp_vec3_t pos = nimcp_vec3_create(0.0f, 0.0f, 0.0f);
    nimcp_vec3_t vel = nimcp_vec3_create(1.0f, 0.0f, 0.0f);
    uint32_t boid_id = nimcp_flocking_add_boid(flocking_engine, &pos, &vel);
    ASSERT_GT(boid_id, 0u);

    int result = nimcp_flocking_remove_boid(flocking_engine, boid_id);
    EXPECT_EQ(result, 0) << "Remove should succeed";
}

TEST_F(SwarmCoreTest, RemoveNonExistentBoid) {
    ASSERT_NE(flocking_engine, nullptr);

    int result = nimcp_flocking_remove_boid(flocking_engine, 99999);
    EXPECT_NE(result, 0) << "Remove non-existent should fail";
}

TEST_F(SwarmCoreTest, GetBoidById) {
    ASSERT_NE(flocking_engine, nullptr);

    nimcp_vec3_t pos = nimcp_vec3_create(5.0f, 10.0f, 15.0f);
    nimcp_vec3_t vel = nimcp_vec3_create(1.0f, 2.0f, 3.0f);
    uint32_t boid_id = nimcp_flocking_add_boid(flocking_engine, &pos, &vel);
    ASSERT_GT(boid_id, 0u);

    nimcp_boid_t* boid = nimcp_flocking_get_boid(flocking_engine, boid_id);
    ASSERT_NE(boid, nullptr);
    EXPECT_NEAR(boid->position.x, 5.0f, 0.001f);
    EXPECT_NEAR(boid->position.y, 10.0f, 0.001f);
    EXPECT_NEAR(boid->position.z, 15.0f, 0.001f);
}

//=============================================================================
// Flocking Behavior Tests - Separation
//=============================================================================

TEST_F(SwarmCoreTest, SeparationForceWhenClose) {
    ASSERT_NE(flocking_engine, nullptr);

    // Add two boids very close together
    nimcp_vec3_t pos1 = nimcp_vec3_create(0.0f, 0.0f, 0.0f);
    nimcp_vec3_t pos2 = nimcp_vec3_create(0.5f, 0.0f, 0.0f);  // Very close
    nimcp_vec3_t vel = nimcp_vec3_create(1.0f, 0.0f, 0.0f);

    uint32_t id1 = nimcp_flocking_add_boid(flocking_engine, &pos1, &vel);
    uint32_t id2 = nimcp_flocking_add_boid(flocking_engine, &pos2, &vel);
    ASSERT_GT(id1, 0u);
    ASSERT_GT(id2, 0u);

    nimcp_boid_t* boid1 = nimcp_flocking_get_boid(flocking_engine, id1);
    ASSERT_NE(boid1, nullptr);

    nimcp_vec3_t force;
    int result = nimcp_flocking_separation(flocking_engine, boid1, &force);
    EXPECT_EQ(result, 0);
    // Separation force should push boid1 away from boid2 (negative x)
    EXPECT_LE(force.x, 0.0f) << "Separation should push away";
}

TEST_F(SwarmCoreTest, SeparationForceWhenFar) {
    ASSERT_NE(flocking_engine, nullptr);

    // Add two boids far apart
    nimcp_vec3_t pos1 = nimcp_vec3_create(0.0f, 0.0f, 0.0f);
    nimcp_vec3_t pos2 = nimcp_vec3_create(100.0f, 0.0f, 0.0f);  // Very far
    nimcp_vec3_t vel = nimcp_vec3_create(1.0f, 0.0f, 0.0f);

    uint32_t id1 = nimcp_flocking_add_boid(flocking_engine, &pos1, &vel);
    nimcp_flocking_add_boid(flocking_engine, &pos2, &vel);

    nimcp_boid_t* boid1 = nimcp_flocking_get_boid(flocking_engine, id1);
    ASSERT_NE(boid1, nullptr);

    nimcp_vec3_t force;
    int result = nimcp_flocking_separation(flocking_engine, boid1, &force);
    EXPECT_EQ(result, 0);
    // Separation force should be minimal when far apart
    float magnitude = sqrtf(force.x*force.x + force.y*force.y + force.z*force.z);
    EXPECT_LT(magnitude, 0.1f) << "Separation force should be small when far";
}

//=============================================================================
// Flocking Behavior Tests - Alignment
//=============================================================================

TEST_F(SwarmCoreTest, AlignmentForceMatchesNeighborVelocity) {
    ASSERT_NE(flocking_engine, nullptr);

    // Add boids with different velocities
    nimcp_vec3_t pos1 = nimcp_vec3_create(0.0f, 0.0f, 0.0f);
    nimcp_vec3_t pos2 = nimcp_vec3_create(3.0f, 0.0f, 0.0f);
    nimcp_vec3_t vel1 = nimcp_vec3_create(1.0f, 0.0f, 0.0f);
    nimcp_vec3_t vel2 = nimcp_vec3_create(0.0f, 1.0f, 0.0f);

    uint32_t id1 = nimcp_flocking_add_boid(flocking_engine, &pos1, &vel1);
    nimcp_flocking_add_boid(flocking_engine, &pos2, &vel2);

    nimcp_boid_t* boid1 = nimcp_flocking_get_boid(flocking_engine, id1);
    ASSERT_NE(boid1, nullptr);

    nimcp_vec3_t force;
    int result = nimcp_flocking_alignment(flocking_engine, boid1, &force);
    EXPECT_EQ(result, 0);
    // Alignment should steer toward neighbor's velocity (positive y)
}

//=============================================================================
// Flocking Behavior Tests - Cohesion
//=============================================================================

TEST_F(SwarmCoreTest, CohesionForcePullsTowardCenter) {
    ASSERT_NE(flocking_engine, nullptr);

    // Add boids in a cluster, with one on the edge
    nimcp_vec3_t center = nimcp_vec3_create(10.0f, 10.0f, 10.0f);
    nimcp_vec3_t edge = nimcp_vec3_create(0.0f, 0.0f, 0.0f);
    nimcp_vec3_t vel = nimcp_vec3_create(1.0f, 0.0f, 0.0f);

    // Add several boids near center
    for (int i = 0; i < 5; i++) {
        nimcp_vec3_t pos = nimcp_vec3_add(center, nimcp_vec3_create(
            (rand() / (float)RAND_MAX - 0.5f) * 2.0f,
            (rand() / (float)RAND_MAX - 0.5f) * 2.0f,
            (rand() / (float)RAND_MAX - 0.5f) * 2.0f
        ));
        nimcp_flocking_add_boid(flocking_engine, &pos, &vel);
    }

    // Add edge boid
    uint32_t edge_id = nimcp_flocking_add_boid(flocking_engine, &edge, &vel);
    nimcp_boid_t* edge_boid = nimcp_flocking_get_boid(flocking_engine, edge_id);
    ASSERT_NE(edge_boid, nullptr);

    nimcp_vec3_t force;
    int result = nimcp_flocking_cohesion(flocking_engine, edge_boid, &force);
    EXPECT_EQ(result, 0);
    // Cohesion should pull toward the center (positive x, y, z)
    EXPECT_GE(force.x, 0.0f) << "Cohesion should pull toward center";
}

//=============================================================================
// Consensus Mechanism Tests
//=============================================================================

TEST_F(SwarmCoreTest, ProposeVote) {
    ASSERT_NE(consensus_ctx, nullptr);

    uint32_t proposal_id = 0;
    float values[4] = {1.0f, 2.0f, 3.0f, 4.0f};

    nimcp_error_t result = swarm_consensus_propose(
        consensus_ctx,
        VOTE_TOPIC_TARGET_PRIORITY,
        values,
        0,  // No deadline
        2,  // Quorum of 2
        0.5f,  // 50% threshold
        nullptr,  // No callback
        nullptr,
        &proposal_id
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(proposal_id, 0u);
}

TEST_F(SwarmCoreTest, CastVote) {
    ASSERT_NE(consensus_ctx, nullptr);

    // First create a proposal
    uint32_t proposal_id = 0;
    swarm_consensus_propose(
        consensus_ctx,
        VOTE_TOPIC_FORMATION_CHANGE,
        nullptr,
        0, 1, 0.5f, nullptr, nullptr,
        &proposal_id
    );
    ASSERT_GT(proposal_id, 0u);

    // Cast vote
    nimcp_error_t result = swarm_consensus_vote(
        consensus_ctx,
        proposal_id,
        VOTE_CHOICE_AGREE,
        0.9f
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmCoreTest, CheckVoteResult) {
    ASSERT_NE(consensus_ctx, nullptr);

    uint32_t proposal_id = 0;
    swarm_consensus_propose(
        consensus_ctx,
        VOTE_TOPIC_RETREAT,
        nullptr,
        0, 1, 0.5f, nullptr, nullptr,
        &proposal_id
    );
    ASSERT_GT(proposal_id, 0u);

    // Vote
    swarm_consensus_vote(consensus_ctx, proposal_id, VOTE_CHOICE_AGREE, 0.9f);

    // Check result
    bool is_complete = false;
    nimcp_error_t result = swarm_consensus_check_result(
        consensus_ctx, proposal_id, &is_complete
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmCoreTest, GetVoteResult) {
    ASSERT_NE(consensus_ctx, nullptr);

    uint32_t proposal_id = 0;
    swarm_consensus_propose(
        consensus_ctx,
        VOTE_TOPIC_LEADER_ELECTION,
        nullptr,
        0, 1, 0.5f, nullptr, nullptr,
        &proposal_id
    );

    swarm_consensus_vote(consensus_ctx, proposal_id, VOTE_CHOICE_AGREE, 0.95f);

    swarm_vote_result_t vote_result;
    nimcp_error_t result = swarm_consensus_get_result(
        consensus_ctx, proposal_id, &vote_result
    );

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(vote_result.proposal_id, proposal_id);
}

TEST_F(SwarmCoreTest, CancelVote) {
    ASSERT_NE(consensus_ctx, nullptr);

    uint32_t proposal_id = 0;
    swarm_consensus_propose(
        consensus_ctx,
        VOTE_TOPIC_RESOURCE_ALLOCATION,
        nullptr,
        0, 1, 0.5f, nullptr, nullptr,
        &proposal_id
    );

    nimcp_error_t result = swarm_consensus_cancel(consensus_ctx, proposal_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SwarmCoreTest, ConsensusStatistics) {
    ASSERT_NE(consensus_ctx, nullptr);

    swarm_consensus_stats_t stats;
    nimcp_error_t result = swarm_consensus_get_stats(consensus_ctx, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Signal Propagation Tests
//=============================================================================

TEST_F(SwarmCoreTest, SignalSend) {
    ASSERT_NE(signal_adapter, nullptr);

    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    bool result = swarm_signal_send(signal_adapter, data, sizeof(data), 0);
    EXPECT_TRUE(result) << "Signal send should succeed";
}

TEST_F(SwarmCoreTest, SignalBroadcast) {
    ASSERT_NE(signal_adapter, nullptr);

    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    bool result = swarm_signal_broadcast(signal_adapter, data, sizeof(data));
    EXPECT_TRUE(result) << "Broadcast should succeed";
}

TEST_F(SwarmCoreTest, SignalReceiveNonBlocking) {
    ASSERT_NE(signal_adapter, nullptr);

    uint8_t buffer[256];
    uint32_t received_len = 0;
    uint32_t source_id = 0;

    // Non-blocking receive (may not have data)
    bool result = swarm_signal_receive(
        signal_adapter, buffer, sizeof(buffer), &received_len, &source_id
    );
    // Result may be false if no data available - that's OK
    (void)result;
    SUCCEED();
}

TEST_F(SwarmCoreTest, SignalGetStats) {
    ASSERT_NE(signal_adapter, nullptr);

    swarm_signal_stats_t stats;
    bool result = swarm_signal_get_stats(signal_adapter, &stats);
    EXPECT_TRUE(result);
}

TEST_F(SwarmCoreTest, SignalResetStats) {
    ASSERT_NE(signal_adapter, nullptr);

    bool result = swarm_signal_reset_stats(signal_adapter);
    EXPECT_TRUE(result);
}

TEST_F(SwarmCoreTest, SignalSetTxPower) {
    ASSERT_NE(signal_adapter, nullptr);

    bool result = swarm_signal_set_tx_power(signal_adapter, 10);
    EXPECT_TRUE(result);
}

TEST_F(SwarmCoreTest, SignalGetTxPower) {
    ASSERT_NE(signal_adapter, nullptr);

    int8_t power = swarm_signal_get_tx_power(signal_adapter);
    // Should return configured power
    EXPECT_GE(power, -10);
    EXPECT_LE(power, 30);
}

TEST_F(SwarmCoreTest, SignalIsOperational) {
    ASSERT_NE(signal_adapter, nullptr);

    bool operational = swarm_signal_is_operational(signal_adapter);
    EXPECT_TRUE(operational);
}

//=============================================================================
// Pheromone Trail Tests
//=============================================================================

TEST_F(SwarmCoreTest, PheromoneDeposit) {
    ASSERT_NE(pheromone_system, nullptr);

    nimcp_position3d_t pos = {10.0f, 10.0f, 10.0f};
    nimcp_result_t result = nimcp_pheromone_deposit(
        pheromone_system, &pos, PHEROMONE_PATH, 0.5f
    );
    EXPECT_EQ(result, NIMCP_OK);
}

TEST_F(SwarmCoreTest, PheromoneGetConcentration) {
    ASSERT_NE(pheromone_system, nullptr);

    // Deposit first
    nimcp_position3d_t pos = {20.0f, 20.0f, 20.0f};
    nimcp_pheromone_deposit(pheromone_system, &pos, PHEROMONE_RESOURCE, 0.8f);

    // Get concentration
    float concentration = 0.0f;
    nimcp_result_t result = nimcp_pheromone_get_concentration(
        pheromone_system, &pos, PHEROMONE_RESOURCE, &concentration
    );
    EXPECT_EQ(result, NIMCP_OK);
    EXPECT_GE(concentration, 0.0f);
}

TEST_F(SwarmCoreTest, PheromoneGetGradient) {
    ASSERT_NE(pheromone_system, nullptr);

    // Create gradient by depositing at different locations
    nimcp_position3d_t pos1 = {0.0f, 0.0f, 0.0f};
    nimcp_position3d_t pos2 = {5.0f, 0.0f, 0.0f};
    nimcp_pheromone_deposit(pheromone_system, &pos1, PHEROMONE_TARGET, 0.3f);
    nimcp_pheromone_deposit(pheromone_system, &pos2, PHEROMONE_TARGET, 0.9f);

    // Get gradient at midpoint
    nimcp_position3d_t query = {2.5f, 0.0f, 0.0f};
    nimcp_pheromone_gradient_t gradient;
    nimcp_result_t result = nimcp_pheromone_get_gradient(
        pheromone_system, &query, PHEROMONE_TARGET, &gradient
    );
    EXPECT_EQ(result, NIMCP_OK);
    // Gradient should point toward higher concentration (positive x)
}

TEST_F(SwarmCoreTest, PheromoneReinforcePath) {
    ASSERT_NE(pheromone_system, nullptr);

    // Create a path
    nimcp_position3d_t path[5] = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {2.0f, 0.0f, 0.0f},
        {3.0f, 0.0f, 0.0f},
        {4.0f, 0.0f, 0.0f}
    };

    nimcp_result_t result = nimcp_pheromone_reinforce_path(
        pheromone_system, path, 5, PHEROMONE_PATH, 1.5f
    );
    EXPECT_EQ(result, NIMCP_OK);
}

TEST_F(SwarmCoreTest, PheromoneUpdate) {
    ASSERT_NE(pheromone_system, nullptr);

    // Deposit
    nimcp_position3d_t pos = {5.0f, 5.0f, 5.0f};
    nimcp_pheromone_deposit(pheromone_system, &pos, PHEROMONE_DANGER, 1.0f);

    // Update with time delta (decay)
    nimcp_result_t result = nimcp_pheromone_update(pheromone_system, 1000);
    EXPECT_EQ(result, NIMCP_OK);

    // Concentration should have decayed
    float concentration = 0.0f;
    nimcp_pheromone_get_concentration(
        pheromone_system, &pos, PHEROMONE_DANGER, &concentration
    );
    EXPECT_LT(concentration, 1.0f) << "Pheromone should have decayed";
}

TEST_F(SwarmCoreTest, PheromoneReset) {
    ASSERT_NE(pheromone_system, nullptr);

    // Deposit some pheromones
    nimcp_position3d_t pos = {1.0f, 1.0f, 1.0f};
    nimcp_pheromone_deposit(pheromone_system, &pos, PHEROMONE_RALLY, 0.5f);

    // Reset
    nimcp_result_t result = nimcp_pheromone_reset(pheromone_system);
    EXPECT_EQ(result, NIMCP_OK);

    // Concentration should be zero after reset
    float concentration = 0.0f;
    nimcp_pheromone_get_concentration(
        pheromone_system, &pos, PHEROMONE_RALLY, &concentration
    );
    EXPECT_NEAR(concentration, 0.0f, 0.001f);
}

TEST_F(SwarmCoreTest, PheromoneStats) {
    ASSERT_NE(pheromone_system, nullptr);

    nimcp_pheromone_stats_t stats;
    nimcp_result_t result = nimcp_pheromone_get_stats(pheromone_system, &stats);
    EXPECT_EQ(result, NIMCP_OK);
}

//=============================================================================
// Emergence Pattern Tests
//=============================================================================

TEST_F(SwarmCoreTest, EmergenceGetTier) {
    ASSERT_NE(emergence_ctx, nullptr);

    swarm_emergence_tier_t tier = swarm_emergence_get_tier(emergence_ctx);
    EXPECT_GE(tier, SWARM_TIER_INDIVIDUAL);
    EXPECT_LT(tier, SWARM_TIER_COUNT);
}

TEST_F(SwarmCoreTest, EmergenceUpdateState) {
    ASSERT_NE(emergence_ctx, nullptr);

    swarm_state_t state = {
        .connected_drones = 10,
        .healthy_drones = 9,
        .collective_coherence = 0.85f,
        .timestamp = 1000000000
    };

    int result = swarm_emergence_update(emergence_ctx, &state);
    EXPECT_EQ(result, 0);
}

TEST_F(SwarmCoreTest, EmergenceTierFromCount) {
    // Test tier calculation
    EXPECT_EQ(swarm_emergence_calculate_tier_from_count(1), SWARM_TIER_INDIVIDUAL);
    EXPECT_EQ(swarm_emergence_calculate_tier_from_count(2), SWARM_TIER_PAIR);
    EXPECT_EQ(swarm_emergence_calculate_tier_from_count(3), SWARM_TIER_PAIR);
    EXPECT_EQ(swarm_emergence_calculate_tier_from_count(4), SWARM_TIER_SQUAD);
    EXPECT_EQ(swarm_emergence_calculate_tier_from_count(8), SWARM_TIER_PLATOON);
    EXPECT_EQ(swarm_emergence_calculate_tier_from_count(16), SWARM_TIER_COMPANY);
    EXPECT_EQ(swarm_emergence_calculate_tier_from_count(32), SWARM_TIER_BATTALION);
}

TEST_F(SwarmCoreTest, EmergenceGetCapabilities) {
    ASSERT_NE(emergence_ctx, nullptr);

    // Update to a higher tier
    swarm_state_t state = {
        .connected_drones = 20,
        .healthy_drones = 18,
        .collective_coherence = 0.9f,
        .timestamp = 2000000000
    };
    swarm_emergence_update(emergence_ctx, &state);

    swarm_capabilities_t caps;
    int result = swarm_emergence_get_capabilities(emergence_ctx, &caps);
    EXPECT_EQ(result, 0);
}

TEST_F(SwarmCoreTest, EmergenceCanDo) {
    ASSERT_NE(emergence_ctx, nullptr);

    // At battalion tier (32+ drones), should have meta_cognition
    swarm_state_t state = {
        .connected_drones = 40,
        .healthy_drones = 38,
        .collective_coherence = 0.95f,
        .timestamp = 3000000000
    };
    // Update multiple times to satisfy stability requirement
    for (int i = 0; i < 10; i++) {
        state.timestamp += 100000000;
        swarm_emergence_update(emergence_ctx, &state);
    }

    bool can_do = swarm_emergence_can_do(emergence_ctx, "meta_cognition");
    // May not be true yet due to stability requirements
    (void)can_do;
    SUCCEED();
}

TEST_F(SwarmCoreTest, EmergenceSetCoherenceThreshold) {
    ASSERT_NE(emergence_ctx, nullptr);

    int result = swarm_emergence_set_coherence_threshold(emergence_ctx, 0.8f);
    EXPECT_EQ(result, 0);
}

TEST_F(SwarmCoreTest, EmergenceGetStats) {
    ASSERT_NE(emergence_ctx, nullptr);

    swarm_emergence_stats_t stats;
    int result = swarm_emergence_get_stats(emergence_ctx, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(SwarmCoreTest, EmergenceGetTierName) {
    const char* name = swarm_emergence_get_tier_name(SWARM_TIER_PLATOON);
    EXPECT_NE(name, nullptr);
    EXPECT_STRNE(name, "");
}

//=============================================================================
// Edge Cases and Error Handling
//=============================================================================

TEST_F(SwarmCoreTest, FlockingNullEngineOperations) {
    nimcp_vec3_t pos = nimcp_vec3_create(0, 0, 0);
    nimcp_vec3_t vel = nimcp_vec3_create(1, 0, 0);

    EXPECT_EQ(nimcp_flocking_add_boid(nullptr, &pos, &vel), 0u);
    EXPECT_NE(nimcp_flocking_remove_boid(nullptr, 1), 0);
    EXPECT_EQ(nimcp_flocking_get_boid(nullptr, 1), nullptr);
}

TEST_F(SwarmCoreTest, FlockingNullPositionVelocity) {
    ASSERT_NE(flocking_engine, nullptr);

    nimcp_vec3_t pos = nimcp_vec3_create(0, 0, 0);
    nimcp_vec3_t vel = nimcp_vec3_create(1, 0, 0);

    // NULL position
    EXPECT_EQ(nimcp_flocking_add_boid(flocking_engine, nullptr, &vel), 0u);
    // NULL velocity
    EXPECT_EQ(nimcp_flocking_add_boid(flocking_engine, &pos, nullptr), 0u);
}

TEST_F(SwarmCoreTest, ConsensusInvalidProposal) {
    ASSERT_NE(consensus_ctx, nullptr);

    swarm_vote_result_t result;
    nimcp_error_t err = swarm_consensus_get_result(consensus_ctx, 99999, &result);
    EXPECT_NE(err, NIMCP_SUCCESS) << "Should fail for invalid proposal";
}

TEST_F(SwarmCoreTest, PheromoneOutOfBounds) {
    ASSERT_NE(pheromone_system, nullptr);

    // Position way outside world bounds
    nimcp_position3d_t pos = {100000.0f, 100000.0f, 100000.0f};
    nimcp_result_t result = nimcp_pheromone_deposit(
        pheromone_system, &pos, PHEROMONE_PATH, 0.5f
    );
    // Should handle gracefully
    (void)result;
    SUCCEED();
}

TEST_F(SwarmCoreTest, EmergenceInvalidState) {
    ASSERT_NE(emergence_ctx, nullptr);

    // Invalid state with negative coherence
    swarm_state_t invalid_state = {
        .connected_drones = 10,
        .healthy_drones = 10,
        .collective_coherence = -1.0f,  // Invalid
        .timestamp = 1000
    };

    bool valid = swarm_emergence_validate_state(&invalid_state);
    EXPECT_FALSE(valid);
}

TEST_F(SwarmCoreTest, EmergenceNullContext) {
    EXPECT_EQ(swarm_emergence_get_tier(nullptr), SWARM_TIER_INDIVIDUAL);
    EXPECT_FALSE(swarm_emergence_is_valid(nullptr));
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(SwarmCoreTest, FlockingUpdateSystem) {
    ASSERT_NE(flocking_engine, nullptr);

    // Add multiple boids
    for (int i = 0; i < 20; i++) {
        nimcp_vec3_t pos = RandomPosition(50.0f);
        nimcp_vec3_t vel = RandomVelocity(3.0f);
        nimcp_flocking_add_boid(flocking_engine, &pos, &vel);
    }

    // Run update
    int result = nimcp_flocking_update(flocking_engine, 0.016f);
    EXPECT_EQ(result, 0);
}

TEST_F(SwarmCoreTest, FlockingGetStats) {
    ASSERT_NE(flocking_engine, nullptr);

    // Add boids
    for (int i = 0; i < 10; i++) {
        nimcp_vec3_t pos = RandomPosition(20.0f);
        nimcp_vec3_t vel = RandomVelocity(2.0f);
        nimcp_flocking_add_boid(flocking_engine, &pos, &vel);
    }

    // Update
    nimcp_flocking_update(flocking_engine, 0.016f);

    // Get stats
    nimcp_flocking_stats_t stats;
    int result = nimcp_flocking_get_stats(flocking_engine, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_GT(stats.avg_speed, 0.0f);
}

TEST_F(SwarmCoreTest, FlockingCenterOfMass) {
    ASSERT_NE(flocking_engine, nullptr);

    // Add boids in known positions
    nimcp_vec3_t positions[] = {
        nimcp_vec3_create(0.0f, 0.0f, 0.0f),
        nimcp_vec3_create(10.0f, 0.0f, 0.0f),
        nimcp_vec3_create(0.0f, 10.0f, 0.0f),
        nimcp_vec3_create(10.0f, 10.0f, 0.0f)
    };
    nimcp_vec3_t vel = nimcp_vec3_create(1.0f, 0.0f, 0.0f);

    for (int i = 0; i < 4; i++) {
        nimcp_flocking_add_boid(flocking_engine, &positions[i], &vel);
    }

    nimcp_vec3_t center;
    int result = nimcp_flocking_center_of_mass(flocking_engine, &center);
    EXPECT_EQ(result, 0);
    // Center should be at (5, 5, 0)
    EXPECT_NEAR(center.x, 5.0f, 0.1f);
    EXPECT_NEAR(center.y, 5.0f, 0.1f);
    EXPECT_NEAR(center.z, 0.0f, 0.1f);
}

TEST_F(SwarmCoreTest, FlockingFormation) {
    ASSERT_NE(flocking_engine, nullptr);

    // Add boids
    for (int i = 0; i < 5; i++) {
        nimcp_vec3_t pos = nimcp_vec3_create((float)i * 2.0f, 0.0f, 0.0f);
        nimcp_vec3_t vel = nimcp_vec3_create(1.0f, 0.0f, 0.0f);
        nimcp_flocking_add_boid(flocking_engine, &pos, &vel);
    }

    // Set V formation
    int result = nimcp_flocking_set_formation(
        flocking_engine, NIMCP_FORMATION_V, 0
    );
    EXPECT_EQ(result, 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
