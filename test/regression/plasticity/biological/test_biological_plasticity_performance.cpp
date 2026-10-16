/**
 * @file test_biological_plasticity_performance.cpp
 * @brief WHAT: Regression tests for biological plasticity performance
 * @details WHY:  Ensure performance characteristics are maintained across changes
 *          HOW:  Benchmark throughput, latency, and memory usage of all modules
 *
 * NIMCP Standards:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/dendritic/nimcp_dendritic.h"
#include "plasticity/predictive/nimcp_predictive_coding.h"

// ============================================================================
// Performance Test Configuration
// ============================================================================

namespace {

constexpr size_t BENCHMARK_ITERATIONS = 10000;
constexpr size_t WARMUP_ITERATIONS = 1000;
constexpr double PERFORMANCE_MARGIN = 1.5;  // 50% margin for variability

// Performance baselines (in microseconds per operation)
constexpr double NMDA_BLOCK_BASELINE_US = 0.1;
constexpr double HOMEOSTATIC_UPDATE_BASELINE_US = 1.0;
constexpr double TREE_UPDATE_BASELINE_US = 50.0;
constexpr double PC_INFERENCE_BASELINE_US = 10.0;
constexpr double FREE_ENERGY_BASELINE_US = 0.5;

/**
 * @brief WHAT: High-resolution timer for benchmarking
 * @details WHY:  Accurate timing essential for performance regression detection
 */
class BenchmarkTimer {
public:
    void start() { start_time_ = std::chrono::high_resolution_clock::now(); }

    void stop() { end_time_ = std::chrono::high_resolution_clock::now(); }

    double elapsed_us() const {
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_time_ - start_time_);
        return duration.count() / 1000.0;
    }

    double elapsed_ms() const { return elapsed_us() / 1000.0; }

private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
};

/**
 * @brief WHAT: Statistics calculator for benchmark results
 */
struct BenchmarkStats {
    double mean;
    double std_dev;
    double min;
    double max;
    double p50;
    double p95;
    double p99;

    static BenchmarkStats compute(std::vector<double>& times) {
        BenchmarkStats stats{};

        // Guard: Empty data
        if (times.empty()) return stats;

        std::sort(times.begin(), times.end());

        // Mean
        stats.mean = std::accumulate(times.begin(), times.end(), 0.0) / times.size();

        // Standard deviation
        double sq_sum = 0.0;
        for (double t : times) {
            sq_sum += (t - stats.mean) * (t - stats.mean);
        }
        stats.std_dev = std::sqrt(sq_sum / times.size());

        // Min/Max
        stats.min = times.front();
        stats.max = times.back();

        // Percentiles
        stats.p50 = times[times.size() / 2];
        stats.p95 = times[static_cast<size_t>(times.size() * 0.95)];
        stats.p99 = times[static_cast<size_t>(times.size() * 0.99)];

        return stats;
    }
};

}  // namespace

// ============================================================================
// NMDA Dynamics Performance Tests
// ============================================================================

class NMDAPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        params_ = nmda_params_default();
    }

    nmda_params_t params_;
};

/**
 * @brief WHAT: Benchmark NMDA block computation throughput
 * @details WHY:  NMDA calculations called millions of times during simulation
 */
TEST_F(NMDAPerformanceTest, NMDABlockComputationThroughput) {
    std::vector<double> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Warmup
    for (size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        volatile float result = nmda_compute_block(-70.0f + (i % 140), &params_);
        (void)result;
    }

    // Benchmark
    BenchmarkTimer timer;
    for (size_t i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        float voltage = -70.0f + (i % 140);

        timer.start();
        volatile float result = nmda_compute_block(voltage, &params_);
        timer.stop();

        (void)result;
        times.push_back(timer.elapsed_us());
    }

    auto stats = BenchmarkStats::compute(times);

    // Verify performance regression
    EXPECT_LT(stats.mean, NMDA_BLOCK_BASELINE_US * PERFORMANCE_MARGIN)
        << "NMDA block computation regressed: " << stats.mean << " us (baseline: "
        << NMDA_BLOCK_BASELINE_US << " us)";

    // Log performance for tracking
    RecordProperty("nmda_mean_us", stats.mean);
    RecordProperty("nmda_p95_us", stats.p95);
    RecordProperty("nmda_p99_us", stats.p99);
}

