/**
 * @file test_bio_async_pipeline.cpp
 * @brief End-to-End Tests for Bio-Async Pipelines
 *
 * WHAT: Complete pipeline tests for biologically-inspired async system
 * WHY:  Verify entire bio-async workflows function correctly end-to-end
 * HOW:  Test neuromodulator channels, phase coupling, predictive coding
 *
 * TEST PIPELINES:
 * - NeuromodulatorPipeline: Multi-channel async coordination with decay dynamics
 * - PhaseCouplingPipeline: Kuramoto oscillator-based synchronization
 * - PredictiveCodingPipeline: Bayesian inference with error callbacks
 * - HybridBioPipeline: Combined subsystems for complex workflows
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <thread>
#include <atomic>
#include <cmath>
#include <vector>
#include <cstdio>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_async.h"
#include "async/nimcp_biological_timescales.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BioAsyncE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_error_t err = nimcp_bio_async_init(NULL);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Bio-async initialization failed";
        ASSERT_TRUE(nimcp_bio_async_is_initialized())
            << "Bio-async not initialized after successful init";
    }

    void TearDown() override {
        nimcp_bio_async_shutdown();
    }
};

//=============================================================================
// Pipeline 1: Neuromodulator Channel Coordination
//=============================================================================

TEST_F(BioAsyncE2ETest, NeuromodulatorPipeline) {
    E2E_PIPELINE_START("Neuromodulator Channel Coordination");

    // Stage 1: Create multi-channel promises
    E2E_STAGE_BEGIN("Create multi-channel promises", 100);

    const size_t NUM_CHANNELS = 4;
    nimcp_bio_channel_type_t channels[] = {
        BIO_CHANNEL_DOPAMINE,
        BIO_CHANNEL_SEROTONIN,
        BIO_CHANNEL_NOREPINEPHRINE,
        BIO_CHANNEL_ACETYLCHOLINE
    };

    std::vector<nimcp_bio_promise_t> promises(NUM_CHANNELS);
    std::vector<nimcp_bio_future_t> futures(NUM_CHANNELS);

    for (size_t i = 0; i < NUM_CHANNELS; i++) {
        promises[i] = nimcp_bio_promise_create(channels[i], sizeof(int));
        E2E_ASSERT_NOT_NULL(promises[i], "Promise creation failed for channel");
        futures[i] = nimcp_bio_promise_get_future(promises[i]);
        E2E_ASSERT_NOT_NULL(futures[i], "Future creation failed");
    }

    E2E_STAGE_END();

    // Stage 2: Complete promises with results
    E2E_STAGE_BEGIN("Complete promises with results", 200);

    std::vector<std::thread> completion_threads;
    std::atomic<int> completion_count{0};

    for (size_t i = 0; i < NUM_CHANNELS; i++) {
        completion_threads.emplace_back([i, &promises, &completion_count]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(i * 10));

            int result = static_cast<int>(i * 100);
            nimcp_error_t err = nimcp_bio_promise_complete(promises[i], &result);
            if (err == NIMCP_SUCCESS) {
                completion_count++;
            }
        });
    }

    for (auto& t : completion_threads) {
        t.join();
    }

    E2E_ASSERT(completion_count == NUM_CHANNELS, "Not all promises completed");

    E2E_STAGE_END();

    // Stage 3: Wait for all futures and verify results
    E2E_STAGE_BEGIN("Wait and verify results", 500);

    for (size_t i = 0; i < NUM_CHANNELS; i++) {
        int result_value = 0;
        nimcp_error_t err = nimcp_bio_future_wait(futures[i], &result_value, 1000);
        E2E_ASSERT(err == NIMCP_SUCCESS, "Future wait failed");

        nimcp_bio_future_state_t state = nimcp_bio_future_state(futures[i]);
        E2E_ASSERT(state == BIO_FUTURE_COMPLETED, "Future not in completed state");

        E2E_ASSERT(result_value == static_cast<int>(i * 100), "Result value incorrect");
    }

    E2E_STAGE_END();

    // Stage 4: Verify statistics
    E2E_STAGE_BEGIN("Verify statistics", 100);

    nimcp_bio_async_stats_t stats;
    nimcp_error_t err = nimcp_bio_async_get_stats(&stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to get stats");
    E2E_ASSERT(stats.total_futures_created >= NUM_CHANNELS,
               "Statistics not tracking futures");
    E2E_ASSERT(stats.total_futures_completed >= NUM_CHANNELS,
               "Statistics not tracking completions");

    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Cleanup resources", 500);  // Increased timeout

    for (size_t i = 0; i < NUM_CHANNELS; i++) {
        nimcp_bio_future_destroy(futures[i]);
        nimcp_bio_promise_destroy(promises[i]);
    }

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 2: Phase-Coupled Synchronization
//=============================================================================

TEST_F(BioAsyncE2ETest, PhaseCouplingPipeline) {
    E2E_PIPELINE_START("Phase-Coupled Synchronization");

    // Stage 1: Create phase sync group with gamma oscillations
    E2E_STAGE_BEGIN("Create phase sync group", 50);

    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);
    E2E_ASSERT_NOT_NULL(sync, "Phase sync creation failed");

    E2E_STAGE_END();

    // Stage 2: Add oscillators (futures)
    E2E_STAGE_BEGIN("Add oscillators", 200);

    const size_t NUM_OSCILLATORS = 8;
    std::vector<nimcp_bio_promise_t> promises(NUM_OSCILLATORS);
    std::vector<nimcp_bio_future_t> futures(NUM_OSCILLATORS);

    for (size_t i = 0; i < NUM_OSCILLATORS; i++) {
        promises[i] = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(float));
        E2E_ASSERT_NOT_NULL(promises[i], "Promise creation failed");

        futures[i] = nimcp_bio_promise_get_future(promises[i]);
        E2E_ASSERT_NOT_NULL(futures[i], "Future creation failed");

        nimcp_error_t err = nimcp_phase_sync_add_future(sync, futures[i]);
        E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to add future to sync group");
    }

    float initial_coherence = nimcp_phase_sync_get_coherence(sync);
    E2E_ASSERT(initial_coherence >= 0.0f, "Initial coherence should be non-negative");

    E2E_STAGE_END();

    // Stage 3: Complete futures concurrently
    E2E_STAGE_BEGIN("Complete futures concurrently", 500);

    std::vector<std::thread> completion_threads;

    for (size_t i = 0; i < NUM_OSCILLATORS; i++) {
        completion_threads.emplace_back([i, &promises]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(i * 5 + (i % 3) * 10));

            float result = static_cast<float>(i) * 0.1f;
            nimcp_bio_promise_complete(promises[i], &result);
        });
    }

    for (auto& t : completion_threads) {
        t.join();
    }

    E2E_STAGE_END();

    // Stage 4: Wait for synchronization
    E2E_STAGE_BEGIN("Wait for synchronization", 2000);

    nimcp_error_t err = nimcp_phase_sync_wait_all(sync, 1500);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Phase sync wait failed");

    float final_coherence = nimcp_phase_sync_get_coherence(sync);
    E2E_ASSERT(final_coherence >= 0.5f, "Final coherence should be significant");

    E2E_STAGE_END();

    // Stage 5: Verify all futures completed
    E2E_STAGE_BEGIN("Verify completion", 100);

    for (size_t i = 0; i < NUM_OSCILLATORS; i++) {
        nimcp_bio_future_state_t state = nimcp_bio_future_state(futures[i]);
        E2E_ASSERT(state == BIO_FUTURE_COMPLETED, "Future should be completed");
    }

    E2E_STAGE_END();

    // Stage 6: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 500);  // Increased timeout

    for (size_t i = 0; i < NUM_OSCILLATORS; i++) {
        nimcp_bio_future_destroy(futures[i]);
        nimcp_bio_promise_destroy(promises[i]);
    }
    nimcp_phase_sync_destroy(sync);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 3: Predictive Coding Error Correction
//=============================================================================

namespace {
    std::atomic<int> g_e2e_pred_callbacks{0};
    std::atomic<float> g_e2e_total_surprise{0};

    void e2e_prediction_callback(const char* signal_name,
                                  float prediction, float actual,
                                  float error, float surprise,
                                  void* user_data) {
        (void)signal_name;
        (void)prediction;
        (void)actual;
        (void)error;
        (void)user_data;

        g_e2e_pred_callbacks++;

        float current = g_e2e_total_surprise.load();
        while (!g_e2e_total_surprise.compare_exchange_weak(current, current + surprise));
    }
}

TEST_F(BioAsyncE2ETest, PredictiveCodingPipeline) {
    E2E_PIPELINE_START("Predictive Coding Error Correction");

    g_e2e_pred_callbacks = 0;
    g_e2e_total_surprise = 0.0f;

    // Stage 1: Create predictive models
    E2E_STAGE_BEGIN("Create predictive models", 100);

    nimcp_predictive_model_t velocity_model = nimcp_predictive_create("velocity", 0.0f, 5.0f);
    nimcp_predictive_model_t position_model = nimcp_predictive_create("position", 100.0f, 10.0f);
    nimcp_predictive_model_t error_model = nimcp_predictive_create("error_signal", 0.5f, 2.0f);

    E2E_ASSERT_NOT_NULL(velocity_model, "Velocity model creation failed");
    E2E_ASSERT_NOT_NULL(position_model, "Position model creation failed");
    E2E_ASSERT_NOT_NULL(error_model, "Error model creation failed");

    E2E_STAGE_END();

    // Stage 2: Register error callbacks
    E2E_STAGE_BEGIN("Register error callbacks", 50);

    nimcp_error_t err;
    err = nimcp_predictive_on_error(velocity_model, e2e_prediction_callback, nullptr, 0.0f);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to register velocity callback");

    err = nimcp_predictive_on_error(position_model, e2e_prediction_callback, nullptr, 0.0f);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to register position callback");

    err = nimcp_predictive_on_error(error_model, e2e_prediction_callback, nullptr, 0.0f);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to register error callback");

    E2E_STAGE_END();

    // Stage 3: Feed observations
    E2E_STAGE_BEGIN("Feed observations", 500);

    const int NUM_OBSERVATIONS = 20;

    for (int i = 0; i < NUM_OBSERVATIONS; i++) {
        float velocity = static_cast<float>(i) * 0.5f;
        nimcp_predictive_observe(velocity_model, velocity);

        float position = 100.0f + static_cast<float>(i * i) * 0.25f;
        nimcp_predictive_observe(position_model, position);

        float error_sig = 0.5f + 0.3f * sinf(static_cast<float>(i) * 0.5f);
        nimcp_predictive_observe(error_model, error_sig);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    E2E_STAGE_END();

    // Stage 4: Verify callbacks
    E2E_STAGE_BEGIN("Verify callbacks", 100);

    int callback_count = g_e2e_pred_callbacks.load();
    E2E_ASSERT(callback_count > 0, "No prediction callbacks triggered");
    E2E_ASSERT(callback_count >= NUM_OBSERVATIONS, "Too few callbacks triggered");

    float total_surprise = g_e2e_total_surprise.load();
    E2E_ASSERT(total_surprise > 0.0f, "Total surprise should be positive");

    E2E_STAGE_END();

    // Stage 5: Verify belief updates
    E2E_STAGE_BEGIN("Verify belief updates", 100);

    float final_velocity_pred = nimcp_predictive_get_prediction(velocity_model);
    float final_position_pred = nimcp_predictive_get_prediction(position_model);

    E2E_ASSERT(final_velocity_pred > 0.0f, "Velocity prediction should have adapted");
    E2E_ASSERT(final_position_pred > 100.0f, "Position prediction should have adapted");

    E2E_STAGE_END();

    // Stage 6: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 500);  // Increased timeout

    nimcp_predictive_destroy(velocity_model);
    nimcp_predictive_destroy(position_model);
    nimcp_predictive_destroy(error_model);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 4: Hybrid Bio-Pipeline
//=============================================================================

TEST_F(BioAsyncE2ETest, HybridBioPipeline) {
    E2E_PIPELINE_START("Hybrid Bio-Pipeline Integration");

    // Stage 1: Setup all subsystems
    E2E_STAGE_BEGIN("Setup subsystems", 200);

    nimcp_bio_promise_t compute_promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(float));
    nimcp_bio_future_t compute_future = nimcp_bio_promise_get_future(compute_promise);
    E2E_ASSERT_NOT_NULL(compute_promise, "Compute promise creation failed");

    nimcp_bio_promise_t bg_promise = nimcp_bio_promise_create(BIO_CHANNEL_SEROTONIN, sizeof(int));
    nimcp_bio_future_t bg_future = nimcp_bio_promise_get_future(bg_promise);
    E2E_ASSERT_NOT_NULL(bg_promise, "Background promise creation failed");

    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_BETA);
    E2E_ASSERT_NOT_NULL(sync, "Phase sync creation failed");

    nimcp_phase_sync_add_future(sync, compute_future);
    nimcp_phase_sync_add_future(sync, bg_future);

    nimcp_predictive_model_t timing_model = nimcp_predictive_create("timing", 50.0f, 1.0f);
    E2E_ASSERT_NOT_NULL(timing_model, "Timing model creation failed");

    E2E_STAGE_END();

    // Stage 2: Launch concurrent computations
    E2E_STAGE_BEGIN("Launch computations", 500);

    std::atomic<bool> compute_done{false};
    std::atomic<bool> bg_done{false};

    std::thread compute_thread([&compute_promise, &compute_done]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        float result = 3.14159f;
        nimcp_bio_promise_complete(compute_promise, &result);
        compute_done = true;
    });

    std::thread bg_thread([&bg_promise, &bg_done]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        int result = 42;
        nimcp_bio_promise_complete(bg_promise, &result);
        bg_done = true;
    });

    E2E_STAGE_END();

    // Stage 3: Wait for synchronization
    E2E_STAGE_BEGIN("Synchronize tasks", 1000);

    nimcp_error_t err = nimcp_phase_sync_wait_all(sync, 800);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Phase sync wait failed");

    compute_thread.join();
    bg_thread.join();

    E2E_ASSERT(compute_done.load(), "Compute task should be done");
    E2E_ASSERT(bg_done.load(), "Background task should be done");

    E2E_STAGE_END();

    // Stage 4: Verify results
    E2E_STAGE_BEGIN("Verify results", 100);

    float compute_result = 0.0f;
    err = nimcp_bio_future_wait(compute_future, &compute_result, 100);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Compute future wait failed");
    E2E_ASSERT(fabsf(compute_result - 3.14159f) < 0.0001f, "Compute result incorrect");

    int bg_result = 0;
    err = nimcp_bio_future_wait(bg_future, &bg_result, 100);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Background future wait failed");
    E2E_ASSERT(bg_result == 42, "Background result incorrect");

    float actual_duration = 100.0f;
    nimcp_predictive_observe(timing_model, actual_duration);

    E2E_STAGE_END();

    // Stage 5: Statistics verification
    E2E_STAGE_BEGIN("Verify statistics", 50);

    nimcp_bio_async_stats_t stats;
    err = nimcp_bio_async_get_stats(&stats);
    E2E_ASSERT(err == NIMCP_SUCCESS, "Failed to get stats");
    E2E_ASSERT(stats.total_futures_created >= 2, "Should have created futures");
    E2E_ASSERT(stats.total_futures_completed >= 2, "Should have completed futures");
    E2E_ASSERT(stats.phase_stats.sync_requests >= 1, "Should have sync requests");
    E2E_ASSERT(stats.predictive_stats.predictions_made >= 1, "Should have made predictions");

    E2E_STAGE_END();

    // Stage 6: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 500);  // Increased timeout

    nimcp_predictive_destroy(timing_model);
    nimcp_phase_sync_destroy(sync);
    nimcp_bio_future_destroy(bg_future);
    nimcp_bio_promise_destroy(bg_promise);
    nimcp_bio_future_destroy(compute_future);
    nimcp_bio_promise_destroy(compute_promise);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 5: Stress Test - High Concurrency
//=============================================================================

TEST_F(BioAsyncE2ETest, StressPipeline) {
    E2E_PIPELINE_START("High Concurrency Stress Test");

    // Stage 1: High-volume promise creation
    E2E_STAGE_BEGIN("High-volume promises", 1000);

    const size_t NUM_PROMISES = 100;
    std::vector<nimcp_bio_promise_t> promises(NUM_PROMISES);
    std::vector<nimcp_bio_future_t> futures(NUM_PROMISES);

    for (size_t i = 0; i < NUM_PROMISES; i++) {
        nimcp_bio_channel_type_t channel = static_cast<nimcp_bio_channel_type_t>(i % 4);
        promises[i] = nimcp_bio_promise_create(channel, sizeof(size_t));
        E2E_ASSERT_NOT_NULL(promises[i], "Promise creation failed");
        futures[i] = nimcp_bio_promise_get_future(promises[i]);
        E2E_ASSERT_NOT_NULL(futures[i], "Future creation failed");
    }

    E2E_STAGE_END();

    // Stage 2: Concurrent completions
    E2E_STAGE_BEGIN("Concurrent completions", 2000);

    std::vector<std::thread> threads;
    std::atomic<size_t> completed{0};

    for (size_t i = 0; i < NUM_PROMISES; i++) {
        threads.emplace_back([i, &promises, &completed]() {
            size_t result = i;
            nimcp_error_t err = nimcp_bio_promise_complete(promises[i], &result);
            if (err == NIMCP_SUCCESS) {
                completed++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    E2E_ASSERT(completed.load() == NUM_PROMISES, "All promises should complete");

    E2E_STAGE_END();

    // Stage 3: Verify all futures
    E2E_STAGE_BEGIN("Verify futures", 2000);

    size_t verified = 0;
    for (size_t i = 0; i < NUM_PROMISES; i++) {
        size_t result = 0;
        if (nimcp_bio_future_wait(futures[i], &result, 100) == NIMCP_SUCCESS) {
            verified++;
        }
    }

    E2E_ASSERT(verified == NUM_PROMISES, "All futures should be verified");

    E2E_STAGE_END();

    // Stage 4: Cleanup
    E2E_STAGE_BEGIN("Cleanup", 500);

    for (size_t i = 0; i < NUM_PROMISES; i++) {
        nimcp_bio_future_destroy(futures[i]);
        nimcp_bio_promise_destroy(promises[i]);
    }

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Pipeline 6: Biological Timing Accuracy
//=============================================================================

TEST_F(BioAsyncE2ETest, BiologicalTimingPipeline) {
    E2E_PIPELINE_START("Biological Timing Accuracy");

    // Stage 1: Verify decay constants
    E2E_STAGE_BEGIN("Verify decay constants", 50);

    float da_tau = BIO_COMP_DA_DECAY_TAU_MS;
    float sht_tau = BIO_COMP_5HT_DECAY_TAU_MS;
    float ne_tau = BIO_COMP_NE_DECAY_TAU_MS;
    float ach_tau = BIO_COMP_ACH_DECAY_TAU_MS;

    // Correct biological ordering: ACh < DA < NE < 5-HT
    E2E_ASSERT(ach_tau < da_tau, "ACh should decay faster than DA");
    E2E_ASSERT(da_tau < ne_tau, "DA should decay faster than NE");
    E2E_ASSERT(ne_tau < sht_tau, "NE should decay faster than 5-HT");

    E2E_STAGE_END();

    // Stage 2: Verify oscillation frequencies
    E2E_STAGE_BEGIN("Verify oscillation frequencies", 50);

    float delta_freq = nimcp_oscillation_center_freq(BIO_OSC_DELTA);
    float theta_freq = nimcp_oscillation_center_freq(BIO_OSC_THETA);
    float alpha_freq = nimcp_oscillation_center_freq(BIO_OSC_ALPHA);
    float beta_freq = nimcp_oscillation_center_freq(BIO_OSC_BETA);
    float gamma_freq = nimcp_oscillation_center_freq(BIO_OSC_GAMMA);

    E2E_ASSERT(delta_freq < theta_freq, "Delta should be slower than theta");
    E2E_ASSERT(theta_freq < alpha_freq, "Theta should be slower than alpha");
    E2E_ASSERT(alpha_freq < beta_freq, "Alpha should be slower than beta");
    E2E_ASSERT(beta_freq < gamma_freq, "Beta should be slower than gamma");

    E2E_ASSERT(delta_freq >= 0.5f && delta_freq <= 4.0f, "Delta should be 0.5-4 Hz");
    E2E_ASSERT(theta_freq >= 4.0f && theta_freq <= 8.0f, "Theta should be 4-8 Hz");
    E2E_ASSERT(alpha_freq >= 8.0f && alpha_freq <= 13.0f, "Alpha should be 8-13 Hz");
    E2E_ASSERT(beta_freq >= 13.0f && beta_freq <= 30.0f, "Beta should be 13-30 Hz");
    E2E_ASSERT(gamma_freq >= 30.0f && gamma_freq <= 100.0f, "Gamma should be 30-100 Hz");

    E2E_STAGE_END();

    // Stage 3: Test confidence decay
    E2E_STAGE_BEGIN("Test confidence decay", 200);

    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    E2E_ASSERT_NOT_NULL(promise, "Promise creation failed");

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    E2E_ASSERT_NOT_NULL(future, "Future creation failed");

    int result = 123;
    nimcp_bio_promise_complete(promise, &result);

    float initial_confidence = nimcp_bio_future_get_confidence(future);
    E2E_ASSERT(initial_confidence > 0.0f, "Initial confidence should be positive");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    float decayed_confidence = nimcp_bio_future_get_confidence(future);

    E2E_ASSERT(decayed_confidence <= initial_confidence, "Confidence should decay over time");

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);

    E2E_STAGE_END();

    // Stage 4: Test Kuramoto dynamics
    E2E_STAGE_BEGIN("Test Kuramoto dynamics", 200);

    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_ALPHA);
    E2E_ASSERT_NOT_NULL(sync, "Sync creation failed");

    nimcp_bio_promise_t p1 = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_promise_t p2 = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t f1 = nimcp_bio_promise_get_future(p1);
    nimcp_bio_future_t f2 = nimcp_bio_promise_get_future(p2);

    nimcp_phase_sync_add_future(sync, f1);
    nimcp_phase_sync_add_future(sync, f2);

    float coherence = nimcp_phase_sync_get_coherence(sync);
    E2E_ASSERT(coherence >= 0.0f && coherence <= 1.0f, "Coherence should be in [0, 1]");

    float mean_phase = nimcp_phase_sync_get_mean_phase(sync);
    E2E_ASSERT(mean_phase >= static_cast<float>(-M_PI) &&
               mean_phase <= static_cast<float>(2*M_PI), "Mean phase should be valid");

    nimcp_bio_future_destroy(f1);
    nimcp_bio_future_destroy(f2);
    nimcp_bio_promise_destroy(p1);
    nimcp_bio_promise_destroy(p2);
    nimcp_phase_sync_destroy(sync);

    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
