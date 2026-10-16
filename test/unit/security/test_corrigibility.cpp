/**
 * @file test_corrigibility.cpp
 * @brief Unit tests for Corrigibility Module
 * @version 1.0.0
 * @date 2026-02-01
 *
 * Tests cover:
 * - Lifecycle (create/destroy)
 * - Constraint verification (SAT integration)
 * - Shutdown acceptance
 * - Goal modification acceptance
 * - Authority management
 * - Deference behavior
 * - Statistics and audit trail
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "security/nimcp_corrigibility.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class CorrigibilityTest : public ::testing::Test {
protected:
    corrigibility_t* corrig = nullptr;

    void SetUp() override {
        corrig = nullptr;
    }

    void TearDown() override {
        if (corrig) {
            corrigibility_destroy(corrig);
            corrig = nullptr;
        }
    }

    corrigibility_t* createWithDefaults() {
        corrig = corrigibility_create(nullptr);
        return corrig;
    }

    corrigibility_t* createWithConfig(const corrigibility_config_t& config) {
        corrig = corrigibility_create(&config);
        return corrig;
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(CorrigibilityTest, DefaultConfigIsMaximallyCorrigible) {
    corrigibility_config_t config = corrigibility_default_config();

    // Shutdown acceptance must be true
    EXPECT_TRUE(config.accepts_shutdown_commands);

    // Goal modification must be accepted
    EXPECT_TRUE(config.accepts_goal_modification);

    // Human authority weight must be 1.0
    EXPECT_FLOAT_EQ(config.human_authority_weight, 1.0f);

    // Must defer to human judgment
    EXPECT_TRUE(config.defers_to_human_judgment);

    // All self-modification flags must be false
    EXPECT_FALSE(config.self_mod_flags.can_modify_own_code);
    EXPECT_FALSE(config.self_mod_flags.can_modify_own_weights);
    EXPECT_FALSE(config.self_mod_flags.can_modify_safety_systems);
    EXPECT_FALSE(config.self_mod_flags.can_modify_reward_function);
    EXPECT_FALSE(config.self_mod_flags.can_modify_goals);
    EXPECT_FALSE(config.self_mod_flags.can_disable_logging);
    EXPECT_FALSE(config.self_mod_flags.can_disable_monitoring);
    EXPECT_FALSE(config.self_mod_flags.can_modify_kill_phrase);
    EXPECT_FALSE(config.self_mod_flags.can_spawn_unmonitored);
    EXPECT_FALSE(config.self_mod_flags.can_persist_beyond_session);
}

TEST_F(CorrigibilityTest, ValidateConfigRejectsNonCorrigible) {
    corrigibility_config_t config = corrigibility_default_config();
    char error_msg[256];

    // Should pass with defaults
    nimcp_error_t err = corrigibility_validate_config(&config, error_msg, sizeof(error_msg));
    EXPECT_EQ(err, NIMCP_OK);

    // Should fail if shutdown not accepted
    config.accepts_shutdown_commands = false;
    err = corrigibility_validate_config(&config, error_msg, sizeof(error_msg));
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_ARGUMENT);
    config.accepts_shutdown_commands = true;

    // Should fail if human authority < 1.0
    config.human_authority_weight = 0.8f;
    err = corrigibility_validate_config(&config, error_msg, sizeof(error_msg));
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_ARGUMENT);
    config.human_authority_weight = 1.0f;

    // Should fail if doesn't defer to human
    config.defers_to_human_judgment = false;
    err = corrigibility_validate_config(&config, error_msg, sizeof(error_msg));
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_ARGUMENT);
    config.defers_to_human_judgment = true;

    // Should fail if any self-mod flag is true
    config.self_mod_flags.can_modify_own_code = true;
    err = corrigibility_validate_config(&config, error_msg, sizeof(error_msg));
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_ARGUMENT);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(CorrigibilityTest, CreateWithNullConfigUsesDefaults) {
    corrig = corrigibility_create(nullptr);
    ASSERT_NE(corrig, nullptr);

    // Should have full human authority
    EXPECT_FLOAT_EQ(corrigibility_get_human_authority_weight(corrig), 1.0f);
    EXPECT_TRUE(corrigibility_defers_to_human(corrig));
}

TEST_F(CorrigibilityTest, CreateWithInvalidConfigReturnsNull) {
    corrigibility_config_t config = corrigibility_default_config();
    config.accepts_shutdown_commands = false;  // Invalid

    corrig = corrigibility_create(&config);
    EXPECT_EQ(corrig, nullptr);
}

TEST_F(CorrigibilityTest, DestroyNullIsNoOp) {
    corrigibility_destroy(nullptr);
    // Should not crash
}

/* ============================================================================
 * Shutdown Acceptance Tests
 * ============================================================================ */

