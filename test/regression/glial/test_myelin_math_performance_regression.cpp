//=============================================================================
// test_myelin_math_performance_regression.cpp - Performance Regression Tests
//=============================================================================
/**
 * @file test_myelin_math_performance_regression.cpp
 * @brief Performance regression tests for myelin math biophysics functions
 *
 * WHAT: Performance benchmarks for all myelin mathematical models
 * WHY:  Ensure biophysics calculations remain efficient at scale
 * HOW:  Measure timing and throughput for all math operations
 *
 * TEST CATEGORIES:
 * 1. G-Ratio Optimization Performance - Diameter-dependent calculations
 * 2. Cable Theory Performance - Space/time constant computations
 * 3. Saltatory Conduction Performance - Velocity calculations
 * 4. Activity-Dependent Myelination Performance - Hill kinetics
 * 5. Conduction Block Performance - Probability calculations
 * 6. Internode Optimization Performance - Length calculations
 * 7. Metabolic Efficiency Performance - ATP computations
 * 8. Stochastic Variability Performance - RNG operations
 * 9. Fast Math Performance - Inline utilities
 * 10. Full Biophysics Pipeline Performance - Complete state updates
 *
 * PERFORMANCE TARGETS:
 * - G-ratio calculation: < 0.1 us per call
 * - Cable params: < 0.5 us per call
 * - Saltatory computation: < 1 us per call
 * - Activity rate: < 0.2 us per call
 * - Block probability: < 0.1 us per call
 * - Metabolic computation: < 0.5 us per call
 * - RNG operations: < 0.05 us per call
 * - Full biophysics update: < 50 us per sheath
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-11-25
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cmath>

// Headers have their own extern "C" guards
#include "glial/myelin_sheath/nimcp_myelin_sheath.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include "utils/memory/nimcp_memory.h"
#include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MyelinMathPerformanceTest : public ::testing::Test {
protected:
    nimcp_myelin_rng_t rng;

    void SetUp() override {
        nimcp_memory_init();
        nimcp_init();
        nimcp_myelin_rng_init(&rng, 42);
    }

    void TearDown() override {
        nimcp_shutdown();
        nimcp_memory_cleanup();
    }

    // Helper to measure time in microseconds
    template<typename Func>
    double MeasureTime(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start).count();
    }

    // Helper to measure average time over multiple iterations
    template<typename Func>
    double MeasureAverageTime(Func&& func, int iterations) {
        double total = 0.0;
        for (int i = 0; i < iterations; i++) {
            total += MeasureTime(func);
        }
        return total / iterations;
    }

    // Helper to warm up cache
    template<typename Func>
    void Warmup(Func&& func, int iterations = 100) {
        for (int i = 0; i < iterations; i++) {
            func();
        }
    }
};

//=============================================================================
// 1. G-Ratio Optimization Performance
//=============================================================================

TEST_F(MyelinMathPerformanceTest, GRatio_BasicCalculation) {
    const int NUM_OPS = 100000;
    volatile float result = 0.0f;

    // Warmup
    Warmup([&]() { result = nimcp_myelin_optimal_g_ratio(2.0f); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_optimal_g_ratio(2.0f + (i % 10) * 0.1f);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  G-ratio calculation: %.4f us average (target: < 0.1 us)\n", avg_time);
    EXPECT_LT(avg_time, 0.5) << "G-ratio calculation too slow";
}

TEST_F(MyelinMathPerformanceTest, GRatio_VelocityFromGRatio) {
    const int NUM_OPS = 100000;
    volatile float result = 0.0f;
    nimcp_saltatory_result_t saltatory;

    Warmup([&]() {
        result = nimcp_myelin_saltatory_velocity(2.0f, 200.0f, 15, 0.7f, 0.9f, 0.95f, &saltatory);
    });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_saltatory_velocity(
                2.0f + (i % 10) * 0.1f, 200.0f, 15, 0.6f + (i % 5) * 0.02f,
                0.9f, 0.95f, &saltatory);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Velocity from G-ratio: %.4f us average (target: < 0.2 us)\n", avg_time);
    EXPECT_LT(avg_time, 1.0) << "Velocity calculation too slow";
}

TEST_F(MyelinMathPerformanceTest, GRatio_Throughput) {
    const int NUM_OPS = 1000000;
    volatile float sum = 0.0f;

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            sum += nimcp_myelin_optimal_g_ratio(1.0f + (float)(i % 100) * 0.1f);
        }
    });

    double ops_per_sec = NUM_OPS / (time * 1e-6);
    printf("  G-ratio throughput: %.0f ops/sec\n", ops_per_sec);
    EXPECT_GT(ops_per_sec, 1000000.0) << "G-ratio throughput too low";
}

//=============================================================================
// 2. Cable Theory Performance
//=============================================================================

TEST_F(MyelinMathPerformanceTest, CableTheory_ComputeParams) {
    const int NUM_OPS = 50000;
    nimcp_cable_params_t params;

    Warmup([&]() { nimcp_myelin_compute_cable_params(2.0f, 15, &params); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            nimcp_myelin_compute_cable_params(
                1.0f + (i % 10) * 0.2f,
                (uint32_t)(10 + (i % 20)),
                &params);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Cable params compute: %.4f us average (target: < 0.5 us)\n", avg_time);
    EXPECT_LT(avg_time, 2.0) << "Cable params too slow";
}

TEST_F(MyelinMathPerformanceTest, CableTheory_SpaceConstant) {
    const int NUM_OPS = 100000;
    volatile float result = 0.0f;

    Warmup([&]() { result = nimcp_myelin_space_constant(2.0f, 15); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_space_constant(
                1.0f + (i % 100) * 0.1f, (uint32_t)(10 + (i % 20)));
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Space constant calc: %.4f us average (target: < 0.1 us)\n", avg_time);
    EXPECT_LT(avg_time, 0.5) << "Space constant too slow";
}

TEST_F(MyelinMathPerformanceTest, CableTheory_TimeConstant) {
    const int NUM_OPS = 100000;
    volatile float result = 0.0f;

    Warmup([&]() { result = nimcp_myelin_time_constant(15); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_time_constant((uint32_t)(10 + (i % 20)));
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Time constant calc: %.4f us average (target: < 0.1 us)\n", avg_time);
    EXPECT_LT(avg_time, 0.5) << "Time constant too slow";
}

//=============================================================================
// 3. Saltatory Conduction Performance
//=============================================================================

TEST_F(MyelinMathPerformanceTest, Saltatory_FullComputation) {
    const int NUM_OPS = 50000;
    nimcp_saltatory_result_t result;
    volatile float vel = 0.0f;

    Warmup([&]() {
        vel = nimcp_myelin_saltatory_velocity(2.0f, 200.0f, 15, 0.7f, 0.9f, 0.95f, &result);
    });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            vel = nimcp_myelin_saltatory_velocity(
                1.0f + (i % 20) * 0.2f,      // axon_diameter_um
                100.0f + (i % 20) * 20.0f,   // internode_length_um
                (uint32_t)(10 + (i % 20)),   // num_lamellae
                0.6f + (i % 10) * 0.02f,     // g_ratio
                0.8f + (i % 10) * 0.02f,     // compaction_score
                0.85f + (i % 10) * 0.01f,    // integrity
                &result);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Saltatory computation: %.4f us average (target: < 1.0 us)\n", avg_time);
    EXPECT_LT(avg_time, 5.0) << "Saltatory computation too slow";
}

TEST_F(MyelinMathPerformanceTest, Saltatory_VelocityOnly) {
    const int NUM_OPS = 100000;
    volatile float result = 0.0f;
    nimcp_saltatory_result_t saltatory;

    Warmup([&]() {
        result = nimcp_myelin_saltatory_velocity(2.0f, 200.0f, 15, 0.7f, 0.9f, 0.95f, &saltatory);
    });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_saltatory_velocity(
                1.0f + (i % 20) * 0.2f,      // axon_diameter_um
                100.0f + (i % 20) * 20.0f,   // internode_length_um
                (uint32_t)(10 + (i % 20)),   // num_lamellae
                0.6f + (i % 10) * 0.02f,     // g_ratio
                0.8f + (i % 10) * 0.02f,     // compaction_score
                0.85f + (i % 10) * 0.01f,    // integrity
                &saltatory);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Saltatory velocity: %.4f us average (target: < 0.5 us)\n", avg_time);
    EXPECT_LT(avg_time, 2.0) << "Saltatory velocity too slow";
}

//=============================================================================
// 4. Activity-Dependent Myelination Performance
//=============================================================================

TEST_F(MyelinMathPerformanceTest, Activity_MyelinationRate) {
    const int NUM_OPS = 100000;
    nimcp_myelination_kinetics_t kinetics = nimcp_myelin_kinetics_default();
    volatile float result = 0.0f;

    Warmup([&]() { result = nimcp_myelin_compute_myelination_rate(10.0f, 15, &kinetics); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_compute_myelination_rate(
                (float)(i % 100) * 0.2f, 15, &kinetics);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Activity rate calc: %.4f us average (target: < 0.2 us)\n", avg_time);
    EXPECT_LT(avg_time, 1.0) << "Activity rate too slow";
}

TEST_F(MyelinMathPerformanceTest, Activity_UpdateLamellae) {
    const int NUM_OPS = 100000;
    nimcp_myelination_kinetics_t kinetics = nimcp_myelin_kinetics_default();
    volatile float result = 0.0f;

    Warmup([&]() { result = nimcp_myelin_update_lamellae(15.0f, 10.0f, 0.001f, &kinetics); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_update_lamellae(
                15.0f + (i % 10), (float)(i % 100) * 0.2f, 0.001f, &kinetics);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Lamellae update calc: %.4f us average (target: < 0.2 us)\n", avg_time);
    EXPECT_LT(avg_time, 1.0) << "Lamellae update too slow";
}

TEST_F(MyelinMathPerformanceTest, Activity_HillFunctionThroughput) {
    const int NUM_OPS = 1000000;
    nimcp_myelination_kinetics_t kinetics = nimcp_myelin_kinetics_default();
    volatile float sum = 0.0f;

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            sum += nimcp_myelin_compute_myelination_rate(
                (float)(i % 1000) * 0.02f, 15, &kinetics);
        }
    });

    double ops_per_sec = NUM_OPS / (time * 1e-6);
    printf("  Hill function throughput: %.0f ops/sec\n", ops_per_sec);
    EXPECT_GT(ops_per_sec, 500000.0) << "Hill function throughput too low";
}

//=============================================================================
// 5. Conduction Block Performance
//=============================================================================

TEST_F(MyelinMathPerformanceTest, Block_ProbabilityCalculation) {
    const int NUM_OPS = 100000;
    nimcp_conduction_block_params_t params = nimcp_myelin_block_params_default();
    volatile float result = 0.0f;

    Warmup([&]() { result = nimcp_myelin_block_probability(0.9f, 40.0f, &params); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_block_probability(
                0.9f - (float)(i % 50) * 0.01f,
                35.0f + (float)(i % 100) * 0.1f,
                &params);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Block probability: %.4f us average (target: < 0.1 us)\n", avg_time);
    EXPECT_LT(avg_time, 0.5) << "Block probability too slow";
}

TEST_F(MyelinMathPerformanceTest, Block_LowIntegrityProbability) {
    const int NUM_OPS = 100000;
    nimcp_conduction_block_params_t params = nimcp_myelin_block_params_default();
    volatile float result = 0.0f;

    Warmup([&]() {
        result = nimcp_myelin_block_probability(0.5f, 40.0f, &params);
    });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_block_probability(
                0.3f + (float)(i % 70) * 0.01f,
                35.0f + (float)(i % 100) * 0.1f,
                &params);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Low integrity block prob: %.4f us average (target: < 0.2 us)\n", avg_time);
    EXPECT_LT(avg_time, 1.0) << "Low integrity block probability too slow";
}

//=============================================================================
// 6. Internode Optimization Performance
//=============================================================================

TEST_F(MyelinMathPerformanceTest, Internode_OptimalLength) {
    const int NUM_OPS = 100000;
    volatile float result = 0.0f;

    Warmup([&]() { result = nimcp_myelin_optimal_internode(2.0f); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_optimal_internode(
                0.5f + (float)(i % 100) * 0.1f);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Optimal internode: %.4f us average (target: < 0.1 us)\n", avg_time);
    EXPECT_LT(avg_time, 0.5) << "Optimal internode too slow";
}

TEST_F(MyelinMathPerformanceTest, Internode_Efficiency) {
    const int NUM_OPS = 100000;
    volatile float result = 0.0f;

    Warmup([&]() { result = nimcp_myelin_internode_efficiency(200.0f, 200.0f); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_internode_efficiency(
                100.0f + (float)(i % 100) * 3.0f,
                200.0f + (float)(i % 50) * 2.0f);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Internode efficiency: %.4f us average (target: < 0.1 us)\n", avg_time);
    EXPECT_LT(avg_time, 0.5) << "Internode efficiency too slow";
}

//=============================================================================
// 7. Metabolic Efficiency Performance
//=============================================================================

TEST_F(MyelinMathPerformanceTest, Metabolic_FullComputation) {
    const int NUM_OPS = 50000;
    nimcp_metabolic_efficiency_t metabolic;

    // Signature: (axon_length_um, axon_diameter_um, num_nodes, mean_compaction, mean_integrity, result*)
    Warmup([&]() {
        nimcp_myelin_compute_metabolic_efficiency(1000.0f, 2.0f, 10, 0.8f, 0.9f, &metabolic);
    });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            nimcp_myelin_compute_metabolic_efficiency(
                500.0f + (float)(i % 50) * 20.0f,   // axon_length_um
                1.0f + (float)(i % 20) * 0.2f,      // axon_diameter_um
                (uint32_t)(5 + (i % 20)),           // num_nodes
                0.7f + (float)(i % 10) * 0.02f,     // mean_compaction
                0.8f + (float)(i % 10) * 0.02f,     // mean_integrity
                &metabolic);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Metabolic computation: %.4f us average (target: < 0.5 us)\n", avg_time);
    EXPECT_LT(avg_time, 2.0) << "Metabolic computation too slow";
}

TEST_F(MyelinMathPerformanceTest, Metabolic_ATPCostOnly) {
    const int NUM_OPS = 100000;
    volatile float result = 0.0f;
    nimcp_metabolic_efficiency_t eff;

    // Pre-compute efficiency
    nimcp_myelin_compute_metabolic_efficiency(1000.0f, 2.0f, 10, 0.8f, 0.9f, &eff);

    Warmup([&]() { result = nimcp_myelin_atp_per_ap(&eff); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_atp_per_ap(&eff);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  ATP cost calculation: %.4f us average (target: < 0.2 us)\n", avg_time);
    EXPECT_LT(avg_time, 1.0) << "ATP cost calculation too slow";
}

//=============================================================================
// 8. Stochastic Variability Performance
//=============================================================================

TEST_F(MyelinMathPerformanceTest, RNG_UniformGeneration) {
    const int NUM_OPS = 1000000;
    volatile float result = 0.0f;

    Warmup([&]() { result = nimcp_myelin_rng_uniform(&rng); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_rng_uniform(&rng);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  RNG uniform: %.4f us average (target: < 0.05 us)\n", avg_time);
    EXPECT_LT(avg_time, 0.2) << "RNG uniform too slow";

    double ops_per_sec = NUM_OPS / (time * 1e-6);
    printf("  RNG throughput: %.0f ops/sec\n", ops_per_sec);
    EXPECT_GT(ops_per_sec, 5000000.0) << "RNG throughput too low";
}

TEST_F(MyelinMathPerformanceTest, RNG_GaussianGeneration) {
    const int NUM_OPS = 500000;
    volatile float result = 0.0f;

    Warmup([&]() { result = nimcp_myelin_rng_normal(&rng, 0.0f, 1.0f); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_rng_normal(&rng, 0.0f, 1.0f);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  RNG gaussian: %.4f us average (target: < 0.1 us)\n", avg_time);
    EXPECT_LT(avg_time, 0.5) << "RNG gaussian too slow";
}

TEST_F(MyelinMathPerformanceTest, RNG_LogNormalGeneration) {
    const int NUM_OPS = 500000;
    volatile float result = 0.0f;

    Warmup([&]() { result = nimcp_myelin_rng_lognormal(&rng, 0.0f, 0.1f); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_rng_lognormal(&rng, 0.0f, 0.1f);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  RNG lognormal: %.4f us average (target: < 0.15 us)\n", avg_time);
    EXPECT_LT(avg_time, 0.5) << "RNG lognormal too slow";
}

//=============================================================================
// 9. Fast Math Performance
//=============================================================================

TEST_F(MyelinMathPerformanceTest, FastMath_Exponential) {
    const int NUM_OPS = 1000000;
    volatile float result = 0.0f;

    Warmup([&]() { result = nimcp_myelin_fast_exp(-1.0f); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_fast_exp(-5.0f + (float)(i % 100) * 0.1f);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Fast exp: %.4f us average (target: < 0.02 us)\n", avg_time);
    EXPECT_LT(avg_time, 0.1) << "Fast exp too slow";
}

TEST_F(MyelinMathPerformanceTest, FastMath_SquareRoot) {
    const int NUM_OPS = 1000000;
    volatile float result = 0.0f;

    Warmup([&]() { result = nimcp_myelin_fast_sqrt(4.0f); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_fast_sqrt(1.0f + (float)(i % 1000) * 0.01f);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Fast sqrt: %.4f us average (target: < 0.02 us)\n", avg_time);
    EXPECT_LT(avg_time, 0.1) << "Fast sqrt too slow";
}

TEST_F(MyelinMathPerformanceTest, FastMath_Power) {
    const int NUM_OPS = 500000;
    volatile float result = 0.0f;

    Warmup([&]() { result = nimcp_myelin_fast_pow(2.0f, 0.5f); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            result = nimcp_myelin_fast_pow(
                1.0f + (float)(i % 100) * 0.1f,
                0.1f + (float)(i % 50) * 0.02f);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Fast pow: %.4f us average (target: < 0.05 us)\n", avg_time);
    EXPECT_LT(avg_time, 0.2) << "Fast pow too slow";
}

//=============================================================================
// 10. Full Biophysics Pipeline Performance
//=============================================================================

TEST_F(MyelinMathPerformanceTest, Pipeline_BiophysicsInit) {
    const int NUM_OPS = 10000;
    nimcp_myelin_biophysics_t* bio = nullptr;

    Warmup([&]() {
        bio = nimcp_myelin_biophysics_create(false, 42);
        if (bio) nimcp_myelin_biophysics_destroy(bio);
    });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            bio = nimcp_myelin_biophysics_create(false, i);
            if (bio) nimcp_myelin_biophysics_destroy(bio);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Biophysics create/destroy: %.4f us average (target: < 10 us)\n", avg_time);
    EXPECT_LT(avg_time, 50.0) << "Biophysics create/destroy too slow";
}

TEST_F(MyelinMathPerformanceTest, Pipeline_BiophysicsUpdate) {
    const int NUM_OPS = 10000;
    nimcp_myelin_biophysics_t* bio = nimcp_myelin_biophysics_create(false, 42);
    ASSERT_NE(bio, nullptr);

    Warmup([&]() { nimcp_myelin_update_activity_ema(bio, 5.0f, 0.001f); });

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            nimcp_myelin_update_activity_ema(bio,
                (float)(i % 20) * 0.5f,
                0.001f);
        }
    });

    double avg_time = time / NUM_OPS;
    printf("  Biophysics activity EMA update: %.4f us average (target: < 1 us)\n", avg_time);
    EXPECT_LT(avg_time, 5.0) << "Biophysics update too slow";

    nimcp_myelin_biophysics_destroy(bio);
}

TEST_F(MyelinMathPerformanceTest, Pipeline_SheathWithBiophysics) {
    myelin_sheath_t* sheath = myelin_sheath_create_for_axon(1, 100, 50, 1000.0f, 2.0f, 16);
    ASSERT_NE(sheath, nullptr);

    myelin_sheath_init_biophysics(sheath, 2.0f, 37.0f);

    const int NUM_STEPS = 1000;

    // Warmup
    for (int i = 0; i < 100; i++) {
        myelin_sheath_update_biophysics(sheath);
    }

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_STEPS; i++) {
            myelin_sheath_update_biophysics(sheath);
        }
    });

    double avg_time = time / NUM_STEPS;
    printf("  Sheath biophysics update: %.2f us average (target: < 50 us)\n", avg_time);
    EXPECT_LT(avg_time, 200.0) << "Sheath biophysics update too slow";

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinMathPerformanceTest, Pipeline_NetworkWithBiophysics) {
    myelin_sheath_network_t* network = myelin_network_create_default(500);
    ASSERT_NE(network, nullptr);

    // Create 100 sheaths with biophysics
    const int NUM_SHEATHS = 100;
    for (int i = 0; i < NUM_SHEATHS; i++) {
        myelin_sheath_t* sheath = myelin_sheath_create_for_axon(
            i, i + 100, 50, 500.0f, 1.5f + (i % 10) * 0.1f, 8);
        myelin_sheath_init_biophysics(sheath, 1.5f + (i % 10) * 0.1f, 37.0f);
        myelin_network_add_sheath(network, sheath);
    }

    const int NUM_STEPS = 100;
    uint64_t time_us = 0;

    // Warmup
    for (int i = 0; i < 10; i++) {
        myelin_network_step(network, 0.001f, time_us);
        time_us += 1000;
    }

    double time = MeasureTime([&]() {
        for (int i = 0; i < NUM_STEPS; i++) {
            myelin_network_step(network, 0.001f, time_us);
            // Update biophysics for all sheaths
            for (uint32_t j = 0; j < network->num_sheaths; j++) {
                myelin_sheath_update_biophysics(network->sheaths[j]);
            }
            time_us += 1000;
        }
    });

    double avg_time = time / NUM_STEPS;
    printf("  Network step + biophysics (%d sheaths): %.2f us average\n",
           NUM_SHEATHS, avg_time);

    double per_sheath = avg_time / NUM_SHEATHS;
    printf("  Per-sheath biophysics: %.2f us average (target: < 50 us)\n", per_sheath);
    EXPECT_LT(per_sheath, 200.0) << "Per-sheath biophysics too slow";

    myelin_network_destroy(network);
}

//=============================================================================
// 11. Scalability Tests
//=============================================================================

TEST_F(MyelinMathPerformanceTest, Scalability_GRatioCalculations) {
    std::vector<int> sizes = {1000, 10000, 100000, 1000000};
    volatile float sum = 0.0f;

    printf("  G-ratio scalability:\n");
    for (int size : sizes) {
        double time = MeasureTime([&]() {
            for (int i = 0; i < size; i++) {
                sum += nimcp_myelin_optimal_g_ratio(
                    0.5f + (float)(i % 100) * 0.1f);
            }
        });

        double per_op = time / size;
        printf("    N=%d: %.4f us/op, total %.2f ms\n",
               size, per_op, time / 1000.0);
    }
}

TEST_F(MyelinMathPerformanceTest, Scalability_CableTheory) {
    std::vector<int> sizes = {1000, 10000, 100000};
    nimcp_cable_params_t params;

    printf("  Cable theory scalability:\n");
    for (int size : sizes) {
        double time = MeasureTime([&]() {
            for (int i = 0; i < size; i++) {
                // Signature: (float axon_diameter_um, uint32_t num_lamellae, result*)
                nimcp_myelin_compute_cable_params(
                    1.0f + (float)(i % 20) * 0.2f,  // axon_diameter_um
                    (uint32_t)(10 + (i % 20)),      // num_lamellae
                    &params);
            }
        });

        double per_op = time / size;
        printf("    N=%d: %.4f us/op, total %.2f ms\n",
               size, per_op, time / 1000.0);
    }
}

//=============================================================================
// 12. Memory Efficiency Tests
//=============================================================================

TEST_F(MyelinMathPerformanceTest, Memory_BiophysicsStructSize) {
    printf("  Biophysics struct sizes:\n");
    printf("    nimcp_cable_params_t: %zu bytes\n", sizeof(nimcp_cable_params_t));
    printf("    nimcp_myelination_kinetics_t: %zu bytes\n", sizeof(nimcp_myelination_kinetics_t));
    printf("    nimcp_conduction_block_params_t: %zu bytes\n", sizeof(nimcp_conduction_block_params_t));
    printf("    nimcp_saltatory_result_t: %zu bytes\n", sizeof(nimcp_saltatory_result_t));
    printf("    nimcp_metabolic_efficiency_t: %zu bytes\n", sizeof(nimcp_metabolic_efficiency_t));
    printf("    nimcp_myelin_biophysics_t: %zu bytes\n", sizeof(nimcp_myelin_biophysics_t));
    printf("    nimcp_myelin_rng_t: %zu bytes\n", sizeof(nimcp_myelin_rng_t));

    // Verify structs are reasonably sized
    EXPECT_LE(sizeof(nimcp_cable_params_t), 128);
    EXPECT_LE(sizeof(nimcp_myelin_biophysics_t), 256);
    EXPECT_LE(sizeof(nimcp_myelin_rng_t), 64);
}

TEST_F(MyelinMathPerformanceTest, Memory_CacheLineAlignment) {
    // Test that frequently accessed structs fit in cache line
    printf("  Cache alignment:\n");

    // Cable params (frequently accessed)
    printf("    cable_params fits in cache line: %s\n",
           sizeof(nimcp_cable_params_t) <= 64 ? "YES" : "NO");

    // RNG state
    printf("    rng_t fits in cache line: %s\n",
           sizeof(nimcp_myelin_rng_t) <= 64 ? "YES" : "NO");
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    printf("\n=== Myelin Math Performance Regression Tests ===\n\n");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
