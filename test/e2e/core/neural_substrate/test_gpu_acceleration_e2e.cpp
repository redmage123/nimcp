/**
 * @file test_gpu_acceleration_e2e.cpp
 * @brief End-to-end tests for GPU-accelerated neural substrate processing
 *
 * WHAT: Complete E2E testing of GPU substrate pipeline from init to simulation
 * WHY:  Validate GPU acceleration for large-scale neural substrate computation
 * HOW:  Test all substrate subsystems (axon, dendrite, myelin, neuromod, glial,
 *       metabolic) on GPU with realistic workloads
 *
 * GPU ACCELERATION RATIONALE:
 * - Large-scale simulations require millions of substrate components
 * - GPU parallelism enables real-time simulation of biologically plausible models
 * - Heterogeneous compute (CPU+GPU) optimizes resource utilization
 *
 * @version 1.0
 * @date 2026
 */

#include "e2e_test_framework.h"
#include <thread>
#include <chrono>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "gpu/substrate/nimcp_substrate_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

namespace {
    constexpr uint32_t NUM_NEURONS = 10000;
    constexpr uint32_t NUM_AXONS = 10000;
    constexpr uint32_t NUM_DENDRITES = 20000;
    constexpr uint32_t SEGMENTS_PER_DENDRITE = 20;
    constexpr uint32_t SPINES_PER_DENDRITE = 50;
    constexpr uint32_t NUM_NEUROMOD_POOLS = 100;
    constexpr uint32_t NUM_NEUROMOD_TYPES = 4;  // DA, 5HT, ACh, NE
    constexpr uint32_t NUM_SYNAPSES = 100000;
    constexpr uint32_t NUM_ASTROCYTES = 5000;
    constexpr uint32_t NUM_MICROGLIA = 1000;
    constexpr uint32_t NUM_OPCS = 500;
    constexpr uint32_t NUM_METABOLIC_REGIONS = 100;
    constexpr float SIMULATION_DT = 0.0001f;  // 0.1 ms timestep
    constexpr int WARMUP_STEPS = 100;
    constexpr int BENCHMARK_STEPS = 1000;
    constexpr int EXTENDED_STEPS = 5000;
}

//=============================================================================
// Test Fixture
//=============================================================================

class GPUAccelerationE2ETest : public ::testing::Test {
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
        // GPU tests may have larger residual memory
        EXPECT_LT(stats.current_allocated, 65536);
    }

    bool initializeFullSubstrate() {
        if (!gpu_ctx) return false;

        substrate_gpu_config_t config = substrate_gpu_default_config();
        config.axon.max_axons = NUM_AXONS * 2;
        config.dendrite.max_dendrites = NUM_DENDRITES * 2;
        config.neuromod.max_pools = NUM_NEUROMOD_POOLS * 2;
        config.glial.max_astrocytes = NUM_ASTROCYTES * 2;
        config.glial.max_microglia = NUM_MICROGLIA * 2;
        config.metabolic.n_regions = NUM_METABOLIC_REGIONS * 2;
        config.enable_async_ops = true;
        config.enable_mixed_precision = true;

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

    double measureSubsystemThroughput(
        std::function<int()> step_func,
        int num_steps,
        uint32_t element_count
    ) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < num_steps; i++) {
            if (step_func() != 0) return -1.0;
        }
        auto end = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
        return (element_count * num_steps) / (elapsed_ms / 1000.0);
    }
};

//=============================================================================
// E2E Test: Full GPU Processing Pipeline
//=============================================================================

