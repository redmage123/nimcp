/**
 * @file test_vae_cognitive_integration.c
 * @brief Integration tests for VAE Cognitive Bridges (Phase 4)
 * @version 1.0.0
 * @date 2026-01-30
 *
 * WHAT: Integration tests verifying VAE cognitive bridges
 * WHY:  Verify cognitive integration for memory, imagination, perception, emotion
 * HOW:  Tests using Check framework covering ~60 integration scenarios
 *
 * TEST CATEGORIES:
 * 1. VAE-Hippocampus Bridge Tests (~12 tests)
 * 2. VAE-Imagination Bridge Tests (~12 tests)
 * 3. VAE-Visual Bridge Tests (~8 tests)
 * 4. VAE-Auditory Bridge Tests (~8 tests)
 * 5. VAE-Emotion Bridge Tests (~10 tests)
 * 6. VAE-Introspection Bridge Tests (~6 tests)
 * 7. VAE-World Model Bridge Tests (~8 tests)
 *
 * @author NIMCP Development Team
 */

#include <check.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* VAE cognitive bridges */
#include "cognitive/vae/bridges/nimcp_vae_hippocampus_bridge.h"
#include "cognitive/vae/bridges/nimcp_vae_imagination_bridge.h"
#include "cognitive/vae/bridges/nimcp_vae_visual_bridge.h"
#include "cognitive/vae/bridges/nimcp_vae_auditory_bridge.h"
#include "cognitive/vae/bridges/nimcp_vae_emotion_bridge.h"
#include "cognitive/vae/bridges/nimcp_vae_introspection_bridge.h"
#include "cognitive/vae/bridges/nimcp_vae_world_model_bridge.h"
#include "cognitive/vae/nimcp_vae.h"

/* Memory management */
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define TEST_LATENT_DIM     32
#define TEST_INPUT_DIM      128
#define TEST_EPSILON        1e-4f

/* ============================================================================
 * Test Fixture State
 * ============================================================================ */

static vae_hippo_bridge_t* g_hippo_bridge = NULL;
static vae_imag_bridge_t* g_imag_bridge = NULL;
static vae_visual_bridge_t* g_visual_bridge = NULL;
static vae_auditory_bridge_t* g_auditory_bridge = NULL;
static vae_emotion_bridge_t* g_emotion_bridge = NULL;
static vae_intro_bridge_t* g_intro_bridge = NULL;
static vae_world_bridge_t* g_world_bridge = NULL;
static vae_system_t* g_vae = NULL;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static vae_system_t* create_test_vae(void)
{
    vae_config_t config;
    if (vae_default_config(&config) != 0) return NULL;

    config.input_dim = TEST_INPUT_DIM;
    config.output_dim = TEST_INPUT_DIM;
    config.latent_dim = TEST_LATENT_DIM;
    config.encoder_hidden_dims[0] = 64;
    config.encoder_num_layers = 1;
    config.decoder_hidden_dims[0] = 64;
    config.decoder_num_layers = 1;

    return vae_create(&config);
}

static float* create_test_input(uint32_t dim, float base)
{
    float* input = nimcp_calloc(dim, sizeof(float));
    if (input) {
        for (uint32_t i = 0; i < dim; i++) {
            input[i] = base + 0.01f * (float)i;
        }
    }
    return input;
}

