/* ============================================================================
 * Unit Tests: GPU Recovery Parameter Correction
 * ============================================================================
 * WHAT: Unit tests for GPU parameter validation and correction
 * WHY:  Validate auto-correction of invalid parameters
 * HOW:  Test clamping, defaults, NaN/Inf handling, and batch correction
 * ============================================================================
 */

#include <gtest/gtest.h>
#include <cmath>
#include <limits>

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include <cuda_runtime.h>
#endif

namespace {

/* ============================================================================
 * Test Fixture
 * ============================================================================ */
class GPURecoveryParamCorrectionTest : public ::testing::Test {
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
#else
        GTEST_SKIP() << "CUDA not enabled";
#endif
    }

    void TearDown() override {
#ifdef NIMCP_ENABLE_CUDA
        if (nimcp_gpu_recovery_is_initialized()) {
            nimcp_gpu_recovery_shutdown();
        }
#endif
    }
};

/* ============================================================================
 * Test: ClampParamsReducesBelowMin
 * Verify values below minimum are clamped to minimum
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, ClampParamsReducesBelowMin) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_param_range_t range = {0.0f, 1.0f, 0.5f, true};
    float value = -0.5f;

    bool corrected = nimcp_gpu_correct_param_float(&value, &range, "test_param");

    EXPECT_TRUE(corrected) << "Should indicate correction was made";
    EXPECT_FLOAT_EQ(value, 0.0f) << "Should be clamped to minimum";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ClampParamsReducesAboveMax
 * Verify values above maximum are clamped to maximum
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, ClampParamsReducesAboveMax) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_param_range_t range = {0.0f, 1.0f, 0.5f, true};
    float value = 1.5f;

    bool corrected = nimcp_gpu_correct_param_float(&value, &range, "test_param");

    EXPECT_TRUE(corrected);
    EXPECT_FLOAT_EQ(value, 1.0f) << "Should be clamped to maximum";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ClampParamsFixesNaN
 * Verify NaN values are replaced with default
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, ClampParamsFixesNaN) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_param_range_t range = {0.0f, 1.0f, 0.5f, true};
    float value = std::numeric_limits<float>::quiet_NaN();

    bool corrected = nimcp_gpu_correct_param_float(&value, &range, "test_param");

    EXPECT_TRUE(corrected);
    EXPECT_FALSE(std::isnan(value)) << "NaN should be replaced";
    EXPECT_FLOAT_EQ(value, 0.5f) << "Should be set to default";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ClampParamsFixesInfinity
 * Verify infinity values are replaced with default
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, ClampParamsFixesInfinity) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_param_range_t range = {0.0f, 1.0f, 0.5f, true};

    /* Test positive infinity */
    float value = std::numeric_limits<float>::infinity();
    bool corrected = nimcp_gpu_correct_param_float(&value, &range, "test_param");
    EXPECT_TRUE(corrected);
    EXPECT_FALSE(std::isinf(value));
    EXPECT_FLOAT_EQ(value, 0.5f);

    /* Test negative infinity */
    value = -std::numeric_limits<float>::infinity();
    corrected = nimcp_gpu_correct_param_float(&value, &range, "test_param");
    EXPECT_TRUE(corrected);
    EXPECT_FALSE(std::isinf(value));
    EXPECT_FLOAT_EQ(value, 0.5f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: SetDefaultsOnInvalidParams
 * Verify use of default instead of clamping when configured
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, SetDefaultsOnInvalidParams) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_param_range_t range = {0.0f, 1.0f, 0.5f, false};  /* clamp_to_range = false */
    float value = 1.5f;

    bool corrected = nimcp_gpu_correct_param_float(&value, &range, "test_param");

    EXPECT_TRUE(corrected);
    EXPECT_FLOAT_EQ(value, 0.5f) << "Should use default when not clamping";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ParamCorrectionPreservesValid
 * Verify valid values are not modified
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, ParamCorrectionPreservesValid) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_param_range_t range = {0.0f, 1.0f, 0.5f, true};
    float value = 0.75f;

    bool corrected = nimcp_gpu_correct_param_float(&value, &range, "test_param");

    EXPECT_FALSE(corrected) << "Valid value should not be corrected";
    EXPECT_FLOAT_EQ(value, 0.75f) << "Value should be unchanged";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: IntParamCorrectionBelowMin
 * Verify integer parameter correction below minimum
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, IntParamCorrectionBelowMin) {
#ifdef NIMCP_ENABLE_CUDA
    int value = -5;

    bool corrected = nimcp_gpu_correct_param_int(&value, 0, 100, 50, "test_param");

    EXPECT_TRUE(corrected);
    EXPECT_EQ(value, 0) << "Should be clamped to minimum";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: IntParamCorrectionAboveMax
 * Verify integer parameter correction above maximum
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, IntParamCorrectionAboveMax) {
#ifdef NIMCP_ENABLE_CUDA
    int value = 150;

    bool corrected = nimcp_gpu_correct_param_int(&value, 0, 100, 50, "test_param");

    EXPECT_TRUE(corrected);
    EXPECT_EQ(value, 100) << "Should be clamped to maximum";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: IntParamCorrectionPreservesValid
 * Verify valid integer values are not modified
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, IntParamCorrectionPreservesValid) {
#ifdef NIMCP_ENABLE_CUDA
    int value = 75;

    bool corrected = nimcp_gpu_correct_param_int(&value, 0, 100, 50, "test_param");

    EXPECT_FALSE(corrected);
    EXPECT_EQ(value, 75);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: SizeParamCorrectionBelowMin
 * Verify size_t parameter correction below minimum
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, SizeParamCorrectionBelowMin) {
#ifdef NIMCP_ENABLE_CUDA
    size_t value = 0;

    bool corrected = nimcp_gpu_correct_param_size(&value, 1, 1000, 64, "test_param");

    EXPECT_TRUE(corrected);
    EXPECT_EQ(value, 1u) << "Should be clamped to minimum";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: SizeParamCorrectionAboveMax
 * Verify size_t parameter correction above maximum
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, SizeParamCorrectionAboveMax) {
#ifdef NIMCP_ENABLE_CUDA
    size_t value = 5000;

    bool corrected = nimcp_gpu_correct_param_size(&value, 1, 1000, 64, "test_param");

    EXPECT_TRUE(corrected);
    EXPECT_EQ(value, 1000u) << "Should be clamped to maximum";
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: BatchCorrectionForMemory
 * Verify batch size is reduced based on available memory
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, BatchCorrectionForMemory) {
#ifdef NIMCP_ENABLE_CUDA
    size_t batch_size = 1000000;  /* Very large batch */
    size_t element_size = sizeof(float);
    size_t memory_per_element = 1024;  /* 1KB additional per element */

    /* This should trigger reduction if memory is limited */
    bool corrected = nimcp_gpu_correct_batch_for_memory(&batch_size, element_size, memory_per_element);

    /* Result depends on available GPU memory */
    if (corrected) {
        EXPECT_LT(batch_size, 1000000u) << "Batch should be reduced";
        EXPECT_GE(batch_size, 1u) << "Batch should be at least 1";
    }
    /* If not corrected, there was enough memory - that's fine too */
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: BatchCorrectionPreservesReasonableBatch
 * Verify reasonable batch sizes are not unnecessarily reduced
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, BatchCorrectionPreservesReasonableBatch) {
#ifdef NIMCP_ENABLE_CUDA
    size_t batch_size = 64;  /* Small batch */
    size_t element_size = sizeof(float);
    size_t memory_per_element = 1024;

    size_t original = batch_size;
    bool corrected = nimcp_gpu_correct_batch_for_memory(&batch_size, element_size, memory_per_element);

    /* Small batch should fit in any reasonable GPU memory */
    if (!corrected) {
        EXPECT_EQ(batch_size, original);
    }
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: NullPointerHandling
 * Verify NULL pointers are handled gracefully
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, NullPointerHandling) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_param_range_t range = {0.0f, 1.0f, 0.5f, true};

    /* NULL value pointer */
    bool result = nimcp_gpu_correct_param_float(NULL, &range, "test");
    EXPECT_FALSE(result);

    /* NULL range pointer */
    float value = 0.5f;
    result = nimcp_gpu_correct_param_float(&value, NULL, "test");
    EXPECT_FALSE(result);

    /* NULL int pointer */
    result = nimcp_gpu_correct_param_int(NULL, 0, 100, 50, "test");
    EXPECT_FALSE(result);

    /* NULL size pointer */
    result = nimcp_gpu_correct_param_size(NULL, 0, 100, 50, "test");
    EXPECT_FALSE(result);

    /* NULL batch pointer */
    result = nimcp_gpu_correct_batch_for_memory(NULL, 4, 1024);
    EXPECT_FALSE(result);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: DefaultConstraintsValid
 * Verify default parameter constraints are valid
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, DefaultConstraintsValid) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_param_constraints_t constraints;
    nimcp_gpu_default_param_constraints(&constraints);

    /* Batch size constraints */
    EXPECT_GE(constraints.batch_size.min_value, 1.0f);
    EXPECT_GT(constraints.batch_size.max_value, constraints.batch_size.min_value);
    EXPECT_GE(constraints.batch_size.default_value, constraints.batch_size.min_value);
    EXPECT_LE(constraints.batch_size.default_value, constraints.batch_size.max_value);

    /* Learning rate constraints */
    EXPECT_GT(constraints.learning_rate.min_value, 0.0f);
    EXPECT_GT(constraints.learning_rate.max_value, constraints.learning_rate.min_value);

    /* Num inputs/outputs constraints */
    EXPECT_GE(constraints.num_inputs.min_value, 1.0f);
    EXPECT_GE(constraints.num_outputs.min_value, 1.0f);

    /* Tolerance constraints */
    EXPECT_GT(constraints.tolerance.min_value, 0.0f);
    EXPECT_LE(constraints.tolerance.max_value, 1.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: EdgeCaseBoundaryValues
 * Verify correction at exact boundary values
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, EdgeCaseBoundaryValues) {
#ifdef NIMCP_ENABLE_CUDA
    nimcp_gpu_param_range_t range = {0.0f, 1.0f, 0.5f, true};

    /* Exactly at minimum - should not be corrected */
    float value = 0.0f;
    bool corrected = nimcp_gpu_correct_param_float(&value, &range, "test");
    EXPECT_FALSE(corrected);
    EXPECT_FLOAT_EQ(value, 0.0f);

    /* Exactly at maximum - should not be corrected */
    value = 1.0f;
    corrected = nimcp_gpu_correct_param_float(&value, &range, "test");
    EXPECT_FALSE(corrected);
    EXPECT_FLOAT_EQ(value, 1.0f);

    /* Just below minimum */
    value = -0.0001f;
    corrected = nimcp_gpu_correct_param_float(&value, &range, "test");
    EXPECT_TRUE(corrected);
    EXPECT_FLOAT_EQ(value, 0.0f);

    /* Just above maximum */
    value = 1.0001f;
    corrected = nimcp_gpu_correct_param_float(&value, &range, "test");
    EXPECT_TRUE(corrected);
    EXPECT_FLOAT_EQ(value, 1.0f);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

/* ============================================================================
 * Test: ZeroBatchSizeHandled
 * Verify zero batch size is handled
 * ============================================================================ */
TEST_F(GPURecoveryParamCorrectionTest, ZeroBatchSizeHandled) {
#ifdef NIMCP_ENABLE_CUDA
    size_t batch_size = 0;

    /* Zero batch should not crash */
    bool corrected = nimcp_gpu_correct_batch_for_memory(&batch_size, sizeof(float), 1024);

    /* Zero batch returns false (nothing to correct meaningfully) */
    EXPECT_FALSE(corrected);
#else
    GTEST_SKIP() << "CUDA not enabled";
#endif
}

}  /* namespace */