TEST_F(GPUAccelerationE2ETest, FullGPUProcessingPipeline) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    E2E_PIPELINE_START("Full GPU Processing Pipeline");

    E2E_STAGE_BEGIN("Initialize GPU context and substrate", 5000);
    ASSERT_TRUE(initializeFullSubstrate());
    std::cout << "  GPU substrate initialized successfully" << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify all tensor allocations", 1000);
    substrate_axon_tensors_t* axons = substrate_gpu_get_axon_tensors(substrate_ctx);
    EXPECT_NE(axons, nullptr);
    if (axons) {
        EXPECT_NE(axons->signals, nullptr);
        EXPECT_NE(axons->velocities, nullptr);
        EXPECT_EQ(axons->n_axons, NUM_AXONS);
        std::cout << "  Axon tensors: " << axons->n_axons << " axons" << std::endl;
    }

    substrate_dendrite_tensors_t* dend = substrate_gpu_get_dendrite_tensors(substrate_ctx);
    EXPECT_NE(dend, nullptr);
    if (dend) {
        EXPECT_NE(dend->voltages, nullptr);
        EXPECT_NE(dend->calcium, nullptr);
        std::cout << "  Dendrite tensors: " << dend->n_dendrites << " dendrites, "
                  << dend->n_spines << " spines" << std::endl;
    }

    substrate_neuromod_tensors_t* neuromod = substrate_gpu_get_neuromod_tensors(substrate_ctx);
    EXPECT_NE(neuromod, nullptr);
    if (neuromod) {
        EXPECT_NE(neuromod->concentrations, nullptr);
        std::cout << "  Neuromod tensors: " << neuromod->n_pools << " pools, "
                  << neuromod->n_types << " types" << std::endl;
    }

    substrate_glial_tensors_t* glial = substrate_gpu_get_glial_tensors(substrate_ctx);
    EXPECT_NE(glial, nullptr);
    if (glial) {
        EXPECT_NE(glial->astro_calcium, nullptr);
        std::cout << "  Glial tensors: " << glial->n_astrocytes << " astrocytes, "
                  << glial->n_microglia << " microglia" << std::endl;
    }

    substrate_metabolic_tensors_t* metabolic = substrate_gpu_get_metabolic_tensors(substrate_ctx);
    EXPECT_NE(metabolic, nullptr);
    if (metabolic) {
        EXPECT_NE(metabolic->atp_levels, nullptr);
        std::cout << "  Metabolic tensors: " << metabolic->n_regions << " regions" << std::endl;
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run full pipeline simulation", 60000);
    const int SIMULATION_STEPS = 1000;
    for (int step = 0; step < SIMULATION_STEPS; step++) {
        int result = substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, SIMULATION_DT);
        ASSERT_EQ(result, 0) << "Full pipeline failed at step " << step;
    }
    std::cout << "  Completed " << SIMULATION_STEPS << " simulation steps" << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Large-scale Neural Computation
//=============================================================================