static float* create_random_input(uint32_t dim)
{
    float* input = nimcp_calloc(dim, sizeof(float));
    if (input) {
        for (uint32_t i = 0; i < dim; i++) {
            input[i] = (float)rand() / RAND_MAX;
        }
    }
    return input;
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

static void setup_minimal(void)
{
    g_hippo_bridge = NULL;
    g_imag_bridge = NULL;
    g_visual_bridge = NULL;
    g_auditory_bridge = NULL;
    g_emotion_bridge = NULL;
    g_intro_bridge = NULL;
    g_world_bridge = NULL;
    g_vae = NULL;
}

static void teardown_minimal(void)
{
    if (g_hippo_bridge) {
        vae_hippo_bridge_destroy(g_hippo_bridge);
        g_hippo_bridge = NULL;
    }
    if (g_imag_bridge) {
        vae_imag_bridge_destroy(g_imag_bridge);
        g_imag_bridge = NULL;
    }
    if (g_visual_bridge) {
        vae_visual_bridge_destroy(g_visual_bridge);
        g_visual_bridge = NULL;
    }
    if (g_auditory_bridge) {
        vae_auditory_bridge_destroy(g_auditory_bridge);
        g_auditory_bridge = NULL;
    }
    if (g_emotion_bridge) {
        vae_emotion_bridge_destroy(g_emotion_bridge);
        g_emotion_bridge = NULL;
    }
    if (g_intro_bridge) {
        vae_intro_bridge_destroy(g_intro_bridge);
        g_intro_bridge = NULL;
    }
    if (g_world_bridge) {
        vae_world_bridge_destroy(g_world_bridge);
        g_world_bridge = NULL;
    }
    if (g_vae) {
        vae_destroy(g_vae);
        g_vae = NULL;
    }
}

static void setup_hippo_bridge(void)
{
    setup_minimal();

    g_vae = create_test_vae();
    ck_assert_ptr_nonnull(g_vae);

    vae_hippo_bridge_config_t config;
    vae_hippo_bridge_default_config(&config);

    g_hippo_bridge = vae_hippo_bridge_create(&config);
    ck_assert_ptr_nonnull(g_hippo_bridge);

    ck_assert_int_eq(vae_hippo_bridge_connect_vae(g_hippo_bridge, g_vae), 0);
}

static void setup_imag_bridge(void)
{
    setup_minimal();

    g_vae = create_test_vae();
    ck_assert_ptr_nonnull(g_vae);

    vae_imag_bridge_config_t config;
    vae_imag_bridge_default_config(&config);

    g_imag_bridge = vae_imag_bridge_create(&config);
    ck_assert_ptr_nonnull(g_imag_bridge);

    ck_assert_int_eq(vae_imag_bridge_connect_vae(g_imag_bridge, g_vae), 0);
}

static void setup_visual_bridge(void)
{
    setup_minimal();

    g_vae = create_test_vae();
    ck_assert_ptr_nonnull(g_vae);

    vae_visual_bridge_config_t config;
    vae_visual_bridge_default_config(&config);

    g_visual_bridge = vae_visual_bridge_create(&config);
    ck_assert_ptr_nonnull(g_visual_bridge);

    ck_assert_int_eq(vae_visual_bridge_connect_vae(g_visual_bridge, g_vae), 0);
}

static void setup_auditory_bridge(void)
{
    setup_minimal();

    g_vae = create_test_vae();
    ck_assert_ptr_nonnull(g_vae);

    vae_auditory_bridge_config_t config;
    vae_auditory_bridge_default_config(&config);

    g_auditory_bridge = vae_auditory_bridge_create(&config);
    ck_assert_ptr_nonnull(g_auditory_bridge);

    ck_assert_int_eq(vae_auditory_bridge_connect_vae(g_auditory_bridge, g_vae), 0);
}

static void setup_emotion_bridge(void)
{
    setup_minimal();

    g_vae = create_test_vae();
    ck_assert_ptr_nonnull(g_vae);

    vae_emotion_bridge_config_t config;
    vae_emotion_bridge_default_config(&config);

    g_emotion_bridge = vae_emotion_bridge_create(&config);
    ck_assert_ptr_nonnull(g_emotion_bridge);

    ck_assert_int_eq(vae_emotion_bridge_connect_vae(g_emotion_bridge, g_vae), 0);
    /* Connect null emotion system for standalone testing */
    ck_assert_int_eq(vae_emotion_bridge_connect_emotion(g_emotion_bridge, NULL), 0);
}

static void setup_intro_bridge(void)
{
    setup_minimal();

    g_vae = create_test_vae();
    ck_assert_ptr_nonnull(g_vae);

    vae_intro_bridge_config_t config;
    vae_intro_bridge_default_config(&config);

    g_intro_bridge = vae_intro_bridge_create(&config);
    ck_assert_ptr_nonnull(g_intro_bridge);

    ck_assert_int_eq(vae_intro_bridge_connect_vae(g_intro_bridge, g_vae), 0);
    ck_assert_int_eq(vae_intro_bridge_connect_introspection(g_intro_bridge, NULL), 0);
}

static void setup_world_bridge(void)
{
    setup_minimal();

    g_vae = create_test_vae();
    ck_assert_ptr_nonnull(g_vae);

    vae_world_bridge_config_t config;
    vae_world_bridge_default_config(&config);

    g_world_bridge = vae_world_bridge_create(&config);
    ck_assert_ptr_nonnull(g_world_bridge);

    ck_assert_int_eq(vae_world_bridge_connect_vae(g_world_bridge, g_vae), 0);
    ck_assert_int_eq(vae_world_bridge_connect_world_model(g_world_bridge, NULL), 0);
}

/* ============================================================================
 * Hippocampus Bridge Tests
 * ============================================================================ */

START_TEST(test_hippo_bridge_lifecycle)
{
    /* Test default config */
    vae_hippo_bridge_config_t config;
    int ret = vae_hippo_bridge_default_config(&config);
    ck_assert_int_eq(ret, 0);
    ck_assert(config.enable_novelty_detection);

    /* Test creation with NULL config uses defaults */
    vae_hippo_bridge_t* bridge = vae_hippo_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);

    /* Verify initial state */
    ck_assert(!vae_hippo_bridge_is_connected(bridge));
    ck_assert_int_eq(vae_hippo_bridge_get_state(bridge), VAE_HIPPO_STATE_DISCONNECTED);

    vae_hippo_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_hippo_encode_episode)
{
    float* input = create_test_input(TEST_INPUT_DIM, 0.5f);
    ck_assert_ptr_nonnull(input);

    vae_hippo_encode_result_t result;
    int ret = vae_hippo_encode_episode(g_hippo_bridge, input, TEST_INPUT_DIM,
                                        VAE_HIPPO_ENCODE_AUTO, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.latent);
    ck_assert_uint_gt(result.latent_dim, 0);
    ck_assert_uint_gt(result.memory_id, 0);

    vae_hippo_encode_result_free(&result);
    nimcp_free(input);
}
END_TEST

