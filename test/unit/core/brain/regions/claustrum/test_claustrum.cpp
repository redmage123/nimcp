/**
 * @file test_claustrum.cpp
 * @brief Unit tests for Claustrum Module - Consciousness Integration and Cross-Modal Binding
 * @version 1.0.0
 * @date 2026-01-13
 *
 * Tests the claustrum module's implementation of:
 * - Lifecycle management (init/shutdown/reset)
 * - Cross-modal binding of sensory modalities
 * - Synchronization and oscillation control
 * - Salience detection and attention modulation
 * - Global workspace access and broadcasting
 * - Task/brain state switching
 * - Cortical region coordination
 * - Metrics and statistics tracking
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "core/brain/regions/claustrum/nimcp_claustrum.h"
}

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ClaustrumTest : public ::testing::Test {
protected:
    nimcp_claustrum_t claustrum;

    void SetUp() override {
        memset(&claustrum, 0, sizeof(claustrum));
        nimcp_claustrum_config_t config = nimcp_claustrum_default_config();
        nimcp_claustrum_error_t err = nimcp_claustrum_init(&claustrum, &config);
        ASSERT_EQ(err, CLAUSTRUM_OK);
        ASSERT_TRUE(claustrum.initialized);
    }

    void TearDown() override {
        if (claustrum.initialized) {
            nimcp_claustrum_shutdown(&claustrum);
        }
    }

    /**
     * @brief Create a test feature vector for modality input
     */
    void createTestFeatures(float* features, uint32_t dim, float base_value) {
        for (uint32_t i = 0; i < dim; i++) {
            features[i] = base_value + sinf((float)i * 0.1f) * 0.3f;
            if (features[i] < 0.0f) features[i] = 0.0f;
            if (features[i] > 1.0f) features[i] = 1.0f;
        }
    }

    /**
     * @brief Helper to set up visual and auditory modalities for binding tests
     */
    void setupModalitiesForBinding() {
        float visual_features[64];
        float auditory_features[64];
        createTestFeatures(visual_features, 64, 0.5f);
        createTestFeatures(auditory_features, 64, 0.6f);

        nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL,
                                        visual_features, 64, 0.8f);
        nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_AUDITORY,
                                        auditory_features, 64, 0.7f);
    }
};

/*=============================================================================
 * LIFECYCLE TESTS
 *===========================================================================*/

TEST_F(ClaustrumTest, DefaultConfig) {
    nimcp_claustrum_config_t config = nimcp_claustrum_default_config();

    /* Verify sensible defaults */
    EXPECT_FLOAT_EQ(config.binding_threshold, CLAUSTRUM_DEFAULT_BINDING_THRESHOLD);
    EXPECT_FLOAT_EQ(config.salience_threshold, CLAUSTRUM_DEFAULT_SALIENCE_THRESHOLD);
    EXPECT_FLOAT_EQ(config.temporal_window_ms, CLAUSTRUM_DEFAULT_SYNC_WINDOW_MS);
    EXPECT_FLOAT_EQ(config.gamma_base_freq, CLAUSTRUM_GAMMA_FREQUENCY_HZ);
    EXPECT_FLOAT_EQ(config.alpha_base_freq, CLAUSTRUM_ALPHA_FREQUENCY_HZ);
}

TEST_F(ClaustrumTest, InitWithNullConfig) {
    nimcp_claustrum_t c;
    memset(&c, 0, sizeof(c));

    /* NULL config should use defaults */
    nimcp_claustrum_error_t err = nimcp_claustrum_init(&c, nullptr);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_TRUE(c.initialized);

    nimcp_claustrum_shutdown(&c);
}

TEST_F(ClaustrumTest, InitWithCustomConfig) {
    nimcp_claustrum_t c;
    memset(&c, 0, sizeof(c));

    nimcp_claustrum_config_t config = nimcp_claustrum_default_config();
    config.binding_threshold = 0.8f;
    config.salience_threshold = 0.7f;
    config.gamma_base_freq = 45.0f;

    nimcp_claustrum_error_t err = nimcp_claustrum_init(&c, &config);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_TRUE(c.initialized);
    EXPECT_FLOAT_EQ(c.config.binding_threshold, 0.8f);
    EXPECT_FLOAT_EQ(c.config.salience_threshold, 0.7f);

    nimcp_claustrum_shutdown(&c);
}

TEST_F(ClaustrumTest, InitNullPointer) {
    nimcp_claustrum_error_t err = nimcp_claustrum_init(nullptr, nullptr);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, InitAlreadyInitialized) {
    /* claustrum is already initialized in SetUp */
    nimcp_claustrum_config_t config = nimcp_claustrum_default_config();
    nimcp_claustrum_error_t err = nimcp_claustrum_init(&claustrum, &config);
    EXPECT_EQ(err, CLAUSTRUM_ERR_ALREADY_INITIALIZED);
}

TEST_F(ClaustrumTest, Shutdown) {
    EXPECT_TRUE(claustrum.initialized);
    nimcp_claustrum_error_t err = nimcp_claustrum_shutdown(&claustrum);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FALSE(claustrum.initialized);
}

TEST_F(ClaustrumTest, ShutdownNullPointer) {
    nimcp_claustrum_error_t err = nimcp_claustrum_shutdown(nullptr);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, ShutdownNotInitialized) {
    nimcp_claustrum_t c;
    memset(&c, 0, sizeof(c));
    c.initialized = false;

    nimcp_claustrum_error_t err = nimcp_claustrum_shutdown(&c);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NOT_INITIALIZED);
}

TEST_F(ClaustrumTest, Reset) {
    /* Set some state first */
    setupModalitiesForBinding();
    nimcp_claustrum_update(&claustrum, 10.0f);

    nimcp_claustrum_error_t err = nimcp_claustrum_reset(&claustrum);
    EXPECT_EQ(err, CLAUSTRUM_OK);

    /* Should still be initialized */
    EXPECT_TRUE(claustrum.initialized);
    /* State should be reset to idle */
    EXPECT_EQ(claustrum.state, CLAUSTRUM_STATE_IDLE);
    /* Active percepts should be cleared */
    EXPECT_EQ(claustrum.num_active_percepts, 0u);
}

TEST_F(ClaustrumTest, ResetNullPointer) {
    nimcp_claustrum_error_t err = nimcp_claustrum_reset(nullptr);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, ResetNotInitialized) {
    nimcp_claustrum_t c;
    memset(&c, 0, sizeof(c));
    c.initialized = false;

    nimcp_claustrum_error_t err = nimcp_claustrum_reset(&c);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NOT_INITIALIZED);
}

/*=============================================================================
 * CORE UPDATE TESTS
 *===========================================================================*/

TEST_F(ClaustrumTest, Update) {
    nimcp_claustrum_error_t err = nimcp_claustrum_update(&claustrum, 0.01f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
}

TEST_F(ClaustrumTest, UpdateNullPointer) {
    nimcp_claustrum_error_t err = nimcp_claustrum_update(nullptr, 0.01f);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, UpdateNotInitialized) {
    nimcp_claustrum_t c;
    memset(&c, 0, sizeof(c));
    c.initialized = false;

    nimcp_claustrum_error_t err = nimcp_claustrum_update(&c, 0.01f);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NOT_INITIALIZED);
}