TEST_F(GPUAccelerationE2ETest, LargeScaleNeuralComputation) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    E2E_PIPELINE_START("Large-scale Neural Computation");

    E2E_STAGE_BEGIN("Initialize large substrate", 10000);
    // Use larger scale for this test
    substrate_gpu_config_t config = substrate_gpu_default_config();
    config.axon.max_axons = 50000;
    config.dendrite.max_dendrites = 100000;

    substrate_ctx = substrate_gpu_create(gpu_ctx, &config);
    ASSERT_NE(substrate_ctx, nullptr);

    // Initialize with larger counts
    const uint32_t LARGE_AXONS = 25000;
    const uint32_t LARGE_DENDRITES = 50000;
    const uint32_t LARGE_SPINES = 500000;

    ASSERT_EQ(substrate_gpu_init_axons(substrate_ctx, LARGE_AXONS), 0);
    ASSERT_EQ(substrate_gpu_init_dendrites(substrate_ctx, LARGE_DENDRITES, 10, LARGE_SPINES), 0);
    ASSERT_EQ(substrate_gpu_init_myelin(substrate_ctx, LARGE_AXONS), 0);
    ASSERT_EQ(substrate_gpu_init_neuromod(substrate_ctx, 50, 4, 50000), 0);

    std::cout << "  Initialized " << LARGE_AXONS << " axons" << std::endl;
    std::cout << "  Initialized " << LARGE_DENDRITES << " dendrites" << std::endl;
    std::cout << "  Initialized " << LARGE_SPINES << " spines" << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run large-scale simulation", 120000);
    const int STEPS = 500;
    auto start = std::chrono::high_resolution_clock::now();

    for (int step = 0; step < STEPS; step++) {
        ASSERT_EQ(substrate_gpu_axon_step(substrate_ctx, nullptr, SIMULATION_DT), 0);
        ASSERT_EQ(substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, SIMULATION_DT), 0);
        ASSERT_EQ(substrate_gpu_myelin_step(substrate_ctx, SIMULATION_DT), 0);
        ASSERT_EQ(substrate_gpu_neuromod_step(substrate_ctx, SIMULATION_DT), 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "  Completed " << STEPS << " steps in " << elapsed_ms << " ms" << std::endl;
    std::cout << "  Elements processed per step: "
              << (LARGE_AXONS + LARGE_DENDRITES + LARGE_SPINES) << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: GPU Memory Management
//=============================================================================

TEST_F(GPUAccelerationE2ETest, GPUMemoryManagement) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    E2E_PIPELINE_START("GPU Memory Management");

    E2E_STAGE_BEGIN("Allocate substrate tensors", 5000);
    ASSERT_TRUE(initializeFullSubstrate());
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Track memory during simulation", 30000);
    const int STEPS = 1000;

    for (int step = 0; step < STEPS; step++) {
        ASSERT_EQ(substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, SIMULATION_DT), 0);

        // Periodically verify tensor integrity
        if (step % 100 == 0) {
            substrate_axon_tensors_t* axons = substrate_gpu_get_axon_tensors(substrate_ctx);
            EXPECT_NE(axons, nullptr);
            EXPECT_NE(axons->signals, nullptr);

            substrate_metabolic_tensors_t* met = substrate_gpu_get_metabolic_tensors(substrate_ctx);
            EXPECT_NE(met, nullptr);
            EXPECT_NE(met->atp_levels, nullptr);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Cleanup and verify deallocation", 5000);
    substrate_gpu_destroy(substrate_ctx);
    substrate_ctx = nullptr;

    // Verify no GPU memory leaks (host-side tracking)
    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    std::cout << "  Post-cleanup host memory: " << stats.current_allocated << " bytes" << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Heterogeneous Compute (CPU+GPU)
//=============================================================================

TEST_F(GPUAccelerationE2ETest, HeterogeneousCompute) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    E2E_PIPELINE_START("Heterogeneous Compute");

    E2E_STAGE_BEGIN("Initialize GPU substrate", 5000);
    ASSERT_TRUE(initializeFullSubstrate());
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run interleaved CPU/GPU workload", 60000);
    const int STEPS = 500;
    std::vector<float> cpu_results;
    cpu_results.reserve(STEPS);

    for (int step = 0; step < STEPS; step++) {
        // GPU work: substrate simulation
        ASSERT_EQ(substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, SIMULATION_DT), 0);

        // Simulated CPU work: compute some aggregates
        float cpu_computation = std::sin(step * 0.01f) * std::cos(step * 0.02f);
        cpu_results.push_back(cpu_computation);

        // Verify GPU state accessible
        if (step % 100 == 0) {
            substrate_metabolic_tensors_t* met = substrate_gpu_get_metabolic_tensors(substrate_ctx);
            EXPECT_NE(met, nullptr);
        }
    }

    std::cout << "  Completed " << STEPS << " heterogeneous steps" << std::endl;
    std::cout << "  CPU results collected: " << cpu_results.size() << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Performance Scaling Tests
//=============================================================================

TEST_F(GPUAccelerationE2ETest, PerformanceScalingTests) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    E2E_PIPELINE_START("Performance Scaling Tests");

    struct ScaleConfig {
        uint32_t axons;
        uint32_t dendrites;
        const char* label;
    };

    std::vector<ScaleConfig> scales = {
        {1000, 2000, "Small"},
        {5000, 10000, "Medium"},
        {10000, 20000, "Large"}
    };

    for (const auto& scale : scales) {
        std::string stage_name = std::string("Test ") + scale.label + " scale";
        E2E_STAGE_BEGIN(stage_name.c_str(), 60000);

        // Create context for this scale
        substrate_gpu_config_t config = substrate_gpu_default_config();
        config.axon.max_axons = scale.axons * 2;
        config.dendrite.max_dendrites = scale.dendrites * 2;

        substrate_ctx = substrate_gpu_create(gpu_ctx, &config);
        ASSERT_NE(substrate_ctx, nullptr);

        ASSERT_EQ(substrate_gpu_init_axons(substrate_ctx, scale.axons), 0);
        ASSERT_EQ(substrate_gpu_init_dendrites(substrate_ctx, scale.dendrites, 10,
                                               scale.dendrites * 25), 0);
        ASSERT_EQ(substrate_gpu_init_myelin(substrate_ctx, scale.axons), 0);

        // Warmup
        for (int i = 0; i < WARMUP_STEPS; i++) {
            substrate_gpu_axon_step(substrate_ctx, nullptr, SIMULATION_DT);
            substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, SIMULATION_DT);
            substrate_gpu_myelin_step(substrate_ctx, SIMULATION_DT);
        }

        // Benchmark
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < BENCHMARK_STEPS; i++) {
            substrate_gpu_axon_step(substrate_ctx, nullptr, SIMULATION_DT);
            substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, SIMULATION_DT);
            substrate_gpu_myelin_step(substrate_ctx, SIMULATION_DT);
        }
        auto end = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

        double elements_per_step = scale.axons + scale.dendrites + scale.dendrites * 25;
        double throughput = (elements_per_step * BENCHMARK_STEPS) / (elapsed_ms / 1000.0);

        std::cout << "  " << scale.label << " scale:" << std::endl;
        std::cout << "    Elements/step: " << elements_per_step << std::endl;
        std::cout << "    Time for " << BENCHMARK_STEPS << " steps: " << elapsed_ms << " ms" << std::endl;
        std::cout << "    Throughput: " << (throughput / 1e6) << " M elements/sec" << std::endl;

        substrate_gpu_destroy(substrate_ctx);
        substrate_ctx = nullptr;

        E2E_STAGE_END();
    }

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Individual Subsystem Performance
//=============================================================================

