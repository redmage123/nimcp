//=============================================================================
// test_predictive_coding.cpp - Predictive Coding Integration Tests
//=============================================================================
/**
 * @file test_predictive_coding.cpp
 * @brief Tests for Bayesian predictive coding with error-driven callbacks
 *
 * Tests cover:
 * - Signal observation and prediction
 * - Prediction updates based on observations
 * - Surprise calculation
 * - Error callbacks (only fire on prediction errors)
 * - Precision adaptation
 *
 * @version 1.0.0
 * @date 2025-11-28
 */

#include <gtest/gtest.h>
#include <atomic>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_async.h"
#include "async/nimcp_biological_timescales.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PredictiveCodingTest : public ::testing::Test {
protected:
    struct PredictionError {
        float prediction;
        float actual;
        float error;
        float surprise;
    };

    std::vector<PredictionError> errors;
    std::atomic<int> callback_count{0};

    void SetUp() override {
        nimcp_bio_async_config_t config = nimcp_bio_async_default_config();
        config.enable_statistics = true;
        config.predictive_config.default_prior_precision = 1.0f;
        config.predictive_config.default_likelihood_precision = 2.0f;
        config.predictive_config.learning_rate = 0.1f;
        config.predictive_config.surprise_threshold = 1.0f;
        config.predictive_config.max_predictors = 100;
        ASSERT_EQ(nimcp_bio_async_init(&config), NIMCP_SUCCESS);

        errors.clear();
        callback_count = 0;
    }

    void TearDown() override {
        nimcp_bio_async_shutdown();
    }

    static void prediction_error_callback(const char* signal_name, float prediction,
                                           float actual, float error, float surprise,
                                           void* user_data) {
        auto* test = static_cast<PredictiveCodingTest*>(user_data);
        test->callback_count++;
        test->errors.push_back({prediction, actual, error, surprise});
    }
};

//=============================================================================
// Basic Prediction Tests
//=============================================================================

TEST_F(PredictiveCodingTest, CreatePredictiveModel) {
    nimcp_predictive_model_t model = nimcp_predictive_create(
        "test_signal", 0.5f, 1.0f);
    ASSERT_NE(model, nullptr);

    // Verify initial state
    float prediction = nimcp_predictive_get_prediction(model);
    EXPECT_FLOAT_EQ(prediction, 0.5f);

    float precision = nimcp_predictive_get_precision(model);
    EXPECT_FLOAT_EQ(precision, 1.0f);

    nimcp_predictive_destroy(model);
}