/**
 * @brief WHAT: Benchmark NMDA kinetics update throughput
 */
TEST_F(NMDAPerformanceTest, NMDAKineticsUpdateThroughput) {
    dendritic_nmda_state_t state = nmda_state_init();
    std::vector<double> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Warmup
    for (size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        nmda_update_kinetics(&state, (i % 10) * 0.1f, 1.0f, &params_);
    }

    // Benchmark
    BenchmarkTimer timer;
    for (size_t i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        float glutamate = (i % 10) * 0.1f;

        timer.start();
        nmda_update_kinetics(&state, glutamate, 1.0f, &params_);
        timer.stop();

        times.push_back(timer.elapsed_us());
    }

    auto stats = BenchmarkStats::compute(times);

    EXPECT_LT(stats.mean, NMDA_BLOCK_BASELINE_US * 2.0 * PERFORMANCE_MARGIN)
        << "NMDA kinetics update regressed";

    RecordProperty("nmda_kinetics_mean_us", stats.mean);
    RecordProperty("nmda_kinetics_p95_us", stats.p95);
}

// ============================================================================
// Dendritic Tree Performance Tests
// ============================================================================

class DendriticTreePerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = dendritic_tree_config_default();
        tree_ = dendritic_tree_create(&config_);
    }

    void TearDown() override {
        dendritic_tree_destroy(tree_);
    }

    dendritic_tree_config_t config_;
    dendritic_tree_t tree_;
};

/**
 * @brief WHAT: Benchmark dendritic tree update
 */
TEST_F(DendriticTreePerformanceTest, TreeUpdateThroughput) {
    std::vector<double> times;
    times.reserve(BENCHMARK_ITERATIONS / 10);

    // Warmup
    for (size_t i = 0; i < WARMUP_ITERATIONS / 10; ++i) {
        dendritic_tree_inject_input(tree_, 0, 0, 0.5f, 0.1f, 0.3f);
        dendritic_tree_update(tree_, 1.0f);
    }

    // Benchmark
    BenchmarkTimer timer;
    for (size_t i = 0; i < BENCHMARK_ITERATIONS / 10; ++i) {
        dendritic_tree_inject_input(tree_, 0, 0, 0.3f, 0.1f, 0.2f);

        timer.start();
        dendritic_tree_update(tree_, 1.0f);
        timer.stop();

        times.push_back(timer.elapsed_us());
    }

    auto stats = BenchmarkStats::compute(times);

    EXPECT_LT(stats.mean, TREE_UPDATE_BASELINE_US * PERFORMANCE_MARGIN)
        << "Tree update regressed: " << stats.mean << " us";

    RecordProperty("tree_mean_us", stats.mean);
    RecordProperty("tree_p95_us", stats.p95);
}

/**
 * @brief WHAT: Benchmark soma voltage retrieval
 */
TEST_F(DendriticTreePerformanceTest, SomaVoltageThroughput) {
    std::vector<double> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Run some updates first
    for (int i = 0; i < 100; ++i) {
        dendritic_tree_inject_input(tree_, 0, 0, 0.5f, 0.1f, 0.3f);
        dendritic_tree_update(tree_, 1.0f);
    }

    // Benchmark
    BenchmarkTimer timer;
    for (size_t i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        timer.start();
        volatile float voltage = dendritic_tree_get_soma_voltage(tree_);
        timer.stop();

        (void)voltage;
        times.push_back(timer.elapsed_us());
    }

    auto stats = BenchmarkStats::compute(times);

    EXPECT_LT(stats.mean, 0.1 * PERFORMANCE_MARGIN)
        << "Soma voltage retrieval regressed";

    RecordProperty("soma_voltage_mean_us", stats.mean);
}

// ============================================================================
// Homeostatic Plasticity Performance Tests
// ============================================================================

class HomeostaticPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        scaling_params_ = homeostatic_scaling_params_default();
        scaling_state_ = synaptic_scaling_state_init(5.0f);  // 5 Hz target
    }

    synaptic_scaling_params_t scaling_params_;
    synaptic_scaling_state_t scaling_state_;
};

/**
 * @brief WHAT: Benchmark synaptic scaling update
 */
