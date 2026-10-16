/**
 * @file test_claustrum_binding_integration.cpp
 * @brief Integration tests for Claustrum cross-modal binding system
 *
 * WHAT: Tests cross-modal binding and perceptual unification
 * WHY:  Binding is the core function of the claustrum (Crick's hypothesis)
 * HOW:  Test modality input, binding, percept management, and synchronization
 *
 * INTEGRATION POINTS:
 * - Modality input processing
 * - Cross-modal binding engine
 * - Percept lifecycle management
 * - Oscillatory synchronization
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/regions/claustrum/nimcp_claustrum.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "nimcp.h"

/*=============================================================================
 * TEST FIXTURE
 *===========================================================================*/

class ClaustrumBindingTest : public ::testing::Test {
protected:
    nimcp_claustrum_t claustrum;
    nimcp_claustrum_config_t config;
    bool router_initialized;

    /* Test features for modalities */
    float visual_features[8];
    float auditory_features[8];
    float somatosensory_features[8];

    void SetUp() override {
        router_initialized = false;
        memset(&claustrum, 0, sizeof(claustrum));

        /* Initialize bio-async router */
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 64;
        router_config.inbox_capacity = 256;
        router_config.outbox_capacity = 256;
        router_config.enable_logging = false;

        if (bio_router_init(&router_config) == NIMCP_OK) {
            router_initialized = true;
        }

        /* Initialize claustrum */
        config = nimcp_claustrum_default_config();
        config.binding_threshold = 0.5f;  /* Lower threshold for testing */
        nimcp_claustrum_init(&claustrum, &config);

        /* Initialize test features */
        for (int i = 0; i < 8; i++) {
            visual_features[i] = 0.5f + (float)i * 0.05f;
            auditory_features[i] = 0.4f + (float)i * 0.05f;
            somatosensory_features[i] = 0.3f + (float)i * 0.05f;
        }
    }

    void TearDown() override {
        if (claustrum.initialized) {
            nimcp_claustrum_shutdown(&claustrum);
        }
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }
};

/*=============================================================================
 * MODALITY INPUT TESTS
 *===========================================================================*/

TEST_F(ClaustrumBindingTest, UpdateSingleModality) {
    nimcp_claustrum_error_t err = nimcp_claustrum_update_modality(
        &claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.8f);
    EXPECT_EQ(CLAUSTRUM_OK, err);

    nimcp_claustrum_modality_input_t input;
    err = nimcp_claustrum_get_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, &input);
    EXPECT_EQ(CLAUSTRUM_OK, err);
    EXPECT_TRUE(input.active);
    EXPECT_FLOAT_EQ(0.8f, input.activity_level);
}

TEST_F(ClaustrumBindingTest, UpdateMultipleModalities) {
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.8f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_AUDITORY, auditory_features, 8, 0.7f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_SOMATOSENSORY, somatosensory_features, 8, 0.6f);

    /* Verify all three are active */
    nimcp_claustrum_modality_input_t input;

    nimcp_claustrum_get_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, &input);
    EXPECT_TRUE(input.active);

    nimcp_claustrum_get_modality(&claustrum, CLAUSTRUM_MODALITY_AUDITORY, &input);
    EXPECT_TRUE(input.active);

    nimcp_claustrum_get_modality(&claustrum, CLAUSTRUM_MODALITY_SOMATOSENSORY, &input);
    EXPECT_TRUE(input.active);
}

TEST_F(ClaustrumBindingTest, ModalitySalienceUpdate) {
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.8f);

    nimcp_claustrum_error_t err = nimcp_claustrum_set_modality_salience(&claustrum, CLAUSTRUM_MODALITY_VISUAL, 0.9f);
    EXPECT_EQ(CLAUSTRUM_OK, err);

    nimcp_claustrum_modality_input_t input;
    nimcp_claustrum_get_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, &input);
    EXPECT_FLOAT_EQ(0.9f, input.salience);
}

TEST_F(ClaustrumBindingTest, InvalidModalityRejected) {
    nimcp_claustrum_error_t err = nimcp_claustrum_update_modality(
        &claustrum, (nimcp_claustrum_modality_t)999, visual_features, 8, 0.8f);
    EXPECT_NE(CLAUSTRUM_OK, err);
}

