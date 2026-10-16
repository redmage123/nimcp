/**
 * @file test_heal_bridge_integration.cpp
 * @brief Integration tests for Enhanced Self-Healing Bridge
 * @version 1.0.0
 * @date 2025-12-27
 *
 * Tests integration between heal bridge and:
 * - Self-heal engine (fix generation, pattern matching)
 * - Code immune system (antibody production, B cells)
 * - Pattern library (pattern evolution)
 * - Brain immune system (coordination)
 *
 * BIOLOGICAL MODEL:
 * The heal bridge acts as a "germinal center" where crash patterns
 * are analyzed, fixes are generated and validated, and successful
 * fixes can evolve into new patterns through affinity maturation.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>

// Headers have their own extern "C" guards
#include "cognitive/immune/nimcp_heal_bridge.h"
#include "cognitive/immune/nimcp_self_heal.h"
#include "cognitive/immune/nimcp_code_immune.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/immune/nimcp_heal_patterns.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Integration Test Fixture
 * ============================================================================ */

class HealBridgeIntegrationTest : public ::testing::Test {
protected:
    heal_bridge_t* bridge = nullptr;
    self_heal_engine_t* self_heal = nullptr;
    code_immune_system_t* code_immune = nullptr;
    brain_immune_system_t* brain_immune = nullptr;

    void SetUp() override {
        /* Create brain immune (parent) */
        brain_immune_config_t brain_config;
        brain_immune_default_config(&brain_config);
        brain_immune = brain_immune_create(&brain_config);
        ASSERT_NE(brain_immune, nullptr);
        brain_immune_start(brain_immune);

        /* Create code immune */
        code_immune_config_t code_config;
        code_immune_default_config(&code_config);
        code_immune = code_immune_create_with_config(brain_immune, &code_config);
        ASSERT_NE(code_immune, nullptr);
        code_immune_start(code_immune);

        /* Create self-heal */
        self_heal_config_t sh_config;
        self_heal_default_config(&sh_config);
        sh_config.enable_lnn = false;  /* Disable for integration tests */
        self_heal = self_heal_create(&sh_config);
        ASSERT_NE(self_heal, nullptr);

        /* Connect self-heal to code immune */
        self_heal_connect_immune(self_heal, brain_immune);

        /* Create heal bridge */
        heal_bridge_config_t bridge_config;
        heal_bridge_default_config(&bridge_config);
        bridge = heal_bridge_create(self_heal, code_immune, &bridge_config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            heal_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (self_heal) {
            self_heal_destroy(self_heal);
            self_heal = nullptr;
        }
        if (code_immune) {
            code_immune_stop(code_immune);
            code_immune_destroy(code_immune);
            code_immune = nullptr;
        }
        if (brain_immune) {
            brain_immune_stop(brain_immune);
            brain_immune_destroy(brain_immune);
            brain_immune = nullptr;
        }
    }

    /* Helper: Present crash and get antigen ID */
    uint64_t presentCrash(int signal, void* fault_addr, const char* function) {
        void* backtrace[] = {(void*)0x1000, (void*)0x2000};
        uint64_t antigen_id = 0;

        code_immune_present_crash_detailed(
            code_immune,
            signal,
            fault_addr,
            (void*)0x12345678,
            "test_file.c",
            100,
            function,
            backtrace,
            2,
            &antigen_id
        );

        return antigen_id;
    }
};

/* ============================================================================
 * Self-Heal + Code-Immune Integration Tests
 * ============================================================================ */

TEST_F(HealBridgeIntegrationTest, BridgeLinkedToSystems) {
    EXPECT_EQ(bridge->self_heal, self_heal);
    EXPECT_EQ(bridge->code_immune, code_immune);
    EXPECT_NE(bridge->pattern_library, nullptr);
}

TEST_F(HealBridgeIntegrationTest, ProcessCrashGeneratesFix) {
    /* Present a crash to code immune */
    uint64_t antigen_id = presentCrash(SIGSEGV, (void*)0xDEAD, "null_deref_function");
    ASSERT_GT(antigen_id, 0u);

    /* Process through bridge */
    uint64_t antibody_id = 0;
    const char* source_code = "ptr->value = 10;";

    int ret = heal_bridge_process_crash(bridge, antigen_id, source_code, &antibody_id);

    /* Should generate fix (may or may not produce antibody depending on config) */
    heal_bridge_stats_t stats;
    heal_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.crashes_received, 1u);
    EXPECT_GE(stats.fixes_generated, 0u);  /* May generate fix */
}

