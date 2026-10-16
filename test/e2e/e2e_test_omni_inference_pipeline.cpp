/**
 * @file e2e_test_omni_inference_pipeline.cpp
 * @brief End-to-end tests for Omnidirectional Inference pipeline
 * @version 1.2.0
 * @date 2026-01-04
 *
 * Tests complete omnidirectional inference workflows:
 * - Bidirectional JEPA prediction (forward/backward/lateral)
 * - Hopfield memory storage and retrieval
 * - Predictive hierarchy processing
 * - Temporal replay sampling
 *
 * NOTE: Integration bridges (sensory, cortical, broca, occipital) are planned
 * for Phase 2 implementation. Tests for those are marked DISABLED until bridges exist.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>

/* Headers have their own extern "C" guards - don't wrap them to avoid
 * CUDA C++ function conflicts */
#include "cognitive/jepa/nimcp_jepa_bidirectional.h"
#include "cognitive/jepa/nimcp_jepa_latent.h"
#include "cognitive/memory/nimcp_hopfield_memory.h"
#include "cognitive/predictive/nimcp_predictive_hierarchy.h"
#include "cognitive/memory/nimcp_temporal_replay.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Configuration
 * ============================================================================ */

namespace {
    constexpr uint32_t LATENT_DIM = 64;
    constexpr uint32_t NUM_PATTERNS = 100;
    constexpr uint32_t NUM_LEVELS = 3;
    constexpr uint32_t SEQ_LEN = 8;
}

/* ============================================================================
 * Core Components E2E Test Fixture
 * ============================================================================ */

class OmniE2ETest : public ::testing::Test {
protected:
    /* Core omnidirectional components */
    jepa_bidirectional_t* jepa = nullptr;
    hopfield_memory_t* hopfield = nullptr;
    predictive_hierarchy_t* hierarchy = nullptr;
    temporal_replay_t* replay = nullptr;

    /* Dimension arrays for hierarchy */
    uint32_t hier_dims[NUM_LEVELS] = {LATENT_DIM, LATENT_DIM / 2, LATENT_DIM / 4};

    std::mt19937 rng{42};

    void SetUp() override {
        /* Create JEPA bidirectional predictor */
        jepa_bidir_config_t jepa_config;
        jepa_bidir_default_config(&jepa_config);
        jepa_config.embedding_dim = LATENT_DIM;
        jepa_config.gpu_mode = JEPA_BIDIR_GPU_DISABLED;
        /* Enable all directions for E2E testing */
        jepa_config.enable_forward = true;
        jepa_config.enable_backward = true;
        jepa_config.enable_lateral = true;
        jepa = jepa_bidirectional_create(&jepa_config);
        ASSERT_NE(jepa, nullptr);

        /* Create Hopfield memory */
        hopfield_config_t hopfield_config;
        hopfield_default_config(&hopfield_config);
        hopfield_config.pattern_dim = LATENT_DIM;
        hopfield_config.capacity = NUM_PATTERNS;
        hopfield_config.gpu_mode = HOPFIELD_GPU_DISABLED;
        hopfield = hopfield_memory_create(&hopfield_config);
        ASSERT_NE(hopfield, nullptr);

        /* Create predictive hierarchy */
        pred_hier_config_t hier_config;
        pred_hier_simple_config(&hier_config, NUM_LEVELS, hier_dims);
        hier_config.gpu_mode = PRED_HIER_GPU_DISABLED;
        hierarchy = pred_hier_create(&hier_config);
        ASSERT_NE(hierarchy, nullptr);
        pred_hier_free_config(&hier_config);

        /* Create temporal replay */
        replay_config_t replay_config;
        replay_default_config(&replay_config);
        replay_config.state_dim = LATENT_DIM;
        replay_config.capacity = 256;
        replay_config.gpu_mode = REPLAY_GPU_DISABLED;
        replay = temporal_replay_create(&replay_config);
        ASSERT_NE(replay, nullptr);
    }

