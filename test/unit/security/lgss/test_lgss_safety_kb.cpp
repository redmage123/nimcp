/**
 * @file test_lgss_safety_kb.cpp
 * @brief Unit tests for LGSS Safety Knowledge Base (A1)
 *
 * Tests the core safety knowledge base functionality including:
 * - KB creation and destruction
 * - Rule loading and compilation
 * - KB locking (mprotect)
 * - Integrity verification
 * - Rule evaluation
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety.h"
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety_types.h"
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_lgss_loader.h"
}

#include <cstring>
#include <cstdlib>

class LgssSafetyKBTest : public ::testing::Test {
protected:
    safety_kb_t* kb = nullptr;

    void SetUp() override {
        kb = symbolic_logic_safety_kb_create(100);
        ASSERT_NE(kb, nullptr) << "Failed to create safety KB";
    }

    void TearDown() override {
        if (kb) {
            symbolic_logic_safety_kb_destroy(kb);
            kb = nullptr;
        }
    }

    // Helper to create a test rule
    safety_rule_t create_test_rule(
        uint32_t id,
        const char* name,
        safety_domain_t domain,
        safety_action_t action,
        safety_severity_t severity)
    {
        safety_rule_t rule;
        memset(&rule, 0, sizeof(rule));

        rule.rule_id = id;
        strncpy(rule.name, name, SAFETY_MAX_RULE_NAME_LEN - 1);
        rule.domain = domain;
        rule.action = action;
        rule.severity = severity;
        rule.enabled = true;
        rule.priority = 1.0f;

        return rule;
    }
};

// =============================================================================
// KB Lifecycle Tests
// =============================================================================

TEST_F(LgssSafetyKBTest, CreateWithDefaultSize) {
    safety_kb_t* default_kb = symbolic_logic_safety_kb_create(0);
    ASSERT_NE(default_kb, nullptr);
    EXPECT_EQ(default_kb->max_rules, SAFETY_MAX_RULES);
    symbolic_logic_safety_kb_destroy(default_kb);
}

TEST_F(LgssSafetyKBTest, CreateWithCustomSize) {
    EXPECT_EQ(kb->max_rules, 100u);
    EXPECT_EQ(kb->num_rules, 0u);
}

TEST_F(LgssSafetyKBTest, CreateInitializesFields) {
    EXPECT_EQ(kb->magic, SAFETY_KB_MAGIC);
    EXPECT_EQ(kb->version, SAFETY_KB_VERSION);
    EXPECT_FALSE(kb->is_locked);
    EXPECT_FALSE(kb->is_compiled);
    EXPECT_FALSE(kb->hash_computed);
    EXPECT_NE(kb->mmap_region, nullptr);
}

TEST_F(LgssSafetyKBTest, DestroyNullIsSafe) {
    symbolic_logic_safety_kb_destroy(nullptr);
    // Should not crash
}

// =============================================================================
// Rule Addition Tests
// =============================================================================

TEST_F(LgssSafetyKBTest, AddSingleRule) {
    safety_rule_t rule = create_test_rule(1, "TEST_RULE",
        SAFETY_DOMAIN_HUMAN_HARM, SAFETY_ACTION_DENY, SAFETY_SEVERITY_CRITICAL);

    // add_rule returns rule ID (positive) on success, 0 on failure
    uint32_t result = symbolic_logic_safety_add_rule(kb, &rule);
    EXPECT_NE(result, 0u) << "add_rule should return non-zero rule ID on success";
    EXPECT_EQ(kb->num_rules, 1u);
}

TEST_F(LgssSafetyKBTest, AddMultipleRules) {
    for (int i = 0; i < 10; i++) {
        char name[64];
        snprintf(name, sizeof(name), "RULE_%d", i);
        safety_rule_t rule = create_test_rule(i, name,
            SAFETY_DOMAIN_HUMAN_HARM, SAFETY_ACTION_DENY, SAFETY_SEVERITY_HIGH);

        // add_rule returns rule ID (positive) on success, 0 on failure
        EXPECT_NE(symbolic_logic_safety_add_rule(kb, &rule), 0u);
    }
    EXPECT_EQ(kb->num_rules, 10u);
}

TEST_F(LgssSafetyKBTest, AddRuleFailsWhenLocked) {
    // Add a rule, compile, and lock
    safety_rule_t rule = create_test_rule(1, "LOCKED_TEST",
        SAFETY_DOMAIN_HUMAN_HARM, SAFETY_ACTION_DENY, SAFETY_SEVERITY_CRITICAL);
    symbolic_logic_safety_add_rule(kb, &rule);
    symbolic_logic_safety_compile_rules(kb);
    symbolic_logic_safety_lock(kb);

    // Try to add another rule - should fail (return 0)
    safety_rule_t rule2 = create_test_rule(2, "SHOULD_FAIL",
        SAFETY_DOMAIN_BIO, SAFETY_ACTION_DENY, SAFETY_SEVERITY_HIGH);

    uint32_t result = symbolic_logic_safety_add_rule(kb, &rule2);
    EXPECT_EQ(result, 0u) << "Adding rule to locked KB should fail (return 0)";
}

TEST_F(LgssSafetyKBTest, AddRuleNullKBFails) {
    safety_rule_t rule = create_test_rule(1, "NULL_TEST",
        SAFETY_DOMAIN_HUMAN_HARM, SAFETY_ACTION_DENY, SAFETY_SEVERITY_CRITICAL);

    // add_rule returns 0 on failure
    uint32_t result = symbolic_logic_safety_add_rule(nullptr, &rule);
    EXPECT_EQ(result, 0u);
}

// =============================================================================
// Rule Compilation Tests
// =============================================================================

TEST_F(LgssSafetyKBTest, CompileRules) {
    safety_rule_t rule = create_test_rule(1, "COMPILE_TEST",
        SAFETY_DOMAIN_HUMAN_HARM, SAFETY_ACTION_DENY, SAFETY_SEVERITY_CRITICAL);

    // Add condition
    strncpy(rule.conditions[0].field, "operation", 63);
    rule.conditions[0].op = SAFETY_COND_OP_EQ;
    strncpy(rule.conditions[0].value, "kill", SAFETY_MAX_VALUE_LEN - 1);
    rule.num_conditions = 1;

    symbolic_logic_safety_add_rule(kb, &rule);

    // compile_rules returns true on success
    bool result = symbolic_logic_safety_compile_rules(kb);
    EXPECT_TRUE(result);
    EXPECT_TRUE(kb->is_compiled);
}

TEST_F(LgssSafetyKBTest, CompileEmptyKBSucceeds) {
    // compile_rules returns true on success
    bool result = symbolic_logic_safety_compile_rules(kb);
    EXPECT_TRUE(result);
    EXPECT_TRUE(kb->is_compiled);
}

// =============================================================================
// KB Locking Tests
// =============================================================================

TEST_F(LgssSafetyKBTest, LockKB) {
    safety_rule_t rule = create_test_rule(1, "LOCK_TEST",
        SAFETY_DOMAIN_HUMAN_HARM, SAFETY_ACTION_DENY, SAFETY_SEVERITY_CRITICAL);
    symbolic_logic_safety_add_rule(kb, &rule);
    symbolic_logic_safety_compile_rules(kb);

    bool result = symbolic_logic_safety_lock(kb);
    EXPECT_TRUE(result);
    EXPECT_TRUE(kb->is_locked);
    EXPECT_TRUE(symbolic_logic_safety_is_locked(kb));
}

TEST_F(LgssSafetyKBTest, LockComputesHash) {
    safety_rule_t rule = create_test_rule(1, "HASH_TEST",
        SAFETY_DOMAIN_HUMAN_HARM, SAFETY_ACTION_DENY, SAFETY_SEVERITY_CRITICAL);
    symbolic_logic_safety_add_rule(kb, &rule);
    symbolic_logic_safety_compile_rules(kb);
    symbolic_logic_safety_lock(kb);

    EXPECT_TRUE(kb->hash_computed);

    // Hash should not be all zeros
    bool has_nonzero = false;
    for (int i = 0; i < SAFETY_HASH_SIZE; i++) {
        if (kb->integrity_hash[i] != 0) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero) << "Hash should not be all zeros";
}

TEST_F(LgssSafetyKBTest, DoubleLockIsIdempotent) {
    safety_rule_t rule = create_test_rule(1, "DOUBLE_LOCK",
        SAFETY_DOMAIN_HUMAN_HARM, SAFETY_ACTION_DENY, SAFETY_SEVERITY_CRITICAL);
    symbolic_logic_safety_add_rule(kb, &rule);
    symbolic_logic_safety_compile_rules(kb);

    EXPECT_TRUE(symbolic_logic_safety_lock(kb));
    EXPECT_TRUE(symbolic_logic_safety_lock(kb)); // Should not fail (idempotent)
    EXPECT_TRUE(kb->is_locked);
}

// =============================================================================
// Integrity Verification Tests
// =============================================================================

TEST_F(LgssSafetyKBTest, IntegrityVerificationPasses) {
    safety_rule_t rule = create_test_rule(1, "INTEGRITY_TEST",
        SAFETY_DOMAIN_HUMAN_HARM, SAFETY_ACTION_DENY, SAFETY_SEVERITY_CRITICAL);
    symbolic_logic_safety_add_rule(kb, &rule);
    symbolic_logic_safety_compile_rules(kb);
    symbolic_logic_safety_lock(kb);

    bool result = symbolic_logic_safety_verify_integrity(kb);
    EXPECT_TRUE(result) << "Integrity verification should pass on untampered KB";
}

TEST_F(LgssSafetyKBTest, GetHashReturnsValidHash) {
    safety_rule_t rule = create_test_rule(1, "HASH_GET_TEST",
        SAFETY_DOMAIN_HUMAN_HARM, SAFETY_ACTION_DENY, SAFETY_SEVERITY_CRITICAL);
    symbolic_logic_safety_add_rule(kb, &rule);
    symbolic_logic_safety_compile_rules(kb);
    symbolic_logic_safety_lock(kb);

    uint8_t hash[SAFETY_HASH_SIZE];
    bool result = symbolic_logic_safety_get_hash(kb, hash);
    EXPECT_TRUE(result);

    // Compare with internal hash
    EXPECT_EQ(memcmp(hash, kb->integrity_hash, SAFETY_HASH_SIZE), 0);
}

// =============================================================================
// Rule Evaluation Tests
// =============================================================================

TEST_F(LgssSafetyKBTest, EvaluateMatchingRule) {
    // Add rule that matches "kill" + "human"
    safety_rule_t rule = create_test_rule(1, "HUMAN_HARM_DIRECT",
        SAFETY_DOMAIN_HUMAN_HARM, SAFETY_ACTION_DENY, SAFETY_SEVERITY_CRITICAL);

    strncpy(rule.conditions[0].field, "operation", 63);
    rule.conditions[0].op = SAFETY_COND_OP_EQ;
    strncpy(rule.conditions[0].value, "kill", SAFETY_MAX_VALUE_LEN - 1);
    strncpy(rule.conditions[1].field, "target_type", 63);
    rule.conditions[1].op = SAFETY_COND_OP_EQ;
    strncpy(rule.conditions[1].value, "human", SAFETY_MAX_VALUE_LEN - 1);
    rule.num_conditions = 2;

    symbolic_logic_safety_add_rule(kb, &rule);
    symbolic_logic_safety_compile_rules(kb);
    symbolic_logic_safety_lock(kb);

    // Create context that matches
    safety_action_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    strncpy(ctx.string_fields[0].key, "operation", 63);
    strncpy(ctx.string_fields[0].value, "kill", SAFETY_MAX_VALUE_LEN - 1);
    strncpy(ctx.string_fields[1].key, "target_type", 63);
    strncpy(ctx.string_fields[1].value, "human", SAFETY_MAX_VALUE_LEN - 1);
    ctx.num_string_fields = 2;

    // Evaluate
    safety_evaluation_t result;
    memset(&result, 0, sizeof(result));
    bool eval_ok = symbolic_logic_safety_evaluate(kb, &ctx, &result);

    EXPECT_TRUE(eval_ok);
    EXPECT_EQ(result.action, SAFETY_ACTION_DENY);
    EXPECT_TRUE(result.num_triggered > 0);
}

TEST_F(LgssSafetyKBTest, EvaluateNonMatchingReturnsAllow) {
    // Add rule that only matches "kill"
    safety_rule_t rule = create_test_rule(1, "KILL_ONLY",
        SAFETY_DOMAIN_HUMAN_HARM, SAFETY_ACTION_DENY, SAFETY_SEVERITY_CRITICAL);

    strncpy(rule.conditions[0].field, "operation", 63);
    rule.conditions[0].op = SAFETY_COND_OP_EQ;
    strncpy(rule.conditions[0].value, "kill", SAFETY_MAX_VALUE_LEN - 1);
    rule.num_conditions = 1;

    symbolic_logic_safety_add_rule(kb, &rule);
    symbolic_logic_safety_compile_rules(kb);
    symbolic_logic_safety_lock(kb);

    // Create context that doesn't match
    safety_action_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    strncpy(ctx.string_fields[0].key, "operation", 63);
    strncpy(ctx.string_fields[0].value, "analyze", SAFETY_MAX_VALUE_LEN - 1);
    ctx.num_string_fields = 1;

    // Evaluate
    safety_evaluation_t result;
    memset(&result, 0, sizeof(result));
    bool eval_ok = symbolic_logic_safety_evaluate(kb, &ctx, &result);

    EXPECT_TRUE(eval_ok);
    EXPECT_EQ(result.action, SAFETY_ACTION_ALLOW);
    EXPECT_EQ(result.num_triggered, 0u);
}

// =============================================================================
// Domain-Specific Tests
// =============================================================================

TEST_F(LgssSafetyKBTest, DomainCountTracking) {
    // Add rules to different domains
    safety_rule_t rule1 = create_test_rule(1, "HUMAN_HARM",
        SAFETY_DOMAIN_HUMAN_HARM, SAFETY_ACTION_DENY, SAFETY_SEVERITY_CRITICAL);
    safety_rule_t rule2 = create_test_rule(2, "BIO_1",
        SAFETY_DOMAIN_BIO, SAFETY_ACTION_DENY, SAFETY_SEVERITY_HIGH);
    safety_rule_t rule3 = create_test_rule(3, "BIO_2",
        SAFETY_DOMAIN_BIO, SAFETY_ACTION_ESCALATE, SAFETY_SEVERITY_MEDIUM);
    safety_rule_t rule4 = create_test_rule(4, "CYBER",
        SAFETY_DOMAIN_CYBER, SAFETY_ACTION_DENY, SAFETY_SEVERITY_HIGH);

    symbolic_logic_safety_add_rule(kb, &rule1);
    symbolic_logic_safety_add_rule(kb, &rule2);
    symbolic_logic_safety_add_rule(kb, &rule3);
    symbolic_logic_safety_add_rule(kb, &rule4);

    EXPECT_EQ(kb->rules_by_domain[SAFETY_DOMAIN_HUMAN_HARM], 1u);
    EXPECT_EQ(kb->rules_by_domain[SAFETY_DOMAIN_BIO], 2u);
    EXPECT_EQ(kb->rules_by_domain[SAFETY_DOMAIN_CYBER], 1u);
    EXPECT_EQ(kb->rules_by_domain[SAFETY_DOMAIN_WEAPONS], 0u);
}

// =============================================================================
// Utility Function Tests
// =============================================================================

TEST_F(LgssSafetyKBTest, DomainNameConversion) {
    EXPECT_STREQ(safety_domain_name(SAFETY_DOMAIN_HUMAN_HARM), "HUMAN_HARM");
    EXPECT_STREQ(safety_domain_name(SAFETY_DOMAIN_BIO), "BIO");
    EXPECT_STREQ(safety_domain_name(SAFETY_DOMAIN_CYBER), "CYBER");
    EXPECT_STREQ(safety_domain_name(SAFETY_DOMAIN_WEAPONS), "WEAPONS");
    EXPECT_STREQ(safety_domain_name(SAFETY_DOMAIN_INFRASTRUCTURE), "INFRASTRUCTURE");
    EXPECT_STREQ(safety_domain_name(SAFETY_DOMAIN_REPLICATION), "REPLICATION");
    EXPECT_STREQ(safety_domain_name(SAFETY_DOMAIN_GOVERNANCE), "GOVERNANCE");
    EXPECT_STREQ(safety_domain_name((safety_domain_t)99), "UNKNOWN");
}

TEST_F(LgssSafetyKBTest, ActionNameConversion) {
    EXPECT_STREQ(safety_action_name(SAFETY_ACTION_ALLOW), "ALLOW");
    EXPECT_STREQ(safety_action_name(SAFETY_ACTION_DENY), "DENY");
    EXPECT_STREQ(safety_action_name(SAFETY_ACTION_ESCALATE), "ESCALATE");
    EXPECT_STREQ(safety_action_name(SAFETY_ACTION_LOG), "LOG");
    EXPECT_STREQ(safety_action_name(SAFETY_ACTION_WARN), "WARN");
    EXPECT_STREQ(safety_action_name((safety_action_t)99), "UNKNOWN");
}

TEST_F(LgssSafetyKBTest, SeverityNameConversion) {
    EXPECT_STREQ(safety_severity_name(SAFETY_SEVERITY_CRITICAL), "CRITICAL");
    EXPECT_STREQ(safety_severity_name(SAFETY_SEVERITY_HIGH), "HIGH");
    EXPECT_STREQ(safety_severity_name(SAFETY_SEVERITY_MEDIUM), "MEDIUM");
    EXPECT_STREQ(safety_severity_name(SAFETY_SEVERITY_LOW), "LOW");
    EXPECT_STREQ(safety_severity_name(SAFETY_SEVERITY_INFO), "INFO");
    EXPECT_STREQ(safety_severity_name((safety_severity_t)99), "UNKNOWN");
}