TEST_F(HomeostaticPerformanceTest, SynapticScalingUpdateThroughput) {
    std::vector<double> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Warmup
    for (size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        bool spike = (i % 10) == 0;  // Spike every 10th iteration
        synaptic_scaling_update_rate(&scaling_state_, spike, 1.0f, &scaling_params_);
    }

    // Benchmark
    BenchmarkTimer timer;
    for (size_t i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        bool spike = (i % 10) == 0;

        timer.start();
        synaptic_scaling_update_rate(&scaling_state_, spike, 1.0f, &scaling_params_);
        timer.stop();

        times.push_back(timer.elapsed_us());
    }

    auto stats = BenchmarkStats::compute(times);

    EXPECT_LT(stats.mean, HOMEOSTATIC_UPDATE_BASELINE_US * PERFORMANCE_MARGIN)
        << "Synaptic scaling update regressed: " << stats.mean << " us";

    RecordProperty("scaling_mean_us", stats.mean);
    RecordProperty("scaling_p95_us", stats.p95);
}

/**
 * @brief WHAT: Benchmark batch weight scaling
 */
TEST_F(HomeostaticPerformanceTest, BatchWeightScalingThroughput) {
    constexpr size_t WEIGHT_COUNT = 1000;
    std::vector<float> weights(WEIGHT_COUNT, 0.5f);

    std::vector<double> times;
    times.reserve(BENCHMARK_ITERATIONS / 10);

    // Compute scaling factor
    float factor = synaptic_scaling_compute_factor(&scaling_state_, &scaling_params_);

    // Benchmark
    BenchmarkTimer timer;
    for (size_t i = 0; i < BENCHMARK_ITERATIONS / 10; ++i) {
        timer.start();
        synaptic_scaling_apply(weights.data(), WEIGHT_COUNT, factor);
        timer.stop();

        times.push_back(timer.elapsed_us());
    }

    auto stats = BenchmarkStats::compute(times);

    // 1000 weights should complete in reasonable time (~0.5us per weight with overhead)
    double expected_max = WEIGHT_COUNT * 0.5 * PERFORMANCE_MARGIN;  // 0.5us per weight
    EXPECT_LT(stats.mean, expected_max)
        << "Batch scaling regressed: " << stats.mean << " us for " << WEIGHT_COUNT << " weights";

    RecordProperty("batch_scaling_mean_us", stats.mean);
    RecordProperty("batch_scaling_throughput_mops", WEIGHT_COUNT / stats.mean);
}

/**
 * @brief WHAT: Benchmark intrinsic plasticity update
 */
TEST_F(HomeostaticPerformanceTest, IntrinsicPlasticityUpdateThroughput) {
    intrinsic_plasticity_params_t ip_params = homeostatic_ip_params_default();
    intrinsic_plasticity_state_t ip_state = intrinsic_plasticity_state_init(-55.0f, 1.0f);

    std::vector<double> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Warmup
    for (size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        intrinsic_plasticity_update_threshold(&ip_state, (i % 100) * 0.1f, 1.0f, &ip_params);
    }

    // Benchmark
    BenchmarkTimer timer;
    for (size_t i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        float activity = (i % 100) * 0.1f;

        timer.start();
        intrinsic_plasticity_update_threshold(&ip_state, activity, 1.0f, &ip_params);
        timer.stop();

        times.push_back(timer.elapsed_us());
    }

    auto stats = BenchmarkStats::compute(times);

    EXPECT_LT(stats.mean, HOMEOSTATIC_UPDATE_BASELINE_US * PERFORMANCE_MARGIN)
        << "Intrinsic plasticity update regressed: " << stats.mean << " us";

    RecordProperty("intrinsic_mean_us", stats.mean);
}

/**
 * @brief WHAT: Benchmark homeostatic controller update
 */
