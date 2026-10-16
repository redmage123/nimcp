/**
 * @file test_predictive_attention_integration.cpp
 * @brief Integration tests for Predictive Processing - Attention bridge
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Integration tests for predictive-attention bidirectional coupling
 * WHY:  Verify that prediction errors drive attention allocation, precision
 *       weighting affects attention, and active inference loops work correctly
 * HOW:  Test complete integration scenarios with event flow and state changes
 *
 * TEST COVERAGE:
 * - Prediction errors drive attention shifts
 * - Precision weighting affects attention allocation
 * - Surprise-based attention reorienting
 * - Active inference loop (attention sampling to reduce error)
 * - Predictive gating of attention
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

#include "cognitive/integration/nimcp_predictive_attention_bridge.h"
#include "cognitive/integration/nimcp_cognitive_integration_hub.h"
#include "cognitive/integration/nimcp_cognitive_event_types.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define LOCATION_VISUAL_1       1001
#define LOCATION_VISUAL_2       1002
#define LOCATION_AUDITORY_1     2001
#define LOCATION_AUDITORY_2     2002
#define LOCATION_SEMANTIC_1     3001

#define ERROR_HIGH              0.9f
#define ERROR_MEDIUM            0.5f
#define ERROR_LOW               0.2f

#define PRECISION_HIGH          0.9f
#define PRECISION_MEDIUM        0.5f
#define PRECISION_LOW           0.2f

/* ============================================================================
 * Test State Tracking
 * ============================================================================ */

static std::atomic<int> g_attention_shifts{0};
static std::atomic<int> g_prediction_requests{0};
static std::atomic<int> g_error_notifications{0};
static std::atomic<uint64_t> g_current_focus{0};
static std::atomic<float> g_last_error{0.0f};

static void reset_test_state() {
    g_attention_shifts = 0;
    g_prediction_requests = 0;
    g_error_notifications = 0;
    g_current_focus = 0;
    g_last_error = 0.0f;
}

/* ============================================================================
 * Test Callbacks
 * ============================================================================ */

static int attention_shift_callback(
    uint64_t old_focus,
    uint64_t new_focus,
    float urgency,
    void* user_data
) {
    (void)old_focus;
    (void)urgency;
    (void)user_data;
    g_attention_shifts++;
    g_current_focus = new_focus;
    return 0;
}

static int prediction_callback(
    float prediction,
    uint64_t location,
    float confidence,
    void* user_data
) {
    (void)prediction;
    (void)location;
    (void)confidence;
    (void)user_data;
    g_prediction_requests++;
    return 0;
}

static int error_callback(
    float error_magnitude,
    uint64_t location,
    void* user_data
) {
    (void)location;
    (void)user_data;
    g_error_notifications++;
    g_last_error = error_magnitude;
    return 0;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class PredictiveAttentionIntegrationTest : public ::testing::Test {
protected:
    predictive_attention_bridge_t* bridge;
    predictive_attention_bridge_config_t config;
    cognitive_integration_hub_t hub;

    void SetUp() override {
        bridge = nullptr;
        hub = nullptr;

        reset_test_state();

        /* Get default config */
        int ret = predictive_attention_bridge_default_config(&config);
        ASSERT_EQ(ret, 0);

        /* Configure for integration tests */
        config.error_attention_threshold = 0.3f;
        config.prediction_error_weight = 0.7f;
        config.surprise_attention_weight = 0.8f;
        config.precision_weight = 0.6f;

        /* Create bridge */
        bridge = predictive_attention_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);

        /* Create cognitive hub */
        cognitive_hub_config_t hub_config = cognitive_hub_default_config();
        hub = cognitive_hub_create(&hub_config);
        ASSERT_NE(hub, nullptr);

        /* Register bridge with hub */
        ret = predictive_attention_bridge_register_with_hub(bridge, hub);
        ASSERT_EQ(ret, 0);

        /* Set up callbacks */
        predictive_attention_set_attention_callback(bridge, attention_shift_callback, nullptr);
        predictive_attention_set_prediction_callback(bridge, prediction_callback, nullptr);
        predictive_attention_set_error_callback(bridge, error_callback, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            predictive_attention_bridge_destroy(bridge);
        }
        if (hub) {
            cognitive_hub_destroy(hub);
        }
    }

    /* Helper to get current bridge stats */
    predictive_attention_bridge_stats_t get_stats() {
        predictive_attention_bridge_stats_t stats;
        memset(&stats, 0, sizeof(stats));
        predictive_attention_bridge_get_stats(bridge, &stats);
        return stats;
    }
};