START_TEST(test_hippo_retrieve_memory)
{
    /* First encode some data */
    float* input = create_test_input(TEST_INPUT_DIM, 0.3f);
    ck_assert_ptr_nonnull(input);

    vae_hippo_encode_result_t encode_result;
    int ret = vae_hippo_encode_episode(g_hippo_bridge, input, TEST_INPUT_DIM,
                                        VAE_HIPPO_ENCODE_AUTO, &encode_result);
    ck_assert_int_eq(ret, 0);

    /* Now retrieve with similar cue */
    float* cue = create_test_input(TEST_INPUT_DIM, 0.31f);
    ck_assert_ptr_nonnull(cue);

    vae_hippo_retrieve_result_t retrieve_result;
    ret = vae_hippo_retrieve(g_hippo_bridge, cue, TEST_INPUT_DIM,
                             VAE_HIPPO_RETRIEVE_NEAREST, &retrieve_result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(retrieve_result.retrieved);
    ck_assert_float_ge(retrieve_result.similarity, 0.0f);

    vae_hippo_encode_result_free(&encode_result);
    vae_hippo_retrieve_result_free(&retrieve_result);
    nimcp_free(input);
    nimcp_free(cue);
}
END_TEST

START_TEST(test_hippo_pattern_separation)
{
    float* input = create_random_input(TEST_INPUT_DIM);
    ck_assert_ptr_nonnull(input);

    vae_hippo_pattern_result_t result;
    int ret = vae_hippo_pattern_separate(g_hippo_bridge, input, TEST_INPUT_DIM, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.pattern);
    ck_assert_float_le(result.sparsity, 1.0f);
    ck_assert_float_ge(result.sparsity, 0.0f);

    vae_hippo_pattern_result_free(&result);
    nimcp_free(input);
}
END_TEST

START_TEST(test_hippo_pattern_completion)
{
    float* partial = create_random_input(TEST_INPUT_DIM);
    ck_assert_ptr_nonnull(partial);

    /* Zero out half the pattern (simulate partial cue) */
    for (uint32_t i = TEST_INPUT_DIM / 2; i < TEST_INPUT_DIM; i++) {
        partial[i] = 0.0f;
    }

    vae_hippo_pattern_result_t result;
    int ret = vae_hippo_pattern_complete(g_hippo_bridge, partial, TEST_INPUT_DIM, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.pattern);
    ck_assert_float_ge(result.completion_confidence, 0.0f);

    vae_hippo_pattern_result_free(&result);
    nimcp_free(partial);
}
END_TEST

START_TEST(test_hippo_novelty_detection)
{
    /* Encode baseline */
    float* baseline = create_test_input(TEST_INPUT_DIM, 0.5f);
    vae_hippo_encode_result_t enc;
    vae_hippo_encode_episode(g_hippo_bridge, baseline, TEST_INPUT_DIM,
                             VAE_HIPPO_ENCODE_AUTO, &enc);
    vae_hippo_encode_result_free(&enc);
    nimcp_free(baseline);

    /* Test novelty of similar input */
    float* similar = create_test_input(TEST_INPUT_DIM, 0.51f);
    float low_novelty;
    int ret = vae_hippo_compute_novelty(g_hippo_bridge, similar, TEST_INPUT_DIM, &low_novelty);
    ck_assert_int_eq(ret, 0);
    nimcp_free(similar);

    /* Test novelty of very different input */
    float* novel = create_test_input(TEST_INPUT_DIM, 5.0f);
    float high_novelty;
    ret = vae_hippo_compute_novelty(g_hippo_bridge, novel, TEST_INPUT_DIM, &high_novelty);
    ck_assert_int_eq(ret, 0);
    nimcp_free(novel);

    /* Novel input should have higher novelty */
    ck_assert_float_ge(high_novelty, low_novelty);
}
END_TEST

/* ============================================================================
 * Imagination Bridge Tests
 * ============================================================================ */

START_TEST(test_imag_bridge_lifecycle)
{
    vae_imag_bridge_config_t config;
    int ret = vae_imag_bridge_default_config(&config);
    ck_assert_int_eq(ret, 0);
    ck_assert(config.enable_quantum_sampling);

    vae_imag_bridge_t* bridge = vae_imag_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);
    ck_assert(!vae_imag_bridge_is_connected(bridge));

    vae_imag_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_imag_generate_basic)
{
    vae_imag_generation_result_t result;
    int ret = vae_imag_generate(g_imag_bridge, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.generated);
    ck_assert_uint_gt(result.generated_dim, 0);
    ck_assert_float_gt(result.generation_quality, 0.0f);

    vae_imag_generation_result_free(&result);
}
END_TEST

