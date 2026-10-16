//=============================================================================
// test_myelin_performance_regression.cpp - Performance Regression Tests
//=============================================================================
/**
 * @file test_myelin_performance_regression.cpp
 * @brief Performance regression tests for myelin sheath module
 *
 * WHAT: Performance benchmarks to detect performance regressions
 * WHY:  Ensure myelin sheath operations remain efficient
 * HOW:  Measure timing and throughput for critical operations
 *
 * TEST CATEGORIES:
 * 1. Memory Pool Performance - O(1) allocation verification
 * 2. Sheath Creation Performance - Creation throughput
 * 3. Network Operations Performance - Add, find, step operations
 * 4. Simulation Step Performance - Per-step timing
 * 5. Scalability Tests - Performance at different scales
 *
 * PERFORMANCE TARGETS:
 * - Pool allocation: < 1μs average
 * - Sheath creation: < 100μs per sheath
 * - Network add: < 10μs per sheath
 * - Network find: < 1μs per lookup
 * - Simulation step: < 100μs for 1000 sheaths
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-11-25
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>

// Headers have their own extern "C" guards
#include "glial/myelin_sheath/nimcp_myelin_sheath.h"
#include "utils/memory/nimcp_memory.h"
#include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MyelinPerformanceRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_memory_init();
        nimcp_init();
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
};

//=============================================================================
// 1. Memory Pool Performance Tests
//=============================================================================

TEST_F(MyelinPerformanceRegressionTest, SheathPool_AllocationSpeed) {
    myelin_sheath_pool_t* pool = myelin_sheath_pool_create(1024);
    ASSERT_NE(pool, nullptr);

    const int NUM_ALLOCS = 1000;
    std::vector<myelin_sheath_t*> sheaths;
    sheaths.reserve(NUM_ALLOCS);

    // Measure allocation time
    double alloc_time = MeasureTime([&]() {
        for (int i = 0; i < NUM_ALLOCS; i++) {
            sheaths.push_back(myelin_sheath_pool_alloc(pool));
        }
    });

    double avg_alloc = alloc_time / NUM_ALLOCS;
    printf("  Pool allocation: %.2f μs average (target: < 1 μs)\n", avg_alloc);

    // Performance target: < 1μs per allocation
    EXPECT_LT(avg_alloc, 5.0) << "Pool allocation too slow";

    // Measure free time
    double free_time = MeasureTime([&]() {
        for (auto* sheath : sheaths) {
            myelin_sheath_pool_free(pool, sheath);
        }
    });

    double avg_free = free_time / NUM_ALLOCS;
    printf("  Pool free: %.2f μs average (target: < 1 μs)\n", avg_free);

    EXPECT_LT(avg_free, 5.0) << "Pool free too slow";

    myelin_sheath_pool_destroy(pool);
}

TEST_F(MyelinPerformanceRegressionTest, SegmentPool_AllocationSpeed) {
    myelin_segment_pool_t* pool = myelin_segment_pool_create(4096);
    ASSERT_NE(pool, nullptr);

    const int NUM_ALLOCS = 2000;
    std::vector<myelin_segment_t*> segments;
    segments.reserve(NUM_ALLOCS);

    double alloc_time = MeasureTime([&]() {
        for (int i = 0; i < NUM_ALLOCS; i++) {
            segments.push_back(myelin_segment_pool_alloc(pool));
        }
    });

    double avg_alloc = alloc_time / NUM_ALLOCS;
    printf("  Segment pool allocation: %.2f μs average\n", avg_alloc);

    EXPECT_LT(avg_alloc, 5.0) << "Segment pool allocation too slow";

    for (auto* seg : segments) {
        myelin_segment_pool_free(pool, seg);
    }

    myelin_segment_pool_destroy(pool);
}

//=============================================================================
// 2. Sheath Creation Performance Tests
//=============================================================================

TEST_F(MyelinPerformanceRegressionTest, SheathCreation_Basic) {
    const int NUM_SHEATHS = 100;
    std::vector<myelin_sheath_t*> sheaths;

    double create_time = MeasureTime([&]() {
        for (int i = 0; i < NUM_SHEATHS; i++) {
            sheaths.push_back(myelin_sheath_create(i, i + 100, 50, 16));
        }
    });

    double avg_create = create_time / NUM_SHEATHS;
    printf("  Basic sheath creation: %.2f μs average (target: < 50 μs)\n", avg_create);

    EXPECT_LT(avg_create, 100.0) << "Sheath creation too slow";

    for (auto* sheath : sheaths) {
        myelin_sheath_destroy(sheath);
    }
}

TEST_F(MyelinPerformanceRegressionTest, SheathCreation_ForAxon) {
    const int NUM_SHEATHS = 100;
    std::vector<myelin_sheath_t*> sheaths;

    double create_time = MeasureTime([&]() {
        for (int i = 0; i < NUM_SHEATHS; i++) {
            sheaths.push_back(myelin_sheath_create_for_axon(
                i, i + 100, 50, 1000.0f, 2.0f, 16));
        }
    });

    double avg_create = create_time / NUM_SHEATHS;
    printf("  Sheath creation for axon: %.2f μs average (target: < 100 μs)\n", avg_create);

    EXPECT_LT(avg_create, 200.0) << "Sheath creation for axon too slow";

    for (auto* sheath : sheaths) {
        myelin_sheath_destroy(sheath);
    }
}

TEST_F(MyelinPerformanceRegressionTest, SegmentAddition_Speed) {
    myelin_sheath_t* sheath = myelin_sheath_create(1, 100, 50, 100);
    ASSERT_NE(sheath, nullptr);

    const int NUM_SEGMENTS = 50;

    double add_time = MeasureTime([&]() {
        for (int i = 0; i < NUM_SEGMENTS; i++) {
            myelin_sheath_add_segment(sheath, (float)i * 100.0f, 100.0f, 2.0f);
        }
    });

    double avg_add = add_time / NUM_SEGMENTS;
    printf("  Segment addition: %.2f μs average (target: < 20 μs)\n", avg_add);

    EXPECT_LT(avg_add, 50.0) << "Segment addition too slow";

    myelin_sheath_destroy(sheath);
}

//=============================================================================
// 3. Network Operations Performance Tests
//=============================================================================

TEST_F(MyelinPerformanceRegressionTest, NetworkAdd_Speed) {
    myelin_sheath_network_t* network = myelin_network_create_default(1000);
    ASSERT_NE(network, nullptr);

    const int NUM_SHEATHS = 500;
    std::vector<myelin_sheath_t*> sheaths;
    for (int i = 0; i < NUM_SHEATHS; i++) {
        sheaths.push_back(myelin_sheath_create(i, i + 100, 50, 8));
    }

    double add_time = MeasureTime([&]() {
        for (auto* sheath : sheaths) {
            myelin_network_add_sheath(network, sheath);
        }
    });

    double avg_add = add_time / NUM_SHEATHS;
    printf("  Network add: %.2f μs average (target: < 10 μs)\n", avg_add);

    EXPECT_LT(avg_add, 20.0) << "Network add too slow";

    myelin_network_destroy(network);
}

TEST_F(MyelinPerformanceRegressionTest, NetworkFind_Speed) {
    myelin_sheath_network_t* network = myelin_network_create_default(1000);
    ASSERT_NE(network, nullptr);

    // Add sheaths
    const int NUM_SHEATHS = 500;
    for (int i = 0; i < NUM_SHEATHS; i++) {
        myelin_sheath_t* sheath = myelin_sheath_create(i, i + 100, 50, 8);
        myelin_network_add_sheath(network, sheath);
    }

    // Measure find by ID
    const int NUM_LOOKUPS = 1000;
    double find_time = MeasureTime([&]() {
        for (int i = 0; i < NUM_LOOKUPS; i++) {
            myelin_network_find_sheath(network, i % NUM_SHEATHS);
        }
    });

    double avg_find = find_time / NUM_LOOKUPS;
    printf("  Network find by ID: %.2f μs average (target: < 5 μs)\n", avg_find);

    EXPECT_LT(avg_find, 10.0) << "Network find too slow";

    // Measure find by axon
    double find_axon_time = MeasureTime([&]() {
        for (int i = 0; i < NUM_LOOKUPS; i++) {
            myelin_network_find_by_axon(network, (i % NUM_SHEATHS) + 100);
        }
    });

    double avg_find_axon = find_axon_time / NUM_LOOKUPS;
    printf("  Network find by axon: %.2f μs average (target: < 5 μs)\n", avg_find_axon);

    EXPECT_LT(avg_find_axon, 10.0) << "Network find by axon too slow";

    myelin_network_destroy(network);
}

//=============================================================================
// 4. Simulation Step Performance Tests
//=============================================================================

TEST_F(MyelinPerformanceRegressionTest, SimulationStep_SingleSheath) {
    myelin_sheath_t* sheath = myelin_sheath_create_for_axon(1, 100, 50, 1000.0f, 2.0f, 16);
    ASSERT_NE(sheath, nullptr);

    const int NUM_STEPS = 1000;
    uint64_t time = 0;

    double step_time = MeasureTime([&]() {
        for (int i = 0; i < NUM_STEPS; i++) {
            myelin_sheath_step(sheath, 0.001f, time);
            time += 1000;
        }
    });

    double avg_step = step_time / NUM_STEPS;
    printf("  Single sheath step: %.2f μs average (target: < 10 μs)\n", avg_step);

    EXPECT_LT(avg_step, 50.0) << "Single sheath step too slow";

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinPerformanceRegressionTest, SimulationStep_Network) {
    myelin_sheath_network_t* network = myelin_network_create_default(1000);
    ASSERT_NE(network, nullptr);

    // Add 100 sheaths with segments
    const int NUM_SHEATHS = 100;
    for (int i = 0; i < NUM_SHEATHS; i++) {
        myelin_sheath_t* sheath = myelin_sheath_create_for_axon(
            i, i + 100, 50, 500.0f, 1.5f, 8);
        myelin_network_add_sheath(network, sheath);
    }

    const int NUM_STEPS = 100;
    uint64_t time = 0;

    double step_time = MeasureTime([&]() {
        for (int i = 0; i < NUM_STEPS; i++) {
            myelin_network_step(network, 0.001f, time);
            time += 1000;
        }
    });

    double avg_step = step_time / NUM_STEPS;
    printf("  Network step (%d sheaths): %.2f μs average (target: < 1000 μs)\n",
           NUM_SHEATHS, avg_step);

    EXPECT_LT(avg_step, 5000.0) << "Network step too slow";

    myelin_network_destroy(network);
}

TEST_F(MyelinPerformanceRegressionTest, SimulationStep_LargeNetwork) {
    myelin_sheath_network_t* network = myelin_network_create_default(2000);
    ASSERT_NE(network, nullptr);

    // Add 500 sheaths
    const int NUM_SHEATHS = 500;
    for (int i = 0; i < NUM_SHEATHS; i++) {
        myelin_sheath_t* sheath = myelin_sheath_create_for_axon(
            i, i + 100, i % 100, 300.0f, 1.0f, 4);
        myelin_network_add_sheath(network, sheath);
    }

    const int NUM_STEPS = 10;
    uint64_t time = 0;

    double step_time = MeasureTime([&]() {
        for (int i = 0; i < NUM_STEPS; i++) {
            myelin_network_step(network, 0.001f, time);
            time += 1000;
        }
    });

    double avg_step = step_time / NUM_STEPS;
    printf("  Large network step (%d sheaths): %.2f μs average\n",
           NUM_SHEATHS, avg_step);

    // Throughput
    double sheaths_per_sec = (NUM_SHEATHS * NUM_STEPS) / (step_time * 1e-6);
    printf("  Throughput: %.0f sheath-steps/sec\n", sheaths_per_sec);

    EXPECT_GT(sheaths_per_sec, 10000.0) << "Throughput too low";

    myelin_network_destroy(network);
}

//=============================================================================
// 5. Scalability Tests
//=============================================================================

TEST_F(MyelinPerformanceRegressionTest, Scalability_LinearGrowth) {
    std::vector<int> sizes = {10, 50, 100, 200, 500};
    std::vector<double> times;

    for (int size : sizes) {
        myelin_sheath_network_t* network = myelin_network_create_default(size + 100);

        for (int i = 0; i < size; i++) {
            myelin_sheath_t* sheath = myelin_sheath_create_for_axon(
                i, i + 100, 50, 200.0f, 1.0f, 4);
            myelin_network_add_sheath(network, sheath);
        }

        uint64_t time = 0;
        double step_time = MeasureTime([&]() {
            myelin_network_step(network, 0.001f, time);
        });

        times.push_back(step_time);
        printf("  Size %d: %.2f μs per step\n", size, step_time);

        myelin_network_destroy(network);
    }

    // Check for reasonable scaling (should be roughly linear)
    // Allow 3x growth factor for 50x size increase
    double ratio = times.back() / times.front();
    double size_ratio = (double)sizes.back() / sizes.front();
    double scaling_factor = ratio / size_ratio;

    printf("  Scaling factor: %.2f (ideal: 1.0, acceptable: < 2.0)\n", scaling_factor);

    EXPECT_LT(scaling_factor, 3.0) << "Scaling worse than expected";
}

//=============================================================================
// 6. Memory Efficiency Tests
//=============================================================================

TEST_F(MyelinPerformanceRegressionTest, MemoryEfficiency_PoolReuse) {
    myelin_sheath_pool_t* pool = myelin_sheath_pool_create(256);
    ASSERT_NE(pool, nullptr);

    // Allocate and free many times
    const int CYCLES = 1000;
    for (int cycle = 0; cycle < CYCLES; cycle++) {
        std::vector<myelin_sheath_t*> sheaths;
        for (int i = 0; i < 100; i++) {
            sheaths.push_back(myelin_sheath_pool_alloc(pool));
        }
        for (auto* sheath : sheaths) {
            myelin_sheath_pool_free(pool, sheath);
        }
    }

    // Pool should still be functional
    myelin_sheath_t* final_alloc = myelin_sheath_pool_alloc(pool);
    EXPECT_NE(final_alloc, nullptr);
    EXPECT_EQ(pool->allocated_count, 1u);

    myelin_sheath_pool_destroy(pool);
}

//=============================================================================
// 7. Operation Throughput Tests
//=============================================================================

TEST_F(MyelinPerformanceRegressionTest, Throughput_Myelination) {
    myelin_sheath_t* sheath = myelin_sheath_create_for_axon(1, 100, 50, 1000.0f, 2.0f, 20);
    ASSERT_NE(sheath, nullptr);

    const int NUM_OPS = 1000;

    double myelinate_time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            myelin_sheath_myelinate(sheath, 1.0f, 0.001f);
        }
    });

    double ops_per_sec = NUM_OPS / (myelinate_time * 1e-6);
    printf("  Myelination throughput: %.0f ops/sec\n", ops_per_sec);

    EXPECT_GT(ops_per_sec, 10000.0) << "Myelination throughput too low";

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinPerformanceRegressionTest, Throughput_ConductionUpdate) {
    myelin_sheath_t* sheath = myelin_sheath_create_for_axon(1, 100, 50, 1000.0f, 2.0f, 20);
    ASSERT_NE(sheath, nullptr);

    const int NUM_OPS = 10000;

    double update_time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            myelin_sheath_update_conduction(sheath);
        }
    });

    double ops_per_sec = NUM_OPS / (update_time * 1e-6);
    printf("  Conduction update throughput: %.0f ops/sec\n", ops_per_sec);

    EXPECT_GT(ops_per_sec, 50000.0) << "Conduction update throughput too low";

    myelin_sheath_destroy(sheath);
}

TEST_F(MyelinPerformanceRegressionTest, Throughput_HealthUpdate) {
    myelin_sheath_t* sheath = myelin_sheath_create_for_axon(1, 100, 50, 1000.0f, 2.0f, 20);
    ASSERT_NE(sheath, nullptr);

    const int NUM_OPS = 10000;

    double update_time = MeasureTime([&]() {
        for (int i = 0; i < NUM_OPS; i++) {
            myelin_sheath_update_health(sheath);
        }
    });

    double ops_per_sec = NUM_OPS / (update_time * 1e-6);
    printf("  Health update throughput: %.0f ops/sec\n", ops_per_sec);

    EXPECT_GT(ops_per_sec, 50000.0) << "Health update throughput too low";

    myelin_sheath_destroy(sheath);
}

//=============================================================================
// 8. CoW Performance Tests
//=============================================================================

TEST_F(MyelinPerformanceRegressionTest, CoW_CopySpeed) {
    myelin_sheath_t* original = myelin_sheath_create_for_axon(1, 100, 50, 1000.0f, 2.0f, 20);
    ASSERT_NE(original, nullptr);

    const int NUM_COPIES = 1000;
    std::vector<myelin_sheath_t*> copies;
    copies.reserve(NUM_COPIES);

    double copy_time = MeasureTime([&]() {
        for (int i = 0; i < NUM_COPIES; i++) {
            copies.push_back(myelin_sheath_cow_copy(original));
        }
    });

    double avg_copy = copy_time / NUM_COPIES;
    printf("  CoW copy: %.2f μs average (target: < 5 μs)\n", avg_copy);

    EXPECT_LT(avg_copy, 20.0) << "CoW copy too slow";

    // Cleanup
    for (auto* copy : copies) {
        myelin_sheath_cow_release(copy);
    }
    myelin_sheath_destroy(original);
}

TEST_F(MyelinPerformanceRegressionTest, CoW_PrepareWriteSpeed) {
    myelin_sheath_t* original = myelin_sheath_create_for_axon(1, 100, 50, 1000.0f, 2.0f, 20);
    ASSERT_NE(original, nullptr);

    const int NUM_OPS = 100;
    double total_time = 0.0;

    for (int i = 0; i < NUM_OPS; i++) {
        myelin_sheath_t* copy = myelin_sheath_cow_copy(original);
        total_time += MeasureTime([&]() {
            myelin_sheath_cow_prepare_write(copy);
        });
        myelin_sheath_destroy(copy);
    }

    double avg_prepare = total_time / NUM_OPS;
    printf("  CoW prepare write: %.2f μs average (target: < 100 μs)\n", avg_prepare);

    EXPECT_LT(avg_prepare, 500.0) << "CoW prepare write too slow";

    myelin_sheath_destroy(original);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    printf("\n=== Myelin Sheath Performance Regression Tests ===\n\n");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
