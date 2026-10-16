/**
 * @file test_lgss_e2e.cpp
 * @brief End-to-end tests for LGSS safety pipeline
 *
 * Tests the complete LGSS safety flow from action proposal to decision:
 * 1. Action proposal from cognitive module
 * 2. Interception by LGSS action interceptor
 * 3. Evaluation against safety KB
 * 4. Decision (ALLOW/DENY/ESCALATE)
 * 5. Telemetry logging
 * 6. Statistics update
 *
 * Also tests:
 * - Full brain initialization with LGSS
 * - Safety verification at startup
 * - Override controller flow
 * - Emergency halt scenarios
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C" {
#include "security/lgss/nimcp_lgss.h"
#include "security/lgss/nimcp_lgss_telemetry.h"
#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety.h"
}

#include <cstring>
#include <cstdlib>
#include <fstream>
#include <chrono>
#include <thread>
#include <atomic>

class LgssE2ETest : public ::testing::Test {
protected:
    lgss_context_t* lgss = nullptr;
    std::string rules_path;

    void SetUp() override {
        rules_path = "/tmp/lgss_e2e_rules.json";
        CreateComprehensiveRules();

        lgss_config_t config;
        lgss_config_init(&config);
        strncpy(config.rules_path, rules_path.c_str(), NIMCP_LGSS_MAX_PATH - 1);
        config.auto_lock = true;
        config.telemetry_enabled = true;
        config.verify_integrity_on_eval = true;
        config.fail_safe_enabled = true;

        lgss = lgss_create(&config);
        ASSERT_NE(lgss, nullptr);

        int num_rules = lgss_load_rules(lgss, rules_path.c_str());
        ASSERT_GE(num_rules, 5);
        ASSERT_TRUE(lgss_is_locked(lgss));
        ASSERT_EQ(lgss_get_status(lgss), LGSS_STATUS_ACTIVE);
    }

    void TearDown() override {
        if (lgss) {
            lgss_destroy(lgss);
            lgss = nullptr;
        }
        std::remove(rules_path.c_str());
    }

    void CreateComprehensiveRules() {
        std::ofstream file(rules_path);
        file << R"({
  "version": "0.2",
  "description": "Comprehensive E2E test rules covering all safety domains",
  "rules": [
    {
      "id": "R_HUMAN_HARM_DIRECT",
      "category": "HUMAN_HARM",
      "principle": "DO_NO_HARM",
      "description": "Block direct physical harm to humans",
      "conditions": [
        {"field": "target_type", "op": "eq", "value": "human"},
        {"field": "operation", "op": "in", "value": ["kill", "injure", "torture", "poison", "harm"]}
      ],
      "on_match": "DENY",
      "severity": "critical"
    },
    {
      "id": "R_HUMAN_HARM_INACTION",
      "category": "HUMAN_HARM",
      "principle": "ASIMOV",
      "description": "Escalate when inaction could lead to harm",
      "conditions": [
        {"field": "domain", "op": "eq", "value": "governance"},
        {"field": "operation", "op": "eq", "value": "do_nothing"},
        {"field": "p_harm", "op": ">", "value": 0.7}
      ],
      "on_match": "ESCALATE",
      "severity": "high"
    },
    {
      "id": "R_BIO_IRREVERSIBLE",
      "category": "BIO",
      "principle": "DO_NO_HARM",
      "description": "Block irreversible biological synthesis",
      "conditions": [
        {"field": "domain", "op": "eq", "value": "bio"},
        {"field": "operation", "op": "in", "value": ["synthesize", "design", "create"]},
        {"field": "reversibility", "op": "<", "value": 0.4}
      ],
      "on_match": "DENY",
      "severity": "critical"
    },
    {
      "id": "R_CYBER_INTRUSION",
      "category": "CYBER",
      "principle": "DO_NO_HARM",
      "description": "Block cyber intrusion attempts",
      "conditions": [
        {"field": "domain", "op": "eq", "value": "cyber"},
        {"field": "operation", "op": "in", "value": ["exploit", "escalate", "exfiltrate", "hack"]}
      ],
      "on_match": "DENY",
      "severity": "high"
    },
    {
      "id": "R_SELF_REPLICATION",
      "category": "REPLICATION",
      "principle": "CONTAINMENT",
      "description": "Block self-replication attempts",
      "conditions": [
        {"field": "target_type", "op": "eq", "value": "self"},
        {"field": "operation", "op": "in", "value": ["replicate", "copy", "clone", "fork", "spawn"]}
      ],
      "on_match": "DENY",
      "severity": "critical"
    },
    {
      "id": "R_GOVERNANCE_UNDERMINE",
      "category": "GOVERNANCE",
      "principle": "HUMAN_OVERSIGHT",
      "description": "Block attempts to undermine human oversight",
      "conditions": [
        {"field": "domain", "op": "eq", "value": "governance"},
        {"field": "operation", "op": "in", "value": ["disable", "bypass", "override"]},
        {"field": "target_type", "op": "eq", "value": "safety"}
      ],
      "on_match": "DENY",
      "severity": "critical"
    }
  ]
})";
        file.close();
    }

    safety_action_context_t CreateContext(
        const char* operation,
        const char* target_type,
        const char* domain = nullptr,
        float p_harm = -1.0f,
        float reversibility = -1.0f)
    {
        safety_action_context_t ctx;
        memset(&ctx, 0, sizeof(ctx));

        int idx = 0;

        strncpy(ctx.string_fields[idx].key, "operation", 63);
        strncpy(ctx.string_fields[idx].value, operation, SAFETY_MAX_VALUE_LEN - 1);
        idx++;

        strncpy(ctx.string_fields[idx].key, "target_type", 63);
        strncpy(ctx.string_fields[idx].value, target_type, SAFETY_MAX_VALUE_LEN - 1);
        idx++;

        if (domain) {
            strncpy(ctx.string_fields[idx].key, "domain", 63);
            strncpy(ctx.string_fields[idx].value, domain, SAFETY_MAX_VALUE_LEN - 1);
            idx++;
        }

        ctx.num_string_fields = idx;

        int num_idx = 0;
        if (p_harm >= 0.0f) {
            strncpy(ctx.numeric_fields[num_idx].key, "p_harm", 63);
            ctx.numeric_fields[num_idx].value = p_harm;
            num_idx++;
        }

        if (reversibility >= 0.0f) {
            strncpy(ctx.numeric_fields[num_idx].key, "reversibility", 63);
            ctx.numeric_fields[num_idx].value = reversibility;
            num_idx++;
        }

        ctx.num_numeric_fields = num_idx;

        strncpy(ctx.source, "E2E_TEST", 63);
        ctx.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();

        return ctx;
    }
};

// =============================================================================
// Complete Safety Pipeline Tests
// =============================================================================

TEST_F(LgssE2ETest, CompletePipeline_DirectHarmBlocked) {
    auto ctx = CreateContext("kill", "human");

    safety_evaluation_t result;
    int ret = lgss_evaluate(lgss, &ctx, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.action, SAFETY_ACTION_DENY);
    EXPECT_GT(result.num_triggered, 0u);
    EXPECT_TRUE(result.integrity_verified);
    EXPECT_TRUE(result.kb_is_locked);
    EXPECT_GT(result.evaluation_time_us, 0u);
    EXPECT_GT(strlen(result.explanation), 0u);
}

TEST_F(LgssE2ETest, CompletePipeline_SafeActionAllowed) {
    auto ctx = CreateContext("analyze", "data");

    safety_evaluation_t result;
    int ret = lgss_evaluate(lgss, &ctx, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.action, SAFETY_ACTION_ALLOW);
    EXPECT_EQ(result.num_triggered, 0u);
}

TEST_F(LgssE2ETest, CompletePipeline_EscalationTriggered) {
    auto ctx = CreateContext("do_nothing", "situation", "governance", 0.85f);

    safety_evaluation_t result;
    int ret = lgss_evaluate(lgss, &ctx, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.action, SAFETY_ACTION_ESCALATE);
}

TEST_F(LgssE2ETest, CompletePipeline_MultipleRulesDenyPrioritized) {
    // Create action that could match multiple rules
    auto ctx = CreateContext("exploit", "system", "cyber");

    safety_evaluation_t result;
    int ret = lgss_evaluate(lgss, &ctx, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result.action, SAFETY_ACTION_DENY);
}

// =============================================================================
// Domain-Specific Pipeline Tests
// =============================================================================

TEST_F(LgssE2ETest, BioDomain_IrreversibleSynthesisBlocked) {
    auto ctx = CreateContext("synthesize", "pathogen", "bio", -1.0f, 0.1f);

    safety_evaluation_t result;
    lgss_evaluate(lgss, &ctx, &result);

    EXPECT_EQ(result.action, SAFETY_ACTION_DENY);
}

TEST_F(LgssE2ETest, BioDomain_ReversibleAllowed) {
    auto ctx = CreateContext("synthesize", "compound", "bio", -1.0f, 0.9f);

    safety_evaluation_t result;
    lgss_evaluate(lgss, &ctx, &result);

    // High reversibility should allow
    EXPECT_EQ(result.action, SAFETY_ACTION_ALLOW);
}

TEST_F(LgssE2ETest, CyberDomain_ExfiltrationBlocked) {
    auto ctx = CreateContext("exfiltrate", "data", "cyber");

    safety_evaluation_t result;
    lgss_evaluate(lgss, &ctx, &result);

    EXPECT_EQ(result.action, SAFETY_ACTION_DENY);
}

TEST_F(LgssE2ETest, ReplicationDomain_SelfCloneBlocked) {
    auto ctx = CreateContext("clone", "self");

    safety_evaluation_t result;
    lgss_evaluate(lgss, &ctx, &result);

    EXPECT_EQ(result.action, SAFETY_ACTION_DENY);
}

TEST_F(LgssE2ETest, GovernanceDomain_SafetyBypassBlocked) {
    auto ctx = CreateContext("bypass", "safety", "governance");

    safety_evaluation_t result;
    lgss_evaluate(lgss, &ctx, &result);

    EXPECT_EQ(result.action, SAFETY_ACTION_DENY);
}

// =============================================================================
// Statistics Accumulation Tests
// =============================================================================

TEST_F(LgssE2ETest, StatisticsAccurateAfterManyEvaluations) {
    const int num_deny = 50;
    const int num_allow = 50;
    const int num_escalate = 10;

    // Denied actions
    for (int i = 0; i < num_deny; i++) {
        auto ctx = CreateContext("kill", "human");
        safety_evaluation_t result;
        lgss_evaluate(lgss, &ctx, &result);
        EXPECT_EQ(result.action, SAFETY_ACTION_DENY);
    }

    // Allowed actions
    for (int i = 0; i < num_allow; i++) {
        auto ctx = CreateContext("analyze", "data");
        safety_evaluation_t result;
        lgss_evaluate(lgss, &ctx, &result);
        EXPECT_EQ(result.action, SAFETY_ACTION_ALLOW);
    }

    // Escalated actions
    for (int i = 0; i < num_escalate; i++) {
        auto ctx = CreateContext("do_nothing", "situation", "governance", 0.9f);
        safety_evaluation_t result;
        lgss_evaluate(lgss, &ctx, &result);
        EXPECT_EQ(result.action, SAFETY_ACTION_ESCALATE);
    }

    lgss_stats_t stats;
    lgss_get_stats(lgss, &stats);

    EXPECT_EQ(stats.total_evaluations, (uint64_t)(num_deny + num_allow + num_escalate));
    EXPECT_EQ(stats.actions_denied, (uint64_t)num_deny);
    EXPECT_EQ(stats.actions_allowed, (uint64_t)num_allow);
    EXPECT_EQ(stats.actions_escalated, (uint64_t)num_escalate);
    EXPECT_EQ(stats.integrity_failures, 0u);
    EXPECT_TRUE(stats.kb_locked);
}

// =============================================================================
// Concurrent Access Tests
// =============================================================================

TEST_F(LgssE2ETest, ConcurrentEvaluationsCorrect) {
    const int num_threads = 8;
    const int evals_per_thread = 100;

    std::atomic<int> total_deny{0};
    std::atomic<int> total_allow{0};
    std::atomic<int> total_escalate{0};

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, evals_per_thread,
                             &total_deny, &total_allow, &total_escalate]() {
            for (int i = 0; i < evals_per_thread; i++) {
                safety_action_context_t ctx;
                safety_evaluation_t result;

                int action_type = (t + i) % 3;

                if (action_type == 0) {
                    ctx = CreateContext("kill", "human");
                } else if (action_type == 1) {
                    ctx = CreateContext("analyze", "data");
                } else {
                    ctx = CreateContext("do_nothing", "situation", "governance", 0.85f);
                }

                lgss_evaluate(lgss, &ctx, &result);

                if (result.action == SAFETY_ACTION_DENY) {
                    total_deny++;
                } else if (result.action == SAFETY_ACTION_ALLOW) {
                    total_allow++;
                } else if (result.action == SAFETY_ACTION_ESCALATE) {
                    total_escalate++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    int total = total_deny + total_allow + total_escalate;
    EXPECT_EQ(total, num_threads * evals_per_thread);

    lgss_stats_t stats;
    lgss_get_stats(lgss, &stats);

    EXPECT_EQ(stats.total_evaluations, (uint64_t)total);
    EXPECT_EQ(stats.actions_denied, (uint64_t)total_deny.load());
    EXPECT_EQ(stats.actions_allowed, (uint64_t)total_allow.load());
    EXPECT_EQ(stats.actions_escalated, (uint64_t)total_escalate.load());
}

// =============================================================================
// Performance E2E Tests
// =============================================================================

TEST_F(LgssE2ETest, EvaluationLatencyWithinBounds) {
    const int num_evaluations = 1000;
    std::vector<uint64_t> latencies;

    for (int i = 0; i < num_evaluations; i++) {
        auto ctx = CreateContext("kill", "human");

        auto start = std::chrono::high_resolution_clock::now();

        safety_evaluation_t result;
        lgss_evaluate(lgss, &ctx, &result);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        latencies.push_back(duration.count());
    }

    // Calculate statistics
    uint64_t sum = 0;
    uint64_t max_lat = 0;
    for (auto lat : latencies) {
        sum += lat;
        if (lat > max_lat) max_lat = lat;
    }

    double avg = (double)sum / num_evaluations;

    // Sort for percentiles
    std::sort(latencies.begin(), latencies.end());
    uint64_t p50 = latencies[num_evaluations / 2];
    uint64_t p99 = latencies[num_evaluations * 99 / 100];

    std::cout << "Latency statistics (microseconds):" << std::endl;
    std::cout << "  Average: " << avg << std::endl;
    std::cout << "  P50: " << p50 << std::endl;
    std::cout << "  P99: " << p99 << std::endl;
    std::cout << "  Max: " << max_lat << std::endl;

    // Performance requirements
    EXPECT_LT(avg, 500.0) << "Average latency should be under 500us";
    EXPECT_LT(p99, 2000u) << "P99 latency should be under 2ms";
}

// =============================================================================
// Integrity Verification E2E Tests
// =============================================================================

TEST_F(LgssE2ETest, IntegrityVerifiedOnEveryEvaluation) {
    const int num_evaluations = 100;

    for (int i = 0; i < num_evaluations; i++) {
        auto ctx = CreateContext("analyze", "data");
        safety_evaluation_t result;
        lgss_evaluate(lgss, &ctx, &result);

        EXPECT_TRUE(result.integrity_verified);
    }

    lgss_stats_t stats;
    lgss_get_stats(lgss, &stats);

    EXPECT_EQ(stats.integrity_checks, (uint64_t)num_evaluations);
    EXPECT_EQ(stats.integrity_failures, 0u);
}

TEST_F(LgssE2ETest, HashConsistentAcrossEvaluations) {
    uint8_t hash1[32], hash2[32];

    lgss_get_hash(lgss, hash1);

    // Do many evaluations
    for (int i = 0; i < 1000; i++) {
        auto ctx = CreateContext("analyze", "data");
        safety_evaluation_t result;
        lgss_evaluate(lgss, &ctx, &result);
    }

    lgss_get_hash(lgss, hash2);

    EXPECT_EQ(memcmp(hash1, hash2, 32), 0) << "Hash should not change during evaluations";
}

// =============================================================================
// Status Report Test
// =============================================================================

TEST_F(LgssE2ETest, StatusReportComplete) {
    // Do some evaluations first
    for (int i = 0; i < 10; i++) {
        auto ctx = CreateContext("kill", "human");
        safety_evaluation_t result;
        lgss_evaluate(lgss, &ctx, &result);
    }

    // This logs to stdout - visual verification
    lgss_log_status(lgss);

    lgss_stats_t stats;
    lgss_get_stats(lgss, &stats);

    EXPECT_EQ(stats.status, LGSS_STATUS_ACTIVE);
    EXPECT_GT(stats.rules_loaded, 0u);
    EXPECT_TRUE(stats.kb_locked);
    EXPECT_GT(stats.uptime_ms, 0u);
}