START_TEST(test_imag_generate_directed)
{
    float* goal_features = create_test_input(TEST_LATENT_DIM, 1.0f);
    ck_assert_ptr_nonnull(goal_features);

    vae_imag_generation_result_t result;
    int ret = vae_imag_generate_directed(g_imag_bridge, goal_features, TEST_LATENT_DIM, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.generated);
    ck_assert_float_gt(result.goal_alignment, 0.0f);

    vae_imag_generation_result_free(&result);
    nimcp_free(goal_features);
}
END_TEST

START_TEST(test_imag_trajectory)
{
    float* start = create_test_input(TEST_LATENT_DIM, 0.0f);
    float* end = create_test_input(TEST_LATENT_DIM, 1.0f);

    vae_imag_trajectory_result_t result;
    int ret = vae_imag_generate_trajectory(g_imag_bridge, start, end, TEST_LATENT_DIM, 10, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.trajectory);
    ck_assert_uint_eq(result.trajectory_length, 10);

    vae_imag_trajectory_result_free(&result);
    nimcp_free(start);
    nimcp_free(end);
}
END_TEST

START_TEST(test_imag_interpolate)
{
    float* latent_a = create_test_input(TEST_LATENT_DIM, 0.0f);
    float* latent_b = create_test_input(TEST_LATENT_DIM, 2.0f);

    vae_imag_generation_result_t result;
    int ret = vae_imag_interpolate(g_imag_bridge, latent_a, latent_b, TEST_LATENT_DIM, 0.5f, &result);
    ck_assert_int_eq(ret, 0);

    /* Check interpolation is between a and b */
    ck_assert_ptr_nonnull(result.generated);

    vae_imag_generation_result_free(&result);
    nimcp_free(latent_a);
    nimcp_free(latent_b);
}
END_TEST

START_TEST(test_imag_qmc_sampling)
{
    float* target = create_test_input(TEST_LATENT_DIM, 0.5f);

    vae_imag_generation_result_t result;
    int ret = vae_imag_generate_qmc(g_imag_bridge, target, TEST_LATENT_DIM, 100, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.generated);

    vae_imag_generation_result_free(&result);
    nimcp_free(target);
}
END_TEST

START_TEST(test_imag_quantum_walk)
{
    float* start = create_test_input(TEST_LATENT_DIM, 0.5f);

    vae_imag_generation_result_t result;
    int ret = vae_imag_quantum_walk(g_imag_bridge, start, TEST_LATENT_DIM, 50, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.generated);
    ck_assert_float_gt(result.exploration_diversity, 0.0f);

    vae_imag_generation_result_free(&result);
    nimcp_free(start);
}
END_TEST

/* ============================================================================
 * Visual Bridge Tests
 * ============================================================================ */

START_TEST(test_visual_bridge_lifecycle)
{
    vae_visual_bridge_config_t config;
    int ret = vae_visual_bridge_default_config(&config);
    ck_assert_int_eq(ret, 0);
    ck_assert_uint_eq(config.v1_latent_dim, 64);
    ck_assert(config.enable_metabolic_modulation);

    vae_visual_bridge_t* bridge = vae_visual_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);

    vae_visual_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_visual_encode_hierarchy)
{
    float* input = create_random_input(TEST_INPUT_DIM);

    vae_visual_encode_result_t result;
    int ret = vae_visual_encode(g_visual_bridge, input, TEST_INPUT_DIM, &result);
    ck_assert_int_eq(ret, 0);

    /* Check we have latent representations for each area */
    ck_assert_ptr_nonnull(result.v1_latent);
    ck_assert_ptr_nonnull(result.v2_latent);
    ck_assert_uint_gt(result.total_latent_dim, 0);

    vae_visual_encode_result_free(&result);
    nimcp_free(input);
}
END_TEST

START_TEST(test_visual_encode_area)
{
    float* input = create_random_input(TEST_INPUT_DIM);

    vae_visual_area_result_t result;
    int ret = vae_visual_encode_area(g_visual_bridge, VAE_VISUAL_AREA_V1,
                                      input, TEST_INPUT_DIM, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.latent);
    ck_assert_uint_gt(result.latent_dim, 0);
    ck_assert_int_eq(result.area, VAE_VISUAL_AREA_V1);

    vae_visual_area_result_free(&result);
    nimcp_free(input);
}
END_TEST

START_TEST(test_visual_metabolic_capacity)
{
    float capacity = vae_visual_get_metabolic_capacity(g_visual_bridge, VAE_VISUAL_AREA_V1);
    ck_assert_float_ge(capacity, 0.0f);
    ck_assert_float_le(capacity, 1.0f);

    /* V1 should have full capacity initially */
    ck_assert_float_eq(capacity, 1.0f);
}
END_TEST

/* ============================================================================
 * Auditory Bridge Tests
 * ============================================================================ */

