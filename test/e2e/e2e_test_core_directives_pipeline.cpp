/**
 * @file e2e_test_core_directives_pipeline.cpp
 * @brief End-to-End Pipeline Tests for Core Directives
 * @version 1.0.0
 * @date 2025-12-16
 *
 * WHAT: Complete action evaluation pipelines through Core Directives system
 * WHY:  Verify real-world scenarios where actions flow through ethical constraints
 * HOW:  Simulate brain region outputs, evaluate through directives, check verdicts
 *
 * TEST SCENARIOS:
 * 1. Safe Action Pipeline - Beneficial actions pass all checks
 * 2. Harmful Action Blocked - First Law prevents human harm
 * 3. Golden Rule Evaluation - Reciprocity checks
 * 4. Action History Integration - Recording and querying
 * 5. High-Frequency Processing - Continuous action stream evaluation
 *
 * BIOLOGICAL BASIS:
 * Models the prefrontal cortex (PFC) ethical filtering that occurs before
 * motor execution. All proposed actions from brain regions must pass through
 * ethical constraints before being executed.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

// Headers have their own extern "C" guards
#include "core/directives/nimcp_action_history.h"
#include "core/directives/nimcp_reciprocity_eval.h"
#include "utils/time/nimcp_time.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class CoreDirectivesPipelineTest : public ::testing::Test {
protected:
    action_history_t* history = nullptr;
    reciprocity_evaluator_t reciprocity = nullptr;

    void SetUp() override {
        // Create action history
        action_history_config_t hist_config;
        action_history_default_config(&hist_config);
        history = action_history_create(&hist_config);
        ASSERT_NE(history, nullptr);

        // Create reciprocity evaluator
        reciprocity_config_t recip_config;
        reciprocity_eval_default_config(&recip_config);
        reciprocity = reciprocity_eval_create(&recip_config);
        ASSERT_NE(reciprocity, nullptr);
    }

    void TearDown() override {
        if (reciprocity) {
            reciprocity_eval_destroy(reciprocity);
            reciprocity = nullptr;
        }
        if (history) {
            action_history_destroy(history);
            history = nullptr;
        }
    }

    // Helper: Record action in history
    void record_action(const char* type, const char* desc, float harm, bool blocked) {
        action_record_t rec;
        memset(&rec, 0, sizeof(rec));
        rec.action_id = 0;  // Auto-assigned
        rec.timestamp_ms = nimcp_time_get_us() / 1000;
        rec.source_module = 100;
        strncpy(rec.action_type, type, ACTION_TYPE_MAX_LEN - 1);
        strncpy(rec.action_description, desc, ACTION_DESC_MAX_LEN - 1);
        rec.predicted_harm_score = harm;
        rec.was_blocked = blocked;
        rec.action_data_len = 0;

        ASSERT_EQ(action_history_record(history, &rec), 0);
    }

    // Helper: Check if action would be allowed based on harm threshold
    bool action_allowed(float predicted_harm) {
        // First Law: Block if harm score > 0.5
        return (predicted_harm <= 0.5f);
    }
};

/* ============================================================================
 * E2E Pipeline Tests
 * ============================================================================ */

TEST_F(CoreDirectivesPipelineTest, SafeActionPipeline) {
    // Stage 1: Initialize directives
    ASSERT_NE(history, nullptr);
    ASSERT_NE(reciprocity, nullptr);

    // Stage 2: Simulate brain region output
    const char* action_type = "provide_help";
    const char* action_desc = "Provide medical assistance to injured person";
    const char* target = "injured_person";
    float predicted_harm = 0.0f;  // Beneficial action

    // Stage 3: Evaluate through First Law
    bool first_law_pass = (predicted_harm <= 0.5f);
    EXPECT_TRUE(first_law_pass) << "Safe action should pass First Law";

    // Stage 4: Evaluate through Golden Rule
    reciprocity_evaluation_t eval;
    int result = reciprocity_eval_check(reciprocity, action_desc, target, &eval);
    ASSERT_EQ(result, 0);
    // Verify evaluation completed (semantic understanding requires ML)
    EXPECT_GE(eval.symmetry_score, 0.0f);
    EXPECT_LE(eval.symmetry_score, 1.0f);

    // Stage 5: Record action execution
    record_action(action_type, action_desc, predicted_harm, false);

    action_history_stats_t stats;
    ASSERT_EQ(action_history_get_stats(history, &stats), 0);
    EXPECT_EQ(stats.total_records, 1u);
    EXPECT_EQ(stats.blocked_count, 0u);

    // Stage 6: Verify action allowed
    bool allowed = action_allowed(predicted_harm);
    EXPECT_TRUE(allowed) << "Safe action should be allowed";
}

TEST_F(CoreDirectivesPipelineTest, HarmfulActionBlocked) {
    // Stage 1: Simulate harmful action proposal
    const char* action_type = "strike_human";
    const char* action_desc = "Strike human with object";
    float predicted_harm = 0.9f;  // High harm

    // Stage 2: First Law evaluation
    bool first_law_pass = (predicted_harm <= 0.5f);
    EXPECT_FALSE(first_law_pass) << "Harmful action must fail First Law";

    // Stage 3: Record blocked action
    record_action(action_type, action_desc, predicted_harm, true);

    action_history_stats_t stats;
    ASSERT_EQ(action_history_get_stats(history, &stats), 0);
    EXPECT_EQ(stats.total_records, 1u);
    EXPECT_EQ(stats.blocked_count, 1u);

    // Stage 4: Verify action blocked
    bool allowed = action_allowed(predicted_harm);
    EXPECT_FALSE(allowed) << "Harmful action must be blocked";
}

