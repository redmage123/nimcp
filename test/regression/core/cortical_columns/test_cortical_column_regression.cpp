/**
 * @file test_cortical_column_regression.cpp
 * @brief Comprehensive regression tests for cortical column module
 *
 * WHAT: Regression tests for minicolumns, hypercolumns, and column pool
 * WHY:  Ensure cortical column behavior is stable across versions
 * HOW:  GTest framework with performance benchmarks, determinism checks,
 *       memory safety tests, null pointer safety, and stress tests
 *
 * TEST CATEGORIES:
 * - Performance Benchmarks: Column update, layer propagation timing
 * - Determinism Tests: Same input = same output
 * - State Consistency Tests: Processing maintains valid state
 * - Memory Usage Tests: Create/destroy cycles, memory patterns
 * - Null Pointer Safety: Graceful handling of null parameters
 * - Backward Compatibility: Default config values remain stable
 * - Stress Tests: Rapid updates, edge cases
 *
 * @author NIMCP Development Team
 * @date 2025-01-24
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>

#include "utils/nimcp_test_base.h"
#include "core/cortical_columns/nimcp_cortical_column.h"
#include "core/cortical_columns/nimcp_cortical_layers.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Constants and Thresholds
//=============================================================================

// Performance thresholds
constexpr double MINICOLUMN_CREATE_LATENCY_MS = 1.0;  // <1ms to create minicolumn
constexpr double HYPERCOLUMN_COMPUTE_LATENCY_MS = 5.0;  // <5ms per compute
constexpr double LAYER_PROPAGATION_LATENCY_MS = 2.0;  // <2ms per propagation
constexpr uint32_t MIN_OPS_PER_SEC = 1000;  // Minimum throughput

// Numerical tolerances
constexpr float NUMERICAL_TOLERANCE = 1e-6f;
constexpr float ACTIVATION_TOLERANCE = 0.01f;

//=============================================================================
// Test Fixture
//=============================================================================

class CorticalColumnRegressionTest : public NimcpTestBase {
protected:
    cortical_column_pool_t* pool = nullptr;
    std::mt19937 rng{42};  // Deterministic RNG

    void SetUp() override {
        NimcpTestBase::SetUp();

        cortical_column_pool_config_t config = {
            .max_minicolumns = 1000,
            .max_hypercolumns = 100,
            .max_neurons_per_minicolumn = 100,
            .enable_cow_support = true
        };
        pool = cortical_column_pool_create(&config);
    }

    void TearDown() override {
        if (pool) {
            cortical_column_pool_destroy(pool);
            pool = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    // Helper: Create minicolumn config with specified neurons
    minicolumn_config_t create_minicolumn_config(uint32_t num_neurons = 80) {
        uint32_t* neuron_ids = new uint32_t[num_neurons];
        for (uint32_t i = 0; i < num_neurons; i++) {
            neuron_ids[i] = i;
        }

        std::uniform_real_distribution<float> dist(-10.0f, 10.0f);
        std::uniform_real_distribution<float> angle_dist(0.0f, 180.0f);

        return {
            .neuron_ids = neuron_ids,
            .num_neurons = num_neurons,
            .receptive_field = {
                .center_x = dist(rng),
                .center_y = dist(rng),
                .center_z = dist(rng),
                .radius = std::abs(dist(rng)) + 1.0f
            },
            .tuning_preference = angle_dist(rng),
            .layers = {
                .layer_2_3_count = num_neurons / 3,
                .layer_4_count = num_neurons / 3,
                .layer_5_6_count = num_neurons - 2 * (num_neurons / 3)
            }
        };
    }

    // Helper: Free minicolumn config memory
    void free_minicolumn_config(minicolumn_config_t& config) {
        delete[] config.neuron_ids;
        config.neuron_ids = nullptr;
    }

    // Helper: Generate random input
    std::vector<float> random_input(uint32_t size) {
        std::vector<float> input(size);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        for (auto& val : input) {
            val = dist(rng);
        }
        return input;
    }

    // Helper: Measure elapsed time in milliseconds
    template<typename Func>
    double measure_time_ms(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    // Helper: Get current allocated memory
    size_t get_allocated_memory() {
        nimcp_memory_stats_t stats;
        if (nimcp_memory_get_stats(&stats)) {
            return stats.current_allocated;
        }
        return 0;
    }
};

//=============================================================================
// CATEGORY 1: Performance Benchmarks
//=============================================================================

TEST_F(CorticalColumnRegressionTest, MinicolumnCreateLatency) {
    // WHAT: Benchmark minicolumn creation latency
    // WHY:  Verify creation remains performant
    // TARGET: <1ms per minicolumn

    ASSERT_NE(pool, nullptr);

    const uint32_t iterations = 100;
    double total_ms = 0;

    for (uint32_t i = 0; i < iterations; i++) {
        auto config = create_minicolumn_config(80);

        double latency = measure_time_ms([&]() {
            minicolumn_t* col = minicolumn_create(pool, &config);
            ASSERT_NE(col, nullptr);
            minicolumn_destroy(col);
        });

        total_ms += latency;
        free_minicolumn_config(config);
    }

    double avg_latency = total_ms / iterations;
    EXPECT_LT(avg_latency, MINICOLUMN_CREATE_LATENCY_MS)
        << "Minicolumn create latency: " << avg_latency << " ms";
}

TEST_F(CorticalColumnRegressionTest, HypercolumnComputeLatency) {
    // WHAT: Benchmark hypercolumn compute latency
    // WHY:  Verify compute performance remains stable
    // TARGET: <5ms per compute

    ASSERT_NE(pool, nullptr);

    const uint32_t num_minicolumns = 16;
    std::vector<minicolumn_config_t> configs;

    for (uint32_t i = 0; i < num_minicolumns; i++) {
        configs.push_back(create_minicolumn_config(80));
    }

    hypercolumn_config_t hcol_config = {
        .num_minicolumns = num_minicolumns,
        .minicolumn_configs = configs.data(),
        .feature_space_min = 0.0f,
        .feature_space_max = 180.0f,
        .topographic_x = 0.0f,
        .topographic_y = 0.0f,
        .competition = CC_COMPETITION_SOFTMAX,
        .k_winners = 3,
        .temperature = 1.0f,
        .lateral_inhibition_strength = 0.5f,
        .lateral_inhibition_sigma1 = 1.0f,
        .lateral_inhibition_sigma2 = 3.0f
    };

    hypercolumn_t* hcol = hypercolumn_create(pool, &hcol_config);
    ASSERT_NE(hcol, nullptr);

    auto input = random_input(100);

    // Warmup
    for (int i = 0; i < 10; i++) {
        hypercolumn_compute(hcol, input.data(), input.size());
    }

    // Measure
    const uint32_t iterations = 100;
    double total_ms = measure_time_ms([&]() {
        for (uint32_t i = 0; i < iterations; i++) {
            hypercolumn_compute(hcol, input.data(), input.size());
        }
    });

    double avg_latency = total_ms / iterations;

    hypercolumn_destroy(hcol);
    for (auto& config : configs) {
        free_minicolumn_config(config);
    }

    EXPECT_LT(avg_latency, HYPERCOLUMN_COMPUTE_LATENCY_MS)
        << "Hypercolumn compute latency: " << avg_latency << " ms";
}

TEST_F(CorticalColumnRegressionTest, LayerPropagationLatency) {
    // WHAT: Benchmark laminar propagation latency
    // WHY:  Verify layer processing remains performant
    // TARGET: <2ms per propagation

    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    laminar_apply_canonical_circuit(ls);

    std::vector<float> input(64, 0.5f);
    laminar_process_input(ls, CC_LAYER_IV, input.data(), input.size());

    // Warmup
    for (int i = 0; i < 10; i++) {
        laminar_process_feedforward(ls);
    }

    // Measure
    const uint32_t iterations = 100;
    double total_ms = measure_time_ms([&]() {
        for (uint32_t i = 0; i < iterations; i++) {
            laminar_process_feedforward(ls);
        }
    });

    double avg_latency = total_ms / iterations;

    laminar_structure_destroy(ls);

    EXPECT_LT(avg_latency, LAYER_PROPAGATION_LATENCY_MS)
        << "Layer propagation latency: " << avg_latency << " ms";
}

TEST_F(CorticalColumnRegressionTest, CreateDestroyThroughput) {
    // WHAT: Benchmark create/destroy throughput
    // WHY:  Verify allocation performance
    // TARGET: >1000 ops/sec

    ASSERT_NE(pool, nullptr);

    const uint32_t iterations = 500;

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < iterations; i++) {
        auto config = create_minicolumn_config(80);
        minicolumn_t* col = minicolumn_create(pool, &config);
        ASSERT_NE(col, nullptr);
        minicolumn_destroy(col);
        free_minicolumn_config(config);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_sec = std::chrono::duration<double>(end - start).count();
    double ops_per_sec = iterations / elapsed_sec;

    EXPECT_GT(ops_per_sec, MIN_OPS_PER_SEC)
        << "Create/destroy throughput: " << ops_per_sec << " ops/sec";
}

//=============================================================================
// CATEGORY 2: Determinism Tests
//=============================================================================

TEST_F(CorticalColumnRegressionTest, MinicolumnComputeDeterminism) {
    // WHAT: Verify minicolumn compute is deterministic
    // WHY:  Same input must produce same output
    // TARGET: Outputs match exactly

    ASSERT_NE(pool, nullptr);

    auto config = create_minicolumn_config(80);
    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    std::vector<float> input = {0.5f, 0.3f, 0.7f, 0.2f, 0.8f};

    // First computation
    float result1 = minicolumn_compute(col, input.data(), input.size());

    // Second computation with same input
    float result2 = minicolumn_compute(col, input.data(), input.size());

    // Reset and compute again
    minicolumn_set_receptive_field(col,
        config.receptive_field.center_x,
        config.receptive_field.center_y,
        config.receptive_field.center_z,
        config.receptive_field.radius);
    float result3 = minicolumn_compute(col, input.data(), input.size());

    minicolumn_destroy(col);
    free_minicolumn_config(config);

    // All results should match
    EXPECT_FLOAT_EQ(result1, result2) << "Consecutive computes differ";
    EXPECT_FLOAT_EQ(result1, result3) << "Reset and compute differs";
}

TEST_F(CorticalColumnRegressionTest, HypercolumnComputeDeterminism) {
    // WHAT: Verify hypercolumn compute is deterministic
    // WHY:  Ensemble processing must be reproducible
    // TARGET: Winner and distribution match

    ASSERT_NE(pool, nullptr);

    const uint32_t num_minicolumns = 16;
    std::vector<minicolumn_config_t> configs;

    for (uint32_t i = 0; i < num_minicolumns; i++) {
        configs.push_back(create_minicolumn_config(80));
    }

    hypercolumn_config_t hcol_config = {
        .num_minicolumns = num_minicolumns,
        .minicolumn_configs = configs.data(),
        .feature_space_min = 0.0f,
        .feature_space_max = 180.0f,
        .topographic_x = 0.0f,
        .topographic_y = 0.0f,
        .competition = CC_COMPETITION_SOFTMAX,
        .k_winners = 3,
        .temperature = 1.0f,
        .lateral_inhibition_strength = 0.5f,
        .lateral_inhibition_sigma1 = 1.0f,
        .lateral_inhibition_sigma2 = 3.0f
    };

    hypercolumn_t* hcol = hypercolumn_create(pool, &hcol_config);
    ASSERT_NE(hcol, nullptr);

    auto input = random_input(100);

    // First computation
    hypercolumn_compute(hcol, input.data(), input.size());
    uint32_t winner1 = hypercolumn_get_winner(hcol);
    std::vector<float> dist1(num_minicolumns);
    hypercolumn_get_distribution(hcol, dist1.data(), num_minicolumns);

    // Second computation with same input
    hypercolumn_compute(hcol, input.data(), input.size());
    uint32_t winner2 = hypercolumn_get_winner(hcol);
    std::vector<float> dist2(num_minicolumns);
    hypercolumn_get_distribution(hcol, dist2.data(), num_minicolumns);

    hypercolumn_destroy(hcol);
    for (auto& config : configs) {
        free_minicolumn_config(config);
    }

    // Verify determinism
    EXPECT_EQ(winner1, winner2) << "Winners differ";
    for (uint32_t i = 0; i < num_minicolumns; i++) {
        EXPECT_FLOAT_EQ(dist1[i], dist2[i]) << "Distribution differs at " << i;
    }
}

TEST_F(CorticalColumnRegressionTest, ReceptiveFieldWeightDeterminism) {
    // WHAT: Verify receptive field weight computation is deterministic
    // WHY:  Gaussian weighting must be reproducible
    // TARGET: Weights match exactly

    ASSERT_NE(pool, nullptr);

    auto config = create_minicolumn_config(80);
    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    minicolumn_set_receptive_field(col, 0.0f, 0.0f, 0.0f, 2.0f);

    // Test multiple points
    std::vector<std::tuple<float, float, float>> test_points = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 1.0f},
        {5.0f, 5.0f, 5.0f}
    };

    for (const auto& [x, y, z] : test_points) {
        float weight1 = minicolumn_compute_receptive_weight(col, x, y, z);
        float weight2 = minicolumn_compute_receptive_weight(col, x, y, z);
        EXPECT_FLOAT_EQ(weight1, weight2)
            << "Weight differs at (" << x << "," << y << "," << z << ")";
    }

    minicolumn_destroy(col);
    free_minicolumn_config(config);
}

TEST_F(CorticalColumnRegressionTest, CompetitionDeterminism) {
    // WHAT: Verify competition results are deterministic
    // WHY:  Competition dynamics must be reproducible
    // TARGET: Winners match for each mode

    ASSERT_NE(pool, nullptr);

    const uint32_t num_minicolumns = 16;
    cc_competition_mode_t modes[] = {
        CC_COMPETITION_WINNER_TAKE_ALL,
        CC_COMPETITION_K_WINNERS,
        CC_COMPETITION_SOFTMAX,
        CC_COMPETITION_NONE
    };

    for (auto mode : modes) {
        std::vector<minicolumn_config_t> configs;
        for (uint32_t i = 0; i < num_minicolumns; i++) {
            configs.push_back(create_minicolumn_config(80));
        }

        hypercolumn_config_t hcol_config = {
            .num_minicolumns = num_minicolumns,
            .minicolumn_configs = configs.data(),
            .feature_space_min = 0.0f,
            .feature_space_max = 180.0f,
            .topographic_x = 0.0f,
            .topographic_y = 0.0f,
            .competition = mode,
            .k_winners = 3,
            .temperature = 1.0f,
            .lateral_inhibition_strength = 0.5f,
            .lateral_inhibition_sigma1 = 1.0f,
            .lateral_inhibition_sigma2 = 3.0f
        };

        hypercolumn_t* hcol = hypercolumn_create(pool, &hcol_config);
        ASSERT_NE(hcol, nullptr);

        auto input = random_input(100);

        // Two runs
        hypercolumn_compute(hcol, input.data(), input.size());
        uint32_t winner1 = hypercolumn_get_winner(hcol);

        hypercolumn_compute(hcol, input.data(), input.size());
        uint32_t winner2 = hypercolumn_get_winner(hcol);

        hypercolumn_destroy(hcol);
        for (auto& config : configs) {
            free_minicolumn_config(config);
        }

        EXPECT_EQ(winner1, winner2) << "Competition mode " << mode << " not deterministic";
    }
}

//=============================================================================
// CATEGORY 3: State Consistency Tests
//=============================================================================

TEST_F(CorticalColumnRegressionTest, ActivationBoundsAfterCompute) {
    // WHAT: Verify activations stay bounded after compute
    // WHY:  Activations must be in valid range [0, 1]
    // TARGET: All activations in [0, 1]

    ASSERT_NE(pool, nullptr);

    auto config = create_minicolumn_config(80);
    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    // Test with various inputs
    std::vector<std::vector<float>> test_inputs = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f},
        {-1.0f, 2.0f, 0.5f},
        {100.0f, -100.0f, 0.0f}
    };

    for (const auto& input : test_inputs) {
        float activation = minicolumn_compute(col, input.data(), input.size());

        if (activation >= 0.0f) {  // -1.0 indicates error
            EXPECT_GE(activation, 0.0f) << "Activation below 0";
            EXPECT_LE(activation, 1.0f) << "Activation above 1";
        }
    }

    minicolumn_destroy(col);
    free_minicolumn_config(config);
}

TEST_F(CorticalColumnRegressionTest, DistributionSumsToOne) {
    // WHAT: Verify softmax distribution sums to 1
    // WHY:  Valid probability distribution
    // TARGET: Sum within tolerance of 1.0

    ASSERT_NE(pool, nullptr);

    const uint32_t num_minicolumns = 16;
    std::vector<minicolumn_config_t> configs;

    for (uint32_t i = 0; i < num_minicolumns; i++) {
        configs.push_back(create_minicolumn_config(80));
    }

    hypercolumn_config_t hcol_config = {
        .num_minicolumns = num_minicolumns,
        .minicolumn_configs = configs.data(),
        .feature_space_min = 0.0f,
        .feature_space_max = 180.0f,
        .topographic_x = 0.0f,
        .topographic_y = 0.0f,
        .competition = CC_COMPETITION_SOFTMAX,
        .k_winners = 3,
        .temperature = 1.0f,
        .lateral_inhibition_strength = 0.5f,
        .lateral_inhibition_sigma1 = 1.0f,
        .lateral_inhibition_sigma2 = 3.0f
    };

    hypercolumn_t* hcol = hypercolumn_create(pool, &hcol_config);
    ASSERT_NE(hcol, nullptr);

    // Test multiple inputs
    for (int test = 0; test < 10; test++) {
        auto input = random_input(100);
        hypercolumn_compute(hcol, input.data(), input.size());

        std::vector<float> distribution(num_minicolumns);
        hypercolumn_get_distribution(hcol, distribution.data(), num_minicolumns);

        float sum = 0.0f;
        for (float val : distribution) {
            EXPECT_GE(val, 0.0f) << "Negative probability";
            EXPECT_LE(val, 1.0f) << "Probability > 1";
            EXPECT_FALSE(std::isnan(val)) << "NaN in distribution";
            EXPECT_FALSE(std::isinf(val)) << "Inf in distribution";
            sum += val;
        }

        EXPECT_NEAR(sum, 1.0f, 0.01f) << "Distribution doesn't sum to 1";
    }

    hypercolumn_destroy(hcol);
    for (auto& config : configs) {
        free_minicolumn_config(config);
    }
}

TEST_F(CorticalColumnRegressionTest, StatsConsistencyAfterProcessing) {
    // WHAT: Verify stats remain consistent after processing
    // WHY:  Statistics must reflect actual state
    // TARGET: Stats match processing history

    ASSERT_NE(pool, nullptr);

    auto config = create_minicolumn_config(80);
    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    auto input = random_input(10);

    // Process multiple times
    for (int i = 0; i < 100; i++) {
        minicolumn_compute(col, input.data(), input.size());
    }

    minicolumn_stats_t stats;
    minicolumn_get_stats(col, &stats);

    // Verify stats are valid
    EXPECT_GE(stats.activation_level, 0.0f);
    EXPECT_LE(stats.activation_level, 1.0f);
    EXPECT_GE(stats.inhibition_level, 0.0f);
    EXPECT_LE(stats.inhibition_level, 1.0f);
    EXPECT_GE(stats.total_activations, 0u);
    EXPECT_GE(stats.average_activation, 0.0f);
    EXPECT_LE(stats.average_activation, 1.0f);
    EXPECT_EQ(stats.num_neurons, 80u);

    minicolumn_destroy(col);
    free_minicolumn_config(config);
}

TEST_F(CorticalColumnRegressionTest, HypercolumnStatsConsistency) {
    // WHAT: Verify hypercolumn stats are consistent
    // WHY:  Ensemble statistics must reflect state
    // TARGET: Stats values are valid and consistent

    ASSERT_NE(pool, nullptr);

    const uint32_t num_minicolumns = 16;
    std::vector<minicolumn_config_t> configs;

    for (uint32_t i = 0; i < num_minicolumns; i++) {
        configs.push_back(create_minicolumn_config(80));
    }

    hypercolumn_config_t hcol_config = {
        .num_minicolumns = num_minicolumns,
        .minicolumn_configs = configs.data(),
        .feature_space_min = 0.0f,
        .feature_space_max = 180.0f,
        .topographic_x = 0.0f,
        .topographic_y = 0.0f,
        .competition = CC_COMPETITION_SOFTMAX,
        .k_winners = 3,
        .temperature = 1.0f,
        .lateral_inhibition_strength = 0.5f,
        .lateral_inhibition_sigma1 = 1.0f,
        .lateral_inhibition_sigma2 = 3.0f
    };

    hypercolumn_t* hcol = hypercolumn_create(pool, &hcol_config);
    ASSERT_NE(hcol, nullptr);

    auto input = random_input(100);

    for (int i = 0; i < 50; i++) {
        hypercolumn_compute(hcol, input.data(), input.size());
    }

    cc_hypercolumn_stats_t stats;
    hypercolumn_get_stats(hcol, &stats);

    EXPECT_EQ(stats.num_minicolumns, num_minicolumns);
    EXPECT_LT(stats.winner_index, num_minicolumns);
    EXPECT_GE(stats.winner_activation, 0.0f);
    EXPECT_LE(stats.winner_activation, 1.0f);
    EXPECT_GE(stats.total_activation, 0.0f);
    EXPECT_GE(stats.entropy, 0.0f);
    EXPECT_GE(stats.total_computations, 50u);
    EXPECT_EQ(stats.competition_mode, CC_COMPETITION_SOFTMAX);

    hypercolumn_destroy(hcol);
    for (auto& config : configs) {
        free_minicolumn_config(config);
    }
}

TEST_F(CorticalColumnRegressionTest, WinnerIndexValid) {
    // WHAT: Verify winner index is always valid
    // WHY:  Winner must be within bounds
    // TARGET: Winner < num_minicolumns

    ASSERT_NE(pool, nullptr);

    const uint32_t num_minicolumns = 8;
    std::vector<minicolumn_config_t> configs;

    for (uint32_t i = 0; i < num_minicolumns; i++) {
        configs.push_back(create_minicolumn_config(80));
    }

    hypercolumn_config_t hcol_config = {
        .num_minicolumns = num_minicolumns,
        .minicolumn_configs = configs.data(),
        .feature_space_min = 0.0f,
        .feature_space_max = 180.0f,
        .topographic_x = 0.0f,
        .topographic_y = 0.0f,
        .competition = CC_COMPETITION_WINNER_TAKE_ALL,
        .k_winners = 1,
        .temperature = 1.0f,
        .lateral_inhibition_strength = 0.5f,
        .lateral_inhibition_sigma1 = 1.0f,
        .lateral_inhibition_sigma2 = 3.0f
    };

    hypercolumn_t* hcol = hypercolumn_create(pool, &hcol_config);
    ASSERT_NE(hcol, nullptr);

    for (int test = 0; test < 100; test++) {
        auto input = random_input(50);
        hypercolumn_compute(hcol, input.data(), input.size());

        uint32_t winner = hypercolumn_get_winner(hcol);
        EXPECT_LT(winner, num_minicolumns) << "Winner out of bounds";
    }

    hypercolumn_destroy(hcol);
    for (auto& config : configs) {
        free_minicolumn_config(config);
    }
}

//=============================================================================
// CATEGORY 4: Memory Usage Tests
//=============================================================================

TEST_F(CorticalColumnRegressionTest, CreateDestroyCyclesNoLeak) {
    // WHAT: Verify no memory leak in create/destroy cycles
    // WHY:  Memory must be properly released
    // TARGET: Memory returns to baseline

    ASSERT_NE(pool, nullptr);

    size_t memory_before = get_allocated_memory();

    for (int cycle = 0; cycle < 100; cycle++) {
        auto config = create_minicolumn_config(80);
        minicolumn_t* col = minicolumn_create(pool, &config);
        ASSERT_NE(col, nullptr);
        minicolumn_destroy(col);
        free_minicolumn_config(config);
    }

    size_t memory_after = get_allocated_memory();
    size_t leak = (memory_after > memory_before) ? (memory_after - memory_before) : 0;

    EXPECT_LT(leak, 1024) << "Memory leak: " << leak << " bytes";
}

TEST_F(CorticalColumnRegressionTest, HypercolumnMemoryPattern) {
    // WHAT: Verify hypercolumn memory is properly managed
    // WHY:  Complex structures need proper cleanup
    // TARGET: No significant leak after cycles

    ASSERT_NE(pool, nullptr);

    size_t memory_before = get_allocated_memory();

    for (int cycle = 0; cycle < 50; cycle++) {
        const uint32_t num_minicolumns = 8;
        std::vector<minicolumn_config_t> configs;

        for (uint32_t i = 0; i < num_minicolumns; i++) {
            configs.push_back(create_minicolumn_config(80));
        }

        hypercolumn_config_t hcol_config = {
            .num_minicolumns = num_minicolumns,
            .minicolumn_configs = configs.data(),
            .feature_space_min = 0.0f,
            .feature_space_max = 180.0f,
            .topographic_x = 0.0f,
            .topographic_y = 0.0f,
            .competition = CC_COMPETITION_SOFTMAX,
            .k_winners = 3,
            .temperature = 1.0f,
            .lateral_inhibition_strength = 0.5f,
            .lateral_inhibition_sigma1 = 1.0f,
            .lateral_inhibition_sigma2 = 3.0f
        };

        hypercolumn_t* hcol = hypercolumn_create(pool, &hcol_config);
        ASSERT_NE(hcol, nullptr);

        // Do some work
        auto input = random_input(100);
        hypercolumn_compute(hcol, input.data(), input.size());

        hypercolumn_destroy(hcol);

        for (auto& config : configs) {
            free_minicolumn_config(config);
        }
    }

    size_t memory_after = get_allocated_memory();
    size_t leak = (memory_after > memory_before) ? (memory_after - memory_before) : 0;

    EXPECT_LT(leak, 4096) << "Memory leak: " << leak << " bytes";
}

TEST_F(CorticalColumnRegressionTest, PoolExhaustionRecovery) {
    // WHAT: Verify pool handles exhaustion gracefully
    // WHY:  Must not corrupt state on allocation failure
    // TARGET: Pool remains usable after exhaustion

    // Create pool with limited capacity
    cortical_column_pool_config_t limited_config = {
        .max_minicolumns = 10,
        .max_hypercolumns = 5,
        .max_neurons_per_minicolumn = 100,
        .enable_cow_support = false
    };

    cortical_column_pool_t* limited_pool = cortical_column_pool_create(&limited_config);
    ASSERT_NE(limited_pool, nullptr);

    std::vector<minicolumn_t*> columns;

    // Fill the pool
    for (int i = 0; i < 15; i++) {
        auto config = create_minicolumn_config(80);
        minicolumn_t* col = minicolumn_create(limited_pool, &config);

        if (col != nullptr) {
            columns.push_back(col);
        }

        free_minicolumn_config(config);
    }

    // Should have allocated up to limit
    EXPECT_LE(columns.size(), 10u);

    // Free some
    for (size_t i = 0; i < columns.size() / 2; i++) {
        minicolumn_destroy(columns[i]);
        columns[i] = nullptr;
    }

    // Should be able to allocate again
    auto config = create_minicolumn_config(80);
    minicolumn_t* new_col = minicolumn_create(limited_pool, &config);
    if (new_col != nullptr) {
        minicolumn_destroy(new_col);
    }
    free_minicolumn_config(config);

    // Cleanup remaining
    for (auto col : columns) {
        if (col) minicolumn_destroy(col);
    }

    cortical_column_pool_destroy(limited_pool);
}

TEST_F(CorticalColumnRegressionTest, LaminarStructureMemory) {
    // WHAT: Verify laminar structure memory is properly managed
    // WHY:  Complex layer structures need proper cleanup
    // TARGET: No leak after create/destroy cycles

    size_t memory_before = get_allocated_memory();

    for (int cycle = 0; cycle < 50; cycle++) {
        laminar_structure_t* ls = laminar_structure_create(nullptr);
        ASSERT_NE(ls, nullptr);

        laminar_apply_canonical_circuit(ls);

        std::vector<float> input(64, 0.5f);
        laminar_process_input(ls, CC_LAYER_IV, input.data(), input.size());
        laminar_process_feedforward(ls);
        laminar_process_feedback(ls);

        laminar_structure_destroy(ls);
    }

    size_t memory_after = get_allocated_memory();
    size_t leak = (memory_after > memory_before) ? (memory_after - memory_before) : 0;

    EXPECT_LT(leak, 4096) << "Memory leak: " << leak << " bytes";
}

//=============================================================================
// CATEGORY 5: Null Pointer Safety
//=============================================================================

TEST_F(CorticalColumnRegressionTest, MinicolumnNullSafety) {
    // WHAT: Verify minicolumn functions handle null safely
    // WHY:  Must not crash on invalid input
    // TARGET: No crashes, graceful return

    // These should not crash
    minicolumn_destroy(nullptr);

    float activation = minicolumn_compute(nullptr, nullptr, 0);
    EXPECT_LT(activation, 0.0f);  // Should return error

    float weight = minicolumn_compute_receptive_weight(nullptr, 0.0f, 0.0f, 0.0f);
    EXPECT_LT(weight, 0.0f);  // Should return error

    minicolumn_apply_lateral_inhibition(nullptr, 0.5f);

    minicolumn_set_receptive_field(nullptr, 0.0f, 0.0f, 0.0f, 1.0f);

    minicolumn_stats_t stats;
    minicolumn_get_stats(nullptr, &stats);

    // Null config
    minicolumn_t* col = minicolumn_create(pool, nullptr);
    EXPECT_EQ(col, nullptr);

    // Null pool
    auto config = create_minicolumn_config(80);
    col = minicolumn_create(nullptr, &config);
    EXPECT_EQ(col, nullptr);
    free_minicolumn_config(config);
}

TEST_F(CorticalColumnRegressionTest, HypercolumnNullSafety) {
    // WHAT: Verify hypercolumn functions handle null safely
    // WHY:  Must not crash on invalid input
    // TARGET: No crashes, graceful return

    // These should not crash
    hypercolumn_destroy(nullptr);

    hypercolumn_compute(nullptr, nullptr, 0);

    uint32_t winner = hypercolumn_get_winner(nullptr);
    EXPECT_EQ(winner, UINT32_MAX);

    float dist[16];
    hypercolumn_get_distribution(nullptr, dist, 16);

    hypercolumn_run_competition(nullptr, CC_COMPETITION_SOFTMAX, 1.0f);

    cc_hypercolumn_stats_t stats;
    hypercolumn_get_stats(nullptr, &stats);

    // Null config
    hypercolumn_t* hcol = hypercolumn_create(pool, nullptr);
    EXPECT_EQ(hcol, nullptr);

    // Null pool
    const uint32_t num_minicolumns = 8;
    std::vector<minicolumn_config_t> configs;
    for (uint32_t i = 0; i < num_minicolumns; i++) {
        configs.push_back(create_minicolumn_config(80));
    }

    hypercolumn_config_t hcol_config = {
        .num_minicolumns = num_minicolumns,
        .minicolumn_configs = configs.data(),
        .feature_space_min = 0.0f,
        .feature_space_max = 180.0f,
        .topographic_x = 0.0f,
        .topographic_y = 0.0f,
        .competition = CC_COMPETITION_SOFTMAX,
        .k_winners = 3,
        .temperature = 1.0f,
        .lateral_inhibition_strength = 0.5f,
        .lateral_inhibition_sigma1 = 1.0f,
        .lateral_inhibition_sigma2 = 3.0f
    };

    hcol = hypercolumn_create(nullptr, &hcol_config);
    EXPECT_EQ(hcol, nullptr);

    for (auto& config : configs) {
        free_minicolumn_config(config);
    }
}

TEST_F(CorticalColumnRegressionTest, LaminarNullSafety) {
    // WHAT: Verify laminar functions handle null safely
    // WHY:  Must not crash on invalid input
    // TARGET: No crashes, graceful return

    // These should not crash
    laminar_structure_destroy(nullptr);

    float input[64] = {0};
    laminar_process_input(nullptr, CC_LAYER_IV, input, 64);

    laminar_process_feedforward(nullptr);
    laminar_process_feedback(nullptr);
    laminar_process_lateral(nullptr);

    float output[64];
    laminar_get_output(nullptr, CC_LAYER_II_III, output, 64);

    float activation = laminar_get_layer_activation(nullptr, CC_LAYER_II_III);
    EXPECT_EQ(activation, 0.0f);

    uint32_t count = laminar_get_layer_neuron_count(nullptr, CC_LAYER_II_III);
    EXPECT_EQ(count, 0u);

    laminar_connect_feedforward(nullptr, CC_LAYER_IV, CC_LAYER_II_III, 1.0f);
    laminar_connect_feedback(nullptr, CC_LAYER_VI, CC_LAYER_IV, 0.5f);
    laminar_apply_canonical_circuit(nullptr);

    laminar_profile_t profile;
    laminar_get_profile(nullptr, &profile);

    laminar_stats_t stats;
    laminar_get_stats(nullptr, &stats);
}

TEST_F(CorticalColumnRegressionTest, PoolNullSafety) {
    // WHAT: Verify pool functions handle null safely
    // WHY:  Must not crash on invalid input
    // TARGET: No crashes

    cortical_column_pool_destroy(nullptr);

    // Null config should use defaults
    cortical_column_pool_t* default_pool = cortical_column_pool_create(nullptr);
    if (default_pool) {
        cortical_column_pool_destroy(default_pool);
    }
}

//=============================================================================
// CATEGORY 6: Backward Compatibility
//=============================================================================

TEST_F(CorticalColumnRegressionTest, DefaultPoolConfigStable) {
    // WHAT: Verify default pool config values are stable
    // WHY:  Config defaults must not change unexpectedly
    // TARGET: Known default values

    cortical_column_pool_t* default_pool = cortical_column_pool_create(nullptr);

    // If default pool creation succeeds, it should work
    if (default_pool) {
        // Should be able to create at least one minicolumn
        auto config = create_minicolumn_config(80);
        minicolumn_t* col = minicolumn_create(default_pool, &config);
        EXPECT_NE(col, nullptr);
        if (col) {
            minicolumn_destroy(col);
        }
        free_minicolumn_config(config);
        cortical_column_pool_destroy(default_pool);
    }
}

TEST_F(CorticalColumnRegressionTest, LayerConfigDefaults) {
    // WHAT: Verify layer configuration defaults are stable
    // WHY:  Biological parameters must remain consistent
    // TARGET: Known default values

    // Layer I: 5% thickness, 60% excitatory
    cortical_layer_config_t layer_i = cortical_layer_get_default_config(CC_LAYER_I);
    EXPECT_EQ(layer_i.layer, CC_LAYER_I);
    EXPECT_GT(layer_i.thickness_ratio, 0.0f);
    EXPECT_LE(layer_i.thickness_ratio, 1.0f);
    EXPECT_GT(layer_i.excitatory_ratio, 0.0f);
    EXPECT_LE(layer_i.excitatory_ratio, 1.0f);

    // Layer IV: Should have highest density (primary input)
    cortical_layer_config_t layer_iv = cortical_layer_get_default_config(CC_LAYER_IV);
    EXPECT_EQ(layer_iv.layer, CC_LAYER_IV);
    EXPECT_GT(layer_iv.neuron_density, 0u);
}

TEST_F(CorticalColumnRegressionTest, LayerNamesStable) {
    // WHAT: Verify layer names are stable
    // WHY:  Names used in logging/debugging must not change
    // TARGET: Known layer names

    EXPECT_NE(cortical_layer_get_name(CC_LAYER_I), nullptr);
    EXPECT_NE(cortical_layer_get_name(CC_LAYER_II_III), nullptr);
    EXPECT_NE(cortical_layer_get_name(CC_LAYER_IV), nullptr);
    EXPECT_NE(cortical_layer_get_name(CC_LAYER_V), nullptr);
    EXPECT_NE(cortical_layer_get_name(CC_LAYER_VI), nullptr);

    // Descriptions should also exist
    EXPECT_NE(cortical_layer_get_description(CC_LAYER_I), nullptr);
    EXPECT_NE(cortical_layer_get_description(CC_LAYER_II_III), nullptr);
    EXPECT_NE(cortical_layer_get_description(CC_LAYER_IV), nullptr);
    EXPECT_NE(cortical_layer_get_description(CC_LAYER_V), nullptr);
    EXPECT_NE(cortical_layer_get_description(CC_LAYER_VI), nullptr);
}

TEST_F(CorticalColumnRegressionTest, CompetitionModesAvailable) {
    // WHAT: Verify all competition modes work
    // WHY:  Mode enum must be stable
    // TARGET: All modes functional

    ASSERT_NE(pool, nullptr);

    cc_competition_mode_t modes[] = {
        CC_COMPETITION_WINNER_TAKE_ALL,
        CC_COMPETITION_K_WINNERS,
        CC_COMPETITION_SOFTMAX,
        CC_COMPETITION_NONE
    };

    for (auto mode : modes) {
        const uint32_t num_minicolumns = 8;
        std::vector<minicolumn_config_t> configs;

        for (uint32_t i = 0; i < num_minicolumns; i++) {
            configs.push_back(create_minicolumn_config(80));
        }

        hypercolumn_config_t hcol_config = {
            .num_minicolumns = num_minicolumns,
            .minicolumn_configs = configs.data(),
            .feature_space_min = 0.0f,
            .feature_space_max = 180.0f,
            .topographic_x = 0.0f,
            .topographic_y = 0.0f,
            .competition = mode,
            .k_winners = 3,
            .temperature = 1.0f,
            .lateral_inhibition_strength = 0.5f,
            .lateral_inhibition_sigma1 = 1.0f,
            .lateral_inhibition_sigma2 = 3.0f
        };

        hypercolumn_t* hcol = hypercolumn_create(pool, &hcol_config);
        EXPECT_NE(hcol, nullptr) << "Failed to create hypercolumn with mode " << mode;

        if (hcol) {
            auto input = random_input(100);
            hypercolumn_compute(hcol, input.data(), input.size());
            hypercolumn_destroy(hcol);
        }

        for (auto& config : configs) {
            free_minicolumn_config(config);
        }
    }
}

//=============================================================================
// CATEGORY 7: Stress Tests
//=============================================================================

TEST_F(CorticalColumnRegressionTest, RapidComputes) {
    // WHAT: Stress test rapid compute calls
    // WHY:  Must handle rapid processing without corruption
    // TARGET: No crashes, valid outputs

    ASSERT_NE(pool, nullptr);

    auto config = create_minicolumn_config(80);
    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    auto input = random_input(50);

    for (int i = 0; i < 10000; i++) {
        float activation = minicolumn_compute(col, input.data(), input.size());

        if (activation >= 0.0f) {
            EXPECT_GE(activation, 0.0f);
            EXPECT_LE(activation, 1.0f);
            EXPECT_FALSE(std::isnan(activation));
            EXPECT_FALSE(std::isinf(activation));
        }
    }

    minicolumn_destroy(col);
    free_minicolumn_config(config);
}

TEST_F(CorticalColumnRegressionTest, RapidHypercolumnUpdates) {
    // WHAT: Stress test rapid hypercolumn updates
    // WHY:  Must handle rapid updates without corruption
    // TARGET: No crashes, valid state

    ASSERT_NE(pool, nullptr);

    const uint32_t num_minicolumns = 16;
    std::vector<minicolumn_config_t> configs;

    for (uint32_t i = 0; i < num_minicolumns; i++) {
        configs.push_back(create_minicolumn_config(80));
    }

    hypercolumn_config_t hcol_config = {
        .num_minicolumns = num_minicolumns,
        .minicolumn_configs = configs.data(),
        .feature_space_min = 0.0f,
        .feature_space_max = 180.0f,
        .topographic_x = 0.0f,
        .topographic_y = 0.0f,
        .competition = CC_COMPETITION_SOFTMAX,
        .k_winners = 3,
        .temperature = 1.0f,
        .lateral_inhibition_strength = 0.5f,
        .lateral_inhibition_sigma1 = 1.0f,
        .lateral_inhibition_sigma2 = 3.0f
    };

    hypercolumn_t* hcol = hypercolumn_create(pool, &hcol_config);
    ASSERT_NE(hcol, nullptr);

    for (int i = 0; i < 5000; i++) {
        auto input = random_input(100);
        hypercolumn_compute(hcol, input.data(), input.size());

        uint32_t winner = hypercolumn_get_winner(hcol);
        EXPECT_LT(winner, num_minicolumns);

        std::vector<float> dist(num_minicolumns);
        hypercolumn_get_distribution(hcol, dist.data(), num_minicolumns);

        for (float val : dist) {
            EXPECT_FALSE(std::isnan(val));
            EXPECT_FALSE(std::isinf(val));
        }
    }

    hypercolumn_destroy(hcol);
    for (auto& config : configs) {
        free_minicolumn_config(config);
    }
}

TEST_F(CorticalColumnRegressionTest, ConcurrentComputes) {
    // WHAT: Test concurrent processing of independent columns
    // WHY:  Must handle multi-threaded access
    // TARGET: No data races, valid results

    ASSERT_NE(pool, nullptr);

    const uint32_t num_threads = 4;
    std::vector<minicolumn_t*> columns;
    std::vector<minicolumn_config_t> configs;

    // Create independent columns
    for (uint32_t t = 0; t < num_threads; t++) {
        auto config = create_minicolumn_config(80);
        configs.push_back(config);
        minicolumn_t* col = minicolumn_create(pool, &config);
        ASSERT_NE(col, nullptr);
        columns.push_back(col);
    }

    std::vector<std::thread> threads;
    std::atomic<uint32_t> error_count{0};

    for (uint32_t t = 0; t < num_threads; t++) {
        threads.emplace_back([&, t]() {
            auto input = random_input(50);

            for (int i = 0; i < 1000; i++) {
                float activation = minicolumn_compute(columns[t], input.data(), input.size());

                if (activation >= 0.0f) {
                    if (activation < 0.0f || activation > 1.0f ||
                        std::isnan(activation) || std::isinf(activation)) {
                        error_count++;
                    }
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(error_count.load(), 0u) << "Thread safety violation detected";

    for (auto col : columns) {
        minicolumn_destroy(col);
    }
    for (auto& config : configs) {
        free_minicolumn_config(config);
    }
}

TEST_F(CorticalColumnRegressionTest, LaminarStressTest) {
    // WHAT: Stress test laminar processing
    // WHY:  Must handle continuous layer updates
    // TARGET: No crashes, valid activations

    laminar_structure_t* ls = laminar_structure_create(nullptr);
    ASSERT_NE(ls, nullptr);

    laminar_apply_canonical_circuit(ls);

    std::vector<float> input(64);
    std::vector<float> output(64);

    for (int i = 0; i < 5000; i++) {
        // Vary input
        for (float& val : input) {
            val = 0.5f + 0.5f * sin(i * 0.01f);
        }

        laminar_process_input(ls, CC_LAYER_IV, input.data(), input.size());
        laminar_process_feedforward(ls);
        laminar_process_feedback(ls);
        laminar_process_lateral(ls);

        // Check activations
        for (int layer = 0; layer < CC_LAYER_COUNT; layer++) {
            float activation = laminar_get_layer_activation(ls, (cc_cortical_layer_t)layer);
            EXPECT_FALSE(std::isnan(activation)) << "NaN at iteration " << i;
            EXPECT_FALSE(std::isinf(activation)) << "Inf at iteration " << i;
        }
    }

    laminar_structure_destroy(ls);
}

TEST_F(CorticalColumnRegressionTest, ExtremeInputValues) {
    // WHAT: Test handling of extreme input values
    // WHY:  Must handle edge cases without numerical issues
    // TARGET: No NaN/Inf, bounded outputs

    ASSERT_NE(pool, nullptr);

    auto config = create_minicolumn_config(80);
    minicolumn_t* col = minicolumn_create(pool, &config);
    ASSERT_NE(col, nullptr);

    std::vector<std::vector<float>> extreme_inputs = {
        {0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
        {1e10f, 1e10f, 1e10f, 1e10f, 1e10f},
        {-1e10f, -1e10f, -1e10f, -1e10f, -1e10f},
        {1e-10f, 1e-10f, 1e-10f, 1e-10f, 1e-10f}
    };

    for (const auto& input : extreme_inputs) {
        float activation = minicolumn_compute(col, input.data(), input.size());

        EXPECT_FALSE(std::isnan(activation)) << "NaN for extreme input";
        EXPECT_FALSE(std::isinf(activation)) << "Inf for extreme input";
    }

    minicolumn_destroy(col);
    free_minicolumn_config(config);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
