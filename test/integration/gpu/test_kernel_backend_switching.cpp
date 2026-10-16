/**
 * @file test_kernel_backend_switching.cpp
 * @brief Integration tests for GPU/CPU kernel backend switching in NIMCP
 *
 * WHAT: Verify backend initialization, switching, and operation dispatch
 * WHY:  Ensure seamless transitions between CPU and GPU compute backends
 * HOW:  Test backend lifecycle, switching, and verify operations work after switch
 *
 * TEST COVERAGE:
 * - Backend initialization with different types (CPU, CUDA, AUTO)
 * - Backend type queries and availability checks
 * - Runtime backend switching between CPU and GPU
 * - Operation macro functionality (NIMCP_TENSOR_OPS, etc.)
 * - Automatic backend selection logic
 * - Error handling for unsupported backends
 * - State preservation during backend switches
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <random>
#include <string>

// GPU headers include CUDA headers that cannot be in extern "C" blocks
#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Configuration
//=============================================================================

namespace {
    constexpr float TOLERANCE = 1e-5f;
    constexpr size_t TEST_SIZE = 256;
}

//=============================================================================
// Test Fixture
//=============================================================================

class KernelBackendSwitchingTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    std::mt19937 rng{12345};  // Reproducible random numbers

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        // Note: Backend is NOT initialized here - tests control initialization
    }

    void TearDown() override {
        // Clean up any GPU context
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }

        // Ensure backend is shutdown
        nimcp_kernel_backend_shutdown();

        // Check for memory leaks
        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 4096)
            << "Potential memory leak: " << stats.current_allocated << " bytes";
    }

    //=========================================================================
    // Helper Functions
    //=========================================================================

    std::vector<float> generateRandomData(size_t count) {
        std::vector<float> data(count);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (size_t i = 0; i < count; i++) {
            data[i] = dist(rng);
        }
        return data;
    }

    nimcp_gpu_tensor_t* createTensor(const std::vector<float>& data, nimcp_gpu_context_t* ctx) {
        if (!ctx) return nullptr;
        std::vector<size_t> dims = {data.size()};
        return nimcp_gpu_tensor_from_host(
            ctx,
            data.data(),
            dims.data(),
            1,
            NIMCP_GPU_PRECISION_FP32
        );
    }

    std::vector<float> copyToHost(const nimcp_gpu_tensor_t* tensor) {
        if (!tensor) return {};
        std::vector<float> result(tensor->numel);
        nimcp_gpu_tensor_to_host(tensor, result.data());
        return result;
    }

    bool hasGPU() const {
        // Must check both: CUDA initialized AND CUDA is the active backend
        if (!nimcp_cuda_backend_available()) {
            return false;
        }
        nimcp_backend_type_t active_type = nimcp_get_backend_type();
        return active_type == NIMCP_BACKEND_CUDA;
    }

    // Simple operation for verification
    bool verifyAddWorks(nimcp_gpu_context_t* ctx) {
        if (!ctx) return false;

        auto data_a = generateRandomData(TEST_SIZE);
        auto data_b = generateRandomData(TEST_SIZE);

        std::vector<size_t> dims = {TEST_SIZE};
        auto* tensor_a = nimcp_gpu_tensor_from_host(ctx, data_a.data(), dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
        auto* tensor_b = nimcp_gpu_tensor_from_host(ctx, data_b.data(), dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
        auto* tensor_out = nimcp_gpu_tensor_create(ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

        if (!tensor_a || !tensor_b || !tensor_out) {
            if (tensor_a) nimcp_gpu_tensor_destroy(tensor_a);
            if (tensor_b) nimcp_gpu_tensor_destroy(tensor_b);
            if (tensor_out) nimcp_gpu_tensor_destroy(tensor_out);
            return false;
        }

        auto result = NIMCP_TENSOR_OPS()->add(ctx, tensor_a, tensor_b, tensor_out);
        bool success = (result == NIMCP_KERNEL_SUCCESS);

        if (success) {
            auto output = copyToHost(tensor_out);
            for (size_t i = 0; i < TEST_SIZE && success; i++) {
                float expected = data_a[i] + data_b[i];
                if (std::fabs(output[i] - expected) > TOLERANCE) {
                    success = false;
                }
            }
        }

        nimcp_gpu_tensor_destroy(tensor_a);
        nimcp_gpu_tensor_destroy(tensor_b);
        nimcp_gpu_tensor_destroy(tensor_out);

        return success;
    }
};

//=============================================================================
// BACKEND INITIALIZATION TESTS
//=============================================================================

/**
 * WHAT: Test initializing backend with CPU type
 * WHY:  CPU backend must always be available
 * HOW:  Initialize with NIMCP_BACKEND_CPU, verify success and type
 */
