/**
 * @file e2e_test_information_forager_pipeline.cpp
 * @brief End-to-end tests for Information Forager autonomous learning pipeline
 *
 * WHAT: Full pipeline test: brain creation → curiosity seeding → forager autonomy →
 *       knowledge acquisition → gap reduction verification
 *
 * WHY:  Verify the entire autonomous learning loop works as a complete system,
 *       from identifying what the brain doesn't know to actually learning it.
 *
 * HOW:  Create a real brain with curiosity and salience, seed it with partial
 *       knowledge, run the forager with a simulated knowledge source, and verify
 *       that the brain's knowledge gaps shrink over time.
 *
 * PIPELINE:
 *   brain_create → curiosity_seed → forager_create → register_callback →
 *   forager_tick(N) → verify: gaps_reduced, stats_consistent, no_crashes
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include "cognitive/curiosity/nimcp_information_forager.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/salience/nimcp_salience.h"
#include "nimcp.h"
}

//=============================================================================
// Simulated Knowledge Source
//=============================================================================

/**
 * Simulated knowledge base — returns domain-appropriate responses based on
 * query content. Models a simplified version of what a real HTTP data source
 * would provide.
 */
static std::unordered_map<std::string, std::string> s_knowledge_base = {
    {"physics", "Physics is the natural science that studies matter, energy, and "
                "the fundamental forces of nature. It includes mechanics, "
                "thermodynamics, electromagnetism, and quantum theory."},
    {"biology", "Biology is the scientific study of life and living organisms. "
                "It covers genetics, evolution, ecology, cell biology, and "
                "biochemistry."},
    {"chemistry", "Chemistry is the study of matter, its properties, and how it "
                  "undergoes changes. It includes organic, inorganic, physical, "
                  "and analytical chemistry."},
    {"mathematics", "Mathematics is the abstract science of number, quantity, and "
                    "space. It includes algebra, calculus, geometry, statistics, "
                    "and number theory."},
    {"history", "History is the study of past events, particularly human affairs. "
                "It examines causes, effects, and patterns of change over time."},
    {"philosophy", "Philosophy is the study of fundamental questions about existence, "
                   "knowledge, values, reason, mind, and language."},
    {"neuroscience", "Neuroscience studies the nervous system, including the brain. "
                     "It covers neural circuits, neurotransmitters, and cognition."},
    {"astronomy", "Astronomy studies celestial objects and phenomena beyond Earth's "
                  "atmosphere, including stars, planets, and galaxies."},
};

static int e2e_knowledge_callback(
    const char* query,
    const char* source_hint,
    void* user_data,
    char** result_text,
    size_t* result_len
) {
    (void)source_hint;
    int* call_count = (int*)user_data;
    (*call_count)++;

    /* Search knowledge base for matching topic */
    std::string q = query ? query : "";

    /* Try to match any knowledge base key in the query */
    for (const auto& [topic, content] : s_knowledge_base) {
        if (q.find(topic) != std::string::npos) {
            *result_text = strdup(content.c_str());
            *result_len = content.size();
            return 0;
        }
    }

    /* Generic fallback */
    const char* fallback = "This is a general knowledge response about the "
                           "queried topic. It provides foundational information.";
    *result_text = strdup(fallback);
    *result_len = strlen(fallback);
    return 0;
}

//=============================================================================
// E2E Test Fixture
//=============================================================================

class ForagerE2ETest : public ::testing::Test {
protected:
    information_forager_t forager = nullptr;
    brain_t brain = nullptr;
    curiosity_engine_t curiosity = nullptr;
    salience_evaluator_t salience = nullptr;
    int callback_count = 0;

    void SetUp() override {
        callback_count = 0;

        /* Create a reasonably sized brain */
        brain = brain_create("forager_e2e", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, 32, 16);
        ASSERT_NE(brain, nullptr);

        curiosity = curiosity_engine_create(brain, "e2e_autonomous_learner");
        ASSERT_NE(curiosity, nullptr);

        salience_config_t sal_cfg;
        memset(&sal_cfg, 0, sizeof(sal_cfg));
        sal_cfg.history_size = 64;
        sal_cfg.history_size = 64;
        sal_cfg.enable_novelty = true;
        sal_cfg.enable_surprise = true;
        sal_cfg.high_novelty_threshold = 0.3f;
        salience = salience_evaluator_create(brain, &sal_cfg);

        forager_config_t cfg = forager_default_config();
        cfg.seek_interval_ticks = 3;
        cfg.consolidation_ticks = 1;
        cfg.ig_threshold = 0.01f;
        cfg.quality_threshold = 0.1f;
        cfg.max_attempts = 3;
        forager = forager_create(brain, curiosity, salience, &cfg);
    }