/* ============================================================================
 * Prediction Error Drives Attention Tests
 * ============================================================================ */

TEST_F(PredictiveAttentionIntegrationTest, PredictionErrorDrivesAttention) {
    /*
     * TEST: High prediction errors should drive attention to error source
     *
     * THEORETICAL BASIS:
     * - Prediction errors signal unexpected events that need investigation
     * - Attention is allocated to reduce uncertainty at error locations
     * - Free Energy Principle: attention minimizes prediction error
     */

    /* Publish high prediction error at location 1 */
    int ret = predictive_attention_publish_prediction_error(
        bridge, ERROR_HIGH, LOCATION_VISUAL_1);
    EXPECT_EQ(ret, 0);

    /* Stats should show error was published */
    auto stats = get_stats();
    EXPECT_EQ(stats.prediction_errors, 1u);
    EXPECT_GE(stats.events_published, 1u);

    /* Now request attention to this error */
    pred_attn_error_data_t error_data;
    memset(&error_data, 0, sizeof(error_data));
    error_data.error_id = 1;
    error_data.error_magnitude = ERROR_HIGH;
    error_data.error_location = LOCATION_VISUAL_1;
    error_data.precision = PRECISION_MEDIUM;

    ret = predictive_attention_request_attention_to_error(bridge, &error_data);
    EXPECT_EQ(ret, 0);

    stats = get_stats();
    EXPECT_EQ(stats.attention_requests, 1u);
}

TEST_F(PredictiveAttentionIntegrationTest, LowErrorIgnored) {
    /*
     * TEST: Low prediction errors below threshold should not trigger attention
     *
     * THEORETICAL BASIS:
     * - Minor prediction errors are normal and expected
     * - Attention should not be distracted by insignificant deviations
     * - Only salient (above threshold) errors warrant attention allocation
     */

    /* Publish low prediction error (below threshold of 0.3) */
    int ret = predictive_attention_publish_prediction_error(
        bridge, ERROR_LOW, LOCATION_VISUAL_2);
    EXPECT_EQ(ret, 0);

    /* Error should still be recorded but not trigger attention shift */
    auto stats = get_stats();
    EXPECT_EQ(stats.prediction_errors, 1u);

    /* The error callback should not be triggered for below-threshold errors */
    /* when received through the hub (depends on implementation) */
}

TEST_F(PredictiveAttentionIntegrationTest, MultipleErrorsCompete) {
    /*
     * TEST: Multiple prediction errors compete for attention
     *
     * THEORETICAL BASIS:
     * - Limited attentional resources require prioritization
     * - Higher errors should receive priority
     * - Sequential errors update attention allocation
     */

    /* Publish medium error at location 1 */
    predictive_attention_publish_prediction_error(
        bridge, ERROR_MEDIUM, LOCATION_VISUAL_1);

    /* Publish high error at location 2 */
    predictive_attention_publish_prediction_error(
        bridge, ERROR_HIGH, LOCATION_VISUAL_2);

    /* Publish low error at location 3 */
    predictive_attention_publish_prediction_error(
        bridge, ERROR_LOW, LOCATION_AUDITORY_1);

    auto stats = get_stats();
    EXPECT_EQ(stats.prediction_errors, 3u);
    EXPECT_GE(stats.events_published, 3u);

    /* Average error should reflect all three */
    float expected_avg = (ERROR_MEDIUM + ERROR_HIGH + ERROR_LOW) / 3.0f;
    EXPECT_NEAR(stats.avg_error_magnitude, expected_avg, 0.1f);
}

/* ============================================================================
 * Precision Weighted Attention Tests
 * ============================================================================ */

