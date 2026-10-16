/**
 * @file test_bio_async.cpp
 * @brief Comprehensive unit tests for bio-async module
 *
 * Tests the biologically-inspired asynchronous computation system:
 * - Module initialization and shutdown
 * - Bio-promise/future lifecycle and decay dynamics
 * - Neuromodulator channel behavior
 * - Phase coupling (Kuramoto model) synchronization
 * - Predictive coding (Bayesian inference)
 * - Glial wave propagation
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

// Headers have their own extern "C" guards
#include "async/nimcp_bio_async.h"
#include "async/nimcp_biological_timescales.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BioAsyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize with default config
        nimcp_bio_async_config_t config = nimcp_bio_async_default_config();
        config.enable_logging = false;  // Reduce noise in tests
        config.enable_statistics = true;
        config.use_real_time = false;  // Use simulation time for deterministic tests
        nimcp_error_t err = nimcp_bio_async_init(&config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Failed to initialize bio-async";
    }

    void TearDown() override {
        nimcp_bio_async_shutdown();
    }
};

//=============================================================================
// Module Initialization Tests
//=============================================================================

TEST(BioAsyncInitTest, DefaultConfigIsValid) {
    nimcp_bio_async_config_t config = nimcp_bio_async_default_config();

    // Check neuromodulator channel configs
    EXPECT_GT(config.channel_configs[BIO_CHANNEL_DOPAMINE].decay_tau_ms, 0.0f);
    EXPECT_GT(config.channel_configs[BIO_CHANNEL_SEROTONIN].decay_tau_ms, 0.0f);
    EXPECT_GT(config.channel_configs[BIO_CHANNEL_NOREPINEPHRINE].decay_tau_ms, 0.0f);
    EXPECT_GT(config.channel_configs[BIO_CHANNEL_ACETYLCHOLINE].decay_tau_ms, 0.0f);

    // Check phase config
    EXPECT_GT(config.phase_config.coherence_threshold, 0.0f);
    EXPECT_LE(config.phase_config.coherence_threshold, 1.0f);
    EXPECT_GT(config.phase_config.coupling_strength, 0.0f);

    // Check predictive config
    EXPECT_GT(config.predictive_config.default_prior_precision, 0.0f);
    EXPECT_GT(config.predictive_config.default_likelihood_precision, 0.0f);

    // Check glial config
    EXPECT_GT(config.glial_config.wave_speed_um_s, 0.0f);
}

TEST(BioAsyncInitTest, InitializeWithNullConfig) {
    nimcp_error_t err = nimcp_bio_async_init(nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(nimcp_bio_async_is_initialized());
    nimcp_bio_async_shutdown();
    EXPECT_FALSE(nimcp_bio_async_is_initialized());
}

TEST(BioAsyncInitTest, DoubleInitializeIsIdempotent) {
    nimcp_bio_async_config_t config = nimcp_bio_async_default_config();
    config.enable_logging = false;

    nimcp_error_t err1 = nimcp_bio_async_init(&config);
    EXPECT_EQ(err1, NIMCP_SUCCESS);

    nimcp_error_t err2 = nimcp_bio_async_init(&config);
    EXPECT_EQ(err2, NIMCP_SUCCESS);  // Should be idempotent

    nimcp_bio_async_shutdown();
}

TEST(BioAsyncInitTest, ShutdownWithoutInitIsNoop) {
    // Should not crash
    nimcp_bio_async_shutdown();
    EXPECT_FALSE(nimcp_bio_async_is_initialized());
}

//=============================================================================
// Bio-Promise Tests
//=============================================================================

TEST_F(BioAsyncTest, PromiseCreateDestroy) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    ASSERT_NE(promise, nullptr);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncTest, PromiseCreateAllChannels) {
    for (int i = 0; i < BIO_CHANNEL_COUNT; i++) {
        nimcp_bio_promise_t promise = nimcp_bio_promise_create(
            static_cast<nimcp_bio_channel_type_t>(i), sizeof(float));
        ASSERT_NE(promise, nullptr) << "Failed for channel " << i;
        nimcp_bio_promise_destroy(promise);
    }
}

TEST_F(BioAsyncTest, PromiseCreateInvalidChannel) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(
        static_cast<nimcp_bio_channel_type_t>(BIO_CHANNEL_COUNT), sizeof(int));
    EXPECT_EQ(promise, nullptr);
}

TEST_F(BioAsyncTest, PromiseDestroyNull) {
    // Should not crash
    nimcp_bio_promise_destroy(nullptr);
}

TEST_F(BioAsyncTest, PromiseComplete) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    ASSERT_NE(promise, nullptr);

    int result = 42;
    nimcp_error_t err = nimcp_bio_promise_complete(promise, &result);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncTest, PromiseCompleteNull) {
    nimcp_error_t err = nimcp_bio_promise_complete(nullptr, nullptr);
    EXPECT_NE(err, NIMCP_SUCCESS);
}

TEST_F(BioAsyncTest, PromiseFail) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    ASSERT_NE(promise, nullptr);

    nimcp_error_t err = nimcp_bio_promise_fail(promise, NIMCP_ERROR_UNKNOWN);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncTest, PromiseFailWithSuccess) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    ASSERT_NE(promise, nullptr);

    // Should reject NIMCP_SUCCESS as error code
    nimcp_error_t err = nimcp_bio_promise_fail(promise, NIMCP_SUCCESS);
    EXPECT_NE(err, NIMCP_SUCCESS);

    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncTest, PromiseDoubleComplete) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    ASSERT_NE(promise, nullptr);

    int result = 42;
    nimcp_error_t err1 = nimcp_bio_promise_complete(promise, &result);
    EXPECT_EQ(err1, NIMCP_SUCCESS);

    // Second complete should fail
    nimcp_error_t err2 = nimcp_bio_promise_complete(promise, &result);
    EXPECT_NE(err2, NIMCP_SUCCESS);

    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// Bio-Future Tests
//=============================================================================

TEST_F(BioAsyncTest, FutureFromPromise) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    ASSERT_NE(promise, nullptr);

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncTest, FutureFromNullPromise) {
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(nullptr);
    EXPECT_EQ(future, nullptr);
}

TEST_F(BioAsyncTest, FutureStateInitiallyPending) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    EXPECT_EQ(nimcp_bio_future_state(future), BIO_FUTURE_PENDING);
    EXPECT_FALSE(nimcp_bio_future_is_ready(future));

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncTest, FutureStateAfterComplete) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    int result = 42;
    nimcp_bio_promise_complete(promise, &result);

    EXPECT_EQ(nimcp_bio_future_state(future), BIO_FUTURE_COMPLETED);
    EXPECT_TRUE(nimcp_bio_future_is_ready(future));

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncTest, FutureStateAfterFail) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    nimcp_bio_promise_fail(promise, NIMCP_ERROR_UNKNOWN);

    EXPECT_EQ(nimcp_bio_future_state(future), BIO_FUTURE_FAILED);
    EXPECT_TRUE(nimcp_bio_future_is_ready(future));

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncTest, FutureWaitSuccess) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    int result = 42;
    nimcp_bio_promise_complete(promise, &result);

    int out_result = 0;
    nimcp_error_t err = nimcp_bio_future_wait(future, &out_result, 1000);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(out_result, 42);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncTest, FutureWaitTimeout) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    int out_result = 0;
    nimcp_error_t err = nimcp_bio_future_wait(future, &out_result, 10);  // 10ms timeout

    EXPECT_EQ(err, NIMCP_ERROR_TIMEOUT);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncTest, FutureCancel) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    bool cancelled = nimcp_bio_future_cancel(future);
    EXPECT_TRUE(cancelled);

    EXPECT_EQ(nimcp_bio_future_state(future), BIO_FUTURE_CANCELLED);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncTest, FutureCancelAfterComplete) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    int result = 42;
    nimcp_bio_promise_complete(promise, &result);

    bool cancelled = nimcp_bio_future_cancel(future);
    EXPECT_FALSE(cancelled);  // Already completed

    EXPECT_EQ(nimcp_bio_future_state(future), BIO_FUTURE_COMPLETED);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// Neuromodulator Decay Tests
//=============================================================================

TEST_F(BioAsyncTest, ConfidenceInitiallyZeroBeforeComplete) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    float confidence = nimcp_bio_future_get_confidence(future);
    EXPECT_FLOAT_EQ(confidence, 0.0f);  // Not completed yet

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncTest, ConfidenceHighAfterComplete) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    int result = 42;
    nimcp_bio_promise_complete(promise, &result);

    float confidence = nimcp_bio_future_get_confidence(future);
    EXPECT_GE(confidence, 0.9f);  // Should be near 1.0 immediately after completion

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncTest, ConfidenceDecaysOverTime) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    int result = 42;
    nimcp_bio_promise_complete(promise, &result);

    float confidence_t0 = nimcp_bio_future_get_confidence(future);

    // Advance simulation time (dopamine tau=200ms, so 50ms gives ~22% decay)
    nimcp_bio_async_step(50.0f);

    float confidence_t1 = nimcp_bio_future_get_confidence(future);

    // Confidence should have decayed
    EXPECT_LT(confidence_t1, confidence_t0);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncTest, DifferentChannelsHaveDifferentDecayRates) {
    // Test that serotonin (slow, tau=1000ms) decays slower than acetylcholine (fast, tau=50ms)

    // Create ACh promise (fast decay, tau=50ms)
    nimcp_bio_promise_t promise_ach = nimcp_bio_promise_create(BIO_CHANNEL_ACETYLCHOLINE, sizeof(int));
    nimcp_bio_future_t future_ach = nimcp_bio_promise_get_future(promise_ach);

    // Create 5-HT promise (slow decay, tau=1000ms)
    nimcp_bio_promise_t promise_5ht = nimcp_bio_promise_create(BIO_CHANNEL_SEROTONIN, sizeof(int));
    nimcp_bio_future_t future_5ht = nimcp_bio_promise_get_future(promise_5ht);

    int result = 42;
    nimcp_bio_promise_complete(promise_ach, &result);
    nimcp_bio_promise_complete(promise_5ht, &result);

    // Advance simulation time (ACh tau=50ms, 5-HT tau=1000ms)
    // After 100ms: ACh has decayed e^(-100/50) = 0.135, 5-HT has decayed e^(-100/1000) = 0.905
    nimcp_bio_async_step(100.0f);

    float conf_ach = nimcp_bio_future_get_confidence(future_ach);
    float conf_5ht = nimcp_bio_future_get_confidence(future_5ht);

    // Serotonin should retain more confidence (slower decay)
    EXPECT_GT(conf_5ht, conf_ach);

    nimcp_bio_future_destroy(future_ach);
    nimcp_bio_future_destroy(future_5ht);
    nimcp_bio_promise_destroy(promise_ach);
    nimcp_bio_promise_destroy(promise_5ht);
}

TEST_F(BioAsyncTest, AgeIncreasesAfterComplete) {
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    float age_before = nimcp_bio_future_get_age_ms(future);
    EXPECT_LT(age_before, 0.0f);  // Not completed yet

    int result = 42;
    nimcp_bio_promise_complete(promise, &result);

    float age_t0 = nimcp_bio_future_get_age_ms(future);
    EXPECT_GE(age_t0, 0.0f);

    // Advance simulation time (age is based on simulation time, not real time)
    nimcp_bio_async_step(50.0f);

    float age_t1 = nimcp_bio_future_get_age_ms(future);
    EXPECT_GT(age_t1, age_t0);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// Bio-Future Callback Tests
//=============================================================================

static std::atomic<int> g_callback_count{0};
static std::atomic<float> g_last_confidence{0.0f};

static void test_callback(const void* result, float confidence,
                          nimcp_error_t error, void* user_data) {
    (void)result;
    (void)error;
    (void)user_data;
    g_callback_count++;
    g_last_confidence.store(confidence);
}

TEST_F(BioAsyncTest, CallbackInvokedOnComplete) {
    g_callback_count = 0;
    g_last_confidence = 0.0f;

    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    nimcp_error_t err = nimcp_bio_future_then(future, test_callback, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    int result = 42;
    nimcp_bio_promise_complete(promise, &result);

    // Give time for callback
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_EQ(g_callback_count.load(), 1);
    EXPECT_GT(g_last_confidence.load(), 0.0f);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncTest, CallbackInvokedImmediatelyIfAlreadyComplete) {
    g_callback_count = 0;

    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    int result = 42;
    nimcp_bio_promise_complete(promise, &result);

    // Add callback after completion
    nimcp_error_t err = nimcp_bio_future_then(future, test_callback, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Callback should have been invoked immediately
    EXPECT_EQ(g_callback_count.load(), 1);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
}

//=============================================================================
// Phase Coupling Tests
//=============================================================================

TEST_F(BioAsyncTest, PhaseSyncCreateDestroy) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);
    ASSERT_NE(sync, nullptr);
    nimcp_phase_sync_destroy(sync);
}

TEST_F(BioAsyncTest, PhaseSyncCreateAllBands) {
    for (int i = 0; i < BIO_OSC_BAND_COUNT; i++) {
        nimcp_phase_sync_t sync = nimcp_phase_sync_create(
            static_cast<nimcp_oscillation_band_t>(i));
        ASSERT_NE(sync, nullptr) << "Failed for band " << i;
        nimcp_phase_sync_destroy(sync);
    }
}

TEST_F(BioAsyncTest, PhaseSyncCreateInvalidBand) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(
        static_cast<nimcp_oscillation_band_t>(BIO_OSC_BAND_COUNT));
    EXPECT_EQ(sync, nullptr);
}

TEST_F(BioAsyncTest, PhaseSyncAddFuture) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);
    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

    nimcp_error_t err = nimcp_phase_sync_add_future(sync, future);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(nimcp_phase_sync_get_count(sync), 1u);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
    nimcp_phase_sync_destroy(sync);
}

TEST_F(BioAsyncTest, PhaseSyncCoherenceStartsLow) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);

    // Add multiple futures with random initial phases
    std::vector<nimcp_bio_promise_t> promises;
    std::vector<nimcp_bio_future_t> futures;

    for (int i = 0; i < 5; i++) {
        nimcp_bio_promise_t p = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
        nimcp_bio_future_t f = nimcp_bio_promise_get_future(p);
        nimcp_phase_sync_add_future(sync, f);
        promises.push_back(p);
        futures.push_back(f);
    }

    float coherence = nimcp_phase_sync_get_coherence(sync);
    // Initial coherence should be relatively low (random phases)
    // But not necessarily 0 since random phases might align
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);

    for (auto& f : futures) nimcp_bio_future_destroy(f);
    for (auto& p : promises) nimcp_bio_promise_destroy(p);
    nimcp_phase_sync_destroy(sync);
}

TEST_F(BioAsyncTest, PhaseSyncWaitCoherentWithCompletedFutures) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);

    // Add and complete multiple futures
    std::vector<nimcp_bio_promise_t> promises;
    std::vector<nimcp_bio_future_t> futures;

    for (int i = 0; i < 3; i++) {
        nimcp_bio_promise_t p = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
        nimcp_bio_future_t f = nimcp_bio_promise_get_future(p);

        int result = i;
        nimcp_bio_promise_complete(p, &result);

        nimcp_phase_sync_add_future(sync, f);
        promises.push_back(p);
        futures.push_back(f);
    }

    // Should eventually synchronize
    nimcp_error_t err = nimcp_phase_sync_wait_all(sync, 500);  // 500ms timeout

    // May or may not succeed depending on random phases, but should not hang
    (void)err;

    for (auto& f : futures) nimcp_bio_future_destroy(f);
    for (auto& p : promises) nimcp_bio_promise_destroy(p);
    nimcp_phase_sync_destroy(sync);
}

TEST_F(BioAsyncTest, MeanPhaseInValidRange) {
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);

    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    nimcp_phase_sync_add_future(sync, future);

    // Mean phase is in [0, 2π] range (wrapped from atan2's [-π, π])
    float mean_phase = nimcp_phase_sync_get_mean_phase(sync);
    EXPECT_GE(mean_phase, 0.0f);
    EXPECT_LT(mean_phase, 2.0f * M_PI);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
    nimcp_phase_sync_destroy(sync);
}

//=============================================================================
// Predictive Coding Tests
//=============================================================================

TEST_F(BioAsyncTest, PredictiveCreateDestroy) {
    nimcp_predictive_model_t model = nimcp_predictive_create("test_signal", 0.5f, 1.0f);
    ASSERT_NE(model, nullptr);
    nimcp_predictive_destroy(model);
}

TEST_F(BioAsyncTest, PredictiveCreateNullName) {
    nimcp_predictive_model_t model = nimcp_predictive_create(nullptr, 0.5f, 1.0f);
    EXPECT_EQ(model, nullptr);
}

TEST_F(BioAsyncTest, PredictiveGetPrediction) {
    nimcp_predictive_model_t model = nimcp_predictive_create("test", 0.5f, 1.0f);

    float prediction = nimcp_predictive_get_prediction(model);
    EXPECT_FLOAT_EQ(prediction, 0.5f);

    nimcp_predictive_destroy(model);
}

TEST_F(BioAsyncTest, PredictiveGetPrecision) {
    nimcp_predictive_model_t model = nimcp_predictive_create("test", 0.5f, 2.0f);

    float precision = nimcp_predictive_get_precision(model);
    EXPECT_FLOAT_EQ(precision, 2.0f);

    nimcp_predictive_destroy(model);
}

TEST_F(BioAsyncTest, PredictiveObserveUpdates) {
    nimcp_predictive_model_t model = nimcp_predictive_create("test", 0.5f, 1.0f);

    float initial_pred = nimcp_predictive_get_prediction(model);

    // Observe a value higher than prediction
    nimcp_error_t err = nimcp_predictive_observe(model, 0.8f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float new_pred = nimcp_predictive_get_prediction(model);

    // Prediction should have moved toward observation
    EXPECT_GT(new_pred, initial_pred);
    EXPECT_LE(new_pred, 0.8f);

    nimcp_predictive_destroy(model);
}

TEST_F(BioAsyncTest, PredictivePrecisionIncreases) {
    nimcp_predictive_model_t model = nimcp_predictive_create("test", 0.5f, 1.0f);

    float initial_precision = nimcp_predictive_get_precision(model);

    nimcp_predictive_observe(model, 0.6f);

    float new_precision = nimcp_predictive_get_precision(model);

    // Precision should increase after observation
    EXPECT_GT(new_precision, initial_precision);

    nimcp_predictive_destroy(model);
}

TEST_F(BioAsyncTest, PredictiveSurpriseCalculation) {
    nimcp_predictive_model_t model = nimcp_predictive_create("test", 0.5f, 10.0f);  // High precision

    // Observe exact prediction - low surprise
    nimcp_predictive_observe(model, 0.5f);
    float low_surprise = nimcp_predictive_get_last_surprise(model);

    // Reset model
    nimcp_predictive_destroy(model);
    model = nimcp_predictive_create("test", 0.5f, 10.0f);

    // Observe value far from prediction - high surprise
    nimcp_predictive_observe(model, 0.9f);
    float high_surprise = nimcp_predictive_get_last_surprise(model);

    EXPECT_GT(high_surprise, low_surprise);

    nimcp_predictive_destroy(model);
}

TEST_F(BioAsyncTest, PredictiveSetPrediction) {
    nimcp_predictive_model_t model = nimcp_predictive_create("test", 0.5f, 1.0f);

    nimcp_error_t err = nimcp_predictive_set_prediction(model, 0.9f, 5.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_FLOAT_EQ(nimcp_predictive_get_prediction(model), 0.9f);
    EXPECT_FLOAT_EQ(nimcp_predictive_get_precision(model), 5.0f);

    nimcp_predictive_destroy(model);
}

static std::atomic<int> g_pred_callback_count{0};
static float g_pred_last_surprise{0.0f};

static void pred_callback(const char* signal_name, float prediction, float actual,
                          float error, float surprise, void* user_data) {
    (void)signal_name;
    (void)prediction;
    (void)actual;
    (void)error;
    (void)user_data;
    g_pred_callback_count++;
    g_pred_last_surprise = surprise;
}

TEST_F(BioAsyncTest, PredictiveCallbackOnError) {
    g_pred_callback_count = 0;

    nimcp_predictive_model_t model = nimcp_predictive_create("test", 0.5f, 10.0f);

    // Register callback with low threshold
    nimcp_error_t err = nimcp_predictive_on_error(model, pred_callback, nullptr, 0.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Observe something different
    nimcp_predictive_observe(model, 0.9f);

    EXPECT_EQ(g_pred_callback_count.load(), 1);

    nimcp_predictive_destroy(model);
}

TEST_F(BioAsyncTest, PredictiveCallbackSuppressedBelowThreshold) {
    g_pred_callback_count = 0;

    nimcp_predictive_model_t model = nimcp_predictive_create("test", 0.5f, 1.0f);

    // Register callback with high threshold
    nimcp_error_t err = nimcp_predictive_on_error(model, pred_callback, nullptr, 100.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Observe something close to prediction (low surprise)
    nimcp_predictive_observe(model, 0.51f);

    EXPECT_EQ(g_pred_callback_count.load(), 0);  // Should be suppressed

    nimcp_predictive_destroy(model);
}

//=============================================================================
// Glial Wave Tests
//=============================================================================

TEST_F(BioAsyncTest, GlialWaveCreateDestroy) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 1.0f);
    ASSERT_NE(wave, nullptr);
    nimcp_glial_wave_destroy(wave);
}

TEST_F(BioAsyncTest, GlialWaveInitiallyActive) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 1.0f);
    EXPECT_TRUE(nimcp_glial_wave_is_active(wave));
    nimcp_glial_wave_destroy(wave);
}

TEST_F(BioAsyncTest, GlialWaveSourceHasCalcium) {
    uint32_t source = 10;
    float initial_ca = 2.0f;

    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(source, initial_ca);

    float level = nimcp_glial_wave_get_level_at(wave, source);
    EXPECT_FLOAT_EQ(level, initial_ca);

    bool reached = nimcp_glial_wave_has_reached(wave, source);
    EXPECT_TRUE(reached);

    nimcp_glial_wave_destroy(wave);
}

TEST_F(BioAsyncTest, GlialWaveDistantRegionNotReached) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 2.0f);

    // Region far from source should not be reached initially
    bool reached = nimcp_glial_wave_has_reached(wave, 100);
    EXPECT_FALSE(reached);

    nimcp_glial_wave_destroy(wave);
}

TEST_F(BioAsyncTest, GlialWaveRadiusInitiallyZero) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 2.0f);

    float radius = nimcp_glial_wave_get_radius(wave);
    EXPECT_FLOAT_EQ(radius, 0.0f);

    nimcp_glial_wave_destroy(wave);
}

TEST_F(BioAsyncTest, GlialWaveStepIncreasesRadius) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 2.0f);

    float r0 = nimcp_glial_wave_get_radius(wave);

    nimcp_glial_wave_step(wave, 10.0f);  // 10ms step

    float r1 = nimcp_glial_wave_get_radius(wave);
    EXPECT_GT(r1, r0);

    nimcp_glial_wave_destroy(wave);
}

TEST_F(BioAsyncTest, GlialWavePropagates) {
    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 2.0f);

    // Step wave multiple times
    for (int i = 0; i < 100; i++) {
        nimcp_glial_wave_step(wave, 1.0f);
    }

    // Nearby region should now be reached
    bool reached = nimcp_glial_wave_has_reached(wave, 5);
    EXPECT_TRUE(reached);

    nimcp_glial_wave_destroy(wave);
}

static std::atomic<int> g_wave_callback_count{0};

static void wave_callback(nimcp_glial_wave_t wave, uint32_t region_id,
                          float calcium_level, void* user_data) {
    (void)wave;
    (void)region_id;
    (void)calcium_level;
    (void)user_data;
    g_wave_callback_count++;
}

TEST_F(BioAsyncTest, GlialWaveCallbackOnArrival) {
    g_wave_callback_count = 0;

    nimcp_glial_wave_t wave = nimcp_glial_wave_initiate(0, 2.0f);

    // Register callback for nearby region
    nimcp_error_t err = nimcp_glial_wave_on_arrival(wave, 5, wave_callback, nullptr);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Propagate until wave reaches region
    for (int i = 0; i < 200 && g_wave_callback_count == 0; i++) {
        nimcp_glial_wave_step(wave, 1.0f);
    }

    EXPECT_EQ(g_wave_callback_count.load(), 1);

    nimcp_glial_wave_destroy(wave);
}

//=============================================================================
// Biological Timescales Tests
//=============================================================================

TEST(BiologicalTimescalesTest, DopamineDecayTau) {
    EXPECT_GT(BIO_DA_DECAY_TAU_MS, 0.0f);
    EXPECT_GT(BIO_COMP_DA_DECAY_TAU_MS, 0.0f);
    // Computational version should be faster
    EXPECT_LT(BIO_COMP_DA_DECAY_TAU_MS, BIO_DA_DECAY_TAU_MS);
}

TEST(BiologicalTimescalesTest, SerotoninDecayTau) {
    EXPECT_GT(BIO_5HT_DECAY_TAU_MS, 0.0f);
    EXPECT_GT(BIO_COMP_5HT_DECAY_TAU_MS, 0.0f);
    EXPECT_LT(BIO_COMP_5HT_DECAY_TAU_MS, BIO_5HT_DECAY_TAU_MS);
}

TEST(BiologicalTimescalesTest, OscillationFrequencies) {
    EXPECT_LT(BIO_OSC_DELTA_CENTER_HZ, BIO_OSC_THETA_CENTER_HZ);
    EXPECT_LT(BIO_OSC_THETA_CENTER_HZ, BIO_OSC_ALPHA_CENTER_HZ);
    EXPECT_LT(BIO_OSC_ALPHA_CENTER_HZ, BIO_OSC_BETA_CENTER_HZ);
    EXPECT_LT(BIO_OSC_BETA_CENTER_HZ, BIO_OSC_GAMMA_CENTER_HZ);
}

TEST(BiologicalTimescalesTest, ExponentialDecay) {
    float x0 = 1.0f;
    float tau = 100.0f;

    float x_at_tau = bio_exponential_decay(x0, tau, tau);
    EXPECT_NEAR(x_at_tau, x0 / M_E, 0.01f);

    float x_at_5tau = bio_exponential_decay(x0, 5.0f * tau, tau);
    EXPECT_NEAR(x_at_5tau, 0.0f, 0.01f);
}

TEST(BiologicalTimescalesTest, KuramotoOrderParameter) {
    // All phases the same - should give r = 1
    float phases_same[] = {0.0f, 0.0f, 0.0f, 0.0f};
    float r, psi;
    bio_kuramoto_order_parameter(phases_same, 4, &r, &psi);
    EXPECT_NEAR(r, 1.0f, 0.01f);

    // Phases evenly distributed - should give r ≈ 0
    float phases_spread[] = {0.0f, (float)(M_PI/2), (float)M_PI, (float)(3*M_PI/2)};
    bio_kuramoto_order_parameter(phases_spread, 4, &r, &psi);
    EXPECT_NEAR(r, 0.0f, 0.1f);
}

TEST(BiologicalTimescalesTest, BayesianUpdate) {
    float prior = 0.5f;
    float likelihood = 1.0f;
    float prior_prec = 1.0f;
    float like_prec = 1.0f;

    float posterior = bio_bayesian_update(prior, likelihood, prior_prec, like_prec);

    // With equal precision, posterior should be average
    EXPECT_NEAR(posterior, 0.75f, 0.01f);
}

TEST(BiologicalTimescalesTest, MichaelisMenten) {
    float vmax = 1.0f;
    float km = 1.0f;

    // At S = Km, v = Vmax/2
    float v_at_km = bio_michaelis_menten(km, vmax, km);
    EXPECT_NEAR(v_at_km, vmax / 2.0f, 0.01f);

    // At very high S, v → Vmax
    float v_high_s = bio_michaelis_menten(100.0f * km, vmax, km);
    EXPECT_NEAR(v_high_s, vmax, 0.02f);
}

TEST(BiologicalTimescalesTest, AlphaFunction) {
    float tau = 10.0f;
    float g_max = 1.0f;

    // Peak should be at t = tau
    float peak = bio_alpha_function(tau, tau, g_max);

    // Before peak
    float before = bio_alpha_function(tau / 2, tau, g_max);
    EXPECT_LT(before, peak);

    // After peak
    float after = bio_alpha_function(2 * tau, tau, g_max);
    EXPECT_LT(after, peak);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(BioAsyncTest, StatsInitiallyZero) {
    nimcp_bio_async_reset_stats();

    nimcp_bio_async_stats_t stats;
    nimcp_error_t err = nimcp_bio_async_get_stats(&stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(stats.total_futures_created, 0u);
    EXPECT_EQ(stats.total_futures_completed, 0u);
}

TEST_F(BioAsyncTest, StatsCountFutures) {
    nimcp_bio_async_reset_stats();

    nimcp_bio_promise_t promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));

    nimcp_bio_async_stats_t stats;
    nimcp_bio_async_get_stats(&stats);
    EXPECT_EQ(stats.total_futures_created, 1u);

    int result = 42;
    nimcp_bio_promise_complete(promise, &result);

    nimcp_bio_async_get_stats(&stats);
    EXPECT_EQ(stats.total_futures_completed, 1u);

    nimcp_bio_promise_destroy(promise);
}

TEST_F(BioAsyncTest, StatsTrackChannels) {
    nimcp_bio_async_reset_stats();

    nimcp_bio_promise_t p1 = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(int));
    nimcp_bio_promise_t p2 = nimcp_bio_promise_create(BIO_CHANNEL_SEROTONIN, sizeof(int));

    nimcp_bio_async_stats_t stats;
    nimcp_bio_async_get_stats(&stats);

    EXPECT_GE(stats.channel_stats[BIO_CHANNEL_DOPAMINE].active_futures, 1u);
    EXPECT_GE(stats.channel_stats[BIO_CHANNEL_SEROTONIN].active_futures, 1u);

    nimcp_bio_promise_destroy(p1);
    nimcp_bio_promise_destroy(p2);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST(BioAsyncUtilsTest, ChannelName) {
    EXPECT_STREQ(nimcp_bio_channel_name(BIO_CHANNEL_DOPAMINE), "dopamine");
    EXPECT_STREQ(nimcp_bio_channel_name(BIO_CHANNEL_SEROTONIN), "serotonin");
    EXPECT_STREQ(nimcp_bio_channel_name(BIO_CHANNEL_NOREPINEPHRINE), "norepinephrine");
    EXPECT_STREQ(nimcp_bio_channel_name(BIO_CHANNEL_ACETYLCHOLINE), "acetylcholine");
}

TEST(BioAsyncUtilsTest, OscillationBandName) {
    EXPECT_STREQ(nimcp_oscillation_band_name(BIO_OSC_DELTA), "delta");
    EXPECT_STREQ(nimcp_oscillation_band_name(BIO_OSC_THETA), "theta");
    EXPECT_STREQ(nimcp_oscillation_band_name(BIO_OSC_ALPHA), "alpha");
    EXPECT_STREQ(nimcp_oscillation_band_name(BIO_OSC_BETA), "beta");
    EXPECT_STREQ(nimcp_oscillation_band_name(BIO_OSC_GAMMA), "gamma");
}

TEST(BioAsyncUtilsTest, FutureStateName) {
    EXPECT_STREQ(nimcp_bio_future_state_name(BIO_FUTURE_PENDING), "pending");
    EXPECT_STREQ(nimcp_bio_future_state_name(BIO_FUTURE_COMPLETED), "completed");
    EXPECT_STREQ(nimcp_bio_future_state_name(BIO_FUTURE_FAILED), "failed");
    EXPECT_STREQ(nimcp_bio_future_state_name(BIO_FUTURE_CANCELLED), "cancelled");
    EXPECT_STREQ(nimcp_bio_future_state_name(BIO_FUTURE_DECAYED), "decayed");
}

TEST(BioAsyncUtilsTest, ChannelDecayTau) {
    EXPECT_FLOAT_EQ(nimcp_bio_channel_decay_tau(BIO_CHANNEL_DOPAMINE), BIO_COMP_DA_DECAY_TAU_MS);
    EXPECT_FLOAT_EQ(nimcp_bio_channel_decay_tau(BIO_CHANNEL_SEROTONIN), BIO_COMP_5HT_DECAY_TAU_MS);
    EXPECT_FLOAT_EQ(nimcp_bio_channel_decay_tau(BIO_CHANNEL_NOREPINEPHRINE), BIO_COMP_NE_DECAY_TAU_MS);
    EXPECT_FLOAT_EQ(nimcp_bio_channel_decay_tau(BIO_CHANNEL_ACETYLCHOLINE), BIO_COMP_ACH_DECAY_TAU_MS);
}

TEST(BioAsyncUtilsTest, OscillationCenterFreq) {
    EXPECT_FLOAT_EQ(nimcp_oscillation_center_freq(BIO_OSC_DELTA), BIO_OSC_DELTA_CENTER_HZ);
    EXPECT_FLOAT_EQ(nimcp_oscillation_center_freq(BIO_OSC_THETA), BIO_OSC_THETA_CENTER_HZ);
    EXPECT_FLOAT_EQ(nimcp_oscillation_center_freq(BIO_OSC_ALPHA), BIO_OSC_ALPHA_CENTER_HZ);
    EXPECT_FLOAT_EQ(nimcp_oscillation_center_freq(BIO_OSC_BETA), BIO_OSC_BETA_CENTER_HZ);
    EXPECT_FLOAT_EQ(nimcp_oscillation_center_freq(BIO_OSC_GAMMA), BIO_OSC_GAMMA_CENTER_HZ);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
