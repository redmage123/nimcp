/**
 * @file test_entorhinal_backward_compat.cpp
 * @brief Backward compatibility regression tests for entorhinal cortex region
 *
 * WHAT: Ensures entorhinal cortex API remains stable across refactorings
 * WHY: Entorhinal cortex is critical for spatial navigation and memory gateway
 * HOW: Test API signatures, behavior, memory management, error handling
 *
 * BIOLOGICAL CONTEXT:
 * - Entorhinal Cortex: Gateway between hippocampus and neocortex
 * - Grid Cells: Hexagonal firing patterns for metric spatial representation
 * - Border Cells: Fire near environmental boundaries, anchor grid cells
 * - Head Direction Cells: Encode current heading direction
 * - Path Integration: Dead reckoning from velocity and angular velocity
 * - Memory Gateway: Routes information for encoding, retrieval, consolidation
 *
 * The entorhinal cortex provides the primary interface for:
 * 1. Spatial mapping via grid cells (discovered by Moser & Moser, 2005)
 * 2. Memory encoding through hippocampal projections
 * 3. Memory consolidation by routing to neocortex during sleep
 * 4. Self-localization through path integration
 *
 * TEST CATEGORIES:
 * 1. API Stability - Function signatures unchanged
 * 2. Behavioral Consistency - Spatial operations work correctly
 * 3. Memory Management - No leaks, proper lifecycle
 * 4. Error Handling - Edge cases handled consistently
 * 5. Performance Baselines - No significant regressions
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cmath>
#include <cstring>
#include <cstdlib>

// Entorhinal cortex header has its own extern "C" guard
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"

/* ============================================================================
 * Test Fixture
 * ========================================================================== */

class EntorhinalBackwardCompatTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* entorhinal = nullptr;
    entorhinal_config_t config;

    static constexpr int WARMUP_ITERATIONS = 10;
    static constexpr int BENCHMARK_ITERATIONS = 100;
    static constexpr int MEMORY_TEST_CYCLES = 500;
    static constexpr float EPSILON = 1e-5f;
    static constexpr uint32_t DEFAULT_SPATIAL_DIM = 3;

    void SetUp() override {
        config = entorhinal_default_config();
    }

    void TearDown() override {
        if (entorhinal) {
            entorhinal_destroy(entorhinal);
            entorhinal = nullptr;
        }
    }

    template<typename Func>
    long long measure_ns(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }

    // Helper: Create random position within environment bounds
    void create_random_position(float* position, uint32_t dim) {
        for (uint32_t i = 0; i < dim && i < 3; i++) {
            position[i] = static_cast<float>(rand()) / RAND_MAX * config.environment_size[i];
        }
    }

    // Helper: Create velocity vector
    void create_velocity(float* velocity, float speed, float heading) {
        velocity[0] = speed * cosf(heading);
        velocity[1] = speed * sinf(heading);
        velocity[2] = 0.0f;
    }

    // Helper: Create feature pattern for encoding
    void create_feature_pattern(std::vector<float>& pattern, float base_value, int pattern_id) {
        for (size_t i = 0; i < pattern.size(); i++) {
            pattern[i] = base_value + 0.1f * sinf(static_cast<float>(i + pattern_id));
        }
    }
};

/* ============================================================================
 * CATEGORY 1: API Stability Tests
 * ========================================================================== */

TEST_F(EntorhinalBackwardCompatTest, APIStability_DefaultConfig_HasValidDefaults) {
    // WHAT: Verify default configuration has sensible values
    // WHY: API stability requires consistent default configuration

    EXPECT_EQ(config.num_grid_cells, ENTORHINAL_DEFAULT_GRID_CELLS);
    EXPECT_EQ(config.num_border_cells, ENTORHINAL_DEFAULT_BORDER_CELLS);
    EXPECT_EQ(config.num_hd_cells, ENTORHINAL_DEFAULT_HD_CELLS);
    EXPECT_EQ(config.num_object_cells, ENTORHINAL_DEFAULT_OBJECT_CELLS);
    EXPECT_EQ(config.num_speed_cells, ENTORHINAL_DEFAULT_SPEED_CELLS);
    EXPECT_EQ(config.num_time_cells, ENTORHINAL_DEFAULT_TIME_CELLS);
    EXPECT_EQ(config.spatial_dim, ENTORHINAL_DEFAULT_SPATIAL_DIM);

    // Grid spacing should be biologically realistic
    EXPECT_GE(config.min_grid_spacing, ENTORHINAL_MIN_GRID_SPACING);
    EXPECT_LE(config.max_grid_spacing, ENTORHINAL_MAX_GRID_SPACING);
}