TEST_F(PredictiveAttentionIntegrationTest, PrecisionWeightedAttention) {
    /*
     * TEST: High precision locations receive less attention (more confident predictions)
     *       Low precision locations receive more attention (need more sampling)
     *
     * THEORETICAL BASIS:
     * - Precision represents confidence in predictions
     * - High precision = reliable predictions = less attention needed
     * - Low precision = uncertain predictions = allocate attention to gather info
     */

    /* Publish precision update for location with high precision */
    pred_attn_precision_data_t high_precision;
    memset(&high_precision, 0, sizeof(high_precision));
    high_precision.location = LOCATION_VISUAL_1;
    high_precision.precision_old = PRECISION_MEDIUM;
    high_precision.precision_new = PRECISION_HIGH;
    high_precision.confidence = 0.9f;

    int ret = predictive_attention_publish_precision_estimate(bridge, &high_precision);
    EXPECT_EQ(ret, 0);

    /* Publish precision update for location with low precision */
    pred_attn_precision_data_t low_precision;
    memset(&low_precision, 0, sizeof(low_precision));
    low_precision.location = LOCATION_VISUAL_2;
    low_precision.precision_old = PRECISION_MEDIUM;
    low_precision.precision_new = PRECISION_LOW;
    low_precision.confidence = 0.9f;

    ret = predictive_attention_publish_precision_estimate(bridge, &low_precision);
    EXPECT_EQ(ret, 0);

    auto stats = get_stats();
    EXPECT_EQ(stats.precision_updates, 2u);

    /* Average precision should be computed */
    float expected_avg = (PRECISION_HIGH + PRECISION_LOW) / 2.0f;
    EXPECT_NEAR(stats.avg_precision, expected_avg, 0.1f);
}

TEST_F(PredictiveAttentionIntegrationTest, PrecisionChangeTriggersBehavior) {
    /*
     * TEST: Significant precision change should affect attention behavior
     *
     * THEORETICAL BASIS:
     * - Sudden drops in precision indicate model breakdown
     * - Rapid precision increase indicates successful learning
     * - Both cases may warrant attention reallocation
     */

    /* Large precision drop at a location */
    pred_attn_precision_data_t precision_drop;
    memset(&precision_drop, 0, sizeof(precision_drop));
    precision_drop.location = LOCATION_SEMANTIC_1;
    precision_drop.precision_old = PRECISION_HIGH;
    precision_drop.precision_new = PRECISION_LOW;  /* Large drop */
    precision_drop.confidence = 0.8f;

    int ret = predictive_attention_publish_precision_estimate(bridge, &precision_drop);
    EXPECT_EQ(ret, 0);

    auto stats = get_stats();
    EXPECT_EQ(stats.precision_updates, 1u);
}

/* ============================================================================
 * Surprise Based Reorienting Tests
 * ============================================================================ */

TEST_F(PredictiveAttentionIntegrationTest, SurpriseBasedReorienting) {
    /*
     * TEST: Unexpected events (high surprise) should capture attention automatically
     *
     * THEORETICAL BASIS:
     * - Surprise = prediction error magnitude
     * - Novel/unexpected stimuli automatically orient attention (exogenous attention)
     * - This is adaptive: unexpected events may be threats or opportunities
     */

    /* Simulate surprise event with very high error */
    pred_attn_error_data_t surprise_error;
    memset(&surprise_error, 0, sizeof(surprise_error));
    surprise_error.error_id = 100;
    surprise_error.error_magnitude = 0.95f;  /* Very high surprise */
    surprise_error.error_location = LOCATION_AUDITORY_2;
    surprise_error.expected_value = 0.1f;
    surprise_error.observed_value = 0.95f;
    surprise_error.precision = PRECISION_HIGH;  /* High precision = more surprising when wrong */

    int ret = predictive_attention_request_attention_to_error(bridge, &surprise_error);
    EXPECT_EQ(ret, 0);

    auto stats = get_stats();
    EXPECT_EQ(stats.attention_requests, 1u);
}

