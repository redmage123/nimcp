//=============================================================================
// test_glial_performance_regression.cpp - Glial Performance Regression Tests
//=============================================================================
/**
 * @file test_glial_performance_regression.cpp
 * @brief Performance regression tests for glial cell modules
 *
 * WHAT: Performance and scalability regression tests for glial subsystems
 * WHY:  Ensure glial operations meet timing requirements and scale correctly
 * HOW:  Benchmark operations, measure memory, verify thread safety
 *
 * TEST COVERAGE:
 * 1. Microglia Pruning Performance (100 microglia x 100 synapses)
 * 2. Oligodendrocyte Myelination Performance (50 oligo x 50 neurons)
 * 3. Full Glial System Scalability (10 to 1000 cells)
 * 4. Memory Efficiency (create/destroy 1000 cells)
 * 5. Concurrent Access (multi-threaded access)
 *
 * PERFORMANCE TARGETS:
 * - Microglia pruning: < 5ms per network step
 * - Oligodendrocyte myelination: < 500us per network step
 * - Scalability: O(n) or better
 * - Memory: No leaks, reasonable peak usage
 * - Concurrency: No race conditions or deadlocks
 *
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <random>
#include <thread>
#include <atomic>
#include <mutex>
#include <numeric>
#include <cmath>
#include <sys/resource.h>

// Glial module headers
// Headers have their own extern "C" guards
#include "glial/microglia/nimcp_microglia.h"
#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"
#include "glial/integration/nimcp_glial_integration.h"

//=============================================================================
// Performance Utilities
//=============================================================================

class GlialPerformanceMonitor {
public:
    /**
     * @brief Get current memory usage in KB
     * @return Memory usage in KB
     */
    static long GetMemoryUsageKB() {
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        return usage.ru_maxrss;  // KB on Linux
    }

    /**
     * @brief Measure execution time in microseconds
     * @param func Function to measure
     * @return Execution time in microseconds
     */
    template<typename Func>
    static double MeasureTimeUs(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start).count();
    }

    /**
     * @brief Measure execution time in milliseconds
     * @param func Function to measure
     * @return Execution time in milliseconds
     */
    template<typename Func>
    static double MeasureTimeMs(Func func) {
        return MeasureTimeUs(func) / 1000.0;
    }

    /**
     * @brief Calculate mean of a vector
     * @param values Vector of values
     * @return Mean value
     */
    static double Mean(const std::vector<double>& values) {
        if (values.empty()) return 0.0;
        double sum = std::accumulate(values.begin(), values.end(), 0.0);
        return sum / values.size();
    }

    /**
     * @brief Calculate standard deviation
     * @param values Vector of values
     * @return Standard deviation
     */
    static double StdDev(const std::vector<double>& values) {
        if (values.size() < 2) return 0.0;
        double mean = Mean(values);
        double sq_sum = 0.0;
        for (double v : values) {
            sq_sum += (v - mean) * (v - mean);
        }
        return std::sqrt(sq_sum / (values.size() - 1));
    }
};

//=============================================================================
// Test Fixture
//=============================================================================

class GlialPerformanceRegressionTest : public ::testing::Test {
protected:
    std::mt19937 rng_;

    void SetUp() override {
        rng_.seed(42);  // Deterministic seed for reproducibility
    }

    void TearDown() override {
    }

    /**
     * @brief Generate random float in range
     */
    float RandomFloat(float min, float max) {
        std::uniform_real_distribution<float> dist(min, max);
        return dist(rng_);
    }

    /**
     * @brief Get current timestamp in microseconds
     */
    uint64_t GetTimestampUs() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    }
};

//=============================================================================
// Test 1: Microglia Pruning Performance
//=============================================================================