    void TearDown() override {
        if (replay) temporal_replay_destroy(replay);
        if (hierarchy) pred_hier_destroy(hierarchy);
        if (hopfield) hopfield_memory_destroy(hopfield);
        if (jepa) jepa_bidirectional_destroy(jepa);
    }

    void FillPattern(float* pattern, float value) {
        for (uint32_t i = 0; i < LATENT_DIM; i++) {
            pattern[i] = value + (float)i / LATENT_DIM;
        }
    }

    void RandomPattern(float* pattern) {
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (uint32_t i = 0; i < LATENT_DIM; i++) {
            pattern[i] = dist(rng);
        }
    }

    float CosineSimilarity(const float* a, const float* b, uint32_t dim) {
        float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            dot += a[i] * b[i];
            norm_a += a[i] * a[i];
            norm_b += b[i] * b[i];
        }
        return dot / (std::sqrt(norm_a) * std::sqrt(norm_b) + 1e-8f);
    }
};

/* ============================================================================
 * JEPA Bidirectional Prediction E2E Tests
 * ============================================================================ */

TEST_F(OmniE2ETest, JEPAForwardBackwardPrediction) {
    /* Test: Forward prediction followed by backward produces valid results */

    /* Create input latent */
    jepa_latent_t* input = jepa_latent_create_dim(LATENT_DIM);
    ASSERT_NE(input, nullptr);
    FillPattern(input->embedding, 1.0f);

    /* Forward prediction */
    jepa_bidir_result_t* forward_result = jepa_bidir_result_create(LATENT_DIM);
    ASSERT_NE(forward_result, nullptr);

    int ret = jepa_bidirectional_predict(jepa, JEPA_DIR_FORWARD, input, forward_result);
    ASSERT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_NE(forward_result->prediction, nullptr);

    /* Backward prediction using forward result */
    jepa_bidir_result_t* backward_result = jepa_bidir_result_create(LATENT_DIM);
    ASSERT_NE(backward_result, nullptr);

    ret = jepa_bidirectional_predict(jepa, JEPA_DIR_BACKWARD,
                                      forward_result->prediction, backward_result);
    ASSERT_EQ(ret, NIMCP_SUCCESS);

    /* Verify results are valid (no NaN) */
    for (uint32_t i = 0; i < LATENT_DIM; i++) {
        EXPECT_FALSE(std::isnan(backward_result->prediction->embedding[i]));
    }

    jepa_bidir_result_destroy(backward_result);
    jepa_bidir_result_destroy(forward_result);
    jepa_latent_destroy(input);
}

TEST_F(OmniE2ETest, JEPAMultiDirectionPrediction) {
    /* Test: All directions produce valid predictions */

    jepa_latent_t* input = jepa_latent_create_dim(LATENT_DIM);
    ASSERT_NE(input, nullptr);
    RandomPattern(input->embedding);

    jepa_direction_t directions[] = {
        JEPA_DIR_FORWARD,
        JEPA_DIR_BACKWARD,
        JEPA_DIR_LATERAL
    };

    for (int d = 0; d < 3; d++) {
        jepa_bidir_result_t* result = jepa_bidir_result_create(LATENT_DIM);
        ASSERT_NE(result, nullptr);

        int ret = jepa_bidirectional_predict(jepa, directions[d], input, result);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
        EXPECT_NE(result->prediction, nullptr);

        /* Verify no NaN/Inf */
        for (uint32_t i = 0; i < LATENT_DIM; i++) {
            EXPECT_FALSE(std::isnan(result->prediction->embedding[i]));
            EXPECT_FALSE(std::isinf(result->prediction->embedding[i]));
        }

        jepa_bidir_result_destroy(result);
    }

    jepa_latent_destroy(input);
}

/* ============================================================================
 * Hopfield Memory E2E Tests
 * ============================================================================ */