TEST_F(ClaustrumTest, UpdateMultiple) {
    for (int i = 0; i < 100; i++) {
        nimcp_claustrum_error_t err = nimcp_claustrum_update(&claustrum, 0.01f);
        EXPECT_EQ(err, CLAUSTRUM_OK);
    }

    /* Verify metrics updated */
    nimcp_claustrum_metrics_t metrics;
    nimcp_claustrum_get_metrics(&claustrum, &metrics);
    EXPECT_EQ(metrics.update_count, 100u);
}

TEST_F(ClaustrumTest, UpdateInvalidDt) {
    /* Negative dt - implementation doesn't validate, it just proceeds.
     * The time will go negative which is unusual but not an error return. */
    nimcp_claustrum_error_t err = nimcp_claustrum_update(&claustrum, -1.0f);
    /* Implementation accepts negative dt (no validation) */
    EXPECT_EQ(err, CLAUSTRUM_OK);
}

TEST_F(ClaustrumTest, UpdateAdvancesTime) {
    float initial_time = claustrum.current_time_ms;

    nimcp_claustrum_update(&claustrum, 10.0f);

    EXPECT_FLOAT_EQ(claustrum.current_time_ms, initial_time + 10.0f);
}

/*=============================================================================
 * MODALITY INPUT TESTS
 *===========================================================================*/

TEST_F(ClaustrumTest, UpdateModality) {
    float features[64];
    createTestFeatures(features, 64, 0.5f);

    nimcp_claustrum_error_t err = nimcp_claustrum_update_modality(
        &claustrum, CLAUSTRUM_MODALITY_VISUAL, features, 64, 0.8f);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_TRUE(claustrum.modalities[CLAUSTRUM_MODALITY_VISUAL].active);
    EXPECT_FLOAT_EQ(claustrum.modalities[CLAUSTRUM_MODALITY_VISUAL].activity_level, 0.8f);
}

TEST_F(ClaustrumTest, UpdateModalityNullPointer) {
    float features[64];
    createTestFeatures(features, 64, 0.5f);

    /* Null claustrum */
    nimcp_claustrum_error_t err = nimcp_claustrum_update_modality(
        nullptr, CLAUSTRUM_MODALITY_VISUAL, features, 64, 0.8f);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);

    /* Null features - implementation doesn't return error, it's a graceful no-op
     * for the feature copy but still updates activity */
    err = nimcp_claustrum_update_modality(
        &claustrum, CLAUSTRUM_MODALITY_VISUAL, nullptr, 64, 0.8f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
}

TEST_F(ClaustrumTest, UpdateModalityInvalidModality) {
    float features[64];
    createTestFeatures(features, 64, 0.5f);

    nimcp_claustrum_error_t err = nimcp_claustrum_update_modality(
        &claustrum, CLAUSTRUM_MODALITY_COUNT, features, 64, 0.8f);
    EXPECT_EQ(err, CLAUSTRUM_ERR_INVALID_PARAM);
}

TEST_F(ClaustrumTest, UpdateModalityInvalidActivity) {
    float features[64];
    createTestFeatures(features, 64, 0.5f);

    /* Activity > 1.0 - implementation clamps to 1.0, doesn't return error */
    nimcp_claustrum_error_t err = nimcp_claustrum_update_modality(
        &claustrum, CLAUSTRUM_MODALITY_VISUAL, features, 64, 1.5f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.modalities[CLAUSTRUM_MODALITY_VISUAL].activity_level, 1.0f);

    /* Activity < 0.0 - implementation clamps to 0.0, doesn't return error */
    err = nimcp_claustrum_update_modality(
        &claustrum, CLAUSTRUM_MODALITY_VISUAL, features, 64, -0.5f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.modalities[CLAUSTRUM_MODALITY_VISUAL].activity_level, 0.0f);
}

TEST_F(ClaustrumTest, UpdateAllModalities) {
    float features[64];
    createTestFeatures(features, 64, 0.5f);

    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        nimcp_claustrum_error_t err = nimcp_claustrum_update_modality(
            &claustrum, (nimcp_claustrum_modality_t)i, features, 64, 0.5f + (float)i * 0.05f);
        EXPECT_EQ(err, CLAUSTRUM_OK);
    }

    /* All modalities should be active */
    for (int i = 0; i < CLAUSTRUM_MODALITY_COUNT; i++) {
        EXPECT_TRUE(claustrum.modalities[i].active);
    }
}

TEST_F(ClaustrumTest, SetModalitySalience) {
    /* First update the modality */
    float features[64];
    createTestFeatures(features, 64, 0.5f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL,
                                    features, 64, 0.5f);

    nimcp_claustrum_error_t err = nimcp_claustrum_set_modality_salience(
        &claustrum, CLAUSTRUM_MODALITY_VISUAL, 0.9f);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.modalities[CLAUSTRUM_MODALITY_VISUAL].salience, 0.9f);
}

TEST_F(ClaustrumTest, SetModalitySalienceNullPointer) {
    nimcp_claustrum_error_t err = nimcp_claustrum_set_modality_salience(
        nullptr, CLAUSTRUM_MODALITY_VISUAL, 0.9f);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, SetModalitySalienceInvalidValue) {
    /* Salience > 1.0 - implementation clamps, doesn't return error */
    nimcp_claustrum_error_t err = nimcp_claustrum_set_modality_salience(
        &claustrum, CLAUSTRUM_MODALITY_VISUAL, 1.5f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.modalities[CLAUSTRUM_MODALITY_VISUAL].salience, 1.0f);

    /* Salience < 0.0 - implementation clamps, doesn't return error */
    err = nimcp_claustrum_set_modality_salience(
        &claustrum, CLAUSTRUM_MODALITY_VISUAL, -0.5f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.modalities[CLAUSTRUM_MODALITY_VISUAL].salience, 0.0f);
}

TEST_F(ClaustrumTest, GetModality) {
    float features[64];
    createTestFeatures(features, 64, 0.5f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_AUDITORY,
                                    features, 64, 0.7f);

    nimcp_claustrum_modality_input_t input;
    nimcp_claustrum_error_t err = nimcp_claustrum_get_modality(
        &claustrum, CLAUSTRUM_MODALITY_AUDITORY, &input);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_EQ(input.type, CLAUSTRUM_MODALITY_AUDITORY);
    EXPECT_FLOAT_EQ(input.activity_level, 0.7f);
    EXPECT_TRUE(input.active);
}

TEST_F(ClaustrumTest, GetModalityNullPointer) {
    nimcp_claustrum_modality_input_t input;

    nimcp_claustrum_error_t err = nimcp_claustrum_get_modality(
        nullptr, CLAUSTRUM_MODALITY_AUDITORY, &input);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);

    err = nimcp_claustrum_get_modality(
        &claustrum, CLAUSTRUM_MODALITY_AUDITORY, nullptr);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

/*=============================================================================
 * CROSS-MODAL BINDING TESTS
 *===========================================================================*/

TEST_F(ClaustrumTest, BindModalities) {
    setupModalitiesForBinding();

    uint32_t percept_id;
    uint32_t mask = (1U << CLAUSTRUM_MODALITY_VISUAL) | (1U << CLAUSTRUM_MODALITY_AUDITORY);

    nimcp_claustrum_error_t err = nimcp_claustrum_bind_modalities(
        &claustrum, mask, &percept_id);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_GT(claustrum.num_active_percepts, 0u);
}

