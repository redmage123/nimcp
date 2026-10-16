/**
 * @file test_nimcp_insula_adapter.cpp
 * @brief Unit tests for nimcp_insula_adapter.c
 *
 * WHAT: Comprehensive unit tests for the Insula adapter
 * WHY:  Ensure correct integration of interoception, emotional awareness, and social emotions
 * HOW:  Use Google Test framework to test lifecycle, interoception, emotional processing,
 *       social emotions, somatic markers, and integration cycles.
 *
 * COVERAGE TARGET: 100%
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "core/brain/regions/insula/nimcp_insula_adapter.h"

// Test Fixture for Insula Adapter
class InsulaAdapterTest : public ::testing::Test {
protected:
    insula_adapter_t* adapter;
    insula_config_t config;

    void SetUp() override {
        config = insula_default_config();
        adapter = insula_create(&config);
        ASSERT_NE(nullptr, adapter) << "Failed to create Insula adapter";
    }

    void TearDown() override {
        insula_destroy(adapter);
        adapter = nullptr;
    }

    // Helper to create interoceptive signal
    insula_intero_signal_t make_intero_signal(insula_intero_channel_t channel,
                                               float intensity,
                                               float reliability = 0.8f) {
        insula_intero_signal_t signal;
        signal.channel = channel;
        signal.intensity = intensity;
        signal.rate_of_change = 0.0f;
        signal.reliability = reliability;
        signal.timestamp_ms = 0.0;
        return signal;
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(InsulaAdapterTest, DefaultConfigHasReasonableValues) {
    insula_config_t default_config = insula_default_config();

    EXPECT_EQ(default_config.interoception_channels, (uint32_t)INSULA_DEFAULT_INTEROCEPTION_CHANNELS);
    EXPECT_EQ(default_config.emotion_dimensions, (uint32_t)INSULA_DEFAULT_EMOTION_DIMENSIONS);
    EXPECT_EQ(default_config.social_emotion_types, (uint32_t)INSULA_DEFAULT_SOCIAL_EMOTION_TYPES);
    EXPECT_GT(default_config.interoception_sensitivity, 0.0f);
    EXPECT_LE(default_config.interoception_sensitivity, 1.0f);
    EXPECT_TRUE(default_config.enable_cardiac_awareness);
    EXPECT_TRUE(default_config.enable_disgust_processing);
    EXPECT_TRUE(default_config.enable_empathy_processing);
}

TEST_F(InsulaAdapterTest, CreateWithNullConfigUsesDefaults) {
    insula_adapter_t* adapter_null = insula_create(NULL);
    ASSERT_NE(nullptr, adapter_null);

    insula_config_t retrieved;
    EXPECT_TRUE(insula_get_config(adapter_null, &retrieved));
    EXPECT_EQ(retrieved.interoception_channels, (uint32_t)INSULA_DEFAULT_INTEROCEPTION_CHANNELS);

    insula_destroy(adapter_null);
}

TEST_F(InsulaAdapterTest, DestroyNullDoesNotCrash) {
    insula_destroy(NULL);
    // Should not crash
}

TEST_F(InsulaAdapterTest, ResetClearsState) {
    // Process some signals first
    insula_intero_signal_t signal = make_intero_signal(INTERO_CHANNEL_CARDIAC, 0.8f);
    EXPECT_TRUE(insula_update_interoception(adapter, &signal));

    // Reset
    EXPECT_TRUE(insula_reset(adapter));

    // Status should be idle after reset
    EXPECT_EQ(insula_get_status(adapter), INSULA_STATUS_IDLE);
    EXPECT_EQ(insula_get_last_error(adapter), INSULA_ERROR_NONE);
}

TEST_F(InsulaAdapterTest, ResetNullReturnsFalse) {
    EXPECT_FALSE(insula_reset(NULL));
}

// ============================================================================
// INTEROCEPTION TESTS
// ============================================================================

TEST_F(InsulaAdapterTest, UpdateInteroceptionSuccess) {
    insula_intero_signal_t signal = make_intero_signal(INTERO_CHANNEL_CARDIAC, 0.7f);
    EXPECT_TRUE(insula_update_interoception(adapter, &signal));
}

TEST_F(InsulaAdapterTest, UpdateInteroceptionNullFails) {
    EXPECT_FALSE(insula_update_interoception(NULL, NULL));
    EXPECT_FALSE(insula_update_interoception(adapter, NULL));
}

TEST_F(InsulaAdapterTest, UpdateInteroceptionInvalidChannelFails) {
    insula_intero_signal_t signal;
    signal.channel = (insula_intero_channel_t)(INTERO_CHANNEL_COUNT + 1);  // Invalid
    signal.intensity = 0.5f;
    signal.reliability = 0.8f;
    signal.timestamp_ms = 0.0;

    EXPECT_FALSE(insula_update_interoception(adapter, &signal));
}

TEST_F(InsulaAdapterTest, UpdateInteroceptionBatchSuccess) {
    insula_intero_signal_t signals[3];
    signals[0] = make_intero_signal(INTERO_CHANNEL_CARDIAC, 0.6f);
    signals[1] = make_intero_signal(INTERO_CHANNEL_RESPIRATORY, 0.5f);
    signals[2] = make_intero_signal(INTERO_CHANNEL_THERMAL, 0.7f);

    EXPECT_TRUE(insula_update_interoception_batch(adapter, signals, 3));
}

TEST_F(InsulaAdapterTest, GetBodyStateSuccess) {
    // Update some channels
    insula_intero_signal_t cardiac = make_intero_signal(INTERO_CHANNEL_CARDIAC, 0.6f);
    insula_intero_signal_t resp = make_intero_signal(INTERO_CHANNEL_RESPIRATORY, 0.5f);
    insula_update_interoception(adapter, &cardiac);
    insula_update_interoception(adapter, &resp);

    insula_body_state_t state;
    EXPECT_TRUE(insula_get_body_state(adapter, &state));

    // Body state should have reasonable values
    EXPECT_GT(state.heart_rate, 40.0f);
    EXPECT_LT(state.heart_rate, 200.0f);
    EXPECT_GT(state.respiratory_rate, 5.0f);
    EXPECT_LT(state.respiratory_rate, 40.0f);
}

TEST_F(InsulaAdapterTest, SetInteroceptiveSensitivitySuccess) {
    // Set all channels
    EXPECT_TRUE(insula_set_interoceptive_sensitivity(adapter, -1, 0.8f));

    // Set specific channel
    EXPECT_TRUE(insula_set_interoceptive_sensitivity(adapter, INTERO_CHANNEL_CARDIAC, 0.9f));
}

TEST_F(InsulaAdapterTest, SetInteroceptiveSensitivityClampsValues) {
    // Sensitivity should be clamped to [0, 1]
    EXPECT_TRUE(insula_set_interoceptive_sensitivity(adapter, -1, 1.5f));
    EXPECT_TRUE(insula_set_interoceptive_sensitivity(adapter, -1, -0.5f));
}

TEST_F(InsulaAdapterTest, HomeostatiAlarmOnExtreme) {
    // Extreme values should trigger alarm
    insula_intero_signal_t extreme = make_intero_signal(INTERO_CHANNEL_PAIN, 0.95f);
    EXPECT_TRUE(insula_update_interoception(adapter, &extreme));

    // Stats should show homeostatic alarm
    insula_stats_t stats;
    EXPECT_TRUE(insula_get_stats(adapter, &stats));
    EXPECT_GE(stats.homeostatic_alarms, 1u);
}

// ============================================================================
// EMOTIONAL AWARENESS TESTS
// ============================================================================

TEST_F(InsulaAdapterTest, ProcessEmotionSuccess) {
    EXPECT_TRUE(insula_process_emotion(adapter, 0.5f, 0.3f, "test_source"));
}

TEST_F(InsulaAdapterTest, ProcessEmotionNullAdapterFails) {
    EXPECT_FALSE(insula_process_emotion(NULL, 0.5f, 0.3f, "test"));
}

TEST_F(InsulaAdapterTest, ProcessEmotionNullSourceOk) {
    // NULL source should still work
    EXPECT_TRUE(insula_process_emotion(adapter, 0.5f, 0.3f, NULL));
}

TEST_F(InsulaAdapterTest, GetEmotionalStateSuccess) {
    // Process some emotion
    insula_process_emotion(adapter, 0.6f, 0.4f, "test");

    insula_emotional_state_t state;
    EXPECT_TRUE(insula_get_emotional_state(adapter, &state));

    // State should reflect positive valence from input
    // Note: exact value depends on integration
    EXPECT_GE(state.valence, -1.0f);
    EXPECT_LE(state.valence, 1.0f);
    EXPECT_GE(state.arousal, -1.0f);
    EXPECT_LE(state.arousal, 1.0f);
}

TEST_F(InsulaAdapterTest, EmotionalStateHasMetrics) {
    insula_emotional_state_t state;
    EXPECT_TRUE(insula_get_emotional_state(adapter, &state));

    // Meta-emotional metrics should be valid
    EXPECT_GE(state.emotional_clarity, 0.0f);
    EXPECT_LE(state.emotional_clarity, 1.0f);
    EXPECT_GE(state.emotional_stability, 0.0f);
    EXPECT_LE(state.emotional_stability, 1.0f);
}

// ============================================================================
// SOMATIC MARKER TESTS
// ============================================================================

TEST_F(InsulaAdapterTest, CreateSomaticMarkerSuccess) {
    EXPECT_TRUE(insula_create_somatic_marker(adapter, 42, 0.8f));
}

TEST_F(InsulaAdapterTest, QuerySomaticMarkerSuccess) {
    // Create marker
    EXPECT_TRUE(insula_create_somatic_marker(adapter, 42, 0.75f));

    // Query it
    float valence, confidence;
    EXPECT_TRUE(insula_query_somatic_marker(adapter, 42, &valence, &confidence));
    EXPECT_NEAR(valence, 0.75f, 0.1f);
    EXPECT_GT(confidence, 0.0f);
}

TEST_F(InsulaAdapterTest, QuerySomaticMarkerNotFound) {
    float valence, confidence;
    EXPECT_FALSE(insula_query_somatic_marker(adapter, 99999, &valence, &confidence));
}

TEST_F(InsulaAdapterTest, UpdateSomaticMarkerOnDuplicate) {
    // Create marker
    EXPECT_TRUE(insula_create_somatic_marker(adapter, 42, 0.8f));

    // Create again with different valence (should update)
    EXPECT_TRUE(insula_create_somatic_marker(adapter, 42, -0.5f));

    // Query should show updated (blended) valence
    float valence, confidence;
    EXPECT_TRUE(insula_query_somatic_marker(adapter, 42, &valence, &confidence));
    // Valence should be between old and new due to learning
    EXPECT_LT(valence, 0.8f);
}

// ============================================================================
// DISGUST PROCESSING TESTS
// ============================================================================

TEST_F(InsulaAdapterTest, ProcessDisgustCoreSuccess) {
    float response = insula_process_disgust(adapter, DISGUST_CORE, 0.8f, false);
    EXPECT_GT(response, 0.0f);
    EXPECT_LE(response, 1.0f);
}

TEST_F(InsulaAdapterTest, ProcessDisgustMoralSuccess) {
    float response = insula_process_disgust(adapter, DISGUST_MORAL, 0.7f, true);
    EXPECT_GT(response, 0.0f);
    EXPECT_LE(response, 1.0f);
}

TEST_F(InsulaAdapterTest, ProcessDisgustNullReturnsZero) {
    float response = insula_process_disgust(NULL, DISGUST_CORE, 0.5f, false);
    EXPECT_EQ(response, 0.0f);
}

TEST_F(InsulaAdapterTest, DisgustUpdatesEmotionalState) {
    insula_process_disgust(adapter, DISGUST_CORE, 0.9f, false);

    insula_emotional_state_t state;
    EXPECT_TRUE(insula_get_emotional_state(adapter, &state));
    EXPECT_GT(state.disgust, 0.0f);
}

TEST_F(InsulaAdapterTest, DisgustUpdatesSocialState) {
    insula_process_disgust(adapter, DISGUST_MORAL, 0.8f, true);

    insula_social_state_t state;
    EXPECT_TRUE(insula_get_social_state(adapter, &state));
    EXPECT_GT(state.disgust_intensity, 0.0f);
    EXPECT_EQ(state.disgust_type, DISGUST_MORAL);
}

// ============================================================================
// EMPATHY PROCESSING TESTS
// ============================================================================

TEST_F(InsulaAdapterTest, ProcessEmpathySuccess) {
    float resonance = insula_process_empathy(adapter, -0.5f, 0.6f, 0.8f);
    EXPECT_GE(resonance, 0.0f);
    EXPECT_LE(resonance, 1.0f);
}

TEST_F(InsulaAdapterTest, EmpathyScalesWithSimilarity) {
    float low_sim = insula_process_empathy(adapter, -0.5f, 0.6f, 0.2f);
    insula_reset(adapter);
    float high_sim = insula_process_empathy(adapter, -0.5f, 0.6f, 0.9f);

    EXPECT_GT(high_sim, low_sim);
}

TEST_F(InsulaAdapterTest, EmpathyNullReturnsZero) {
    float resonance = insula_process_empathy(NULL, 0.5f, 0.5f, 0.5f);
    EXPECT_EQ(resonance, 0.0f);
}

// ============================================================================
// TRUST ASSESSMENT TESTS
// ============================================================================

TEST_F(InsulaAdapterTest, AssessTrustSuccess) {
    float trust = insula_assess_trust(adapter, 0.7f, 0.8f, 0.9f);
    EXPECT_GE(trust, 0.0f);
    EXPECT_LE(trust, 1.0f);
}

TEST_F(InsulaAdapterTest, TrustCombinesCues) {
    // High trust cues
    float high_trust = insula_assess_trust(adapter, 0.9f, 0.9f, 0.9f);

    insula_reset(adapter);

    // Low trust cues
    float low_trust = insula_assess_trust(adapter, 0.1f, 0.1f, 0.1f);

    EXPECT_GT(high_trust, low_trust);
}

TEST_F(InsulaAdapterTest, BetrayalDetection) {
    // Establish high trust
    insula_assess_trust(adapter, 0.9f, 0.9f, 0.9f);

    // Sharp drop
    insula_assess_trust(adapter, 0.1f, 0.1f, 0.1f);

    insula_social_state_t state;
    EXPECT_TRUE(insula_get_social_state(adapter, &state));
    EXPECT_TRUE(state.betrayal_detected);
}

// ============================================================================
// FAIRNESS PROCESSING TESTS
// ============================================================================

TEST_F(InsulaAdapterTest, ProcessFairnessEqualOutcomes) {
    float fairness = insula_process_fairness(adapter, 0.5f, 0.5f, false);
    // Equal outcomes should be perceived as fair (positive or near 1)
    EXPECT_GT(fairness, 0.5f);
}

TEST_F(InsulaAdapterTest, ProcessFairnessDisadvantageous) {
    float fairness = insula_process_fairness(adapter, 0.2f, 0.8f, true);
    // Being disadvantaged should feel unfair (negative)
    EXPECT_LT(fairness, 0.0f);
}

TEST_F(InsulaAdapterTest, ProcessFairnessAdvantageous) {
    float fairness = insula_process_fairness(adapter, 0.8f, 0.2f, false);
    // Being advantaged also feels somewhat unfair
    EXPECT_LT(fairness, 0.8f);
}

// ============================================================================
// REJECTION PROCESSING TESTS
// ============================================================================

TEST_F(InsulaAdapterTest, ProcessRejectionSuccess) {
    float pain = insula_process_rejection(adapter, 0.7f, 0.8f);
    EXPECT_GE(pain, 0.0f);
    EXPECT_LE(pain, 1.0f);
}

TEST_F(InsulaAdapterTest, RejectionAffectsEmotionalState) {
    insula_process_rejection(adapter, 0.9f, 0.9f);

    insula_emotional_state_t state;
    EXPECT_TRUE(insula_get_emotional_state(adapter, &state));
    EXPECT_GT(state.sadness, 0.0f);
}

TEST_F(InsulaAdapterTest, RejectionUpdatesSocialState) {
    insula_process_rejection(adapter, 0.8f, 0.7f);

    insula_social_state_t state;
    EXPECT_TRUE(insula_get_social_state(adapter, &state));
    EXPECT_GT(state.social_pain, 0.0f);
    EXPECT_GT(state.belonging_need, 0.0f);
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

TEST_F(InsulaAdapterTest, IntegrateSuccess) {
    // Feed various inputs
    insula_intero_signal_t cardiac = make_intero_signal(INTERO_CHANNEL_CARDIAC, 0.6f);
    insula_update_interoception(adapter, &cardiac);
    insula_process_emotion(adapter, 0.5f, 0.4f, "test");
    insula_process_empathy(adapter, 0.3f, 0.5f, 0.7f);

    // Integrate
    insula_output_t output;
    EXPECT_TRUE(insula_integrate(adapter, &output));

    // Output should be populated
    EXPECT_GT(output.interoceptive_accuracy, 0.0f);
    EXPECT_GE(output.approach_motivation, 0.0f);
    EXPECT_GE(output.avoidance_motivation, 0.0f);
}

TEST_F(InsulaAdapterTest, IntegrateWithNullOutputOk) {
    EXPECT_TRUE(insula_integrate(adapter, NULL));
}

TEST_F(InsulaAdapterTest, StepAdvancesTime) {
    double t1 = 0.0;
    double t2 = 100.0;

    insula_step(adapter, t1);
    insula_step(adapter, t2);

    // Should not crash, and status should be valid
    EXPECT_NE(insula_get_status(adapter), INSULA_STATUS_ERROR);
}

TEST_F(InsulaAdapterTest, GetOutputSuccess) {
    insula_integrate(adapter, NULL);

    insula_output_t output;
    EXPECT_TRUE(insula_get_output(adapter, &output));
}

// ============================================================================
// CALLBACK TESTS
// ============================================================================

static bool body_callback_called = false;
static void test_body_callback(const insula_body_state_t* state, void* user_data) {
    body_callback_called = true;
    (void)state;
    (void)user_data;
}

TEST_F(InsulaAdapterTest, SetBodyCallbackSuccess) {
    EXPECT_TRUE(insula_set_body_callback(adapter, test_body_callback, NULL));
}

static bool emotion_callback_called = false;
static void test_emotion_callback(const insula_emotional_state_t* state, void* user_data) {
    emotion_callback_called = true;
    (void)state;
    (void)user_data;
}

TEST_F(InsulaAdapterTest, SetEmotionCallbackSuccess) {
    EXPECT_TRUE(insula_set_emotion_callback(adapter, test_emotion_callback, NULL));
}

static bool social_callback_called = false;
static void test_social_callback(insula_social_emotion_t type, float intensity, void* user_data) {
    social_callback_called = true;
    (void)type;
    (void)intensity;
    (void)user_data;
}

TEST_F(InsulaAdapterTest, SetSocialCallbackSuccess) {
    EXPECT_TRUE(insula_set_social_callback(adapter, test_social_callback, NULL));
}

static bool alarm_callback_called = false;
static void test_alarm_callback(const char* type, float urgency, void* user_data) {
    alarm_callback_called = true;
    (void)type;
    (void)urgency;
    (void)user_data;
}

TEST_F(InsulaAdapterTest, SetAlarmCallbackSuccess) {
    EXPECT_TRUE(insula_set_alarm_callback(adapter, test_alarm_callback, NULL));
}

TEST_F(InsulaAdapterTest, EmotionCallbackInvoked) {
    emotion_callback_called = false;
    insula_set_emotion_callback(adapter, test_emotion_callback, NULL);

    insula_process_emotion(adapter, 0.5f, 0.5f, "test");

    EXPECT_TRUE(emotion_callback_called);
}

TEST_F(InsulaAdapterTest, SocialCallbackInvokedOnDisgust) {
    social_callback_called = false;
    insula_set_social_callback(adapter, test_social_callback, NULL);

    insula_process_disgust(adapter, DISGUST_CORE, 0.8f, false);

    EXPECT_TRUE(social_callback_called);
}

// ============================================================================
// STATUS AND DIAGNOSTICS TESTS
// ============================================================================

TEST_F(InsulaAdapterTest, GetStatusSuccess) {
    insula_status_t status = insula_get_status(adapter);
    EXPECT_NE(status, INSULA_STATUS_ERROR);
}

TEST_F(InsulaAdapterTest, GetStatusNullReturnsError) {
    EXPECT_EQ(insula_get_status(NULL), INSULA_STATUS_ERROR);
}

TEST_F(InsulaAdapterTest, GetLastErrorSuccess) {
    insula_error_t error = insula_get_last_error(adapter);
    EXPECT_EQ(error, INSULA_ERROR_NONE);
}

TEST_F(InsulaAdapterTest, GetLastErrorNullReturnsInternal) {
    EXPECT_EQ(insula_get_last_error(NULL), INSULA_ERROR_INTERNAL);
}

TEST_F(InsulaAdapterTest, ErrorStringNotNull) {
    for (int i = 0; i <= INSULA_ERROR_INTERNAL; i++) {
        const char* str = insula_error_string((insula_error_t)i);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

TEST_F(InsulaAdapterTest, StatusStringNotNull) {
    for (int i = 0; i <= INSULA_STATUS_ERROR; i++) {
        const char* str = insula_status_string((insula_status_t)i);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

TEST_F(InsulaAdapterTest, GetStatsSuccess) {
    // Do some processing
    insula_intero_signal_t signal = make_intero_signal(INTERO_CHANNEL_CARDIAC, 0.7f);
    insula_update_interoception(adapter, &signal);
    insula_process_emotion(adapter, 0.5f, 0.4f, "test");
    insula_integrate(adapter, NULL);

    insula_stats_t stats;
    EXPECT_TRUE(insula_get_stats(adapter, &stats));

    EXPECT_GE(stats.intero_signals_processed, 1u);
    EXPECT_GE(stats.emotional_updates, 1u);
    EXPECT_GE(stats.integration_cycles, 1u);
}

TEST_F(InsulaAdapterTest, GetStatsNullFails) {
    EXPECT_FALSE(insula_get_stats(NULL, NULL));
    insula_stats_t stats;
    EXPECT_FALSE(insula_get_stats(NULL, &stats));
    EXPECT_FALSE(insula_get_stats(adapter, NULL));
}

TEST_F(InsulaAdapterTest, GetConfigSuccess) {
    insula_config_t retrieved;
    EXPECT_TRUE(insula_get_config(adapter, &retrieved));

    // Should match what we created with
    EXPECT_EQ(retrieved.interoception_channels, config.interoception_channels);
    EXPECT_EQ(retrieved.enable_disgust_processing, config.enable_disgust_processing);
}

// ============================================================================
// BIO-ASYNC TESTS
// ============================================================================

TEST_F(InsulaAdapterTest, GetBioContextNullAdapterReturnsNull) {
    bio_module_context_t ctx = insula_get_bio_context(NULL);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(InsulaAdapterTest, ProcessBioMessagesZeroForNullAdapter) {
    uint32_t processed = insula_process_bio_messages(NULL, 10);
    EXPECT_EQ(processed, 0u);
}

TEST_F(InsulaAdapterTest, BroadcastBodyStateNullFails) {
    insula_body_state_t state = {};
    EXPECT_EQ(insula_broadcast_body_state(NULL, &state), NIMCP_BIO_ERROR_NOT_INITIALIZED);
    EXPECT_EQ(insula_broadcast_body_state(adapter, NULL), NIMCP_BIO_ERROR_NOT_INITIALIZED);
}

TEST_F(InsulaAdapterTest, BroadcastEmotionalStateNullFails) {
    insula_emotional_state_t state = {};
    EXPECT_EQ(insula_broadcast_emotional_state(NULL, &state), NIMCP_BIO_ERROR_NOT_INITIALIZED);
    EXPECT_EQ(insula_broadcast_emotional_state(adapter, NULL), NIMCP_BIO_ERROR_NOT_INITIALIZED);
}

TEST_F(InsulaAdapterTest, BroadcastSocialAlarmNullFails) {
    EXPECT_EQ(insula_broadcast_social_alarm(NULL, SOCIAL_EMOTION_DISGUST, 0.8f),
              NIMCP_BIO_ERROR_NOT_INITIALIZED);
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

TEST_F(InsulaAdapterTest, AllChannelTypes) {
    // Test all interoceptive channel types
    for (int ch = 0; ch < INTERO_CHANNEL_COUNT; ch++) {
        insula_intero_signal_t signal = make_intero_signal((insula_intero_channel_t)ch, 0.5f);
        EXPECT_TRUE(insula_update_interoception(adapter, &signal))
            << "Failed for channel " << ch;
    }
}

TEST_F(InsulaAdapterTest, AllDisgustTypes) {
    // Test all disgust types
    for (int d = 0; d < DISGUST_COUNT; d++) {
        float response = insula_process_disgust(adapter, (insula_disgust_type_t)d, 0.5f, false);
        EXPECT_GE(response, 0.0f) << "Failed for disgust type " << d;
        EXPECT_LE(response, 1.0f) << "Failed for disgust type " << d;
    }
}

TEST_F(InsulaAdapterTest, ExtremeValues) {
    // Test with extreme values
    insula_intero_signal_t signal;

    // Max intensity
    signal = make_intero_signal(INTERO_CHANNEL_CARDIAC, 1.0f);
    EXPECT_TRUE(insula_update_interoception(adapter, &signal));

    // Min intensity
    signal = make_intero_signal(INTERO_CHANNEL_CARDIAC, 0.0f);
    EXPECT_TRUE(insula_update_interoception(adapter, &signal));

    // Emotional extremes
    EXPECT_TRUE(insula_process_emotion(adapter, 1.0f, 1.0f, "max"));
    EXPECT_TRUE(insula_process_emotion(adapter, -1.0f, -1.0f, "min"));
}

TEST_F(InsulaAdapterTest, RapidUpdates) {
    // Rapid succession of updates
    for (int i = 0; i < 100; i++) {
        insula_intero_signal_t signal = make_intero_signal(
            (insula_intero_channel_t)(i % INTERO_CHANNEL_COUNT),
            (float)i / 100.0f);
        EXPECT_TRUE(insula_update_interoception(adapter, &signal));
    }

    // Should still be functional
    insula_output_t output;
    EXPECT_TRUE(insula_integrate(adapter, &output));
}