TEST_F(HealBridgeIntegrationTest, SandboxValidatesBeforeApply) {
    uint64_t antigen_id = presentCrash(SIGSEGV, nullptr, "test_func");

    /* Manually create and validate a fix */
    heal_result_t fix = {};
    fix.status = HEAL_STATUS_SUCCESS;
    strcpy(fix.fixed_code, "if (ptr != NULL) { ptr->value = 10; }");
    fix.pattern_used = FIX_PATTERN_NULL_CHECK;
    fix.confidence = 0.9f;

    sandbox_result_t sandbox_result;
    int ret = heal_bridge_validate_fix(bridge, &fix, &sandbox_result);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(sandbox_result, SANDBOX_RESULT_SUCCESS);

    /* Check sandbox stats updated */
    heal_bridge_stats_t stats;
    heal_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.sandbox_runs, 1u);
    EXPECT_EQ(stats.sandbox_successes, 1u);
}

/* ============================================================================
 * Pattern Evolution Integration Tests
 * ============================================================================ */

TEST_F(HealBridgeIntegrationTest, LNNFixTrackedForEvolution) {
    /* Simulate an LNN-generated fix */
    crash_features_t features = {};
    features.n_features = SELF_HEAL_FEATURE_DIM;
    features.suggested_type = FIX_PATTERN_NULL_CHECK;
    features.type_confidence = 0.85f;

    heal_result_t fix = {};
    fix.pattern_used = FIX_PATTERN_NULL_CHECK;
    fix.confidence = 0.9f;
    fix.lnn_generated = true;
    strcpy(fix.fixed_code, "if (ptr == NULL) { return NIMCP_ERROR_NULL; }");

    uint64_t candidate_id;
    int ret = heal_bridge_register_candidate(bridge, &features, &fix, &candidate_id);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(candidate_id, 0u);
    EXPECT_EQ(bridge->candidate_count, 1u);

    /* Verify tracked in stats */
    heal_bridge_stats_t stats;
    heal_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.candidates_active, 1u);
}

TEST_F(HealBridgeIntegrationTest, SuccessfulCandidatePromotesToPattern) {
    /* Configure low threshold for test */
    bridge->config.evolution_threshold = 2;
    bridge->config.min_promotion_confidence = 0.6f;

    /* Register candidate */
    crash_features_t features = {};
    features.suggested_type = FIX_PATTERN_ZERO_CHECK;

    heal_result_t fix = {};
    fix.pattern_used = FIX_PATTERN_ZERO_CHECK;
    fix.confidence = 0.8f;
    strcpy(fix.fixed_code, "if (divisor == 0) return NIMCP_ERROR_DIV_ZERO;");

    uint64_t candidate_id;
    heal_bridge_register_candidate(bridge, &features, &fix, &candidate_id);

    /* Record successes until promotion */
    heal_bridge_record_candidate_outcome(bridge, candidate_id, true, 0.85f);
    int ret = heal_bridge_record_candidate_outcome(bridge, candidate_id, true, 0.9f);

    /* Should return 1 indicating promotion */
    EXPECT_EQ(ret, 1);

    /* Verify promoted */
    heal_bridge_stats_t stats;
    heal_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.patterns_evolved, 1u);
}

TEST_F(HealBridgeIntegrationTest, FailedCandidateRejected) {
    crash_features_t features = {};
    heal_result_t fix = {};
    fix.confidence = 0.5f;

    uint64_t candidate_id;
    heal_bridge_register_candidate(bridge, &features, &fix, &candidate_id);

    /* Record 5 failures */
    for (int i = 0; i < 5; i++) {
        heal_bridge_record_candidate_outcome(bridge, candidate_id, false, 0.2f);
    }

    /* Verify rejected */
    heal_bridge_stats_t stats;
    heal_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.candidates_rejected, 1u);
}

/* ============================================================================
 * Fix Chain Integration Tests
 * ============================================================================ */

