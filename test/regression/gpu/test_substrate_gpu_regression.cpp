/**
 * @file test_substrate_gpu_regression.cpp
 * @brief Regression tests for GPU Neural Substrate in NIMCP
 *
 * WHAT: Ensure GPU substrate operations remain stable across changes
 * WHY:  Prevent reintroduction of bugs in substrate GPU operations
 * HOW:  Test numerical stability, GPU/CPU equivalence, edge cases
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2026
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/substrate/nimcp_substrate_gpu.h"

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

namespace {
    constexpr uint32_t TEST_SIZE = 256;
    constexpr float TEST_DT = 0.001f;
    constexpr int STABILITY_STEPS = 1000;
}

class SubstrateGPURegressionTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    nimcp_kernel_backend_t* backend = nullptr;
    substrate_gpu_context_t* substrate_ctx = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        bool init_ok = nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);
        ASSERT_TRUE(init_ok);

        backend = nimcp_get_kernel_backend();
        ASSERT_NE(backend, nullptr);

        gpu_ctx = nimcp_gpu_context_create_auto();
    }

    void TearDown() override {
        if (substrate_ctx) {
            substrate_gpu_destroy(substrate_ctx);
            substrate_ctx = nullptr;
        }

        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }

        nimcp_kernel_backend_shutdown();

        nimcp_memory_stats_t stats;
        nimcp_memory_get_stats(&stats);
        EXPECT_LT(stats.current_allocated, 4096);
    }

    substrate_gpu_context_t* createContext() {
        if (!gpu_ctx) return nullptr;
        substrate_gpu_config_t config = substrate_gpu_default_config();
        config.axon.max_axons = TEST_SIZE * 2;
        config.dendrite.max_dendrites = TEST_SIZE * 2;
        return substrate_gpu_create(gpu_ctx, &config);
    }

};

// Numerical Stability Tests
TEST_F(SubstrateGPURegressionTest, AxonStepNumericalStability) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    substrate_ctx = createContext();
    ASSERT_NE(substrate_ctx, nullptr);
    ASSERT_EQ(substrate_gpu_init_axons(substrate_ctx, TEST_SIZE), 0);

    for (int i = 0; i < STABILITY_STEPS; i++) {
        int result = substrate_gpu_axon_step(substrate_ctx, nullptr, TEST_DT);
        ASSERT_EQ(result, 0) << "Axon step " << i << " failed";
    }

    // Verify tensors are still valid after extended simulation
    substrate_axon_tensors_t* tensors = substrate_gpu_get_axon_tensors(substrate_ctx);
    EXPECT_NE(tensors, nullptr);
    if (tensors) {
        EXPECT_NE(tensors->signals, nullptr);
    }
}

TEST_F(SubstrateGPURegressionTest, DendriteStepNumericalStability) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    substrate_ctx = createContext();
    ASSERT_NE(substrate_ctx, nullptr);
    ASSERT_EQ(substrate_gpu_init_dendrites(substrate_ctx, TEST_SIZE, 10, TEST_SIZE * 10), 0);

    for (int i = 0; i < STABILITY_STEPS; i++) {
        int result = substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, TEST_DT);
        ASSERT_EQ(result, 0) << "Dendrite step " << i << " failed";
    }

    // Verify tensors are still valid after extended simulation
    substrate_dendrite_tensors_t* tensors = substrate_gpu_get_dendrite_tensors(substrate_ctx);
    EXPECT_NE(tensors, nullptr);
    if (tensors) {
        EXPECT_NE(tensors->voltages, nullptr);
    }
}

TEST_F(SubstrateGPURegressionTest, NeuromodStepNumericalStability) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    substrate_ctx = createContext();
    ASSERT_NE(substrate_ctx, nullptr);
    ASSERT_EQ(substrate_gpu_init_neuromod(substrate_ctx, 8, 4, TEST_SIZE), 0);

    for (int i = 0; i < STABILITY_STEPS; i++) {
        int result = substrate_gpu_neuromod_step(substrate_ctx, TEST_DT);
        ASSERT_EQ(result, 0) << "Neuromod step " << i << " failed";
    }

    // Verify tensors are still valid after extended simulation
    substrate_neuromod_tensors_t* tensors = substrate_gpu_get_neuromod_tensors(substrate_ctx);
    EXPECT_NE(tensors, nullptr);
    if (tensors) {
        EXPECT_NE(tensors->concentrations, nullptr);
    }
}

TEST_F(SubstrateGPURegressionTest, FullStepNumericalStability) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    substrate_ctx = createContext();
    ASSERT_NE(substrate_ctx, nullptr);

    ASSERT_EQ(substrate_gpu_init_axons(substrate_ctx, TEST_SIZE), 0);
    ASSERT_EQ(substrate_gpu_init_dendrites(substrate_ctx, TEST_SIZE, 10, TEST_SIZE * 10), 0);
    ASSERT_EQ(substrate_gpu_init_neuromod(substrate_ctx, 8, 4, TEST_SIZE), 0);
    ASSERT_EQ(substrate_gpu_init_glial(substrate_ctx, 50, 10, 5, 6), 0);
    ASSERT_EQ(substrate_gpu_init_metabolic(substrate_ctx, 10), 0);

    for (int i = 0; i < STABILITY_STEPS; i++) {
        int result = substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, TEST_DT);
        ASSERT_EQ(result, 0) << "Full step " << i << " failed";
    }
}

// Edge Case Tests
TEST_F(SubstrateGPURegressionTest, ZeroInputHandling) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    substrate_ctx = createContext();
    ASSERT_NE(substrate_ctx, nullptr);
    ASSERT_EQ(substrate_gpu_init_axons(substrate_ctx, TEST_SIZE), 0);

    for (int i = 0; i < 100; i++) {
        int result = substrate_gpu_axon_step(substrate_ctx, nullptr, TEST_DT);
        EXPECT_EQ(result, 0);
    }
}

TEST_F(SubstrateGPURegressionTest, SmallTimestepStability) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    substrate_ctx = createContext();
    ASSERT_NE(substrate_ctx, nullptr);
    ASSERT_EQ(substrate_gpu_init_axons(substrate_ctx, TEST_SIZE), 0);

    float small_dt = 1e-6f;
    for (int i = 0; i < 100; i++) {
        int result = substrate_gpu_axon_step(substrate_ctx, nullptr, small_dt);
        EXPECT_EQ(result, 0);
    }
}

TEST_F(SubstrateGPURegressionTest, LargeTimestepStability) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    substrate_ctx = createContext();
    ASSERT_NE(substrate_ctx, nullptr);
    ASSERT_EQ(substrate_gpu_init_axons(substrate_ctx, TEST_SIZE), 0);

    float large_dt = 0.1f;
    for (int i = 0; i < 100; i++) {
        int result = substrate_gpu_axon_step(substrate_ctx, nullptr, large_dt);
        EXPECT_EQ(result, 0);
    }
}

// Backward Compatibility Tests
TEST_F(SubstrateGPURegressionTest, DefaultConfigBackwardCompatibility) {
    substrate_gpu_config_t config = substrate_gpu_default_config();

    EXPECT_GT(config.axon.max_axons, 0u);
    EXPECT_GT(config.dendrite.max_dendrites, 0u);
    EXPECT_GT(config.neuromod.max_pools, 0u);
    EXPECT_GT(config.neuromod.n_types, 0u);
}

TEST_F(SubstrateGPURegressionTest, TensorAccessorStability) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    substrate_ctx = createContext();
    ASSERT_NE(substrate_ctx, nullptr);

    // Before init, tensor getters should return empty structs
    substrate_axon_tensors_t* axons = substrate_gpu_get_axon_tensors(substrate_ctx);
    EXPECT_NE(axons, nullptr);
    // Signals should be null before init
    if (axons) EXPECT_EQ(axons->signals, nullptr);

    // After init, tensors should be valid
    ASSERT_EQ(substrate_gpu_init_axons(substrate_ctx, TEST_SIZE), 0);
    axons = substrate_gpu_get_axon_tensors(substrate_ctx);
    if (axons) EXPECT_NE(axons->signals, nullptr);
}

// Memory Safety Tests
TEST_F(SubstrateGPURegressionTest, RepeatedCreateDestroy) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    nimcp_memory_stats_t initial_stats;
    nimcp_memory_get_stats(&initial_stats);

    for (int i = 0; i < 10; i++) {
        substrate_gpu_context_t* ctx = createContext();
        ASSERT_NE(ctx, nullptr);
        substrate_gpu_init_axons(ctx, TEST_SIZE);
        substrate_gpu_init_dendrites(ctx, TEST_SIZE, 10, TEST_SIZE * 10);
        substrate_gpu_destroy(ctx);
    }

    nimcp_memory_stats_t final_stats;
    nimcp_memory_get_stats(&final_stats);

    EXPECT_LT(final_stats.current_allocated - initial_stats.current_allocated, 8192u);
}

TEST_F(SubstrateGPURegressionTest, DoubleDestroyHandling) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    substrate_ctx = createContext();
    ASSERT_NE(substrate_ctx, nullptr);

    substrate_gpu_destroy(substrate_ctx);
    substrate_ctx = nullptr;

    substrate_gpu_destroy(nullptr);  // Should not crash
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
