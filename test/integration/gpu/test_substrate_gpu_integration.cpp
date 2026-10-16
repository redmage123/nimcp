/**
 * @file test_substrate_gpu_integration.cpp
 * @brief Integration tests for GPU Neural Substrate in NIMCP
 *
 * WHAT: Verify GPU substrate integration with brain factory and kernel backend
 * WHY:  Ensure substrate GPU operations work correctly in full system context
 * HOW:  Test brain creation with substrate GPU, kernel dispatch, and CPU fallback
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
    constexpr uint32_t TEST_AXONS = 100;
    constexpr uint32_t TEST_DENDRITES = 200;
    constexpr uint32_t TEST_SEGMENTS = 10;
    constexpr uint32_t TEST_SPINES = 500;
    constexpr uint32_t TEST_POOLS = 4;
    constexpr uint32_t TEST_TYPES = 4;
    constexpr uint32_t TEST_SYNAPSES = 1000;
    constexpr float TEST_DT = 0.001f;
}

class SubstrateGPUIntegrationTest : public ::testing::Test {
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

    substrate_gpu_context_t* createSubstrateContext() {
        if (!gpu_ctx) return nullptr;
        substrate_gpu_config_t config = substrate_gpu_default_config();
        config.axon.max_axons = TEST_AXONS * 2;
        config.dendrite.max_dendrites = TEST_DENDRITES * 2;
        config.neuromod.max_pools = TEST_POOLS * 2;
        return substrate_gpu_create(gpu_ctx, &config);
    }
};

// Context Tests
TEST_F(SubstrateGPUIntegrationTest, ContextCreationWithGPU) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    substrate_ctx = createSubstrateContext();
    ASSERT_NE(substrate_ctx, nullptr);
}

TEST_F(SubstrateGPUIntegrationTest, ContextCreationFallbackWithoutGPU) {
    substrate_gpu_context_t* ctx = substrate_gpu_create(nullptr, nullptr);
    EXPECT_EQ(ctx, nullptr);
}

// Subsystem Initialization Tests
TEST_F(SubstrateGPUIntegrationTest, AxonSubsystemInitialization) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    substrate_ctx = createSubstrateContext();
    ASSERT_NE(substrate_ctx, nullptr);

    int result = substrate_gpu_init_axons(substrate_ctx, TEST_AXONS);
    EXPECT_EQ(result, 0);

    substrate_axon_tensors_t* tensors = substrate_gpu_get_axon_tensors(substrate_ctx);
    EXPECT_NE(tensors, nullptr);
    if (tensors) {
        EXPECT_NE(tensors->signals, nullptr);
        EXPECT_NE(tensors->velocities, nullptr);
        EXPECT_NE(tensors->myelination, nullptr);
    }
}

TEST_F(SubstrateGPUIntegrationTest, DendriteSubsystemInitialization) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    substrate_ctx = createSubstrateContext();
    ASSERT_NE(substrate_ctx, nullptr);

    int result = substrate_gpu_init_dendrites(substrate_ctx, TEST_DENDRITES, TEST_SEGMENTS, TEST_SPINES);
    EXPECT_EQ(result, 0);

    substrate_dendrite_tensors_t* tensors = substrate_gpu_get_dendrite_tensors(substrate_ctx);
    EXPECT_NE(tensors, nullptr);
    if (tensors) {
        EXPECT_NE(tensors->voltages, nullptr);
        EXPECT_NE(tensors->calcium, nullptr);
    }
}

TEST_F(SubstrateGPUIntegrationTest, MyelinSubsystemInitialization) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    substrate_ctx = createSubstrateContext();
    ASSERT_NE(substrate_ctx, nullptr);

    int result = substrate_gpu_init_myelin(substrate_ctx, TEST_AXONS);
    EXPECT_EQ(result, 0);

    substrate_myelin_tensors_t* tensors = substrate_gpu_get_myelin_tensors(substrate_ctx);
    EXPECT_NE(tensors, nullptr);
    if (tensors) {
        EXPECT_NE(tensors->g_ratio, nullptr);
        EXPECT_NE(tensors->thickness, nullptr);
    }
}

TEST_F(SubstrateGPUIntegrationTest, NeuromodSubsystemInitialization) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    substrate_ctx = createSubstrateContext();
    ASSERT_NE(substrate_ctx, nullptr);

    int result = substrate_gpu_init_neuromod(substrate_ctx, TEST_POOLS, TEST_TYPES, TEST_SYNAPSES);
    EXPECT_EQ(result, 0);

    substrate_neuromod_tensors_t* tensors = substrate_gpu_get_neuromod_tensors(substrate_ctx);
    EXPECT_NE(tensors, nullptr);
    if (tensors) {
        EXPECT_NE(tensors->concentrations, nullptr);
    }
}

TEST_F(SubstrateGPUIntegrationTest, GlialSubsystemInitialization) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    substrate_ctx = createSubstrateContext();
    ASSERT_NE(substrate_ctx, nullptr);

    int result = substrate_gpu_init_glial(substrate_ctx, 50, 10, 5, 6);
    EXPECT_EQ(result, 0);

    substrate_glial_tensors_t* tensors = substrate_gpu_get_glial_tensors(substrate_ctx);
    EXPECT_NE(tensors, nullptr);
    if (tensors) {
        EXPECT_NE(tensors->astro_calcium, nullptr);
        EXPECT_NE(tensors->micro_state, nullptr);
    }
}

TEST_F(SubstrateGPUIntegrationTest, MetabolicSubsystemInitialization) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    substrate_ctx = createSubstrateContext();
    ASSERT_NE(substrate_ctx, nullptr);

    int result = substrate_gpu_init_metabolic(substrate_ctx, 10);
    EXPECT_EQ(result, 0);

    substrate_metabolic_tensors_t* tensors = substrate_gpu_get_metabolic_tensors(substrate_ctx);
    EXPECT_NE(tensors, nullptr);
    if (tensors) {
        EXPECT_NE(tensors->atp_levels, nullptr);
        EXPECT_NE(tensors->oxygen_levels, nullptr);
    }
}

// Step Execution Tests
TEST_F(SubstrateGPUIntegrationTest, AxonStepExecution) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    substrate_ctx = createSubstrateContext();
    ASSERT_NE(substrate_ctx, nullptr);
    ASSERT_EQ(substrate_gpu_init_axons(substrate_ctx, TEST_AXONS), 0);

    int result = substrate_gpu_axon_step(substrate_ctx, nullptr, TEST_DT);
    EXPECT_EQ(result, 0);
}

TEST_F(SubstrateGPUIntegrationTest, DendriteStepExecution) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    substrate_ctx = createSubstrateContext();
    ASSERT_NE(substrate_ctx, nullptr);
    ASSERT_EQ(substrate_gpu_init_dendrites(substrate_ctx, TEST_DENDRITES, TEST_SEGMENTS, TEST_SPINES), 0);

    int result = substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, TEST_DT);
    EXPECT_EQ(result, 0);
}

TEST_F(SubstrateGPUIntegrationTest, NeuromodStepExecution) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    substrate_ctx = createSubstrateContext();
    ASSERT_NE(substrate_ctx, nullptr);
    ASSERT_EQ(substrate_gpu_init_neuromod(substrate_ctx, TEST_POOLS, TEST_TYPES, TEST_SYNAPSES), 0);

    int result = substrate_gpu_neuromod_step(substrate_ctx, TEST_DT);
    EXPECT_EQ(result, 0);
}

TEST_F(SubstrateGPUIntegrationTest, FullStepExecution) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    substrate_ctx = createSubstrateContext();
    ASSERT_NE(substrate_ctx, nullptr);

    ASSERT_EQ(substrate_gpu_init_axons(substrate_ctx, TEST_AXONS), 0);
    ASSERT_EQ(substrate_gpu_init_dendrites(substrate_ctx, TEST_DENDRITES, TEST_SEGMENTS, TEST_SPINES), 0);
    ASSERT_EQ(substrate_gpu_init_myelin(substrate_ctx, TEST_AXONS), 0);
    ASSERT_EQ(substrate_gpu_init_neuromod(substrate_ctx, TEST_POOLS, TEST_TYPES, TEST_SYNAPSES), 0);
    ASSERT_EQ(substrate_gpu_init_glial(substrate_ctx, 50, 10, 5, 6), 0);
    ASSERT_EQ(substrate_gpu_init_metabolic(substrate_ctx, 10), 0);

    int result = substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, TEST_DT);
    EXPECT_EQ(result, 0);
}

// Kernel Backend Tests
TEST_F(SubstrateGPUIntegrationTest, KernelBackendHasSubstrateOps) {
    ASSERT_NE(backend, nullptr);
    EXPECT_NE(backend->substrate.axon_propagate, nullptr);
    EXPECT_NE(backend->substrate.axon_refractory, nullptr);
    EXPECT_NE(backend->substrate.dendrite_cable_integrate, nullptr);
    EXPECT_NE(backend->substrate.myelin_g_ratio, nullptr);
    EXPECT_NE(backend->substrate.neuromod_decay, nullptr);
    EXPECT_NE(backend->substrate.astrocyte_calcium_wave, nullptr);
    EXPECT_NE(backend->substrate.metabolic_effects, nullptr);
}

// Multi-Step Test
TEST_F(SubstrateGPUIntegrationTest, MultiStepSimulation) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    substrate_ctx = createSubstrateContext();
    ASSERT_NE(substrate_ctx, nullptr);

    ASSERT_EQ(substrate_gpu_init_axons(substrate_ctx, TEST_AXONS), 0);
    ASSERT_EQ(substrate_gpu_init_dendrites(substrate_ctx, TEST_DENDRITES, TEST_SEGMENTS, TEST_SPINES), 0);
    ASSERT_EQ(substrate_gpu_init_neuromod(substrate_ctx, TEST_POOLS, TEST_TYPES, TEST_SYNAPSES), 0);
    ASSERT_EQ(substrate_gpu_init_glial(substrate_ctx, 50, 10, 5, 6), 0);
    ASSERT_EQ(substrate_gpu_init_metabolic(substrate_ctx, 10), 0);

    for (int i = 0; i < 100; i++) {
        int result = substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, TEST_DT);
        ASSERT_EQ(result, 0) << "Step " << i << " failed";
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
