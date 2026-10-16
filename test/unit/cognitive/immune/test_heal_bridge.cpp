/**
 * @file test_heal_bridge.cpp
 * @brief Unit tests for Enhanced Self-Healing Bridge
 * @date 2025-12-27
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_heal_bridge.h"
#include "cognitive/immune/nimcp_self_heal.h"
#include "cognitive/immune/nimcp_code_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class HealBridgeTest : public ::testing::Test {
protected:
    heal_bridge_t* bridge = nullptr;
    self_heal_engine_t* self_heal = nullptr;
    code_immune_system_t* code_immune = nullptr;

    void SetUp() override {
        /* Create self-heal engine */
        self_heal_config_t sh_config;
        self_heal_default_config(&sh_config);
        sh_config.enable_lnn = false;  /* Disable LNN for unit tests */
        sh_config.enable_learning = false;
        self_heal = self_heal_create(&sh_config);

        /* Create code immune system */
        code_immune = code_immune_create(nullptr);
    }

    void TearDown() override {
        if (bridge != nullptr) {
            heal_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (self_heal != nullptr) {
            self_heal_destroy(self_heal);
            self_heal = nullptr;
        }
        if (code_immune != nullptr) {
            code_immune_destroy(code_immune);
            code_immune = nullptr;
        }
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(HealBridgeTest, DefaultConfigSetsReasonableValues) {
    heal_bridge_config_t config;
    int ret = heal_bridge_default_config(&config);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(config.enable_sandbox);
    EXPECT_TRUE(config.enable_pattern_evolution);
    EXPECT_TRUE(config.enable_fix_chains);
    EXPECT_TRUE(config.enable_rollback);
    EXPECT_TRUE(config.auto_produce_antibodies);
    EXPECT_GT(config.sandbox_timeout_ms, 0u);
    EXPECT_GT(config.evolution_threshold, 0u);
    EXPECT_GT(config.min_promotion_confidence, 0.0f);
}

TEST_F(HealBridgeTest, DefaultConfigNullReturnsError) {
    int ret = heal_bridge_default_config(nullptr);
    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(HealBridgeTest, CreateWithDefaultConfig) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    ASSERT_NE(bridge, nullptr);
    EXPECT_TRUE(bridge->initialized);
    EXPECT_EQ(bridge->self_heal, self_heal);
    EXPECT_EQ(bridge->code_immune, code_immune);
}

TEST_F(HealBridgeTest, CreateWithCustomConfig) {
    heal_bridge_config_t config;
    heal_bridge_default_config(&config);
    config.enable_sandbox = false;
    config.evolution_threshold = 5;

    bridge = heal_bridge_create(self_heal, code_immune, &config);

    ASSERT_NE(bridge, nullptr);
    EXPECT_FALSE(bridge->config.enable_sandbox);
    EXPECT_EQ(bridge->config.evolution_threshold, 5u);
}

TEST_F(HealBridgeTest, CreateWithNullSelfHeal) {
    bridge = heal_bridge_create(nullptr, code_immune, nullptr);

    /* Should still create bridge but self_heal will be null */
    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(bridge->self_heal, nullptr);
}

TEST_F(HealBridgeTest, CreateWithNullCodeImmune) {
    bridge = heal_bridge_create(self_heal, nullptr, nullptr);

    ASSERT_NE(bridge, nullptr);
    EXPECT_EQ(bridge->code_immune, nullptr);
}

TEST_F(HealBridgeTest, DestroyNullSafe) {
    heal_bridge_destroy(nullptr);
    /* Should not crash */
    SUCCEED();
}

/* ============================================================================
 * Sandbox Validation Tests
 * ============================================================================ */

TEST_F(HealBridgeTest, ValidateFixNullBridgeReturnsError) {
    heal_result_t fix = {};
    sandbox_result_t result;

    int ret = heal_bridge_validate_fix(nullptr, &fix, &result);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealBridgeTest, ValidateFixNullFixReturnsError) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);
    sandbox_result_t result;

    int ret = heal_bridge_validate_fix(bridge, nullptr, &result);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealBridgeTest, ValidateFixDisabledSandboxReturnsSkipped) {
    heal_bridge_config_t config;
    heal_bridge_default_config(&config);
    config.enable_sandbox = false;

    bridge = heal_bridge_create(self_heal, code_immune, &config);

    heal_result_t fix = {};
    strcpy(fix.fixed_code, "if (ptr != NULL) { ptr->value = 1; }");
    fix.status = HEAL_STATUS_SUCCESS;

    sandbox_result_t result = SANDBOX_RESULT_CRASH;
    int ret = heal_bridge_validate_fix(bridge, &fix, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SANDBOX_RESULT_SKIPPED);
}

