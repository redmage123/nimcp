/* ============================================================================
 * Integration Tests: GPU Recovery with Specialized Modules
 * ============================================================================
 * WHAT: Integration tests for GPU recovery with specialized modules
 * WHY:  Validate recovery works correctly with dragonfly, portia, sparse,
 *       ternary, and topology GPU operations
 * HOW:  Test recovery scenarios across specialized computational domains
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include <cuda_runtime.h>
#endif

namespace {

/* ============================================================================
 * Common CPU Fallback Functions for Specialized Modules
 * ============================================================================ */

#ifdef NIMCP_ENABLE_CUDA

/* CPU fallback for Kalman filter prediction (dragonfly) */
static bool kalman_predict_cpu_fallback(void* context, void* params, void* result) {
    (void)context;
    if (!params || !result) return false;

    /* Simple state prediction: x_new = A * x */
    float* state = static_cast<float*>(params);
    float* predicted = static_cast<float*>(result);

    /* Identity prediction for fallback */
    *predicted = *state;
    return true;
}

/* CPU fallback for optical flow computation (dragonfly) */
static bool optical_flow_cpu_fallback(void* context, void* params, void* result) {
    (void)context;
    if (!params || !result) return false;

    float* flow = static_cast<float*>(result);
    /* Zero flow as safe fallback */
    flow[0] = 0.0f;  /* dx */
    flow[1] = 0.0f;  /* dy */
    return true;
}

/* CPU fallback for portia attention computation */
static bool portia_attention_cpu_fallback(void* context, void* params, void* result) {
    (void)context;
    if (!params || !result) return false;

    float* attention = static_cast<float*>(result);
    /* Uniform attention as fallback */
    *attention = 1.0f;
    return true;
}

/* CPU fallback for sparse matrix-vector multiply */
static bool sparse_spmv_cpu_fallback(void* context, void* params, void* result) {
    (void)context;
    if (!params || !result) return false;

    /* Simple dense fallback: y = 0 (safe default) */
    float* y = static_cast<float*>(result);
    *y = 0.0f;
    return true;
}

/* CPU fallback for ternary quantization */
static bool ternary_quantize_cpu_fallback(void* context, void* params, void* result) {
    (void)context;
    if (!params || !result) return false;

    float* input = static_cast<float*>(params);
    int8_t* output = static_cast<int8_t*>(result);

    /* Ternary quantization: -1, 0, +1 */
    float threshold = 0.5f;
    if (*input > threshold) {
        *output = 1;
    } else if (*input < -threshold) {
        *output = -1;
    } else {
        *output = 0;
    }
    return true;
}

/* CPU fallback for topology PageRank computation */
static bool pagerank_cpu_fallback(void* context, void* params, void* result) {
    (void)context;
    if (!params || !result) return false;

    int* num_nodes = static_cast<int*>(params);
    float* ranks = static_cast<float*>(result);

    /* Uniform distribution as fallback */
    float uniform = 1.0f / static_cast<float>(*num_nodes);
    ranks[0] = uniform;
    return true;
}

/* CPU fallback for BFS shortest path */
static bool bfs_cpu_fallback(void* context, void* params, void* result) {
    (void)context;
    if (!params || !result) return false;

    int* distances = static_cast<int*>(result);
    /* Unreachable marker */
    *distances = -1;
    return true;
}

#endif /* NIMCP_ENABLE_CUDA */

/* ============================================================================
 * Test Fixture: Dragonfly Vision Module Recovery
 * ============================================================================ */
class DragonflyRecoveryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        int device_count = 0;
        cudaGetDeviceCount(&device_count);
        if (device_count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }
        ctx_ = nimcp_gpu_recovery_context_create(NULL);
#else
        GTEST_SKIP() << "CUDA not enabled";
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (ctx_) {
            nimcp_gpu_recovery_context_destroy(ctx_);
            ctx_ = nullptr;
        }
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx_ = nullptr;
#endif
};

/* ============================================================================
 * Test: Dragonfly Kalman Filter OOM Recovery
 * ============================================================================ */