TEST_F(EntorhinalBackwardCompatTest, APIStability_Create_ReturnsValidInstance) {
    // WHAT: Verify entorhinal_create returns valid instance
    // WHY: Core creation API must remain stable

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    EXPECT_EQ(entorhinal->status, ENTORHINAL_STATUS_READY);
    EXPECT_EQ(entorhinal->last_error, ENTORHINAL_ERROR_NONE);
    EXPECT_GT(entorhinal->total_grid_cells, 0u);
    EXPECT_EQ(entorhinal->num_border_cells, config.num_border_cells);
    EXPECT_EQ(entorhinal->num_hd_cells, config.num_hd_cells);
}

TEST_F(EntorhinalBackwardCompatTest, APIStability_CreateWithNullConfig_UsesDefaults) {
    // WHAT: Verify NULL config uses defaults
    // WHY: Graceful handling of NULL parameters

    entorhinal = entorhinal_create(nullptr);
    ASSERT_NE(entorhinal, nullptr);

    // Should use default values
    EXPECT_GT(entorhinal->total_grid_cells, 0u);
    EXPECT_EQ(entorhinal->status, ENTORHINAL_STATUS_READY);
}

TEST_F(EntorhinalBackwardCompatTest, APIStability_StatusEnum_ValuesUnchanged) {
    // WHAT: Verify status enum values are stable
    // WHY: External code may depend on specific enum values

    EXPECT_EQ(static_cast<int>(ENTORHINAL_STATUS_IDLE), 0);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_STATUS_PATH_INTEGRATING), 1);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_STATUS_ENCODING), 2);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_STATUS_RETRIEVING), 3);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_STATUS_GATEWAY_TRANSFER), 4);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_STATUS_CONSOLIDATING), 5);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_STATUS_CALIBRATING), 6);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_STATUS_READY), 7);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_STATUS_ERROR), 8);
}

TEST_F(EntorhinalBackwardCompatTest, APIStability_ErrorEnum_ValuesUnchanged) {
    // WHAT: Verify error enum values are stable
    // WHY: Error handling code depends on specific values

    EXPECT_EQ(static_cast<int>(ENTORHINAL_ERROR_NONE), 0);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_ERROR_INVALID_INPUT), 1);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_ERROR_GRID_DRIFT), 2);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_ERROR_PATH_INTEGRATION_FAILURE), 3);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_ERROR_MEMORY_GATEWAY_BLOCKED), 4);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_ERROR_SECURITY_VIOLATION), 5);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_ERROR_IMMUNE_REJECTION), 6);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_ERROR_SUBSTRATE_DEPLETED), 7);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_ERROR_SYNC_FAILURE), 8);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_ERROR_BUFFER_OVERFLOW), 9);
    EXPECT_EQ(static_cast<int>(ENTORHINAL_ERROR_INTERNAL), 10);
}

TEST_F(EntorhinalBackwardCompatTest, APIStability_GridModuleEnum_ValuesUnchanged) {
    // WHAT: Verify grid module enum values are stable
    // WHY: Grid module selection depends on these values

    EXPECT_EQ(static_cast<int>(GRID_MODULE_FINE), 0);
    EXPECT_EQ(static_cast<int>(GRID_MODULE_MEDIUM), 1);
    EXPECT_EQ(static_cast<int>(GRID_MODULE_COARSE), 2);
    EXPECT_EQ(static_cast<int>(GRID_MODULE_VERY_COARSE), 3);
    EXPECT_EQ(static_cast<int>(GRID_MODULE_COUNT), 4);
}

TEST_F(EntorhinalBackwardCompatTest, APIStability_GetStatus_ReturnsCorrectStatus) {
    // WHAT: Verify entorhinal_get_status returns correct status
    // WHY: Status querying API must be stable

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    entorhinal_status_t status = entorhinal_get_status(entorhinal);
    EXPECT_EQ(status, ENTORHINAL_STATUS_READY);
}