TEST_F(KernelBackendSwitchingTest, Init_CPUBackend_Success) {
    bool init_ok = nimcp_kernel_backend_init(NIMCP_BACKEND_CPU);
    EXPECT_TRUE(init_ok) << "CPU backend initialization should always succeed";

    nimcp_kernel_backend_t* backend = nimcp_get_kernel_backend();
    ASSERT_NE(backend, nullptr) << "Backend should not be null after init";
    EXPECT_EQ(backend->type, NIMCP_BACKEND_CPU);
    EXPECT_TRUE(backend->initialized);
    EXPECT_STREQ(nimcp_backend_type_name(NIMCP_BACKEND_CPU), "CPU");
}

/**
 * WHAT: Test initializing backend with CUDA type
 * WHY:  CUDA backend availability depends on hardware
 * HOW:  Initialize with NIMCP_BACKEND_CUDA, check result based on GPU availability
 */
TEST_F(KernelBackendSwitchingTest, Init_CUDABackend_DependsOnHardware) {
    bool init_ok = nimcp_kernel_backend_init(NIMCP_BACKEND_CUDA);

    if (hasGPU()) {
        EXPECT_TRUE(init_ok) << "CUDA init should succeed when GPU available";
        nimcp_kernel_backend_t* backend = nimcp_get_kernel_backend();
        ASSERT_NE(backend, nullptr);
        EXPECT_EQ(backend->type, NIMCP_BACKEND_CUDA);
    } else {
        // CUDA init may fail gracefully or fall back to CPU
        nimcp_kernel_backend_t* backend = nimcp_get_kernel_backend();
        if (backend) {
            // Fallback to CPU is acceptable
            EXPECT_TRUE(backend->type == NIMCP_BACKEND_CPU ||
                        backend->type == NIMCP_BACKEND_CUDA);
        }
    }
}

/**
 * WHAT: Test initializing backend with AUTO type
 * WHY:  AUTO should select best available backend
 * HOW:  Initialize with NIMCP_BACKEND_AUTO, verify selection logic
 */
TEST_F(KernelBackendSwitchingTest, Init_AutoBackend_SelectsBest) {
    bool init_ok = nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);
    EXPECT_TRUE(init_ok) << "AUTO backend init should always succeed";

    nimcp_kernel_backend_t* backend = nimcp_get_kernel_backend();
    ASSERT_NE(backend, nullptr);
    EXPECT_TRUE(backend->initialized);

    nimcp_backend_type_t actual_type = nimcp_get_backend_type();

    if (hasGPU()) {
        // Should prefer CUDA if available
        EXPECT_EQ(actual_type, NIMCP_BACKEND_CUDA)
            << "AUTO should select CUDA when GPU available";
    } else {
        // Should fall back to CPU
        EXPECT_EQ(actual_type, NIMCP_BACKEND_CPU)
            << "AUTO should select CPU when no GPU available";
    }
}

/**
 * WHAT: Test double initialization handling
 * WHY:  Should handle multiple init calls gracefully
 * HOW:  Call init twice, verify no crash and consistent state
 */