TEST_F(DragonflyRecoveryIntegrationTest, KalmanFilterOOMRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, kalman_predict_cpu_fallback, nullptr);

    /* Simulate OOM during Kalman state allocation */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);

    /* OOM should try FREE_CACHE first */
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_FREE_CACHE);
    EXPECT_NE(result.action_taken, GPU_RECOVERY_NONE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Dragonfly Optical Flow Numerical Recovery
 * ============================================================================ */
TEST_F(DragonflyRecoveryIntegrationTest, OpticalFlowNumericalRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, optical_flow_cpu_fallback, nullptr);

    /* Numerical errors in optical flow (divide by zero, NaN) */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_NUMERICAL,
                                           cudaSuccess, &result);

    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_REDUCE_PRECISION);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Dragonfly TSDN Kernel Launch Failure
 * ============================================================================ */
TEST_F(DragonflyRecoveryIntegrationTest, TSDNKernelLaunchRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, optical_flow_cpu_fallback, nullptr);

    /* TSDN kernel launch failure */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_KERNEL_LAUNCH,
                                           cudaErrorLaunchFailure, &result);

    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_STREAM_SYNC);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Dragonfly CPU Fallback on Device Failure
 * ============================================================================ */
TEST_F(DragonflyRecoveryIntegrationTest, DeviceFailureCPUFallback) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, kalman_predict_cpu_fallback, nullptr);

    /* Device unavailable triggers CPU fallback */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_DEVICE_NOT_AVAILABLE,
                                           cudaErrorNoDevice, &result);

    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_CPU_FALLBACK);
    EXPECT_TRUE(ctx_->cpu_fallback_active);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test Fixture: Portia Spider Vision Module Recovery
 * ============================================================================ */
class PortiaRecoveryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        int device_count = 0;
        cudaGetDeviceCount(&device_count);
        if (device_count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }
        ctx_ = nimcp_gpu_recovery_context_create(NULL);
#else
        GTEST_SKIP() << "CUDA not enabled";
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (ctx_) {
            nimcp_gpu_recovery_context_destroy(ctx_);
            ctx_ = nullptr;
        }
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx_ = nullptr;
#endif
};

/* ============================================================================
 * Test: Portia Attention Map OOM Recovery
 * ============================================================================ */
TEST_F(PortiaRecoveryIntegrationTest, AttentionMapOOMRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, portia_attention_cpu_fallback, nullptr);

    /* Large attention map allocation failure */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);

    EXPECT_EQ(result.action_taken, GPU_RECOVERY_FREE_CACHE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Portia Prey Detection Invalid Parameters
 * ============================================================================ */
TEST_F(PortiaRecoveryIntegrationTest, PreyDetectionInvalidParams) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, portia_attention_cpu_fallback, nullptr);

    /* Invalid parameter recovery */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_INVALID_PARAMS,
                                           cudaErrorInvalidValue, &result);

    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_PARAM_CORRECTION);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Portia Visual Processing Timeout
 * ============================================================================ */
TEST_F(PortiaRecoveryIntegrationTest, VisualProcessingTimeout) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, portia_attention_cpu_fallback, nullptr);

    /* Timeout during complex visual computation */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_TIMEOUT,
                                           cudaSuccess, &result);

    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_ASYNC_SPLIT);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test Fixture: Sparse Matrix Operations Recovery
 * ============================================================================ */
class SparseRecoveryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        int device_count = 0;
        cudaGetDeviceCount(&device_count);
        if (device_count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }
        ctx_ = nimcp_gpu_recovery_context_create(NULL);
#else
        GTEST_SKIP() << "CUDA not enabled";
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (ctx_) {
            nimcp_gpu_recovery_context_destroy(ctx_);
            ctx_ = nullptr;
        }
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx_ = nullptr;
#endif
};

/* ============================================================================
 * Test: Sparse SpMV OOM Recovery
 * ============================================================================ */
TEST_F(SparseRecoveryIntegrationTest, SpMVOOMRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, sparse_spmv_cpu_fallback, nullptr);

    /* Large sparse matrix allocation failure */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);

    EXPECT_EQ(result.action_taken, GPU_RECOVERY_FREE_CACHE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Sparse Context Invalid Recovery
 * ============================================================================ */
TEST_F(SparseRecoveryIntegrationTest, ContextInvalidRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, sparse_spmv_cpu_fallback, nullptr);

    /* cuSPARSE context corruption */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_CONTEXT_INVALID,
                                           cudaSuccess, &result);

    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_CONTEXT_RESET);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Sparse Workspace Allocation Failure
 * ============================================================================ */