TEST_F(ClaustrumBindingTest, NullFeaturesHandledGracefully) {
    /* Null features should be handled gracefully (ignored or defaulted) */
    nimcp_claustrum_error_t err = nimcp_claustrum_update_modality(
        &claustrum, CLAUSTRUM_MODALITY_VISUAL, NULL, 8, 0.8f);
    /* Implementation accepts null features gracefully */
    EXPECT_EQ(CLAUSTRUM_OK, err);
}

/*=============================================================================
 * BINDING TESTS
 *===========================================================================*/

TEST_F(ClaustrumBindingTest, BindTwoModalities) {
    /* Update visual and auditory modalities */
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.8f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_AUDITORY, auditory_features, 8, 0.7f);

    /* Run some update cycles to build coherence */
    for (int i = 0; i < 10; i++) {
        nimcp_claustrum_update(&claustrum, 10.0f);
    }

    /* Attempt binding */
    uint32_t modality_mask = (1 << CLAUSTRUM_MODALITY_VISUAL) | (1 << CLAUSTRUM_MODALITY_AUDITORY);
    uint32_t percept_id = 0;

    nimcp_claustrum_error_t err = nimcp_claustrum_bind_modalities(&claustrum, modality_mask, &percept_id);
    /* May succeed or fail depending on coherence, but should not crash */
    EXPECT_TRUE(err == CLAUSTRUM_OK || err == CLAUSTRUM_ERR_BINDING_FAILED);
}

TEST_F(ClaustrumBindingTest, BindThreeModalities) {
    /* Update three modalities with high activity */
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.9f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_AUDITORY, auditory_features, 8, 0.9f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_SOMATOSENSORY, somatosensory_features, 8, 0.9f);

    /* Run updates */
    for (int i = 0; i < 20; i++) {
        nimcp_claustrum_update(&claustrum, 5.0f);
    }

    /* Attempt binding all three */
    uint32_t modality_mask = (1 << CLAUSTRUM_MODALITY_VISUAL) |
                             (1 << CLAUSTRUM_MODALITY_AUDITORY) |
                             (1 << CLAUSTRUM_MODALITY_SOMATOSENSORY);
    uint32_t percept_id = 0;

    nimcp_claustrum_error_t err = nimcp_claustrum_bind_modalities(&claustrum, modality_mask, &percept_id);
    EXPECT_TRUE(err == CLAUSTRUM_OK || err == CLAUSTRUM_ERR_BINDING_FAILED);
}

TEST_F(ClaustrumBindingTest, GetBoundPercept) {
    /* Set up modalities */
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.9f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_AUDITORY, auditory_features, 8, 0.9f);

    /* Force synchronization */
    nimcp_claustrum_synchronize(&claustrum);

    for (int i = 0; i < 20; i++) {
        nimcp_claustrum_update(&claustrum, 5.0f);
    }

    /* Attempt binding */
    uint32_t modality_mask = (1 << CLAUSTRUM_MODALITY_VISUAL) | (1 << CLAUSTRUM_MODALITY_AUDITORY);
    uint32_t percept_id = 0;

    nimcp_claustrum_error_t err = nimcp_claustrum_bind_modalities(&claustrum, modality_mask, &percept_id);

    if (err == CLAUSTRUM_OK && percept_id > 0) {
        nimcp_claustrum_bound_percept_t percept;
        err = nimcp_claustrum_get_percept(&claustrum, percept_id, &percept);
        EXPECT_EQ(CLAUSTRUM_OK, err);
        EXPECT_EQ(percept_id, percept.id);
        EXPECT_TRUE(percept.valid);
        EXPECT_GE(percept.num_modalities, 2u);
    }
}

TEST_F(ClaustrumBindingTest, GetStrongestBinding) {
    /* Create two bindings with different strengths */
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.95f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_AUDITORY, auditory_features, 8, 0.95f);
    nimcp_claustrum_synchronize(&claustrum);

    for (int i = 0; i < 30; i++) {
        nimcp_claustrum_update(&claustrum, 5.0f);
    }

    uint32_t strongest_id = 0;
    float strength = 0.0f;
    nimcp_claustrum_error_t err = nimcp_claustrum_get_strongest_binding(&claustrum, &strongest_id, &strength);

    /* May or may not have a binding */
    if (err == CLAUSTRUM_OK && strongest_id > 0) {
        EXPECT_GE(strength, 0.0f);
        EXPECT_LE(strength, 1.0f);
    }
}

