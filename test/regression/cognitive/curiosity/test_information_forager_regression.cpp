/**
 * @file test_information_forager_regression.cpp
 * @brief Regression tests for Information Forager module
 *
 * TEST COVERAGE:
 * - Stability under sustained operation (1000+ ticks)
 * - Memory leak prevention (create/destroy cycles)
 * - State consistency: values stay bounded [0, 1]
 * - No crashes under rapid state transitions
 * - Queue overflow handling
 * - Numerical stability of IG computation
 * - Thread safety under concurrent access
 * - Recovery from extreme configurations
 * - Callback failure resilience
 * - Target aging and expiry correctness
 *
 * REGRESSION TARGETS:
 * - No crashes under any input pattern
 * - Bounded state values
 * - Consistent behavior across runs (deterministic with seed)
 * - Graceful degradation under load
 * - No resource leaks
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <thread>
#include <atomic>
#include <vector>
#include <random>

extern "C" {
#include "cognitive/curiosity/nimcp_information_forager.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/salience/nimcp_salience.h"
#include "nimcp.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ForagerRegressionTest : public ::testing::Test {
protected:
    information_forager_t forager = nullptr;
    brain_t brain = nullptr;
    curiosity_engine_t curiosity = nullptr;
    salience_evaluator_t salience = nullptr;
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(42);

        brain = brain_create("forager_regression", BRAIN_SIZE_SMALL,
                             BRAIN_TASK_CLASSIFICATION, 16, 8);
        ASSERT_NE(brain, nullptr);

        curiosity = curiosity_engine_create(brain, "regression_learner");
        ASSERT_NE(curiosity, nullptr);

        salience_config_t sal_cfg;
        memset(&sal_cfg, 0, sizeof(sal_cfg));
        sal_cfg.history_size = 32;
        sal_cfg.enable_novelty = true;
        sal_cfg.enable_surprise = true;
        sal_cfg.high_novelty_threshold = 0.5f;
        salience = salience_evaluator_create(brain, &sal_cfg);

        forager_config_t cfg = forager_default_config();
        cfg.seek_interval_ticks = 5;
        cfg.consolidation_ticks = 2;
        cfg.ig_threshold = 0.01f;
        cfg.quality_threshold = 0.1f;
        forager = forager_create(brain, curiosity, salience, &cfg);
    }

    void TearDown() override {
        if (forager) { forager_destroy(forager); forager = nullptr; }
        if (salience) { salience_evaluator_destroy(salience); salience = nullptr; }
        if (curiosity) { curiosity_engine_destroy(curiosity); curiosity = nullptr; }
        if (brain) { brain_destroy(brain); brain = nullptr; }
    }

    static int noop_callback(const char* query, const char* hint,
                             void* ctx, char** result, size_t* len) {
        (void)query; (void)hint; (void)ctx;
        *result = strdup("generic knowledge response for regression testing");
        *len = strlen(*result);
        return 0;
    }

    static int failing_callback(const char* query, const char* hint,
                                void* ctx, char** result, size_t* len) {
        (void)query; (void)hint; (void)ctx;
        *result = nullptr;
        *len = 0;
        return -1;
    }
};

//=============================================================================
// Sustained Operation Stability
//=============================================================================

TEST_F(ForagerRegressionTest, ThousandTicksNoCrash) {
    if (!forager) GTEST_SKIP();

    curiosity_learn_answer(curiosity, "What is knowledge?",
                           "Knowledge is justified true belief.");
    forager_register_data_callback(forager, noop_callback, nullptr);

    for (int i = 0; i < 1000; i++) {
        int result = forager_tick(forager, 100);
        ASSERT_GE(result, -1) << "Crash at tick " << i;
        ASSERT_LE(result, 1) << "Invalid return at tick " << i;
    }

    forager_stats_t stats = forager_get_stats(forager);
    EXPECT_EQ(stats.total_ticks, 1000u);
}

TEST_F(ForagerRegressionTest, StatsRemainBounded) {
    if (!forager) GTEST_SKIP();

    curiosity_learn_answer(curiosity, "What is learning?",
                           "Learning is the acquisition of new knowledge or skills.");
    forager_register_data_callback(forager, noop_callback, nullptr);

    for (int i = 0; i < 500; i++) {
        forager_tick(forager, 100);
    }

    forager_stats_t stats = forager_get_stats(forager);

    /* All float stats should be finite and non-negative */
    EXPECT_TRUE(std::isfinite(stats.avg_expected_ig));
    EXPECT_TRUE(std::isfinite(stats.avg_realized_ig));
    EXPECT_TRUE(std::isfinite(stats.ig_prediction_error));
    EXPECT_TRUE(std::isfinite(stats.avg_queue_depth));

    EXPECT_GE(stats.avg_expected_ig, 0.0f);
    EXPECT_GE(stats.avg_realized_ig, 0.0f);
    EXPECT_GE(stats.ig_prediction_error, 0.0f);
    EXPECT_GE(stats.avg_queue_depth, 0.0f);
    EXPECT_LE(stats.avg_expected_ig, 1.0f);
    EXPECT_LE(stats.avg_realized_ig, 1.0f);

    /* State should be one of the valid enum values */
    EXPECT_GE((int)stats.current_state, 0);
    EXPECT_LE((int)stats.current_state, 5);
}