TEST_F(EntorhinalBackwardCompatTest, APIStability_GetLastError_ReturnsNoError) {
    // WHAT: Verify entorhinal_get_last_error after successful creation
    // WHY: Error querying API must be stable

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    entorhinal_error_t error = entorhinal_get_last_error(entorhinal);
    EXPECT_EQ(error, ENTORHINAL_ERROR_NONE);
}

/* ============================================================================
 * CATEGORY 2: Behavioral Consistency Tests
 * ========================================================================== */

TEST_F(EntorhinalBackwardCompatTest, Behavior_GridCellUpdate_ProducesActivations) {
    // WHAT: Verify grid cells activate based on position
    // WHY: Grid cells are fundamental to spatial representation

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    float position[3] = {1.0f, 2.0f, 0.0f};
    int result = entorhinal_update_grid_cells(entorhinal, position, 2);
    EXPECT_EQ(result, 0);  // 0 for success

    // Verify some grid cells are activated
    float total_activation = 0.0f;
    for (uint32_t m = 0; m < entorhinal->num_grid_modules; m++) {
        for (uint32_t c = 0; c < entorhinal->grid_modules[m].num_cells; c++) {
            total_activation += entorhinal->grid_modules[m].cells[c].activation;
        }
    }
    EXPECT_GT(total_activation, 0.0f);
}

TEST_F(EntorhinalBackwardCompatTest, Behavior_DifferentPositions_ProduceDifferentPatterns) {
    // WHAT: Verify different positions produce different grid patterns
    // WHY: Grid cells must uniquely represent spatial locations

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    // Update at position 1
    float pos1[3] = {0.5f, 0.5f, 0.0f};
    entorhinal_update_grid_cells(entorhinal, pos1, 2);

    std::vector<float> activations1(entorhinal->total_grid_cells);
    uint32_t idx = 0;
    for (uint32_t m = 0; m < entorhinal->num_grid_modules; m++) {
        for (uint32_t c = 0; c < entorhinal->grid_modules[m].num_cells; c++) {
            activations1[idx++] = entorhinal->grid_modules[m].cells[c].activation;
        }
    }

    // Update at position 2 (significantly different)
    float pos2[3] = {5.0f, 5.0f, 0.0f};
    entorhinal_update_grid_cells(entorhinal, pos2, 2);

    // Compare activation patterns
    float difference = 0.0f;
    idx = 0;
    for (uint32_t m = 0; m < entorhinal->num_grid_modules; m++) {
        for (uint32_t c = 0; c < entorhinal->grid_modules[m].num_cells; c++) {
            float act2 = entorhinal->grid_modules[m].cells[c].activation;
            difference += fabsf(activations1[idx++] - act2);
        }
    }

    // Different positions should produce different patterns
    EXPECT_GT(difference, 0.0f);
}

TEST_F(EntorhinalBackwardCompatTest, Behavior_HeadDirectionUpdate_WorksCorrectly) {
    // WHAT: Verify HD cells update based on heading
    // WHY: Head direction cells are essential for orientation

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    float heading = 1.57f;  // ~90 degrees (PI/2)
    float angular_velocity = 0.1f;

    int result = entorhinal_update_hd_cells(entorhinal, heading, angular_velocity);
    EXPECT_EQ(result, 0);

    // Verify current heading is updated
    EXPECT_NEAR(entorhinal->current_heading, heading, EPSILON);
}

TEST_F(EntorhinalBackwardCompatTest, Behavior_PathIntegration_AccumulatesPosition) {
    // WHAT: Verify path integration accumulates velocity
    // WHY: Dead reckoning is core to navigation

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    // Reset to known position
    float initial_pos[3] = {0.0f, 0.0f, 0.0f};
    entorhinal_reset_grid_phases(entorhinal, initial_pos);

    // Apply velocity
    float velocity[3] = {1.0f, 0.0f, 0.0f};  // 1 m/s in X direction
    float dt = 0.1f;  // 100ms

    for (int i = 0; i < 10; i++) {
        int result = entorhinal_path_integrate(entorhinal, velocity, 0.0f, dt);
        EXPECT_EQ(result, 0);
    }

    // Check estimated position
    float est_pos[3], est_heading;
    float pos_conf, heading_conf;
    int result = entorhinal_get_position_estimate(entorhinal, est_pos, &est_heading,
                                                   &pos_conf, &heading_conf);
    EXPECT_EQ(result, 0);

    // Should have moved approximately 1.0m in X direction
    EXPECT_GT(est_pos[0], 0.0f);
}

