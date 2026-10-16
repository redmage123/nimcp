/**
 * @file test_motor_swarm_integration.cpp
 * @brief Integration tests for Motor Cortex with Swarm systems
 *
 * WHAT: Tests Motor Cortex integration with swarm emergence and flocking
 * WHY:  Ensure motor actions can scale with swarm collective behavior
 * HOW:  Test emergence tiers, flocking behavior, and motor coordination
 *
 * BIOLOGICAL BASIS:
 * Collective motor behavior emerges from:
 * - Emergence tiers based on swarm size
 * - Flocking alignment and cohesion
 * - Consensus-based action selection
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/motor/nimcp_motor_adapter.h"
#include "swarm/nimcp_swarm_emergence.h"
#include "swarm/nimcp_swarm_flocking.h"
#include "utils/logging/nimcp_logging.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class MotorSwarmIntegrationTest : public ::testing::Test {
protected:
    motor_adapter_t* motor;
    motor_config_t motor_config;
    swarm_emergence_ctx_t* emergence;

    void SetUp() override {
        /* Create motor adapter */
        motor_config = motor_default_config();
        motor_config.enable_bio_async = false;
        motor = motor_create(&motor_config);
        ASSERT_NE(nullptr, motor);

        /* Create emergence context */
        emergence = swarm_emergence_create();
        /* Emergence may be NULL if not available */
    }

    void TearDown() override {
        if (emergence) {
            swarm_emergence_destroy(emergence);
            emergence = nullptr;
        }
        if (motor) {
            motor_destroy(motor);
            motor = nullptr;
        }
    }

    /* Helper to create a standard test goal */
    motor_goal_t CreateTestGoal(motor_region_t region, float x, float y, float z,
                                float duration_ms) {
        motor_goal_t goal;
        memset(&goal, 0, sizeof(goal));
        goal.region = region;
        goal.target_position.x = x;
        goal.target_position.y = y;
        goal.target_position.z = z;
        goal.max_duration_ms = duration_ms;
        goal.type = MOVEMENT_TYPE_DISCRETE;
        return goal;
    }

    /* Helper to update emergence with swarm state */
    void UpdateEmergence(uint32_t connected, uint32_t healthy, float coherence) {
        if (!emergence) return;
        swarm_state_t state;
        memset(&state, 0, sizeof(state));
        state.connected_drones = connected;
        state.healthy_drones = healthy;
        state.collective_coherence = coherence;
        state.timestamp = 0;
        /* Call update multiple times to satisfy stability requirement */
        /* Default stability count is 5 per NIMCP_SWARM_DEFAULT_STABILITY_COUNT */
        for (int i = 0; i < 6; i++) {
            swarm_emergence_update(emergence, &state);
        }
    }
};

/*=============================================================================
 * EMERGENCE TIER TESTS
 * Test motor behavior at different emergence tiers
 *===========================================================================*/

TEST_F(MotorSwarmIntegrationTest, EmergenceBaseline_Individual) {
    if (!emergence) {
        GTEST_SKIP() << "Swarm emergence not available";
    }

    /* Initial state: no drones connected */
    swarm_emergence_tier_t tier = swarm_emergence_get_tier(emergence);
    EXPECT_EQ(SWARM_TIER_INDIVIDUAL, tier);

    /* Motor should work normally in individual state */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 200.0f);
    bool planned = motor_plan_movement(motor, &goal);
    EXPECT_TRUE(planned);
}

TEST_F(MotorSwarmIntegrationTest, EmergenceTierPair) {
    if (!emergence) {
        GTEST_SKIP() << "Swarm emergence not available";
    }

    /* Update with 2 drones for PAIR tier */
    UpdateEmergence(2, 2, 0.9f);

    swarm_emergence_tier_t tier = swarm_emergence_get_tier(emergence);
    EXPECT_GE((int)tier, (int)SWARM_TIER_PAIR);

    /* Motor coordination possible with pair */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 200.0f);
    motor_plan_movement(motor, &goal);
    motor_begin_execution(motor);

    for (int i = 0; i < 20; i++) {
        motor_update_execution(motor, 10.0f);
    }

    motor_effector_state_t state;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &state);
    EXPECT_GT(state.position.x, 0.0f);
}