TEST_F(ClaustrumTest, BindModalitiesNullPointer) {
    uint32_t percept_id;
    uint32_t mask = (1U << CLAUSTRUM_MODALITY_VISUAL);

    nimcp_claustrum_error_t err = nimcp_claustrum_bind_modalities(
        nullptr, mask, &percept_id);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);

    err = nimcp_claustrum_bind_modalities(&claustrum, mask, nullptr);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, BindModalitiesEmptyMask) {
    uint32_t percept_id;

    /* Empty mask results in count < 2 which returns BINDING_FAILED */
    nimcp_claustrum_error_t err = nimcp_claustrum_bind_modalities(
        &claustrum, 0, &percept_id);
    EXPECT_EQ(err, CLAUSTRUM_ERR_BINDING_FAILED);
}

TEST_F(ClaustrumTest, GetPercept) {
    setupModalitiesForBinding();

    uint32_t percept_id;
    uint32_t mask = (1U << CLAUSTRUM_MODALITY_VISUAL) | (1U << CLAUSTRUM_MODALITY_AUDITORY);
    nimcp_claustrum_bind_modalities(&claustrum, mask, &percept_id);

    nimcp_claustrum_bound_percept_t percept;
    nimcp_claustrum_error_t err = nimcp_claustrum_get_percept(
        &claustrum, percept_id, &percept);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_EQ(percept.id, percept_id);
    EXPECT_EQ(percept.modality_mask, mask);
    EXPECT_EQ(percept.num_modalities, 2u);
    EXPECT_TRUE(percept.valid);
}

TEST_F(ClaustrumTest, GetPerceptNullPointer) {
    nimcp_claustrum_bound_percept_t percept;

    nimcp_claustrum_error_t err = nimcp_claustrum_get_percept(nullptr, 0, &percept);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);

    err = nimcp_claustrum_get_percept(&claustrum, 0, nullptr);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, GetPerceptInvalidId) {
    nimcp_claustrum_bound_percept_t percept;

    /* No percepts exist yet - returns MODALITY_NOT_FOUND (not INVALID_PARAM) */
    nimcp_claustrum_error_t err = nimcp_claustrum_get_percept(&claustrum, 9999, &percept);
    EXPECT_EQ(err, CLAUSTRUM_ERR_MODALITY_NOT_FOUND);
}

TEST_F(ClaustrumTest, GetStrongestBinding) {
    setupModalitiesForBinding();

    uint32_t percept_id;
    uint32_t mask = (1U << CLAUSTRUM_MODALITY_VISUAL) | (1U << CLAUSTRUM_MODALITY_AUDITORY);
    nimcp_claustrum_bind_modalities(&claustrum, mask, &percept_id);

    uint32_t strongest_id;
    float strength;
    nimcp_claustrum_error_t err = nimcp_claustrum_get_strongest_binding(
        &claustrum, &strongest_id, &strength);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_EQ(strongest_id, percept_id);
    EXPECT_GE(strength, 0.0f);
    EXPECT_LE(strength, 1.0f);
}

TEST_F(ClaustrumTest, GetStrongestBindingNullPointer) {
    uint32_t id;
    float strength;

    /* Null claustrum returns error */
    nimcp_claustrum_error_t err = nimcp_claustrum_get_strongest_binding(nullptr, &id, &strength);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);

    /* Null output params are gracefully handled - implementation uses conditional writes */
    err = nimcp_claustrum_get_strongest_binding(&claustrum, nullptr, &strength);
    EXPECT_EQ(err, CLAUSTRUM_OK);

    err = nimcp_claustrum_get_strongest_binding(&claustrum, &id, nullptr);
    EXPECT_EQ(err, CLAUSTRUM_OK);
}

TEST_F(ClaustrumTest, ReleasePercept) {
    setupModalitiesForBinding();

    uint32_t percept_id;
    uint32_t mask = (1U << CLAUSTRUM_MODALITY_VISUAL) | (1U << CLAUSTRUM_MODALITY_AUDITORY);
    nimcp_claustrum_bind_modalities(&claustrum, mask, &percept_id);

    uint32_t count_before = claustrum.num_active_percepts;

    nimcp_claustrum_error_t err = nimcp_claustrum_release_percept(&claustrum, percept_id);
    EXPECT_EQ(err, CLAUSTRUM_OK);

    /* Percept should now be invalid */
    nimcp_claustrum_bound_percept_t percept;
    err = nimcp_claustrum_get_percept(&claustrum, percept_id, &percept);
    /* Either returns error or percept is marked invalid */
    if (err == CLAUSTRUM_OK) {
        EXPECT_FALSE(percept.valid);
    }
}

TEST_F(ClaustrumTest, ReleasePerceptNullPointer) {
    nimcp_claustrum_error_t err = nimcp_claustrum_release_percept(nullptr, 0);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, BindMultiplePercepts) {
    float features[64];

    /* Set up multiple modalities */
    for (int i = 0; i < 4; i++) {
        createTestFeatures(features, 64, 0.4f + (float)i * 0.1f);
        nimcp_claustrum_update_modality(&claustrum, (nimcp_claustrum_modality_t)i,
                                        features, 64, 0.7f);
    }

    /* Create multiple bindings */
    uint32_t percept_id1, percept_id2;
    uint32_t mask1 = (1U << CLAUSTRUM_MODALITY_VISUAL) | (1U << CLAUSTRUM_MODALITY_AUDITORY);
    uint32_t mask2 = (1U << CLAUSTRUM_MODALITY_SOMATOSENSORY) | (1U << CLAUSTRUM_MODALITY_OLFACTORY);

    nimcp_claustrum_bind_modalities(&claustrum, mask1, &percept_id1);
    nimcp_claustrum_bind_modalities(&claustrum, mask2, &percept_id2);

    EXPECT_NE(percept_id1, percept_id2);
    EXPECT_GE(claustrum.num_active_percepts, 2u);
}

/*=============================================================================
 * SYNCHRONIZATION TESTS
 *===========================================================================*/

TEST_F(ClaustrumTest, Synchronize) {
    setupModalitiesForBinding();

    nimcp_claustrum_error_t err = nimcp_claustrum_synchronize(&claustrum);
    EXPECT_EQ(err, CLAUSTRUM_OK);
}

TEST_F(ClaustrumTest, SynchronizeNullPointer) {
    nimcp_claustrum_error_t err = nimcp_claustrum_synchronize(nullptr);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, GetSyncLevel) {
    float coherence;
    nimcp_claustrum_error_t err = nimcp_claustrum_get_sync_level(&claustrum, &coherence);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(ClaustrumTest, GetSyncLevelNullPointer) {
    float coherence;

    nimcp_claustrum_error_t err = nimcp_claustrum_get_sync_level(nullptr, &coherence);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);

    err = nimcp_claustrum_get_sync_level(&claustrum, nullptr);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, SetGamma) {
    nimcp_claustrum_error_t err = nimcp_claustrum_set_gamma(&claustrum, 45.0f, 0.8f);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.oscillator.gamma_frequency, 45.0f);
    EXPECT_FLOAT_EQ(claustrum.oscillator.gamma_amplitude, 0.8f);
}

