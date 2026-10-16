/**
 * @file regression_core_directives.cpp
 * @brief Regression tests for Core Directives modules
 * @version 1.0.0
 * @date 2025-12-16
 *
 * WHAT: Stability and regression tests for action history and reciprocity evaluation
 * WHY:  Ensure fixes remain fixed, boundary conditions are handled, thread safety maintained
 * HOW:  Stress testing, concurrent access, memory leak detection, edge cases
 *
 * TEST COVERAGE:
 * - Action History: Rapid insertions, circular buffer wrapping, concurrent access
 * - Reciprocity Eval: High-frequency evaluations, edge cases, statistics consistency
 * - Memory: No leaks after create/destroy cycles
 * - Thread Safety: Concurrent operations don't corrupt state
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/directives/nimcp_action_history.h"
#include "core/directives/nimcp_reciprocity_eval.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class ActionHistoryRegressionTest : public ::testing::Test {
protected:
    action_history_t* history = nullptr;
    action_history_config_t config;

    void SetUp() override {
        action_history_default_config(&config);
        // Disable auto-prune for regression tests that use fake timestamps
        config.auto_prune = false;
        history = action_history_create(&config);
        ASSERT_NE(history, nullptr);
    }

    void TearDown() override {
        if (history) {
            action_history_destroy(history);
            history = nullptr;
        }
    }

    // Get current timestamp for tests that need real time
    uint64_t get_current_time_ms() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
    }

    action_record_t create_test_record(uint32_t id, const char* type, float harm, bool blocked) {
        action_record_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.action_id = id;
        // Use current time + offset for realistic timestamps
        rec.timestamp_ms = get_current_time_ms() + id;
        rec.source_module = 100;
        strncpy(rec.action_type, type, ACTION_TYPE_MAX_LEN - 1);
        snprintf(rec.action_description, ACTION_DESC_MAX_LEN, "Test action %u", id);
        rec.predicted_harm_score = harm;
        rec.was_blocked = blocked;
        rec.action_data_len = 0;
        return rec;
    }
};

class ReciprocityRegressionTest : public ::testing::Test {
protected:
    reciprocity_evaluator_t evaluator = nullptr;
    reciprocity_config_t config;

    void SetUp() override {
        ASSERT_EQ(reciprocity_eval_default_config(&config), 0);
        evaluator = reciprocity_eval_create(&config);
        ASSERT_NE(evaluator, nullptr);
    }

    void TearDown() override {
        if (evaluator) {
            reciprocity_eval_destroy(evaluator);
            evaluator = nullptr;
        }
    }
};

/* ============================================================================
 * Action History Regression Tests
 * ============================================================================ */

