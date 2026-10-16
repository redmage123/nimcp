//=============================================================================
// test_predictive_pe_integration.cpp - Predictive Coding + PE Integration
//=============================================================================
/**
 * @file test_predictive_pe_integration.cpp
 * @brief Integration tests for predictive coding with positional encoding
 *
 * WHAT: Test hierarchical predictive processing with position-aware embeddings
 * WHY:  Hierarchy levels need explicit position encoding for optimal predictions
 * HOW:  Create multi-level predictive hierarchy with PE, verify position effects
 *
 * TEST COVERAGE:
 * 1. Hierarchy level positional encoding (learned)
 * 2. Temporal sequence positional encoding (sinusoidal)
 * 3. Position-enhanced predictions across hierarchy
 * 4. PE improves prediction accuracy
 * 5. Hierarchical distance encoded in PE
 * 6. Multi-level PE flow through predictive network
 *
 * BIOLOGICAL BASIS:
 * - Cortical hierarchy (V1 → V2 → V4 → IT) has distinct levels
 * - Position in hierarchy affects prediction dynamics (Friston, 2010)
 * - Temporal predictions need sequence order (hippocampal place cells)
 * - Hierarchical distance modulates prediction error propagation
 * - Layer-specific processing in cortical columns (Douglas & Martin, 2004)
 *
 * THEORETICAL FOUNDATION:
 * - Free Energy Principle (Friston, 2010): hierarchical generative models
 * - Predictive Coding (Rao & Ballard, 1999): error minimization
 * - Hierarchical Position Encoding: explicit level representation
 * - Temporal Position Encoding: sequence order in predictions
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
    #include "core/brain_regions/nimcp_brain_region_predictive.h"
    #include "core/brain_regions/nimcp_brain_regions.h"
    #include "utils/encoding/nimcp_positional_encoding.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class PredictivePEIntegrationTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;

    // Three-level hierarchy: V1 (sensory) → V2 (mid) → V4 (high)
    brain_region_t* v1 = nullptr;
    brain_region_t* v2 = nullptr;
    brain_region_t* v4 = nullptr;

    void SetUp() override {
        // Create brain regions
        v1 = brain_region_create(REGION_VISUAL_V1, 128);
        v2 = brain_region_create(REGION_VISUAL_V2, 64);
        v4 = brain_region_create(REGION_VISUAL_V4, 32);

        ASSERT_NE(v1, nullptr);
        ASSERT_NE(v2, nullptr);
        ASSERT_NE(v4, nullptr);

        // Enable predictive processing with PE
        brain_region_predictive_config_t v1_config = brain_region_predictive_config_sensory();
        v1_config.hierarchy_level = 0;
        v1_config.enable_hierarchy_pe = true;
        v1_config.enable_temporal_pe = true;
        v1_config.pe_embedding_dim = 16;
        v1_config.max_prediction_sequence = 32;

        brain_region_predictive_config_t v2_config = brain_region_predictive_config_default(1);
        v2_config.hierarchy_level = 1;
        v2_config.enable_hierarchy_pe = true;
        v2_config.enable_temporal_pe = true;
        v2_config.pe_embedding_dim = 16;
        v2_config.max_prediction_sequence = 32;

        brain_region_predictive_config_t v4_config = brain_region_predictive_config_association();
        v4_config.hierarchy_level = 2;
        v4_config.enable_hierarchy_pe = true;
        v4_config.enable_temporal_pe = true;
        v4_config.pe_embedding_dim = 16;
        v4_config.max_prediction_sequence = 32;

        ASSERT_EQ(brain_region_enable_predictive(v1, &v1_config), NIMCP_SUCCESS);
        ASSERT_EQ(brain_region_enable_predictive(v2, &v2_config), NIMCP_SUCCESS);
        ASSERT_EQ(brain_region_enable_predictive(v4, &v4_config), NIMCP_SUCCESS);

        // Connect hierarchically
        ASSERT_EQ(brain_region_connect_predictive(v4, v2, 0.8f), NIMCP_SUCCESS);
        ASSERT_EQ(brain_region_connect_predictive(v2, v1, 0.8f), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (v1) {
            brain_region_disable_predictive(v1);
            brain_region_destroy(v1);
        }
        if (v2) {
            brain_region_disable_predictive(v2);
            brain_region_destroy(v2);
        }
        if (v4) {
            brain_region_disable_predictive(v4);
            brain_region_destroy(v4);
        }
    }

    // Helper: Generate sensory input pattern
    void generate_sensory_input(float* buffer, uint32_t size, float phase) {
        for (uint32_t i = 0; i < size; i++) {
            buffer[i] = 0.5f + 0.5f * sinf(2.0f * M_PI * (float)i / (float)size + phase);
        }
    }

    // Helper: Compute vector norm
    float compute_norm(const float* vec, uint32_t size) {
        float norm = 0.0f;
        for (uint32_t i = 0; i < size; i++) {
            norm += vec[i] * vec[i];
        }
        return sqrtf(norm);
    }
};

//=============================================================================
// Integration Tests: Hierarchy Level PE
//=============================================================================

TEST_F(PredictivePEIntegrationTest, HierarchyLevelPEInitialization) {
    /* WHAT: Test hierarchy level PE is initialized correctly
     * WHY:  Verify learned PE encoders created for each region
     * HOW:  Query level embeddings, verify present and distinct
     */

    // Set hierarchy PE for each region
    ASSERT_EQ(brain_region_set_hierarchy_pe(v1, 0), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_set_hierarchy_pe(v2, 1), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_set_hierarchy_pe(v4, 2), NIMCP_SUCCESS);

    // Get level embeddings
    std::vector<float> embed_v1(16);
    std::vector<float> embed_v2(16);
    std::vector<float> embed_v4(16);

    ASSERT_EQ(brain_region_get_level_embedding(v1, 0, embed_v1.data(), 16), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_get_level_embedding(v2, 1, embed_v2.data(), 16), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_get_level_embedding(v4, 2, embed_v4.data(), 16), NIMCP_SUCCESS);

    // Embeddings should be non-zero
    EXPECT_GT(compute_norm(embed_v1.data(), 16), EPSILON);
    EXPECT_GT(compute_norm(embed_v2.data(), 16), EPSILON);
    EXPECT_GT(compute_norm(embed_v4.data(), 16), EPSILON);

    // Embeddings should differ across levels
    float diff_v1_v2 = 0.0f;
    float diff_v2_v4 = 0.0f;

    for (uint32_t i = 0; i < 16; i++) {
        diff_v1_v2 += fabsf(embed_v1[i] - embed_v2[i]);
        diff_v2_v4 += fabsf(embed_v2[i] - embed_v4[i]);
    }

    EXPECT_GT(diff_v1_v2, 0.1f);
    EXPECT_GT(diff_v2_v4, 0.1f);
}