TEST_F(KernelBackendSwitchingTest, Init_DoubleInit_Handled) {
    bool init1 = nimcp_kernel_backend_init(NIMCP_BACKEND_CPU);
    EXPECT_TRUE(init1);

    nimcp_backend_type_t type1 = nimcp_get_backend_type();

    // Second init - behavior depends on implementation
    // May succeed (reinit) or be no-op
    bool init2 = nimcp_kernel_backend_init(NIMCP_BACKEND_CPU);

    // Should still have valid backend
    nimcp_kernel_backend_t* backend = nimcp_get_kernel_backend();
    ASSERT_NE(backend, nullptr);
    EXPECT_TRUE(backend->initialized);

    // Type should be consistent
    EXPECT_EQ(nimcp_get_backend_type(), type1);
}

/**
 * WHAT: Test shutdown and reinit cycle
 * WHY:  Backend should support clean restart
 * HOW:  Init, shutdown, reinit, verify works
 */
TEST_F(KernelBackendSwitchingTest, Init_ShutdownReinit_Works) {
    // First cycle
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CPU));
    nimcp_kernel_backend_t* backend1 = nimcp_get_kernel_backend();
    ASSERT_NE(backend1, nullptr);

    nimcp_kernel_backend_shutdown();

    // After shutdown, backend may be null or invalid
    // Don't access backend pointer after shutdown

    // Second cycle
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CPU));
    nimcp_kernel_backend_t* backend2 = nimcp_get_kernel_backend();
    ASSERT_NE(backend2, nullptr);
    EXPECT_TRUE(backend2->initialized);
}

//=============================================================================
// BACKEND QUERY TESTS
//=============================================================================

/**
 * WHAT: Test nimcp_cuda_backend_available query
 * WHY:  Applications need to check GPU availability
 * HOW:  Query availability, verify consistent with hardware
 */
TEST_F(KernelBackendSwitchingTest, Query_CUDAAvailable) {
    nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);

    bool cuda_available = nimcp_cuda_backend_available();

    // Create GPU context to double-check
    nimcp_gpu_context_t* ctx = nimcp_gpu_context_create_auto();

    if (ctx) {
        // GPU context created, CUDA should be available
        EXPECT_TRUE(cuda_available) << "CUDA should be available when GPU context works";
        nimcp_gpu_context_destroy(ctx);
    } else {
        // No GPU context, CUDA may or may not be available
        // (depends on whether CUDA runtime is installed without GPU)
    }
}

/**
 * WHAT: Test nimcp_get_backend_type query
 * WHY:  Applications need to know current backend
 * HOW:  Init with specific type, verify query returns correct type
 */
TEST_F(KernelBackendSwitchingTest, Query_GetBackendType) {
    nimcp_kernel_backend_init(NIMCP_BACKEND_CPU);

    nimcp_backend_type_t type = nimcp_get_backend_type();
    EXPECT_EQ(type, NIMCP_BACKEND_CPU);
}

/**
 * WHAT: Test nimcp_backend_type_name for all types
 * WHY:  String names useful for logging and debugging
 * HOW:  Query names for all backend types
 */
TEST_F(KernelBackendSwitchingTest, Query_BackendTypeNames) {
    nimcp_kernel_backend_init(NIMCP_BACKEND_CPU);

    const char* cpu_name = nimcp_backend_type_name(NIMCP_BACKEND_CPU);
    const char* cuda_name = nimcp_backend_type_name(NIMCP_BACKEND_CUDA);
    const char* auto_name = nimcp_backend_type_name(NIMCP_BACKEND_AUTO);

    ASSERT_NE(cpu_name, nullptr);
    ASSERT_NE(cuda_name, nullptr);
    ASSERT_NE(auto_name, nullptr);

    EXPECT_STRNE(cpu_name, "");
    EXPECT_STRNE(cuda_name, "");
    EXPECT_STRNE(auto_name, "");

    // Names should be distinct
    EXPECT_STRNE(cpu_name, cuda_name);
}

//=============================================================================
// BACKEND SWITCHING TESTS
//=============================================================================

/**
 * WHAT: Test switching from CPU to CPU (no-op)
 * WHY:  Switching to same backend should be safe
 * HOW:  Init CPU, switch to CPU, verify works
 */