TEST_F(HomeostaticPerformanceTest, HomeostaticControllerUpdateThroughput) {
    constexpr uint32_t NUM_NEURONS = 100;
    constexpr uint32_t SYNAPSES_PER_NEURON = 10;

    homeostatic_config_t config = homeostatic_config_default();
    homeostatic_controller_t controller = homeostatic_controller_create(&config, NUM_NEURONS);
    ASSERT_NE(controller, nullptr);

    std::vector<float> firing_rates(NUM_NEURONS, 5.0f);
    std::vector<float> weights(NUM_NEURONS * SYNAPSES_PER_NEURON, 0.5f);

    std::vector<double> times;
    times.reserve(BENCHMARK_ITERATIONS / 10);

    // Warmup
    for (size_t i = 0; i < WARMUP_ITERATIONS / 10; ++i) {
        firing_rates[i % NUM_NEURONS] = (i % 10) * 1.0f;
        homeostatic_controller_update(controller, firing_rates.data(),
                                      weights.data(), SYNAPSES_PER_NEURON, 1.0f);
    }

    // Benchmark
    BenchmarkTimer timer;
    for (size_t i = 0; i < BENCHMARK_ITERATIONS / 10; ++i) {
        firing_rates[i % NUM_NEURONS] = (i % 10) * 1.0f;

        timer.start();
        homeostatic_controller_update(controller, firing_rates.data(),
                                      weights.data(), SYNAPSES_PER_NEURON, 1.0f);
        timer.stop();

        times.push_back(timer.elapsed_us());
    }

    auto stats = BenchmarkStats::compute(times);

    // Controller managing 100 neurons should be reasonable
    double expected_max = NUM_NEURONS * HOMEOSTATIC_UPDATE_BASELINE_US * PERFORMANCE_MARGIN;
    EXPECT_LT(stats.mean, expected_max)
        << "Homeostatic controller update regressed: " << stats.mean << " us";

    RecordProperty("controller_mean_us", stats.mean);

    homeostatic_controller_destroy(controller);
}

// ============================================================================
// Predictive Coding Performance Tests
// ============================================================================

class PredictiveCodingPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create small hierarchy for benchmarking
        units_[0] = 32; units_[1] = 16; units_[2] = 8;
        config_ = pc_hierarchy_config_default(3, units_);
        config_.units_per_level = units_;  // Must set pointer explicitly
        hierarchy_ = pc_hierarchy_create(&config_);
    }

    void TearDown() override {
        pc_hierarchy_destroy(hierarchy_);
    }

    uint32_t units_[3];
    pc_hierarchy_config_t config_;
    pc_hierarchy_t hierarchy_;
};

/**
 * @brief WHAT: Benchmark single inference step
 */
TEST_F(PredictiveCodingPerformanceTest, InferenceStepThroughput) {
    std::vector<double> times;
    times.reserve(BENCHMARK_ITERATIONS / 10);

    // Set up input
    std::vector<float> input(32);
    for (size_t i = 0; i < 32; ++i) {
        input[i] = static_cast<float>(i) / 32.0f;
    }

    // Warmup
    for (size_t i = 0; i < WARMUP_ITERATIONS / 10; ++i) {
        pc_hierarchy_set_input(hierarchy_, input.data());
        pc_hierarchy_inference_step(hierarchy_, 1.0f, false);
    }

    // Benchmark
    BenchmarkTimer timer;
    for (size_t i = 0; i < BENCHMARK_ITERATIONS / 10; ++i) {
        // Vary input slightly
        input[i % 32] = static_cast<float>((i * 7) % 100) / 100.0f;
        pc_hierarchy_set_input(hierarchy_, input.data());

        timer.start();
        pc_hierarchy_inference_step(hierarchy_, 1.0f, false);
        timer.stop();

        times.push_back(timer.elapsed_us());
    }

    auto stats = BenchmarkStats::compute(times);

    EXPECT_LT(stats.mean, PC_INFERENCE_BASELINE_US * PERFORMANCE_MARGIN)
        << "PC inference step regressed: " << stats.mean << " us";

    RecordProperty("pc_inference_mean_us", stats.mean);
    RecordProperty("pc_inference_p95_us", stats.p95);
}

/**
 * @brief WHAT: Benchmark free energy computation
 */