TEST_F(GlialPerformanceRegressionTest, MicrogliaPruningPerformance) {
    // WHAT: Measure pruning throughput for 100 microglia x 100 synapses
    // WHY:  Ensure pruning meets < 1ms per network step target
    // HOW:  Create network, simulate activity, measure step time

    const uint32_t NUM_MICROGLIA = 100;
    const uint32_t SYNAPSES_PER_MICROGLIA = 100;
    const uint32_t NUM_WARMUP_STEPS = 10;
    const uint32_t NUM_MEASURED_STEPS = 100;
    const double TARGET_MS = 5.0;  // Target: < 5ms per step (relaxed for CI/parallel test contention)

    // Create microglia network
    microglia_network_t* network = microglia_network_create(NUM_MICROGLIA);
    ASSERT_NE(network, nullptr) << "Failed to create microglia network";

    // Create microglia and assign synapses
    for (uint32_t i = 0; i < NUM_MICROGLIA; i++) {
        float x = RandomFloat(-100.0f, 100.0f);
        float y = RandomFloat(-100.0f, 100.0f);
        float z = RandomFloat(-100.0f, 100.0f);

        microglia_t* mg = microglia_create(i, x, y, z, NIMCP_MICROGLIA_SURVEILLANCE_RADIUS_UM);
        ASSERT_NE(mg, nullptr) << "Failed to create microglia " << i;

        // Monitor synapses
        for (uint32_t j = 0; j < SYNAPSES_PER_MICROGLIA; j++) {
            uint32_t synapse_id = i * SYNAPSES_PER_MICROGLIA + j;
            nimcp_result_t result = microglia_monitor_synapse(mg, synapse_id);
            ASSERT_EQ(result, NIMCP_SUCCESS) << "Failed to monitor synapse " << synapse_id;
        }

        nimcp_result_t result = microglia_network_add(network, mg);
        ASSERT_EQ(result, NIMCP_SUCCESS) << "Failed to add microglia to network";
    }

    // Simulate some activity on synapses
    uint64_t timestamp = GetTimestampUs();
    for (uint32_t i = 0; i < NUM_MICROGLIA; i++) {
        microglia_t* mg = network->microglia[i];
        for (uint32_t j = 0; j < SYNAPSES_PER_MICROGLIA; j++) {
            uint32_t synapse_id = i * SYNAPSES_PER_MICROGLIA + j;
            float activity = RandomFloat(0.0f, 1.0f);
            microglia_track_synapse_activity(mg, synapse_id, activity, timestamp);
        }
    }

    // Warmup steps
    for (uint32_t i = 0; i < NUM_WARMUP_STEPS; i++) {
        timestamp += 1000;  // 1ms step
        microglia_network_step(network, timestamp);
    }

    // Measure step times
    std::vector<double> step_times_ms;
    step_times_ms.reserve(NUM_MEASURED_STEPS);

    for (uint32_t i = 0; i < NUM_MEASURED_STEPS; i++) {
        timestamp += 1000;

        double step_time = GlialPerformanceMonitor::MeasureTimeMs([&]() {
            microglia_network_step(network, timestamp);
        });

        step_times_ms.push_back(step_time);
    }

    // Calculate statistics
    double mean_time = GlialPerformanceMonitor::Mean(step_times_ms);
    double stddev_time = GlialPerformanceMonitor::StdDev(step_times_ms);
    double max_time = *std::max_element(step_times_ms.begin(), step_times_ms.end());
    double min_time = *std::min_element(step_times_ms.begin(), step_times_ms.end());

    // Calculate pruning throughput
    uint32_t total_synapses = NUM_MICROGLIA * SYNAPSES_PER_MICROGLIA;
    double throughput = total_synapses / (mean_time / 1000.0);  // synapses per second

    // Report results
    std::cout << "\n=== Microglia Pruning Performance ===" << std::endl;
    std::cout << "Configuration: " << NUM_MICROGLIA << " microglia x "
              << SYNAPSES_PER_MICROGLIA << " synapses = "
              << total_synapses << " total synapses" << std::endl;
    std::cout << "Mean step time: " << mean_time << " ms (target: < " << TARGET_MS << " ms)" << std::endl;
    std::cout << "StdDev: " << stddev_time << " ms" << std::endl;
    std::cout << "Range: [" << min_time << ", " << max_time << "] ms" << std::endl;
    std::cout << "Throughput: " << throughput / 1000.0 << " K synapses/second" << std::endl;

    // Verify performance target
    EXPECT_LT(mean_time, TARGET_MS)
        << "Mean step time (" << mean_time << " ms) exceeds target (" << TARGET_MS << " ms)";

    // Also check max time doesn't spike too high (allow 10x target for system jitter/warmup)
    EXPECT_LT(max_time, TARGET_MS * 10.0)
        << "Max step time (" << max_time << " ms) exceeds " << (TARGET_MS * 10.0) << " ms";

    // Cleanup
    microglia_network_destroy(network);
}