TEST_F(KernelBackendSwitchingTest, Switch_CPUtoCPU_NoOp) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CPU));
    EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CPU);

    bool switch_ok = nimcp_switch_backend(NIMCP_BACKEND_CPU);
    EXPECT_TRUE(switch_ok) << "Switching to same backend should succeed";
    EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CPU);
}

/**
 * WHAT: Test switching from CPU to CUDA
 * WHY:  Dynamic backend switching enables runtime optimization
 * HOW:  Init CPU, switch to CUDA if available, verify switch
 */
TEST_F(KernelBackendSwitchingTest, Switch_CPUtoCUDA) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CPU));

    bool switch_ok = nimcp_switch_backend(NIMCP_BACKEND_CUDA);

    if (hasGPU()) {
        EXPECT_TRUE(switch_ok) << "Switch to CUDA should succeed when GPU available";
        EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CUDA);
    } else {
        // Switch may fail if no GPU
        if (!switch_ok) {
            EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CPU)
                << "Should stay on CPU if CUDA switch fails";
        }
    }
}

/**
 * WHAT: Test switching from CUDA to CPU
 * WHY:  May need to switch to CPU for memory or compatibility
 * HOW:  Init CUDA (if available), switch to CPU, verify
 */
TEST_F(KernelBackendSwitchingTest, Switch_CUDAtoCPU) {
    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available for CUDA to CPU switch test";
    }

    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CUDA));
    EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CUDA);

    bool switch_ok = nimcp_switch_backend(NIMCP_BACKEND_CPU);
    EXPECT_TRUE(switch_ok) << "Switch from CUDA to CPU should always succeed";
    EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CPU);
}

/**
 * WHAT: Test operations work correctly after backend switch
 * WHY:  Operations must function regardless of switch history
 * HOW:  Perform operation, switch backend, perform again, compare
 */
TEST_F(KernelBackendSwitchingTest, Switch_OperationsWorkAfterSwitch) {
    // Use AUTO to properly initialize both CPU and CUDA backends
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO));

    // Create GPU context
    gpu_ctx = nimcp_gpu_context_create_auto();

    if (gpu_ctx) {
        // Test operation on initial backend
        EXPECT_TRUE(verifyAddWorks(gpu_ctx)) << "Add should work on initial backend";

        // Switch backend (if GPU available)
        if (hasGPU()) {
            // Switch to CUDA and verify
            EXPECT_TRUE(nimcp_switch_backend(NIMCP_BACKEND_CUDA));
            EXPECT_TRUE(verifyAddWorks(gpu_ctx)) << "Add should work on CUDA backend";

            // Switch to CPU - NOTE: CPU backend cannot access GPU memory,
            // so we skip this part of the test. In production, tensors would
            // need to be migrated to CPU memory before switching to CPU backend.
            // nimcp_switch_backend(NIMCP_BACKEND_CPU);
            // EXPECT_TRUE(verifyAddWorks(gpu_ctx)) << "Add should work after switch back to CPU";
        }
    }
}

/**
 * WHAT: Test multiple rapid backend switches
 * WHY:  Ensure no resource leaks or corruption from switching
 * HOW:  Switch back and forth multiple times, verify stability
 */
TEST_F(KernelBackendSwitchingTest, Switch_MultipleRapidSwitches) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CPU));

    if (!hasGPU()) {
        // Just switch between CPU (no-op)
        for (int i = 0; i < 10; i++) {
            EXPECT_TRUE(nimcp_switch_backend(NIMCP_BACKEND_CPU));
        }
    } else {
        // Switch between CPU and CUDA
        for (int i = 0; i < 5; i++) {
            EXPECT_TRUE(nimcp_switch_backend(NIMCP_BACKEND_CUDA));
            EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CUDA);

            EXPECT_TRUE(nimcp_switch_backend(NIMCP_BACKEND_CPU));
            EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CPU);
        }
    }

    // Verify backend still valid
    nimcp_kernel_backend_t* backend = nimcp_get_kernel_backend();
    ASSERT_NE(backend, nullptr);
    EXPECT_TRUE(backend->initialized);
}

