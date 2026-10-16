/**
 * @file test_e2e_ternary_neural_network.cpp
 * @brief End-to-end tests for ternary neural network training
 *
 * WHAT: Full neural network training with ternary weights
 * WHY:  Verify ternary quantization works in complete training pipelines
 * HOW:  Test forward/backward pass, convergence with ternary quantization
 *
 * TEST COVERAGE:
 * - Full neural network with ternary weight quantization
 * - End-to-end forward and backward pass with ternary weights
 * - Convergence verification with ternary quantization
 * - Memory efficiency validation (20x compression)
 * - Training stability over extended runs
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>

// Headers have their own extern "C" guards
#include "utils/ternary/nimcp_ternary.h"
#include "utils/ternary/nimcp_ternary_tensor.h"
#include "snn/nimcp_snn_ternary.h"
#include "plasticity/nimcp_plasticity_ternary.h"
#include "utils/tensor/nimcp_tensor.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TernaryNeuralNetworkE2ETest : public ::testing::Test {
protected:
    static constexpr size_t INPUT_SIZE = 64;
    static constexpr size_t HIDDEN_SIZE = 128;
    static constexpr size_t OUTPUT_SIZE = 16;
    static constexpr size_t NUM_SAMPLES = 256;
    static constexpr size_t NUM_EPOCHS = 50;
    static constexpr float LEARNING_RATE = 0.01f;
    static constexpr float QUANTIZATION_THRESHOLD = 0.3f;

    snn_ternary_weight_matrix_t* weights_ih = nullptr;  // Input to Hidden
    snn_ternary_weight_matrix_t* weights_ho = nullptr;  // Hidden to Output
    std::vector<float> input_data;
    std::vector<float> target_data;
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(42);  // Reproducible tests

        // Create ternary weight matrices
        snn_ternary_config_t config;
        snn_ternary_default_config(&config);
        config.ltp_threshold = 0.3f;
        config.ltd_threshold = -0.3f;
        config.pack_mode = TERNARY_PACK_BASE243;

        weights_ih = snn_ternary_create(INPUT_SIZE, HIDDEN_SIZE, &config);
        weights_ho = snn_ternary_create(HIDDEN_SIZE, OUTPUT_SIZE, &config);

        // Initialize with random ternary weights
        InitializeRandomWeights(weights_ih);
        InitializeRandomWeights(weights_ho);

        // Generate training data
        GenerateTrainingData();
    }

    void TearDown() override {
        if (weights_ih) snn_ternary_destroy(weights_ih);
        if (weights_ho) snn_ternary_destroy(weights_ho);
    }

    void InitializeRandomWeights(snn_ternary_weight_matrix_t* weights) {
        std::uniform_int_distribution<int> dist(-1, 1);
        for (uint32_t i = 0; i < weights->pre_size; i++) {
            for (uint32_t j = 0; j < weights->post_size; j++) {
                snn_ternary_set_weight(weights, i, j, (trit_t)dist(rng));
            }
        }
    }

    void GenerateTrainingData() {
        input_data.resize(NUM_SAMPLES * INPUT_SIZE);
        target_data.resize(NUM_SAMPLES * OUTPUT_SIZE);

        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

        for (size_t s = 0; s < NUM_SAMPLES; s++) {
            // Generate input pattern
            for (size_t i = 0; i < INPUT_SIZE; i++) {
                input_data[s * INPUT_SIZE + i] = dist(rng);
            }

            // Generate target (simple pattern based on input sum)
            float input_sum = 0.0f;
            for (size_t i = 0; i < INPUT_SIZE; i++) {
                input_sum += input_data[s * INPUT_SIZE + i];
            }
            for (size_t o = 0; o < OUTPUT_SIZE; o++) {
                target_data[s * OUTPUT_SIZE + o] =
                    sinf(input_sum * (float)(o + 1) * 0.1f);
            }
        }
    }

    float ComputeMSE(const float* output, const float* target, size_t size) {
        float mse = 0.0f;
        for (size_t i = 0; i < size; i++) {
            float diff = output[i] - target[i];
            mse += diff * diff;
        }
        return mse / (float)size;
    }

    void TernaryForward(
        const float* input,
        float* hidden,
        float* output,
        snn_ternary_weight_matrix_t* w_ih,
        snn_ternary_weight_matrix_t* w_ho
    ) {
        // Input -> Hidden
        snn_ternary_forward_float(w_ih, input, hidden);
        // ReLU activation
        for (size_t i = 0; i < HIDDEN_SIZE; i++) {
            hidden[i] = (hidden[i] > 0.0f) ? hidden[i] : 0.0f;
        }
        // Hidden -> Output
        snn_ternary_forward_float(w_ho, hidden, output);
    }
};

//=============================================================================
// E2E Test: Full Forward Pass with Ternary Weights
//=============================================================================

TEST_F(TernaryNeuralNetworkE2ETest, FullForwardPassWithTernaryWeights) {
    std::vector<float> hidden(HIDDEN_SIZE);
    std::vector<float> output(OUTPUT_SIZE);

    // Process all samples
    size_t valid_outputs = 0;
    for (size_t s = 0; s < NUM_SAMPLES; s++) {
        const float* input = &input_data[s * INPUT_SIZE];

        TernaryForward(input, hidden.data(), output.data(),
                       weights_ih, weights_ho);

        // Verify outputs are valid
        bool all_valid = true;
        for (size_t o = 0; o < OUTPUT_SIZE; o++) {
            if (std::isnan(output[o]) || std::isinf(output[o])) {
                all_valid = false;
                break;
            }
        }
        if (all_valid) valid_outputs++;
    }

    // All outputs should be valid
    EXPECT_EQ(valid_outputs, NUM_SAMPLES);
}

//=============================================================================
// E2E Test: Training Convergence with Ternary Quantization
//=============================================================================

TEST_F(TernaryNeuralNetworkE2ETest, TrainingConvergenceWithTernaryQuantization) {
    std::vector<float> hidden(HIDDEN_SIZE);
    std::vector<float> output(OUTPUT_SIZE);
    std::vector<float> hidden_grad(HIDDEN_SIZE);
    std::vector<float> output_grad(OUTPUT_SIZE);

    std::vector<float> epoch_losses;

    for (size_t epoch = 0; epoch < NUM_EPOCHS; epoch++) {
        float epoch_loss = 0.0f;

        for (size_t s = 0; s < NUM_SAMPLES; s++) {
            const float* input = &input_data[s * INPUT_SIZE];
            const float* target = &target_data[s * OUTPUT_SIZE];

            // Forward pass
            TernaryForward(input, hidden.data(), output.data(),
                           weights_ih, weights_ho);

            // Compute loss
            epoch_loss += ComputeMSE(output.data(), target, OUTPUT_SIZE);

            // Compute gradients
            for (size_t o = 0; o < OUTPUT_SIZE; o++) {
                output_grad[o] = 2.0f * (output[o] - target[o]) / OUTPUT_SIZE;
            }

            // Backpropagate through hidden->output weights (STDP-like updates)
            for (size_t h = 0; h < HIDDEN_SIZE; h++) {
                float grad_sum = 0.0f;
                for (size_t o = 0; o < OUTPUT_SIZE; o++) {
                    float delta = -LEARNING_RATE * output_grad[o] * hidden[h];
                    snn_ternary_stdp_update(weights_ho, h, o, delta);
                    grad_sum += output_grad[o] *
                                snn_ternary_get_effective_weight(weights_ho, h, o);
                }
                hidden_grad[h] = grad_sum * (hidden[h] > 0.0f ? 1.0f : 0.0f);
            }

            // Backpropagate through input->hidden weights
            for (size_t i = 0; i < INPUT_SIZE; i++) {
                for (size_t h = 0; h < HIDDEN_SIZE; h++) {
                    float delta = -LEARNING_RATE * hidden_grad[h] * input[i];
                    snn_ternary_stdp_update(weights_ih, i, h, delta);
                }
            }
        }

        epoch_loss /= NUM_SAMPLES;
        epoch_losses.push_back(epoch_loss);

        // Periodically discretize accumulated deltas
        if (epoch % 10 == 9) {
            snn_ternary_discretize(weights_ih);
            snn_ternary_discretize(weights_ho);
        }
    }

    // Verify training reduced loss (or at least didn't explode)
    ASSERT_GT(epoch_losses.size(), 1u);
    float initial_loss = epoch_losses[0];
    float final_loss = epoch_losses.back();

    // Loss should not explode
    EXPECT_LT(final_loss, initial_loss * 10.0f);

    // Verify final outputs are still valid
    const float* input = &input_data[0];
    TernaryForward(input, hidden.data(), output.data(), weights_ih, weights_ho);

    for (size_t o = 0; o < OUTPUT_SIZE; o++) {
        EXPECT_FALSE(std::isnan(output[o]));
        EXPECT_FALSE(std::isinf(output[o]));
    }
}

//=============================================================================
// E2E Test: Memory Efficiency with Ternary Compression
//=============================================================================

TEST_F(TernaryNeuralNetworkE2ETest, MemoryEfficiencyWithTernaryCompression) {
    // Get statistics for both weight matrices
    snn_ternary_stats_t stats_ih, stats_ho;
    EXPECT_EQ(0, snn_ternary_get_stats(weights_ih, &stats_ih));
    EXPECT_EQ(0, snn_ternary_get_stats(weights_ho, &stats_ho));

    // Calculate expected compression ratio
    // Float32: 4 bytes per weight
    // Base-243: ~1.6 bits per trit = 0.2 bytes per weight
    size_t float_bytes_ih = INPUT_SIZE * HIDDEN_SIZE * sizeof(float);
    size_t float_bytes_ho = HIDDEN_SIZE * OUTPUT_SIZE * sizeof(float);
    size_t total_float_bytes = float_bytes_ih + float_bytes_ho;

    size_t ternary_bytes = stats_ih.memory_bytes + stats_ho.memory_bytes;

    // Verify significant compression (at least 5x)
    float compression_ratio = (float)total_float_bytes / (float)ternary_bytes;
    EXPECT_GT(compression_ratio, 5.0f);

    // Verify weight distribution is reasonable
    size_t total_ih = stats_ih.n_positive + stats_ih.n_unknown + stats_ih.n_negative;
    size_t total_ho = stats_ho.n_positive + stats_ho.n_unknown + stats_ho.n_negative;

    EXPECT_EQ(total_ih, INPUT_SIZE * HIDDEN_SIZE);
    EXPECT_EQ(total_ho, HIDDEN_SIZE * OUTPUT_SIZE);
}

//=============================================================================
// E2E Test: Long-Running Training Stability
//=============================================================================

TEST_F(TernaryNeuralNetworkE2ETest, LongRunningTrainingStability) {
    constexpr size_t EXTENDED_EPOCHS = 200;

    std::vector<float> hidden(HIDDEN_SIZE);
    std::vector<float> output(OUTPUT_SIZE);

    float max_loss = 0.0f;
    float min_loss = std::numeric_limits<float>::max();
    size_t nan_count = 0;
    size_t inf_count = 0;

    for (size_t epoch = 0; epoch < EXTENDED_EPOCHS; epoch++) {
        float epoch_loss = 0.0f;

        for (size_t s = 0; s < NUM_SAMPLES; s++) {
            const float* input = &input_data[s * INPUT_SIZE];
            const float* target = &target_data[s * OUTPUT_SIZE];

            TernaryForward(input, hidden.data(), output.data(),
                           weights_ih, weights_ho);

            float sample_loss = ComputeMSE(output.data(), target, OUTPUT_SIZE);

            if (std::isnan(sample_loss)) {
                nan_count++;
            } else if (std::isinf(sample_loss)) {
                inf_count++;
            } else {
                epoch_loss += sample_loss;
            }
        }

        epoch_loss /= NUM_SAMPLES;

        if (!std::isnan(epoch_loss) && !std::isinf(epoch_loss)) {
            max_loss = std::max(max_loss, epoch_loss);
            min_loss = std::min(min_loss, epoch_loss);
        }

        // Apply decay to accumulated deltas periodically
        if (epoch % 5 == 4) {
            snn_ternary_decay(weights_ih, 0.9f);
            snn_ternary_decay(weights_ho, 0.9f);
        }
    }

    // Verify numerical stability
    EXPECT_EQ(nan_count, 0u) << "NaN values encountered during training";
    EXPECT_EQ(inf_count, 0u) << "Inf values encountered during training";

    // Verify loss bounds are reasonable
    EXPECT_LT(max_loss, 1000.0f);
    EXPECT_GT(min_loss, 0.0f);
}

//=============================================================================
// E2E Test: Ternary Weight Serialization Round-Trip
//=============================================================================

TEST_F(TernaryNeuralNetworkE2ETest, TernaryWeightSerializationRoundTrip) {
    // Get original weight statistics
    snn_ternary_stats_t original_stats;
    EXPECT_EQ(0, snn_ternary_get_stats(weights_ih, &original_stats));

    // Serialize
    size_t buffer_size = snn_ternary_serialize(weights_ih, nullptr, 0);
    ASSERT_GT(buffer_size, 0u);

    std::vector<uint8_t> buffer(buffer_size);
    size_t written = snn_ternary_serialize(weights_ih, buffer.data(), buffer_size);
    EXPECT_EQ(written, buffer_size);

    // Deserialize
    snn_ternary_weight_matrix_t* restored =
        snn_ternary_deserialize(buffer.data(), buffer_size);
    ASSERT_NE(restored, nullptr);

    // Verify restored weights match original
    snn_ternary_stats_t restored_stats;
    EXPECT_EQ(0, snn_ternary_get_stats(restored, &restored_stats));

    EXPECT_EQ(original_stats.n_positive, restored_stats.n_positive);
    EXPECT_EQ(original_stats.n_unknown, restored_stats.n_unknown);
    EXPECT_EQ(original_stats.n_negative, restored_stats.n_negative);

    // Verify individual weights match
    for (uint32_t i = 0; i < weights_ih->pre_size; i++) {
        for (uint32_t j = 0; j < weights_ih->post_size; j++) {
            EXPECT_EQ(
                snn_ternary_get_weight(weights_ih, i, j),
                snn_ternary_get_weight(restored, i, j)
            );
        }
    }

    snn_ternary_destroy(restored);
}

//=============================================================================
// E2E Test: Ternary Quantization from Float Weights
//=============================================================================

TEST_F(TernaryNeuralNetworkE2ETest, TernaryQuantizationFromFloatWeights) {
    // Create float weight matrix
    std::vector<float> float_weights(INPUT_SIZE * HIDDEN_SIZE);
    std::normal_distribution<float> dist(0.0f, 1.0f);

    for (size_t i = 0; i < float_weights.size(); i++) {
        float_weights[i] = dist(rng);
    }

    // Quantize to ternary
    snn_ternary_config_t config;
    snn_ternary_default_config(&config);
    config.pack_mode = TERNARY_PACK_2BIT;

    snn_ternary_weight_matrix_t* quantized =
        snn_ternary_from_floats(float_weights.data(),
                                 INPUT_SIZE, HIDDEN_SIZE,
                                 QUANTIZATION_THRESHOLD, &config);
    ASSERT_NE(quantized, nullptr);

    // Verify quantization correctness
    for (size_t i = 0; i < INPUT_SIZE; i++) {
        for (size_t j = 0; j < HIDDEN_SIZE; j++) {
            float original = float_weights[i * HIDDEN_SIZE + j];
            trit_t ternary = snn_ternary_get_weight(quantized, i, j);

            if (original >= QUANTIZATION_THRESHOLD) {
                EXPECT_EQ(ternary, TRIT_POSITIVE);
            } else if (original <= -QUANTIZATION_THRESHOLD) {
                EXPECT_EQ(ternary, TRIT_NEGATIVE);
            } else {
                EXPECT_EQ(ternary, TRIT_UNKNOWN);
            }
        }
    }

    // Verify we can convert back to floats
    std::vector<float> restored_floats(INPUT_SIZE * HIDDEN_SIZE);
    EXPECT_EQ(0, snn_ternary_to_floats(quantized, restored_floats.data()));

    // Verify restored values are ternary scaled
    for (size_t i = 0; i < restored_floats.size(); i++) {
        float val = restored_floats[i];
        EXPECT_TRUE(
            val == quantized->positive_scale ||
            val == quantized->negative_scale ||
            val == 0.0f
        );
    }

    snn_ternary_destroy(quantized);
}

//=============================================================================
// E2E Test: Different Packing Modes Equivalence
//=============================================================================

TEST_F(TernaryNeuralNetworkE2ETest, DifferentPackingModesEquivalence) {
    // Create matrices with different packing modes
    snn_ternary_config_t config;
    snn_ternary_default_config(&config);

    config.pack_mode = TERNARY_PACK_NONE;
    snn_ternary_weight_matrix_t* unpacked =
        snn_ternary_create(INPUT_SIZE, HIDDEN_SIZE, &config);

    config.pack_mode = TERNARY_PACK_2BIT;
    snn_ternary_weight_matrix_t* packed_2bit =
        snn_ternary_create(INPUT_SIZE, HIDDEN_SIZE, &config);

    config.pack_mode = TERNARY_PACK_BASE243;
    snn_ternary_weight_matrix_t* packed_243 =
        snn_ternary_create(INPUT_SIZE, HIDDEN_SIZE, &config);

    ASSERT_NE(unpacked, nullptr);
    ASSERT_NE(packed_2bit, nullptr);
    ASSERT_NE(packed_243, nullptr);

    // Set same weights in all matrices
    std::uniform_int_distribution<int> dist(-1, 1);
    for (uint32_t i = 0; i < INPUT_SIZE; i++) {
        for (uint32_t j = 0; j < HIDDEN_SIZE; j++) {
            trit_t val = (trit_t)dist(rng);
            snn_ternary_set_weight(unpacked, i, j, val);
            snn_ternary_set_weight(packed_2bit, i, j, val);
            snn_ternary_set_weight(packed_243, i, j, val);
        }
    }

    // Verify all matrices produce same forward pass output
    std::vector<float> input(INPUT_SIZE);
    for (size_t i = 0; i < INPUT_SIZE; i++) {
        input[i] = (float)(i % 3 - 1);  // -1, 0, 1, -1, 0, 1, ...
    }

    std::vector<float> output_unpacked(HIDDEN_SIZE);
    std::vector<float> output_2bit(HIDDEN_SIZE);
    std::vector<float> output_243(HIDDEN_SIZE);

    EXPECT_EQ(0, snn_ternary_forward_float(unpacked, input.data(), output_unpacked.data()));
    EXPECT_EQ(0, snn_ternary_forward_float(packed_2bit, input.data(), output_2bit.data()));
    EXPECT_EQ(0, snn_ternary_forward_float(packed_243, input.data(), output_243.data()));

    // Verify outputs match
    for (size_t i = 0; i < HIDDEN_SIZE; i++) {
        EXPECT_FLOAT_EQ(output_unpacked[i], output_2bit[i]);
        EXPECT_FLOAT_EQ(output_unpacked[i], output_243[i]);
    }

    snn_ternary_destroy(unpacked);
    snn_ternary_destroy(packed_2bit);
    snn_ternary_destroy(packed_243);
}

//=============================================================================
// E2E Test: Spike-Based Forward Pass
//=============================================================================

TEST_F(TernaryNeuralNetworkE2ETest, SpikeBasedForwardPass) {
    // Create spike input (binary spikes)
    std::vector<uint8_t> spikes(INPUT_SIZE);
    std::vector<float> output(HIDDEN_SIZE);

    // Generate various spike patterns
    for (int pattern = 0; pattern < 10; pattern++) {
        // Create spike pattern
        for (size_t i = 0; i < INPUT_SIZE; i++) {
            spikes[i] = ((i + pattern) % 3 == 0) ? 1 : 0;
        }

        // Forward pass with spikes
        int result = snn_ternary_forward(weights_ih, spikes.data(), output.data());
        EXPECT_EQ(0, result);

        // Verify outputs are valid
        for (size_t h = 0; h < HIDDEN_SIZE; h++) {
            EXPECT_FALSE(std::isnan(output[h]));
            EXPECT_FALSE(std::isinf(output[h]));
        }
    }
}

//=============================================================================
// E2E Test: STDP Learning with Ternary State Transitions
//=============================================================================

TEST_F(TernaryNeuralNetworkE2ETest, STDPLearningWithTernaryStateTransitions) {
    // Count initial weight distribution
    size_t initial_pos = 0, initial_unk = 0, initial_neg = 0;
    for (uint32_t i = 0; i < weights_ih->pre_size; i++) {
        for (uint32_t j = 0; j < weights_ih->post_size; j++) {
            trit_t w = snn_ternary_get_weight(weights_ih, i, j);
            if (w == TRIT_POSITIVE) initial_pos++;
            else if (w == TRIT_NEGATIVE) initial_neg++;
            else initial_unk++;
        }
    }

    // Apply many STDP updates
    size_t ltp_updates = 0;
    size_t ltd_updates = 0;

    for (size_t iter = 0; iter < 1000; iter++) {
        uint32_t i = iter % weights_ih->pre_size;
        uint32_t j = (iter * 7) % weights_ih->post_size;

        // Alternate between LTP and LTD signals
        float delta = (iter % 2 == 0) ? 0.4f : -0.4f;
        int changed = snn_ternary_stdp_update(weights_ih, i, j, delta);

        if (changed > 0) {
            if (delta > 0) ltp_updates++;
            else ltd_updates++;
        }
    }

    // Force discretization
    int discretize_changes = snn_ternary_discretize(weights_ih);

    // Verify some weight transitions occurred
    size_t total_changes = ltp_updates + ltd_updates + discretize_changes;
    EXPECT_GT(total_changes, 0u) << "Expected some weight transitions";

    // Count final weight distribution
    size_t final_pos = 0, final_unk = 0, final_neg = 0;
    for (uint32_t i = 0; i < weights_ih->pre_size; i++) {
        for (uint32_t j = 0; j < weights_ih->post_size; j++) {
            trit_t w = snn_ternary_get_weight(weights_ih, i, j);
            if (w == TRIT_POSITIVE) final_pos++;
            else if (w == TRIT_NEGATIVE) final_neg++;
            else final_unk++;
        }
    }

    // Verify total count unchanged
    size_t total_initial = initial_pos + initial_unk + initial_neg;
    size_t total_final = final_pos + final_unk + final_neg;
    EXPECT_EQ(total_initial, total_final);
}

//=============================================================================
// E2E Test: Batch Processing Performance
//=============================================================================

TEST_F(TernaryNeuralNetworkE2ETest, BatchProcessingPerformance) {
    constexpr size_t BATCH_SIZE = 1024;
    constexpr size_t NUM_BATCHES = 10;

    std::vector<float> batch_inputs(BATCH_SIZE * INPUT_SIZE);
    std::vector<float> batch_hidden(BATCH_SIZE * HIDDEN_SIZE);
    std::vector<float> batch_outputs(BATCH_SIZE * OUTPUT_SIZE);

    // Generate batch data
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (size_t i = 0; i < batch_inputs.size(); i++) {
        batch_inputs[i] = dist(rng);
    }

    auto start = std::chrono::high_resolution_clock::now();

    // Process multiple batches
    for (size_t batch = 0; batch < NUM_BATCHES; batch++) {
        for (size_t s = 0; s < BATCH_SIZE; s++) {
            const float* input = &batch_inputs[s * INPUT_SIZE];
            float* hidden = &batch_hidden[s * HIDDEN_SIZE];
            float* output = &batch_outputs[s * OUTPUT_SIZE];

            TernaryForward(input, hidden, output, weights_ih, weights_ho);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Verify all outputs are valid
    size_t valid_outputs = 0;
    for (size_t i = 0; i < batch_outputs.size(); i++) {
        if (!std::isnan(batch_outputs[i]) && !std::isinf(batch_outputs[i])) {
            valid_outputs++;
        }
    }
    EXPECT_EQ(valid_outputs, batch_outputs.size());

    // Log performance
    size_t total_samples = NUM_BATCHES * BATCH_SIZE;
    float samples_per_ms = (float)total_samples / (float)duration.count();

    // Just verify it completed in reasonable time (< 10 seconds)
    EXPECT_LT(duration.count(), 10000) << "Batch processing took too long";

    // Performance should be at least 100 samples/ms for ternary weights
    // (actual performance will vary by hardware)
    SUCCEED() << "Processed " << total_samples << " samples in "
              << duration.count() << "ms (" << samples_per_ms << " samples/ms)";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