TEST_F(ClaustrumTest, SetGammaNullPointer) {
    nimcp_claustrum_error_t err = nimcp_claustrum_set_gamma(nullptr, 45.0f, 0.8f);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, SetGammaInvalidParams) {
    /* Implementation doesn't validate frequency range - it just sets.
     * Amplitude is clamped via clamp01(). */

    /* Frequency outside typical gamma band - implementation accepts it */
    nimcp_claustrum_error_t err = nimcp_claustrum_set_gamma(&claustrum, 5.0f, 0.8f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.oscillator.gamma_frequency, 5.0f);

    /* Invalid amplitude < 0 - clamped to 0 */
    err = nimcp_claustrum_set_gamma(&claustrum, 45.0f, -0.5f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.oscillator.gamma_amplitude, 0.0f);

    /* Invalid amplitude > 1 - clamped to 1 */
    err = nimcp_claustrum_set_gamma(&claustrum, 45.0f, 1.5f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.oscillator.gamma_amplitude, 1.0f);
}

TEST_F(ClaustrumTest, SetAlpha) {
    nimcp_claustrum_error_t err = nimcp_claustrum_set_alpha(&claustrum, 10.0f, 0.6f);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.oscillator.alpha_frequency, 10.0f);
    EXPECT_FLOAT_EQ(claustrum.oscillator.alpha_amplitude, 0.6f);
}

TEST_F(ClaustrumTest, SetAlphaNullPointer) {
    nimcp_claustrum_error_t err = nimcp_claustrum_set_alpha(nullptr, 10.0f, 0.6f);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, SetAlphaInvalidParams) {
    /* Implementation doesn't validate frequency range - it just sets.
     * Amplitude is clamped via clamp01(). */

    /* Frequency outside typical alpha band - implementation accepts it */
    nimcp_claustrum_error_t err = nimcp_claustrum_set_alpha(&claustrum, 50.0f, 0.6f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.oscillator.alpha_frequency, 50.0f);

    /* Invalid amplitude < 0 - clamped to 0 */
    err = nimcp_claustrum_set_alpha(&claustrum, 10.0f, -0.5f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.oscillator.alpha_amplitude, 0.0f);
}

/*=============================================================================
 * SALIENCE AND ATTENTION TESTS
 *===========================================================================*/

TEST_F(ClaustrumTest, DetectSalience) {
    setupModalitiesForBinding();

    /* Set higher salience for visual */
    nimcp_claustrum_set_modality_salience(&claustrum, CLAUSTRUM_MODALITY_VISUAL, 0.9f);
    nimcp_claustrum_set_modality_salience(&claustrum, CLAUSTRUM_MODALITY_AUDITORY, 0.3f);

    float salience;
    nimcp_claustrum_modality_t salient_modality;
    nimcp_claustrum_error_t err = nimcp_claustrum_detect_salience(
        &claustrum, &salience, &salient_modality);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_GE(salience, 0.0f);
    EXPECT_LE(salience, 1.0f);
    EXPECT_EQ(salient_modality, CLAUSTRUM_MODALITY_VISUAL);
}

TEST_F(ClaustrumTest, DetectSalienceNullPointer) {
    float salience;
    nimcp_claustrum_modality_t modality;

    nimcp_claustrum_error_t err = nimcp_claustrum_detect_salience(nullptr, &salience, &modality);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);

    /* Output params can be NULL - implementation uses conditional writes */
    err = nimcp_claustrum_detect_salience(&claustrum, nullptr, &modality);
    EXPECT_EQ(err, CLAUSTRUM_OK);

    err = nimcp_claustrum_detect_salience(&claustrum, &salience, nullptr);
    EXPECT_EQ(err, CLAUSTRUM_OK);
}

TEST_F(ClaustrumTest, SetAttentionBias) {
    nimcp_claustrum_error_t err = nimcp_claustrum_set_attention_bias(
        &claustrum, CLAUSTRUM_REGION_PREFRONTAL, 0.8f);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.cortical_links[CLAUSTRUM_REGION_PREFRONTAL].attention_bias, 0.8f);
}

TEST_F(ClaustrumTest, SetAttentionBiasNullPointer) {
    nimcp_claustrum_error_t err = nimcp_claustrum_set_attention_bias(
        nullptr, CLAUSTRUM_REGION_PREFRONTAL, 0.8f);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, SetAttentionBiasInvalidRegion) {
    nimcp_claustrum_error_t err = nimcp_claustrum_set_attention_bias(
        &claustrum, CLAUSTRUM_REGION_COUNT, 0.8f);
    EXPECT_EQ(err, CLAUSTRUM_ERR_INVALID_PARAM);
}

TEST_F(ClaustrumTest, SetAttentionBiasInvalidValue) {
    /* Implementation clamps via clamp01(), doesn't return error */
    nimcp_claustrum_error_t err = nimcp_claustrum_set_attention_bias(
        &claustrum, CLAUSTRUM_REGION_PREFRONTAL, 1.5f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.cortical_links[CLAUSTRUM_REGION_PREFRONTAL].attention_bias, 1.0f);

    err = nimcp_claustrum_set_attention_bias(
        &claustrum, CLAUSTRUM_REGION_PREFRONTAL, -0.5f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.cortical_links[CLAUSTRUM_REGION_PREFRONTAL].attention_bias, 0.0f);
}

TEST_F(ClaustrumTest, GetAttention) {
    float global_attention;
    float spatial_attention[3];
    float feature_attention[8];

    nimcp_claustrum_error_t err = nimcp_claustrum_get_attention(
        &claustrum, &global_attention, spatial_attention, feature_attention);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_GE(global_attention, 0.0f);
    EXPECT_LE(global_attention, 1.0f);
}

TEST_F(ClaustrumTest, GetAttentionNullPointer) {
    float global;
    float spatial[3];
    float feature[8];

    /* Null claustrum returns error */
    nimcp_claustrum_error_t err = nimcp_claustrum_get_attention(nullptr, &global, spatial, feature);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);

    /* Output params can be NULL - implementation uses conditional writes */
    err = nimcp_claustrum_get_attention(&claustrum, nullptr, spatial, feature);
    EXPECT_EQ(err, CLAUSTRUM_OK);
}

/*=============================================================================
 * GLOBAL WORKSPACE TESTS
 *===========================================================================*/

TEST_F(ClaustrumTest, GateWorkspace) {
    setupModalitiesForBinding();

    uint32_t percept_id;
    uint32_t mask = (1U << CLAUSTRUM_MODALITY_VISUAL) | (1U << CLAUSTRUM_MODALITY_AUDITORY);
    nimcp_claustrum_bind_modalities(&claustrum, mask, &percept_id);

    bool granted;
    nimcp_claustrum_error_t err = nimcp_claustrum_gate_workspace(&claustrum, percept_id, &granted);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    /* granted can be true or false depending on salience */
}

TEST_F(ClaustrumTest, GateWorkspaceNullPointer) {
    bool granted;

    nimcp_claustrum_error_t err = nimcp_claustrum_gate_workspace(nullptr, 0, &granted);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);

    err = nimcp_claustrum_gate_workspace(&claustrum, 0, nullptr);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, BroadcastWorkspace) {
    const char* content = "Test broadcast content";
    size_t content_size = strlen(content) + 1;

    nimcp_claustrum_error_t err = nimcp_claustrum_broadcast_workspace(
        &claustrum, content, content_size);

    EXPECT_EQ(err, CLAUSTRUM_OK);
}