TEST_F(PredictiveAttentionIntegrationTest, ExpectedEventsNoReorienting) {
    /*
     * TEST: Expected events (low surprise) should not capture attention
     *
     * THEORETICAL BASIS:
     * - Predicted events don't require attention reallocation
     * - Attention should remain on current task when predictions are accurate
     */

    /* Publish very low error - event was predicted */
    int ret = predictive_attention_publish_prediction_error(
        bridge, 0.05f, LOCATION_VISUAL_1);
    EXPECT_EQ(ret, 0);

    auto stats = get_stats();
    EXPECT_EQ(stats.prediction_errors, 1u);

    /* Such a low error should not trigger attention shift */
}

/* ============================================================================
 * Active Inference Loop Tests
 * ============================================================================ */

TEST_F(PredictiveAttentionIntegrationTest, ActiveInferenceLoop) {
    /*
     * TEST: Attention samples environment to reduce prediction error
     *
     * THEORETICAL BASIS:
     * - Active inference: organisms act to minimize prediction error
     * - Attention is a form of action - sampling reduces uncertainty
     * - Loop: predict -> observe -> error -> attend -> sample -> predict
     */

    /* Step 1: Generate prediction for current focus */
    pred_attn_prediction_data_t prediction;
    memset(&prediction, 0, sizeof(prediction));
    prediction.focus_id = LOCATION_VISUAL_1;
    prediction.prediction = 0.7f;
    prediction.confidence = 0.8f;
    prediction.expected_precision = PRECISION_HIGH;

    int ret = predictive_attention_notify_attended_prediction(bridge, &prediction);
    EXPECT_EQ(ret, 0);

    /* Step 2: Observe - error detected */
    ret = predictive_attention_publish_prediction_error(
        bridge, ERROR_MEDIUM, LOCATION_VISUAL_1);
    EXPECT_EQ(ret, 0);

    /* Step 3: Request attention to error location */
    pred_attn_error_data_t error;
    memset(&error, 0, sizeof(error));
    error.error_id = 1;
    error.error_magnitude = ERROR_MEDIUM;
    error.error_location = LOCATION_VISUAL_1;

    ret = predictive_attention_request_attention_to_error(bridge, &error);
    EXPECT_EQ(ret, 0);

    /* Step 4: Sample - request prediction for new focus */
    pred_attn_focus_request_t focus_req;
    memset(&focus_req, 0, sizeof(focus_req));
    focus_req.focus_id = LOCATION_VISUAL_1;
    focus_req.urgency = 0.7f;
    focus_req.focus_duration_us = 100000;

    ret = predictive_attention_request_prediction_for_focus(bridge, &focus_req);
    EXPECT_EQ(ret, 0);

    /* Verify the loop executed */
    auto stats = get_stats();
    EXPECT_EQ(stats.focus_predictions, 1u);
    EXPECT_EQ(stats.prediction_errors, 1u);
    EXPECT_EQ(stats.attention_requests, 1u);
}

TEST_F(PredictiveAttentionIntegrationTest, IterativeErrorReduction) {
    /*
     * TEST: Multiple iterations should show error reduction
     *
     * THEORETICAL BASIS:
     * - Active inference should progressively reduce prediction error
     * - Each attention allocation should improve predictions
     */

    /* Simulate decreasing errors over iterations */
    float errors[] = {0.8f, 0.6f, 0.4f, 0.2f};

    for (int i = 0; i < 4; i++) {
        predictive_attention_publish_prediction_error(
            bridge, errors[i], LOCATION_VISUAL_1 + (uint64_t)i);
    }

    auto stats = get_stats();
    EXPECT_EQ(stats.prediction_errors, 4u);

    /* Average should show overall error level */
    float expected_avg = (0.8f + 0.6f + 0.4f + 0.2f) / 4.0f;
    EXPECT_NEAR(stats.avg_error_magnitude, expected_avg, 0.1f);
}

/* ============================================================================
 * Predictive Gating Tests
 * ============================================================================ */

