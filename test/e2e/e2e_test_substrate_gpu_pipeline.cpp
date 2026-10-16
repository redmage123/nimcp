/**
 * @file e2e_test_substrate_gpu_pipeline.cpp
 * @brief End-to-end tests for GPU Neural Substrate pipeline
 *
 * WHAT: Complete E2E testing of GPU substrate from initialization to simulation
 * WHY:  Validate full GPU-accelerated neural substrate workflow
 * HOW:  Test realistic neural simulation scenarios with all substrate components
 *
 * @version 1.0
 * @date 2026
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <chrono>

#include "gpu/substrate/nimcp_substrate_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

// Headers have their own extern "C" guards
#include "utils/memory/nimcp_memory.h"

namespace {
    constexpr uint32_t NUM_NEURONS = 1000;
    constexpr uint32_t NUM_AXONS = 1000;
    constexpr uint32_t NUM_DENDRITES = 2000;
    constexpr uint32_t SEGMENTS_PER_DENDRITE = 20;
    constexpr uint32_t SPINES_PER_DENDRITE = 50;
    constexpr uint32_t NUM_NEUROMOD_POOLS = 10;
    constexpr uint32_t NUM_NEUROMOD_TYPES = 4;
    constexpr uint32_t NUM_SYNAPSES = 10000;
    constexpr uint32_t NUM_ASTROCYTES = 500;
    constexpr uint32_t NUM_MICROGLIA = 100;
    constexpr uint32_t NUM_OPCS = 50;
    constexpr uint32_t NUM_METABOLIC_REGIONS = 20;
    constexpr float SIMULATION_DT = 0.0001f;
    constexpr int WARMUP_STEPS = 100;
    constexpr int BENCHMARK_STEPS = 1000;
}

class SubstrateGPUE2ETest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    substrate_gpu_context_t* substrate_ctx = nullptr;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
        nimcp_memory_clear_stats();

        bool init_ok = nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);
        ASSERT_TRUE(init_ok);

        if (nimcp_cuda_backend_available()) {
            gpu_ctx = nimcp_gpu_context_create(0);
        }
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
        EXPECT_LT(stats.current_allocated, 8192);
    }

    bool initializeFullSubstrate() {
        if (!gpu_ctx) return false;

        substrate_gpu_config_t config = substrate_gpu_default_config();
        config.axon.max_axons = NUM_AXONS * 2;
        config.dendrite.max_dendrites = NUM_DENDRITES * 2;
        config.neuromod.max_pools = NUM_NEUROMOD_POOLS * 2;

        substrate_ctx = substrate_gpu_create(gpu_ctx, &config);
        if (!substrate_ctx) return false;

        if (substrate_gpu_init_axons(substrate_ctx, NUM_AXONS) != 0) return false;
        if (substrate_gpu_init_dendrites(substrate_ctx, NUM_DENDRITES,
                                         SEGMENTS_PER_DENDRITE,
                                         NUM_DENDRITES * SPINES_PER_DENDRITE) != 0) return false;
        if (substrate_gpu_init_myelin(substrate_ctx, NUM_AXONS) != 0) return false;
        if (substrate_gpu_init_neuromod(substrate_ctx, NUM_NEUROMOD_POOLS,
                                        NUM_NEUROMOD_TYPES, NUM_SYNAPSES) != 0) return false;
        if (substrate_gpu_init_glial(substrate_ctx, NUM_ASTROCYTES,
                                     NUM_MICROGLIA, NUM_OPCS, 6) != 0) return false;
        if (substrate_gpu_init_metabolic(substrate_ctx, NUM_METABOLIC_REGIONS) != 0) return false;

        return true;
    }

    double measureThroughput(int num_steps) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_steps; i++) {
            if (substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, SIMULATION_DT) != 0)
                return -1.0;
        }
        auto end = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        return (NUM_NEURONS * num_steps) / (elapsed_ms / 1000.0);
    }
};

TEST_F(SubstrateGPUE2ETest, FullSubstratePipelineInitialization) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    ASSERT_TRUE(initializeFullSubstrate());

    substrate_axon_tensors_t* axons = substrate_gpu_get_axon_tensors(substrate_ctx);
    EXPECT_NE(axons, nullptr);
    if (axons) EXPECT_NE(axons->signals, nullptr);

    substrate_dendrite_tensors_t* dend = substrate_gpu_get_dendrite_tensors(substrate_ctx);
    EXPECT_NE(dend, nullptr);
    if (dend) EXPECT_NE(dend->voltages, nullptr);

    substrate_myelin_tensors_t* myelin = substrate_gpu_get_myelin_tensors(substrate_ctx);
    EXPECT_NE(myelin, nullptr);
    if (myelin) EXPECT_NE(myelin->g_ratio, nullptr);

    substrate_neuromod_tensors_t* neuromod = substrate_gpu_get_neuromod_tensors(substrate_ctx);
    EXPECT_NE(neuromod, nullptr);
    if (neuromod) EXPECT_NE(neuromod->concentrations, nullptr);

    substrate_glial_tensors_t* glial = substrate_gpu_get_glial_tensors(substrate_ctx);
    EXPECT_NE(glial, nullptr);
    if (glial) EXPECT_NE(glial->astro_calcium, nullptr);

    substrate_metabolic_tensors_t* metabolic = substrate_gpu_get_metabolic_tensors(substrate_ctx);
    EXPECT_NE(metabolic, nullptr);
    if (metabolic) EXPECT_NE(metabolic->atp_levels, nullptr);
}

TEST_F(SubstrateGPUE2ETest, FullSubstratePipelineSimulation) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    ASSERT_TRUE(initializeFullSubstrate());

    const int SIMULATION_STEPS = 100;
    for (int step = 0; step < SIMULATION_STEPS; step++) {
        int result = substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, SIMULATION_DT);
        ASSERT_EQ(result, 0) << "Simulation failed at step " << step;
    }
}

TEST_F(SubstrateGPUE2ETest, SubsystemCouplingValidation) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    ASSERT_TRUE(initializeFullSubstrate());

    EXPECT_EQ(substrate_gpu_axon_step(substrate_ctx, nullptr, SIMULATION_DT), 0);
    EXPECT_EQ(substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, SIMULATION_DT), 0);
    EXPECT_EQ(substrate_gpu_myelin_step(substrate_ctx, SIMULATION_DT), 0);
    EXPECT_EQ(substrate_gpu_neuromod_step(substrate_ctx, SIMULATION_DT), 0);
    EXPECT_EQ(substrate_gpu_glial_step(substrate_ctx, SIMULATION_DT), 0);
    EXPECT_EQ(substrate_gpu_metabolic_step(substrate_ctx, nullptr, SIMULATION_DT), 0);
}

TEST_F(SubstrateGPUE2ETest, PerformanceBenchmark) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    ASSERT_TRUE(initializeFullSubstrate());

    for (int i = 0; i < WARMUP_STEPS; i++) {
        substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, SIMULATION_DT);
    }

    double throughput = measureThroughput(BENCHMARK_STEPS);
    ASSERT_GT(throughput, 0);

    std::cout << "GPU Substrate Throughput: " << (throughput / 1e6)
              << " million neurons/sec" << std::endl;
    EXPECT_GT(throughput, 100000.0);
}

TEST_F(SubstrateGPUE2ETest, ExtendedSimulationStability) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";
    ASSERT_TRUE(initializeFullSubstrate());

    const int EXTENDED_STEPS = 1000;
    for (int step = 0; step < EXTENDED_STEPS; step++) {
        int result = substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, SIMULATION_DT);
        ASSERT_EQ(result, 0) << "Extended simulation failed at step " << step;
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