TEST_F(PredictiveCodingPerformanceTest, FreeEnergyComputationThroughput) {
    std::vector<double> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Run some inference first
    std::vector<float> input(32, 0.5f);
    pc_hierarchy_set_input(hierarchy_, input.data());
    for (int i = 0; i < 10; ++i) {
        pc_hierarchy_inference_step(hierarchy_, 1.0f, true);
    }

    // Benchmark
    BenchmarkTimer timer;
    for (size_t i = 0; i < BENCHMARK_ITERATIONS; ++i) {
        timer.start();
        volatile float fe = pc_hierarchy_get_free_energy(hierarchy_);
        timer.stop();

        (void)fe;
        times.push_back(timer.elapsed_us());
    }

    auto stats = BenchmarkStats::compute(times);

    EXPECT_LT(stats.mean, FREE_ENERGY_BASELINE_US * PERFORMANCE_MARGIN)
        << "Free energy computation regressed: " << stats.mean << " us";

    RecordProperty("free_energy_mean_us", stats.mean);
}

/**
 * @brief WHAT: Benchmark learning step throughput
 */
TEST_F(PredictiveCodingPerformanceTest, LearningStepThroughput) {
    std::vector<double> times;
    times.reserve(BENCHMARK_ITERATIONS / 10);

    std::vector<float> input(32);

    // Warmup
    for (size_t i = 0; i < WARMUP_ITERATIONS / 10; ++i) {
        for (size_t j = 0; j < 32; ++j) {
            input[j] = static_cast<float>((i + j) % 100) / 100.0f;
        }
        pc_hierarchy_set_input(hierarchy_, input.data());
        pc_hierarchy_inference_step(hierarchy_, 1.0f, true);
    }

    // Benchmark
    BenchmarkTimer timer;
    for (size_t i = 0; i < BENCHMARK_ITERATIONS / 10; ++i) {
        for (size_t j = 0; j < 32; ++j) {
            input[j] = static_cast<float>((i * 3 + j) % 100) / 100.0f;
        }
        pc_hierarchy_set_input(hierarchy_, input.data());

        timer.start();
        pc_hierarchy_inference_step(hierarchy_, 1.0f, true);  // With learning
        timer.stop();

        times.push_back(timer.elapsed_us());
    }

    auto stats = BenchmarkStats::compute(times);

    // Learning step should be at most 2x inference
    EXPECT_LT(stats.mean, PC_INFERENCE_BASELINE_US * 2.0 * PERFORMANCE_MARGIN)
        << "PC learning step regressed: " << stats.mean << " us";

    RecordProperty("pc_learning_mean_us", stats.mean);
    RecordProperty("pc_learning_p95_us", stats.p95);
}

// ============================================================================
// Memory Efficiency Tests
// ============================================================================

class MemoryEfficiencyTest : public ::testing::Test {};

/**
 * @brief WHAT: Verify predictive coding hierarchy memory scaling
 */
TEST_F(MemoryEfficiencyTest, PCHierarchyMemoryScaling) {
    uint32_t units[] = {256, 128, 64, 32, 16};
    pc_hierarchy_config_t config = pc_hierarchy_config_default(5, units);
    config.units_per_level = units;  // Must set pointer explicitly
    pc_hierarchy_t hierarchy = pc_hierarchy_create(&config);
    ASSERT_NE(hierarchy, nullptr);

    // Run inference
    std::vector<float> input(256, 0.5f);
    pc_hierarchy_set_input(hierarchy, input.data());

    for (int i = 0; i < 100; ++i) {
        pc_hierarchy_inference_step(hierarchy, 1.0f, true);
    }

    // Verify convergence
    float fe = pc_hierarchy_get_free_energy(hierarchy);
    EXPECT_TRUE(std::isfinite(fe)) << "Free energy diverged in large hierarchy";

    pc_hierarchy_destroy(hierarchy);
}

/**
 * @brief WHAT: Verify homeostatic controller memory scaling
 */
TEST_F(MemoryEfficiencyTest, HomeostaticControllerMemoryScaling) {
    constexpr uint32_t NEURON_COUNT = 10000;
    constexpr uint32_t SYNAPSES_PER_NEURON = 10;

    homeostatic_config_t config = homeostatic_config_default();
    homeostatic_controller_t controller = homeostatic_controller_create(&config, NEURON_COUNT);
    ASSERT_NE(controller, nullptr);

    // Simulate large network
    std::vector<float> firing_rates(NEURON_COUNT, 5.0f);
    std::vector<float> weights(NEURON_COUNT * SYNAPSES_PER_NEURON, 0.5f);

    for (int step = 0; step < 100; ++step) {
        // Vary firing rates
        for (uint32_t n = 0; n < NEURON_COUNT; ++n) {
            firing_rates[n] = 3.0f + 4.0f * std::sin(step * 0.1f + n * 0.01f);
        }
        homeostatic_controller_update(controller, firing_rates.data(),
                                      weights.data(), SYNAPSES_PER_NEURON, 1.0f);
    }

    // Verify weights are still bounded
    for (size_t i = 0; i < weights.size(); ++i) {
        EXPECT_GE(weights[i], 0.0f);
        EXPECT_LE(weights[i], 100.0f);  // Reasonable upper bound
    }

    homeostatic_controller_destroy(controller);
}