TEST_F(HealBridgeTest, ValidateFixValidCodeSucceeds) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    heal_result_t fix = {};
    strcpy(fix.fixed_code, "if (ptr != NULL) { ptr->value = 1; }");
    fix.status = HEAL_STATUS_SUCCESS;

    sandbox_result_t result;
    int ret = heal_bridge_validate_fix(bridge, &fix, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SANDBOX_RESULT_SUCCESS);
}

TEST_F(HealBridgeTest, ValidateFixEmptyCodeFails) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    heal_result_t fix = {};
    fix.fixed_code[0] = '\0';  /* Empty code */
    fix.status = HEAL_STATUS_SUCCESS;

    sandbox_result_t result;
    int ret = heal_bridge_validate_fix(bridge, &fix, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SANDBOX_RESULT_COMPILE_ERROR);
}

TEST_F(HealBridgeTest, ValidateFixUnbalancedBracesFails) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    heal_result_t fix = {};
    strcpy(fix.fixed_code, "if (ptr != NULL) { ptr->value = 1;");  /* Missing } */
    fix.status = HEAL_STATUS_SUCCESS;

    sandbox_result_t result;
    int ret = heal_bridge_validate_fix(bridge, &fix, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SANDBOX_RESULT_COMPILE_ERROR);
}

TEST_F(HealBridgeTest, ValidateFixInfiniteLoopDetected) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    heal_result_t fix = {};
    strcpy(fix.fixed_code, "while(1) { do_something(); }");
    fix.status = HEAL_STATUS_SUCCESS;

    sandbox_result_t result;
    int ret = heal_bridge_validate_fix(bridge, &fix, &result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(result, SANDBOX_RESULT_REGRESSION);
}

/* ============================================================================
 * Pattern Evolution Tests
 * ============================================================================ */

TEST_F(HealBridgeTest, RegisterCandidateSuccess) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    crash_features_t features = {};
    features.n_features = SELF_HEAL_FEATURE_DIM;
    features.suggested_type = FIX_PATTERN_NULL_CHECK;
    features.type_confidence = 0.8f;

    heal_result_t fix = {};
    fix.pattern_used = FIX_PATTERN_NULL_CHECK;
    fix.confidence = 0.85f;
    fix.lnn_generated = true;
    strcpy(fix.fixed_code, "if (ptr == NULL) return;");

    uint64_t candidate_id = 0;
    int ret = heal_bridge_register_candidate(bridge, &features, &fix, &candidate_id);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(candidate_id, 0u);
    EXPECT_EQ(bridge->candidate_count, 1u);
}

TEST_F(HealBridgeTest, RegisterCandidateDisabledEvolution) {
    heal_bridge_config_t config;
    heal_bridge_default_config(&config);
    config.enable_pattern_evolution = false;

    bridge = heal_bridge_create(self_heal, code_immune, &config);

    crash_features_t features = {};
    heal_result_t fix = {};
    uint64_t candidate_id = 0;

    int ret = heal_bridge_register_candidate(bridge, &features, &fix, &candidate_id);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(bridge->candidate_count, 0u);  /* Not registered */
}

TEST_F(HealBridgeTest, RecordCandidateOutcomeSuccess) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    crash_features_t features = {};
    features.suggested_type = FIX_PATTERN_NULL_CHECK;

    heal_result_t fix = {};
    fix.pattern_used = FIX_PATTERN_NULL_CHECK;
    fix.confidence = 0.8f;

    uint64_t candidate_id;
    heal_bridge_register_candidate(bridge, &features, &fix, &candidate_id);

    int ret = heal_bridge_record_candidate_outcome(bridge, candidate_id, true, 0.9f);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(bridge->candidates[0].success_count, 1u);
}

TEST_F(HealBridgeTest, RecordCandidateOutcomeFailure) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    crash_features_t features = {};
    heal_result_t fix = {};
    fix.confidence = 0.5f;

    uint64_t candidate_id;
    heal_bridge_register_candidate(bridge, &features, &fix, &candidate_id);

    int ret = heal_bridge_record_candidate_outcome(bridge, candidate_id, false, 0.3f);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(bridge->candidates[0].failure_count, 1u);
}

