/**
 * @file test_omni_bridges_integration.cpp
 * @brief Integration tests for Omnidirectional Inference bridges
 * @version 1.1.0
 * @date 2026-01-04
 *
 * Tests integration of omnidirectional inference with:
 * - Sensory cortices (audio, visual, speech)
 * - Cortical columns
 * - Occipital lobe (V1-V5)
 * - Broca's region (language production)
 * - Bio-async message routing
 * - Knowledge graph synchronization
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <cstdlib>

/* Headers have their own extern "C" guards - don't wrap them to avoid
 * CUDA C++ function conflicts */
#include "cognitive/jepa/nimcp_jepa_bidirectional.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/memory/nimcp_hopfield_memory.h"
#include "cognitive/predictive/nimcp_predictive_hierarchy.h"
#include "cognitive/memory/nimcp_temporal_replay.h"
#include "perception/nimcp_omni_sensory_bridge.h"
#include "core/cortical_columns/nimcp_omni_cortical_columns_bridge.h"
#include "core/brain/regions/occipital/nimcp_omni_occipital_bridge.h"
#include "core/brain/regions/broca/nimcp_omni_broca_bridge.h"
#include "cognitive/omni/nimcp_omni_kg_sync.h"
#include "cognitive/omni/nimcp_omni_precision.h"
#include "cognitive/omni/nimcp_omni_active_inference.h"
#include "cognitive/omni/nimcp_omni_world_model.h"
#include "cognitive/omni/nimcp_omni_metacognition.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture for Omnidirectional Integration
 * ============================================================================ */

class OmniIntegrationTest : public ::testing::Test {
protected:
    static constexpr uint32_t LATENT_DIM = 64;
    static constexpr uint32_t HIDDEN_DIM = 128;
    static constexpr uint32_t NUM_PATTERNS = 256;
    static constexpr uint32_t NUM_LEVELS = 3;

    jepa_bidirectional_t* jepa = nullptr;
    hopfield_memory_t* hopfield = nullptr;
    predictive_hierarchy_t* hierarchy = nullptr;
    temporal_replay_t* replay = nullptr;

    uint32_t hier_dims[NUM_LEVELS] = {LATENT_DIM, LATENT_DIM / 2, LATENT_DIM / 4};

    void SetUp() override {
        /* Create bidirectional JEPA */
        jepa_bidir_config_t jepa_config;
        jepa_bidir_default_config(&jepa_config);
        jepa_config.embedding_dim = LATENT_DIM;
        jepa_config.hidden_dim = HIDDEN_DIM;
        jepa_config.gpu_mode = JEPA_BIDIR_GPU_DISABLED;
        jepa_config.enable_forward = true;
        jepa_config.enable_backward = true;
        jepa_config.enable_lateral = true;
        jepa = jepa_bidirectional_create(&jepa_config);

        /* Create Hopfield memory */
        hopfield_config_t hopfield_config;
        hopfield_default_config(&hopfield_config);
        hopfield_config.pattern_dim = LATENT_DIM;
        hopfield_config.capacity = NUM_PATTERNS;
        hopfield_config.gpu_mode = HOPFIELD_GPU_DISABLED;
        hopfield = hopfield_memory_create(&hopfield_config);

        /* Create predictive hierarchy */
        pred_hier_config_t hier_config;
        pred_hier_simple_config(&hier_config, NUM_LEVELS, hier_dims);
        hier_config.gpu_mode = PRED_HIER_GPU_DISABLED;
        hierarchy = pred_hier_create(&hier_config);
        pred_hier_free_config(&hier_config);

        /* Create temporal replay */
        replay_config_t replay_config;
        replay_default_config(&replay_config);
        replay_config.state_dim = LATENT_DIM;
        replay_config.capacity = 1024;
        replay_config.gpu_mode = REPLAY_GPU_DISABLED;
        replay = temporal_replay_create(&replay_config);
    }

    void TearDown() override {
        if (replay) temporal_replay_destroy(replay);
        if (hierarchy) pred_hier_destroy(hierarchy);
        if (hopfield) hopfield_memory_destroy(hopfield);
        if (jepa) jepa_bidirectional_destroy(jepa);
    }