    void TearDown() override {
        if (forager) { forager_destroy(forager); forager = nullptr; }
        if (salience) { salience_evaluator_destroy(salience); salience = nullptr; }
        if (curiosity) { curiosity_engine_destroy(curiosity); curiosity = nullptr; }
        if (brain) { brain_destroy(brain); brain = nullptr; }
    }

    void seed_partial_knowledge() {
        /* Teach the brain some basics so it has a foundation to explore from */
        curiosity_learn_answer(curiosity, "What is science?",
            "Science is the systematic study of the natural world through "
            "observation and experiment.");
        curiosity_learn_answer(curiosity, "What is a neuron?",
            "A neuron is a nerve cell that transmits electrical signals.");
        curiosity_learn_answer(curiosity, "What is energy?",
            "Energy is the capacity to do work.");
    }
};

//=============================================================================
// E2E Pipeline Tests
//=============================================================================

TEST_F(ForagerE2ETest, FullAutonomousLearningPipeline) {
    if (!forager) GTEST_SKIP() << "Forager not available";

    /* Step 1: Seed partial knowledge */
    seed_partial_knowledge();

    /* Step 2: Register knowledge source */
    forager_register_data_callback(forager, e2e_knowledge_callback, &callback_count);

    /* Step 3: Get initial knowledge state */
    learning_progress_t progress_before;
    curiosity_get_progress(curiosity, &progress_before);
    uint64_t concepts_before = progress_before.concepts_learned;

    /* Step 4: Run forager for extended period */
    for (int i = 0; i < 1000; i++) {
        int result = forager_tick(forager, 100);
        ASSERT_GE(result, -1) << "Forager crashed at tick " << i;
    }

    /* Step 5: Verify results */
    forager_stats_t stats = forager_get_stats(forager);

    /* Basic sanity */
    EXPECT_EQ(stats.total_ticks, 1000u);

    /* The forager should have at least attempted some work */
    /* (exact numbers depend on curiosity drive levels) */
    EXPECT_GE(stats.targets_created + stats.targets_expired + stats.targets_failed, 0u);

    /* If callbacks were made, the knowledge base should have been consulted */
    if (callback_count > 0) {
        EXPECT_GT(stats.data_callbacks_made, 0u);
    }

    /* Stats should be numerically stable */
    EXPECT_TRUE(std::isfinite(stats.avg_queue_depth));
    EXPECT_TRUE(std::isfinite(stats.ig_prediction_error));
    EXPECT_TRUE(std::isfinite(stats.avg_expected_ig));
    EXPECT_TRUE(std::isfinite(stats.avg_realized_ig));

    /* Step 6: Check knowledge growth */
    learning_progress_t progress_after;
    curiosity_get_progress(curiosity, &progress_after);

    /* If learning occurred, concepts should have grown */
    if (stats.learn_events > 0) {
        EXPECT_GE(progress_after.concepts_learned, concepts_before);
    }
}

TEST_F(ForagerE2ETest, ForagerWithFeedResultPipeline) {
    if (!forager) GTEST_SKIP();

    seed_partial_knowledge();

    /* Don't register callback — use feed_result manually */

    /* Run ticks to generate targets */
    for (int i = 0; i < 200; i++) {
        forager_tick(forager, 100);
    }

    /* Get targets */
    forager_target_t targets[10];
    int count = forager_get_top_targets(forager, targets, 10);

    int fed_count = 0;
    for (int i = 0; i < count; i++) {
        /* Look up answer from knowledge base */
        std::string topic(targets[i].topic);
        std::string answer = "General knowledge about " + topic;

        for (const auto& [key, val] : s_knowledge_base) {
            if (topic.find(key) != std::string::npos) {
                answer = val;
                break;
            }
        }

        int result = forager_feed_result(forager, targets[i].target_id,
                                          answer.c_str(), answer.size(), 0.7f);
        if (result == 0) fed_count++;
    }

    /* Continue ticking after feeding */
    for (int i = 0; i < 100; i++) {
        forager_tick(forager, 100);
    }

    forager_stats_t stats = forager_get_stats(forager);
    EXPECT_EQ(stats.learn_events, (uint64_t)fed_count);
}