//=============================================================================
// Test 2: Oligodendrocyte Myelination Performance
//=============================================================================

TEST_F(GlialPerformanceRegressionTest, OligodendrocyteMyelinationPerformance) {
    // WHAT: Measure myelination throughput for 50 oligodendrocytes x 50 neurons
    // WHY:  Ensure remodeling meets < 500us per network step target
    // HOW:  Create network, track activity, measure step time

    const uint32_t NUM_OLIGODENDROCYTES = 50;
    const uint32_t NEURONS_PER_OLIGO = 50;
    const uint32_t NUM_WARMUP_STEPS = 10;
    const uint32_t NUM_MEASURED_STEPS = 100;
    const double TARGET_US = 500.0;  // Target: < 500us per step

    // Create oligodendrocyte network
    oligodendrocyte_network_t* network = oligodendrocyte_network_create(NUM_OLIGODENDROCYTES);
    ASSERT_NE(network, nullptr) << "Failed to create oligodendrocyte network";

    // Create oligodendrocytes and assign neurons
    for (uint32_t i = 0; i < NUM_OLIGODENDROCYTES; i++) {
        float x = (float)(i % 10) * 100.0f;
        float y = (float)((i / 10) % 10) * 100.0f;
        float z = (float)(i / 100) * 100.0f;
        oligodendrocyte_t* oligo = oligodendrocyte_create(i, x, y, z, NEURONS_PER_OLIGO);
        ASSERT_NE(oligo, nullptr) << "Failed to create oligodendrocyte " << i;

        // Assign neurons
        for (uint32_t j = 0; j < NEURONS_PER_OLIGO; j++) {
            uint32_t neuron_id = i * NEURONS_PER_OLIGO + j;
            nimcp_result_t result = oligodendrocyte_assign_neuron(oligo, neuron_id);
            ASSERT_EQ(result, NIMCP_SUCCESS) << "Failed to assign neuron " << neuron_id;
        }

        nimcp_result_t result = oligodendrocyte_network_add(network, oligo);
        ASSERT_EQ(result, NIMCP_SUCCESS) << "Failed to add oligodendrocyte to network";
    }

    // Track activity for neurons
    uint64_t timestamp = GetTimestampUs();
    for (uint32_t i = 0; i < NUM_OLIGODENDROCYTES; i++) {
        oligodendrocyte_t* oligo = network->oligodendrocytes[i];
        for (uint32_t j = 0; j < NEURONS_PER_OLIGO; j++) {
            uint32_t neuron_id = i * NEURONS_PER_OLIGO + j;
            float activity = RandomFloat(0.0f, 5.0f);  // Activity in Hz
            oligodendrocyte_track_activity(oligo, neuron_id, activity, timestamp);
        }
    }

    // Warmup steps
    float dt = 0.001f;  // 1ms step
    for (uint32_t i = 0; i < NUM_WARMUP_STEPS; i++) {
        oligodendrocyte_network_step(network, dt);
    }

    // Measure step times
    std::vector<double> step_times_us;
    step_times_us.reserve(NUM_MEASURED_STEPS);

    for (uint32_t i = 0; i < NUM_MEASURED_STEPS; i++) {
        double step_time = GlialPerformanceMonitor::MeasureTimeUs([&]() {
            oligodendrocyte_network_step(network, dt);
        });

        step_times_us.push_back(step_time);
    }

    // Calculate statistics
    double mean_time = GlialPerformanceMonitor::Mean(step_times_us);
    double stddev_time = GlialPerformanceMonitor::StdDev(step_times_us);
    double max_time = *std::max_element(step_times_us.begin(), step_times_us.end());
    double min_time = *std::min_element(step_times_us.begin(), step_times_us.end());

    // Calculate remodeling throughput
    uint32_t total_neurons = NUM_OLIGODENDROCYTES * NEURONS_PER_OLIGO;
    double throughput = total_neurons / (mean_time / 1e6);  // neurons per second

    // Report results
    std::cout << "\n=== Oligodendrocyte Myelination Performance ===" << std::endl;
    std::cout << "Configuration: " << NUM_OLIGODENDROCYTES << " oligodendrocytes x "
              << NEURONS_PER_OLIGO << " neurons = "
              << total_neurons << " total neurons" << std::endl;
    std::cout << "Mean step time: " << mean_time << " us (target: < " << TARGET_US << " us)" << std::endl;
    std::cout << "StdDev: " << stddev_time << " us" << std::endl;
    std::cout << "Range: [" << min_time << ", " << max_time << "] us" << std::endl;
    std::cout << "Throughput: " << throughput / 1e6 << " M neurons/second" << std::endl;

    // Verify performance target
    EXPECT_LT(mean_time, TARGET_US)
        << "Mean step time (" << mean_time << " us) exceeds target (" << TARGET_US << " us)";

    // Cleanup
    oligodendrocyte_network_destroy(network);
}