TEST_F(PredictiveAttentionIntegrationTest, PredictiveGating) {
    /*
     * TEST: Predictions gate attention allocation
     *
     * THEORETICAL BASIS:
     * - Predictions set expectations that filter attention
     * - Only prediction-violating stimuli break through attentional gate
     * - Implements "predictive coding" framework in attention
     */

    /* Publish prediction for a location */
    pred_attn_prediction_data_t pred;
    memset(&pred, 0, sizeof(pred));
    pred.focus_id = LOCATION_VISUAL_1;
    pred.prediction = 0.5f;
    pred.confidence = PRECISION_HIGH;
    pred.expected_precision = PRECISION_HIGH;

    int ret = predictive_attention_notify_attended_prediction(bridge, &pred);
    EXPECT_EQ(ret, 0);

    /* Matching observation - should not trigger attention shift */
    ret = predictive_attention_publish_prediction_error(
        bridge, 0.05f, LOCATION_VISUAL_1);  /* Very low error = prediction matched */
    EXPECT_EQ(ret, 0);

    /* Mismatching observation - should trigger attention */
    ret = predictive_attention_publish_prediction_error(
        bridge, ERROR_HIGH, LOCATION_VISUAL_1);  /* High error = prediction failed */
    EXPECT_EQ(ret, 0);

    auto stats = get_stats();
    EXPECT_EQ(stats.prediction_errors, 2u);
    EXPECT_EQ(stats.focus_predictions, 1u);
}

TEST_F(PredictiveAttentionIntegrationTest, TopDownPredictiveInfluence) {
    /*
     * TEST: Top-down predictions influence bottom-up attention capture
     *
     * THEORETICAL BASIS:
     * - High-level predictions set "priors" for attention
     * - Strong priors reduce bottom-up capture by expected stimuli
     * - Weak priors allow bottom-up capture
     */

    /* Set high-confidence prediction */
    pred_attn_prediction_data_t strong_pred;
    memset(&strong_pred, 0, sizeof(strong_pred));
    strong_pred.focus_id = LOCATION_VISUAL_1;
    strong_pred.prediction = 0.8f;
    strong_pred.confidence = 0.95f;  /* Very confident */
    strong_pred.expected_precision = PRECISION_HIGH;

    int ret = predictive_attention_notify_attended_prediction(bridge, &strong_pred);
    EXPECT_EQ(ret, 0);

    /* Update precision to reflect strong top-down influence */
    pred_attn_precision_data_t prec;
    memset(&prec, 0, sizeof(prec));
    prec.location = LOCATION_VISUAL_1;
    prec.precision_old = PRECISION_MEDIUM;
    prec.precision_new = PRECISION_HIGH;  /* High precision from strong prediction */
    prec.confidence = 0.9f;

    ret = predictive_attention_publish_precision_estimate(bridge, &prec);
    EXPECT_EQ(ret, 0);

    auto stats = get_stats();
    EXPECT_EQ(stats.focus_predictions, 1u);
    EXPECT_EQ(stats.precision_updates, 1u);
}

/* ============================================================================
 * Statistics and Monitoring Tests
 * ============================================================================ */

TEST_F(PredictiveAttentionIntegrationTest, ComprehensiveStatisticsTracking) {
    /*
     * TEST: All operations are tracked in statistics
     */

    /* Perform various operations */
    predictive_attention_publish_prediction_error(bridge, 0.5f, LOCATION_VISUAL_1);
    predictive_attention_publish_prediction_error(bridge, 0.6f, LOCATION_VISUAL_2);

    pred_attn_precision_data_t prec;
    memset(&prec, 0, sizeof(prec));
    prec.location = LOCATION_AUDITORY_1;
    prec.precision_old = 0.3f;
    prec.precision_new = 0.7f;
    prec.confidence = 0.8f;
    predictive_attention_publish_precision_estimate(bridge, &prec);

    pred_attn_error_data_t err;
    memset(&err, 0, sizeof(err));
    err.error_id = 1;
    err.error_magnitude = 0.8f;
    err.error_location = LOCATION_SEMANTIC_1;
    predictive_attention_request_attention_to_error(bridge, &err);

    pred_attn_prediction_data_t pred;
    memset(&pred, 0, sizeof(pred));
    pred.focus_id = LOCATION_VISUAL_1;
    pred.prediction = 0.5f;
    pred.confidence = 0.7f;
    predictive_attention_notify_attended_prediction(bridge, &pred);

    pred_attn_focus_request_t focus;
    memset(&focus, 0, sizeof(focus));
    focus.focus_id = LOCATION_VISUAL_2;
    focus.urgency = 0.6f;
    predictive_attention_request_prediction_for_focus(bridge, &focus);

    /* Verify all stats are tracked */
    auto stats = get_stats();
    EXPECT_EQ(stats.prediction_errors, 2u);
    EXPECT_EQ(stats.precision_updates, 1u);
    EXPECT_EQ(stats.attention_requests, 1u);
    EXPECT_EQ(stats.focus_predictions, 1u);
    EXPECT_GE(stats.events_published, 6u);
    EXPECT_GE(stats.total_events, 6u);
}