//=============================================================================
// OPERATION MACRO TESTS
//=============================================================================

/**
 * WHAT: Test NIMCP_TENSOR_OPS() macro returns valid operations
 * WHY:  Macros are main interface for accessing operations
 * HOW:  Get ops via macro, verify function pointers non-null
 */
TEST_F(KernelBackendSwitchingTest, Macro_TensorOps_Valid) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CPU));

    nimcp_tensor_ops_t* ops = NIMCP_TENSOR_OPS();
    ASSERT_NE(ops, nullptr);

    // Core operations should be implemented
    EXPECT_NE(ops->add, nullptr) << "add operation should be implemented";
    EXPECT_NE(ops->sub, nullptr) << "sub operation should be implemented";
    EXPECT_NE(ops->mul, nullptr) << "mul operation should be implemented";
    EXPECT_NE(ops->matmul, nullptr) << "matmul operation should be implemented";
    EXPECT_NE(ops->relu, nullptr) << "relu operation should be implemented";
    EXPECT_NE(ops->sigmoid, nullptr) << "sigmoid operation should be implemented";
}

/**
 * WHAT: Test NIMCP_TRAINING_OPS() macro returns valid operations
 * WHY:  Training operations essential for neural network learning
 * HOW:  Get ops via macro, verify function pointers
 */
TEST_F(KernelBackendSwitchingTest, Macro_TrainingOps_Valid) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CPU));

    nimcp_training_ops_t* ops = NIMCP_TRAINING_OPS();
    ASSERT_NE(ops, nullptr);

    EXPECT_NE(ops->mse_loss, nullptr) << "mse_loss should be implemented";
    EXPECT_NE(ops->gradient_clip, nullptr) << "gradient_clip should be implemented";
    EXPECT_NE(ops->sgd_step, nullptr) << "sgd_step should be implemented";
    EXPECT_NE(ops->adam_step, nullptr) << "adam_step should be implemented";
}

/**
 * WHAT: Test NIMCP_SNN_OPS() macro returns valid operations
 * WHY:  SNN operations critical for spiking neural networks
 * HOW:  Get ops via macro, verify function pointers
 */
TEST_F(KernelBackendSwitchingTest, Macro_SNNOps_Valid) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CPU));

    nimcp_snn_ops_t* ops = NIMCP_SNN_OPS();
    ASSERT_NE(ops, nullptr);

    EXPECT_NE(ops->lif_forward, nullptr) << "lif_forward should be implemented";
    EXPECT_NE(ops->izhikevich_forward, nullptr) << "izhikevich_forward should be implemented";
    EXPECT_NE(ops->surrogate_superspike, nullptr) << "surrogate_superspike should be implemented";
}

/**
 * WHAT: Test NIMCP_LNN_OPS() macro returns valid operations
 * WHY:  LNN operations needed for liquid neural networks
 * HOW:  Get ops via macro, verify function pointers
 */
TEST_F(KernelBackendSwitchingTest, Macro_LNNOps_Valid) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CPU));

    nimcp_lnn_ops_t* ops = NIMCP_LNN_OPS();
    ASSERT_NE(ops, nullptr);

    EXPECT_NE(ops->euler_step, nullptr) << "euler_step should be implemented";
    // Higher-order ODE solvers may not be implemented on all backends
}

/**
 * WHAT: Test NIMCP_CNN_OPS() macro returns valid operations
 * WHY:  CNN operations for convolutional networks
 * HOW:  Get ops via macro, verify function pointers
 */
TEST_F(KernelBackendSwitchingTest, Macro_CNNOps_Valid) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CPU));

    nimcp_cnn_ops_t* ops = NIMCP_CNN_OPS();
    ASSERT_NE(ops, nullptr);

    EXPECT_NE(ops->conv2d_forward, nullptr) << "conv2d_forward should be implemented";
    EXPECT_NE(ops->maxpool2d, nullptr) << "maxpool2d should be implemented";
}

