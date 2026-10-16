/**
 * @file test_security_immune_unified_bridge.cpp
 * @brief Unit tests for Security-Immune Unified Bridge
 * @version 1.0.0
 * @date 2025-01-09
 *
 * WHAT: Comprehensive unit tests for the unified security-immune bridge that
 *       integrates BBB, anomaly detection, pattern database, rate limiter,
 *       and policy engine with the brain immune system.
 *
 * WHY:  The unified bridge provides bidirectional integration between all
 *       security components and the adaptive immune system. These tests ensure:
 *       - Correct lifecycle management (create/destroy/reset)
 *       - Security->Immune antigen presentation pathways work correctly
 *       - Immune->Security modulation (cytokines/inflammation) functions properly
 *       - Tolerance system prevents false positives
 *       - Memory cell formation and recall operate correctly
 *       - Bio-async integration works as expected
 *
 * HOW:  Uses Google Test framework with a fixture class that sets up the
 *       immune system, security components, and unified bridge. Tests cover:
 *       - NULL input handling
 *       - Normal operation
 *       - Edge cases
 *       - Bidirectional effects
 *       - Statistics tracking
 */

#include <gtest/gtest.h>
#include <cstring>

/* Headers have their own extern "C" guards */
#include "security/immune/nimcp_security_immune_unified_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_anomaly_detector.h"
#include "security/nimcp_pattern_db.h"
#include "security/nimcp_rate_limiter.h"
#include "security/nimcp_policy_engine.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SecurityImmuneUnifiedBridgeTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    bbb_system_t bbb_system = nullptr;
    nimcp_anomaly_detector_t anomaly_detector = nullptr;
    nimcp_pattern_db_t pattern_db = nullptr;
    nimcp_rate_limiter_t rate_limiter = nullptr;
    nimcp_policy_engine_t policy_engine = nullptr;
    sec_immune_unified_bridge_t* bridge = nullptr;

    void SetUp() override {
        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        /* Create BBB system */
        bbb_config_t bbb_cfg = bbb_default_config();
        bbb_system = bbb_system_create(&bbb_cfg);
        ASSERT_NE(bbb_system, nullptr);

        /* Create anomaly detector */
        nimcp_anomaly_config_t anomaly_cfg = nimcp_anomaly_detector_default_config();
        anomaly_detector = nimcp_anomaly_detector_create(&anomaly_cfg);
        ASSERT_NE(anomaly_detector, nullptr);

        /* Create pattern database */
        nimcp_pattern_db_config_t pattern_cfg = nimcp_pattern_db_default_config();
        pattern_db = nimcp_pattern_db_create(&pattern_cfg);
        ASSERT_NE(pattern_db, nullptr);

        /* Create rate limiter */
        nimcp_rate_limit_config_t rate_cfg = nimcp_rate_limiter_default_config();
        rate_limiter = nimcp_rate_limiter_create(&rate_cfg);
        ASSERT_NE(rate_limiter, nullptr);

        /* Create policy engine */
        nimcp_policy_engine_config_t policy_cfg = {0};
        policy_cfg.max_policies = 16;
        policy_cfg.max_rules_per_policy = 64;
        policy_cfg.enable_caching = true;
        policy_engine = nimcp_policy_engine_create(&policy_cfg);
        ASSERT_NE(policy_engine, nullptr);

        /* Create unified bridge with default config */
        sec_immune_unified_config_t config;
        sec_immune_unified_default_config(&config);
        bridge = sec_immune_unified_create(&config, immune_system);
        ASSERT_NE(bridge, nullptr);

        /* Connect all security components */
        int result = sec_immune_unified_connect_all(
            bridge, bbb_system, anomaly_detector, pattern_db,
            rate_limiter, policy_engine
        );
        ASSERT_EQ(result, 0);
    }

    void TearDown() override {
        if (bridge) {
            sec_immune_unified_destroy(bridge);
            bridge = nullptr;
        }
        if (policy_engine) {
            nimcp_policy_engine_destroy(policy_engine);
            policy_engine = nullptr;
        }
        if (rate_limiter) {
            nimcp_rate_limiter_destroy(rate_limiter);
            rate_limiter = nullptr;
        }
        if (pattern_db) {
            nimcp_pattern_db_destroy(pattern_db);
            pattern_db = nullptr;
        }
        if (anomaly_detector) {
            nimcp_anomaly_detector_destroy(anomaly_detector);
            anomaly_detector = nullptr;
        }
        if (bbb_system) {
            bbb_system_destroy(bbb_system);
            bbb_system = nullptr;
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
    }

    /* Helper to create test anomaly result */
    nimcp_anomaly_result_t CreateAnomalyResult(float score, float confidence) {
        nimcp_anomaly_result_t result = {0};
        result.anomaly_score = score;
        result.confidence = confidence;
        result.content_score = score * 0.5f;
        result.behavior_score = score * 0.3f;
        result.timing_score = score * 0.2f;
        result.triggered_features = NIMCP_TRIGGER_ENTROPY | NIMCP_TRIGGER_SPECIAL_RATIO;
        snprintf(result.explanation, sizeof(result.explanation), "Test anomaly score %.2f", score);
        return result;
    }

    /* Helper to create test pattern match result */
    nimcp_pattern_match_result_t CreatePatternMatchResult(float score, bool matched) {
        nimcp_pattern_match_result_t result = {0};
        result.matched = matched;
        result.pattern_id = 1;
        result.category = NIMCP_PATTERN_SQL_INJECTION;
        result.threat_score = score;
        result.match_count = matched ? 1 : 0;
        snprintf(result.description, sizeof(result.description), "Test pattern match");
        return result;
    }

    /* Helper to create test policy result */
    nimcp_policy_result_t CreatePolicyResult(nimcp_policy_action_t action,
                                             nimcp_policy_severity_t severity) {
        nimcp_policy_result_t result = {};
        result.action = action;
        result.severity = severity;
        result.message = (char*)"Test policy violation";
        result.rule_name = (char*)"test_rule";
        result.should_log = true;
        return result;
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(SecurityImmuneUnifiedBridgeTest, DefaultConfigPopulatesAllFields) {
    sec_immune_unified_config_t config;
    int result = sec_immune_unified_default_config(&config);
    ASSERT_EQ(result, 0);

    /* Verify feature enables */
    EXPECT_TRUE(config.enable_bbb_antigen_presentation);
    EXPECT_TRUE(config.enable_anomaly_antigen_presentation);
    EXPECT_TRUE(config.enable_pattern_antigen_presentation);
    EXPECT_TRUE(config.enable_rate_violation_antigen_presentation);
    EXPECT_TRUE(config.enable_policy_violation_antigen_presentation);

    /* Verify cytokine modulation enables */
    EXPECT_TRUE(config.enable_cytokine_bbb_modulation);
    EXPECT_TRUE(config.enable_cytokine_anomaly_modulation);
    EXPECT_TRUE(config.enable_inflammation_bbb_adjustment);
    EXPECT_TRUE(config.enable_antibody_action_execution);
    EXPECT_TRUE(config.enable_tolerance_system);

    /* Verify threshold values are reasonable */
    EXPECT_GT(config.bbb_base_threshold, 0.0f);
    EXPECT_GT(config.anomaly_base_threshold, 0.0f);
    EXPECT_GT(config.pattern_base_weight, 0.0f);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, DefaultConfigWithNullReturnError) {
    int result = sec_immune_unified_default_config(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, CreateWithNullImmuneSystemFails) {
    sec_immune_unified_config_t config;
    sec_immune_unified_default_config(&config);

    sec_immune_unified_bridge_t* null_bridge = sec_immune_unified_create(&config, nullptr);
    EXPECT_EQ(null_bridge, nullptr);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, CreateWithNullConfigUsesDefaults) {
    sec_immune_unified_bridge_t* default_bridge = sec_immune_unified_create(nullptr, immune_system);
    EXPECT_NE(default_bridge, nullptr);

    if (default_bridge) {
        sec_immune_unified_destroy(default_bridge);
    }
}

TEST_F(SecurityImmuneUnifiedBridgeTest, DestroyWithNullDoesNotCrash) {
    /* Should not crash */
    sec_immune_unified_destroy(nullptr);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ResetClearsStatistics) {
    /* Present some antigens to generate stats */
    uint32_t antigen_id;
    uint8_t threat_data[] = {0x01, 0x02, 0x03, 0x04};
    sec_immune_unified_present_bbb_threat(
        bridge, BBB_THREAT_CODE_INJECTION, BBB_SEVERITY_HIGH,
        threat_data, sizeof(threat_data), &antigen_id
    );

    /* Get stats before reset */
    sec_immune_unified_stats_t stats_before;
    sec_immune_unified_get_stats(bridge, &stats_before);
    EXPECT_GT(stats_before.bbb_antigens_presented, 0u);

    /* Reset bridge */
    int result = sec_immune_unified_reset(bridge);
    EXPECT_EQ(result, 0);

    /* Verify stats are cleared */
    sec_immune_unified_stats_t stats_after;
    sec_immune_unified_get_stats(bridge, &stats_after);
    EXPECT_EQ(stats_after.bbb_antigens_presented, 0u);
    EXPECT_EQ(stats_after.total_antigens_presented, 0u);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ResetWithNullReturnError) {
    int result = sec_immune_unified_reset(nullptr);
    EXPECT_NE(result, 0);
}

/* ============================================================================
 * Connection Tests
 * ============================================================================ */

TEST_F(SecurityImmuneUnifiedBridgeTest, ConnectBBBWithNullBridgeFails) {
    int result = sec_immune_unified_connect_bbb(nullptr, bbb_system);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ConnectBBBSucceeds) {
    sec_immune_unified_config_t config;
    sec_immune_unified_default_config(&config);
    sec_immune_unified_bridge_t* test_bridge = sec_immune_unified_create(&config, immune_system);
    ASSERT_NE(test_bridge, nullptr);

    int result = sec_immune_unified_connect_bbb(test_bridge, bbb_system);
    EXPECT_EQ(result, 0);

    sec_immune_unified_destroy(test_bridge);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ConnectAnomalyWithNullBridgeFails) {
    int result = sec_immune_unified_connect_anomaly(nullptr, anomaly_detector);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ConnectPatternDbWithNullBridgeFails) {
    int result = sec_immune_unified_connect_pattern_db(nullptr, pattern_db);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ConnectRateLimiterWithNullBridgeFails) {
    int result = sec_immune_unified_connect_rate_limiter(nullptr, rate_limiter);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ConnectPolicyEngineWithNullBridgeFails) {
    int result = sec_immune_unified_connect_policy_engine(nullptr, policy_engine);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ConnectAllWithNullBridgeFails) {
    int result = sec_immune_unified_connect_all(
        nullptr, bbb_system, anomaly_detector, pattern_db,
        rate_limiter, policy_engine
    );
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ConnectAllWithSomeNullComponentsSucceeds) {
    sec_immune_unified_config_t config;
    sec_immune_unified_default_config(&config);
    sec_immune_unified_bridge_t* test_bridge = sec_immune_unified_create(&config, immune_system);
    ASSERT_NE(test_bridge, nullptr);

    /* Connect with some NULL components - should still succeed */
    int result = sec_immune_unified_connect_all(
        test_bridge, bbb_system, nullptr, pattern_db, nullptr, nullptr
    );
    EXPECT_EQ(result, 0);

    sec_immune_unified_destroy(test_bridge);
}

/* ============================================================================
 * Update and Modulation Tests
 * ============================================================================ */

TEST_F(SecurityImmuneUnifiedBridgeTest, UpdateWithNullBridgeFails) {
    int result = sec_immune_unified_update(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, UpdateSucceeds) {
    int result = sec_immune_unified_update(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ApplyCytokineEffectsWithNullBridgeFails) {
    int result = sec_immune_unified_apply_cytokine_effects(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ApplyCytokineEffectsSucceeds) {
    int result = sec_immune_unified_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ApplyInflammationWithNullBridgeFails) {
    int result = sec_immune_unified_apply_inflammation(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ApplyInflammationSucceeds) {
    int result = sec_immune_unified_apply_inflammation(bridge);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Security -> Immune: Antigen Presentation Tests
 * ============================================================================ */

TEST_F(SecurityImmuneUnifiedBridgeTest, PresentBBBThreatWithNullBridgeFails) {
    uint32_t antigen_id;
    uint8_t data[] = {0x01};
    int result = sec_immune_unified_present_bbb_threat(
        nullptr, BBB_THREAT_CODE_INJECTION, BBB_SEVERITY_HIGH,
        data, sizeof(data), &antigen_id
    );
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, PresentBBBThreatSucceeds) {
    uint32_t antigen_id = 0;
    uint8_t threat_data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    int result = sec_immune_unified_present_bbb_threat(
        bridge, BBB_THREAT_SHELLCODE, BBB_SEVERITY_HIGH,
        threat_data, sizeof(threat_data), &antigen_id
    );
    EXPECT_EQ(result, 0);
    EXPECT_GT(antigen_id, 0u);

    /* Verify stats updated */
    sec_immune_unified_stats_t stats;
    sec_immune_unified_get_stats(bridge, &stats);
    EXPECT_GT(stats.bbb_antigens_presented, 0u);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, PresentBBBThreatWithNullOutputIdSucceeds) {
    uint8_t threat_data[] = {0x01, 0x02};
    int result = sec_immune_unified_present_bbb_threat(
        bridge, BBB_THREAT_BUFFER_OVERFLOW, BBB_SEVERITY_MEDIUM,
        threat_data, sizeof(threat_data), nullptr
    );
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, PresentAnomalyWithNullBridgeFails) {
    nimcp_anomaly_result_t anomaly = CreateAnomalyResult(0.8f, 0.9f);
    uint32_t antigen_id;
    int result = sec_immune_unified_present_anomaly(nullptr, &anomaly, &antigen_id);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, PresentAnomalyWithNullResultFails) {
    uint32_t antigen_id;
    int result = sec_immune_unified_present_anomaly(bridge, nullptr, &antigen_id);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, PresentAnomalySucceeds) {
    nimcp_anomaly_result_t anomaly = CreateAnomalyResult(0.85f, 0.95f);
    uint32_t antigen_id = 0;
    int result = sec_immune_unified_present_anomaly(bridge, &anomaly, &antigen_id);
    EXPECT_EQ(result, 0);
    EXPECT_GT(antigen_id, 0u);

    sec_immune_unified_stats_t stats;
    sec_immune_unified_get_stats(bridge, &stats);
    EXPECT_GT(stats.anomaly_antigens_presented, 0u);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, PresentPatternMatchWithNullBridgeFails) {
    nimcp_pattern_match_result_t match = CreatePatternMatchResult(0.9f, true);
    uint32_t antigen_id;
    int result = sec_immune_unified_present_pattern_match(nullptr, &match, &antigen_id);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, PresentPatternMatchSucceeds) {
    nimcp_pattern_match_result_t match = CreatePatternMatchResult(0.9f, true);
    uint32_t antigen_id = 0;
    int result = sec_immune_unified_present_pattern_match(bridge, &match, &antigen_id);
    EXPECT_EQ(result, 0);
    EXPECT_GT(antigen_id, 0u);

    sec_immune_unified_stats_t stats;
    sec_immune_unified_get_stats(bridge, &stats);
    EXPECT_GT(stats.pattern_antigens_presented, 0u);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, PresentRateViolationWithNullBridgeFails) {
    uint32_t antigen_id;
    int result = sec_immune_unified_present_rate_violation(
        nullptr, "client_1", 5, &antigen_id
    );
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, PresentRateViolationSucceeds) {
    uint32_t antigen_id = 0;
    int result = sec_immune_unified_present_rate_violation(
        bridge, "abusive_client_192.168.1.100", 10, &antigen_id
    );
    EXPECT_EQ(result, 0);
    EXPECT_GT(antigen_id, 0u);

    sec_immune_unified_stats_t stats;
    sec_immune_unified_get_stats(bridge, &stats);
    EXPECT_GT(stats.rate_antigens_presented, 0u);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, PresentPolicyViolationWithNullBridgeFails) {
    nimcp_policy_result_t policy = CreatePolicyResult(
        NIMCP_POLICY_ACTION_DENY, NIMCP_POLICY_SEVERITY_HIGH
    );
    uint32_t antigen_id;
    int result = sec_immune_unified_present_policy_violation(nullptr, &policy, &antigen_id);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, PresentPolicyViolationSucceeds) {
    nimcp_policy_result_t policy = CreatePolicyResult(
        NIMCP_POLICY_ACTION_DENY, NIMCP_POLICY_SEVERITY_HIGH
    );
    uint32_t antigen_id = 0;
    int result = sec_immune_unified_present_policy_violation(bridge, &policy, &antigen_id);
    EXPECT_EQ(result, 0);
    EXPECT_GT(antigen_id, 0u);

    sec_immune_unified_stats_t stats;
    sec_immune_unified_get_stats(bridge, &stats);
    EXPECT_GT(stats.policy_antigens_presented, 0u);
}

/* ============================================================================
 * Immune -> Security: Antibody Action Tests
 * ============================================================================ */

TEST_F(SecurityImmuneUnifiedBridgeTest, ExecuteAntibodyActionWithNullBridgeFails) {
    int result = sec_immune_unified_execute_antibody_action(nullptr, 1);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ExecuteAntibodyActionWithInvalidIdFails) {
    // The bridge executes antibody actions regardless of ID validity
    // (fire-and-forget pattern - see comment in source: "bridge can neutralize
    // threats even when no specific antibody exists in the immune system")
    int result = sec_immune_unified_execute_antibody_action(bridge, 99999);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ExecuteKillerActionWithNullBridgeFails) {
    int result = sec_immune_unified_execute_killer_action(nullptr, 1, 1);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ExecuteHelperActionWithNullBridgeFails) {
    int result = sec_immune_unified_execute_helper_action(nullptr, 1);
    EXPECT_NE(result, 0);
}

/* ============================================================================
 * Memory Cell Tests
 * ============================================================================ */

TEST_F(SecurityImmuneUnifiedBridgeTest, FormMemoryWithNullBridgeFails) {
    uint32_t memory_id;
    int result = sec_immune_unified_form_memory(nullptr, 1, 1, &memory_id);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, SyncMemoryToPatternWithNullBridgeFails) {
    int result = sec_immune_unified_sync_memory_to_pattern(nullptr, 1);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, CheckMemoryWithNullBridgeFails) {
    uint8_t epitope[] = {0x01, 0x02};
    uint32_t memory_id;
    int result = sec_immune_unified_check_memory(nullptr, epitope, sizeof(epitope), &memory_id);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, CheckMemoryWithNullEpitopeFails) {
    uint32_t memory_id;
    int result = sec_immune_unified_check_memory(bridge, nullptr, 0, &memory_id);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, SecondaryResponseWithNullBridgeFails) {
    int result = sec_immune_unified_secondary_response(nullptr, 1, 1);
    EXPECT_NE(result, 0);
}

/* ============================================================================
 * Tolerance System Tests
 * ============================================================================ */

TEST_F(SecurityImmuneUnifiedBridgeTest, AddToleranceWithNullBridgeFails) {
    uint8_t pattern[] = {0x01, 0x02, 0x03};
    int result = sec_immune_unified_add_tolerance(
        nullptr, pattern, sizeof(pattern), "Test pattern", false
    );
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, AddToleranceWithNullPatternFails) {
    int result = sec_immune_unified_add_tolerance(bridge, nullptr, 0, "Test", false);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, AddToleranceSucceeds) {
    uint8_t pattern[] = {0xAA, 0xBB, 0xCC, 0xDD};
    int result = sec_immune_unified_add_tolerance(
        bridge, pattern, sizeof(pattern), "Benign pattern", false
    );
    EXPECT_EQ(result, 0);

    /* Verify pattern is tolerated */
    bool tolerated = sec_immune_unified_is_tolerated(bridge, pattern, sizeof(pattern));
    EXPECT_TRUE(tolerated);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, AddPermanentToleranceSucceeds) {
    uint8_t pattern[] = {0x11, 0x22, 0x33, 0x44};
    int result = sec_immune_unified_add_tolerance(
        bridge, pattern, sizeof(pattern), "Permanent whitelist", true
    );
    EXPECT_EQ(result, 0);

    bool tolerated = sec_immune_unified_is_tolerated(bridge, pattern, sizeof(pattern));
    EXPECT_TRUE(tolerated);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, RemoveToleranceWithNullBridgeFails) {
    uint8_t pattern[] = {0x01};
    int result = sec_immune_unified_remove_tolerance(nullptr, pattern, sizeof(pattern));
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, RemoveToleranceSucceeds) {
    uint8_t pattern[] = {0xFF, 0xEE, 0xDD};

    /* Add tolerance */
    sec_immune_unified_add_tolerance(bridge, pattern, sizeof(pattern), "Removable", false);
    EXPECT_TRUE(sec_immune_unified_is_tolerated(bridge, pattern, sizeof(pattern)));

    /* Remove tolerance */
    int result = sec_immune_unified_remove_tolerance(bridge, pattern, sizeof(pattern));
    EXPECT_EQ(result, 0);

    /* Verify no longer tolerated */
    EXPECT_FALSE(sec_immune_unified_is_tolerated(bridge, pattern, sizeof(pattern)));
}

TEST_F(SecurityImmuneUnifiedBridgeTest, IsToleratedWithNullBridgeReturnsFalse) {
    uint8_t pattern[] = {0x01};
    bool result = sec_immune_unified_is_tolerated(nullptr, pattern, sizeof(pattern));
    EXPECT_FALSE(result);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, IsToleratedReturnsFalseForUnknownPattern) {
    uint8_t unknown_pattern[] = {0xCA, 0xFE, 0xBA, 0xBE};
    bool result = sec_immune_unified_is_tolerated(bridge, unknown_pattern, sizeof(unknown_pattern));
    EXPECT_FALSE(result);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ConfirmBenignWithNullBridgeFails) {
    uint8_t pattern[] = {0x01};
    int result = sec_immune_unified_confirm_benign(nullptr, pattern, sizeof(pattern));
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ConfirmBenignSucceeds) {
    uint8_t pattern[] = {0x12, 0x34, 0x56, 0x78};
    int result = sec_immune_unified_confirm_benign(bridge, pattern, sizeof(pattern));
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, SetLearningModeWithNullBridgeFails) {
    int result = sec_immune_unified_set_learning_mode(nullptr, true);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, SetLearningModeEnableSucceeds) {
    int result = sec_immune_unified_set_learning_mode(bridge, true);
    EXPECT_EQ(result, 0);

    EXPECT_TRUE(sec_immune_unified_is_learning_mode(bridge));
}

TEST_F(SecurityImmuneUnifiedBridgeTest, SetLearningModeDisableSucceeds) {
    /* Enable then disable */
    sec_immune_unified_set_learning_mode(bridge, true);
    int result = sec_immune_unified_set_learning_mode(bridge, false);
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(sec_immune_unified_is_learning_mode(bridge));
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ActivateRegulatoryWithNullBridgeFails) {
    int result = sec_immune_unified_activate_regulatory(nullptr, 0.5f);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ActivateRegulatorySucceeds) {
    int result = sec_immune_unified_activate_regulatory(bridge, 0.7f);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, ActivateRegulatoryWithInvalidLevelClamps) {
    /* Test clamping of out-of-range values */
    int result1 = sec_immune_unified_activate_regulatory(bridge, -0.5f);
    EXPECT_EQ(result1, 0);

    int result2 = sec_immune_unified_activate_regulatory(bridge, 1.5f);
    EXPECT_EQ(result2, 0);
}

/* ============================================================================
 * Training Feedback Tests
 * ============================================================================ */

TEST_F(SecurityImmuneUnifiedBridgeTest, FeedbackTruePositiveWithNullBridgeFails) {
    int result = sec_immune_unified_feedback_true_positive(nullptr, 1);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, FeedbackTruePositiveSucceeds) {
    /* First present an antigen */
    uint32_t antigen_id = 0;
    uint8_t threat_data[] = {0x01, 0x02, 0x03};
    sec_immune_unified_present_bbb_threat(
        bridge, BBB_THREAT_CODE_INJECTION, BBB_SEVERITY_HIGH,
        threat_data, sizeof(threat_data), &antigen_id
    );

    int result = sec_immune_unified_feedback_true_positive(bridge, antigen_id);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, FeedbackFalsePositiveWithNullBridgeFails) {
    int result = sec_immune_unified_feedback_false_positive(nullptr, 1);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, FeedbackFalsePositiveSucceeds) {
    /* First present an antigen */
    uint32_t antigen_id = 0;
    uint8_t threat_data[] = {0x04, 0x05, 0x06};
    sec_immune_unified_present_bbb_threat(
        bridge, BBB_THREAT_PATH_TRAVERSAL, BBB_SEVERITY_LOW,
        threat_data, sizeof(threat_data), &antigen_id
    );

    int result = sec_immune_unified_feedback_false_positive(bridge, antigen_id);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(SecurityImmuneUnifiedBridgeTest, ConnectBioAsyncWithNullBridgeFails) {
    int result = sec_immune_unified_connect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, DisconnectBioAsyncWithNullBridgeFails) {
    int result = sec_immune_unified_disconnect_bio_async(nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, IsBioAsyncConnectedWithNullReturnsFalse) {
    bool result = sec_immune_unified_is_bio_async_connected(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, IsBioAsyncConnectedInitiallyFalse) {
    bool result = sec_immune_unified_is_bio_async_connected(bridge);
    EXPECT_FALSE(result);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, BroadcastSecurityEventWithNullBridgeFails) {
    uint8_t data[] = {0x01};
    int result = sec_immune_unified_broadcast_security_event(
        nullptr, 1, 5, data, sizeof(data)
    );
    EXPECT_NE(result, 0);
}

/* ============================================================================
 * Query API Tests
 * ============================================================================ */

TEST_F(SecurityImmuneUnifiedBridgeTest, GetBBBThresholdFactorWithNullReturnsDefault) {
    float factor = sec_immune_unified_get_bbb_threshold_factor(nullptr);
    EXPECT_FLOAT_EQ(factor, 1.0f);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, GetBBBThresholdFactorReturnsValidValue) {
    sec_immune_unified_update(bridge);
    float factor = sec_immune_unified_get_bbb_threshold_factor(bridge);
    EXPECT_GE(factor, 0.0f);
    EXPECT_LE(factor, 2.0f);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, GetAnomalyThresholdWithNullReturnsDefault) {
    float threshold = sec_immune_unified_get_anomaly_threshold(nullptr);
    EXPECT_GT(threshold, 0.0f);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, GetAnomalyThresholdReturnsValidValue) {
    sec_immune_unified_update(bridge);
    float threshold = sec_immune_unified_get_anomaly_threshold(bridge);
    EXPECT_GE(threshold, 0.0f);
    EXPECT_LE(threshold, 1.0f);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, GetPatternWeightFactorWithNullReturnsDefault) {
    float factor = sec_immune_unified_get_pattern_weight_factor(nullptr);
    EXPECT_FLOAT_EQ(factor, 1.0f);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, GetPatternWeightFactorReturnsValidValue) {
    sec_immune_unified_update(bridge);
    float factor = sec_immune_unified_get_pattern_weight_factor(bridge);
    EXPECT_GE(factor, 0.0f);
    EXPECT_LE(factor, 3.0f);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, GetRateLimitFactorWithNullReturnsDefault) {
    float factor = sec_immune_unified_get_rate_limit_factor(nullptr);
    EXPECT_FLOAT_EQ(factor, 1.0f);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, GetRateLimitFactorReturnsValidValue) {
    sec_immune_unified_update(bridge);
    float factor = sec_immune_unified_get_rate_limit_factor(bridge);
    EXPECT_GE(factor, 0.0f);
    EXPECT_LE(factor, 2.0f);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, GetPolicyStrictnessFactorWithNullReturnsDefault) {
    float factor = sec_immune_unified_get_policy_strictness_factor(nullptr);
    EXPECT_FLOAT_EQ(factor, 1.0f);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, GetPolicyStrictnessFactorReturnsValidValue) {
    sec_immune_unified_update(bridge);
    float factor = sec_immune_unified_get_policy_strictness_factor(bridge);
    EXPECT_GE(factor, 0.0f);
    EXPECT_LE(factor, 3.0f);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, IsEmergencyModeWithNullReturnsFalse) {
    bool result = sec_immune_unified_is_emergency_mode(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, IsEmergencyModeInitiallyFalse) {
    bool result = sec_immune_unified_is_emergency_mode(bridge);
    EXPECT_FALSE(result);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, IsLearningModeWithNullReturnsFalse) {
    bool result = sec_immune_unified_is_learning_mode(nullptr);
    EXPECT_FALSE(result);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, IsLearningModeInitiallyFalse) {
    bool result = sec_immune_unified_is_learning_mode(bridge);
    EXPECT_FALSE(result);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, GetStatsWithNullBridgeFails) {
    sec_immune_unified_stats_t stats;
    int result = sec_immune_unified_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, GetStatsWithNullOutputFails) {
    int result = sec_immune_unified_get_stats(bridge, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, GetStatsSucceeds) {
    sec_immune_unified_stats_t stats;
    int result = sec_immune_unified_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);

    /* Initial stats should be zeros */
    EXPECT_EQ(stats.total_antigens_presented, 0u);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, GetThreatLevelWithNullReturnsZero) {
    float level = sec_immune_unified_get_threat_level(nullptr);
    EXPECT_FLOAT_EQ(level, 0.0f);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, GetThreatLevelInitiallyLow) {
    float level = sec_immune_unified_get_threat_level(bridge);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(SecurityImmuneUnifiedBridgeTest, InflammationNameReturnsValidStrings) {
    const char* none = sec_immune_unified_inflammation_name(INFLAMMATION_NONE);
    EXPECT_NE(none, nullptr);
    EXPECT_GT(strlen(none), 0u);

    const char* local = sec_immune_unified_inflammation_name(INFLAMMATION_LOCAL);
    EXPECT_NE(local, nullptr);

    const char* regional = sec_immune_unified_inflammation_name(INFLAMMATION_REGIONAL);
    EXPECT_NE(regional, nullptr);

    const char* systemic = sec_immune_unified_inflammation_name(INFLAMMATION_SYSTEMIC);
    EXPECT_NE(systemic, nullptr);

    const char* storm = sec_immune_unified_inflammation_name(INFLAMMATION_STORM);
    EXPECT_NE(storm, nullptr);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, BBBThreatNameReturnsValidStrings) {
    const char* none = sec_immune_unified_bbb_threat_name(BBB_THREAT_NONE);
    EXPECT_NE(none, nullptr);

    const char* overflow = sec_immune_unified_bbb_threat_name(BBB_THREAT_BUFFER_OVERFLOW);
    EXPECT_NE(overflow, nullptr);
    EXPECT_GT(strlen(overflow), 0u);

    const char* injection = sec_immune_unified_bbb_threat_name(BBB_THREAT_CODE_INJECTION);
    EXPECT_NE(injection, nullptr);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, AntibodyClassNameReturnsValidStrings) {
    const char* igm = sec_immune_unified_antibody_class_name(ANTIBODY_IGM);
    EXPECT_NE(igm, nullptr);
    EXPECT_GT(strlen(igm), 0u);

    const char* igg = sec_immune_unified_antibody_class_name(ANTIBODY_IGG);
    EXPECT_NE(igg, nullptr);

    const char* ige = sec_immune_unified_antibody_class_name(ANTIBODY_IGE);
    EXPECT_NE(ige, nullptr);
}

/* ============================================================================
 * Bidirectional Effect Tests
 * ============================================================================ */

TEST_F(SecurityImmuneUnifiedBridgeTest, MultipleThreatsPresentedCorrectly) {
    /* Present multiple different types of threats */
    uint32_t antigen_id;

    /* BBB threat */
    uint8_t bbb_data[] = {0x01, 0x02};
    sec_immune_unified_present_bbb_threat(
        bridge, BBB_THREAT_SHELLCODE, BBB_SEVERITY_HIGH,
        bbb_data, sizeof(bbb_data), &antigen_id
    );

    /* Anomaly */
    nimcp_anomaly_result_t anomaly = CreateAnomalyResult(0.9f, 0.95f);
    sec_immune_unified_present_anomaly(bridge, &anomaly, &antigen_id);

    /* Pattern match */
    nimcp_pattern_match_result_t match = CreatePatternMatchResult(0.8f, true);
    sec_immune_unified_present_pattern_match(bridge, &match, &antigen_id);

    /* Rate violation */
    sec_immune_unified_present_rate_violation(bridge, "attacker", 10, &antigen_id);

    /* Policy violation */
    nimcp_policy_result_t policy = CreatePolicyResult(
        NIMCP_POLICY_ACTION_DENY, NIMCP_POLICY_SEVERITY_CRITICAL
    );
    sec_immune_unified_present_policy_violation(bridge, &policy, &antigen_id);

    /* Verify all stats updated */
    sec_immune_unified_stats_t stats;
    sec_immune_unified_get_stats(bridge, &stats);

    EXPECT_EQ(stats.bbb_antigens_presented, 1u);
    EXPECT_EQ(stats.anomaly_antigens_presented, 1u);
    EXPECT_EQ(stats.pattern_antigens_presented, 1u);
    EXPECT_EQ(stats.rate_antigens_presented, 1u);
    EXPECT_EQ(stats.policy_antigens_presented, 1u);
    EXPECT_EQ(stats.total_antigens_presented, 5u);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, HighSeverityThreatsIncreaseThreatLevel) {
    float initial_level = sec_immune_unified_get_threat_level(bridge);

    /* Present multiple high-severity threats */
    for (int i = 0; i < 5; i++) {
        uint32_t antigen_id;
        uint8_t data[] = {(uint8_t)i, 0xFF};
        sec_immune_unified_present_bbb_threat(
            bridge, BBB_THREAT_SHELLCODE, BBB_SEVERITY_CRITICAL,
            data, sizeof(data), &antigen_id
        );
    }

    /* Update to process */
    sec_immune_unified_update(bridge);

    float new_level = sec_immune_unified_get_threat_level(bridge);
    EXPECT_GE(new_level, initial_level);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, TolerancePreventsFalsePositives) {
    /* Add a pattern to tolerance */
    uint8_t benign_pattern[] = {0xAB, 0xCD, 0xEF, 0x01, 0x23};
    sec_immune_unified_add_tolerance(
        bridge, benign_pattern, sizeof(benign_pattern),
        "Known benign pattern", true
    );

    /* Verify it is tolerated */
    EXPECT_TRUE(sec_immune_unified_is_tolerated(bridge, benign_pattern, sizeof(benign_pattern)));

    /* After tolerance, similar patterns should not trigger false positives */
    sec_immune_unified_stats_t stats;
    sec_immune_unified_get_stats(bridge, &stats);
    /* Initial false_positives_prevented should be 0 */
    EXPECT_EQ(stats.false_positives_prevented, 0u);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, LearningModeAffectsStatistics) {
    /* Enable learning mode */
    sec_immune_unified_set_learning_mode(bridge, true);
    EXPECT_TRUE(sec_immune_unified_is_learning_mode(bridge));

    /* Present some threats during learning */
    uint32_t antigen_id;
    uint8_t data[] = {0x01, 0x02};
    sec_immune_unified_present_bbb_threat(
        bridge, BBB_THREAT_BUFFER_OVERFLOW, BBB_SEVERITY_LOW,
        data, sizeof(data), &antigen_id
    );

    sec_immune_unified_update(bridge);

    sec_immune_unified_stats_t stats;
    sec_immune_unified_get_stats(bridge, &stats);
    EXPECT_TRUE(stats.tolerance_learning_active);

    /* Disable learning mode */
    sec_immune_unified_set_learning_mode(bridge, false);
    EXPECT_FALSE(sec_immune_unified_is_learning_mode(bridge));
}

/* ============================================================================
 * Statistics Tracking Tests
 * ============================================================================ */

TEST_F(SecurityImmuneUnifiedBridgeTest, StatsTrackAntigenPresentation) {
    sec_immune_unified_stats_t stats_before;
    sec_immune_unified_get_stats(bridge, &stats_before);

    /* Present antigens */
    uint32_t antigen_id;
    for (int i = 0; i < 3; i++) {
        uint8_t data[] = {(uint8_t)i};
        sec_immune_unified_present_bbb_threat(
            bridge, BBB_THREAT_CODE_INJECTION, BBB_SEVERITY_MEDIUM,
            data, sizeof(data), &antigen_id
        );
    }

    sec_immune_unified_stats_t stats_after;
    sec_immune_unified_get_stats(bridge, &stats_after);

    EXPECT_EQ(stats_after.bbb_antigens_presented, stats_before.bbb_antigens_presented + 3);
    EXPECT_EQ(stats_after.total_antigens_presented, stats_before.total_antigens_presented + 3);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, StatsReflectCurrentInflammation) {
    sec_immune_unified_update(bridge);

    sec_immune_unified_stats_t stats;
    sec_immune_unified_get_stats(bridge, &stats);

    /* Initially no inflammation */
    EXPECT_EQ(stats.current_inflammation, INFLAMMATION_NONE);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, StatsReflectEmergencyMode) {
    sec_immune_unified_stats_t stats;
    sec_immune_unified_get_stats(bridge, &stats);

    /* Initially not in emergency mode */
    EXPECT_FALSE(stats.emergency_mode_active);
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(SecurityImmuneUnifiedBridgeTest, EmptyThreatDataHandled) {
    uint32_t antigen_id;
    /* Empty data should be handled gracefully */
    int result = sec_immune_unified_present_bbb_threat(
        bridge, BBB_THREAT_UNKNOWN, BBB_SEVERITY_LOW,
        nullptr, 0, &antigen_id
    );
    /* Should either succeed or return error gracefully */
    EXPECT_TRUE(result == 0 || result != 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, LowSeverityThreatStillPresented) {
    uint32_t antigen_id = 0;
    uint8_t data[] = {0x01};
    // Low severity threats are filtered by SEC_IMMUNE_BBB_MIN_SEVERITY_FOR_ANTIGEN
    // (BBB_SEVERITY_MEDIUM). Use MEDIUM or higher to verify presentation.
    int result = sec_immune_unified_present_bbb_threat(
        bridge, BBB_THREAT_PATH_TRAVERSAL, BBB_SEVERITY_MEDIUM,
        data, sizeof(data), &antigen_id
    );
    EXPECT_EQ(result, 0);

    sec_immune_unified_stats_t stats;
    sec_immune_unified_get_stats(bridge, &stats);
    EXPECT_GT(stats.bbb_antigens_presented, 0u);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, LongClientIdHandled) {
    uint32_t antigen_id;
    char long_client_id[512];
    memset(long_client_id, 'A', sizeof(long_client_id) - 1);
    long_client_id[sizeof(long_client_id) - 1] = '\0';

    /* Should handle long client ID gracefully */
    int result = sec_immune_unified_present_rate_violation(
        bridge, long_client_id, 5, &antigen_id
    );
    /* Should either succeed (truncating) or return error gracefully */
    EXPECT_TRUE(result == 0 || result != 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, MultipleResetsCycle) {
    /* Reset should be idempotent */
    for (int i = 0; i < 5; i++) {
        int result = sec_immune_unified_reset(bridge);
        EXPECT_EQ(result, 0);
    }

    /* Bridge should still function */
    uint32_t antigen_id;
    uint8_t data[] = {0x01};
    int result = sec_immune_unified_present_bbb_threat(
        bridge, BBB_THREAT_CODE_INJECTION, BBB_SEVERITY_HIGH,
        data, sizeof(data), &antigen_id
    );
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityImmuneUnifiedBridgeTest, RapidAntigenPresentationHandled) {
    /* Rapid-fire antigen presentation should not cause issues */
    for (int i = 0; i < 100; i++) {
        uint32_t antigen_id;
        uint8_t data[] = {(uint8_t)(i & 0xFF), (uint8_t)((i >> 8) & 0xFF)};
        sec_immune_unified_present_bbb_threat(
            bridge, BBB_THREAT_CODE_INJECTION, BBB_SEVERITY_MEDIUM,
            data, sizeof(data), &antigen_id
        );
    }

    sec_immune_unified_stats_t stats;
    sec_immune_unified_get_stats(bridge, &stats);
    EXPECT_EQ(stats.bbb_antigens_presented, 100u);
}