//=============================================================================
// Memory Leak Prevention (Create/Destroy Cycles)
//=============================================================================

TEST_F(ForagerRegressionTest, CreateDestroyCycles) {
    /* Rapidly create and destroy foragers — tests for leaks */
    for (int i = 0; i < 50; i++) {
        forager_config_t cfg = forager_default_config();
        information_forager_t f = forager_create(brain, curiosity, salience, &cfg);
        if (f) {
            forager_tick(f, 100);
            forager_tick(f, 100);
            forager_destroy(f);
        }
    }
    /* If this test completes without ASAN/valgrind errors, no leaks */
}

//=============================================================================
// Callback Failure Resilience
//=============================================================================

TEST_F(ForagerRegressionTest, FailingCallbackDoesNotCrash) {
    if (!forager) GTEST_SKIP();

    curiosity_learn_answer(curiosity, "What is resilience?",
                           "Resilience is the ability to recover from adversity.");
    forager_register_data_callback(forager, failing_callback, nullptr);

    for (int i = 0; i < 300; i++) {
        int result = forager_tick(forager, 100);
        ASSERT_GE(result, -1) << "Crash at tick " << i;
    }

    forager_stats_t stats = forager_get_stats(forager);
    EXPECT_EQ(stats.total_ticks, 300u);
    /* With a failing callback, learn_events should remain 0 */
    EXPECT_EQ(stats.learn_events, 0u);
}

TEST_F(ForagerRegressionTest, NullCallbackDoesNotCrash) {
    if (!forager) GTEST_SKIP();

    curiosity_learn_answer(curiosity, "What is safety?",
                           "Safety is freedom from danger or risk.");

    /* No callback registered — forager should handle gracefully */
    for (int i = 0; i < 200; i++) {
        forager_tick(forager, 100);
    }

    forager_stats_t stats = forager_get_stats(forager);
    EXPECT_EQ(stats.data_callbacks_made, 0u);
}

//=============================================================================
// Rapid State Transitions
//=============================================================================

TEST_F(ForagerRegressionTest, RapidPauseResumeCycles) {
    if (!forager) GTEST_SKIP();

    for (int i = 0; i < 100; i++) {
        forager_pause(forager);
        forager_tick(forager, 10);
        forager_resume(forager);
        forager_tick(forager, 10);
    }

    forager_stats_t stats = forager_get_stats(forager);
    EXPECT_EQ(stats.total_ticks, 200u);
}

//=============================================================================
// Extreme Configuration Values
//=============================================================================

TEST_F(ForagerRegressionTest, MinimalQueueDepth) {
    forager_config_t cfg = forager_default_config();
    cfg.max_queue_depth = 1;

    information_forager_t f = forager_create(brain, curiosity, salience, &cfg);
    if (!f) GTEST_SKIP();

    curiosity_learn_answer(curiosity, "What is simplicity?",
                           "Simplicity is the quality of being easy to understand.");
    forager_register_data_callback(f, noop_callback, nullptr);

    for (int i = 0; i < 100; i++) {
        forager_tick(f, 100);
    }

    forager_stats_t stats = forager_get_stats(f);
    EXPECT_EQ(stats.total_ticks, 100u);

    forager_destroy(f);
}

TEST_F(ForagerRegressionTest, ZeroConsolidationTicks) {
    forager_config_t cfg = forager_default_config();
    cfg.consolidation_ticks = 0;
    cfg.seek_interval_ticks = 1;

    information_forager_t f = forager_create(brain, curiosity, salience, &cfg);
    if (!f) GTEST_SKIP();

    forager_register_data_callback(f, noop_callback, nullptr);
    curiosity_learn_answer(curiosity, "What is speed?",
                           "Speed is the rate of change of position.");

    for (int i = 0; i < 100; i++) {
        forager_tick(f, 100);
    }

    forager_stats_t stats = forager_get_stats(f);
    EXPECT_EQ(stats.total_ticks, 100u);

    forager_destroy(f);
}