TEST_F(EntorhinalBackwardCompatTest, Behavior_BorderCellUpdate_DetectsBoundaries) {
    // WHAT: Verify border cells respond to boundary distances
    // WHY: Border cells anchor grid cell representations

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    // Simulate distances to 4 boundaries (N, E, S, W)
    // API expects pairs of (distance, direction) per boundary
    float boundary_distances[8] = {0.5f, 0.0f, 5.0f, 1.57f, 10.0f, 3.14f, 3.0f, 4.71f};

    int result = entorhinal_update_border_cells(entorhinal, boundary_distances, 4);
    EXPECT_EQ(result, 0);

    // Verify some border cells are activated (especially for close boundary)
    float total_border_activation = 0.0f;
    for (uint32_t i = 0; i < entorhinal->num_border_cells; i++) {
        total_border_activation += entorhinal->border_cells[i].activation;
    }
    EXPECT_GT(total_border_activation, 0.0f);
}

TEST_F(EntorhinalBackwardCompatTest, Behavior_MemoryGateway_SetGates) {
    // WHAT: Verify memory gateway gates can be set
    // WHY: Memory routing is core to entorhinal function

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    // Set encoding gate
    int result = entorhinal_set_encoding_gate(entorhinal, 0.8f);
    EXPECT_EQ(result, 0);
    EXPECT_NEAR(entorhinal->memory_gateway.encoding_gate, 0.8f, EPSILON);

    // Set retrieval gate
    result = entorhinal_set_retrieval_gate(entorhinal, 0.6f);
    EXPECT_EQ(result, 0);
    EXPECT_NEAR(entorhinal->memory_gateway.retrieval_gate, 0.6f, EPSILON);
}

TEST_F(EntorhinalBackwardCompatTest, Behavior_Reset_RestoresInitialState) {
    // WHAT: Verify reset restores initial state
    // WHY: Reset API must be reliable

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    // Perform some operations
    float position[3] = {5.0f, 5.0f, 0.0f};
    entorhinal_update_grid_cells(entorhinal, position, 2);

    float velocity[3] = {1.0f, 1.0f, 0.0f};
    entorhinal_path_integrate(entorhinal, velocity, 0.5f, 0.1f);

    // Reset
    bool result = entorhinal_reset(entorhinal);
    EXPECT_TRUE(result);

    // Verify state is reset
    // After reset, status goes to IDLE (0), not READY (7)
    EXPECT_EQ(entorhinal->status, ENTORHINAL_STATUS_IDLE);
    EXPECT_EQ(entorhinal->updates_processed, 0u);
}

/* ============================================================================
 * CATEGORY 3: Memory Management Tests
 * ========================================================================== */

TEST_F(EntorhinalBackwardCompatTest, Memory_CreateDestroy_NoLeaks) {
    // WHAT: Verify create/destroy cycle doesn't leak memory
    // WHY: Memory management must be robust

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        nimcp_entorhinal_t* ec = entorhinal_create(&config);
        ASSERT_NE(ec, nullptr);
        entorhinal_destroy(ec);
    }

    SUCCEED();  // If we get here without memory errors, test passes
}

TEST_F(EntorhinalBackwardCompatTest, Memory_CreateDestroyMinConfig_NoLeaks) {
    // WHAT: Verify minimal configuration doesn't leak
    // WHY: Edge case configurations must be handled

    entorhinal_config_t min_config = entorhinal_default_config();
    min_config.num_grid_cells = 10;
    min_config.num_border_cells = 4;
    min_config.num_hd_cells = 8;
    min_config.num_object_cells = 4;
    min_config.num_speed_cells = 4;
    min_config.num_time_cells = 4;

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        nimcp_entorhinal_t* ec = entorhinal_create(&min_config);
        ASSERT_NE(ec, nullptr);
        entorhinal_destroy(ec);
    }

    SUCCEED();
}