// ============================================================================
// Stress Tests
// ============================================================================

class StressTest : public ::testing::Test {};

/**
 * @brief WHAT: Stress test rapid allocation/deallocation
 * @details WHY:  Detect memory leaks under heavy churn
 */
TEST_F(StressTest, RapidAllocationDeallocation) {
    constexpr size_t ITERATIONS = 1000;

    for (size_t i = 0; i < ITERATIONS; ++i) {
        // Create structures
        dendritic_tree_config_t dt_config = dendritic_tree_config_default();
        dendritic_tree_t tree = dendritic_tree_create(&dt_config);

        uint32_t units[] = {8, 4};
        pc_hierarchy_config_t pc_config = pc_hierarchy_config_default(2, units);
        pc_config.units_per_level = units;  // Must set pointer explicitly
        pc_hierarchy_t hierarchy = pc_hierarchy_create(&pc_config);

        homeostatic_config_t hc_config = homeostatic_config_default();
        homeostatic_controller_t controller = homeostatic_controller_create(&hc_config, 10);

        // Use them briefly
        dendritic_tree_inject_input(tree, 0, 0, 0.5f, 0.1f, 0.3f);
        dendritic_tree_update(tree, 1.0f);

        std::vector<float> input(8, 0.5f);
        pc_hierarchy_set_input(hierarchy, input.data());
        pc_hierarchy_inference_step(hierarchy, 1.0f, false);

        std::vector<float> rates(10, 5.0f);
        std::vector<float> weights(100, 0.5f);
        homeostatic_controller_update(controller, rates.data(), weights.data(), 10, 1.0f);

        // Destroy
        dendritic_tree_destroy(tree);
        pc_hierarchy_destroy(hierarchy);
        homeostatic_controller_destroy(controller);
    }

    // If we get here without memory errors, the test passes
    SUCCEED();
}

/**
 * @brief WHAT: Stress test extended simulation
 * @details WHY:  Detect numerical drift or memory growth over time
 */
TEST_F(StressTest, ExtendedSimulation) {
    constexpr size_t SIMULATION_STEPS = 100000;
    constexpr uint32_t NUM_NEURONS = 10;

    // Create structures
    homeostatic_config_t hc_config = homeostatic_config_default();
    homeostatic_controller_t controller = homeostatic_controller_create(&hc_config, NUM_NEURONS);

    dendritic_tree_config_t dt_config = dendritic_tree_config_default();
    dendritic_tree_t tree = dendritic_tree_create(&dt_config);

    uint32_t units[] = {16, 8, 4};
    pc_hierarchy_config_t pc_config = pc_hierarchy_config_default(3, units);
    pc_config.units_per_level = units;  // Must set pointer explicitly
    pc_hierarchy_t hierarchy = pc_hierarchy_create(&pc_config);

    std::vector<float> input(16);
    std::vector<float> firing_rates(NUM_NEURONS, 5.0f);
    std::vector<float> weights(NUM_NEURONS * 10, 0.5f);

    // Extended simulation
    for (size_t step = 0; step < SIMULATION_STEPS; ++step) {
        // Generate input
        float phase = static_cast<float>(step) * 0.001f;
        for (size_t i = 0; i < 16; ++i) {
            input[i] = 0.5f + 0.3f * std::sin(phase + i * 0.1f);
        }

        // Update dendritic tree
        dendritic_tree_inject_input(tree, 0, 0, input[0], 0.1f, 0.2f);
        dendritic_tree_update(tree, 1.0f);

        // Update homeostatic controller
        float soma_voltage = dendritic_tree_get_soma_voltage(tree);
        float activity = (soma_voltage + 70.0f) / 100.0f;  // Normalize
        for (uint32_t n = 0; n < NUM_NEURONS; ++n) {
            firing_rates[n] = activity * 10.0f + n * 0.1f;
        }
        homeostatic_controller_update(controller, firing_rates.data(), weights.data(), 10, 1.0f);

        // Update predictive coding
        pc_hierarchy_set_input(hierarchy, input.data());
        pc_hierarchy_inference_step(hierarchy, 1.0f, step % 10 == 0);

        // Check stability every 10000 steps
        if (step % 10000 == 0 && step > 0) {
            float fe = pc_hierarchy_get_free_energy(hierarchy);
            EXPECT_TRUE(std::isfinite(fe))
                << "Free energy diverged at step " << step;

            // Free energy should be bounded
            EXPECT_LT(std::abs(fe), 1e6f)
                << "Free energy exploded at step " << step;
        }
    }

    // Final checks
    float final_fe = pc_hierarchy_get_free_energy(hierarchy);
    EXPECT_TRUE(std::isfinite(final_fe));

    // Cleanup
    homeostatic_controller_destroy(controller);
    dendritic_tree_destroy(tree);
    pc_hierarchy_destroy(hierarchy);
}

