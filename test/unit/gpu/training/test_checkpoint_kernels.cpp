//=============================================================================
// test_checkpoint_kernels.cpp - Unit Tests for Checkpointing GPU Kernels
//=============================================================================
/**
 * @file test_checkpoint_kernels.cpp
 * @brief GTest unit tests for gradient/activation checkpointing with recovery
 *
 * WHAT: Tests checkpointing systems for memory-efficient training
 * WHY:  Ensure checkpointing works correctly and recovers from errors
 * HOW:  Uses GTest framework with checkpoint context operations
 *
 * TEST CATEGORIES:
 * - Checkpoint context lifecycle (create, destroy, reset)
 * - Checkpoint strategy configuration (SQRT, EVERY_N, SELECTIVE, MEMORY_BUDGET)
 * - Activation storage and retrieval
 * - Sequential checkpoint forward/backward
 * - Memory estimation and statistics
 * - Recovery scenarios
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>

extern "C" {
#include "gpu/training/nimcp_gradient_checkpoint.h"
#include "gpu/training/nimcp_activation_checkpoint.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class CheckpointKernelsTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;

    void SetUp() override {
        // Initialize GPU recovery system
        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }

        // Create GPU context
        gpu_ctx = nimcp_gpu_context_create(0);
        if (!gpu_ctx) {
            GTEST_SKIP() << "GPU context creation failed - skipping GPU tests";
        }
    }

    void TearDown() override {
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }
    }

    // Helper to create test tensor with specific data
    nimcp_gpu_tensor_t* create_test_tensor(const std::vector<size_t>& dims,
                                           const std::vector<float>& data) {
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(
            gpu_ctx, dims.data(), dims.size(), NIMCP_DTYPE_FLOAT32);
        if (tensor && !data.empty()) {
            nimcp_gpu_tensor_copy_from_host(gpu_ctx, tensor, data.data(),
                                            data.size() * sizeof(float));
        }
        return tensor;
    }

    // Helper to create test tensor with random data
    nimcp_gpu_tensor_t* create_random_tensor(const std::vector<size_t>& dims,
                                              float min_val = -1.0f,
                                              float max_val = 1.0f) {
        size_t total_elements = 1;
        for (size_t d : dims) total_elements *= d;

        std::vector<float> data(total_elements);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(min_val, max_val);
        for (auto& v : data) v = dist(gen);

        return create_test_tensor(dims, data);
    }

    // Helper to get tensor data from GPU
    std::vector<float> get_tensor_data(nimcp_gpu_tensor_t* tensor) {
        size_t total_elements = 1;
        for (uint32_t i = 0; i < tensor->ndim; i++) {
            total_elements *= tensor->dims[i];
        }
        std::vector<float> data(total_elements);
        nimcp_gpu_tensor_copy_to_host(gpu_ctx, tensor, data.data(),
                                      data.size() * sizeof(float));
        return data;
    }
};

//=============================================================================
// Checkpoint Context Lifecycle Tests
//=============================================================================

TEST_F(CheckpointKernelsTest, CreateDestroyCheckpointContext) {
    int total_layers = 12;
    size_t memory_budget = 1024 * 1024 * 100;  // 100 MB

    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, total_layers, memory_budget);

    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->total_layers, total_layers);
    EXPECT_EQ(ctx->memory_budget, memory_budget);
    EXPECT_EQ(ctx->strategy, CKPT_STRATEGY_SQRT);

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointKernelsTest, CreateContextDifferentStrategies) {
    int total_layers = 8;

    // Test NONE strategy
    nimcp_checkpoint_ctx_t* ctx_none = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_NONE, total_layers, 0);
    ASSERT_NE(ctx_none, nullptr);
    EXPECT_EQ(ctx_none->strategy, CKPT_STRATEGY_NONE);
    nimcp_checkpoint_ctx_destroy(ctx_none);

    // Test SQRT strategy
    nimcp_checkpoint_ctx_t* ctx_sqrt = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, total_layers, 0);
    ASSERT_NE(ctx_sqrt, nullptr);
    EXPECT_EQ(ctx_sqrt->strategy, CKPT_STRATEGY_SQRT);
    nimcp_checkpoint_ctx_destroy(ctx_sqrt);

    // Test EVERY_N strategy
    nimcp_checkpoint_ctx_t* ctx_every_n = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_EVERY_N, total_layers, 0);
    ASSERT_NE(ctx_every_n, nullptr);
    EXPECT_EQ(ctx_every_n->strategy, CKPT_STRATEGY_EVERY_N);
    nimcp_checkpoint_ctx_destroy(ctx_every_n);

    // Test SELECTIVE strategy
    nimcp_checkpoint_ctx_t* ctx_selective = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SELECTIVE, total_layers, 0);
    ASSERT_NE(ctx_selective, nullptr);
    EXPECT_EQ(ctx_selective->strategy, CKPT_STRATEGY_SELECTIVE);
    nimcp_checkpoint_ctx_destroy(ctx_selective);

    // Test MEMORY_BUDGET strategy
    nimcp_checkpoint_ctx_t* ctx_memory = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_MEMORY_BUDGET, total_layers, 1024 * 1024 * 50);
    ASSERT_NE(ctx_memory, nullptr);
    EXPECT_EQ(ctx_memory->strategy, CKPT_STRATEGY_MEMORY_BUDGET);
    nimcp_checkpoint_ctx_destroy(ctx_memory);
}

TEST_F(CheckpointKernelsTest, DestroyNullContext) {
    // Should not crash
    nimcp_checkpoint_ctx_destroy(nullptr);
    SUCCEED();
}

TEST_F(CheckpointKernelsTest, ResetCheckpointContext) {
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, 10, 0);
    ASSERT_NE(ctx, nullptr);

    bool result = nimcp_checkpoint_ctx_reset(ctx);
    EXPECT_TRUE(result);

    nimcp_checkpoint_ctx_destroy(ctx);
}

//=============================================================================
// Checkpoint Configuration Tests
//=============================================================================

TEST_F(CheckpointKernelsTest, ConfigureEveryNStrategy) {
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_NONE, 12, 0);
    ASSERT_NE(ctx, nullptr);

    bool result = nimcp_checkpoint_configure(ctx, CKPT_STRATEGY_EVERY_N, 3);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx->strategy, CKPT_STRATEGY_EVERY_N);
    EXPECT_EQ(ctx->checkpoint_every_n, 3);

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointKernelsTest, ConfigureSqrtStrategy) {
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_NONE, 16, 0);
    ASSERT_NE(ctx, nullptr);

    bool result = nimcp_checkpoint_configure(ctx, CKPT_STRATEGY_SQRT, 0);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx->strategy, CKPT_STRATEGY_SQRT);

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointKernelsTest, ConfigureSelectiveStrategy) {
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_NONE, 10, 0);
    ASSERT_NE(ctx, nullptr);

    int layer_indices[] = {0, 3, 6, 9};
    bool result = nimcp_checkpoint_configure_selective(ctx, layer_indices, 4);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx->strategy, CKPT_STRATEGY_SELECTIVE);
    EXPECT_EQ(ctx->num_selective_layers, 4);

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointKernelsTest, SetLayerActivationSize) {
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, 8, 0);
    ASSERT_NE(ctx, nullptr);

    for (int i = 0; i < 8; i++) {
        size_t size = (i + 1) * 1024 * 1024;  // Increasing sizes
        bool result = nimcp_checkpoint_set_layer_size(ctx, i, size);
        EXPECT_TRUE(result);
    }

    nimcp_checkpoint_ctx_destroy(ctx);
}

//=============================================================================
// Checkpoint Operations Tests
//=============================================================================

TEST_F(CheckpointKernelsTest, ShouldSaveCheckpoint) {
    // Test EVERY_N strategy
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_EVERY_N, 12, 0);
    ASSERT_NE(ctx, nullptr);
    nimcp_checkpoint_configure(ctx, CKPT_STRATEGY_EVERY_N, 3);

    // Layers 0, 3, 6, 9 should be checkpointed
    EXPECT_TRUE(nimcp_checkpoint_should_save(ctx, 0));
    EXPECT_FALSE(nimcp_checkpoint_should_save(ctx, 1));
    EXPECT_FALSE(nimcp_checkpoint_should_save(ctx, 2));
    EXPECT_TRUE(nimcp_checkpoint_should_save(ctx, 3));
    EXPECT_FALSE(nimcp_checkpoint_should_save(ctx, 4));
    EXPECT_FALSE(nimcp_checkpoint_should_save(ctx, 5));
    EXPECT_TRUE(nimcp_checkpoint_should_save(ctx, 6));

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointKernelsTest, IsCheckpointLayer) {
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_EVERY_N, 10, 0);
    ASSERT_NE(ctx, nullptr);
    nimcp_checkpoint_configure(ctx, CKPT_STRATEGY_EVERY_N, 2);

    // Layers 0, 2, 4, 6, 8 should be checkpoint layers
    EXPECT_TRUE(nimcp_checkpoint_is_checkpoint_layer(ctx, 0));
    EXPECT_FALSE(nimcp_checkpoint_is_checkpoint_layer(ctx, 1));
    EXPECT_TRUE(nimcp_checkpoint_is_checkpoint_layer(ctx, 2));
    EXPECT_FALSE(nimcp_checkpoint_is_checkpoint_layer(ctx, 3));
    EXPECT_TRUE(nimcp_checkpoint_is_checkpoint_layer(ctx, 4));

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointKernelsTest, MarkLayerActivation) {
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_EVERY_N, 6, 0);
    ASSERT_NE(ctx, nullptr);
    nimcp_checkpoint_configure(ctx, CKPT_STRATEGY_EVERY_N, 2);

    nimcp_checkpoint_begin_forward(ctx);

    // Mark layers with activations
    nimcp_gpu_tensor_t* act0 = create_random_tensor({4, 64});
    nimcp_gpu_tensor_t* act1 = create_random_tensor({4, 64});
    nimcp_gpu_tensor_t* act2 = create_random_tensor({4, 64});

    EXPECT_TRUE(nimcp_checkpoint_mark_layer(ctx, 0, act0));
    EXPECT_TRUE(nimcp_checkpoint_mark_layer(ctx, 1, act1));
    EXPECT_TRUE(nimcp_checkpoint_mark_layer(ctx, 2, act2));

    nimcp_checkpoint_end_forward(ctx);

    nimcp_gpu_tensor_destroy(gpu_ctx, act0);
    nimcp_gpu_tensor_destroy(gpu_ctx, act1);
    nimcp_gpu_tensor_destroy(gpu_ctx, act2);
    nimcp_checkpoint_ctx_destroy(ctx);
}

//=============================================================================
// Forward/Backward Pass Control Tests
//=============================================================================

TEST_F(CheckpointKernelsTest, ForwardBackwardPassControl) {
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, 8, 0);
    ASSERT_NE(ctx, nullptr);

    // Begin forward pass
    EXPECT_TRUE(nimcp_checkpoint_begin_forward(ctx));
    EXPECT_TRUE(ctx->in_forward_pass);
    EXPECT_FALSE(ctx->in_backward_pass);

    // End forward pass
    EXPECT_TRUE(nimcp_checkpoint_end_forward(ctx));
    EXPECT_FALSE(ctx->in_forward_pass);

    // Begin backward pass
    EXPECT_TRUE(nimcp_checkpoint_begin_backward(ctx));
    EXPECT_FALSE(ctx->in_forward_pass);
    EXPECT_TRUE(ctx->in_backward_pass);

    // End backward pass
    EXPECT_TRUE(nimcp_checkpoint_end_backward(ctx));
    EXPECT_FALSE(ctx->in_backward_pass);

    nimcp_checkpoint_ctx_destroy(ctx);
}

//=============================================================================
// Memory Estimation Tests
//=============================================================================

TEST_F(CheckpointKernelsTest, EstimateMemoryRequirements) {
    int num_layers = 12;
    size_t layer_sizes[12];

    // Set uniform layer sizes (10 MB each)
    for (int i = 0; i < num_layers; i++) {
        layer_sizes[i] = 10 * 1024 * 1024;  // 10 MB
    }

    nimcp_checkpoint_estimate_t estimate;
    bool result = nimcp_checkpoint_estimate(layer_sizes, num_layers, &estimate);
    EXPECT_TRUE(result);

    // Without checkpointing: 12 * 10MB = 120 MB
    EXPECT_EQ(estimate.no_checkpoint_memory, 120 * 1024 * 1024);

    // With sqrt checkpointing: approximately sqrt(12) ~ 3-4 checkpoints
    // Memory should be less than without checkpointing
    EXPECT_LT(estimate.sqrt_checkpoint_memory, estimate.no_checkpoint_memory);

    // Compute overhead should be >= 1.0 for sqrt strategy
    EXPECT_GE(estimate.sqrt_recompute_overhead, 1.0);
}

TEST_F(CheckpointKernelsTest, RecommendStrategy) {
    int num_layers = 16;
    size_t layer_sizes[16];

    // Set layer sizes (8 MB each)
    for (int i = 0; i < num_layers; i++) {
        layer_sizes[i] = 8 * 1024 * 1024;
    }

    nimcp_checkpoint_strategy_t strategy;
    int checkpoint_n;

    // With tight budget, should recommend aggressive checkpointing
    size_t tight_budget = 32 * 1024 * 1024;  // 32 MB
    bool result = nimcp_checkpoint_recommend_strategy(
        layer_sizes, num_layers, tight_budget, &strategy, &checkpoint_n);
    EXPECT_TRUE(result);

    // With large budget, may recommend no checkpointing
    size_t large_budget = 256 * 1024 * 1024;  // 256 MB
    result = nimcp_checkpoint_recommend_strategy(
        layer_sizes, num_layers, large_budget, &strategy, &checkpoint_n);
    EXPECT_TRUE(result);
}

TEST_F(CheckpointKernelsTest, SqrtInterval) {
    // Test sqrt interval calculation
    EXPECT_EQ(nimcp_checkpoint_sqrt_interval(4), 2);    // sqrt(4) = 2
    EXPECT_EQ(nimcp_checkpoint_sqrt_interval(9), 3);    // sqrt(9) = 3
    EXPECT_EQ(nimcp_checkpoint_sqrt_interval(16), 4);   // sqrt(16) = 4
    EXPECT_EQ(nimcp_checkpoint_sqrt_interval(25), 5);   // sqrt(25) = 5

    // Non-perfect squares should round appropriately
    int interval_10 = nimcp_checkpoint_sqrt_interval(10);
    EXPECT_TRUE(interval_10 == 3 || interval_10 == 4);  // sqrt(10) ~ 3.16
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(CheckpointKernelsTest, GetCheckpointStatistics) {
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, 8, 0);
    ASSERT_NE(ctx, nullptr);

    size_t current_memory = 0;
    size_t peak_memory = 0;
    size_t saved_memory = 0;
    int recompute_count = 0;
    double recompute_time_ms = 0.0;

    nimcp_checkpoint_get_stats(ctx, &current_memory, &peak_memory,
                               &saved_memory, &recompute_count, &recompute_time_ms);

    // Initial stats should be zero or minimal
    EXPECT_EQ(recompute_count, 0);
    EXPECT_GE(recompute_time_ms, 0.0);

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointKernelsTest, ProfilingEnable) {
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, 8, 0);
    ASSERT_NE(ctx, nullptr);

    nimcp_checkpoint_set_profiling(ctx, true);
    EXPECT_TRUE(ctx->enable_profiling);

    nimcp_checkpoint_set_profiling(ctx, false);
    EXPECT_FALSE(ctx->enable_profiling);

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointKernelsTest, VerboseLogging) {
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, 8, 0);
    ASSERT_NE(ctx, nullptr);

    nimcp_checkpoint_set_verbose(ctx, true);
    EXPECT_TRUE(ctx->verbose);

    nimcp_checkpoint_set_verbose(ctx, false);
    EXPECT_FALSE(ctx->verbose);

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointKernelsTest, GetInfoString) {
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, 8, 0);
    ASSERT_NE(ctx, nullptr);

    char buffer[1024];
    int len = nimcp_checkpoint_get_info_string(ctx, buffer, sizeof(buffer));

    EXPECT_GT(len, 0);
    EXPECT_LT(len, (int)sizeof(buffer));

    // Buffer should contain strategy info
    std::string info(buffer);
    EXPECT_FALSE(info.empty());

    nimcp_checkpoint_ctx_destroy(ctx);
}

//=============================================================================
// Sequential Checkpoint Tests
//=============================================================================

TEST_F(CheckpointKernelsTest, CreateDestroySequentialCheckpoint) {
    int num_layers = 6;
    size_t memory_budget = 50 * 1024 * 1024;  // 50 MB

    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, num_layers, memory_budget);

    ASSERT_NE(ckpt, nullptr);
    EXPECT_EQ(ckpt->num_layers, num_layers);

    nimcp_sequential_checkpoint_destroy(ckpt);
}

TEST_F(CheckpointKernelsTest, CreateSequentialWithConfig) {
    nimcp_seq_checkpoint_config_t config;
    nimcp_seq_checkpoint_config_init(&config);

    config.strategy = CKPT_STRATEGY_EVERY_N;
    config.checkpoint_every_n = 2;
    config.memory_budget = 100 * 1024 * 1024;
    config.preserve_rng = true;
    config.enable_profiling = true;
    config.verbose = false;

    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create_with_config(
        gpu_ctx, 8, &config);

    ASSERT_NE(ckpt, nullptr);
    EXPECT_EQ(ckpt->num_layers, 8);
    EXPECT_EQ(ckpt->config.strategy, CKPT_STRATEGY_EVERY_N);
    EXPECT_EQ(ckpt->config.checkpoint_every_n, 2);
    EXPECT_TRUE(ckpt->config.preserve_rng);
    EXPECT_TRUE(ckpt->config.enable_profiling);

    nimcp_sequential_checkpoint_destroy(ckpt);
}

TEST_F(CheckpointKernelsTest, SequentialCheckpointReset) {
    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, 6, 0);
    ASSERT_NE(ckpt, nullptr);

    bool result = nimcp_sequential_checkpoint_reset(ckpt);
    EXPECT_TRUE(result);

    nimcp_sequential_checkpoint_destroy(ckpt);
}

TEST_F(CheckpointKernelsTest, SetLayerContext) {
    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, 4, 0);
    ASSERT_NE(ckpt, nullptr);

    // Set some dummy layer contexts
    int ctx_data[4] = {10, 20, 30, 40};
    for (int i = 0; i < 4; i++) {
        bool result = nimcp_sequential_checkpoint_set_layer_ctx(ckpt, i, &ctx_data[i]);
        EXPECT_TRUE(result);
    }

    nimcp_sequential_checkpoint_destroy(ckpt);
}

TEST_F(CheckpointKernelsTest, SetLayerOutputSize) {
    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, 6, 0);
    ASSERT_NE(ckpt, nullptr);

    for (int i = 0; i < 6; i++) {
        size_t size = (i + 1) * 1024 * 1024;
        bool result = nimcp_sequential_checkpoint_set_layer_size(ckpt, i, size);
        EXPECT_TRUE(result);
    }

    nimcp_sequential_checkpoint_destroy(ckpt);
}

TEST_F(CheckpointKernelsTest, ConfigureSequentialCheckpoint) {
    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, 8, 0);
    ASSERT_NE(ckpt, nullptr);

    bool result = nimcp_sequential_checkpoint_configure(ckpt, CKPT_STRATEGY_EVERY_N, 3);
    EXPECT_TRUE(result);
    EXPECT_EQ(ckpt->config.strategy, CKPT_STRATEGY_EVERY_N);
    EXPECT_EQ(ckpt->config.checkpoint_every_n, 3);

    nimcp_sequential_checkpoint_destroy(ckpt);
}

TEST_F(CheckpointKernelsTest, SequentialCheckpointActivationStorage) {
    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, 4, 0);
    ASSERT_NE(ckpt, nullptr);

    // Initially no activations stored
    for (int i = 0; i < 4; i++) {
        EXPECT_FALSE(nimcp_sequential_checkpoint_has_activation(ckpt, i));
    }

    // Store activation at layer 0
    nimcp_gpu_tensor_t* activation = create_random_tensor({4, 64});
    bool result = nimcp_sequential_checkpoint_store_activation(ckpt, 0, activation);
    EXPECT_TRUE(result);
    EXPECT_TRUE(nimcp_sequential_checkpoint_has_activation(ckpt, 0));

    // Free the stored activation
    result = nimcp_sequential_checkpoint_free_activation(ckpt, 0);
    EXPECT_TRUE(result);
    EXPECT_FALSE(nimcp_sequential_checkpoint_has_activation(ckpt, 0));

    nimcp_gpu_tensor_destroy(gpu_ctx, activation);
    nimcp_sequential_checkpoint_destroy(ckpt);
}

TEST_F(CheckpointKernelsTest, SequentialCheckpointStatistics) {
    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, 6, 0);
    ASSERT_NE(ckpt, nullptr);

    size_t total_memory = 0;
    size_t saved_memory = 0;
    int recompute_count = 0;
    double recompute_time_ms = 0.0;

    nimcp_sequential_checkpoint_get_stats(ckpt, &total_memory, &saved_memory,
                                          &recompute_count, &recompute_time_ms);

    // Initial stats
    EXPECT_GE(total_memory, 0u);
    EXPECT_GE(saved_memory, 0u);
    EXPECT_EQ(recompute_count, 0);
    EXPECT_GE(recompute_time_ms, 0.0);

    nimcp_sequential_checkpoint_destroy(ckpt);
}

TEST_F(CheckpointKernelsTest, SequentialCheckpointInfoString) {
    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, 6, 0);
    ASSERT_NE(ckpt, nullptr);

    char buffer[1024];
    int len = nimcp_sequential_checkpoint_get_info_string(ckpt, buffer, sizeof(buffer));

    EXPECT_GT(len, 0);
    EXPECT_LT(len, (int)sizeof(buffer));

    nimcp_sequential_checkpoint_destroy(ckpt);
}

//=============================================================================
// Sequential Checkpoint Memory Estimation Tests
//=============================================================================

TEST_F(CheckpointKernelsTest, SequentialEstimateMemory) {
    int num_layers = 8;
    size_t layer_sizes[8];

    // Set uniform layer sizes (5 MB each)
    for (int i = 0; i < num_layers; i++) {
        layer_sizes[i] = 5 * 1024 * 1024;
    }

    size_t no_ckpt_memory = 0;
    size_t sqrt_ckpt_memory = 0;
    int optimal_n = 0;

    bool result = nimcp_sequential_checkpoint_estimate_memory(
        num_layers, layer_sizes, &no_ckpt_memory, &sqrt_ckpt_memory, &optimal_n);

    EXPECT_TRUE(result);
    // Without checkpointing: 8 * 5MB = 40 MB
    EXPECT_EQ(no_ckpt_memory, 40 * 1024 * 1024);
    // With sqrt: should be less
    EXPECT_LT(sqrt_ckpt_memory, no_ckpt_memory);
}

TEST_F(CheckpointKernelsTest, SequentialRecommendStrategy) {
    int num_layers = 12;
    size_t layer_sizes[12];

    for (int i = 0; i < num_layers; i++) {
        layer_sizes[i] = 4 * 1024 * 1024;  // 4 MB each
    }

    nimcp_checkpoint_strategy_t strategy;
    int checkpoint_n;

    // Request with moderate budget
    size_t memory_budget = 24 * 1024 * 1024;  // 24 MB (about half)

    bool result = nimcp_sequential_checkpoint_recommend(
        num_layers, layer_sizes, memory_budget, &strategy, &checkpoint_n);

    EXPECT_TRUE(result);
    // Should recommend some form of checkpointing
    EXPECT_NE(strategy, CKPT_STRATEGY_NONE);
}

//=============================================================================
// Checkpointed Function Tests
//=============================================================================

// Dummy forward function for testing
static void test_forward_fn(void* ctx, nimcp_gpu_tensor_t* input, nimcp_gpu_tensor_t* output) {
    // Simple pass-through (in real use, would compute layer forward)
    (void)ctx;
    (void)input;
    (void)output;
}

// Dummy backward function for testing
static void test_backward_fn(void* ctx, nimcp_gpu_tensor_t* grad_output, nimcp_gpu_tensor_t* grad_input) {
    // Simple pass-through (in real use, would compute layer backward)
    (void)ctx;
    (void)grad_output;
    (void)grad_input;
}

TEST_F(CheckpointKernelsTest, CreateDestroyCheckpointedFunction) {
    nimcp_checkpointed_fn_t* fn = nimcp_checkpointed_fn_create(
        test_forward_fn,
        test_backward_fn,
        nullptr,
        false
    );

    ASSERT_NE(fn, nullptr);
    EXPECT_EQ(fn->forward, test_forward_fn);
    EXPECT_EQ(fn->backward, test_backward_fn);
    EXPECT_FALSE(fn->preserve_rng);

    nimcp_checkpointed_fn_destroy(fn);
}

TEST_F(CheckpointKernelsTest, CreateCheckpointedFunctionWithRNG) {
    nimcp_checkpointed_fn_t* fn = nimcp_checkpointed_fn_create(
        test_forward_fn,
        test_backward_fn,
        nullptr,
        true  // preserve RNG
    );

    ASSERT_NE(fn, nullptr);
    EXPECT_TRUE(fn->preserve_rng);

    nimcp_checkpointed_fn_destroy(fn);
}

//=============================================================================
// Transformer Checkpoint Tests
//=============================================================================

TEST_F(CheckpointKernelsTest, CreateDestroyTransformerCheckpoint) {
    int num_blocks = 12;
    size_t memory_budget = 256 * 1024 * 1024;  // 256 MB

    nimcp_transformer_checkpoint_t* ckpt = nimcp_transformer_checkpoint_create(
        gpu_ctx, num_blocks, memory_budget);

    ASSERT_NE(ckpt, nullptr);
    EXPECT_EQ(ckpt->num_blocks, num_blocks);

    nimcp_transformer_checkpoint_destroy(ckpt);
}

TEST_F(CheckpointKernelsTest, ConfigureTransformerBlock) {
    nimcp_transformer_checkpoint_t* ckpt = nimcp_transformer_checkpoint_create(
        gpu_ctx, 6, 0);
    ASSERT_NE(ckpt, nullptr);

    nimcp_transformer_block_t block = {
        .num_heads = 8,
        .embed_dim = 512,
        .ffn_dim = 2048,
        .pre_norm = true,
        .dropout_p = 0.1f
    };

    for (int i = 0; i < 6; i++) {
        bool result = nimcp_transformer_checkpoint_configure_block(ckpt, i, &block);
        EXPECT_TRUE(result);
    }

    nimcp_transformer_checkpoint_destroy(ckpt);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(CheckpointKernelsTest, CreateContextNullGPU) {
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        nullptr, CKPT_STRATEGY_SQRT, 8, 0);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(CheckpointKernelsTest, CreateContextInvalidLayers) {
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, 0, 0);  // 0 layers invalid
    EXPECT_EQ(ctx, nullptr);

    ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, -1, 0);  // negative layers invalid
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(CheckpointKernelsTest, ConfigureNullContext) {
    bool result = nimcp_checkpoint_configure(nullptr, CKPT_STRATEGY_SQRT, 0);
    EXPECT_FALSE(result);
}

TEST_F(CheckpointKernelsTest, SetLayerSizeOutOfBounds) {
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, 4, 0);
    ASSERT_NE(ctx, nullptr);

    // Out of bounds layer index
    bool result = nimcp_checkpoint_set_layer_size(ctx, 10, 1024);
    EXPECT_FALSE(result);

    result = nimcp_checkpoint_set_layer_size(ctx, -1, 1024);
    EXPECT_FALSE(result);

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointKernelsTest, SequentialCreateNullGPU) {
    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        nullptr, 6, 0);
    EXPECT_EQ(ckpt, nullptr);
}

//=============================================================================
// Recovery Integration Tests
//=============================================================================

TEST_F(CheckpointKernelsTest, RecoverySystemInitialized) {
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
}

TEST_F(CheckpointKernelsTest, CheckpointAfterRecoveryInit) {
    // Verify checkpointing works after recovery system is initialized
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, 8, 0);
    ASSERT_NE(ctx, nullptr);

    // Perform some operations
    nimcp_checkpoint_begin_forward(ctx);
    nimcp_checkpoint_end_forward(ctx);

    // Recovery system should still be functional
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointKernelsTest, SequentialCheckpointWithRecovery) {
    // Test sequential checkpoint creation with recovery
    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, 6, 50 * 1024 * 1024);
    ASSERT_NE(ckpt, nullptr);

    // Configure checkpointing
    nimcp_sequential_checkpoint_configure(ckpt, CKPT_STRATEGY_EVERY_N, 2);

    // Set layer sizes
    for (int i = 0; i < 6; i++) {
        nimcp_sequential_checkpoint_set_layer_size(ckpt, i, 8 * 1024 * 1024);
    }

    // Store and retrieve activation
    nimcp_gpu_tensor_t* activation = create_random_tensor({4, 128, 128});
    ASSERT_NE(activation, nullptr);

    bool stored = nimcp_sequential_checkpoint_store_activation(ckpt, 0, activation);
    EXPECT_TRUE(stored);
    EXPECT_TRUE(nimcp_sequential_checkpoint_has_activation(ckpt, 0));

    // Recovery system should handle any memory issues gracefully
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    nimcp_gpu_tensor_destroy(gpu_ctx, activation);
    nimcp_sequential_checkpoint_destroy(ckpt);
}

//=============================================================================
// Config Initialization Test
//=============================================================================

TEST_F(CheckpointKernelsTest, ConfigInitDefaults) {
    nimcp_seq_checkpoint_config_t config;
    nimcp_seq_checkpoint_config_init(&config);

    // Check default values
    EXPECT_EQ(config.strategy, CKPT_STRATEGY_SQRT);
    EXPECT_EQ(config.checkpoint_every_n, 0);
    EXPECT_EQ(config.memory_budget, 0u);
    EXPECT_FALSE(config.preserve_rng);
    EXPECT_FALSE(config.enable_profiling);
    EXPECT_FALSE(config.verbose);
    EXPECT_FALSE(config.auto_configure);
}