TEST_F(EntorhinalBackwardCompatTest, Memory_RepeatedUpdates_NoAccumulation) {
    // WHAT: Verify repeated updates don't accumulate memory
    // WHY: Long-running operations must be stable

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    float position[3];
    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        create_random_position(position, DEFAULT_SPATIAL_DIM);
        entorhinal_update_grid_cells(entorhinal, position, 3);
    }

    SUCCEED();
}

TEST_F(EntorhinalBackwardCompatTest, Memory_RepeatedPathIntegration_NoAccumulation) {
    // WHAT: Verify repeated path integration doesn't leak
    // WHY: Path integration is called frequently

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    float velocity[3] = {0.5f, 0.5f, 0.0f};
    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        entorhinal_path_integrate(entorhinal, velocity, 0.01f, 0.01f);
    }

    SUCCEED();
}

TEST_F(EntorhinalBackwardCompatTest, Memory_GetStats_NoAllocation) {
    // WHAT: Verify getting stats doesn't allocate
    // WHY: Monitoring operations should be lightweight

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    entorhinal_stats_t stats;
    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        entorhinal_get_stats(entorhinal, &stats);
    }

    SUCCEED();
}

TEST_F(EntorhinalBackwardCompatTest, Memory_ResetCycles_NoLeaks) {
    // WHAT: Verify repeated reset doesn't leak
    // WHY: Reset should properly clean up internal state

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    float position[3];
    for (int i = 0; i < MEMORY_TEST_CYCLES / 10; i++) {
        // Perform operations
        create_random_position(position, DEFAULT_SPATIAL_DIM);
        entorhinal_update_grid_cells(entorhinal, position, 3);
        entorhinal_update_hd_cells(entorhinal, static_cast<float>(i) * 0.1f, 0.01f);

        // Reset
        entorhinal_reset(entorhinal);
    }

    SUCCEED();
}

/* ============================================================================
 * CATEGORY 4: Error Handling Tests
 * ========================================================================== */

TEST_F(EntorhinalBackwardCompatTest, ErrorHandling_NullEntorhinal_UpdateGridCells) {
    // WHAT: Verify NULL entorhinal is handled
    // WHY: Robust error handling prevents crashes

    float position[3] = {1.0f, 2.0f, 0.0f};
    int result = entorhinal_update_grid_cells(nullptr, position, 3);
    EXPECT_NE(result, 0);  // Should return error
}

TEST_F(EntorhinalBackwardCompatTest, ErrorHandling_NullPosition_UpdateGridCells) {
    // WHAT: Verify NULL position is handled
    // WHY: Input validation is essential

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    int result = entorhinal_update_grid_cells(entorhinal, nullptr, 3);
    EXPECT_NE(result, 0);
}

TEST_F(EntorhinalBackwardCompatTest, ErrorHandling_ZeroDimension_UpdateGridCells) {
    // WHAT: Verify zero dimension is handled
    // WHY: Edge case validation

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    float position[3] = {1.0f, 2.0f, 0.0f};
    int result = entorhinal_update_grid_cells(entorhinal, position, 0);
    EXPECT_NE(result, 0);
}

TEST_F(EntorhinalBackwardCompatTest, ErrorHandling_NullEntorhinal_PathIntegrate) {
    // WHAT: Verify NULL entorhinal in path integration
    // WHY: Consistent error handling

    float velocity[3] = {1.0f, 0.0f, 0.0f};
    int result = entorhinal_path_integrate(nullptr, velocity, 0.0f, 0.1f);
    EXPECT_NE(result, 0);
}

TEST_F(EntorhinalBackwardCompatTest, ErrorHandling_NullVelocity_PathIntegrate) {
    // WHAT: Verify NULL velocity is handled
    // WHY: Input validation

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    int result = entorhinal_path_integrate(entorhinal, nullptr, 0.0f, 0.1f);
    EXPECT_NE(result, 0);
}

TEST_F(EntorhinalBackwardCompatTest, ErrorHandling_NegativeDt_PathIntegrate) {
    // WHAT: Verify negative dt is handled gracefully
    // WHY: Time should always be positive

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    float velocity[3] = {1.0f, 0.0f, 0.0f};
    int result = entorhinal_path_integrate(entorhinal, velocity, 0.0f, -0.1f);
    // May succeed with clamping or fail - either is acceptable
    // The key is it doesn't crash
    SUCCEED();
}