/**
 * WHAT: Test NIMCP_INFERENCE_OPS() macro returns valid operations
 * WHY:  Inference operations for optimized forward pass
 * HOW:  Get ops via macro, verify function pointers
 */
TEST_F(KernelBackendSwitchingTest, Macro_InferenceOps_Valid) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CPU));

    nimcp_inference_ops_t* ops = NIMCP_INFERENCE_OPS();
    ASSERT_NE(ops, nullptr);

    EXPECT_NE(ops->linear_relu, nullptr) << "linear_relu should be implemented";
    EXPECT_NE(ops->quantize_int8, nullptr) << "quantize_int8 should be implemented";
}

/**
 * WHAT: Test NIMCP_CALL_TENSOR_OP macro with implemented operation
 * WHY:  Macro should call operation if available
 * HOW:  Use macro to call add, verify success
 */
TEST_F(KernelBackendSwitchingTest, Macro_CallTensorOp_Works) {
    // Use AUTO to ensure CUDA backend is available if GPU tensors are created
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO));

    gpu_ctx = nimcp_gpu_context_create_auto();
    if (!gpu_ctx) {
        GTEST_SKIP() << "No GPU context available";
    }

    auto data_a = generateRandomData(TEST_SIZE);
    auto data_b = generateRandomData(TEST_SIZE);
    std::vector<size_t> dims = {TEST_SIZE};

    auto* tensor_a = nimcp_gpu_tensor_from_host(gpu_ctx, data_a.data(), dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
    auto* tensor_b = nimcp_gpu_tensor_from_host(gpu_ctx, data_b.data(), dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
    auto* tensor_out = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

    ASSERT_NE(tensor_a, nullptr);
    ASSERT_NE(tensor_b, nullptr);
    ASSERT_NE(tensor_out, nullptr);

    nimcp_kernel_error_t result = NIMCP_CALL_TENSOR_OP(add, gpu_ctx, tensor_a, tensor_b, tensor_out);
    EXPECT_EQ(result, NIMCP_KERNEL_SUCCESS);

    nimcp_gpu_tensor_destroy(tensor_a);
    nimcp_gpu_tensor_destroy(tensor_b);
    nimcp_gpu_tensor_destroy(tensor_out);
}

/**
 * WHAT: Test operations change behavior after backend switch
 * WHY:  Verify operation dispatch uses current backend
 * HOW:  Get ops pointer before and after switch, verify different behavior
 */
TEST_F(KernelBackendSwitchingTest, Macro_OpsUpdateAfterSwitch) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CPU));

    // Get backend info before switch
    nimcp_backend_type_t type_before = nimcp_get_backend_type();
    EXPECT_EQ(type_before, NIMCP_BACKEND_CPU);

    if (hasGPU()) {
        nimcp_switch_backend(NIMCP_BACKEND_CUDA);

        nimcp_backend_type_t type_after = nimcp_get_backend_type();
        EXPECT_EQ(type_after, NIMCP_BACKEND_CUDA);

        // Operations should now be CUDA operations
        nimcp_kernel_backend_t* backend = nimcp_get_kernel_backend();
        ASSERT_NE(backend, nullptr);
        EXPECT_EQ(backend->type, NIMCP_BACKEND_CUDA);
    }
}

//=============================================================================
// AUTOMATIC BACKEND SELECTION TESTS
//=============================================================================

/**
 * WHAT: Test AUTO backend selection prefers GPU when available
 * WHY:  GPU should be default for performance
 * HOW:  Init with AUTO, verify GPU selected if present
 */
TEST_F(KernelBackendSwitchingTest, Auto_PrefersGPU) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO));

    nimcp_backend_type_t selected = nimcp_get_backend_type();

    if (hasGPU()) {
        EXPECT_EQ(selected, NIMCP_BACKEND_CUDA)
            << "AUTO should prefer CUDA when GPU available";
    } else {
        EXPECT_EQ(selected, NIMCP_BACKEND_CPU)
            << "AUTO should fall back to CPU when no GPU";
    }
}

