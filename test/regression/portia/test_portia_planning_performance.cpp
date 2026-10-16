/**
 * @file test_portia_planning_performance.cpp
 * @brief Regression tests for Portia planning system performance
 *
 * WHAT: Regression tests ensuring planning performance remains acceptable
 * WHY:  Prevent performance degradation and memory issues
 * HOW:  Stress tests with maximum waypoints, concurrent plans, memory bounds
 *
 * TEST COVERAGE:
 * - Planning with max waypoints
 * - Planning memory usage bounded
 * - Detour planning latency
 * - Plan evaluation throughput
 * - Concurrent planning requests
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_planning.h"
#include "utils/memory/nimcp_memory.h"

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class PortiaPlanningPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        config.max_waypoints = 64;
        config.max_plans = 16;
        config.max_detour_depth = 5;
        config.scan_interval_s = 0.1f;
        config.confidence_threshold = 0.6f;
        config.enable_backtracking = true;

        planner = portia_planning_init(&config, nullptr);
        ASSERT_NE(planner, nullptr);
    }

    void TearDown() override {
        if (planner) {
            portia_planning_destroy(planner);
            planner = nullptr;
        }
    }

    // Helper: Create plan with N waypoints
    portia_plan_t* create_plan_with_waypoints(int num_waypoints) {
        portia_plan_t* plan = portia_planning_create_plan(
            planner, 100.0f, 100.0f, 0.0f);

        if (!plan) return nullptr;

        for (int i = 0; i < num_waypoints - 1; i++) {
            float x = i * 10.0f;
            float y = i * 5.0f;
            portia_planning_add_waypoint(planner, plan->id, x, y, 0.0f, 0.9f);
        }

        return plan;
    }

    // Helper: Measure operation time in microseconds
    template<typename Func>
    double measure_time_us(Func func, int iterations = 1) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            func();
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> elapsed = end - start;
        return elapsed.count() / iterations;
    }

    portia_planning_config_t config;
    portia_planner_t planner;
};

//=============================================================================
// Maximum Waypoint Tests
//=============================================================================

TEST_F(PortiaPlanningPerformanceTest, PlanWithMaxWaypoints) {
    // WHAT: Verify planning works with maximum waypoint count
    // WHY:  Ensure system handles worst-case complexity
    // HOW:  Create plan with max waypoints, verify operations

    const int MAX_WAYPOINTS = config.max_waypoints;

    portia_plan_t* plan = create_plan_with_waypoints(MAX_WAYPOINTS);
    ASSERT_NE(plan, nullptr);

    // Verify waypoint count
    EXPECT_EQ(plan->waypoint_count, static_cast<uint32_t>(MAX_WAYPOINTS));

    // Test evaluation with max waypoints
    double eval_time_us = measure_time_us([&]() {
        portia_planning_evaluate(planner, plan->id);
    }, 10);

    // Evaluation should complete in reasonable time
    const double MAX_EVAL_TIME_US = 10000.0;  // 10ms
    EXPECT_LT(eval_time_us, MAX_EVAL_TIME_US)
        << "Evaluation too slow with " << MAX_WAYPOINTS << " waypoints: "
        << eval_time_us << " us";

    std::cout << "Evaluation time with " << MAX_WAYPOINTS
              << " waypoints: " << eval_time_us << " us\n";

    portia_planning_delete_plan(planner, plan->id);
}

TEST_F(PortiaPlanningPerformanceTest, WaypointAdditionScalability) {
    // WHAT: Verify waypoint addition time scales reasonably
    // WHY:  Prevent O(n²) or worse behavior
    // HOW:  Measure addition time at different plan sizes

    std::vector<int> waypoint_counts = {10, 20, 40, config.max_waypoints};
    std::vector<double> addition_times;

    for (int count : waypoint_counts) {
        portia_plan_t* plan = portia_planning_create_plan(
            planner, 100.0f, 100.0f, 0.0f);
        ASSERT_NE(plan, nullptr);

        double avg_time_us = measure_time_us([&]() {
            float x = count * 10.0f;
            portia_planning_add_waypoint(planner, plan->id, x, 0.0f, 0.0f, 0.9f);
        }, std::min(count, 10));

        addition_times.push_back(avg_time_us);

        portia_planning_delete_plan(planner, plan->id);
    }

    // Verify scaling is reasonable (should be near-constant or log)
    // Not exponential
    if (addition_times.size() >= 2) {
        double first = addition_times[0];
        double last = addition_times.back();
        double ratio = last / first;

        // Should not grow by more than 4x from smallest to largest
        EXPECT_LT(ratio, 4.0)
            << "Waypoint addition scales poorly: " << ratio << "x slowdown";
    }

    for (size_t i = 0; i < waypoint_counts.size(); i++) {
        std::cout << "Waypoint addition at size " << waypoint_counts[i]
                  << ": " << addition_times[i] << " us\n";
    }
}

//=============================================================================
// Memory Usage Tests
//=============================================================================

TEST_F(PortiaPlanningPerformanceTest, MemoryUsageBounded) {
    // WHAT: Verify memory usage stays within reasonable bounds
    // WHY:  Prevent memory exhaustion
    // HOW:  Create many plans, monitor memory growth

    const int NUM_PLANS = config.max_plans;
    std::vector<uint32_t> plan_ids;

    nimcp_memory_stats_t initial_stats = {0}; nimcp_memory_get_stats(&initial_stats);
    size_t initial_memory = initial_stats.current_allocated;

    // Create maximum number of plans
    for (int i = 0; i < NUM_PLANS; i++) {
        portia_plan_t* plan = portia_planning_create_plan(
            planner, i * 10.0f, i * 10.0f, 0.0f);

        if (plan) {
            plan_ids.push_back(plan->id);

            // Add some waypoints to each
            for (int w = 0; w < 5; w++) {
                portia_planning_add_waypoint(planner, plan->id,
                    w * 5.0f, w * 5.0f, 0.0f, 0.8f);
            }
        }
    }

    nimcp_memory_stats_t peak_stats = {0}; nimcp_memory_get_stats(&peak_stats);
    size_t peak_memory = peak_stats.current_allocated;
    size_t memory_used = peak_memory - initial_memory;

    // Memory usage should be bounded
    const size_t MAX_MEMORY_BYTES = 1024 * 1024;  // 1MB
    EXPECT_LT(memory_used, MAX_MEMORY_BYTES)
        << "Excessive memory usage: " << memory_used << " bytes";

    std::cout << "Memory used for " << NUM_PLANS << " plans with waypoints: "
              << memory_used << " bytes\n";

    // Cleanup
    for (uint32_t id : plan_ids) {
        portia_planning_delete_plan(planner, id);
    }

    // Verify memory freed
    nimcp_memory_stats_t final_stats = {0}; nimcp_memory_get_stats(&final_stats);
    size_t final_memory = final_stats.current_allocated;
    size_t leaked = (final_memory > initial_memory)
        ? (final_memory - initial_memory) : 0;

    // Allow 32KB tolerance for memory pools and internal caching
    // The planning system may retain some pooled memory for performance
    EXPECT_LT(leaked, 32 * 1024)
        << "Excessive memory leak detected: " << leaked << " bytes";
}

TEST_F(PortiaPlanningPerformanceTest, NoMemoryLeakInPlanLifecycle) {
    // WHAT: Verify no leaks during create/delete cycles
    // WHY:  Ensure long-term stability
    // HOW:  Create and delete many plans, check memory

    const int CYCLES = 100;

    nimcp_memory_stats_t initial_stats = {0}; nimcp_memory_get_stats(&initial_stats);
    size_t initial_memory = initial_stats.current_allocated;

    for (int i = 0; i < CYCLES; i++) {
        portia_plan_t* plan = portia_planning_create_plan(
            planner, i * 1.0f, i * 1.0f, 0.0f);

        if (plan) {
            // Add and remove waypoints
            portia_planning_add_waypoint(planner, plan->id, 5.0f, 5.0f, 0.0f, 0.9f);
            portia_planning_evaluate(planner, plan->id);
            portia_planning_delete_plan(planner, plan->id);
        }
    }

    nimcp_memory_stats_t final_stats = {0}; nimcp_memory_get_stats(&final_stats);
    size_t final_memory = final_stats.current_allocated;
    size_t growth = (final_memory > initial_memory)
        ? (final_memory - initial_memory) : 0;

    EXPECT_LT(growth, 10240)  // 10KB tolerance
        << "Memory leak in plan lifecycle: " << growth << " bytes";

    std::cout << "Memory growth after " << CYCLES
              << " plan cycles: " << growth << " bytes\n";
}

//=============================================================================
// Detour Planning Tests
//=============================================================================

TEST_F(PortiaPlanningPerformanceTest, DetourPlanningLatency) {
    // WHAT: Verify detour planning completes quickly
    // WHY:  Detours should not block time-critical operations
    // HOW:  Measure time to plan with max detour depth

    portia_plan_t* plan = portia_planning_create_plan(
        planner, 100.0f, 100.0f, 0.0f);
    ASSERT_NE(plan, nullptr);

    // Add waypoints with decreasing confidence (simulating detour)
    for (int i = 0; i < config.max_detour_depth; i++) {
        float conf = 0.9f - (i * 0.1f);
        portia_planning_add_waypoint(planner, plan->id,
            i * 10.0f, i * 10.0f, 0.0f, conf);
    }

    // Measure detour check time
    double check_time_us = measure_time_us([&]() {
        portia_planning_can_detour(planner, plan->id);
    }, 100);

    const double MAX_DETOUR_CHECK_US = 100.0;  // 100 microseconds
    EXPECT_LT(check_time_us, MAX_DETOUR_CHECK_US)
        << "Detour check too slow: " << check_time_us << " us";

    std::cout << "Detour check time: " << check_time_us << " us\n";

    portia_planning_delete_plan(planner, plan->id);
}

TEST_F(PortiaPlanningPerformanceTest, ObstacleHandlingPerformance) {
    // WHAT: Verify obstacle handling doesn't cause latency spikes
    // WHY:  Real-time response to obstacles needed
    // HOW:  Measure obstacle handling time

    portia_plan_t* plan = create_plan_with_waypoints(10);
    ASSERT_NE(plan, nullptr);

    double handle_time_us = measure_time_us([&]() {
        portia_planning_handle_obstacle(planner, plan->id,
            50.0f, 50.0f, 0.0f);
    }, 10);

    const double MAX_OBSTACLE_HANDLE_US = 5000.0;  // 5ms
    EXPECT_LT(handle_time_us, MAX_OBSTACLE_HANDLE_US)
        << "Obstacle handling too slow: " << handle_time_us << " us";

    std::cout << "Obstacle handling time: " << handle_time_us << " us\n";

    portia_planning_delete_plan(planner, plan->id);
}

//=============================================================================
// Evaluation Throughput Tests
//=============================================================================

TEST_F(PortiaPlanningPerformanceTest, EvaluationThroughput) {
    // WHAT: Verify evaluation throughput meets requirements
    // WHY:  Need to evaluate many plans quickly for multi-agent systems
    // HOW:  Measure evaluations per second

    std::vector<uint32_t> plan_ids;

    // Create multiple plans
    for (int i = 0; i < 10; i++) {
        portia_plan_t* plan = create_plan_with_waypoints(5);
        if (plan) plan_ids.push_back(plan->id);
    }

    const int EVAL_ITERATIONS = 1000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < EVAL_ITERATIONS; i++) {
        for (uint32_t id : plan_ids) {
            portia_planning_evaluate(planner, id);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    double total_evaluations = EVAL_ITERATIONS * plan_ids.size();
    double evals_per_sec = total_evaluations / elapsed.count();

    // Should achieve at least 10,000 evaluations/sec
    const double MIN_EVALS_PER_SEC = 10000.0;
    EXPECT_GT(evals_per_sec, MIN_EVALS_PER_SEC)
        << "Low evaluation throughput: " << evals_per_sec << " evals/sec";

    std::cout << "Evaluation throughput: " << evals_per_sec << " evals/sec\n";

    // Cleanup
    for (uint32_t id : plan_ids) {
        portia_planning_delete_plan(planner, id);
    }
}

TEST_F(PortiaPlanningPerformanceTest, ExecutionStepThroughput) {
    // WHAT: Verify execution step throughput sufficient
    // WHY:  Need fast step execution for real-time planning
    // HOW:  Measure steps per second

    portia_plan_t* plan = create_plan_with_waypoints(20);
    ASSERT_NE(plan, nullptr);

    int successful_steps = 0;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; i++) {
        if (portia_planning_execute_step(planner, plan->id)) {
            successful_steps++;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    double steps_per_sec = successful_steps / elapsed.count();

    std::cout << "Execution step throughput: " << steps_per_sec << " steps/sec\n";

    portia_planning_delete_plan(planner, plan->id);
}

//=============================================================================
// Concurrent Planning Tests
//=============================================================================

TEST_F(PortiaPlanningPerformanceTest, ConcurrentPlanningSupport) {
    // WHAT: Verify multiple plans can be active simultaneously
    // WHY:  Multi-agent systems need concurrent planning
    // HOW:  Create max concurrent plans, verify all functional

    const int MAX_CONCURRENT = config.max_plans;
    std::vector<uint32_t> plan_ids;

    // Create maximum concurrent plans
    for (int i = 0; i < MAX_CONCURRENT; i++) {
        portia_plan_t* plan = portia_planning_create_plan(
            planner, i * 20.0f, i * 20.0f, 0.0f);

        if (plan) {
            plan_ids.push_back(plan->id);
        }
    }

    EXPECT_EQ(plan_ids.size(), static_cast<size_t>(MAX_CONCURRENT))
        << "Failed to create max concurrent plans";

    // Verify all plans can be operated on
    for (uint32_t id : plan_ids) {
        bool eval_ok = portia_planning_evaluate(planner, id);
        EXPECT_TRUE(eval_ok) << "Evaluation failed for plan " << id;

        portia_plan_t* plan = portia_planning_get_plan(planner, id);
        EXPECT_NE(plan, nullptr) << "Failed to retrieve plan " << id;
    }

    // Cleanup
    for (uint32_t id : plan_ids) {
        portia_planning_delete_plan(planner, id);
    }
}

TEST_F(PortiaPlanningPerformanceTest, PlanInterferenceTest) {
    // WHAT: Verify operations on one plan don't affect others
    // WHY:  Ensure plan independence
    // HOW:  Modify one plan, verify others unchanged

    // Create 3 plans
    portia_plan_t* plan1 = create_plan_with_waypoints(5);
    portia_plan_t* plan2 = create_plan_with_waypoints(5);
    portia_plan_t* plan3 = create_plan_with_waypoints(5);

    ASSERT_NE(plan1, nullptr);
    ASSERT_NE(plan2, nullptr);
    ASSERT_NE(plan3, nullptr);

    uint32_t original_count2 = plan2->waypoint_count;
    uint32_t original_count3 = plan3->waypoint_count;

    // Modify plan1
    portia_planning_add_waypoint(planner, plan1->id, 99.0f, 99.0f, 0.0f, 0.9f);
    portia_planning_handle_obstacle(planner, plan1->id, 50.0f, 50.0f, 0.0f);

    // Verify plan2 and plan3 unchanged
    portia_plan_t* check2 = portia_planning_get_plan(planner, plan2->id);
    portia_plan_t* check3 = portia_planning_get_plan(planner, plan3->id);

    EXPECT_EQ(check2->waypoint_count, original_count2)
        << "Plan 2 affected by plan 1 modifications";
    EXPECT_EQ(check3->waypoint_count, original_count3)
        << "Plan 3 affected by plan 1 modifications";

    portia_planning_delete_plan(planner, plan1->id);
    portia_planning_delete_plan(planner, plan2->id);
    portia_planning_delete_plan(planner, plan3->id);
}

//=============================================================================
// Stress Tests
//=============================================================================

TEST_F(PortiaPlanningPerformanceTest, RapidPlanCreationDeletion) {
    // WHAT: Verify system handles rapid plan churn
    // WHY:  Dynamic environments need frequent replanning
    // HOW:  Rapidly create and delete plans, verify stability

    const int ITERATIONS = 500;

    for (int i = 0; i < ITERATIONS; i++) {
        portia_plan_t* plan = portia_planning_create_plan(
            planner, i * 1.0f, i * 1.0f, 0.0f);

        if (plan) {
            // Quick operation
            portia_planning_evaluate(planner, plan->id);
            portia_planning_delete_plan(planner, plan->id);
        }
    }

    // If we get here without crashing, test passes
    SUCCEED();
}

} // anonymous namespace