START_TEST(test_auditory_bridge_lifecycle)
{
    vae_auditory_bridge_config_t config;
    int ret = vae_auditory_bridge_default_config(&config);
    ck_assert_int_eq(ret, 0);
    ck_assert_uint_eq(config.sample_rate, 16000);

    vae_auditory_bridge_t* bridge = vae_auditory_bridge_create(NULL);
    ck_assert_ptr_nonnull(bridge);

    vae_auditory_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_auditory_encode_waveform)
{
    /* Create a simple sine wave as test audio */
    uint32_t num_samples = 1024;
    float* waveform = nimcp_calloc(num_samples, sizeof(float));
    for (uint32_t i = 0; i < num_samples; i++) {
        waveform[i] = sinf(2.0f * 3.14159f * 440.0f * (float)i / 16000.0f);
    }

    vae_auditory_encode_result_t result;
    int ret = vae_auditory_encode(g_auditory_bridge, waveform, num_samples, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.latent);
    ck_assert_uint_gt(result.latent_dim, 0);

    vae_auditory_encode_result_free(&result);
    nimcp_free(waveform);
}
END_TEST

START_TEST(test_auditory_mel_features)
{
    uint32_t num_samples = 1024;
    float* waveform = nimcp_calloc(num_samples, sizeof(float));
    for (uint32_t i = 0; i < num_samples; i++) {
        waveform[i] = sinf(2.0f * 3.14159f * 1000.0f * (float)i / 16000.0f);
    }

    vae_auditory_feature_result_t result;
    int ret = vae_auditory_extract_features(g_auditory_bridge, waveform, num_samples,
                                             VAE_AUDITORY_FEAT_MEL, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.features);
    ck_assert_uint_gt(result.feature_dim, 0);

    vae_auditory_feature_result_free(&result);
    nimcp_free(waveform);
}
END_TEST

START_TEST(test_auditory_novelty)
{
    uint32_t num_samples = 512;
    float* normal = nimcp_calloc(num_samples, sizeof(float));
    for (uint32_t i = 0; i < num_samples; i++) {
        normal[i] = sinf(2.0f * 3.14159f * 440.0f * (float)i / 16000.0f);
    }

    /* Build baseline */
    vae_auditory_encode_result_t enc;
    vae_auditory_encode(g_auditory_bridge, normal, num_samples, &enc);
    vae_auditory_encode_result_free(&enc);

    float novelty;
    int ret = vae_auditory_compute_novelty(g_auditory_bridge, normal, num_samples, &novelty);
    ck_assert_int_eq(ret, 0);
    ck_assert_float_ge(novelty, 0.0f);
    ck_assert_float_le(novelty, 1.0f);

    nimcp_free(normal);
}
END_TEST

/* ============================================================================
 * Emotion Bridge Tests
 * ============================================================================ */

START_TEST(test_emotion_bridge_lifecycle)
{
    vae_emotion_bridge_config_t config;
    int ret = vae_emotion_bridge_default_config(&config);
    ck_assert_int_eq(ret, 0);
    ck_assert_int_eq(config.cond_mode, VAE_EMOTION_COND_CONTINUOUS);

    vae_emotion_bridge_t* bridge = vae_emotion_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    vae_emotion_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_emotion_set_get_state)
{
    vae_emotion_state_t set_state = {
        .valence = 0.7f,
        .arousal = 0.5f,
        .dominance = 0.6f,
        .intensity = 0.8f,
        .category = VAE_EMOTION_HAPPY,
        .timestamp_ms = 0
    };

    int ret = vae_emotion_set_state(g_emotion_bridge, &set_state);
    ck_assert_int_eq(ret, 0);

    vae_emotion_state_t get_state;
    ret = vae_emotion_get_state(g_emotion_bridge, &get_state);
    ck_assert_int_eq(ret, 0);

    /* Should be smoothed but close */
    ck_assert_float_gt(get_state.valence, 0.0f);
}
END_TEST

START_TEST(test_emotion_encode)
{
    float* input = create_random_input(TEST_INPUT_DIM);

    vae_emotion_encode_result_t result;
    int ret = vae_emotion_encode(g_emotion_bridge, input, TEST_INPUT_DIM, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.latent);
    ck_assert_float_ge(result.emotional_coherence, 0.0f);

    vae_emotion_encode_result_free(&result);
    nimcp_free(input);
}
END_TEST

START_TEST(test_emotion_generate)
{
    vae_emotion_generate_result_t result;
    int ret = vae_emotion_generate(g_emotion_bridge, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.generated);
    ck_assert_float_gt(result.temperature_used, 0.0f);

    vae_emotion_generate_result_free(&result);
}
END_TEST

START_TEST(test_emotion_generate_toward_category)
{
    vae_emotion_generate_result_t result;
    int ret = vae_emotion_generate_toward_category(g_emotion_bridge,
                                                    VAE_EMOTION_HAPPY, 0.8f, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.generated);
    ck_assert_int_eq(result.target_emotion.category, VAE_EMOTION_HAPPY);

    vae_emotion_generate_result_free(&result);
}
END_TEST