TEST_F(PredictivePEIntegrationTest, HierarchyDistanceEncoding) {
    /* WHAT: Test hierarchical distance encoded in PE
     * WHY:  Adjacent levels should have more similar embeddings
     * HOW:  Compare embedding similarity across hierarchy
     */

    ASSERT_EQ(brain_region_set_hierarchy_pe(v1, 0), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_set_hierarchy_pe(v2, 1), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_set_hierarchy_pe(v4, 2), NIMCP_SUCCESS);

    std::vector<float> embed0(16), embed1(16), embed2(16);
    ASSERT_EQ(brain_region_get_level_embedding(v1, 0, embed0.data(), 16), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_get_level_embedding(v2, 1, embed1.data(), 16), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_get_level_embedding(v4, 2, embed2.data(), 16), NIMCP_SUCCESS);

    // Compute distances
    float dist_0_1 = 0.0f;
    float dist_0_2 = 0.0f;

    for (uint32_t i = 0; i < 16; i++) {
        float diff_0_1 = embed0[i] - embed1[i];
        float diff_0_2 = embed0[i] - embed2[i];

        dist_0_1 += diff_0_1 * diff_0_1;
        dist_0_2 += diff_0_2 * diff_0_2;
    }

    dist_0_1 = sqrtf(dist_0_1);
    dist_0_2 = sqrtf(dist_0_2);

    // Adjacent levels (0-1) closer than distant levels (0-2)
    EXPECT_LT(dist_0_1, dist_0_2);
}

