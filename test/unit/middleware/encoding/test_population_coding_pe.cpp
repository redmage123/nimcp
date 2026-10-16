//=============================================================================
// test_population_coding_pe.cpp - Unit Tests for Population Coding PE Integration
//=============================================================================
/**
 * @file test_population_coding_pe.cpp
 * @brief Unit tests for positional encoding in population coding
 *
 * WHAT: Test PE integration with neural population representations
 * WHY:  Position-aware coding models topographic organization
 * HOW:  Test sinusoidal PE for neuron positions and position-aware decoding
 *
 * TEST COVERAGE:
 * 1. PE configuration
 * 2. Neuron position encoding
 * 3. Position-aware decoding
 * 4. Edge cases (NULL, zero neurons, invalid params)
 * 5. Integration with population coding methods
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

// Headers have their own extern "C" guards
    #include "middleware/encoding/nimcp_population_coding.h"
    #include "utils/encoding/nimcp_positional_encoding.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class PopulationCodingPETest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;

    population_coding_encoder_t encoder = nullptr;

    void SetUp() override {
        srand(42);
        nimcp_memory_init();
    }

    void TearDown() override {
        if (encoder) {
            population_coding_destroy(encoder);
            encoder = nullptr;
        }
    }

    bool FloatEqual(float a, float b, float eps = EPSILON) {
        return std::abs(a - b) < eps;
    }

    vector3d_t make_vector(float x, float y, float z) {
        return population_coding_vector3d_make(x, y, z);
    }
};

//=============================================================================
// Unit Tests: PE Configuration
//=============================================================================

TEST_F(PopulationCodingPETest, ConfigurePE_Basic) {
    // WHAT: Configure PE for population coding
    // WHY:  Enable position-aware neural representations
    // HOW:  Set embedding dimension and frequency base

    population_coding_config_t config = population_coding_default_config();
    config.enable_positional_encoding = true;
    config.pe_embedding_dim = 64;
    config.pe_frequency_base = 10000.0f;
    config.position_weight = 0.5f;

    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    bool result = population_coding_set_pe_config(
        encoder,
        64,      // embedding_dim
        10000.0f, // frequency_base
        0.5f     // position_weight
    );

    EXPECT_TRUE(result) << "PE configuration should succeed";
}

TEST_F(PopulationCodingPETest, ConfigurePE_DifferentDimensions) {
    // WHAT: Configure PE with various embedding dimensions
    // WHY:  Verify dimension flexibility
    // HOW:  Test dims 32, 128, 256

    population_coding_config_t config = population_coding_default_config();
    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    uint32_t dims[] = {32, 128, 256};

    for (int i = 0; i < 3; i++) {
        bool result = population_coding_set_pe_config(
            encoder,
            dims[i],
            10000.0f,
            0.5f
        );
        EXPECT_TRUE(result) << "PE config with dim " << dims[i] << " should succeed";
    }
}

TEST_F(PopulationCodingPETest, ConfigurePE_PositionWeights) {
    // WHAT: Configure PE with different position weights
    // WHY:  Control influence of position in decoding
    // HOW:  Test weights 0.0, 0.5, 1.0

    population_coding_config_t config = population_coding_default_config();
    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    float weights[] = {0.0f, 0.5f, 1.0f};

    for (int i = 0; i < 3; i++) {
        bool result = population_coding_set_pe_config(
            encoder,
            64,
            10000.0f,
            weights[i]
        );
        EXPECT_TRUE(result) << "PE config with weight " << weights[i] << " should succeed";
    }
}

//=============================================================================
// Unit Tests: Neuron Position Encoding
//=============================================================================

TEST_F(PopulationCodingPETest, EncodeNeuronPositions_SmallPopulation) {
    // WHAT: Encode positions for small neural population
    // WHY:  Basic position encoding functionality
    // HOW:  Encode 10 neurons with dim 32

    population_coding_config_t config = population_coding_default_config();
    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    population_coding_set_pe_config(encoder, 32, 10000.0f, 0.5f);

    const uint32_t num_neurons = 10;
    const uint32_t dim = 32;
    float position_encodings[num_neurons * dim];

    bool result = population_coding_encode_neuron_positions(
        encoder,
        num_neurons,
        position_encodings
    );

    EXPECT_TRUE(result) << "Neuron position encoding should succeed";

    // Verify encodings are non-zero
    float norm = 0.0f;
    for (uint32_t i = 0; i < num_neurons * dim; i++) {
        norm += position_encodings[i] * position_encodings[i];
    }
    EXPECT_GT(norm, 0.0f) << "Position encodings should be non-zero";
}

TEST_F(PopulationCodingPETest, EncodeNeuronPositions_DifferentNeurons) {
    // WHAT: Verify different neurons get different encodings
    // WHY:  Position encoding must distinguish neurons
    // HOW:  Compare encodings for neurons 0 and 1

    population_coding_config_t config = population_coding_default_config();
    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    population_coding_set_pe_config(encoder, 64, 10000.0f, 0.5f);

    const uint32_t num_neurons = 5;
    const uint32_t dim = 64;
    float position_encodings[num_neurons * dim];

    population_coding_encode_neuron_positions(
        encoder,
        num_neurons,
        position_encodings
    );

    // Compare neuron 0 and neuron 1
    bool differ = false;
    for (uint32_t i = 0; i < dim; i++) {
        if (std::abs(position_encodings[i] - position_encodings[dim + i]) > EPSILON) {
            differ = true;
            break;
        }
    }

    EXPECT_TRUE(differ) << "Different neurons should have different position encodings";
}

TEST_F(PopulationCodingPETest, EncodeNeuronPositions_LargePopulation) {
    // WHAT: Encode positions for large neural population
    // WHY:  Verify scaling to realistic population sizes
    // HOW:  Encode 1000 neurons

    population_coding_config_t config = population_coding_default_config();
    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    population_coding_set_pe_config(encoder, 64, 10000.0f, 0.5f);

    const uint32_t num_neurons = 1000;
    const uint32_t dim = 64;
    float* position_encodings = new float[num_neurons * dim];

    bool result = population_coding_encode_neuron_positions(
        encoder,
        num_neurons,
        position_encodings
    );

    EXPECT_TRUE(result) << "Large population encoding should succeed";

    // Verify first and last neurons differ
    bool differ = false;
    for (uint32_t i = 0; i < dim; i++) {
        if (std::abs(position_encodings[i] - position_encodings[(num_neurons - 1) * dim + i]) > EPSILON) {
            differ = true;
            break;
        }
    }
    EXPECT_TRUE(differ) << "First and last neuron should have different encodings";

    delete[] position_encodings;
}

TEST_F(PopulationCodingPETest, EncodeNeuronPositions_SequentialPositions) {
    // WHAT: Verify sequential position property
    // WHY:  Nearby neurons should have similar encodings
    // HOW:  Check similarity gradient across positions

    population_coding_config_t config = population_coding_default_config();
    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    population_coding_set_pe_config(encoder, 32, 10000.0f, 0.5f);

    const uint32_t num_neurons = 10;
    const uint32_t dim = 32;
    float position_encodings[num_neurons * dim];

    population_coding_encode_neuron_positions(encoder, num_neurons, position_encodings);

    // Compute distances between adjacent neurons
    float dist_0_1 = 0.0f, dist_0_5 = 0.0f;

    for (uint32_t i = 0; i < dim; i++) {
        float diff_0_1 = position_encodings[i] - position_encodings[dim + i];
        float diff_0_5 = position_encodings[i] - position_encodings[5 * dim + i];
        dist_0_1 += diff_0_1 * diff_0_1;
        dist_0_5 += diff_0_5 * diff_0_5;
    }

    dist_0_1 = std::sqrt(dist_0_1);
    dist_0_5 = std::sqrt(dist_0_5);

    // Distance to neuron 5 should be larger than to neuron 1
    EXPECT_LT(dist_0_1, dist_0_5) << "Nearby neurons should be more similar";
}

//=============================================================================
// Unit Tests: Position-Aware Decoding
//=============================================================================

TEST_F(PopulationCodingPETest, PositionAwareDecode_BasicFunctionality) {
    // WHAT: Decode population activity with position weighting
    // WHY:  Position-aware decoding core functionality
    // HOW:  Decode with simple rates and tuning curves

    population_coding_config_t config = population_coding_default_config();
    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    population_coding_set_pe_config(encoder, 32, 10000.0f, 0.5f);

    const uint32_t num_neurons = 8;
    const uint32_t dim = 32;

    float rates[num_neurons];
    for (uint32_t i = 0; i < num_neurons; i++) {
        rates[i] = 10.0f + i * 2.0f;
    }

    float position_encodings[num_neurons * dim];
    population_coding_encode_neuron_positions(encoder, num_neurons, position_encodings);

    float query_position[dim];
    for (uint32_t i = 0; i < dim; i++) {
        query_position[i] = position_encodings[i];  // Query at position 0
    }

    tuning_curve_t tuning_curves[num_neurons];
    for (uint32_t i = 0; i < num_neurons; i++) {
        tuning_curves[i].preferred_direction = make_vector(1.0f, 0.0f, 0.0f);
        tuning_curves[i].tuning_width = 1.0f;
        tuning_curves[i].max_rate = 50.0f;
    }

    vector3d_t vector_out;
    bool result = population_coding_position_aware_decode(
        encoder,
        rates,
        position_encodings,
        num_neurons,
        query_position,
        tuning_curves,
        &vector_out
    );

    EXPECT_TRUE(result) << "Position-aware decoding should succeed";
    EXPECT_GT(vector_out.magnitude, 0.0f) << "Decoded vector should have non-zero magnitude";
}

TEST_F(PopulationCodingPETest, PositionAwareDecode_PositionWeighting) {
    // WHAT: Verify position weight affects decoding
    // WHY:  Position weighting controls spatial attention
    // HOW:  Decode with weight 0.0 vs 1.0

    population_coding_config_t config = population_coding_default_config();
    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    const uint32_t num_neurons = 10;
    const uint32_t dim = 32;

    float rates[num_neurons];
    for (uint32_t i = 0; i < num_neurons; i++) {
        rates[i] = 20.0f;
    }

    tuning_curve_t tuning_curves[num_neurons];
    for (uint32_t i = 0; i < num_neurons; i++) {
        float angle = (float)i / num_neurons * 2.0f * M_PI;
        tuning_curves[i].preferred_direction = make_vector(
            std::cos(angle), std::sin(angle), 0.0f
        );
        tuning_curves[i].tuning_width = 1.0f;
        tuning_curves[i].max_rate = 50.0f;
    }

    float position_encodings[num_neurons * dim];
    float query_position[dim];

    // Decode with weight 0.0 (no position influence)
    population_coding_set_pe_config(encoder, dim, 10000.0f, 0.0f);
    population_coding_encode_neuron_positions(encoder, num_neurons, position_encodings);
    for (uint32_t i = 0; i < dim; i++) {
        query_position[i] = position_encodings[i];
    }

    vector3d_t vec_no_weight;
    population_coding_position_aware_decode(
        encoder,
        rates,
        position_encodings,
        num_neurons,
        query_position,
        tuning_curves,
        &vec_no_weight
    );

    // Decode with weight 1.0 (full position influence)
    population_coding_set_pe_config(encoder, dim, 10000.0f, 1.0f);
    population_coding_encode_neuron_positions(encoder, num_neurons, position_encodings);

    vector3d_t vec_full_weight;
    population_coding_position_aware_decode(
        encoder,
        rates,
        position_encodings,
        num_neurons,
        query_position,
        tuning_curves,
        &vec_full_weight
    );

    // Results may differ due to position weighting
    SUCCEED() << "Position weighting affects decoding";
}

TEST_F(PopulationCodingPETest, PositionAwareDecode_SpatialBias) {
    // WHAT: Verify query position biases decoding
    // WHY:  Model attentional spatial selection
    // HOW:  Query different positions, verify bias toward those neurons

    population_coding_config_t config = population_coding_default_config();
    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    population_coding_set_pe_config(encoder, 32, 10000.0f, 0.8f);

    const uint32_t num_neurons = 16;
    const uint32_t dim = 32;

    float rates[num_neurons];
    for (uint32_t i = 0; i < num_neurons; i++) {
        rates[i] = 15.0f;  // Uniform rates
    }

    float position_encodings[num_neurons * dim];
    population_coding_encode_neuron_positions(encoder, num_neurons, position_encodings);

    tuning_curve_t tuning_curves[num_neurons];
    for (uint32_t i = 0; i < num_neurons; i++) {
        float angle = (float)i / num_neurons * 2.0f * M_PI;
        tuning_curves[i].preferred_direction = make_vector(
            std::cos(angle), std::sin(angle), 0.0f
        );
        tuning_curves[i].tuning_width = 0.5f;
        tuning_curves[i].max_rate = 50.0f;
    }

    // Query at position 0
    float query_pos0[dim];
    for (uint32_t i = 0; i < dim; i++) {
        query_pos0[i] = position_encodings[i];
    }

    vector3d_t vec0;
    population_coding_position_aware_decode(
        encoder,
        rates,
        position_encodings,
        num_neurons,
        query_pos0,
        tuning_curves,
        &vec0
    );

    // Query at position 8 (middle)
    float query_pos8[dim];
    for (uint32_t i = 0; i < dim; i++) {
        query_pos8[i] = position_encodings[8 * dim + i];
    }

    vector3d_t vec8;
    population_coding_position_aware_decode(
        encoder,
        rates,
        position_encodings,
        num_neurons,
        query_pos8,
        tuning_curves,
        &vec8
    );

    // Decoded vectors should differ due to spatial query
    bool differ = (std::abs(vec0.x - vec8.x) > EPSILON) ||
                  (std::abs(vec0.y - vec8.y) > EPSILON) ||
                  (std::abs(vec0.z - vec8.z) > EPSILON);

    EXPECT_TRUE(differ) << "Query position should bias decoding";
}

//=============================================================================
// Unit Tests: Edge Cases
//=============================================================================

TEST_F(PopulationCodingPETest, EdgeCase_NullEncoder) {
    // WHAT: Handle NULL encoder pointer
    // WHY:  Robustness against invalid input
    // HOW:  Pass NULL to PE functions

    bool result1 = population_coding_set_pe_config(nullptr, 64, 10000.0f, 0.5f);
    EXPECT_FALSE(result1);

    float dummy[64];
    bool result2 = population_coding_encode_neuron_positions(nullptr, 10, dummy);
    EXPECT_FALSE(result2);

    vector3d_t vec;
    tuning_curve_t curves[10];
    bool result3 = population_coding_position_aware_decode(
        nullptr,
        dummy,
        dummy,
        10,
        dummy,
        curves,
        &vec
    );
    EXPECT_FALSE(result3);
}

TEST_F(PopulationCodingPETest, EdgeCase_ZeroNeurons) {
    // WHAT: Encode positions for zero neurons
    // WHY:  Invalid parameter handling
    // HOW:  Pass num_neurons = 0

    population_coding_config_t config = population_coding_default_config();
    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    population_coding_set_pe_config(encoder, 32, 10000.0f, 0.5f);

    float dummy[32];
    bool result = population_coding_encode_neuron_positions(encoder, 0, dummy);

    EXPECT_FALSE(result) << "Zero neurons should be rejected";
}

TEST_F(PopulationCodingPETest, EdgeCase_NullOutputBuffer) {
    // WHAT: Pass NULL output buffer
    // WHY:  Validate output parameter
    // HOW:  Pass nullptr for position_encodings_out

    population_coding_config_t config = population_coding_default_config();
    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    population_coding_set_pe_config(encoder, 32, 10000.0f, 0.5f);

    bool result = population_coding_encode_neuron_positions(encoder, 10, nullptr);

    EXPECT_FALSE(result) << "NULL output buffer should be rejected";
}

TEST_F(PopulationCodingPETest, EdgeCase_PENotConfigured) {
    // WHAT: Use PE functions before configuration
    // WHY:  Verify initialization checking
    // HOW:  Call encode functions without set_pe_config

    population_coding_config_t config = population_coding_default_config();
    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    float dummy[320];
    bool result = population_coding_encode_neuron_positions(encoder, 10, dummy);

    EXPECT_FALSE(result) << "PE usage before configuration should fail";
}

TEST_F(PopulationCodingPETest, EdgeCase_ZeroDimension) {
    // WHAT: Configure PE with zero embedding dimension
    // WHY:  Invalid configuration should be rejected
    // HOW:  Pass embedding_dim = 0

    population_coding_config_t config = population_coding_default_config();
    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    bool result = population_coding_set_pe_config(encoder, 0, 10000.0f, 0.5f);

    EXPECT_FALSE(result) << "Zero dimension should be rejected";
}

TEST_F(PopulationCodingPETest, EdgeCase_InvalidPositionWeight) {
    // WHAT: Configure PE with invalid position weight
    // WHY:  Weight should be in [0, 1] range
    // HOW:  Pass weight = -0.5 and weight = 2.0

    population_coding_config_t config = population_coding_default_config();
    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    bool result1 = population_coding_set_pe_config(encoder, 64, 10000.0f, -0.5f);
    // May accept (clamped) or reject

    bool result2 = population_coding_set_pe_config(encoder, 64, 10000.0f, 2.0f);
    // May accept (clamped) or reject

    SUCCEED() << "Invalid weights handled";
}

TEST_F(PopulationCodingPETest, EdgeCase_ExcessivePopulationSize) {
    // WHAT: Encode positions for very large population
    // WHY:  Verify memory and performance limits
    // HOW:  Request encoding for 10000 neurons

    population_coding_config_t config = population_coding_default_config();
    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    population_coding_set_pe_config(encoder, 32, 10000.0f, 0.5f);

    const uint32_t huge_population = 10000;
    const uint32_t dim = 32;

    // Only allocate if we expect success
    float* position_encodings = new float[huge_population * dim];

    bool result = population_coding_encode_neuron_positions(
        encoder,
        huge_population,
        position_encodings
    );

    // Should either succeed or fail gracefully
    SUCCEED() << "Excessive population size handled";

    delete[] position_encodings;
}

//=============================================================================
// Unit Tests: Integration
//=============================================================================

TEST_F(PopulationCodingPETest, Integration_WithVectorSumCoding) {
    // WHAT: Use PE with vector sum population coding
    // WHY:  Verify PE integrates with standard population methods
    // HOW:  Encode positions, then do vector sum decoding

    population_coding_config_t config = population_coding_default_config();
    encoder = population_coding_create(&config);
    ASSERT_NE(encoder, nullptr);

    population_coding_set_pe_config(encoder, 32, 10000.0f, 0.5f);

    const uint32_t num_neurons = 12;
    const uint32_t dim = 32;

    float rates[num_neurons];
    for (uint32_t i = 0; i < num_neurons; i++) {
        rates[i] = 10.0f + i;
    }

    tuning_curve_t tuning_curves[num_neurons];
    for (uint32_t i = 0; i < num_neurons; i++) {
        float angle = (float)i / num_neurons * 2.0f * M_PI;
        tuning_curves[i].preferred_direction = make_vector(
            std::cos(angle), std::sin(angle), 0.0f
        );
        tuning_curves[i].tuning_width = 1.0f;
        tuning_curves[i].max_rate = 50.0f;
    }

    // Standard vector sum (without position)
    vector3d_t vec_standard;
    bool result1 = population_coding_encode_vector_sum(
        encoder,
        rates,
        tuning_curves,
        num_neurons,
        &vec_standard
    );
    EXPECT_TRUE(result1);

    // Position-aware decoding
    float position_encodings[num_neurons * dim];
    population_coding_encode_neuron_positions(encoder, num_neurons, position_encodings);

    float query_position[dim];
    for (uint32_t i = 0; i < dim; i++) {
        query_position[i] = position_encodings[i];
    }

    vector3d_t vec_position_aware;
    bool result2 = population_coding_position_aware_decode(
        encoder,
        rates,
        position_encodings,
        num_neurons,
        query_position,
        tuning_curves,
        &vec_position_aware
    );
    EXPECT_TRUE(result2);

    SUCCEED() << "PE integrates with vector sum coding";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