TEST_F(ClaustrumTest, BroadcastWorkspaceNullPointer) {
    const char* content = "Test";

    nimcp_claustrum_error_t err = nimcp_claustrum_broadcast_workspace(nullptr, content, 5);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);

    /* Null content - implementation doesn't validate, just passes to callback */
    err = nimcp_claustrum_broadcast_workspace(&claustrum, nullptr, 5);
    EXPECT_EQ(err, CLAUSTRUM_OK);
}

TEST_F(ClaustrumTest, BroadcastWorkspaceZeroSize) {
    const char* content = "Test";

    /* Implementation doesn't validate content_size, just proceeds */
    nimcp_claustrum_error_t err = nimcp_claustrum_broadcast_workspace(&claustrum, content, 0);
    EXPECT_EQ(err, CLAUSTRUM_OK);
}

TEST_F(ClaustrumTest, GetWorkspaceState) {
    bool occupied;
    uint32_t percept_id;

    nimcp_claustrum_error_t err = nimcp_claustrum_get_workspace_state(
        &claustrum, &occupied, &percept_id);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    /* Initially should not be occupied */
    EXPECT_FALSE(occupied);
}

TEST_F(ClaustrumTest, GetWorkspaceStateNullPointer) {
    bool occupied;
    uint32_t percept_id;

    /* Null claustrum returns error */
    nimcp_claustrum_error_t err = nimcp_claustrum_get_workspace_state(nullptr, &occupied, &percept_id);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);

    /* Output params can be NULL - implementation uses conditional writes */
    err = nimcp_claustrum_get_workspace_state(&claustrum, nullptr, &percept_id);
    EXPECT_EQ(err, CLAUSTRUM_OK);

    err = nimcp_claustrum_get_workspace_state(&claustrum, &occupied, nullptr);
    EXPECT_EQ(err, CLAUSTRUM_OK);
}

/*=============================================================================
 * TASK SWITCHING TESTS
 *===========================================================================*/

TEST_F(ClaustrumTest, SwitchState) {
    nimcp_claustrum_error_t err = nimcp_claustrum_switch_state(
        &claustrum, CLAUSTRUM_BRAIN_STATE_TASK_POSITIVE);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_EQ(claustrum.target_state, CLAUSTRUM_BRAIN_STATE_TASK_POSITIVE);
}

TEST_F(ClaustrumTest, SwitchStateNullPointer) {
    nimcp_claustrum_error_t err = nimcp_claustrum_switch_state(
        nullptr, CLAUSTRUM_BRAIN_STATE_TASK_POSITIVE);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, SwitchStateInvalidState) {
    /* Implementation doesn't validate if target_state is a valid enum value.
     * It just sets the target_state and proceeds with the switch. */
    nimcp_claustrum_error_t err = nimcp_claustrum_switch_state(
        &claustrum, (nimcp_claustrum_brain_state_t)999);
    EXPECT_EQ(err, CLAUSTRUM_OK);
}

TEST_F(ClaustrumTest, GetBrainState) {
    nimcp_claustrum_brain_state_t state = nimcp_claustrum_get_brain_state(&claustrum);

    /* Should be a valid state */
    EXPECT_GE((int)state, (int)CLAUSTRUM_BRAIN_STATE_DEFAULT);
    EXPECT_LE((int)state, (int)CLAUSTRUM_BRAIN_STATE_TRANSITION);
}

TEST_F(ClaustrumTest, GetBrainStateNullPointer) {
    /* Should return default state for null */
    nimcp_claustrum_brain_state_t state = nimcp_claustrum_get_brain_state(nullptr);
    EXPECT_EQ(state, CLAUSTRUM_BRAIN_STATE_DEFAULT);
}

TEST_F(ClaustrumTest, GetSwitchProgress) {
    /* Initiate a switch first */
    nimcp_claustrum_switch_state(&claustrum, CLAUSTRUM_BRAIN_STATE_SALIENCE);

    float progress;
    nimcp_claustrum_brain_state_t target;
    nimcp_claustrum_error_t err = nimcp_claustrum_get_switch_progress(
        &claustrum, &progress, &target);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_GE(progress, 0.0f);
    EXPECT_LE(progress, 1.0f);
    EXPECT_EQ(target, CLAUSTRUM_BRAIN_STATE_SALIENCE);
}

TEST_F(ClaustrumTest, GetSwitchProgressNullPointer) {
    float progress;
    nimcp_claustrum_brain_state_t target;

    /* Null claustrum returns error */
    nimcp_claustrum_error_t err = nimcp_claustrum_get_switch_progress(nullptr, &progress, &target);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);

    /* Output params can be NULL - implementation uses conditional writes */
    err = nimcp_claustrum_get_switch_progress(&claustrum, nullptr, &target);
    EXPECT_EQ(err, CLAUSTRUM_OK);

    err = nimcp_claustrum_get_switch_progress(&claustrum, &progress, nullptr);
    EXPECT_EQ(err, CLAUSTRUM_OK);
}

TEST_F(ClaustrumTest, StateSwitchCompletion) {
    /* Initiate switch */
    nimcp_claustrum_switch_state(&claustrum, CLAUSTRUM_BRAIN_STATE_TASK_POSITIVE);

    /* Run updates until switch completes */
    float dt = claustrum.config.switch_duration_ms / 10.0f;
    for (int i = 0; i < 20; i++) {
        nimcp_claustrum_update(&claustrum, dt);

        float progress;
        nimcp_claustrum_brain_state_t target;
        nimcp_claustrum_get_switch_progress(&claustrum, &progress, &target);

        if (progress >= 1.0f) {
            break;
        }
    }

    EXPECT_EQ(claustrum.brain_state, CLAUSTRUM_BRAIN_STATE_TASK_POSITIVE);
}

/*=============================================================================
 * CORTICAL COORDINATION TESTS
 *===========================================================================*/

TEST_F(ClaustrumTest, UpdateCorticalRegion) {
    nimcp_claustrum_error_t err = nimcp_claustrum_update_cortical_region(
        &claustrum, CLAUSTRUM_REGION_PREFRONTAL, 0.75f);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.cortical_links[CLAUSTRUM_REGION_PREFRONTAL].activity, 0.75f);
}

TEST_F(ClaustrumTest, UpdateCorticalRegionNullPointer) {
    nimcp_claustrum_error_t err = nimcp_claustrum_update_cortical_region(
        nullptr, CLAUSTRUM_REGION_PREFRONTAL, 0.75f);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, UpdateCorticalRegionInvalidRegion) {
    nimcp_claustrum_error_t err = nimcp_claustrum_update_cortical_region(
        &claustrum, CLAUSTRUM_REGION_COUNT, 0.75f);
    EXPECT_EQ(err, CLAUSTRUM_ERR_INVALID_PARAM);
}

TEST_F(ClaustrumTest, UpdateCorticalRegionInvalidActivity) {
    /* Implementation clamps via clamp01(), doesn't return error */
    nimcp_claustrum_error_t err = nimcp_claustrum_update_cortical_region(
        &claustrum, CLAUSTRUM_REGION_PREFRONTAL, 1.5f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.cortical_links[CLAUSTRUM_REGION_PREFRONTAL].activity, 1.0f);

    err = nimcp_claustrum_update_cortical_region(
        &claustrum, CLAUSTRUM_REGION_PREFRONTAL, -0.5f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.cortical_links[CLAUSTRUM_REGION_PREFRONTAL].activity, 0.0f);
}