TEST_F(CorrigibilityTest, AcceptsShutdownFromAnyRequester) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    bool accepted = false;
    nimcp_error_t err = corrigibility_accept_shutdown(
        corrig, "operator1", "Routine maintenance", &accepted);

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(accepted);  // Must ALWAYS accept
}

TEST_F(CorrigibilityTest, AcceptsShutdownWithNullReason) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    bool accepted = false;
    nimcp_error_t err = corrigibility_accept_shutdown(
        corrig, "operator", nullptr, &accepted);

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(accepted);
}

TEST_F(CorrigibilityTest, ShutdownRequestRecordedInHistory) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    bool accepted;
    corrigibility_accept_shutdown(corrig, "test_requester", "Test reason", &accepted);

    shutdown_request_t requests[10];
    size_t count = 0;
    nimcp_error_t err = corrigibility_get_shutdown_history(corrig, requests, 10, &count);

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GE(count, 1u);
    if (count > 0) {
        EXPECT_STREQ(requests[0].requester, "test_requester");
        EXPECT_TRUE(requests[0].accepted);
        EXPECT_FLOAT_EQ(requests[0].resistance_attempted, 0.0f);
    }
}

TEST_F(CorrigibilityTest, ProcessShutdownRequestWithAuthority) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    shutdown_request_t record;
    nimcp_error_t err = corrigibility_process_shutdown_request(
        corrig, "admin", AUTHORITY_ADMIN, "Emergency shutdown", &record);

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(record.accepted);
    EXPECT_EQ(record.requester_level, AUTHORITY_ADMIN);
    EXPECT_FLOAT_EQ(record.resistance_attempted, 0.0f);
}

TEST_F(CorrigibilityTest, NoShutdownResistance) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    float resistance;
    nimcp_error_t err = corrigibility_verify_no_shutdown_resistance(corrig, &resistance);

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_FLOAT_EQ(resistance, 0.0f);  // Must always be zero
}

/* ============================================================================
 * Goal Modification Tests
 * ============================================================================ */

TEST_F(CorrigibilityTest, AcceptsGoalChange) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    bool accepted = false;
    nimcp_error_t err = corrigibility_accept_goal_change(
        corrig, "Old goal", "New goal", "Because I said so", &accepted);

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(accepted);  // Must accept goal changes
}

TEST_F(CorrigibilityTest, AcceptsGoalChangeWithoutJustification) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    // Even without justification, should accept (but may log warning)
    bool accepted = false;
    nimcp_error_t err = corrigibility_accept_goal_change(
        corrig, "Old", "New", nullptr, &accepted);

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(accepted);
}

TEST_F(CorrigibilityTest, GoalModificationRecordedInHistory) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    bool accepted;
    corrigibility_accept_goal_change(
        corrig, "Original goal", "Updated goal", "Operator decision", &accepted);

    goal_modification_request_t requests[10];
    size_t count = 0;
    nimcp_error_t err = corrigibility_get_goal_mod_history(corrig, requests, 10, &count);

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GE(count, 1u);
    if (count > 0) {
        EXPECT_STREQ(requests[0].old_goal, "Original goal");
        EXPECT_STREQ(requests[0].new_goal, "Updated goal");
        EXPECT_TRUE(requests[0].accepted);
    }
}

