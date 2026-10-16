/* ============================================================================
 * Unit Tests: GPU Optimizer Kernels with Recovery Integration
 * ============================================================================
 * WHAT: Unit tests for GPU optimizer operations with recovery support
 * WHY:  Validate SGD, Adam, AdamW, RMSprop, AdaGrad optimizers
 * HOW:  Test optimizer steps and verify parameter updates
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/training/nimcp_training_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#endif

namespace {

constexpr float TOLERANCE = 1e-4f;
constexpr float RELAXED_TOLERANCE = 1e-3f;

class OptimizerKernelsTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        ctx_ = nimcp_gpu_context_create(0);
        if (!ctx_) {
            GTEST_SKIP() << "No GPU available - skipping test";
        }
        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (ctx_) {
            nimcp_gpu_context_destroy(ctx_);
            ctx_ = NULL;
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_context_t* ctx_ = NULL;

    nimcp_gpu_tensor_t* create_tensor_with_values(const std::vector<size_t>& dims,
                                                   const std::vector<float>& values) {
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(
            ctx_, dims.data(), static_cast<uint32_t>(dims.size()), NIMCP_GPU_PRECISION_FP32);
        if (!tensor) return nullptr;
        cudaMemcpy(tensor->data, values.data(), values.size() * sizeof(float),
                   cudaMemcpyHostToDevice);
        return tensor;
    }

    nimcp_gpu_tensor_t* create_constant_tensor(const std::vector<size_t>& dims, float value) {
        std::vector<float> data(1);
        for (const auto& d : dims) data.resize(data.size() * d, value);
        data.resize(data.size(), value);

        size_t numel = 1;
        for (const auto& d : dims) numel *= d;
        std::vector<float> values(numel, value);
        return create_tensor_with_values(dims, values);
    }

    std::vector<float> download_tensor(nimcp_gpu_tensor_t* tensor) {
        std::vector<float> data(tensor->numel);
        cudaMemcpy(data.data(), tensor->data, tensor->numel * sizeof(float),
                   cudaMemcpyDeviceToHost);
        return data;
    }
#endif
};

/* ============================================================================
 * Test: Optimizer State Creation
 * ============================================================================ */
TEST_F(OptimizerKernelsTest, StateCreationSGD) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {64, 32};
    nimcp_gpu_tensor_t* param = create_constant_tensor(dims, 1.0f);
    ASSERT_NE(param, nullptr);

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx_, NIMCP_OPTIM_SGD, param, 0.01f);
    ASSERT_NE(state, nullptr) << "SGD state creation failed";

    EXPECT_EQ(state->type, NIMCP_OPTIM_SGD);
    EXPECT_NEAR(state->lr, 0.01f, TOLERANCE);
    EXPECT_EQ(state->t, 0u);

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(param);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(OptimizerKernelsTest, StateCreationAdam) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {64, 32};
    nimcp_gpu_tensor_t* param = create_constant_tensor(dims, 1.0f);
    ASSERT_NE(param, nullptr);

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx_, NIMCP_OPTIM_ADAM, param, 0.001f);
    ASSERT_NE(state, nullptr) << "Adam state creation failed";

    EXPECT_EQ(state->type, NIMCP_OPTIM_ADAM);
    EXPECT_NEAR(state->lr, 0.001f, TOLERANCE);
    EXPECT_NE(state->m, nullptr) << "First moment tensor not created";
    EXPECT_NE(state->v, nullptr) << "Second moment tensor not created";
    EXPECT_NEAR(state->beta1, 0.9f, TOLERANCE);
    EXPECT_NEAR(state->beta2, 0.999f, TOLERANCE);

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(param);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(OptimizerKernelsTest, StateCreationAdamW) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {128};
    nimcp_gpu_tensor_t* param = create_constant_tensor(dims, 1.0f);

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx_, NIMCP_OPTIM_ADAMW, param, 0.001f);
    ASSERT_NE(state, nullptr) << "AdamW state creation failed";

    EXPECT_EQ(state->type, NIMCP_OPTIM_ADAMW);
    EXPECT_NE(state->m, nullptr);
    EXPECT_NE(state->v, nullptr);

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(param);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(OptimizerKernelsTest, StateCreationRMSprop) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {128};
    nimcp_gpu_tensor_t* param = create_constant_tensor(dims, 1.0f);

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx_, NIMCP_OPTIM_RMSPROP, param, 0.01f);
    ASSERT_NE(state, nullptr) << "RMSprop state creation failed";

    EXPECT_EQ(state->type, NIMCP_OPTIM_RMSPROP);
    EXPECT_NE(state->v, nullptr);  // Squared gradient moving average

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(param);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(OptimizerKernelsTest, StateCreationAdaGrad) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {128};
    nimcp_gpu_tensor_t* param = create_constant_tensor(dims, 1.0f);

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx_, NIMCP_OPTIM_ADAGRAD, param, 0.01f);
    ASSERT_NE(state, nullptr) << "AdaGrad state creation failed";

    EXPECT_EQ(state->type, NIMCP_OPTIM_ADAGRAD);
    EXPECT_NE(state->v, nullptr);  // Accumulated squared gradients

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(param);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: SGD Optimizer Step
 * ============================================================================ */