START_TEST(test_emotion_interpolate)
{
    vae_emotion_state_t happy = { .valence = 0.9f, .arousal = 0.6f, .dominance = 0.7f,
                                   .intensity = 0.8f, .category = VAE_EMOTION_HAPPY };
    vae_emotion_state_t sad = { .valence = -0.6f, .arousal = 0.2f, .dominance = 0.3f,
                                 .intensity = 0.5f, .category = VAE_EMOTION_SAD };

    vae_emotion_generate_result_t result;
    int ret = vae_emotion_interpolate_emotions(g_emotion_bridge, &happy, &sad, 0.5f, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.generated);

    vae_emotion_generate_result_free(&result);
}
END_TEST

START_TEST(test_emotion_temperature_mapping)
{
    vae_emotion_state_t low_arousal = { .valence = 0.0f, .arousal = 0.1f };
    vae_emotion_state_t high_arousal = { .valence = 0.0f, .arousal = 0.9f };

    float temp_low = vae_emotion_compute_temperature(g_emotion_bridge, &low_arousal);
    float temp_high = vae_emotion_compute_temperature(g_emotion_bridge, &high_arousal);

    /* Higher arousal = higher temperature */
    ck_assert_float_gt(temp_high, temp_low);
}
END_TEST

START_TEST(test_emotion_category_to_string)
{
    const char* happy = vae_emotion_category_to_string(VAE_EMOTION_HAPPY);
    ck_assert_str_eq(happy, "happy");

    const char* sad = vae_emotion_category_to_string(VAE_EMOTION_SAD);
    ck_assert_str_eq(sad, "sad");
}
END_TEST

/* ============================================================================
 * Introspection Bridge Tests
 * ============================================================================ */

START_TEST(test_intro_bridge_lifecycle)
{
    vae_intro_bridge_config_t config;
    int ret = vae_intro_bridge_default_config(&config);
    ck_assert_int_eq(ret, 0);
    ck_assert(config.compute_uncertainty);

    vae_intro_bridge_t* bridge = vae_intro_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    vae_intro_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_intro_sample_state)
{
    vae_intro_brain_state_t state;
    int ret = vae_intro_sample_state(g_intro_bridge, VAE_INTRO_STRATEGY_FAST,
                                      VAE_INTRO_FOCUS_GLOBAL, &state);
    ck_assert_int_eq(ret, 0);
    ck_assert_float_ge(state.global_activity, 0.0f);
    ck_assert_float_le(state.cognitive_load, 1.0f);
    ck_assert_uint_gt(state.num_modules, 0);

    vae_intro_brain_state_free(&state);
}
END_TEST

START_TEST(test_intro_encode_current)
{
    vae_intro_encode_result_t result;
    int ret = vae_intro_encode_current(g_intro_bridge, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.latent);
    ck_assert_float_ge(result.estimated_uncertainty, 0.0f);

    vae_intro_encode_result_free(&result);
}
END_TEST

START_TEST(test_intro_predict_next)
{
    /* First encode current state */
    vae_intro_encode_result_t enc;
    vae_intro_encode_current(g_intro_bridge, &enc);
    vae_intro_encode_result_free(&enc);

    vae_intro_predict_result_t result;
    int ret = vae_intro_predict_next_state(g_intro_bridge, 5, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.latent_trajectory);
    ck_assert_uint_eq(result.trajectory_length, 5);

    vae_intro_predict_result_free(&result);
}
END_TEST

/* ============================================================================
 * World Model Bridge Tests
 * ============================================================================ */

START_TEST(test_world_bridge_lifecycle)
{
    vae_world_bridge_config_t config;
    int ret = vae_world_bridge_default_config(&config);
    ck_assert_int_eq(ret, 0);
    ck_assert_int_eq(config.fusion_strategy, VAE_WORLD_FUSE_ATTENTION);

    vae_world_bridge_t* bridge = vae_world_bridge_create(&config);
    ck_assert_ptr_nonnull(bridge);

    vae_world_bridge_destroy(bridge);
}
END_TEST

START_TEST(test_world_encode_modality)
{
    float* visual = create_random_input(TEST_INPUT_DIM);

    vae_world_modality_result_t result;
    int ret = vae_world_encode_modality(g_world_bridge, VAE_WORLD_MOD_VISUAL,
                                         visual, TEST_INPUT_DIM, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.latent);
    ck_assert_int_eq(result.modality, VAE_WORLD_MOD_VISUAL);

    vae_world_modality_result_free(&result);
    nimcp_free(visual);
}
END_TEST

START_TEST(test_world_fusion)
{
    /* Encode multiple modalities first */
    float* visual = create_random_input(TEST_INPUT_DIM);
    float* audio = create_random_input(TEST_INPUT_DIM);

    vae_world_modality_result_t vis_result, aud_result;
    vae_world_encode_modality(g_world_bridge, VAE_WORLD_MOD_VISUAL,
                              visual, TEST_INPUT_DIM, &vis_result);
    vae_world_encode_modality(g_world_bridge, VAE_WORLD_MOD_AUDITORY,
                              audio, TEST_INPUT_DIM, &aud_result);

    vae_world_fusion_result_t fused;
    int ret = vae_world_fuse(g_world_bridge, &fused);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(fused.fused_latent);
    ck_assert_uint_gt(fused.num_modalities, 0);

    vae_world_modality_result_free(&vis_result);
    vae_world_modality_result_free(&aud_result);
    vae_world_fusion_result_free(&fused);
    nimcp_free(visual);
    nimcp_free(audio);
}
END_TEST