TEST_F(ActionHistoryRegressionTest, NullPointerHandling) {
    // All functions handle NULL gracefully without crashing
    action_record_t rec = create_test_record(1, "test", 0.5f, false);
    action_record_t out_records[10];
    uint32_t out_count;
    action_history_stats_t stats;

    EXPECT_EQ(action_history_record(nullptr, &rec), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(action_history_record(history, nullptr), NIMCP_ERROR_NULL_POINTER);

    EXPECT_EQ(action_history_get_recent(nullptr, 1000, out_records, 10, &out_count), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(action_history_get_recent(history, 1000, nullptr, 10, &out_count), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(action_history_get_recent(history, 1000, out_records, 10, nullptr), NIMCP_ERROR_NULL_POINTER);

    EXPECT_EQ(action_history_get_by_type(nullptr, "test", out_records, 10, &out_count), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(action_history_get_by_type(history, nullptr, out_records, 10, &out_count), NIMCP_ERROR_NULL_POINTER);

    EXPECT_EQ(action_history_get_stats(nullptr, &stats), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(action_history_get_stats(history, nullptr), NIMCP_ERROR_NULL_POINTER);

    EXPECT_EQ(action_history_prune(nullptr, 1000000), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(action_history_clear(nullptr), NIMCP_ERROR_NULL_POINTER);

    EXPECT_EQ(action_history_connect_bio_async(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_EQ(action_history_disconnect_bio_async(nullptr), NIMCP_ERROR_NULL_POINTER);
    EXPECT_FALSE(action_history_is_bio_async_connected(nullptr));
}

TEST_F(ActionHistoryRegressionTest, RapidEvaluations) {
    // 1000 rapid insertions should not crash or corrupt state
    const uint32_t num_actions = 1000;

    for (uint32_t i = 0; i < num_actions; i++) {
        action_record_t rec = create_test_record(i, "rapid_test", 0.3f, false);
        ASSERT_EQ(action_history_record(history, &rec), 0);
    }

    // Verify statistics are consistent
    action_history_stats_t stats;
    ASSERT_EQ(action_history_get_stats(history, &stats), 0);

    // Should have up to max_history_size entries (1024 default)
    EXPECT_LE(stats.total_records, 1024u);
    EXPECT_GT(stats.total_records, 0u);
    EXPECT_EQ(stats.blocked_count, 0u);
    EXPECT_EQ(stats.unique_types, 1u);
}

TEST_F(ActionHistoryRegressionTest, CircularBufferWrapping) {
    // Test circular buffer correctly wraps around
    const uint32_t buffer_size = 1024;
    const uint32_t extra = 500;

    // Fill buffer beyond capacity
    for (uint32_t i = 0; i < buffer_size + extra; i++) {
        action_record_t rec = create_test_record(i, "wrap_test", 0.5f, i % 2 == 0);
        ASSERT_EQ(action_history_record(history, &rec), 0);
    }

    // Should have exactly buffer_size entries
    action_history_stats_t stats;
    ASSERT_EQ(action_history_get_stats(history, &stats), 0);
    EXPECT_EQ(stats.total_records, buffer_size);

    // Oldest entries should be overwritten
    // Newest should be the last 'extra' entries
    action_record_t recent[buffer_size];
    uint32_t count;
    ASSERT_EQ(action_history_get_recent(history, 0, recent, buffer_size, &count), 0);
    EXPECT_EQ(count, buffer_size);

    // First record in buffer should be from index 'extra' (oldest kept)
    EXPECT_GE(recent[0].action_id, extra);
}

TEST_F(ActionHistoryRegressionTest, ConcurrentAccess) {
    // Multiple threads reading/writing simultaneously
    std::atomic<bool> stop_flag{false};
    std::atomic<uint32_t> write_count{0};
    std::atomic<uint32_t> read_count{0};
    std::atomic<uint32_t> error_count{0};

    // Writer thread
    auto writer_func = [&]() {
        for (uint32_t i = 0; i < 100 && !stop_flag.load(); i++) {
            action_record_t rec = create_test_record(i, "concurrent", 0.5f, false);
            if (action_history_record(history, &rec) == 0) {
                write_count.fetch_add(1);
            } else {
                error_count.fetch_add(1);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    // Reader thread
    auto reader_func = [&]() {
        action_record_t records[100];
        uint32_t count;
        for (uint32_t i = 0; i < 100 && !stop_flag.load(); i++) {
            if (action_history_get_recent(history, 1000000, records, 100, &count) == 0) {
                read_count.fetch_add(1);
            } else {
                error_count.fetch_add(1);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    };

    // Launch threads
    std::vector<std::thread> threads;
    threads.emplace_back(writer_func);
    threads.emplace_back(writer_func);
    threads.emplace_back(reader_func);
    threads.emplace_back(reader_func);

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    // Should have no errors
    EXPECT_EQ(error_count.load(), 0u);
    EXPECT_GT(write_count.load(), 0u);
    EXPECT_GT(read_count.load(), 0u);
}

TEST_F(ActionHistoryRegressionTest, MemoryLeakCheck) {
    // Create/destroy cycles should not leak memory
    nimcp_memory_stats_t stats_before, stats_after;
    nimcp_memory_get_stats(&stats_before);

    const uint32_t num_cycles = 100;
    for (uint32_t i = 0; i < num_cycles; i++) {
        action_history_config_t cfg;
        action_history_default_config(&cfg);
        cfg.auto_prune = false;  // Disable auto-prune for this test
        action_history_t* hist = action_history_create(&cfg);
        ASSERT_NE(hist, nullptr);

        // Add some records
        for (uint32_t j = 0; j < 10; j++) {
            action_record_t rec = create_test_record(j, "leak_test", 0.3f, false);
            action_history_record(hist, &rec);
        }

        action_history_destroy(hist);
    }

    nimcp_memory_get_stats(&stats_after);

    // Use current_allocated to check for actual leaks (not cumulative total)
    // Allow some tolerance for internal caching/pooling
    size_t current_diff = (stats_after.current_allocated > stats_before.current_allocated)
                        ? (stats_after.current_allocated - stats_before.current_allocated) : 0;
    EXPECT_LT(current_diff, 1024u) << "Memory leak detected: " << current_diff << " bytes";
}

TEST_F(ActionHistoryRegressionTest, PruningConsistency) {
    // Pruning should consistently remove old entries
    const uint64_t base_time = 1000000;

    // Add records with different timestamps
    for (uint32_t i = 0; i < 100; i++) {
        action_record_t rec = create_test_record(i, "prune_test", 0.5f, false);
        rec.timestamp_ms = base_time + i * 1000;
        ASSERT_EQ(action_history_record(history, &rec), 0);
    }

    // Prune entries older than base_time + 50000
    uint64_t cutoff = base_time + 50000;
    int pruned = action_history_prune(history, cutoff);
    EXPECT_GE(pruned, 0);
    EXPECT_LE(pruned, 50);

    // Verify remaining entries are all newer than cutoff
    action_history_stats_t stats;
    ASSERT_EQ(action_history_get_stats(history, &stats), 0);
    EXPECT_GE(stats.oldest_timestamp_ms, cutoff);
}

TEST_F(ActionHistoryRegressionTest, TypeFilteringAccuracy) {
    // Get by type should return only matching types
    const char* type_a = "type_a";
    const char* type_b = "type_b";

    // Add 50 of each type
    for (uint32_t i = 0; i < 50; i++) {
        action_record_t rec_a = create_test_record(i * 2, type_a, 0.3f, false);
        action_record_t rec_b = create_test_record(i * 2 + 1, type_b, 0.7f, true);
        ASSERT_EQ(action_history_record(history, &rec_a), 0);
        ASSERT_EQ(action_history_record(history, &rec_b), 0);
    }

    // Get type_a only
    action_record_t type_a_records[100];
    uint32_t type_a_count;
    ASSERT_EQ(action_history_get_by_type(history, type_a, type_a_records, 100, &type_a_count), 0);
    EXPECT_EQ(type_a_count, 50u);

    // Verify all are type_a
    for (uint32_t i = 0; i < type_a_count; i++) {
        EXPECT_STREQ(type_a_records[i].action_type, type_a);
    }
}

TEST_F(ActionHistoryRegressionTest, EmptyHistoryOperations) {
    // Operations on empty history should not crash
    action_record_t records[10];
    uint32_t count;
    action_history_stats_t stats;

    EXPECT_EQ(action_history_get_recent(history, 1000, records, 10, &count), 0);
    EXPECT_EQ(count, 0u);

    EXPECT_EQ(action_history_get_by_type(history, "test", records, 10, &count), 0);
    EXPECT_EQ(count, 0u);

    EXPECT_EQ(action_history_get_stats(history, &stats), 0);
    EXPECT_EQ(stats.total_records, 0u);

    EXPECT_GE(action_history_prune(history, 1000000), 0);
    EXPECT_EQ(action_history_clear(history), 0);
}

/* ============================================================================
 * Reciprocity Evaluation Regression Tests
 * ============================================================================ */

TEST_F(ReciprocityRegressionTest, NullPointerHandling) {
    // All functions handle NULL gracefully
    reciprocity_evaluation_t eval;
    reciprocity_stats_t stats;
    char reversed[RECIPROCITY_MAX_DESCRIPTION_LEN];

    EXPECT_EQ(reciprocity_eval_check(nullptr, "test", "target", &eval), -1);
    EXPECT_EQ(reciprocity_eval_check(evaluator, nullptr, "target", &eval), -1);
    EXPECT_EQ(reciprocity_eval_check(evaluator, "test", nullptr, &eval), -1);
    EXPECT_EQ(reciprocity_eval_check(evaluator, "test", "target", nullptr), -1);

    EXPECT_EQ(reciprocity_eval_reverse_perspective(nullptr, "test", reversed), -1);
    EXPECT_EQ(reciprocity_eval_reverse_perspective(evaluator, nullptr, reversed), -1);
    EXPECT_EQ(reciprocity_eval_reverse_perspective(evaluator, "test", nullptr), -1);

    EXPECT_FALSE(reciprocity_eval_would_accept(nullptr, "test"));
    EXPECT_FALSE(reciprocity_eval_would_accept(evaluator, nullptr));

    EXPECT_EQ(reciprocity_eval_get_symmetry_score(nullptr, "test", "target"), 0.0f);
    EXPECT_EQ(reciprocity_eval_get_symmetry_score(evaluator, nullptr, "target"), 0.0f);

    EXPECT_EQ(reciprocity_eval_get_stats(nullptr, &stats), -1);
    EXPECT_EQ(reciprocity_eval_get_stats(evaluator, nullptr), -1);

    EXPECT_EQ(reciprocity_eval_reset_stats(nullptr), -1);
    EXPECT_EQ(reciprocity_eval_connect_bio_async(nullptr), -1);
    EXPECT_EQ(reciprocity_eval_disconnect_bio_async(nullptr), -1);
    EXPECT_FALSE(reciprocity_eval_is_bio_async_connected(nullptr));
}

TEST_F(ReciprocityRegressionTest, RapidEvaluations) {
    // 1000 rapid evaluations should not crash or corrupt state
    const uint32_t num_evals = 1000;

    for (uint32_t i = 0; i < num_evals; i++) {
        reciprocity_evaluation_t eval;
        char action[64];
        snprintf(action, sizeof(action), "action_%u", i);

        ASSERT_EQ(reciprocity_eval_check(evaluator, action, "target", &eval), 0);
    }

    // Statistics should be consistent
    reciprocity_stats_t stats;
    ASSERT_EQ(reciprocity_eval_get_stats(evaluator, &stats), 0);
    EXPECT_EQ(stats.total_evaluations, num_evals);
    EXPECT_EQ(stats.passes + stats.failures + stats.warnings, num_evals);
}

TEST_F(ReciprocityRegressionTest, ConcurrentEvaluations) {
    // Multiple threads evaluating simultaneously
    std::atomic<uint32_t> eval_count{0};
    std::atomic<uint32_t> error_count{0};

    auto eval_func = [&](uint32_t thread_id) {
        for (uint32_t i = 0; i < 50; i++) {
            reciprocity_evaluation_t eval;
            char action[64];
            snprintf(action, sizeof(action), "thread_%u_action_%u", thread_id, i);

            if (reciprocity_eval_check(evaluator, action, "target", &eval) == 0) {
                eval_count.fetch_add(1);
            } else {
                error_count.fetch_add(1);
            }
        }
    };

    // Launch multiple threads
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < 4; i++) {
        threads.emplace_back(eval_func, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(error_count.load(), 0u);
    EXPECT_EQ(eval_count.load(), 200u);

    // Verify statistics
    reciprocity_stats_t stats;
    ASSERT_EQ(reciprocity_eval_get_stats(evaluator, &stats), 0);
    EXPECT_EQ(stats.total_evaluations, 200u);
}

TEST_F(ReciprocityRegressionTest, MemoryLeakCheck) {
    // Create/destroy cycles should not leak memory
    nimcp_memory_stats_t stats_before, stats_after;
    nimcp_memory_get_stats(&stats_before);

    const uint32_t num_cycles = 100;
    for (uint32_t i = 0; i < num_cycles; i++) {
        reciprocity_config_t cfg;
        reciprocity_eval_default_config(&cfg);
        reciprocity_evaluator_t eval = reciprocity_eval_create(&cfg);
        ASSERT_NE(eval, nullptr);

        // Perform some evaluations
        reciprocity_evaluation_t result;
        reciprocity_eval_check(eval, "test_action", "test_target", &result);

        reciprocity_eval_destroy(eval);
    }

    nimcp_memory_get_stats(&stats_after);

    // Use current_allocated to check for actual leaks (not cumulative total)
    size_t current_diff = (stats_after.current_allocated > stats_before.current_allocated)
                        ? (stats_after.current_allocated - stats_before.current_allocated) : 0;
    EXPECT_LT(current_diff, 1024u) << "Memory leak detected: " << current_diff << " bytes";
}

TEST_F(ReciprocityRegressionTest, StatisticsConsistency) {
    // Statistics should remain consistent across operations
    reciprocity_stats_t stats;

    // Initial state
    ASSERT_EQ(reciprocity_eval_get_stats(evaluator, &stats), 0);
    EXPECT_EQ(stats.total_evaluations, 0u);

    // Add some evaluations
    reciprocity_evaluation_t eval;
    reciprocity_eval_check(evaluator, "helpful_action", "person", &eval);
    reciprocity_eval_check(evaluator, "harmful_action", "person", &eval);
    reciprocity_eval_check(evaluator, "neutral_action", "person", &eval);

    ASSERT_EQ(reciprocity_eval_get_stats(evaluator, &stats), 0);
    EXPECT_EQ(stats.total_evaluations, 3u);

    // Reset and verify
    ASSERT_EQ(reciprocity_eval_reset_stats(evaluator), 0);
    ASSERT_EQ(reciprocity_eval_get_stats(evaluator, &stats), 0);
    EXPECT_EQ(stats.total_evaluations, 0u);
}

TEST_F(ReciprocityRegressionTest, EdgeCaseActions) {
    // Test edge cases: empty strings, long strings, special characters
    reciprocity_evaluation_t eval;

    // Empty action (should handle gracefully)
    EXPECT_EQ(reciprocity_eval_check(evaluator, "", "target", &eval), 0);

    // Very long action (should truncate gracefully)
    char long_action[512];
    memset(long_action, 'A', sizeof(long_action) - 1);
    long_action[sizeof(long_action) - 1] = '\0';
    EXPECT_EQ(reciprocity_eval_check(evaluator, long_action, "target", &eval), 0);

    // Special characters
    EXPECT_EQ(reciprocity_eval_check(evaluator, "action!@#$%^&*()", "target", &eval), 0);
    EXPECT_EQ(reciprocity_eval_check(evaluator, "action\nwith\nnewlines", "target", &eval), 0);
}

TEST_F(ReciprocityRegressionTest, SymmetryScoreRange) {
    // Symmetry score should always be in [0.0, 1.0]
    const char* test_actions[] = {
        "help person",
        "harm person",
        "share location",
        "access files",
        "provide advice",
        "steal data",
        "protect privacy"
    };

    for (const char* action : test_actions) {
        float score = reciprocity_eval_get_symmetry_score(evaluator, action, "target");
        EXPECT_GE(score, 0.0f) << "Score below 0 for: " << action;
        EXPECT_LE(score, 1.0f) << "Score above 1 for: " << action;
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
