/**
 * @file test_omni_gpu.cpp
 * @brief Unit tests for GPU-accelerated Omnidirectional Inference
 *
 * WHAT: Unit tests for GPU Omni kernels and API
 * WHY:  Verify correctness of GPU-accelerated bidirectional prediction, Hopfield, hierarchy, replay
 * HOW:  Test individual operations: prediction, retrieval, hierarchy propagation, replay sampling
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
#include "gpu/cognitive/nimcp_omni_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Fixture
//=============================================================================

class OmniGPUUnitTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx;
    nimcp_omni_gpu_state_t* omni_state;
    std::mt19937 rng;

    // Test dimensions
    static constexpr uint32_t LATENT_DIM = 128;
    static constexpr uint32_t HIDDEN_DIM = 256;
    static constexpr uint32_t NUM_PATTERNS = 100;
    static constexpr uint32_t NUM_LEVELS = 4;
    static constexpr uint32_t BATCH_SIZE = 8;
    static constexpr uint32_t SEQUENCE_LEN = 16;
    static constexpr uint32_t REPLAY_CAPACITY = 50;

    void SetUp() override {
        gpu_ctx = nullptr;
        omni_state = nullptr;
        rng.seed(42);

        // Initialize kernel backend to detect GPU
        nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);

        // Try to create GPU context
        if (nimcp_cuda_backend_available()) {
            gpu_ctx = nimcp_gpu_context_create(0);
        }

        if (gpu_ctx) {
            omni_state = nimcp_omni_gpu_create(
                gpu_ctx, LATENT_DIM, HIDDEN_DIM, NUM_PATTERNS, NUM_LEVELS
            );
        }
    }

    void TearDown() override {
        if (omni_state) {
            nimcp_omni_gpu_destroy(omni_state);
            omni_state = nullptr;
        }
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }
        nimcp_kernel_backend_shutdown();
    }

    bool hasGPU() const {
        return gpu_ctx != nullptr && omni_state != nullptr;
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

    nimcp_gpu_tensor_t* createTensor1D(const std::vector<float>& data) {
        if (!gpu_ctx) return nullptr;
        size_t dims[1] = {data.size()};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(gpu_ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_tensor_upload(tensor, data.data(), data.size() * sizeof(float));
        }
        return tensor;
    }

    bool uploadBidirectionalWeights() {
        if (!omni_state) return false;

        auto forward_weights = generateRandomVector(LATENT_DIM * LATENT_DIM);
        auto forward_bias = generateRandomVector(LATENT_DIM);

        return nimcp_omni_gpu_upload_weights(
            omni_state, NIMCP_OMNI_GPU_DIR_FORWARD,
            forward_weights.data(), forward_bias.data()
        );
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(OmniGPUUnitTest, Create_WithNullContext_ReturnsNull) {
    nimcp_omni_gpu_state_t* state = nimcp_omni_gpu_create(nullptr, 128, 256, 100, 4);
    EXPECT_EQ(state, nullptr);
}

TEST_F(OmniGPUUnitTest, Create_WithValidContext_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_NE(omni_state, nullptr);
    EXPECT_TRUE(nimcp_omni_gpu_is_valid(omni_state));
}

TEST_F(OmniGPUUnitTest, Destroy_WithNull_DoesNotCrash) {
    nimcp_omni_gpu_destroy(nullptr);
    SUCCEED();
}

TEST_F(OmniGPUUnitTest, IsValid_WithNull_ReturnsFalse) {
    EXPECT_FALSE(nimcp_omni_gpu_is_valid(nullptr));
}

//=============================================================================
// Bidirectional Prediction Tests
//=============================================================================

TEST_F(OmniGPUUnitTest, Predict_Forward_ProducesOutput) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(uploadBidirectionalWeights());

    auto input_data = generateRandomVector(BATCH_SIZE * LATENT_DIM);
    nimcp_gpu_tensor_t* input = createTensor(input_data, BATCH_SIZE, LATENT_DIM);

    size_t out_dims[2] = {BATCH_SIZE, LATENT_DIM};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_omni_gpu_predict(omni_state, input, output, NIMCP_OMNI_GPU_DIR_FORWARD));

    // Verify output is non-zero
    std::vector<float> output_data(BATCH_SIZE * LATENT_DIM);
    nimcp_gpu_tensor_download(output, output_data.data(), output_data.size() * sizeof(float));

    bool has_nonzero = false;
    for (float val : output_data) {
        if (std::abs(val) > 1e-10f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    nimcp_gpu_tensor_destroy(output);
    nimcp_gpu_tensor_destroy(input);
}

TEST_F(OmniGPUUnitTest, Predict_AllDirections_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Upload weights for all directions
    for (int dir = 0; dir < NIMCP_OMNI_GPU_DIR_COUNT; dir++) {
        auto weights = generateRandomVector(LATENT_DIM * LATENT_DIM);
        auto bias = generateRandomVector(LATENT_DIM);
        nimcp_omni_gpu_upload_weights(omni_state, (nimcp_omni_gpu_direction_t)dir,
                                       weights.data(), bias.data());
    }

    auto input_data = generateRandomVector(BATCH_SIZE * LATENT_DIM);
    nimcp_gpu_tensor_t* input = createTensor(input_data, BATCH_SIZE, LATENT_DIM);

    size_t out_dims[2] = {BATCH_SIZE, LATENT_DIM};

    // Test each direction
    nimcp_omni_gpu_direction_t directions[] = {
        NIMCP_OMNI_GPU_DIR_FORWARD,
        NIMCP_OMNI_GPU_DIR_BACKWARD,
        NIMCP_OMNI_GPU_DIR_LATERAL,
        NIMCP_OMNI_GPU_DIR_HIER_UP,
        NIMCP_OMNI_GPU_DIR_HIER_DOWN
    };

    for (auto dir : directions) {
        nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);
        EXPECT_TRUE(nimcp_omni_gpu_predict(omni_state, input, output, dir))
            << "Failed for direction: " << nimcp_omni_gpu_direction_to_string(dir);
        nimcp_gpu_tensor_destroy(output);
    }

    nimcp_gpu_tensor_destroy(input);
}

TEST_F(OmniGPUUnitTest, PredictMulti_ProcessesAllDirections) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Upload weights
    for (int dir = 0; dir < 3; dir++) {
        auto weights = generateRandomVector(LATENT_DIM * LATENT_DIM);
        auto bias = generateRandomVector(LATENT_DIM);
        nimcp_omni_gpu_upload_weights(omni_state, (nimcp_omni_gpu_direction_t)dir,
                                       weights.data(), bias.data());
    }

    auto input_data = generateRandomVector(BATCH_SIZE * LATENT_DIM);
    nimcp_gpu_tensor_t* input = createTensor(input_data, BATCH_SIZE, LATENT_DIM);

    nimcp_omni_gpu_direction_t directions[] = {
        NIMCP_OMNI_GPU_DIR_FORWARD,
        NIMCP_OMNI_GPU_DIR_BACKWARD,
        NIMCP_OMNI_GPU_DIR_LATERAL
    };

    size_t out_dims[2] = {BATCH_SIZE, LATENT_DIM};
    nimcp_gpu_tensor_t* outputs[3];
    for (int i = 0; i < 3; i++) {
        outputs[i] = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);
    }

    EXPECT_TRUE(nimcp_omni_gpu_predict_multi(omni_state, input, outputs, directions, 3));

    // Verify outputs are different (different direction weights)
    std::vector<float> out0(BATCH_SIZE * LATENT_DIM);
    std::vector<float> out1(BATCH_SIZE * LATENT_DIM);
    nimcp_gpu_tensor_download(outputs[0], out0.data(), out0.size() * sizeof(float));
    nimcp_gpu_tensor_download(outputs[1], out1.data(), out1.size() * sizeof(float));

    bool outputs_differ = false;
    for (size_t i = 0; i < out0.size(); i++) {
        if (std::abs(out0[i] - out1[i]) > 1e-6f) {
            outputs_differ = true;
            break;
        }
    }
    EXPECT_TRUE(outputs_differ);

    for (int i = 0; i < 3; i++) {
        nimcp_gpu_tensor_destroy(outputs[i]);
    }
    nimcp_gpu_tensor_destroy(input);
}

TEST_F(OmniGPUUnitTest, PredictPrecision_WeightsByPrecision) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(uploadBidirectionalWeights());

    auto input_data = generateRandomVector(BATCH_SIZE * LATENT_DIM);
    nimcp_gpu_tensor_t* input = createTensor(input_data, BATCH_SIZE, LATENT_DIM);

    // Create precision weights (higher for first half)
    std::vector<float> precision_data(LATENT_DIM);
    for (uint32_t i = 0; i < LATENT_DIM; i++) {
        precision_data[i] = (i < LATENT_DIM / 2) ? 2.0f : 0.5f;
    }
    nimcp_gpu_tensor_t* precision = createTensor1D(precision_data);

    size_t out_dims[2] = {BATCH_SIZE, LATENT_DIM};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_omni_gpu_predict_precision(
        omni_state, input, precision, output, NIMCP_OMNI_GPU_DIR_FORWARD
    ));

    nimcp_gpu_tensor_destroy(output);
    nimcp_gpu_tensor_destroy(precision);
    nimcp_gpu_tensor_destroy(input);
}

//=============================================================================
// Hopfield Memory Tests
//=============================================================================

TEST_F(OmniGPUUnitTest, HopfieldInit_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_TRUE(nimcp_omni_gpu_hopfield_init(
        omni_state, LATENT_DIM, NUM_PATTERNS, 1.0f, NIMCP_HOPFIELD_GPU_SOFTMAX
    ));
}

TEST_F(OmniGPUUnitTest, HopfieldStore_ReturnsIndex) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(nimcp_omni_gpu_hopfield_init(
        omni_state, LATENT_DIM, NUM_PATTERNS, 1.0f, NIMCP_HOPFIELD_GPU_SOFTMAX
    ));

    auto pattern_data = generateRandomVector(LATENT_DIM);
    nimcp_gpu_tensor_t* pattern = createTensor1D(pattern_data);

    int idx = nimcp_omni_gpu_hopfield_store(omni_state, pattern);
    EXPECT_GE(idx, 0);

    nimcp_gpu_tensor_destroy(pattern);
}

TEST_F(OmniGPUUnitTest, HopfieldStoreBatch_StoresMultiple) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(nimcp_omni_gpu_hopfield_init(
        omni_state, LATENT_DIM, NUM_PATTERNS, 1.0f, NIMCP_HOPFIELD_GPU_SOFTMAX
    ));

    uint32_t num_to_store = 10;
    auto patterns_data = generateRandomVector(num_to_store * LATENT_DIM);
    nimcp_gpu_tensor_t* patterns = createTensor(patterns_data, num_to_store, LATENT_DIM);

    int stored = nimcp_omni_gpu_hopfield_store_batch(omni_state, patterns, num_to_store);
    EXPECT_EQ(stored, (int)num_to_store);

    nimcp_gpu_tensor_destroy(patterns);
}

TEST_F(OmniGPUUnitTest, HopfieldRetrieve_ReturnsStoredPattern) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(nimcp_omni_gpu_hopfield_init(
        omni_state, LATENT_DIM, NUM_PATTERNS, 10.0f, NIMCP_HOPFIELD_GPU_SOFTMAX
    ));

    // Store a pattern
    auto pattern_data = generateRandomVector(LATENT_DIM);
    nimcp_gpu_tensor_t* stored_pattern = createTensor1D(pattern_data);
    nimcp_omni_gpu_hopfield_store(omni_state, stored_pattern);

    // Add noise to create query
    std::vector<float> query_data = pattern_data;
    std::normal_distribution<float> noise(0.0f, 0.1f);
    for (float& val : query_data) {
        val += noise(rng);
    }
    nimcp_gpu_tensor_t* query = createTensor(query_data, 1, LATENT_DIM);

    size_t out_dims[2] = {1, LATENT_DIM};
    nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(gpu_ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_omni_gpu_hopfield_retrieve(omni_state, query, output, 10));

    // Retrieved pattern should be similar to original
    std::vector<float> retrieved(LATENT_DIM);
    nimcp_gpu_tensor_download(output, retrieved.data(), retrieved.size() * sizeof(float));

    float correlation = 0.0f;
    float norm1 = 0.0f, norm2 = 0.0f;
    for (uint32_t i = 0; i < LATENT_DIM; i++) {
        correlation += pattern_data[i] * retrieved[i];
        norm1 += pattern_data[i] * pattern_data[i];
        norm2 += retrieved[i] * retrieved[i];
    }
    correlation /= (std::sqrt(norm1) * std::sqrt(norm2) + 1e-10f);

    EXPECT_GT(correlation, 0.8f); // Should be highly correlated

    nimcp_gpu_tensor_destroy(output);
    nimcp_gpu_tensor_destroy(query);
    nimcp_gpu_tensor_destroy(stored_pattern);
}

TEST_F(OmniGPUUnitTest, HopfieldTopK_ReturnsOrderedResults) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(nimcp_omni_gpu_hopfield_init(
        omni_state, LATENT_DIM, NUM_PATTERNS, 1.0f, NIMCP_HOPFIELD_GPU_SOFTMAX
    ));

    // Store multiple patterns
    uint32_t num_patterns = 10;
    auto patterns_data = generateRandomVector(num_patterns * LATENT_DIM);
    nimcp_gpu_tensor_t* patterns = createTensor(patterns_data, num_patterns, LATENT_DIM);
    nimcp_omni_gpu_hopfield_store_batch(omni_state, patterns, num_patterns);

    // Query with first pattern
    std::vector<float> query_data(patterns_data.begin(), patterns_data.begin() + LATENT_DIM);
    nimcp_gpu_tensor_t* query = createTensor1D(query_data);

    uint32_t k = 5;
    std::vector<uint32_t> indices(k);
    std::vector<float> similarities(k);

    EXPECT_TRUE(nimcp_omni_gpu_hopfield_top_k(omni_state, query, k, indices.data(), similarities.data()));

    // First result should be most similar (index 0)
    EXPECT_EQ(indices[0], 0u);

    // Similarities should be in descending order
    for (uint32_t i = 1; i < k; i++) {
        EXPECT_GE(similarities[i-1], similarities[i]);
    }

    nimcp_gpu_tensor_destroy(query);
    nimcp_gpu_tensor_destroy(patterns);
}

TEST_F(OmniGPUUnitTest, HopfieldEnergy_ComputesValue) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(nimcp_omni_gpu_hopfield_init(
        omni_state, LATENT_DIM, NUM_PATTERNS, 1.0f, NIMCP_HOPFIELD_GPU_SOFTMAX
    ));

    // Store patterns
    auto patterns_data = generateRandomVector(5 * LATENT_DIM);
    nimcp_gpu_tensor_t* patterns = createTensor(patterns_data, 5, LATENT_DIM);
    nimcp_omni_gpu_hopfield_store_batch(omni_state, patterns, 5);

    // Compute energy for first stored pattern (should be low)
    std::vector<float> pattern_data(patterns_data.begin(), patterns_data.begin() + LATENT_DIM);
    nimcp_gpu_tensor_t* pattern = createTensor1D(pattern_data);

    float energy = 999.0f;
    EXPECT_TRUE(nimcp_omni_gpu_hopfield_energy(omni_state, pattern, &energy));
    EXPECT_TRUE(std::isfinite(energy));

    nimcp_gpu_tensor_destroy(pattern);
    nimcp_gpu_tensor_destroy(patterns);
}

//=============================================================================
// Predictive Hierarchy Tests
//=============================================================================

TEST_F(OmniGPUUnitTest, HierarchyInit_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint32_t level_dims[] = {LATENT_DIM, LATENT_DIM/2, LATENT_DIM/4, LATENT_DIM/8};

    EXPECT_TRUE(nimcp_omni_gpu_hierarchy_init(
        omni_state, level_dims, NUM_LEVELS, NIMCP_PRECISION_GPU_ADAPTIVE
    ));
}

TEST_F(OmniGPUUnitTest, HierarchyForward_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint32_t level_dims[] = {LATENT_DIM, LATENT_DIM/2, LATENT_DIM/4, LATENT_DIM/8};
    ASSERT_TRUE(nimcp_omni_gpu_hierarchy_init(
        omni_state, level_dims, NUM_LEVELS, NIMCP_PRECISION_GPU_ADAPTIVE
    ));

    // Upload hierarchy weights
    for (uint32_t level = 0; level < NUM_LEVELS - 1; level++) {
        auto up_weights = generateRandomVector(level_dims[level] * level_dims[level+1]);
        auto down_weights = generateRandomVector(level_dims[level+1] * level_dims[level]);
        nimcp_omni_gpu_upload_hierarchy_weights(
            omni_state, level, up_weights.data(), down_weights.data()
        );
    }

    auto input_data = generateRandomVector(BATCH_SIZE * LATENT_DIM);
    nimcp_gpu_tensor_t* input = createTensor(input_data, BATCH_SIZE, LATENT_DIM);

    EXPECT_TRUE(nimcp_omni_gpu_hierarchy_forward(omni_state, input));

    nimcp_gpu_tensor_destroy(input);
}

TEST_F(OmniGPUUnitTest, HierarchyBackward_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint32_t level_dims[] = {LATENT_DIM, LATENT_DIM/2, LATENT_DIM/4, LATENT_DIM/8};
    ASSERT_TRUE(nimcp_omni_gpu_hierarchy_init(
        omni_state, level_dims, NUM_LEVELS, NIMCP_PRECISION_GPU_FIXED
    ));

    // Upload weights and do forward first
    for (uint32_t level = 0; level < NUM_LEVELS - 1; level++) {
        auto up_weights = generateRandomVector(level_dims[level] * level_dims[level+1]);
        auto down_weights = generateRandomVector(level_dims[level+1] * level_dims[level]);
        nimcp_omni_gpu_upload_hierarchy_weights(
            omni_state, level, up_weights.data(), down_weights.data()
        );
    }

    auto input_data = generateRandomVector(BATCH_SIZE * LATENT_DIM);
    nimcp_gpu_tensor_t* input = createTensor(input_data, BATCH_SIZE, LATENT_DIM);
    nimcp_omni_gpu_hierarchy_forward(omni_state, input);

    // Now backward
    EXPECT_TRUE(nimcp_omni_gpu_hierarchy_backward(omni_state));

    nimcp_gpu_tensor_destroy(input);
}

TEST_F(OmniGPUUnitTest, HierarchyComputeErrors_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint32_t level_dims[] = {LATENT_DIM, LATENT_DIM/2, LATENT_DIM/4, LATENT_DIM/8};
    ASSERT_TRUE(nimcp_omni_gpu_hierarchy_init(
        omni_state, level_dims, NUM_LEVELS, NIMCP_PRECISION_GPU_FIXED
    ));

    // Setup and run forward/backward
    for (uint32_t level = 0; level < NUM_LEVELS - 1; level++) {
        auto up_weights = generateRandomVector(level_dims[level] * level_dims[level+1]);
        auto down_weights = generateRandomVector(level_dims[level+1] * level_dims[level]);
        nimcp_omni_gpu_upload_hierarchy_weights(
            omni_state, level, up_weights.data(), down_weights.data()
        );
    }

    auto input_data = generateRandomVector(BATCH_SIZE * LATENT_DIM);
    nimcp_gpu_tensor_t* input = createTensor(input_data, BATCH_SIZE, LATENT_DIM);
    nimcp_omni_gpu_hierarchy_forward(omni_state, input);
    nimcp_omni_gpu_hierarchy_backward(omni_state);

    EXPECT_TRUE(nimcp_omni_gpu_hierarchy_compute_errors(omni_state));

    nimcp_gpu_tensor_destroy(input);
}

TEST_F(OmniGPUUnitTest, HierarchyFreeEnergy_ComputesValue) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint32_t level_dims[] = {LATENT_DIM, LATENT_DIM/2, LATENT_DIM/4, LATENT_DIM/8};
    ASSERT_TRUE(nimcp_omni_gpu_hierarchy_init(
        omni_state, level_dims, NUM_LEVELS, NIMCP_PRECISION_GPU_FIXED
    ));

    for (uint32_t level = 0; level < NUM_LEVELS - 1; level++) {
        auto up_weights = generateRandomVector(level_dims[level] * level_dims[level+1]);
        auto down_weights = generateRandomVector(level_dims[level+1] * level_dims[level]);
        nimcp_omni_gpu_upload_hierarchy_weights(
            omni_state, level, up_weights.data(), down_weights.data()
        );
    }

    auto input_data = generateRandomVector(BATCH_SIZE * LATENT_DIM);
    nimcp_gpu_tensor_t* input = createTensor(input_data, BATCH_SIZE, LATENT_DIM);
    nimcp_omni_gpu_hierarchy_forward(omni_state, input);
    nimcp_omni_gpu_hierarchy_backward(omni_state);
    nimcp_omni_gpu_hierarchy_compute_errors(omni_state);

    float free_energy = -1.0f;
    EXPECT_TRUE(nimcp_omni_gpu_hierarchy_free_energy(omni_state, &free_energy));
    EXPECT_GE(free_energy, 0.0f);

    nimcp_gpu_tensor_destroy(input);
}

TEST_F(OmniGPUUnitTest, HierarchyGetLevel_ReturnsState) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint32_t level_dims[] = {LATENT_DIM, LATENT_DIM/2, LATENT_DIM/4, LATENT_DIM/8};
    ASSERT_TRUE(nimcp_omni_gpu_hierarchy_init(
        omni_state, level_dims, NUM_LEVELS, NIMCP_PRECISION_GPU_FIXED
    ));

    for (uint32_t level = 0; level < NUM_LEVELS - 1; level++) {
        auto up_weights = generateRandomVector(level_dims[level] * level_dims[level+1]);
        auto down_weights = generateRandomVector(level_dims[level+1] * level_dims[level]);
        nimcp_omni_gpu_upload_hierarchy_weights(
            omni_state, level, up_weights.data(), down_weights.data()
        );
    }

    auto input_data = generateRandomVector(BATCH_SIZE * LATENT_DIM);
    nimcp_gpu_tensor_t* input = createTensor(input_data, BATCH_SIZE, LATENT_DIM);
    nimcp_omni_gpu_hierarchy_forward(omni_state, input);

    size_t level_out_dims[2] = {BATCH_SIZE, level_dims[1]};
    nimcp_gpu_tensor_t* level_state = nimcp_gpu_tensor_create(gpu_ctx, level_out_dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_omni_gpu_hierarchy_get_level(omni_state, 1, level_state));

    nimcp_gpu_tensor_destroy(level_state);
    nimcp_gpu_tensor_destroy(input);
}

//=============================================================================
// Temporal Replay Tests
//=============================================================================

TEST_F(OmniGPUUnitTest, ReplayInit_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_TRUE(nimcp_omni_gpu_replay_init(
        omni_state, LATENT_DIM, REPLAY_CAPACITY, SEQUENCE_LEN
    ));
}

TEST_F(OmniGPUUnitTest, ReplayStore_ReturnsIndex) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(nimcp_omni_gpu_replay_init(
        omni_state, LATENT_DIM, REPLAY_CAPACITY, SEQUENCE_LEN
    ));

    auto sequence_data = generateRandomVector(SEQUENCE_LEN * LATENT_DIM);
    nimcp_gpu_tensor_t* sequence = createTensor(sequence_data, SEQUENCE_LEN, LATENT_DIM);

    int idx = nimcp_omni_gpu_replay_store(omni_state, sequence, 1.0f);
    EXPECT_GE(idx, 0);

    nimcp_gpu_tensor_destroy(sequence);
}

TEST_F(OmniGPUUnitTest, ReplaySample_ReturnsSequences) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(nimcp_omni_gpu_replay_init(
        omni_state, LATENT_DIM, REPLAY_CAPACITY, SEQUENCE_LEN
    ));

    // Store multiple sequences
    for (int i = 0; i < 10; i++) {
        auto sequence_data = generateRandomVector(SEQUENCE_LEN * LATENT_DIM);
        nimcp_gpu_tensor_t* sequence = createTensor(sequence_data, SEQUENCE_LEN, LATENT_DIM);
        nimcp_omni_gpu_replay_store(omni_state, sequence, 1.0f + 0.1f * i);
        nimcp_gpu_tensor_destroy(sequence);
    }

    // Sample
    uint32_t sample_batch = 4;
    size_t sample_dims[3] = {sample_batch, SEQUENCE_LEN, LATENT_DIM};
    nimcp_gpu_tensor_t* sampled = nimcp_gpu_tensor_create(gpu_ctx, sample_dims, 3, NIMCP_GPU_PRECISION_FP32);

    std::vector<uint32_t> indices(sample_batch);

    EXPECT_TRUE(nimcp_omni_gpu_replay_sample(
        omni_state, sample_batch, NIMCP_REPLAY_GPU_PRIORITY, sampled, indices.data()
    ));

    nimcp_gpu_tensor_destroy(sampled);
}

TEST_F(OmniGPUUnitTest, ReplayForwardSweep_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(nimcp_omni_gpu_replay_init(
        omni_state, LATENT_DIM, REPLAY_CAPACITY, SEQUENCE_LEN
    ));

    // Store a sequence
    auto sequence_data = generateRandomVector(SEQUENCE_LEN * LATENT_DIM);
    nimcp_gpu_tensor_t* sequence = createTensor(sequence_data, SEQUENCE_LEN, LATENT_DIM);
    int idx = nimcp_omni_gpu_replay_store(omni_state, sequence, 1.0f);

    // Forward sweep
    uint32_t sweep_len = 8;
    size_t sweep_dims[2] = {sweep_len, LATENT_DIM};
    nimcp_gpu_tensor_t* sweep_output = nimcp_gpu_tensor_create(gpu_ctx, sweep_dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_omni_gpu_replay_forward_sweep(
        omni_state, idx, 0, sweep_len, sweep_output
    ));

    nimcp_gpu_tensor_destroy(sweep_output);
    nimcp_gpu_tensor_destroy(sequence);
}

TEST_F(OmniGPUUnitTest, ReplayBackwardSweep_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(nimcp_omni_gpu_replay_init(
        omni_state, LATENT_DIM, REPLAY_CAPACITY, SEQUENCE_LEN
    ));

    auto sequence_data = generateRandomVector(SEQUENCE_LEN * LATENT_DIM);
    nimcp_gpu_tensor_t* sequence = createTensor(sequence_data, SEQUENCE_LEN, LATENT_DIM);
    int idx = nimcp_omni_gpu_replay_store(omni_state, sequence, 1.0f);

    uint32_t sweep_len = 8;
    size_t sweep_dims[2] = {sweep_len, LATENT_DIM};
    nimcp_gpu_tensor_t* sweep_output = nimcp_gpu_tensor_create(gpu_ctx, sweep_dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_omni_gpu_replay_backward_sweep(
        omni_state, idx, SEQUENCE_LEN - 1, sweep_len, sweep_output
    ));

    nimcp_gpu_tensor_destroy(sweep_output);
    nimcp_gpu_tensor_destroy(sequence);
}

TEST_F(OmniGPUUnitTest, ReplayUpdatePriorities_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    ASSERT_TRUE(nimcp_omni_gpu_replay_init(
        omni_state, LATENT_DIM, REPLAY_CAPACITY, SEQUENCE_LEN
    ));

    // Store sequences
    std::vector<int> stored_indices;
    for (int i = 0; i < 5; i++) {
        auto sequence_data = generateRandomVector(SEQUENCE_LEN * LATENT_DIM);
        nimcp_gpu_tensor_t* sequence = createTensor(sequence_data, SEQUENCE_LEN, LATENT_DIM);
        stored_indices.push_back(nimcp_omni_gpu_replay_store(omni_state, sequence, 1.0f));
        nimcp_gpu_tensor_destroy(sequence);
    }

    // Update priorities
    std::vector<uint32_t> indices(stored_indices.begin(), stored_indices.end());
    std::vector<float> new_priorities = {2.0f, 1.5f, 1.0f, 0.5f, 0.1f};

    EXPECT_TRUE(nimcp_omni_gpu_replay_update_priorities(
        omni_state, indices.data(), new_priorities.data(), 5
    ));
}

//=============================================================================
// Free Energy Computation Tests
//=============================================================================

TEST_F(OmniGPUUnitTest, ComputeFreeEnergy_ReturnsValue) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    auto error_data = generateRandomVector(BATCH_SIZE * LATENT_DIM);
    std::vector<float> precision_data(LATENT_DIM, 1.0f);

    nimcp_gpu_tensor_t* prediction_error = createTensor(error_data, BATCH_SIZE, LATENT_DIM);
    nimcp_gpu_tensor_t* precision = createTensor1D(precision_data);

    float total_fe = -1.0f;
    EXPECT_TRUE(nimcp_omni_gpu_compute_free_energy(
        omni_state, prediction_error, precision, &total_fe
    ));

    EXPECT_GE(total_fe, 0.0f);

    nimcp_gpu_tensor_destroy(precision);
    nimcp_gpu_tensor_destroy(prediction_error);
}

TEST_F(OmniGPUUnitTest, ComputeFEGradient_ReturnsGradient) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    auto error_data = generateRandomVector(BATCH_SIZE * LATENT_DIM);
    std::vector<float> precision_data(LATENT_DIM, 1.0f);

    nimcp_gpu_tensor_t* prediction_error = createTensor(error_data, BATCH_SIZE, LATENT_DIM);
    nimcp_gpu_tensor_t* precision = createTensor1D(precision_data);

    size_t grad_dims[2] = {BATCH_SIZE, LATENT_DIM};
    nimcp_gpu_tensor_t* gradient = nimcp_gpu_tensor_create(gpu_ctx, grad_dims, 2, NIMCP_GPU_PRECISION_FP32);

    EXPECT_TRUE(nimcp_omni_gpu_compute_fe_gradient(
        omni_state, prediction_error, precision, gradient
    ));

    // Verify gradient is non-zero
    std::vector<float> grad_data(BATCH_SIZE * LATENT_DIM);
    nimcp_gpu_tensor_download(gradient, grad_data.data(), grad_data.size() * sizeof(float));

    bool has_nonzero = false;
    for (float val : grad_data) {
        if (std::abs(val) > 1e-10f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    nimcp_gpu_tensor_destroy(gradient);
    nimcp_gpu_tensor_destroy(precision);
    nimcp_gpu_tensor_destroy(prediction_error);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(OmniGPUUnitTest, Synchronize_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_TRUE(nimcp_omni_gpu_synchronize(omni_state));
}

TEST_F(OmniGPUUnitTest, MemoryUsage_ReturnsValues) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    size_t allocated = 0, peak = 0;
    nimcp_omni_gpu_memory_usage(omni_state, &allocated, &peak);

    // Should have some memory allocated
    EXPECT_GT(allocated, 0u);
    EXPECT_GE(peak, allocated);
}

TEST_F(OmniGPUUnitTest, DirectionToString_ReturnsValidString) {
    const char* str = nimcp_omni_gpu_direction_to_string(NIMCP_OMNI_GPU_DIR_FORWARD);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);
}

TEST_F(OmniGPUUnitTest, HopfieldModeToString_ReturnsValidString) {
    const char* str = nimcp_hopfield_gpu_mode_to_string(NIMCP_HOPFIELD_GPU_SOFTMAX);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);
}

TEST_F(OmniGPUUnitTest, ReplayModeToString_ReturnsValidString) {
    const char* str = nimcp_replay_gpu_mode_to_string(NIMCP_REPLAY_GPU_PRIORITY);
    EXPECT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);
}

//=============================================================================
// GPU Recovery Tests
//=============================================================================

TEST_F(OmniGPUUnitTest, Recovery_InitializedOnCreate) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
}

TEST_F(OmniGPUUnitTest, Recovery_HandlesNullInputGracefully) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_FALSE(nimcp_omni_gpu_predict(omni_state, nullptr, nullptr, NIMCP_OMNI_GPU_DIR_FORWARD));
    EXPECT_FALSE(nimcp_omni_gpu_hopfield_retrieve(omni_state, nullptr, nullptr, 10));
    EXPECT_FALSE(nimcp_omni_gpu_hierarchy_forward(omni_state, nullptr));
    EXPECT_FALSE(nimcp_omni_gpu_compute_free_energy(omni_state, nullptr, nullptr, nullptr));
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(OmniGPUUnitTest, NullSafety_AllFunctionsHandleNull) {
    nimcp_omni_gpu_destroy(nullptr);
    EXPECT_FALSE(nimcp_omni_gpu_is_valid(nullptr));
    EXPECT_FALSE(nimcp_omni_gpu_predict(nullptr, nullptr, nullptr, NIMCP_OMNI_GPU_DIR_FORWARD));
    EXPECT_FALSE(nimcp_omni_gpu_predict_multi(nullptr, nullptr, nullptr, nullptr, 0));
    EXPECT_FALSE(nimcp_omni_gpu_hopfield_init(nullptr, 0, 0, 0.0f, NIMCP_HOPFIELD_GPU_SOFTMAX));
    EXPECT_EQ(nimcp_omni_gpu_hopfield_store(nullptr, nullptr), -1);
    EXPECT_FALSE(nimcp_omni_gpu_hopfield_retrieve(nullptr, nullptr, nullptr, 0));
    EXPECT_FALSE(nimcp_omni_gpu_hierarchy_init(nullptr, nullptr, 0, NIMCP_PRECISION_GPU_FIXED));
    EXPECT_FALSE(nimcp_omni_gpu_hierarchy_forward(nullptr, nullptr));
    EXPECT_FALSE(nimcp_omni_gpu_replay_init(nullptr, 0, 0, 0));
    EXPECT_EQ(nimcp_omni_gpu_replay_store(nullptr, nullptr, 0.0f), -1);
    EXPECT_FALSE(nimcp_omni_gpu_replay_sample(nullptr, 0, NIMCP_REPLAY_GPU_FORWARD, nullptr, nullptr));
    EXPECT_FALSE(nimcp_omni_gpu_compute_free_energy(nullptr, nullptr, nullptr, nullptr));
    EXPECT_FALSE(nimcp_omni_gpu_synchronize(nullptr));

    SUCCEED();
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