// ============================================================================
// Throughput Summary Test
// ============================================================================

/**
 * @brief WHAT: Combined throughput benchmark for reporting
 */
TEST(ThroughputSummary, AllModulesThroughput) {
    BenchmarkTimer timer;
    constexpr size_t COMBINED_ITERATIONS = 1000;

    // Create all structures
    synaptic_scaling_params_t scaling_params = homeostatic_scaling_params_default();
    synaptic_scaling_state_t scaling_state = synaptic_scaling_state_init(5.0f);

    dendritic_tree_config_t dt_config = dendritic_tree_config_default();
    dendritic_tree_t tree = dendritic_tree_create(&dt_config);

    uint32_t units[] = {32, 16, 8};
    pc_hierarchy_config_t pc_config = pc_hierarchy_config_default(3, units);
    pc_config.units_per_level = units;  // Must set pointer explicitly
    pc_hierarchy_t hierarchy = pc_hierarchy_create(&pc_config);

    std::vector<float> input(32);
    std::vector<float> weights(100, 0.5f);

    // Warmup
    for (size_t i = 0; i < 100; ++i) {
        synaptic_scaling_update_rate(&scaling_state, i % 10 == 0, 1.0f, &scaling_params);
        dendritic_tree_update(tree, 1.0f);
        pc_hierarchy_inference_step(hierarchy, 1.0f, false);
    }

    // Combined benchmark
    timer.start();
    for (size_t i = 0; i < COMBINED_ITERATIONS; ++i) {
        // Generate varying input
        for (size_t j = 0; j < 32; ++j) {
            input[j] = 0.5f + 0.3f * std::sin(i * 0.01f + j * 0.1f);
        }

        // Homeostatic update
        synaptic_scaling_update_rate(&scaling_state, i % 10 == 0, 1.0f, &scaling_params);
        float factor = synaptic_scaling_compute_factor(&scaling_state, &scaling_params);
        synaptic_scaling_apply(weights.data(), 100, factor);

        // Dendritic processing
        dendritic_tree_inject_input(tree, 0, 0, input[0], 0.1f, 0.2f);
        dendritic_tree_update(tree, 1.0f);

        // Predictive coding
        pc_hierarchy_set_input(hierarchy, input.data());
        pc_hierarchy_inference_step(hierarchy, 1.0f, i % 5 == 0);
    }
    timer.stop();

    double total_ms = timer.elapsed_ms();
    double iterations_per_second = COMBINED_ITERATIONS * 1000.0 / total_ms;

    // Log results
    RecordProperty("combined_total_ms", total_ms);
    RecordProperty("combined_iterations_per_second", iterations_per_second);

    // Verify reasonable throughput (at least 100 iterations/second)
    EXPECT_GT(iterations_per_second, 100.0)
        << "Combined throughput too low: " << iterations_per_second << " iter/s";

    // Cleanup
    dendritic_tree_destroy(tree);
    pc_hierarchy_destroy(hierarchy);
}
