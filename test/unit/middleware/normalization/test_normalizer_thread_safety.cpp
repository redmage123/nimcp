/**
 * @file test_normalizer_thread_safety.cpp
 * @brief Thread safety tests for normalizer modules
 *
 * WHAT: Verify normalizer create/destroy lifecycle with mutex integration,
 *       and concurrent update correctness
 * WHY:  Bug H13 added mutexes to all 3 normalizers; verify no crash or leak
 * HOW:  GTest with multi-threaded stress tests
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cmath>

#include "middleware/normalization/nimcp_homeostatic_normalizer.h"
#include "middleware/normalization/nimcp_min_max_normalizer.h"
#include "middleware/normalization/nimcp_zscore_normalizer.h"

// ============================================================================
// Homeostatic Normalizer Thread Safety
// ============================================================================

class HomeostaticNormalizerThreadTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HomeostaticNormalizerThreadTest, CreateDestroy_Lifecycle) {
    // Verify create/destroy works with new mutex field
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(4, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);
    EXPECT_EQ(homeostatic_normalizer_num_channels(norm), 4u);

    // Verify basic operations still work
    EXPECT_TRUE(homeostatic_normalizer_update(norm, 0, 0.5f, 1.0f));
    float scaling = homeostatic_normalizer_get_scaling(norm, 0);
    EXPECT_GT(scaling, 0.0f);

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerThreadTest, NullDestroy_Safe) {
    homeostatic_normalizer_destroy(nullptr);
    // Must not crash
}

TEST_F(HomeostaticNormalizerThreadTest, ConcurrentUpdate_NoCorruption) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(4, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    const int num_threads = 4;
    const int iterations = 500;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([norm, t, iterations]() {
            for (int i = 0; i < iterations; i++) {
                size_t channel = (size_t)(t % 4);
                float activity = 0.5f + 0.1f * (float)(i % 10);
                homeostatic_normalizer_update(norm, channel, activity, 0.1f);
            }
        });
    }

    for (auto& th : threads) th.join();

    // Verify scaling factors are in valid range after concurrent updates
    for (size_t ch = 0; ch < 4; ch++) {
        float scaling = homeostatic_normalizer_get_scaling(norm, ch);
        EXPECT_GE(scaling, 0.1f);
        EXPECT_LE(scaling, 10.0f);
    }

    homeostatic_normalizer_destroy(norm);
}

TEST_F(HomeostaticNormalizerThreadTest, ConcurrentResetAndUpdate_NoCrash) {
    homeostatic_normalizer_t* norm = homeostatic_normalizer_create(2, 1.0f, 10.0f);
    ASSERT_NE(norm, nullptr);

    std::vector<std::thread> threads;

    // Thread 1: Updates channel 0 repeatedly
    threads.emplace_back([norm]() {
        for (int i = 0; i < 200; i++) {
            homeostatic_normalizer_update(norm, 0, 0.8f, 0.1f);
        }
    });

    // Thread 2: Resets all channels repeatedly
    threads.emplace_back([norm]() {
        for (int i = 0; i < 50; i++) {
            homeostatic_normalizer_reset_all(norm);
        }
    });

    for (auto& th : threads) th.join();
    homeostatic_normalizer_destroy(norm);
}

// ============================================================================
// Min-Max Normalizer Thread Safety
// ============================================================================

class MinMaxNormalizerThreadTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MinMaxNormalizerThreadTest, CreateDestroy_Lifecycle) {
    min_max_normalizer_t* norm = minmax_normalizer_create(4, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);
    EXPECT_EQ(minmax_normalizer_num_channels(norm), 4u);

    EXPECT_TRUE(minmax_normalizer_fit(norm, 0, 5.0f));
    EXPECT_TRUE(minmax_normalizer_fit(norm, 0, 10.0f));

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerThreadTest, NullDestroy_Safe) {
    minmax_normalizer_destroy(nullptr);
}

TEST_F(MinMaxNormalizerThreadTest, ConcurrentFit_NoCorruption) {
    min_max_normalizer_t* norm = minmax_normalizer_create(4, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    const int num_threads = 4;
    const int iterations = 500;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([norm, t, iterations]() {
            for (int i = 0; i < iterations; i++) {
                size_t channel = (size_t)(t % 4);
                float value = (float)(t * 100 + i);
                minmax_normalizer_fit(norm, channel, value);
            }
        });
    }

    for (auto& th : threads) th.join();

    // Verify stats are obtainable and sane
    for (size_t ch = 0; ch < 4; ch++) {
        minmax_stats_t stats;
        bool ok = minmax_normalizer_get_stats(norm, ch, &stats);
        EXPECT_TRUE(ok);
        EXPECT_GT(stats.sample_count, 0u);
    }

    minmax_normalizer_destroy(norm);
}

TEST_F(MinMaxNormalizerThreadTest, ConcurrentResetAndFit_NoCrash) {
    min_max_normalizer_t* norm = minmax_normalizer_create(2, 0.0f, 1.0f, false);
    ASSERT_NE(norm, nullptr);

    std::vector<std::thread> threads;

    threads.emplace_back([norm]() {
        for (int i = 0; i < 200; i++) {
            minmax_normalizer_fit(norm, 0, (float)i);
        }
    });

    threads.emplace_back([norm]() {
        for (int i = 0; i < 50; i++) {
            minmax_normalizer_reset_all(norm);
        }
    });

    for (auto& th : threads) th.join();
    minmax_normalizer_destroy(norm);
}

// ============================================================================
// Z-Score Normalizer Thread Safety
// ============================================================================

class ZscoreNormalizerThreadTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ZscoreNormalizerThreadTest, CreateDestroy_Lifecycle) {
    zscore_normalizer_t* norm = zscore_normalizer_create(4, 0, 3.0f);
    ASSERT_NE(norm, nullptr);
    EXPECT_EQ(zscore_normalizer_num_channels(norm), 4u);

    EXPECT_TRUE(zscore_normalizer_fit(norm, 0, 1.0f));
    EXPECT_TRUE(zscore_normalizer_fit(norm, 0, 2.0f));
    EXPECT_TRUE(zscore_normalizer_fit(norm, 0, 3.0f));

    float mean = zscore_normalizer_mean(norm, 0);
    EXPECT_NEAR(mean, 2.0f, 0.01f);

    zscore_normalizer_destroy(norm);
}

TEST_F(ZscoreNormalizerThreadTest, CreateDestroy_WithWindow) {
    zscore_normalizer_t* norm = zscore_normalizer_create(2, 100, 3.0f);
    ASSERT_NE(norm, nullptr);

    for (int i = 0; i < 50; i++) {
        zscore_normalizer_fit(norm, 0, (float)i);
    }

    zscore_normalizer_destroy(norm);
}

TEST_F(ZscoreNormalizerThreadTest, NullDestroy_Safe) {
    zscore_normalizer_destroy(nullptr);
}

TEST_F(ZscoreNormalizerThreadTest, ConcurrentFit_NoCorruption) {
    zscore_normalizer_t* norm = zscore_normalizer_create(4, 0, 3.0f);
    ASSERT_NE(norm, nullptr);

    const int num_threads = 4;
    const int iterations = 500;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([norm, t, iterations]() {
            for (int i = 0; i < iterations; i++) {
                size_t channel = (size_t)(t % 4);
                float value = (float)(t * 100 + i);
                zscore_normalizer_fit(norm, channel, value);
            }
        });
    }

    for (auto& th : threads) th.join();

    // Verify stats are obtainable
    for (size_t ch = 0; ch < 4; ch++) {
        zscore_stats_t stats;
        bool ok = zscore_normalizer_get_stats(norm, ch, &stats);
        EXPECT_TRUE(ok);
        EXPECT_GT(stats.sample_count, 0u);
        EXPECT_GT(stats.stddev, 0.0f);
    }

    zscore_normalizer_destroy(norm);
}

TEST_F(ZscoreNormalizerThreadTest, ConcurrentResetAndFit_NoCrash) {
    zscore_normalizer_t* norm = zscore_normalizer_create(2, 0, 3.0f);
    ASSERT_NE(norm, nullptr);

    std::vector<std::thread> threads;

    threads.emplace_back([norm]() {
        for (int i = 0; i < 200; i++) {
            zscore_normalizer_fit(norm, 0, (float)i);
        }
    });

    threads.emplace_back([norm]() {
        for (int i = 0; i < 50; i++) {
            zscore_normalizer_reset_all(norm);
        }
    });

    for (auto& th : threads) th.join();
    zscore_normalizer_destroy(norm);
}