TEST_F(SparseRecoveryIntegrationTest, WorkspaceAllocationRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, sparse_spmv_cpu_fallback, nullptr);

    /* Workspace buffer allocation failure */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);

    /* Should attempt FREE_CACHE then potentially BATCH_REDUCE */
    EXPECT_NE(result.action_taken, GPU_RECOVERY_NONE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Sparse Format Conversion Numerical Error
 * ============================================================================ */
TEST_F(SparseRecoveryIntegrationTest, FormatConversionNumerical) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, sparse_spmv_cpu_fallback, nullptr);

    /* Numerical issues during CSR to COO conversion */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_NUMERICAL,
                                           cudaSuccess, &result);

    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_REDUCE_PRECISION);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test Fixture: Ternary Quantization Recovery
 * ============================================================================ */
class TernaryRecoveryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        int device_count = 0;
        cudaGetDeviceCount(&device_count);
        if (device_count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }
        ctx_ = nimcp_gpu_recovery_context_create(NULL);
#else
        GTEST_SKIP() << "CUDA not enabled";
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (ctx_) {
            nimcp_gpu_recovery_context_destroy(ctx_);
            ctx_ = nullptr;
        }
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx_ = nullptr;
#endif
};

/* ============================================================================
 * Test: Ternary Tensor Creation OOM
 * ============================================================================ */
TEST_F(TernaryRecoveryIntegrationTest, TensorCreationOOM) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, ternary_quantize_cpu_fallback, nullptr);

    /* Large ternary tensor allocation failure */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);

    EXPECT_EQ(result.action_taken, GPU_RECOVERY_FREE_CACHE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Ternary Quantization Numerical Stability
 * ============================================================================ */
TEST_F(TernaryRecoveryIntegrationTest, QuantizationNumericalRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, ternary_quantize_cpu_fallback, nullptr);

    /* Numerical instability during quantization threshold computation */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_NUMERICAL,
                                           cudaSuccess, &result);

    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_REDUCE_PRECISION);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Ternary GEMV Kernel Launch
 * ============================================================================ */
TEST_F(TernaryRecoveryIntegrationTest, GEMVKernelLaunch) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, ternary_quantize_cpu_fallback, nullptr);

    /* Ternary GEMV kernel launch failure */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_KERNEL_LAUNCH,
                                           cudaErrorLaunchFailure, &result);

    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_STREAM_SYNC);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Ternary CPU Fallback Execution
 * ============================================================================ */
TEST_F(TernaryRecoveryIntegrationTest, CPUFallbackExecution) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, ternary_quantize_cpu_fallback, nullptr);

    /* Test actual CPU fallback execution */
    float input = 0.8f;
    int8_t output = 0;

    bool success = nimcp_gpu_execute_cpu_fallback(ctx_, &input, &output);

    EXPECT_TRUE(success);
    EXPECT_EQ(output, 1);  /* 0.8 > 0.5 threshold */
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test Fixture: Topology Graph Operations Recovery
 * ============================================================================ */
class TopologyRecoveryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        int device_count = 0;
        cudaGetDeviceCount(&device_count);
        if (device_count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }
        ctx_ = nimcp_gpu_recovery_context_create(NULL);
#else
        GTEST_SKIP() << "CUDA not enabled";
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (ctx_) {
            nimcp_gpu_recovery_context_destroy(ctx_);
            ctx_ = nullptr;
        }
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx_ = nullptr;
#endif
};

/* ============================================================================
 * Test: Topology Graph Creation OOM
 * ============================================================================ */
TEST_F(TopologyRecoveryIntegrationTest, GraphCreationOOM) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, pagerank_cpu_fallback, nullptr);

    /* Large graph adjacency matrix allocation failure */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);

    EXPECT_EQ(result.action_taken, GPU_RECOVERY_FREE_CACHE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Topology PageRank Numerical Convergence
 * ============================================================================ */
TEST_F(TopologyRecoveryIntegrationTest, PageRankNumericalRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, pagerank_cpu_fallback, nullptr);

    /* PageRank convergence issues (numerical instability) */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_NUMERICAL,
                                           cudaSuccess, &result);

    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_REDUCE_PRECISION);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Topology BFS Kernel Launch
 * ============================================================================ */