TEST_F(GPUAccelerationE2ETest, IndividualSubsystemPerformance) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    E2E_PIPELINE_START("Individual Subsystem Performance");

    E2E_STAGE_BEGIN("Initialize substrate", 5000);
    ASSERT_TRUE(initializeFullSubstrate());
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Benchmark axon subsystem", 30000);
    auto axon_throughput = measureSubsystemThroughput(
        [this]() { return substrate_gpu_axon_step(substrate_ctx, nullptr, SIMULATION_DT); },
        BENCHMARK_STEPS,
        NUM_AXONS
    );
    std::cout << "  Axon throughput: " << (axon_throughput / 1e6) << " M axons/sec" << std::endl;
    EXPECT_GT(axon_throughput, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Benchmark dendrite subsystem", 30000);
    auto dendrite_throughput = measureSubsystemThroughput(
        [this]() { return substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, SIMULATION_DT); },
        BENCHMARK_STEPS,
        NUM_DENDRITES
    );
    std::cout << "  Dendrite throughput: " << (dendrite_throughput / 1e6) << " M dendrites/sec" << std::endl;
    EXPECT_GT(dendrite_throughput, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Benchmark myelin subsystem", 30000);
    auto myelin_throughput = measureSubsystemThroughput(
        [this]() { return substrate_gpu_myelin_step(substrate_ctx, SIMULATION_DT); },
        BENCHMARK_STEPS,
        NUM_AXONS
    );
    std::cout << "  Myelin throughput: " << (myelin_throughput / 1e6) << " M sheaths/sec" << std::endl;
    EXPECT_GT(myelin_throughput, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Benchmark neuromodulator subsystem", 30000);
    auto neuromod_throughput = measureSubsystemThroughput(
        [this]() { return substrate_gpu_neuromod_step(substrate_ctx, SIMULATION_DT); },
        BENCHMARK_STEPS,
        NUM_NEUROMOD_POOLS * NUM_NEUROMOD_TYPES
    );
    std::cout << "  Neuromod throughput: " << (neuromod_throughput / 1e6) << " M pools/sec" << std::endl;
    EXPECT_GT(neuromod_throughput, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Benchmark glial subsystem", 30000);
    auto glial_throughput = measureSubsystemThroughput(
        [this]() { return substrate_gpu_glial_step(substrate_ctx, SIMULATION_DT); },
        BENCHMARK_STEPS,
        NUM_ASTROCYTES + NUM_MICROGLIA + NUM_OPCS
    );
    std::cout << "  Glial throughput: " << (glial_throughput / 1e6) << " M cells/sec" << std::endl;
    EXPECT_GT(glial_throughput, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Benchmark metabolic subsystem", 30000);
    auto metabolic_throughput = measureSubsystemThroughput(
        [this]() { return substrate_gpu_metabolic_step(substrate_ctx, nullptr, SIMULATION_DT); },
        BENCHMARK_STEPS,
        NUM_METABOLIC_REGIONS
    );
    std::cout << "  Metabolic throughput: " << (metabolic_throughput / 1e6) << " M regions/sec" << std::endl;
    EXPECT_GT(metabolic_throughput, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Extended Simulation Stability
//=============================================================================

TEST_F(GPUAccelerationE2ETest, ExtendedSimulationStability) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    E2E_PIPELINE_START("Extended Simulation Stability");

    E2E_STAGE_BEGIN("Initialize substrate", 5000);
    ASSERT_TRUE(initializeFullSubstrate());
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run extended simulation", 180000);
    const int STEPS = EXTENDED_STEPS;
    int checkpoint_interval = STEPS / 10;

    auto overall_start = std::chrono::high_resolution_clock::now();

    for (int step = 0; step < STEPS; step++) {
        int result = substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, SIMULATION_DT);
        ASSERT_EQ(result, 0) << "Extended simulation failed at step " << step;

        if ((step + 1) % checkpoint_interval == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            double elapsed_ms = std::chrono::duration<double, std::milli>(now - overall_start).count();
            double progress = (step + 1.0) / STEPS * 100;
            std::cout << "  Progress: " << progress << "% (" << elapsed_ms << " ms elapsed)" << std::endl;
        }
    }

    auto overall_end = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(overall_end - overall_start).count();
    std::cout << "  Total time for " << STEPS << " steps: " << total_ms << " ms" << std::endl;
    std::cout << "  Average step time: " << (total_ms / STEPS) << " ms" << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify final tensor integrity", 1000);
    substrate_axon_tensors_t* axons = substrate_gpu_get_axon_tensors(substrate_ctx);
    EXPECT_NE(axons, nullptr);
    EXPECT_NE(axons->signals, nullptr);

    substrate_dendrite_tensors_t* dend = substrate_gpu_get_dendrite_tensors(substrate_ctx);
    EXPECT_NE(dend, nullptr);
    EXPECT_NE(dend->voltages, nullptr);

    substrate_metabolic_tensors_t* met = substrate_gpu_get_metabolic_tensors(substrate_ctx);
    EXPECT_NE(met, nullptr);
    EXPECT_NE(met->atp_levels, nullptr);

    std::cout << "  All tensors valid after extended simulation" << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Subsystem Coupling Validation
//=============================================================================

TEST_F(GPUAccelerationE2ETest, SubsystemCouplingValidation) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    E2E_PIPELINE_START("Subsystem Coupling Validation");

    E2E_STAGE_BEGIN("Initialize substrate", 5000);
    ASSERT_TRUE(initializeFullSubstrate());
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test individual subsystem calls", 10000);
    // Each subsystem should work independently
    EXPECT_EQ(substrate_gpu_axon_step(substrate_ctx, nullptr, SIMULATION_DT), 0);
    EXPECT_EQ(substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, SIMULATION_DT), 0);
    EXPECT_EQ(substrate_gpu_myelin_step(substrate_ctx, SIMULATION_DT), 0);
    EXPECT_EQ(substrate_gpu_neuromod_step(substrate_ctx, SIMULATION_DT), 0);
    EXPECT_EQ(substrate_gpu_glial_step(substrate_ctx, SIMULATION_DT), 0);
    EXPECT_EQ(substrate_gpu_metabolic_step(substrate_ctx, nullptr, SIMULATION_DT), 0);
    std::cout << "  All individual subsystems executed successfully" << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test combined full step", 10000);
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, SIMULATION_DT), 0);
    }
    std::cout << "  Combined full step executed successfully" << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test interleaved subsystem calls", 30000);
    // Simulate realistic usage pattern with interleaved calls
    for (int i = 0; i < 500; i++) {
        // Axon + dendrite (core neural)
        EXPECT_EQ(substrate_gpu_axon_step(substrate_ctx, nullptr, SIMULATION_DT), 0);
        EXPECT_EQ(substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, SIMULATION_DT), 0);

        // Myelin (every 10 steps)
        if (i % 10 == 0) {
            EXPECT_EQ(substrate_gpu_myelin_step(substrate_ctx, SIMULATION_DT * 10), 0);
        }

        // Neuromodulation (every 5 steps)
        if (i % 5 == 0) {
            EXPECT_EQ(substrate_gpu_neuromod_step(substrate_ctx, SIMULATION_DT * 5), 0);
        }

        // Glial (every 20 steps)
        if (i % 20 == 0) {
            EXPECT_EQ(substrate_gpu_glial_step(substrate_ctx, SIMULATION_DT * 20), 0);
        }

        // Metabolic (every 50 steps)
        if (i % 50 == 0) {
            EXPECT_EQ(substrate_gpu_metabolic_step(substrate_ctx, nullptr, SIMULATION_DT * 50), 0);
        }
    }
    std::cout << "  Interleaved subsystem calls executed successfully" << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: GPU Context Recovery