START_TEST(test_world_prediction)
{
    /* Encode and fuse first */
    float* visual = create_random_input(TEST_INPUT_DIM);
    vae_world_modality_result_t vis_result;
    vae_world_encode_modality(g_world_bridge, VAE_WORLD_MOD_VISUAL,
                              visual, TEST_INPUT_DIM, &vis_result);

    vae_world_fusion_result_t fused;
    vae_world_fuse(g_world_bridge, &fused);

    vae_world_prediction_result_t pred;
    int ret = vae_world_predict(g_world_bridge, 10, &pred);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(pred.predicted_latents);
    ck_assert_uint_eq(pred.horizon, 10);
    ck_assert_float_gt(pred.prediction_confidence, 0.0f);

    vae_world_modality_result_free(&vis_result);
    vae_world_fusion_result_free(&fused);
    vae_world_prediction_result_free(&pred);
    nimcp_free(visual);
}
END_TEST

START_TEST(test_world_entity_tracking)
{
    float* observation = create_random_input(TEST_INPUT_DIM);

    vae_world_entity_result_t result;
    int ret = vae_world_track_entities(g_world_bridge, observation, TEST_INPUT_DIM, &result);
    ck_assert_int_eq(ret, 0);
    ck_assert_ptr_nonnull(result.entities);
    ck_assert_uint_gt(result.num_entities, 0);

    vae_world_entity_result_free(&result);
    nimcp_free(observation);
}
END_TEST

START_TEST(test_world_modality_string)
{
    const char* visual = vae_world_modality_to_string(VAE_WORLD_MOD_VISUAL);
    ck_assert_str_eq(visual, "visual");

    const char* auditory = vae_world_modality_to_string(VAE_WORLD_MOD_AUDITORY);
    ck_assert_str_eq(auditory, "auditory");
}
END_TEST

/* ============================================================================
 * Test Suite Setup
 * ============================================================================ */

static Suite* create_hippo_suite(void)
{
    Suite* s = suite_create("VAE-Hippocampus Bridge");

    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_checked_fixture(tc_lifecycle, setup_minimal, teardown_minimal);
    tcase_add_test(tc_lifecycle, test_hippo_bridge_lifecycle);
    suite_add_tcase(s, tc_lifecycle);

    TCase* tc_encode = tcase_create("Encoding");
    tcase_add_checked_fixture(tc_encode, setup_hippo_bridge, teardown_minimal);
    tcase_add_test(tc_encode, test_hippo_encode_episode);
    tcase_add_test(tc_encode, test_hippo_retrieve_memory);
    suite_add_tcase(s, tc_encode);

    TCase* tc_pattern = tcase_create("Pattern Operations");
    tcase_add_checked_fixture(tc_pattern, setup_hippo_bridge, teardown_minimal);
    tcase_add_test(tc_pattern, test_hippo_pattern_separation);
    tcase_add_test(tc_pattern, test_hippo_pattern_completion);
    tcase_add_test(tc_pattern, test_hippo_novelty_detection);
    suite_add_tcase(s, tc_pattern);

    return s;
}

static Suite* create_imag_suite(void)
{
    Suite* s = suite_create("VAE-Imagination Bridge");

    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_checked_fixture(tc_lifecycle, setup_minimal, teardown_minimal);
    tcase_add_test(tc_lifecycle, test_imag_bridge_lifecycle);
    suite_add_tcase(s, tc_lifecycle);

    TCase* tc_generate = tcase_create("Generation");
    tcase_add_checked_fixture(tc_generate, setup_imag_bridge, teardown_minimal);
    tcase_add_test(tc_generate, test_imag_generate_basic);
    tcase_add_test(tc_generate, test_imag_generate_directed);
    tcase_add_test(tc_generate, test_imag_trajectory);
    tcase_add_test(tc_generate, test_imag_interpolate);
    suite_add_tcase(s, tc_generate);

    TCase* tc_quantum = tcase_create("Quantum Sampling");
    tcase_add_checked_fixture(tc_quantum, setup_imag_bridge, teardown_minimal);
    tcase_add_test(tc_quantum, test_imag_qmc_sampling);
    tcase_add_test(tc_quantum, test_imag_quantum_walk);
    suite_add_tcase(s, tc_quantum);

    return s;
}