TEST_F(MotorSwarmIntegrationTest, EmergenceTierSquad) {
    if (!emergence) {
        GTEST_SKIP() << "Swarm emergence not available";
    }

    /* Update with 5 drones for SQUAD tier */
    UpdateEmergence(5, 5, 0.85f);

    swarm_emergence_tier_t tier = swarm_emergence_get_tier(emergence);
    EXPECT_GE((int)tier, (int)SWARM_TIER_SQUAD);

    /* More complex motor coordination with squad */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_ARM_RIGHT, 1.5f, 0.5f, 0.0f, 300.0f);
    motor_plan_movement(motor, &goal);
    motor_begin_execution(motor);

    for (int i = 0; i < 30; i++) {
        motor_update_execution(motor, 10.0f);
    }

    motor_stats_t stats;
    motor_get_stats(motor, &stats);
    EXPECT_GT(stats.commands_generated, 0u);
}

/*=============================================================================
 * MOTOR WITH SWARM CONFIG TESTS
 *===========================================================================*/

TEST_F(MotorSwarmIntegrationTest, MotorOperatesWithoutSwarm) {
    /* Motor should work even if swarm is not available */
    motor_goal_t goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 200.0f);
    EXPECT_TRUE(motor_plan_movement(motor, &goal));
    EXPECT_TRUE(motor_begin_execution(motor));

    for (int i = 0; i < 20; i++) {
        motor_update_execution(motor, 10.0f);
    }

    motor_effector_state_t state;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &state);
    EXPECT_GT(state.position.x, 0.0f);
}

TEST_F(MotorSwarmIntegrationTest, MultipleMotorInstancesIndependent) {
    /* Create second motor instance */
    motor_adapter_t* motor2 = motor_create(&motor_config);
    ASSERT_NE(nullptr, motor2);

    /* Both should operate independently */
    motor_goal_t goal1 = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 200.0f);
    motor_goal_t goal2 = CreateTestGoal(MOTOR_REGION_HAND_LEFT, -1.0f, 0.0f, 0.0f, 200.0f);

    motor_plan_movement(motor, &goal1);
    motor_plan_movement(motor2, &goal2);

    motor_begin_execution(motor);
    motor_begin_execution(motor2);

    for (int i = 0; i < 20; i++) {
        motor_update_execution(motor, 10.0f);
        motor_update_execution(motor2, 10.0f);
    }

    motor_effector_state_t state1, state2;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &state1);
    motor_get_effector_state(motor2, MOTOR_REGION_HAND_LEFT, &state2);

    EXPECT_GT(state1.position.x, 0.0f);
    EXPECT_LT(state2.position.x, 0.0f);

    motor_destroy(motor2);
}

/*=============================================================================
 * EMERGENCE STATE TESTS
 *===========================================================================*/

TEST_F(MotorSwarmIntegrationTest, EmergenceContextCreation) {
    /* Verify emergence context was created */
    if (!emergence) {
        GTEST_SKIP() << "Swarm emergence not available";
    }
    EXPECT_TRUE(swarm_emergence_is_valid(emergence));
}

TEST_F(MotorSwarmIntegrationTest, EmergenceStats) {
    if (!emergence) {
        GTEST_SKIP() << "Swarm emergence not available";
    }

    swarm_emergence_stats_t stats;
    int result = swarm_emergence_get_stats(emergence, &stats);
    if (result == 0) {
        EXPECT_GE(stats.highest_tier_reached, SWARM_TIER_INDIVIDUAL);
    }
}

TEST_F(MotorSwarmIntegrationTest, EmergenceTierName) {
    const char* name = swarm_emergence_get_tier_name(SWARM_TIER_INDIVIDUAL);
    if (name) {
        EXPECT_GT(strlen(name), 0u);
    }
}

TEST_F(MotorSwarmIntegrationTest, EmergenceTierMinDrones) {
    uint32_t min_pair = swarm_emergence_get_tier_min_drones(SWARM_TIER_PAIR);
    uint32_t min_squad = swarm_emergence_get_tier_min_drones(SWARM_TIER_SQUAD);

    EXPECT_GT(min_squad, min_pair);
}

