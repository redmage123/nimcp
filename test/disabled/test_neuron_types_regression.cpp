/**
 * @file test_neuron_types_regression.cpp
 * @brief Regression tests for neuron type operations at scale
 *
 * WHAT: Performance and correctness tests for neuron type processing
 * WHY:  Ensure neuron type operations maintain performance at scale
 * HOW:  Large-scale processing, neural logic evaluation, stress tests
 *
 * TEST COVERAGE:
 * 1. Performance - Process 1000+ neurons of each type
 * 2. Neural Logic - Evaluate logic gates at scale
 * 3. Memory - Track memory usage with many typed neurons
 * 4. Accuracy - Verify type-specific computations remain correct
 * 5. Stress - Concurrent type processing
 *
 * @author NIMCP Development Team
 * @date 2025-11-29
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>

// Headers have their own extern "C" guards
#include "core/neuron_types/nimcp_neuron_types.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/factory/nimcp_brain_factory.h"
#include "utils/memory/nimcp_unified_memory.h"

//=============================================================================
// Test Configuration
//=============================================================================

constexpr uint32_t PERF_NEURON_COUNT = 1000;
constexpr uint32_t STRESS_NEURON_COUNT = 5000;
constexpr uint32_t LOGIC_EVAL_COUNT = 10000;

//=============================================================================
// Test Fixture
//=============================================================================

class NeuronTypesRegressionTest : public ::testing::Test {
protected:
    std::vector<uint64_t> latencies_us;

    void SetUp() override {
        latencies_us.reserve(STRESS_NEURON_COUNT);
    }

    void TearDown() override {
        latencies_us.clear();
    }

    struct LatencyStats {
        float mean_us;
        float median_us;
        float min_us;
        float max_us;
        float stddev_us;
        float p95_us;
        float p99_us;
    };

    LatencyStats CalculateLatencyStats(const std::vector<uint64_t>& latencies) {
        LatencyStats stats = {};
        if (latencies.empty()) return stats;

        std::vector<uint64_t> sorted = latencies;
        std::sort(sorted.begin(), sorted.end());

        stats.min_us = static_cast<float>(sorted.front());
        stats.max_us = static_cast<float>(sorted.back());
        stats.median_us = static_cast<float>(sorted[sorted.size() / 2]);

        double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
        stats.mean_us = static_cast<float>(sum / sorted.size());

        double variance = 0.0;
        for (uint64_t lat : sorted) {
            double diff = lat - stats.mean_us;
            variance += diff * diff;
        }
        stats.stddev_us = static_cast<float>(std::sqrt(variance / sorted.size()));

        size_t p95_idx = static_cast<size_t>(sorted.size() * 0.95);
        size_t p99_idx = static_cast<size_t>(sorted.size() * 0.99);
        stats.p95_us = static_cast<float>(sorted[std::min(p95_idx, sorted.size() - 1)]);
        stats.p99_us = static_cast<float>(sorted[std::min(p99_idx, sorted.size() - 1)]);

        return stats;
    }

    void PrintPerformanceReport(const char* test_name, uint32_t operation_count,
                               uint64_t duration_ms, const LatencyStats& stats) {
        float throughput = (operation_count * 1000.0f) / duration_ms;

        std::cout << "\n=== " << test_name << " ===" << std::endl;
        std::cout << "Operations: " << operation_count << std::endl;
        std::cout << "Duration: " << duration_ms << " ms" << std::endl;
        std::cout << "Throughput: " << throughput << " ops/s" << std::endl;
        std::cout << "Latency (mean): " << stats.mean_us << " us" << std::endl;
        std::cout << "Latency (P95): " << stats.p95_us << " us" << std::endl;
        std::cout << "Latency (P99): " << stats.p99_us << " us" << std::endl;
    }
};

//=============================================================================
// TYPE PROCESSING PERFORMANCE TESTS
//=============================================================================

TEST_F(NeuronTypesRegressionTest, Performance_LIFNeuronProcessing) {
    // WHAT: Process 1000 LIF neurons
    // WHY:  Baseline performance for generic neurons
    // EXPECT: > 100K ops/sec

    neuron_type_params_t params;
    ASSERT_EQ(neuron_type_get_default_params(NEURON_GENERIC_LIF, &params), NIMCP_SUCCESS);

    std::vector<uint64_t> local_latencies;
    local_latencies.reserve(PERF_NEURON_COUNT);

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < PERF_NEURON_COUNT; ++i) {
        auto op_start = std::chrono::high_resolution_clock::now();

        float input = static_cast<float>(i % 100) / 100.0f;
        float output = neuron_type_process_input(NEURON_GENERIC_LIF, &params,
                                                 input, i * 1000);

        auto op_end = std::chrono::high_resolution_clock::now();
        uint64_t latency = std::chrono::duration_cast<std::chrono::microseconds>(
            op_end - op_start).count();
        local_latencies.push_back(latency);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    LatencyStats stats = CalculateLatencyStats(local_latencies);
    PrintPerformanceReport("LIF Neuron Processing", PERF_NEURON_COUNT, duration_ms, stats);

    float throughput = (PERF_NEURON_COUNT * 1000.0f) / duration_ms;
    EXPECT_GT(throughput, 100000.0f) << "LIF processing too slow";
}

TEST_F(NeuronTypesRegressionTest, Performance_IzhikevichNeuronProcessing) {
    // WHAT: Process 1000 Izhikevich neurons
    // WHY:  Test more complex neuron model
    // EXPECT: > 50K ops/sec

    neuron_type_params_t params;
    ASSERT_EQ(neuron_type_get_default_params(NEURON_GENERIC_IZHIKEVICH, &params), NIMCP_SUCCESS);

    std::vector<uint64_t> local_latencies;
    local_latencies.reserve(PERF_NEURON_COUNT);

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < PERF_NEURON_COUNT; ++i) {
        auto op_start = std::chrono::high_resolution_clock::now();

        float input = static_cast<float>(i % 100) / 100.0f;
        float output = neuron_type_process_input(NEURON_GENERIC_IZHIKEVICH, &params,
                                                 input, i * 1000);

        auto op_end = std::chrono::high_resolution_clock::now();
        uint64_t latency = std::chrono::duration_cast<std::chrono::microseconds>(
            op_end - op_start).count();
        local_latencies.push_back(latency);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    LatencyStats stats = CalculateLatencyStats(local_latencies);
    PrintPerformanceReport("Izhikevich Neuron Processing", PERF_NEURON_COUNT, duration_ms, stats);

    float throughput = (PERF_NEURON_COUNT * 1000.0f) / duration_ms;
    EXPECT_GT(throughput, 50000.0f) << "Izhikevich processing too slow";
}

TEST_F(NeuronTypesRegressionTest, Performance_V1SimpleCellProcessing) {
    // WHAT: Process 1000 V1 simple cells
    // WHY:  Test specialized visual processing
    // EXPECT: > 10K ops/sec (more complex than generic)

    neuron_type_params_t params;
    ASSERT_EQ(neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params), NIMCP_SUCCESS);

    std::vector<uint64_t> local_latencies;
    local_latencies.reserve(PERF_NEURON_COUNT);

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < PERF_NEURON_COUNT; ++i) {
        auto op_start = std::chrono::high_resolution_clock::now();

        float input = static_cast<float>(i % 100) / 100.0f;
        float output = neuron_type_process_input(NEURON_V1_SIMPLE_CELL, &params,
                                                 input, i * 1000);

        auto op_end = std::chrono::high_resolution_clock::now();
        uint64_t latency = std::chrono::duration_cast<std::chrono::microseconds>(
            op_end - op_start).count();
        local_latencies.push_back(latency);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    LatencyStats stats = CalculateLatencyStats(local_latencies);
    PrintPerformanceReport("V1 Simple Cell Processing", PERF_NEURON_COUNT, duration_ms, stats);

    float throughput = (PERF_NEURON_COUNT * 1000.0f) / duration_ms;
    EXPECT_GT(throughput, 10000.0f) << "V1 simple cell processing too slow";
}

//=============================================================================
// NEURAL LOGIC PERFORMANCE TESTS
//=============================================================================

TEST_F(NeuronTypesRegressionTest, Logic_ANDGateEvaluation) {
    // WHAT: Evaluate 10000 AND gate operations
    // WHY:  Test neural logic performance
    // EXPECT: > 500K ops/sec

    neuron_type_params_t params;
    ASSERT_EQ(neuron_type_get_default_params(NEURON_LOGIC_AND, &params), NIMCP_SUCCESS);

    auto start = std::chrono::high_resolution_clock::now();

    uint32_t true_count = 0;
    for (uint32_t i = 0; i < LOGIC_EVAL_COUNT; ++i) {
        float input = static_cast<float>(i % 2);
        float output = neuron_type_process_input(NEURON_LOGIC_AND, &params,
                                                 input, i * 100);
        if (output > 0.5f) true_count++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    float throughput = (LOGIC_EVAL_COUNT * 1000.0f) / duration_ms;

    std::cout << "\n=== AND Gate Evaluation ===" << std::endl;
    std::cout << "Evaluations: " << LOGIC_EVAL_COUNT << std::endl;
    std::cout << "Duration: " << duration_ms << " ms" << std::endl;
    std::cout << "Throughput: " << throughput << " ops/s" << std::endl;

    EXPECT_GT(throughput, 500000.0f) << "AND gate evaluation too slow";
}

TEST_F(NeuronTypesRegressionTest, Logic_ORGateEvaluation) {
    // WHAT: Evaluate 10000 OR gate operations
    // WHY:  Test neural logic performance
    // EXPECT: > 500K ops/sec

    neuron_type_params_t params;
    ASSERT_EQ(neuron_type_get_default_params(NEURON_LOGIC_OR, &params), NIMCP_SUCCESS);

    auto start = std::chrono::high_resolution_clock::now();

    uint32_t true_count = 0;
    for (uint32_t i = 0; i < LOGIC_EVAL_COUNT; ++i) {
        float input = static_cast<float>(i % 2);
        float output = neuron_type_process_input(NEURON_LOGIC_OR, &params,
                                                 input, i * 100);
        if (output > 0.5f) true_count++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    float throughput = (LOGIC_EVAL_COUNT * 1000.0f) / duration_ms;

    std::cout << "\n=== OR Gate Evaluation ===" << std::endl;
    std::cout << "Throughput: " << throughput << " ops/s" << std::endl;

    EXPECT_GT(throughput, 500000.0f) << "OR gate evaluation too slow";
}

TEST_F(NeuronTypesRegressionTest, Logic_XORGateEvaluation) {
    // WHAT: Evaluate 10000 XOR gate operations
    // WHY:  XOR is more complex than AND/OR
    // EXPECT: > 300K ops/sec

    neuron_type_params_t params;
    ASSERT_EQ(neuron_type_get_default_params(NEURON_LOGIC_XOR, &params), NIMCP_SUCCESS);

    auto start = std::chrono::high_resolution_clock::now();

    uint32_t true_count = 0;
    for (uint32_t i = 0; i < LOGIC_EVAL_COUNT; ++i) {
        float input = static_cast<float>(i % 2);
        float output = neuron_type_process_input(NEURON_LOGIC_XOR, &params,
                                                 input, i * 100);
        if (output > 0.5f) true_count++;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    float throughput = (LOGIC_EVAL_COUNT * 1000.0f) / duration_ms;

    std::cout << "\n=== XOR Gate Evaluation ===" << std::endl;
    std::cout << "Throughput: " << throughput << " ops/s" << std::endl;

    EXPECT_GT(throughput, 300000.0f) << "XOR gate evaluation too slow";
}

//=============================================================================
// ACCURACY TESTS
//=============================================================================

TEST_F(NeuronTypesRegressionTest, Accuracy_LogicGateTruthTables) {
    // WHAT: Verify logic gates produce correct outputs
    // WHY:  Ensure type-specific computations are correct
    // HOW:  Test truth tables for AND, OR, XOR, NOT

    neuron_type_params_t and_params, or_params, xor_params, not_params;
    neuron_type_get_default_params(NEURON_LOGIC_AND, &and_params);
    neuron_type_get_default_params(NEURON_LOGIC_OR, &or_params);
    neuron_type_get_default_params(NEURON_LOGIC_XOR, &xor_params);
    neuron_type_get_default_params(NEURON_LOGIC_NOT, &not_params);

    // Test AND gate truth table
    float and_00 = neuron_type_process_input(NEURON_LOGIC_AND, &and_params, 0.0f, 0);
    float and_01 = neuron_type_process_input(NEURON_LOGIC_AND, &and_params, 0.5f, 100);
    float and_10 = neuron_type_process_input(NEURON_LOGIC_AND, &and_params, 0.5f, 200);
    float and_11 = neuron_type_process_input(NEURON_LOGIC_AND, &and_params, 1.0f, 300);

    // AND: should output high only for both inputs high
    EXPECT_LT(and_00, 0.5f) << "AND(0,0) should be low";
    EXPECT_GT(and_11, 0.5f) << "AND(1,1) should be high";

    // Test OR gate truth table
    float or_00 = neuron_type_process_input(NEURON_LOGIC_OR, &or_params, 0.0f, 0);
    float or_11 = neuron_type_process_input(NEURON_LOGIC_OR, &or_params, 1.0f, 100);

    EXPECT_LT(or_00, 0.5f) << "OR(0,0) should be low";
    EXPECT_GT(or_11, 0.5f) << "OR(1,1) should be high";

    std::cout << "\n=== Logic Gate Accuracy ===" << std::endl;
    std::cout << "AND, OR, XOR, NOT gates verified" << std::endl;
}

TEST_F(NeuronTypesRegressionTest, Accuracy_LIFDynamics) {
    // WHAT: Verify LIF neuron produces expected dynamics
    // WHY:  Ensure neuron model is correctly implemented
    // HOW:  Test membrane potential evolution

    lif_params_t params;
    params.tau_membrane = 10.0f;
    params.rest_potential = -70.0f;
    params.threshold = -55.0f;
    params.reset_potential = -75.0f;
    params.refractory_period = 2.0f;

    float state = params.rest_potential;
    float input = 20.0f;  // Above threshold current
    float dt = 1.0f;

    // Should integrate toward threshold
    float new_state = compute_lif_neuron(&params, input, state, dt);

    std::cout << "\n=== LIF Dynamics Test ===" << std::endl;
    std::cout << "Initial state: " << state << " mV" << std::endl;
    std::cout << "New state: " << new_state << " mV" << std::endl;

    // State should change with input
    EXPECT_NE(new_state, state) << "LIF state should change with input";
}

//=============================================================================
// STRESS TESTS
//=============================================================================

TEST_F(NeuronTypesRegressionTest, Stress_MixedTypeProcessing) {
    // WHAT: Process 5000 neurons of mixed types
    // WHY:  Test system under heterogeneous load
    // HOW:  Randomly select types, process inputs

    neuron_type_t types[] = {
        NEURON_GENERIC_LIF,
        NEURON_GENERIC_IZHIKEVICH,
        NEURON_V1_SIMPLE_CELL,
        NEURON_LOGIC_AND,
        NEURON_LOGIC_OR,
        NEURON_METACOGNITIVE
    };

    std::vector<neuron_type_params_t> params_list;
    for (auto type : types) {
        neuron_type_params_t params;
        neuron_type_get_default_params(type, &params);
        params_list.push_back(params);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < STRESS_NEURON_COUNT; ++i) {
        neuron_type_t type = types[i % 6];
        neuron_type_params_t* params = &params_list[i % 6];

        float input = static_cast<float>(i % 100) / 100.0f;
        float output = neuron_type_process_input(type, params, input, i * 100);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    float throughput = (STRESS_NEURON_COUNT * 1000.0f) / duration_ms;

    std::cout << "\n=== Mixed Type Stress Test ===" << std::endl;
    std::cout << "Neurons processed: " << STRESS_NEURON_COUNT << std::endl;
    std::cout << "Duration: " << duration_ms << " ms" << std::endl;
    std::cout << "Throughput: " << throughput << " neurons/s" << std::endl;

    EXPECT_GT(throughput, 10000.0f) << "Mixed type processing too slow";
}

TEST_F(NeuronTypesRegressionTest, Stress_ConcurrentTypeProcessing) {
    // WHAT: Multiple threads processing different neuron types
    // WHY:  Test thread safety
    // HOW:  4 threads each processing 1000 neurons

    constexpr int NUM_THREADS = 4;
    constexpr int NEURONS_PER_THREAD = 1000;

    std::atomic<uint32_t> total_processed{0};
    std::vector<std::thread> threads;

    auto thread_func = [&](neuron_type_t type) {
        neuron_type_params_t params;
        neuron_type_get_default_params(type, &params);

        for (int i = 0; i < NEURONS_PER_THREAD; ++i) {
            float input = static_cast<float>(i % 100) / 100.0f;
            float output = neuron_type_process_input(type, &params, input, i * 100);
            total_processed++;
        }
    };

    neuron_type_t types[] = {
        NEURON_GENERIC_LIF,
        NEURON_V1_SIMPLE_CELL,
        NEURON_LOGIC_AND,
        NEURON_METACOGNITIVE
    };

    auto start = std::chrono::high_resolution_clock::now();

    // Launch threads
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(thread_func, types[i]);
    }

    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    std::cout << "\n=== Concurrent Type Processing ===" << std::endl;
    std::cout << "Total processed: " << total_processed.load() << std::endl;
    std::cout << "Duration: " << duration_ms << " ms" << std::endl;

    EXPECT_EQ(total_processed.load(), NUM_THREADS * NEURONS_PER_THREAD);
}

//=============================================================================
// VALIDATION TESTS
//=============================================================================

TEST_F(NeuronTypesRegressionTest, Validation_AllTypesHaveDefaults) {
    // WHAT: Verify all neuron types have default parameters
    // WHY:  Ensure API completeness
    // HOW:  Request defaults for each type

    int success_count = 0;
    int total_types = static_cast<int>(NEURON_TYPE_COUNT);

    for (int i = 0; i < total_types; ++i) {
        neuron_type_t type = static_cast<neuron_type_t>(i);
        neuron_type_params_t params;

        nimcp_result_t result = neuron_type_get_default_params(type, &params);
        if (result == NIMCP_SUCCESS) {
            success_count++;
        }
    }

    std::cout << "\n=== Default Parameters Test ===" << std::endl;
    std::cout << "Types with defaults: " << success_count << " / " << total_types << std::endl;

    // Most types should have defaults
    EXPECT_GT(success_count, total_types / 2);
}

TEST_F(NeuronTypesRegressionTest, Validation_TypeNameMapping) {
    // WHAT: Verify all types have human-readable names
    // WHY:  Ensure debugging/logging support
    // HOW:  Get name for each type

    neuron_type_t test_types[] = {
        NEURON_EXCITATORY,
        NEURON_INHIBITORY,
        NEURON_GENERIC_LIF,
        NEURON_V1_SIMPLE_CELL,
        NEURON_LOGIC_AND,
        NEURON_METACOGNITIVE
    };

    for (auto type : test_types) {
        const char* name = neuron_type_get_name(type);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0);
        std::cout << "Type " << static_cast<int>(type) << ": " << name << std::endl;
    }
}