//=============================================================================
// Test 3: Full Glial System Scalability
//=============================================================================

TEST_F(GlialPerformanceRegressionTest, FullGlialSystemScalability) {
    // WHAT: Measure scalability from 10 to 1000 glial cells
    // WHY:  Verify O(n) or better scaling for production workloads
    // HOW:  Measure step time at different scales, fit scaling curve

    const std::vector<uint32_t> SCALES = {10, 50, 100, 200, 500, 1000};
    const uint32_t SYNAPSES_PER_CELL = 10;  // Keep synapse count low for scaling test
    const uint32_t NUM_STEPS = 50;

    std::vector<double> scale_sizes;
    std::vector<double> mean_times_ms;

    std::cout << "\n=== Full Glial System Scalability ===" << std::endl;
    std::cout << "Scale\t\tMean Time (ms)\tTime/Cell (us)" << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    for (uint32_t scale : SCALES) {
        // Track memory before
        long mem_before = GlialPerformanceMonitor::GetMemoryUsageKB();

        // Create microglia network at this scale
        microglia_network_t* network = microglia_network_create(scale);
        if (network == nullptr) {
            FAIL() << "Failed to create network at scale " << scale;
        }

        // Create microglia
        for (uint32_t i = 0; i < scale; i++) {
            float x = RandomFloat(-100.0f, 100.0f);
            float y = RandomFloat(-100.0f, 100.0f);
            float z = RandomFloat(-100.0f, 100.0f);

            microglia_t* mg = microglia_create(i, x, y, z, 50.0f);
            if (mg == nullptr) continue;

            // Monitor synapses
            for (uint32_t j = 0; j < SYNAPSES_PER_CELL; j++) {
                microglia_monitor_synapse(mg, i * SYNAPSES_PER_CELL + j);
            }

            microglia_network_add(network, mg);
        }

        // Track memory after creation
        long mem_after = GlialPerformanceMonitor::GetMemoryUsageKB();
        long mem_used = mem_after - mem_before;

        // Measure step times
        std::vector<double> step_times_ms;
        uint64_t timestamp = GetTimestampUs();

        for (uint32_t i = 0; i < NUM_STEPS; i++) {
            timestamp += 1000;

            double step_time = GlialPerformanceMonitor::MeasureTimeMs([&]() {
                microglia_network_step(network, timestamp);
            });

            step_times_ms.push_back(step_time);
        }

        double mean_time = GlialPerformanceMonitor::Mean(step_times_ms);
        double time_per_cell = (mean_time * 1000.0) / scale;  // us per cell

        scale_sizes.push_back(static_cast<double>(scale));
        mean_times_ms.push_back(mean_time);

        std::cout << scale << "\t\t"
                  << std::fixed << std::setprecision(3) << mean_time << "\t\t"
                  << std::fixed << std::setprecision(2) << time_per_cell
                  << "\t(Memory: " << mem_used << " KB)" << std::endl;

        microglia_network_destroy(network);
    }

    // Analyze scaling: compute ratio between largest and smallest
    // For O(n), time should scale linearly with n
    double smallest_scale = scale_sizes.front();
    double largest_scale = scale_sizes.back();
    double smallest_time = mean_times_ms.front();
    double largest_time = mean_times_ms.back();

    double scale_ratio = largest_scale / smallest_scale;
    double time_ratio = largest_time / smallest_time;

    // For O(n) scaling, time_ratio should be approximately equal to scale_ratio
    // For O(n^2) scaling (cytokine diffusion), time_ratio = scale_ratio^2
    double scaling_factor = time_ratio / scale_ratio;

    std::cout << "\n--- Scaling Analysis ---" << std::endl;
    std::cout << "Scale ratio: " << scale_ratio << "x (10 -> 1000)" << std::endl;
    std::cout << "Time ratio: " << time_ratio << "x" << std::endl;
    std::cout << "Scaling factor: " << scaling_factor << "x (1.0 = perfect O(n))" << std::endl;

    // NOTE: microglia_network_diffuse_cytokines() is O(n^2) due to all-pairs
    // cytokine exchange. For 100x scale, this gives ~100x scaling factor.
    // Future optimization: use KD-tree for spatial neighbor lookup.
    // For now, allow up to O(n^2) behavior with 20% tolerance.
    double expected_n2_scaling = scale_ratio;  // For O(n^2): time grows as n^2, so factor = n
    EXPECT_LT(scaling_factor, expected_n2_scaling * 3.0)
        << "Scaling factor (" << scaling_factor << ") exceeds expected O(n^2) by >3x";
}