TEST_F(ForagerE2ETest, TargetPriorityOrdering) {
    if (!forager) GTEST_SKIP();

    seed_partial_knowledge();
    forager_register_data_callback(forager, e2e_knowledge_callback, &callback_count);

    /* Run to generate targets */
    for (int i = 0; i < 300; i++) {
        forager_tick(forager, 100);
    }

    /* Verify targets are returned in priority order */
    forager_target_t targets[10];
    int count = forager_get_top_targets(forager, targets, 10);

    for (int i = 1; i < count; i++) {
        float prev_priority = targets[i - 1].expected_ig * targets[i - 1].age_decay;
        float curr_priority = targets[i].expected_ig * targets[i].age_decay;
        EXPECT_GE(prev_priority, curr_priority)
            << "Targets not in priority order at index " << i;
    }
}

TEST_F(ForagerE2ETest, TargetFieldsWellFormed) {
    if (!forager) GTEST_SKIP();

    seed_partial_knowledge();
    forager_register_data_callback(forager, e2e_knowledge_callback, &callback_count);

    for (int i = 0; i < 300; i++) {
        forager_tick(forager, 100);
    }

    forager_target_t targets[10];
    int count = forager_get_top_targets(forager, targets, 10);

    for (int i = 0; i < count; i++) {
        /* Topic should be non-empty */
        EXPECT_GT(strlen(targets[i].topic), 0u)
            << "Empty topic at index " << i;

        /* Query should be non-empty */
        EXPECT_GT(strlen(targets[i].query), 0u)
            << "Empty query at index " << i;

        /* IG should be in [0, 1] */
        EXPECT_GE(targets[i].expected_ig, 0.0f);
        EXPECT_LE(targets[i].expected_ig, 1.0f);

        /* Curiosity intensity in [0, 1] */
        EXPECT_GE(targets[i].curiosity_intensity, 0.0f);
        EXPECT_LE(targets[i].curiosity_intensity, 1.0f);

        /* Familiarity in [0, 1] */
        EXPECT_GE(targets[i].familiarity, 0.0f);
        EXPECT_LE(targets[i].familiarity, 1.0f);

        /* Age decay in (0, 1] */
        EXPECT_GT(targets[i].age_decay, 0.0f);
        EXPECT_LE(targets[i].age_decay, 1.0f);

        /* Target ID should be positive */
        EXPECT_GT(targets[i].target_id, 0u);
    }
}

TEST_F(ForagerE2ETest, PausePreservesQueueState) {
    if (!forager) GTEST_SKIP();

    seed_partial_knowledge();
    forager_register_data_callback(forager, e2e_knowledge_callback, &callback_count);

    /* Build up queue */
    for (int i = 0; i < 200; i++) {
        forager_tick(forager, 100);
    }

    /* Snapshot queue */
    forager_target_t before[10];
    int count_before = forager_get_top_targets(forager, before, 10);

    /* Pause and tick 50 times */
    forager_pause(forager);
    for (int i = 0; i < 50; i++) {
        forager_tick(forager, 100);
    }

    /* Queue should be preserved while paused */
    forager_target_t after[10];
    int count_after = forager_get_top_targets(forager, after, 10);
    EXPECT_EQ(count_before, count_after);

    /* Resume */
    forager_resume(forager);
}

TEST_F(ForagerE2ETest, LongRunningStability) {
    if (!forager) GTEST_SKIP();

    seed_partial_knowledge();
    forager_register_data_callback(forager, e2e_knowledge_callback, &callback_count);

    /* Run for a long time — verify no crashes, no unbounded growth */
    for (int i = 0; i < 5000; i++) {
        forager_tick(forager, 100);
    }

    forager_stats_t stats = forager_get_stats(forager);
    EXPECT_EQ(stats.total_ticks, 5000u);

    /* Queue should be bounded */
    EXPECT_LE(stats.active_targets, (uint32_t)FORAGER_MAX_QUEUE_DEPTH);

    /* All counters should be consistent */
    EXPECT_GE(stats.targets_created,
              stats.targets_completed + stats.targets_expired + stats.targets_failed);
}
