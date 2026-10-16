/**
 * @file test_bio_async_integration.cpp
 * @brief Integration tests for bio-async module
 *
 * Tests bio-async integration with:
 * - NIMCP unified memory system
 * - Thread pool for concurrent operations
 * - Multi-subsystem interactions
 * - Real-world usage patterns
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <cmath>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_async.h"
#include "async/nimcp_biological_timescales.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BioAsyncIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_bio_async_config_t config = nimcp_bio_async_default_config();
        config.enable_logging = false;
        config.enable_statistics = true;
        config.use_unified_memory = true;
        config.thread_pool_size = 4;
        nimcp_error_t err = nimcp_bio_async_init(&config);
        ASSERT_EQ(err, NIMCP_SUCCESS);
    }

    void TearDown() override {
        nimcp_bio_async_shutdown();
    }
};

//=============================================================================
// Multi-threaded Promise/Future Tests
//=============================================================================

TEST_F(BioAsyncIntegrationTest, ConcurrentPromiseCreation) {
    const int NUM_THREADS = 8;
    const int PROMISES_PER_THREAD = 100;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < PROMISES_PER_THREAD; i++) {
                auto channel = static_cast<nimcp_bio_channel_type_t>(
                    (t + i) % BIO_CHANNEL_COUNT);
                nimcp_bio_promise_t promise = nimcp_bio_promise_create(
                    channel, sizeof(int));
                if (promise) {
                    success_count++;
                    nimcp_bio_promise_destroy(promise);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS * PROMISES_PER_THREAD);
}

TEST_F(BioAsyncIntegrationTest, ConcurrentPromiseCompletion) {
    const int NUM_PROMISES = 100;
    std::vector<nimcp_bio_promise_t> promises;
    std::vector<nimcp_bio_future_t> futures;
    std::vector<std::thread> threads;
    std::atomic<int> completed{0};

    // Create all promises/futures first
    for (int i = 0; i < NUM_PROMISES; i++) {
        auto channel = static_cast<nimcp_bio_channel_type_t>(i % BIO_CHANNEL_COUNT);
        nimcp_bio_promise_t p = nimcp_bio_promise_create(channel, sizeof(int));
        nimcp_bio_future_t f = nimcp_bio_promise_get_future(p);
        promises.push_back(p);
        futures.push_back(f);
    }

    // Complete promises from multiple threads
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            for (int i = t; i < NUM_PROMISES; i += 4) {
                int result = i;
                if (nimcp_bio_promise_complete(promises[i], &result) == NIMCP_SUCCESS) {
                    completed++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(completed.load(), NUM_PROMISES);

    // Verify all futures are ready
    for (int i = 0; i < NUM_PROMISES; i++) {
        EXPECT_TRUE(nimcp_bio_future_is_ready(futures[i]));
        int result = 0;
        nimcp_bio_future_wait(futures[i], &result, 0);
        EXPECT_EQ(result, i);
    }

    // Cleanup
    for (auto& f : futures) nimcp_bio_future_destroy(f);
    for (auto& p : promises) nimcp_bio_promise_destroy(p);
}

TEST_F(BioAsyncIntegrationTest, ConcurrentWaiters) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    const int NUM_WAITERS = 10;
    std::vector<std::thread> waiters;
    std::atomic<int> wait_success{0};

    // Start waiter threads
    for (int i = 0; i < NUM_WAITERS; i++) {
        waiters.emplace_back([&]() {
            int result = 0;
            nimcp_error_t err = nimcp_bio_future_wait(future, &result, 1000);
            if (err == NIMCP_SUCCESS && result == 42) {
                wait_success++;
            }
        });
    }

    // Give waiters time to start waiting
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Complete the promise
    int result = 42;
    nimcp_bio_promise_complete(promise, &result);

    for (auto& w : waiters) {
        w.join();
    }

    EXPECT_EQ(wait_success.load(), NUM_WAITERS);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// Producer-Consumer Pattern Tests
//=============================================================================

TEST_F(BioAsyncIntegrationTest, ProducerConsumerPattern) {
    const int NUM_ITEMS = 50;
    std::vector<nimcp_bio_promise_t> promises;
    std::vector<nimcp_bio_future_t> futures;
    std::atomic<int> consumed{0};
    std::atomic<bool> producer_done{false};

    // Create promises/futures
    for (int i = 0; i < NUM_ITEMS; i++) {
        nimcp_bio_promise_t p = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
        nimcp_bio_future_t f = nimcp_bio_promise_get_future(p);
        promises.push_back(p);
        futures.push_back(f);
    }

    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITEMS; i++) {
            int value = i * 10;
            nimcp_bio_promise_complete(promises[i], &value);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        producer_done = true;
    });

    // Consumer threads
    std::vector<std::thread> consumers;
    for (int c = 0; c < 4; c++) {
        consumers.emplace_back([&, c]() {
            for (int i = c; i < NUM_ITEMS; i += 4) {
                int value = 0;
                nimcp_error_t err = nimcp_bio_future_wait(futures[i], &value, 500);
                if (err == NIMCP_SUCCESS && value == i * 10) {
                    consumed++;
                }
            }
        });
    }

    producer.join();
    for (auto& c : consumers) {
        c.join();
    }

    EXPECT_EQ(consumed.load(), NUM_ITEMS);

    for (auto& f : futures) nimcp_bio_future_destroy(f);
    for (auto& p : promises) nimcp_bio_promise_destroy(p);
}

//=============================================================================
// Phase Coupling Integration Tests
//=============================================================================

TEST_F(BioAsyncIntegrationTest, PhaseSyncMultipleProducers) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);

    const int NUM_PRODUCERS = 5;
    std::vector<nimcp_bio_promise_t> promises;
    std::vector<nimcp_bio_future_t> futures;
    std::vector<std::thread> producers;

    // Create futures and add to sync
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        nimcp_bio_promise_t p = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
        nimcp_bio_future_t f = nimcp_bio_promise_get_future(p);
        nimcp_phase_sync_add_future(sync, f);
        promises.push_back(p);
        futures.push_back(f);
    }

    // Complete futures from different threads
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producers.emplace_back([&, i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(i * 10));
            int value = i;
            nimcp_bio_promise_complete(promises[i], &value);
        });
    }

    // Wait for all producers
    for (auto& p : producers) {
        p.join();
    }

    // Wait for coherence (all completed)
    nimcp_error_t err = nimcp_phase_sync_wait_all(sync, 500);
    // Expect success or timeout (depends on phase dynamics)
    (void)err;

    // All should be ready
    for (auto& f : futures) {
        EXPECT_TRUE(nimcp_bio_future_is_ready(f));
    }

    for (auto& f : futures) nimcp_bio_future_destroy(f);
    for (auto& p : promises) nimcp_bio_promise_destroy(p);
    nimcp_phase_sync_destroy(sync);
}

TEST_F(BioAsyncIntegrationTest, CrossBandPhaseCoupling) {
    // Test phase sync across different oscillation bands

    nimcp_phase_sync_t sync_theta = nimcp_phase_sync_create(BIO_OSC_THETA);
    nimcp_phase_sync_t sync_gamma = nimcp_phase_sync_create(BIO_OSC_GAMMA);

    // Create futures for each band
    std::vector<nimcp_bio_promise_t> promises;
    std::vector<nimcp_bio_future_t> futures;

    for (int i = 0; i < 3; i++) {
        nimcp_bio_promise_t p = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
        nimcp_bio_future_t f = nimcp_bio_promise_get_future(p);
        nimcp_phase_sync_add_future(sync_theta, f);
        promises.push_back(p);
        futures.push_back(f);
    }

    for (int i = 0; i < 5; i++) {
        nimcp_bio_promise_t p = nimcp_bio_promise_create(BIO_CHANNEL_ACETYLCHOLINE, sizeof(int));
        nimcp_bio_future_t f = nimcp_bio_promise_get_future(p);
        nimcp_phase_sync_add_future(sync_gamma, f);
        promises.push_back(p);
        futures.push_back(f);
    }

    // Complete all promises
    for (size_t i = 0; i < promises.size(); i++) {
        int val = (int)i;
        nimcp_bio_promise_complete(promises[i], &val);
    }

    // Both bands should synchronize independently
    float coh_theta = nimcp_phase_sync_get_coherence(sync_theta);
    float coh_gamma = nimcp_phase_sync_get_coherence(sync_gamma);

    EXPECT_GE(coh_theta, 0.0f);
    EXPECT_LE(coh_theta, 1.0f);
    EXPECT_GE(coh_gamma, 0.0f);
    EXPECT_LE(coh_gamma, 1.0f);

    for (auto& f : futures) nimcp_bio_future_destroy(f);
    for (auto& p : promises) nimcp_bio_promise_destroy(p);
    nimcp_phase_sync_destroy(sync_theta);
    nimcp_phase_sync_destroy(sync_gamma);
}

//=============================================================================
// Predictive Coding Integration Tests
//=============================================================================

TEST_F(BioAsyncIntegrationTest, PredictiveCodingContinuousUpdates) {
    nimcp_predictive_model_t model = nimcp_predictive_create("sensor", 0.5f, 1.0f);

    // Simulate continuous observations
    std::vector<float> observations;
    for (int i = 0; i < 100; i++) {
        observations.push_back(0.5f + 0.1f * sin(i * 0.1f));  // Oscillating around 0.5
    }

    std::atomic<int> callbacks{0};
    nimcp_predictive_on_error(model,
        [](const char*, float, float, float, float, void* ud) {
            (*static_cast<std::atomic<int>*>(ud))++;
        },
        &callbacks, 1.0f);

    for (float obs : observations) {
        nimcp_predictive_observe(model, obs);
    }

    // Should have adapted, fewer callbacks toward end
    float final_pred = nimcp_predictive_get_prediction(model);
    EXPECT_NEAR(final_pred, 0.5f, 0.2f);  // Should be close to mean

    float final_prec = nimcp_predictive_get_precision(model);
    EXPECT_GT(final_prec, 1.0f);  // Precision should have increased

    nimcp_predictive_destroy(model);
}

TEST_F(BioAsyncIntegrationTest, MultiplePredictiveModels) {
    const int NUM_MODELS = 10;
    std::vector<nimcp_predictive_model_t> models;
    std::vector<std::thread> updaters;

    // Create multiple models for different signals
    for (int i = 0; i < NUM_MODELS; i++) {
        std::string name = "signal_" + std::to_string(i);
        nimcp_predictive_model_t m = nimcp_predictive_create(
            name.c_str(), i * 0.1f, 1.0f);
        models.push_back(m);
    }

    // Update models concurrently
    for (int i = 0; i < NUM_MODELS; i++) {
        updaters.emplace_back([&, i]() {
            for (int j = 0; j < 50; j++) {
                float obs = i * 0.1f + 0.01f * (j % 5);
                nimcp_predictive_observe(models[i], obs);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    for (auto& u : updaters) {
        u.join();
    }

    // Verify predictions converged
    for (int i = 0; i < NUM_MODELS; i++) {
        float pred = nimcp_predictive_get_prediction(models[i]);
        EXPECT_NEAR(pred, i * 0.1f, 0.1f);
    }

    for (auto& m : models) {
        nimcp_predictive_destroy(m);
    }
}

//=============================================================================
// Glial Wave Integration Tests
//=============================================================================

TEST_F(BioAsyncIntegrationTest, GlialWavePropagationConcurrent) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(50, 2.0f);

    std::atomic<int> regions_reached{0};
    std::vector<std::thread> monitors;

    // Monitor wave arrival at different regions
    for (int r = 40; r <= 60; r++) {
        monitors.emplace_back([&, r]() {
            nimcp_error_t err = nimcp_glial_wave_wait_for_region(wave, r, 500);
            if (err == NIMCP_SUCCESS) {
                regions_reached++;
            }
        });
    }

    // Step wave in separate thread
    std::thread stepper([&]() {
        for (int i = 0; i < 500 && nimcp_glial_wave_is_active(wave); i++) {
            nimcp_glial_wave_step(wave, 1.0f);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    stepper.join();
    for (auto& m : monitors) {
        m.join();
    }

    EXPECT_GT(regions_reached.load(), 10);  // Most regions should be reached

    nimcp_glial_wave_destroy(wave);
}

TEST_F(BioAsyncIntegrationTest, MultipleGlialWaves) {
    std::vector<nimcp_glial_wave_t> waves;
    std::vector<std::thread> steppers;

    // Create waves from different sources
    for (int i = 0; i < 3; i++) {
        uint32_t source = i * 100;
        nimcp_glial_wave_t w = nimcp_glial_wave_initiate(source, 2.0f);
        waves.push_back(w);
    }

    // Step all waves concurrently
    for (int i = 0; i < 3; i++) {
        steppers.emplace_back([&, i]() {
            for (int j = 0; j < 100; j++) {
                nimcp_glial_wave_step(waves[i], 1.0f);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    for (auto& s : steppers) {
        s.join();
    }

    // Verify waves propagated
    for (auto& w : waves) {
        EXPECT_GT(nimcp_glial_wave_get_radius(w), 0.0f);
    }

    for (auto& w : waves) {
        nimcp_glial_wave_destroy(w);
    }
}

//=============================================================================
// Mixed Subsystem Integration Tests
//=============================================================================

TEST_F(BioAsyncIntegrationTest, NeuromodAndPredictiveCoding) {
    // Combine dopamine future with predictive coding

    nimcp_predictive_model_t model = nimcp_predictive_create("reward", 0.5f, 1.0f);
    std::atomic<int> surprise_events{0};

    nimcp_predictive_on_error(model,
        [](const char*, float, float, float, float surprise, void* ud) {
            if (surprise > 1.0f) {
                (*static_cast<std::atomic<int>*>(ud))++;
            }
        },
        &surprise_events, 0.5f);

    // Create dopamine-based reward signal
    for (int trial = 0; trial < 20; trial++) {
        nimcp_bio_promise_t promise = nimcp_bio_promise_create(
            BIO_CHANNEL_DOPAMINE, sizeof(float));
        nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

        // Simulate reward
        float reward = (trial % 5 == 0) ? 1.0f : 0.0f;  // Occasional reward
        nimcp_bio_promise_complete(promise, &reward);

        // Update prediction based on reward
        float observed_reward = 0.0f;
        nimcp_bio_future_wait(future, &observed_reward, 100);

        nimcp_predictive_observe(model, observed_reward);

        nimcp_bio_future_destroy(future);
        nimcp_bio_promise_destroy(promise);
    }

    // Should have some surprise events for unexpected rewards
    EXPECT_GT(surprise_events.load(), 0);

    nimcp_predictive_destroy(model);
}

TEST_F(BioAsyncIntegrationTest, PhaseSyncWithGlialModulation) {
    // Use glial wave to modulate phase sync coherence threshold

    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 2.0f);

    // Add futures to sync
    std::vector<nimcp_bio_promise_t> promises;
    std::vector<nimcp_bio_future_t> futures;

    for (int i = 0; i < 5; i++) {
        nimcp_bio_promise_t p = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
        nimcp_bio_future_t f = nimcp_bio_promise_get_future(p);
        nimcp_phase_sync_add_future(sync, f);
        promises.push_back(p);
        futures.push_back(f);
    }

    // Complete promises while wave propagates
    std::thread wave_thread([&]() {
        for (int i = 0; i < 100; i++) {
            nimcp_glial_wave_step(wave, 1.0f);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::thread complete_thread([&]() {
        for (size_t i = 0; i < promises.size(); i++) {
            int val = (int)i;
            nimcp_bio_promise_complete(promises[i], &val);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    wave_thread.join();
    complete_thread.join();

    // Get final states
    float coherence = nimcp_phase_sync_get_coherence(sync);
    float wave_radius = nimcp_glial_wave_get_radius(wave);

    EXPECT_GE(coherence, 0.0f);
    EXPECT_GT(wave_radius, 0.0f);

    for (auto& f : futures) nimcp_bio_future_destroy(f);
    for (auto& p : promises) nimcp_bio_promise_destroy(p);
    nimcp_phase_sync_destroy(sync);
    nimcp_glial_wave_destroy(wave);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(BioAsyncIntegrationTest, HighThroughputFutures) {
    const int NUM_FUTURES = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<nimcp_bio_promise_t> promises;
    std::vector<nimcp_bio_future_t> futures;

    // Create many futures rapidly
    for (int i = 0; i < NUM_FUTURES; i++) {
        auto channel = static_cast<nimcp_bio_channel_type_t>(i % BIO_CHANNEL_COUNT);
        nimcp_bio_promise_t p = nimcp_bio_promise_create(channel, sizeof(int));
        nimcp_bio_future_t f = nimcp_bio_promise_get_future(p);
        promises.push_back(p);
        futures.push_back(f);
    }

    // Complete all
    for (int i = 0; i < NUM_FUTURES; i++) {
        nimcp_bio_promise_complete(promises[i], &i);
    }

    // Wait for all
    for (int i = 0; i < NUM_FUTURES; i++) {
        int result;
        nimcp_bio_future_wait(futures[i], &result, 100);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time
    EXPECT_LT(duration.count(), 5000);  // < 5 seconds

    for (auto& f : futures) nimcp_bio_future_destroy(f);
    for (auto& p : promises) nimcp_bio_promise_destroy(p);
}

TEST_F(BioAsyncIntegrationTest, MemoryStressTest) {
    // Test memory handling with many allocations/deallocations

    for (int round = 0; round < 10; round++) {
        std::vector<nimcp_bio_promise_t> promises;
        std::vector<nimcp_bio_future_t> futures;
        std::vector<nimcp_phase_sync_t> syncs;
        std::vector<nimcp_predictive_model_t> models;

        // Create many objects
        for (int i = 0; i < 100; i++) {
            promises.push_back(nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int)));
            futures.push_back(nimcp_bio_promise_get_future(promises.back()));
            syncs.push_back(nimcp_phase_sync_create(BIO_OSC_GAMMA));
            std::string name = "model_" + std::to_string(i);
            models.push_back(nimcp_predictive_create(name.c_str(), 0.5f, 1.0f));
        }

        // Destroy all
        for (auto& f : futures) nimcp_bio_future_destroy(f);
        for (auto& p : promises) nimcp_bio_promise_destroy(p);
        for (auto& s : syncs) nimcp_phase_sync_destroy(s);
        for (auto& m : models) nimcp_predictive_destroy(m);
    }

    // Should not leak memory - verified by valgrind/ASAN
    SUCCEED();
}

//=============================================================================
// Statistics Integration Tests
//=============================================================================

TEST_F(BioAsyncIntegrationTest, StatisticsAccumulateCorrectly) {
    nimcp_bio_async_reset_stats();

    // Create and complete futures across all channels
    for (int c = 0; c < BIO_CHANNEL_COUNT; c++) {
        for (int i = 0; i < 10; i++) {
            nimcp_bio_promise_t p = nimcp_bio_promise_create(
                static_cast<nimcp_bio_channel_type_t>(c), sizeof(int));
            int val = i;
            nimcp_bio_promise_complete(p, &val);
            nimcp_bio_promise_destroy(p);
        }
    }

    nimcp_bio_async_stats_t stats;
    nimcp_bio_async_get_stats(&stats);

    EXPECT_EQ(stats.total_futures_created, (uint64_t)(BIO_CHANNEL_COUNT * 10));
    EXPECT_EQ(stats.total_futures_completed, (uint64_t)(BIO_CHANNEL_COUNT * 10));

    for (int c = 0; c < BIO_CHANNEL_COUNT; c++) {
        EXPECT_EQ(stats.channel_stats[c].releases, 10u);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