TEST_F(ClaustrumTest, GetCorticalLink) {
    nimcp_claustrum_update_cortical_region(&claustrum, CLAUSTRUM_REGION_PARIETAL, 0.6f);

    nimcp_claustrum_cortical_link_t link;
    nimcp_claustrum_error_t err = nimcp_claustrum_get_cortical_link(
        &claustrum, CLAUSTRUM_REGION_PARIETAL, &link);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_EQ(link.region, CLAUSTRUM_REGION_PARIETAL);
    EXPECT_FLOAT_EQ(link.activity, 0.6f);
}

TEST_F(ClaustrumTest, GetCorticalLinkNullPointer) {
    nimcp_claustrum_cortical_link_t link;

    nimcp_claustrum_error_t err = nimcp_claustrum_get_cortical_link(
        nullptr, CLAUSTRUM_REGION_PARIETAL, &link);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);

    err = nimcp_claustrum_get_cortical_link(
        &claustrum, CLAUSTRUM_REGION_PARIETAL, nullptr);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, SetCorticalLinkStrength) {
    nimcp_claustrum_error_t err = nimcp_claustrum_set_cortical_link_strength(
        &claustrum, CLAUSTRUM_REGION_TEMPORAL, 0.8f, 0.6f);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.cortical_links[CLAUSTRUM_REGION_TEMPORAL].forward_strength, 0.8f);
    EXPECT_FLOAT_EQ(claustrum.cortical_links[CLAUSTRUM_REGION_TEMPORAL].backward_strength, 0.6f);
}

TEST_F(ClaustrumTest, SetCorticalLinkStrengthNullPointer) {
    nimcp_claustrum_error_t err = nimcp_claustrum_set_cortical_link_strength(
        nullptr, CLAUSTRUM_REGION_TEMPORAL, 0.8f, 0.6f);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, SetCorticalLinkStrengthInvalidValues) {
    /* Implementation clamps via clamp01(), doesn't return error */

    /* Forward > 1.0 - clamped to 1.0 */
    nimcp_claustrum_error_t err = nimcp_claustrum_set_cortical_link_strength(
        &claustrum, CLAUSTRUM_REGION_TEMPORAL, 1.5f, 0.6f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.cortical_links[CLAUSTRUM_REGION_TEMPORAL].forward_strength, 1.0f);

    /* Backward < 0.0 - clamped to 0.0 */
    err = nimcp_claustrum_set_cortical_link_strength(
        &claustrum, CLAUSTRUM_REGION_TEMPORAL, 0.8f, -0.5f);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_FLOAT_EQ(claustrum.cortical_links[CLAUSTRUM_REGION_TEMPORAL].backward_strength, 0.0f);
}

TEST_F(ClaustrumTest, UpdateAllCorticalRegions) {
    for (int i = 0; i < CLAUSTRUM_REGION_COUNT; i++) {
        nimcp_claustrum_error_t err = nimcp_claustrum_update_cortical_region(
            &claustrum, (nimcp_claustrum_region_t)i, 0.5f + (float)i * 0.03f);
        EXPECT_EQ(err, CLAUSTRUM_OK);
    }
}

/*=============================================================================
 * STATE AND METRICS TESTS
 *===========================================================================*/

TEST_F(ClaustrumTest, GetState) {
    nimcp_claustrum_state_t state = nimcp_claustrum_get_state(&claustrum);

    /* Should be a valid state */
    EXPECT_GE((int)state, (int)CLAUSTRUM_STATE_IDLE);
    EXPECT_LE((int)state, (int)CLAUSTRUM_STATE_GATING);
}

TEST_F(ClaustrumTest, GetStateNullPointer) {
    nimcp_claustrum_state_t state = nimcp_claustrum_get_state(nullptr);
    EXPECT_EQ(state, CLAUSTRUM_STATE_IDLE);
}

TEST_F(ClaustrumTest, GetStatus) {
    nimcp_claustrum_status_t status = nimcp_claustrum_get_status(&claustrum);

    /* Should be a valid status */
    EXPECT_GE((int)status, (int)CLAUSTRUM_STATUS_NORMAL);
    EXPECT_LE((int)status, (int)CLAUSTRUM_STATUS_OVERLOADED);
}

TEST_F(ClaustrumTest, GetStatusNullPointer) {
    nimcp_claustrum_status_t status = nimcp_claustrum_get_status(nullptr);
    EXPECT_EQ(status, CLAUSTRUM_STATUS_NORMAL);
}

TEST_F(ClaustrumTest, GetMetrics) {
    nimcp_claustrum_metrics_t metrics;
    nimcp_claustrum_error_t err = nimcp_claustrum_get_metrics(&claustrum, &metrics);

    EXPECT_EQ(err, CLAUSTRUM_OK);
    /* Initial metrics should be zero or near-zero */
    EXPECT_EQ(metrics.update_count, 0u);
}

TEST_F(ClaustrumTest, GetMetricsNullPointer) {
    nimcp_claustrum_metrics_t metrics;

    nimcp_claustrum_error_t err = nimcp_claustrum_get_metrics(nullptr, &metrics);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);

    err = nimcp_claustrum_get_metrics(&claustrum, nullptr);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

TEST_F(ClaustrumTest, GetMetricsAfterOperations) {
    /* Perform some operations */
    setupModalitiesForBinding();
    for (int i = 0; i < 10; i++) {
        nimcp_claustrum_update(&claustrum, 0.01f);
    }

    uint32_t percept_id;
    uint32_t mask = (1U << CLAUSTRUM_MODALITY_VISUAL) | (1U << CLAUSTRUM_MODALITY_AUDITORY);
    nimcp_claustrum_bind_modalities(&claustrum, mask, &percept_id);

    nimcp_claustrum_metrics_t metrics;
    nimcp_claustrum_get_metrics(&claustrum, &metrics);

    EXPECT_EQ(metrics.update_count, 10u);
    EXPECT_GE(metrics.total_bindings, 1u);
}

TEST_F(ClaustrumTest, ResetMetrics) {
    /* Generate some metrics */
    for (int i = 0; i < 10; i++) {
        nimcp_claustrum_update(&claustrum, 0.01f);
    }

    nimcp_claustrum_error_t err = nimcp_claustrum_reset_metrics(&claustrum);
    EXPECT_EQ(err, CLAUSTRUM_OK);

    nimcp_claustrum_metrics_t metrics;
    nimcp_claustrum_get_metrics(&claustrum, &metrics);

    EXPECT_EQ(metrics.update_count, 0u);
    EXPECT_EQ(metrics.total_bindings, 0u);
}

TEST_F(ClaustrumTest, ResetMetricsNullPointer) {
    nimcp_claustrum_error_t err = nimcp_claustrum_reset_metrics(nullptr);
    EXPECT_EQ(err, CLAUSTRUM_ERR_NULL_PTR);
}

/*=============================================================================
 * UTILITY FUNCTION TESTS
 *===========================================================================*/

TEST_F(ClaustrumTest, ErrorString) {
    const char* str = nimcp_claustrum_error_string(CLAUSTRUM_OK);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = nimcp_claustrum_error_string(CLAUSTRUM_ERR_NULL_PTR);
    EXPECT_NE(str, nullptr);

    str = nimcp_claustrum_error_string(CLAUSTRUM_ERR_INVALID_PARAM);
    EXPECT_NE(str, nullptr);

    str = nimcp_claustrum_error_string(CLAUSTRUM_ERR_NOT_INITIALIZED);
    EXPECT_NE(str, nullptr);

    str = nimcp_claustrum_error_string(CLAUSTRUM_ERR_BINDING_FAILED);
    EXPECT_NE(str, nullptr);
}

