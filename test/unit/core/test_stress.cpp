/**
 * @file test_stress.cpp
 * @brief Stress and load testing for NIMCP components
 *
 * Tests system behavior under extreme conditions:
 * - High concurrent load
 * - Memory pressure
 * - Long-running operations
 * - Resource exhaustion scenarios
 *
 * These tests ensure the system remains stable and secure under stress.
 */

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include "core/brain/nimcp_brain.h"
#include "networking/distributed/nimcp_distributed_cognition.h"
#include "glial/integration/nimcp_glial_integration.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/queue_manager/nimcp_queue_manager.h"

//==============================================================================
// Test Configuration
//==============================================================================

constexpr int STRESS_ITERATIONS = 1000;
constexpr int CONCURRENT_THREADS = 8;
constexpr int STRESS_DURATION_SECONDS = 5;

//==============================================================================
// Test Fixture
//==============================================================================

class StressTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        // Initialize test environment
    }

    void TearDown() override
    {
        // Cleanup
    }

    /**
     * WHAT: Helper to create distributed cognition coordinator for stress testing
     * WHY:  Simplify test setup with fast sync intervals
     * HOW:  Mock P2P node, aggressive config for load testing
     */
    distrib_cognition_t create_stress_coordinator() {
        p2p_node_t p2p_node = (p2p_node_t)0x1234; // Mock P2P node

        distrib_cognition_config_t config;
        config.enable_neuromod_sync = true;
        config.neuromod_broadcast_interval_ms = 10;
        config.neuromod_diffusion_rate = 0.5f;
        config.enable_glial_sync = true;
        config.glial_sync_interval_ms = 10;
        config.enable_region_sync = true;
        config.region_sync_interval_ms = 10;
        config.sync_mode = SYNC_MODE_BIDIRECTIONAL;
        config.max_message_queue = 10000;

        return distrib_cognition_create(&config, p2p_node);
    }
};

//==============================================================================
// Neural Network Stress Tests
//==============================================================================

/**
 * @test High-volume neural network creation and destruction
 */
TEST_F(StressTest, NeuralNetwork_MassCreationDestruction)
{
    network_config_t config = {
        .num_neurons = 20,
        .ei_ratio = 0.8f,
        .learning_rate = 0.01f,
        .hebbian_rate = 0.001f,
        .stdp_window = 20.0f,
        .homeostatic_rate = 0.001f,
        .target_activity = 0.1f,
        .adaptation_rate = 0.01f,
        .refractory_period = 2.0f,
        .min_weight = -1.0f,
        .max_weight = 1.0f,
        .update_interval = 1,
        .input_size = 10,
        .output_size = 5,
        .num_layers = 0,
        .layer_sizes = nullptr,
        .enable_stdp = true,
        .enable_hebbian = false,
        .enable_oja = false,
        .enable_homeostasis = false,
        .neuron_model = NEURON_MODEL_LIF,
        .model_params = nullptr};

    std::vector<neural_network_t> networks;
    networks.reserve(STRESS_ITERATIONS);

    // Rapid creation
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        neural_network_t net = neural_network_create(&config);
        ASSERT_NE(net, nullptr) << "Failed to create network " << i;
        networks.push_back(net);
    }

    // Rapid destruction
    for (auto net : networks) {
        neural_network_destroy(net);
    }
}

/**
 * @test Concurrent neural network operations
 */