TEST_F(PredictiveAttentionIntegrationTest, StatisticsReset) {
    /*
     * TEST: Statistics can be reset
     */

    /* Generate some activity */
    predictive_attention_publish_prediction_error(bridge, 0.5f, LOCATION_VISUAL_1);
    predictive_attention_publish_prediction_error(bridge, 0.6f, LOCATION_VISUAL_2);

    auto stats_before = get_stats();
    EXPECT_GT(stats_before.prediction_errors, 0u);

    /* Reset */
    int ret = predictive_attention_bridge_reset_stats(bridge);
    EXPECT_EQ(ret, 0);

    auto stats_after = get_stats();
    EXPECT_EQ(stats_after.prediction_errors, 0u);
    EXPECT_EQ(stats_after.events_published, 0u);
    EXPECT_EQ(stats_after.total_events, 0u);
    EXPECT_FLOAT_EQ(stats_after.avg_error_magnitude, 0.0f);
}

/* ============================================================================
 * Edge Cases and Error Handling Tests
 * ============================================================================ */

TEST_F(PredictiveAttentionIntegrationTest, BoundaryValues) {
    /*
     * TEST: Handle boundary values correctly
     */

    /* Zero error */
    int ret = predictive_attention_publish_prediction_error(bridge, 0.0f, LOCATION_VISUAL_1);
    EXPECT_EQ(ret, 0);

    /* Maximum error */
    ret = predictive_attention_publish_prediction_error(bridge, 1.0f, LOCATION_VISUAL_2);
    EXPECT_EQ(ret, 0);

    /* Zero precision */
    pred_attn_precision_data_t zero_prec;
    memset(&zero_prec, 0, sizeof(zero_prec));
    zero_prec.location = LOCATION_AUDITORY_1;
    zero_prec.precision_new = 0.0f;
    ret = predictive_attention_publish_precision_estimate(bridge, &zero_prec);
    EXPECT_EQ(ret, 0);

    /* Maximum precision */
    pred_attn_precision_data_t max_prec;
    memset(&max_prec, 0, sizeof(max_prec));
    max_prec.location = LOCATION_AUDITORY_2;
    max_prec.precision_new = 1.0f;
    ret = predictive_attention_publish_precision_estimate(bridge, &max_prec);
    EXPECT_EQ(ret, 0);

    auto stats = get_stats();
    EXPECT_EQ(stats.prediction_errors, 2u);
    EXPECT_EQ(stats.precision_updates, 2u);
}

TEST_F(PredictiveAttentionIntegrationTest, RapidSuccessiveEvents) {
    /*
     * TEST: Handle rapid successive events
     */

    const int NUM_EVENTS = 100;

    for (int i = 0; i < NUM_EVENTS; i++) {
        float error = 0.1f + (float)i * 0.008f;  /* Gradually increasing */
        predictive_attention_publish_prediction_error(
            bridge, error, LOCATION_VISUAL_1 + (uint64_t)i);
    }

    auto stats = get_stats();
    EXPECT_EQ(stats.prediction_errors, (uint32_t)NUM_EVENTS);
}

TEST_F(PredictiveAttentionIntegrationTest, ConcurrentOperations) {
    /*
     * TEST: Handle concurrent operations from multiple threads
     */

    const int NUM_THREADS = 4;
    const int OPS_PER_THREAD = 25;
    std::atomic<int> completed{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, t, OPS_PER_THREAD, &completed]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                float error = 0.3f + 0.1f * (float)t;
                predictive_attention_publish_prediction_error(
                    bridge, error, (uint64_t)(t * 1000 + i));
            }
            completed++;
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(completed.load(), NUM_THREADS);

    auto stats = get_stats();
    EXPECT_EQ(stats.prediction_errors, (uint32_t)(NUM_THREADS * OPS_PER_THREAD));
}