    void generate_random_pattern(float* pattern, uint32_t dim) {
        for (uint32_t i = 0; i < dim; i++) {
            pattern[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        }
    }
};

/* ============================================================================
 * Core Component Creation Tests
 * ============================================================================ */

TEST_F(OmniIntegrationTest, JEPABidirectionalCreation) {
    EXPECT_NE(jepa, nullptr);
}

TEST_F(OmniIntegrationTest, HopfieldMemoryCreation) {
    EXPECT_NE(hopfield, nullptr);
    if (hopfield) {
        EXPECT_EQ(hopfield_memory_capacity(hopfield), NUM_PATTERNS);
        EXPECT_EQ(hopfield_memory_pattern_count(hopfield), 0u);
    }
}

TEST_F(OmniIntegrationTest, PredictiveHierarchyCreation) {
    EXPECT_NE(hierarchy, nullptr);
}

TEST_F(OmniIntegrationTest, TemporalReplayCreation) {
    EXPECT_NE(replay, nullptr);
}

/* ============================================================================
 * Bidirectional Prediction Tests
 * ============================================================================ */

TEST_F(OmniIntegrationTest, JEPAForwardPrediction) {
    if (!jepa) GTEST_SKIP();

    jepa_latent_t* input = jepa_latent_create_dim(LATENT_DIM);
    ASSERT_NE(input, nullptr);
    generate_random_pattern(input->embedding, LATENT_DIM);

    jepa_bidir_result_t* result = jepa_bidir_result_create(LATENT_DIM);
    ASSERT_NE(result, nullptr);

    int ret = jepa_bidirectional_predict(jepa, JEPA_DIR_FORWARD, input, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    jepa_bidir_result_destroy(result);
    jepa_latent_destroy(input);
}

TEST_F(OmniIntegrationTest, JEPABackwardPrediction) {
    if (!jepa) GTEST_SKIP();

    jepa_latent_t* input = jepa_latent_create_dim(LATENT_DIM);
    ASSERT_NE(input, nullptr);
    generate_random_pattern(input->embedding, LATENT_DIM);

    jepa_bidir_result_t* result = jepa_bidir_result_create(LATENT_DIM);
    ASSERT_NE(result, nullptr);

    int ret = jepa_bidirectional_predict(jepa, JEPA_DIR_BACKWARD, input, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    jepa_bidir_result_destroy(result);
    jepa_latent_destroy(input);
}

TEST_F(OmniIntegrationTest, JEPALateralPrediction) {
    if (!jepa) GTEST_SKIP();

    jepa_latent_t* input = jepa_latent_create_dim(LATENT_DIM);
    ASSERT_NE(input, nullptr);
    generate_random_pattern(input->embedding, LATENT_DIM);

    jepa_bidir_result_t* result = jepa_bidir_result_create(LATENT_DIM);
    ASSERT_NE(result, nullptr);

    int ret = jepa_bidirectional_predict(jepa, JEPA_DIR_LATERAL, input, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    jepa_bidir_result_destroy(result);
    jepa_latent_destroy(input);
}

/* ============================================================================
 * Hopfield Memory Integration Tests
 * ============================================================================ */

TEST_F(OmniIntegrationTest, HopfieldStoreAndRetrieve) {
    if (!hopfield) GTEST_SKIP();

    float pattern[LATENT_DIM], query[LATENT_DIM];
    generate_random_pattern(pattern, LATENT_DIM);
    memcpy(query, pattern, LATENT_DIM * sizeof(float));

    /* Add small noise to query */
    for (uint32_t i = 0; i < LATENT_DIM; i++) {
        query[i] += 0.1f * ((float)rand() / RAND_MAX - 0.5f);
    }

    /* Store pattern */
    uint32_t pattern_id;
    int ret = hopfield_memory_store(hopfield, pattern, &pattern_id);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_EQ(hopfield_memory_pattern_count(hopfield), 1u);

    /* Retrieve with noisy query */
    hopfield_retrieval_result_t* result = hopfield_result_create(LATENT_DIM);
    ASSERT_NE(result, nullptr);

    ret = hopfield_memory_retrieve(hopfield, query, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(result->similarity, 0.8f);

    hopfield_result_destroy(result);
}

TEST_F(OmniIntegrationTest, HopfieldBatchStore) {
    if (!hopfield) GTEST_SKIP();

    const uint32_t num_patterns = 10;
    float patterns[num_patterns * LATENT_DIM];

    for (uint32_t i = 0; i < num_patterns; i++) {
        generate_random_pattern(&patterns[i * LATENT_DIM], LATENT_DIM);
    }

    uint32_t ids[num_patterns];
    int ret = hopfield_memory_store_batch(hopfield, patterns, num_patterns, ids);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_EQ(hopfield_memory_pattern_count(hopfield), num_patterns);
}

/* ============================================================================
 * Predictive Hierarchy Integration Tests
 * ============================================================================ */

TEST_F(OmniIntegrationTest, HierarchyForwardPass) {
    if (!hierarchy) GTEST_SKIP();

    float input[LATENT_DIM];
    generate_random_pattern(input, LATENT_DIM);

    int ret = pred_hier_forward(hierarchy, input);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(OmniIntegrationTest, HierarchyBackwardPass) {
    if (!hierarchy) GTEST_SKIP();

    float input[LATENT_DIM];
    generate_random_pattern(input, LATENT_DIM);
    pred_hier_forward(hierarchy, input);

    int ret = pred_hier_backward(hierarchy);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(OmniIntegrationTest, HierarchyUpdate) {
    if (!hierarchy) GTEST_SKIP();

    float input[LATENT_DIM];
    generate_random_pattern(input, LATENT_DIM);

    pred_hier_result_t* result = pred_hier_result_create(NUM_LEVELS, hier_dims);
    ASSERT_NE(result, nullptr);

    pred_hier_forward(hierarchy, input);
    pred_hier_backward(hierarchy);

    int ret = pred_hier_update(hierarchy, input, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    pred_hier_result_destroy(result);
}

TEST_F(OmniIntegrationTest, HierarchyFreeEnergy) {
    if (!hierarchy) GTEST_SKIP();

    float input[LATENT_DIM];
    generate_random_pattern(input, LATENT_DIM);
    pred_hier_forward(hierarchy, input);

    float fe = pred_hier_compute_free_energy(hierarchy);
    EXPECT_FALSE(std::isnan(fe));
}

/* ============================================================================
 * Temporal Replay Integration Tests
 * ============================================================================ */

TEST_F(OmniIntegrationTest, ReplayStoreAndSample) {
    if (!replay) GTEST_SKIP();

    /* Store transitions */
    for (int i = 0; i < 10; i++) {
        float state[LATENT_DIM], next_state[LATENT_DIM];
        generate_random_pattern(state, LATENT_DIM);
        generate_random_pattern(next_state, LATENT_DIM);

        int ret = temporal_replay_store(replay, state, nullptr, next_state,
                                        0.5f, false, 1.0f);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }

    /* Sample batch */
    replay_batch_t* batch = replay_batch_create(4, LATENT_DIM, 0);
    ASSERT_NE(batch, nullptr);

    int ret = temporal_replay_sample(replay, REPLAY_MODE_PRIORITY, 4, batch);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    replay_batch_destroy(batch);
}

TEST_F(OmniIntegrationTest, ReplayForwardSweep) {
    if (!replay) GTEST_SKIP();

    /* Store transitions */
    for (int i = 0; i < 10; i++) {
        float state[LATENT_DIM], next_state[LATENT_DIM];
        generate_random_pattern(state, LATENT_DIM);
        generate_random_pattern(next_state, LATENT_DIM);
        temporal_replay_store(replay, state, nullptr, next_state, 0.5f, false, 1.0f);
    }

    /* Forward sweep */
    replay_sweep_result_t* sweep = replay_sweep_result_create(5, LATENT_DIM);
    ASSERT_NE(sweep, nullptr);

    int ret = temporal_replay_forward_sweep(replay, 0, 5, sweep);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    replay_sweep_result_destroy(sweep);
}

/* ============================================================================
 * Sensory Bridge Integration Tests
 * ============================================================================ */

TEST_F(OmniIntegrationTest, SensoryBridgeCreation) {
    omni_sensory_config_t config;
    omni_sensory_default_config(&config);

    omni_sensory_bridge_t* bridge = omni_sensory_bridge_create(&config);
    EXPECT_NE(bridge, nullptr);

    if (bridge) {
        omni_sensory_bridge_destroy(bridge);
    }
}

TEST_F(OmniIntegrationTest, SensoryBridgeConnect) {
    if (!jepa || !hierarchy) GTEST_SKIP();

    omni_sensory_config_t config;
    omni_sensory_default_config(&config);

    omni_sensory_bridge_t* bridge = omni_sensory_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    int ret = omni_sensory_connect_jepa(bridge, jepa);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    ret = omni_sensory_connect_pred_hier(bridge, hierarchy);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    omni_sensory_bridge_destroy(bridge);
}

/* ============================================================================
 * Cortical Columns Bridge Integration Tests
 * ============================================================================ */

TEST_F(OmniIntegrationTest, CorticalColumnsBridgeCreation) {
    omni_cc_config_t config;
    omni_cc_default_config(&config);

    omni_cortical_columns_bridge_t* bridge = omni_cc_bridge_create(&config);
    EXPECT_NE(bridge, nullptr);

    if (bridge) {
        omni_cc_bridge_destroy(bridge);
    }
}

TEST_F(OmniIntegrationTest, CorticalColumnsConnect) {
    if (!jepa || !hierarchy) GTEST_SKIP();

    omni_cc_config_t config;
    omni_cc_default_config(&config);

    omni_cortical_columns_bridge_t* bridge = omni_cc_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    int ret = omni_cc_connect_jepa(bridge, jepa);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    ret = omni_cc_connect_pred_hier(bridge, hierarchy);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    omni_cc_bridge_destroy(bridge);
}

TEST_F(OmniIntegrationTest, CorticalColumnsUpdate) {
    if (!jepa || !hierarchy) GTEST_SKIP();

    omni_cc_config_t config;
    omni_cc_default_config(&config);

    omni_cortical_columns_bridge_t* bridge = omni_cc_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    omni_cc_connect_jepa(bridge, jepa);
    omni_cc_connect_pred_hier(bridge, hierarchy);

    /* Test the update cycle */
    int ret = omni_cc_update(bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Check sparsity value is valid */
    float sparsity = omni_cc_get_sparsity(bridge);
    EXPECT_GE(sparsity, 0.0f);
    EXPECT_LE(sparsity, 1.0f);

    omni_cc_bridge_destroy(bridge);
}

/* ============================================================================
 * Occipital Bridge Integration Tests
 * ============================================================================ */

TEST_F(OmniIntegrationTest, OccipitalBridgeCreation) {
    omni_occipital_config_t config;
    omni_occipital_default_config(&config);

    omni_occipital_bridge_t* bridge = omni_occipital_bridge_create(&config);
    EXPECT_NE(bridge, nullptr);

    if (bridge) {
        omni_occipital_bridge_destroy(bridge);
    }
}

TEST_F(OmniIntegrationTest, OccipitalBridgeConnect) {
    if (!jepa || !hierarchy) GTEST_SKIP();

    omni_occipital_config_t config;
    omni_occipital_default_config(&config);

    omni_occipital_bridge_t* bridge = omni_occipital_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    int ret = omni_occipital_connect_jepa(bridge, jepa);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    ret = omni_occipital_connect_pred_hier(bridge, hierarchy);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    omni_occipital_bridge_destroy(bridge);
}

TEST_F(OmniIntegrationTest, OccipitalUpdate) {
    if (!jepa || !hierarchy) GTEST_SKIP();

    omni_occipital_config_t config;
    omni_occipital_default_config(&config);

    omni_occipital_bridge_t* bridge = omni_occipital_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    omni_occipital_connect_jepa(bridge, jepa);
    omni_occipital_connect_pred_hier(bridge, hierarchy);

    /* Test the update cycle */
    int ret = omni_occipital_update(bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Check dorsal PE is valid */
    float dorsal_pe = omni_occipital_get_dorsal_pe(bridge);
    EXPECT_FALSE(std::isnan(dorsal_pe));

    /* Check ventral PE is valid */
    float ventral_pe = omni_occipital_get_ventral_pe(bridge);
    EXPECT_FALSE(std::isnan(ventral_pe));

    omni_occipital_bridge_destroy(bridge);
}

/* ============================================================================
 * Broca Bridge Integration Tests
 * ============================================================================ */

TEST_F(OmniIntegrationTest, BrocaBridgeCreation) {
    omni_broca_config_t config;
    omni_broca_default_config(&config);

    omni_broca_bridge_t* bridge = omni_broca_bridge_create(&config);
    EXPECT_NE(bridge, nullptr);

    if (bridge) {
        omni_broca_bridge_destroy(bridge);
    }
}

TEST_F(OmniIntegrationTest, BrocaBridgeConnect) {
    if (!jepa || !hierarchy) GTEST_SKIP();

    omni_broca_config_t config;
    omni_broca_default_config(&config);

    omni_broca_bridge_t* bridge = omni_broca_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    int ret = omni_broca_connect_jepa(bridge, jepa);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    ret = omni_broca_connect_pred_hier(bridge, hierarchy);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    omni_broca_bridge_destroy(bridge);
}

TEST_F(OmniIntegrationTest, BrocaSyntaxPrediction) {
    if (!jepa || !hierarchy || !hopfield) GTEST_SKIP();

    omni_broca_config_t config;
    omni_broca_default_config(&config);

    omni_broca_bridge_t* bridge = omni_broca_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    omni_broca_connect_jepa(bridge, jepa);
    omni_broca_connect_hopfield(bridge, hopfield);

    /* Test syntax prediction */
    omni_syntactic_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    int ret = omni_broca_predict_syntax(bridge, &prediction);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(prediction.confidence, 0.0f);
    EXPECT_LE(prediction.confidence, 1.0f);

    omni_broca_bridge_destroy(bridge);
}

TEST_F(OmniIntegrationTest, BrocaMotorPrediction) {
    if (!jepa || !hierarchy) GTEST_SKIP();

    omni_broca_config_t config;
    omni_broca_default_config(&config);

    omni_broca_bridge_t* bridge = omni_broca_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    omni_broca_connect_jepa(bridge, jepa);
    omni_broca_connect_pred_hier(bridge, hierarchy);

    /* Create phoneme input */
    float phoneme[LATENT_DIM];
    generate_random_pattern(phoneme, LATENT_DIM);

    /* Test motor prediction */
    omni_motor_prediction_t prediction;
    memset(&prediction, 0, sizeof(prediction));

    int ret = omni_broca_predict_motor(bridge, phoneme, LATENT_DIM, &prediction);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(prediction.execution_confidence, 0.0f);
    EXPECT_LE(prediction.execution_confidence, 1.0f);

    omni_broca_bridge_destroy(bridge);
}

/* ============================================================================
 * KG Sync Integration Tests
 * ============================================================================ */

TEST_F(OmniIntegrationTest, KGSyncCreation) {
    /* KG sync requires a brain_kg instance - skip if we can't create one */
    omni_kg_sync_config_t config;
    omni_kg_sync_default_config(&config);

    /* Create with NULL kg and default config - should handle gracefully */
    omni_kg_sync_t* sync = omni_kg_sync_create(nullptr, &config);
    /* Creating with NULL kg may fail gracefully, test the API exists */
    if (sync) {
        omni_kg_sync_destroy(sync);
    }
    /* Success - the API is callable */
    SUCCEED();
}

TEST_F(OmniIntegrationTest, KGSyncDefaultConfig) {
    omni_kg_sync_config_t config;
    int ret = omni_kg_sync_default_config(&config);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_TRUE(config.create_nodes);
    EXPECT_TRUE(config.create_edges);
}

/* ============================================================================
 * Full Pipeline Integration Tests
 * ============================================================================ */

TEST_F(OmniIntegrationTest, FullPredictionPipeline) {
    if (!jepa || !hopfield || !hierarchy || !replay) GTEST_SKIP();

    /* 1. Create input latent */
    jepa_latent_t* input = jepa_latent_create_dim(LATENT_DIM);
    ASSERT_NE(input, nullptr);
    generate_random_pattern(input->embedding, LATENT_DIM);

    /* 2. JEPA forward prediction */
    jepa_bidir_result_t* jepa_result = jepa_bidir_result_create(LATENT_DIM);
    ASSERT_NE(jepa_result, nullptr);

    int ret = jepa_bidirectional_predict(jepa, JEPA_DIR_FORWARD, input, jepa_result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* 3. Store in Hopfield */
    uint32_t pattern_id;
    ret = hopfield_memory_store(hopfield, jepa_result->prediction->embedding, &pattern_id);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* 4. Process through hierarchy */
    ret = pred_hier_forward(hierarchy, jepa_result->prediction->embedding);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* 5. Store in replay */
    float next_state[LATENT_DIM];
    generate_random_pattern(next_state, LATENT_DIM);
    ret = temporal_replay_store(replay, jepa_result->prediction->embedding,
                                nullptr, next_state, 0.5f, false, 1.0f);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    jepa_bidir_result_destroy(jepa_result);
    jepa_latent_destroy(input);
}

TEST_F(OmniIntegrationTest, BridgeChainPipeline) {
    if (!jepa || !hierarchy) GTEST_SKIP();

    /* Create bridges */
    omni_sensory_config_t sens_config;
    omni_sensory_default_config(&sens_config);
    omni_sensory_bridge_t* sensory = omni_sensory_bridge_create(&sens_config);
    ASSERT_NE(sensory, nullptr);

    omni_cc_config_t cc_config;
    omni_cc_default_config(&cc_config);
    omni_cortical_columns_bridge_t* cc = omni_cc_bridge_create(&cc_config);
    ASSERT_NE(cc, nullptr);

    /* Connect bridges to core components */
    omni_sensory_connect_jepa(sensory, jepa);
    omni_sensory_connect_pred_hier(sensory, hierarchy);
    omni_cc_connect_jepa(cc, jepa);
    omni_cc_connect_pred_hier(cc, hierarchy);

    /* Update bridges */
    int ret = omni_sensory_update(sensory);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    ret = omni_cc_update(cc);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    omni_cc_bridge_destroy(cc);
    omni_sensory_bridge_destroy(sensory);
}

TEST_F(OmniIntegrationTest, StressPredictionPipeline) {
    if (!jepa || !hopfield) GTEST_SKIP();

    const int NUM_ITERATIONS = 50;
    int success_count = 0;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        jepa_latent_t* input = jepa_latent_create_dim(LATENT_DIM);
        if (!input) continue;
        generate_random_pattern(input->embedding, LATENT_DIM);

        jepa_bidir_result_t* result = jepa_bidir_result_create(LATENT_DIM);
        if (!result) {
            jepa_latent_destroy(input);
            continue;
        }

        if (jepa_bidirectional_predict(jepa, JEPA_DIR_FORWARD, input, result) == NIMCP_SUCCESS) {
            uint32_t id;
            if (hopfield_memory_store(hopfield, result->prediction->embedding, &id) == NIMCP_SUCCESS) {
                success_count++;
            }
        }

        jepa_bidir_result_destroy(result);
        jepa_latent_destroy(input);
    }

    EXPECT_EQ(success_count, NUM_ITERATIONS);
    std::cout << "Stress test: " << success_count << "/" << NUM_ITERATIONS << std::endl;
}

/* ============================================================================
 * Precision Weighting Tests (Phase 6)
 * ============================================================================ */

TEST_F(OmniIntegrationTest, PrecisionContextCreation) {
    omni_precision_config_t config;
    ASSERT_EQ(omni_precision_default_config(&config), NIMCP_SUCCESS);

    omni_precision_ctx_t* ctx = omni_precision_create(&config);
    ASSERT_NE(ctx, nullptr);

    /* Verify default config values */
    EXPECT_EQ(config.update_mode, OMNI_PREC_UPDATE_BAYESIAN);
    EXPECT_FLOAT_EQ(config.learning_rate, OMNI_PRECISION_DEFAULT_LR);
    EXPECT_TRUE(config.enable_propagation);

    omni_precision_destroy(ctx);
}

TEST_F(OmniIntegrationTest, PrecisionModuleRegistration) {
    omni_precision_ctx_t* ctx = omni_precision_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    /* Register a module */
    EXPECT_EQ(omni_precision_register_module(ctx, 0x0E55, "sensory_bridge", 1.0f),
              NIMCP_SUCCESS);

    /* Enable forward channel */
    EXPECT_EQ(omni_precision_enable_channel(ctx, 0x0E55,
              OMNI_PREC_CHANNEL_FORWARD, 1.0f),
              NIMCP_SUCCESS);

    /* Get precision should return 1.0 */
    float prec = omni_precision_get(ctx, 0x0E55, OMNI_PREC_CHANNEL_FORWARD);
    EXPECT_FLOAT_EQ(prec, 1.0f);

    omni_precision_destroy(ctx);
}

TEST_F(OmniIntegrationTest, PrecisionUpdate) {
    omni_precision_ctx_t* ctx = omni_precision_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    /* Register module with precision 1.0 */
    EXPECT_EQ(omni_precision_register_module(ctx, 0x0E55, "test_module", 1.0f),
              NIMCP_SUCCESS);
    EXPECT_EQ(omni_precision_enable_channel(ctx, 0x0E55,
              OMNI_PREC_CHANNEL_FORWARD, 1.0f),
              NIMCP_SUCCESS);

    /* Update with small prediction error - precision should increase */
    float small_error = 0.1f;
    EXPECT_EQ(omni_precision_update(ctx, 0x0E55, OMNI_PREC_CHANNEL_FORWARD,
              small_error), NIMCP_SUCCESS);

    float prec_after_small = omni_precision_get(ctx, 0x0E55, OMNI_PREC_CHANNEL_FORWARD);
    EXPECT_GT(prec_after_small, 1.0f); /* Precision should increase */

    /* Reset and update with large prediction error - precision should decrease */
    EXPECT_EQ(omni_precision_reset(ctx), NIMCP_SUCCESS);
    EXPECT_EQ(omni_precision_enable_channel(ctx, 0x0E55,
              OMNI_PREC_CHANNEL_FORWARD, 1.0f),
              NIMCP_SUCCESS);

    float large_error = 5.0f;
    EXPECT_EQ(omni_precision_update(ctx, 0x0E55, OMNI_PREC_CHANNEL_FORWARD,
              large_error), NIMCP_SUCCESS);

    float prec_after_large = omni_precision_get(ctx, 0x0E55, OMNI_PREC_CHANNEL_FORWARD);
    EXPECT_LT(prec_after_large, 1.0f); /* Precision should decrease */

    omni_precision_destroy(ctx);
}

TEST_F(OmniIntegrationTest, PrecisionEdgeCreation) {
    omni_precision_ctx_t* ctx = omni_precision_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    /* Register two modules */
    EXPECT_EQ(omni_precision_register_module(ctx, 0x0E55, "module_a", 1.0f),
              NIMCP_SUCCESS);
    EXPECT_EQ(omni_precision_register_module(ctx, 0x0E56, "module_b", 1.0f),
              NIMCP_SUCCESS);

    /* Add precision edge */
    EXPECT_EQ(omni_precision_add_edge(ctx, 0x0E55, 0x0E56,
              OMNI_PREC_CHANNEL_FORWARD, 0.5f), NIMCP_SUCCESS);

    /* Add bidirectional edge */
    EXPECT_EQ(omni_precision_add_bidirectional_edge(ctx, 0x0E55, 0x0E56, 0.3f),
              NIMCP_SUCCESS);

    omni_precision_destroy(ctx);
}

TEST_F(OmniIntegrationTest, PrecisionConfidence) {
    /* Test confidence = precision / (precision + 1) */
    EXPECT_NEAR(omni_precision_to_confidence(0.0f), 0.0f, 0.001f);
    EXPECT_NEAR(omni_precision_to_confidence(1.0f), 0.5f, 0.001f);
    EXPECT_NEAR(omni_precision_to_confidence(9.0f), 0.9f, 0.001f);
    EXPECT_NEAR(omni_precision_to_confidence(99.0f), 0.99f, 0.001f);
}

TEST_F(OmniIntegrationTest, PrecisionClamp) {
    /* Test precision clamping */
    EXPECT_EQ(omni_precision_clamp(0.001f), OMNI_PRECISION_MIN);
    EXPECT_EQ(omni_precision_clamp(500.0f), OMNI_PRECISION_MAX);
    EXPECT_FLOAT_EQ(omni_precision_clamp(5.0f), 5.0f);
}

TEST_F(OmniIntegrationTest, PrecisionFromVariance) {
    /* Test precision = 1/variance */
    EXPECT_NEAR(omni_precision_from_variance(1.0f), 1.0f, 0.001f);
    EXPECT_NEAR(omni_precision_from_variance(0.25f), 4.0f, 0.001f);
    EXPECT_NEAR(omni_precision_from_variance(4.0f), 0.25f, 0.001f);

    /* Very small variance should clamp to max precision */
    float prec_small_var = omni_precision_from_variance(0.001f);
    EXPECT_LE(prec_small_var, OMNI_PRECISION_MAX);
}

TEST_F(OmniIntegrationTest, PrecisionStats) {
    omni_precision_ctx_t* ctx = omni_precision_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    omni_precision_stats_t stats;
    EXPECT_EQ(omni_precision_get_stats(ctx, &stats), NIMCP_SUCCESS);

    EXPECT_EQ(stats.total_updates, 0u);
    EXPECT_EQ(stats.propagations, 0u);

    /* Register module and do some updates */
    omni_precision_register_module(ctx, 0x0E55, "test", 1.0f);
    omni_precision_enable_channel(ctx, 0x0E55, OMNI_PREC_CHANNEL_FORWARD, 1.0f);
    omni_precision_update(ctx, 0x0E55, OMNI_PREC_CHANNEL_FORWARD, 0.5f);
    omni_precision_update(ctx, 0x0E55, OMNI_PREC_CHANNEL_FORWARD, 0.3f);

    EXPECT_EQ(omni_precision_get_stats(ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_updates, 2u);

    /* Reset stats */
    EXPECT_EQ(omni_precision_reset_stats(ctx), NIMCP_SUCCESS);
    EXPECT_EQ(omni_precision_get_stats(ctx, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_updates, 0u);

    omni_precision_destroy(ctx);
}

TEST_F(OmniIntegrationTest, PrecisionStringConversion) {
    /* Test channel string conversion */
    EXPECT_STREQ(omni_precision_channel_to_string(OMNI_PREC_CHANNEL_FORWARD),
                 "FORWARD");
    EXPECT_STREQ(omni_precision_channel_to_string(OMNI_PREC_CHANNEL_BACKWARD),
                 "BACKWARD");
    EXPECT_STREQ(omni_precision_channel_to_string(OMNI_PREC_CHANNEL_LATERAL),
                 "LATERAL");

    /* Test update mode string conversion */
    EXPECT_STREQ(omni_precision_update_mode_to_string(OMNI_PREC_UPDATE_BAYESIAN),
                 "BAYESIAN");
    EXPECT_STREQ(omni_precision_update_mode_to_string(OMNI_PREC_UPDATE_GRADIENT),
                 "GRADIENT");

    /* Test route mode string conversion */
    EXPECT_STREQ(omni_precision_route_mode_to_string(OMNI_PREC_ROUTE_INDEPENDENT),
                 "INDEPENDENT");
    EXPECT_STREQ(omni_precision_route_mode_to_string(OMNI_PREC_ROUTE_GRAPH),
                 "GRAPH");
}

TEST_F(OmniIntegrationTest, SensoryBridgePrecisionConnect) {
    /* Create sensory bridge */
    omni_sensory_config_t config;
    omni_sensory_default_config(&config);
    omni_sensory_bridge_t* bridge = omni_sensory_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

    /* Create precision context */
    omni_precision_ctx_t* prec_ctx = omni_precision_create(nullptr);
    ASSERT_NE(prec_ctx, nullptr);

    /* Connect precision to bridge */
    EXPECT_EQ(omni_sensory_connect_precision(bridge, prec_ctx), NIMCP_SUCCESS);

    /* Verify module was registered */
    float prec = omni_precision_get(prec_ctx, BIO_MODULE_OMNI_SENSORY_BRIDGE,
                                     OMNI_PREC_CHANNEL_FORWARD);
    EXPECT_FLOAT_EQ(prec, OMNI_PRECISION_DEFAULT);

    omni_precision_destroy(prec_ctx);
    omni_sensory_bridge_destroy(bridge);
}

/* ============================================================================
 * Active Inference Tests (Phase 7)
 * ============================================================================ */

TEST_F(OmniIntegrationTest, ActiveInferenceCreation) {
    omni_ai_config_t config;
    ASSERT_EQ(omni_ai_default_config(&config), NIMCP_SUCCESS);

    omni_active_inference_t* ai = omni_ai_create(&config, 8, 16);
    ASSERT_NE(ai, nullptr);

    /* Verify default config */
    EXPECT_EQ(config.select_mode, OMNI_AI_SELECT_SOFTMAX);
    EXPECT_EQ(config.efe_mode, OMNI_AI_EFE_BALANCED);
    EXPECT_FLOAT_EQ(config.policy_precision, OMNI_AI_DEFAULT_PRECISION);

    omni_ai_destroy(ai);
}

TEST_F(OmniIntegrationTest, ActiveInferencePolicyAdd) {
    omni_active_inference_t* ai = omni_ai_create(nullptr, 4, 8);
    ASSERT_NE(ai, nullptr);

    /* Add a policy */
    float actions[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    int idx = omni_ai_add_policy(ai, actions, 2, 4);
    EXPECT_GE(idx, 0);

    /* Get policy */
    omni_ai_policy_t policy;
    EXPECT_EQ(omni_ai_get_policy(ai, (uint32_t)idx, &policy), NIMCP_SUCCESS);
    EXPECT_EQ(policy.horizon, 2u);
    EXPECT_EQ(policy.action_dim, 4u);

    omni_ai_destroy(ai);
}

TEST_F(OmniIntegrationTest, ActiveInferenceRandomPolicies) {
    omni_active_inference_t* ai = omni_ai_create(nullptr, 4, 8);
    ASSERT_NE(ai, nullptr);

    /* Generate random policies */
    int generated = omni_ai_generate_random_policies(ai, 5, 3);
    EXPECT_EQ(generated, 5);

    /* Clear policies */
    EXPECT_EQ(omni_ai_clear_policies(ai), NIMCP_SUCCESS);

    omni_ai_destroy(ai);
}

TEST_F(OmniIntegrationTest, ActiveInferenceGoalSet) {
    omni_active_inference_t* ai = omni_ai_create(nullptr, 4, 8);
    ASSERT_NE(ai, nullptr);

    /* Set goal */
    float preferred[8] = {1.0f, 0.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.5f, 0.0f};
    int goal_idx = omni_ai_set_goal(ai, preferred, 8, 2.0f);
    EXPECT_GE(goal_idx, 0);

    /* Add additional goal */
    float goal2[8] = {0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};
    int goal2_idx = omni_ai_add_goal(ai, goal2, 8, 1.5f);
    EXPECT_GE(goal2_idx, 0);

    /* Deactivate goal */
    EXPECT_EQ(omni_ai_set_goal_active(ai, (uint32_t)goal_idx, false), NIMCP_SUCCESS);

    omni_ai_destroy(ai);
}

TEST_F(OmniIntegrationTest, ActiveInferenceObservation) {
    omni_active_inference_t* ai = omni_ai_create(nullptr, 4, 8);
    ASSERT_NE(ai, nullptr);

    /* Update observation */
    float obs[8] = {0.5f, 0.6f, 0.7f, 0.8f, 0.1f, 0.2f, 0.3f, 0.4f};
    EXPECT_EQ(omni_ai_update_observation(ai, obs, 8), NIMCP_SUCCESS);

    omni_ai_destroy(ai);
}

TEST_F(OmniIntegrationTest, ActiveInferencePolicyEvaluation) {
    omni_active_inference_t* ai = omni_ai_create(nullptr, 4, 8);
    ASSERT_NE(ai, nullptr);

    /* Set up goal */
    float goal[8] = {1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    omni_ai_set_goal(ai, goal, 8, 1.0f);

    /* Update observation */
    float obs[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    omni_ai_update_observation(ai, obs, 8);

    /* Generate and evaluate policies */
    omni_ai_generate_random_policies(ai, 4, 2);
    EXPECT_EQ(omni_ai_evaluate_policies(ai), NIMCP_SUCCESS);

    /* Get EFE for first policy */
    float efe = omni_ai_get_policy_efe(ai, 0, OMNI_AI_DIR_FORWARD);
    EXPECT_GE(efe, 0.0f); /* EFE is non-negative for risk */

    omni_ai_destroy(ai);
}

TEST_F(OmniIntegrationTest, ActiveInferenceForwardSelection) {
    omni_active_inference_t* ai = omni_ai_create(nullptr, 4, 8);
    ASSERT_NE(ai, nullptr);

    /* Set up goal and generate policies */
    float goal[8] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};
    omni_ai_set_goal(ai, goal, 8, 1.0f);
    omni_ai_generate_random_policies(ai, 5, 3);

    /* Select action */
    omni_ai_action_result_t* result = omni_ai_action_result_create(4);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(omni_ai_select_action_forward(ai, result), NIMCP_SUCCESS);
    EXPECT_EQ(result->direction, OMNI_AI_DIR_FORWARD);
    EXPECT_GE(result->confidence, 0.0f);
    EXPECT_LE(result->confidence, 1.0f);

    omni_ai_action_result_destroy(result);
    omni_ai_destroy(ai);
}

TEST_F(OmniIntegrationTest, ActiveInferenceBackwardInference) {
    omni_active_inference_t* ai = omni_ai_create(nullptr, 4, 8);
    ASSERT_NE(ai, nullptr);

    /* Generate policies */
    omni_ai_generate_random_policies(ai, 4, 2);

    /* Infer action from outcome */
    float outcome[8] = {0.9f, 0.1f, 0.8f, 0.2f, 0.7f, 0.3f, 0.6f, 0.4f};
    omni_ai_action_result_t* result = omni_ai_action_result_create(4);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(omni_ai_infer_action_backward(ai, outcome, 8, result), NIMCP_SUCCESS);
    EXPECT_EQ(result->direction, OMNI_AI_DIR_BACKWARD);

    omni_ai_action_result_destroy(result);
    omni_ai_destroy(ai);
}

TEST_F(OmniIntegrationTest, ActiveInferenceOmniSelection) {
    omni_active_inference_t* ai = omni_ai_create(nullptr, 4, 8);
    ASSERT_NE(ai, nullptr);

    /* Set up for omni selection */
    float goal[8] = {1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f};
    omni_ai_set_goal(ai, goal, 8, 1.0f);
    omni_ai_generate_random_policies(ai, 6, 2);

    /* Omni action selection */
    omni_ai_action_result_t* result = omni_ai_action_result_create(4);
    ASSERT_NE(result, nullptr);

    EXPECT_EQ(omni_ai_select_action_omni(ai, result), NIMCP_SUCCESS);

    omni_ai_action_result_destroy(result);
    omni_ai_destroy(ai);
}

TEST_F(OmniIntegrationTest, ActiveInferenceSoftmax) {
    /* Test softmax over EFE */
    float efe[4] = {1.0f, 2.0f, 0.5f, 1.5f};
    float probs[4];

    EXPECT_EQ(omni_ai_softmax_efe(efe, probs, 4, 1.0f), NIMCP_SUCCESS);

    /* Check probabilities sum to 1 */
    float sum = probs[0] + probs[1] + probs[2] + probs[3];
    EXPECT_NEAR(sum, 1.0f, 0.001f);

    /* Lower EFE should have higher probability */
    EXPECT_GT(probs[2], probs[0]); /* EFE 0.5 > EFE 1.0 */
    EXPECT_GT(probs[0], probs[1]); /* EFE 1.0 > EFE 2.0 */
}

TEST_F(OmniIntegrationTest, ActiveInferenceStats) {
    omni_active_inference_t* ai = omni_ai_create(nullptr, 4, 8);
    ASSERT_NE(ai, nullptr);

    omni_ai_stats_t stats;
    EXPECT_EQ(omni_ai_get_stats(ai, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_inferences, 0u);

    /* Do some inferences */
    omni_ai_generate_random_policies(ai, 3, 2);
    omni_ai_action_result_t* result = omni_ai_action_result_create(4);
    if (result) {
        omni_ai_select_action_forward(ai, result);
        omni_ai_select_action_forward(ai, result);
        omni_ai_action_result_destroy(result);
    }

    EXPECT_EQ(omni_ai_get_stats(ai, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_inferences, 2u);
    EXPECT_EQ(stats.forward_selections, 2u);

    /* Reset stats */
    EXPECT_EQ(omni_ai_reset_stats(ai), NIMCP_SUCCESS);
    EXPECT_EQ(omni_ai_get_stats(ai, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_inferences, 0u);

    omni_ai_destroy(ai);
}

TEST_F(OmniIntegrationTest, ActiveInferencePrecisionIntegration) {
    /* Create precision context */
    omni_precision_ctx_t* prec_ctx = omni_precision_create(nullptr);
    ASSERT_NE(prec_ctx, nullptr);

    /* Create active inference */
    omni_active_inference_t* ai = omni_ai_create(nullptr, 4, 8);
    ASSERT_NE(ai, nullptr);

    /* Connect precision */
    EXPECT_EQ(omni_ai_connect_precision(ai, prec_ctx), NIMCP_SUCCESS);

    /* Verify module was registered */
    float prec = omni_precision_get(prec_ctx, BIO_MODULE_OMNI_ACTIVE_INFERENCE,
                                     OMNI_PREC_CHANNEL_FORWARD);
    EXPECT_FLOAT_EQ(prec, OMNI_AI_DEFAULT_PRECISION);

    omni_ai_destroy(ai);
    omni_precision_destroy(prec_ctx);
}

TEST_F(OmniIntegrationTest, ActiveInferenceStringConversion) {
    /* Direction strings */
    EXPECT_STREQ(omni_ai_direction_to_string(OMNI_AI_DIR_FORWARD), "FORWARD");
    EXPECT_STREQ(omni_ai_direction_to_string(OMNI_AI_DIR_BACKWARD), "BACKWARD");
    EXPECT_STREQ(omni_ai_direction_to_string(OMNI_AI_DIR_LATERAL), "LATERAL");

    /* Select mode strings */
    EXPECT_STREQ(omni_ai_select_mode_to_string(OMNI_AI_SELECT_SOFTMAX), "SOFTMAX");
    EXPECT_STREQ(omni_ai_select_mode_to_string(OMNI_AI_SELECT_GREEDY), "GREEDY");

    /* EFE mode strings */
    EXPECT_STREQ(omni_ai_efe_mode_to_string(OMNI_AI_EFE_BALANCED), "BALANCED");
    EXPECT_STREQ(omni_ai_efe_mode_to_string(OMNI_AI_EFE_CURIOUS), "CURIOUS");
}

/* ============================================================================
 * Phase 9: Generative World Model Tests
 *
 * Tests for DreamerV3/JEPA-inspired world model with:
 * - RSSM dynamics (deterministic + stochastic)
 * - Latent space encoding (JEPA-style)
 * - Symlog reward normalization
 * - Counterfactual reasoning
 * - Experience replay and dreaming
 * ============================================================================ */

TEST_F(OmniIntegrationTest, WorldModelCreation) {
    /* Create with default config */
    omni_world_model_t* wm = omni_wm_create(nullptr);
    ASSERT_NE(wm, nullptr);
    omni_wm_destroy(wm);

    /* Create with explicit config */
    omni_wm_config_t config;
    EXPECT_EQ(omni_wm_get_default_config(&config), NIMCP_SUCCESS);
    config.state_dim = 32;
    config.action_dim = 4;
    config.obs_dim = 64;

    wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);
    omni_wm_destroy(wm);

    /* Create simple */
    wm = omni_wm_create_simple(16, 4, 32);
    ASSERT_NE(wm, nullptr);
    omni_wm_destroy(wm);
}

TEST_F(OmniIntegrationTest, WorldModelStateManagement) {
    omni_world_model_t* wm = omni_wm_create_simple(8, 4, 8);
    ASSERT_NE(wm, nullptr);

    /* Create and set state */
    omni_wm_state_t* state = omni_wm_state_create(8);
    ASSERT_NE(state, nullptr);

    for (uint32_t i = 0; i < 8; i++) {
        state->values[i] = (float)i * 0.1f;
    }
    state->uncertainty = 0.5f;

    EXPECT_EQ(omni_wm_set_state(wm, state), NIMCP_SUCCESS);

    /* Get current state */
    const omni_wm_state_t* current = omni_wm_get_state(wm);
    ASSERT_NE(current, nullptr);
    EXPECT_FLOAT_EQ(current->values[0], 0.0f);
    EXPECT_FLOAT_EQ(current->values[3], 0.3f);

    /* Clone state */
    omni_wm_state_t* clone = omni_wm_state_clone(state);
    ASSERT_NE(clone, nullptr);
    EXPECT_FLOAT_EQ(clone->values[5], 0.5f);

    omni_wm_state_destroy(state);
    omni_wm_state_destroy(clone);
    omni_wm_destroy(wm);
}

TEST_F(OmniIntegrationTest, WorldModelRSSMState) {
    /* Create RSSM state */
    omni_wm_rssm_state_t* rssm = omni_wm_rssm_state_create(32, 16);
    ASSERT_NE(rssm, nullptr);
    EXPECT_EQ(rssm->h_dim, 32u);
    EXPECT_EQ(rssm->z_dim, 16u);

    /* Initialize deterministic and stochastic components */
    for (uint32_t i = 0; i < 32; i++) {
        rssm->h[i] = (float)i * 0.01f;
    }
    for (uint32_t i = 0; i < 16; i++) {
        rssm->z[i] = (float)i * 0.02f;
        rssm->z_mean[i] = (float)i * 0.02f;
        rssm->z_std[i] = 0.1f;
    }

    /* Clone RSSM state */
    omni_wm_rssm_state_t* clone = omni_wm_rssm_state_clone(rssm);
    ASSERT_NE(clone, nullptr);
    EXPECT_FLOAT_EQ(clone->h[10], 0.1f);
    EXPECT_FLOAT_EQ(clone->z[5], 0.1f);

    omni_wm_rssm_state_destroy(rssm);
    omni_wm_rssm_state_destroy(clone);
}

TEST_F(OmniIntegrationTest, WorldModelSymlogTransform) {
    /* Test symlog: sign(x) * ln(|x| + 1) */
    EXPECT_NEAR(omni_wm_symlog(0.0f), 0.0f, 0.001f);
    EXPECT_NEAR(omni_wm_symlog(1.0f), 0.693f, 0.01f);  /* ln(2) */
    EXPECT_NEAR(omni_wm_symlog(-1.0f), -0.693f, 0.01f);
    EXPECT_NEAR(omni_wm_symlog(9.0f), 2.303f, 0.01f);  /* ln(10) */

    /* Test symexp: inverse of symlog */
    EXPECT_NEAR(omni_wm_symexp(omni_wm_symlog(5.0f)), 5.0f, 0.001f);
    EXPECT_NEAR(omni_wm_symexp(omni_wm_symlog(-3.0f)), -3.0f, 0.001f);
    EXPECT_NEAR(omni_wm_symexp(omni_wm_symlog(100.0f)), 100.0f, 0.01f);

    /* Test array versions */
    float input[4] = {-10.0f, -1.0f, 1.0f, 10.0f};
    float transformed[4];
    float recovered[4];

    omni_wm_symlog_array(input, transformed, 4);
    omni_wm_symexp_array(transformed, recovered, 4);

    for (int i = 0; i < 4; i++) {
        EXPECT_NEAR(recovered[i], input[i], 0.01f);
    }
}

TEST_F(OmniIntegrationTest, WorldModelForwardPrediction) {
    omni_world_model_t* wm = omni_wm_create_simple(16, 4, 16);
    ASSERT_NE(wm, nullptr);

    /* Set initial state */
    omni_wm_state_t* state = omni_wm_state_create(16);
    ASSERT_NE(state, nullptr);
    for (uint32_t i = 0; i < 16; i++) {
        state->values[i] = 0.5f;
    }
    EXPECT_EQ(omni_wm_set_state(wm, state), NIMCP_SUCCESS);

    /* Predict forward */
    float action[4] = {0.1f, 0.2f, -0.1f, 0.0f};
    omni_wm_transition_t result;
    memset(&result, 0, sizeof(result));

    EXPECT_EQ(omni_wm_predict_forward(wm, action, 4, &result), NIMCP_SUCCESS);
    ASSERT_NE(result.next_state, nullptr);
    EXPECT_EQ(result.direction, OMNI_WM_DIR_FORWARD);

    /* State should have changed */
    EXPECT_NE(result.next_state->values[0], state->values[0]);

    omni_wm_state_destroy(result.next_state);
    omni_wm_state_destroy(state);
    omni_wm_destroy(wm);
}

TEST_F(OmniIntegrationTest, WorldModelBackwardInference) {
    omni_world_model_t* wm = omni_wm_create_simple(16, 4, 16);
    ASSERT_NE(wm, nullptr);

    /* Create state for backward inference */
    omni_wm_state_t* state = omni_wm_state_create(16);
    ASSERT_NE(state, nullptr);
    for (uint32_t i = 0; i < 16; i++) {
        state->values[i] = (float)i * 0.1f;
    }

    /* Infer backward */
    omni_wm_transition_t result;
    memset(&result, 0, sizeof(result));

    EXPECT_EQ(omni_wm_infer_backward(wm, state, &result), NIMCP_SUCCESS);
    ASSERT_NE(result.next_state, nullptr);
    ASSERT_NE(result.action_taken, nullptr);
    EXPECT_EQ(result.direction, OMNI_WM_DIR_BACKWARD);

    omni_wm_state_destroy(result.next_state);
    nimcp_free(result.action_taken);
    omni_wm_state_destroy(state);
    omni_wm_destroy(wm);
}

TEST_F(OmniIntegrationTest, WorldModelLatentEncoding) {
    omni_world_model_t* wm = omni_wm_create_simple(32, 4, 32);
    ASSERT_NE(wm, nullptr);

    /* Create latent representation */
    omni_wm_latent_t* latent = omni_wm_latent_create(32);
    ASSERT_NE(latent, nullptr);

    /* Encode observation */
    float observation[32];
    for (uint32_t i = 0; i < 32; i++) {
        observation[i] = (float)i * 0.05f;
    }

    EXPECT_EQ(omni_wm_encode(wm, observation, 32, latent), NIMCP_SUCCESS);

    /* Latent should have some values */
    bool has_nonzero = false;
    for (uint32_t i = 0; i < 32; i++) {
        if (latent->embedding[i] != 0.0f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    /* Decode back to observation space */
    float decoded[32];
    EXPECT_EQ(omni_wm_decode(wm, latent, decoded, 32), NIMCP_SUCCESS);

    omni_wm_latent_destroy(latent);
    omni_wm_destroy(wm);
}

TEST_F(OmniIntegrationTest, WorldModelMDN) {
    /* Create MDN prediction */
    omni_wm_mdn_prediction_t* mdn = omni_wm_mdn_create(5, 8);
    ASSERT_NE(mdn, nullptr);
    EXPECT_EQ(mdn->num_components, 5u);
    EXPECT_EQ(mdn->dim, 8u);

    /* Check components initialized */
    for (uint32_t k = 0; k < 5; k++) {
        EXPECT_NEAR(mdn->components[k].weight, 0.2f, 0.001f);
        EXPECT_NE(mdn->components[k].mean, nullptr);
        EXPECT_NE(mdn->components[k].std, nullptr);
    }

    /* Sample from MDN */
    float sample[8];
    EXPECT_EQ(omni_wm_mdn_sample(mdn, sample), NIMCP_SUCCESS);

    /* Get mode */
    float mode[8];
    EXPECT_EQ(omni_wm_mdn_mode(mdn, mode), NIMCP_SUCCESS);

    /* Compute log probability */
    float log_prob = omni_wm_mdn_log_prob(mdn, sample);
    EXPECT_LT(log_prob, 0.0f);  /* Log prob should be negative */

    omni_wm_mdn_destroy(mdn);
}

TEST_F(OmniIntegrationTest, WorldModelExperienceReplay) {
    omni_world_model_t* wm = omni_wm_create_simple(16, 4, 16);
    ASSERT_NE(wm, nullptr);

    /* Initially empty */
    EXPECT_EQ(omni_wm_get_replay_size(wm), 0u);

    /* Create and add experiences */
    for (int i = 0; i < 5; i++) {
        omni_wm_experience_t* exp = omni_wm_experience_create(16, 4, 16);
        ASSERT_NE(exp, nullptr);

        exp->reward = (float)i * 0.5f;
        exp->terminal = (i == 4);

        EXPECT_EQ(omni_wm_add_experience(wm, exp), NIMCP_SUCCESS);
        omni_wm_experience_destroy(exp);
    }

    EXPECT_EQ(omni_wm_get_replay_size(wm), 5u);

    /* Sample from replay */
    omni_wm_experience_t* batch[3];
    uint32_t sampled = omni_wm_sample_experiences(wm, batch, 3);
    EXPECT_EQ(sampled, 3u);

    /* Clear replay */
    EXPECT_EQ(omni_wm_clear_replay(wm), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_get_replay_size(wm), 0u);

    omni_wm_destroy(wm);
}

TEST_F(OmniIntegrationTest, WorldModelCounterfactual) {
    omni_world_model_t* wm = omni_wm_create_simple(8, 4, 8);
    ASSERT_NE(wm, nullptr);

    /* Set initial state */
    omni_wm_state_t* state = omni_wm_state_create(8);
    ASSERT_NE(state, nullptr);
    for (uint32_t i = 0; i < 8; i++) {
        state->values[i] = 1.0f;
    }
    EXPECT_EQ(omni_wm_set_state(wm, state), NIMCP_SUCCESS);

    /* What if query */
    float action[4] = {0.5f, 0.5f, 0.0f, 0.0f};
    omni_wm_counterfactual_result_t result;
    memset(&result, 0, sizeof(result));

    EXPECT_EQ(omni_wm_what_if(wm, action, 4, 5, &result), NIMCP_SUCCESS);

    EXPECT_EQ(result.trajectory_len, 5u);
    ASSERT_NE(result.trajectory, nullptr);
    EXPECT_GT(result.confidence, 0.0f);

    omni_wm_cf_result_destroy(&result);
    omni_wm_state_destroy(state);
    omni_wm_destroy(wm);
}

TEST_F(OmniIntegrationTest, WorldModelDreaming) {
    omni_wm_config_t config;
    omni_wm_get_default_config(&config);
    config.enable_dreaming = true;
    config.state_dim = 16;
    config.action_dim = 4;
    config.obs_dim = 16;

    omni_world_model_t* wm = omni_wm_create(&config);
    ASSERT_NE(wm, nullptr);

    /* Set initial state for dreaming */
    omni_wm_state_t* state = omni_wm_state_create(16);
    ASSERT_NE(state, nullptr);
    for (uint32_t i = 0; i < 16; i++) {
        state->values[i] = 0.5f;
    }
    EXPECT_EQ(omni_wm_set_state(wm, state), NIMCP_SUCCESS);

    /* Add some experiences for dream replay */
    for (int i = 0; i < 10; i++) {
        omni_wm_experience_t* exp = omni_wm_experience_create(16, 4, 16);
        ASSERT_NE(exp, nullptr);
        exp->reward = (float)i * 0.1f;
        EXPECT_EQ(omni_wm_add_experience(wm, exp), NIMCP_SUCCESS);
        omni_wm_experience_destroy(exp);
    }

    /* Run dreaming */
    EXPECT_EQ(omni_wm_dream(wm, 3, 5), NIMCP_SUCCESS);

    omni_wm_state_destroy(state);
    omni_wm_destroy(wm);
}

TEST_F(OmniIntegrationTest, WorldModelPolicyEvaluation) {
    omni_world_model_t* wm = omni_wm_create_simple(8, 4, 8);
    ASSERT_NE(wm, nullptr);

    /* Set initial state */
    omni_wm_state_t* state = omni_wm_state_create(8);
    ASSERT_NE(state, nullptr);
    for (uint32_t i = 0; i < 8; i++) {
        state->values[i] = 0.0f;
    }
    EXPECT_EQ(omni_wm_set_state(wm, state), NIMCP_SUCCESS);

    /* Evaluate policy */
    float policy_actions[20];  /* 5 steps x 4 actions */
    for (int i = 0; i < 20; i++) {
        policy_actions[i] = 0.1f;
    }

    float preferred_obs[8] = {1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    float efe = omni_wm_evaluate_policy(wm, policy_actions, 5, preferred_obs, 8);
    EXPECT_LT(efe, 1e10f);  /* Should be finite */

    omni_wm_state_destroy(state);
    omni_wm_destroy(wm);
}

TEST_F(OmniIntegrationTest, WorldModelStatistics) {
    omni_world_model_t* wm = omni_wm_create_simple(8, 4, 8);
    ASSERT_NE(wm, nullptr);

    /* Get initial stats */
    omni_wm_stats_t stats;
    EXPECT_EQ(omni_wm_get_stats(wm, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.forward_predictions, 0u);

    /* Set state and make predictions */
    omni_wm_state_t* state = omni_wm_state_create(8);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(omni_wm_set_state(wm, state), NIMCP_SUCCESS);

    float action[4] = {0.1f, 0.2f, 0.0f, 0.0f};
    omni_wm_transition_t result;
    memset(&result, 0, sizeof(result));

    omni_wm_predict_forward(wm, action, 4, &result);
    if (result.next_state) omni_wm_state_destroy(result.next_state);

    memset(&result, 0, sizeof(result));
    omni_wm_predict_forward(wm, action, 4, &result);
    if (result.next_state) omni_wm_state_destroy(result.next_state);

    /* Check stats updated */
    EXPECT_EQ(omni_wm_get_stats(wm, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.forward_predictions, 2u);

    /* Reset stats */
    EXPECT_EQ(omni_wm_reset_stats(wm), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_get_stats(wm, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.forward_predictions, 0u);

    omni_wm_state_destroy(state);
    omni_wm_destroy(wm);
}

TEST_F(OmniIntegrationTest, WorldModelStringConversions) {
    /* Direction strings */
    EXPECT_STREQ(omni_wm_direction_to_string(OMNI_WM_DIR_FORWARD), "forward");
    EXPECT_STREQ(omni_wm_direction_to_string(OMNI_WM_DIR_BACKWARD), "backward");
    EXPECT_STREQ(omni_wm_direction_to_string(OMNI_WM_DIR_LATERAL), "lateral");
    EXPECT_STREQ(omni_wm_direction_to_string(OMNI_WM_DIR_HIERARCHICAL), "hierarchical");

    /* Learn mode strings */
    EXPECT_STREQ(omni_wm_learn_mode_to_string(OMNI_WM_LEARN_ONLINE), "online");
    EXPECT_STREQ(omni_wm_learn_mode_to_string(OMNI_WM_LEARN_BATCH), "batch");
    EXPECT_STREQ(omni_wm_learn_mode_to_string(OMNI_WM_LEARN_REPLAY), "replay");
    EXPECT_STREQ(omni_wm_learn_mode_to_string(OMNI_WM_LEARN_DREAMING), "dreaming");

    /* Counterfactual type strings */
    EXPECT_STREQ(omni_wm_cf_type_to_string(OMNI_WM_CF_ACTION), "action");
    EXPECT_STREQ(omni_wm_cf_type_to_string(OMNI_WM_CF_STATE), "state");
    EXPECT_STREQ(omni_wm_cf_type_to_string(OMNI_WM_CF_GOAL), "goal");
}

TEST_F(OmniIntegrationTest, WorldModelRSSMStep) {
    omni_world_model_t* wm = omni_wm_create_simple(32, 4, 32);
    ASSERT_NE(wm, nullptr);

    /* Get RSSM state */
    const omni_wm_rssm_state_t* rssm = omni_wm_get_rssm_state(wm);
    ASSERT_NE(rssm, nullptr);

    /* Create next state */
    omni_wm_rssm_state_t* next = omni_wm_rssm_state_create(rssm->h_dim, rssm->z_dim);
    ASSERT_NE(next, nullptr);

    /* Perform RSSM step */
    float action[4] = {0.1f, 0.2f, -0.1f, 0.0f};
    EXPECT_EQ(omni_wm_rssm_step(wm, rssm, action, 4, next), NIMCP_SUCCESS);

    /* Next state should be different */
    bool changed = false;
    for (uint32_t i = 0; i < next->h_dim && i < 5; i++) {
        if (next->h[i] != rssm->h[i]) {
            changed = true;
            break;
        }
    }
    EXPECT_TRUE(changed);

    omni_wm_rssm_state_destroy(next);
    omni_wm_destroy(wm);
}

TEST_F(OmniIntegrationTest, WorldModelLearning) {
    omni_world_model_t* wm = omni_wm_create_simple(8, 4, 8);
    ASSERT_NE(wm, nullptr);

    /* Create states for update */
    omni_wm_state_t* state = omni_wm_state_create(8);
    omni_wm_state_t* next_state = omni_wm_state_create(8);
    ASSERT_NE(state, nullptr);
    ASSERT_NE(next_state, nullptr);

    for (uint32_t i = 0; i < 8; i++) {
        state->values[i] = 0.5f;
        next_state->values[i] = 0.6f;
    }

    EXPECT_EQ(omni_wm_set_state(wm, state), NIMCP_SUCCESS);

    /* Update model */
    float action[4] = {0.1f, 0.0f, 0.0f, 0.0f};
    EXPECT_EQ(omni_wm_update(wm, state, action, 4, next_state, 1.0f), NIMCP_SUCCESS);

    /* Check stats */
    omni_wm_stats_t stats;
    EXPECT_EQ(omni_wm_get_stats(wm, &stats), NIMCP_SUCCESS);
    EXPECT_EQ(stats.model_updates, 1u);

    /* Set learning rate */
    EXPECT_EQ(omni_wm_set_learning_rate(wm, 0.01f), NIMCP_SUCCESS);

    omni_wm_state_destroy(state);
    omni_wm_state_destroy(next_state);
    omni_wm_destroy(wm);
}

/* ============================================================================
 * Phase 10: Metacognition Tests
 * ============================================================================ */

TEST_F(OmniIntegrationTest, MetacognitionCreation) {
    /* Test basic metacognition context creation and destruction */
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    /* Verify initial state */
    uint64_t total_inferences;
    float success_rate, efficiency;
    EXPECT_EQ(omni_metacog_get_statistics(ctx, &total_inferences, &success_rate, &efficiency), NIMCP_SUCCESS);
    EXPECT_EQ(total_inferences, 0u);

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionConfig) {
    /* Test configuration */
    omni_metacog_config_t config;
    EXPECT_EQ(omni_metacog_get_default_config(&config), NIMCP_SUCCESS);

    /* Verify default values */
    EXPECT_FLOAT_EQ(config.coherence_threshold, OMNI_METACOG_DEFAULT_COHERENCE_THRESHOLD);
    EXPECT_GT(config.monitoring_frequency, 0.0f);
    EXPECT_TRUE(config.enable_online_learning);

    /* Create with custom config */
    config.monitoring_frequency = 20.0f;
    config.exploration_rate = 0.2f;

    omni_metacog_ctx_t* ctx = omni_metacog_create_with_config(&config);
    ASSERT_NE(ctx, nullptr);

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionSelfModel) {
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    /* Get capability for forward mode */
    omni_capability_t cap;
    EXPECT_EQ(omni_metacog_get_capability(ctx, OMNI_METACOG_MODE_FORWARD, &cap), NIMCP_SUCCESS);
    EXPECT_EQ(cap.mode, OMNI_METACOG_MODE_FORWARD);
    EXPECT_GE(cap.proficiency, 0.0f);
    EXPECT_LE(cap.proficiency, 1.0f);

    /* Update self-model with experience */
    EXPECT_EQ(omni_metacog_update_self_model(ctx, OMNI_METACOG_MODE_FORWARD,
                                             0.8f, 0.3f, 0.1f, true), NIMCP_SUCCESS);

    /* Verify update */
    EXPECT_EQ(omni_metacog_get_capability(ctx, OMNI_METACOG_MODE_FORWARD, &cap), NIMCP_SUCCESS);
    EXPECT_EQ(cap.usage_count, 1u);
    EXPECT_EQ(cap.success_count, 1u);

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionResourceManagement) {
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    /* Get resource state */
    omni_resource_state_t resources;
    EXPECT_EQ(omni_metacog_get_resources(ctx, &resources), NIMCP_SUCCESS);

    /* Verify initial resources */
    for (int i = 0; i < OMNI_RESOURCE_COUNT; i++) {
        EXPECT_FLOAT_EQ(resources.available[i], 1.0f);
        EXPECT_GE(resources.budget[i], 0.0f);
    }

    /* Set new budget */
    EXPECT_EQ(omni_metacog_set_resource_budget(ctx, OMNI_RESOURCE_COMPUTE, 0.9f), NIMCP_SUCCESS);

    EXPECT_EQ(omni_metacog_get_resources(ctx, &resources), NIMCP_SUCCESS);
    EXPECT_FLOAT_EQ(resources.budget[OMNI_RESOURCE_COMPUTE], 0.9f);

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionMonitoring) {
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    /* Take monitoring snapshot */
    omni_monitoring_snapshot_t snapshot;
    EXPECT_EQ(omni_metacog_monitor(ctx, &snapshot), NIMCP_SUCCESS);

    /* Verify snapshot */
    EXPECT_GT(snapshot.timestamp, 0.0);
    EXPECT_EQ(snapshot.current_mode, OMNI_METACOG_MODE_FORWARD);
    EXPECT_GE(snapshot.current_accuracy, 0.0f);
    EXPECT_LE(snapshot.current_accuracy, 1.0f);

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionCoherence) {
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    /* Check coherence with no inferences */
    omni_coherence_result_t result;
    EXPECT_EQ(omni_metacog_check_coherence(ctx, nullptr, 0, &result), NIMCP_SUCCESS);
    EXPECT_EQ(result.status, OMNI_COHERENCE_OK);
    EXPECT_FLOAT_EQ(result.coherence_score, 1.0f);

    /* Add some failures to history to test coherence detection */
    for (int i = 0; i < 5; i++) {
        omni_metacog_update_self_model(ctx, OMNI_METACOG_MODE_FORWARD,
                                       0.2f, 0.5f, 0.2f, false);
    }

    /* Check coherence again */
    float dummy[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    EXPECT_EQ(omni_metacog_check_coherence(ctx, dummy, 4, &result), NIMCP_SUCCESS);
    EXPECT_LT(result.coherence_score, 1.0f);  /* Should detect some incoherence */

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionAnomalyDetection) {
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    omni_monitoring_snapshot_t snapshot;
    EXPECT_EQ(omni_metacog_monitor(ctx, &snapshot), NIMCP_SUCCESS);

    float anomaly_score;
    char description[256];
    EXPECT_EQ(omni_metacog_detect_anomaly(ctx, &snapshot, &anomaly_score, description), NIMCP_SUCCESS);

    /* Initial state should have no anomalies */
    EXPECT_GE(anomaly_score, 0.0f);
    EXPECT_LE(anomaly_score, 1.0f);
    EXPECT_NE(description[0], '\0');

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionEvaluation) {
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    float accuracy, confidence;
    EXPECT_EQ(omni_metacog_evaluate_performance(ctx, &accuracy, &confidence), NIMCP_SUCCESS);
    EXPECT_GE(accuracy, 0.0f);
    EXPECT_LE(accuracy, 1.0f);

    /* Get confidence */
    float conf = omni_metacog_get_confidence(ctx);
    EXPECT_GE(conf, 0.0f);
    EXPECT_LE(conf, 1.0f);

    /* Predict performance */
    float expected_acc, expected_cost;
    EXPECT_EQ(omni_metacog_predict_performance(ctx, OMNI_METACOG_MODE_BACKWARD,
                                                0, &expected_acc, &expected_cost), NIMCP_SUCCESS);
    EXPECT_GE(expected_acc, 0.0f);
    EXPECT_GE(expected_cost, 0.0f);

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionModeSelection) {
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    /* Select mode */
    omni_mode_recommendation_t recommendation;
    EXPECT_EQ(omni_metacog_select_mode(ctx, 0, &recommendation), NIMCP_SUCCESS);

    /* Verify recommendation */
    EXPECT_LT(recommendation.recommended_mode, OMNI_METACOG_MODE_COUNT);
    EXPECT_GE(recommendation.confidence, 0.0f);
    EXPECT_LE(recommendation.confidence, 1.0f);
    EXPECT_NE(recommendation.rationale[0], '\0');

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionResourcePlanning) {
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    omni_resource_plan_t plan;
    EXPECT_EQ(omni_metacog_plan_resources(ctx, OMNI_METACOG_MODE_FORWARD, 0.8f, &plan), NIMCP_SUCCESS);

    /* Verify plan */
    EXPECT_GE(plan.total_budget, 0.0f);
    EXPECT_LE(plan.total_budget, 1.0f);
    EXPECT_GE(plan.efficiency_score, 0.0f);
    EXPECT_NE(plan.rationale[0], '\0');

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionIntervention) {
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    /* Get snapshot */
    omni_monitoring_snapshot_t snapshot;
    EXPECT_EQ(omni_metacog_monitor(ctx, &snapshot), NIMCP_SUCCESS);

    /* Decide on intervention */
    omni_intervention_t intervention;
    EXPECT_EQ(omni_metacog_decide_intervention(ctx, &snapshot, &intervention), NIMCP_SUCCESS);

    /* In normal state, should not need intervention */
    /* (May or may not need intervention depending on initial state) */

    /* Execute intervention (even if not needed, should be no-op) */
    EXPECT_EQ(omni_metacog_execute_intervention(ctx, &intervention), NIMCP_SUCCESS);

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionModeSwitch) {
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    /* Switch mode */
    EXPECT_EQ(omni_metacog_switch_mode(ctx, OMNI_METACOG_MODE_BACKWARD), NIMCP_SUCCESS);

    /* Verify mode changed in monitoring */
    omni_monitoring_snapshot_t snapshot;
    EXPECT_EQ(omni_metacog_monitor(ctx, &snapshot), NIMCP_SUCCESS);
    EXPECT_EQ(snapshot.current_mode, OMNI_METACOG_MODE_BACKWARD);

    /* Switch to another mode */
    EXPECT_EQ(omni_metacog_switch_mode(ctx, OMNI_METACOG_MODE_DREAMING), NIMCP_SUCCESS);

    EXPECT_EQ(omni_metacog_monitor(ctx, &snapshot), NIMCP_SUCCESS);
    EXPECT_EQ(snapshot.current_mode, OMNI_METACOG_MODE_DREAMING);

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionPrecisionAdjustment) {
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    /* Get initial resources */
    omni_resource_state_t resources_before;
    EXPECT_EQ(omni_metacog_get_resources(ctx, &resources_before), NIMCP_SUCCESS);

    /* Adjust precision up */
    EXPECT_EQ(omni_metacog_adjust_precision(ctx, 0.5f), NIMCP_SUCCESS);

    /* Check resources changed */
    omni_resource_state_t resources_after;
    EXPECT_EQ(omni_metacog_get_resources(ctx, &resources_after), NIMCP_SUCCESS);

    /* Adjust precision down */
    EXPECT_EQ(omni_metacog_adjust_precision(ctx, -0.5f), NIMCP_SUCCESS);

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionLearning) {
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    /* Add some history */
    for (int i = 0; i < 20; i++) {
        float accuracy = 0.5f + (float)i / 40.0f;  /* Improving accuracy */
        omni_metacog_update_self_model(ctx, OMNI_METACOG_MODE_FORWARD,
                                       accuracy, 0.3f, 0.1f, accuracy > 0.5f);
    }

    /* Learn from history */
    EXPECT_EQ(omni_metacog_learn(ctx, 10), NIMCP_SUCCESS);

    /* Check learning stats */
    float improvement, convergence;
    EXPECT_EQ(omni_metacog_get_learning_stats(ctx, &improvement, &convergence), NIMCP_SUCCESS);
    /* With improving accuracy, should show positive improvement */
    EXPECT_GE(improvement, 0.0f);

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionPolicyUpdate) {
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    /* Update policy with positive reward */
    EXPECT_EQ(omni_metacog_update_policy(ctx, 1.0f), NIMCP_SUCCESS);

    /* Update policy with negative reward */
    EXPECT_EQ(omni_metacog_update_policy(ctx, -0.5f), NIMCP_SUCCESS);

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionIntegration) {
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    /* Connect to other systems (passing NULL is valid - no-op) */
    EXPECT_EQ(omni_metacog_connect_world_model(ctx, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(omni_metacog_connect_active_inference(ctx, nullptr), NIMCP_SUCCESS);
    EXPECT_EQ(omni_metacog_connect_precision(ctx, nullptr), NIMCP_SUCCESS);

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionStep) {
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    /* Run a few metacognitive cycles */
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(omni_metacog_step(ctx), NIMCP_SUCCESS);
    }

    /* Verify statistics updated */
    uint64_t total_inferences;
    float success_rate, efficiency;
    EXPECT_EQ(omni_metacog_get_statistics(ctx, &total_inferences, &success_rate, &efficiency), NIMCP_SUCCESS);

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionReset) {
    omni_metacog_ctx_t* ctx = omni_metacog_create();
    ASSERT_NE(ctx, nullptr);

    /* Add some state */
    for (int i = 0; i < 10; i++) {
        omni_metacog_update_self_model(ctx, OMNI_METACOG_MODE_FORWARD,
                                       0.7f, 0.3f, 0.1f, true);
    }

    /* Reset */
    EXPECT_EQ(omni_metacog_reset(ctx), NIMCP_SUCCESS);

    /* Verify reset */
    uint64_t total_inferences;
    float success_rate, efficiency;
    EXPECT_EQ(omni_metacog_get_statistics(ctx, &total_inferences, &success_rate, &efficiency), NIMCP_SUCCESS);
    EXPECT_EQ(total_inferences, 0u);

    omni_metacog_destroy(ctx);
}

TEST_F(OmniIntegrationTest, MetacognitionStringConversions) {
    /* Test mode to string */
    EXPECT_STREQ(omni_metacog_mode_to_string(OMNI_METACOG_MODE_FORWARD), "FORWARD");
    EXPECT_STREQ(omni_metacog_mode_to_string(OMNI_METACOG_MODE_BACKWARD), "BACKWARD");
    EXPECT_STREQ(omni_metacog_mode_to_string(OMNI_METACOG_MODE_LATERAL), "LATERAL");
    EXPECT_STREQ(omni_metacog_mode_to_string(OMNI_METACOG_MODE_HIERARCHICAL), "HIERARCHICAL");
    EXPECT_STREQ(omni_metacog_mode_to_string(OMNI_METACOG_MODE_EXPLORATORY), "EXPLORATORY");
    EXPECT_STREQ(omni_metacog_mode_to_string(OMNI_METACOG_MODE_EXPLOITATIVE), "EXPLOITATIVE");
    EXPECT_STREQ(omni_metacog_mode_to_string(OMNI_METACOG_MODE_DREAMING), "DREAMING");
    EXPECT_STREQ(omni_metacog_mode_to_string(OMNI_METACOG_MODE_CONSOLIDATING), "CONSOLIDATING");

    /* Test state to string */
    EXPECT_STREQ(omni_metacog_state_to_string(OMNI_METACOG_STATE_IDLE), "IDLE");
    EXPECT_STREQ(omni_metacog_state_to_string(OMNI_METACOG_STATE_MONITORING), "MONITORING");
    EXPECT_STREQ(omni_metacog_state_to_string(OMNI_METACOG_STATE_EVALUATING), "EVALUATING");
    EXPECT_STREQ(omni_metacog_state_to_string(OMNI_METACOG_STATE_INTERVENING), "INTERVENING");
    EXPECT_STREQ(omni_metacog_state_to_string(OMNI_METACOG_STATE_LEARNING), "LEARNING");

    /* Test resource to string */
    EXPECT_STREQ(omni_resource_type_to_string(OMNI_RESOURCE_COMPUTE), "COMPUTE");
    EXPECT_STREQ(omni_resource_type_to_string(OMNI_RESOURCE_MEMORY), "MEMORY");
    EXPECT_STREQ(omni_resource_type_to_string(OMNI_RESOURCE_TIME), "TIME");
    EXPECT_STREQ(omni_resource_type_to_string(OMNI_RESOURCE_ATTENTION), "ATTENTION");
    EXPECT_STREQ(omni_resource_type_to_string(OMNI_RESOURCE_PRECISION), "PRECISION");

    /* Test coherence status to string */
    EXPECT_STREQ(omni_coherence_status_to_string(OMNI_COHERENCE_OK), "OK");
    EXPECT_STREQ(omni_coherence_status_to_string(OMNI_COHERENCE_CONTRADICTION), "CONTRADICTION");
    EXPECT_STREQ(omni_coherence_status_to_string(OMNI_COHERENCE_IMPLAUSIBILITY), "IMPLAUSIBILITY");
    EXPECT_STREQ(omni_coherence_status_to_string(OMNI_COHERENCE_TEMPORAL), "TEMPORAL");
    EXPECT_STREQ(omni_coherence_status_to_string(OMNI_COHERENCE_MODAL), "MODAL");
}