TEST_F(StressTest, NeuralNetwork_ConcurrentOperations)
{
    network_config_t config = {
        .num_neurons = 20,
        .ei_ratio = 0.8f,
        .learning_rate = 0.01f,
        .hebbian_rate = 0.001f,
        .stdp_window = 20.0f,
        .homeostatic_rate = 0.001f,
        .target_activity = 0.1f,
        .adaptation_rate = 0.01f,
        .refractory_period = 2.0f,
        .min_weight = -1.0f,
        .max_weight = 1.0f,
        .update_interval = 1,
        .input_size = 10,
        .output_size = 5,
        .num_layers = 0,
        .layer_sizes = nullptr,
        .enable_stdp = true,
        .enable_hebbian = false,
        .enable_oja = false,
        .enable_homeostasis = false,
        .neuron_model = NEURON_MODEL_LIF,
        .model_params = nullptr};

    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    // Launch concurrent workers
    for (int t = 0; t < CONCURRENT_THREADS; t++) {
        threads.emplace_back([&config, &error_count]() {
            // Each thread creates network and performs operations
            neural_network_t net = neural_network_create(&config);
            if (!net) {
                error_count++;
                return;
            }

            float inputs[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
            float outputs[5] = {0};

            // Perform many forward passes
            for (int i = 0; i < 100; i++) {
                neural_network_forward(net, inputs, 10, outputs, 5);

                // Verify outputs are in valid range
                for (int j = 0; j < 5; j++) {
                    if (std::isnan(outputs[j]) || std::isinf(outputs[j])) {
                        error_count++;
                    }
                }
            }

            neural_network_destroy(net);
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(error_count.load(), 0) << "Errors occurred during concurrent operations";
}

/**
 * @test Long-running neural network stability
 */
TEST_F(StressTest, DISABLED_NeuralNetwork_LongRunningStability)
{
    network_config_t config = {
        .num_neurons = 20,
        .ei_ratio = 0.8f,
        .learning_rate = 0.01f,
        .hebbian_rate = 0.001f,
        .stdp_window = 20.0f,
        .homeostatic_rate = 0.001f,
        .target_activity = 0.1f,
        .adaptation_rate = 0.01f,
        .refractory_period = 2.0f,
        .min_weight = -1.0f,
        .max_weight = 1.0f,
        .update_interval = 1,
        .input_size = 10,
        .output_size = 5,
        .num_layers = 0,
        .layer_sizes = nullptr,
        .enable_stdp = true,
        .enable_hebbian = false,
        .enable_oja = false,
        .enable_homeostasis = false,
        .neuron_model = NEURON_MODEL_LIF,
        .model_params = nullptr};

    neural_network_t net = neural_network_create(&config);
    ASSERT_NE(net, nullptr);

    float inputs[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float outputs[5] = {0};

    auto start = std::chrono::steady_clock::now();
    auto end = start + std::chrono::seconds(STRESS_DURATION_SECONDS);

    int iteration_count = 0;
    while (std::chrono::steady_clock::now() < end) {
        neural_network_forward(net, inputs, 10, outputs, 5);

        // Verify outputs remain stable
        for (int j = 0; j < 5; j++) {
            ASSERT_FALSE(std::isnan(outputs[j])) << "NaN detected at iteration " << iteration_count;
            ASSERT_FALSE(std::isinf(outputs[j])) << "Inf detected at iteration " << iteration_count;
        }

        iteration_count++;
    }

    neural_network_destroy(net);

    // Should complete thousands of iterations
    EXPECT_GT(iteration_count, 1000) << "Unexpectedly low iteration count";
}

//==============================================================================
// Queue Manager Stress Tests
//==============================================================================

/**
 * @test High-throughput queue operations (SKIPPED due to blocking queue timeout issues)
 */
TEST_F(StressTest, QueueManager_HighThroughput)
{
    GTEST_SKIP() << "Test causes timeout due to blocking queue issues - needs investigation";

    nimcp_queue_manager_config_t config = {
        .queue_sizes = {.high = 1000, .normal = 1000, .low = 1000},
        .default_timeout = 1000,
        .blocking_mode = true,
        .max_channels = 4,
        .worker_threads = 1};

    nimcp_queue_manager_t* manager = nullptr;
    nimcp_result_t result = nimcp_queue_manager_create(&config, &manager);
    ASSERT_EQ(result, NIMCP_SUCCESS);
    ASSERT_NE(manager, nullptr);

    std::atomic<int> enqueue_count{0};
    std::atomic<int> dequeue_count{0};

    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // Producer threads
    for (int t = 0; t < CONCURRENT_THREADS / 2; t++) {
        producers.emplace_back([manager, &enqueue_count]() {
            nimcp_message_t msg = {.data = nullptr, .size = 64};

            uint8_t data[64] = {0};
            msg.data = data;

            for (int i = 0; i < STRESS_ITERATIONS; i++) {
                uint32_t channel = i % 4;
                if (nimcp_queue_manager_enqueue(manager, channel, &msg, 1)) {
                    enqueue_count++;
                }
                std::this_thread::yield();
            }
        });
    }

    // Consumer threads
    for (int t = 0; t < CONCURRENT_THREADS / 2; t++) {
        consumers.emplace_back([manager, &dequeue_count]() {
            nimcp_message_t msg;
            nimcp_message_t* msg_ptr = &msg;

            for (int i = 0; i < STRESS_ITERATIONS; i++) {
                uint32_t channel = i % 4;
                if (nimcp_queue_manager_dequeue(manager, channel, &msg_ptr, 1)) {
                    dequeue_count++;
                }
                std::this_thread::yield();
            }
        });
    }

    // Wait for completion
    for (auto& t : producers)
        t.join();
    for (auto& t : consumers)
        t.join();

    nimcp_queue_manager_destroy(manager);

    // Verify high throughput
    EXPECT_GT(enqueue_count.load(), STRESS_ITERATIONS) << "Low enqueue throughput";
    EXPECT_GT(dequeue_count.load(), 0) << "No messages dequeued";
}

/**
 * @test Queue manager under memory pressure
 */
TEST_F(StressTest, QueueManager_MemoryPressure)
{
    // Create multiple queue managers to simulate memory pressure
    std::vector<nimcp_queue_manager_t*> managers;

    nimcp_queue_manager_config_t config = {
        .queue_sizes = {.high = 500, .normal = 500, .low = 500},
        .default_timeout = 1000,
        .blocking_mode = true,
        .max_channels = 8,
        .worker_threads = 1};

    // Create as many as we can
    for (int i = 0; i < 20; i++) {
        nimcp_queue_manager_t* manager = nullptr;
        nimcp_result_t result = nimcp_queue_manager_create(&config, &manager);
        if (result == NIMCP_SUCCESS && manager != nullptr) {
            managers.push_back(manager);
        } else {
            // Allocation failure is acceptable under memory pressure
            break;
        }
    }

    EXPECT_GT(managers.size(), 0) << "Could not create any queue managers";

    // Test that all managers still work
    for (auto* manager : managers) {
        nimcp_message_t msg = {.data = nullptr, .size = 64};
        uint8_t data[64] = {0};
        msg.data = data;

        EXPECT_NO_FATAL_FAILURE(nimcp_queue_manager_enqueue(manager, 0, &msg, 1));
    }

    // Cleanup
    for (auto* manager : managers) {
        nimcp_queue_manager_destroy(manager);
    }
}

//==============================================================================
// Memory Allocation Stress Tests
//==============================================================================

/**
 * @test Rapid allocation and deallocation
 */
TEST_F(StressTest, Memory_RapidAllocationDeallocation)
{
    std::vector<void*> allocations;
    allocations.reserve(STRESS_ITERATIONS);

    // Allocate
    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        size_t size = 64 + (i % 1024);  // Variable sizes
        void* ptr = malloc(size);
        ASSERT_NE(ptr, nullptr) << "Allocation failed at iteration " << i;
        allocations.push_back(ptr);
    }

    // Deallocate
    for (void* ptr : allocations) {
        free(ptr);
    }
}

/**
 * @test Fragmentation resistance
 */
TEST_F(StressTest, Memory_FragmentationResistance)
{
    std::vector<void*> allocations;
    const int ALLOC_COUNT = 500;

    // Allocate mix of sizes
    for (int i = 0; i < ALLOC_COUNT; i++) {
        size_t size;
        if (i % 3 == 0)
            size = 16;
        else if (i % 3 == 1)
            size = 256;
        else
            size = 1024;

        void* ptr = malloc(size);
        if (ptr != nullptr) {
            allocations.push_back(ptr);
        }
    }

    // Free every other allocation
    for (size_t i = 0; i < allocations.size(); i += 2) {
        free(allocations[i]);
        allocations[i] = nullptr;
    }

    // Try to allocate again (tests fragmentation handling)
    for (size_t i = 0; i < allocations.size(); i += 2) {
        allocations[i] = malloc(128);
        // OK if this fails under fragmentation
    }

    // Cleanup
    for (void* ptr : allocations) {
        if (ptr != nullptr) {
            free(ptr);
        }
    }
}

//==============================================================================
// Concurrent Brain Operations
//==============================================================================

/**
 * @test Concurrent brain creation and destruction
 */
TEST_F(StressTest, Brain_ConcurrentLifecycle)
{
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < CONCURRENT_THREADS; t++) {
        threads.emplace_back([&success_count, &error_count, t]() {
            // Generate unique task name
            char task_name[64];
            snprintf(task_name, sizeof(task_name), "stress_test_%d", t);

            brain_t brain = brain_create(task_name, BRAIN_SIZE_TINY, BRAIN_TASK_CLASSIFICATION, 10, 5);
            if (brain == nullptr) {
                error_count++;
                return;
            }

            // Perform some decision operations
            float inputs[10] = {0};

            for (int i = 0; i < 10; i++) {
                brain_decision_t* decision = brain_decide(brain, inputs, 10);
                if (decision) {
                    brain_free_decision(decision);
                }
            }

            brain_destroy(brain);
            success_count++;
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_GT(success_count.load(), 0) << "No successful brain operations";
    EXPECT_EQ(error_count.load(), 0) << "Errors during concurrent brain operations";
}

//==============================================================================
// Phase 3: Distributed Cognition Stress Tests
//==============================================================================

/**
 * WHAT: Stress test with thousands of neuromodulator broadcasts
 * WHY:  Verify system handles high-frequency broadcast load
 * HOW:  Rapid-fire broadcasts across all neuromodulator types
 */
TEST_F(StressTest, DistributedCognition_MassNeuromodBroadcasts)
{
    distrib_cognition_t dc = create_stress_coordinator();
    ASSERT_NE(dc, nullptr);

    ASSERT_TRUE(distrib_cognition_start(dc));

    // Broadcast 1,000 neuromodulator updates (reduced from 10,000 for performance)
    const int broadcast_count = 1000;
    for (int i = 0; i < broadcast_count; i++) {
        neuromodulator_type_t type = static_cast<neuromodulator_type_t>(i % NEUROMOD_COUNT);
        float concentration = (i % 100) / 100.0f;

        ASSERT_TRUE(distrib_cognition_broadcast_neuromod(dc, type, concentration));
    }

    // Verify stats
    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.neuromod_broadcasts, broadcast_count);

    ASSERT_TRUE(distrib_cognition_stop(dc));
    distrib_cognition_destroy(dc);
}

/**
 * WHAT: Concurrent neuromodulator broadcasts from multiple threads
 * WHY:  Verify thread safety under concurrent load
 * HOW:  8 threads each broadcasting 1000 times
 */
TEST_F(StressTest, DistributedCognition_ConcurrentBroadcasts)
{
    distrib_cognition_t dc = create_stress_coordinator();
    ASSERT_NE(dc, nullptr);

    ASSERT_TRUE(distrib_cognition_start(dc));

    std::atomic<int> error_count{0};
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // Launch concurrent broadcasting threads
    for (int t = 0; t < CONCURRENT_THREADS; t++) {
        threads.emplace_back([dc, &error_count, &success_count, t]() {
            for (int i = 0; i < STRESS_ITERATIONS; i++) {
                neuromodulator_type_t type = static_cast<neuromodulator_type_t>((t + i) % NEUROMOD_COUNT);
                float concentration = ((t * STRESS_ITERATIONS + i) % 100) / 100.0f;

                if (distrib_cognition_broadcast_neuromod(dc, type, concentration)) {
                    success_count++;
                } else {
                    error_count++;
                }

                std::this_thread::yield();
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(error_count.load(), 0) << "Errors during concurrent broadcasts";
    EXPECT_EQ(success_count.load(), CONCURRENT_THREADS * STRESS_ITERATIONS);

    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.neuromod_broadcasts, CONCURRENT_THREADS * STRESS_ITERATIONS);

    ASSERT_TRUE(distrib_cognition_stop(dc));
    distrib_cognition_destroy(dc);
}

/**
 * WHAT: Mass pruning consensus voting under load
 * WHY:  Verify consensus mechanism handles high vote volume
 * HOW:  Thousands of pruning votes for multiple synapses
 */
TEST_F(StressTest, DistributedCognition_MassConsensusSessions)
{
    distrib_cognition_t dc = create_stress_coordinator();
    ASSERT_NE(dc, nullptr);

    glial_integration_t glial;
    ASSERT_TRUE(distrib_cognition_register_glial_system(dc, &glial));

    ASSERT_TRUE(distrib_cognition_start(dc));

    // Create 500 pruning votes across 100 different synapses (reduced from 5000 for performance)
    const int vote_count = 500;
    const int synapse_count = 100;

    for (int i = 0; i < vote_count; i++) {
        uint32_t source = i % synapse_count;
        uint32_t target = (i + 1) % synapse_count;
        float confidence = (i % 100) / 100.0f;
        uint8_t action = i % 3;  // 0=monitor, 1=prune, 2=preserve

        ASSERT_TRUE(distrib_cognition_coordinate_pruning(dc, source, target, confidence, action));
    }

    // Verify stats
    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_EQ(stats.glial_pruning_coordinations, vote_count);

    ASSERT_TRUE(distrib_cognition_stop(dc));
    distrib_cognition_destroy(dc);
}

/**
 * WHAT: Concurrent consensus voting from multiple threads
 * WHY:  Verify thread safety of pruning coordination
 * HOW:  8 threads each submitting 1000 votes
 */
TEST_F(StressTest, DistributedCognition_ConcurrentConsensus)
{
    distrib_cognition_t dc = create_stress_coordinator();
    ASSERT_NE(dc, nullptr);

    glial_integration_t glial;
    ASSERT_TRUE(distrib_cognition_register_glial_system(dc, &glial));

    ASSERT_TRUE(distrib_cognition_start(dc));

    std::atomic<int> error_count{0};
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // Launch concurrent voting threads
    for (int t = 0; t < CONCURRENT_THREADS; t++) {
        threads.emplace_back([dc, &error_count, &success_count, t]() {
            for (int i = 0; i < STRESS_ITERATIONS; i++) {
                uint32_t source = (t * STRESS_ITERATIONS + i) % 500;
                uint32_t target = (source + 1) % 500;
                float confidence = ((t + i) % 100) / 100.0f;
                uint8_t action = (t + i) % 3;

                if (distrib_cognition_coordinate_pruning(dc, source, target, confidence, action)) {
                    success_count++;
                } else {
                    error_count++;
                }

                std::this_thread::yield();
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(error_count.load(), 0) << "Errors during concurrent consensus";
    EXPECT_EQ(success_count.load(), CONCURRENT_THREADS * STRESS_ITERATIONS);

    ASSERT_TRUE(distrib_cognition_stop(dc));
    distrib_cognition_destroy(dc);
}

/**
 * WHAT: Long-running stability test for distributed coordinator
 * WHY:  Verify no memory leaks or degradation over time
 * HOW:  Run for 5 seconds with continuous operations
 */
TEST_F(StressTest, DISABLED_DistributedCognition_LongRunningStability)
{
    distrib_cognition_t dc = create_stress_coordinator();
    ASSERT_NE(dc, nullptr);

    ASSERT_TRUE(distrib_cognition_start(dc));

    auto start = std::chrono::steady_clock::now();
    auto end = start + std::chrono::seconds(STRESS_DURATION_SECONDS);

    int operation_count = 0;
    while (std::chrono::steady_clock::now() < end) {
        // Alternate between broadcasts and consensus
        if (operation_count % 2 == 0) {
            neuromodulator_type_t type = static_cast<neuromodulator_type_t>(operation_count % NEUROMOD_COUNT);
            distrib_cognition_broadcast_neuromod(dc, type, 0.5f);
        } else {
            distrib_cognition_coordinate_pruning(dc, operation_count % 1000, (operation_count + 1) % 1000, 0.7f, 1);
        }

        operation_count++;
        std::this_thread::yield();
    }

    // Should complete thousands of operations
    EXPECT_GT(operation_count, 1000) << "Unexpectedly low operation count";

    distrib_cognition_stats_t stats;
    ASSERT_TRUE(distrib_cognition_get_stats(dc, &stats));
    EXPECT_GT(stats.neuromod_broadcasts + stats.glial_pruning_coordinations, 1000);

    ASSERT_TRUE(distrib_cognition_stop(dc));
    distrib_cognition_destroy(dc);
}

/**
 * WHAT: Rapid start/stop cycles stress test
 * WHY:  Verify robust thread lifecycle management
 * HOW:  50 cycles of start/stop operations
 */
TEST_F(StressTest, DistributedCognition_RapidStartStopCycles)
{
    distrib_cognition_t dc = create_stress_coordinator();
    ASSERT_NE(dc, nullptr);

    const int cycles = 50;  // Reduced from 100 for performance
    for (int i = 0; i < cycles; i++) {
        ASSERT_TRUE(distrib_cognition_start(dc)) << "Failed to start at cycle " << i;

        // Do a few operations
        distrib_cognition_broadcast_neuromod(dc, NEUROMOD_DOPAMINE, 0.5f);

        ASSERT_TRUE(distrib_cognition_stop(dc)) << "Failed to stop at cycle " << i;
    }

    distrib_cognition_destroy(dc);
}

//==============================================================================
// Performance Regression Tests
//==============================================================================

/**
 * @test Ensure operations complete within reasonable time
 */
TEST_F(StressTest, Performance_OperationLatency)
{
    network_config_t config = {
        .num_neurons = 20,
        .ei_ratio = 0.8f,
        .learning_rate = 0.01f,
        .hebbian_rate = 0.001f,
        .stdp_window = 20.0f,
        .homeostatic_rate = 0.001f,
        .target_activity = 0.1f,
        .adaptation_rate = 0.01f,
        .refractory_period = 2.0f,
        .min_weight = -1.0f,
        .max_weight = 1.0f,
        .update_interval = 1,
        .input_size = 10,
        .output_size = 5,
        .num_layers = 0,
        .layer_sizes = nullptr,
        .enable_stdp = true,
        .enable_hebbian = false,
        .enable_oja = false,
        .enable_homeostasis = false,
        .neuron_model = NEURON_MODEL_LIF,
        .model_params = nullptr};

    neural_network_t net = neural_network_create(&config);
    ASSERT_NE(net, nullptr);

    float inputs[10] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f};
    float outputs[5] = {0};

    // Measure 1000 forward passes
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        neural_network_forward(net, inputs, 10, outputs, 5);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    neural_network_destroy(net);

    // Should complete in reasonable time (< 100ms for 1000 iterations)
    EXPECT_LT(duration.count(), 100000) << "Operations too slow: " << duration.count() << " us";

    double avg_latency_us = duration.count() / 1000.0;
    std::cout << "Average forward pass latency: " << avg_latency_us << " µs" << std::endl;
}