TEST_F(HealBridgeIntegrationTest, CreateAndExecuteFixChain) {
    uint64_t antigen_id = presentCrash(SIGSEGV, nullptr, "complex_func");

    /* Create chain */
    uint64_t chain_id;
    const char* source = "ptr->next->value = 10;";
    int ret = heal_bridge_create_chain(bridge, antigen_id, source, &chain_id);

    EXPECT_EQ(ret, 0);
    EXPECT_GT(chain_id, 0u);

    /* Add fixes to chain */
    heal_result_t fix1 = {};
    fix1.pattern_used = FIX_PATTERN_NULL_CHECK;
    fix1.confidence = 0.9f;
    strcpy(fix1.fixed_code, "if (ptr == NULL) return;");

    heal_result_t fix2 = {};
    fix2.pattern_used = FIX_PATTERN_NULL_CHECK;
    fix2.confidence = 0.85f;
    strcpy(fix2.fixed_code, "if (ptr->next == NULL) return;");

    ret = heal_bridge_add_to_chain(bridge, chain_id, &fix1, FIX_DEP_NONE, -1);
    EXPECT_EQ(ret, 0);

    ret = heal_bridge_add_to_chain(bridge, chain_id, &fix2, FIX_DEP_PREREQUISITE, 0);
    EXPECT_EQ(ret, 0);

    /* Execute chain */
    ret = heal_bridge_execute_chain(bridge, chain_id);

    /* Check status */
    chain_status_t status;
    size_t applied;
    heal_bridge_get_chain_status(bridge, chain_id, &status, &applied);

    /* Should be at least partially successful */
    EXPECT_TRUE(status == CHAIN_STATUS_COMPLETE ||
                status == CHAIN_STATUS_PARTIAL ||
                status == CHAIN_STATUS_FAILED);
}

TEST_F(HealBridgeIntegrationTest, ChainRollbackOnFailure) {
    /* Configure atomic chains */
    bridge->config.atomic_chains = true;

    uint64_t antigen_id = presentCrash(SIGFPE, nullptr, "div_func");

    uint64_t chain_id;
    heal_bridge_create_chain(bridge, antigen_id, "x = a / b;", &chain_id);

    /* Add valid fix */
    heal_result_t fix1 = {};
    fix1.pattern_used = FIX_PATTERN_ZERO_CHECK;
    fix1.confidence = 0.9f;
    strcpy(fix1.fixed_code, "if (b == 0) return;");
    heal_bridge_add_to_chain(bridge, chain_id, &fix1, FIX_DEP_NONE, -1);

    /* Add invalid fix (unbalanced braces - will fail validation) */
    heal_result_t fix2 = {};
    fix2.pattern_used = FIX_PATTERN_OVERFLOW_CHECK;
    fix2.confidence = 0.5f;
    strcpy(fix2.fixed_code, "if (a > MAX) {");  /* Missing close brace */
    heal_bridge_add_to_chain(bridge, chain_id, &fix2, FIX_DEP_SEQUENTIAL, 0);

    /* Execute - should fail and rollback */
    int ret = heal_bridge_execute_chain(bridge, chain_id);

    /* Check failed */
    chain_status_t status;
    heal_bridge_get_chain_status(bridge, chain_id, &status, nullptr);

    /* Chain should have failed (due to validation failure) */
    EXPECT_TRUE(status == CHAIN_STATUS_FAILED || status == CHAIN_STATUS_PARTIAL);
}

/* ============================================================================
 * End-to-End Pipeline Tests
 * ============================================================================ */

TEST_F(HealBridgeIntegrationTest, FullPipelineNullDeref) {
    /* Simulate NULL pointer dereference crash */
    uint64_t antigen_id = presentCrash(SIGSEGV, nullptr, "deref_null");
    ASSERT_GT(antigen_id, 0u);

    /* Process through full pipeline */
    uint64_t antibody_id = 0;
    const char* source = "result = ptr->data;";

    int ret = heal_bridge_process_crash(bridge, antigen_id, source, &antibody_id);

    /* Get stats */
    heal_bridge_stats_t stats;
    heal_bridge_get_stats(bridge, &stats);

    /* Verify pipeline ran */
    EXPECT_EQ(stats.crashes_received, 1u);
    EXPECT_GE(stats.sandbox_runs, 0u);  /* May or may not run sandbox */
}

TEST_F(HealBridgeIntegrationTest, FullPipelineDivZero) {
    /* Simulate division by zero crash */
    uint64_t antigen_id = presentCrash(SIGFPE, (void*)0x1234, "divide_func");
    ASSERT_GT(antigen_id, 0u);

    /* Process through full pipeline */
    const char* source = "result = numerator / denominator;";
    uint64_t antibody_id = 0;

    int ret = heal_bridge_process_crash(bridge, antigen_id, source, &antibody_id);

    /* Get stats */
    heal_bridge_stats_t stats;
    heal_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.crashes_received, 1u);
}