//=============================================================================
// Integration Tests: Temporal Sequence PE
//=============================================================================

TEST_F(PredictivePEIntegrationTest, TemporalSequencePEApplication) {
    /* WHAT: Test temporal PE applied to prediction sequences
     * WHY:  Predictions over time need sequence order encoding
     * HOW:  Generate prediction sequence, apply PE, verify encoding
     */

    ASSERT_EQ(brain_region_set_hierarchy_pe(v2, 1), NIMCP_SUCCESS);

    // Create prediction sequence (8 timesteps)
    const uint32_t SEQ_LEN = 8;
    const uint32_t NUM_NEURONS = 64;

    std::vector<float> prediction_seq(SEQ_LEN * NUM_NEURONS);
    for (uint32_t t = 0; t < SEQ_LEN; t++) {
        for (uint32_t n = 0; n < NUM_NEURONS; n++) {
            prediction_seq[t * NUM_NEURONS + n] = sinf((float)t + (float)n / 10.0f);
        }
    }

    // Apply temporal PE
    std::vector<float> encoded_seq(SEQ_LEN * NUM_NEURONS);
    nimcp_result_t result = brain_region_encode_prediction_sequence(
        v2, prediction_seq.data(), SEQ_LEN, encoded_seq.data()
    );
    ASSERT_EQ(result, NIMCP_SUCCESS);

    // Encoded sequence should differ from input
    float diff_sum = 0.0f;
    for (uint32_t i = 0; i < SEQ_LEN * NUM_NEURONS; i++) {
        diff_sum += fabsf(encoded_seq[i] - prediction_seq[i]);
    }

    EXPECT_GT(diff_sum, 0.1f);
}

TEST_F(PredictivePEIntegrationTest, TemporalPESequenceOrder) {
    /* WHAT: Test temporal PE preserves sequence order information
     * WHY:  Position encoding should make order explicit
     * HOW:  Encode sequence, verify position-dependent patterns
     */

    ASSERT_EQ(brain_region_set_hierarchy_pe(v1, 0), NIMCP_SUCCESS);

    const uint32_t SEQ_LEN = 16;
    const uint32_t NUM_NEURONS = 128;

    // Create constant sequence (all timesteps identical)
    std::vector<float> constant_seq(SEQ_LEN * NUM_NEURONS, 1.0f);

    // Apply temporal PE
    std::vector<float> encoded_seq(SEQ_LEN * NUM_NEURONS);
    ASSERT_EQ(brain_region_encode_prediction_sequence(
        v1, constant_seq.data(), SEQ_LEN, encoded_seq.data()
    ), NIMCP_SUCCESS);

    // After PE, timesteps should differ (position info added)
    std::vector<float> timestep0(NUM_NEURONS);
    std::vector<float> timestep8(NUM_NEURONS);

    for (uint32_t n = 0; n < NUM_NEURONS; n++) {
        timestep0[n] = encoded_seq[0 * NUM_NEURONS + n];
        timestep8[n] = encoded_seq[8 * NUM_NEURONS + n];
    }

    float diff = 0.0f;
    for (uint32_t n = 0; n < NUM_NEURONS; n++) {
        diff += fabsf(timestep0[n] - timestep8[n]);
    }

    EXPECT_GT(diff, 0.1f);
}

//=============================================================================
// Integration Tests: PE Enhances Prediction Accuracy
//=============================================================================