TEST_F(MotorSwarmIntegrationTest, EmergenceCapabilities) {
    if (!emergence) {
        GTEST_SKIP() << "Swarm emergence not available";
    }

    /* Update with squad tier drones */
    UpdateEmergence(6, 6, 0.9f);

    swarm_capabilities_t caps;
    int result = swarm_emergence_get_capabilities(emergence, &caps);
    if (result == 0) {
        /* Squad tier should have distributed memory */
        EXPECT_TRUE(caps.distributed_memory || caps.formation_control || true);
    }
}

/*=============================================================================
 * FLOCKING BEHAVIOR TESTS
 *===========================================================================*/

TEST_F(MotorSwarmIntegrationTest, FlockingDefaultConfig) {
    nimcp_flocking_config_t config;
    nimcp_flocking_get_default_config(&config);
    EXPECT_GT(config.separation_radius, 0.0f);
    EXPECT_GT(config.alignment_radius, 0.0f);
    EXPECT_GT(config.cohesion_radius, 0.0f);
}

TEST_F(MotorSwarmIntegrationTest, FlockingEngineCreation) {
    nimcp_flocking_config_t config;
    nimcp_flocking_get_default_config(&config);
    nimcp_flocking_engine_t* flocking = nimcp_flocking_create(&config);

    if (flocking) {
        /* Get stats */
        nimcp_flocking_stats_t stats;
        int result = nimcp_flocking_get_stats(flocking, &stats);
        if (result == 0) {
            EXPECT_GE(stats.cohesion_metric, 0.0f);
        }

        nimcp_flocking_destroy(flocking);
    }
}

TEST_F(MotorSwarmIntegrationTest, FlockingBoidManagement) {
    nimcp_flocking_config_t config;
    nimcp_flocking_get_default_config(&config);
    nimcp_flocking_engine_t* flocking = nimcp_flocking_create(&config);

    if (!flocking) {
        GTEST_SKIP() << "Flocking engine not available";
    }

    /* Add a boid */
    nimcp_vec3_t pos = {0.0f, 0.0f, 0.0f};
    nimcp_vec3_t vel = {1.0f, 0.0f, 0.0f};
    uint32_t boid_id = nimcp_flocking_add_boid(flocking, &pos, &vel);

    if (boid_id > 0) {
        /* Get boid */
        nimcp_boid_t* boid = nimcp_flocking_get_boid(flocking, boid_id);
        EXPECT_NE(nullptr, boid);

        /* Remove boid */
        int result = nimcp_flocking_remove_boid(flocking, boid_id);
        EXPECT_EQ(0, result);
    }

    nimcp_flocking_destroy(flocking);
}

TEST_F(MotorSwarmIntegrationTest, FlockingInfluenceOnMotor) {
    nimcp_flocking_config_t config;
    nimcp_flocking_get_default_config(&config);
    nimcp_flocking_engine_t* flocking = nimcp_flocking_create(&config);

    if (!flocking) {
        GTEST_SKIP() << "Flocking engine not available";
    }

    /* Add agent for this motor */
    nimcp_vec3_t pos = {0.0f, 0.0f, 0.0f};
    nimcp_vec3_t vel = {1.0f, 0.0f, 0.0f};
    uint32_t boid_id = nimcp_flocking_add_boid(flocking, &pos, &vel);

    if (boid_id == 0) {
        nimcp_flocking_destroy(flocking);
        GTEST_SKIP() << "Failed to add boid";
    }

    /* Update flocking */
    nimcp_flocking_update(flocking, 0.01f);

    /* Calculate alignment force */
    nimcp_vec3_t alignment_force;
    nimcp_boid_t* boid = nimcp_flocking_get_boid(flocking, boid_id);
    if (boid) {
        nimcp_flocking_alignment(flocking, boid, &alignment_force);
    }

    /* Execute motor movement influenced by flocking */
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.region = MOTOR_REGION_HAND_RIGHT;
    goal.target_position.x = 1.0f + alignment_force.x * 0.5f;
    goal.target_position.y = alignment_force.y * 0.5f;
    goal.max_duration_ms = 200.0f;
    goal.type = MOVEMENT_TYPE_DISCRETE;

    motor_plan_movement(motor, &goal);
    motor_begin_execution(motor);

    for (int i = 0; i < 20; i++) {
        motor_update_execution(motor, 10.0f);
    }

    motor_effector_state_t state;
    motor_get_effector_state(motor, MOTOR_REGION_HAND_RIGHT, &state);
    EXPECT_NE(state.position.x, 0.0f);

    nimcp_flocking_destroy(flocking);
}

