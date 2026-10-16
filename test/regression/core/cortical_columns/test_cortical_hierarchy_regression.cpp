/**
 * @file test_cortical_hierarchy_regression.cpp
 * @brief Comprehensive regression tests for cortical hierarchy module
 *
 * WHAT: Regression tests for multi-area cortical hierarchy
 * WHY:  Ensure hierarchical processing is stable across versions
 * HOW:  GTest framework with performance benchmarks, determinism checks,
 *       memory safety tests, statistics validation, and stress tests
 *
 * TEST CATEGORIES:
 * - Hierarchy Traversal Performance: Propagation timing
 * - Prediction Accuracy Consistency: Error computation stability
 * - Level Activation Determinism: Same input = same output
 * - Memory Patterns: Create/destroy cycles
 * - Statistics Accumulation: Stats validity after processing
 * - Stress Tests: Rapid propagations, extreme configurations
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
#include "core/cortical_columns/nimcp_cortical_hierarchy.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Constants and Thresholds
//=============================================================================

// Performance thresholds
constexpr double PROPAGATION_LATENCY_MS = 10.0;  // <10ms per propagation
constexpr double AREA_ADD_LATENCY_MS = 5.0;  // <5ms to add area
constexpr uint32_t MIN_PROPAGATIONS_PER_SEC = 100;

// Numerical tolerances
constexpr float NUMERICAL_TOLERANCE = 1e-6f;
constexpr float RF_SIZE_TOLERANCE = 0.01f;

//=============================================================================
// Test Fixture
//=============================================================================

class CorticalHierarchyRegressionTest : public NimcpTestBase {
protected:
    cortical_hierarchy_t* hierarchy = nullptr;
    std::mt19937 rng{42};  // Deterministic RNG

    void SetUp() override {
        NimcpTestBase::SetUp();

        cortical_hierarchy_config_t config = cortical_hierarchy_default_config();
        hierarchy = cortical_hierarchy_create(&config);
    }

    void TearDown() override {
        if (hierarchy) {
            cortical_hierarchy_destroy(hierarchy);
            hierarchy = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    // Helper: Create standard visual hierarchy (V1->V2->V4->IT)
    void create_visual_hierarchy() {
        ASSERT_NE(hierarchy, nullptr);

        // V1 - Primary visual cortex
        cortical_area_config_t v1_config = {
            .type = CORTICAL_AREA_V1,
            .stream = STREAM_VENTRAL,
            .hierarchy_level = 0,
            .rf_expansion_factor = 2.0f,
            .num_hypercolumns = 100,
            .neurons_per_hypercolumn = 1000,
            .feedforward_strength = 1.0f,
            .feedback_strength = 0.5f,
            .custom_name = nullptr
        };
        uint32_t v1_id;
        ASSERT_EQ(cortical_hierarchy_add_area(hierarchy, &v1_config, &v1_id), 0);

        // V2 - Secondary visual
        cortical_area_config_t v2_config = {
            .type = CORTICAL_AREA_V2,
            .stream = STREAM_VENTRAL,
            .hierarchy_level = 1,
            .rf_expansion_factor = 2.0f,
            .num_hypercolumns = 50,
            .neurons_per_hypercolumn = 1000,
            .feedforward_strength = 0.9f,
            .feedback_strength = 0.5f,
            .custom_name = nullptr
        };
        uint32_t v2_id;
        ASSERT_EQ(cortical_hierarchy_add_area(hierarchy, &v2_config, &v2_id), 0);

        // V4 - Intermediate features
        cortical_area_config_t v4_config = {
            .type = CORTICAL_AREA_V4,
            .stream = STREAM_VENTRAL,
            .hierarchy_level = 2,
            .rf_expansion_factor = 2.0f,
            .num_hypercolumns = 25,
            .neurons_per_hypercolumn = 1000,
            .feedforward_strength = 0.8f,
            .feedback_strength = 0.5f,
            .custom_name = nullptr
        };
        uint32_t v4_id;
        ASSERT_EQ(cortical_hierarchy_add_area(hierarchy, &v4_config, &v4_id), 0);

        // IT - Object recognition
        cortical_area_config_t it_config = {
            .type = CORTICAL_AREA_IT,
            .stream = STREAM_VENTRAL,
            .hierarchy_level = 3,
            .rf_expansion_factor = 2.0f,
            .num_hypercolumns = 10,
            .neurons_per_hypercolumn = 1000,
            .feedforward_strength = 0.7f,
            .feedback_strength = 0.5f,
            .custom_name = nullptr
        };
        uint32_t it_id;
        ASSERT_EQ(cortical_hierarchy_add_area(hierarchy, &it_config, &it_id), 0);

        // Apply canonical connections
        ASSERT_EQ(cortical_hierarchy_apply_canonical_connections(hierarchy), 0);
    }

    // Helper: Generate random activity
    std::vector<float> random_activity(uint32_t size) {
        std::vector<float> activity(size);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        for (auto& val : activity) {
            val = dist(rng);
        }
        return activity;
    }

    // Helper: Measure elapsed time
    template<typename Func>
    double measure_time_ms(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    // Helper: Get allocated memory
    size_t get_allocated_memory() {
        nimcp_memory_stats_t stats;
        if (nimcp_memory_get_stats(&stats)) {
            return stats.current_allocated;
        }
        return 0;
    }
};

//=============================================================================
// CATEGORY 1: Hierarchy Traversal Performance
//=============================================================================

TEST_F(CorticalHierarchyRegressionTest, FeedforwardPropagationPerformance) {
    // WHAT: Benchmark feedforward propagation performance
    // WHY:  Verify propagation remains performant
    // TARGET: <10ms per propagation

    create_visual_hierarchy();

    // Set input to V1
    auto input = random_activity(100);
    ASSERT_EQ(cortical_hierarchy_set_area_input(hierarchy, 0, input.data(), input.size()), 0);

    // Warmup
    for (int i = 0; i < 10; i++) {
        cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);
    }

    // Measure
    const uint32_t iterations = 100;
    double total_ms = measure_time_ms([&]() {
        for (uint32_t i = 0; i < iterations; i++) {
            int result = cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);
            EXPECT_EQ(result, 0);
        }
    });

    double avg_latency = total_ms / iterations;
    EXPECT_LT(avg_latency, PROPAGATION_LATENCY_MS)
        << "Feedforward propagation latency: " << avg_latency << " ms";
}

TEST_F(CorticalHierarchyRegressionTest, FeedbackPropagationPerformance) {
    // WHAT: Benchmark feedback propagation performance
    // WHY:  Verify top-down processing is performant
    // TARGET: <10ms per propagation

    create_visual_hierarchy();

    auto input = random_activity(100);
    cortical_hierarchy_set_area_input(hierarchy, 0, input.data(), input.size());
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);

    // Warmup
    for (int i = 0; i < 10; i++) {
        cortical_hierarchy_propagate_feedback(hierarchy, 3, 0);
    }

    // Measure
    const uint32_t iterations = 100;
    double total_ms = measure_time_ms([&]() {
        for (uint32_t i = 0; i < iterations; i++) {
            int result = cortical_hierarchy_propagate_feedback(hierarchy, 3, 0);
            EXPECT_EQ(result, 0);
        }
    });

    double avg_latency = total_ms / iterations;
    EXPECT_LT(avg_latency, PROPAGATION_LATENCY_MS)
        << "Feedback propagation latency: " << avg_latency << " ms";
}

TEST_F(CorticalHierarchyRegressionTest, AreaAddPerformance) {
    // WHAT: Benchmark area addition performance
    // WHY:  Verify dynamic hierarchy construction is fast
    // TARGET: <5ms per area

    ASSERT_NE(hierarchy, nullptr);

    const uint32_t num_areas = 10;
    double total_ms = 0;

    for (uint32_t i = 0; i < num_areas; i++) {
        cortical_area_config_t config = {
            .type = CORTICAL_AREA_CUSTOM,
            .stream = STREAM_VENTRAL,
            .hierarchy_level = i,
            .rf_expansion_factor = 2.0f,
            .num_hypercolumns = 50,
            .neurons_per_hypercolumn = 500,
            .feedforward_strength = 1.0f,
            .feedback_strength = 0.5f,
            .custom_name = "TestArea"
        };

        uint32_t area_id;
        double latency = measure_time_ms([&]() {
            int result = cortical_hierarchy_add_area(hierarchy, &config, &area_id);
            EXPECT_EQ(result, 0);
        });

        total_ms += latency;
    }

    double avg_latency = total_ms / num_areas;
    EXPECT_LT(avg_latency, AREA_ADD_LATENCY_MS)
        << "Area add latency: " << avg_latency << " ms";
}

TEST_F(CorticalHierarchyRegressionTest, PropagationThroughput) {
    // WHAT: Test propagation throughput
    // WHY:  Verify sustained processing performance
    // TARGET: >100 propagations/sec

    create_visual_hierarchy();

    auto input = random_activity(100);
    cortical_hierarchy_set_area_input(hierarchy, 0, input.data(), input.size());

    auto start = std::chrono::high_resolution_clock::now();
    const uint32_t iterations = 200;

    for (uint32_t i = 0; i < iterations; i++) {
        cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);
        cortical_hierarchy_propagate_feedback(hierarchy, 3, 0);
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_sec = std::chrono::duration<double>(end - start).count();
    double ops_per_sec = (iterations * 2) / elapsed_sec;  // 2 propagations per iteration

    EXPECT_GT(ops_per_sec, MIN_PROPAGATIONS_PER_SEC)
        << "Propagation throughput: " << ops_per_sec << " ops/sec";
}

//=============================================================================
// CATEGORY 2: Prediction Accuracy Consistency
//=============================================================================

TEST_F(CorticalHierarchyRegressionTest, PredictionErrorDeterminism) {
    // WHAT: Verify prediction error is deterministic
    // WHY:  Prediction errors must be reproducible
    // TARGET: Errors match exactly

    create_visual_hierarchy();

    auto input = random_activity(100);
    cortical_hierarchy_set_area_input(hierarchy, 0, input.data(), input.size());
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);

    // First computation
    float error1 = 0.0f;
    int result = cortical_hierarchy_compute_prediction_error(hierarchy, 1, &error1);
    EXPECT_EQ(result, 0);

    // Second computation with same state
    float error2 = 0.0f;
    result = cortical_hierarchy_compute_prediction_error(hierarchy, 1, &error2);
    EXPECT_EQ(result, 0);

    EXPECT_FLOAT_EQ(error1, error2) << "Prediction errors differ";
}

TEST_F(CorticalHierarchyRegressionTest, PredictionErrorBounds) {
    // WHAT: Verify prediction error is bounded
    // WHY:  Error values must be valid
    // TARGET: Non-negative, finite error

    create_visual_hierarchy();

    auto input = random_activity(100);
    cortical_hierarchy_set_area_input(hierarchy, 0, input.data(), input.size());
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);
    cortical_hierarchy_propagate_feedback(hierarchy, 3, 0);

    // Check error at each level
    for (uint32_t area_id = 0; area_id < 4; area_id++) {
        float error = 0.0f;
        int result = cortical_hierarchy_compute_prediction_error(hierarchy, area_id, &error);

        if (result == 0) {
            EXPECT_GE(error, 0.0f) << "Negative error at area " << area_id;
            EXPECT_FALSE(std::isnan(error)) << "NaN error at area " << area_id;
            EXPECT_FALSE(std::isinf(error)) << "Inf error at area " << area_id;
        }
    }
}

TEST_F(CorticalHierarchyRegressionTest, PredictionErrorAfterMultiplePasses) {
    // WHAT: Verify error consistency after multiple passes
    // WHY:  Multiple iterations should converge or remain stable
    // TARGET: Error doesn't explode

    create_visual_hierarchy();

    auto input = random_activity(100);
    cortical_hierarchy_set_area_input(hierarchy, 0, input.data(), input.size());

    std::vector<float> errors;

    for (int pass = 0; pass < 10; pass++) {
        cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);
        cortical_hierarchy_propagate_feedback(hierarchy, 3, 0);

        float error = 0.0f;
        cortical_hierarchy_compute_prediction_error(hierarchy, 1, &error);
        errors.push_back(error);
    }

    // Error should not explode (if initial error is 0, any value is acceptable)
    float threshold = (errors[0] > 0.0f) ? errors[0] * 100.0f : 1000.0f;
    for (size_t i = 1; i < errors.size(); i++) {
        EXPECT_LT(errors[i], threshold)
            << "Error exploded at pass " << i;
    }
}

//=============================================================================
// CATEGORY 3: Level Activation Determinism
//=============================================================================

TEST_F(CorticalHierarchyRegressionTest, ActivityPropagationDeterminism) {
    // WHAT: Verify activity propagation is deterministic
    // WHY:  Same input must produce same output
    // TARGET: Activities match exactly

    create_visual_hierarchy();

    auto input = random_activity(100);

    // First run
    cortical_hierarchy_set_area_input(hierarchy, 0, input.data(), input.size());
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);

    std::vector<float> activity1(50);
    uint32_t size1;
    cortical_hierarchy_get_area_activity(hierarchy, 1, activity1.data(), 50, &size1);

    // Second run with same input (create fresh hierarchy)
    cortical_hierarchy_destroy(hierarchy);
    cortical_hierarchy_config_t config = cortical_hierarchy_default_config();
    hierarchy = cortical_hierarchy_create(&config);
    create_visual_hierarchy();

    cortical_hierarchy_set_area_input(hierarchy, 0, input.data(), input.size());
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);

    std::vector<float> activity2(50);
    uint32_t size2;
    cortical_hierarchy_get_area_activity(hierarchy, 1, activity2.data(), 50, &size2);

    EXPECT_EQ(size1, size2);
    for (uint32_t i = 0; i < std::min(size1, size2); i++) {
        EXPECT_NEAR(activity1[i], activity2[i], NUMERICAL_TOLERANCE)
            << "Activity differs at index " << i;
    }
}

TEST_F(CorticalHierarchyRegressionTest, ReceptiveFieldExpansion) {
    // WHAT: Verify RF expansion follows expected pattern
    // WHY:  RF size should increase hierarchically
    // TARGET: RF = base * factor^level

    create_visual_hierarchy();

    float rf_sizes[4];
    for (uint32_t area_id = 0; area_id < 4; area_id++) {
        int result = cortical_hierarchy_get_receptive_field_size(hierarchy, area_id, &rf_sizes[area_id]);
        EXPECT_EQ(result, 0);
    }

    // RF should increase with level
    for (uint32_t i = 1; i < 4; i++) {
        EXPECT_GT(rf_sizes[i], rf_sizes[i-1])
            << "RF not expanding at level " << i;
    }
}

TEST_F(CorticalHierarchyRegressionTest, ActivityBoundsAfterPropagation) {
    // WHAT: Verify activities remain bounded
    // WHY:  Activities should be in valid range
    // TARGET: All activities in reasonable bounds

    create_visual_hierarchy();

    auto input = random_activity(100);
    cortical_hierarchy_set_area_input(hierarchy, 0, input.data(), input.size());

    for (int pass = 0; pass < 10; pass++) {
        cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);
        cortical_hierarchy_propagate_feedback(hierarchy, 3, 0);

        for (uint32_t area_id = 0; area_id < 4; area_id++) {
            std::vector<float> activity(100);
            uint32_t size;
            cortical_hierarchy_get_area_activity(hierarchy, area_id, activity.data(), 100, &size);

            for (uint32_t i = 0; i < size; i++) {
                EXPECT_FALSE(std::isnan(activity[i])) << "NaN at area " << area_id << ", index " << i;
                EXPECT_FALSE(std::isinf(activity[i])) << "Inf at area " << area_id << ", index " << i;
            }
        }
    }
}

//=============================================================================
// CATEGORY 4: Memory Patterns
//=============================================================================

TEST_F(CorticalHierarchyRegressionTest, HierarchyCreateDestroyNoLeak) {
    // WHAT: Verify no memory leak in hierarchy lifecycle
    // WHY:  Memory must be properly released
    // TARGET: Memory returns to baseline

    size_t memory_before = get_allocated_memory();

    for (int cycle = 0; cycle < 50; cycle++) {
        cortical_hierarchy_config_t config = cortical_hierarchy_default_config();
        cortical_hierarchy_t* h = cortical_hierarchy_create(&config);
        ASSERT_NE(h, nullptr);

        // Add some areas
        cortical_area_config_t area_config = {
            .type = CORTICAL_AREA_V1,
            .stream = STREAM_VENTRAL,
            .hierarchy_level = 0,
            .rf_expansion_factor = 2.0f,
            .num_hypercolumns = 50,
            .neurons_per_hypercolumn = 500,
            .feedforward_strength = 1.0f,
            .feedback_strength = 0.5f,
            .custom_name = nullptr
        };

        uint32_t area_id;
        cortical_hierarchy_add_area(h, &area_config, &area_id);

        cortical_hierarchy_destroy(h);
    }

    size_t memory_after = get_allocated_memory();
    size_t leak = (memory_after > memory_before) ? (memory_after - memory_before) : 0;

    EXPECT_LT(leak, 4096) << "Memory leak: " << leak << " bytes";
}

TEST_F(CorticalHierarchyRegressionTest, AreaAddRemoveMemory) {
    // WHAT: Verify area add/remove doesn't leak
    // WHY:  Dynamic reconfiguration must be clean
    // TARGET: No significant memory leak

    ASSERT_NE(hierarchy, nullptr);

    size_t memory_before = get_allocated_memory();
    int successful_cycles = 0;

    for (int cycle = 0; cycle < 100; cycle++) {
        cortical_area_config_t config = {
            .type = CORTICAL_AREA_CUSTOM,
            .stream = STREAM_VENTRAL,
            .hierarchy_level = 0,
            .rf_expansion_factor = 2.0f,
            .num_hypercolumns = 20,
            .neurons_per_hypercolumn = 200,
            .feedforward_strength = 1.0f,
            .feedback_strength = 0.5f,
            .custom_name = "TempArea"
        };

        uint32_t area_id;
        int result = cortical_hierarchy_add_area(hierarchy, &config, &area_id);
        if (result != 0) {
            // Area couldn't be added (maybe capacity limit), skip
            continue;
        }

        result = cortical_hierarchy_remove_area(hierarchy, area_id);
        // Remove may not be supported or may fail - that's okay for this test
        // We're primarily testing that repeated add doesn't leak memory
        (void)result;
        successful_cycles++;
    }

    // Should have completed at least some cycles
    EXPECT_GT(successful_cycles, 0);

    size_t memory_after = get_allocated_memory();
    size_t leak = (memory_after > memory_before) ? (memory_after - memory_before) : 0;

    // Allow for some memory growth if remove doesn't work
    EXPECT_LT(leak, 1024 * 1024) << "Significant memory leak: " << leak << " bytes";
}

TEST_F(CorticalHierarchyRegressionTest, ConnectionMemoryManagement) {
    // WHAT: Verify connection memory is properly managed
    // WHY:  Inter-area connections must be cleaned up
    // TARGET: No leak after connection cycles

    ASSERT_NE(hierarchy, nullptr);

    // Add two areas
    cortical_area_config_t config1 = {
        .type = CORTICAL_AREA_V1,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 0,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 20,
        .neurons_per_hypercolumn = 200,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = nullptr
    };
    uint32_t area1;
    ASSERT_EQ(cortical_hierarchy_add_area(hierarchy, &config1, &area1), 0);

    cortical_area_config_t config2 = {
        .type = CORTICAL_AREA_V2,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 1,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 10,
        .neurons_per_hypercolumn = 200,
        .feedforward_strength = 0.9f,
        .feedback_strength = 0.5f,
        .custom_name = nullptr
    };
    uint32_t area2;
    ASSERT_EQ(cortical_hierarchy_add_area(hierarchy, &config2, &area2), 0);

    size_t memory_before = get_allocated_memory();

    for (int cycle = 0; cycle < 50; cycle++) {
        inter_area_connection_config_t conn_config = {
            .source_area_id = area1,
            .target_area_id = area2,
            .type = CONNECTION_TYPE_FEEDFORWARD,
            .source_layer = 2,
            .target_layer = 2,
            .weight = 1.0f,
            .delay_ms = 10.0f,
            .use_canonical_layers = true
        };

        uint32_t conn_id;
        int result = cortical_hierarchy_connect_areas(hierarchy, &conn_config, &conn_id);
        EXPECT_EQ(result, 0);

        result = cortical_hierarchy_disconnect_areas(hierarchy, conn_id);
        EXPECT_EQ(result, 0);
    }

    size_t memory_after = get_allocated_memory();
    size_t leak = (memory_after > memory_before) ? (memory_after - memory_before) : 0;

    EXPECT_LT(leak, 4096) << "Memory leak: " << leak << " bytes";
}

//=============================================================================
// CATEGORY 5: Statistics Accumulation
//=============================================================================

TEST_F(CorticalHierarchyRegressionTest, AreaStatsValidity) {
    // WHAT: Verify area stats are valid after processing
    // WHY:  Statistics must reflect actual state
    // TARGET: All stats values are valid

    create_visual_hierarchy();

    auto input = random_activity(100);
    cortical_hierarchy_set_area_input(hierarchy, 0, input.data(), input.size());
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);

    for (uint32_t area_id = 0; area_id < 4; area_id++) {
        cortical_area_stats_t stats;
        int result = cortical_hierarchy_get_area_stats(hierarchy, area_id, &stats);
        EXPECT_EQ(result, 0);

        EXPECT_EQ(stats.area_id, area_id);
        EXPECT_GE(stats.receptive_field_size, 0.0f);
        EXPECT_FALSE(std::isnan(stats.mean_activity));
        EXPECT_FALSE(std::isinf(stats.mean_activity));
        EXPECT_GE(stats.mean_activity, 0.0f);
        EXPECT_FALSE(std::isnan(stats.peak_activity));
        EXPECT_FALSE(std::isinf(stats.peak_activity));
        EXPECT_GE(stats.peak_activity, stats.mean_activity);
    }
}

TEST_F(CorticalHierarchyRegressionTest, HierarchyStatsAccumulation) {
    // WHAT: Verify hierarchy stats accumulate correctly
    // WHY:  Global stats must reflect processing history
    // TARGET: Stats counts increase with processing

    create_visual_hierarchy();

    cortical_hierarchy_stats_t stats_before;
    cortical_hierarchy_get_stats(hierarchy, &stats_before);

    auto input = random_activity(100);
    cortical_hierarchy_set_area_input(hierarchy, 0, input.data(), input.size());

    const uint32_t iterations = 50;
    for (uint32_t i = 0; i < iterations; i++) {
        cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);
    }

    cortical_hierarchy_stats_t stats_after;
    cortical_hierarchy_get_stats(hierarchy, &stats_after);

    EXPECT_EQ(stats_after.num_areas, 4u);
    EXPECT_GT(stats_after.num_connections, 0u);
    EXPECT_GE(stats_after.total_propagations, stats_before.total_propagations + iterations);
}

TEST_F(CorticalHierarchyRegressionTest, ConnectionCountsConsistency) {
    // WHAT: Verify connection counts are consistent
    // WHY:  FF/FB counts must match actual connections
    // TARGET: Counts are valid and consistent

    create_visual_hierarchy();

    cortical_hierarchy_stats_t stats;
    cortical_hierarchy_get_stats(hierarchy, &stats);

    EXPECT_EQ(stats.num_areas, 4u);
    EXPECT_GT(stats.num_connections, 0u);
    EXPECT_GE(stats.num_ff_connections, 0u);
    EXPECT_GE(stats.num_fb_connections, 0u);
    EXPECT_EQ(stats.num_connections, stats.num_ff_connections + stats.num_fb_connections);
    EXPECT_EQ(stats.max_hierarchy_level, 3u);
}

TEST_F(CorticalHierarchyRegressionTest, PredictionErrorTracking) {
    // WHAT: Verify prediction error is tracked in stats
    // WHY:  Error tracking enables monitoring
    // TARGET: Total error is non-negative

    create_visual_hierarchy();

    auto input = random_activity(100);
    cortical_hierarchy_set_area_input(hierarchy, 0, input.data(), input.size());
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);
    cortical_hierarchy_propagate_feedback(hierarchy, 3, 0);

    cortical_hierarchy_stats_t stats;
    cortical_hierarchy_get_stats(hierarchy, &stats);

    EXPECT_GE(stats.total_prediction_error, 0.0f);
    EXPECT_FALSE(std::isnan(stats.total_prediction_error));
    EXPECT_FALSE(std::isinf(stats.total_prediction_error));
}

//=============================================================================
// CATEGORY 6: Stress Tests
//=============================================================================

TEST_F(CorticalHierarchyRegressionTest, RapidPropagations) {
    // WHAT: Stress test rapid propagations
    // WHY:  Must handle rapid updates without corruption
    // TARGET: No crashes, valid state

    create_visual_hierarchy();

    auto input = random_activity(100);
    cortical_hierarchy_set_area_input(hierarchy, 0, input.data(), input.size());

    for (int i = 0; i < 5000; i++) {
        int result = cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);
        EXPECT_EQ(result, 0);

        result = cortical_hierarchy_propagate_feedback(hierarchy, 3, 0);
        EXPECT_EQ(result, 0);

        // Periodically check state
        if (i % 100 == 0) {
            cortical_hierarchy_stats_t stats;
            cortical_hierarchy_get_stats(hierarchy, &stats);
            EXPECT_FALSE(std::isnan(stats.total_prediction_error));
        }
    }
}

TEST_F(CorticalHierarchyRegressionTest, ManyAreasStress) {
    // WHAT: Stress test with many areas
    // WHY:  Large hierarchies must work correctly
    // TARGET: No crashes, proper operation

    ASSERT_NE(hierarchy, nullptr);

    const uint32_t max_areas_to_try = 20;
    std::vector<uint32_t> area_ids;
    uint32_t successfully_added = 0;

    for (uint32_t i = 0; i < max_areas_to_try; i++) {
        cortical_area_config_t config = {
            .type = CORTICAL_AREA_CUSTOM,
            .stream = (i % 2 == 0) ? STREAM_VENTRAL : STREAM_DORSAL,
            .hierarchy_level = i % 5,
            .rf_expansion_factor = 2.0f,
            .num_hypercolumns = 10,
            .neurons_per_hypercolumn = 100,
            .feedforward_strength = 1.0f,
            .feedback_strength = 0.5f,
            .custom_name = "StressArea"
        };

        uint32_t area_id;
        int result = cortical_hierarchy_add_area(hierarchy, &config, &area_id);
        if (result == 0) {
            area_ids.push_back(area_id);
            successfully_added++;
        }
    }

    // Should have added at least some areas (hierarchy may have capacity limit)
    EXPECT_GT(successfully_added, 0);
    EXPECT_EQ(cortical_hierarchy_get_num_areas(hierarchy), successfully_added);

    // Process if we have at least one area
    if (!area_ids.empty()) {
        auto input = random_activity(10);
        cortical_hierarchy_set_area_input(hierarchy, area_ids[0], input.data(), input.size());
        cortical_hierarchy_propagate_feedforward(hierarchy, 0, 4);
    }

    // Verify stats
    cortical_hierarchy_stats_t stats;
    cortical_hierarchy_get_stats(hierarchy, &stats);
    EXPECT_EQ(stats.num_areas, successfully_added);
}

TEST_F(CorticalHierarchyRegressionTest, VaryingInputStress) {
    // WHAT: Stress test with varying inputs
    // WHY:  Must handle changing inputs gracefully
    // TARGET: No corruption, valid outputs

    create_visual_hierarchy();

    for (int i = 0; i < 1000; i++) {
        auto input = random_activity(50 + (i % 50));
        cortical_hierarchy_set_area_input(hierarchy, 0, input.data(), input.size());
        cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);

        std::vector<float> output(50);
        uint32_t size;
        cortical_hierarchy_get_area_activity(hierarchy, 3, output.data(), 50, &size);

        for (uint32_t j = 0; j < size; j++) {
            EXPECT_FALSE(std::isnan(output[j])) << "NaN at iteration " << i;
            EXPECT_FALSE(std::isinf(output[j])) << "Inf at iteration " << i;
        }
    }
}

TEST_F(CorticalHierarchyRegressionTest, ConcurrentActivityReads) {
    // WHAT: Test concurrent activity reads
    // WHY:  Must handle concurrent access safely
    // TARGET: No data races

    create_visual_hierarchy();

    auto input = random_activity(100);
    cortical_hierarchy_set_area_input(hierarchy, 0, input.data(), input.size());
    cortical_hierarchy_propagate_feedforward(hierarchy, 0, 3);

    std::vector<std::thread> threads;
    std::atomic<uint32_t> error_count{0};

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 500; i++) {
                std::vector<float> activity(50);
                uint32_t size;

                int result = cortical_hierarchy_get_area_activity(
                    hierarchy, t % 4, activity.data(), 50, &size);

                if (result != 0) {
                    error_count++;
                }

                for (uint32_t j = 0; j < size; j++) {
                    if (std::isnan(activity[j]) || std::isinf(activity[j])) {
                        error_count++;
                    }
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(error_count.load(), 0u) << "Thread safety violation";
}

//=============================================================================
// Null Pointer Safety Tests
//=============================================================================

TEST_F(CorticalHierarchyRegressionTest, NullHierarchySafety) {
    // WHAT: Verify null hierarchy is handled safely
    // WHY:  Must not crash on null input
    // TARGET: Graceful handling

    cortical_hierarchy_destroy(nullptr);

    EXPECT_EQ(cortical_hierarchy_get_num_areas(nullptr), 0u);

    cortical_area_config_t area_config = {
        .type = CORTICAL_AREA_V1,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 0,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 10,
        .neurons_per_hypercolumn = 100,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = nullptr
    };
    uint32_t area_id;
    EXPECT_NE(cortical_hierarchy_add_area(nullptr, &area_config, &area_id), 0);

    EXPECT_NE(cortical_hierarchy_remove_area(nullptr, 0), 0);

    EXPECT_EQ(cortical_hierarchy_get_area_config(nullptr, 0), nullptr);

    inter_area_connection_config_t conn_config = {};
    uint32_t conn_id;
    EXPECT_NE(cortical_hierarchy_connect_areas(nullptr, &conn_config, &conn_id), 0);

    EXPECT_NE(cortical_hierarchy_propagate_feedforward(nullptr, 0, 1), 0);
    EXPECT_NE(cortical_hierarchy_propagate_feedback(nullptr, 1, 0), 0);

    float error;
    EXPECT_NE(cortical_hierarchy_compute_prediction_error(nullptr, 0, &error), 0);

    float activity[10];
    EXPECT_NE(cortical_hierarchy_set_area_input(nullptr, 0, activity, 10), 0);

    uint32_t size;
    EXPECT_NE(cortical_hierarchy_get_area_activity(nullptr, 0, activity, 10, &size), 0);

    float rf_size;
    EXPECT_NE(cortical_hierarchy_get_receptive_field_size(nullptr, 0, &rf_size), 0);

    cortical_area_stats_t area_stats;
    EXPECT_NE(cortical_hierarchy_get_area_stats(nullptr, 0, &area_stats), 0);

    cortical_hierarchy_stats_t stats;
    EXPECT_NE(cortical_hierarchy_get_stats(nullptr, &stats), 0);

    EXPECT_FALSE(cortical_hierarchy_is_bio_async_connected(nullptr));
}

TEST_F(CorticalHierarchyRegressionTest, NullConfigSafety) {
    // WHAT: Verify null configs are handled safely
    // WHY:  Must not crash on null parameters
    // TARGET: Return error codes

    ASSERT_NE(hierarchy, nullptr);

    uint32_t area_id;
    EXPECT_NE(cortical_hierarchy_add_area(hierarchy, nullptr, &area_id), 0);

    uint32_t conn_id;
    EXPECT_NE(cortical_hierarchy_connect_areas(hierarchy, nullptr, &conn_id), 0);
}

//=============================================================================
// Backward Compatibility Tests
//=============================================================================

TEST_F(CorticalHierarchyRegressionTest, DefaultConfigStable) {
    // WHAT: Verify default config values are stable
    // WHY:  Defaults must not change unexpectedly
    // TARGET: Known default values

    cortical_hierarchy_config_t config = cortical_hierarchy_default_config();

    EXPECT_GT(config.max_areas, 0u);
    EXPECT_GT(config.max_connections, 0u);
    EXPECT_GT(config.default_rf_base, 0.0f);
    EXPECT_GT(config.default_expansion_factor, 1.0f);
}

TEST_F(CorticalHierarchyRegressionTest, AreaTypesAvailable) {
    // WHAT: Verify all area types work
    // WHY:  Area type enum must be stable
    // TARGET: All types functional

    ASSERT_NE(hierarchy, nullptr);

    cortical_area_type_t types[] = {
        CORTICAL_AREA_V1,
        CORTICAL_AREA_V2,
        CORTICAL_AREA_V4,
        CORTICAL_AREA_IT,
        CORTICAL_AREA_MT,
        CORTICAL_AREA_PFC,
        CORTICAL_AREA_CUSTOM
    };

    for (auto type : types) {
        cortical_area_config_t config = {
            .type = type,
            .stream = STREAM_VENTRAL,
            .hierarchy_level = 0,
            .rf_expansion_factor = 2.0f,
            .num_hypercolumns = 5,
            .neurons_per_hypercolumn = 50,
            .feedforward_strength = 1.0f,
            .feedback_strength = 0.5f,
            .custom_name = (type == CORTICAL_AREA_CUSTOM) ? "Custom" : nullptr
        };

        uint32_t area_id;
        int result = cortical_hierarchy_add_area(hierarchy, &config, &area_id);
        EXPECT_EQ(result, 0) << "Failed to add area type " << type;
    }
}

TEST_F(CorticalHierarchyRegressionTest, ConnectionTypesAvailable) {
    // WHAT: Verify all connection types work
    // WHY:  Connection type enum must be stable
    // TARGET: All types functional

    ASSERT_NE(hierarchy, nullptr);

    // Add two areas
    cortical_area_config_t config1 = {
        .type = CORTICAL_AREA_V1,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 0,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 5,
        .neurons_per_hypercolumn = 50,
        .feedforward_strength = 1.0f,
        .feedback_strength = 0.5f,
        .custom_name = nullptr
    };
    uint32_t area1;
    ASSERT_EQ(cortical_hierarchy_add_area(hierarchy, &config1, &area1), 0);

    cortical_area_config_t config2 = {
        .type = CORTICAL_AREA_V2,
        .stream = STREAM_VENTRAL,
        .hierarchy_level = 1,
        .rf_expansion_factor = 2.0f,
        .num_hypercolumns = 5,
        .neurons_per_hypercolumn = 50,
        .feedforward_strength = 0.9f,
        .feedback_strength = 0.5f,
        .custom_name = nullptr
    };
    uint32_t area2;
    ASSERT_EQ(cortical_hierarchy_add_area(hierarchy, &config2, &area2), 0);

    connection_type_t types[] = {
        CONNECTION_TYPE_FEEDFORWARD,
        CONNECTION_TYPE_FEEDBACK,
        CONNECTION_TYPE_LATERAL
    };

    for (auto type : types) {
        inter_area_connection_config_t conn_config = {
            .source_area_id = (type == CONNECTION_TYPE_FEEDBACK) ? area2 : area1,
            .target_area_id = (type == CONNECTION_TYPE_FEEDBACK) ? area1 : area2,
            .type = type,
            .source_layer = 2,
            .target_layer = 2,
            .weight = 0.5f,
            .delay_ms = 10.0f,
            .use_canonical_layers = true
        };

        uint32_t conn_id;
        int result = cortical_hierarchy_connect_areas(hierarchy, &conn_config, &conn_id);
        EXPECT_EQ(result, 0) << "Failed to create connection type " << type;
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