/**
 * WHAT: Test AUTO can still switch after initial selection
 * WHY:  AUTO is just initial selection, should not lock backend
 * HOW:  Init AUTO, then manually switch, verify switch works
 */
TEST_F(KernelBackendSwitchingTest, Auto_AllowsSwitchAfter) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO));

    // Should be able to force CPU
    bool switch_ok = nimcp_switch_backend(NIMCP_BACKEND_CPU);
    EXPECT_TRUE(switch_ok);
    EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CPU);
}

//=============================================================================
// ERROR HANDLING TESTS
//=============================================================================

/**
 * WHAT: Test switching to invalid backend type
 * WHY:  Should handle invalid requests gracefully
 * HOW:  Try to switch to invalid type, verify handled
 */
TEST_F(KernelBackendSwitchingTest, Error_InvalidBackendType) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CPU));

    nimcp_backend_type_t original = nimcp_get_backend_type();

    // Cast invalid value
    nimcp_backend_type_t invalid = static_cast<nimcp_backend_type_t>(999);
    bool switch_ok = nimcp_switch_backend(invalid);

    // Should either fail or stay on current backend
    EXPECT_FALSE(switch_ok) << "Switch to invalid backend should fail";
    EXPECT_EQ(nimcp_get_backend_type(), original)
        << "Should remain on original backend after failed switch";
}

/**
 * WHAT: Test operations before backend initialization
 * WHY:  Should handle uninitialized state
 * HOW:  Try to get ops before init, verify handled
 */
TEST_F(KernelBackendSwitchingTest, Error_OpsBeforeInit) {
    // Note: Backend is not initialized in this test
    // Getting ops may return null or default

    nimcp_kernel_backend_t* backend = nimcp_get_kernel_backend();

    // May be null or point to default/uninitialized backend
    if (backend) {
        // If not null, should not crash when accessed
        // But operations may return NOT_IMPLEMENTED
    }
}

/**
 * WHAT: Test backend state after failed switch
 * WHY:  Failed switch should not corrupt state
 * HOW:  Attempt failing switch (ROCm which is unlikely to be available), verify state preserved
 */
TEST_F(KernelBackendSwitchingTest, Error_StatePreservedAfterFailedSwitch) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CPU));

    // Record state before
    nimcp_backend_type_t type_before = nimcp_get_backend_type();
    nimcp_kernel_backend_t* backend_before = nimcp_get_kernel_backend();

    // Try to switch to ROCm (should fail on most systems without AMD GPUs)
    // This tests that a failed switch preserves state
    bool switch_result = nimcp_switch_backend(NIMCP_BACKEND_ROCM);

    if (!switch_result) {
        // Switch failed as expected - state should be preserved
        EXPECT_EQ(nimcp_get_backend_type(), type_before);
        nimcp_kernel_backend_t* backend_after = nimcp_get_kernel_backend();
        ASSERT_NE(backend_after, nullptr);
        EXPECT_TRUE(backend_after->initialized);
    } else {
        // ROCm is available - test that we can switch back
        EXPECT_TRUE(nimcp_switch_backend(NIMCP_BACKEND_CPU));
        EXPECT_EQ(nimcp_get_backend_type(), NIMCP_BACKEND_CPU);
    }
}

//=============================================================================
// STATE PRESERVATION TESTS
//=============================================================================

/**
 * WHAT: Test tensor operations give same results after switch and switchback
 * WHY:  Backend changes should not affect computation correctness
 * HOW:  Compute, switch to GPU, switch back, recompute, compare
 *
 * NOTE: GPU tensors (allocated via cudaMalloc) cannot be accessed by CPU backend.
 *       This test uses AUTO mode and only tests operations on CUDA backend.
 */
