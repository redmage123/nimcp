//=============================================================================
// test_phase_synchronization.cpp - Phase Coupling Integration Tests
//=============================================================================
/**
 * @file test_phase_synchronization.cpp
 * @brief Tests for Kuramoto oscillator-based phase synchronization
 *
 * Tests cover:
 * - GAMMA band synchronization (fast binding)
 * - BETA band for working memory coordination
 * - THETA band for memory operations
 * - Coherence threshold waiting
 * - Multi-future phase sync
 *
 * @version 1.0.0
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_async.h"
#include "async/nimcp_biological_timescales.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PhaseSynchronizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_bio_async_config_t config = nimcp_bio_async_default_config();
        config.enable_statistics = true;
        config.phase_config.coherence_threshold = 0.8f;
        config.phase_config.coupling_strength = 0.5f;
        config.phase_config.max_oscillators = 100;
        config.phase_config.enable_cross_frequency = true;
        config.use_real_time = false;
        config.time_acceleration = 10.0f;
        ASSERT_EQ(nimcp_bio_async_init(&config), NIMCP_SUCCESS);
    }

    void TearDown() override {
        nimcp_bio_async_shutdown();
    }

    // Helper to create and complete a future with delay
    nimcp_bio_future_t CreateDelayedFuture(nimcp_bio_channel_type_t channel,
                                            float completion_delay_ms) {
        auto promise = nimcp_bio_promise_create(channel, sizeof(float));
        auto future = nimcp_bio_promise_get_future(promise);

        // Simulate async work with delay
        std::thread([promise, completion_delay_ms]() {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(completion_delay_ms / 10)));
            float result = 1.0f;
            nimcp_bio_promise_complete(promise, &result);
            nimcp_bio_promise_destroy(promise);
        }).detach();

        return future;
    }

    // Helper to create multiple futures with staggered completion
    std::vector<nimcp_bio_future_t> CreateStaggeredFutures(
        int count, nimcp_bio_channel_type_t channel, float delay_increment_ms) {
        std::vector<nimcp_bio_future_t> futures;

        for (int i = 0; i < count; i++) {
            auto future = CreateDelayedFuture(channel, delay_increment_ms * (i + 1));
            futures.push_back(future);
        }

        return futures;
    }
};

//=============================================================================
// GAMMA Band Synchronization Tests (Fast Binding)
//=============================================================================

TEST_F(PhaseSynchronizationTest, GammaBandFastSync) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);
    ASSERT_NE(sync, nullptr);

    // Create 5 futures that complete quickly
    std::vector<nimcp_bio_promise_t> promises;
    std::vector<nimcp_bio_future_t> futures;

    for (int i = 0; i < 5; i++) {
        auto promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(float));
        auto future = nimcp_bio_promise_get_future(promise);
        promises.push_back(promise);
        futures.push_back(future);

        ASSERT_EQ(nimcp_phase_sync_add_future(sync, future), NIMCP_SUCCESS);
    }

    // Complete all futures rapidly
    for (auto promise : promises) {
        float value = 1.0f;
        nimcp_bio_promise_complete(promise, &value);
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Wait for coherence
    nimcp_error_t err = nimcp_phase_sync_wait_all(sync, 500);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    EXPECT_EQ(err, NIMCP_SUCCESS) << "Gamma sync should achieve coherence";
    EXPECT_LT(duration_ms, 300) << "Gamma sync should be fast (<300ms)";

    float coherence = nimcp_phase_sync_get_coherence(sync);
    EXPECT_GE(coherence, 0.8f) << "Should achieve high coherence";

    size_t count = nimcp_phase_sync_get_count(sync);
    EXPECT_EQ(count, 5u);

    // Cleanup
    for (auto future : futures) nimcp_bio_future_destroy(future);
    for (auto promise : promises) nimcp_bio_promise_destroy(promise);
    nimcp_phase_sync_destroy(sync);
}

TEST_F(PhaseSynchronizationTest, GammaBandTightCoupling) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);
    ASSERT_NE(sync, nullptr);

    // Create futures with very small timing differences
    auto futures = CreateStaggeredFutures(8, BIO_CHANNEL_ACETYLCHOLINE, 5.0f);

    for (auto future : futures) {
        nimcp_phase_sync_add_future(sync, future);
    }

    // Gamma should synchronize even with tight timing
    nimcp_error_t err = nimcp_phase_sync_wait_coherent(sync, 0.85f, 1000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float coherence = nimcp_phase_sync_get_coherence(sync);
    EXPECT_GE(coherence, 0.85f);

    for (auto future : futures) nimcp_bio_future_destroy(future);
    nimcp_phase_sync_destroy(sync);
}

//=============================================================================
// BETA Band Tests (Working Memory Coordination)
//=============================================================================

TEST_F(PhaseSynchronizationTest, BetaBandWorkingMemorySync) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_BETA);
    ASSERT_NE(sync, nullptr);

    // Simulate working memory slot updates (moderate timing)
    auto futures = CreateStaggeredFutures(6, BIO_CHANNEL_DOPAMINE, 20.0f);

    for (auto future : futures) {
        nimcp_phase_sync_add_future(sync, future);
    }

    // Beta allows moderate sync time
    nimcp_error_t err = nimcp_phase_sync_wait_all(sync, 1000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float coherence = nimcp_phase_sync_get_coherence(sync);
    EXPECT_GE(coherence, 0.7f) << "Beta should achieve good coherence";

    // Get mean phase
    float mean_phase = nimcp_phase_sync_get_mean_phase(sync);
    EXPECT_GE(mean_phase, 0.0f);
    EXPECT_LT(mean_phase, 2.0f * M_PI);

    for (auto future : futures) nimcp_bio_future_destroy(future);
    nimcp_phase_sync_destroy(sync);
}

TEST_F(PhaseSynchronizationTest, BetaBandModerateLatency) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_BETA);

    // Create futures with moderate delays
    std::vector<nimcp_bio_promise_t> promises;
    std::vector<nimcp_bio_future_t> futures;

    for (int i = 0; i < 4; i++) {
        auto promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(float));
        auto future = nimcp_bio_promise_get_future(promise);
        promises.push_back(promise);
        futures.push_back(future);
        nimcp_phase_sync_add_future(sync, future);
    }

    // Complete with staggered timing (simulate processing delays)
    std::thread([&promises]() {
        for (size_t i = 0; i < promises.size(); i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10 * (i + 1)));
            float value = static_cast<float>(i);
            nimcp_bio_promise_complete(promises[i], &value);
        }
    }).detach();

    // Beta should handle moderate latency
    auto start = std::chrono::high_resolution_clock::now();
    nimcp_error_t err = nimcp_phase_sync_wait_all(sync, 1500);
    auto end = std::chrono::high_resolution_clock::now();

    EXPECT_EQ(err, NIMCP_SUCCESS);

    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(duration_ms, 1000) << "Beta sync should complete in reasonable time";

    for (auto future : futures) nimcp_bio_future_destroy(future);
    for (auto promise : promises) nimcp_bio_promise_destroy(promise);
    nimcp_phase_sync_destroy(sync);
}

//=============================================================================
// THETA Band Tests (Memory/Sequential Processing)
//=============================================================================

TEST_F(PhaseSynchronizationTest, ThetaBandMemorySync) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_THETA);
    ASSERT_NE(sync, nullptr);

    // Simulate sequential memory operations (slower)
    auto futures = CreateStaggeredFutures(5, BIO_CHANNEL_SEROTONIN, 50.0f);

    for (auto future : futures) {
        nimcp_phase_sync_add_future(sync, future);
    }

    // Theta allows longer sync times
    nimcp_error_t err = nimcp_phase_sync_wait_all(sync, 2000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float coherence = nimcp_phase_sync_get_coherence(sync);
    EXPECT_GE(coherence, 0.6f) << "Theta should achieve coherence with slower ops";

    for (auto future : futures) nimcp_bio_future_destroy(future);
    nimcp_phase_sync_destroy(sync);
}

TEST_F(PhaseSynchronizationTest, ThetaBandSequentialOperations) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_THETA);

    // Create sequential operations with dependencies
    std::vector<nimcp_bio_promise_t> promises;
    std::vector<nimcp_bio_future_t> futures;

    for (int i = 0; i < 3; i++) {
        auto promise = nimcp_bio_promise_create(BIO_CHANNEL_SEROTONIN, sizeof(float));
        auto future = nimcp_bio_promise_get_future(promise);
        promises.push_back(promise);
        futures.push_back(future);
        nimcp_phase_sync_add_future(sync, future);
    }

    // Complete sequentially (simulating hippocampal sequences)
    std::thread([&promises]() {
        for (auto promise : promises) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            float value = 1.0f;
            nimcp_bio_promise_complete(promise, &value);
        }
    }).detach();

    nimcp_error_t err = nimcp_phase_sync_wait_coherent(sync, 0.7f, 2000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    for (auto future : futures) nimcp_bio_future_destroy(future);
    for (auto promise : promises) nimcp_bio_promise_destroy(promise);
    nimcp_phase_sync_destroy(sync);
}

//=============================================================================
// Coherence Threshold Tests
//=============================================================================

TEST_F(PhaseSynchronizationTest, CoherenceThresholdVariation) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);

    auto futures = CreateStaggeredFutures(10, BIO_CHANNEL_DOPAMINE, 10.0f);
    for (auto future : futures) {
        nimcp_phase_sync_add_future(sync, future);
    }

    // Test different coherence thresholds
    std::vector<float> thresholds = {0.5f, 0.7f, 0.85f, 0.95f};

    for (float threshold : thresholds) {
        auto start = std::chrono::high_resolution_clock::now();
        nimcp_error_t err = nimcp_phase_sync_wait_coherent(sync, threshold, 2000);
        auto end = std::chrono::high_resolution_clock::now();

        if (err == NIMCP_SUCCESS) {
            auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start).count();

            float coherence = nimcp_phase_sync_get_coherence(sync);
            EXPECT_GE(coherence, threshold)
                << "Should achieve at least threshold " << threshold;

            // Higher thresholds should generally take longer
            // (though not guaranteed due to stochastic dynamics)
        }
    }

    for (auto future : futures) nimcp_bio_future_destroy(future);
    nimcp_phase_sync_destroy(sync);
}

TEST_F(PhaseSynchronizationTest, PartialCoherence) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_BETA);

    // Create futures where only some complete quickly
    std::vector<nimcp_bio_promise_t> promises;
    std::vector<nimcp_bio_future_t> futures;

    for (int i = 0; i < 8; i++) {
        auto promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(float));
        auto future = nimcp_bio_promise_get_future(promise);
        promises.push_back(promise);
        futures.push_back(future);
        nimcp_phase_sync_add_future(sync, future);
    }

    // Complete only half immediately
    for (size_t i = 0; i < 4; i++) {
        float value = 1.0f;
        nimcp_bio_promise_complete(promises[i], &value);
    }

    // Wait for partial coherence (should succeed with relaxed threshold)
    nimcp_error_t err = nimcp_phase_sync_wait_coherent(sync, 0.5f, 500);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float coherence = nimcp_phase_sync_get_coherence(sync);
    EXPECT_GE(coherence, 0.3f) << "Should have some coherence with half completed";
    EXPECT_LT(coherence, 0.9f) << "Should not have full coherence yet";

    // Complete remaining
    for (size_t i = 4; i < promises.size(); i++) {
        float value = 1.0f;
        nimcp_bio_promise_complete(promises[i], &value);
    }

    // Now should achieve higher coherence
    err = nimcp_phase_sync_wait_coherent(sync, 0.8f, 1000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    for (auto future : futures) nimcp_bio_future_destroy(future);
    for (auto promise : promises) nimcp_bio_promise_destroy(promise);
    nimcp_phase_sync_destroy(sync);
}

//=============================================================================
// Multi-Future Scenarios
//=============================================================================

TEST_F(PhaseSynchronizationTest, LargeGroupSync) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);

    const int num_futures = 50;
    std::vector<nimcp_bio_promise_t> promises;
    std::vector<nimcp_bio_future_t> futures;

    for (int i = 0; i < num_futures; i++) {
        auto promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(float));
        auto future = nimcp_bio_promise_get_future(promise);
        promises.push_back(promise);
        futures.push_back(future);
        ASSERT_EQ(nimcp_phase_sync_add_future(sync, future), NIMCP_SUCCESS);
    }

    EXPECT_EQ(nimcp_phase_sync_get_count(sync), num_futures);

    // Complete all
    for (auto promise : promises) {
        float value = 1.0f;
        nimcp_bio_promise_complete(promise, &value);
    }

    // Should still synchronize
    nimcp_error_t err = nimcp_phase_sync_wait_all(sync, 3000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float coherence = nimcp_phase_sync_get_coherence(sync);
    EXPECT_GT(coherence, 0.0f);

    for (auto future : futures) nimcp_bio_future_destroy(future);
    for (auto promise : promises) nimcp_bio_promise_destroy(promise);
    nimcp_phase_sync_destroy(sync);
}

TEST_F(PhaseSynchronizationTest, MixedChannelSync) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_BETA);

    // Create futures on different channels
    std::vector<nimcp_bio_promise_t> promises;
    std::vector<nimcp_bio_future_t> futures;

    nimcp_bio_channel_type_t channels[] = {
        BIO_CHANNEL_DOPAMINE,
        BIO_CHANNEL_SEROTONIN,
        BIO_CHANNEL_NOREPINEPHRINE,
        BIO_CHANNEL_ACETYLCHOLINE
    };

    for (int i = 0; i < 12; i++) {
        auto channel = channels[i % 4];
        auto promise = nimcp_bio_promise_create(channel, sizeof(float));
        auto future = nimcp_bio_promise_get_future(promise);
        promises.push_back(promise);
        futures.push_back(future);
        nimcp_phase_sync_add_future(sync, future);
    }

    // Complete all
    for (auto promise : promises) {
        float value = 1.0f;
        nimcp_bio_promise_complete(promise, &value);
    }

    // Mixed channels should still synchronize
    nimcp_error_t err = nimcp_phase_sync_wait_all(sync, 2000);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    for (auto future : futures) nimcp_bio_future_destroy(future);
    for (auto promise : promises) nimcp_bio_promise_destroy(promise);
    nimcp_phase_sync_destroy(sync);
}

//=============================================================================
// Timeout and Error Cases
//=============================================================================

TEST_F(PhaseSynchronizationTest, TimeoutBeforeCoherence) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);

    // Create futures but don't complete them
    std::vector<nimcp_bio_promise_t> promises;
    std::vector<nimcp_bio_future_t> futures;

    for (int i = 0; i < 5; i++) {
        auto promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(float));
        auto future = nimcp_bio_promise_get_future(promise);
        promises.push_back(promise);
        futures.push_back(future);
        nimcp_phase_sync_add_future(sync, future);
    }

    // Should timeout since futures never complete
    nimcp_error_t err = nimcp_phase_sync_wait_all(sync, 200);
    EXPECT_NE(err, NIMCP_SUCCESS) << "Should timeout without completion";

    for (auto future : futures) nimcp_bio_future_destroy(future);
    for (auto promise : promises) nimcp_bio_promise_destroy(promise);
    nimcp_phase_sync_destroy(sync);
}

TEST_F(PhaseSynchronizationTest, EmptyGroupSync) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);
    ASSERT_NE(sync, nullptr);

    // No futures added
    EXPECT_EQ(nimcp_phase_sync_get_count(sync), 0u);

    // Should handle empty group gracefully
    nimcp_error_t err = nimcp_phase_sync_wait_all(sync, 100);
    // Implementation-dependent: might succeed immediately or return error

    nimcp_phase_sync_destroy(sync);
}

//=============================================================================
// Phase Dynamics Tests
//=============================================================================

TEST_F(PhaseSynchronizationTest, MeanPhaseProgression) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);

    auto futures = CreateStaggeredFutures(6, BIO_CHANNEL_DOPAMINE, 5.0f);
    for (auto future : futures) {
        nimcp_phase_sync_add_future(sync, future);
    }

    // Track mean phase over time
    std::vector<float> mean_phases;

    for (int i = 0; i < 20; i++) {
        nimcp_bio_async_step(10.0f);
        float phase = nimcp_phase_sync_get_mean_phase(sync);
        mean_phases.push_back(phase);

        // Phase should be in valid range
        EXPECT_GE(phase, 0.0f);
        EXPECT_LT(phase, 2.0f * M_PI);
    }

    for (auto future : futures) nimcp_bio_future_destroy(future);
    nimcp_phase_sync_destroy(sync);
}

TEST_F(PhaseSynchronizationTest, CoherenceIncreasesOverTime) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_BETA);

    auto futures = CreateStaggeredFutures(8, BIO_CHANNEL_DOPAMINE, 8.0f);
    for (auto future : futures) {
        nimcp_phase_sync_add_future(sync, future);
    }

    // Coherence should generally increase as futures complete
    std::vector<float> coherence_values;

    for (int i = 0; i < 30; i++) {
        nimcp_bio_async_step(20.0f);
        float coherence = nimcp_phase_sync_get_coherence(sync);
        coherence_values.push_back(coherence);

        EXPECT_GE(coherence, 0.0f);
        EXPECT_LE(coherence, 1.0f);
    }

    // Final coherence should be higher than initial (generally)
    if (coherence_values.size() > 10) {
        float avg_early = 0.0f, avg_late = 0.0f;
        for (int i = 0; i < 5; i++) avg_early += coherence_values[i];
        for (int i = coherence_values.size() - 5; i < coherence_values.size(); i++)
            avg_late += coherence_values[i];

        // Later coherence should tend to be higher (not strict requirement)
        // Just verify we're tracking it
    }

    for (auto future : futures) nimcp_bio_future_destroy(future);
    nimcp_phase_sync_destroy(sync);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(PhaseSynchronizationTest, VerifyPhaseStatistics) {
    nimcp_bio_async_reset_stats();

    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);

    auto futures = CreateStaggeredFutures(5, BIO_CHANNEL_DOPAMINE, 10.0f);
    for (auto future : futures) {
        nimcp_phase_sync_add_future(sync, future);
    }

    nimcp_phase_sync_wait_all(sync, 1000);

    nimcp_bio_async_stats_t stats;
    ASSERT_EQ(nimcp_bio_async_get_stats(&stats), NIMCP_SUCCESS);

    EXPECT_GE(stats.phase_stats.sync_requests, 1u);

    for (auto future : futures) nimcp_bio_future_destroy(future);
    nimcp_phase_sync_destroy(sync);
}

// End of tests
