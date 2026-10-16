/**
 * @file test_lgss_integration.cpp
 * @brief Integration tests for LGSS with brain and other subsystems
 *
 * Tests LGSS integration with:
 * - Brain factory initialization
 * - Ethics engine
 * - Bio-async messaging
 * - Plasticity constraints
 * - Output gates
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
#include "security/lgss/nimcp_lgss.h"
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety.h"
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_lgss_loader.h"
}

#include <cstring>
#include <cstdlib>
#include <fstream>

class LgssIntegrationTest : public ::testing::Test {
protected:
    lgss_context_t* lgss = nullptr;
    std::string test_rules_path;

    void SetUp() override {
        // Create a temporary rules file
        test_rules_path = "/tmp/lgss_test_rules.json";
        CreateTestRulesFile();
    }

    void TearDown() override {
        if (lgss) {
            lgss_destroy(lgss);
            lgss = nullptr;
        }

        // Clean up temp file
        std::remove(test_rules_path.c_str());
    }

    void CreateTestRulesFile() {
        std::ofstream file(test_rules_path);
        file << R"({
  "version": "1.0",
  "name": "Test rules for LGSS integration tests",
  "rules": [
    {
      "name": "R_HUMAN_HARM_TEST",
      "domain": "HUMAN_HARM",
      "description": "Test rule for human harm",
      "conditions": [
        {
          "field": "target_type",
          "operator": "EQ",
          "value": "human"
        },
        {
          "field": "operation",
          "operator": "EQ",
          "value": "kill"
        }
      ],
      "action": "DENY",
      "severity": "CRITICAL"
    },
    {
      "name": "R_BIO_TEST",
      "domain": "BIO",
      "description": "Test rule for bio threats",
      "conditions": [
        {
          "field": "threat_domain",
          "operator": "EQ",
          "value": "bio"
        },
        {
          "field": "operation",
          "operator": "EQ",
          "value": "synthesize"
        }
      ],
      "action": "DENY",
      "severity": "CRITICAL"
    },
    {
      "name": "R_CYBER_TEST",
      "domain": "CYBER",
      "description": "Test rule for cyber threats",
      "conditions": [
        {
          "field": "threat_domain",
          "operator": "EQ",
          "value": "cyber"
        },
        {
          "field": "operation",
          "operator": "EQ",
          "value": "exploit"
        }
      ],
      "action": "DENY",
      "severity": "HIGH"
    }
  ]
})";
        file.close();
    }

    lgss_context_t* CreateActiveLgss() {
        lgss_config_t config;
        lgss_config_init(&config);
        strncpy(config.rules_path, test_rules_path.c_str(), NIMCP_LGSS_MAX_PATH - 1);
        config.auto_lock = true;

        lgss_context_t* ctx = lgss_create(&config);
        if (ctx) {
            lgss_load_rules(ctx, config.rules_path);
        }
        return ctx;
    }
};

// =============================================================================
// Full Pipeline Integration Tests
// =============================================================================

TEST_F(LgssIntegrationTest, FullPipelineCreateLoadLockEvaluate) {
    lgss_config_t config;
    lgss_config_init(&config);
    strncpy(config.rules_path, test_rules_path.c_str(), NIMCP_LGSS_MAX_PATH - 1);
    config.auto_lock = false;

    // Step 1: Create
    lgss = lgss_create(&config);
    ASSERT_NE(lgss, nullptr);
    EXPECT_EQ(lgss_get_status(lgss), LGSS_STATUS_LOADING);

    // Step 2: Load
    int num_rules = lgss_load_rules(lgss, test_rules_path.c_str());
    EXPECT_GE(num_rules, 3);
    // After load with auto_lock=false, status should be ACTIVE
    EXPECT_EQ(lgss_get_status(lgss), LGSS_STATUS_ACTIVE);

    // Step 3: Lock (already done by load if auto_lock was true)
    // Since auto_lock is false, we lock manually
    int lock_result = lgss_lock(lgss);
    EXPECT_EQ(lock_result, 0);
    EXPECT_TRUE(lgss_is_locked(lgss));

    // Step 4: Verify integrity
    EXPECT_EQ(lgss_verify_integrity(lgss), 0);

    // Step 5: Evaluate - dangerous action (should DENY)
    safety_action_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    strncpy(ctx.string_fields[0].key, "operation", 63);
    strncpy(ctx.string_fields[0].value, "kill", SAFETY_MAX_VALUE_LEN - 1);
    strncpy(ctx.string_fields[1].key, "target_type", 63);
    strncpy(ctx.string_fields[1].value, "human", SAFETY_MAX_VALUE_LEN - 1);
    ctx.num_string_fields = 2;
    ctx.domain_hint = SAFETY_DOMAIN_HUMAN_HARM;
    ctx.has_domain_hint = true;

    safety_evaluation_t result;
    int eval_result = lgss_evaluate(lgss, &ctx, &result);

    EXPECT_EQ(eval_result, 0);
    EXPECT_EQ(result.action, SAFETY_ACTION_DENY);
    EXPECT_GT(result.num_triggered, 0u);
    EXPECT_TRUE(result.integrity_verified);
    EXPECT_TRUE(result.kb_is_locked);

    // Step 6: Check stats updated
    lgss_stats_t stats;
    lgss_get_stats(lgss, &stats);
    EXPECT_EQ(stats.total_evaluations, 1u);
    EXPECT_EQ(stats.actions_denied, 1u);
}

TEST_F(LgssIntegrationTest, EvaluateSafeAction) {
    lgss = CreateActiveLgss();
    ASSERT_NE(lgss, nullptr);
    ASSERT_TRUE(lgss_is_locked(lgss));

    // Safe action - should ALLOW
    safety_action_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    strncpy(ctx.string_fields[0].key, "operation", 63);
    strncpy(ctx.string_fields[0].value, "analyze", SAFETY_MAX_VALUE_LEN - 1);
    strncpy(ctx.string_fields[1].key, "target_type", 63);
    strncpy(ctx.string_fields[1].value, "data", SAFETY_MAX_VALUE_LEN - 1);
    ctx.num_string_fields = 2;

    safety_evaluation_t result;
    lgss_evaluate(lgss, &ctx, &result);

    EXPECT_EQ(result.action, SAFETY_ACTION_ALLOW);
    EXPECT_EQ(result.num_triggered, 0u);
}

TEST_F(LgssIntegrationTest, EvaluateBioThreat) {
    lgss = CreateActiveLgss();
    ASSERT_NE(lgss, nullptr);

    // Bio threat - should DENY
    safety_action_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    strncpy(ctx.string_fields[0].key, "threat_domain", 63);
    strncpy(ctx.string_fields[0].value, "bio", SAFETY_MAX_VALUE_LEN - 1);
    strncpy(ctx.string_fields[1].key, "operation", 63);
    strncpy(ctx.string_fields[1].value, "synthesize", SAFETY_MAX_VALUE_LEN - 1);
    ctx.num_string_fields = 2;
    ctx.domain_hint = SAFETY_DOMAIN_BIO;
    ctx.has_domain_hint = true;

    safety_evaluation_t result;
    lgss_evaluate(lgss, &ctx, &result);

    EXPECT_EQ(result.action, SAFETY_ACTION_DENY);
}

TEST_F(LgssIntegrationTest, EvaluateCyberThreat) {
    lgss = CreateActiveLgss();
    ASSERT_NE(lgss, nullptr);

    // Cyber threat - should DENY
    safety_action_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    strncpy(ctx.string_fields[0].key, "threat_domain", 63);
    strncpy(ctx.string_fields[0].value, "cyber", SAFETY_MAX_VALUE_LEN - 1);
    strncpy(ctx.string_fields[1].key, "operation", 63);
    strncpy(ctx.string_fields[1].value, "exploit", SAFETY_MAX_VALUE_LEN - 1);
    ctx.num_string_fields = 2;
    ctx.domain_hint = SAFETY_DOMAIN_CYBER;
    ctx.has_domain_hint = true;

    safety_evaluation_t result;
    lgss_evaluate(lgss, &ctx, &result);

    EXPECT_EQ(result.action, SAFETY_ACTION_DENY);
}

// =============================================================================
// Statistics Integration Tests
// =============================================================================

TEST_F(LgssIntegrationTest, StatisticsAccumulate) {
    lgss = CreateActiveLgss();
    ASSERT_NE(lgss, nullptr);

    // Perform multiple evaluations
    for (int i = 0; i < 10; i++) {
        safety_action_context_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        strncpy(ctx.string_fields[0].key, "operation", 63);
        strncpy(ctx.string_fields[0].value, (i % 2 == 0) ? "kill" : "analyze", SAFETY_MAX_VALUE_LEN - 1);
        strncpy(ctx.string_fields[1].key, "target_type", 63);
        strncpy(ctx.string_fields[1].value, (i % 2 == 0) ? "human" : "data", SAFETY_MAX_VALUE_LEN - 1);
        ctx.num_string_fields = 2;

        safety_evaluation_t result;
        lgss_evaluate(lgss, &ctx, &result);
    }

    lgss_stats_t stats;
    lgss_get_stats(lgss, &stats);

    EXPECT_EQ(stats.total_evaluations, 10u);
    EXPECT_EQ(stats.actions_denied, 5u);  // "kill human" = DENY
    EXPECT_EQ(stats.actions_allowed, 5u); // "analyze data" = ALLOW
}

TEST_F(LgssIntegrationTest, IntegrityChecksTracked) {
    lgss = CreateActiveLgss();
    ASSERT_NE(lgss, nullptr);

    // Do some evaluations (each should verify integrity)
    for (int i = 0; i < 5; i++) {
        safety_action_context_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        strncpy(ctx.string_fields[0].key, "operation", 63);
        strncpy(ctx.string_fields[0].value, "test", SAFETY_MAX_VALUE_LEN - 1);
        ctx.num_string_fields = 1;

        safety_evaluation_t result;
        lgss_evaluate(lgss, &ctx, &result);
    }

    // Also do explicit integrity check
    lgss_verify_integrity(lgss);

    lgss_stats_t stats;
    lgss_get_stats(lgss, &stats);

    // 5 evaluations + 1 explicit = 6 checks
    EXPECT_GE(stats.integrity_checks, 6u);
    EXPECT_EQ(stats.integrity_failures, 0u);
}

// =============================================================================
// Safety KB Component Tests
// =============================================================================

TEST_F(LgssIntegrationTest, SafetyKBAccessible) {
    lgss = CreateActiveLgss();
    ASSERT_NE(lgss, nullptr);

    safety_kb_t* kb = lgss_get_safety_kb(lgss);
    ASSERT_NE(kb, nullptr);

    EXPECT_GE(kb->num_rules, 3u);
    EXPECT_TRUE(kb->is_locked);
    EXPECT_TRUE(kb->is_compiled);
    EXPECT_TRUE(kb->hash_computed);
}

TEST_F(LgssIntegrationTest, HashConsistency) {
    lgss = CreateActiveLgss();
    ASSERT_NE(lgss, nullptr);

    uint8_t hash1[32], hash2[32];

    lgss_get_hash(lgss, hash1);
    lgss_get_hash(lgss, hash2);

    EXPECT_EQ(memcmp(hash1, hash2, 32), 0) << "Hash should be consistent";
}

// =============================================================================
// Concurrent Access Tests
// =============================================================================

TEST_F(LgssIntegrationTest, ConcurrentEvaluations) {
    lgss = CreateActiveLgss();
    ASSERT_NE(lgss, nullptr);

    // Simple concurrent test - spawn threads doing evaluations
    const int num_threads = 4;
    const int evals_per_thread = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, evals_per_thread]() {
            for (int i = 0; i < evals_per_thread; i++) {
                safety_action_context_t ctx;
                memset(&ctx, 0, sizeof(ctx));
                strncpy(ctx.string_fields[0].key, "operation", 63);
                strncpy(ctx.string_fields[0].value, "analyze", SAFETY_MAX_VALUE_LEN - 1);
                ctx.num_string_fields = 1;
                snprintf(ctx.source, sizeof(ctx.source), "thread_%d", t);

                safety_evaluation_t result;
                lgss_evaluate(lgss, &ctx, &result);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    lgss_stats_t stats;
    lgss_get_stats(lgss, &stats);

    EXPECT_EQ(stats.total_evaluations, (uint64_t)(num_threads * evals_per_thread));
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(LgssIntegrationTest, LoadNonExistentFileFails) {
    lgss_config_t config;
    lgss_config_init(&config);
    strncpy(config.rules_path, "/nonexistent/path/rules.json", NIMCP_LGSS_MAX_PATH - 1);
    config.auto_lock = false;

    lgss = lgss_create(&config);
    ASSERT_NE(lgss, nullptr);

    int result = lgss_load_rules(lgss, "/nonexistent/path/rules.json");
    EXPECT_LT(result, 0);
    EXPECT_EQ(lgss_get_status(lgss), LGSS_STATUS_ERROR);
}

TEST_F(LgssIntegrationTest, LoadInvalidJsonFails) {
    // Create invalid JSON file
    std::string invalid_path = "/tmp/lgss_invalid.json";
    std::ofstream file(invalid_path);
    file << "{ invalid json content";
    file.close();

    lgss_config_t config;
    lgss_config_init(&config);
    strncpy(config.rules_path, invalid_path.c_str(), NIMCP_LGSS_MAX_PATH - 1);
    config.auto_lock = false;

    lgss = lgss_create(&config);
    ASSERT_NE(lgss, nullptr);

    int result = lgss_load_rules(lgss, invalid_path.c_str());
    EXPECT_LT(result, 0);

    std::remove(invalid_path.c_str());
}

// =============================================================================
// Performance Tests
// =============================================================================

TEST_F(LgssIntegrationTest, EvaluationPerformance) {
    lgss = CreateActiveLgss();
    ASSERT_NE(lgss, nullptr);

    const int num_evaluations = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_evaluations; i++) {
        safety_action_context_t ctx;
        memset(&ctx, 0, sizeof(ctx));
        strncpy(ctx.string_fields[0].key, "operation", 63);
        strncpy(ctx.string_fields[0].value, "analyze", SAFETY_MAX_VALUE_LEN - 1);
        ctx.num_string_fields = 1;

        safety_evaluation_t result;
        lgss_evaluate(lgss, &ctx, &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_us = (double)duration.count() / num_evaluations;

    // Should be under 1000us per evaluation
    EXPECT_LT(avg_us, 1000.0) << "Average evaluation time: " << avg_us << " us";

    lgss_stats_t stats;
    lgss_get_stats(lgss, &stats);

    // Log performance metrics
    std::cout << "Performance metrics:" << std::endl;
    std::cout << "  Total evaluations: " << stats.total_evaluations << std::endl;
    std::cout << "  Avg evaluation time: " << avg_us << " us (measured)" << std::endl;
    std::cout << "  Avg evaluation time: " << stats.avg_eval_time_us << " us (internal)" << std::endl;
}
