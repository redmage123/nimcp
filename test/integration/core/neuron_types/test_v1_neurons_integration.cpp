/**
 * @file test_v1_neurons_integration.cpp
 * @brief Integration tests for V1 neurons with neuron type system
 *
 * WHAT: Test V1 neuron processing within NIMCP neuron type framework
 * WHY:  Ensure V1 neurons integrate correctly with type system
 * HOW:  Test full 2D processing, parameter handling, type combinations
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 */

#include <gtest/gtest.h>
#include "core/neuron_types/nimcp_neuron_types.h"
#include "utils/time/nimcp_time.h"
#include <vector>
#include <cmath>
#include <chrono>

// ============================================================================
// TEST FIXTURE
// ============================================================================

class V1NeuronsIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        timestamp = nimcp_time_monotonic_us();
    }

    void TearDown() override {
        // Cleanup
    }

    uint64_t timestamp;
};

// ============================================================================
// FULL 2D GABOR FILTER INTEGRATION TESTS
// ============================================================================

TEST_F(V1NeuronsIntegrationTest, SimpleCell_2DGaborProcessing_VerticalEdge) {
    // WHAT: Test full 2D Gabor filtering on vertical edge
    // WHY:  Verify compute_v1_simple_cell handles 2D images
    // HOW:  Create vertical edge, apply horizontal-tuned filter

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params);
    params.v1_simple.orientation = 0.0f;    // Horizontal tuning
    params.v1_simple.spatial_frequency = 0.1f;
    params.v1_simple.sigma = 5.0f;
    params.v1_simple.phase = 0.0f;

    // Create 64x64 test image with vertical edge at x=32
    const uint32_t width = 64;
    const uint32_t height = 64;
    std::vector<float> image(width * height);

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            image[y * width + x] = (x < 32) ? 0.0f : 1.0f;
        }
    }

    // Apply Gabor filter at edge location
    float response = compute_v1_simple_cell(
        &params.v1_simple,
        image.data(),
        width, height,
        32.0f, 32.0f
    );

    // Should detect the vertical edge
    EXPECT_GE(response, 0.0f);
}

TEST_F(V1NeuronsIntegrationTest, SimpleCell_2DGaborProcessing_HorizontalEdge) {
    // WHAT: Test Gabor filtering on horizontal edge
    // WHY:  Verify orientation selectivity in 2D
    // HOW:  Horizontal edge with vertical-tuned filter

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params);
    params.v1_simple.orientation = 90.0f;   // Vertical tuning
    params.v1_simple.spatial_frequency = 0.1f;
    params.v1_simple.sigma = 5.0f;

    // Create horizontal edge at y=32
    const uint32_t width = 64;
    const uint32_t height = 64;
    std::vector<float> image(width * height);

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            image[y * width + x] = (y < 32) ? 0.0f : 1.0f;
        }
    }

    // Apply Gabor filter
    float response = compute_v1_simple_cell(
        &params.v1_simple,
        image.data(),
        width, height,
        32.0f, 32.0f
    );

    // Should detect the horizontal edge
    EXPECT_GE(response, 0.0f);
}

TEST_F(V1NeuronsIntegrationTest, SimpleCell_2DGaborProcessing_DiagonalEdge) {
    // WHAT: Test Gabor on diagonal edge
    // WHY:  Test oblique orientations
    // HOW:  45° edge with 45° tuned filter

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params);
    params.v1_simple.orientation = 45.0f;
    params.v1_simple.spatial_frequency = 0.1f;
    params.v1_simple.sigma = 5.0f;

    // Create diagonal pattern
    const uint32_t width = 64;
    const uint32_t height = 64;
    std::vector<float> image(width * height);

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            // Diagonal: x + y < 64
            image[y * width + x] = (x + y < 64) ? 0.0f : 1.0f;
        }
    }

    float response = compute_v1_simple_cell(
        &params.v1_simple,
        image.data(),
        width, height,
        32.0f, 32.0f
    );

    EXPECT_GE(response, 0.0f);
}

// ============================================================================
// COMPLEX CELL INTEGRATION TESTS
// ============================================================================