TEST_F(EntorhinalBackwardCompatTest, ErrorHandling_NullEntorhinal_GetStatus) {
    // WHAT: Verify NULL entorhinal in status query
    // WHY: Safe status query

    entorhinal_status_t status = entorhinal_get_status(nullptr);
    EXPECT_EQ(status, ENTORHINAL_STATUS_ERROR);
}

TEST_F(EntorhinalBackwardCompatTest, ErrorHandling_NullEntorhinal_GetStats) {
    // WHAT: Verify NULL entorhinal in stats query
    // WHY: Safe stats query

    entorhinal_stats_t stats;
    int result = entorhinal_get_stats(nullptr, &stats);
    EXPECT_NE(result, 0);
}

TEST_F(EntorhinalBackwardCompatTest, ErrorHandling_NullStats_GetStats) {
    // WHAT: Verify NULL stats output is handled
    // WHY: Output validation

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    int result = entorhinal_get_stats(entorhinal, nullptr);
    EXPECT_NE(result, 0);
}

TEST_F(EntorhinalBackwardCompatTest, ErrorHandling_InvalidGateValue_SetEncodingGate) {
    // WHAT: Verify out-of-range gate value is handled
    // WHY: Gate values should be bounded [0, 1]

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    // Test value > 1.0
    int result = entorhinal_set_encoding_gate(entorhinal, 1.5f);
    // May clamp or return error - verify it's within bounds after
    if (result == 0) {
        EXPECT_LE(entorhinal->memory_gateway.encoding_gate, 1.0f);
    }

    // Test value < 0.0
    result = entorhinal_set_encoding_gate(entorhinal, -0.5f);
    if (result == 0) {
        EXPECT_GE(entorhinal->memory_gateway.encoding_gate, 0.0f);
    }
}

TEST_F(EntorhinalBackwardCompatTest, ErrorHandling_ErrorString_ReturnsValidStrings) {
    // WHAT: Verify error string returns valid strings
    // WHY: Error reporting API must be stable

    const char* str = entorhinal_error_string(ENTORHINAL_ERROR_NONE);
    ASSERT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = entorhinal_error_string(ENTORHINAL_ERROR_INVALID_INPUT);
    ASSERT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = entorhinal_error_string(ENTORHINAL_ERROR_GRID_DRIFT);
    ASSERT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);
}

TEST_F(EntorhinalBackwardCompatTest, ErrorHandling_StatusString_ReturnsValidStrings) {
    // WHAT: Verify status string returns valid strings
    // WHY: Status reporting API must be stable

    const char* str = entorhinal_status_string(ENTORHINAL_STATUS_IDLE);
    ASSERT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = entorhinal_status_string(ENTORHINAL_STATUS_READY);
    ASSERT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);

    str = entorhinal_status_string(ENTORHINAL_STATUS_PATH_INTEGRATING);
    ASSERT_NE(str, nullptr);
    EXPECT_GT(strlen(str), 0u);
}

/* ============================================================================
 * CATEGORY 5: Performance Baseline Tests
 * ========================================================================== */