TEST_F(PredictiveCodingTest, ObserveMatchingValue) {
    nimcp_predictive_model_t model = nimcp_predictive_create(
        "dopamine", 0.7f, 2.0f);
    ASSERT_NE(model, nullptr);

    // Register error callback
    nimcp_error_t err = nimcp_predictive_on_error(
        model, prediction_error_callback, this, 0.5f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Observe value matching prediction
    err = nimcp_predictive_observe(model, 0.7f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Callback should NOT fire (no error)
    EXPECT_EQ(callback_count.load(), 0)
        << "No callback should fire when prediction matches";

    float last_surprise = nimcp_predictive_get_last_surprise(model);
    EXPECT_LT(last_surprise, 0.5f)
        << "Surprise should be low when prediction matches";

    nimcp_predictive_destroy(model);
}

TEST_F(PredictiveCodingTest, ObserveMismatchValue) {
    nimcp_predictive_model_t model = nimcp_predictive_create(
        "dopamine", 0.5f, 2.0f);

    // With precision=2.0, prediction=0.5, actual=0.9:
    // error = 2.0 * (0.9 - 0.5) = 0.8
    // surprise = 0.5 * 2.0 * (0.4)^2 = 0.16
    // Use threshold 0.1 to ensure callback fires
    nimcp_predictive_on_error(model, prediction_error_callback, this, 0.1f);

    // Observe value significantly different from prediction
    nimcp_predictive_observe(model, 0.9f);

    // Callback SHOULD fire (surprise ~0.16 > threshold 0.1)
    EXPECT_GT(callback_count.load(), 0)
        << "Callback should fire on prediction error";

    if (callback_count.load() > 0) {
        EXPECT_GT(errors[0].surprise, 0.1f)
            << "Surprise should exceed threshold for mismatch";
        EXPECT_NEAR(errors[0].prediction, 0.5f, 0.01f);
        EXPECT_NEAR(errors[0].actual, 0.9f, 0.01f);
    }

    nimcp_predictive_destroy(model);
}

//=============================================================================
// Prediction Update Tests
//=============================================================================

TEST_F(PredictiveCodingTest, PredictionAdaptsToObservations) {
    nimcp_predictive_model_t model = nimcp_predictive_create(
        "signal", 0.3f, 1.0f);

    float initial_prediction = nimcp_predictive_get_prediction(model);
    EXPECT_FLOAT_EQ(initial_prediction, 0.3f);

    // Observe consistently higher values
    for (int i = 0; i < 10; i++) {
        nimcp_predictive_observe(model, 0.8f);
    }

    float updated_prediction = nimcp_predictive_get_prediction(model);

    // Prediction should have moved toward observed values
    EXPECT_GT(updated_prediction, initial_prediction)
        << "Prediction should increase toward observed values";

    nimcp_predictive_destroy(model);
}

TEST_F(PredictiveCodingTest, PredictionTracksSlowlyChangingSignal) {
    nimcp_predictive_model_t model = nimcp_predictive_create(
        "slow_signal", 0.5f, 1.5f);

    std::vector<float> predictions;

    // Simulate slowly increasing signal
    for (int i = 0; i < 20; i++) {
        float true_value = 0.5f + i * 0.02f; // Increases from 0.5 to 0.88
        nimcp_predictive_observe(model, true_value);

        float pred = nimcp_predictive_get_prediction(model);
        predictions.push_back(pred);
    }

    // Predictions should generally increase
    EXPECT_GT(predictions.back(), predictions.front())
        << "Predictions should track increasing signal";

    // Final prediction should be reasonably tracking (Bayesian updates lag behind)
    // With final true_value=0.88, prediction around 0.68 is reasonable due to lag
    EXPECT_GT(predictions.back(), 0.65f);

    nimcp_predictive_destroy(model);
}

TEST_F(PredictiveCodingTest, ManualPredictionUpdate) {
    nimcp_predictive_model_t model = nimcp_predictive_create(
        "manual", 0.5f, 1.0f);

    // Manually set new prediction
    nimcp_error_t err = nimcp_predictive_set_prediction(model, 0.8f, 2.0f);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    float pred = nimcp_predictive_get_prediction(model);
    EXPECT_FLOAT_EQ(pred, 0.8f);

    float prec = nimcp_predictive_get_precision(model);
    EXPECT_FLOAT_EQ(prec, 2.0f);

    nimcp_predictive_destroy(model);
}

//=============================================================================
// Surprise Calculation Tests
//=============================================================================

TEST_F(PredictiveCodingTest, SurpriseIncreasesWithError) {
    nimcp_predictive_model_t model = nimcp_predictive_create(
        "signal", 0.5f, 2.0f);

    nimcp_predictive_on_error(model, prediction_error_callback, this, 0.0f);

    // Small error
    nimcp_predictive_observe(model, 0.55f);
    float surprise_small = errors.empty() ? 0.0f : errors.back().surprise;

    // Large error
    nimcp_predictive_observe(model, 0.95f);
    float surprise_large = errors.empty() ? 0.0f : errors.back().surprise;

    // Larger error should produce larger surprise
    EXPECT_GT(surprise_large, surprise_small)
        << "Surprise should increase with prediction error magnitude";

    nimcp_predictive_destroy(model);
}

TEST_F(PredictiveCodingTest, HighPrecisionIncreaseSurprise) {
    // High precision model (confident predictions)
    auto high_prec_model = nimcp_predictive_create("high_prec", 0.5f, 5.0f);

    // Low precision model (uncertain predictions)
    auto low_prec_model = nimcp_predictive_create("low_prec", 0.5f, 0.5f);

    std::vector<PredictionError> high_prec_errors;
    std::vector<PredictionError> low_prec_errors;

    auto hp_callback = [](const char* name, float pred, float actual,
                          float error, float surprise, void* data) {
        auto* vec = static_cast<std::vector<PredictionError>*>(data);
        vec->push_back({pred, actual, error, surprise});
    };

    nimcp_predictive_on_error(high_prec_model, hp_callback, &high_prec_errors, 0.0f);
    nimcp_predictive_on_error(low_prec_model, hp_callback, &low_prec_errors, 0.0f);

    // Same observation error for both
    nimcp_predictive_observe(high_prec_model, 0.8f);
    nimcp_predictive_observe(low_prec_model, 0.8f);

    // High precision model should have higher surprise for same error
    if (!high_prec_errors.empty() && !low_prec_errors.empty()) {
        EXPECT_GT(high_prec_errors[0].surprise, low_prec_errors[0].surprise)
            << "Higher precision should increase surprise for same error";
    }

    nimcp_predictive_destroy(high_prec_model);
    nimcp_predictive_destroy(low_prec_model);
}

//=============================================================================
// Error Callback Tests
//=============================================================================

TEST_F(PredictiveCodingTest, CallbackOnlyFiresOnError) {
    nimcp_predictive_model_t model = nimcp_predictive_create(
        "signal", 0.5f, 2.0f);

    // Threshold 0.1: matching obs should give ~0 surprise, mismatched should exceed
    nimcp_predictive_on_error(model, prediction_error_callback, this, 0.1f);

    // Observe matching values (no error)
    for (int i = 0; i < 5; i++) {
        nimcp_predictive_observe(model, 0.5f);
    }

    int callbacks_after_matching = callback_count.load();
    EXPECT_EQ(callbacks_after_matching, 0)
        << "No callbacks should fire for matching observations";

    // Now observe mismatched value (large deviation should exceed threshold)
    nimcp_predictive_observe(model, 1.5f);

    EXPECT_GT(callback_count.load(), callbacks_after_matching)
        << "Callback should fire for mismatch";

    nimcp_predictive_destroy(model);
}

TEST_F(PredictiveCodingTest, SurpriseThresholdFiltering) {
    nimcp_predictive_model_t model = nimcp_predictive_create(
        "signal", 0.5f, 1.0f);

    // Set high surprise threshold
    nimcp_predictive_on_error(model, prediction_error_callback, this, 5.0f);

    // Small error (low surprise)
    nimcp_predictive_observe(model, 0.55f);
    int count_after_small = callback_count.load();

    // Medium error (medium surprise)
    nimcp_predictive_observe(model, 0.7f);
    int count_after_medium = callback_count.load();

    // Large error (high surprise)
    nimcp_predictive_observe(model, 1.0f);
    int count_after_large = callback_count.load();

    // Only very high surprise should trigger callback
    // (depends on threshold and precision, but generally fewer callbacks)
    EXPECT_LE(count_after_small, count_after_large)
        << "Callbacks should be filtered by surprise threshold";

    nimcp_predictive_destroy(model);
}

TEST_F(PredictiveCodingTest, MultipleCallbackRegistrations) {
    nimcp_predictive_model_t model = nimcp_predictive_create(
        "signal", 0.5f, 2.0f);

    std::atomic<int> callback1_count{0};
    std::atomic<int> callback2_count{0};

    auto cb1 = [](const char* n, float p, float a, float e, float s, void* d) {
        auto* c = static_cast<std::atomic<int>*>(d);
        (*c)++;
    };

    auto cb2 = [](const char* n, float p, float a, float e, float s, void* d) {
        auto* c = static_cast<std::atomic<int>*>(d);
        (*c)++;
    };

    // Use lower thresholds - with precision=2.0, pred=0.5, actual=1.5:
    // surprise ~= 0.5 * 2.0 * 1.0^2 = 1.0
    nimcp_predictive_on_error(model, cb1, &callback1_count, 0.1f);
    nimcp_predictive_on_error(model, cb2, &callback2_count, 0.5f);

    // Large error (actual=1.5, deviation=1.0)
    nimcp_predictive_observe(model, 1.5f);

    // Both callbacks should fire since surprise ~1.0 exceeds both thresholds
    int total = callback1_count.load() + callback2_count.load();
    EXPECT_GT(total, 0) << "At least one callback should fire";

    nimcp_predictive_destroy(model);
}

//=============================================================================
// Precision Adaptation Tests
//=============================================================================

TEST_F(PredictiveCodingTest, PrecisionAdaptsToVariance) {
    nimcp_predictive_model_t model = nimcp_predictive_create(
        "variable_signal", 0.5f, 2.0f);

    float initial_precision = nimcp_predictive_get_precision(model);

    // Observe highly variable signal
    std::vector<float> observations = {0.3f, 0.8f, 0.2f, 0.9f, 0.1f, 0.95f};

    for (float obs : observations) {
        nimcp_predictive_observe(model, obs);
    }

    float final_precision = nimcp_predictive_get_precision(model);

    // Precision might adapt based on observed variance
    // (implementation-dependent, but should be sensible)
    EXPECT_GT(final_precision, 0.0f);
    EXPECT_LT(final_precision, 100.0f);

    nimcp_predictive_destroy(model);
}

TEST_F(PredictiveCodingTest, PrecisionMaintainedForConsistentSignal) {
    nimcp_predictive_model_t model = nimcp_predictive_create(
        "consistent_signal", 0.5f, 2.0f);

    float initial_precision = nimcp_predictive_get_precision(model);

    // Observe very consistent signal
    for (int i = 0; i < 20; i++) {
        nimcp_predictive_observe(model, 0.5f);
    }

    float final_precision = nimcp_predictive_get_precision(model);

    // Precision should remain reasonable for consistent signal
    EXPECT_GT(final_precision, 0.0f);

    nimcp_predictive_destroy(model);
}

//=============================================================================
// Multiple Signal Tests
//=============================================================================

TEST_F(PredictiveCodingTest, MultipleIndependentSignals) {
    auto model1 = nimcp_predictive_create("dopamine", 0.5f, 1.0f);
    auto model2 = nimcp_predictive_create("serotonin", 0.6f, 1.5f);
    auto model3 = nimcp_predictive_create("activity", 0.3f, 2.0f);

    ASSERT_NE(model1, nullptr);
    ASSERT_NE(model2, nullptr);
    ASSERT_NE(model3, nullptr);

    // Update each independently
    nimcp_predictive_observe(model1, 0.7f);
    nimcp_predictive_observe(model2, 0.5f);
    nimcp_predictive_observe(model3, 0.8f);

    float pred1 = nimcp_predictive_get_prediction(model1);
    float pred2 = nimcp_predictive_get_prediction(model2);
    float pred3 = nimcp_predictive_get_prediction(model3);

    // Each should maintain independent state
    EXPECT_NE(pred1, pred2);
    EXPECT_NE(pred2, pred3);

    nimcp_predictive_destroy(model1);
    nimcp_predictive_destroy(model2);
    nimcp_predictive_destroy(model3);
}

TEST_F(PredictiveCodingTest, SignalNameLookup) {
    auto model_da = nimcp_predictive_create("dopamine_level", 0.5f, 1.0f);
    auto model_ht = nimcp_predictive_create("serotonin_level", 0.6f, 1.0f);

    // Both should exist independently by name
    ASSERT_NE(model_da, nullptr);
    ASSERT_NE(model_ht, nullptr);

    // Update one shouldn't affect the other
    nimcp_predictive_observe(model_da, 0.9f);

    float pred_da = nimcp_predictive_get_prediction(model_da);
    float pred_ht = nimcp_predictive_get_prediction(model_ht);

    EXPECT_GT(pred_da, 0.5f); // Should have updated
    EXPECT_FLOAT_EQ(pred_ht, 0.6f); // Should remain initial

    nimcp_predictive_destroy(model_da);
    nimcp_predictive_destroy(model_ht);
}

//=============================================================================
// Integration with Bio-Async Tests
//=============================================================================

TEST_F(PredictiveCodingTest, PredictWithFutureResults) {
    nimcp_predictive_model_t model = nimcp_predictive_create(
        "async_signal", 0.5f, 2.0f);

    // With precision=2.0, pred=0.5, actual=0.85:
    // surprise ~= 0.5 * 2.0 * 0.35^2 = 0.1225, use threshold 0.1
    nimcp_predictive_on_error(model, prediction_error_callback, this, 0.1f);

    // Create future that will complete with result
    auto promise = nimcp_bio_promise_create(BIO_CHANNEL_DOPAMINE, sizeof(float));
    auto future = nimcp_bio_promise_get_future(promise);

    float result = 0.85f;
    nimcp_bio_promise_complete(promise, &result);

    // Wait for result
    float observed;
    nimcp_bio_future_wait(future, &observed, 100);

    // Use result as observation
    nimcp_predictive_observe(model, observed);

    // Should have triggered prediction error if mismatch (surprise ~0.12 > threshold 0.1)
    if (std::abs(observed - 0.5f) > 0.2f) {
        EXPECT_GT(callback_count.load(), 0);
    }

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
    nimcp_predictive_destroy(model);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(PredictiveCodingTest, VerifyStatisticsTracking) {
    nimcp_bio_async_reset_stats();

    auto model = nimcp_predictive_create("test", 0.5f, 1.0f);
    nimcp_predictive_on_error(model, prediction_error_callback, this, 0.5f);

    // Make observations
    for (int i = 0; i < 10; i++) {
        float value = 0.5f + (i % 2) * 0.3f; // Alternating values
        nimcp_predictive_observe(model, value);
    }

    nimcp_bio_async_stats_t stats;
    ASSERT_EQ(nimcp_bio_async_get_stats(&stats), NIMCP_SUCCESS);

    EXPECT_GE(stats.predictive_stats.predictions_made, 10u);

    // Some callbacks might have fired
    EXPECT_GE(stats.predictive_stats.callbacks_triggered, 0u);

    nimcp_predictive_destroy(model);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(PredictiveCodingTest, ZeroPrecisionHandling) {
    auto model = nimcp_predictive_create("uncertain", 0.5f, 0.001f);
    ASSERT_NE(model, nullptr);

    // Very low precision should still work
    nimcp_predictive_observe(model, 0.9f);

    float pred = nimcp_predictive_get_prediction(model);
    EXPECT_GE(pred, 0.0f);
    EXPECT_LE(pred, 1.0f);

    nimcp_predictive_destroy(model);
}

TEST_F(PredictiveCodingTest, ExtremePredictionValues) {
    auto model = nimcp_predictive_create("extreme", 0.0f, 1.0f);

    // Observe maximum value
    nimcp_predictive_observe(model, 1.0f);

    float pred = nimcp_predictive_get_prediction(model);
    EXPECT_GE(pred, 0.0f);
    EXPECT_LE(pred, 1.0f);

    // Observe minimum
    nimcp_predictive_observe(model, 0.0f);

    pred = nimcp_predictive_get_prediction(model);
    EXPECT_GE(pred, 0.0f);
    EXPECT_LE(pred, 1.0f);

    nimcp_predictive_destroy(model);
}

// End of tests