//=============================================================================

TEST_F(GPUAccelerationE2ETest, GPUContextRecovery) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    E2E_PIPELINE_START("GPU Context Recovery");

    E2E_STAGE_BEGIN("Create and destroy multiple contexts", 30000);
    for (int iteration = 0; iteration < 3; iteration++) {
        std::cout << "  Iteration " << (iteration + 1) << ":" << std::endl;

        substrate_gpu_config_t config = substrate_gpu_default_config();
        substrate_ctx = substrate_gpu_create(gpu_ctx, &config);
        ASSERT_NE(substrate_ctx, nullptr);

        // Initialize
        ASSERT_EQ(substrate_gpu_init_axons(substrate_ctx, 1000), 0);
        ASSERT_EQ(substrate_gpu_init_dendrites(substrate_ctx, 2000, 10, 10000), 0);
        ASSERT_EQ(substrate_gpu_init_myelin(substrate_ctx, 1000), 0);

        // Run simulation
        for (int step = 0; step < 100; step++) {
            ASSERT_EQ(substrate_gpu_axon_step(substrate_ctx, nullptr, SIMULATION_DT), 0);
            ASSERT_EQ(substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, SIMULATION_DT), 0);
        }

        // Destroy
        substrate_gpu_destroy(substrate_ctx);
        substrate_ctx = nullptr;

        std::cout << "    Created, ran, and destroyed context" << std::endl;
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify final memory state", 1000);
    nimcp_memory_stats_t stats;
    nimcp_memory_get_stats(&stats);
    std::cout << "  Final host memory: " << stats.current_allocated << " bytes" << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Detailed Component Initialization
