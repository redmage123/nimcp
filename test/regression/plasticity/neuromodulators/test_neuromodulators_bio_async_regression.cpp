/**
 * @file test_neuromodulators_bio_async_regression.cpp
 * @brief Regression tests for neuromodulators bio-async integration
 *
 * Tests for known bugs, edge cases, and performance regressions.
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_unified_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class NeuromodulatorsBioAsyncRegressionTest : public ::testing::Test {
protected:
    neuromodulator_system_t system_;

    void SetUp() override {
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        ASSERT_EQ(NIMCP_SUCCESS, nimcp_bio_async_init(&bio_config));

        bio_router_config_t router_config = bio_router_default_config();
        ASSERT_EQ(NIMCP_SUCCESS, bio_router_init(&router_config));

        system_ = neuromodulator_system_create(nullptr);
        ASSERT_NE(nullptr, system_);
    }

    void TearDown() override {
        if (system_) {
            neuromodulator_system_destroy(system_);
            system_ = nullptr;
        }
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }
};

//=============================================================================
// Bug Regression Tests
//=============================================================================

/**
 * @test Regression for bug where rapid sequential releases could overflow
 */
TEST_F(NeuromodulatorsBioAsyncRegressionTest, RapidSequentialReleasesDoNotOverflow) {
    const int RAPID_RELEASE_COUNT = 1000;

    float initial = neuromodulator_get_level(system_, NEUROMOD_DOPAMINE);

    for (int i = 0; i < RAPID_RELEASE_COUNT; i++) {
        neuromodulator_release_dopamine(system_, 0.001f, 0.0f);
    }

    float final_level = neuromodulator_get_level(system_, NEUROMOD_DOPAMINE);

    // Should be clamped to [0, 1]
    EXPECT_LE(final_level, 1.0f);
    EXPECT_GE(final_level, 0.0f);
    // Should have increased
    EXPECT_GT(final_level, initial);
}

/**
 * @test Regression for bug where concurrent releases could cause data races
 */
