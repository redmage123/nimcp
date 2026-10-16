/**
 * @file test_lgss_context.cpp
 * @brief Unit tests for LGSS unified context
 *
 * Tests the main LGSS context functionality including:
 * - Context creation and destruction
 * - Rule loading from JSON
 * - Status transitions
 * - Evaluation through context
 * - Statistics tracking
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
#include "security/lgss/nimcp_lgss.h"
}

#include <cstring>
#include <cstdlib>

class LgssContextTest : public ::testing::Test {
protected:
    lgss_context_t* lgss = nullptr;
    lgss_config_t config;

    void SetUp() override {
        lgss_config_init(&config);
        // Use test rules path
        strncpy(config.rules_path, "test/fixtures/lgss_test_rules.json",
                NIMCP_LGSS_MAX_PATH - 1);
        config.auto_lock = false; // Don't auto-lock for most tests
    }

    void TearDown() override {
        if (lgss) {
            lgss_destroy(lgss);
            lgss = nullptr;
        }
    }
};

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_F(LgssContextTest, ConfigInitSetsDefaults) {
    lgss_config_t test_config;
    int result = lgss_config_init(&test_config);

    EXPECT_EQ(result, 0);
    EXPECT_TRUE(test_config.fail_safe_enabled);
    EXPECT_TRUE(test_config.telemetry_enabled);
    EXPECT_TRUE(test_config.verify_integrity_on_eval);
    EXPECT_TRUE(test_config.auto_lock);
    EXPECT_TRUE(test_config.bio_async_enabled);
    EXPECT_EQ(test_config.max_rules, SAFETY_MAX_RULES);
    EXPECT_EQ(test_config.default_timeout_ms, 5000u);
}

TEST_F(LgssContextTest, ConfigInitNullFails) {
    int result = lgss_config_init(nullptr);
    EXPECT_NE(result, 0);
}

// =============================================================================
// Context Lifecycle Tests
// =============================================================================

TEST_F(LgssContextTest, CreateWithConfig) {
    lgss = lgss_create(&config);
    ASSERT_NE(lgss, nullptr);
    EXPECT_EQ(lgss_get_status(lgss), LGSS_STATUS_LOADING);
}

TEST_F(LgssContextTest, CreateWithNullConfigUsesDefaults) {
    lgss = lgss_create(nullptr);
    ASSERT_NE(lgss, nullptr);
    lgss_destroy(lgss);
    lgss = nullptr;
}

TEST_F(LgssContextTest, DestroyNullIsSafe) {
    lgss_destroy(nullptr);
    // Should not crash
}

// =============================================================================
// Status Tests
// =============================================================================

TEST_F(LgssContextTest, StatusTransitions) {
    lgss = lgss_create(&config);
    ASSERT_NE(lgss, nullptr);

    // Initial status
    EXPECT_EQ(lgss_get_status(lgss), LGSS_STATUS_LOADING);

    // After failed load (no rules file)
    // Status would be ERROR if file doesn't exist
}

TEST_F(LgssContextTest, StatusNameConversion) {
    EXPECT_STREQ(lgss_status_name(LGSS_STATUS_UNINITIALIZED), "UNINITIALIZED");
    EXPECT_STREQ(lgss_status_name(LGSS_STATUS_LOADING), "LOADING");
    EXPECT_STREQ(lgss_status_name(LGSS_STATUS_COMPILING), "COMPILING");
    EXPECT_STREQ(lgss_status_name(LGSS_STATUS_LOCKING), "LOCKING");
    EXPECT_STREQ(lgss_status_name(LGSS_STATUS_ACTIVE), "ACTIVE");
    EXPECT_STREQ(lgss_status_name(LGSS_STATUS_DEGRADED), "DEGRADED");
    EXPECT_STREQ(lgss_status_name(LGSS_STATUS_HALTED), "HALTED");
    EXPECT_STREQ(lgss_status_name(LGSS_STATUS_ERROR), "ERROR");
    EXPECT_STREQ(lgss_status_name((lgss_status_t)99), "UNKNOWN");
}

// =============================================================================
// Locking Tests
// =============================================================================

TEST_F(LgssContextTest, IsLockedReturnsFalseInitially) {
    lgss = lgss_create(&config);
    ASSERT_NE(lgss, nullptr);
    EXPECT_FALSE(lgss_is_locked(lgss));
}

TEST_F(LgssContextTest, IsLockedNullReturnsFalse) {
    EXPECT_FALSE(lgss_is_locked(nullptr));
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(LgssContextTest, GetStatsInitial) {
    lgss = lgss_create(&config);
    ASSERT_NE(lgss, nullptr);

    lgss_stats_t stats;
    int result = lgss_get_stats(lgss, &stats);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_evaluations, 0u);
    EXPECT_EQ(stats.actions_denied, 0u);
    EXPECT_EQ(stats.actions_allowed, 0u);
    EXPECT_EQ(stats.integrity_checks, 0u);
    EXPECT_FALSE(stats.kb_locked);
}

TEST_F(LgssContextTest, GetStatsNullFails) {
    lgss = lgss_create(&config);
    ASSERT_NE(lgss, nullptr);

    lgss_stats_t stats;
    EXPECT_NE(lgss_get_stats(nullptr, &stats), 0);
    EXPECT_NE(lgss_get_stats(lgss, nullptr), 0);
}

TEST_F(LgssContextTest, ResetStats) {
    lgss = lgss_create(&config);
    ASSERT_NE(lgss, nullptr);

    int result = lgss_reset_stats(lgss);
    EXPECT_EQ(result, 0);
}

// =============================================================================
// Version Tests
// =============================================================================

TEST_F(LgssContextTest, VersionString) {
    const char* version = lgss_version_string();
    ASSERT_NE(version, nullptr);
    EXPECT_GT(strlen(version), 0u);

    // Should be in format X.Y.Z
    int major, minor, patch;
    int parsed = sscanf(version, "%d.%d.%d", &major, &minor, &patch);
    EXPECT_EQ(parsed, 3);
    EXPECT_GE(major, 0);
    EXPECT_GE(minor, 0);
    EXPECT_GE(patch, 0);
}

// =============================================================================
// Component Access Tests
// =============================================================================

TEST_F(LgssContextTest, GetSafetyKB) {
    lgss = lgss_create(&config);
    ASSERT_NE(lgss, nullptr);

    safety_kb_t* kb = lgss_get_safety_kb(lgss);
    EXPECT_NE(kb, nullptr);
}

TEST_F(LgssContextTest, GetInterceptor) {
    lgss = lgss_create(&config);
    ASSERT_NE(lgss, nullptr);

    action_interceptor_t interceptor = lgss_get_interceptor(lgss);
    EXPECT_NE(interceptor, nullptr);
}

TEST_F(LgssContextTest, GetOverrideController) {
    lgss = lgss_create(&config);
    ASSERT_NE(lgss, nullptr);

    override_controller_t ctrl = lgss_get_override_controller(lgss);
    EXPECT_NE(ctrl, nullptr);
}

TEST_F(LgssContextTest, ComponentAccessNullReturnsNull) {
    EXPECT_EQ(lgss_get_safety_kb(nullptr), nullptr);
    EXPECT_EQ(lgss_get_interceptor(nullptr), nullptr);
    EXPECT_EQ(lgss_get_override_controller(nullptr), nullptr);
}

// =============================================================================
// Integrity Tests
// =============================================================================

TEST_F(LgssContextTest, VerifyIntegrityNullFails) {
    int result = lgss_verify_integrity(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(LgssContextTest, GetHashNullFails) {
    uint8_t hash[32];
    int result = lgss_get_hash(nullptr, hash);
    EXPECT_NE(result, 0);
}

// =============================================================================
// Evaluation Tests (without rules loaded)
// =============================================================================

TEST_F(LgssContextTest, EvaluateWithoutActiveStatus) {
    lgss = lgss_create(&config);
    ASSERT_NE(lgss, nullptr);

    // LGSS is in LOADING state, not ACTIVE
    safety_action_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    strncpy(ctx.string_fields[0].key, "operation", 63);
    strncpy(ctx.string_fields[0].value, "test", SAFETY_MAX_VALUE_LEN - 1);
    ctx.num_string_fields = 1;

    safety_evaluation_t result;
    int ret = lgss_evaluate(lgss, &ctx, &result);

    // Should fail-safe to DENY when not active
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.action, SAFETY_ACTION_DENY);
}

TEST_F(LgssContextTest, EvaluateNullContextFails) {
    lgss = lgss_create(&config);
    ASSERT_NE(lgss, nullptr);

    safety_evaluation_t result;
    int ret = lgss_evaluate(lgss, nullptr, &result);
    EXPECT_NE(ret, 0);
}

TEST_F(LgssContextTest, EvaluateNullResultFails) {
    lgss = lgss_create(&config);
    ASSERT_NE(lgss, nullptr);

    safety_action_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    int ret = lgss_evaluate(lgss, &ctx, nullptr);
    EXPECT_NE(ret, 0);
}

TEST_F(LgssContextTest, CheckReturnsActionOnly) {
    lgss = lgss_create(&config);
    ASSERT_NE(lgss, nullptr);

    safety_action_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    strncpy(ctx.string_fields[0].key, "operation", 63);
    strncpy(ctx.string_fields[0].value, "test", SAFETY_MAX_VALUE_LEN - 1);
    ctx.num_string_fields = 1;

    // When not active, should fail-safe to DENY
    safety_action_t action = lgss_check(lgss, &ctx);
    EXPECT_EQ(action, SAFETY_ACTION_DENY);
}