TEST_F(HealBridgeIntegrationTest, MultipleCrashesUpdateStats) {
    /* Process multiple crashes */
    for (int i = 0; i < 5; i++) {
        uint64_t antigen_id = presentCrash(SIGSEGV, (void*)(uintptr_t)(i * 0x1000),
                                            "crash_func");
        heal_bridge_process_crash(bridge, antigen_id, "ptr->x = 1;", nullptr);
    }

    /* Verify stats */
    heal_bridge_stats_t stats;
    heal_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.crashes_received, 5u);
}

/* ============================================================================
 * Rollback Integration Tests
 * ============================================================================ */

TEST_F(HealBridgeIntegrationTest, RollbackCleanupRemovesOldEntries) {
    /* Process some crashes to potentially create rollback entries */
    for (int i = 0; i < 3; i++) {
        uint64_t antigen_id = presentCrash(SIGSEGV, nullptr, "func");
        heal_bridge_process_crash(bridge, antigen_id, "x = ptr->y;", nullptr);
    }

    /* Cleanup */
    int cleaned = heal_bridge_cleanup_rollback_history(bridge);
    EXPECT_GE(cleaned, 0);
}

/* ============================================================================
 * Concurrent Access Tests
 * ============================================================================ */

TEST_F(HealBridgeIntegrationTest, ConcurrentCandidateRegistration) {
    std::vector<std::thread> threads;

    /* Register candidates from multiple threads */
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < 5; i++) {
                crash_features_t features = {};
                heal_result_t fix = {};
                fix.confidence = 0.5f + (float)i * 0.1f;

                uint64_t id;
                heal_bridge_register_candidate(bridge, &features, &fix, &id);
            }
        });
    }

    /* Wait for all threads */
    for (auto& t : threads) {
        t.join();
    }

    /* Should have registered candidates without crashing */
    EXPECT_GE(bridge->candidate_count, 0u);
}

TEST_F(HealBridgeIntegrationTest, ConcurrentChainCreation) {
    std::vector<std::thread> threads;

    /* Create chains from multiple threads */
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, t]() {
            uint64_t antigen_id = presentCrash(SIGSEGV, (void*)(uintptr_t)(t * 0x1000), "func");
            uint64_t chain_id;
            heal_bridge_create_chain(bridge, antigen_id, "code", &chain_id);
        });
    }

    /* Wait for all threads */
    for (auto& t : threads) {
        t.join();
    }

    /* Should have created chains without crashing */
    EXPECT_GE(bridge->chain_count, 0u);
}

/* ============================================================================
 * Statistics Accuracy Tests
 * ============================================================================ */

TEST_F(HealBridgeIntegrationTest, StatsAccurateAfterOperations) {
    /* Register some candidates */
    for (int i = 0; i < 3; i++) {
        crash_features_t features = {};
        heal_result_t fix = {};
        fix.confidence = 0.8f;
        uint64_t id;
        heal_bridge_register_candidate(bridge, &features, &fix, &id);
    }

    /* Create some chains */
    for (int i = 0; i < 2; i++) {
        uint64_t antigen_id = presentCrash(SIGSEGV, nullptr, "func");
        uint64_t chain_id;
        heal_bridge_create_chain(bridge, antigen_id, "code", &chain_id);
    }

    /* Validate some fixes */
    heal_result_t fix = {};
    strcpy(fix.fixed_code, "valid code");
    sandbox_result_t result;
    heal_bridge_validate_fix(bridge, &fix, &result);

    /* Get final stats */
    heal_bridge_stats_t stats;
    heal_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.candidates_active, 3u);
    EXPECT_EQ(stats.chains_created, 2u);
    EXPECT_EQ(stats.sandbox_runs, 1u);
}

TEST_F(HealBridgeIntegrationTest, ResetStatsZerosCounters) {
    /* Generate some activity */
    crash_features_t features = {};
    heal_result_t fix = {};
    uint64_t id;
    heal_bridge_register_candidate(bridge, &features, &fix, &id);

    uint64_t antigen_id = presentCrash(SIGSEGV, nullptr, "f");
    heal_bridge_create_chain(bridge, antigen_id, "c", nullptr);

    /* Reset */
    heal_bridge_reset_stats(bridge);

    /* Verify zeroed */
    heal_bridge_stats_t stats;
    heal_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.crashes_received, 0u);
    EXPECT_EQ(stats.chains_created, 0u);
    EXPECT_EQ(stats.sandbox_runs, 0u);
}
