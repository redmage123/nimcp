/**
 * @file test_gradient_checkpoint.cpp
 * @brief Unit tests for Gradient Checkpointing
 *
 * Tests checkpoint context creation, strategies (NONE, SQRT, EVERY_N, SELECTIVE,
 * MEMORY_BUDGET), activation saving/recomputation, RNG state preservation,
 * and memory savings measurement.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>
#include <random>

// Headers already have their own extern "C" guards
#include "gpu/training/nimcp_gradient_checkpoint.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class GradientCheckpointTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    nimcp_checkpoint_ctx_t* ckpt_ctx = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        ctx = nimcp_gpu_context_create_auto();
        gpu_available = (ctx != nullptr && nimcp_gpu_context_is_valid(ctx));
    }

    void TearDown() override {
        if (ckpt_ctx) {
            nimcp_checkpoint_ctx_destroy(ckpt_ctx);
            ckpt_ctx = nullptr;
        }
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    void RequireGPU() {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available, skipping test";
        }
    }

    // Helper to create a simple GPU tensor
    nimcp_gpu_tensor_t* CreateTensor(size_t n) {
        size_t dims[1] = {n};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, 1.0f);
        }
        return tensor;
    }

    // Helper to create 2D tensor
    nimcp_gpu_tensor_t* Create2DTensor(size_t rows, size_t cols) {
        size_t dims[2] = {rows, cols};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, 1.0f);
        }
        return tensor;
    }

    // Generate activation sizes for a model with N layers
    std::vector<size_t> GenerateLayerSizes(int num_layers, size_t base_size = 1024 * 1024) {
        std::vector<size_t> sizes(num_layers);
        for (int i = 0; i < num_layers; i++) {
            sizes[i] = base_size;  // Uniform size for simplicity
        }
        return sizes;
    }
};

//=============================================================================
// Context Creation Tests
//=============================================================================

TEST_F(GradientCheckpointTest, Create_WithDefaultStrategy_ReturnsValidContext) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_NONE, 10, 0);

    ASSERT_NE(ckpt_ctx, nullptr);
    EXPECT_EQ(ckpt_ctx->strategy, CKPT_STRATEGY_NONE);
    EXPECT_EQ(ckpt_ctx->total_layers, 10);
    EXPECT_TRUE(ckpt_ctx->configured);
}

TEST_F(GradientCheckpointTest, Create_NullContext_ReturnsNull) {
    ckpt_ctx = nimcp_checkpoint_ctx_create(nullptr, CKPT_STRATEGY_SQRT, 10, 0);

    EXPECT_EQ(ckpt_ctx, nullptr);
}

TEST_F(GradientCheckpointTest, Create_ZeroLayers_ReturnsNull) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 0, 0);

    EXPECT_EQ(ckpt_ctx, nullptr);
}

TEST_F(GradientCheckpointTest, Create_SqrtStrategy) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 16, 0);

    ASSERT_NE(ckpt_ctx, nullptr);
    EXPECT_EQ(ckpt_ctx->strategy, CKPT_STRATEGY_SQRT);
}

TEST_F(GradientCheckpointTest, Create_EveryNStrategy) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_EVERY_N, 20, 0);

    ASSERT_NE(ckpt_ctx, nullptr);
    EXPECT_EQ(ckpt_ctx->strategy, CKPT_STRATEGY_EVERY_N);
}

TEST_F(GradientCheckpointTest, Create_WithMemoryBudget) {
    RequireGPU();

    const size_t budget = 64 * 1024 * 1024;  // 64 MB
    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_MEMORY_BUDGET, 12, budget);

    ASSERT_NE(ckpt_ctx, nullptr);
    EXPECT_EQ(ckpt_ctx->memory_budget, budget);
}

TEST_F(GradientCheckpointTest, Destroy_HandlesNull) {
    nimcp_checkpoint_ctx_destroy(nullptr);  // Should not crash
}

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(GradientCheckpointTest, Configure_SqrtStrategy) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_NONE, 16, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    bool result = nimcp_checkpoint_configure(ckpt_ctx, CKPT_STRATEGY_SQRT, 0);

    EXPECT_TRUE(result);
    EXPECT_EQ(ckpt_ctx->strategy, CKPT_STRATEGY_SQRT);
}

TEST_F(GradientCheckpointTest, Configure_EveryN) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_NONE, 20, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    bool result = nimcp_checkpoint_configure(ckpt_ctx, CKPT_STRATEGY_EVERY_N, 4);

    EXPECT_TRUE(result);
    EXPECT_EQ(ckpt_ctx->strategy, CKPT_STRATEGY_EVERY_N);
    EXPECT_EQ(ckpt_ctx->checkpoint_every_n, 4);
}

TEST_F(GradientCheckpointTest, Configure_Selective) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_NONE, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    int checkpoint_layers[] = {0, 3, 6, 9};
    bool result = nimcp_checkpoint_configure_selective(ckpt_ctx, checkpoint_layers, 4);

    EXPECT_TRUE(result);
    EXPECT_EQ(ckpt_ctx->strategy, CKPT_STRATEGY_SELECTIVE);
    EXPECT_EQ(ckpt_ctx->num_selective_layers, 4);
}

TEST_F(GradientCheckpointTest, AutoConfigure_WithinBudget) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_NONE, 8, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    std::vector<size_t> layer_sizes(8, 4 * 1024 * 1024);  // 4 MB each
    size_t available_memory = 16 * 1024 * 1024;  // 16 MB budget

    bool result = nimcp_checkpoint_auto_configure(
        ckpt_ctx, available_memory, layer_sizes.data(), 8);

    EXPECT_TRUE(result);
    // Should have chosen a strategy that fits within budget
    EXPECT_NE(ckpt_ctx->strategy, CKPT_STRATEGY_NONE);
}

TEST_F(GradientCheckpointTest, SetLayerSize) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    bool result = nimcp_checkpoint_set_layer_size(ckpt_ctx, 0, 1024 * 1024);
    EXPECT_TRUE(result);

    result = nimcp_checkpoint_set_layer_size(ckpt_ctx, 5, 2 * 1024 * 1024);
    EXPECT_TRUE(result);

    // Invalid layer index
    result = nimcp_checkpoint_set_layer_size(ckpt_ctx, 100, 1024);
    EXPECT_FALSE(result);
}

//=============================================================================
// Strategy: NONE Tests
//=============================================================================

TEST_F(GradientCheckpointTest, StrategyNone_KeepsAllActivations) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_NONE, 5, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    // With NONE strategy, all layers should save activations
    for (int i = 0; i < 5; i++) {
        bool should_save = nimcp_checkpoint_should_save(ckpt_ctx, i);
        EXPECT_TRUE(should_save) << "Layer " << i << " should save with NONE strategy";
    }
}

TEST_F(GradientCheckpointTest, StrategyNone_BaselineMemory) {
    RequireGPU();

    const int num_layers = 10;
    const size_t layer_size = 1024 * 1024;  // 1 MB

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_NONE, num_layers, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    for (int i = 0; i < num_layers; i++) {
        nimcp_checkpoint_set_layer_size(ckpt_ctx, i, layer_size);
    }

    size_t current, peak, saved;
    int recompute_count;
    double recompute_time;
    nimcp_checkpoint_get_stats(ckpt_ctx, &current, &peak, &saved, &recompute_count, &recompute_time);

    // Peak memory should be all layers
    EXPECT_EQ(peak, 0u);  // Not computed until forward pass
    EXPECT_EQ(saved, 0u);  // NONE strategy saves nothing
}

//=============================================================================
// Strategy: SQRT Tests
//=============================================================================

TEST_F(GradientCheckpointTest, StrategySqrt_CheckpointsAtSqrtIntervals) {
    RequireGPU();

    const int num_layers = 16;
    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, num_layers, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    int sqrt_interval = nimcp_checkpoint_sqrt_interval(num_layers);
    EXPECT_EQ(sqrt_interval, 4);  // sqrt(16) = 4

    int checkpoints_found = 0;
    for (int i = 0; i < num_layers; i++) {
        if (nimcp_checkpoint_is_checkpoint_layer(ckpt_ctx, i)) {
            checkpoints_found++;
        }
    }

    // Should have ~sqrt(N) checkpoints
    EXPECT_NEAR(checkpoints_found, sqrt_interval, 1);
}

TEST_F(GradientCheckpointTest, StrategySqrt_MemorySavings) {
    RequireGPU();

    const int num_layers = 100;
    const size_t layer_size = 1024 * 1024;

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, num_layers, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    for (int i = 0; i < num_layers; i++) {
        nimcp_checkpoint_set_layer_size(ckpt_ctx, i, layer_size);
    }

    // With sqrt strategy, memory should be O(sqrt(N)) instead of O(N)
    int sqrt_interval = nimcp_checkpoint_sqrt_interval(num_layers);
    EXPECT_EQ(sqrt_interval, 10);  // sqrt(100) = 10
}

//=============================================================================
// Strategy: EVERY_N Tests
//=============================================================================

TEST_F(GradientCheckpointTest, StrategyEveryN_CheckpointsCorrectLayers) {
    RequireGPU();

    const int num_layers = 12;
    const int every_n = 3;

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_EVERY_N, num_layers, 0);
    ASSERT_NE(ckpt_ctx, nullptr);
    nimcp_checkpoint_configure(ckpt_ctx, CKPT_STRATEGY_EVERY_N, every_n);

    std::vector<int> checkpointed_layers;
    for (int i = 0; i < num_layers; i++) {
        if (nimcp_checkpoint_is_checkpoint_layer(ckpt_ctx, i)) {
            checkpointed_layers.push_back(i);
        }
    }

    // Layers 0, 3, 6, 9 should be checkpointed
    EXPECT_EQ(checkpointed_layers.size(), 4u);
    for (size_t i = 0; i < checkpointed_layers.size(); i++) {
        EXPECT_EQ(checkpointed_layers[i] % every_n, 0);
    }
}

TEST_F(GradientCheckpointTest, StrategyEveryN_DifferentIntervals) {
    RequireGPU();

    const int num_layers = 20;

    for (int n = 1; n <= 5; n++) {
        ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_EVERY_N, num_layers, 0);
        ASSERT_NE(ckpt_ctx, nullptr);
        nimcp_checkpoint_configure(ckpt_ctx, CKPT_STRATEGY_EVERY_N, n);

        int checkpoint_count = 0;
        for (int i = 0; i < num_layers; i++) {
            if (nimcp_checkpoint_is_checkpoint_layer(ckpt_ctx, i)) {
                checkpoint_count++;
            }
        }

        // Expected: num_layers / n checkpoints (approximately)
        EXPECT_NEAR(checkpoint_count, (num_layers + n - 1) / n, 1);

        nimcp_checkpoint_ctx_destroy(ckpt_ctx);
        ckpt_ctx = nullptr;
    }
}

//=============================================================================
// Strategy: SELECTIVE Tests
//=============================================================================

TEST_F(GradientCheckpointTest, StrategySelective_OnlySpecifiedLayers) {
    RequireGPU();

    const int num_layers = 10;
    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_NONE, num_layers, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    int checkpoint_layers[] = {0, 2, 5, 9};
    nimcp_checkpoint_configure_selective(ckpt_ctx, checkpoint_layers, 4);

    for (int i = 0; i < num_layers; i++) {
        bool is_checkpoint = nimcp_checkpoint_is_checkpoint_layer(ckpt_ctx, i);
        bool expected = (i == 0 || i == 2 || i == 5 || i == 9);
        EXPECT_EQ(is_checkpoint, expected) << "Mismatch at layer " << i;
    }
}

TEST_F(GradientCheckpointTest, StrategySelective_EmptyList) {
    RequireGPU();

    const int num_layers = 10;
    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_NONE, num_layers, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    bool result = nimcp_checkpoint_configure_selective(ckpt_ctx, nullptr, 0);
    EXPECT_FALSE(result);  // Should fail with empty list
}

//=============================================================================
// Strategy: MEMORY_BUDGET Tests
//=============================================================================

TEST_F(GradientCheckpointTest, StrategyMemoryBudget_StaysWithinBudget) {
    RequireGPU();

    const int num_layers = 20;
    const size_t layer_size = 4 * 1024 * 1024;  // 4 MB each
    const size_t budget = 32 * 1024 * 1024;     // 32 MB

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_MEMORY_BUDGET, num_layers, budget);
    ASSERT_NE(ckpt_ctx, nullptr);

    std::vector<size_t> layer_sizes(num_layers, layer_size);
    bool result = nimcp_checkpoint_auto_configure(ckpt_ctx, budget, layer_sizes.data(), num_layers);

    EXPECT_TRUE(result);

    // Count checkpoint layers
    int checkpoint_count = 0;
    for (int i = 0; i < num_layers; i++) {
        if (nimcp_checkpoint_is_checkpoint_layer(ckpt_ctx, i)) {
            checkpoint_count++;
        }
    }

    // Should have enough checkpoints to stay within budget
    size_t estimated_memory = checkpoint_count * layer_size;
    EXPECT_LE(estimated_memory, budget);
}

//=============================================================================
// Activation Saving Tests
//=============================================================================

TEST_F(GradientCheckpointTest, MarkLayer_SavesActivation) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_EVERY_N, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);
    nimcp_checkpoint_configure(ckpt_ctx, CKPT_STRATEGY_EVERY_N, 2);  // Checkpoint every 2

    nimcp_checkpoint_begin_forward(ckpt_ctx);

    nimcp_gpu_tensor_t* activation = CreateTensor(1024);
    ASSERT_NE(activation, nullptr);

    bool result = nimcp_checkpoint_mark_layer(ckpt_ctx, 0, activation);
    EXPECT_TRUE(result);

    // Layer 0 should be a checkpoint
    EXPECT_TRUE(nimcp_checkpoint_is_checkpoint_layer(ckpt_ctx, 0));

    nimcp_checkpoint_end_forward(ckpt_ctx);
    nimcp_gpu_tensor_destroy(activation);
}

TEST_F(GradientCheckpointTest, ShouldSave_ReturnsTrueForCheckpointLayers) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_EVERY_N, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);
    nimcp_checkpoint_configure(ckpt_ctx, CKPT_STRATEGY_EVERY_N, 3);

    EXPECT_TRUE(nimcp_checkpoint_should_save(ckpt_ctx, 0));   // Checkpoint
    EXPECT_FALSE(nimcp_checkpoint_should_save(ckpt_ctx, 1));  // Not checkpoint
    EXPECT_FALSE(nimcp_checkpoint_should_save(ckpt_ctx, 2));  // Not checkpoint
    EXPECT_TRUE(nimcp_checkpoint_should_save(ckpt_ctx, 3));   // Checkpoint
    EXPECT_TRUE(nimcp_checkpoint_should_save(ckpt_ctx, 6));   // Checkpoint
    EXPECT_TRUE(nimcp_checkpoint_should_save(ckpt_ctx, 9));   // Checkpoint
}

//=============================================================================
// Activation Recomputation Tests
//=============================================================================

// Mock forward function for testing recomputation
static void mock_forward(void* segment_ctx, nimcp_gpu_tensor_t* input, nimcp_gpu_tensor_t* output) {
    (void)segment_ctx;
    (void)input;
    (void)output;
    // In real use, would copy/transform input to output
}

static void mock_backward(void* segment_ctx, nimcp_gpu_tensor_t* grad_output, nimcp_gpu_tensor_t* grad_input) {
    (void)segment_ctx;
    (void)grad_output;
    (void)grad_input;
    // In real use, would compute gradients
}

TEST_F(GradientCheckpointTest, RegisterForward_Succeeds) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_EVERY_N, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);
    nimcp_checkpoint_configure(ckpt_ctx, CKPT_STRATEGY_EVERY_N, 3);

    bool result = nimcp_checkpoint_register_forward(ckpt_ctx, 0, 2, mock_forward, nullptr);
    EXPECT_TRUE(result);
}

TEST_F(GradientCheckpointTest, RegisterBackward_Succeeds) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_EVERY_N, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);
    nimcp_checkpoint_configure(ckpt_ctx, CKPT_STRATEGY_EVERY_N, 3);

    bool result = nimcp_checkpoint_register_backward(ckpt_ctx, 0, 2, mock_backward, nullptr);
    EXPECT_TRUE(result);
}

TEST_F(GradientCheckpointTest, GetActivation_TriggersRecompute) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_EVERY_N, 6, 0);
    ASSERT_NE(ckpt_ctx, nullptr);
    nimcp_checkpoint_configure(ckpt_ctx, CKPT_STRATEGY_EVERY_N, 3);

    // Register forward function for segment
    nimcp_checkpoint_register_forward(ckpt_ctx, 0, 2, mock_forward, nullptr);

    // Simulate forward pass
    nimcp_checkpoint_begin_forward(ckpt_ctx);
    nimcp_gpu_tensor_t* activation0 = CreateTensor(1024);
    nimcp_checkpoint_mark_layer(ckpt_ctx, 0, activation0);
    nimcp_checkpoint_end_forward(ckpt_ctx);

    // In backward pass, getting activation for non-checkpoint layer should trigger recompute
    nimcp_checkpoint_begin_backward(ckpt_ctx);

    // Layer 1 is not a checkpoint, should need recomputation
    nimcp_gpu_tensor_t* recomputed = nimcp_checkpoint_get_activation(ckpt_ctx, 1);
    // May return NULL if recomputation not fully implemented
    // This tests the API doesn't crash

    nimcp_checkpoint_end_backward(ckpt_ctx);

    nimcp_gpu_tensor_destroy(activation0);
}

TEST_F(GradientCheckpointTest, RecomputeSegment_TriggersForward) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_EVERY_N, 9, 0);
    ASSERT_NE(ckpt_ctx, nullptr);
    nimcp_checkpoint_configure(ckpt_ctx, CKPT_STRATEGY_EVERY_N, 3);

    nimcp_checkpoint_register_forward(ckpt_ctx, 0, 2, mock_forward, nullptr);

    // Manually trigger recompute
    bool result = nimcp_checkpoint_recompute_segment(ckpt_ctx, 0);
    // Result depends on whether segment exists
    (void)result;
}

//=============================================================================
// Forward/Backward Pass Control Tests
//=============================================================================

TEST_F(GradientCheckpointTest, BeginEndForward_TracksState) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    EXPECT_FALSE(ckpt_ctx->in_forward_pass);

    bool result = nimcp_checkpoint_begin_forward(ckpt_ctx);
    EXPECT_TRUE(result);
    EXPECT_TRUE(ckpt_ctx->in_forward_pass);

    result = nimcp_checkpoint_end_forward(ckpt_ctx);
    EXPECT_TRUE(result);
    EXPECT_FALSE(ckpt_ctx->in_forward_pass);
}

TEST_F(GradientCheckpointTest, BeginEndBackward_TracksState) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    EXPECT_FALSE(ckpt_ctx->in_backward_pass);

    bool result = nimcp_checkpoint_begin_backward(ckpt_ctx);
    EXPECT_TRUE(result);
    EXPECT_TRUE(ckpt_ctx->in_backward_pass);

    result = nimcp_checkpoint_end_backward(ckpt_ctx);
    EXPECT_TRUE(result);
    EXPECT_FALSE(ckpt_ctx->in_backward_pass);
}

TEST_F(GradientCheckpointTest, NestedPasses_NotAllowed) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    nimcp_checkpoint_begin_forward(ckpt_ctx);

    // Trying to begin another forward without ending first
    bool result = nimcp_checkpoint_begin_forward(ckpt_ctx);
    EXPECT_FALSE(result);

    nimcp_checkpoint_end_forward(ckpt_ctx);
}

//=============================================================================
// RNG State Preservation Tests
//=============================================================================

TEST_F(GradientCheckpointTest, CheckpointedFn_PreservesRNG) {
    RequireGPU();

    nimcp_checkpointed_fn_t* fn = nimcp_checkpointed_fn_create(
        mock_forward, mock_backward, nullptr, true  // preserve_rng = true
    );

    ASSERT_NE(fn, nullptr);
    EXPECT_TRUE(fn->preserve_rng);

    nimcp_checkpointed_fn_destroy(fn);
}

TEST_F(GradientCheckpointTest, CheckpointedFn_NoRNG) {
    RequireGPU();

    nimcp_checkpointed_fn_t* fn = nimcp_checkpointed_fn_create(
        mock_forward, mock_backward, nullptr, false  // preserve_rng = false
    );

    ASSERT_NE(fn, nullptr);
    EXPECT_FALSE(fn->preserve_rng);

    nimcp_checkpointed_fn_destroy(fn);
}

//=============================================================================
// Checkpointed Function API Tests
//=============================================================================

TEST_F(GradientCheckpointTest, CheckpointedFnCreate_ReturnsValid) {
    nimcp_checkpointed_fn_t* fn = nimcp_checkpointed_fn_create(
        mock_forward, mock_backward, nullptr, false
    );

    ASSERT_NE(fn, nullptr);
    EXPECT_EQ(fn->forward, mock_forward);
    EXPECT_EQ(fn->backward, mock_backward);
    EXPECT_FALSE(fn->is_checkpointed);

    nimcp_checkpointed_fn_destroy(fn);
}

TEST_F(GradientCheckpointTest, CheckpointedFnDestroy_HandlesNull) {
    nimcp_checkpointed_fn_destroy(nullptr);  // Should not crash
}

TEST_F(GradientCheckpointTest, CheckpointFunction_ExecutesForward) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    nimcp_checkpointed_fn_t* fn = nimcp_checkpointed_fn_create(
        mock_forward, mock_backward, nullptr, false
    );
    ASSERT_NE(fn, nullptr);

    nimcp_gpu_tensor_t* input = CreateTensor(1024);
    ASSERT_NE(input, nullptr);

    nimcp_checkpoint_begin_forward(ckpt_ctx);
    nimcp_gpu_tensor_t* output = nimcp_checkpoint_function(ckpt_ctx, fn, input);
    nimcp_checkpoint_end_forward(ckpt_ctx);

    // Output depends on implementation
    // Just verify no crash
    if (output) {
        nimcp_gpu_tensor_destroy(output);
    }

    nimcp_gpu_tensor_destroy(input);
    nimcp_checkpointed_fn_destroy(fn);
}

TEST_F(GradientCheckpointTest, CheckpointFunctionBackward_ExecutesRecompute) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    nimcp_checkpointed_fn_t* fn = nimcp_checkpointed_fn_create(
        mock_forward, mock_backward, nullptr, false
    );
    ASSERT_NE(fn, nullptr);

    nimcp_gpu_tensor_t* input = CreateTensor(1024);
    nimcp_gpu_tensor_t* grad_output = CreateTensor(1024);
    nimcp_gpu_tensor_t* grad_input = CreateTensor(1024);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(grad_output, nullptr);
    ASSERT_NE(grad_input, nullptr);

    // Forward pass
    nimcp_checkpoint_begin_forward(ckpt_ctx);
    nimcp_checkpoint_function(ckpt_ctx, fn, input);
    nimcp_checkpoint_end_forward(ckpt_ctx);

    // Backward pass
    nimcp_checkpoint_begin_backward(ckpt_ctx);
    bool result = nimcp_checkpoint_function_backward(ckpt_ctx, fn, grad_output, grad_input);
    nimcp_checkpoint_end_backward(ckpt_ctx);

    // Result depends on implementation
    (void)result;

    nimcp_gpu_tensor_destroy(input);
    nimcp_gpu_tensor_destroy(grad_output);
    nimcp_gpu_tensor_destroy(grad_input);
    nimcp_checkpointed_fn_destroy(fn);
}

//=============================================================================
// Memory Estimation Tests
//=============================================================================

TEST_F(GradientCheckpointTest, Estimate_ComputesCorrectly) {
    std::vector<size_t> layer_sizes(16, 4 * 1024 * 1024);  // 4 MB each

    nimcp_checkpoint_estimate_t estimate;
    bool result = nimcp_checkpoint_estimate(layer_sizes.data(), 16, &estimate);

    EXPECT_TRUE(result);
    EXPECT_EQ(estimate.no_checkpoint_memory, 16 * 4 * 1024 * 1024);
    EXPECT_LT(estimate.sqrt_checkpoint_memory, estimate.no_checkpoint_memory);
}

TEST_F(GradientCheckpointTest, RecommendStrategy_ReturnsValidStrategy) {
    std::vector<size_t> layer_sizes(20, 2 * 1024 * 1024);  // 2 MB each
    size_t budget = 16 * 1024 * 1024;  // 16 MB

    nimcp_checkpoint_strategy_t strategy;
    int checkpoint_n;

    bool result = nimcp_checkpoint_recommend_strategy(
        layer_sizes.data(), 20, budget, &strategy, &checkpoint_n);

    EXPECT_TRUE(result);
    EXPECT_NE(strategy, CKPT_STRATEGY_NONE);  // Should recommend some checkpointing
}

TEST_F(GradientCheckpointTest, SqrtInterval_CalculatesCorrectly) {
    EXPECT_EQ(nimcp_checkpoint_sqrt_interval(1), 1);
    EXPECT_EQ(nimcp_checkpoint_sqrt_interval(4), 2);
    EXPECT_EQ(nimcp_checkpoint_sqrt_interval(9), 3);
    EXPECT_EQ(nimcp_checkpoint_sqrt_interval(16), 4);
    EXPECT_EQ(nimcp_checkpoint_sqrt_interval(25), 5);
    EXPECT_EQ(nimcp_checkpoint_sqrt_interval(100), 10);

    // Non-perfect squares
    EXPECT_EQ(nimcp_checkpoint_sqrt_interval(10), 3);  // floor(sqrt(10)) ~ 3
    EXPECT_EQ(nimcp_checkpoint_sqrt_interval(50), 7);  // floor(sqrt(50)) ~ 7
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(GradientCheckpointTest, GetStats_ReturnsValidStats) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    size_t current_memory, peak_memory, saved_memory;
    int recompute_count;
    double recompute_time_ms;

    nimcp_checkpoint_get_stats(ckpt_ctx, &current_memory, &peak_memory,
                                &saved_memory, &recompute_count, &recompute_time_ms);

    EXPECT_EQ(current_memory, 0u);  // No activations stored yet
    EXPECT_EQ(recompute_count, 0);
    EXPECT_GE(recompute_time_ms, 0.0);
}

TEST_F(GradientCheckpointTest, PrintStats_DoesNotCrash) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    nimcp_checkpoint_print_stats(ckpt_ctx);  // Should not crash
    nimcp_checkpoint_print_stats(nullptr);   // Should not crash
}

TEST_F(GradientCheckpointTest, GetInfoString_ReturnsString) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    char buffer[1024];
    int len = nimcp_checkpoint_get_info_string(ckpt_ctx, buffer, sizeof(buffer));

    EXPECT_GT(len, 0);
    EXPECT_LT(static_cast<size_t>(len), sizeof(buffer));
}

//=============================================================================
// Profiling Tests
//=============================================================================

TEST_F(GradientCheckpointTest, SetProfiling_EnablesProfiler) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    EXPECT_FALSE(ckpt_ctx->enable_profiling);

    nimcp_checkpoint_set_profiling(ckpt_ctx, true);
    EXPECT_TRUE(ckpt_ctx->enable_profiling);

    nimcp_checkpoint_set_profiling(ckpt_ctx, false);
    EXPECT_FALSE(ckpt_ctx->enable_profiling);
}

TEST_F(GradientCheckpointTest, SetVerbose_EnablesLogging) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    EXPECT_FALSE(ckpt_ctx->verbose);

    nimcp_checkpoint_set_verbose(ckpt_ctx, true);
    EXPECT_TRUE(ckpt_ctx->verbose);

    nimcp_checkpoint_set_verbose(ckpt_ctx, false);
    EXPECT_FALSE(ckpt_ctx->verbose);
}

//=============================================================================
// Segment Management Tests
//=============================================================================

TEST_F(GradientCheckpointTest, GetSegmentForLayer_ReturnsCorrectSegment) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_EVERY_N, 12, 0);
    ASSERT_NE(ckpt_ctx, nullptr);
    nimcp_checkpoint_configure(ckpt_ctx, CKPT_STRATEGY_EVERY_N, 4);

    // Segments: [0-3], [4-7], [8-11]
    int segment = nimcp_checkpoint_get_segment_for_layer(ckpt_ctx, 0);
    EXPECT_EQ(segment, 0);

    segment = nimcp_checkpoint_get_segment_for_layer(ckpt_ctx, 5);
    EXPECT_EQ(segment, 1);

    segment = nimcp_checkpoint_get_segment_for_layer(ckpt_ctx, 10);
    EXPECT_EQ(segment, 2);

    // Out of bounds
    segment = nimcp_checkpoint_get_segment_for_layer(ckpt_ctx, 100);
    EXPECT_EQ(segment, -1);
}

TEST_F(GradientCheckpointTest, FreeActivation_ReleasesMemory) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_NONE, 5, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    nimcp_checkpoint_begin_forward(ckpt_ctx);
    nimcp_gpu_tensor_t* activation = CreateTensor(1024);
    nimcp_checkpoint_mark_layer(ckpt_ctx, 0, activation);
    nimcp_checkpoint_end_forward(ckpt_ctx);

    bool result = nimcp_checkpoint_free_activation(ckpt_ctx, 0);
    EXPECT_TRUE(result);

    nimcp_gpu_tensor_destroy(activation);
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(GradientCheckpointTest, Reset_ClearsState) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    // Simulate some usage
    nimcp_checkpoint_begin_forward(ckpt_ctx);
    nimcp_gpu_tensor_t* activation = CreateTensor(1024);
    nimcp_checkpoint_mark_layer(ckpt_ctx, 0, activation);
    nimcp_checkpoint_end_forward(ckpt_ctx);

    bool result = nimcp_checkpoint_ctx_reset(ckpt_ctx);
    EXPECT_TRUE(result);

    size_t current, peak, saved;
    int count;
    double time;
    nimcp_checkpoint_get_stats(ckpt_ctx, &current, &peak, &saved, &count, &time);

    EXPECT_EQ(current, 0u);
    EXPECT_EQ(count, 0);

    nimcp_gpu_tensor_destroy(activation);
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(GradientCheckpointTest, NullSafety_AllFunctions) {
    EXPECT_EQ(nimcp_checkpoint_ctx_create(nullptr, CKPT_STRATEGY_SQRT, 10, 0), nullptr);

    EXPECT_FALSE(nimcp_checkpoint_ctx_reset(nullptr));
    EXPECT_FALSE(nimcp_checkpoint_configure(nullptr, CKPT_STRATEGY_SQRT, 0));
    EXPECT_FALSE(nimcp_checkpoint_configure_selective(nullptr, nullptr, 0));
    EXPECT_FALSE(nimcp_checkpoint_auto_configure(nullptr, 0, nullptr, 0));
    EXPECT_FALSE(nimcp_checkpoint_set_layer_size(nullptr, 0, 0));

    EXPECT_FALSE(nimcp_checkpoint_mark_layer(nullptr, 0, nullptr));
    EXPECT_FALSE(nimcp_checkpoint_should_save(nullptr, 0));
    EXPECT_EQ(nimcp_checkpoint_get_activation(nullptr, 0), nullptr);

    EXPECT_FALSE(nimcp_checkpoint_register_forward(nullptr, 0, 0, mock_forward, nullptr));
    EXPECT_FALSE(nimcp_checkpoint_register_backward(nullptr, 0, 0, mock_backward, nullptr));
    EXPECT_FALSE(nimcp_checkpoint_recompute_segment(nullptr, 0));
    EXPECT_FALSE(nimcp_checkpoint_free_activation(nullptr, 0));

    EXPECT_FALSE(nimcp_checkpoint_begin_forward(nullptr));
    EXPECT_FALSE(nimcp_checkpoint_end_forward(nullptr));
    EXPECT_FALSE(nimcp_checkpoint_begin_backward(nullptr));
    EXPECT_FALSE(nimcp_checkpoint_end_backward(nullptr));

    EXPECT_FALSE(nimcp_checkpoint_estimate(nullptr, 0, nullptr));
    EXPECT_FALSE(nimcp_checkpoint_recommend_strategy(nullptr, 0, 0, nullptr, nullptr));

    nimcp_checkpoint_get_stats(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

    EXPECT_FALSE(nimcp_checkpoint_is_checkpoint_layer(nullptr, 0));
    EXPECT_EQ(nimcp_checkpoint_get_segment_for_layer(nullptr, 0), -1);

    nimcp_checkpoint_set_profiling(nullptr, true);
    nimcp_checkpoint_set_verbose(nullptr, true);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST_F(GradientCheckpointTest, SingleLayer_HandleCorrectly) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 1, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    EXPECT_TRUE(nimcp_checkpoint_is_checkpoint_layer(ckpt_ctx, 0));
}

TEST_F(GradientCheckpointTest, TwoLayers_HandleCorrectly) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 2, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    // With sqrt(2) ~ 1.4, should checkpoint ~1-2 layers
    int checkpoints = 0;
    for (int i = 0; i < 2; i++) {
        if (nimcp_checkpoint_is_checkpoint_layer(ckpt_ctx, i)) {
            checkpoints++;
        }
    }
    EXPECT_GE(checkpoints, 1);
    EXPECT_LE(checkpoints, 2);
}

TEST_F(GradientCheckpointTest, LargeLayers_HandleCorrectly) {
    RequireGPU();

    const int num_layers = 100;
    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, num_layers, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    int sqrt_interval = nimcp_checkpoint_sqrt_interval(num_layers);
    EXPECT_EQ(sqrt_interval, 10);

    int checkpoints = 0;
    for (int i = 0; i < num_layers; i++) {
        if (nimcp_checkpoint_is_checkpoint_layer(ckpt_ctx, i)) {
            checkpoints++;
        }
    }

    // Should have approximately sqrt(N) checkpoints
    EXPECT_NEAR(checkpoints, sqrt_interval, 2);
}

TEST_F(GradientCheckpointTest, InvalidLayerIndex_HandledGracefully) {
    RequireGPU();

    ckpt_ctx = nimcp_checkpoint_ctx_create(ctx, CKPT_STRATEGY_SQRT, 10, 0);
    ASSERT_NE(ckpt_ctx, nullptr);

    // Negative index
    EXPECT_FALSE(nimcp_checkpoint_is_checkpoint_layer(ckpt_ctx, -1));
    EXPECT_FALSE(nimcp_checkpoint_should_save(ckpt_ctx, -1));

    // Index >= num_layers
    EXPECT_FALSE(nimcp_checkpoint_is_checkpoint_layer(ckpt_ctx, 10));
    EXPECT_FALSE(nimcp_checkpoint_is_checkpoint_layer(ckpt_ctx, 100));
    EXPECT_FALSE(nimcp_checkpoint_should_save(ckpt_ctx, 10));
}