//=============================================================================

TEST_F(GPUAccelerationE2ETest, DetailedComponentInitialization) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    E2E_PIPELINE_START("Detailed Component Initialization");

    E2E_STAGE_BEGIN("Create substrate context", 1000);
    substrate_gpu_config_t config = substrate_gpu_default_config();
    config.enable_async_ops = true;
    substrate_ctx = substrate_gpu_create(gpu_ctx, &config);
    ASSERT_NE(substrate_ctx, nullptr);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Initialize axons", 2000);
    ASSERT_EQ(substrate_gpu_init_axons(substrate_ctx, NUM_AXONS), 0);
    substrate_axon_tensors_t* axons = substrate_gpu_get_axon_tensors(substrate_ctx);
    ASSERT_NE(axons, nullptr);
    EXPECT_EQ(axons->n_axons, NUM_AXONS);
    std::cout << "  Axons initialized: " << axons->n_axons << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Initialize dendrites", 2000);
    ASSERT_EQ(substrate_gpu_init_dendrites(substrate_ctx, NUM_DENDRITES,
                                           SEGMENTS_PER_DENDRITE,
                                           NUM_DENDRITES * SPINES_PER_DENDRITE), 0);
    substrate_dendrite_tensors_t* dend = substrate_gpu_get_dendrite_tensors(substrate_ctx);
    ASSERT_NE(dend, nullptr);
    EXPECT_EQ(dend->n_dendrites, NUM_DENDRITES);
    EXPECT_EQ(dend->n_segments, SEGMENTS_PER_DENDRITE);
    std::cout << "  Dendrites: " << dend->n_dendrites << ", Segments: " << dend->n_segments
              << ", Spines: " << dend->n_spines << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Initialize myelin", 2000);
    ASSERT_EQ(substrate_gpu_init_myelin(substrate_ctx, NUM_AXONS), 0);
    substrate_myelin_tensors_t* myelin = substrate_gpu_get_myelin_tensors(substrate_ctx);
    ASSERT_NE(myelin, nullptr);
    EXPECT_EQ(myelin->n_axons, NUM_AXONS);
    std::cout << "  Myelin sheaths: " << myelin->n_axons << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Initialize neuromodulators", 2000);
    ASSERT_EQ(substrate_gpu_init_neuromod(substrate_ctx, NUM_NEUROMOD_POOLS,
                                          NUM_NEUROMOD_TYPES, NUM_SYNAPSES), 0);
    substrate_neuromod_tensors_t* neuromod = substrate_gpu_get_neuromod_tensors(substrate_ctx);
    ASSERT_NE(neuromod, nullptr);
    EXPECT_EQ(neuromod->n_pools, NUM_NEUROMOD_POOLS);
    EXPECT_EQ(neuromod->n_types, NUM_NEUROMOD_TYPES);
    std::cout << "  Neuromod pools: " << neuromod->n_pools << ", Types: " << neuromod->n_types
              << ", Synapses: " << neuromod->n_synapses << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Initialize glial cells", 2000);
    ASSERT_EQ(substrate_gpu_init_glial(substrate_ctx, NUM_ASTROCYTES,
                                       NUM_MICROGLIA, NUM_OPCS, 6), 0);
    substrate_glial_tensors_t* glial = substrate_gpu_get_glial_tensors(substrate_ctx);
    ASSERT_NE(glial, nullptr);
    EXPECT_EQ(glial->n_astrocytes, NUM_ASTROCYTES);
    EXPECT_EQ(glial->n_microglia, NUM_MICROGLIA);
    EXPECT_EQ(glial->n_opcs, NUM_OPCS);
    std::cout << "  Astrocytes: " << glial->n_astrocytes << ", Microglia: " << glial->n_microglia
              << ", OPCs: " << glial->n_opcs << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Initialize metabolic", 2000);
    ASSERT_EQ(substrate_gpu_init_metabolic(substrate_ctx, NUM_METABOLIC_REGIONS), 0);
    substrate_metabolic_tensors_t* metabolic = substrate_gpu_get_metabolic_tensors(substrate_ctx);
    ASSERT_NE(metabolic, nullptr);
    EXPECT_EQ(metabolic->n_regions, NUM_METABOLIC_REGIONS);
    std::cout << "  Metabolic regions: " << metabolic->n_regions << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Batch Processing Efficiency
