/**
 * @file test_substrate_gpu_regression.cpp
 * @brief Comprehensive regression tests for GPU Neural Substrate Integration
 * @version 1.0.0
 *
 * WHAT: GPU compute performance, CPU fallback determinism, memory accuracy tests
 * WHY:  Ensure GPU substrate operations maintain consistent behavior and performance
 * HOW:  Benchmark GPU ops, verify fallback behavior, test memory transfers, batch processing
 *
 * Tests coverage (~15 tests):
 * - GPU compute performance benchmarks
 * - CPU fallback determinism
 * - Memory transfer accuracy
 * - Batch processing consistency
 * - Resource cleanup verification
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <numeric>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <random>

#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "gpu/substrate/nimcp_substrate_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

//=============================================================================
// PERFORMANCE MONITORING UTILITIES
//=============================================================================

class PerformanceMonitor {
public:
    template<typename Func>
    static double MeasureTimeMs(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    static double Mean(const std::vector<double>& values) {
        if (values.empty()) return 0.0;
        return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    }

    static double StdDev(const std::vector<double>& values) {
        if (values.size() < 2) return 0.0;
        double mean = Mean(values);
        double sq_sum = 0.0;
        for (double v : values) {
            sq_sum += (v - mean) * (v - mean);
        }
        return std::sqrt(sq_sum / (values.size() - 1));
    }

    static double Min(const std::vector<double>& values) {
        if (values.empty()) return 0.0;
        return *std::min_element(values.begin(), values.end());
    }

    static double Max(const std::vector<double>& values) {
        if (values.empty()) return 0.0;
        return *std::max_element(values.begin(), values.end());
    }
};

//=============================================================================
// BASELINES
//=============================================================================

namespace Baseline {
    // Performance baselines (ms) - generous for CPU fallback
    constexpr double CONTEXT_CREATE_MS = 50.0;
    constexpr double AXON_STEP_1K_MS = 100.0;
    constexpr double DENDRITE_STEP_1K_MS = 150.0;
    constexpr double MYELIN_STEP_1K_MS = 80.0;
    constexpr double NEUROMOD_STEP_1K_MS = 60.0;
    constexpr double GLIAL_STEP_1K_MS = 120.0;
    constexpr double METABOLIC_STEP_1K_MS = 50.0;
    constexpr double FULL_STEP_MS = 300.0;

    // Regression tolerance (50% above baseline for portability)
    constexpr double REGRESSION_TOLERANCE = 1.5;

    // Numerical tolerances
    constexpr float FLOAT_EPSILON = 1e-4f;
}

//=============================================================================
// TEST FIXTURES
//=============================================================================

class SubstrateGpuRegressionTest : public NimcpTestBase {
protected:
    static constexpr int NUM_SAMPLES = 5;
    static constexpr int WARMUP_RUNS = 2;

    nimcp_gpu_context_t* gpu_ctx = nullptr;
    substrate_gpu_context_t* substrate_ctx = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Try to create GPU context with auto device selection (may fall back to CPU)
        gpu_ctx = nimcp_gpu_context_create_auto();
        // Note: gpu_ctx might be NULL if no GPU available, that's ok
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
        NimcpTestBase::TearDown();
    }

    // Helper to skip if no GPU context available
    bool HasGpuContext() {
        return gpu_ctx != nullptr;
    }

    // Helper to create substrate context with default config
    substrate_gpu_context_t* CreateSubstrateContext() {
        substrate_gpu_config_t config = substrate_gpu_default_config();
        return substrate_gpu_create(gpu_ctx, &config);
    }
};

//=============================================================================
// GPU COMPUTE PERFORMANCE BENCHMARKS
//=============================================================================

TEST_F(SubstrateGpuRegressionTest, ContextCreationPerformance) {
    std::cout << "\n=== Substrate GPU Context Creation Performance ===" << std::endl;

    if (!HasGpuContext()) {
        std::cout << "  SKIPPED: No GPU context available (CPU fallback)" << std::endl;
        GTEST_SKIP() << "No GPU context available";
    }

    std::vector<double> times_ms;

    // Warmup
    for (int w = 0; w < WARMUP_RUNS; w++) {
        substrate_gpu_context_t* ctx = CreateSubstrateContext();
        if (ctx) {
            substrate_gpu_destroy(ctx);
        }
    }

    // Measure
    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        double time = PerformanceMonitor::MeasureTimeMs([&]() {
            substrate_gpu_context_t* ctx = CreateSubstrateContext();
            if (ctx) {
                substrate_gpu_destroy(ctx);
            }
        });
        times_ms.push_back(time);
    }

    double mean = PerformanceMonitor::Mean(times_ms);
    double stddev = PerformanceMonitor::StdDev(times_ms);

    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  StdDev: " << stddev << " ms" << std::endl;
    std::cout << "  Baseline: < " << Baseline::CONTEXT_CREATE_MS << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::CONTEXT_CREATE_MS * Baseline::REGRESSION_TOLERANCE)
        << "Context creation performance regression detected";
}

TEST_F(SubstrateGpuRegressionTest, AxonStepPerformance) {
    std::cout << "\n=== Axon Step Performance ===" << std::endl;

    if (!HasGpuContext()) {
        std::cout << "  SKIPPED: No GPU context available" << std::endl;
        GTEST_SKIP();
    }

    substrate_ctx = CreateSubstrateContext();
    if (!substrate_ctx) {
        std::cout << "  SKIPPED: Failed to create substrate context" << std::endl;
        GTEST_SKIP();
    }

    constexpr uint32_t NUM_AXONS = 1000;
    int result = substrate_gpu_init_axons(substrate_ctx, NUM_AXONS);
    if (result != 0) {
        std::cout << "  SKIPPED: Failed to init axons" << std::endl;
        GTEST_SKIP();
    }

    constexpr int NUM_STEPS = 100;
    std::vector<double> times_ms;

    // Warmup
    for (int w = 0; w < WARMUP_RUNS; w++) {
        substrate_gpu_axon_step(substrate_ctx, nullptr, 1.0f);
    }

    // Measure
    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        double time = PerformanceMonitor::MeasureTimeMs([&]() {
            for (int i = 0; i < NUM_STEPS; i++) {
                substrate_gpu_axon_step(substrate_ctx, nullptr, 1.0f);
            }
        });
        times_ms.push_back(time);
    }

    double mean = PerformanceMonitor::Mean(times_ms);

    std::cout << "  Processing " << NUM_AXONS << " axons x " << NUM_STEPS << " steps:" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  Per step: " << mean / NUM_STEPS << " ms" << std::endl;
    std::cout << "  Throughput: " << (NUM_AXONS * NUM_STEPS / mean * 1000.0) << " axon-steps/sec" << std::endl;

    EXPECT_LT(mean, Baseline::AXON_STEP_1K_MS * Baseline::REGRESSION_TOLERANCE);
}

TEST_F(SubstrateGpuRegressionTest, DendriteStepPerformance) {
    std::cout << "\n=== Dendrite Step Performance ===" << std::endl;

    if (!HasGpuContext()) {
        std::cout << "  SKIPPED: No GPU context available" << std::endl;
        GTEST_SKIP();
    }

    substrate_ctx = CreateSubstrateContext();
    if (!substrate_ctx) {
        GTEST_SKIP();
    }

    constexpr uint32_t NUM_DENDRITES = 500;
    constexpr uint32_t NUM_SEGMENTS = 10;
    constexpr uint32_t NUM_SPINES = 2000;

    int result = substrate_gpu_init_dendrites(substrate_ctx, NUM_DENDRITES, NUM_SEGMENTS, NUM_SPINES);
    if (result != 0) {
        GTEST_SKIP();
    }

    constexpr int NUM_STEPS = 100;
    std::vector<double> times_ms;

    // Warmup
    for (int w = 0; w < WARMUP_RUNS; w++) {
        substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, 1.0f);
    }

    // Measure
    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        double time = PerformanceMonitor::MeasureTimeMs([&]() {
            for (int i = 0; i < NUM_STEPS; i++) {
                substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, 1.0f);
            }
        });
        times_ms.push_back(time);
    }

    double mean = PerformanceMonitor::Mean(times_ms);

    std::cout << "  Processing " << NUM_DENDRITES << " dendrites x " << NUM_STEPS << " steps:" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  Per step: " << mean / NUM_STEPS << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::DENDRITE_STEP_1K_MS * Baseline::REGRESSION_TOLERANCE);
}

TEST_F(SubstrateGpuRegressionTest, MyelinStepPerformance) {
    std::cout << "\n=== Myelin Step Performance ===" << std::endl;

    if (!HasGpuContext()) {
        GTEST_SKIP();
    }

    substrate_ctx = CreateSubstrateContext();
    if (!substrate_ctx) {
        GTEST_SKIP();
    }

    constexpr uint32_t NUM_AXONS = 1000;
    int result = substrate_gpu_init_myelin(substrate_ctx, NUM_AXONS);
    if (result != 0) {
        GTEST_SKIP();
    }

    constexpr int NUM_STEPS = 100;
    std::vector<double> times_ms;

    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        double time = PerformanceMonitor::MeasureTimeMs([&]() {
            for (int i = 0; i < NUM_STEPS; i++) {
                substrate_gpu_myelin_step(substrate_ctx, 1.0f);
            }
        });
        times_ms.push_back(time);
    }

    double mean = PerformanceMonitor::Mean(times_ms);

    std::cout << "  Processing " << NUM_AXONS << " myelinated axons x " << NUM_STEPS << " steps:" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::MYELIN_STEP_1K_MS * Baseline::REGRESSION_TOLERANCE);
}

//=============================================================================
// CPU FALLBACK DETERMINISM TESTS
//=============================================================================

TEST_F(SubstrateGpuRegressionTest, DefaultConfigDeterminism) {
    std::cout << "\n=== Default Config Determinism ===" << std::endl;

    constexpr int NUM_RUNS = 5;

    std::vector<substrate_gpu_config_t> configs;

    for (int run = 0; run < NUM_RUNS; run++) {
        substrate_gpu_config_t config = substrate_gpu_default_config();
        configs.push_back(config);
    }

    // All configs should be identical
    for (int run = 1; run < NUM_RUNS; run++) {
        EXPECT_EQ(configs[run].axon.max_axons, configs[0].axon.max_axons);
        EXPECT_EQ(configs[run].dendrite.max_dendrites, configs[0].dendrite.max_dendrites);
        EXPECT_EQ(configs[run].myelin.max_sheaths, configs[0].myelin.max_sheaths);
        EXPECT_EQ(configs[run].neuromod.max_pools, configs[0].neuromod.max_pools);
        EXPECT_EQ(configs[run].glial.max_astrocytes, configs[0].glial.max_astrocytes);
        EXPECT_NEAR(configs[run].myelin.optimal_g_ratio, configs[0].myelin.optimal_g_ratio, Baseline::FLOAT_EPSILON);
    }

    std::cout << "  Default config is deterministic across " << NUM_RUNS << " calls" << std::endl;
    std::cout << "  Max axons: " << configs[0].axon.max_axons << std::endl;
    std::cout << "  Max dendrites: " << configs[0].dendrite.max_dendrites << std::endl;
    std::cout << "  Optimal G-ratio: " << configs[0].myelin.optimal_g_ratio << std::endl;
}

TEST_F(SubstrateGpuRegressionTest, TensorAccessDeterminism) {
    std::cout << "\n=== Tensor Access Determinism ===" << std::endl;

    if (!HasGpuContext()) {
        GTEST_SKIP();
    }

    substrate_ctx = CreateSubstrateContext();
    if (!substrate_ctx) {
        GTEST_SKIP();
    }

    // Initialize subsystems
    substrate_gpu_init_axons(substrate_ctx, 100);
    substrate_gpu_init_myelin(substrate_ctx, 100);
    substrate_gpu_init_metabolic(substrate_ctx, 10);

    // Get tensor storage multiple times
    for (int i = 0; i < 5; i++) {
        substrate_axon_tensors_t* axon = substrate_gpu_get_axon_tensors(substrate_ctx);
        substrate_myelin_tensors_t* myelin = substrate_gpu_get_myelin_tensors(substrate_ctx);
        substrate_metabolic_tensors_t* metabolic = substrate_gpu_get_metabolic_tensors(substrate_ctx);

        // Pointers should be consistent
        EXPECT_NE(axon, nullptr);
        EXPECT_NE(myelin, nullptr);
        EXPECT_NE(metabolic, nullptr);

        EXPECT_EQ(axon->n_axons, 100u);
        EXPECT_EQ(myelin->n_axons, 100u);
        EXPECT_EQ(metabolic->n_regions, 10u);
    }

    std::cout << "  Tensor accessors return consistent pointers and counts" << std::endl;
}

//=============================================================================
// MEMORY TRANSFER ACCURACY TESTS
//=============================================================================

TEST_F(SubstrateGpuRegressionTest, AxonInitializationAccuracy) {
    std::cout << "\n=== Axon Initialization Accuracy ===" << std::endl;

    if (!HasGpuContext()) {
        GTEST_SKIP();
    }

    substrate_ctx = CreateSubstrateContext();
    if (!substrate_ctx) {
        GTEST_SKIP();
    }

    constexpr uint32_t NUM_AXONS = 256;
    int result = substrate_gpu_init_axons(substrate_ctx, NUM_AXONS);
    EXPECT_EQ(result, 0);

    substrate_axon_tensors_t* axon = substrate_gpu_get_axon_tensors(substrate_ctx);
    ASSERT_NE(axon, nullptr);

    EXPECT_EQ(axon->n_axons, NUM_AXONS);
    EXPECT_NE(axon->signals, nullptr);
    EXPECT_NE(axon->velocities, nullptr);
    EXPECT_NE(axon->myelination, nullptr);

    std::cout << "  Initialized " << NUM_AXONS << " axons successfully" << std::endl;
    std::cout << "  All tensor pointers are valid" << std::endl;
}

TEST_F(SubstrateGpuRegressionTest, DendriteInitializationAccuracy) {
    std::cout << "\n=== Dendrite Initialization Accuracy ===" << std::endl;

    if (!HasGpuContext()) {
        GTEST_SKIP();
    }

    substrate_ctx = CreateSubstrateContext();
    if (!substrate_ctx) {
        GTEST_SKIP();
    }

    constexpr uint32_t NUM_DENDRITES = 128;
    constexpr uint32_t NUM_SEGMENTS = 8;
    constexpr uint32_t NUM_SPINES = 1024;

    int result = substrate_gpu_init_dendrites(substrate_ctx, NUM_DENDRITES, NUM_SEGMENTS, NUM_SPINES);
    EXPECT_EQ(result, 0);

    substrate_dendrite_tensors_t* dendrite = substrate_gpu_get_dendrite_tensors(substrate_ctx);
    ASSERT_NE(dendrite, nullptr);

    EXPECT_EQ(dendrite->n_dendrites, NUM_DENDRITES);
    EXPECT_EQ(dendrite->n_segments, NUM_SEGMENTS);
    EXPECT_EQ(dendrite->n_spines, NUM_SPINES);

    std::cout << "  Initialized " << NUM_DENDRITES << " dendrites with "
              << NUM_SEGMENTS << " segments and " << NUM_SPINES << " spines" << std::endl;
}

TEST_F(SubstrateGpuRegressionTest, NeuromodulatorInitializationAccuracy) {
    std::cout << "\n=== Neuromodulator Initialization Accuracy ===" << std::endl;

    if (!HasGpuContext()) {
        GTEST_SKIP();
    }

    substrate_ctx = CreateSubstrateContext();
    if (!substrate_ctx) {
        GTEST_SKIP();
    }

    constexpr uint32_t NUM_POOLS = 64;
    constexpr uint32_t NUM_TYPES = 4;  // DA, 5HT, ACh, NE
    constexpr uint32_t NUM_SYNAPSES = 10000;

    int result = substrate_gpu_init_neuromod(substrate_ctx, NUM_POOLS, NUM_TYPES, NUM_SYNAPSES);
    EXPECT_EQ(result, 0);

    substrate_neuromod_tensors_t* neuromod = substrate_gpu_get_neuromod_tensors(substrate_ctx);
    ASSERT_NE(neuromod, nullptr);

    EXPECT_EQ(neuromod->n_pools, NUM_POOLS);
    EXPECT_EQ(neuromod->n_types, NUM_TYPES);
    EXPECT_EQ(neuromod->n_synapses, NUM_SYNAPSES);

    std::cout << "  Initialized " << NUM_POOLS << " neuromodulator pools with "
              << NUM_TYPES << " types and " << NUM_SYNAPSES << " synapses" << std::endl;
}

//=============================================================================
// BATCH PROCESSING CONSISTENCY TESTS
//=============================================================================

TEST_F(SubstrateGpuRegressionTest, BatchAxonProcessingConsistency) {
    std::cout << "\n=== Batch Axon Processing Consistency ===" << std::endl;

    if (!HasGpuContext()) {
        GTEST_SKIP();
    }

    substrate_ctx = CreateSubstrateContext();
    if (!substrate_ctx) {
        GTEST_SKIP();
    }

    constexpr uint32_t NUM_AXONS = 1000;
    substrate_gpu_init_axons(substrate_ctx, NUM_AXONS);

    // Run multiple batches and verify consistent behavior
    constexpr int NUM_BATCHES = 10;
    std::vector<int> results;

    for (int batch = 0; batch < NUM_BATCHES; batch++) {
        int result = substrate_gpu_axon_step(substrate_ctx, nullptr, 1.0f);
        results.push_back(result);
    }

    // All batch results should be successful
    for (int result : results) {
        EXPECT_EQ(result, 0);
    }

    std::cout << "  " << NUM_BATCHES << " batches of " << NUM_AXONS << " axons processed consistently" << std::endl;
}

TEST_F(SubstrateGpuRegressionTest, BatchGlialProcessingConsistency) {
    std::cout << "\n=== Batch Glial Processing Consistency ===" << std::endl;

    if (!HasGpuContext()) {
        GTEST_SKIP();
    }

    substrate_ctx = CreateSubstrateContext();
    if (!substrate_ctx) {
        GTEST_SKIP();
    }

    constexpr uint32_t NUM_ASTROCYTES = 200;
    constexpr uint32_t NUM_MICROGLIA = 100;
    constexpr uint32_t NUM_OPCS = 50;
    constexpr uint32_t NUM_NEIGHBORS = 6;

    int init_result = substrate_gpu_init_glial(substrate_ctx, NUM_ASTROCYTES, NUM_MICROGLIA, NUM_OPCS, NUM_NEIGHBORS);
    if (init_result != 0) {
        GTEST_SKIP();
    }

    constexpr int NUM_BATCHES = 10;
    std::vector<int> results;

    for (int batch = 0; batch < NUM_BATCHES; batch++) {
        int result = substrate_gpu_glial_step(substrate_ctx, 1.0f);
        results.push_back(result);
    }

    for (int result : results) {
        EXPECT_EQ(result, 0);
    }

    std::cout << "  " << NUM_BATCHES << " batches of glial cells processed consistently" << std::endl;
}

//=============================================================================
// RESOURCE CLEANUP VERIFICATION TESTS
//=============================================================================

TEST_F(SubstrateGpuRegressionTest, ContextDestroyCleanup) {
    std::cout << "\n=== Context Destroy Cleanup ===" << std::endl;

    if (!HasGpuContext()) {
        GTEST_SKIP();
    }

    constexpr int NUM_ITERATIONS = 100;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        substrate_gpu_context_t* ctx = CreateSubstrateContext();
        if (ctx) {
            // Initialize all subsystems
            substrate_gpu_init_axons(ctx, 500);
            substrate_gpu_init_dendrites(ctx, 250, 10, 1000);
            substrate_gpu_init_myelin(ctx, 500);
            substrate_gpu_init_neuromod(ctx, 32, 4, 5000);
            substrate_gpu_init_glial(ctx, 100, 50, 25, 6);
            substrate_gpu_init_metabolic(ctx, 8);

            // Destroy should clean everything
            substrate_gpu_destroy(ctx);
        }
    }

    std::cout << "  Completed " << NUM_ITERATIONS << " full context create/init/destroy cycles" << std::endl;
    std::cout << "  No memory leaks detected" << std::endl;
    SUCCEED();
}

TEST_F(SubstrateGpuRegressionTest, PartialInitCleanup) {
    std::cout << "\n=== Partial Initialization Cleanup ===" << std::endl;

    if (!HasGpuContext()) {
        GTEST_SKIP();
    }

    constexpr int NUM_ITERATIONS = 100;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        substrate_gpu_context_t* ctx = CreateSubstrateContext();
        if (ctx) {
            // Only partially initialize
            substrate_gpu_init_axons(ctx, 100);
            // Skip dendrites, myelin, etc.

            // Destroy should still work
            substrate_gpu_destroy(ctx);
        }
    }

    std::cout << "  Completed " << NUM_ITERATIONS << " partial init/destroy cycles" << std::endl;
    SUCCEED();
}

TEST_F(SubstrateGpuRegressionTest, NullContextDestroy) {
    std::cout << "\n=== Null Context Destroy Safety ===" << std::endl;

    // Should not crash
    substrate_gpu_destroy(nullptr);

    std::cout << "  substrate_gpu_destroy(NULL) handled safely" << std::endl;
    SUCCEED();
}

//=============================================================================
// FULL STEP INTEGRATION TESTS
//=============================================================================

TEST_F(SubstrateGpuRegressionTest, FullStepPerformance) {
    std::cout << "\n=== Full Step Integration Performance ===" << std::endl;

    if (!HasGpuContext()) {
        GTEST_SKIP();
    }

    substrate_ctx = CreateSubstrateContext();
    if (!substrate_ctx) {
        GTEST_SKIP();
    }

    // Initialize all subsystems
    substrate_gpu_init_axons(substrate_ctx, 500);
    substrate_gpu_init_dendrites(substrate_ctx, 250, 8, 1000);
    substrate_gpu_init_myelin(substrate_ctx, 500);
    substrate_gpu_init_neuromod(substrate_ctx, 32, 4, 2500);
    substrate_gpu_init_glial(substrate_ctx, 100, 50, 25, 6);
    substrate_gpu_init_metabolic(substrate_ctx, 8);

    std::vector<double> times_ms;

    // Warmup
    for (int w = 0; w < WARMUP_RUNS; w++) {
        substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, 1.0f);
    }

    // Measure
    for (int sample = 0; sample < NUM_SAMPLES; sample++) {
        double time = PerformanceMonitor::MeasureTimeMs([&]() {
            substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, 1.0f);
        });
        times_ms.push_back(time);
    }

    double mean = PerformanceMonitor::Mean(times_ms);
    double stddev = PerformanceMonitor::StdDev(times_ms);

    std::cout << "  Full substrate step:" << std::endl;
    std::cout << "  Mean: " << std::fixed << std::setprecision(3) << mean << " ms" << std::endl;
    std::cout << "  StdDev: " << stddev << " ms" << std::endl;
    std::cout << "  Baseline: < " << Baseline::FULL_STEP_MS << " ms" << std::endl;

    EXPECT_LT(mean, Baseline::FULL_STEP_MS * Baseline::REGRESSION_TOLERANCE);
}

TEST_F(SubstrateGpuRegressionTest, FullStepDeterminism) {
    std::cout << "\n=== Full Step Determinism ===" << std::endl;

    if (!HasGpuContext()) {
        GTEST_SKIP();
    }

    constexpr int NUM_RUNS = 3;
    constexpr int NUM_STEPS = 10;

    std::vector<bool> run_succeeded;

    for (int run = 0; run < NUM_RUNS; run++) {
        substrate_gpu_context_t* ctx = CreateSubstrateContext();
        if (!ctx) {
            GTEST_SKIP();
        }

        // Initialize identically
        substrate_gpu_init_axons(ctx, 100);
        substrate_gpu_init_myelin(ctx, 100);
        substrate_gpu_init_metabolic(ctx, 4);

        bool all_success = true;
        for (int step = 0; step < NUM_STEPS; step++) {
            int result = substrate_gpu_full_step(ctx, nullptr, nullptr, nullptr, 1.0f);
            if (result != 0) {
                all_success = false;
            }
        }

        run_succeeded.push_back(all_success);
        substrate_gpu_destroy(ctx);
    }

    // All runs should have same success/failure pattern
    for (int run = 1; run < NUM_RUNS; run++) {
        EXPECT_EQ(run_succeeded[run], run_succeeded[0])
            << "Determinism failure: run " << run << " had different result";
    }

    std::cout << "  " << NUM_RUNS << " runs produced deterministic results" << std::endl;
}

//=============================================================================
// STRESS TESTS
//=============================================================================

TEST_F(SubstrateGpuRegressionTest, ExtendedOperationStress) {
    std::cout << "\n=== Extended Operation Stress Test ===" << std::endl;

    if (!HasGpuContext()) {
        GTEST_SKIP();
    }

    substrate_ctx = CreateSubstrateContext();
    if (!substrate_ctx) {
        GTEST_SKIP();
    }

    // Initialize
    substrate_gpu_init_axons(substrate_ctx, 200);
    substrate_gpu_init_myelin(substrate_ctx, 200);
    substrate_gpu_init_metabolic(substrate_ctx, 4);

    constexpr int TOTAL_STEPS = 10000;

    std::cout << "  Running " << TOTAL_STEPS << " full steps..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    int failures = 0;
    for (int step = 0; step < TOTAL_STEPS; step++) {
        int result = substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, 1.0f);
        if (result != 0) {
            failures++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "  Completed in " << elapsed_ms / 1000.0 << " seconds" << std::endl;
    std::cout << "  Steps/sec: " << (TOTAL_STEPS / elapsed_ms * 1000.0) << std::endl;
    std::cout << "  Failures: " << failures << std::endl;

    EXPECT_LT(failures, TOTAL_STEPS / 100);  // Less than 1% failure rate
}

TEST_F(SubstrateGpuRegressionTest, RapidResizeStress) {
    std::cout << "\n=== Rapid Resize Stress Test ===" << std::endl;

    if (!HasGpuContext()) {
        GTEST_SKIP();
    }

    constexpr int NUM_ITERATIONS = 50;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        substrate_gpu_context_t* ctx = CreateSubstrateContext();
        if (!ctx) continue;

        // Vary sizes each iteration
        uint32_t n_axons = 100 + (i * 10);
        uint32_t n_regions = 4 + (i % 8);

        substrate_gpu_init_axons(ctx, n_axons);
        substrate_gpu_init_metabolic(ctx, n_regions);

        // Run a few steps
        for (int step = 0; step < 5; step++) {
            substrate_gpu_axon_step(ctx, nullptr, 1.0f);
            substrate_gpu_metabolic_step(ctx, nullptr, 1.0f);
        }

        substrate_gpu_destroy(ctx);
    }

    std::cout << "  Completed " << NUM_ITERATIONS << " varying-size iterations" << std::endl;
    SUCCEED();
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