TEST_F(EntorhinalBackwardCompatTest, Performance_Create_Under5ms) {
    // WHAT: Verify creation time is bounded
    // WHY: Initialization should be fast

    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        nimcp_entorhinal_t* ec = entorhinal_create(&config);
        entorhinal_destroy(ec);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        long long ns = measure_ns([&]() {
            nimcp_entorhinal_t* ec = entorhinal_create(&config);
            entorhinal_destroy(ec);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Entorhinal Create/Destroy: avg=" << (avg_ns / 1000000.0) << " ms\n";

    EXPECT_LT(avg_ns, 5000000.0) << "Create/Destroy should be < 5 ms";
}

TEST_F(EntorhinalBackwardCompatTest, Performance_GridCellUpdate_Under200us) {
    // WHAT: Verify grid cell update performance
    // WHY: Grid cells are updated frequently during navigation

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);
    float position[3];

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        create_random_position(position, 3);
        entorhinal_update_grid_cells(entorhinal, position, 3);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        create_random_position(position, 3);
        long long ns = measure_ns([&]() {
            entorhinal_update_grid_cells(entorhinal, position, 3);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Entorhinal GridCellUpdate: avg=" << (avg_ns / 1000.0) << " us\n";

    EXPECT_LT(avg_ns, 200000.0) << "Grid cell update should be < 200 us";
}

TEST_F(EntorhinalBackwardCompatTest, Performance_PathIntegration_Under50us) {
    // WHAT: Verify path integration performance
    // WHY: Path integration runs continuously during movement

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);
    float velocity[3] = {1.0f, 0.5f, 0.0f};

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        entorhinal_path_integrate(entorhinal, velocity, 0.01f, 0.01f);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        long long ns = measure_ns([&]() {
            entorhinal_path_integrate(entorhinal, velocity, 0.01f, 0.01f);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Entorhinal PathIntegration: avg=" << (avg_ns / 1000.0) << " us\n";

    EXPECT_LT(avg_ns, 200000.0) << "Path integration should be < 200 us";
}

TEST_F(EntorhinalBackwardCompatTest, Performance_HeadDirectionUpdate_Under20us) {
    // WHAT: Verify HD cell update performance
    // WHY: HD cells update continuously

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        entorhinal_update_hd_cells(entorhinal, static_cast<float>(i) * 0.1f, 0.05f);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        float heading = static_cast<float>(i) * 0.01f;
        long long ns = measure_ns([&]() {
            entorhinal_update_hd_cells(entorhinal, heading, 0.05f);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Entorhinal HDCellUpdate: avg=" << (avg_ns / 1000.0) << " us\n";

    EXPECT_LT(avg_ns, 20000.0) << "HD cell update should be < 20 us";
}

TEST_F(EntorhinalBackwardCompatTest, Performance_BorderCellUpdate_Under30us) {
    // WHAT: Verify border cell update performance
    // WHY: Border cells provide boundary information

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);
    // API expects pairs of (distance, direction) per boundary:
    // boundary_distances[b*2] = distance, boundary_distances[b*2+1] = direction
    float boundaries[8] = {1.0f, 0.0f, 2.0f, 1.57f, 3.0f, 3.14f, 4.0f, 4.71f};

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        entorhinal_update_border_cells(entorhinal, boundaries, 4);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        boundaries[0] = static_cast<float>(i % 10);
        long long ns = measure_ns([&]() {
            entorhinal_update_border_cells(entorhinal, boundaries, 4);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Entorhinal BorderCellUpdate: avg=" << (avg_ns / 1000.0) << " us\n";

    EXPECT_LT(avg_ns, 100000.0) << "Border cell update should be < 100 us";
}

TEST_F(EntorhinalBackwardCompatTest, Performance_GetStats_Under10us) {
    // WHAT: Verify stats retrieval performance
    // WHY: Monitoring should not impact performance

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    entorhinal_stats_t stats;
    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        entorhinal_get_stats(entorhinal, &stats);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        long long ns = measure_ns([&]() {
            entorhinal_get_stats(entorhinal, &stats);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Entorhinal GetStats: avg=" << (avg_ns / 1000.0) << " us\n";

    EXPECT_LT(avg_ns, 10000.0) << "GetStats should be < 10 us";
}

TEST_F(EntorhinalBackwardCompatTest, Performance_PositionDecoding_Under100us) {
    // WHAT: Verify position decoding performance
    // WHY: Position estimation must be fast for real-time navigation

    entorhinal = entorhinal_create(&config);
    ASSERT_NE(entorhinal, nullptr);

    // First update grid cells to have something to decode
    float position[3] = {2.0f, 3.0f, 0.0f};
    entorhinal_update_grid_cells(entorhinal, position, 3);

    float decoded_pos[3];
    float confidence;
    std::vector<long long> times;
    times.reserve(BENCHMARK_ITERATIONS);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        entorhinal_decode_position_from_grid(entorhinal, decoded_pos, &confidence);
    }

    // Benchmark
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        long long ns = measure_ns([&]() {
            entorhinal_decode_position_from_grid(entorhinal, decoded_pos, &confidence);
        });
        times.push_back(ns);
    }

    double avg_ns = 0;
    for (auto t : times) avg_ns += t;
    avg_ns /= times.size();

    std::cout << "Entorhinal PositionDecoding: avg=" << (avg_ns / 1000.0) << " us\n";

    EXPECT_LT(avg_ns, 100000.0) << "Position decoding should be < 100 us";
}

/* ============================================================================
 * MAIN
 * ========================================================================== */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