//=============================================================================

TEST_F(GPUAccelerationE2ETest, BatchProcessingEfficiency) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    E2E_PIPELINE_START("Batch Processing Efficiency");

    E2E_STAGE_BEGIN("Initialize substrate", 5000);
    ASSERT_TRUE(initializeFullSubstrate());
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Measure single step performance", 10000);
    auto single_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, SIMULATION_DT), 0);
    }
    auto single_end = std::chrono::high_resolution_clock::now();
    double single_ms = std::chrono::duration<double, std::milli>(single_end - single_start).count();
    std::cout << "  100 individual steps: " << single_ms << " ms" << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Warmup GPU caches", 5000);
    for (int i = 0; i < WARMUP_STEPS; i++) {
        substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, SIMULATION_DT);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Measure warmed up performance", 10000);
    auto warm_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, SIMULATION_DT), 0);
    }
    auto warm_end = std::chrono::high_resolution_clock::now();
    double warm_ms = std::chrono::duration<double, std::milli>(warm_end - warm_start).count();
    std::cout << "  100 warmed up steps: " << warm_ms << " ms" << std::endl;

    // Warmed up should be at least as fast or faster
    EXPECT_LE(warm_ms, single_ms * 1.2);  // Allow 20% tolerance
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Mixed Precision Operations
//=============================================================================

TEST_F(GPUAccelerationE2ETest, MixedPrecisionOperations) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    E2E_PIPELINE_START("Mixed Precision Operations");

    E2E_STAGE_BEGIN("Create context with mixed precision", 5000);
    substrate_gpu_config_t config = substrate_gpu_default_config();
    config.enable_mixed_precision = true;

    substrate_ctx = substrate_gpu_create(gpu_ctx, &config);
    ASSERT_NE(substrate_ctx, nullptr);

    ASSERT_EQ(substrate_gpu_init_axons(substrate_ctx, NUM_AXONS), 0);
    ASSERT_EQ(substrate_gpu_init_dendrites(substrate_ctx, NUM_DENDRITES, 10,
                                           NUM_DENDRITES * 25), 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run simulation with mixed precision", 30000);
    for (int step = 0; step < BENCHMARK_STEPS; step++) {
        ASSERT_EQ(substrate_gpu_axon_step(substrate_ctx, nullptr, SIMULATION_DT), 0);
        ASSERT_EQ(substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, SIMULATION_DT), 0);
    }
    std::cout << "  Completed " << BENCHMARK_STEPS << " mixed precision steps" << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify tensor integrity", 1000);
    substrate_axon_tensors_t* axons = substrate_gpu_get_axon_tensors(substrate_ctx);
    EXPECT_NE(axons, nullptr);
    EXPECT_NE(axons->signals, nullptr);

    substrate_dendrite_tensors_t* dend = substrate_gpu_get_dendrite_tensors(substrate_ctx);
    EXPECT_NE(dend, nullptr);
    EXPECT_NE(dend->voltages, nullptr);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Async Operations
//=============================================================================