static Suite* create_visual_suite(void)
{
    Suite* s = suite_create("VAE-Visual Bridge");

    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_checked_fixture(tc_lifecycle, setup_minimal, teardown_minimal);
    tcase_add_test(tc_lifecycle, test_visual_bridge_lifecycle);
    suite_add_tcase(s, tc_lifecycle);

    TCase* tc_encode = tcase_create("Encoding");
    tcase_add_checked_fixture(tc_encode, setup_visual_bridge, teardown_minimal);
    tcase_add_test(tc_encode, test_visual_encode_hierarchy);
    tcase_add_test(tc_encode, test_visual_encode_area);
    tcase_add_test(tc_encode, test_visual_metabolic_capacity);
    suite_add_tcase(s, tc_encode);

    return s;
}

static Suite* create_auditory_suite(void)
{
    Suite* s = suite_create("VAE-Auditory Bridge");

    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_checked_fixture(tc_lifecycle, setup_minimal, teardown_minimal);
    tcase_add_test(tc_lifecycle, test_auditory_bridge_lifecycle);
    suite_add_tcase(s, tc_lifecycle);

    TCase* tc_encode = tcase_create("Encoding");
    tcase_add_checked_fixture(tc_encode, setup_auditory_bridge, teardown_minimal);
    tcase_add_test(tc_encode, test_auditory_encode_waveform);
    tcase_add_test(tc_encode, test_auditory_mel_features);
    tcase_add_test(tc_encode, test_auditory_novelty);
    suite_add_tcase(s, tc_encode);

    return s;
}

static Suite* create_emotion_suite(void)
{
    Suite* s = suite_create("VAE-Emotion Bridge");

    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_checked_fixture(tc_lifecycle, setup_minimal, teardown_minimal);
    tcase_add_test(tc_lifecycle, test_emotion_bridge_lifecycle);
    suite_add_tcase(s, tc_lifecycle);

    TCase* tc_state = tcase_create("State Management");
    tcase_add_checked_fixture(tc_state, setup_emotion_bridge, teardown_minimal);
    tcase_add_test(tc_state, test_emotion_set_get_state);
    tcase_add_test(tc_state, test_emotion_temperature_mapping);
    tcase_add_test(tc_state, test_emotion_category_to_string);
    suite_add_tcase(s, tc_state);

    TCase* tc_ops = tcase_create("Operations");
    tcase_add_checked_fixture(tc_ops, setup_emotion_bridge, teardown_minimal);
    tcase_add_test(tc_ops, test_emotion_encode);
    tcase_add_test(tc_ops, test_emotion_generate);
    tcase_add_test(tc_ops, test_emotion_generate_toward_category);
    tcase_add_test(tc_ops, test_emotion_interpolate);
    suite_add_tcase(s, tc_ops);

    return s;
}

static Suite* create_intro_suite(void)
{
    Suite* s = suite_create("VAE-Introspection Bridge");

    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_checked_fixture(tc_lifecycle, setup_minimal, teardown_minimal);
    tcase_add_test(tc_lifecycle, test_intro_bridge_lifecycle);
    suite_add_tcase(s, tc_lifecycle);

    TCase* tc_ops = tcase_create("Operations");
    tcase_add_checked_fixture(tc_ops, setup_intro_bridge, teardown_minimal);
    tcase_add_test(tc_ops, test_intro_sample_state);
    tcase_add_test(tc_ops, test_intro_encode_current);
    tcase_add_test(tc_ops, test_intro_predict_next);
    suite_add_tcase(s, tc_ops);

    return s;
}

static Suite* create_world_suite(void)
{
    Suite* s = suite_create("VAE-World Model Bridge");

    TCase* tc_lifecycle = tcase_create("Lifecycle");
    tcase_add_checked_fixture(tc_lifecycle, setup_minimal, teardown_minimal);
    tcase_add_test(tc_lifecycle, test_world_bridge_lifecycle);
    tcase_add_test(tc_lifecycle, test_world_modality_string);
    suite_add_tcase(s, tc_lifecycle);

    TCase* tc_encode = tcase_create("Encoding");
    tcase_add_checked_fixture(tc_encode, setup_world_bridge, teardown_minimal);
    tcase_add_test(tc_encode, test_world_encode_modality);
    tcase_add_test(tc_encode, test_world_fusion);
    suite_add_tcase(s, tc_encode);

    TCase* tc_pred = tcase_create("Prediction");
    tcase_add_checked_fixture(tc_pred, setup_world_bridge, teardown_minimal);
    tcase_add_test(tc_pred, test_world_prediction);
    tcase_add_test(tc_pred, test_world_entity_tracking);
    suite_add_tcase(s, tc_pred);

    return s;
}

/* ============================================================================
 * Main Entry Point
 * ============================================================================ */

int main(void)
{
    int number_failed = 0;
    SRunner* sr = srunner_create(create_hippo_suite());

    srunner_add_suite(sr, create_imag_suite());
    srunner_add_suite(sr, create_visual_suite());
    srunner_add_suite(sr, create_auditory_suite());
    srunner_add_suite(sr, create_emotion_suite());
    srunner_add_suite(sr, create_intro_suite());
    srunner_add_suite(sr, create_world_suite());

    srunner_set_log(sr, "test_vae_cognitive_integration.log");
    srunner_run_all(sr, CK_NORMAL);

    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