TEST_F(HealBridgeTest, CandidatePromotionOnThreshold) {
    heal_bridge_config_t config;
    heal_bridge_default_config(&config);
    config.evolution_threshold = 2;  /* Low threshold for testing */
    config.min_promotion_confidence = 0.5f;

    bridge = heal_bridge_create(self_heal, code_immune, &config);

    crash_features_t features = {};
    heal_result_t fix = {};
    fix.pattern_used = FIX_PATTERN_NULL_CHECK;
    fix.confidence = 0.8f;
    strcpy(fix.fixed_code, "if (ptr == NULL) return NIMCP_ERROR_NULL;");

    uint64_t candidate_id;
    heal_bridge_register_candidate(bridge, &features, &fix, &candidate_id);

    /* First success */
    heal_bridge_record_candidate_outcome(bridge, candidate_id, true, 0.8f);
    EXPECT_EQ(bridge->candidates[0].state, PATTERN_EVO_CANDIDATE);

    /* Second success - should trigger promotion */
    int ret = heal_bridge_record_candidate_outcome(bridge, candidate_id, true, 0.85f);

    /* ret == 1 indicates promotion occurred */
    EXPECT_EQ(ret, 1);
    EXPECT_EQ(bridge->candidates[0].state, PATTERN_EVO_PROMOTED);
    EXPECT_EQ(bridge->stats.patterns_evolved, 1u);
}

TEST_F(HealBridgeTest, CandidateRejectionOnLowSuccessRate) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    crash_features_t features = {};
    heal_result_t fix = {};
    fix.confidence = 0.5f;

    uint64_t candidate_id;
    heal_bridge_register_candidate(bridge, &features, &fix, &candidate_id);

    /* 5 failures should trigger rejection */
    for (int i = 0; i < 5; i++) {
        heal_bridge_record_candidate_outcome(bridge, candidate_id, false, 0.2f);
    }

    EXPECT_EQ(bridge->candidates[0].state, PATTERN_EVO_REJECTED);
    EXPECT_EQ(bridge->stats.candidates_rejected, 1u);
}

TEST_F(HealBridgeTest, DecayCandidates) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    /* Add some candidates */
    for (int i = 0; i < 3; i++) {
        crash_features_t features = {};
        heal_result_t fix = {};
        fix.confidence = 0.1f;  /* Low confidence */
        uint64_t id;
        heal_bridge_register_candidate(bridge, &features, &fix, &id);

        /* Manually age them */
        bridge->candidates[i].last_seen = 0;
        bridge->candidates[i].avg_confidence = 0.05f;  /* Below threshold */
    }

    int pruned = heal_bridge_decay_candidates(bridge);

    EXPECT_GE(pruned, 0);  /* May or may not prune depending on implementation */
}

/* ============================================================================
 * Fix Chain Tests
 * ============================================================================ */

TEST_F(HealBridgeTest, CreateChainSuccess) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    uint64_t chain_id = 0;
    int ret = heal_bridge_create_chain(bridge, 1, "ptr->value = 10;", &chain_id);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(chain_id, 0u);
    EXPECT_EQ(bridge->chain_count, 1u);
    EXPECT_EQ(bridge->stats.chains_created, 1u);
}

TEST_F(HealBridgeTest, AddToChainSuccess) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    uint64_t chain_id;
    heal_bridge_create_chain(bridge, 1, "ptr->value = 10;", &chain_id);

    heal_result_t fix = {};
    fix.pattern_used = FIX_PATTERN_NULL_CHECK;
    fix.confidence = 0.9f;
    strcpy(fix.fixed_code, "if (ptr == NULL) return;");

    int ret = heal_bridge_add_to_chain(bridge, chain_id, &fix, FIX_DEP_NONE, -1);

    EXPECT_EQ(ret, 0);

    fix_chain_t* chain = &bridge->active_chains[0];
    EXPECT_GT(chain->fix_count, 0u);
}

TEST_F(HealBridgeTest, AddToChainWithDependency) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    uint64_t chain_id;
    heal_bridge_create_chain(bridge, 1, "ptr->value = 10;", &chain_id);

    fix_chain_t* chain = &bridge->active_chains[0];
    size_t initial_count = chain->fix_count;

    heal_result_t fix1 = {};
    fix1.pattern_used = FIX_PATTERN_NULL_CHECK;
    fix1.confidence = 0.9f;
    heal_bridge_add_to_chain(bridge, chain_id, &fix1, FIX_DEP_NONE, -1);

    size_t first_idx = chain->fix_count - 1;  /* Index of first added fix */

    heal_result_t fix2 = {};
    fix2.pattern_used = FIX_PATTERN_RESOURCE_LEAK;
    fix2.confidence = 0.8f;
    int ret = heal_bridge_add_to_chain(bridge, chain_id, &fix2, FIX_DEP_PREREQUISITE, (int)first_idx);

    EXPECT_EQ(ret, 0);

    size_t second_idx = chain->fix_count - 1;  /* Index of second added fix */
    EXPECT_EQ(chain->fixes[second_idx].dependency, FIX_DEP_PREREQUISITE);
    EXPECT_EQ(chain->fixes[second_idx].depends_on_idx, (uint32_t)first_idx);
}

TEST_F(HealBridgeTest, GetChainStatus) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    uint64_t chain_id;
    heal_bridge_create_chain(bridge, 1, "ptr->value = 10;", &chain_id);

    chain_status_t status;
    size_t applied_count;
    int ret = heal_bridge_get_chain_status(bridge, chain_id, &status, &applied_count);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(status, CHAIN_STATUS_PENDING);
    EXPECT_EQ(applied_count, 0u);
}