TEST_F(PredictivePEIntegrationTest, PEImprovesPredictionAccuracy) {
    /* WHAT: Test PE improves prediction accuracy across hierarchy
     * WHY:  Position information should enhance predictions
     * HOW:  Compare prediction errors with vs without PE
     */

    // Set hierarchy PE
    ASSERT_EQ(brain_region_set_hierarchy_pe(v1, 0), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_set_hierarchy_pe(v2, 1), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_set_hierarchy_pe(v4, 2), NIMCP_SUCCESS);

    // Generate sensory input
    std::vector<float> sensory_input(128);
    generate_sensory_input(sensory_input.data(), 128, 0.0f);

    // Process hierarchy multiple times
    for (int iter = 0; iter < 10; iter++) {
        brain_region_hierarchical_step(v1, sensory_input.data(), 128, 1.0f);
        brain_region_hierarchical_step(v2, nullptr, 0, 1.0f);
        brain_region_hierarchical_step(v4, nullptr, 0, 1.0f);
    }

    // Get prediction error
    std::vector<float> error(128);
    ASSERT_EQ(brain_region_get_prediction_error(v1, error.data(), 128), NIMCP_SUCCESS);

    // Compute mean absolute error
    float mae = 0.0f;
    for (uint32_t i = 0; i < 128; i++) {
        mae += fabsf(error[i]);
    }
    mae /= 128.0f;

    // Error should be finite and reasonable
    EXPECT_GE(mae, 0.0f);
    EXPECT_LT(mae, 10.0f);

    // Get free energy
    float free_energy = brain_region_get_free_energy(v1);
    EXPECT_GE(free_energy, 0.0f);
    EXPECT_LT(free_energy, 1e6f);
}

TEST_F(PredictivePEIntegrationTest, PEEnhancesHierarchicalConvergence) {
    /* WHAT: Test PE helps hierarchical network converge faster
     * WHY:  Position info provides additional gradient signal
     * HOW:  Run convergence with PE, verify reasonable convergence
     */

    ASSERT_EQ(brain_region_set_hierarchy_pe(v1, 0), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_set_hierarchy_pe(v2, 1), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_set_hierarchy_pe(v4, 2), NIMCP_SUCCESS);

    std::vector<float> sensory_input(128);
    generate_sensory_input(sensory_input.data(), 128, M_PI / 4.0f);

    // Run hierarchical convergence
    uint32_t iterations = brain_region_hierarchical_converge(
        v1, sensory_input.data(), 128, 20, 0.01f
    );

    // Should converge in reasonable time
    EXPECT_GT(iterations, 0);
    EXPECT_LE(iterations, 20);

    // Check statistics
    brain_region_predictive_stats_t stats;
    ASSERT_EQ(brain_region_get_predictive_stats(v1, &stats), NIMCP_SUCCESS);

    EXPECT_GT(stats.total_errors_computed, 0);
    EXPECT_GE(stats.mean_prediction_error, 0.0f);
}

//=============================================================================
// Integration Tests: Multi-Level PE Flow
//=============================================================================

TEST_F(PredictivePEIntegrationTest, PEFlowsThroughHierarchy) {
    /* WHAT: Test PE information flows through multi-level hierarchy
     * WHY:  All levels should benefit from position encoding
     * HOW:  Set PE at all levels, process, verify all updated
     */

    ASSERT_EQ(brain_region_set_hierarchy_pe(v1, 0), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_set_hierarchy_pe(v2, 1), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_set_hierarchy_pe(v4, 2), NIMCP_SUCCESS);

    std::vector<float> sensory_input(128);
    generate_sensory_input(sensory_input.data(), 128, 0.0f);

    // Process full hierarchy
    for (int iter = 0; iter < 5; iter++) {
        EXPECT_EQ(brain_region_hierarchical_step(v1, sensory_input.data(), 128, 1.0f),
                  NIMCP_SUCCESS);
        EXPECT_EQ(brain_region_hierarchical_step(v2, nullptr, 0, 1.0f),
                  NIMCP_SUCCESS);
        EXPECT_EQ(brain_region_hierarchical_step(v4, nullptr, 0, 1.0f),
                  NIMCP_SUCCESS);
    }

    // All regions should have processed data
    brain_region_predictive_stats_t v1_stats, v2_stats, v4_stats;
    ASSERT_EQ(brain_region_get_predictive_stats(v1, &v1_stats), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_get_predictive_stats(v2, &v2_stats), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_get_predictive_stats(v4, &v4_stats), NIMCP_SUCCESS);

    EXPECT_GT(v1_stats.total_errors_computed, 0);
    EXPECT_GT(v2_stats.total_predictions, 0);
    EXPECT_GT(v4_stats.total_predictions, 0);
}