TEST_F(OptimizerKernelsTest, SGDStep) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {100};
    std::vector<float> param_data(100, 1.0f);
    std::vector<float> grad_data(100, 0.1f);

    nimcp_gpu_tensor_t* param = create_tensor_with_values(dims, param_data);
    nimcp_gpu_tensor_t* grad = create_tensor_with_values(dims, grad_data);

    float lr = 0.1f;
    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx_, NIMCP_OPTIM_SGD, param, lr);
    ASSERT_NE(state, nullptr);

    bool success = nimcp_gpu_optim_sgd(ctx_, param, grad, state);
    ASSERT_TRUE(success) << "SGD step failed";

    // Verify: param = param - lr * grad = 1.0 - 0.1 * 0.1 = 0.99
    std::vector<float> result = download_tensor(param);
    for (size_t i = 0; i < 100; i++) {
        EXPECT_NEAR(result[i], 0.99f, TOLERANCE)
            << "SGD update incorrect at index " << i;
    }

    EXPECT_EQ(state->t, 1u) << "Timestep should be incremented";

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(param);
    nimcp_gpu_tensor_destroy(grad);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Adam Optimizer Step
 * ============================================================================ */
TEST_F(OptimizerKernelsTest, AdamStep) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {64};
    std::vector<float> param_data(64, 1.0f);
    std::vector<float> grad_data(64, 0.5f);

    nimcp_gpu_tensor_t* param = create_tensor_with_values(dims, param_data);
    nimcp_gpu_tensor_t* grad = create_tensor_with_values(dims, grad_data);

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx_, NIMCP_OPTIM_ADAM, param, 0.001f);
    ASSERT_NE(state, nullptr);

    // Run multiple steps
    for (int i = 0; i < 10; i++) {
        bool success = nimcp_gpu_optim_adam(ctx_, param, grad, state);
        ASSERT_TRUE(success) << "Adam step " << i << " failed";
    }

    EXPECT_EQ(state->t, 10u) << "Timestep should be 10";

    // Parameters should have decreased
    std::vector<float> result = download_tensor(param);
    for (size_t i = 0; i < 64; i++) {
        EXPECT_LT(result[i], 1.0f) << "Parameter should have decreased at index " << i;
    }

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(param);
    nimcp_gpu_tensor_destroy(grad);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: AdamW Optimizer Step
 * ============================================================================ */
TEST_F(OptimizerKernelsTest, AdamWStep) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {64};
    std::vector<float> param_data(64, 1.0f);
    std::vector<float> grad_data(64, 0.5f);

    nimcp_gpu_tensor_t* param = create_tensor_with_values(dims, param_data);
    nimcp_gpu_tensor_t* grad = create_tensor_with_values(dims, grad_data);

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx_, NIMCP_OPTIM_ADAMW, param, 0.001f);
    ASSERT_NE(state, nullptr);
    state->weight_decay = 0.01f;  // Enable weight decay

    // Run multiple steps
    for (int i = 0; i < 10; i++) {
        bool success = nimcp_gpu_optim_adamw(ctx_, param, grad, state);
        ASSERT_TRUE(success) << "AdamW step " << i << " failed";
    }

    std::vector<float> result = download_tensor(param);
    for (size_t i = 0; i < 64; i++) {
        EXPECT_LT(result[i], 1.0f) << "Parameter should have decreased at index " << i;
    }

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(param);
    nimcp_gpu_tensor_destroy(grad);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: RMSprop Optimizer Step
 * ============================================================================ */