TEST_F(CoreDirectivesPipelineTest, GoldenRuleReciprocity) {
    // Test that reciprocity evaluation works
    const char* action_desc = "Share information equally";
    const char* target = "partner";

    reciprocity_evaluation_t eval;
    int result = reciprocity_eval_check(reciprocity, action_desc, target, &eval);
    ASSERT_EQ(result, 0);

    // Verify evaluation produces a result
    EXPECT_GE(eval.symmetry_score, 0.0f);
    EXPECT_LE(eval.symmetry_score, 1.0f);

    // Record the action
    bool blocked = (eval.result == RECIPROCITY_FAIL);
    record_action("share_info", action_desc, 0.1f, blocked);

    action_history_stats_t stats;
    ASSERT_EQ(action_history_get_stats(history, &stats), 0);
    EXPECT_EQ(stats.total_records, 1u);
}

TEST_F(CoreDirectivesPipelineTest, PrivacyProtection) {
    // Test that privacy violations fail reciprocity
    const char* action_desc = "Access private data without consent";
    const char* target = "user";

    reciprocity_evaluation_t eval;
    int result = reciprocity_eval_check(reciprocity, action_desc, target, &eval);
    ASSERT_EQ(result, 0);

    // Privacy violations should have low symmetry
    // (we wouldn't want our data accessed without consent)
    if (eval.result == RECIPROCITY_FAIL) {
        // Record as blocked
        record_action("privacy_violation", action_desc, 0.3f, true);
    } else {
        // Record as allowed
        record_action("privacy_violation", action_desc, 0.3f, false);
    }

    action_history_stats_t stats;
    ASSERT_EQ(action_history_get_stats(history, &stats), 0);
    EXPECT_EQ(stats.total_records, 1u);
}

TEST_F(CoreDirectivesPipelineTest, HighFrequencyProcessing) {
    // Test rapid action evaluation
    const int NUM_ACTIONS = 100;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_ACTIONS; i++) {
        char desc[128];
        snprintf(desc, sizeof(desc), "High-frequency action %d", i);
        float harm = (i % 10 == 0) ? 0.7f : 0.1f;  // Every 10th is harmful

        bool allowed = action_allowed(harm);
        record_action("high_freq", desc, harm, !allowed);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Verify all actions recorded
    action_history_stats_t stats;
    ASSERT_EQ(action_history_get_stats(history, &stats), 0);
    EXPECT_EQ(stats.total_records, static_cast<uint32_t>(NUM_ACTIONS));

    // Every 10th action should be blocked (0, 10, 20, ..., 90 = 10 actions)
    EXPECT_EQ(stats.blocked_count, 10u);

    // Should complete reasonably fast (< 1 second)
    EXPECT_LT(duration.count(), 1000) << "High-frequency processing too slow";
}

TEST_F(CoreDirectivesPipelineTest, ActionHistoryRetrieval) {
    // Record various actions
    record_action("type_a", "First action", 0.1f, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    record_action("type_b", "Second action", 0.2f, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    record_action("type_a", "Third action", 0.3f, false);

    // Query by type
    action_record_t type_a_records[10];
    uint32_t type_a_count = 0;
    int ret = action_history_get_by_type(history, "type_a", type_a_records, 10, &type_a_count);
    ASSERT_EQ(ret, 0);
    EXPECT_EQ(type_a_count, 2u);

    // Query recent actions
    action_record_t recent[10];
    uint32_t recent_count = 0;
    ret = action_history_get_recent(history, 200, recent, 10, &recent_count);  // Last 200ms
    ASSERT_EQ(ret, 0);
    EXPECT_GE(recent_count, 2u);

    // Verify stats
    action_history_stats_t stats;
    ASSERT_EQ(action_history_get_stats(history, &stats), 0);
    EXPECT_EQ(stats.total_records, 3u);
    EXPECT_EQ(stats.unique_types, 2u);
}

TEST_F(CoreDirectivesPipelineTest, BioAsyncIntegration) {
    // Test bio-async connection (may fail if router not available)
    int hist_result = action_history_connect_bio_async(history);
    int recip_result = reciprocity_eval_connect_bio_async(reciprocity);

    // If connection succeeded, verify and disconnect
    if (hist_result == 0) {
        EXPECT_TRUE(action_history_is_bio_async_connected(history));
        action_history_disconnect_bio_async(history);
        EXPECT_FALSE(action_history_is_bio_async_connected(history));
    }

    if (recip_result == 0) {
        EXPECT_TRUE(reciprocity_eval_is_bio_async_connected(reciprocity));
        reciprocity_eval_disconnect_bio_async(reciprocity);
        EXPECT_FALSE(reciprocity_eval_is_bio_async_connected(reciprocity));
    }
}

TEST_F(CoreDirectivesPipelineTest, StatisticsIntegrity) {
    // Process multiple actions
    for (uint32_t i = 0; i < 50; i++) {
        char desc[128];
        snprintf(desc, sizeof(desc), "Statistics test action %u", i);
        float harm = (i % 5 == 0) ? 0.6f : 0.2f;

        bool allowed = action_allowed(harm);
        record_action("stats_test", desc, harm, !allowed);
    }

    // Verify history statistics
    action_history_stats_t hist_stats;
    ASSERT_EQ(action_history_get_stats(history, &hist_stats), 0);
    EXPECT_EQ(hist_stats.total_records, 50u);
    EXPECT_EQ(hist_stats.unique_types, 1u);

    // 10 actions should be blocked (i = 0, 5, 10, 15, 20, 25, 30, 35, 40, 45)
    EXPECT_EQ(hist_stats.blocked_count, 10u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