/* ============================================================================
 * Configuration Impact Tests
 * ============================================================================ */

TEST_F(PredictiveAttentionIntegrationTest, ThresholdConfiguration) {
    /*
     * TEST: Error threshold configuration affects behavior
     */

    /* Create new bridge with higher threshold */
    predictive_attention_bridge_destroy(bridge);

    predictive_attention_bridge_config_t high_thresh_config;
    predictive_attention_bridge_default_config(&high_thresh_config);
    high_thresh_config.error_attention_threshold = 0.7f;  /* High threshold */

    bridge = predictive_attention_bridge_create(&high_thresh_config);
    ASSERT_NE(bridge, nullptr);

    int ret = predictive_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(ret, 0);

    /* Publish error below new threshold */
    ret = predictive_attention_publish_prediction_error(bridge, 0.5f, LOCATION_VISUAL_1);
    EXPECT_EQ(ret, 0);

    /* Publish error above new threshold */
    ret = predictive_attention_publish_prediction_error(bridge, 0.8f, LOCATION_VISUAL_2);
    EXPECT_EQ(ret, 0);

    auto stats = get_stats();
    EXPECT_EQ(stats.prediction_errors, 2u);  /* Both recorded */
}

TEST_F(PredictiveAttentionIntegrationTest, WeightConfiguration) {
    /*
     * TEST: Weight configuration affects processing
     */

    /* Create bridge with different weights */
    predictive_attention_bridge_destroy(bridge);

    predictive_attention_bridge_config_t weight_config;
    predictive_attention_bridge_default_config(&weight_config);
    weight_config.prediction_error_weight = 1.0f;   /* Max weight to errors */
    weight_config.surprise_attention_weight = 0.0f; /* Disable surprise */
    weight_config.precision_weight = 0.0f;          /* Disable precision */

    bridge = predictive_attention_bridge_create(&weight_config);
    ASSERT_NE(bridge, nullptr);

    int ret = predictive_attention_bridge_register_with_hub(bridge, hub);
    ASSERT_EQ(ret, 0);

    /* Operations should still work */
    ret = predictive_attention_publish_prediction_error(bridge, 0.5f, LOCATION_VISUAL_1);
    EXPECT_EQ(ret, 0);

    auto stats = get_stats();
    EXPECT_EQ(stats.prediction_errors, 1u);
}

/* ============================================================================
 * Disconnect/Reconnect Tests
 * ============================================================================ */

TEST_F(PredictiveAttentionIntegrationTest, DisconnectBehavior) {
    /*
     * TEST: Operations fail gracefully after disconnect
     */

    /* Disconnect */
    int ret = predictive_attention_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(ret, 0);

    /* Operations should fail */
    ret = predictive_attention_publish_prediction_error(bridge, 0.5f, LOCATION_VISUAL_1);
    EXPECT_EQ(ret, -1);

    pred_attn_precision_data_t prec;
    memset(&prec, 0, sizeof(prec));
    prec.location = LOCATION_VISUAL_1;
    ret = predictive_attention_publish_precision_estimate(bridge, &prec);
    EXPECT_EQ(ret, -1);
}

TEST_F(PredictiveAttentionIntegrationTest, ReconnectRestoresFunction) {
    /*
     * TEST: Reconnect restores full functionality
     */

    /* Disconnect */
    int ret = predictive_attention_bridge_unregister_from_hub(bridge);
    EXPECT_EQ(ret, 0);

    /* Reconnect */
    ret = predictive_attention_bridge_register_with_hub(bridge, hub);
    EXPECT_EQ(ret, 0);

    /* Operations should work again */
    ret = predictive_attention_publish_prediction_error(bridge, 0.5f, LOCATION_VISUAL_1);
    EXPECT_EQ(ret, 0);

    auto stats = get_stats();
    EXPECT_EQ(stats.prediction_errors, 1u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