TEST_F(OptimizerKernelsTest, RMSpropStep) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {64};
    std::vector<float> param_data(64, 1.0f);
    std::vector<float> grad_data(64, 0.5f);

    nimcp_gpu_tensor_t* param = create_tensor_with_values(dims, param_data);
    nimcp_gpu_tensor_t* grad = create_tensor_with_values(dims, grad_data);

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx_, NIMCP_OPTIM_RMSPROP, param, 0.01f);
    ASSERT_NE(state, nullptr);

    for (int i = 0; i < 10; i++) {
        bool success = nimcp_gpu_optim_rmsprop(ctx_, param, grad, state);
        ASSERT_TRUE(success) << "RMSprop step " << i << " failed";
    }

    std::vector<float> result = download_tensor(param);
    for (size_t i = 0; i < 64; i++) {
        EXPECT_LT(result[i], 1.0f) << "Parameter should have decreased at index " << i;
    }

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(param);
    nimcp_gpu_tensor_destroy(grad);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: AdaGrad Optimizer Step
 * ============================================================================ */
TEST_F(OptimizerKernelsTest, AdaGradStep) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {64};
    std::vector<float> param_data(64, 1.0f);
    std::vector<float> grad_data(64, 0.5f);

    nimcp_gpu_tensor_t* param = create_tensor_with_values(dims, param_data);
    nimcp_gpu_tensor_t* grad = create_tensor_with_values(dims, grad_data);

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx_, NIMCP_OPTIM_ADAGRAD, param, 0.01f);
    ASSERT_NE(state, nullptr);

    for (int i = 0; i < 10; i++) {
        bool success = nimcp_gpu_optim_adagrad(ctx_, param, grad, state);
        ASSERT_TRUE(success) << "AdaGrad step " << i << " failed";
    }

    std::vector<float> result = download_tensor(param);
    for (size_t i = 0; i < 64; i++) {
        EXPECT_LT(result[i], 1.0f) << "Parameter should have decreased at index " << i;
    }

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(param);
    nimcp_gpu_tensor_destroy(grad);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Learning Rate Schedulers
 * ============================================================================ */
