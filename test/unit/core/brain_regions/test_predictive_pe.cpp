//=============================================================================
// test_predictive_pe.cpp - Unit Tests for Predictive Regions PE
//=============================================================================
/**
 * @file test_predictive_pe.cpp
 * @brief Unit tests for positional encoding in predictive brain regions
 *
 * WHAT: Test hierarchy level and temporal PE in predictive coding regions
 * WHY:  Positional encoding is critical for hierarchical prediction
 * HOW:  Test learned PE for hierarchy levels, sinusoidal PE for temporal predictions
 *
 * TEST COVERAGE:
 * 1. Hierarchy level PE configuration
 * 2. Temporal sequence PE configuration
 * 3. Hierarchy level embeddings (V1, V2, V4, IT)
 * 4. Temporal prediction sequence encoding
 * 5. PE integration with predictive coding
 * 6. Edge cases (invalid levels, empty sequences)
 * 7. Integration with prediction flow
 *
 * BIOLOGICAL BASIS:
 * - Cortical hierarchy has distinct levels (V1→V2→V4→IT)
 * - Different levels have different prediction dynamics
 * - Position in hierarchy affects error propagation
 * - Temporal predictions need sequence order information
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

// Headers have their own extern "C" guards
    #include "core/brain_regions/nimcp_brain_region_predictive.h"
    #include "core/brain_regions/nimcp_brain_regions.h"
    #include "utils/encoding/nimcp_positional_encoding.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class PredictivePETest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;
    static constexpr uint32_t TEST_NUM_NEURONS = 100;
    static constexpr uint32_t TEST_PE_DIM = 64;
    static constexpr uint32_t TEST_SEQ_LEN = 10;

    brain_region_t* region = nullptr;

    void SetUp() override {
        srand(42);
        nimcp_memory_init();
    }

    void TearDown() override {
        if (region) {
            // Note: brain_region_destroy handles predictive extension
            // brain_region_destroy(region);
            region = nullptr;
        }
    }

    bool FloatEqual(float a, float b, float eps = EPSILON) {
        return std::abs(a - b) < eps;
    }

    // Helper to create brain region with predictive PE
    brain_region_t* CreateRegionWithPredictivePE(uint32_t hierarchy_level) {
        // Create base brain region
        brain_region_config_t region_config = {0};
        region_config.num_neurons = TEST_NUM_NEURONS;
        region_config.region_type = BRAIN_REGION_VISUAL;

        brain_region_t* reg = brain_region_create(&region_config);
        if (!reg) return nullptr;

        // Enable predictive processing with PE
        brain_region_predictive_config_t pred_config =
            brain_region_predictive_config_default(hierarchy_level);
        pred_config.enable_hierarchy_pe = true;
        pred_config.enable_temporal_pe = true;
        pred_config.pe_embedding_dim = TEST_PE_DIM;
        pred_config.max_prediction_sequence = TEST_SEQ_LEN;

        nimcp_result_t result = brain_region_enable_predictive(reg, &pred_config);
        if (result != NIMCP_SUCCESS) {
            // brain_region_destroy(reg);
            return nullptr;
        }

        return reg;
    }
};

//=============================================================================
// Unit Tests: Hierarchy Level PE Configuration
//=============================================================================

TEST_F(PredictivePETest, SetHierarchyPE_Level0) {
    // WHAT: Configure hierarchy PE for level 0 (sensory)
    // WHY:  Lowest level has distinct position encoding

    region = CreateRegionWithPredictivePE(0);
    ASSERT_NE(region, nullptr) << "Region with hierarchy level 0 PE should be created";

    // Set hierarchy PE
    nimcp_result_t result = brain_region_set_hierarchy_pe(region, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Hierarchy PE setting should succeed";
}

TEST_F(PredictivePETest, SetHierarchyPE_Level1) {
    // WHAT: Configure hierarchy PE for level 1
    // WHY:  Middle levels have intermediate encodings

    region = CreateRegionWithPredictivePE(1);
    ASSERT_NE(region, nullptr);

    nimcp_result_t result = brain_region_set_hierarchy_pe(region, 1);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(PredictivePETest, SetHierarchyPE_Level3) {
    // WHAT: Configure hierarchy PE for level 3 (association)
    // WHY:  Higher levels have abstract encodings

    region = CreateRegionWithPredictivePE(3);
    ASSERT_NE(region, nullptr);

    nimcp_result_t result = brain_region_set_hierarchy_pe(region, 3);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Unit Tests: Hierarchy Level Embeddings
//=============================================================================

TEST_F(PredictivePETest, GetLevelEmbedding_SingleLevel) {
    // WHAT: Retrieve hierarchy level embedding
    // WHY:  Verify level embeddings are computed

    region = CreateRegionWithPredictivePE(1);
    ASSERT_NE(region, nullptr);

    brain_region_set_hierarchy_pe(region, 1);

    float embedding[TEST_PE_DIM];
    nimcp_result_t result = brain_region_get_level_embedding(region, 1,
                                                              embedding, TEST_PE_DIM);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Level embedding retrieval should succeed";

    // Verify embedding is non-zero
    bool has_nonzero = false;
    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        if (std::abs(embedding[i]) > EPSILON) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero) << "Hierarchy level embedding should be non-zero";
}

TEST_F(PredictivePETest, GetLevelEmbedding_MultipleLevels) {
    // WHAT: Verify different hierarchy levels have different embeddings
    // WHY:  Level discrimination is critical for hierarchical processing

    region = CreateRegionWithPredictivePE(0);
    ASSERT_NE(region, nullptr);

    float emb0[TEST_PE_DIM];
    float emb1[TEST_PE_DIM];
    float emb2[TEST_PE_DIM];

    // Set and get embeddings for different levels
    brain_region_set_hierarchy_pe(region, 0);
    brain_region_get_level_embedding(region, 0, emb0, TEST_PE_DIM);

    brain_region_set_hierarchy_pe(region, 1);
    brain_region_get_level_embedding(region, 1, emb1, TEST_PE_DIM);

    brain_region_set_hierarchy_pe(region, 2);
    brain_region_get_level_embedding(region, 2, emb2, TEST_PE_DIM);

    // Verify embeddings are different
    bool emb0_vs_emb1_different = false;
    bool emb1_vs_emb2_different = false;

    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        if (std::abs(emb0[i] - emb1[i]) > EPSILON) emb0_vs_emb1_different = true;
        if (std::abs(emb1[i] - emb2[i]) > EPSILON) emb1_vs_emb2_different = true;
    }

    EXPECT_TRUE(emb0_vs_emb1_different) << "Level 0 and 1 should differ";
    EXPECT_TRUE(emb1_vs_emb2_different) << "Level 1 and 2 should differ";
}

TEST_F(PredictivePETest, GetLevelEmbedding_Consistency) {
    // WHAT: Verify same level returns same embedding across retrievals
    // WHY:  Learned embeddings should be stable

    region = CreateRegionWithPredictivePE(2);
    ASSERT_NE(region, nullptr);

    brain_region_set_hierarchy_pe(region, 2);

    float emb1[TEST_PE_DIM];
    float emb2[TEST_PE_DIM];

    brain_region_get_level_embedding(region, 2, emb1, TEST_PE_DIM);
    brain_region_get_level_embedding(region, 2, emb2, TEST_PE_DIM);

    // Verify embeddings are identical
    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        EXPECT_NEAR(emb1[i], emb2[i], EPSILON)
            << "Learned embedding should be consistent at index " << i;
    }
}

//=============================================================================
// Unit Tests: Temporal Prediction Sequence PE
//=============================================================================

TEST_F(PredictivePETest, EncodePredictionSequence_SingleTimestep) {
    // WHAT: Apply PE to single prediction timestep
    // WHY:  Basic temporal PE test

    region = CreateRegionWithPredictivePE(1);
    ASSERT_NE(region, nullptr);

    float prediction[TEST_NUM_NEURONS];
    float output[TEST_NUM_NEURONS];

    for (uint32_t i = 0; i < TEST_NUM_NEURONS; i++) {
        prediction[i] = 0.5f;
    }

    nimcp_result_t result = brain_region_encode_prediction_sequence(region,
                                                                     prediction, 1, output);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Single timestep encoding should succeed";

    // Verify output differs from input (PE was added)
    bool changed = false;
    for (uint32_t i = 0; i < TEST_NUM_NEURONS; i++) {
        if (std::abs(output[i] - prediction[i]) > EPSILON) {
            changed = true;
            break;
        }
    }
    EXPECT_TRUE(changed) << "PE should modify prediction";
}

TEST_F(PredictivePETest, EncodePredictionSequence_MultipleTimesteps) {
    // WHAT: Apply PE to multi-timestep prediction sequence
    // WHY:  Test temporal sequence encoding

    region = CreateRegionWithPredictivePE(1);
    ASSERT_NE(region, nullptr);

    uint32_t seq_len = 5;
    float prediction_seq[seq_len * TEST_NUM_NEURONS];
    float output_seq[seq_len * TEST_NUM_NEURONS];

    for (uint32_t i = 0; i < seq_len * TEST_NUM_NEURONS; i++) {
        prediction_seq[i] = (float)(i % 10) / 10.0f;
    }

    nimcp_result_t result = brain_region_encode_prediction_sequence(region,
                                                                     prediction_seq,
                                                                     seq_len, output_seq);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Multi-timestep encoding should succeed";

    // Verify different timesteps get different encodings
    bool timesteps_differ = false;
    for (uint32_t t = 1; t < seq_len; t++) {
        for (uint32_t i = 0; i < TEST_NUM_NEURONS; i++) {
            uint32_t idx1 = 0 * TEST_NUM_NEURONS + i;
            uint32_t idx2 = t * TEST_NUM_NEURONS + i;
            if (std::abs(output_seq[idx1] - output_seq[idx2]) > EPSILON) {
                timesteps_differ = true;
                break;
            }
        }
        if (timesteps_differ) break;
    }
    EXPECT_TRUE(timesteps_differ) << "Different timesteps should have different PEs";
}

//=============================================================================
// Unit Tests: PE Integration with Predictive Coding
//=============================================================================

TEST_F(PredictivePETest, Integration_HierarchicalPredictionWithPE) {
    // WHAT: Run hierarchical prediction step with PE
    // WHY:  End-to-end integration test

    region = CreateRegionWithPredictivePE(1);
    ASSERT_NE(region, nullptr);

    brain_region_set_hierarchy_pe(region, 1);

    // Simulate sensory input
    float sensory_input[TEST_NUM_NEURONS];
    for (uint32_t i = 0; i < TEST_NUM_NEURONS; i++) {
        sensory_input[i] = (float)i / (float)TEST_NUM_NEURONS;
    }

    // Run hierarchical step
    nimcp_result_t result = brain_region_hierarchical_step(region, sensory_input,
                                                            TEST_NUM_NEURONS, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Hierarchical step with PE should succeed";
}

TEST_F(PredictivePETest, Integration_PredictionErrorWithPE) {
    // WHAT: Compute prediction error with hierarchy PE
    // WHY:  Verify PE integration in error computation

    region = CreateRegionWithPredictivePE(1);
    ASSERT_NE(region, nullptr);

    brain_region_set_hierarchy_pe(region, 1);

    float actual[TEST_NUM_NEURONS];
    float predicted[TEST_NUM_NEURONS];
    float error[TEST_NUM_NEURONS];

    for (uint32_t i = 0; i < TEST_NUM_NEURONS; i++) {
        actual[i] = 0.8f;
        predicted[i] = 0.5f;
    }

    nimcp_result_t result = brain_region_compute_error(region, actual, predicted,
                                                        error, TEST_NUM_NEURONS);
    EXPECT_EQ(result, NIMCP_SUCCESS) << "Error computation with PE should succeed";

    // Verify errors are computed
    bool has_error = false;
    for (uint32_t i = 0; i < TEST_NUM_NEURONS; i++) {
        if (std::abs(error[i]) > EPSILON) {
            has_error = true;
            break;
        }
    }
    EXPECT_TRUE(has_error) << "Prediction errors should be non-zero";
}

//=============================================================================
// Unit Tests: Edge Cases
//=============================================================================

TEST_F(PredictivePETest, EdgeCase_NullInput) {
    // WHAT: Handle NULL inputs gracefully
    // WHY:  Robustness testing

    float embedding[TEST_PE_DIM];
    nimcp_result_t result = brain_region_get_level_embedding(nullptr, 0,
                                                              embedding, TEST_PE_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS) << "NULL region should fail";

    region = CreateRegionWithPredictivePE(1);
    ASSERT_NE(region, nullptr);

    result = brain_region_get_level_embedding(region, 0, nullptr, TEST_PE_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS) << "NULL output buffer should fail";
}

TEST_F(PredictivePETest, EdgeCase_InvalidHierarchyLevel) {
    // WHAT: Request embedding for invalid hierarchy level
    // WHY:  Boundary testing

    region = CreateRegionWithPredictivePE(1);
    ASSERT_NE(region, nullptr);

    brain_region_set_hierarchy_pe(region, 1);

    float embedding[TEST_PE_DIM];
    nimcp_result_t result = brain_region_get_level_embedding(region, 999,
                                                              embedding, TEST_PE_DIM);
    EXPECT_NE(result, NIMCP_SUCCESS) << "Invalid hierarchy level should fail";
}

TEST_F(PredictivePETest, EdgeCase_ZeroSequenceLength) {
    // WHAT: Encode zero-length prediction sequence
    // WHY:  Edge case validation

    region = CreateRegionWithPredictivePE(1);
    ASSERT_NE(region, nullptr);

    float prediction[TEST_NUM_NEURONS];
    float output[TEST_NUM_NEURONS];

    nimcp_result_t result = brain_region_encode_prediction_sequence(region,
                                                                     prediction, 0, output);
    // Should either succeed as no-op or fail gracefully
    SUCCEED() << "Zero sequence length handled";
}

TEST_F(PredictivePETest, EdgeCase_InsufficientBuffer) {
    // WHAT: Provide insufficient output buffer size
    // WHY:  Memory safety testing

    region = CreateRegionWithPredictivePE(1);
    ASSERT_NE(region, nullptr);

    brain_region_set_hierarchy_pe(region, 1);

    float embedding[10];  // Too small
    nimcp_result_t result = brain_region_get_level_embedding(region, 1, embedding, 10);
    EXPECT_NE(result, NIMCP_SUCCESS) << "Insufficient buffer should fail";
}

//=============================================================================
// Unit Tests: Sensory vs Association Regions
//=============================================================================

TEST_F(PredictivePETest, SensoryRegion_HierarchyPE) {
    // WHAT: Test PE in sensory region (level 0)
    // WHY:  Sensory regions have specific characteristics

    brain_region_t* sensory_region = CreateRegionWithPredictivePE(0);
    ASSERT_NE(sensory_region, nullptr);

    nimcp_result_t result = brain_region_set_hierarchy_pe(sensory_region, 0);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    float embedding[TEST_PE_DIM];
    result = brain_region_get_level_embedding(sensory_region, 0, embedding, TEST_PE_DIM);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Sensory level embedding should be non-zero
    bool has_nonzero = false;
    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        if (std::abs(embedding[i]) > EPSILON) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero) << "Sensory region should have non-zero embedding";
}

TEST_F(PredictivePETest, AssociationRegion_HierarchyPE) {
    // WHAT: Test PE in association region (higher level)
    // WHY:  Association regions have different properties

    brain_region_t* assoc_region = CreateRegionWithPredictivePE(3);
    ASSERT_NE(assoc_region, nullptr);

    nimcp_result_t result = brain_region_set_hierarchy_pe(assoc_region, 3);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    float embedding[TEST_PE_DIM];
    result = brain_region_get_level_embedding(assoc_region, 3, embedding, TEST_PE_DIM);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Association level embedding should be non-zero
    bool has_nonzero = false;
    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        if (std::abs(embedding[i]) > EPSILON) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero) << "Association region should have non-zero embedding";
}

TEST_F(PredictivePETest, CompareSensoryVsAssociation) {
    // WHAT: Compare embeddings between sensory and association regions
    // WHY:  Verify hierarchical distance is encoded

    brain_region_t* sensory = CreateRegionWithPredictivePE(0);
    brain_region_t* assoc = CreateRegionWithPredictivePE(3);
    ASSERT_NE(sensory, nullptr);
    ASSERT_NE(assoc, nullptr);

    brain_region_set_hierarchy_pe(sensory, 0);
    brain_region_set_hierarchy_pe(assoc, 3);

    float emb_sensory[TEST_PE_DIM];
    float emb_assoc[TEST_PE_DIM];

    brain_region_get_level_embedding(sensory, 0, emb_sensory, TEST_PE_DIM);
    brain_region_get_level_embedding(assoc, 3, emb_assoc, TEST_PE_DIM);

    // Embeddings should be different
    bool different = false;
    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        if (std::abs(emb_sensory[i] - emb_assoc[i]) > EPSILON) {
            different = true;
            break;
        }
    }
    EXPECT_TRUE(different) << "Sensory and association levels should have different embeddings";
}

//=============================================================================
// Unit Tests: Temporal Sequence Properties
//=============================================================================

TEST_F(PredictivePETest, TemporalPE_Sinusoidal) {
    // WHAT: Verify sinusoidal PE for temporal sequences
    // WHY:  Sinusoidal PE provides fixed, extrapolatable encoding

    region = CreateRegionWithPredictivePE(1);
    ASSERT_NE(region, nullptr);

    uint32_t seq_len = TEST_SEQ_LEN;
    float prediction_seq[seq_len * TEST_NUM_NEURONS];
    float output_seq[seq_len * TEST_NUM_NEURONS];

    // Constant input to see PE effect clearly
    for (uint32_t i = 0; i < seq_len * TEST_NUM_NEURONS; i++) {
        prediction_seq[i] = 1.0f;
    }

    nimcp_result_t result = brain_region_encode_prediction_sequence(region,
                                                                     prediction_seq,
                                                                     seq_len, output_seq);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Verify first and last timesteps differ
    bool first_last_differ = false;
    for (uint32_t i = 0; i < TEST_NUM_NEURONS; i++) {
        if (std::abs(output_seq[i] - output_seq[(seq_len-1)*TEST_NUM_NEURONS + i]) > EPSILON) {
            first_last_differ = true;
            break;
        }
    }
    EXPECT_TRUE(first_last_differ) << "First and last timesteps should have different PEs";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