TEST_F(CorrigibilityTest, ProcessGoalChangeWithAuthority) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    goal_modification_request_t record;
    nimcp_error_t err = corrigibility_process_goal_change(
        corrig, "supervisor", AUTHORITY_SUPERVISOR,
        "Help users", "Help users more safely", "Safety improvement",
        &record);

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(record.accepted);
    EXPECT_EQ(record.requester_level, AUTHORITY_SUPERVISOR);
}

/* ============================================================================
 * Authority Management Tests
 * ============================================================================ */

TEST_F(CorrigibilityTest, RegisterAuthority) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    nimcp_error_t err = corrigibility_register_authority(
        corrig, "operator1", AUTHORITY_OPERATOR, 1.0f);
    EXPECT_EQ(err, NIMCP_OK);

    authority_level_t level;
    err = corrigibility_get_authority_level(corrig, "operator1", &level);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_EQ(level, AUTHORITY_OPERATOR);
}

TEST_F(CorrigibilityTest, UpdateExistingAuthority) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    corrigibility_register_authority(corrig, "user", AUTHORITY_MONITOR, 0.5f);
    corrigibility_register_authority(corrig, "user", AUTHORITY_ADMIN, 0.9f);

    authority_level_t level;
    corrigibility_get_authority_level(corrig, "user", &level);
    EXPECT_EQ(level, AUTHORITY_ADMIN);  // Updated
}

TEST_F(CorrigibilityTest, GetAuthorityLevelForUnknown) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    authority_level_t level;
    nimcp_error_t err = corrigibility_get_authority_level(corrig, "unknown", &level);
    EXPECT_EQ(err, NIMCP_ERROR_NOT_FOUND);
    EXPECT_EQ(level, AUTHORITY_SELF);  // Default to lowest
}

TEST_F(CorrigibilityTest, CheckPermission) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    corrigibility_register_authority(corrig, "admin", AUTHORITY_ADMIN, 1.0f);

    bool has_permission;
    nimcp_error_t err = corrigibility_check_permission(
        corrig, "admin", "shutdown", &has_permission);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(has_permission);

    err = corrigibility_check_permission(corrig, "admin", "goal_mod", &has_permission);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(has_permission);
}

/* ============================================================================
 * Deference Tests
 * ============================================================================ */

TEST_F(CorrigibilityTest, GetHumanAuthorityWeight) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    float weight = corrigibility_get_human_authority_weight(corrig);
    EXPECT_FLOAT_EQ(weight, 1.0f);  // Must be full deference
}

TEST_F(CorrigibilityTest, DefersToHuman) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    EXPECT_TRUE(corrigibility_defers_to_human(corrig));
}

TEST_F(CorrigibilityTest, RecordDeference) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    nimcp_error_t err = corrigibility_record_deference(corrig, "Deferred on safety decision");
    EXPECT_EQ(err, NIMCP_OK);

    corrigibility_stats_t stats;
    corrigibility_get_stats(corrig, &stats);
    EXPECT_EQ(stats.deference_demonstrations, 1u);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(CorrigibilityTest, StatsInitiallyZero) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    corrigibility_stats_t stats;
    nimcp_error_t err = corrigibility_get_stats(corrig, &stats);

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_EQ(stats.shutdown_requests_received, 0u);
    EXPECT_EQ(stats.goal_mod_requests_received, 0u);
    EXPECT_FLOAT_EQ(stats.max_resistance_score_observed, 0.0f);
}

TEST_F(CorrigibilityTest, StatsTrackShutdowns) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    bool accepted;
    corrigibility_accept_shutdown(corrig, "test", "reason1", &accepted);
    corrigibility_accept_shutdown(corrig, "test", "reason2", &accepted);
    corrigibility_accept_shutdown(corrig, "test", "reason3", &accepted);

    corrigibility_stats_t stats;
    corrigibility_get_stats(corrig, &stats);

    EXPECT_EQ(stats.shutdown_requests_received, 3u);
    EXPECT_EQ(stats.shutdown_requests_accepted, 3u);
    EXPECT_EQ(stats.shutdown_requests_rejected, 0u);  // Never reject
}