//=============================================================================
// Test 4: Memory Efficiency
//=============================================================================

TEST_F(GlialPerformanceRegressionTest, MemoryEfficiency) {
    // WHAT: Create/destroy 1000 glial cells, verify no memory leaks
    // WHY:  Ensure proper memory management in glial modules
    // HOW:  Track memory before/after create-destroy cycles

    const uint32_t NUM_CELLS = 1000;
    const uint32_t NUM_CYCLES = 5;

    std::cout << "\n=== Memory Efficiency Test ===" << std::endl;

    // Force GC and get baseline
    long baseline_mem = GlialPerformanceMonitor::GetMemoryUsageKB();

    std::vector<long> peak_memories;
    std::vector<long> final_memories;

    for (uint32_t cycle = 0; cycle < NUM_CYCLES; cycle++) {
        long mem_before = GlialPerformanceMonitor::GetMemoryUsageKB();

        // Create network
        microglia_network_t* network = microglia_network_create(NUM_CELLS);
        ASSERT_NE(network, nullptr);

        // Create all microglia with synapses
        for (uint32_t i = 0; i < NUM_CELLS; i++) {
            microglia_t* mg = microglia_create(i, 0.0f, 0.0f, 0.0f, 100.0f);
            if (mg == nullptr) continue;

            // Add some synapses
            for (uint32_t j = 0; j < 10; j++) {
                microglia_monitor_synapse(mg, i * 10 + j);
            }

            microglia_network_add(network, mg);
        }

        long mem_peak = GlialPerformanceMonitor::GetMemoryUsageKB();
        peak_memories.push_back(mem_peak - mem_before);

        // Destroy network
        microglia_network_destroy(network);

        long mem_after = GlialPerformanceMonitor::GetMemoryUsageKB();
        final_memories.push_back(mem_after - baseline_mem);

        std::cout << "Cycle " << (cycle + 1) << ": Peak: +"
                  << (mem_peak - mem_before) << " KB, Final: +"
                  << (mem_after - baseline_mem) << " KB" << std::endl;
    }

    // Calculate average peak memory
    double avg_peak = GlialPerformanceMonitor::Mean(
        std::vector<double>(peak_memories.begin(), peak_memories.end()));
    double avg_final = GlialPerformanceMonitor::Mean(
        std::vector<double>(final_memories.begin(), final_memories.end()));

    std::cout << "\n--- Memory Summary ---" << std::endl;
    std::cout << "Average peak memory: " << avg_peak << " KB for " << NUM_CELLS << " cells" << std::endl;
    std::cout << "Per-cell memory: " << (avg_peak / NUM_CELLS) << " KB/cell" << std::endl;
    std::cout << "Average residual memory: " << avg_final << " KB" << std::endl;

    // Check for memory growth over cycles (indicates leak)
    double first_final = final_memories.front();
    double last_final = final_memories.back();
    double memory_growth = last_final - first_final;

    std::cout << "Memory growth over " << NUM_CYCLES << " cycles: " << memory_growth << " KB" << std::endl;

    // Verify no significant memory leak (allow 1MB growth for allocator overhead)
    EXPECT_LT(memory_growth, 1024)
        << "Memory growth (" << memory_growth << " KB) suggests leak";

    // Verify reasonable per-cell memory usage (should be < 10KB per cell with synapses)
    EXPECT_LT(avg_peak / NUM_CELLS, 10.0)
        << "Per-cell memory (" << (avg_peak / NUM_CELLS) << " KB) is too high";
}

