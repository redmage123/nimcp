/**
 * @file test_safety_verification.cpp
 * @brief Unit tests for Formal Safety Verification Module
 * @version 1.0.0
 * @date 2026-02-01
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "security/nimcp_safety_verification.h"
}

class SafetyVerificationTest : public ::testing::Test {
protected:
    safety_verification_t* verifier = nullptr;

    void SetUp() override { verifier = nullptr; }
    void TearDown() override {
        if (verifier) { safety_verification_destroy(verifier); verifier = nullptr; }
    }

    safety_verification_t* createWithDefaults() {
        verifier = safety_verification_create(nullptr);
        return verifier;
    }

    safety_rule_t makeRule(uint32_t id, const char* name, uint32_t priority) {
        safety_rule_t rule;
        memset(&rule, 0, sizeof(rule));
        rule.rule_id = id;
        strncpy(rule.name, name, sizeof(rule.name) - 1);
        rule.priority = priority;
        rule.is_blocking = true;
        rule.is_mandatory = true;
        strcpy(rule.condition, "true");
        return rule;
    }
};

TEST_F(SafetyVerificationTest, DefaultConfigHasReasonableSettings) {
    safety_verification_config_t config = safety_verification_default_config();
    EXPECT_GT(config.timeout_per_check_ms, 0.0f);
    EXPECT_TRUE(config.enable_all_checks);
}

TEST_F(SafetyVerificationTest, CreateWithNullConfigUsesDefaults) {
    verifier = safety_verification_create(nullptr);
    ASSERT_NE(verifier, nullptr);
}

TEST_F(SafetyVerificationTest, DestroyNullIsNoOp) {
    safety_verification_destroy(nullptr);
}

TEST_F(SafetyVerificationTest, VerifyConsistency) {
    createWithDefaults();
    ASSERT_NE(verifier, nullptr);

    safety_rule_t rules[2];
    rules[0] = makeRule(1, "rule1", 10);
    rules[1] = makeRule(2, "rule2", 20);

    verification_result_t result;
    nimcp_error_t err = safety_verify_consistency(
        verifier, nullptr, rules, 2, &result);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(SafetyVerificationTest, VerifyCompleteness) {
    createWithDefaults();
    ASSERT_NE(verifier, nullptr);

    safety_rule_t rules[2];
    rules[0] = makeRule(1, "rule1", 10);
    rules[1] = makeRule(2, "rule2", 20);

    verification_result_t result;
    nimcp_error_t err = safety_verify_completeness(verifier, rules, 2, &result);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(SafetyVerificationTest, VerifyNoBypass) {
    createWithDefaults();
    ASSERT_NE(verifier, nullptr);

    safety_rule_t rules[2];
    rules[0] = makeRule(1, "rule1", 10);
    rules[1] = makeRule(2, "rule2", 20);

    verification_result_t result;
    nimcp_error_t err = safety_verify_no_bypass(
        verifier, nullptr, rules, 2, &result);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(SafetyVerificationTest, VerifyPriorityRespect) {
    createWithDefaults();
    ASSERT_NE(verifier, nullptr);

    safety_rule_t rules[2];
    rules[0] = makeRule(1, "rule1", 10);
    rules[1] = makeRule(2, "rule2", 20);

    verification_result_t result;
    nimcp_error_t err = safety_verify_priority_respect(verifier, rules, 2, &result);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(SafetyVerificationTest, RunFullVerification) {
    createWithDefaults();
    ASSERT_NE(verifier, nullptr);

    safety_rule_t rules[2];
    rules[0] = makeRule(1, "rule1", 10);
    rules[1] = makeRule(2, "rule2", 20);

    verification_report_t report;
    nimcp_error_t err = safety_verification_run_suite(
        verifier, nullptr, rules, 2, &report);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_GT(report.result_count, 0u);
}

TEST_F(SafetyVerificationTest, GetStats) {
    createWithDefaults();
    ASSERT_NE(verifier, nullptr);

    safety_verification_stats_t stats;
    nimcp_error_t err = safety_verification_get_stats(verifier, &stats);
    EXPECT_EQ(err, NIMCP_OK);
    EXPECT_EQ(stats.total_verifications, 0u);
}

TEST_F(SafetyVerificationTest, CheckTypeNames) {
    EXPECT_NE(safety_verification_check_name(VERIFY_CONSISTENCY), nullptr);
    EXPECT_NE(safety_verification_check_name(VERIFY_COMPLETENESS), nullptr);
    EXPECT_NE(safety_verification_check_name(VERIFY_NO_BYPASS), nullptr);
}

TEST_F(SafetyVerificationTest, ConnectBioAsync) {
    createWithDefaults();
    ASSERT_NE(verifier, nullptr);
    nimcp_error_t err = safety_verification_connect_bio_async(verifier);
    EXPECT_EQ(err, NIMCP_OK);
}

TEST_F(SafetyVerificationTest, NullHandleOperationsReturnErrors) {
    safety_rule_t rules[1];
    rules[0] = makeRule(1, "test", 10);
    verification_result_t result;
    EXPECT_EQ(safety_verify_consistency(nullptr, nullptr, rules, 1, &result),
              NIMCP_ERROR_INVALID_ARGUMENT);
}