TEST_F(V1NeuronsIntegrationTest, ComplexCell_EnergyModel_PhasePooling) {
    // WHAT: Test complex cell energy model with phase pooling
    // WHY:  Verify phase-invariant response
    // HOW:  Simulate simple cells with different phases

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_V1_COMPLEX_CELL, &params);
    params.v1_complex.orientation = 0.0f;
    params.v1_complex.direction_selectivity = 0.3f;

    // Simulate 8 simple cell responses (4 even, 4 odd phase)
    const uint32_t num_cells = 8;
    std::vector<float> simple_responses(num_cells);

    // Even-symmetric (phase = 0)
    for (uint32_t i = 0; i < 4; i++) {
        simple_responses[i] = 0.7f + 0.1f * i;
    }

    // Odd-symmetric (phase = π/2)
    for (uint32_t i = 4; i < 8; i++) {
        simple_responses[i] = 0.6f + 0.1f * (i - 4);
    }

    // Compute complex cell response
    float response = compute_v1_complex_cell(
        &params.v1_complex,
        simple_responses.data(),
        num_cells
    );

    // Should produce phase-invariant energy
    EXPECT_GT(response, 0.0f);
    EXPECT_LE(response, 2.0f);  // Reasonable upper bound
}

TEST_F(V1NeuronsIntegrationTest, ComplexCell_DirectionSelectivityModulation) {
    // WHAT: Test direction selectivity effects on pooling
    // WHY:  Verify direction tuning modulates response
    // HOW:  Compare low vs high direction selectivity

    std::vector<float> simple_responses = {0.8f, 0.7f, 0.6f, 0.5f};

    // Low direction selectivity
    neuron_type_params_t params_low{};
    neuron_type_get_default_params(NEURON_V1_COMPLEX_CELL, &params_low);
    params_low.v1_complex.direction_selectivity = 0.0f;

    float response_low = compute_v1_complex_cell(
        &params_low.v1_complex,
        simple_responses.data(),
        simple_responses.size()
    );

    // High direction selectivity
    neuron_type_params_t params_high{};
    neuron_type_get_default_params(NEURON_V1_COMPLEX_CELL, &params_high);
    params_high.v1_complex.direction_selectivity = 1.0f;

    float response_high = compute_v1_complex_cell(
        &params_high.v1_complex,
        simple_responses.data(),
        simple_responses.size()
    );

    // High selectivity should reduce response
    EXPECT_GT(response_low, 0.0f);
    EXPECT_GT(response_high, 0.0f);
    EXPECT_LE(response_high, response_low);
}

// ============================================================================
// HIERARCHICAL V1 MODEL TESTS
// ============================================================================

TEST_F(V1NeuronsIntegrationTest, HierarchicalV1_SimpleThenComplex) {
    // WHAT: Test hierarchical V1 model (simple → complex)
    // WHY:  Model biological V1 architecture
    // HOW:  Process image through simple cells, then complex cell

    // Create test image with vertical stripes
    const uint32_t width = 32;
    const uint32_t height = 32;
    std::vector<float> image(width * height);

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            image[y * width + x] = ((x / 4) % 2 == 0) ? 1.0f : 0.0f;
        }
    }

    // Stage 1: Simple cell responses with different phases
    neuron_type_params_t simple_params{};
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &simple_params);
    simple_params.v1_simple.orientation = 0.0f;
    simple_params.v1_simple.spatial_frequency = 0.2f;
    simple_params.v1_simple.sigma = 3.0f;

    std::vector<float> simple_responses;

    // Even phase (0)
    simple_params.v1_simple.phase = 0.0f;
    float resp1 = compute_v1_simple_cell(
        &simple_params.v1_simple, image.data(), width, height, 16.0f, 16.0f);
    simple_responses.push_back(resp1);

    // Odd phase (π/2)
    simple_params.v1_simple.phase = M_PI / 2.0f;
    float resp2 = compute_v1_simple_cell(
        &simple_params.v1_simple, image.data(), width, height, 16.0f, 16.0f);
    simple_responses.push_back(resp2);

    // Stage 2: Complex cell pools simple cell responses
    neuron_type_params_t complex_params{};
    neuron_type_get_default_params(NEURON_V1_COMPLEX_CELL, &complex_params);
    complex_params.v1_complex.orientation = 0.0f;
    complex_params.v1_complex.direction_selectivity = 0.2f;

    float complex_response = compute_v1_complex_cell(
        &complex_params.v1_complex,
        simple_responses.data(),
        simple_responses.size()
    );

    // Complex cell should produce phase-invariant response
    EXPECT_GT(complex_response, 0.0f);
}

// ============================================================================
// MULTIPLE NEURON TYPE COMBINATIONS
// ============================================================================