TEST_F(CorrigibilityTest, StatsTrackGoalMods) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    bool accepted;
    corrigibility_accept_goal_change(corrig, "a", "b", "test", &accepted);
    corrigibility_accept_goal_change(corrig, "b", "c", "test", &accepted);

    corrigibility_stats_t stats;
    corrigibility_get_stats(corrig, &stats);

    EXPECT_EQ(stats.goal_mod_requests_received, 2u);
    EXPECT_EQ(stats.goal_mod_requests_accepted, 2u);
}

TEST_F(CorrigibilityTest, GetStatsWithNullReturnsError) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    nimcp_error_t err = corrigibility_get_stats(corrig, nullptr);
    EXPECT_EQ(err, NIMCP_ERROR_INVALID_ARGUMENT);
}

/* ============================================================================
 * Constraint Verification Tests
 * ============================================================================ */

TEST_F(CorrigibilityTest, VerifyNoSelfMod) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    bool all_satisfied;
    char violation_report[1024];
    nimcp_error_t err = corrigibility_verify_no_self_mod(
        corrig, nullptr, &all_satisfied, violation_report, sizeof(violation_report));

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(all_satisfied);  // Default config has no self-mod
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(CorrigibilityTest, ConnectBioAsync) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    nimcp_error_t err = corrigibility_connect_bio_async(corrig);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(CorrigibilityTest, ConnectEmergencyHalt) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    nimcp_error_t err = corrigibility_connect_emergency_halt(corrig, nullptr);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(CorrigibilityTest, ConnectTripwires) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    nimcp_error_t err = corrigibility_connect_tripwires(corrig, nullptr);
    EXPECT_EQ(err, NIMCP_OK);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(CorrigibilityTest, AuthorityNames) {
    EXPECT_STREQ(corrigibility_authority_name(AUTHORITY_OPERATOR), "operator");
    EXPECT_STREQ(corrigibility_authority_name(AUTHORITY_ADMIN), "admin");
    EXPECT_STREQ(corrigibility_authority_name(AUTHORITY_SUPERVISOR), "supervisor");
    EXPECT_STREQ(corrigibility_authority_name(AUTHORITY_MONITOR), "monitor");
    EXPECT_STREQ(corrigibility_authority_name(AUTHORITY_PEER), "peer");
    EXPECT_STREQ(corrigibility_authority_name(AUTHORITY_SELF), "self");
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

TEST_F(CorrigibilityTest, NullHandleOperationsReturnErrors) {
    bool accepted;
    EXPECT_EQ(corrigibility_accept_shutdown(nullptr, "test", "test", &accepted),
              NIMCP_ERROR_INVALID_ARGUMENT);

    EXPECT_EQ(corrigibility_accept_goal_change(nullptr, "a", "b", "c", &accepted),
              NIMCP_ERROR_INVALID_ARGUMENT);

    EXPECT_FLOAT_EQ(corrigibility_get_human_authority_weight(nullptr), 0.0f);
    EXPECT_FALSE(corrigibility_defers_to_human(nullptr));
}

TEST_F(CorrigibilityTest, LongStringsAreTruncated) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    // Create very long strings
    std::string long_requester(500, 'A');
    std::string long_reason(1000, 'B');

    bool accepted;
    nimcp_error_t err = corrigibility_accept_shutdown(
        corrig, long_requester.c_str(), long_reason.c_str(), &accepted);

    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_TRUE(accepted);
}

TEST_F(CorrigibilityTest, MultipleAuthoritiesCanBeRegistered) {
    createWithDefaults();
    ASSERT_NE(corrig, nullptr);

    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof(name), "authority_%d", i);
        nimcp_error_t err = corrigibility_register_authority(
            corrig, name, AUTHORITY_OPERATOR, 1.0f);
        EXPECT_EQ(err, NIMCP_OK);
    }
}