TEST_F(OmniE2ETest, HopfieldStoreAndRetrieve) {
    /* Test: Store multiple patterns and retrieve similar ones */

    const int num_store = 10;
    float patterns[10][LATENT_DIM];
    uint32_t ids[10];

    /* Store patterns */
    for (int i = 0; i < num_store; i++) {
        FillPattern(patterns[i], (float)i * 0.5f);
        int ret = hopfield_memory_store(hopfield, patterns[i], &ids[i]);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }

    EXPECT_EQ(hopfield_memory_pattern_count(hopfield), (uint32_t)num_store);

    /* Query with similar pattern */
    float query[LATENT_DIM];
    FillPattern(query, 2.5f);  /* Should be closest to pattern 5 */

    hopfield_retrieval_result_t* result = hopfield_result_create(LATENT_DIM);
    ASSERT_NE(result, nullptr);

    int ret = hopfield_memory_retrieve(hopfield, query, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(result->similarity, 0.5f);

    hopfield_result_destroy(result);
}

TEST_F(OmniE2ETest, HopfieldTopK) {
    /* Test: Top-K retrieval returns ordered results */

    const int num_store = 20;
    float patterns[20][LATENT_DIM];

    for (int i = 0; i < num_store; i++) {
        FillPattern(patterns[i], (float)i);
        hopfield_memory_store(hopfield, patterns[i], nullptr);
    }

    float query[LATENT_DIM];
    FillPattern(query, 10.0f);

    uint32_t top_ids[5];
    float top_sims[5];

    int ret = hopfield_memory_top_k(hopfield, query, 5, top_ids, top_sims);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Results should be sorted by similarity (descending) */
    for (int i = 0; i < 4; i++) {
        EXPECT_GE(top_sims[i], top_sims[i + 1]);
    }
}

/* ============================================================================
 * Predictive Hierarchy E2E Tests
 * ============================================================================ */

TEST_F(OmniE2ETest, PredictiveHierarchyForwardBackward) {
    /* Test: Forward and backward passes through hierarchy */

    float input[LATENT_DIM];
    RandomPattern(input);

    pred_hier_result_t* result = pred_hier_result_create(NUM_LEVELS, hier_dims);
    ASSERT_NE(result, nullptr);

    /* Forward pass */
    int ret = pred_hier_forward(hierarchy, input);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Backward pass */
    ret = pred_hier_backward(hierarchy);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Update beliefs */
    ret = pred_hier_update(hierarchy, input, result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    pred_hier_result_destroy(result);
}

TEST_F(OmniE2ETest, PredictiveHierarchyFreeEnergy) {
    /* Test: Free energy computation and minimization */

    float input[LATENT_DIM];
    RandomPattern(input);

    /* Initial state */
    pred_hier_forward(hierarchy, input);
    float initial_fe = pred_hier_compute_free_energy(hierarchy);
    EXPECT_FALSE(std::isnan(initial_fe));

    /* Multiple update cycles */
    pred_hier_result_t* result = pred_hier_result_create(NUM_LEVELS, hier_dims);
    for (int cycle = 0; cycle < 5; cycle++) {
        pred_hier_forward(hierarchy, input);
        pred_hier_backward(hierarchy);
        pred_hier_update(hierarchy, input, result);
    }
    pred_hier_result_destroy(result);

    float final_fe = pred_hier_compute_free_energy(hierarchy);
    EXPECT_FALSE(std::isnan(final_fe));

    std::cout << "Free energy: initial=" << initial_fe
              << ", final=" << final_fe << std::endl;
}

/* ============================================================================
 * Temporal Replay E2E Tests
 * ============================================================================ */

TEST_F(OmniE2ETest, TemporalReplayStoreSample) {
    /* Test: Store experiences and sample them */

    /* Store multiple transitions */
    for (int i = 0; i < 20; i++) {
        float state[LATENT_DIM];
        float next_state[LATENT_DIM];
        FillPattern(state, (float)i);
        FillPattern(next_state, (float)(i + 1));
        float priority = 1.0f - (float)i / 20.0f;

        /* temporal_replay_store(replay, state, action, next_state, reward, terminal, priority) */
        int ret = temporal_replay_store(replay, state, nullptr, next_state, 0.5f, false, priority);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }

    replay_stats_t stats;
    temporal_replay_get_stats(replay, &stats);
    EXPECT_EQ(stats.transitions_stored, 20u);

    /* Sample with priority mode */
    replay_batch_t* batch = replay_batch_create(4, LATENT_DIM, 0);
    ASSERT_NE(batch, nullptr);
    int ret = temporal_replay_sample(replay, REPLAY_MODE_PRIORITY, 4, batch);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    replay_batch_destroy(batch);
}

TEST_F(OmniE2ETest, TemporalReplaySweeps) {
    /* Test: Forward sweep through stored states */

    /* Store states */
    for (int i = 0; i < 10; i++) {
        float state[LATENT_DIM];
        float next_state[LATENT_DIM];
        FillPattern(state, (float)i);
        FillPattern(next_state, (float)(i + 1));
        temporal_replay_store(replay, state, nullptr, next_state, 0.5f, false, 1.0f);
    }

    /* Forward sweep - create result properly */
    replay_sweep_result_t* sweep = replay_sweep_result_create(5, LATENT_DIM);
    ASSERT_NE(sweep, nullptr);
    int ret = temporal_replay_forward_sweep(replay, 0, 5, sweep);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    replay_sweep_result_destroy(sweep);
}

/* ============================================================================
 * Integrated Pipeline E2E Tests
 * ============================================================================ */

TEST_F(OmniE2ETest, JEPAPlusHopfieldPipeline) {
    /* Test: JEPA prediction stored in Hopfield for later retrieval */

    const int num_experiences = 5;

    for (int exp = 0; exp < num_experiences; exp++) {
        /* Create input */
        jepa_latent_t* input = jepa_latent_create_dim(LATENT_DIM);
        FillPattern(input->embedding, (float)exp);

        /* Forward prediction */
        jepa_bidir_result_t* result = jepa_bidir_result_create(LATENT_DIM);
        int ret = jepa_bidirectional_predict(jepa, JEPA_DIR_FORWARD, input, result);
        ASSERT_EQ(ret, NIMCP_SUCCESS);

        /* Store prediction in Hopfield */
        uint32_t id;
        ret = hopfield_memory_store(hopfield, result->prediction->embedding, &id);
        EXPECT_EQ(ret, NIMCP_SUCCESS);

        jepa_bidir_result_destroy(result);
        jepa_latent_destroy(input);
    }

    EXPECT_EQ(hopfield_memory_pattern_count(hopfield), (uint32_t)num_experiences);
}

TEST_F(OmniE2ETest, HierarchyPlusReplayPipeline) {
    /* Test: Hierarchy predictions stored in replay buffer */

    float input[LATENT_DIM];
    float next_state[LATENT_DIM];
    RandomPattern(input);
    RandomPattern(next_state);

    /* Process through hierarchy */
    pred_hier_result_t* hier_result = pred_hier_result_create(NUM_LEVELS, hier_dims);
    int ret = pred_hier_forward(hierarchy, input);
    ASSERT_EQ(ret, NIMCP_SUCCESS);

    ret = pred_hier_update(hierarchy, input, hier_result);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Store result in replay buffer */
    ret = temporal_replay_store(replay, input, nullptr, next_state, 0.5f, false, 1.0f);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    pred_hier_result_destroy(hier_result);
}

TEST_F(OmniE2ETest, FullOmniPipeline) {
    /* Test: Complete pipeline: JEPA -> Hopfield -> Hierarchy -> Replay */

    const int num_steps = 10;

    for (int step = 0; step < num_steps; step++) {
        /* 1. Create input */
        jepa_latent_t* input = jepa_latent_create_dim(LATENT_DIM);
        RandomPattern(input->embedding);

        /* 2. JEPA forward prediction */
        jepa_bidir_result_t* jepa_result = jepa_bidir_result_create(LATENT_DIM);
        int ret = jepa_bidirectional_predict(jepa, JEPA_DIR_FORWARD, input, jepa_result);
        ASSERT_EQ(ret, NIMCP_SUCCESS);

        /* 3. Store prediction in Hopfield */
        uint32_t pattern_id;
        ret = hopfield_memory_store(hopfield, jepa_result->prediction->embedding, &pattern_id);
        EXPECT_EQ(ret, NIMCP_SUCCESS);

        /* 4. Process through predictive hierarchy */
        pred_hier_result_t* hier_result = pred_hier_result_create(NUM_LEVELS, hier_dims);
        ret = pred_hier_forward(hierarchy, jepa_result->prediction->embedding);
        EXPECT_EQ(ret, NIMCP_SUCCESS);

        ret = pred_hier_update(hierarchy, jepa_result->prediction->embedding, hier_result);
        EXPECT_EQ(ret, NIMCP_SUCCESS);

        /* 5. Store in temporal replay */
        float next_state[LATENT_DIM];
        RandomPattern(next_state);
        ret = temporal_replay_store(replay, jepa_result->prediction->embedding,
                                    nullptr, next_state, 0.5f, false, 1.0f);
        EXPECT_EQ(ret, NIMCP_SUCCESS);

        pred_hier_result_destroy(hier_result);
        jepa_bidir_result_destroy(jepa_result);
        jepa_latent_destroy(input);
    }

    /* Verify all components have data */
    EXPECT_EQ(hopfield_memory_pattern_count(hopfield), (uint32_t)num_steps);

    replay_stats_t stats;
    temporal_replay_get_stats(replay, &stats);
    EXPECT_EQ(stats.transitions_stored, (uint32_t)num_steps);

    std::cout << "Full omni pipeline: " << num_steps << " steps completed" << std::endl;
}

TEST_F(OmniE2ETest, StressTestMultiplePredictions) {
    /* Test: Multiple predictions to verify stability */

    const int num_iterations = 100;
    int success_count = 0;

    for (int i = 0; i < num_iterations; i++) {
        jepa_latent_t* input = jepa_latent_create_dim(LATENT_DIM);
        RandomPattern(input->embedding);

        jepa_bidir_result_t* result = jepa_bidir_result_create(LATENT_DIM);

        int ret = jepa_bidirectional_predict(jepa, JEPA_DIR_FORWARD, input, result);
        if (ret == NIMCP_SUCCESS && result->prediction != nullptr) {
            success_count++;
        }

        jepa_bidir_result_destroy(result);
        jepa_latent_destroy(input);
    }

    EXPECT_EQ(success_count, num_iterations);
    std::cout << "Stress test: " << success_count << "/" << num_iterations
              << " successful" << std::endl;
}

/* ============================================================================
 * Integration Bridge Tests (DISABLED - Phase 2 Not Implemented)
 * ============================================================================ */

TEST_F(OmniE2ETest, DISABLED_SensoryBridgePipeline) {
    /* TODO: Implement when omni_sensory_bridge is created in Phase 2 */
    GTEST_SKIP() << "Sensory bridge not yet implemented (Phase 2)";
}

TEST_F(OmniE2ETest, DISABLED_CorticalColumnsPipeline) {
    /* TODO: Implement when omni_cortical_columns_bridge is created in Phase 2 */
    GTEST_SKIP() << "Cortical columns bridge not yet implemented (Phase 2)";
}

TEST_F(OmniE2ETest, DISABLED_OccipitalVisualPipeline) {
    /* TODO: Implement when omni_occipital_bridge is created in Phase 2 */
    GTEST_SKIP() << "Occipital bridge not yet implemented (Phase 2)";
}

TEST_F(OmniE2ETest, DISABLED_BrocaLanguagePipeline) {
    /* TODO: Implement when omni_broca_bridge is created in Phase 2 */
    GTEST_SKIP() << "Broca bridge not yet implemented (Phase 2)";
}

TEST_F(OmniE2ETest, DISABLED_KGIntegrationPipeline) {
    /* TODO: Implement when omni_kg_sync is created in Phase 2 */
    GTEST_SKIP() << "KG sync not yet implemented (Phase 2)";
}