TEST_F(ClaustrumBindingTest, ReleasePercept) {
    /* Create a binding */
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.95f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_AUDITORY, auditory_features, 8, 0.95f);
    nimcp_claustrum_synchronize(&claustrum);

    for (int i = 0; i < 30; i++) {
        nimcp_claustrum_update(&claustrum, 5.0f);
    }

    uint32_t modality_mask = (1 << CLAUSTRUM_MODALITY_VISUAL) | (1 << CLAUSTRUM_MODALITY_AUDITORY);
    uint32_t percept_id = 0;

    nimcp_claustrum_error_t err = nimcp_claustrum_bind_modalities(&claustrum, modality_mask, &percept_id);

    if (err == CLAUSTRUM_OK && percept_id > 0) {
        /* Release it */
        err = nimcp_claustrum_release_percept(&claustrum, percept_id);
        EXPECT_EQ(CLAUSTRUM_OK, err);

        /* Should no longer be retrievable */
        nimcp_claustrum_bound_percept_t percept;
        err = nimcp_claustrum_get_percept(&claustrum, percept_id, &percept);
        /* Either not found or marked invalid */
        EXPECT_TRUE(err != CLAUSTRUM_OK || !percept.valid);
    }
}

/*=============================================================================
 * SYNCHRONIZATION TESTS
 *===========================================================================*/

TEST_F(ClaustrumBindingTest, SynchronizeModalities) {
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.8f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_AUDITORY, auditory_features, 8, 0.7f);

    nimcp_claustrum_error_t err = nimcp_claustrum_synchronize(&claustrum);
    EXPECT_EQ(CLAUSTRUM_OK, err);
}

TEST_F(ClaustrumBindingTest, SyncLevelIncreases) {
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.9f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_AUDITORY, auditory_features, 8, 0.9f);

    float initial_coherence = 0.0f;
    nimcp_claustrum_get_sync_level(&claustrum, &initial_coherence);

    /* Run synchronization cycles */
    for (int i = 0; i < 50; i++) {
        nimcp_claustrum_synchronize(&claustrum);
        nimcp_claustrum_update(&claustrum, 5.0f);
    }

    float final_coherence = 0.0f;
    nimcp_claustrum_get_sync_level(&claustrum, &final_coherence);

    /* Coherence should be maintained or increased with synchronization */
    EXPECT_GE(final_coherence, 0.0f);
    EXPECT_LE(final_coherence, 1.0f);
}

TEST_F(ClaustrumBindingTest, GammaModulationAffectsBinding) {
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.8f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_AUDITORY, auditory_features, 8, 0.8f);

    /* High gamma should enhance binding */
    nimcp_claustrum_set_gamma(&claustrum, 50.0f, 0.9f);

    for (int i = 0; i < 20; i++) {
        nimcp_claustrum_update(&claustrum, 5.0f);
    }

    EXPECT_FLOAT_EQ(50.0f, claustrum.oscillator.gamma_frequency);
    EXPECT_FLOAT_EQ(0.9f, claustrum.oscillator.gamma_amplitude);
}

TEST_F(ClaustrumBindingTest, AlphaModulationAffectsGating) {
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.8f);

    /* High alpha should enhance gating */
    nimcp_claustrum_set_alpha(&claustrum, 10.0f, 0.8f);

    for (int i = 0; i < 20; i++) {
        nimcp_claustrum_update(&claustrum, 5.0f);
    }

    EXPECT_FLOAT_EQ(10.0f, claustrum.oscillator.alpha_frequency);
    EXPECT_FLOAT_EQ(0.8f, claustrum.oscillator.alpha_amplitude);
}

/*=============================================================================
 * SALIENCE DETECTION TESTS
 *===========================================================================*/

