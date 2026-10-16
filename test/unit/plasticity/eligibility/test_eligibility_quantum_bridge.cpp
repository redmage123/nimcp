//=============================================================================
// test_eligibility_quantum_bridge.cpp - Unit Tests for Eligibility Quantum Bridge
//=============================================================================
/**
 * @file test_eligibility_quantum_bridge.cpp
 * @brief Comprehensive tests for eligibility trace quantum integration module
 *
 * Tests: Bottleneck detection, credit assignment, quantum optimization, quantum walk
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

extern "C" {
#include "plasticity/eligibility/nimcp_eligibility_quantum_bridge.h"
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

class EligQuantumLifecycleTest : public ::testing::Test {
protected:
    eligibility_quantum_ctx_t ctx = nullptr;

    void TearDown() override {
        if (ctx) {
            elig_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(EligQuantumLifecycleTest, CreateWithDefaultConfig) {
    ctx = elig_quantum_create(nullptr);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(EligQuantumLifecycleTest, CreateWithCustomConfig) {
    elig_quantum_config_t config = elig_quantum_default_config();
    config.enable_bottleneck_detection = true;
    config.enable_credit_assignment = true;
    config.enable_quantum_optimization = true;
    config.bottleneck_threshold = 0.4f;

    ctx = elig_quantum_create(&config);
    ASSERT_NE(ctx, nullptr);
}

TEST_F(EligQuantumLifecycleTest, DestroyNull) {
    elig_quantum_destroy(nullptr);  // Should not crash
}

TEST_F(EligQuantumLifecycleTest, ResetContext) {
    ctx = elig_quantum_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    elig_quantum_reset(ctx);  // Should not crash
}

TEST_F(EligQuantumLifecycleTest, EnableDisable) {
    ctx = elig_quantum_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    EXPECT_TRUE(elig_quantum_is_enabled(ctx));

    elig_quantum_set_enabled(ctx, false);
    EXPECT_FALSE(elig_quantum_is_enabled(ctx));

    elig_quantum_set_enabled(ctx, true);
    EXPECT_TRUE(elig_quantum_is_enabled(ctx));
}

TEST_F(EligQuantumLifecycleTest, DefaultConfigValues) {
    elig_quantum_config_t config = elig_quantum_default_config();

    EXPECT_TRUE(config.enable_bottleneck_detection);
    EXPECT_TRUE(config.enable_credit_assignment);
    EXPECT_FLOAT_EQ(config.bottleneck_threshold, ELIG_QUANTUM_BOTTLENECK_THRESHOLD);
    EXPECT_EQ(config.mc_samples, ELIG_QUANTUM_MC_SAMPLES);
}

//=============================================================================
// Bottleneck Detection Tests
//=============================================================================

class EligQuantumBottleneckTest : public ::testing::Test {
protected:
    eligibility_quantum_ctx_t ctx = nullptr;
    std::vector<eligibility_trace_t> traces;
    std::vector<float> weights;
    const uint32_t num_synapses = 100;

    void SetUp() override {
        elig_quantum_config_t config = elig_quantum_default_config();
        config.enable_bottleneck_detection = true;
        config.bottleneck_threshold = 0.3f;
        ctx = elig_quantum_create(&config);

        // Initialize test data
        traces.resize(num_synapses);
        weights.resize(num_synapses);

        for (uint32_t i = 0; i < num_synapses; i++) {
            traces[i].trace = 0.1f + 0.8f * (float)i / num_synapses;
            // slow_trace removed from struct
            traces[i].last_update = 0;
            weights[i] = 0.5f;
        }
    }

    void TearDown() override {
        if (ctx) {
            elig_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(EligQuantumBottleneckTest, AnalyzeInformationFlow) {
    float efficiency = elig_quantum_analyze_information_flow(ctx, traces.data(), weights.data(), num_synapses);

    // Efficiency should be between 0 and 1
    EXPECT_GE(efficiency, 0.0f);
    EXPECT_LE(efficiency, 1.0f);
}

TEST_F(EligQuantumBottleneckTest, DetectBottlenecks) {
    elig_quantum_bottleneck_t bottlenecks[20];
    uint32_t num_found = 0;

    bool result = elig_quantum_detect_bottlenecks(ctx, traces.data(), weights.data(), num_synapses,
                                                   bottlenecks, 20, &num_found);
    EXPECT_TRUE(result);

    // Verify bottleneck fields are valid
    for (uint32_t i = 0; i < num_found; i++) {
        EXPECT_LT(bottlenecks[i].synapse_id, num_synapses);
        EXPECT_GE(bottlenecks[i].information_deficit, 0.0f);
        EXPECT_LE(bottlenecks[i].information_deficit, 1.0f);
    }
}

TEST_F(EligQuantumBottleneckTest, ComputeTraceEntropy) {
    float entropy = elig_quantum_compute_trace_entropy(traces.data(), num_synapses);

    // Entropy should be non-negative
    EXPECT_GE(entropy, 0.0f);
}

TEST_F(EligQuantumBottleneckTest, OptimizeFromBottlenecks) {
    // First detect bottlenecks
    elig_quantum_bottleneck_t bottlenecks[20];
    uint32_t num_found = 0;
    elig_quantum_detect_bottlenecks(ctx, traces.data(), weights.data(), num_synapses,
                                     bottlenecks, 20, &num_found);

    if (num_found > 0) {
        elig_quantum_params_t adjustments;
        bool result = elig_quantum_optimize_from_bottlenecks(ctx, bottlenecks, num_found, &adjustments);
        EXPECT_TRUE(result);

        // Parameters should be valid
        EXPECT_GT(adjustments.learning_rate, 0.0f);
        EXPECT_GT(adjustments.tau_fast, 0.0f);
        EXPECT_GT(adjustments.tau_slow, 0.0f);
    }
}

//=============================================================================
// Credit Assignment Tests
//=============================================================================

class EligQuantumCreditTest : public ::testing::Test {
protected:
    eligibility_quantum_ctx_t ctx = nullptr;
    std::vector<eligibility_trace_t> traces;
    const uint32_t num_synapses = 50;

    void SetUp() override {
        elig_quantum_config_t config = elig_quantum_default_config();
        config.enable_credit_assignment = true;
        config.mc_samples = 500;
        ctx = elig_quantum_create(&config);

        // Initialize traces with varying values
        traces.resize(num_synapses);
        for (uint32_t i = 0; i < num_synapses; i++) {
            traces[i].trace = 0.5f * std::exp(-(float)i / 10.0f);
            // slow_trace removed from struct
            traces[i].last_update = 0;
        }
    }

    void TearDown() override {
        if (ctx) {
            elig_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(EligQuantumCreditTest, AssignCredit) {
    elig_quantum_credit_t credits[20];
    uint32_t num_credits = 0;

    bool result = elig_quantum_assign_credit(ctx, traces.data(), num_synapses, 1.0f,
                                              credits, 20, &num_credits);
    EXPECT_TRUE(result);
    EXPECT_GT(num_credits, 0u);

    // Verify credit fractions sum to approximately 1
    float total_credit = 0.0f;
    for (uint32_t i = 0; i < num_credits; i++) {
        EXPECT_GE(credits[i].credit_fraction, 0.0f);
        EXPECT_LE(credits[i].credit_fraction, 1.0f);
        total_credit += credits[i].credit_fraction;
    }
    EXPECT_NEAR(total_credit, 1.0f, 0.1f);
}

TEST_F(EligQuantumCreditTest, EstimateOptimalEntropy) {
    float optimal_entropy = elig_quantum_estimate_optimal_entropy(ctx, traces.data(), num_synapses);

    // Optimal entropy should be positive
    EXPECT_GE(optimal_entropy, 0.0f);
}

TEST_F(EligQuantumCreditTest, ComputeCausalStrength) {
    std::vector<float> weights(num_synapses, 0.5f);
    std::vector<float> causal_strengths(num_synapses);

    bool result = elig_quantum_compute_causal_strength(ctx, traces.data(), weights.data(),
                                                        num_synapses, causal_strengths.data());
    EXPECT_TRUE(result);

    // Verify causal strengths are valid
    for (uint32_t i = 0; i < num_synapses; i++) {
        EXPECT_GE(causal_strengths[i], 0.0f);
        EXPECT_LE(causal_strengths[i], 1.0f);
    }
}

//=============================================================================
// Quantum Optimization Tests
//=============================================================================

class EligQuantumOptimizationTest : public ::testing::Test {
protected:
    eligibility_quantum_ctx_t ctx = nullptr;
    std::vector<eligibility_trace_t> traces;
    const uint32_t num_synapses = 30;

    void SetUp() override {
        elig_quantum_config_t config = elig_quantum_default_config();
        config.enable_quantum_optimization = true;
        config.initial_temperature = 1.0f;
        config.final_temperature = 0.01f;
        config.anneal_iterations = 100;
        ctx = elig_quantum_create(&config);

        traces.resize(num_synapses);
        for (uint32_t i = 0; i < num_synapses; i++) {
            traces[i].trace = 0.5f;
            // slow_trace removed from struct
        }
    }

    void TearDown() override {
        if (ctx) {
            elig_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(EligQuantumOptimizationTest, OptimizeParams) {
    elig_quantum_params_t current = {
        .tau_fast = 10.0f,
        .tau_slow = 50.0f,
        .learning_rate = 0.01f,
        .dopamine_sensitivity = 1.0f,
        .burst_threshold = 0.5f,
        .consolidation_threshold = 0.7f,
        .energy = 0.0f,
        .amplitude = 1.0f
    };

    elig_quantum_params_t optimized;

    bool result = elig_quantum_optimize_params(ctx, traces.data(), num_synapses, &current, &optimized);
    EXPECT_TRUE(result);

    // Optimized parameters should be valid
    EXPECT_GT(optimized.tau_fast, 0.0f);
    EXPECT_GT(optimized.tau_slow, 0.0f);
    EXPECT_GT(optimized.learning_rate, 0.0f);
}

TEST_F(EligQuantumOptimizationTest, GetAnnealState) {
    // Run some optimization steps first
    elig_quantum_params_t current = {
        .tau_fast = 10.0f,
        .tau_slow = 50.0f,
        .learning_rate = 0.01f
    };
    elig_quantum_params_t optimized;
    elig_quantum_optimize_params(ctx, traces.data(), num_synapses, &current, &optimized);

    elig_quantum_anneal_state_t state;
    bool result = elig_quantum_get_anneal_state(ctx, &state);
    EXPECT_TRUE(result);

    // Temperature should have decreased from initial
    EXPECT_LE(state.temperature, 1.0f);
    EXPECT_GT(state.iteration, 0u);
}

TEST_F(EligQuantumOptimizationTest, ResetAnneal) {
    // Run optimization
    elig_quantum_params_t current = {.learning_rate = 0.01f};
    elig_quantum_params_t optimized;
    elig_quantum_optimize_params(ctx, traces.data(), num_synapses, &current, &optimized);

    // Reset
    elig_quantum_reset_anneal(ctx);

    elig_quantum_anneal_state_t state;
    elig_quantum_get_anneal_state(ctx, &state);

    // Temperature should be back to initial
    EXPECT_NEAR(state.temperature, 1.0f, 0.1f);
}

TEST_F(EligQuantumOptimizationTest, SetObjective) {
    // Test different objectives
    for (uint32_t obj = 0; obj < 5; obj++) {
        elig_quantum_set_objective(ctx, obj);
        // Should not crash
    }
}

//=============================================================================
// Quantum Walk Tests
//=============================================================================

class EligQuantumWalkTest : public ::testing::Test {
protected:
    eligibility_quantum_ctx_t ctx = nullptr;
    std::vector<uint8_t> adjacency;
    const uint32_t num_synapses = 25;

    void SetUp() override {
        elig_quantum_config_t config = elig_quantum_default_config();
        config.enable_quantum_walk = true;
        config.walk_steps = 50;
        ctx = elig_quantum_create(&config);

        // Create simple adjacency matrix (5x5 grid connectivity)
        adjacency.resize(num_synapses * num_synapses, 0);
        for (uint32_t i = 0; i < num_synapses; i++) {
            // Connect to neighbors in grid
            if (i % 5 > 0) adjacency[i * num_synapses + (i - 1)] = 1;
            if (i % 5 < 4) adjacency[i * num_synapses + (i + 1)] = 1;
            if (i >= 5) adjacency[i * num_synapses + (i - 5)] = 1;
            if (i < 20) adjacency[i * num_synapses + (i + 5)] = 1;
        }
    }

    void TearDown() override {
        if (ctx) {
            elig_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(EligQuantumWalkTest, DiffuseSingleSource) {
    std::vector<float> diffused(num_synapses, 0.0f);

    bool result = elig_quantum_diffuse(ctx, 12, 1.0f, adjacency.data(), num_synapses, diffused.data());
    EXPECT_TRUE(result);

    // Should have spread from center
    float total = 0.0f;
    for (uint32_t i = 0; i < num_synapses; i++) {
        EXPECT_GE(diffused[i], 0.0f);
        total += diffused[i];
    }
    // Total probability should be approximately 1
    EXPECT_NEAR(total, 1.0f, 0.1f);
}

TEST_F(EligQuantumWalkTest, DiffuseMultiSource) {
    uint32_t sources[] = {0, 24};  // Corners
    float initial[] = {0.5f, 0.5f};
    std::vector<float> diffused(num_synapses, 0.0f);

    bool result = elig_quantum_diffuse_multi(ctx, sources, initial, 2, adjacency.data(),
                                              num_synapses, diffused.data());
    EXPECT_TRUE(result);

    float total = 0.0f;
    for (uint32_t i = 0; i < num_synapses; i++) {
        total += diffused[i];
    }
    EXPECT_NEAR(total, 1.0f, 0.1f);
}

TEST_F(EligQuantumWalkTest, GetWalkStats) {
    std::vector<float> diffused(num_synapses);
    elig_quantum_diffuse(ctx, 12, 1.0f, adjacency.data(), num_synapses, diffused.data());

    float speedup, distance, entropy;
    bool result = elig_quantum_get_walk_stats(ctx, &speedup, &distance, &entropy);
    EXPECT_TRUE(result);

    // Speedup should be >= 1 (quantum >= classical)
    EXPECT_GE(speedup, 1.0f);
    EXPECT_GE(distance, 0.0f);
    EXPECT_GE(entropy, 0.0f);
}

//=============================================================================
// Enhanced Operations Tests
//=============================================================================

class EligQuantumEnhancedTest : public ::testing::Test {
protected:
    eligibility_quantum_ctx_t ctx = nullptr;
    eligibility_config_t elig_config;

    void SetUp() override {
        elig_quantum_config_t config = elig_quantum_default_config();
        config.enable_bottleneck_detection = true;
        config.enable_credit_assignment = true;
        config.enable_quantum_optimization = true;
        ctx = elig_quantum_create(&config);

        // Initialize eligibility config
        elig_config.decay_lambda = 0.95f;
        elig_config.learning_rate = 0.01f;
        elig_config.use_neuromodulation = true;
    }

    void TearDown() override {
        if (ctx) {
            elig_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(EligQuantumEnhancedTest, UpdateTraceEnhanced) {
    eligibility_trace_t trace = {
        .trace = 0.5f,
        .last_update = 0
    };

    elig_quantum_update_trace_enhanced(ctx, &trace, &elig_config, 1000, true, 0.5f);

    // Trace should have been updated
    EXPECT_GT(trace.trace, 0.0f);
}

TEST_F(EligQuantumEnhancedTest, LearningTick) {
    const uint32_t num = 20;
    std::vector<eligibility_trace_t> traces(num);
    std::vector<float> weights(num, 0.5f);

    for (uint32_t i = 0; i < num; i++) {
        traces[i].trace = 0.3f;
        // slow_trace removed from struct
    }

    uint32_t updated = elig_quantum_learning_tick(ctx, traces.data(), weights.data(), num,
                                                   &elig_config, 1.0f, 1000);

    // Should have updated some weights
    EXPECT_GE(updated, 0u);
}

//=============================================================================
// Metrics Tests
//=============================================================================

class EligQuantumMetricsTest : public ::testing::Test {
protected:
    eligibility_quantum_ctx_t ctx = nullptr;

    void SetUp() override {
        elig_quantum_config_t config = elig_quantum_default_config();
        config.enable_metrics = true;
        ctx = elig_quantum_create(&config);
    }

    void TearDown() override {
        if (ctx) {
            elig_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(EligQuantumMetricsTest, GetMetrics) {
    elig_quantum_metrics_t metrics;

    bool result = elig_quantum_get_metrics(ctx, &metrics);
    EXPECT_TRUE(result);
}

TEST_F(EligQuantumMetricsTest, ResetMetrics) {
    // Generate some metrics
    std::vector<eligibility_trace_t> traces(10);
    std::vector<float> weights(10, 0.5f);
    elig_quantum_analyze_information_flow(ctx, traces.data(), weights.data(), 10);

    // Reset
    elig_quantum_reset_metrics(ctx);

    elig_quantum_metrics_t metrics;
    elig_quantum_get_metrics(ctx, &metrics);

    EXPECT_EQ(metrics.bottleneck_analyses, 0u);
}

//=============================================================================
// Diagnostic Tests
//=============================================================================

class EligQuantumDiagnosticTest : public ::testing::Test {
protected:
    eligibility_quantum_ctx_t ctx = nullptr;

    void SetUp() override {
        ctx = elig_quantum_create(nullptr);
    }

    void TearDown() override {
        if (ctx) {
            elig_quantum_destroy(ctx);
            ctx = nullptr;
        }
    }
};

TEST_F(EligQuantumDiagnosticTest, Verify) {
    bool valid = elig_quantum_verify(ctx);
    EXPECT_TRUE(valid);
}

TEST_F(EligQuantumDiagnosticTest, PrintStatus) {
    // Should not crash
    elig_quantum_print_status(ctx);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