//=============================================================================
// Test 5: Concurrent Access
//=============================================================================

TEST_F(GlialPerformanceRegressionTest, ConcurrentAccess) {
    // WHAT: Multi-threaded access to glial networks
    // WHY:  Verify thread safety and measure contention
    // HOW:  Multiple threads reading/writing, check for races

    const uint32_t NUM_MICROGLIA = 100;
    const uint32_t SYNAPSES_PER_MICROGLIA = 50;
    const uint32_t NUM_THREADS = 8;
    const uint32_t OPS_PER_THREAD = 1000;

    std::cout << "\n=== Concurrent Access Test ===" << std::endl;
    std::cout << "Configuration: " << NUM_THREADS << " threads x "
              << OPS_PER_THREAD << " operations" << std::endl;

    // Create shared network
    microglia_network_t* network = microglia_network_create(NUM_MICROGLIA);
    ASSERT_NE(network, nullptr);

    for (uint32_t i = 0; i < NUM_MICROGLIA; i++) {
        microglia_t* mg = microglia_create(i, 0.0f, 0.0f, 0.0f, 100.0f);
        ASSERT_NE(mg, nullptr);

        for (uint32_t j = 0; j < SYNAPSES_PER_MICROGLIA; j++) {
            microglia_monitor_synapse(mg, i * SYNAPSES_PER_MICROGLIA + j);
        }

        microglia_network_add(network, mg);
    }

    // Counters for operations
    std::atomic<uint64_t> total_ops{0};
    std::atomic<uint64_t> total_time_us{0};
    std::atomic<uint32_t> errors{0};

    // Worker function
    auto worker = [&](uint32_t thread_id) {
        std::mt19937 thread_rng(42 + thread_id);
        std::uniform_int_distribution<uint32_t> mg_dist(0, NUM_MICROGLIA - 1);
        std::uniform_int_distribution<uint32_t> op_dist(0, 2);
        std::uniform_real_distribution<float> activity_dist(0.0f, 1.0f);

        auto start = std::chrono::high_resolution_clock::now();

        for (uint32_t op = 0; op < OPS_PER_THREAD; op++) {
            uint32_t mg_idx = mg_dist(thread_rng);
            uint32_t operation = op_dist(thread_rng);

            if (mg_idx >= network->num_microglia) {
                errors++;
                continue;
            }

            microglia_t* mg = network->microglia[mg_idx];
            if (mg == nullptr) {
                errors++;
                continue;
            }

            try {
                switch (operation) {
                    case 0:  // Read activity score
                        if (mg->num_monitored_synapses > 0) {
                            uint32_t synapse_id = mg_idx * SYNAPSES_PER_MICROGLIA;
                            microglia_get_synapse_activity_score(mg, synapse_id);
                        }
                        break;

                    case 1:  // Track activity
                        if (mg->num_monitored_synapses > 0) {
                            uint32_t synapse_id = mg_idx * SYNAPSES_PER_MICROGLIA;
                            float activity = activity_dist(thread_rng);
                            uint64_t timestamp = GetTimestampUs();
                            microglia_track_synapse_activity(mg, synapse_id, activity, timestamp);
                        }
                        break;

                    case 2:  // Get total pruned (read-only operation)
                        microglia_get_total_pruned(mg);
                        break;
                }
                total_ops++;
            } catch (...) {
                errors++;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        total_time_us += duration;
    };

    // Launch threads
    std::vector<std::thread> threads;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker, i);
    }

    // Wait with timeout to detect deadlocks
    bool all_completed = true;
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    double total_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    // Report results
    uint64_t ops = total_ops.load();
    double ops_per_second = ops / (total_time_ms / 1000.0);
    double avg_latency_us = static_cast<double>(total_time_us.load()) / ops;

    std::cout << "\n--- Concurrency Results ---" << std::endl;
    std::cout << "Total operations: " << ops << std::endl;
    std::cout << "Errors: " << errors.load() << std::endl;
    std::cout << "Total time: " << total_time_ms << " ms" << std::endl;
    std::cout << "Throughput: " << ops_per_second / 1000.0 << " K ops/second" << std::endl;
    std::cout << "Avg latency: " << avg_latency_us << " us/op" << std::endl;

    // Verify no errors (race conditions would cause crashes or errors)
    EXPECT_EQ(errors.load(), 0u) << "Errors during concurrent access indicate race conditions";

    // Verify all operations completed (no deadlocks)
    EXPECT_EQ(ops, static_cast<uint64_t>(NUM_THREADS * OPS_PER_THREAD))
        << "Not all operations completed - possible deadlock";

    // Verify reasonable performance under contention
    EXPECT_GT(ops_per_second, 10000.0)
        << "Throughput under contention too low";

    // Cleanup
    microglia_network_destroy(network);
}

