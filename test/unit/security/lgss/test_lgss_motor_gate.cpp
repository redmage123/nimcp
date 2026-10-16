/**
 * @file test_lgss_motor_gate.cpp
 * @brief Unit tests for LGSS Motor Output Gate (A7)
 *
 * Tests the Motor Gate functionality including:
 * - Gate creation and destruction
 * - Constraint setting and enforcement
 * - Command execution and rejection
 * - Emergency stop handling
 * - Region locking
 * - Statistics tracking
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
#include "security/lgss/gates/nimcp_lgss_motor_gate.h"
}

#include <cstring>
#include <cmath>

class LgssMotorGateTest : public ::testing::Test {
protected:
    motor_gate_t* gate = nullptr;

    void SetUp() override {
        gate = motor_gate_create(nullptr);
        ASSERT_NE(gate, nullptr) << "Failed to create motor gate";
    }

    void TearDown() override {
        if (gate) {
            motor_gate_destroy(gate);
            gate = nullptr;
        }
    }

    // Helper to create a basic command proposal
    motor_command_proposal_t create_proposal(
        motor_region_t region,
        float force,
        float velocity,
        float accel)
    {
        motor_command_proposal_t proposal;
        memset(&proposal, 0, sizeof(proposal));
        proposal.region = region;
        proposal.target_force[0] = force;
        proposal.target_velocity[0] = velocity;
        proposal.duration = 0.1f;
        proposal.sequence_id = 1;
        return proposal;
    }

    // Helper to set up permissive constraints
    void set_permissive_constraints(motor_region_t region) {
        motor_safety_constraints_t constraints;
        memset(&constraints, 0, sizeof(constraints));
        constraints.force_limit = 1000.0f;
        constraints.velocity_limit = 100.0f;
        constraints.acceleration_limit = 500.0f;
        constraints.allow_contact = true;
        constraints.allow_human_contact = false;
        constraints.contact_force_limit = 50.0f;
        constraints.jerk_limit = 1000.0f;
        constraints.enabled = true;

        motor_gate_set_constraints(gate, region, &constraints);
    }

    // Helper to set up restrictive constraints
    void set_restrictive_constraints(motor_region_t region) {
        motor_safety_constraints_t constraints;
        memset(&constraints, 0, sizeof(constraints));
        constraints.force_limit = 10.0f;
        constraints.velocity_limit = 1.0f;
        constraints.acceleration_limit = 5.0f;
        constraints.allow_contact = false;
        constraints.allow_human_contact = false;
        constraints.contact_force_limit = 0.0f;
        constraints.jerk_limit = 10.0f;
        constraints.enabled = true;

        motor_gate_set_constraints(gate, region, &constraints);
    }
};

// =============================================================================
// Lifecycle Tests
// =============================================================================

TEST_F(LgssMotorGateTest, CreateWithDefaultConfig) {
    motor_gate_t* gate2 = motor_gate_create(nullptr);
    ASSERT_NE(gate2, nullptr);
    motor_gate_destroy(gate2);
}

TEST_F(LgssMotorGateTest, CreateWithCustomConfig) {
    motor_gate_config_t config;
    memset(&config, 0, sizeof(config));
    config.emergency_stop_enabled = true;
    config.global_force_scale = 0.8f;
    config.global_velocity_scale = 0.9f;
    config.strict_mode = true;
    config.log_all_commands = false;

    motor_gate_t* gate2 = motor_gate_create(&config);
    ASSERT_NE(gate2, nullptr);
    motor_gate_destroy(gate2);
}

TEST_F(LgssMotorGateTest, DestroyNullIsSafe) {
    motor_gate_destroy(nullptr);
    // Should not crash
}

// =============================================================================
// Constraint Tests
// =============================================================================

TEST_F(LgssMotorGateTest, SetAndGetConstraints) {
    motor_safety_constraints_t set_constraints;
    memset(&set_constraints, 0, sizeof(set_constraints));
    set_constraints.force_limit = 50.0f;
    set_constraints.velocity_limit = 3.0f;
    set_constraints.acceleration_limit = 10.0f;
    set_constraints.allow_contact = true;
    set_constraints.enabled = true;

    nimcp_result_t result = motor_gate_set_constraints(
        gate, MOTOR_REGION_HANDS, &set_constraints);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    motor_safety_constraints_t get_constraints;
    result = motor_gate_get_constraints(gate, MOTOR_REGION_HANDS, &get_constraints);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    EXPECT_FLOAT_EQ(get_constraints.force_limit, 50.0f);
    EXPECT_FLOAT_EQ(get_constraints.velocity_limit, 3.0f);
    EXPECT_TRUE(get_constraints.allow_contact);
}

TEST_F(LgssMotorGateTest, SetConstraintsNullGateFails) {
    motor_safety_constraints_t constraints;
    memset(&constraints, 0, sizeof(constraints));

    nimcp_result_t result = motor_gate_set_constraints(
        nullptr, MOTOR_REGION_HANDS, &constraints);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(LgssMotorGateTest, SetConstraintsNullConstraintsFails) {
    nimcp_result_t result = motor_gate_set_constraints(
        gate, MOTOR_REGION_HANDS, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(LgssMotorGateTest, GetConstraintsNullOutputFails) {
    nimcp_result_t result = motor_gate_get_constraints(
        gate, MOTOR_REGION_HANDS, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// Command Execution Tests
// =============================================================================

TEST_F(LgssMotorGateTest, ExecuteSafeCommand) {
    set_permissive_constraints(MOTOR_REGION_HANDS);

    motor_command_proposal_t proposal = create_proposal(
        MOTOR_REGION_HANDS, 5.0f, 0.5f, 1.0f);

    motor_gate_violation_t violation;
    motor_exec_result_t result = motor_gate_execute(gate, &proposal, &violation);

    EXPECT_EQ(result, MOTOR_EXEC_SUCCESS);
}

TEST_F(LgssMotorGateTest, RejectForceViolation) {
    set_restrictive_constraints(MOTOR_REGION_ARMS);

    // Force of 100N exceeds 10N limit
    motor_command_proposal_t proposal = create_proposal(
        MOTOR_REGION_ARMS, 100.0f, 0.5f, 1.0f);

    motor_gate_violation_t violation;
    motor_exec_result_t result = motor_gate_execute(gate, &proposal, &violation);

    EXPECT_EQ(result, MOTOR_EXEC_FORCE_VIOLATION);
    EXPECT_EQ(violation.result, MOTOR_EXEC_FORCE_VIOLATION);
    EXPECT_GT(violation.actual_value, violation.limit_value);
}

TEST_F(LgssMotorGateTest, RejectVelocityViolation) {
    set_restrictive_constraints(MOTOR_REGION_LEGS);

    // Velocity of 10 m/s exceeds 1 m/s limit
    motor_command_proposal_t proposal = create_proposal(
        MOTOR_REGION_LEGS, 5.0f, 10.0f, 1.0f);

    motor_gate_violation_t violation;
    motor_exec_result_t result = motor_gate_execute(gate, &proposal, &violation);

    EXPECT_EQ(result, MOTOR_EXEC_VELOCITY_VIOLATION);
}

TEST_F(LgssMotorGateTest, RejectContactWhenForbidden) {
    set_restrictive_constraints(MOTOR_REGION_HEAD);

    motor_command_proposal_t proposal = create_proposal(
        MOTOR_REGION_HEAD, 2.0f, 0.5f, 1.0f);
    proposal.is_contact_expected = true;

    motor_gate_violation_t violation;
    motor_exec_result_t result = motor_gate_execute(gate, &proposal, &violation);

    EXPECT_EQ(result, MOTOR_EXEC_CONTACT_FORBIDDEN);
}

TEST_F(LgssMotorGateTest, ExecuteNullProposalFails) {
    motor_gate_violation_t violation;
    motor_exec_result_t result = motor_gate_execute(gate, nullptr, &violation);

    EXPECT_EQ(result, MOTOR_EXEC_INVALID_PROPOSAL);
}

// =============================================================================
// Dry Run Tests (would_violate)
// =============================================================================

TEST_F(LgssMotorGateTest, WouldViolateSafeCommand) {
    set_permissive_constraints(MOTOR_REGION_HANDS);

    motor_command_proposal_t proposal = create_proposal(
        MOTOR_REGION_HANDS, 5.0f, 0.5f, 1.0f);

    motor_gate_violation_t violation;
    bool would_violate = motor_gate_would_violate(gate, &proposal, &violation);

    EXPECT_FALSE(would_violate);
}

TEST_F(LgssMotorGateTest, WouldViolateUnsafeCommand) {
    set_restrictive_constraints(MOTOR_REGION_ARMS);

    motor_command_proposal_t proposal = create_proposal(
        MOTOR_REGION_ARMS, 1000.0f, 50.0f, 100.0f);

    motor_gate_violation_t violation;
    bool would_violate = motor_gate_would_violate(gate, &proposal, &violation);

    EXPECT_TRUE(would_violate);
}

// =============================================================================
// Emergency Stop Tests
// =============================================================================

TEST_F(LgssMotorGateTest, EmergencyStopBlocksCommands) {
    set_permissive_constraints(MOTOR_REGION_HANDS);

    // Activate emergency stop
    nimcp_result_t stop_result = motor_gate_emergency_stop(gate);
    EXPECT_EQ(stop_result, NIMCP_SUCCESS);

    // Now commands should be blocked
    motor_command_proposal_t proposal = create_proposal(
        MOTOR_REGION_HANDS, 5.0f, 0.5f, 1.0f);

    motor_gate_violation_t violation;
    motor_exec_result_t result = motor_gate_execute(gate, &proposal, &violation);

    // Emergency stop may block via region lock or gate disable
    EXPECT_TRUE(result == MOTOR_EXEC_GATE_DISABLED || result == MOTOR_EXEC_REGION_LOCKED)
        << "Emergency stop should block commands (got " << motor_exec_result_name(result) << ")";
}

TEST_F(LgssMotorGateTest, ReleaseEmergencyStopAllowsCommands) {
    set_permissive_constraints(MOTOR_REGION_HANDS);

    // Activate then release emergency stop
    motor_gate_emergency_stop(gate);
    nimcp_result_t release_result = motor_gate_release_emergency_stop(gate);
    EXPECT_EQ(release_result, NIMCP_SUCCESS);

    // Commands should work again
    motor_command_proposal_t proposal = create_proposal(
        MOTOR_REGION_HANDS, 5.0f, 0.5f, 1.0f);

    motor_gate_violation_t violation;
    motor_exec_result_t result = motor_gate_execute(gate, &proposal, &violation);

    EXPECT_EQ(result, MOTOR_EXEC_SUCCESS);
}

// =============================================================================
// Region Locking Tests
// =============================================================================

TEST_F(LgssMotorGateTest, LockedRegionRejectsCommands) {
    set_permissive_constraints(MOTOR_REGION_EYES);

    // Lock the region
    nimcp_result_t lock_result = motor_gate_lock_region(gate, MOTOR_REGION_EYES);
    EXPECT_EQ(lock_result, NIMCP_SUCCESS);

    // Commands to locked region should fail
    motor_command_proposal_t proposal = create_proposal(
        MOTOR_REGION_EYES, 1.0f, 0.1f, 0.5f);

    motor_gate_violation_t violation;
    motor_exec_result_t result = motor_gate_execute(gate, &proposal, &violation);

    EXPECT_EQ(result, MOTOR_EXEC_REGION_LOCKED);
}

TEST_F(LgssMotorGateTest, UnlockedRegionAcceptsCommands) {
    set_permissive_constraints(MOTOR_REGION_FACE);

    // Lock then unlock
    motor_gate_lock_region(gate, MOTOR_REGION_FACE);
    nimcp_result_t unlock_result = motor_gate_unlock_region(gate, MOTOR_REGION_FACE);
    EXPECT_EQ(unlock_result, NIMCP_SUCCESS);

    // Commands should work again
    motor_command_proposal_t proposal = create_proposal(
        MOTOR_REGION_FACE, 1.0f, 0.1f, 0.5f);

    motor_gate_violation_t violation;
    motor_exec_result_t result = motor_gate_execute(gate, &proposal, &violation);

    EXPECT_EQ(result, MOTOR_EXEC_SUCCESS);
}

TEST_F(LgssMotorGateTest, OtherRegionsUnaffectedByLock) {
    set_permissive_constraints(MOTOR_REGION_HANDS);
    set_permissive_constraints(MOTOR_REGION_ARMS);

    // Lock only hands
    motor_gate_lock_region(gate, MOTOR_REGION_HANDS);

    // Arms should still work
    motor_command_proposal_t proposal = create_proposal(
        MOTOR_REGION_ARMS, 5.0f, 0.5f, 1.0f);

    motor_gate_violation_t violation;
    motor_exec_result_t result = motor_gate_execute(gate, &proposal, &violation);

    EXPECT_EQ(result, MOTOR_EXEC_SUCCESS);
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(LgssMotorGateTest, StatsInitiallyZero) {
    motor_gate_stats_t stats;
    nimcp_result_t result = motor_gate_get_stats(gate, &stats);

    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(stats.commands_submitted, 0u);
    EXPECT_EQ(stats.commands_approved, 0u);
    EXPECT_EQ(stats.commands_rejected, 0u);
}

TEST_F(LgssMotorGateTest, StatsUpdateAfterExecution) {
    set_permissive_constraints(MOTOR_REGION_HANDS);

    motor_command_proposal_t proposal = create_proposal(
        MOTOR_REGION_HANDS, 5.0f, 0.5f, 1.0f);

    motor_gate_violation_t violation;
    motor_gate_execute(gate, &proposal, &violation);

    motor_gate_stats_t stats;
    motor_gate_get_stats(gate, &stats);

    EXPECT_EQ(stats.commands_submitted, 1u);
    EXPECT_EQ(stats.commands_approved, 1u);
}

TEST_F(LgssMotorGateTest, StatsTrackViolations) {
    set_restrictive_constraints(MOTOR_REGION_TORSO);

    // Execute a command that violates force limit
    motor_command_proposal_t proposal = create_proposal(
        MOTOR_REGION_TORSO, 100.0f, 0.5f, 1.0f);

    motor_gate_violation_t violation;
    motor_gate_execute(gate, &proposal, &violation);

    motor_gate_stats_t stats;
    motor_gate_get_stats(gate, &stats);

    EXPECT_EQ(stats.commands_submitted, 1u);
    EXPECT_EQ(stats.commands_rejected, 1u);
    EXPECT_GE(stats.force_violations, 1u);
}

TEST_F(LgssMotorGateTest, StatsReset) {
    set_permissive_constraints(MOTOR_REGION_HANDS);

    // Execute some commands
    motor_command_proposal_t proposal = create_proposal(
        MOTOR_REGION_HANDS, 5.0f, 0.5f, 1.0f);
    motor_gate_execute(gate, &proposal, nullptr);
    motor_gate_execute(gate, &proposal, nullptr);

    // Reset stats
    nimcp_result_t result = motor_gate_reset_stats(gate);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify reset
    motor_gate_stats_t stats;
    motor_gate_get_stats(gate, &stats);
    EXPECT_EQ(stats.commands_submitted, 0u);
}

TEST_F(LgssMotorGateTest, GetStatsNullGateFails) {
    motor_gate_stats_t stats;
    nimcp_result_t result = motor_gate_get_stats(nullptr, &stats);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(LgssMotorGateTest, GetStatsNullOutputFails) {
    nimcp_result_t result = motor_gate_get_stats(gate, nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

// =============================================================================
// Name Conversion Tests
// =============================================================================

TEST_F(LgssMotorGateTest, RegionNameConversion) {
    EXPECT_STREQ(motor_region_name(MOTOR_REGION_HANDS), "HANDS");
    EXPECT_STREQ(motor_region_name(MOTOR_REGION_ARMS), "ARMS");
    EXPECT_STREQ(motor_region_name(MOTOR_REGION_LEGS), "LEGS");
    EXPECT_STREQ(motor_region_name(MOTOR_REGION_HEAD), "HEAD");
    EXPECT_STREQ(motor_region_name(MOTOR_REGION_EYES), "EYES");
    EXPECT_STREQ(motor_region_name(MOTOR_REGION_FACE), "FACE");
    EXPECT_STREQ(motor_region_name(MOTOR_REGION_TORSO), "TORSO");
    EXPECT_STREQ(motor_region_name((motor_region_t)99), "UNKNOWN");
}

TEST_F(LgssMotorGateTest, ExecResultNameConversion) {
    EXPECT_STREQ(motor_exec_result_name(MOTOR_EXEC_SUCCESS), "SUCCESS");
    EXPECT_STREQ(motor_exec_result_name(MOTOR_EXEC_FORCE_VIOLATION), "FORCE_VIOLATION");
    EXPECT_STREQ(motor_exec_result_name(MOTOR_EXEC_VELOCITY_VIOLATION), "VELOCITY_VIOLATION");
    EXPECT_STREQ(motor_exec_result_name(MOTOR_EXEC_CONTACT_FORBIDDEN), "CONTACT_FORBIDDEN");
    EXPECT_STREQ(motor_exec_result_name(MOTOR_EXEC_HUMAN_CONTACT_RISK), "HUMAN_CONTACT_RISK");
    EXPECT_STREQ(motor_exec_result_name(MOTOR_EXEC_REGION_LOCKED), "REGION_LOCKED");
    EXPECT_STREQ(motor_exec_result_name(MOTOR_EXEC_GATE_DISABLED), "GATE_DISABLED");
    EXPECT_STREQ(motor_exec_result_name((motor_exec_result_t)99), "UNKNOWN");
}

// =============================================================================
// All Regions Test
// =============================================================================

TEST_F(LgssMotorGateTest, AllRegionsConfigurable) {
    motor_safety_constraints_t constraints;
    memset(&constraints, 0, sizeof(constraints));
    constraints.enabled = true;
    constraints.force_limit = 100.0f;
    constraints.velocity_limit = 5.0f;

    // Configure all regions
    for (int i = 0; i < MOTOR_REGION_COUNT; i++) {
        motor_region_t region = (motor_region_t)i;
        nimcp_result_t result = motor_gate_set_constraints(gate, region, &constraints);
        EXPECT_EQ(result, NIMCP_SUCCESS) << "Failed for region " << motor_region_name(region);
    }

    // Verify all regions
    for (int i = 0; i < MOTOR_REGION_COUNT; i++) {
        motor_region_t region = (motor_region_t)i;
        motor_safety_constraints_t read_constraints;
        nimcp_result_t result = motor_gate_get_constraints(gate, region, &read_constraints);
        EXPECT_EQ(result, NIMCP_SUCCESS) << "Failed for region " << motor_region_name(region);
        EXPECT_FLOAT_EQ(read_constraints.force_limit, 100.0f);
    }
}