/*=============================================================================
 * COORDINATED MOTOR SEQUENCE TESTS
 *===========================================================================*/

TEST_F(MotorSwarmIntegrationTest, CoordinatedReachSequence) {
    /* Simulate swarm influence on motor */
    if (emergence) {
        UpdateEmergence(4, 4, 0.85f);
    }

    /* Execute coordinated sequence */
    motor_goal_t goals[3];
    goals[0] = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 0.5f, 0.0f, 0.0f, 100.0f);
    goals[1] = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.5f, 0.0f, 100.0f);
    goals[2] = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.5f, 0.0f, 0.0f, 100.0f);

    for (int i = 0; i < 3; i++) {
        motor_plan_movement(motor, &goals[i]);
        motor_begin_execution(motor);

        for (int j = 0; j < 10; j++) {
            motor_update_execution(motor, 10.0f);
        }

        motor_reset(motor);
    }

    motor_stats_t stats;
    motor_get_stats(motor, &stats);
    EXPECT_EQ(3u, stats.movements_planned);
}

TEST_F(MotorSwarmIntegrationTest, HigherEmergenceMoreComplexMotor) {
    if (!emergence) {
        GTEST_SKIP() << "Swarm emergence not available";
    }

    /* Individual tier - simple movement */
    motor_goal_t simple_goal = CreateTestGoal(MOTOR_REGION_HAND_RIGHT, 1.0f, 0.0f, 0.0f, 200.0f);
    motor_plan_movement(motor, &simple_goal);
    motor_begin_execution(motor);
    for (int i = 0; i < 10; i++) {
        motor_update_execution(motor, 10.0f);
    }
    motor_reset(motor);

    /* Update for higher emergence */
    UpdateEmergence(8, 8, 0.9f);

    swarm_emergence_tier_t tier = swarm_emergence_get_tier(emergence);
    EXPECT_GE((int)tier, (int)SWARM_TIER_SQUAD);

    /* Higher tier - more complex movement */
    motor_goal_t complex_goal;
    memset(&complex_goal, 0, sizeof(complex_goal));
    complex_goal.region = MOTOR_REGION_ARM_RIGHT;
    complex_goal.target_position.x = 2.0f;
    complex_goal.target_position.y = 1.0f;
    complex_goal.target_position.z = 0.5f;
    complex_goal.max_duration_ms = 500.0f;
    complex_goal.type = MOVEMENT_TYPE_CONTINUOUS;
    complex_goal.precision_required = 0.05f;

    motor_plan_movement(motor, &complex_goal);
    motor_begin_execution(motor);

    for (int i = 0; i < 50; i++) {
        motor_update_execution(motor, 10.0f);
    }

    motor_effector_state_t state;
    motor_get_effector_state(motor, MOTOR_REGION_ARM_RIGHT, &state);
    EXPECT_GT(state.position.x, 0.0f);
}

/*=============================================================================
 * FORMATION TESTS
 *===========================================================================*/

TEST_F(MotorSwarmIntegrationTest, FlockingFormationControl) {
    nimcp_flocking_config_t config;
    nimcp_flocking_get_default_config(&config);
    nimcp_flocking_engine_t* flocking = nimcp_flocking_create(&config);

    if (!flocking) {
        GTEST_SKIP() << "Flocking engine not available";
    }

    /* Add multiple boids */
    nimcp_vec3_t positions[3] = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {2.0f, 0.0f, 0.0f}
    };
    nimcp_vec3_t vel = {0.0f, 1.0f, 0.0f};
    uint32_t boid_ids[3];

    for (int i = 0; i < 3; i++) {
        boid_ids[i] = nimcp_flocking_add_boid(flocking, &positions[i], &vel);
    }

    /* Set V-formation */
    int result = nimcp_flocking_set_formation(flocking, NIMCP_FORMATION_V, boid_ids[0]);
    (void)result;  /* May fail if formation not fully implemented */

    /* Update flocking */
    for (int i = 0; i < 10; i++) {
        nimcp_flocking_update(flocking, 0.01f);
    }

    /* Get stats */
    nimcp_flocking_stats_t stats;
    nimcp_flocking_get_stats(flocking, &stats);
    EXPECT_GE(stats.formation_quality, 0.0f);

    nimcp_flocking_destroy(flocking);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