TEST_F(TopologyRecoveryIntegrationTest, BFSKernelLaunchRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, bfs_cpu_fallback, nullptr);

    /* BFS frontier expansion kernel failure */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_KERNEL_LAUNCH,
                                           cudaErrorLaunchFailure, &result);

    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_STREAM_SYNC);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Topology Clustering Coefficient Timeout
 * ============================================================================ */
TEST_F(TopologyRecoveryIntegrationTest, ClusteringCoefficientTimeout) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, pagerank_cpu_fallback, nullptr);

    /* Long-running clustering computation timeout */
    nimcp_gpu_recovery_result_t result;
    bool recovered = nimcp_gpu_try_recover(ctx_, GPU_ERROR_TIMEOUT,
                                           cudaSuccess, &result);

    EXPECT_TRUE(recovered);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_ASYNC_SPLIT);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Topology Louvain Community Detection OOM
 * ============================================================================ */
TEST_F(TopologyRecoveryIntegrationTest, LouvainOOMRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, pagerank_cpu_fallback, nullptr);

    /* Louvain modularity matrix allocation failure */
    nimcp_gpu_recovery_result_t result;
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);

    /* Should attempt FREE_CACHE */
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_FREE_CACHE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Topology Floyd-Warshall Large Graph
 * ============================================================================ */
TEST_F(TopologyRecoveryIntegrationTest, FloydWarshallBatchReduction) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, bfs_cpu_fallback, nullptr);

    /* Floyd-Warshall on very large graph - memory pressure */
    nimcp_gpu_recovery_result_t result;

    /* First recovery attempt */
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);

    /* Verify recovery action was attempted */
    EXPECT_NE(result.action_taken, GPU_RECOVERY_NONE);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test Fixture: Cross-Module Recovery Integration
 * ============================================================================ */
class CrossModuleRecoveryIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
#ifdef NIMCP_ENABLE_CUDA
        int device_count = 0;
        cudaGetDeviceCount(&device_count);
        if (device_count == 0) {
            GTEST_SKIP() << "No CUDA devices available";
        }
        if (!nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_init(NULL);
        }
        ctx_ = nimcp_gpu_recovery_context_create(NULL);
#else
        GTEST_SKIP() << "CUDA not enabled";
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (ctx_) {
            nimcp_gpu_recovery_context_destroy(ctx_);
            ctx_ = nullptr;
        }
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
        }
#endif
    }

#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_recovery_context_t* ctx_ = nullptr;
#endif
};

/* ============================================================================
 * Test: Sequential Module Recovery
 * Verify recovery works across multiple modules in sequence
 * ============================================================================ */
TEST_F(CrossModuleRecoveryIntegrationTest, SequentialModuleRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    /* Dragonfly recovery */
    nimcp_gpu_set_cpu_fallback(ctx_, kalman_predict_cpu_fallback, nullptr);
    nimcp_gpu_recovery_result_t result1;
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result1);
    EXPECT_NE(result1.action_taken, GPU_RECOVERY_NONE);

    /* Reset for next module */
    nimcp_gpu_recovery_context_reset(ctx_);

    /* Sparse recovery */
    nimcp_gpu_set_cpu_fallback(ctx_, sparse_spmv_cpu_fallback, nullptr);
    nimcp_gpu_recovery_result_t result2;
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_KERNEL_LAUNCH,
                          cudaErrorLaunchFailure, &result2);
    EXPECT_TRUE(result2.recovered);
    EXPECT_EQ(result2.action_taken, GPU_RECOVERY_STREAM_SYNC);

    /* Reset for next module */
    nimcp_gpu_recovery_context_reset(ctx_);

    /* Topology recovery */
    nimcp_gpu_set_cpu_fallback(ctx_, pagerank_cpu_fallback, nullptr);
    nimcp_gpu_recovery_result_t result3;
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_NUMERICAL,
                          cudaSuccess, &result3);
    EXPECT_TRUE(result3.recovered);
    EXPECT_EQ(result3.action_taken, GPU_RECOVERY_REDUCE_PRECISION);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery Statistics Accumulation
 * Verify stats are properly accumulated across recovery attempts
 * ============================================================================ */