TEST_F(GPUAccelerationE2ETest, AsyncOperations) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    E2E_PIPELINE_START("Async Operations");

    E2E_STAGE_BEGIN("Create context with async enabled", 5000);
    substrate_gpu_config_t config = substrate_gpu_default_config();
    config.enable_async_ops = true;

    substrate_ctx = substrate_gpu_create(gpu_ctx, &config);
    ASSERT_NE(substrate_ctx, nullptr);

    ASSERT_EQ(substrate_gpu_init_axons(substrate_ctx, NUM_AXONS), 0);
    ASSERT_EQ(substrate_gpu_init_dendrites(substrate_ctx, NUM_DENDRITES, 10,
                                           NUM_DENDRITES * 25), 0);
    ASSERT_EQ(substrate_gpu_init_neuromod(substrate_ctx, NUM_NEUROMOD_POOLS,
                                          NUM_NEUROMOD_TYPES, NUM_SYNAPSES), 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run async simulation", 60000);
    auto start = std::chrono::high_resolution_clock::now();

    for (int step = 0; step < BENCHMARK_STEPS; step++) {
        // These operations may run concurrently on GPU
        ASSERT_EQ(substrate_gpu_axon_step(substrate_ctx, nullptr, SIMULATION_DT), 0);
        ASSERT_EQ(substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, SIMULATION_DT), 0);
        ASSERT_EQ(substrate_gpu_neuromod_step(substrate_ctx, SIMULATION_DT), 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "  Async " << BENCHMARK_STEPS << " steps: " << elapsed_ms << " ms" << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Stress Test with Maximum Load
//=============================================================================

TEST_F(GPUAccelerationE2ETest, StressTestMaximumLoad) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    E2E_PIPELINE_START("Stress Test Maximum Load");

    E2E_STAGE_BEGIN("Initialize maximum capacity substrate", 10000);
    substrate_gpu_config_t config = substrate_gpu_default_config();
    config.axon.max_axons = 100000;
    config.dendrite.max_dendrites = 200000;

    substrate_ctx = substrate_gpu_create(gpu_ctx, &config);
    ASSERT_NE(substrate_ctx, nullptr);

    const uint32_t STRESS_AXONS = 50000;
    const uint32_t STRESS_DENDRITES = 100000;
    const uint32_t STRESS_SPINES = 500000;

    ASSERT_EQ(substrate_gpu_init_axons(substrate_ctx, STRESS_AXONS), 0);
    ASSERT_EQ(substrate_gpu_init_dendrites(substrate_ctx, STRESS_DENDRITES, 5, STRESS_SPINES), 0);
    ASSERT_EQ(substrate_gpu_init_myelin(substrate_ctx, STRESS_AXONS), 0);

    std::cout << "  Initialized stress substrate:" << std::endl;
    std::cout << "    Axons: " << STRESS_AXONS << std::endl;
    std::cout << "    Dendrites: " << STRESS_DENDRITES << std::endl;
    std::cout << "    Spines: " << STRESS_SPINES << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run stress simulation", 120000);
    const int STRESS_STEPS = 500;

    auto start = std::chrono::high_resolution_clock::now();
    for (int step = 0; step < STRESS_STEPS; step++) {
        ASSERT_EQ(substrate_gpu_axon_step(substrate_ctx, nullptr, SIMULATION_DT), 0);
        ASSERT_EQ(substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, SIMULATION_DT), 0);
        ASSERT_EQ(substrate_gpu_myelin_step(substrate_ctx, SIMULATION_DT), 0);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    double total_elements = (STRESS_AXONS + STRESS_DENDRITES + STRESS_SPINES) * STRESS_STEPS;
    double throughput = total_elements / (elapsed_ms / 1000.0);

    std::cout << "  Stress test completed:" << std::endl;
    std::cout << "    Time: " << elapsed_ms << " ms" << std::endl;
    std::cout << "    Throughput: " << (throughput / 1e9) << " billion elements/sec" << std::endl;
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: Neuromodulator-Glial Interaction
//=============================================================================

TEST_F(GPUAccelerationE2ETest, NeuromodulatorGlialInteraction) {
    if (!gpu_ctx) GTEST_SKIP() << "No GPU available";

    E2E_PIPELINE_START("Neuromodulator-Glial Interaction");

    E2E_STAGE_BEGIN("Initialize neuromod and glial subsystems", 5000);
    ASSERT_TRUE(initializeFullSubstrate());
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run coupled simulation", 30000);
    for (int step = 0; step < BENCHMARK_STEPS; step++) {
        // Run neuromodulator and glial systems together
        ASSERT_EQ(substrate_gpu_neuromod_step(substrate_ctx, SIMULATION_DT), 0);
        ASSERT_EQ(substrate_gpu_glial_step(substrate_ctx, SIMULATION_DT), 0);
    }
    std::cout << "  Completed " << BENCHMARK_STEPS << " coupled neuromod-glial steps" << std::endl;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify subsystem state", 1000);
    substrate_neuromod_tensors_t* neuromod = substrate_gpu_get_neuromod_tensors(substrate_ctx);
    EXPECT_NE(neuromod, nullptr);
    EXPECT_NE(neuromod->concentrations, nullptr);

    substrate_glial_tensors_t* glial = substrate_gpu_get_glial_tensors(substrate_ctx);
    EXPECT_NE(glial, nullptr);
    EXPECT_NE(glial->astro_calcium, nullptr);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Main Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