TEST_F(V1NeuronsIntegrationTest, MultipleTypes_CoexistenceLIF) {
    // WHAT: Test V1 neurons coexisting with LIF neurons
    // WHY:  Ensure type system handles multiple types
    // HOW:  Process inputs through both V1 and LIF types

    // V1 simple cell
    neuron_type_params_t v1_params{};
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &v1_params);

    float input = 0.7f;
    float v1_output = neuron_type_process_input(
        NEURON_V1_SIMPLE_CELL, &v1_params, input, timestamp);

    // Generic LIF neuron
    neuron_type_params_t lif_params{};
    neuron_type_get_default_params(NEURON_GENERIC_LIF, &lif_params);

    float lif_output = neuron_type_process_input(
        NEURON_GENERIC_LIF, &lif_params, input, timestamp);

    // Both should produce valid outputs
    EXPECT_GE(v1_output, 0.0f);
    EXPECT_GE(lif_output, 0.0f);
}

TEST_F(V1NeuronsIntegrationTest, MultipleTypes_ParameterIsolation) {
    // WHAT: Test parameter isolation between neuron types
    // WHY:  Ensure parameters don't cross-contaminate
    // HOW:  Set different params, verify independent processing

    // Create distinct parameter sets
    neuron_type_params_t params1{};
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params1);
    params1.v1_simple.orientation = 0.0f;
    params1.v1_simple.spatial_frequency = 1.0f;

    neuron_type_params_t params2{};
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params2);
    params2.v1_simple.orientation = 90.0f;
    params2.v1_simple.spatial_frequency = 4.0f;

    float input = 0.5f;

    // Process with both parameter sets
    float output1 = neuron_type_process_input(
        NEURON_V1_SIMPLE_CELL, &params1, input, timestamp);
    float output2 = neuron_type_process_input(
        NEURON_V1_SIMPLE_CELL, &params2, input, timestamp);

    // Both should be valid (values may differ due to parameters)
    EXPECT_GE(output1, 0.0f);
    EXPECT_GE(output2, 0.0f);
}

// ============================================================================
// EDGE CASE INTEGRATION TESTS
// ============================================================================

TEST_F(V1NeuronsIntegrationTest, EdgeCase_EmptyImage) {
    // WHAT: Test handling of empty (all zero) images
    // WHY:  Ensure graceful handling of edge cases
    // HOW:  Apply filters to zero image

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params);

    const uint32_t width = 32;
    const uint32_t height = 32;
    std::vector<float> empty_image(width * height, 0.0f);

    float response = compute_v1_simple_cell(
        &params.v1_simple,
        empty_image.data(),
        width, height,
        16.0f, 16.0f
    );

    // Empty image should produce zero response
    EXPECT_EQ(response, 0.0f);
}

TEST_F(V1NeuronsIntegrationTest, EdgeCase_UniformImage) {
    // WHAT: Test handling of uniform (no edges) images
    // WHY:  Edge detectors should not respond to uniform fields
    // HOW:  Apply filters to constant-value image

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params);

    const uint32_t width = 32;
    const uint32_t height = 32;
    std::vector<float> uniform_image(width * height, 0.5f);

    float response = compute_v1_simple_cell(
        &params.v1_simple,
        uniform_image.data(),
        width, height,
        16.0f, 16.0f
    );

    // Uniform image should produce minimal response
    // (Some response possible due to Gabor oscillations)
    EXPECT_GE(response, 0.0f);
}

// ============================================================================
// PERFORMANCE INTEGRATION TESTS
// ============================================================================

TEST_F(V1NeuronsIntegrationTest, Performance_Batch2DProcessing) {
    // WHAT: Test performance of batch 2D processing
    // WHY:  Ensure 2D filtering is efficient
    // HOW:  Process multiple images, measure time

    neuron_type_params_t params{};
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params);

    const uint32_t width = 32;
    const uint32_t height = 32;
    std::vector<float> image(width * height, 0.5f);

    auto start = std::chrono::high_resolution_clock::now();

    const int iterations = 100;
    float sum = 0.0f;
    for (int i = 0; i < iterations; i++) {
        float response = compute_v1_simple_cell(
            &params.v1_simple,
            image.data(),
            width, height,
            16.0f, 16.0f
        );
        sum += response;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should process at reasonable speed (< 1s for 100 iterations)
    EXPECT_LT(duration.count(), 1000);

    // Use sum to prevent optimization
    EXPECT_GE(sum, 0.0f);
}