TEST_F(CrossModuleRecoveryIntegrationTest, RecoveryStatsAccumulation) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    /* Reset global stats */
    nimcp_gpu_recovery_reset_stats();

    nimcp_gpu_set_cpu_fallback(ctx_, ternary_quantize_cpu_fallback, nullptr);

    /* Multiple recovery attempts */
    for (int i = 0; i < 5; i++) {
        nimcp_gpu_recovery_result_t result;
        nimcp_gpu_try_recover(ctx_, GPU_ERROR_KERNEL_LAUNCH,
                              cudaErrorLaunchFailure, &result);
    }

    /* Check accumulated stats */
    nimcp_gpu_recovery_stats_t stats;
    nimcp_gpu_recovery_get_stats(&stats);

    EXPECT_GE(stats.total_recovery_attempts, 5u);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Fallback Switching Between Modules
 * Verify CPU fallback can be switched dynamically
 * ============================================================================ */
TEST_F(CrossModuleRecoveryIntegrationTest, FallbackSwitching) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    /* Set dragonfly fallback */
    nimcp_gpu_set_cpu_fallback(ctx_, kalman_predict_cpu_fallback, nullptr);

    float state = 5.0f;
    float predicted = 0.0f;

    bool success1 = nimcp_gpu_execute_cpu_fallback(ctx_, &state, &predicted);
    EXPECT_TRUE(success1);
    EXPECT_FLOAT_EQ(predicted, 5.0f);  /* Identity prediction */

    /* Switch to ternary fallback */
    nimcp_gpu_set_cpu_fallback(ctx_, ternary_quantize_cpu_fallback, nullptr);

    float input = -0.8f;
    int8_t output = 0;

    bool success2 = nimcp_gpu_execute_cpu_fallback(ctx_, &input, &output);
    EXPECT_TRUE(success2);
    EXPECT_EQ(output, -1);  /* -0.8 < -0.5 threshold */
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Recovery Exhaustion Handling
 * Verify behavior when all recovery strategies are exhausted
 * ============================================================================ */
TEST_F(CrossModuleRecoveryIntegrationTest, RecoveryExhaustion) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    /* Configure for limited retries */
    nimcp_gpu_recovery_config_t config;
    nimcp_gpu_recovery_default_config(&config);
    config.max_retries = 1;
    config.enable_cpu_fallback = false;  /* Disable CPU fallback */

    nimcp_gpu_recovery_context_t* limited_ctx =
        nimcp_gpu_recovery_context_create(&config);
    ASSERT_NE(limited_ctx, nullptr);

    /* Try to recover multiple times from persistent error */
    nimcp_gpu_recovery_result_t result;
    for (int i = 0; i < 5; i++) {
        nimcp_gpu_try_recover(limited_ctx, GPU_ERROR_DEVICE_NOT_AVAILABLE,
                              cudaErrorNoDevice, &result);
    }

    /* Eventually should exhaust strategies */
    /* CPU fallback is disabled, so device unavailable has no recovery path */
    EXPECT_FALSE(result.recovered);

    nimcp_gpu_recovery_context_destroy(limited_ctx);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: Mixed Error Type Recovery
 * Verify recovery handles different error types in sequence
 * ============================================================================ */
TEST_F(CrossModuleRecoveryIntegrationTest, MixedErrorTypeRecovery) {
#ifdef NIMCP_ENABLE_CUDA
    ASSERT_NE(ctx_, nullptr);

    nimcp_gpu_set_cpu_fallback(ctx_, sparse_spmv_cpu_fallback, nullptr);

    nimcp_gpu_recovery_result_t result;

    /* OOM error */
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_OUT_OF_MEMORY,
                          cudaErrorMemoryAllocation, &result);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_FREE_CACHE);

    nimcp_gpu_recovery_context_reset(ctx_);

    /* Kernel launch error */
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_KERNEL_LAUNCH,
                          cudaErrorLaunchFailure, &result);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_STREAM_SYNC);

    nimcp_gpu_recovery_context_reset(ctx_);

    /* Numerical error */
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_NUMERICAL,
                          cudaSuccess, &result);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_REDUCE_PRECISION);

    nimcp_gpu_recovery_context_reset(ctx_);

    /* Context invalid */
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_CONTEXT_INVALID,
                          cudaSuccess, &result);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_CONTEXT_RESET);

    nimcp_gpu_recovery_context_reset(ctx_);

    /* Timeout */
    nimcp_gpu_try_recover(ctx_, GPU_ERROR_TIMEOUT,
                          cudaSuccess, &result);
    EXPECT_EQ(result.action_taken, GPU_RECOVERY_ASYNC_SPLIT);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */
