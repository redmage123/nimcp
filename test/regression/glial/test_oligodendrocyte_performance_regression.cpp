/**
 * @file test_oligodendrocyte_performance_regression.cpp
 * @brief Performance regression tests for Enhanced Oligodendrocyte Module
 *
 * Verifies performance characteristics don't regress:
 * - Creation/destruction performance
 * - Network step throughput
 * - Spatial indexing performance
 * - RK4 integration overhead
 * - G-ratio optimization overhead
 * - Memory efficiency
 *
 * Baselines established from initial implementation.
 * Future changes should not significantly degrade these metrics.
 *
 * 11 regression tests
 */

#include <gtest/gtest.h>
#include <cmath>
#include <chrono>
#include <vector>
#include <numeric>
#include <random>

// Headers have their own extern "C" guards
#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// PERFORMANCE BASELINES (established from initial implementation)
//=============================================================================

namespace Baseline {
    // Creation time for 100 oligodendrocytes (ms)
    constexpr double CREATION_100_MS = 50.0;

    // Destruction time for 100 oligodendrocytes (ms) - higher due to internode cleanup
    constexpr double DESTRUCTION_100_MS = 200.0;

    // Network step time for 100 oligodendrocytes with 10 axons each (ms)
    constexpr double NETWORK_STEP_100X10_MS = 10.0;

    // Spatial index build time for 100 oligodendrocytes (ms)
    constexpr double SPATIAL_INDEX_BUILD_MS = 20.0;

    // Nearest neighbor query time per query (µs)
    constexpr double NEAREST_QUERY_US = 100.0;

    // RK4 step overhead per oligodendrocyte (µs)
    constexpr double RK4_STEP_US = 50.0;

    // G-ratio optimization overhead per oligodendrocyte (µs)
    constexpr double G_RATIO_OPT_US = 30.0;

    // Memory per oligodendrocyte with 20 axons (bytes)
    constexpr size_t MEMORY_PER_OLIGO = 20000;

    // Activity tracking throughput (ops/sec)
    constexpr double ACTIVITY_TRACK_OPS_SEC = 100000.0;

    // Tolerance for performance regression (20%)
    constexpr double REGRESSION_TOLERANCE = 1.2;
}

//=============================================================================
// TEST FIXTURE
//=============================================================================

class OligodendrocytePerformanceTest : public ::testing::Test {
protected:
    oligodendrocyte_network_t* network = nullptr;
    std::vector<oligodendrocyte_t*> oligos;

    void SetUp() override {
        network = nullptr;
        oligos.clear();
    }

    void TearDown() override {
        if (network) {
            oligodendrocyte_network_destroy(network);
            network = nullptr;
        }
        for (auto* o : oligos) {
            if (o) oligodendrocyte_destroy(o);
        }
        oligos.clear();
    }

    double measureTimeMs(std::function<void()> fn) {
        auto start = std::chrono::high_resolution_clock::now();
        fn();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    double measureTimeUs(std::function<void()> fn) {
        auto start = std::chrono::high_resolution_clock::now();
        fn();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::micro>(end - start).count();
    }
};

//=============================================================================
// REGRESSION TEST 1: Creation Performance
//=============================================================================

TEST_F(OligodendrocytePerformanceTest, CreationPerformance) {
    const int count = 100;

    double time_ms = measureTimeMs([&]() {
        for (int i = 0; i < count; i++) {
            oligodendrocyte_t* o = oligodendrocyte_create(i, i * 10.0f, i * 10.0f, i * 10.0f, 20);
            ASSERT_NE(o, nullptr);
            oligos.push_back(o);
        }
    });

    std::cout << "Creation time for " << count << " oligodendrocytes: " << time_ms << " ms" << std::endl;
    EXPECT_LT(time_ms, Baseline::CREATION_100_MS * Baseline::REGRESSION_TOLERANCE);
}

//=============================================================================
// REGRESSION TEST 2: Destruction Performance
//=============================================================================

TEST_F(OligodendrocytePerformanceTest, DestructionPerformance) {
    const int count = 100;

    // Create first
    for (int i = 0; i < count; i++) {
        oligodendrocyte_t* o = oligodendrocyte_create(i, 0, 0, 0, 20);
        oligos.push_back(o);
    }

    double time_ms = measureTimeMs([&]() {
        for (auto* o : oligos) {
            oligodendrocyte_destroy(o);
        }
    });

    oligos.clear();  // Don't double-free in TearDown

    std::cout << "Destruction time for " << count << " oligodendrocytes: " << time_ms << " ms" << std::endl;
    EXPECT_LT(time_ms, Baseline::DESTRUCTION_100_MS * Baseline::REGRESSION_TOLERANCE);
}

//=============================================================================
// REGRESSION TEST 3: Network Step Performance
//=============================================================================

TEST_F(OligodendrocytePerformanceTest, NetworkStepPerformance) {
    network = oligodendrocyte_network_create(100);
    ASSERT_NE(network, nullptr);

    // Create 100 oligodendrocytes with 10 axons each
    for (int i = 0; i < 100; i++) {
        oligodendrocyte_t* o = oligodendrocyte_create(i, i * 10.0f, 0, 0, 20);
        ASSERT_NE(o, nullptr);

        for (int j = 0; j < 10; j++) {
            oligodendrocyte_assign_neuron(o, i * 100 + j);
            oligodendrocyte_track_activity(o, i * 100 + j, 5.0f, nimcp_time_monotonic_us());
        }

        oligodendrocyte_network_add(network, o);
    }

    // Warm up
    oligodendrocyte_network_step(network, 0.1f);

    // Measure
    double time_ms = measureTimeMs([&]() {
        oligodendrocyte_network_step(network, 0.1f);
    });

    std::cout << "Network step time (100 oligos, 10 axons each): " << time_ms << " ms" << std::endl;
    EXPECT_LT(time_ms, Baseline::NETWORK_STEP_100X10_MS * Baseline::REGRESSION_TOLERANCE);
}

//=============================================================================
// REGRESSION TEST 4: Spatial Index Build Performance
//=============================================================================

TEST_F(OligodendrocytePerformanceTest, SpatialIndexBuildPerformance) {
    network = oligodendrocyte_network_create(100);
    ASSERT_NE(network, nullptr);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0, 10000);

    for (int i = 0; i < 100; i++) {
        oligodendrocyte_t* o = oligodendrocyte_create(i, dist(gen), dist(gen), dist(gen), 20);
        oligodendrocyte_network_add(network, o);
    }

    double time_ms = measureTimeMs([&]() {
        oligodendrocyte_network_rebuild_spatial_index(network);
    });

    std::cout << "Spatial index build time (100 oligos): " << time_ms << " ms" << std::endl;
    EXPECT_LT(time_ms, Baseline::SPATIAL_INDEX_BUILD_MS * Baseline::REGRESSION_TOLERANCE);
}

//=============================================================================
// REGRESSION TEST 5: Nearest Neighbor Query Performance
//=============================================================================

TEST_F(OligodendrocytePerformanceTest, NearestNeighborQueryPerformance) {
    network = oligodendrocyte_network_create(100);
    ASSERT_NE(network, nullptr);

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(0, 10000);

    for (int i = 0; i < 100; i++) {
        oligodendrocyte_t* o = oligodendrocyte_create(i, dist(gen), dist(gen), dist(gen), 20);
        oligodendrocyte_network_add(network, o);
    }

    oligodendrocyte_network_rebuild_spatial_index(network);

    const int queries = 1000;
    double total_time_us = 0;

    for (int i = 0; i < queries; i++) {
        float x = dist(gen);
        float y = dist(gen);
        float z = dist(gen);

        total_time_us += measureTimeUs([&]() {
            oligodendrocyte_network_find_nearest(network, x, y, z);
        });
    }

    double avg_time_us = total_time_us / queries;

    std::cout << "Avg nearest neighbor query time: " << avg_time_us << " µs" << std::endl;
    EXPECT_LT(avg_time_us, Baseline::NEAREST_QUERY_US * Baseline::REGRESSION_TOLERANCE);
}

//=============================================================================
// REGRESSION TEST 6: RK4 Step Overhead
//=============================================================================

TEST_F(OligodendrocytePerformanceTest, RK4StepOverhead) {
    oligodendrocyte_t* o = oligodendrocyte_create(1, 0, 0, 0, 20);
    ASSERT_NE(o, nullptr);
    oligos.push_back(o);

    // Assign axons and track activity
    for (int i = 0; i < 10; i++) {
        oligodendrocyte_assign_neuron(o, i);
        oligodendrocyte_track_activity(o, i, 5.0f, nimcp_time_monotonic_us());
    }

    // Warm up
    oligodendrocyte_update_state_dynamics(o, 0.1f);

    const int iterations = 1000;
    double time_us = measureTimeUs([&]() {
        for (int i = 0; i < iterations; i++) {
            oligodendrocyte_update_state_dynamics(o, 0.001f);
        }
    });

    double avg_time_us = time_us / iterations;

    std::cout << "Avg RK4 step time: " << avg_time_us << " µs" << std::endl;
    EXPECT_LT(avg_time_us, Baseline::RK4_STEP_US * Baseline::REGRESSION_TOLERANCE);
}

//=============================================================================
// REGRESSION TEST 7: G-Ratio Optimization Overhead
//=============================================================================

TEST_F(OligodendrocytePerformanceTest, GRatioOptimizationOverhead) {
    oligodendrocyte_t* o = oligodendrocyte_create(1, 0, 0, 0, 20);
    ASSERT_NE(o, nullptr);
    oligos.push_back(o);

    // Assign myelinated axons
    for (int i = 0; i < 10; i++) {
        oligodendrocyte_assign_axon_at(o, i, 0, 0, 0, 2.0f, 500.0f);
        oligodendrocyte_set_myelination_level(o, i, 0.8f);
    }

    // Warm up
    oligodendrocyte_optimize_g_ratios(o, 0.1f);

    const int iterations = 1000;
    double time_us = measureTimeUs([&]() {
        for (int i = 0; i < iterations; i++) {
            oligodendrocyte_optimize_g_ratios(o, 0.001f);
        }
    });

    double avg_time_us = time_us / iterations;

    std::cout << "Avg G-ratio optimization time: " << avg_time_us << " µs" << std::endl;
    EXPECT_LT(avg_time_us, Baseline::G_RATIO_OPT_US * Baseline::REGRESSION_TOLERANCE);
}

//=============================================================================
// REGRESSION TEST 8: Activity Tracking Throughput
//=============================================================================

TEST_F(OligodendrocytePerformanceTest, ActivityTrackingThroughput) {
    oligodendrocyte_t* o = oligodendrocyte_create(1, 0, 0, 0, 20);
    ASSERT_NE(o, nullptr);
    oligos.push_back(o);

    for (int i = 0; i < 10; i++) {
        oligodendrocyte_assign_neuron(o, i);
    }

    const int operations = 100000;
    uint64_t timestamp = nimcp_time_monotonic_us();

    double time_ms = measureTimeMs([&]() {
        for (int i = 0; i < operations; i++) {
            oligodendrocyte_track_activity(o, i % 10, 5.0f, timestamp + i);
        }
    });

    double ops_per_sec = operations / (time_ms / 1000.0);

    std::cout << "Activity tracking throughput: " << ops_per_sec << " ops/sec" << std::endl;
    EXPECT_GT(ops_per_sec, Baseline::ACTIVITY_TRACK_OPS_SEC / Baseline::REGRESSION_TOLERANCE);
}

//=============================================================================
// REGRESSION TEST 9: Remodeling Performance
//=============================================================================

TEST_F(OligodendrocytePerformanceTest, RemodelingPerformance) {
    oligodendrocyte_t* o = oligodendrocyte_create(1, 0, 0, 0, 20);
    ASSERT_NE(o, nullptr);
    oligos.push_back(o);

    for (int i = 0; i < 20; i++) {
        oligodendrocyte_assign_neuron(o, i);
        oligodendrocyte_track_activity(o, i, 10.0f, nimcp_time_monotonic_us());
    }

    // Warm up
    oligodendrocyte_remodel_myelination(o, 0.1f);

    const int iterations = 1000;
    double time_us = measureTimeUs([&]() {
        for (int i = 0; i < iterations; i++) {
            oligodendrocyte_remodel_myelination(o, 0.001f);
        }
    });

    double avg_time_us = time_us / iterations;

    std::cout << "Avg remodeling time (20 axons): " << avg_time_us << " µs" << std::endl;
    EXPECT_LT(avg_time_us, 100.0 * Baseline::REGRESSION_TOLERANCE);  // 100µs baseline
}

//=============================================================================
// REGRESSION TEST 10: Lactate Shuttle Performance
//=============================================================================

TEST_F(OligodendrocytePerformanceTest, LactateShuttlePerformance) {
    oligodendrocyte_t* o = oligodendrocyte_create(1, 0, 0, 0, 20);
    ASSERT_NE(o, nullptr);
    oligos.push_back(o);

    for (int i = 0; i < 20; i++) {
        oligodendrocyte_assign_neuron(o, i);
        oligodendrocyte_set_axon_demand(o, i, 1.0f + i * 0.1f);
    }

    // Warm up
    oligodendrocyte_update_lactate_shuttle(o, 0.1f);

    const int iterations = 1000;
    double time_us = measureTimeUs([&]() {
        for (int i = 0; i < iterations; i++) {
            oligodendrocyte_update_lactate_shuttle(o, 0.001f);
        }
    });

    double avg_time_us = time_us / iterations;

    std::cout << "Avg lactate shuttle update time: " << avg_time_us << " µs" << std::endl;
    EXPECT_LT(avg_time_us, 50.0 * Baseline::REGRESSION_TOLERANCE);  // 50µs baseline
}

//=============================================================================
// REGRESSION TEST 11: Full Network Update Throughput
//=============================================================================

TEST_F(OligodendrocytePerformanceTest, FullNetworkThroughput) {
    network = oligodendrocyte_network_create(50);
    ASSERT_NE(network, nullptr);

    // Create realistic network
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> pos_dist(0, 1000);

    for (int i = 0; i < 50; i++) {
        oligodendrocyte_t* o = oligodendrocyte_create(i, pos_dist(gen), pos_dist(gen), pos_dist(gen), 20);

        for (int j = 0; j < 15; j++) {
            oligodendrocyte_assign_axon_at(o, i * 100 + j, 0, 0, 0, 1.5f + j * 0.2f, 500.0f);
            oligodendrocyte_track_activity(o, i * 100 + j, 5.0f + j, nimcp_time_monotonic_us());
            oligodendrocyte_set_myelination_level(o, i * 100 + j, 0.3f + j * 0.02f);
        }

        oligodendrocyte_network_add(network, o);
    }

    oligodendrocyte_network_rebuild_spatial_index(network);

    // Warm up
    oligodendrocyte_network_step(network, 0.1f);

    // Measure sustained throughput
    const int steps = 100;
    double time_ms = measureTimeMs([&]() {
        for (int i = 0; i < steps; i++) {
            oligodendrocyte_network_step(network, 0.01f);
        }
    });

    double steps_per_sec = steps / (time_ms / 1000.0);

    std::cout << "Network update throughput (50 oligos, 15 axons): " << steps_per_sec << " steps/sec" << std::endl;
    EXPECT_GT(steps_per_sec, 100.0);  // At least 100 steps/sec (real-time capable)
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