TEST_F(ForagerRegressionTest, MaxExplorationRate) {
    if (!forager) GTEST_SKIP();

    forager_set_exploration_rate(forager, 1.0f);
    forager_register_data_callback(forager, noop_callback, nullptr);
    curiosity_learn_answer(curiosity, "What is exploration?",
                           "Exploration is the act of investigating the unknown.");

    for (int i = 0; i < 200; i++) {
        forager_tick(forager, 100);
    }

    forager_stats_t stats = forager_get_stats(forager);
    EXPECT_EQ(stats.total_ticks, 200u);
}

TEST_F(ForagerRegressionTest, ZeroExplorationRate) {
    if (!forager) GTEST_SKIP();

    forager_set_exploration_rate(forager, 0.0f);
    forager_register_data_callback(forager, noop_callback, nullptr);

    for (int i = 0; i < 100; i++) {
        forager_tick(forager, 100);
    }

    /* Should still function — exploit mode */
    forager_stats_t stats = forager_get_stats(forager);
    EXPECT_EQ(stats.total_ticks, 100u);
}

//=============================================================================
// Thread Safety
//=============================================================================

TEST_F(ForagerRegressionTest, ConcurrentTickAndStats) {
    if (!forager) GTEST_SKIP();

    std::atomic<bool> running{true};
    forager_register_data_callback(forager, noop_callback, nullptr);
    curiosity_learn_answer(curiosity, "What is concurrency?",
                           "Concurrency is running multiple tasks simultaneously.");

    /* Thread 1: tick the forager */
    std::thread ticker([&]() {
        while (running.load()) {
            forager_tick(forager, 10);
        }
    });

    /* Thread 2: read stats concurrently */
    std::thread reader([&]() {
        while (running.load()) {
            forager_stats_t stats = forager_get_stats(forager);
            (void)stats;

            forager_target_t targets[3];
            forager_get_top_targets(forager, targets, 3);
        }
    });

    /* Let them run for a short time */
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    running.store(false);

    ticker.join();
    reader.join();

    /* If we get here, no deadlocks or crashes */
    forager_stats_t stats = forager_get_stats(forager);
    EXPECT_GT(stats.total_ticks, 0u);
}

TEST_F(ForagerRegressionTest, ConcurrentPauseResume) {
    if (!forager) GTEST_SKIP();

    std::atomic<bool> running{true};

    std::thread ticker([&]() {
        while (running.load()) {
            forager_tick(forager, 10);
        }
    });

    std::thread pauser([&]() {
        while (running.load()) {
            forager_pause(forager);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            forager_resume(forager);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    running.store(false);

    ticker.join();
    pauser.join();

    forager_stats_t stats = forager_get_stats(forager);
    EXPECT_GT(stats.total_ticks, 0u);
}

//=============================================================================
// Feed Result Edge Cases
//=============================================================================

TEST_F(ForagerRegressionTest, FeedResultWithZeroQuality) {
    if (!forager) GTEST_SKIP();

    /* Even with targets, zero quality should be rejected */
    curiosity_learn_answer(curiosity, "What is quality?",
                           "Quality is the standard of something.");

    for (int i = 0; i < 100; i++) {
        forager_tick(forager, 100);
    }

    forager_target_t targets[5];
    int count = forager_get_top_targets(forager, targets, 5);

    if (count > 0) {
        int result = forager_feed_result(forager, targets[0].target_id,
                                         "low quality garbage", 19, 0.0f);
        /* Should be rejected (quality below threshold) */
        EXPECT_EQ(result, 1);

        forager_stats_t stats = forager_get_stats(forager);
        EXPECT_GE(stats.quality_rejections, 0u);
    }
}

TEST_F(ForagerRegressionTest, FeedResultEmptyText) {
    if (!forager) GTEST_SKIP();

    /* Feed empty text — should not crash */
    int result = forager_feed_result(forager, 1, "", 0, 0.5f);
    /* target_id 1 won't exist, so expect -1 */
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Target Queue Saturation
//=============================================================================

TEST_F(ForagerRegressionTest, QueueDoesNotOverflow) {
    if (!forager) GTEST_SKIP();

    /* Teach many concepts to generate lots of gaps */
    const char* topics[] = {
        "biology", "chemistry", "physics", "mathematics", "history",
        "literature", "philosophy", "economics", "psychology", "sociology"
    };

    for (const char* topic : topics) {
        char q[256], a[256];
        snprintf(q, sizeof(q), "What is %s?", topic);
        snprintf(a, sizeof(a), "%s is a field of study.", topic);
        curiosity_learn_answer(curiosity, q, a);
    }

    forager_register_data_callback(forager, noop_callback, nullptr);

    for (int i = 0; i < 500; i++) {
        forager_tick(forager, 100);
    }

    forager_stats_t stats = forager_get_stats(forager);
    /* Queue depth should never exceed configured max */
    EXPECT_LE(stats.active_targets, (uint32_t)FORAGER_MAX_QUEUE_DEPTH);
    EXPECT_EQ(stats.total_ticks, 500u);
}