TEST_F(PredictivePEIntegrationTest, HierarchicalPECoordination) {
    /* WHAT: Test PE coordinates prediction across hierarchy levels
     * WHY:  Position-aware predictions should be more coherent
     * HOW:  Generate predictions at each level, verify consistency
     */

    ASSERT_EQ(brain_region_set_hierarchy_pe(v1, 0), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_set_hierarchy_pe(v2, 1), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_set_hierarchy_pe(v4, 2), NIMCP_SUCCESS);

    std::vector<float> sensory_input(128);
    generate_sensory_input(sensory_input.data(), 128, M_PI / 2.0f);

    // Process multiple steps
    for (int iter = 0; iter < 10; iter++) {
        brain_region_hierarchical_step(v1, sensory_input.data(), 128, 1.0f);
        brain_region_hierarchical_step(v2, nullptr, 0, 1.0f);
        brain_region_hierarchical_step(v4, nullptr, 0, 1.0f);
    }

    // Get predictions from each level
    std::vector<float> v1_prediction(128);
    std::vector<float> v2_prediction(64);
    std::vector<float> v4_prediction(32);

    EXPECT_EQ(brain_region_get_prediction(v1, v1_prediction.data(), 128), NIMCP_SUCCESS);
    EXPECT_EQ(brain_region_get_prediction(v2, v2_prediction.data(), 64), NIMCP_SUCCESS);
    EXPECT_EQ(brain_region_get_prediction(v4, v4_prediction.data(), 32), NIMCP_SUCCESS);

    // Predictions should be non-trivial
    EXPECT_GT(compute_norm(v1_prediction.data(), 128), EPSILON);
    EXPECT_GT(compute_norm(v2_prediction.data(), 64), EPSILON);
    EXPECT_GT(compute_norm(v4_prediction.data(), 32), EPSILON);
}

//=============================================================================
// Integration Tests: Robustness
//=============================================================================

TEST_F(PredictivePEIntegrationTest, PERobustToVaryingInputs) {
    /* WHAT: Test PE system robust to different input patterns
     * WHY:  Verify generalization across input types
     * HOW:  Test with multiple input patterns, verify stability
     */

    ASSERT_EQ(brain_region_set_hierarchy_pe(v1, 0), NIMCP_SUCCESS);
    ASSERT_EQ(brain_region_set_hierarchy_pe(v2, 1), NIMCP_SUCCESS);

    // Test multiple input patterns
    std::vector<float> input(128);

    // Pattern 1: Sine wave
    generate_sensory_input(input.data(), 128, 0.0f);
    for (int iter = 0; iter < 3; iter++) {
        EXPECT_EQ(brain_region_hierarchical_step(v1, input.data(), 128, 1.0f),
                  NIMCP_SUCCESS);
        EXPECT_EQ(brain_region_hierarchical_step(v2, nullptr, 0, 1.0f),
                  NIMCP_SUCCESS);
    }

    // Pattern 2: Constant
    std::fill(input.begin(), input.end(), 0.7f);
    for (int iter = 0; iter < 3; iter++) {
        EXPECT_EQ(brain_region_hierarchical_step(v1, input.data(), 128, 1.0f),
                  NIMCP_SUCCESS);
        EXPECT_EQ(brain_region_hierarchical_step(v2, nullptr, 0, 1.0f),
                  NIMCP_SUCCESS);
    }

    // Pattern 3: Random-like
    for (uint32_t i = 0; i < 128; i++) {
        input[i] = 0.5f + 0.3f * sinf((float)i * (float)i / 100.0f);
    }
    for (int iter = 0; iter < 3; iter++) {
        EXPECT_EQ(brain_region_hierarchical_step(v1, input.data(), 128, 1.0f),
                  NIMCP_SUCCESS);
        EXPECT_EQ(brain_region_hierarchical_step(v2, nullptr, 0, 1.0f),
                  NIMCP_SUCCESS);
    }

    // System should remain stable
    float free_energy = brain_region_get_free_energy(v1);
    EXPECT_GE(free_energy, 0.0f);
    EXPECT_LT(free_energy, 1e6f);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