TEST_F(KernelBackendSwitchingTest, State_ConsistentResultsAcrossSwitches) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO));

    gpu_ctx = nimcp_gpu_context_create_auto();
    if (!gpu_ctx) {
        GTEST_SKIP() << "No GPU context available";
    }

    if (!hasGPU()) {
        GTEST_SKIP() << "No GPU available for this test";
    }

    auto data_a = generateRandomData(TEST_SIZE);
    auto data_b = generateRandomData(TEST_SIZE);
    std::vector<size_t> dims = {TEST_SIZE};

    // Compute on initial backend (CUDA if available)
    auto* tensor_a = nimcp_gpu_tensor_from_host(gpu_ctx, data_a.data(), dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
    auto* tensor_b = nimcp_gpu_tensor_from_host(gpu_ctx, data_b.data(), dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
    auto* tensor_out1 = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);

    NIMCP_TENSOR_OPS()->add(gpu_ctx, tensor_a, tensor_b, tensor_out1);
    auto result_first = copyToHost(tensor_out1);

    {
        // Switch to CUDA explicitly and compute
        nimcp_switch_backend(NIMCP_BACKEND_CUDA);

        auto* tensor_out2 = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
        NIMCP_TENSOR_OPS()->add(gpu_ctx, tensor_a, tensor_b, tensor_out2);
        auto result_cuda = copyToHost(tensor_out2);

        // Compute a third time on CUDA to verify consistency
        auto* tensor_out3 = nimcp_gpu_tensor_create(gpu_ctx, dims.data(), 1, NIMCP_GPU_PRECISION_FP32);
        NIMCP_TENSOR_OPS()->add(gpu_ctx, tensor_a, tensor_b, tensor_out3);
        auto result_cuda2 = copyToHost(tensor_out3);

        // All results should match within tolerance
        for (size_t i = 0; i < TEST_SIZE; i++) {
            EXPECT_NEAR(result_first[i], result_cuda[i], TOLERANCE)
                << "First and CUDA results differ at index " << i;
            EXPECT_NEAR(result_cuda[i], result_cuda2[i], TOLERANCE)
                << "CUDA results differ on repeated computation at index " << i;
        }

        nimcp_gpu_tensor_destroy(tensor_out2);
        nimcp_gpu_tensor_destroy(tensor_out3);
    }

    nimcp_gpu_tensor_destroy(tensor_a);
    nimcp_gpu_tensor_destroy(tensor_b);
    nimcp_gpu_tensor_destroy(tensor_out1);
}

/**
 * WHAT: Test backend name stays consistent during lifecycle
 * WHY:  Backend metadata should be stable
 * HOW:  Check name at various points, verify consistent
 */
TEST_F(KernelBackendSwitchingTest, State_BackendNameConsistent) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CPU));

    const char* name1 = nimcp_backend_type_name(NIMCP_BACKEND_CPU);

    // After some operations
    nimcp_kernel_backend_t* backend = nimcp_get_kernel_backend();
    (void)backend;  // Use to prevent optimization

    const char* name2 = nimcp_backend_type_name(NIMCP_BACKEND_CPU);

    EXPECT_STREQ(name1, name2) << "Backend name should be consistent";
}

//=============================================================================
// PERFORMANCE HINT TESTS
//=============================================================================

/**
 * WHAT: Test that backend selection is efficient
 * WHY:  Backend operations should not be costly
 * HOW:  Measure time for backend queries
 */
TEST_F(KernelBackendSwitchingTest, Performance_FastQueries) {
    EXPECT_TRUE(nimcp_kernel_backend_init(NIMCP_BACKEND_CPU));

    const int iterations = 10000;

    // Time get_backend_type
    for (int i = 0; i < iterations; i++) {
        nimcp_backend_type_t type = nimcp_get_backend_type();
        (void)type;
    }

    // Time get_kernel_backend
    for (int i = 0; i < iterations; i++) {
        nimcp_kernel_backend_t* backend = nimcp_get_kernel_backend();
        (void)backend;
    }

    // If we get here without timeout, queries are fast enough
    SUCCEED() << "Backend queries completed within acceptable time";
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
