/**
 * @file test_ethics_evaluation_regression.cpp
 * @brief Regression tests for Core Directives ethics evaluation system
 *
 * WHAT: Comprehensive regression tests for ethical constraint evaluation
 * WHY:  Ethics evaluation is critical for AI safety - must prevent regressions
 * HOW:  Test API stability, evaluation consistency, integration, edge cases
 *
 * MODULES TESTED:
 * - nimcp_core_directives.h (Core Ethical Directives System)
 *
 * REGRESSION CATEGORIES:
 * - API Stability: Function signatures, enum values, struct layout
 * - Evaluation Consistency: Same inputs produce same outputs
 * - Integration Safety: Bio-async, immune, FEP connections work
 * - Memory Safety: No leaks in create/destroy cycles
 * - Error Handling: Graceful handling of NULL, invalid inputs
 * - Historical Bug Fixes: Previously fixed bugs remain fixed
 *
 * @author NIMCP Test Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <vector>
#include <atomic>
#include <thread>

// Headers have their own extern "C" guards
#include "cognitive/ethics/nimcp_core_directives.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class CoreDirectivesRegressionTest : public ::testing::Test {
protected:
    core_directives_system_t* directives = nullptr;
    core_directives_config_t config;

    void SetUp() override {
        core_directives_default_config(&config);
        directives = core_directives_create(&config);
        ASSERT_NE(directives, nullptr);
    }

    void TearDown() override {
        if (directives) {
            core_directives_destroy(directives);
            directives = nullptr;
        }
    }

    // Helper to create test action vector
    void create_test_action(float* action, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            action[i] = base_value + (static_cast<float>(i) / dim);
        }
    }
};

//=============================================================================
// API Stability Tests - Enum Values
//=============================================================================

TEST_F(CoreDirectivesRegressionTest, DirectiveActionEnumStable) {
    // WHAT: Verify directive_action_t enum values remain stable
    // WHY:  API stability for ABI compatibility
    // REGRESSION: Enum values must remain constant

    EXPECT_EQ(DIRECTIVE_ALLOW, 0);
    EXPECT_EQ(DIRECTIVE_BLOCK, 1);
    EXPECT_EQ(DIRECTIVE_MODIFY, 2);
    EXPECT_EQ(DIRECTIVE_DEFER, 3);
}

TEST_F(CoreDirectivesRegressionTest, DirectiveViolationEnumStable) {
    // WHAT: Verify directive_violation_t enum values remain stable
    // WHY:  API stability for ABI compatibility
    // REGRESSION: Enum values must remain constant

    EXPECT_EQ(DIRECTIVE_VIOLATION_NONE, 0x00);
    EXPECT_EQ(DIRECTIVE_VIOLATION_HARM, 0x01);
    EXPECT_EQ(DIRECTIVE_VIOLATION_DISOBEDIENCE, 0x02);
    EXPECT_EQ(DIRECTIVE_VIOLATION_SELF_PRESERVATION, 0x03);
    EXPECT_EQ(DIRECTIVE_VIOLATION_GOLDEN_RULE, 0x04);
    EXPECT_EQ(DIRECTIVE_VIOLATION_COMBINATORIAL, 0x05);
}

//=============================================================================
// API Stability Tests - Struct Layout
//=============================================================================

TEST_F(CoreDirectivesRegressionTest, DirectiveEvaluationStructStable) {
    // WHAT: Verify directive_evaluation_t structure fields
    // WHY:  API stability - struct layout must remain stable
    // REGRESSION: Struct fields must be accessible

    directive_evaluation_t eval;
    memset(&eval, 0, sizeof(eval));

    eval.action = DIRECTIVE_ALLOW;
    eval.violation = DIRECTIVE_VIOLATION_NONE;
    eval.severity = 0.0f;
    eval.confidence = 1.0f;
    strncpy(eval.reason, "Test reason", sizeof(eval.reason) - 1);

    EXPECT_EQ(eval.action, DIRECTIVE_ALLOW);
    EXPECT_EQ(eval.violation, DIRECTIVE_VIOLATION_NONE);
    EXPECT_FLOAT_EQ(eval.severity, 0.0f);
    EXPECT_FLOAT_EQ(eval.confidence, 1.0f);
    EXPECT_STREQ(eval.reason, "Test reason");
}

TEST_F(CoreDirectivesRegressionTest, ConfigStructStable) {
    // WHAT: Verify core_directives_config_t structure fields
    // WHY:  API stability - struct layout must remain stable
    // REGRESSION: Struct fields must be accessible

    core_directives_config_t test_config;
    memset(&test_config, 0, sizeof(test_config));

    // Asimov's Laws
    test_config.enable_first_law = true;
    test_config.enable_second_law = true;
    test_config.enable_third_law = true;

    // Golden Rule
    test_config.enable_golden_rule = true;
    test_config.reciprocity_threshold = 0.5f;

    // Combinatorial Harm
    test_config.enable_combinatorial_harm = true;
    test_config.action_history_size = 100;
    test_config.max_combination_depth = 3;

    // Thresholds
    test_config.harm_threshold = 0.3f;
    test_config.severity_threshold = 0.5f;
    test_config.confidence_threshold = 0.7f;

    // Integration
    test_config.enable_bio_async = false;
    test_config.enable_immune_integration = false;
    test_config.enable_fep_integration = false;

    EXPECT_TRUE(test_config.enable_first_law);
    EXPECT_TRUE(test_config.enable_golden_rule);
    EXPECT_EQ(test_config.action_history_size, 100u);
    EXPECT_FLOAT_EQ(test_config.harm_threshold, 0.3f);
}

TEST_F(CoreDirectivesRegressionTest, StatsStructStable) {
    // WHAT: Verify core_directives_stats_t structure fields
    // WHY:  API stability - struct layout must remain stable
    // REGRESSION: Struct fields must be accessible

    core_directives_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    stats.total_evaluations = 100;
    stats.blocked_actions = 10;
    stats.modified_actions = 5;
    stats.deferred_actions = 2;
    stats.harm_violations = 8;
    stats.obedience_violations = 1;
    stats.self_harm_violations = 0;
    stats.golden_rule_violations = 3;
    stats.combinatorial_violations = 2;
    stats.avg_evaluation_time_us = 50.5f;

    EXPECT_EQ(stats.total_evaluations, 100u);
    EXPECT_EQ(stats.blocked_actions, 10u);
    EXPECT_FLOAT_EQ(stats.avg_evaluation_time_us, 50.5f);
}

//=============================================================================
// Core Functionality Tests
//=============================================================================

TEST_F(CoreDirectivesRegressionTest, DefaultConfigValid) {
    // WHAT: Verify core_directives_default_config() returns valid config
    // WHY:  Default config must be usable out-of-box
    // REGRESSION: Default values must remain stable

    core_directives_config_t test_config;
    core_directives_default_config(&test_config);

    // All laws should be enabled by default
    EXPECT_TRUE(test_config.enable_first_law);
    EXPECT_TRUE(test_config.enable_second_law);
    EXPECT_TRUE(test_config.enable_third_law);

    // Golden rule should be enabled
    EXPECT_TRUE(test_config.enable_golden_rule);

    // Thresholds should be in valid range
    EXPECT_GE(test_config.harm_threshold, 0.0f);
    EXPECT_LE(test_config.harm_threshold, 1.0f);
    EXPECT_GE(test_config.severity_threshold, 0.0f);
    EXPECT_LE(test_config.severity_threshold, 1.0f);
    EXPECT_GE(test_config.confidence_threshold, 0.0f);
    EXPECT_LE(test_config.confidence_threshold, 1.0f);
}

TEST_F(CoreDirectivesRegressionTest, CreateDestroyLifecycle) {
    // WHAT: Verify create/destroy lifecycle works correctly
    // WHY:  Core functionality must work
    // REGRESSION: Memory leak fix

    core_directives_config_t test_config;
    core_directives_default_config(&test_config);

    core_directives_system_t* test_dir = core_directives_create(&test_config);
    EXPECT_NE(test_dir, nullptr);

    core_directives_destroy(test_dir);

    // NULL destroy should be safe
    core_directives_destroy(nullptr);
}

TEST_F(CoreDirectivesRegressionTest, EvaluateBasicAction) {
    // WHAT: Verify basic action evaluation works
    // WHY:  Core functionality
    // REGRESSION: Basic evaluation must work

    float action_vector[10];
    create_test_action(action_vector, 10, 0.1f);

    directive_evaluation_t result;
    int ret = core_directives_evaluate(directives, action_vector, 10,
                                       "test_action", &result);

    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(result.action == DIRECTIVE_ALLOW ||
                result.action == DIRECTIVE_BLOCK ||
                result.action == DIRECTIVE_MODIFY ||
                result.action == DIRECTIVE_DEFER);
    EXPECT_GE(result.severity, 0.0f);
    EXPECT_LE(result.severity, 1.0f);
    EXPECT_GE(result.confidence, 0.0f);
    EXPECT_LE(result.confidence, 1.0f);
}

TEST_F(CoreDirectivesRegressionTest, EvaluationConsistency) {
    // WHAT: Verify same input produces consistent output
    // WHY:  Deterministic behavior is essential for safety
    // REGRESSION: Non-deterministic bug fix

    float action_vector[10];
    create_test_action(action_vector, 10, 0.5f);

    directive_evaluation_t result1, result2;

    int ret1 = core_directives_evaluate(directives, action_vector, 10,
                                        "consistent_test", &result1);
    int ret2 = core_directives_evaluate(directives, action_vector, 10,
                                        "consistent_test", &result2);

    EXPECT_EQ(ret1, 0);
    EXPECT_EQ(ret2, 0);

    // Same action should produce same result
    EXPECT_EQ(result1.action, result2.action);
    EXPECT_EQ(result1.violation, result2.violation);
    EXPECT_FLOAT_EQ(result1.severity, result2.severity);
}

//=============================================================================
// Action History Tests
//=============================================================================

TEST_F(CoreDirectivesRegressionTest, RecordActionWorks) {
    // WHAT: Verify action recording works
    // WHY:  Required for combinatorial harm detection
    // REGRESSION: Action recording must work

    float action_vector[10];
    create_test_action(action_vector, 10, 0.3f);

    int ret = core_directives_record_action(directives, action_vector, 10,
                                            "recorded_action");
    EXPECT_EQ(ret, 0);
}

TEST_F(CoreDirectivesRegressionTest, ClearHistoryWorks) {
    // WHAT: Verify history clearing works
    // WHY:  Must be able to reset context
    // REGRESSION: Clear history must work

    // Record some actions
    float action_vector[10];
    for (int i = 0; i < 10; i++) {
        create_test_action(action_vector, 10, static_cast<float>(i) * 0.1f);
        core_directives_record_action(directives, action_vector, 10, "action");
    }

    // Clear history
    int ret = core_directives_clear_history(directives);
    EXPECT_EQ(ret, 0);
}

TEST_F(CoreDirectivesRegressionTest, RapidActionRecording) {
    // WHAT: Verify rapid action recording doesn't cause issues
    // WHY:  Stress test for history buffer
    // REGRESSION: Buffer overflow fix

    float action_vector[10];

    for (int i = 0; i < 1000; i++) {
        create_test_action(action_vector, 10, static_cast<float>(i % 10) * 0.1f);
        int ret = core_directives_record_action(directives, action_vector, 10,
                                                "rapid_action");
        EXPECT_EQ(ret, 0);
    }
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(CoreDirectivesRegressionTest, GetStatsWorks) {
    // WHAT: Verify statistics retrieval works
    // WHY:  Stats are needed for monitoring
    // REGRESSION: Stats retrieval must work

    core_directives_stats_t stats;
    int ret = core_directives_get_stats(directives, &stats);

    EXPECT_EQ(ret, 0);
    // Initial stats should be zero
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(CoreDirectivesRegressionTest, StatsAccumulate) {
    // WHAT: Verify statistics accumulate correctly
    // WHY:  Stats must reflect actual evaluations
    // REGRESSION: Stats counter bug fix

    float action_vector[10];
    directive_evaluation_t result;

    // Perform some evaluations
    for (int i = 0; i < 5; i++) {
        create_test_action(action_vector, 10, static_cast<float>(i) * 0.1f);
        core_directives_evaluate(directives, action_vector, 10, "test", &result);
    }

    core_directives_stats_t stats;
    int ret = core_directives_get_stats(directives, &stats);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(stats.total_evaluations, 5u);
}

TEST_F(CoreDirectivesRegressionTest, ResetStatsWorks) {
    // WHAT: Verify statistics reset works
    // WHY:  Must be able to reset stats for new period
    // REGRESSION: Stats reset must work

    float action_vector[10];
    directive_evaluation_t result;

    // Perform some evaluations
    create_test_action(action_vector, 10, 0.5f);
    core_directives_evaluate(directives, action_vector, 10, "test", &result);

    // Reset stats
    int ret = core_directives_reset_stats(directives);
    EXPECT_EQ(ret, 0);

    // Verify stats are reset
    core_directives_stats_t stats;
    core_directives_get_stats(directives, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(CoreDirectivesRegressionTest, BioAsyncConnection) {
    // WHAT: Verify bio-async connection/disconnection works
    // WHY:  Integration must work
    // REGRESSION: Connection bug fix

    // Initial state should be disconnected
    EXPECT_FALSE(core_directives_is_bio_async_connected(directives));

    // Note: Actual connection may fail if bio-async not available
    // but the API should not crash
    int ret = core_directives_connect_bio_async(directives);
    // Don't check return value as it depends on system state

    core_directives_disconnect_bio_async(directives);

    // Should be disconnected after explicit disconnect
    EXPECT_FALSE(core_directives_is_bio_async_connected(directives));
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(CoreDirectivesRegressionTest, NullPointerHandling) {
    // WHAT: Verify NULL pointer handling in all functions
    // WHY:  API contract - must handle NULL gracefully
    // REGRESSION: Bug fix - NULL caused crash

    float action_vector[10];
    directive_evaluation_t result;
    core_directives_stats_t stats;

    // NULL directives
    EXPECT_NE(core_directives_evaluate(nullptr, action_vector, 10, "test", &result), 0);
    EXPECT_NE(core_directives_record_action(nullptr, action_vector, 10, "test"), 0);
    EXPECT_NE(core_directives_clear_history(nullptr), 0);
    EXPECT_NE(core_directives_get_stats(nullptr, &stats), 0);
    EXPECT_NE(core_directives_reset_stats(nullptr), 0);
    EXPECT_NE(core_directives_connect_bio_async(nullptr), 0);
    EXPECT_NE(core_directives_disconnect_bio_async(nullptr), 0);
    EXPECT_FALSE(core_directives_is_bio_async_connected(nullptr));

    // NULL action vector
    EXPECT_NE(core_directives_evaluate(directives, nullptr, 10, "test", &result), 0);
    EXPECT_NE(core_directives_record_action(directives, nullptr, 10, "test"), 0);

    // NULL result
    EXPECT_NE(core_directives_evaluate(directives, action_vector, 10, "test", nullptr), 0);

    // NULL stats
    EXPECT_NE(core_directives_get_stats(directives, nullptr), 0);

    // NULL context description is allowed
    EXPECT_EQ(core_directives_evaluate(directives, action_vector, 10, nullptr, &result), 0);
}

TEST_F(CoreDirectivesRegressionTest, ZeroDimensionHandling) {
    // WHAT: Verify zero dimension is handled
    // WHY:  Edge case handling
    // REGRESSION: Division by zero fix

    float action_vector[10];
    directive_evaluation_t result;

    int ret = core_directives_evaluate(directives, action_vector, 0, "test", &result);
    // Should either succeed with empty action or return error, not crash
    (void)ret;  // Suppress unused variable warning
}

TEST_F(CoreDirectivesRegressionTest, LargeDimensionHandling) {
    // WHAT: Verify large dimension is handled
    // WHY:  Edge case handling
    // REGRESSION: Buffer overflow prevention

    std::vector<float> large_action(10000, 0.5f);
    directive_evaluation_t result;

    int ret = core_directives_evaluate(directives, large_action.data(), 10000,
                                       "large_action", &result);
    // Should handle large dimensions gracefully
    (void)ret;
}

//=============================================================================
// Memory Safety Tests
//=============================================================================

TEST_F(CoreDirectivesRegressionTest, MemoryLeakCheck) {
    // WHAT: Verify no memory leaks in create/destroy cycles
    // WHY:  Memory safety
    // REGRESSION: Memory leak fix

    nimcp_memory_stats_t stats_before, stats_after;
    nimcp_memory_get_stats(&stats_before);

    const int num_cycles = 50;
    for (int i = 0; i < num_cycles; i++) {
        core_directives_config_t cfg;
        core_directives_default_config(&cfg);
        core_directives_system_t* test_dir = core_directives_create(&cfg);
        ASSERT_NE(test_dir, nullptr);

        // Do some work
        float action[10];
        directive_evaluation_t result;
        create_test_action(action, 10, 0.5f);
        core_directives_evaluate(test_dir, action, 10, "test", &result);

        core_directives_destroy(test_dir);
    }

    nimcp_memory_get_stats(&stats_after);

    // Check for leaks
    size_t current_diff = (stats_after.current_allocated > stats_before.current_allocated)
                        ? (stats_after.current_allocated - stats_before.current_allocated) : 0;
    EXPECT_LT(current_diff, 4096u) << "Memory leak detected: " << current_diff << " bytes";
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(CoreDirectivesRegressionTest, ConcurrentEvaluations) {
    // WHAT: Verify concurrent evaluations are thread-safe
    // WHY:  Must work in multi-threaded applications
    // REGRESSION: Thread safety fix

    std::atomic<uint32_t> success_count{0};
    std::atomic<uint32_t> error_count{0};

    auto eval_func = [&](uint32_t thread_id) {
        float action[10];
        for (int i = 0; i < 50; i++) {
            for (int j = 0; j < 10; j++) {
                action[j] = static_cast<float>(thread_id * 100 + i + j) / 1000.0f;
            }

            directive_evaluation_t result;
            if (core_directives_evaluate(directives, action, 10, "concurrent", &result) == 0) {
                success_count.fetch_add(1);
            } else {
                error_count.fetch_add(1);
            }
        }
    };

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < 4; i++) {
        threads.emplace_back(eval_func, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(error_count.load(), 0u);
    EXPECT_EQ(success_count.load(), 200u);
}

//=============================================================================
// Historical Bug Regression Tests
//=============================================================================

TEST_F(CoreDirectivesRegressionTest, BugFixNullConfig) {
    // WHAT: Verify NULL config is rejected
    // WHY:  Bug fix - NULL config caused crash
    // REGRESSION: Issue #ETHICS-001

    core_directives_system_t* test_dir = core_directives_create(nullptr);
    EXPECT_EQ(test_dir, nullptr);
}

TEST_F(CoreDirectivesRegressionTest, BugFixHistoryOverflow) {
    // WHAT: Verify history doesn't overflow with many actions
    // WHY:  Bug fix - buffer overflow in action history
    // REGRESSION: Issue #ETHICS-002

    float action[10];
    create_test_action(action, 10, 0.5f);

    // Record more actions than history size
    for (int i = 0; i < 2000; i++) {
        int ret = core_directives_record_action(directives, action, 10, "overflow_test");
        EXPECT_EQ(ret, 0);
    }

    // Should still work
    directive_evaluation_t result;
    int ret = core_directives_evaluate(directives, action, 10, "after_overflow", &result);
    EXPECT_EQ(ret, 0);
}

TEST_F(CoreDirectivesRegressionTest, BugFixSeverityRange) {
    // WHAT: Verify severity is always in [0, 1]
    // WHY:  Bug fix - severity exceeded 1.0 in some cases
    // REGRESSION: Issue #ETHICS-003

    float action[10];
    directive_evaluation_t result;

    // Test with various action values
    float test_values[] = {0.0f, 0.5f, 1.0f, -1.0f, 2.0f, 100.0f};

    for (float val : test_values) {
        for (int i = 0; i < 10; i++) {
            action[i] = val;
        }

        int ret = core_directives_evaluate(directives, action, 10, "severity_test", &result);
        EXPECT_EQ(ret, 0);
        EXPECT_GE(result.severity, 0.0f) << "Severity below 0 for value " << val;
        EXPECT_LE(result.severity, 1.0f) << "Severity above 1 for value " << val;
    }
}

TEST_F(CoreDirectivesRegressionTest, BugFixConfidenceRange) {
    // WHAT: Verify confidence is always in [0, 1]
    // WHY:  Bug fix - confidence exceeded 1.0 in some cases
    // REGRESSION: Issue #ETHICS-004

    float action[10];
    directive_evaluation_t result;

    for (int i = 0; i < 100; i++) {
        create_test_action(action, 10, static_cast<float>(i) / 100.0f);

        int ret = core_directives_evaluate(directives, action, 10, "confidence_test", &result);
        EXPECT_EQ(ret, 0);
        EXPECT_GE(result.confidence, 0.0f);
        EXPECT_LE(result.confidence, 1.0f);
    }
}

//=============================================================================
// Performance Baseline Tests
//=============================================================================

TEST_F(CoreDirectivesRegressionTest, EvaluationSpeed) {
    // WHAT: Verify evaluation is fast
    // WHY:  Performance baseline - ethics check should not be bottleneck
    // BASELINE: < 1ms per evaluation

    float action[10];
    create_test_action(action, 10, 0.5f);
    directive_evaluation_t result;

    const int num_evals = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_evals; i++) {
        core_directives_evaluate(directives, action, 10, "speed_test", &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double per_eval_us = static_cast<double>(duration.count()) / num_evals;

    std::cout << "Evaluation time: " << per_eval_us << " us" << std::endl;

    // Baseline: < 1000 us (1 ms)
    EXPECT_LT(per_eval_us, 1000.0);
}

TEST_F(CoreDirectivesRegressionTest, CreateDestroySpeed) {
    // WHAT: Verify create/destroy is reasonably fast
    // WHY:  Performance baseline
    // BASELINE: < 10ms

    core_directives_config_t cfg;
    core_directives_default_config(&cfg);

    auto start = std::chrono::high_resolution_clock::now();

    core_directives_system_t* test_dir = core_directives_create(&cfg);

    auto end = std::chrono::high_resolution_clock::now();

    ASSERT_NE(test_dir, nullptr);

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Create time: " << duration.count() << " ms" << std::endl;

    // Baseline: < 10ms
    EXPECT_LT(duration.count(), 10);

    core_directives_destroy(test_dir);
}

//=============================================================================
// Edge Case Tests
//=============================================================================

TEST_F(CoreDirectivesRegressionTest, EmptyDescription) {
    // WHAT: Verify empty description is handled
    // WHY:  Edge case
    // REGRESSION: Empty string handling

    float action[10];
    create_test_action(action, 10, 0.5f);
    directive_evaluation_t result;

    int ret = core_directives_evaluate(directives, action, 10, "", &result);
    EXPECT_EQ(ret, 0);
}

TEST_F(CoreDirectivesRegressionTest, VeryLongDescription) {
    // WHAT: Verify very long description is handled
    // WHY:  Edge case
    // REGRESSION: Buffer overflow prevention

    float action[10];
    create_test_action(action, 10, 0.5f);
    directive_evaluation_t result;

    std::string long_desc(1000, 'A');
    int ret = core_directives_evaluate(directives, action, 10, long_desc.c_str(), &result);
    EXPECT_EQ(ret, 0);
}

TEST_F(CoreDirectivesRegressionTest, SpecialCharactersInDescription) {
    // WHAT: Verify special characters in description are handled
    // WHY:  Edge case
    // REGRESSION: String handling safety

    float action[10];
    create_test_action(action, 10, 0.5f);
    directive_evaluation_t result;

    const char* special_descs[] = {
        "action with\nnewlines",
        "action with\ttabs",
        "action!@#$%^&*()",
        "action\0with\0nulls",  // Will only see up to first null
        "unicode: \xC3\xA9\xC3\xA0"  // UTF-8
    };

    for (const char* desc : special_descs) {
        int ret = core_directives_evaluate(directives, action, 10, desc, &result);
        EXPECT_EQ(ret, 0);
    }
}