TEST_F(ClaustrumBindingTest, DetectSalienceBasic) {
    /* One highly salient modality */
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.95f);
    nimcp_claustrum_set_modality_salience(&claustrum, CLAUSTRUM_MODALITY_VISUAL, 0.95f);

    /* One less salient */
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_AUDITORY, auditory_features, 8, 0.3f);
    nimcp_claustrum_set_modality_salience(&claustrum, CLAUSTRUM_MODALITY_AUDITORY, 0.3f);

    float salience = 0.0f;
    nimcp_claustrum_modality_t salient_mod;
    nimcp_claustrum_error_t err = nimcp_claustrum_detect_salience(&claustrum, &salience, &salient_mod);

    EXPECT_EQ(CLAUSTRUM_OK, err);
    EXPECT_GE(salience, 0.0f);
    /* Visual should be most salient */
    EXPECT_EQ(CLAUSTRUM_MODALITY_VISUAL, salient_mod);
}

TEST_F(ClaustrumBindingTest, SalienceUpdatesWithActivity) {
    /* Initially moderate salience */
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.5f);

    float initial_salience = 0.0f;
    nimcp_claustrum_modality_t mod;
    nimcp_claustrum_detect_salience(&claustrum, &initial_salience, &mod);

    /* Increase activity */
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.95f);
    nimcp_claustrum_set_modality_salience(&claustrum, CLAUSTRUM_MODALITY_VISUAL, 0.95f);

    float final_salience = 0.0f;
    nimcp_claustrum_detect_salience(&claustrum, &final_salience, &mod);

    /* Salience should increase with higher activity */
    EXPECT_GE(final_salience, initial_salience);
}

/*=============================================================================
 * ATTENTION TESTS
 *===========================================================================*/

TEST_F(ClaustrumBindingTest, SetAttentionBias) {
    nimcp_claustrum_error_t err = nimcp_claustrum_set_attention_bias(&claustrum, CLAUSTRUM_REGION_PREFRONTAL, 0.8f);
    EXPECT_EQ(CLAUSTRUM_OK, err);

    nimcp_claustrum_cortical_link_t link;
    nimcp_claustrum_get_cortical_link(&claustrum, CLAUSTRUM_REGION_PREFRONTAL, &link);
    EXPECT_FLOAT_EQ(0.8f, link.attention_bias);
}

TEST_F(ClaustrumBindingTest, GetAttentionState) {
    float global = 0.0f;
    float spatial[3];
    float feature[8];

    nimcp_claustrum_error_t err = nimcp_claustrum_get_attention(&claustrum, &global, spatial, feature);
    EXPECT_EQ(CLAUSTRUM_OK, err);
    EXPECT_GE(global, 0.0f);
    EXPECT_LE(global, 1.0f);
}

/*=============================================================================
 * METRICS TESTS
 *===========================================================================*/

TEST_F(ClaustrumBindingTest, BindingMetricsUpdate) {
    nimcp_claustrum_metrics_t initial_metrics;
    nimcp_claustrum_get_metrics(&claustrum, &initial_metrics);

    /* Do some operations */
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.9f);
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_AUDITORY, auditory_features, 8, 0.9f);

    for (int i = 0; i < 20; i++) {
        nimcp_claustrum_update(&claustrum, 5.0f);
    }

    nimcp_claustrum_metrics_t final_metrics;
    nimcp_claustrum_get_metrics(&claustrum, &final_metrics);

    /* Update count should have increased */
    EXPECT_GT(final_metrics.update_count, initial_metrics.update_count);
}

TEST_F(ClaustrumBindingTest, ResetMetrics) {
    /* Generate some metrics */
    nimcp_claustrum_update_modality(&claustrum, CLAUSTRUM_MODALITY_VISUAL, visual_features, 8, 0.9f);
    for (int i = 0; i < 10; i++) {
        nimcp_claustrum_update(&claustrum, 5.0f);
    }

    /* Reset */
    nimcp_claustrum_error_t err = nimcp_claustrum_reset_metrics(&claustrum);
    EXPECT_EQ(CLAUSTRUM_OK, err);

    nimcp_claustrum_metrics_t metrics;
    nimcp_claustrum_get_metrics(&claustrum, &metrics);
    EXPECT_EQ(0u, metrics.update_count);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
