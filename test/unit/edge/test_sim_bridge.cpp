/**
 * @file test_sim_bridge.cpp
 * @brief Unit tests for NIMCP sim bridge — stub-mode cart-pole physics,
 *        domain randomization, sensor composition, and lifecycle.
 *
 * WHAT: Test sim bridge lifecycle, stepping, reset, sensor composition,
 *       domain randomization, state management, and NULL safety.
 * WHY:  The sim bridge is the sim-to-real pipeline; regressions here break
 *       simulated training and transfer to real hardware.
 * HOW:  Google Test, stub mode (built-in cart-pole, no real simulators).
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "edge/nimcp_sim_bridge.h"
}

// ============================================================================
// Lifecycle
// ============================================================================

TEST(SimBridge, CreateDestroyDefault) {
    nimcp_sim_bridge_t* bridge = nimcp_sim_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);
    nimcp_sim_bridge_destroy(bridge);
}

TEST(SimBridge, DestroyNull) {
    nimcp_sim_bridge_destroy(NULL);
    SUCCEED() << "nimcp_sim_bridge_destroy(NULL) did not crash";
}

// ============================================================================
// Config Defaults
// ============================================================================

TEST(SimBridge, ConfigDefaults) {
    nimcp_sim_config_t cfg = nimcp_sim_config_default();
    EXPECT_EQ(cfg.sim_type, NIMCP_SIM_GAZEBO);
    EXPECT_FLOAT_EQ(cfg.sim_step_hz, 240.0f);
    EXPECT_FLOAT_EQ(cfg.brain_hz, 30.0f);
    EXPECT_TRUE(cfg.sync_mode);
    EXPECT_FALSE(cfg.enable_domain_randomization);
}

// ============================================================================
// Connection (Stub Mode)
// ============================================================================

TEST(SimBridge, ConnectStubMode) {
    nimcp_sim_bridge_t* bridge = nimcp_sim_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    int rc = nimcp_sim_bridge_connect(bridge);
    EXPECT_EQ(rc, 0) << "Stub mode connect should always succeed";
    EXPECT_TRUE(nimcp_sim_bridge_is_connected(bridge));

    nimcp_sim_bridge_destroy(bridge);
}

TEST(SimBridge, DisconnectClearsFlag) {
    nimcp_sim_bridge_t* bridge = nimcp_sim_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);

    nimcp_sim_bridge_connect(bridge);
    ASSERT_TRUE(nimcp_sim_bridge_is_connected(bridge));

    int rc = nimcp_sim_bridge_disconnect(bridge);
    EXPECT_EQ(rc, 0);
    EXPECT_FALSE(nimcp_sim_bridge_is_connected(bridge));

    nimcp_sim_bridge_destroy(bridge);
}

// ============================================================================
// Stepping (Cart-Pole Stub)
// ============================================================================

TEST(SimBridge, StepReturnsState) {
    nimcp_sim_bridge_t* bridge = nimcp_sim_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(nimcp_sim_bridge_connect(bridge), 0);

    nimcp_sim_state_t* state = nimcp_sim_state_create(1);
    ASSERT_NE(state, nullptr);

    float action[1] = {0.1f};
    int rc = nimcp_sim_bridge_step(bridge, action, 1, state);
    EXPECT_EQ(rc, 0);

    // Cart-pole stub should have non-trivial body position or sim_time
    EXPECT_GT(state->sim_time, 0.0f) << "Sim time should advance after step";

    nimcp_sim_state_destroy(state);
    nimcp_sim_bridge_destroy(bridge);
}

TEST(SimBridge, MultipleStepsChangeState) {
    nimcp_sim_bridge_t* bridge = nimcp_sim_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(nimcp_sim_bridge_connect(bridge), 0);

    nimcp_sim_state_t* state = nimcp_sim_state_create(1);
    ASSERT_NE(state, nullptr);

    float action[1] = {0.5f};

    // Step once
    nimcp_sim_bridge_step(bridge, action, 1, state);
    float time1 = state->sim_time;

    // Step again
    nimcp_sim_bridge_step(bridge, action, 1, state);
    float time2 = state->sim_time;

    EXPECT_GT(time2, time1) << "Sim time should increase with steps";

    nimcp_sim_state_destroy(state);
    nimcp_sim_bridge_destroy(bridge);
}

// ============================================================================
// Reset
// ============================================================================

TEST(SimBridge, ResetToInitialState) {
    nimcp_sim_bridge_t* bridge = nimcp_sim_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(nimcp_sim_bridge_connect(bridge), 0);

    nimcp_sim_state_t* state = nimcp_sim_state_create(1);
    ASSERT_NE(state, nullptr);

    // Step several times to advance state
    float action[1] = {1.0f};
    for (int i = 0; i < 10; i++) {
        nimcp_sim_bridge_step(bridge, action, 1, state);
    }
    EXPECT_GT(state->sim_time, 0.0f);

    // Reset
    int rc = nimcp_sim_bridge_reset(bridge, state);
    EXPECT_EQ(rc, 0);
    EXPECT_NEAR(state->sim_time, 0.0f, 0.001f) << "Reset should bring sim_time back to ~0";

    nimcp_sim_state_destroy(state);
    nimcp_sim_bridge_destroy(bridge);
}

// ============================================================================
// Sensor Composition
// ============================================================================

TEST(SimBridge, ComposeSensorsFromState) {
    nimcp_sim_bridge_t* bridge = nimcp_sim_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(nimcp_sim_bridge_connect(bridge), 0);

    nimcp_sim_state_t* state = nimcp_sim_state_create(2);
    ASSERT_NE(state, nullptr);

    // Step to get a state
    float action[1] = {0.0f};
    nimcp_sim_bridge_step(bridge, action, 1, state);

    // Compose sensor features: 13 base + num_joints
    float features[32];
    memset(features, 0, sizeof(features));
    int n = nimcp_sim_bridge_compose_sensors(bridge, state, features, 32);
    EXPECT_GT(n, 0) << "Should compose at least some features";
    // With 2 joints: expect 13 + 2 = 15 features
    EXPECT_GE(n, 13) << "At least 13 base features expected";

    nimcp_sim_state_destroy(state);
    nimcp_sim_bridge_destroy(bridge);
}

// ============================================================================
// Domain Randomization
// ============================================================================

TEST(SimBridge, DomainRandomization) {
    nimcp_sim_config_t cfg = nimcp_sim_config_default();
    cfg.enable_domain_randomization = true;
    cfg.randomization = nimcp_domain_randomization_default();

    nimcp_sim_bridge_t* bridge = nimcp_sim_bridge_create(&cfg);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(nimcp_sim_bridge_connect(bridge), 0);

    int rc = nimcp_sim_bridge_randomize(bridge);
    EXPECT_EQ(rc, 0) << "Domain randomization should succeed in stub mode";

    nimcp_sim_bridge_destroy(bridge);
}

TEST(SimBridge, DomainRandomizationDefaults) {
    nimcp_domain_randomization_t dr = nimcp_domain_randomization_default();
    EXPECT_TRUE(dr.randomize_physics);
    EXPECT_TRUE(dr.randomize_visuals);
    EXPECT_TRUE(dr.randomize_sensors);
    EXPECT_TRUE(dr.randomize_dynamics);
    EXPECT_NEAR(dr.physics_range, 0.3f, 0.1f);
}

// ============================================================================
// Sim State Create/Destroy
// ============================================================================

TEST(SimBridge, SimStateCreateDestroy) {
    nimcp_sim_state_t* state = nimcp_sim_state_create(4);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->num_joints, 4u);
    EXPECT_NE(state->joint_positions, nullptr);
    EXPECT_NE(state->joint_velocities, nullptr);
    EXPECT_NE(state->body_position, nullptr);
    EXPECT_NE(state->body_orientation, nullptr);
    nimcp_sim_state_destroy(state);
}

TEST(SimBridge, SimStateDestroyNull) {
    nimcp_sim_state_destroy(NULL);
    SUCCEED() << "nimcp_sim_state_destroy(NULL) did not crash";
}

// ============================================================================
// Reward Signal
// ============================================================================

TEST(SimBridge, RewardSignal) {
    nimcp_sim_bridge_t* bridge = nimcp_sim_bridge_create(NULL);
    ASSERT_NE(bridge, nullptr);
    ASSERT_EQ(nimcp_sim_bridge_connect(bridge), 0);

    nimcp_sim_state_t* state = nimcp_sim_state_create(1);
    ASSERT_NE(state, nullptr);

    // After reset, the cart-pole should be balanced -> reward near 1.0
    nimcp_sim_bridge_reset(bridge, state);
    float action[1] = {0.0f};
    nimcp_sim_bridge_step(bridge, action, 1, state);
    EXPECT_GE(state->reward, 0.0f) << "Reward should be non-negative";

    nimcp_sim_state_destroy(state);
    nimcp_sim_bridge_destroy(bridge);
}

// ============================================================================
// NULL Safety
// ============================================================================

TEST(SimBridge, NullSafety) {
    float action[1] = {0.0f};
    nimcp_sim_state_t* state = nimcp_sim_state_create(1);
    ASSERT_NE(state, nullptr);

    EXPECT_LT(nimcp_sim_bridge_connect(NULL), 0);
    EXPECT_LT(nimcp_sim_bridge_disconnect(NULL), 0);
    EXPECT_FALSE(nimcp_sim_bridge_is_connected(NULL));
    EXPECT_LT(nimcp_sim_bridge_step(NULL, action, 1, state), 0);
    EXPECT_LT(nimcp_sim_bridge_reset(NULL, state), 0);
    EXPECT_LT(nimcp_sim_bridge_randomize(NULL), 0);

    float features[32];
    EXPECT_LT(nimcp_sim_bridge_compose_sensors(NULL, state, features, 32), 0);

    nimcp_sim_state_destroy(state);
}