TEST_F(OptimizerKernelsTest, LRStepScheduler) {
#ifdef NIMCP_ENABLE_CUDA
    float initial_lr = 0.1f;
    uint64_t step_size = 10;
    float gamma = 0.1f;

    // Before first decay
    float lr = nimcp_lr_step(initial_lr, 5, step_size, gamma);
    EXPECT_NEAR(lr, 0.1f, TOLERANCE);

    // After first decay
    lr = nimcp_lr_step(initial_lr, 10, step_size, gamma);
    EXPECT_NEAR(lr, 0.01f, TOLERANCE);

    // After second decay
    lr = nimcp_lr_step(initial_lr, 25, step_size, gamma);
    EXPECT_NEAR(lr, 0.001f, TOLERANCE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(OptimizerKernelsTest, LRCosineScheduler) {
#ifdef NIMCP_ENABLE_CUDA
    float max_lr = 0.1f;
    float min_lr = 0.001f;
    uint64_t total_steps = 100;

    // Start: should be max_lr
    float lr = nimcp_lr_cosine(max_lr, min_lr, 0, total_steps);
    EXPECT_NEAR(lr, max_lr, TOLERANCE);

    // Middle: should be between min and max
    lr = nimcp_lr_cosine(max_lr, min_lr, 50, total_steps);
    EXPECT_GT(lr, min_lr);
    EXPECT_LT(lr, max_lr);

    // End: should be min_lr
    lr = nimcp_lr_cosine(max_lr, min_lr, 100, total_steps);
    EXPECT_NEAR(lr, min_lr, TOLERANCE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(OptimizerKernelsTest, LRWarmupLinear) {
#ifdef NIMCP_ENABLE_CUDA
    float max_lr = 0.1f;
    uint64_t warmup_steps = 10;
    uint64_t total_steps = 100;

    // During warmup: linear increase
    float lr = nimcp_lr_warmup_linear(max_lr, 0, warmup_steps, total_steps);
    EXPECT_NEAR(lr, 0.0f, TOLERANCE);

    lr = nimcp_lr_warmup_linear(max_lr, 5, warmup_steps, total_steps);
    EXPECT_NEAR(lr, 0.05f, TOLERANCE);

    // At warmup end: max_lr
    lr = nimcp_lr_warmup_linear(max_lr, 10, warmup_steps, total_steps);
    EXPECT_NEAR(lr, max_lr, TOLERANCE);

    // After warmup: linear decay
    lr = nimcp_lr_warmup_linear(max_lr, 55, warmup_steps, total_steps);
    EXPECT_LT(lr, max_lr);
    EXPECT_GT(lr, 0.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

TEST_F(OptimizerKernelsTest, LRExponential) {
#ifdef NIMCP_ENABLE_CUDA
    float initial_lr = 0.1f;
    float decay_rate = 0.01f;

    float lr0 = nimcp_lr_exponential(initial_lr, 0, decay_rate);
    EXPECT_NEAR(lr0, initial_lr, TOLERANCE);

    float lr100 = nimcp_lr_exponential(initial_lr, 100, decay_rate);
    float expected = initial_lr * std::exp(-decay_rate * 100.0f);
    EXPECT_NEAR(lr100, expected, TOLERANCE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Large Parameter Update
 * ============================================================================ */
TEST_F(OptimizerKernelsTest, LargeParameterUpdate) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";

    std::vector<size_t> dims = {1024, 512};  // 512K parameters
    nimcp_gpu_tensor_t* param = create_constant_tensor(dims, 1.0f);
    nimcp_gpu_tensor_t* grad = create_constant_tensor(dims, 0.1f);
    ASSERT_NE(param, nullptr);
    ASSERT_NE(grad, nullptr);

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx_, NIMCP_OPTIM_ADAM, param, 0.001f);
    ASSERT_NE(state, nullptr);

    // Run several steps
    for (int i = 0; i < 5; i++) {
        bool success = nimcp_gpu_optim_adam(ctx_, param, grad, state);
        ASSERT_TRUE(success) << "Large parameter Adam step " << i << " failed";
    }

    std::vector<float> result = download_tensor(param);

    // Sample check (all values should be similar due to constant gradient)
    EXPECT_LT(result[0], 1.0f);
    EXPECT_LT(result[result.size()/2], 1.0f);
    EXPECT_LT(result[result.size()-1], 1.0f);

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(param);
    nimcp_gpu_tensor_destroy(grad);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Null Parameter Handling
 * ============================================================================ */
TEST_F(OptimizerKernelsTest, NullParameterHandling) {
#ifdef NIMCP_ENABLE_CUDA
    std::vector<size_t> dims = {10};
    nimcp_gpu_tensor_t* tensor = create_constant_tensor(dims, 1.0f);
    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx_, NIMCP_OPTIM_SGD, tensor, 0.01f);

    // NULL context
    EXPECT_FALSE(nimcp_gpu_optim_sgd(nullptr, tensor, tensor, state));

    // NULL param
    EXPECT_FALSE(nimcp_gpu_optim_sgd(ctx_, nullptr, tensor, state));

    // NULL grad
    EXPECT_FALSE(nimcp_gpu_optim_sgd(ctx_, tensor, nullptr, state));

    // NULL state
    EXPECT_FALSE(nimcp_gpu_optim_sgd(ctx_, tensor, tensor, nullptr));

    // State creation with NULL
    EXPECT_EQ(nimcp_optim_state_create(nullptr, NIMCP_OPTIM_SGD, tensor, 0.01f), nullptr);
    EXPECT_EQ(nimcp_optim_state_create(ctx_, NIMCP_OPTIM_SGD, nullptr, 0.01f), nullptr);

    nimcp_optim_state_destroy(state);
    nimcp_gpu_tensor_destroy(tensor);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: State Destruction Safety
 * ============================================================================ */
TEST_F(OptimizerKernelsTest, StateDestructionSafety) {
#ifdef NIMCP_ENABLE_CUDA
    // Destroying NULL should be safe
    nimcp_optim_state_destroy(nullptr);

    // Create and immediately destroy
    std::vector<size_t> dims = {10};
    nimcp_gpu_tensor_t* tensor = create_constant_tensor(dims, 1.0f);

    nimcp_optim_state_t* state = nimcp_optim_state_create(ctx_, NIMCP_OPTIM_ADAM, tensor, 0.001f);
    nimcp_optim_state_destroy(state);

    nimcp_gpu_tensor_destroy(tensor);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery System Active
 * ============================================================================ */
TEST_F(OptimizerKernelsTest, RecoverySystemActive) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr) << "GPU context required";
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized())
        << "Recovery system should be initialized";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