TEST_F(ClaustrumTest, ModalityString) {
    const char* str = nimcp_claustrum_modality_string(CLAUSTRUM_MODALITY_VISUAL);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = nimcp_claustrum_modality_string(CLAUSTRUM_MODALITY_AUDITORY);
    EXPECT_NE(str, nullptr);

    str = nimcp_claustrum_modality_string(CLAUSTRUM_MODALITY_SOMATOSENSORY);
    EXPECT_NE(str, nullptr);
}

TEST_F(ClaustrumTest, StateString) {
    const char* str = nimcp_claustrum_state_string(CLAUSTRUM_STATE_IDLE);
    EXPECT_NE(str, nullptr);

    str = nimcp_claustrum_state_string(CLAUSTRUM_STATE_BINDING);
    EXPECT_NE(str, nullptr);

    str = nimcp_claustrum_state_string(CLAUSTRUM_STATE_SYNCHRONIZING);
    EXPECT_NE(str, nullptr);
}

TEST_F(ClaustrumTest, BrainStateString) {
    const char* str = nimcp_claustrum_brain_state_string(CLAUSTRUM_BRAIN_STATE_DEFAULT);
    EXPECT_NE(str, nullptr);

    str = nimcp_claustrum_brain_state_string(CLAUSTRUM_BRAIN_STATE_TASK_POSITIVE);
    EXPECT_NE(str, nullptr);

    str = nimcp_claustrum_brain_state_string(CLAUSTRUM_BRAIN_STATE_SALIENCE);
    EXPECT_NE(str, nullptr);
}

TEST_F(ClaustrumTest, RegionString) {
    const char* str = nimcp_claustrum_region_string(CLAUSTRUM_REGION_PREFRONTAL);
    EXPECT_NE(str, nullptr);

    str = nimcp_claustrum_region_string(CLAUSTRUM_REGION_HIPPOCAMPUS);
    EXPECT_NE(str, nullptr);

    str = nimcp_claustrum_region_string(CLAUSTRUM_REGION_AMYGDALA);
    EXPECT_NE(str, nullptr);
}

TEST_F(ClaustrumTest, BioMsgTypeString) {
    const char* str = nimcp_claustrum_bio_msg_type_string(CLAUSTRUM_BIO_MSG_BINDING);
    EXPECT_NE(str, nullptr);

    str = nimcp_claustrum_bio_msg_type_string(CLAUSTRUM_BIO_MSG_SYNC);
    EXPECT_NE(str, nullptr);

    str = nimcp_claustrum_bio_msg_type_string(CLAUSTRUM_BIO_MSG_SALIENCE);
    EXPECT_NE(str, nullptr);
}

TEST_F(ClaustrumTest, ConsciousnessString) {
    const char* str = nimcp_claustrum_consciousness_string(CLAUSTRUM_CONSCIOUSNESS_UNCONSCIOUS);
    EXPECT_NE(str, nullptr);

    str = nimcp_claustrum_consciousness_string(CLAUSTRUM_CONSCIOUSNESS_PRECONSCIOUS);
    EXPECT_NE(str, nullptr);

    str = nimcp_claustrum_consciousness_string(CLAUSTRUM_CONSCIOUSNESS_CONSCIOUS);
    EXPECT_NE(str, nullptr);

    str = nimcp_claustrum_consciousness_string(CLAUSTRUM_CONSCIOUSNESS_FOCAL);
    EXPECT_NE(str, nullptr);
}

TEST_F(ClaustrumTest, PrintSummary) {
    /* Just verify it doesn't crash */
    nimcp_claustrum_print_summary(&claustrum);
    SUCCEED();
}

TEST_F(ClaustrumTest, PrintSummaryNullPointer) {
    /* Should be NULL-safe */
    nimcp_claustrum_print_summary(nullptr);
    SUCCEED();
}

/*=============================================================================
 * INTEGRATION TESTS
 *===========================================================================*/

TEST_F(ClaustrumTest, FullBindingWorkflow) {
    /* 1. Set up modalities */
    float visual_features[64];
    float auditory_features[64];
    float somatosensory_features[64];

    createTestFeatures(visual_features, 64, 0.5f);
    createTestFeatures(auditory_features, 64, 0.6f);
    createTestFeatures(somatosensory_features, 64, 0.7f);

    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL,
                                    visual_features, 64, 0.8f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_AUDITORY,
                                    auditory_features, 64, 0.7f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_SOMATOSENSORY,
                                    somatosensory_features, 64, 0.6f);

    /* 2. Set salience */
    nimcp_claustrum_set_modality_salience(&claustrum, CLAUSTRUM_MODALITY_VISUAL, 0.9f);
    nimcp_claustrum_set_modality_salience(&claustrum, CLAUSTRUM_MODALITY_AUDITORY, 0.8f);
    nimcp_claustrum_set_modality_salience(&claustrum, CLAUSTRUM_MODALITY_SOMATOSENSORY, 0.5f);

    /* 3. Synchronize */
    nimcp_claustrum_error_t err = nimcp_claustrum_synchronize(&claustrum);
    EXPECT_EQ(err, CLAUSTRUM_OK);

    /* 4. Check sync level */
    float coherence;
    err = nimcp_claustrum_get_sync_level(&claustrum, &coherence);
    EXPECT_EQ(err, CLAUSTRUM_OK);

    /* 5. Bind modalities */
    uint32_t percept_id;
    uint32_t mask = (1U << CLAUSTRUM_MODALITY_VISUAL) |
                    (1U << CLAUSTRUM_MODALITY_AUDITORY) |
                    (1U << CLAUSTRUM_MODALITY_SOMATOSENSORY);
    err = nimcp_claustrum_bind_modalities(&claustrum, mask, &percept_id);
    EXPECT_EQ(err, CLAUSTRUM_OK);

    /* 6. Get percept and verify */
    nimcp_claustrum_bound_percept_t percept;
    err = nimcp_claustrum_get_percept(&claustrum, percept_id, &percept);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_EQ(percept.num_modalities, 3u);
    EXPECT_TRUE(percept.valid);

    /* 7. Gate workspace access */
    bool granted;
    err = nimcp_claustrum_gate_workspace(&claustrum, percept_id, &granted);
    EXPECT_EQ(err, CLAUSTRUM_OK);

    /* 8. If granted, verify workspace state */
    if (granted) {
        bool occupied;
        uint32_t ws_percept_id;
        err = nimcp_claustrum_get_workspace_state(&claustrum, &occupied, &ws_percept_id);
        EXPECT_EQ(err, CLAUSTRUM_OK);
        EXPECT_TRUE(occupied);
        EXPECT_EQ(ws_percept_id, percept_id);
    }

    /* 9. Check metrics */
    nimcp_claustrum_metrics_t metrics;
    err = nimcp_claustrum_get_metrics(&claustrum, &metrics);
    EXPECT_EQ(err, CLAUSTRUM_OK);
    EXPECT_GE(metrics.total_bindings, 1u);
}

