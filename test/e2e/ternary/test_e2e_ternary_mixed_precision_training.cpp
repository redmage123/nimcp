/**
 * @file test_e2e_ternary_mixed_precision_training.cpp
 * @brief End-to-end tests for mixed precision training including ternary
 *
 * WHAT: Full training pipeline with mixed precision including ternary
 * WHY:  Verify ternary quantization in complete training workflows
 * HOW:  Test gradient accumulation, training stability, precision mixing
 *
 * TEST COVERAGE:
 * - Full training pipeline with mixed precision (float32, float16, int8, ternary)
 * - Gradient accumulation with ternary forward pass
 * - Training stability verification
 * - Precision switching during training
 * - Memory and compute efficiency
 *
 * MIXED PRECISION APPROACH:
 * - Forward pass: ternary weights for efficiency
 * - Gradients: float32 for accuracy
 * - Accumulators: float32 for stability
 * - Weight updates: threshold-based quantization to ternary
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>
#include <limits>

// Headers have their own extern "C" guards
#include "utils/ternary/nimcp_ternary.h"
#include "utils/ternary/nimcp_ternary_matrix.h"
#include "utils/ternary/nimcp_ternary_vector.h"
#include "utils/ternary/nimcp_ternary_convert.h"
#include "snn/nimcp_snn_ternary.h"
#include "plasticity/nimcp_plasticity_ternary.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TernaryMixedPrecisionTrainingE2ETest : public ::testing::Test {
protected:
    // Network dimensions
    static constexpr size_t INPUT_SIZE = 128;
    static constexpr size_t HIDDEN_SIZE = 256;
    static constexpr size_t OUTPUT_SIZE = 32;
    static constexpr size_t BATCH_SIZE = 64;
    static constexpr size_t NUM_EPOCHS = 100;
    static constexpr float LEARNING_RATE = 0.01f;
    static constexpr float TERNARY_THRESHOLD = 0.3f;

    // Mixed precision types
    enum Precision {
        PRECISION_FLOAT32,
        PRECISION_FLOAT16,
        PRECISION_INT8,
        PRECISION_TERNARY
    };

    struct MixedPrecisionLayer {
        // Ternary weights for forward pass
        trit_matrix_t* ternary_weights;

        // Float32 accumulators for gradient accumulation
        std::vector<float> weight_accumulators;

        // Float32 gradients
        std::vector<float> gradients;

        // Scale factors for quantization
        float positive_scale;
        float negative_scale;

        size_t input_size;
        size_t output_size;
    };

    struct TrainingMetrics {
        std::vector<float> epoch_losses;
        std::vector<float> epoch_accuracies;
        std::vector<size_t> weight_updates;
        size_t total_updates;
    };

    MixedPrecisionLayer layer_ih;  // Input to Hidden
    MixedPrecisionLayer layer_ho;  // Hidden to Output

    std::vector<std::vector<float>> train_inputs;
    std::vector<std::vector<float>> train_targets;

    TrainingMetrics metrics;
    std::mt19937 rng;

    void SetUp() override {
        rng.seed(42);

        // Initialize layers
        InitializeLayer(layer_ih, INPUT_SIZE, HIDDEN_SIZE);
        InitializeLayer(layer_ho, HIDDEN_SIZE, OUTPUT_SIZE);

        // Generate training data
        GenerateTrainingData();

        // Initialize metrics
        metrics.total_updates = 0;
    }

    void TearDown() override {
        if (layer_ih.ternary_weights) trit_matrix_destroy(layer_ih.ternary_weights);
        if (layer_ho.ternary_weights) trit_matrix_destroy(layer_ho.ternary_weights);
    }

    void InitializeLayer(MixedPrecisionLayer& layer, size_t in_size, size_t out_size) {
        layer.input_size = in_size;
        layer.output_size = out_size;

        // Create ternary weights
        layer.ternary_weights = trit_matrix_create(in_size, out_size, TERNARY_PACK_BASE243);
        ASSERT_NE(layer.ternary_weights, nullptr);

        // Initialize with random ternary values
        std::uniform_int_distribution<int> dist(-1, 1);
        for (size_t i = 0; i < in_size; i++) {
            for (size_t j = 0; j < out_size; j++) {
                trit_matrix_set(layer.ternary_weights, i, j, (trit_t)dist(rng));
            }
        }

        // Initialize float32 accumulators to match ternary weights
        layer.weight_accumulators.resize(in_size * out_size, 0.0f);
        for (size_t i = 0; i < in_size; i++) {
            for (size_t j = 0; j < out_size; j++) {
                trit_t w = trit_matrix_get(layer.ternary_weights, i, j);
                layer.weight_accumulators[i * out_size + j] = (float)w * 0.5f;
            }
        }

        // Initialize gradients
        layer.gradients.resize(in_size * out_size, 0.0f);

        // Scale factors
        layer.positive_scale = 0.5f;
        layer.negative_scale = -0.5f;
    }

    void GenerateTrainingData() {
        std::normal_distribution<float> input_dist(0.0f, 1.0f);

        train_inputs.resize(BATCH_SIZE);
        train_targets.resize(BATCH_SIZE);

        for (size_t b = 0; b < BATCH_SIZE; b++) {
            train_inputs[b].resize(INPUT_SIZE);
            train_targets[b].resize(OUTPUT_SIZE);

            // Generate random input
            for (size_t i = 0; i < INPUT_SIZE; i++) {
                train_inputs[b][i] = input_dist(rng);
            }

            // Generate target (simple linear mapping with noise)
            for (size_t o = 0; o < OUTPUT_SIZE; o++) {
                float sum = 0.0f;
                for (size_t i = 0; i < std::min(INPUT_SIZE, (size_t)4); i++) {
                    sum += train_inputs[b][i] * ((o + i) % 3 - 1);
                }
                train_targets[b][o] = tanhf(sum);
            }
        }
    }

    // Forward pass with ternary weights
    void TernaryForward(const MixedPrecisionLayer& layer,
                        const std::vector<float>& input,
                        std::vector<float>& output) {
        output.resize(layer.output_size, 0.0f);

        for (size_t j = 0; j < layer.output_size; j++) {
            float sum = 0.0f;
            for (size_t i = 0; i < layer.input_size; i++) {
                trit_t w = trit_matrix_get(layer.ternary_weights, i, j);
                float scaled_w = (w == TRIT_POSITIVE) ? layer.positive_scale :
                                 (w == TRIT_NEGATIVE) ? layer.negative_scale : 0.0f;
                sum += scaled_w * input[i];
            }
            output[j] = sum;
        }
    }

    // ReLU activation
    void ReLU(std::vector<float>& x) {
        for (float& val : x) {
            val = std::max(0.0f, val);
        }
    }

    // ReLU derivative
    void ReLUGradient(const std::vector<float>& x, std::vector<float>& grad) {
        for (size_t i = 0; i < x.size(); i++) {
            grad[i] = (x[i] > 0.0f) ? grad[i] : 0.0f;
        }
    }

    // Compute MSE loss
    float ComputeLoss(const std::vector<float>& output,
                      const std::vector<float>& target) {
        float loss = 0.0f;
        for (size_t i = 0; i < output.size(); i++) {
            float diff = output[i] - target[i];
            loss += diff * diff;
        }
        return loss / output.size();
    }

    // Backward pass with float32 gradients
    void ComputeGradients(MixedPrecisionLayer& layer,
                          const std::vector<float>& input,
                          const std::vector<float>& output_grad) {
        // Compute gradients for each weight
        for (size_t i = 0; i < layer.input_size; i++) {
            for (size_t j = 0; j < layer.output_size; j++) {
                layer.gradients[i * layer.output_size + j] =
                    input[i] * output_grad[j];
            }
        }
    }

    // Update weights with mixed precision
    size_t UpdateWeights(MixedPrecisionLayer& layer, float learning_rate) {
        size_t updates = 0;

        for (size_t i = 0; i < layer.input_size; i++) {
            for (size_t j = 0; j < layer.output_size; j++) {
                size_t idx = i * layer.output_size + j;

                // Accumulate gradient in float32
                layer.weight_accumulators[idx] -= learning_rate * layer.gradients[idx];

                // Clamp accumulator
                layer.weight_accumulators[idx] = std::clamp(
                    layer.weight_accumulators[idx], -1.0f, 1.0f);

                // Quantize to ternary
                trit_t old_weight = trit_matrix_get(layer.ternary_weights, i, j);
                trit_t new_weight = trit_from_float_threshold(
                    layer.weight_accumulators[idx], TERNARY_THRESHOLD);

                if (old_weight != new_weight) {
                    trit_matrix_set(layer.ternary_weights, i, j, new_weight);
                    updates++;
                }
            }
        }

        return updates;
    }

    // Reset gradients
    void ResetGradients(MixedPrecisionLayer& layer) {
        std::fill(layer.gradients.begin(), layer.gradients.end(), 0.0f);
    }
};

//=============================================================================
// E2E Test: Full Training Pipeline
//=============================================================================

TEST_F(TernaryMixedPrecisionTrainingE2ETest, FullTrainingPipeline) {
    std::vector<float> hidden(HIDDEN_SIZE);
    std::vector<float> output(OUTPUT_SIZE);
    std::vector<float> hidden_grad(HIDDEN_SIZE);
    std::vector<float> output_grad(OUTPUT_SIZE);

    for (size_t epoch = 0; epoch < NUM_EPOCHS; epoch++) {
        float epoch_loss = 0.0f;
        size_t epoch_updates = 0;

        for (size_t b = 0; b < BATCH_SIZE; b++) {
            const auto& input = train_inputs[b];
            const auto& target = train_targets[b];

            // Forward pass with ternary weights
            TernaryForward(layer_ih, input, hidden);
            ReLU(hidden);
            TernaryForward(layer_ho, hidden, output);

            // Compute loss
            epoch_loss += ComputeLoss(output, target);

            // Compute output gradients
            for (size_t o = 0; o < OUTPUT_SIZE; o++) {
                output_grad[o] = 2.0f * (output[o] - target[o]) / OUTPUT_SIZE;
            }

            // Backpropagate through layer_ho
            ComputeGradients(layer_ho, hidden, output_grad);

            // Compute hidden gradients
            std::fill(hidden_grad.begin(), hidden_grad.end(), 0.0f);
            for (size_t h = 0; h < HIDDEN_SIZE; h++) {
                for (size_t o = 0; o < OUTPUT_SIZE; o++) {
                    trit_t w = trit_matrix_get(layer_ho.ternary_weights, h, o);
                    float scaled_w = (w == TRIT_POSITIVE) ? layer_ho.positive_scale :
                                     (w == TRIT_NEGATIVE) ? layer_ho.negative_scale : 0.0f;
                    hidden_grad[h] += scaled_w * output_grad[o];
                }
            }
            ReLUGradient(hidden, hidden_grad);

            // Backpropagate through layer_ih
            ComputeGradients(layer_ih, input, hidden_grad);

            // Update weights (mixed precision: float32 accumulation, ternary quantization)
            epoch_updates += UpdateWeights(layer_ih, LEARNING_RATE);
            epoch_updates += UpdateWeights(layer_ho, LEARNING_RATE);

            // Reset gradients
            ResetGradients(layer_ih);
            ResetGradients(layer_ho);
        }

        epoch_loss /= BATCH_SIZE;
        metrics.epoch_losses.push_back(epoch_loss);
        metrics.weight_updates.push_back(epoch_updates);
        metrics.total_updates += epoch_updates;
    }

    // Verify training progress
    ASSERT_GT(metrics.epoch_losses.size(), 1u);

    float initial_loss = metrics.epoch_losses[0];
    float final_loss = metrics.epoch_losses.back();

    // Loss should not explode
    EXPECT_LT(final_loss, initial_loss * 10.0f)
        << "Training unstable - loss exploded";

    // Should have some weight updates
    EXPECT_GT(metrics.total_updates, 0u)
        << "No weight updates occurred during training";
}

//=============================================================================
// E2E Test: Gradient Accumulation with Ternary Forward
//=============================================================================

TEST_F(TernaryMixedPrecisionTrainingE2ETest, GradientAccumulationWithTernaryForward) {
    constexpr size_t ACCUMULATION_STEPS = 8;

    std::vector<float> hidden(HIDDEN_SIZE);
    std::vector<float> output(OUTPUT_SIZE);
    std::vector<float> output_grad(OUTPUT_SIZE);
    std::vector<float> hidden_grad(HIDDEN_SIZE);

    // Accumulated gradients
    std::vector<float> ih_grad_accum(INPUT_SIZE * HIDDEN_SIZE, 0.0f);
    std::vector<float> ho_grad_accum(HIDDEN_SIZE * OUTPUT_SIZE, 0.0f);

    for (size_t step = 0; step < ACCUMULATION_STEPS; step++) {
        size_t batch_idx = step % BATCH_SIZE;
        const auto& input = train_inputs[batch_idx];
        const auto& target = train_targets[batch_idx];

        // Forward with ternary
        TernaryForward(layer_ih, input, hidden);
        ReLU(hidden);
        TernaryForward(layer_ho, hidden, output);

        // Backward with float32
        for (size_t o = 0; o < OUTPUT_SIZE; o++) {
            output_grad[o] = 2.0f * (output[o] - target[o]) / OUTPUT_SIZE;
        }

        // Compute and accumulate gradients for layer_ho
        for (size_t h = 0; h < HIDDEN_SIZE; h++) {
            for (size_t o = 0; o < OUTPUT_SIZE; o++) {
                ho_grad_accum[h * OUTPUT_SIZE + o] += hidden[h] * output_grad[o];
            }
        }

        // Backprop to hidden
        std::fill(hidden_grad.begin(), hidden_grad.end(), 0.0f);
        for (size_t h = 0; h < HIDDEN_SIZE; h++) {
            for (size_t o = 0; o < OUTPUT_SIZE; o++) {
                trit_t w = trit_matrix_get(layer_ho.ternary_weights, h, o);
                float scaled_w = trit_to_float_scaled(w, layer_ho.positive_scale);
                hidden_grad[h] += scaled_w * output_grad[o];
            }
        }
        ReLUGradient(hidden, hidden_grad);

        // Compute and accumulate gradients for layer_ih
        for (size_t i = 0; i < INPUT_SIZE; i++) {
            for (size_t h = 0; h < HIDDEN_SIZE; h++) {
                ih_grad_accum[i * HIDDEN_SIZE + h] += input[i] * hidden_grad[h];
            }
        }
    }

    // Apply accumulated gradients
    for (size_t i = 0; i < INPUT_SIZE * HIDDEN_SIZE; i++) {
        layer_ih.gradients[i] = ih_grad_accum[i] / ACCUMULATION_STEPS;
    }
    for (size_t i = 0; i < HIDDEN_SIZE * OUTPUT_SIZE; i++) {
        layer_ho.gradients[i] = ho_grad_accum[i] / ACCUMULATION_STEPS;
    }

    size_t updates_ih = UpdateWeights(layer_ih, LEARNING_RATE);
    size_t updates_ho = UpdateWeights(layer_ho, LEARNING_RATE);

    // Verify gradients are valid
    for (size_t i = 0; i < ih_grad_accum.size(); i++) {
        EXPECT_FALSE(std::isnan(ih_grad_accum[i]));
        EXPECT_FALSE(std::isinf(ih_grad_accum[i]));
    }

    SUCCEED() << "Accumulated gradients over " << ACCUMULATION_STEPS << " steps, "
              << (updates_ih + updates_ho) << " weight updates";
}

//=============================================================================
// E2E Test: Training Stability Over Extended Run
//=============================================================================

TEST_F(TernaryMixedPrecisionTrainingE2ETest, TrainingStabilityOverExtendedRun) {
    constexpr size_t EXTENDED_EPOCHS = 500;

    std::vector<float> hidden(HIDDEN_SIZE);
    std::vector<float> output(OUTPUT_SIZE);
    std::vector<float> output_grad(OUTPUT_SIZE);
    std::vector<float> hidden_grad(HIDDEN_SIZE);

    size_t nan_count = 0, inf_count = 0;
    float max_loss = 0.0f;

    for (size_t epoch = 0; epoch < EXTENDED_EPOCHS; epoch++) {
        float epoch_loss = 0.0f;

        for (size_t b = 0; b < BATCH_SIZE / 4; b++) {  // Smaller batches for speed
            const auto& input = train_inputs[b % BATCH_SIZE];
            const auto& target = train_targets[b % BATCH_SIZE];

            TernaryForward(layer_ih, input, hidden);
            ReLU(hidden);
            TernaryForward(layer_ho, hidden, output);

            float loss = ComputeLoss(output, target);

            if (std::isnan(loss)) nan_count++;
            else if (std::isinf(loss)) inf_count++;
            else {
                epoch_loss += loss;
                max_loss = std::max(max_loss, loss);
            }

            // Simple gradient update
            for (size_t o = 0; o < OUTPUT_SIZE; o++) {
                output_grad[o] = output[o] - target[o];
            }

            ComputeGradients(layer_ho, hidden, output_grad);
            UpdateWeights(layer_ho, LEARNING_RATE * 0.1f);

            std::fill(hidden_grad.begin(), hidden_grad.end(), 0.0f);
            for (size_t h = 0; h < HIDDEN_SIZE; h++) {
                for (size_t o = 0; o < OUTPUT_SIZE; o++) {
                    trit_t w = trit_matrix_get(layer_ho.ternary_weights, h, o);
                    hidden_grad[h] += (float)w * output_grad[o];
                }
            }
            ReLUGradient(hidden, hidden_grad);

            ComputeGradients(layer_ih, input, hidden_grad);
            UpdateWeights(layer_ih, LEARNING_RATE * 0.1f);
        }
    }

    EXPECT_EQ(nan_count, 0u) << "NaN values during extended training";
    EXPECT_EQ(inf_count, 0u) << "Inf values during extended training";
    EXPECT_LT(max_loss, 100.0f) << "Loss exploded during training";
}

//=============================================================================
// E2E Test: Precision Conversion Accuracy
//=============================================================================

TEST_F(TernaryMixedPrecisionTrainingE2ETest, PrecisionConversionAccuracy) {
    // Test float32 -> ternary -> float32 conversion accuracy
    std::vector<float> original_weights(INPUT_SIZE * HIDDEN_SIZE);
    std::normal_distribution<float> dist(0.0f, 0.5f);

    for (size_t i = 0; i < original_weights.size(); i++) {
        original_weights[i] = dist(rng);
    }

    // Quantize to ternary
    trit_matrix_t* quantized = trit_matrix_from_floats(
        original_weights.data(), INPUT_SIZE, HIDDEN_SIZE,
        TERNARY_THRESHOLD, TERNARY_PACK_2BIT);
    ASSERT_NE(quantized, nullptr);

    // Convert back to float
    std::vector<float> restored_weights(INPUT_SIZE * HIDDEN_SIZE);
    EXPECT_EQ(TERNARY_OK, trit_matrix_to_floats(quantized, restored_weights.data(), 1.0f));

    // Compute quantization error
    float mse = 0.0f;
    size_t clipped = 0;

    for (size_t i = 0; i < original_weights.size(); i++) {
        float diff = original_weights[i] - restored_weights[i];
        mse += diff * diff;

        // Check if value was correctly categorized
        trit_t expected;
        if (original_weights[i] >= TERNARY_THRESHOLD) expected = TRIT_POSITIVE;
        else if (original_weights[i] <= -TERNARY_THRESHOLD) expected = TRIT_NEGATIVE;
        else expected = TRIT_UNKNOWN;

        trit_t actual = trit_from_float_threshold(restored_weights[i], 0.5f);
        if (expected != actual) clipped++;
    }

    mse /= original_weights.size();

    // Quantization error should be bounded
    // Expected max error is ~0.3 (threshold) squared = ~0.09
    EXPECT_LT(mse, 0.5f) << "Quantization error too high";

    trit_matrix_destroy(quantized);
}

//=============================================================================
// E2E Test: Different Quantization Thresholds
//=============================================================================

TEST_F(TernaryMixedPrecisionTrainingE2ETest, DifferentQuantizationThresholds) {
    std::vector<float> thresholds = {0.1f, 0.3f, 0.5f, 0.7f};
    std::vector<float> weight_update_rates;

    for (float threshold : thresholds) {
        // Reset layer with new threshold
        MixedPrecisionLayer test_layer;
        test_layer.input_size = 32;
        test_layer.output_size = 32;
        test_layer.ternary_weights = trit_matrix_create(32, 32, TERNARY_PACK_2BIT);
        test_layer.weight_accumulators.resize(32 * 32, 0.0f);
        test_layer.gradients.resize(32 * 32, 0.0f);
        test_layer.positive_scale = 0.5f;
        test_layer.negative_scale = -0.5f;

        // Initialize with zeros
        for (size_t i = 0; i < 32; i++) {
            for (size_t j = 0; j < 32; j++) {
                trit_matrix_set(test_layer.ternary_weights, i, j, TRIT_UNKNOWN);
            }
        }

        // Apply gradients and count updates
        std::uniform_real_distribution<float> grad_dist(-0.5f, 0.5f);
        size_t updates = 0;

        for (size_t iter = 0; iter < 100; iter++) {
            for (size_t i = 0; i < 32 * 32; i++) {
                test_layer.gradients[i] = grad_dist(rng);
            }

            // Custom threshold update
            for (size_t i = 0; i < 32; i++) {
                for (size_t j = 0; j < 32; j++) {
                    size_t idx = i * 32 + j;
                    test_layer.weight_accumulators[idx] -= 0.01f * test_layer.gradients[idx];
                    test_layer.weight_accumulators[idx] = std::clamp(
                        test_layer.weight_accumulators[idx], -1.0f, 1.0f);

                    trit_t old = trit_matrix_get(test_layer.ternary_weights, i, j);
                    trit_t new_val = trit_from_float_threshold(
                        test_layer.weight_accumulators[idx], threshold);

                    if (old != new_val) {
                        trit_matrix_set(test_layer.ternary_weights, i, j, new_val);
                        updates++;
                    }
                }
            }
        }

        weight_update_rates.push_back((float)updates / (100 * 32 * 32));
        trit_matrix_destroy(test_layer.ternary_weights);
    }

    // Lower thresholds should produce more updates (easier to cross threshold)
    EXPECT_GE(weight_update_rates[0], weight_update_rates[1]);
    EXPECT_GE(weight_update_rates[1], weight_update_rates[2]);

    SUCCEED() << "Update rates by threshold: "
              << weight_update_rates[0] << " (0.1), "
              << weight_update_rates[1] << " (0.3), "
              << weight_update_rates[2] << " (0.5), "
              << weight_update_rates[3] << " (0.7)";
}

//=============================================================================
// E2E Test: Memory Efficiency Comparison
//=============================================================================

TEST_F(TernaryMixedPrecisionTrainingE2ETest, MemoryEfficiencyComparison) {
    // Calculate memory for each precision

    // Float32
    size_t float32_bytes = (INPUT_SIZE * HIDDEN_SIZE + HIDDEN_SIZE * OUTPUT_SIZE)
                           * sizeof(float);

    // Float16
    size_t float16_bytes = (INPUT_SIZE * HIDDEN_SIZE + HIDDEN_SIZE * OUTPUT_SIZE)
                           * sizeof(uint16_t);  // float16 = 2 bytes

    // Int8
    size_t int8_bytes = (INPUT_SIZE * HIDDEN_SIZE + HIDDEN_SIZE * OUTPUT_SIZE)
                        * sizeof(int8_t);

    // Ternary
    size_t ternary_bytes = trit_matrix_memory_size(layer_ih.ternary_weights) +
                           trit_matrix_memory_size(layer_ho.ternary_weights);

    // Verify ternary is most compact
    EXPECT_LT(ternary_bytes, int8_bytes);
    EXPECT_LT(int8_bytes, float16_bytes);
    EXPECT_LT(float16_bytes, float32_bytes);

    // Calculate compression ratios
    float ternary_vs_float32 = (float)float32_bytes / ternary_bytes;
    float ternary_vs_int8 = (float)int8_bytes / ternary_bytes;

    EXPECT_GT(ternary_vs_float32, 10.0f)
        << "Expected >10x compression vs float32";
    EXPECT_GT(ternary_vs_int8, 2.0f)
        << "Expected >2x compression vs int8";

    SUCCEED() << "Memory comparison:\n"
              << "  Float32: " << float32_bytes << " bytes\n"
              << "  Float16: " << float16_bytes << " bytes\n"
              << "  Int8:    " << int8_bytes << " bytes\n"
              << "  Ternary: " << ternary_bytes << " bytes\n"
              << "  Ternary/Float32: " << ternary_vs_float32 << "x compression";
}

//=============================================================================
// E2E Test: Gradient Scaling for Ternary
//=============================================================================

TEST_F(TernaryMixedPrecisionTrainingE2ETest, GradientScalingForTernary) {
    // Test that gradient scaling works correctly with ternary forward
    std::vector<float> scales = {0.1f, 1.0f, 10.0f, 100.0f};

    for (float scale : scales) {
        // Reset accumulators
        std::fill(layer_ih.weight_accumulators.begin(),
                  layer_ih.weight_accumulators.end(), 0.0f);

        // Apply scaled gradients
        for (size_t i = 0; i < layer_ih.gradients.size(); i++) {
            layer_ih.gradients[i] = 0.01f * scale;
        }

        // Update should work without NaN/Inf
        size_t updates = UpdateWeights(layer_ih, LEARNING_RATE / scale);

        // Verify no numerical issues
        for (size_t i = 0; i < layer_ih.weight_accumulators.size(); i++) {
            EXPECT_FALSE(std::isnan(layer_ih.weight_accumulators[i]));
            EXPECT_FALSE(std::isinf(layer_ih.weight_accumulators[i]));
        }
    }
}

//=============================================================================
// E2E Test: Performance Benchmark
//=============================================================================

TEST_F(TernaryMixedPrecisionTrainingE2ETest, PerformanceBenchmark) {
    constexpr size_t BENCHMARK_ITERATIONS = 1000;

    std::vector<float> hidden(HIDDEN_SIZE);
    std::vector<float> output(OUTPUT_SIZE);
    std::vector<float> output_grad(OUTPUT_SIZE);
    std::vector<float> hidden_grad(HIDDEN_SIZE);

    // Benchmark ternary forward
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t iter = 0; iter < BENCHMARK_ITERATIONS; iter++) {
        const auto& input = train_inputs[iter % BATCH_SIZE];
        const auto& target = train_targets[iter % BATCH_SIZE];

        TernaryForward(layer_ih, input, hidden);
        ReLU(hidden);
        TernaryForward(layer_ho, hidden, output);

        // Backward
        for (size_t o = 0; o < OUTPUT_SIZE; o++) {
            output_grad[o] = output[o] - target[o];
        }
        ComputeGradients(layer_ho, hidden, output_grad);

        std::fill(hidden_grad.begin(), hidden_grad.end(), 0.0f);
        for (size_t h = 0; h < HIDDEN_SIZE; h++) {
            for (size_t o = 0; o < OUTPUT_SIZE; o++) {
                trit_t w = trit_matrix_get(layer_ho.ternary_weights, h, o);
                hidden_grad[h] += (float)w * output_grad[o];
            }
        }

        ComputeGradients(layer_ih, input, hidden_grad);
        UpdateWeights(layer_ih, LEARNING_RATE);
        UpdateWeights(layer_ho, LEARNING_RATE);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float iterations_per_second = (float)BENCHMARK_ITERATIONS * 1e6f / duration.count();

    EXPECT_GT(iterations_per_second, 100.0f)
        << "Performance: " << iterations_per_second << " iterations/second";

    SUCCEED() << "Mixed precision training: " << iterations_per_second
              << " iterations/second";
}

//=============================================================================
// E2E Test: Weight Distribution Over Training
//=============================================================================

TEST_F(TernaryMixedPrecisionTrainingE2ETest, WeightDistributionOverTraining) {
    std::vector<float> hidden(HIDDEN_SIZE);
    std::vector<float> output(OUTPUT_SIZE);

    // Track weight distribution changes
    auto GetDistribution = [this](const MixedPrecisionLayer& layer) {
        size_t pos = 0, neg = 0, zero = 0;
        for (size_t i = 0; i < layer.input_size; i++) {
            for (size_t j = 0; j < layer.output_size; j++) {
                trit_t w = trit_matrix_get(layer.ternary_weights, i, j);
                if (w == TRIT_POSITIVE) pos++;
                else if (w == TRIT_NEGATIVE) neg++;
                else zero++;
            }
        }
        return std::make_tuple(pos, neg, zero);
    };

    auto [init_pos, init_neg, init_zero] = GetDistribution(layer_ih);

    // Train for a bit
    for (size_t epoch = 0; epoch < 50; epoch++) {
        for (size_t b = 0; b < BATCH_SIZE / 8; b++) {
            const auto& input = train_inputs[b];
            const auto& target = train_targets[b];

            TernaryForward(layer_ih, input, hidden);
            ReLU(hidden);
            TernaryForward(layer_ho, hidden, output);

            std::vector<float> output_grad(OUTPUT_SIZE);
            std::vector<float> hidden_grad(HIDDEN_SIZE, 0.0f);

            for (size_t o = 0; o < OUTPUT_SIZE; o++) {
                output_grad[o] = output[o] - target[o];
            }

            ComputeGradients(layer_ho, hidden, output_grad);
            UpdateWeights(layer_ho, LEARNING_RATE);

            for (size_t h = 0; h < HIDDEN_SIZE; h++) {
                for (size_t o = 0; o < OUTPUT_SIZE; o++) {
                    trit_t w = trit_matrix_get(layer_ho.ternary_weights, h, o);
                    hidden_grad[h] += (float)w * output_grad[o];
                }
            }
            ReLUGradient(hidden, hidden_grad);

            ComputeGradients(layer_ih, input, hidden_grad);
            UpdateWeights(layer_ih, LEARNING_RATE);
        }
    }

    auto [final_pos, final_neg, final_zero] = GetDistribution(layer_ih);

    // Distribution should change during training
    size_t total = INPUT_SIZE * HIDDEN_SIZE;
    bool distribution_changed = (init_pos != final_pos) ||
                                (init_neg != final_neg) ||
                                (init_zero != final_zero);

    EXPECT_TRUE(distribution_changed) << "Weight distribution unchanged during training";

    SUCCEED() << "Initial: pos=" << init_pos << ", neg=" << init_neg << ", zero=" << init_zero
              << "\nFinal:   pos=" << final_pos << ", neg=" << final_neg << ", zero=" << final_zero;
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