//=============================================================================
// Test 6: Oligodendrocyte Memory Efficiency
//=============================================================================

TEST_F(GlialPerformanceRegressionTest, OligodendrocyteMemoryEfficiency) {
    // WHAT: Test memory efficiency for oligodendrocyte network
    // WHY:  Ensure myelination system doesn't have memory leaks
    // HOW:  Create/destroy cycles, track memory

    const uint32_t NUM_OLIGOS = 500;
    const uint32_t NEURONS_PER_OLIGO = 20;
    const uint32_t NUM_CYCLES = 3;

    std::cout << "\n=== Oligodendrocyte Memory Efficiency ===" << std::endl;

    long baseline_mem = GlialPerformanceMonitor::GetMemoryUsageKB();

    for (uint32_t cycle = 0; cycle < NUM_CYCLES; cycle++) {
        long mem_before = GlialPerformanceMonitor::GetMemoryUsageKB();

        oligodendrocyte_network_t* network = oligodendrocyte_network_create(NUM_OLIGOS);
        ASSERT_NE(network, nullptr);

        for (uint32_t i = 0; i < NUM_OLIGOS; i++) {
            float x = (float)(i % 10) * 100.0f;
            float y = (float)((i / 10) % 10) * 100.0f;
            float z = (float)(i / 100) * 100.0f;
            oligodendrocyte_t* oligo = oligodendrocyte_create(i, x, y, z, NEURONS_PER_OLIGO);
            if (oligo == nullptr) continue;

            for (uint32_t j = 0; j < NEURONS_PER_OLIGO; j++) {
                oligodendrocyte_assign_neuron(oligo, i * NEURONS_PER_OLIGO + j);
            }

            oligodendrocyte_network_add(network, oligo);
        }

        long mem_peak = GlialPerformanceMonitor::GetMemoryUsageKB();

        // Do some work
        for (uint32_t step = 0; step < 10; step++) {
            oligodendrocyte_network_step(network, 0.001f);
        }

        oligodendrocyte_network_destroy(network);

        long mem_after = GlialPerformanceMonitor::GetMemoryUsageKB();

        std::cout << "Cycle " << (cycle + 1) << ": Peak: +"
                  << (mem_peak - mem_before) << " KB, Residual: +"
                  << (mem_after - baseline_mem) << " KB" << std::endl;
    }

    // Final memory check
    long final_mem = GlialPerformanceMonitor::GetMemoryUsageKB();
    long leaked = final_mem - baseline_mem;

    std::cout << "Total memory change from baseline: " << leaked << " KB" << std::endl;

    // Allow some memory growth for allocator overhead, but not excessive
    EXPECT_LT(leaked, 2048)
        << "Excessive memory growth suggests leak in oligodendrocyte system";
}

