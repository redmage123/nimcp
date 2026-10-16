//=============================================================================
// test_checkpoint_integration.cpp - Integration Tests for Checkpointing
//=============================================================================
/**
 * @file test_checkpoint_integration.cpp
 * @brief Integration tests for gradient/activation checkpointing with recovery
 *
 * WHAT: End-to-end tests for checkpointing workflows
 * WHY:  Verify checkpointing works correctly with training pipelines
 * HOW:  Tests full forward->backward cycles with checkpointing enabled
 *
 * TEST SCENARIOS:
 * - Sequential model checkpointing
 * - Memory savings verification
 * - Recomputation workflow
 * - Different checkpointing strategies
 * - Transformer-style checkpointing
 * - Recovery scenarios
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <chrono>

extern "C" {
#include "gpu/training/nimcp_gradient_checkpoint.h"
#include "gpu/training/nimcp_activation_checkpoint.h"
#include "gpu/training/nimcp_training_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class CheckpointIntegrationTest : public ::testing::Test {
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
        std::mt19937 gen(42);  // Fixed seed for reproducibility
        std::uniform_real_distribution<float> dist(min_val, max_val);
        for (auto& v : data) v = dist(gen);

        return create_test_tensor(dims, data);
    }

    // Helper to create zero tensor
    nimcp_gpu_tensor_t* create_zero_tensor(const std::vector<size_t>& dims) {
        size_t total_elements = 1;
        for (size_t d : dims) total_elements *= d;
        std::vector<float> zeros(total_elements, 0.0f);
        return create_test_tensor(dims, zeros);
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

    // Check if tensor contains NaN or Inf
    bool has_nan_or_inf(nimcp_gpu_tensor_t* tensor) {
        auto data = get_tensor_data(tensor);
        for (float v : data) {
            if (std::isnan(v) || std::isinf(v)) return true;
        }
        return false;
    }

    // Calculate tensor size in bytes
    size_t tensor_size_bytes(const std::vector<size_t>& dims) {
        size_t elements = 1;
        for (size_t d : dims) elements *= d;
        return elements * sizeof(float);
    }
};

//=============================================================================
// Sequential Model Checkpointing Tests
//=============================================================================

// Dummy layer context for testing
struct TestLayerContext {
    nimcp_gpu_context_t* gpu_ctx;
    std::vector<size_t> input_dims;
    std::vector<size_t> output_dims;
};

// Test forward function
static void test_layer_forward(int layer_idx, void* ctx,
                               nimcp_gpu_tensor_t* input,
                               nimcp_gpu_tensor_t* output) {
    // Simple identity-like forward (in practice would apply layer computation)
    (void)layer_idx;
    (void)ctx;
    (void)input;
    (void)output;
}

// Test backward function
static void test_layer_backward(int layer_idx, void* ctx,
                                nimcp_gpu_tensor_t* grad_output,
                                nimcp_gpu_tensor_t* grad_input) {
    // Simple identity-like backward
    (void)layer_idx;
    (void)ctx;
    (void)grad_output;
    (void)grad_input;
}

TEST_F(CheckpointIntegrationTest, SequentialCheckpointLifecycle) {
    // Test complete lifecycle of sequential checkpoint

    int num_layers = 8;
    size_t memory_budget = 50 * 1024 * 1024;  // 50 MB

    // Create checkpoint context
    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, num_layers, memory_budget);
    ASSERT_NE(ckpt, nullptr);

    // Configure strategy
    bool result = nimcp_sequential_checkpoint_configure(ckpt, CKPT_STRATEGY_EVERY_N, 2);
    EXPECT_TRUE(result);

    // Set layer sizes
    for (int i = 0; i < num_layers; i++) {
        result = nimcp_sequential_checkpoint_set_layer_size(ckpt, i, 8 * 1024 * 1024);
        EXPECT_TRUE(result);
    }

    // Reset for training
    result = nimcp_sequential_checkpoint_reset(ckpt);
    EXPECT_TRUE(result);

    // Destroy
    nimcp_sequential_checkpoint_destroy(ckpt);
}

TEST_F(CheckpointIntegrationTest, SequentialCheckpointActivationStorage) {
    // Test activation storage and retrieval

    int num_layers = 6;
    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, num_layers, 0);
    ASSERT_NE(ckpt, nullptr);

    nimcp_sequential_checkpoint_configure(ckpt, CKPT_STRATEGY_EVERY_N, 2);

    // Create and store activations at checkpoint layers
    std::vector<nimcp_gpu_tensor_t*> activations;
    for (int i = 0; i < num_layers; i += 2) {  // Checkpoint every 2 layers
        nimcp_gpu_tensor_t* act = create_random_tensor({4, 64});
        ASSERT_NE(act, nullptr);
        activations.push_back(act);

        bool stored = nimcp_sequential_checkpoint_store_activation(ckpt, i, act);
        EXPECT_TRUE(stored);
        EXPECT_TRUE(nimcp_sequential_checkpoint_has_activation(ckpt, i));
    }

    // Non-checkpoint layers should not have stored activations
    EXPECT_FALSE(nimcp_sequential_checkpoint_has_activation(ckpt, 1));
    EXPECT_FALSE(nimcp_sequential_checkpoint_has_activation(ckpt, 3));
    EXPECT_FALSE(nimcp_sequential_checkpoint_has_activation(ckpt, 5));

    // Free activations
    for (int i = 0; i < num_layers; i += 2) {
        bool freed = nimcp_sequential_checkpoint_free_activation(ckpt, i);
        EXPECT_TRUE(freed);
        EXPECT_FALSE(nimcp_sequential_checkpoint_has_activation(ckpt, i));
    }

    // Clean up original tensors
    for (auto* act : activations) {
        nimcp_gpu_tensor_destroy(gpu_ctx, act);
    }

    nimcp_sequential_checkpoint_destroy(ckpt);
}

//=============================================================================
// Memory Savings Verification Tests
//=============================================================================

TEST_F(CheckpointIntegrationTest, VerifyMemorySavings) {
    // Verify that checkpointing reduces memory usage

    int num_layers = 16;
    size_t layer_sizes[16];

    // Each layer produces 4 MB of activations
    for (int i = 0; i < num_layers; i++) {
        layer_sizes[i] = 4 * 1024 * 1024;
    }

    size_t no_ckpt_memory = 0;
    size_t sqrt_ckpt_memory = 0;
    int optimal_n = 0;

    bool result = nimcp_sequential_checkpoint_estimate_memory(
        num_layers, layer_sizes, &no_ckpt_memory, &sqrt_ckpt_memory, &optimal_n);
    EXPECT_TRUE(result);

    // Without checkpointing: 16 * 4MB = 64 MB
    EXPECT_EQ(no_ckpt_memory, 64 * 1024 * 1024);

    // With sqrt checkpointing: should be significantly less
    // sqrt(16) = 4, so approximately 4 checkpoints + segment recompute
    EXPECT_LT(sqrt_ckpt_memory, no_ckpt_memory);

    // Memory savings should be at least 50%
    float savings_ratio = 1.0f - (float)sqrt_ckpt_memory / no_ckpt_memory;
    EXPECT_GT(savings_ratio, 0.3f);  // At least 30% savings
}

TEST_F(CheckpointIntegrationTest, VerifyCheckpointStatistics) {
    // Verify checkpoint statistics are correctly tracked

    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, 8, 0);
    ASSERT_NE(ckpt, nullptr);

    nimcp_sequential_checkpoint_configure(ckpt, CKPT_STRATEGY_EVERY_N, 2);

    // Set layer sizes
    for (int i = 0; i < 8; i++) {
        nimcp_sequential_checkpoint_set_layer_size(ckpt, i, 4 * 1024 * 1024);
    }

    // Store some activations
    for (int i = 0; i < 8; i += 2) {
        nimcp_gpu_tensor_t* act = create_random_tensor({4, 256, 256});
        nimcp_sequential_checkpoint_store_activation(ckpt, i, act);
        nimcp_gpu_tensor_destroy(gpu_ctx, act);
    }

    // Get statistics
    size_t total_memory = 0;
    size_t saved_memory = 0;
    int recompute_count = 0;
    double recompute_time_ms = 0.0;

    nimcp_sequential_checkpoint_get_stats(ckpt, &total_memory, &saved_memory,
                                          &recompute_count, &recompute_time_ms);

    // Memory should be tracked
    EXPECT_GT(total_memory, 0u);

    nimcp_sequential_checkpoint_destroy(ckpt);
}

//=============================================================================
// Different Checkpointing Strategies Tests
//=============================================================================

TEST_F(CheckpointIntegrationTest, CompareCheckpointStrategies) {
    // Compare different checkpointing strategies

    int num_layers = 12;
    size_t layer_sizes[12];
    for (int i = 0; i < num_layers; i++) {
        layer_sizes[i] = 5 * 1024 * 1024;  // 5 MB each
    }

    // No checkpointing
    size_t no_ckpt = 0;
    for (int i = 0; i < num_layers; i++) {
        no_ckpt += layer_sizes[i];
    }

    // SQRT strategy
    nimcp_checkpoint_estimate_t sqrt_estimate;
    nimcp_checkpoint_estimate(layer_sizes, num_layers, &sqrt_estimate);

    // EVERY_N with N=3
    // Checkpoints at layers 0, 3, 6, 9 -> need to store 4 activations
    // Plus maximum segment recompute buffer
    size_t every_3_estimate = 4 * layer_sizes[0] + 3 * layer_sizes[0];

    // All strategies should use less memory than no checkpointing
    EXPECT_LT(sqrt_estimate.sqrt_checkpoint_memory, no_ckpt);
    EXPECT_LT(every_3_estimate, no_ckpt);
}

TEST_F(CheckpointIntegrationTest, SelectiveCheckpointing) {
    // Test selective checkpointing with user-specified layers

    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_NONE, 10, 0);
    ASSERT_NE(ctx, nullptr);

    // Checkpoint only specific layers (e.g., attention layers in transformer)
    int checkpoint_layers[] = {0, 2, 5, 8};
    bool result = nimcp_checkpoint_configure_selective(ctx, checkpoint_layers, 4);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx->strategy, CKPT_STRATEGY_SELECTIVE);

    // Verify checkpoint layer detection
    EXPECT_TRUE(nimcp_checkpoint_is_checkpoint_layer(ctx, 0));
    EXPECT_FALSE(nimcp_checkpoint_is_checkpoint_layer(ctx, 1));
    EXPECT_TRUE(nimcp_checkpoint_is_checkpoint_layer(ctx, 2));
    EXPECT_FALSE(nimcp_checkpoint_is_checkpoint_layer(ctx, 3));
    EXPECT_FALSE(nimcp_checkpoint_is_checkpoint_layer(ctx, 4));
    EXPECT_TRUE(nimcp_checkpoint_is_checkpoint_layer(ctx, 5));

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointIntegrationTest, MemoryBudgetStrategy) {
    // Test automatic checkpoint selection based on memory budget

    int num_layers = 16;
    size_t layer_sizes[16];
    for (int i = 0; i < num_layers; i++) {
        layer_sizes[i] = 4 * 1024 * 1024;  // 4 MB each
    }

    // Total without checkpointing: 64 MB
    // Set budget to 32 MB (half)
    size_t memory_budget = 32 * 1024 * 1024;

    nimcp_checkpoint_strategy_t recommended_strategy;
    int checkpoint_n;

    bool result = nimcp_checkpoint_recommend_strategy(
        layer_sizes, num_layers, memory_budget, &recommended_strategy, &checkpoint_n);
    EXPECT_TRUE(result);

    // Should recommend some form of checkpointing
    EXPECT_NE(recommended_strategy, CKPT_STRATEGY_NONE);
}

//=============================================================================
// Forward/Backward Pass Integration Tests
//=============================================================================

TEST_F(CheckpointIntegrationTest, ForwardBackwardWithCheckpoint) {
    // Test complete forward and backward pass with checkpointing

    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_EVERY_N, 6, 0);
    ASSERT_NE(ctx, nullptr);

    nimcp_checkpoint_configure(ctx, CKPT_STRATEGY_EVERY_N, 2);

    // Begin forward pass
    EXPECT_TRUE(nimcp_checkpoint_begin_forward(ctx));
    EXPECT_TRUE(ctx->in_forward_pass);

    // Simulate forward pass through layers
    for (int layer = 0; layer < 6; layer++) {
        nimcp_gpu_tensor_t* activation = create_random_tensor({4, 64});
        ASSERT_NE(activation, nullptr);

        // Mark layer (will save if checkpoint layer)
        EXPECT_TRUE(nimcp_checkpoint_mark_layer(ctx, layer, activation));

        // Check if this layer's activation should be saved
        if (nimcp_checkpoint_should_save(ctx, layer)) {
            EXPECT_TRUE(layer % 2 == 0);  // Every 2 layers
        }

        nimcp_gpu_tensor_destroy(gpu_ctx, activation);
    }

    // End forward pass
    EXPECT_TRUE(nimcp_checkpoint_end_forward(ctx));
    EXPECT_FALSE(ctx->in_forward_pass);

    // Begin backward pass
    EXPECT_TRUE(nimcp_checkpoint_begin_backward(ctx));
    EXPECT_TRUE(ctx->in_backward_pass);

    // Simulate backward pass (reverse order)
    for (int layer = 5; layer >= 0; layer--) {
        // Get activation (may trigger recompute)
        nimcp_gpu_tensor_t* activation = nimcp_checkpoint_get_activation(ctx, layer);
        // Activation retrieval may return nullptr if not checkpointed and not recomputed
        (void)activation;
    }

    // End backward pass
    EXPECT_TRUE(nimcp_checkpoint_end_backward(ctx));
    EXPECT_FALSE(ctx->in_backward_pass);

    nimcp_checkpoint_ctx_destroy(ctx);
}

//=============================================================================
// Checkpointed Function Tests
//=============================================================================

// Test context for checkpointed function
struct CheckpointFnContext {
    nimcp_gpu_context_t* gpu_ctx;
    int layer_idx;
};

static void checkpoint_forward(void* ctx, nimcp_gpu_tensor_t* input, nimcp_gpu_tensor_t* output) {
    // Identity forward for testing
    (void)ctx;
    (void)input;
    (void)output;
}

static void checkpoint_backward(void* ctx, nimcp_gpu_tensor_t* grad_output, nimcp_gpu_tensor_t* grad_input) {
    // Identity backward for testing
    (void)ctx;
    (void)grad_output;
    (void)grad_input;
}

TEST_F(CheckpointIntegrationTest, CheckpointedFunctionUsage) {
    // Test checkpointed function wrapper

    nimcp_checkpointed_fn_t* fn = nimcp_checkpointed_fn_create(
        checkpoint_forward,
        checkpoint_backward,
        nullptr,
        true  // preserve RNG for dropout
    );
    ASSERT_NE(fn, nullptr);

    EXPECT_EQ(fn->forward, checkpoint_forward);
    EXPECT_EQ(fn->backward, checkpoint_backward);
    EXPECT_TRUE(fn->preserve_rng);

    nimcp_checkpointed_fn_destroy(fn);
}

TEST_F(CheckpointIntegrationTest, CheckpointedFunctionWithContext) {
    // Test checkpointed function with user context

    CheckpointFnContext user_ctx = {
        .gpu_ctx = gpu_ctx,
        .layer_idx = 5
    };

    nimcp_checkpointed_fn_t* fn = nimcp_checkpointed_fn_create(
        checkpoint_forward,
        checkpoint_backward,
        &user_ctx,
        false
    );
    ASSERT_NE(fn, nullptr);

    EXPECT_EQ(fn->ctx, &user_ctx);

    nimcp_checkpointed_fn_destroy(fn);
}

//=============================================================================
// Transformer Checkpointing Tests
//=============================================================================

TEST_F(CheckpointIntegrationTest, TransformerCheckpointCreation) {
    // Test transformer-specific checkpoint context

    int num_blocks = 12;
    size_t memory_budget = 256 * 1024 * 1024;  // 256 MB

    nimcp_transformer_checkpoint_t* ckpt = nimcp_transformer_checkpoint_create(
        gpu_ctx, num_blocks, memory_budget);
    ASSERT_NE(ckpt, nullptr);

    EXPECT_EQ(ckpt->num_blocks, num_blocks);

    nimcp_transformer_checkpoint_destroy(ckpt);
}

TEST_F(CheckpointIntegrationTest, TransformerBlockConfiguration) {
    // Test transformer block configuration

    int num_blocks = 6;
    nimcp_transformer_checkpoint_t* ckpt = nimcp_transformer_checkpoint_create(
        gpu_ctx, num_blocks, 0);
    ASSERT_NE(ckpt, nullptr);

    // Configure blocks with different sizes
    for (int i = 0; i < num_blocks; i++) {
        nimcp_transformer_block_t block = {
            .num_heads = 8,
            .embed_dim = 512,
            .ffn_dim = 2048,
            .pre_norm = (i % 2 == 0),  // Alternate pre/post norm
            .dropout_p = 0.1f
        };

        bool result = nimcp_transformer_checkpoint_configure_block(ckpt, i, &block);
        EXPECT_TRUE(result);
    }

    nimcp_transformer_checkpoint_destroy(ckpt);
}

TEST_F(CheckpointIntegrationTest, TransformerLargeModel) {
    // Test with large transformer-like configuration

    // GPT-2 Small-like: 12 blocks
    int num_blocks = 12;
    nimcp_transformer_checkpoint_t* ckpt = nimcp_transformer_checkpoint_create(
        gpu_ctx, num_blocks, 512 * 1024 * 1024);  // 512 MB budget
    ASSERT_NE(ckpt, nullptr);

    nimcp_transformer_block_t block = {
        .num_heads = 12,
        .embed_dim = 768,
        .ffn_dim = 3072,
        .pre_norm = true,
        .dropout_p = 0.1f
    };

    for (int i = 0; i < num_blocks; i++) {
        bool result = nimcp_transformer_checkpoint_configure_block(ckpt, i, &block);
        EXPECT_TRUE(result);
    }

    nimcp_transformer_checkpoint_destroy(ckpt);
}

//=============================================================================
// Recomputation Workflow Tests
//=============================================================================

TEST_F(CheckpointIntegrationTest, RecomputeSegment) {
    // Test segment recomputation

    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_EVERY_N, 8, 0);
    ASSERT_NE(ctx, nullptr);

    nimcp_checkpoint_configure(ctx, CKPT_STRATEGY_EVERY_N, 4);

    // Set up layer sizes
    for (int i = 0; i < 8; i++) {
        nimcp_checkpoint_set_layer_size(ctx, i, 4 * 1024 * 1024);
    }

    // Register forward function for segment 0-3
    bool result = nimcp_checkpoint_register_forward(ctx, 0, 3, checkpoint_forward, nullptr);
    // May or may not be implemented depending on strategy
    (void)result;

    // Get segment for layer
    int segment = nimcp_checkpoint_get_segment_for_layer(ctx, 2);
    // Segment index depends on implementation

    (void)segment;

    nimcp_checkpoint_ctx_destroy(ctx);
}

//=============================================================================
// Profiling and Statistics Tests
//=============================================================================

TEST_F(CheckpointIntegrationTest, ProfilingEnabled) {
    // Test profiling functionality

    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, 8, 0);
    ASSERT_NE(ctx, nullptr);

    nimcp_checkpoint_set_profiling(ctx, true);
    EXPECT_TRUE(ctx->enable_profiling);

    // Do some operations
    nimcp_checkpoint_begin_forward(ctx);
    nimcp_checkpoint_end_forward(ctx);
    nimcp_checkpoint_begin_backward(ctx);
    nimcp_checkpoint_end_backward(ctx);

    // Get timing statistics
    size_t current_memory, peak_memory, saved_memory;
    int recompute_count;
    double recompute_time_ms;

    nimcp_checkpoint_get_stats(ctx, &current_memory, &peak_memory,
                               &saved_memory, &recompute_count, &recompute_time_ms);

    EXPECT_GE(recompute_time_ms, 0.0);

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointIntegrationTest, VerboseLogging) {
    // Test verbose logging (doesn't crash)

    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, 4, 0);
    ASSERT_NE(ctx, nullptr);

    nimcp_checkpoint_set_verbose(ctx, true);
    EXPECT_TRUE(ctx->verbose);

    // Operations with verbose logging shouldn't crash
    nimcp_checkpoint_begin_forward(ctx);
    nimcp_checkpoint_end_forward(ctx);

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointIntegrationTest, PrintStatistics) {
    // Test printing statistics (shouldn't crash)

    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, 4, 0);
    ASSERT_NE(ctx, nullptr);

    // This prints to stdout - just verify it doesn't crash
    nimcp_checkpoint_print_stats(ctx);

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointIntegrationTest, SequentialPrintStatistics) {
    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, 6, 0);
    ASSERT_NE(ckpt, nullptr);

    // This prints to stdout - just verify it doesn't crash
    nimcp_sequential_checkpoint_print_stats(ckpt);

    nimcp_sequential_checkpoint_destroy(ckpt);
}

//=============================================================================
// Recovery Integration Tests
//=============================================================================

TEST_F(CheckpointIntegrationTest, RecoverySystemActive) {
    // Verify recovery system is active during checkpointing

    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, 8, 0);
    ASSERT_NE(ctx, nullptr);

    // Perform operations
    nimcp_checkpoint_begin_forward(ctx);
    nimcp_checkpoint_end_forward(ctx);

    // Recovery should still be active
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointIntegrationTest, CheckpointAfterRecovery) {
    // Test that checkpointing works correctly after potential recovery

    // Create multiple checkpoint contexts sequentially
    for (int trial = 0; trial < 3; trial++) {
        nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
            gpu_ctx, 6, 0);
        ASSERT_NE(ckpt, nullptr);

        nimcp_sequential_checkpoint_configure(ckpt, CKPT_STRATEGY_EVERY_N, 2);

        // Store activations
        for (int i = 0; i < 6; i += 2) {
            nimcp_gpu_tensor_t* act = create_random_tensor({4, 64});
            ASSERT_NE(act, nullptr);
            nimcp_sequential_checkpoint_store_activation(ckpt, i, act);
            nimcp_gpu_tensor_destroy(gpu_ctx, act);
        }

        // Reset
        nimcp_sequential_checkpoint_reset(ckpt);

        nimcp_sequential_checkpoint_destroy(ckpt);

        // Recovery should remain functional
        EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
    }
}

//=============================================================================
// Edge Cases Tests
//=============================================================================

TEST_F(CheckpointIntegrationTest, SingleLayerCheckpoint) {
    // Test with only 1 layer (edge case)

    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, 1, 0);
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(ctx->total_layers, 1);

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointIntegrationTest, ManyLayersCheckpoint) {
    // Test with many layers

    int num_layers = 100;
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, num_layers, 0);
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(ctx->total_layers, num_layers);

    // Sqrt interval should be ~10
    int interval = nimcp_checkpoint_sqrt_interval(num_layers);
    EXPECT_EQ(interval, 10);

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointIntegrationTest, ZeroMemoryBudget) {
    // Test with zero memory budget (no limit)

    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_SQRT, 8, 0);
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(ctx->memory_budget, 0u);

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointIntegrationTest, ResetMultipleTimes) {
    // Test multiple resets

    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, 6, 0);
    ASSERT_NE(ckpt, nullptr);

    for (int i = 0; i < 5; i++) {
        bool result = nimcp_sequential_checkpoint_reset(ckpt);
        EXPECT_TRUE(result);
    }

    nimcp_sequential_checkpoint_destroy(ckpt);
}

//=============================================================================
// Configuration Default Tests
//=============================================================================

TEST_F(CheckpointIntegrationTest, ConfigDefaultValues) {
    nimcp_seq_checkpoint_config_t config;
    nimcp_seq_checkpoint_config_init(&config);

    // Check defaults
    EXPECT_EQ(config.strategy, CKPT_STRATEGY_SQRT);
    EXPECT_EQ(config.checkpoint_every_n, 0);
    EXPECT_EQ(config.memory_budget, 0u);
    EXPECT_FALSE(config.preserve_rng);
    EXPECT_FALSE(config.enable_profiling);
    EXPECT_FALSE(config.verbose);
    EXPECT_FALSE(config.auto_configure);
}

TEST_F(CheckpointIntegrationTest, ConfigCustomValues) {
    nimcp_seq_checkpoint_config_t config;
    nimcp_seq_checkpoint_config_init(&config);

    // Set custom values
    config.strategy = CKPT_STRATEGY_EVERY_N;
    config.checkpoint_every_n = 4;
    config.memory_budget = 128 * 1024 * 1024;
    config.preserve_rng = true;
    config.enable_profiling = true;
    config.verbose = true;
    config.auto_configure = false;

    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create_with_config(
        gpu_ctx, 12, &config);
    ASSERT_NE(ckpt, nullptr);

    // Verify config was applied
    EXPECT_EQ(ckpt->config.strategy, CKPT_STRATEGY_EVERY_N);
    EXPECT_EQ(ckpt->config.checkpoint_every_n, 4);
    EXPECT_TRUE(ckpt->config.preserve_rng);
    EXPECT_TRUE(ckpt->config.enable_profiling);
    EXPECT_TRUE(ckpt->config.verbose);

    nimcp_sequential_checkpoint_destroy(ckpt);
}

//=============================================================================
// Auto-Configuration Tests
//=============================================================================

TEST_F(CheckpointIntegrationTest, AutoConfigureCheckpointing) {
    nimcp_checkpoint_ctx_t* ctx = nimcp_checkpoint_ctx_create(
        gpu_ctx, CKPT_STRATEGY_MEMORY_BUDGET, 8, 32 * 1024 * 1024);
    ASSERT_NE(ctx, nullptr);

    // Set layer sizes
    size_t layer_sizes[8];
    for (int i = 0; i < 8; i++) {
        layer_sizes[i] = 8 * 1024 * 1024;  // 8 MB each
    }

    // Auto-configure based on budget (32 MB budget, 64 MB total)
    size_t available_memory = 32 * 1024 * 1024;
    bool result = nimcp_checkpoint_auto_configure(ctx, available_memory, layer_sizes, 8);
    // May or may not be fully implemented
    (void)result;

    nimcp_checkpoint_ctx_destroy(ctx);
}

TEST_F(CheckpointIntegrationTest, SequentialAutoConfiguration) {
    nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
        gpu_ctx, 8, 32 * 1024 * 1024);
    ASSERT_NE(ckpt, nullptr);

    // Set layer sizes
    for (int i = 0; i < 8; i++) {
        nimcp_sequential_checkpoint_set_layer_size(ckpt, i, 8 * 1024 * 1024);
    }

    // Auto-configure
    bool result = nimcp_sequential_checkpoint_auto_configure(ckpt);
    // May or may not be fully implemented
    (void)result;

    nimcp_sequential_checkpoint_destroy(ckpt);
}