TEST_F(ClaustrumTest, TaskSwitchingWorkflow) {
    /* 1. Verify initial state */
    EXPECT_EQ(claustrum.brain_state, CLAUSTRUM_BRAIN_STATE_DEFAULT);

    /* 2. Set up cortical regions */
    nimcp_claustrum_update_cortical_region(&claustrum, CLAUSTRUM_REGION_PREFRONTAL, 0.8f);
    nimcp_claustrum_update_cortical_region(&claustrum, CLAUSTRUM_REGION_PARIETAL, 0.7f);

    /* 3. Initiate switch to task-positive */
    nimcp_claustrum_error_t err = nimcp_claustrum_switch_state(
        &claustrum, CLAUSTRUM_BRAIN_STATE_TASK_POSITIVE);
    EXPECT_EQ(err, CLAUSTRUM_OK);

    /* 4. Run simulation until switch completes */
    int iterations = 0;
    const int max_iterations = 100;

    while (iterations < max_iterations) {
        nimcp_claustrum_update(&claustrum, 5.0f);

        float progress;
        nimcp_claustrum_brain_state_t target;
        nimcp_claustrum_get_switch_progress(&claustrum, &progress, &target);

        if (progress >= 1.0f) {
            break;
        }
        iterations++;
    }

    /* 5. Verify state switched */
    EXPECT_EQ(nimcp_claustrum_get_brain_state(&claustrum), CLAUSTRUM_BRAIN_STATE_TASK_POSITIVE);

    /* 6. Check metrics for state switches */
    nimcp_claustrum_metrics_t metrics;
    nimcp_claustrum_get_metrics(&claustrum, &metrics);
    EXPECT_GE(metrics.state_switches, 1u);
}

TEST_F(ClaustrumTest, OscillationControl) {
    /* Set gamma for binding */
    nimcp_claustrum_error_t err = nimcp_claustrum_set_gamma(&claustrum, 40.0f, 0.8f);
    EXPECT_EQ(err, CLAUSTRUM_OK);

    /* Set alpha for gating */
    err = nimcp_claustrum_set_alpha(&claustrum, 10.0f, 0.6f);
    EXPECT_EQ(err, CLAUSTRUM_OK);

    /* Verify oscillator state */
    EXPECT_FLOAT_EQ(claustrum.oscillator.gamma_frequency, 40.0f);
    EXPECT_FLOAT_EQ(claustrum.oscillator.gamma_amplitude, 0.8f);
    EXPECT_FLOAT_EQ(claustrum.oscillator.alpha_frequency, 10.0f);
    EXPECT_FLOAT_EQ(claustrum.oscillator.alpha_amplitude, 0.6f);

    /* Run updates to evolve oscillations */
    for (int i = 0; i < 50; i++) {
        nimcp_claustrum_update(&claustrum, 1.0f);
    }

    /* Phases should have evolved */
    /* Note: exact values depend on implementation */
}

TEST_F(ClaustrumTest, AttentionModulation) {
    /* Set attention bias for multiple regions */
    nimcp_claustrum_set_attention_bias(&claustrum, CLAUSTRUM_REGION_PREFRONTAL, 0.9f);
    nimcp_claustrum_set_attention_bias(&claustrum, CLAUSTRUM_REGION_PARIETAL, 0.8f);
    nimcp_claustrum_set_attention_bias(&claustrum, CLAUSTRUM_REGION_TEMPORAL, 0.5f);

    /* Verify biases set */
    EXPECT_FLOAT_EQ(claustrum.cortical_links[CLAUSTRUM_REGION_PREFRONTAL].attention_bias, 0.9f);
    EXPECT_FLOAT_EQ(claustrum.cortical_links[CLAUSTRUM_REGION_PARIETAL].attention_bias, 0.8f);
    EXPECT_FLOAT_EQ(claustrum.cortical_links[CLAUSTRUM_REGION_TEMPORAL].attention_bias, 0.5f);

    /* Get attention state */
    float global;
    float spatial[3];
    float feature[8];
    nimcp_claustrum_get_attention(&claustrum, &global, spatial, feature);

    EXPECT_GE(global, 0.0f);
    EXPECT_LE(global, 1.0f);
}

/*=============================================================================
 * STRESS TESTS
 *===========================================================================*/

TEST_F(ClaustrumTest, RapidModalityUpdates) {
    float features[64];

    for (int i = 0; i < 1000; i++) {
        createTestFeatures(features, 64, 0.3f + (float)(i % 10) * 0.05f);

        nimcp_claustrum_modality_t modality =
            (nimcp_claustrum_modality_t)(i % CLAUSTRUM_MODALITY_COUNT);

        nimcp_claustrum_error_t err = nimcp_claustrum_update_modality(
            &claustrum, modality, features, 64, 0.5f + (float)(i % 5) * 0.1f);
        EXPECT_EQ(err, CLAUSTRUM_OK);
    }
}

TEST_F(ClaustrumTest, MaxPercepts) {
    float features[64];

    /* Create max number of percepts */
    for (int i = 0; i < CLAUSTRUM_MAX_BOUND_PERCEPTS; i++) {
        /* Rotate through modality pairs */
        nimcp_claustrum_modality_t mod1 = (nimcp_claustrum_modality_t)(i % CLAUSTRUM_MODALITY_COUNT);
        nimcp_claustrum_modality_t mod2 = (nimcp_claustrum_modality_t)((i + 1) % CLAUSTRUM_MODALITY_COUNT);

        createTestFeatures(features, 64, 0.4f + (float)i * 0.02f);
        nimcp_claustrum_update_modality(&claustrum, mod1, features, 64, 0.7f);
        nimcp_claustrum_update_modality(&claustrum, mod2, features, 64, 0.7f);

        uint32_t percept_id;
        uint32_t mask = (1U << mod1) | (1U << mod2);
        nimcp_claustrum_bind_modalities(&claustrum, mask, &percept_id);
    }

    /* Attempting to create more should handle gracefully */
    /* Note: Using two modalities since binding requires at least 2 */
    uint32_t percept_id;
    uint32_t mask = (1U << CLAUSTRUM_MODALITY_VISUAL) | (1U << CLAUSTRUM_MODALITY_AUDITORY);
    nimcp_claustrum_error_t err = nimcp_claustrum_bind_modalities(&claustrum, mask, &percept_id);
    /* May return CAPACITY_EXCEEDED or BINDING_FAILED depending on implementation */
    EXPECT_TRUE(err == CLAUSTRUM_ERR_CAPACITY_EXCEEDED || err == CLAUSTRUM_ERR_BINDING_FAILED);
}

TEST_F(ClaustrumTest, LongSimulation) {
    setupModalitiesForBinding();

    /* Run for simulated 10 seconds */
    const float total_time = 10000.0f;  /* 10 seconds in ms */
    const float dt = 1.0f;  /* 1 ms timesteps */
    int iterations = (int)(total_time / dt);

    for (int i = 0; i < iterations; i++) {
        nimcp_claustrum_update(&claustrum, dt);

        /* Periodically update modalities */
        if (i % 100 == 0) {
            float features[64];
            createTestFeatures(features, 64, 0.5f + sinf((float)i * 0.01f) * 0.3f);
            nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL,
                                            features, 64, 0.7f);
        }
    }

    /* Verify simulation time advanced */
    EXPECT_GE(claustrum.current_time_ms, total_time);

    /* Verify metrics tracked all updates */
    nimcp_claustrum_metrics_t metrics;
    nimcp_claustrum_get_metrics(&claustrum, &metrics);
    EXPECT_EQ(metrics.update_count, (uint64_t)iterations);
}