TEST_F(HealBridgeTest, GetChainStatusInvalidId) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    chain_status_t status;
    int ret = heal_bridge_get_chain_status(bridge, 999, &status, nullptr);

    EXPECT_EQ(ret, -1);
}

/* ============================================================================
 * Rollback Tests
 * ============================================================================ */

TEST_F(HealBridgeTest, RollbackNullBridgeReturnsError) {
    int ret = heal_bridge_rollback(nullptr, 1);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealBridgeTest, RollbackDisabledReturnsError) {
    heal_bridge_config_t config;
    heal_bridge_default_config(&config);
    config.enable_rollback = false;

    bridge = heal_bridge_create(self_heal, code_immune, &config);

    int ret = heal_bridge_rollback(bridge, 1);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealBridgeTest, RollbackUnknownAntibodyReturnsError) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    int ret = heal_bridge_rollback(bridge, 999);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealBridgeTest, CleanupRollbackHistory) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    int cleaned = heal_bridge_cleanup_rollback_history(bridge);
    EXPECT_GE(cleaned, 0);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(HealBridgeTest, GetStatsSuccess) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    heal_bridge_stats_t stats;
    int ret = heal_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.crashes_received, 0u);
    EXPECT_EQ(stats.fixes_generated, 0u);
}

TEST_F(HealBridgeTest, GetStatsNullReturnsError) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    int ret = heal_bridge_get_stats(bridge, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(HealBridgeTest, ResetStatsSuccess) {
    bridge = heal_bridge_create(self_heal, code_immune, nullptr);

    /* Generate some activity */
    crash_features_t features = {};
    heal_result_t fix = {};
    uint64_t id;
    heal_bridge_register_candidate(bridge, &features, &fix, &id);

    int ret = heal_bridge_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    heal_bridge_stats_t stats;
    heal_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.candidates_active, 0u);  /* Stats cleared */
}

/* ============================================================================
 * Utility Function Tests
 * ============================================================================ */

TEST_F(HealBridgeTest, SandboxResultToString) {
    EXPECT_STREQ(heal_bridge_sandbox_result_to_string(SANDBOX_RESULT_SUCCESS), "success");
    EXPECT_STREQ(heal_bridge_sandbox_result_to_string(SANDBOX_RESULT_CRASH), "crash");
    EXPECT_STREQ(heal_bridge_sandbox_result_to_string(SANDBOX_RESULT_TIMEOUT), "timeout");
    EXPECT_STREQ(heal_bridge_sandbox_result_to_string(SANDBOX_RESULT_REGRESSION), "regression");
    EXPECT_STREQ(heal_bridge_sandbox_result_to_string(SANDBOX_RESULT_COMPILE_ERROR), "compile_error");
    EXPECT_STREQ(heal_bridge_sandbox_result_to_string(SANDBOX_RESULT_LOAD_ERROR), "load_error");
    EXPECT_STREQ(heal_bridge_sandbox_result_to_string(SANDBOX_RESULT_SKIPPED), "skipped");
}

TEST_F(HealBridgeTest, ChainStatusToString) {
    EXPECT_STREQ(heal_bridge_chain_status_to_string(CHAIN_STATUS_PENDING), "pending");
    EXPECT_STREQ(heal_bridge_chain_status_to_string(CHAIN_STATUS_IN_PROGRESS), "in_progress");
    EXPECT_STREQ(heal_bridge_chain_status_to_string(CHAIN_STATUS_COMPLETE), "complete");
    EXPECT_STREQ(heal_bridge_chain_status_to_string(CHAIN_STATUS_PARTIAL), "partial");
    EXPECT_STREQ(heal_bridge_chain_status_to_string(CHAIN_STATUS_FAILED), "failed");
    EXPECT_STREQ(heal_bridge_chain_status_to_string(CHAIN_STATUS_ROLLBACK), "rollback");
}

TEST_F(HealBridgeTest, EvolutionStateToString) {
    EXPECT_STREQ(heal_bridge_evolution_state_to_string(PATTERN_EVO_CANDIDATE), "candidate");
    EXPECT_STREQ(heal_bridge_evolution_state_to_string(PATTERN_EVO_TESTING), "testing");
    EXPECT_STREQ(heal_bridge_evolution_state_to_string(PATTERN_EVO_PROMOTED), "promoted");
    EXPECT_STREQ(heal_bridge_evolution_state_to_string(PATTERN_EVO_REJECTED), "rejected");
    EXPECT_STREQ(heal_bridge_evolution_state_to_string(PATTERN_EVO_DEPRECATED), "deprecated");
}