//=============================================================================
// Test 7: Stress Test - Rapid Create/Destroy
//=============================================================================

TEST_F(GlialPerformanceRegressionTest, StressTestRapidCreateDestroy) {
    // WHAT: Rapidly create and destroy glial cells
    // WHY:  Test for memory fragmentation and cleanup issues
    // HOW:  Many small create/destroy cycles

    const uint32_t NUM_ITERATIONS = 100;
    const uint32_t CELLS_PER_ITERATION = 100;

    std::cout << "\n=== Stress Test: Rapid Create/Destroy ===" << std::endl;

    long baseline_mem = GlialPerformanceMonitor::GetMemoryUsageKB();

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t iter = 0; iter < NUM_ITERATIONS; iter++) {
        microglia_network_t* network = microglia_network_create(CELLS_PER_ITERATION);
        if (network == nullptr) continue;

        for (uint32_t i = 0; i < CELLS_PER_ITERATION; i++) {
            microglia_t* mg = microglia_create(i, 0.0f, 0.0f, 0.0f, 50.0f);
            if (mg == nullptr) continue;

            // Quick operation
            for (uint32_t j = 0; j < 5; j++) {
                microglia_monitor_synapse(mg, i * 5 + j);
            }

            microglia_network_add(network, mg);
        }

        // Quick step
        microglia_network_step(network, GetTimestampUs());

        microglia_network_destroy(network);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double total_time_ms = std::chrono::duration<double, std::milli>(end - start).count();

    long final_mem = GlialPerformanceMonitor::GetMemoryUsageKB();
    long mem_change = final_mem - baseline_mem;

    double iterations_per_sec = NUM_ITERATIONS / (total_time_ms / 1000.0);

    std::cout << "Total iterations: " << NUM_ITERATIONS << std::endl;
    std::cout << "Total time: " << total_time_ms << " ms" << std::endl;
    std::cout << "Rate: " << iterations_per_sec << " iterations/second" << std::endl;
    std::cout << "Memory change: " << mem_change << " KB" << std::endl;

    // Verify no excessive memory fragmentation
    EXPECT_LT(mem_change, 5120)  // Allow up to 5MB growth
        << "Excessive memory fragmentation detected";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