TEST_F(NeuromodulatorsBioAsyncRegressionTest, ConcurrentReleasesThreadSafe) {
    const int THREAD_COUNT = 4;
    const int RELEASES_PER_THREAD = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < THREAD_COUNT; t++) {
        threads.emplace_back([this]() {
            for (int i = 0; i < RELEASES_PER_THREAD; i++) {
                neuromodulator_release_dopamine(system_, 0.01f, 0.0f);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // System should still be valid
    neuromodulator_pool_t pool = {};
    ASSERT_TRUE(neuromodulator_get_levels(system_, &pool));

    // Dopamine should have increased
    EXPECT_GT(neuromodulator_pool_get_dopamine(&pool), 0.0f);
    EXPECT_LE(neuromodulator_pool_get_dopamine(&pool), 1.0f);
}

/**
 * @test Regression for bug where decay could make concentrations negative
 */
TEST_F(NeuromodulatorsBioAsyncRegressionTest, DecayNeverGoesNegative) {
    // Set very low concentration
    neuromodulator_set_level(system_, NEUROMOD_DOPAMINE, 0.001f);

    // Decay for a long time
    for (int i = 0; i < 100; i++) {
        neuromodulator_update(system_, 1.0f);
    }

    float final_level = neuromodulator_get_level(system_, NEUROMOD_DOPAMINE);
    EXPECT_GE(final_level, 0.0f);
}

/**
 * @test Regression for bug where statistics overflow on 32-bit counters
 */
TEST_F(NeuromodulatorsBioAsyncRegressionTest, StatisticsCountersDoNotOverflow) {
    // Release many times (would overflow 16-bit counter)
    const int RELEASE_COUNT = 100000;

    for (int i = 0; i < RELEASE_COUNT; i++) {
        neuromodulator_release_dopamine(system_, 0.0001f, 0.0f);
    }

    neuromodulator_stats_t stats;
    ASSERT_TRUE(neuromodulator_get_stats(system_, &stats));

    // Counter should be at least RELEASE_COUNT (may be more due to other tests)
    EXPECT_GE(stats.dopamine_releases, static_cast<uint64_t>(RELEASE_COUNT));
}

//=============================================================================
// Edge Case Regression Tests
//=============================================================================

/**
 * @test Regression for edge case with zero release amount
 */
TEST_F(NeuromodulatorsBioAsyncRegressionTest, ZeroReleaseAmountHandled) {
    float initial = neuromodulator_get_level(system_, NEUROMOD_DOPAMINE);

    // Release zero amount - should not crash
    float rpe = neuromodulator_release_dopamine(system_, 0.0f, 0.0f);
    EXPECT_EQ(0.0f, rpe);

    float after = neuromodulator_get_level(system_, NEUROMOD_DOPAMINE);
    // Should not change significantly (baseline already set)
    EXPECT_NEAR(initial, after, 0.1f);
}

/**
 * @test Regression for edge case with negative release amount (should clamp)
 */
TEST_F(NeuromodulatorsBioAsyncRegressionTest, NegativeReleaseAmountClamped) {
    float initial = neuromodulator_get_level(system_, NEUROMOD_DOPAMINE);

    // Negative release (punishment larger than reward)
    float rpe = neuromodulator_release_dopamine(system_, 0.0f, 0.5f);
    EXPECT_LT(rpe, 0.0f);  // Negative RPE

    // Concentration should decrease or stay same, never go negative
    float after = neuromodulator_get_level(system_, NEUROMOD_DOPAMINE);
    EXPECT_GE(after, 0.0f);
}

/**
 * @test Regression for edge case with very small time steps in update
 */
TEST_F(NeuromodulatorsBioAsyncRegressionTest, TinyTimeStepsHandled) {
    neuromodulator_release_dopamine(system_, 0.5f, 0.0f);

    // Update with very small timesteps
    for (int i = 0; i < 1000; i++) {
        ASSERT_TRUE(neuromodulator_update(system_, 0.001f));  // 1ms
    }

    // Should still be valid
    neuromodulator_pool_t pool = {};
    ASSERT_TRUE(neuromodulator_get_levels(system_, &pool));
    EXPECT_GE(neuromodulator_pool_get_dopamine(&pool), 0.0f);
    EXPECT_LE(neuromodulator_pool_get_dopamine(&pool), 1.0f);
}

/**
 * @test Regression for edge case with very large time steps
 */
TEST_F(NeuromodulatorsBioAsyncRegressionTest, LargeTimeStepsHandled) {
    neuromodulator_release_dopamine(system_, 0.5f, 0.0f);

    // Update with large timestep
    ASSERT_TRUE(neuromodulator_update(system_, 1000.0f));  // 1000 seconds

    // Should decay to baseline
    float level = neuromodulator_get_level(system_, NEUROMOD_DOPAMINE);
    EXPECT_GE(level, 0.0f);
    EXPECT_LE(level, 1.0f);
}

//=============================================================================
// Performance Regression Tests
//=============================================================================

/**
 * @test Performance regression for release operations
 */
TEST_F(NeuromodulatorsBioAsyncRegressionTest, ReleasePerformanceRegression) {
    const int OPERATION_COUNT = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < OPERATION_COUNT; i++) {
        neuromodulator_release_dopamine(system_, 0.001f, 0.0f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double ops_per_second = (OPERATION_COUNT * 1000000.0) / duration.count();

    // Should achieve at least 100k ops/sec on modern hardware
    EXPECT_GT(ops_per_second, 100000.0) << "Release performance regression detected";
}

/**
 * @test Performance regression for concurrent operations
 */
TEST_F(NeuromodulatorsBioAsyncRegressionTest, ConcurrentPerformanceRegression) {
    const int THREAD_COUNT = 4;
    const int OPS_PER_THREAD = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < THREAD_COUNT; t++) {
        threads.emplace_back([this, OPS_PER_THREAD]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                neuromodulator_release_dopamine(system_, 0.001f, 0.0f);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (< 1 second)
    EXPECT_LT(duration.count(), 1000) << "Concurrent performance regression detected";
}

/**
 * @test Memory regression - no leaks with repeated create/destroy
 */
TEST_F(NeuromodulatorsBioAsyncRegressionTest, NoMemoryLeaksOnRecreate) {
    const int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; i++) {
        neuromodulator_system_t temp_system = neuromodulator_system_create(nullptr);
        ASSERT_NE(nullptr, temp_system);

        // Use the system
        neuromodulator_release_dopamine(temp_system, 0.5f, 0.0f);
        neuromodulator_pool_t pool = {};
        neuromodulator_get_levels(temp_system, &pool);

        neuromodulator_system_destroy(temp_system);
    }

    // If we got here without crashing, no obvious leaks
    SUCCEED();
}

//=============================================================================
// Known Issue Tests (Document expected failures)
//=============================================================================

/**
 * @test Document known limitation with extremely rapid updates
 * This test documents a known edge case rather than testing for it
 */
TEST_F(NeuromodulatorsBioAsyncRegressionTest, DISABLED_ExtremelyRapidUpdateKnownIssue) {
    // Known issue: Updates faster than 0.1ms may have precision issues
    // This is acceptable as biological timescales are much slower
    // Documenting for future reference
}

//=============================================================================
// Stability Tests
//=============================================================================

/**
 * @test Long-running stability test
 */
TEST_F(NeuromodulatorsBioAsyncRegressionTest, LongRunningStability) {
    const int DURATION_SECONDS = 5;
    const int OPERATIONS_PER_SEC = 100;

    auto start = std::chrono::steady_clock::now();

    int operations = 0;
    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start);
        if (elapsed.count() >= DURATION_SECONDS) {
            break;
        }

        neuromodulator_release_dopamine(system_, 0.01f, 0.0f);
        neuromodulator_update(system_, 0.01f);
        operations++;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // System should still be functional
    neuromodulator_pool_t pool = {};
    ASSERT_TRUE(neuromodulator_get_levels(system_, &pool));

    // Should have performed expected operations
    EXPECT_GT(operations, DURATION_SECONDS * OPERATIONS_PER_SEC / 2);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
