/**
 * @file test_neuron_bridge.cpp
 * @brief Unit tests for AWS Neuron/Inferentia NeuronCore bridge
 *
 * WHAT: Tests Neuron context lifecycle, cache management, and stub fallbacks
 * WHY:  Verify Neuron integration works on non-Inferentia systems (stubs)
 *       and on Inferentia systems (real NRT)
 * HOW:  GTest suite testing creation, destruction, and graceful degradation
 *
 * NOTE: Tests that require actual Inferentia hardware use GTEST_SKIP().
 *       Stub/fallback tests run on all systems.
 */

#include <gtest/gtest.h>

/* NOTE: Do NOT wrap these in extern "C" — they transitively include CUDA
 * headers (cuda_runtime.h, cublas_v2.h) which contain C++ templates. The
 * C headers themselves already have extern "C" guards internally. */
#include "gpu/neuron/nimcp_neuron_context.h"
#include "gpu/neuron/nimcp_neuron_bridge.h"
#include "gpu/execution/nimcp_gpu_detect.h"
#include "gpu/backend/nimcp_kernel_backend.h"

//=============================================================================
// Neuron Context Tests (work on all systems via stubs)
//=============================================================================

class NeuronContextTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(NeuronContextTest, CreateReturnsNullOnNonInferentiaSystem) {
    // On systems without NRT, this should return NULL gracefully
    nimcp_neuron_context_t* ctx = nimcp_neuron_context_create(0);

    // On Inferentia: non-NULL; on other systems: NULL (stub)
    // Either way, cleanup should be safe
    if (ctx) {
        EXPECT_TRUE(nimcp_neuron_context_is_valid(ctx));
        nimcp_neuron_context_destroy(ctx);
    } else {
        // Stubs return NULL — expected on non-Inferentia
        SUCCEED();
    }
}

TEST_F(NeuronContextTest, DestroyHandlesNull) {
    // Should not crash
    nimcp_neuron_context_destroy(NULL);
    SUCCEED();
}

TEST_F(NeuronContextTest, IsValidReturnsFalseForNull) {
    EXPECT_FALSE(nimcp_neuron_context_is_valid(NULL));
}

TEST_F(NeuronContextTest, LoadNeffFailsWithNullContext) {
    int result = nimcp_neuron_load_neff(NULL, "/tmp/test.neff");
    EXPECT_EQ(result, -1);
}

TEST_F(NeuronContextTest, UnloadModelFailsWithNullContext) {
    int result = nimcp_neuron_unload_model(NULL);
    EXPECT_EQ(result, -1);
}

TEST_F(NeuronContextTest, ExecuteFailsWithNullContext) {
    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float output[2] = {0};
    int result = nimcp_neuron_execute(NULL, input, sizeof(input), output, sizeof(output));
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Neuron Bridge / Cache Tests
//=============================================================================

class NeuronBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(NeuronBridgeTest, CacheCreateFailsWithNullContext) {
    uint32_t layers[] = {128, 64, 32};
    nimcp_neuron_inference_cache_t* cache = nimcp_neuron_cache_create(NULL, layers, 3);
    EXPECT_EQ(cache, nullptr);
}

TEST_F(NeuronBridgeTest, CacheCreateFailsWithNullLayerSizes) {
    // Even with a valid-looking context pointer, NULL layer_sizes should fail
    nimcp_neuron_context_t* ctx = nimcp_neuron_context_create(0);
    nimcp_neuron_inference_cache_t* cache = nimcp_neuron_cache_create(ctx, NULL, 3);
    EXPECT_EQ(cache, nullptr);
    nimcp_neuron_context_destroy(ctx);
}

TEST_F(NeuronBridgeTest, CacheCreateFailsWithTooFewLayers) {
    nimcp_neuron_context_t* ctx = nimcp_neuron_context_create(0);
    uint32_t layers[] = {128};
    nimcp_neuron_inference_cache_t* cache = nimcp_neuron_cache_create(ctx, layers, 1);
    EXPECT_EQ(cache, nullptr);
    nimcp_neuron_context_destroy(ctx);
}

TEST_F(NeuronBridgeTest, CacheDestroyHandlesNull) {
    // Should not crash
    nimcp_neuron_cache_destroy(NULL);
    SUCCEED();
}

TEST_F(NeuronBridgeTest, ForwardPassFailsWithNullCache) {
    float input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float output[2] = {0};
    int result = nimcp_neuron_forward_pass(NULL, input, 4, output, 2);
    EXPECT_EQ(result, -1);
}

TEST_F(NeuronBridgeTest, ForwardPassBatchFailsWithNullCache) {
    float inputs[8] = {0};
    float outputs[4] = {0};
    int result = nimcp_neuron_forward_pass_batch(NULL, inputs, 2, 4, outputs, 2);
    EXPECT_EQ(result, -1);
}

TEST_F(NeuronBridgeTest, IsReadyReturnsFalseForNull) {
    EXPECT_FALSE(nimcp_neuron_is_ready(NULL));
}

TEST_F(NeuronBridgeTest, InvalidateWeightsHandlesNull) {
    // Should not crash
    nimcp_neuron_invalidate_weights(NULL);
    SUCCEED();
}

TEST_F(NeuronBridgeTest, ExportOnnxFailsWithNullParams) {
    int result = nimcp_neuron_export_onnx(NULL, NULL, "/tmp/test.onnx");
    EXPECT_EQ(result, -1);
}

TEST_F(NeuronBridgeTest, CompileNeffFailsWithNullParams) {
    int result = nimcp_neuron_compile_neff(NULL, NULL, "/tmp/test.neff");
    EXPECT_EQ(result, -1);
}

TEST_F(NeuronBridgeTest, LoadNeffModelFailsWithNullParams) {
    int result = nimcp_neuron_load_neff_model(NULL, "/tmp/test.neff");
    EXPECT_EQ(result, -1);
}

//=============================================================================
// GPU Detection — Neuron Backend
//=============================================================================

class NeuronDetectionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(NeuronDetectionTest, BackendNameIncludesNeuron) {
    const char* name = gpu_backend_name(GPU_BACKEND_NEURON);
    EXPECT_STREQ(name, "Neuron");
}

TEST_F(NeuronDetectionTest, VendorNameIncludesAWS) {
    const char* name = gpu_vendor_name(GPU_VENDOR_AWS);
    EXPECT_STREQ(name, "AWS");
}

TEST_F(NeuronDetectionTest, BackendTypeNameIncludesNeuron) {
    const char* name = nimcp_backend_type_name(NIMCP_BACKEND_NEURON);
    EXPECT_STREQ(name, "Neuron");
}

TEST_F(NeuronDetectionTest, DetectionReportsNeuronField) {
    gpu_detect_result_t caps = {};
    gpu_detect_capabilities(&caps);

    // On Inferentia: neuron_available = true
    // On other systems: neuron_available = false
    // Either way, the field should be accessible
    if (caps.neuron_available) {
        EXPECT_GT(caps.neuron_device_count, 0u);
        EXPECT_GT(caps.neuron_cores_per_device, 0u);
        EXPECT_TRUE(caps.available_backends & GPU_BACKEND_NEURON);
    } else {
        EXPECT_EQ(caps.neuron_device_count, 0u);
    }
}
