/**
 * @file test_e2e_ternary_cortical_processing.cpp
 * @brief End-to-end tests for cortical processing with ternary sparse coding
 *
 * WHAT: Full cortical column pipeline with ternary sparse coding
 * WHY:  Verify ternary sparse representations in sensory processing
 * HOW:  Test cortical column processing, sparsity targets
 *
 * TEST COVERAGE:
 * - Full cortical column pipeline with ternary sparse coding
 * - Sensory processing with ternary representations
 * - Sparsity target verification and maintenance
 * - Lateral inhibition with ternary states
 * - Hierarchical processing
 *
 * BIOLOGICAL BASIS:
 * - Cortical columns use sparse distributed representations
 * - Ternary states: excitatory (+1), inhibitory (-1), silent (0)
 * - Lateral inhibition enforces sparse activity
 * - Hierarchical feature extraction
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>

// Headers have their own extern "C" guards
#include "utils/ternary/nimcp_ternary.h"
#include "utils/ternary/nimcp_ternary_tensor.h"
#include "utils/ternary/nimcp_ternary_logic.h"
#include "utils/ternary/nimcp_ternary_convert.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TernaryCorticalProcessingE2ETest : public ::testing::Test {
protected:
    // Cortical column dimensions
    static constexpr size_t NUM_MINICOLUMNS = 32;
    static constexpr size_t CELLS_PER_MINICOLUMN = 16;
    static constexpr size_t NUM_LAYERS = 6;  // L1-L6 (L4 primary input)
    static constexpr size_t INPUT_SIZE = 64;
    static constexpr size_t NUM_COLUMNS = 4;  // Multiple columns in hierarchy

    // Sparsity targets
    static constexpr float TARGET_SPARSITY = 0.05f;  // 5% active cells
    static constexpr float SPARSITY_TOLERANCE = 0.02f;

    struct CorticalColumn {
        std::vector<trit_tensor_t*> layer_states;    // One tensor per layer
        std::vector<trit_matrix_t*> feedforward_weights;
        std::vector<trit_matrix_t*> lateral_weights;
        trit_matrix_t* feedback_weights;
        float inhibition_strength;
    };

    std::vector<CorticalColumn> columns;
    std::vector<float> sensory_input;
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(42);

        // Create cortical columns
        for (size_t c = 0; c < NUM_COLUMNS; c++) {
            CorticalColumn col;
            col.inhibition_strength = 0.8f;

            // Create layer states
            for (size_t l = 0; l < NUM_LAYERS; l++) {
                size_t layer_dims[] = {NUM_MINICOLUMNS, CELLS_PER_MINICOLUMN};
                trit_tensor_t* layer = trit_tensor_create(
                    layer_dims, 2, TERNARY_PACK_2BIT);
                ASSERT_NE(layer, nullptr);
                col.layer_states.push_back(layer);
            }

            // Create feedforward weights (for L4 input)
            size_t input_dim = (c == 0) ? INPUT_SIZE : NUM_MINICOLUMNS * CELLS_PER_MINICOLUMN;
            trit_matrix_t* ff = trit_matrix_create(
                input_dim, NUM_MINICOLUMNS * CELLS_PER_MINICOLUMN,
                TERNARY_PACK_BASE243);
            ASSERT_NE(ff, nullptr);
            InitializeSparseWeights(ff, 0.1f);  // Sparse connectivity
            col.feedforward_weights.push_back(ff);

            // Create lateral weights (within L4)
            trit_matrix_t* lat = trit_matrix_create(
                NUM_MINICOLUMNS * CELLS_PER_MINICOLUMN,
                NUM_MINICOLUMNS * CELLS_PER_MINICOLUMN,
                TERNARY_PACK_2BIT);
            ASSERT_NE(lat, nullptr);
            InitializeLateralInhibition(lat);
            col.lateral_weights.push_back(lat);

            // Create feedback weights
            col.feedback_weights = trit_matrix_create(
                NUM_MINICOLUMNS * CELLS_PER_MINICOLUMN,
                NUM_MINICOLUMNS * CELLS_PER_MINICOLUMN,
                TERNARY_PACK_2BIT);
            ASSERT_NE(col.feedback_weights, nullptr);
            InitializeSparseWeights(col.feedback_weights, 0.05f);

            columns.push_back(col);
        }

        // Generate sensory input
        sensory_input.resize(INPUT_SIZE);
        GenerateSensoryInput();
    }

    void TearDown() override {
        for (auto& col : columns) {
            for (auto* layer : col.layer_states) {
                if (layer) trit_tensor_destroy(layer);
            }
            for (auto* ff : col.feedforward_weights) {
                if (ff) trit_matrix_destroy(ff);
            }
            for (auto* lat : col.lateral_weights) {
                if (lat) trit_matrix_destroy(lat);
            }
            if (col.feedback_weights) trit_matrix_destroy(col.feedback_weights);
        }
    }

    void InitializeSparseWeights(trit_matrix_t* matrix, float connectivity) {
        std::bernoulli_distribution connected(connectivity);
        std::discrete_distribution<int> value_dist({0.4, 0.2, 0.4});  // -1, 0, +1

        for (size_t i = 0; i < matrix->rows; i++) {
            for (size_t j = 0; j < matrix->cols; j++) {
                if (connected(rng)) {
                    int val = value_dist(rng) - 1;  // Maps to -1, 0, +1
                    trit_matrix_set(matrix, i, j, (trit_t)val);
                } else {
                    trit_matrix_set(matrix, i, j, TRIT_UNKNOWN);
                }
            }
        }
    }

    void InitializeLateralInhibition(trit_matrix_t* matrix) {
        // Lateral inhibition: mostly inhibitory (-1) connections
        // Cells within same minicolumn cooperate, between minicolumns compete
        for (size_t i = 0; i < matrix->rows; i++) {
            size_t mc_i = i / CELLS_PER_MINICOLUMN;
            for (size_t j = 0; j < matrix->cols; j++) {
                size_t mc_j = j / CELLS_PER_MINICOLUMN;

                if (i == j) {
                    // No self-connection
                    trit_matrix_set(matrix, i, j, TRIT_UNKNOWN);
                } else if (mc_i == mc_j) {
                    // Within minicolumn: weak excitation or no connection
                    trit_matrix_set(matrix, i, j, TRIT_UNKNOWN);
                } else {
                    // Between minicolumns: inhibition
                    trit_matrix_set(matrix, i, j, TRIT_NEGATIVE);
                }
            }
        }
    }

    void GenerateSensoryInput() {
        // Create a structured sensory input (like edge detection)
        for (size_t i = 0; i < INPUT_SIZE; i++) {
            float x = (float)i / INPUT_SIZE;
            sensory_input[i] = sinf(x * 4.0f * M_PI) * 0.5f + 0.5f;
        }
    }

    // Convert float input to ternary
    trit_vector_t* FloatToTernary(const std::vector<float>& input, float threshold) {
        trit_vector_t* vec = trit_vector_create(input.size(), TERNARY_PACK_2BIT);
        if (!vec) return nullptr;

        for (size_t i = 0; i < input.size(); i++) {
            trit_t val = trit_from_float_threshold(input[i] - 0.5f, threshold);
            trit_vector_set(vec, i, val);
        }
        return vec;
    }

    // Compute feedforward activation
    void FeedforwardActivation(const trit_matrix_t* weights,
                                const trit_vector_t* input,
                                std::vector<float>& output) {
        output.resize(weights->cols, 0.0f);

        for (size_t j = 0; j < weights->cols; j++) {
            float sum = 0.0f;
            for (size_t i = 0; i < weights->rows && i < input->length; i++) {
                trit_t w = trit_matrix_get(weights, i, j);
                trit_t x = trit_vector_get(input, i);
                sum += (float)w * (float)x;
            }
            output[j] = sum;
        }
    }

    // Apply lateral inhibition
    void ApplyLateralInhibition(const trit_matrix_t* lateral,
                                 std::vector<float>& activations,
                                 float strength) {
        std::vector<float> inhibition(activations.size(), 0.0f);

        for (size_t i = 0; i < activations.size(); i++) {
            for (size_t j = 0; j < activations.size(); j++) {
                trit_t w = trit_matrix_get(lateral, j, i);
                if (w == TRIT_NEGATIVE) {
                    inhibition[i] += activations[j] * strength;
                }
            }
        }

        for (size_t i = 0; i < activations.size(); i++) {
            activations[i] = std::max(0.0f, activations[i] - inhibition[i]);
        }
    }

    // Apply winner-take-all within minicolumns
    void WinnerTakeAllSparsify(std::vector<float>& activations,
                                trit_tensor_t* output,
                                float target_sparsity) {
        size_t total_cells = NUM_MINICOLUMNS * CELLS_PER_MINICOLUMN;
        size_t target_active = (size_t)(total_cells * target_sparsity);

        // Ensure at least 1 active cell to prevent 100% sparsity
        if (target_active < 1) target_active = 1;

        // Create index-sorted pairs for top-k selection
        std::vector<std::pair<float, size_t>> indexed_acts(activations.size());
        for (size_t i = 0; i < activations.size(); i++) {
            indexed_acts[i] = {activations[i], i};
        }

        // Sort by activation (descending) - stable to preserve order for ties
        std::sort(indexed_acts.begin(), indexed_acts.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });

        // Mark top-k indices as active
        std::vector<trit_t> states(total_cells, TRIT_UNKNOWN);
        for (size_t k = 0; k < target_active && k < indexed_acts.size(); k++) {
            size_t idx = indexed_acts[k].second;
            states[idx] = TRIT_POSITIVE;  // Active
        }

        // Optionally mark strongly negative activations as inhibited
        // Use the threshold from the k-th element
        float threshold = (target_active < indexed_acts.size()) ?
            indexed_acts[target_active].first : 0.0f;
        for (size_t i = 0; i < activations.size(); i++) {
            if (states[i] == TRIT_UNKNOWN && activations[i] < -std::abs(threshold)) {
                states[i] = TRIT_NEGATIVE;  // Inhibited
            }
        }

        // Write to output tensor
        size_t idx = 0;
        for (size_t mc = 0; mc < NUM_MINICOLUMNS; mc++) {
            for (size_t c = 0; c < CELLS_PER_MINICOLUMN; c++) {
                size_t coords[] = {mc, c};
                trit_tensor_set_element(output, coords, states[idx]);
                idx++;
            }
        }
    }

    // Compute sparsity of layer state
    float ComputeSparsity(trit_tensor_t* state) {
        size_t total = NUM_MINICOLUMNS * CELLS_PER_MINICOLUMN;
        size_t silent = 0;

        for (size_t mc = 0; mc < NUM_MINICOLUMNS; mc++) {
            for (size_t c = 0; c < CELLS_PER_MINICOLUMN; c++) {
                size_t coords[] = {mc, c};
                if (trit_tensor_get_element(state, coords) == TRIT_UNKNOWN) {
                    silent++;
                }
            }
        }

        return (float)silent / total;
    }
};

//=============================================================================
// E2E Test: Full Cortical Column Processing
//=============================================================================

TEST_F(TernaryCorticalProcessingE2ETest, FullCorticalColumnProcessing) {
    CorticalColumn& col = columns[0];

    // Convert sensory input to ternary
    trit_vector_t* ternary_input = FloatToTernary(sensory_input, 0.3f);
    ASSERT_NE(ternary_input, nullptr);

    // Feedforward to L4
    std::vector<float> l4_activations;
    FeedforwardActivation(col.feedforward_weights[0], ternary_input, l4_activations);

    // Apply lateral inhibition
    ApplyLateralInhibition(col.lateral_weights[0], l4_activations, col.inhibition_strength);

    // Winner-take-all sparsification
    WinnerTakeAllSparsify(l4_activations, col.layer_states[3], TARGET_SPARSITY);  // L4 is index 3

    // Verify sparsity target
    float sparsity = ComputeSparsity(col.layer_states[3]);
    float active_ratio = 1.0f - sparsity;

    // Active ratio should be close to target
    EXPECT_NEAR(active_ratio, TARGET_SPARSITY, SPARSITY_TOLERANCE * 2)
        << "Sparsity: " << sparsity << ", Active: " << active_ratio;

    // Verify non-trivial activity
    size_t active_count = 0;
    for (size_t mc = 0; mc < NUM_MINICOLUMNS; mc++) {
        for (size_t c = 0; c < CELLS_PER_MINICOLUMN; c++) {
            size_t coords[] = {mc, c};
            trit_t state = trit_tensor_get_element(col.layer_states[3], coords);
            if (state != TRIT_UNKNOWN) active_count++;
        }
    }
    EXPECT_GT(active_count, 0u) << "No active cells in L4";

    trit_vector_destroy(ternary_input);
}

//=============================================================================
// E2E Test: Hierarchical Processing Pipeline
//=============================================================================

TEST_F(TernaryCorticalProcessingE2ETest, HierarchicalProcessingPipeline) {
    // Process through multiple cortical columns in hierarchy
    trit_vector_t* current_input = FloatToTernary(sensory_input, 0.3f);
    ASSERT_NE(current_input, nullptr);

    std::vector<float> sparsities;

    for (size_t col_idx = 0; col_idx < NUM_COLUMNS; col_idx++) {
        CorticalColumn& col = columns[col_idx];

        // Feedforward activation
        std::vector<float> activations;
        FeedforwardActivation(col.feedforward_weights[0], current_input, activations);

        // Lateral inhibition
        ApplyLateralInhibition(col.lateral_weights[0], activations, col.inhibition_strength);

        // Sparsify to L4
        WinnerTakeAllSparsify(activations, col.layer_states[3], TARGET_SPARSITY);

        float sparsity = ComputeSparsity(col.layer_states[3]);
        sparsities.push_back(sparsity);

        // Prepare input for next column (convert L4 state to vector)
        trit_vector_destroy(current_input);

        size_t output_size = NUM_MINICOLUMNS * CELLS_PER_MINICOLUMN;
        current_input = trit_vector_create(output_size, TERNARY_PACK_2BIT);
        ASSERT_NE(current_input, nullptr);

        size_t idx = 0;
        for (size_t mc = 0; mc < NUM_MINICOLUMNS; mc++) {
            for (size_t c = 0; c < CELLS_PER_MINICOLUMN; c++) {
                size_t coords[] = {mc, c};
                trit_t state = trit_tensor_get_element(col.layer_states[3], coords);
                trit_vector_set(current_input, idx++, state);
            }
        }
    }

    trit_vector_destroy(current_input);

    // Verify all columns maintain sparsity
    for (size_t i = 0; i < sparsities.size(); i++) {
        float active_ratio = 1.0f - sparsities[i];
        EXPECT_NEAR(active_ratio, TARGET_SPARSITY, SPARSITY_TOLERANCE * 3)
            << "Column " << i << " failed sparsity target";
    }
}

//=============================================================================
// E2E Test: Sparsity Maintenance Under Varying Input
//=============================================================================

TEST_F(TernaryCorticalProcessingE2ETest, SparsityMaintenanceUnderVaryingInput) {
    CorticalColumn& col = columns[0];

    std::vector<float> all_sparsities;

    // Test with different input patterns
    for (int pattern = 0; pattern < 10; pattern++) {
        // Generate different input patterns
        std::vector<float> input(INPUT_SIZE);
        for (size_t i = 0; i < INPUT_SIZE; i++) {
            float x = (float)i / INPUT_SIZE;
            float freq = 1.0f + pattern * 0.5f;
            input[i] = sinf(freq * x * 2.0f * M_PI) * 0.5f + 0.5f;
        }

        trit_vector_t* ternary_input = FloatToTernary(input, 0.3f);
        ASSERT_NE(ternary_input, nullptr);

        std::vector<float> activations;
        FeedforwardActivation(col.feedforward_weights[0], ternary_input, activations);
        ApplyLateralInhibition(col.lateral_weights[0], activations, col.inhibition_strength);
        WinnerTakeAllSparsify(activations, col.layer_states[3], TARGET_SPARSITY);

        float sparsity = ComputeSparsity(col.layer_states[3]);
        all_sparsities.push_back(sparsity);

        trit_vector_destroy(ternary_input);
    }

    // Verify sparsity is consistent across patterns
    float mean_sparsity = 0.0f;
    for (float s : all_sparsities) mean_sparsity += s;
    mean_sparsity /= all_sparsities.size();

    float variance = 0.0f;
    for (float s : all_sparsities) {
        float diff = s - mean_sparsity;
        variance += diff * diff;
    }
    variance /= all_sparsities.size();

    // Sparsity should be consistent (low variance)
    EXPECT_LT(variance, 0.01f) << "Sparsity inconsistent across patterns";

    // Mean active ratio should be close to target
    float mean_active = 1.0f - mean_sparsity;
    EXPECT_NEAR(mean_active, TARGET_SPARSITY, SPARSITY_TOLERANCE * 2);
}

//=============================================================================
// E2E Test: Lateral Inhibition Effectiveness
//=============================================================================

TEST_F(TernaryCorticalProcessingE2ETest, LateralInhibitionEffectiveness) {
    CorticalColumn& col = columns[0];

    // Test with uniform high input (all cells should try to fire)
    std::vector<float> high_input(INPUT_SIZE, 1.0f);
    trit_vector_t* ternary_input = FloatToTernary(high_input, 0.1f);
    ASSERT_NE(ternary_input, nullptr);

    // Get activations without inhibition
    std::vector<float> activations_no_inhibition;
    FeedforwardActivation(col.feedforward_weights[0], ternary_input, activations_no_inhibition);

    size_t active_no_inhibition = 0;
    for (float a : activations_no_inhibition) {
        if (a > 0.0f) active_no_inhibition++;
    }

    // Get activations with inhibition
    std::vector<float> activations_with_inhibition = activations_no_inhibition;
    ApplyLateralInhibition(col.lateral_weights[0], activations_with_inhibition,
                           col.inhibition_strength);

    size_t active_with_inhibition = 0;
    for (float a : activations_with_inhibition) {
        if (a > 0.0f) active_with_inhibition++;
    }

    // Inhibition should reduce number of active cells
    EXPECT_LT(active_with_inhibition, active_no_inhibition)
        << "Lateral inhibition did not reduce activity";

    // Apply WTA and verify sparsity
    WinnerTakeAllSparsify(activations_with_inhibition, col.layer_states[3], TARGET_SPARSITY);
    float sparsity = ComputeSparsity(col.layer_states[3]);

    EXPECT_GT(sparsity, 0.9f) << "Expected sparse activity after inhibition";

    trit_vector_destroy(ternary_input);
}

//=============================================================================
// E2E Test: Pattern Discrimination
//=============================================================================

TEST_F(TernaryCorticalProcessingE2ETest, PatternDiscrimination) {
    CorticalColumn& col = columns[0];

    // Create two distinct patterns
    std::vector<float> pattern_A(INPUT_SIZE);
    std::vector<float> pattern_B(INPUT_SIZE);

    for (size_t i = 0; i < INPUT_SIZE; i++) {
        pattern_A[i] = (i % 4 < 2) ? 1.0f : 0.0f;  // Stripes
        pattern_B[i] = (i < INPUT_SIZE / 2) ? 1.0f : 0.0f;  // Half-field
    }

    // Process pattern A
    trit_vector_t* input_A = FloatToTernary(pattern_A, 0.3f);
    std::vector<float> act_A;
    FeedforwardActivation(col.feedforward_weights[0], input_A, act_A);
    ApplyLateralInhibition(col.lateral_weights[0], act_A, col.inhibition_strength);
    WinnerTakeAllSparsify(act_A, col.layer_states[3], TARGET_SPARSITY);

    // Copy state A
    std::vector<trit_t> state_A(NUM_MINICOLUMNS * CELLS_PER_MINICOLUMN);
    size_t idx = 0;
    for (size_t mc = 0; mc < NUM_MINICOLUMNS; mc++) {
        for (size_t c = 0; c < CELLS_PER_MINICOLUMN; c++) {
            size_t coords[] = {mc, c};
            state_A[idx++] = trit_tensor_get_element(col.layer_states[3], coords);
        }
    }

    // Process pattern B
    trit_vector_t* input_B = FloatToTernary(pattern_B, 0.3f);
    std::vector<float> act_B;
    FeedforwardActivation(col.feedforward_weights[0], input_B, act_B);
    ApplyLateralInhibition(col.lateral_weights[0], act_B, col.inhibition_strength);
    WinnerTakeAllSparsify(act_B, col.layer_states[3], TARGET_SPARSITY);

    // Compare states
    size_t matching = 0, total = 0;
    idx = 0;
    for (size_t mc = 0; mc < NUM_MINICOLUMNS; mc++) {
        for (size_t c = 0; c < CELLS_PER_MINICOLUMN; c++) {
            size_t coords[] = {mc, c};
            trit_t state_b = trit_tensor_get_element(col.layer_states[3], coords);
            if (state_A[idx] == state_b) matching++;
            total++;
            idx++;
        }
    }

    float overlap = (float)matching / total;

    // Patterns should produce different representations
    // With sparse coding, expect high overlap of silent cells, but different active cells
    EXPECT_LT(overlap, 0.99f) << "Patterns produced identical representations";

    trit_vector_destroy(input_A);
    trit_vector_destroy(input_B);
}

//=============================================================================
// E2E Test: Ternary Logic Integration
//=============================================================================

TEST_F(TernaryCorticalProcessingE2ETest, TernaryLogicIntegration) {
    CorticalColumn& col = columns[0];

    // Process input
    trit_vector_t* input = FloatToTernary(sensory_input, 0.3f);
    std::vector<float> activations;
    FeedforwardActivation(col.feedforward_weights[0], input, activations);
    ApplyLateralInhibition(col.lateral_weights[0], activations, col.inhibition_strength);
    WinnerTakeAllSparsify(activations, col.layer_states[3], TARGET_SPARSITY);

    // Extract column states for ternary logic operations
    std::vector<trit_t> column_votes(NUM_MINICOLUMNS);
    for (size_t mc = 0; mc < NUM_MINICOLUMNS; mc++) {
        // Majority vote within minicolumn
        int sum = 0;
        for (size_t c = 0; c < CELLS_PER_MINICOLUMN; c++) {
            size_t coords[] = {mc, c};
            sum += trit_tensor_get_element(col.layer_states[3], coords);
        }
        if (sum > 0) column_votes[mc] = TRIT_POSITIVE;
        else if (sum < 0) column_votes[mc] = TRIT_NEGATIVE;
        else column_votes[mc] = TRIT_UNKNOWN;
    }

    // Apply ternary logic: majority vote across minicolumns
    trit_t global_decision = trit_majority(column_votes.data(), column_votes.size());

    // Verify decision is valid ternary value
    EXPECT_TRUE(global_decision == TRIT_POSITIVE ||
                global_decision == TRIT_NEGATIVE ||
                global_decision == TRIT_UNKNOWN);

    // Test ternary quantifier operations
    trit_t any_active = trit_any(column_votes.data(), column_votes.size());
    trit_t all_active = trit_all(column_votes.data(), column_votes.size());
    trit_t unanimous = trit_unanimous(column_votes.data(), column_votes.size());

    // With sparse coding, not all should be active
    EXPECT_NE(all_active, TRIT_POSITIVE) << "All-active unexpected with sparse coding";

    // But some should be active
    EXPECT_EQ(any_active, TRIT_POSITIVE) << "No active minicolumns";

    trit_vector_destroy(input);
}

//=============================================================================
// E2E Test: Long-Running Stability
//=============================================================================

TEST_F(TernaryCorticalProcessingE2ETest, LongRunningStability) {
    CorticalColumn& col = columns[0];

    constexpr size_t NUM_ITERATIONS = 1000;
    size_t valid_outputs = 0;
    float min_sparsity = 1.0f, max_sparsity = 0.0f;

    for (size_t iter = 0; iter < NUM_ITERATIONS; iter++) {
        // Generate varying input
        std::vector<float> input(INPUT_SIZE);
        for (size_t i = 0; i < INPUT_SIZE; i++) {
            float phase = (float)iter * 0.1f;
            input[i] = sinf(2.0f * M_PI * i / INPUT_SIZE + phase) * 0.5f + 0.5f;
        }

        trit_vector_t* ternary_input = FloatToTernary(input, 0.3f);
        if (!ternary_input) continue;

        std::vector<float> activations;
        FeedforwardActivation(col.feedforward_weights[0], ternary_input, activations);
        ApplyLateralInhibition(col.lateral_weights[0], activations, col.inhibition_strength);
        WinnerTakeAllSparsify(activations, col.layer_states[3], TARGET_SPARSITY);

        // Check validity
        bool valid = true;
        for (size_t mc = 0; mc < NUM_MINICOLUMNS && valid; mc++) {
            for (size_t c = 0; c < CELLS_PER_MINICOLUMN && valid; c++) {
                size_t coords[] = {mc, c};
                trit_t state = trit_tensor_get_element(col.layer_states[3], coords);
                if (state != TRIT_POSITIVE && state != TRIT_NEGATIVE &&
                    state != TRIT_UNKNOWN) {
                    valid = false;
                }
            }
        }

        if (valid) {
            valid_outputs++;
            float sparsity = ComputeSparsity(col.layer_states[3]);
            min_sparsity = std::min(min_sparsity, sparsity);
            max_sparsity = std::max(max_sparsity, sparsity);
        }

        trit_vector_destroy(ternary_input);
    }

    EXPECT_EQ(valid_outputs, NUM_ITERATIONS) << "Invalid outputs during long run";

    // Sparsity should remain in reasonable range
    EXPECT_GT(min_sparsity, 0.8f) << "Sparsity dropped too low";
    EXPECT_LT(max_sparsity, 0.99f) << "Sparsity too high (no activity)";
}

//=============================================================================
// E2E Test: Memory Efficiency
//=============================================================================

TEST_F(TernaryCorticalProcessingE2ETest, MemoryEfficiency) {
    CorticalColumn& col = columns[0];

    // Calculate ternary memory usage
    size_t ternary_bytes = 0;
    for (auto* layer : col.layer_states) {
        ternary_bytes += trit_tensor_memory_size(layer);
    }
    for (auto* ff : col.feedforward_weights) {
        ternary_bytes += trit_matrix_memory_size(ff);
    }
    for (auto* lat : col.lateral_weights) {
        ternary_bytes += trit_matrix_memory_size(lat);
    }
    ternary_bytes += trit_matrix_memory_size(col.feedback_weights);

    // Calculate float equivalent
    size_t layer_elements = NUM_LAYERS * NUM_MINICOLUMNS * CELLS_PER_MINICOLUMN;
    size_t ff_elements = INPUT_SIZE * NUM_MINICOLUMNS * CELLS_PER_MINICOLUMN;
    size_t lat_elements = NUM_MINICOLUMNS * CELLS_PER_MINICOLUMN *
                          NUM_MINICOLUMNS * CELLS_PER_MINICOLUMN;
    size_t float_bytes = (layer_elements + ff_elements + lat_elements) * sizeof(float);

    // Ternary should use less memory
    float compression = (float)float_bytes / (float)ternary_bytes;
    EXPECT_GT(compression, 2.0f) << "Expected at least 2x compression";

    SUCCEED() << "Compression ratio: " << compression << "x ("
              << float_bytes << " float bytes vs " << ternary_bytes << " ternary bytes)";
}

//=============================================================================
// E2E Test: Performance Benchmark
//=============================================================================

TEST_F(TernaryCorticalProcessingE2ETest, PerformanceBenchmark) {
    CorticalColumn& col = columns[0];
    constexpr size_t BENCHMARK_ITERATIONS = 500;

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t iter = 0; iter < BENCHMARK_ITERATIONS; iter++) {
        trit_vector_t* input = FloatToTernary(sensory_input, 0.3f);
        if (!input) continue;

        std::vector<float> activations;
        FeedforwardActivation(col.feedforward_weights[0], input, activations);
        ApplyLateralInhibition(col.lateral_weights[0], activations, col.inhibition_strength);
        WinnerTakeAllSparsify(activations, col.layer_states[3], TARGET_SPARSITY);

        trit_vector_destroy(input);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    float iterations_per_second = (float)BENCHMARK_ITERATIONS * 1000.0f / duration.count();

    // Performance threshold: 25 iterations/second is reasonable for E2E tests
    // which may run on various hardware configurations (CI, VMs, etc.)
    // The threshold accounts for overhead from robust top-k sparsification
    EXPECT_GT(iterations_per_second, 25.0f)
        << "Performance: " << iterations_per_second << " iterations/second";

    SUCCEED() << "Cortical processing: " << iterations_per_second << " iterations/second";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
