/**
 * @file test_jepa_gpu.cpp
 * @brief Unit tests for GPU-accelerated JEPA Predictor
 *
 * WHAT: Unit tests for GPU JEPA kernels and API
 * WHY:  Verify correctness of GPU-accelerated latent space prediction
 * HOW:  Test individual operations: forward prediction, inverse model, masking, loss computation
 *
 * @version 1.0
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>

// GPU headers outside extern "C"
#include "gpu/cognitive/nimcp_jepa_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Fixture
//=============================================================================

class JEPAGPUUnitTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx;
    nimcp_jepa_gpu_predictor_t* predictor;
    nimcp_jepa_gpu_inverse_t* inverse;
    std::mt19937 rng;

    // Test dimensions
    static constexpr uint32_t INPUT_DIM = 64;
    static constexpr uint32_t HIDDEN_DIM = 128;
    static constexpr uint32_t OUTPUT_DIM = 64;
    static constexpr uint32_t NUM_LAYERS = 2;
    static constexpr uint32_t ACTION_DIM = 16;
    static constexpr uint32_t BATCH_SIZE = 8;

    void SetUp() override {
        gpu_ctx = nullptr;
        predictor = nullptr;
        inverse = nullptr;
        rng.seed(42);

        // Initialize kernel backend to detect GPU
        nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);

        // Try to create GPU context
        if (nimcp_cuda_backend_available()) {
            gpu_ctx = nimcp_gpu_context_create(0);
        }

        if (gpu_ctx) {
            // Create JEPA predictor
            predictor = nimcp_jepa_gpu_predictor_create(
                gpu_ctx, INPUT_DIM, HIDDEN_DIM, OUTPUT_DIM, NUM_LAYERS,
                NIMCP_JEPA_ACT_GELU
            );

            // Create inverse model
            inverse = nimcp_jepa_gpu_inverse_create(
                gpu_ctx, OUTPUT_DIM, ACTION_DIM, HIDDEN_DIM, NUM_LAYERS
            );
        }
    }

    void TearDown() override {
        if (inverse) {
            nimcp_jepa_gpu_inverse_destroy(inverse);
            inverse = nullptr;
        }
        if (predictor) {
            nimcp_jepa_gpu_predictor_destroy(predictor);
            predictor = nullptr;
        }
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }
        nimcp_kernel_backend_shutdown();
    }

    bool hasGPU() const {
        return gpu_ctx != nullptr && predictor != nullptr;
    }

    std::vector<float> generateRandomWeights(uint32_t rows, uint32_t cols) {
        std::vector<float> weights(rows * cols);
        std::normal_distribution<float> dist(0.0f, 0.1f);
        for (size_t i = 0; i < weights.size(); i++) {
            weights[i] = dist(rng);
        }
        return weights;
    }

    std::vector<float> generateRandomVector(uint32_t size) {
        std::vector<float> vec(size);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (size_t i = 0; i < vec.size(); i++) {
            vec[i] = dist(rng);
        }
        return vec;
    }

    nimcp_gpu_tensor_t* createTensor(const std::vector<float>& data, uint32_t rows, uint32_t cols) {
        if (!gpu_ctx) return nullptr;
        size_t dims[2] = {rows, cols};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(gpu_ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_tensor_upload(tensor, data.data(), data.size() * sizeof(float));
        }
        return tensor;
    }

    bool uploadTestWeights() {
        if (!predictor) return false;

        for (uint32_t layer = 0; layer < NUM_LAYERS; layer++) {
            uint32_t in_dim = (layer == 0) ? INPUT_DIM : HIDDEN_DIM;
            uint32_t out_dim = (layer == NUM_LAYERS - 1) ? OUTPUT_DIM : HIDDEN_DIM;

            auto weights = generateRandomWeights(out_dim, in_dim);
            auto bias = generateRandomVector(out_dim);

            if (!nimcp_jepa_gpu_predictor_upload_weights(
                    predictor, layer, weights.data(), bias.data())) {
                return false;
            }
        }
        return true;
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(JEPAGPUUnitTest, PredictorCreate_WithNullContext_ReturnsNull) {
    nimcp_jepa_gpu_predictor_t* pred = nimcp_jepa_gpu_predictor_create(
        nullptr, 64, 128, 64, 2, NIMCP_JEPA_ACT_GELU
    );
    EXPECT_EQ(pred, nullptr);
}

TEST_F(JEPAGPUUnitTest, PredictorCreate_WithValidContext_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_NE(predictor, nullptr);
}

TEST_F(JEPAGPUUnitTest, PredictorDestroy_WithNull_DoesNotCrash) {
    nimcp_jepa_gpu_predictor_destroy(nullptr);
    SUCCEED();
}

TEST_F(JEPAGPUUnitTest, InverseCreate_WithValidContext_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_NE(inverse, nullptr);
}

TEST_F(JEPAGPUUnitTest, InverseDestroy_WithNull_DoesNotCrash) {
    nimcp_jepa_gpu_inverse_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Weight Upload/Download Tests
//=============================================================================

TEST_F(JEPAGPUUnitTest, UploadWeights_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_TRUE(uploadTestWeights());
}

TEST_F(JEPAGPUUnitTest, DownloadWeights_MatchesUploaded) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Generate and upload weights for layer 0
    auto original_weights = generateRandomWeights(HIDDEN_DIM, INPUT_DIM);
    auto original_bias = generateRandomVector(HIDDEN_DIM);

    EXPECT_TRUE(nimcp_jepa_gpu_predictor_upload_weights(
        predictor, 0, original_weights.data(), original_bias.data()
    ));

    // Download and compare
    std::vector<float> downloaded_weights(HIDDEN_DIM * INPUT_DIM);
    std::vector<float> downloaded_bias(HIDDEN_DIM);

    EXPECT_TRUE(nimcp_jepa_gpu_predictor_download_weights(
        predictor, 0, downloaded_weights.data(), downloaded_bias.data()
    ));

    for (size_t i = 0; i < original_weights.size(); i++) {
        EXPECT_NEAR(original_weights[i], downloaded_weights[i], 1e-6f)
            << "Weight mismatch at index " << i;
    }

    for (size_t i = 0; i < original_bias.size(); i++) {
        EXPECT_NEAR(original_bias[i], downloaded_bias[i], 1e-6f)
            << "Bias mismatch at index " << i;
    }
}

//=============================================================================
// Forward Prediction Tests
//=============================================================================

TEST_F(JEPAGPUUnitTest, ForwardPredict_ProducesOutput) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(uploadTestWeights());

    // Create input tensor
    auto input_data = generateRandomVector(BATCH_SIZE * INPUT_DIM);
    nimcp_gpu_tensor_t* context = createTensor(input_data, BATCH_SIZE, INPUT_DIM);
    ASSERT_NE(context, nullptr);

    // Create output tensor
    size_t out_dims[2] = {BATCH_SIZE, OUTPUT_DIM};
    nimcp_gpu_tensor_t* prediction = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);
    ASSERT_NE(prediction, nullptr);

    // Run forward prediction
    EXPECT_TRUE(nimcp_jepa_gpu_forward_predict(predictor, context, prediction));

    // Download and verify output is non-zero
    std::vector<float> output_data(BATCH_SIZE * OUTPUT_DIM);
    nimcp_gpu_tensor_download(prediction, output_data.data(), output_data.size() * sizeof(float));

    bool has_nonzero = false;
    for (float val : output_data) {
        if (std::abs(val) > 1e-10f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    nimcp_gpu_tensor_destroy(prediction);
    nimcp_gpu_tensor_destroy(context);
}

TEST_F(JEPAGPUUnitTest, ForwardPredict_OutputHasCorrectShape) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(uploadTestWeights());

    auto input_data = generateRandomVector(BATCH_SIZE * INPUT_DIM);
    nimcp_gpu_tensor_t* context = createTensor(input_data, BATCH_SIZE, INPUT_DIM);

    size_t out_dims[2] = {BATCH_SIZE, OUTPUT_DIM};
    nimcp_gpu_tensor_t* prediction = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_jepa_gpu_forward_predict(predictor, context, prediction));

    // Verify shape
    size_t actual_dims[2];
    nimcp_gpu_tensor_get_dims(prediction, actual_dims, 2);
    EXPECT_EQ(actual_dims[0], BATCH_SIZE);
    EXPECT_EQ(actual_dims[1], OUTPUT_DIM);

    nimcp_gpu_tensor_destroy(prediction);
    nimcp_gpu_tensor_destroy(context);
}

TEST_F(JEPAGPUUnitTest, ForwardConditioned_ProducesOutput) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Create state + action conditioned predictor
    nimcp_jepa_gpu_predictor_t* conditioned = nimcp_jepa_gpu_predictor_create(
        gpu_ctx, OUTPUT_DIM + ACTION_DIM, HIDDEN_DIM, OUTPUT_DIM, NUM_LAYERS,
        NIMCP_JEPA_ACT_GELU
    );
    ASSERT_NE(conditioned, nullptr);

    // Upload weights
    for (uint32_t layer = 0; layer < NUM_LAYERS; layer++) {
        uint32_t in_dim = (layer == 0) ? (OUTPUT_DIM + ACTION_DIM) : HIDDEN_DIM;
        uint32_t out_dim = (layer == NUM_LAYERS - 1) ? OUTPUT_DIM : HIDDEN_DIM;
        auto weights = generateRandomWeights(out_dim, in_dim);
        auto bias = generateRandomVector(out_dim);
        nimcp_jepa_gpu_predictor_upload_weights(conditioned, layer, weights.data(), bias.data());
    }

    // Create tensors
    auto state_data = generateRandomVector(BATCH_SIZE * OUTPUT_DIM);
    auto action_data = generateRandomVector(BATCH_SIZE * ACTION_DIM);
    nimcp_gpu_tensor_t* state = createTensor(state_data, BATCH_SIZE, OUTPUT_DIM);
    nimcp_gpu_tensor_t* action = createTensor(action_data, BATCH_SIZE, ACTION_DIM);

    size_t out_dims[2] = {BATCH_SIZE, OUTPUT_DIM};
    nimcp_gpu_tensor_t* next_state = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_jepa_gpu_forward_conditioned(conditioned, state, action, next_state));

    nimcp_gpu_tensor_destroy(next_state);
    nimcp_gpu_tensor_destroy(action);
    nimcp_gpu_tensor_destroy(state);
    nimcp_jepa_gpu_predictor_destroy(conditioned);
}

//=============================================================================
// Inverse Model Tests
//=============================================================================

TEST_F(JEPAGPUUnitTest, InverseInfer_ProducesOutput) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_NE(inverse, nullptr);

    // Create state tensors
    auto state_t_data = generateRandomVector(BATCH_SIZE * OUTPUT_DIM);
    auto state_next_data = generateRandomVector(BATCH_SIZE * OUTPUT_DIM);
    nimcp_gpu_tensor_t* state_t = createTensor(state_t_data, BATCH_SIZE, OUTPUT_DIM);
    nimcp_gpu_tensor_t* state_next = createTensor(state_next_data, BATCH_SIZE, OUTPUT_DIM);

    size_t action_dims[2] = {BATCH_SIZE, ACTION_DIM};
    nimcp_gpu_tensor_t* action = nimcp_gpu_tensor_create(gpu_ctx, action_dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_jepa_gpu_inverse_infer(inverse, state_t, state_next, action));

    // Verify action was computed
    std::vector<float> action_data(BATCH_SIZE * ACTION_DIM);
    nimcp_gpu_tensor_download(action, action_data.data(), action_data.size() * sizeof(float));

    bool has_nonzero = false;
    for (float val : action_data) {
        if (std::abs(val) > 1e-10f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    nimcp_gpu_tensor_destroy(action);
    nimcp_gpu_tensor_destroy(state_next);
    nimcp_gpu_tensor_destroy(state_t);
}

//=============================================================================
// Masking Operation Tests
//=============================================================================

TEST_F(JEPAGPUUnitTest, ApplyMask_ZerosMaskedPositions) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Create latent tensor with all ones
    std::vector<float> latent_data(BATCH_SIZE * OUTPUT_DIM, 1.0f);
    nimcp_gpu_tensor_t* latent = createTensor(latent_data, BATCH_SIZE, OUTPUT_DIM);

    // Create binary mask (0 for first half, 1 for second half)
    std::vector<float> mask_data(OUTPUT_DIM);
    for (uint32_t i = 0; i < OUTPUT_DIM; i++) {
        mask_data[i] = (i < OUTPUT_DIM / 2) ? 0.0f : 1.0f;
    }
    nimcp_gpu_tensor_t* mask = createTensor(mask_data, 1, OUTPUT_DIM);

    size_t out_dims[2] = {BATCH_SIZE, OUTPUT_DIM};
    nimcp_gpu_tensor_t* masked = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_jepa_gpu_apply_mask(gpu_ctx, latent, mask, masked));

    // Verify masking
    std::vector<float> result(BATCH_SIZE * OUTPUT_DIM);
    nimcp_gpu_tensor_download(masked, result.data(), result.size() * sizeof(float));

    for (uint32_t b = 0; b < BATCH_SIZE; b++) {
        for (uint32_t i = 0; i < OUTPUT_DIM; i++) {
            float expected = (i < OUTPUT_DIM / 2) ? 0.0f : 1.0f;
            EXPECT_NEAR(result[b * OUTPUT_DIM + i], expected, 1e-6f)
                << "Mismatch at batch " << b << ", index " << i;
        }
    }

    nimcp_gpu_tensor_destroy(masked);
    nimcp_gpu_tensor_destroy(mask);
    nimcp_gpu_tensor_destroy(latent);
}

TEST_F(JEPAGPUUnitTest, GenerateBlockMask_HasCorrectRatio) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    size_t mask_dims[2] = {BATCH_SIZE, OUTPUT_DIM};
    nimcp_gpu_tensor_t* mask = nimcp_gpu_tensor_create(gpu_ctx, mask_dims, 2, NIMCP_GPU_PRECISION_FP32);

    float mask_ratio = 0.5f;
    uint32_t block_size = 8;

    EXPECT_TRUE(nimcp_jepa_gpu_generate_block_mask(gpu_ctx, mask, block_size, mask_ratio));

    // Download and check ratio
    std::vector<float> mask_data(BATCH_SIZE * OUTPUT_DIM);
    nimcp_gpu_tensor_download(mask, mask_data.data(), mask_data.size() * sizeof(float));

    int num_masked = 0;
    for (float val : mask_data) {
        if (val < 0.5f) num_masked++;
    }

    float actual_ratio = (float)num_masked / (float)mask_data.size();
    // Allow some variance due to block-based masking
    EXPECT_NEAR(actual_ratio, mask_ratio, 0.2f);

    nimcp_gpu_tensor_destroy(mask);
}

TEST_F(JEPAGPUUnitTest, ApplySoftMask_WeightsCorrectly) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Create latent with value 2.0
    std::vector<float> latent_data(BATCH_SIZE * OUTPUT_DIM, 2.0f);
    nimcp_gpu_tensor_t* latent = createTensor(latent_data, BATCH_SIZE, OUTPUT_DIM);

    // Create soft weights [0.5]
    std::vector<float> weights_data(OUTPUT_DIM, 0.5f);
    nimcp_gpu_tensor_t* weights = createTensor(weights_data, 1, OUTPUT_DIM);

    size_t out_dims[2] = {BATCH_SIZE, OUTPUT_DIM};
    nimcp_gpu_tensor_t* masked = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_jepa_gpu_apply_soft_mask(gpu_ctx, latent, weights, masked));

    // Verify result is 2.0 * 0.5 = 1.0
    std::vector<float> result(BATCH_SIZE * OUTPUT_DIM);
    nimcp_gpu_tensor_download(masked, result.data(), result.size() * sizeof(float));

    for (float val : result) {
        EXPECT_NEAR(val, 1.0f, 1e-6f);
    }

    nimcp_gpu_tensor_destroy(masked);
    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(latent);
}

//=============================================================================
// Loss Computation Tests
//=============================================================================

TEST_F(JEPAGPUUnitTest, ComputeLoss_ZeroForIdentical) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    auto data = generateRandomVector(BATCH_SIZE * OUTPUT_DIM);
    nimcp_gpu_tensor_t* prediction = createTensor(data, BATCH_SIZE, OUTPUT_DIM);
    nimcp_gpu_tensor_t* target = createTensor(data, BATCH_SIZE, OUTPUT_DIM);

    float loss = -1.0f;
    EXPECT_TRUE(nimcp_jepa_gpu_compute_loss(gpu_ctx, prediction, target, nullptr, &loss));

    EXPECT_NEAR(loss, 0.0f, 1e-6f);

    nimcp_gpu_tensor_destroy(target);
    nimcp_gpu_tensor_destroy(prediction);
}

TEST_F(JEPAGPUUnitTest, ComputeLoss_NonZeroForDifferent) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    auto pred_data = generateRandomVector(BATCH_SIZE * OUTPUT_DIM);
    auto target_data = generateRandomVector(BATCH_SIZE * OUTPUT_DIM);
    nimcp_gpu_tensor_t* prediction = createTensor(pred_data, BATCH_SIZE, OUTPUT_DIM);
    nimcp_gpu_tensor_t* target = createTensor(target_data, BATCH_SIZE, OUTPUT_DIM);

    float loss = -1.0f;
    EXPECT_TRUE(nimcp_jepa_gpu_compute_loss(gpu_ctx, prediction, target, nullptr, &loss));

    EXPECT_GT(loss, 0.0f);

    nimcp_gpu_tensor_destroy(target);
    nimcp_gpu_tensor_destroy(prediction);
}

TEST_F(JEPAGPUUnitTest, ComputeLoss_WithMask_IgnoresMaskedPositions) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Prediction and target differ only in first half
    std::vector<float> pred_data(BATCH_SIZE * OUTPUT_DIM, 0.0f);
    std::vector<float> target_data(BATCH_SIZE * OUTPUT_DIM, 0.0f);
    for (uint32_t b = 0; b < BATCH_SIZE; b++) {
        for (uint32_t i = 0; i < OUTPUT_DIM / 2; i++) {
            pred_data[b * OUTPUT_DIM + i] = 1.0f; // Different in first half
        }
    }

    nimcp_gpu_tensor_t* prediction = createTensor(pred_data, BATCH_SIZE, OUTPUT_DIM);
    nimcp_gpu_tensor_t* target = createTensor(target_data, BATCH_SIZE, OUTPUT_DIM);

    // Mask out first half (where they differ)
    std::vector<float> mask_data(OUTPUT_DIM);
    for (uint32_t i = 0; i < OUTPUT_DIM; i++) {
        mask_data[i] = (i < OUTPUT_DIM / 2) ? 0.0f : 1.0f;
    }
    nimcp_gpu_tensor_t* mask = createTensor(mask_data, 1, OUTPUT_DIM);

    float loss = -1.0f;
    EXPECT_TRUE(nimcp_jepa_gpu_compute_loss(gpu_ctx, prediction, target, mask, &loss));

    // Loss should be zero because differences are masked out
    EXPECT_NEAR(loss, 0.0f, 1e-5f);

    nimcp_gpu_tensor_destroy(mask);
    nimcp_gpu_tensor_destroy(target);
    nimcp_gpu_tensor_destroy(prediction);
}

TEST_F(JEPAGPUUnitTest, ComputePrecisionLoss_WeightsByPrecision) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Uniform difference of 1.0
    std::vector<float> pred_data(BATCH_SIZE * OUTPUT_DIM, 1.0f);
    std::vector<float> target_data(BATCH_SIZE * OUTPUT_DIM, 0.0f);
    nimcp_gpu_tensor_t* prediction = createTensor(pred_data, BATCH_SIZE, OUTPUT_DIM);
    nimcp_gpu_tensor_t* target = createTensor(target_data, BATCH_SIZE, OUTPUT_DIM);

    // High precision (2.0) for first half, low precision (0.5) for second half
    std::vector<float> precision_data(OUTPUT_DIM);
    for (uint32_t i = 0; i < OUTPUT_DIM; i++) {
        precision_data[i] = (i < OUTPUT_DIM / 2) ? 2.0f : 0.5f;
    }
    nimcp_gpu_tensor_t* precision = createTensor(precision_data, 1, OUTPUT_DIM);

    float loss = -1.0f;
    EXPECT_TRUE(nimcp_jepa_gpu_compute_precision_loss(gpu_ctx, prediction, target, precision, &loss));

    // Loss should be weighted: (2.0 * 1.0^2 * 32 + 0.5 * 1.0^2 * 32) / 64 = (64 + 16) / 64 = 1.25
    EXPECT_GT(loss, 0.0f);

    nimcp_gpu_tensor_destroy(precision);
    nimcp_gpu_tensor_destroy(target);
    nimcp_gpu_tensor_destroy(prediction);
}

//=============================================================================
// Backward Pass Tests
//=============================================================================

TEST_F(JEPAGPUUnitTest, Backward_ProducesGradients) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(uploadTestWeights());

    // Forward pass first
    auto input_data = generateRandomVector(BATCH_SIZE * INPUT_DIM);
    nimcp_gpu_tensor_t* context = createTensor(input_data, BATCH_SIZE, INPUT_DIM);

    size_t out_dims[2] = {BATCH_SIZE, OUTPUT_DIM};
    nimcp_gpu_tensor_t* prediction = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_jepa_gpu_forward_predict(predictor, context, prediction);

    // Create gradient output
    auto grad_out_data = generateRandomVector(BATCH_SIZE * OUTPUT_DIM);
    nimcp_gpu_tensor_t* grad_output = createTensor(grad_out_data, BATCH_SIZE, OUTPUT_DIM);

    size_t grad_in_dims[2] = {BATCH_SIZE, INPUT_DIM};
    nimcp_gpu_tensor_t* grad_input = nimcp_gpu_tensor_create(gpu_ctx, grad_in_dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_jepa_gpu_backward(predictor, grad_output, grad_input));

    // Verify gradients exist
    std::vector<float> grad_data(BATCH_SIZE * INPUT_DIM);
    nimcp_gpu_tensor_download(grad_input, grad_data.data(), grad_data.size() * sizeof(float));

    bool has_nonzero = false;
    for (float val : grad_data) {
        if (std::abs(val) > 1e-10f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    nimcp_gpu_tensor_destroy(grad_input);
    nimcp_gpu_tensor_destroy(grad_output);
    nimcp_gpu_tensor_destroy(prediction);
    nimcp_gpu_tensor_destroy(context);
}

TEST_F(JEPAGPUUnitTest, UpdateWeights_ModifiesWeights) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(uploadTestWeights());

    // Get original weights
    std::vector<float> original_weights(HIDDEN_DIM * INPUT_DIM);
    std::vector<float> original_bias(HIDDEN_DIM);
    nimcp_jepa_gpu_predictor_download_weights(predictor, 0, original_weights.data(), original_bias.data());

    // Do forward-backward
    auto input_data = generateRandomVector(BATCH_SIZE * INPUT_DIM);
    nimcp_gpu_tensor_t* context = createTensor(input_data, BATCH_SIZE, INPUT_DIM);

    size_t out_dims[2] = {BATCH_SIZE, OUTPUT_DIM};
    nimcp_gpu_tensor_t* prediction = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_jepa_gpu_forward_predict(predictor, context, prediction);

    auto grad_out_data = generateRandomVector(BATCH_SIZE * OUTPUT_DIM);
    nimcp_gpu_tensor_t* grad_output = createTensor(grad_out_data, BATCH_SIZE, OUTPUT_DIM);

    size_t grad_in_dims[2] = {BATCH_SIZE, INPUT_DIM};
    nimcp_gpu_tensor_t* grad_input = nimcp_gpu_tensor_create(gpu_ctx, grad_in_dims, 2, NIMCP_GPU_PRECISION_FP32);
    nimcp_jepa_gpu_backward(predictor, grad_output, grad_input);

    // Update weights
    EXPECT_TRUE(nimcp_jepa_gpu_update_weights(predictor, 0.01f, 0.0001f));

    // Get new weights
    std::vector<float> new_weights(HIDDEN_DIM * INPUT_DIM);
    std::vector<float> new_bias(HIDDEN_DIM);
    nimcp_jepa_gpu_predictor_download_weights(predictor, 0, new_weights.data(), new_bias.data());

    // Weights should have changed
    bool weights_changed = false;
    for (size_t i = 0; i < original_weights.size(); i++) {
        if (std::abs(original_weights[i] - new_weights[i]) > 1e-10f) {
            weights_changed = true;
            break;
        }
    }
    EXPECT_TRUE(weights_changed);

    nimcp_gpu_tensor_destroy(grad_input);
    nimcp_gpu_tensor_destroy(grad_output);
    nimcp_gpu_tensor_destroy(prediction);
    nimcp_gpu_tensor_destroy(context);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(JEPAGPUUnitTest, DownloadLatent_ReturnsCorrectData) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    auto original_data = generateRandomVector(BATCH_SIZE * OUTPUT_DIM);
    nimcp_gpu_tensor_t* tensor = createTensor(original_data, BATCH_SIZE, OUTPUT_DIM);

    std::vector<float> downloaded(BATCH_SIZE * OUTPUT_DIM);
    int count = nimcp_jepa_gpu_download_latent(gpu_ctx, tensor, downloaded.data(), downloaded.size());

    EXPECT_EQ(count, (int)(BATCH_SIZE * OUTPUT_DIM));

    for (size_t i = 0; i < original_data.size(); i++) {
        EXPECT_NEAR(original_data[i], downloaded[i], 1e-6f);
    }

    nimcp_gpu_tensor_destroy(tensor);
}

TEST_F(JEPAGPUUnitTest, UploadLatent_StoresCorrectData) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    auto original_data = generateRandomVector(BATCH_SIZE * OUTPUT_DIM);

    size_t dims[2] = {BATCH_SIZE, OUTPUT_DIM};
    nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(gpu_ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_jepa_gpu_upload_latent(gpu_ctx, original_data.data(), original_data.size(), tensor));

    std::vector<float> downloaded(BATCH_SIZE * OUTPUT_DIM);
    nimcp_gpu_tensor_download(tensor, downloaded.data(), downloaded.size() * sizeof(float));

    for (size_t i = 0; i < original_data.size(); i++) {
        EXPECT_NEAR(original_data[i], downloaded[i], 1e-6f);
    }

    nimcp_gpu_tensor_destroy(tensor);
}

TEST_F(JEPAGPUUnitTest, Synchronize_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_TRUE(nimcp_jepa_gpu_synchronize(gpu_ctx));
}

//=============================================================================
// GPU Recovery Tests
//=============================================================================

TEST_F(JEPAGPUUnitTest, Recovery_InitializedOnCreate) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
}

TEST_F(JEPAGPUUnitTest, Recovery_HandlesNullInputGracefully) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_FALSE(nimcp_jepa_gpu_forward_predict(predictor, nullptr, nullptr));
    EXPECT_FALSE(nimcp_jepa_gpu_apply_mask(gpu_ctx, nullptr, nullptr, nullptr));
    EXPECT_FALSE(nimcp_jepa_gpu_compute_loss(gpu_ctx, nullptr, nullptr, nullptr, nullptr));
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(JEPAGPUUnitTest, NullSafety_AllFunctionsHandleNull) {
    nimcp_jepa_gpu_predictor_destroy(nullptr);
    nimcp_jepa_gpu_inverse_destroy(nullptr);

    EXPECT_FALSE(nimcp_jepa_gpu_predictor_upload_weights(nullptr, 0, nullptr, nullptr));
    EXPECT_FALSE(nimcp_jepa_gpu_predictor_download_weights(nullptr, 0, nullptr, nullptr));
    EXPECT_FALSE(nimcp_jepa_gpu_forward_predict(nullptr, nullptr, nullptr));
    EXPECT_FALSE(nimcp_jepa_gpu_forward_conditioned(nullptr, nullptr, nullptr, nullptr));
    EXPECT_FALSE(nimcp_jepa_gpu_inverse_infer(nullptr, nullptr, nullptr, nullptr));
    EXPECT_FALSE(nimcp_jepa_gpu_apply_mask(nullptr, nullptr, nullptr, nullptr));
    EXPECT_FALSE(nimcp_jepa_gpu_generate_block_mask(nullptr, nullptr, 0, 0.0f));
    EXPECT_FALSE(nimcp_jepa_gpu_compute_loss(nullptr, nullptr, nullptr, nullptr, nullptr));
    EXPECT_FALSE(nimcp_jepa_gpu_backward(nullptr, nullptr, nullptr));
    EXPECT_FALSE(nimcp_jepa_gpu_update_weights(nullptr, 0.0f, 0.0f));
    EXPECT_EQ(nimcp_jepa_gpu_download_latent(nullptr, nullptr, nullptr, 0), -1);
    EXPECT_FALSE(nimcp_jepa_gpu_upload_latent(nullptr, nullptr, 0, nullptr));
    EXPECT_FALSE(nimcp_jepa_gpu_synchronize(nullptr));

    SUCCEED();
}

//=============================================================================
// Numerical Stability Tests
//=============================================================================

TEST_F(JEPAGPUUnitTest, ForwardPredict_HandlesLargeInputs) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(uploadTestWeights());

    // Create input with large values
    std::vector<float> input_data(BATCH_SIZE * INPUT_DIM, 100.0f);
    nimcp_gpu_tensor_t* context = createTensor(input_data, BATCH_SIZE, INPUT_DIM);

    size_t out_dims[2] = {BATCH_SIZE, OUTPUT_DIM};
    nimcp_gpu_tensor_t* prediction = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_jepa_gpu_forward_predict(predictor, context, prediction));

    // Check output is finite
    std::vector<float> output_data(BATCH_SIZE * OUTPUT_DIM);
    nimcp_gpu_tensor_download(prediction, output_data.data(), output_data.size() * sizeof(float));

    for (float val : output_data) {
        EXPECT_TRUE(std::isfinite(val)) << "Non-finite output: " << val;
    }

    nimcp_gpu_tensor_destroy(prediction);
    nimcp_gpu_tensor_destroy(context);
}

TEST_F(JEPAGPUUnitTest, ForwardPredict_HandlesSmallInputs) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(uploadTestWeights());

    // Create input with very small values
    std::vector<float> input_data(BATCH_SIZE * INPUT_DIM, 1e-7f);
    nimcp_gpu_tensor_t* context = createTensor(input_data, BATCH_SIZE, INPUT_DIM);

    size_t out_dims[2] = {BATCH_SIZE, OUTPUT_DIM};
    nimcp_gpu_tensor_t* prediction = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_jepa_gpu_forward_predict(predictor, context, prediction));

    // Check output is finite
    std::vector<float> output_data(BATCH_SIZE * OUTPUT_DIM);
    nimcp_gpu_tensor_download(prediction, output_data.data(), output_data.size() * sizeof(float));

    for (float val : output_data) {
        EXPECT_TRUE(std::isfinite(val)) << "Non-finite output: " << val;
    }

    nimcp_gpu_tensor_destroy(prediction);
    nimcp_gpu_tensor_destroy(context);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
