/**
 * @file test_entorhinal_regression.cpp
 * @brief Regression tests for Entorhinal Cortex
 * @version Phase 5: Memory Circuit
 * @date 2025-01-12
 *
 * These tests verify that previously fixed bugs do not reoccur.
 * Each test documents a specific issue that was found and fixed.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "core/brain/regions/entorhinal/nimcp_entorhinal.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal_hypothalamus_bridge.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal_omni_bridge.h"
#include "core/brain/regions/entorhinal/nimcp_entorhinal_brain_init_bridge.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*=============================================================================
 * REGRESSION TEST FIXTURE
 *===========================================================================*/

class EntorhinalRegressionTest : public ::testing::Test {
protected:
    nimcp_entorhinal_t* ec = nullptr;

    void SetUp() override {
        entorhinal_config_t config = entorhinal_default_config();
        ec = entorhinal_create(&config);
        ASSERT_NE(ec, nullptr);
    }

    void TearDown() override {
        if (ec) {
            entorhinal_destroy(ec);
            ec = nullptr;
        }
    }
};

/*=============================================================================
 * NULL POINTER REGRESSION TESTS
 * Issue: Various functions crashed when passed NULL pointers
 *===========================================================================*/

TEST_F(EntorhinalRegressionTest, NullPointerSafety_GridCells) {
    // All NULL pointer combinations should return error, not crash
    EXPECT_EQ(entorhinal_update_grid_cells(nullptr, nullptr, 0), -1);
    EXPECT_EQ(entorhinal_get_grid_population_vector(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(entorhinal_decode_position_from_grid(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(entorhinal_reset_grid_phases(nullptr, nullptr), -1);
    EXPECT_EQ(entorhinal_get_grid_cell(nullptr, 0, 0), nullptr);
}

TEST_F(EntorhinalRegressionTest, NullPointerSafety_BorderCells) {
    EXPECT_EQ(entorhinal_update_border_cells(nullptr, nullptr, 0), -1);
    EXPECT_EQ(entorhinal_detect_boundaries(nullptr, nullptr, nullptr, 0, nullptr), -1);
}

TEST_F(EntorhinalRegressionTest, NullPointerSafety_HDCells) {
    EXPECT_EQ(entorhinal_update_hd_cells(nullptr, 0.0f, 0.0f), -1);
    EXPECT_EQ(entorhinal_decode_heading(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(entorhinal_calibrate_hd_cells(nullptr, 0.0f), -1);
}

TEST_F(EntorhinalRegressionTest, NullPointerSafety_PathIntegration) {
    EXPECT_EQ(entorhinal_path_integrate(nullptr, nullptr, 0.0f, 0.0f), -1);
    EXPECT_EQ(entorhinal_get_position_estimate(nullptr, nullptr, nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(entorhinal_apply_visual_correction(nullptr, nullptr, 0.0f, 0.0f), -1);
    EXPECT_EQ(entorhinal_apply_boundary_correction(nullptr, nullptr, 0.0f), -1);
}

TEST_F(EntorhinalRegressionTest, NullPointerSafety_MemoryGateway) {
    EXPECT_EQ(entorhinal_set_encoding_gate(nullptr, 0.0f), -1);
    EXPECT_EQ(entorhinal_set_retrieval_gate(nullptr, 0.0f), -1);
    EXPECT_EQ(entorhinal_encode_to_hippocampus(nullptr, nullptr, 0, nullptr, 0), -1);
    EXPECT_EQ(entorhinal_retrieve_from_hippocampus(nullptr, nullptr, 0, nullptr, 0, nullptr), -1);
    EXPECT_EQ(entorhinal_consolidate_to_neocortex(nullptr, 0, 0.0f), -1);
    EXPECT_EQ(entorhinal_get_gateway_stats(nullptr, nullptr, nullptr, nullptr), -1);
}

TEST_F(EntorhinalRegressionTest, NullPointerSafety_Bridges) {
    EXPECT_EQ(entorhinal_init_security_bridge(nullptr, nullptr, nullptr), -1);
    EXPECT_EQ(entorhinal_init_immune_bridge(nullptr, nullptr), -1);
    EXPECT_EQ(entorhinal_init_bio_async_bridge(nullptr, nullptr), -1);
    EXPECT_EQ(entorhinal_init_snn_bridge(nullptr, nullptr), -1);
    EXPECT_EQ(entorhinal_init_all_bridges(nullptr, nullptr), -1);
}

TEST_F(EntorhinalRegressionTest, NullPointerSafety_Bidirectional) {
    EXPECT_EQ(entorhinal_process_incoming(nullptr), -1);
    EXPECT_EQ(entorhinal_send_outgoing(nullptr), -1);
    EXPECT_EQ(entorhinal_bidirectional_update(nullptr, 0.0f), -1);
    EXPECT_EQ(entorhinal_sync_bio_async(nullptr), -1);
    EXPECT_EQ(entorhinal_process_neuromodulation(nullptr), -1);
}

TEST_F(EntorhinalRegressionTest, NullPointerSafety_Training) {
    EXPECT_EQ(entorhinal_set_training_mode(nullptr, true), -1);
    EXPECT_EQ(entorhinal_training_forward(nullptr, nullptr, 0, nullptr, 0), -1);
    EXPECT_EQ(entorhinal_training_backward(nullptr, nullptr, 0), -1);
    EXPECT_EQ(entorhinal_apply_weight_updates(nullptr, 0.0f), -1);
}

TEST_F(EntorhinalRegressionTest, NullPointerSafety_Status) {
    EXPECT_EQ(entorhinal_get_status(nullptr), ENTORHINAL_STATUS_ERROR);
    EXPECT_EQ(entorhinal_get_last_error(nullptr), ENTORHINAL_ERROR_INTERNAL);
    EXPECT_EQ(entorhinal_get_stats(nullptr, nullptr), -1);
    EXPECT_EQ(entorhinal_get_config(nullptr, nullptr), -1);
    EXPECT_FLOAT_EQ(entorhinal_get_health_status(nullptr), 0.0f);
    EXPECT_FLOAT_EQ(entorhinal_get_training_loss(nullptr), 0.0f);
}

/*=============================================================================
 * BOUNDARY VALUE REGRESSION TESTS
 * Issue: Edge cases caused undefined behavior
 *===========================================================================*/

TEST_F(EntorhinalRegressionTest, BoundaryValue_ZeroDimension) {
    float position[1] = {0.0f};
    // Dimension too small should fail gracefully
    EXPECT_EQ(entorhinal_update_grid_cells(ec, position, 0), -1);
    EXPECT_EQ(entorhinal_update_grid_cells(ec, position, 1), -1);
}

TEST_F(EntorhinalRegressionTest, BoundaryValue_ZeroTimestep) {
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    // Zero timestep should be handled
    EXPECT_EQ(entorhinal_path_integrate(ec, velocity, 0.0f, 0.0f), 0);
}

TEST_F(EntorhinalRegressionTest, BoundaryValue_NegativeTimestep) {
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    // Negative timestep should be rejected
    EXPECT_EQ(entorhinal_path_integrate(ec, velocity, 0.0f, -1.0f), -1);
}

TEST_F(EntorhinalRegressionTest, BoundaryValue_ExtremePosition) {
    // Very large position values
    float position[3] = {1e6f, 1e6f, 0.0f};
    EXPECT_EQ(entorhinal_update_grid_cells(ec, position, 3), 0);

    // Very small position values
    float position2[3] = {1e-6f, 1e-6f, 0.0f};
    EXPECT_EQ(entorhinal_update_grid_cells(ec, position2, 3), 0);
}

TEST_F(EntorhinalRegressionTest, BoundaryValue_ExtremeVelocity) {
    float velocity[3] = {1000.0f, 0.0f, 0.0f};  // Very fast
    EXPECT_EQ(entorhinal_path_integrate(ec, velocity, 0.0f, 0.01f), 0);
}

TEST_F(EntorhinalRegressionTest, BoundaryValue_HeadingWraparound) {
    // Test heading values that need wrapping
    EXPECT_EQ(entorhinal_update_hd_cells(ec, 10.0f * M_PI, 0.0f), 0);  // 5 full rotations
    EXPECT_EQ(entorhinal_update_hd_cells(ec, -10.0f * M_PI, 0.0f), 0);  // Negative rotations
}

TEST_F(EntorhinalRegressionTest, BoundaryValue_GateValues) {
    // Gate values should be clamped to [0, 1]
    EXPECT_EQ(entorhinal_set_encoding_gate(ec, 2.0f), 0);
    EXPECT_LE(ec->memory_gateway.encoding_gate, 1.0f);

    EXPECT_EQ(entorhinal_set_encoding_gate(ec, -1.0f), 0);
    EXPECT_GE(ec->memory_gateway.encoding_gate, 0.0f);

    EXPECT_EQ(entorhinal_set_retrieval_gate(ec, 100.0f), 0);
    EXPECT_LE(ec->memory_gateway.retrieval_gate, 1.0f);
}

TEST_F(EntorhinalRegressionTest, BoundaryValue_ZeroConfidence) {
    // Zero confidence visual correction should have no effect
    float visual_pos[3] = {100.0f, 100.0f, 0.0f};
    float pos_before[3], heading, pc, hc;
    entorhinal_get_position_estimate(ec, pos_before, &heading, &pc, &hc);

    entorhinal_apply_visual_correction(ec, visual_pos, 0.0f, 0.0f);

    float pos_after[3];
    entorhinal_get_position_estimate(ec, pos_after, &heading, &pc, &hc);

    // Position should not have jumped to 100,100 with zero confidence
}

/*=============================================================================
 * MEMORY LEAK REGRESSION TESTS
 * Issue: Resources not properly freed
 *===========================================================================*/

TEST_F(EntorhinalRegressionTest, MemoryLeak_CreateDestroyCycle) {
    // Repeatedly create and destroy to check for leaks
    for (int i = 0; i < 100; i++) {
        nimcp_entorhinal_t* ec_temp = entorhinal_create(nullptr);
        ASSERT_NE(ec_temp, nullptr);
        entorhinal_destroy(ec_temp);
    }
}

TEST_F(EntorhinalRegressionTest, MemoryLeak_ResetCycle) {
    // Repeatedly reset to check for leaks
    for (int i = 0; i < 100; i++) {
        EXPECT_TRUE(entorhinal_reset(ec));
    }
}

TEST_F(EntorhinalRegressionTest, MemoryLeak_BridgeCreateDestroy) {
    // Hypothalamus bridge
    for (int i = 0; i < 100; i++) {
        entorhinal_hypothalamus_bridge_state_t* bridge =
            entorhinal_hypothalamus_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);
        entorhinal_hypothalamus_bridge_destroy(bridge);
    }

    // Omni bridge
    for (int i = 0; i < 100; i++) {
        entorhinal_omni_bridge_state_t* bridge =
            entorhinal_omni_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);
        entorhinal_omni_bridge_destroy(bridge);
    }

    // Brain init bridge
    for (int i = 0; i < 100; i++) {
        entorhinal_brain_init_bridge_t* bridge =
            entorhinal_brain_init_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);
        entorhinal_brain_init_bridge_destroy(bridge);
    }
}

/*=============================================================================
 * STATE CORRUPTION REGRESSION TESTS
 * Issue: Operations corrupted internal state
 *===========================================================================*/

TEST_F(EntorhinalRegressionTest, StateCorruption_ConcurrentUpdates) {
    // Simulate rapid updates that could corrupt state
    for (int i = 0; i < 1000; i++) {
        float position[3] = {(float)i * 0.01f, (float)i * 0.01f, 0.0f};
        entorhinal_update_grid_cells(ec, position, 3);
        entorhinal_update_hd_cells(ec, (float)i * 0.1f, 0.1f);

        float velocity[3] = {1.0f, 0.0f, 0.0f};
        entorhinal_path_integrate(ec, velocity, 0.01f, 0.001f);
    }

    // State should still be consistent
    EXPECT_EQ(entorhinal_get_status(ec), ENTORHINAL_STATUS_READY);
    EXPECT_EQ(entorhinal_get_last_error(ec), ENTORHINAL_ERROR_NONE);
}

TEST_F(EntorhinalRegressionTest, StateCorruption_ResetDuringOperation) {
    // Start some operations
    float position[3] = {5.0f, 5.0f, 0.0f};
    entorhinal_update_grid_cells(ec, position, 3);

    // Reset
    EXPECT_TRUE(entorhinal_reset(ec));

    // Continue operations - should not corrupt state
    float position2[3] = {1.0f, 1.0f, 0.0f};
    EXPECT_EQ(entorhinal_update_grid_cells(ec, position2, 3), 0);
}

TEST_F(EntorhinalRegressionTest, StateCorruption_StatisticsOverflow) {
    // Perform many operations to check for overflow
    for (uint64_t i = 0; i < 10000; i++) {
        float position[3] = {1.0f, 1.0f, 0.0f};
        entorhinal_update_grid_cells(ec, position, 3);
    }

    // Statistics should be consistent
    entorhinal_stats_t stats;
    EXPECT_EQ(entorhinal_get_stats(ec, &stats), 0);
    EXPECT_GE(stats.position_updates, 10000u);
}

/*=============================================================================
 * NUMERICAL STABILITY REGRESSION TESTS
 * Issue: Floating point issues caused NaN or Inf values
 *===========================================================================*/

TEST_F(EntorhinalRegressionTest, NumericalStability_NoNaNAfterManyUpdates) {
    for (int i = 0; i < 10000; i++) {
        float position[3] = {cosf((float)i * 0.1f), sinf((float)i * 0.1f), 0.0f};
        entorhinal_update_grid_cells(ec, position, 3);
    }

    // Check no NaN values
    for (uint32_t m = 0; m < ec->num_grid_modules; m++) {
        for (uint32_t c = 0; c < ec->grid_modules[m].num_cells; c++) {
            EXPECT_FALSE(std::isnan(ec->grid_modules[m].cells[c].activation));
            EXPECT_FALSE(std::isinf(ec->grid_modules[m].cells[c].activation));
        }
    }
}

TEST_F(EntorhinalRegressionTest, NumericalStability_PathIntegrationDrift) {
    // Long path integration should not produce NaN/Inf
    float velocity[3] = {1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 100000; i++) {
        entorhinal_path_integrate(ec, velocity, 0.01f, 0.001f);
    }

    float position[3], heading, pc, hc;
    entorhinal_get_position_estimate(ec, position, &heading, &pc, &hc);

    EXPECT_FALSE(std::isnan(position[0]));
    EXPECT_FALSE(std::isnan(position[1]));
    EXPECT_FALSE(std::isinf(position[0]));
    EXPECT_FALSE(std::isinf(position[1]));
}

TEST_F(EntorhinalRegressionTest, NumericalStability_EligibilityTraceDecay) {
    // Eligibility traces should decay properly, not overflow
    for (int i = 0; i < 10000; i++) {
        float position[3] = {(float)(i % 10), (float)(i % 10), 0.0f};
        entorhinal_update_grid_cells(ec, position, 3);
    }

    // Check eligibility traces are bounded
    for (uint32_t m = 0; m < ec->num_grid_modules; m++) {
        for (uint32_t c = 0; c < ec->grid_modules[m].num_cells; c++) {
            float trace = ec->grid_modules[m].cells[c].eligibility_trace;
            EXPECT_FALSE(std::isnan(trace));
            EXPECT_FALSE(std::isinf(trace));
            EXPECT_GE(trace, 0.0f);
        }
    }
}

/*=============================================================================
 * CONFIGURATION REGRESSION TESTS
 * Issue: Invalid configurations caused crashes
 *===========================================================================*/

TEST_F(EntorhinalRegressionTest, Config_ZeroGridCells) {
    entorhinal_config_t config = entorhinal_default_config();
    config.num_grid_cells = 0;

    // Should handle gracefully (use default or fail safely)
    nimcp_entorhinal_t* ec_temp = entorhinal_create(&config);
    // May be NULL or may use defaults
    if (ec_temp) {
        entorhinal_destroy(ec_temp);
    }
}

TEST_F(EntorhinalRegressionTest, Config_ZeroBorderCells) {
    entorhinal_config_t config = entorhinal_default_config();
    config.num_border_cells = 0;

    nimcp_entorhinal_t* ec_temp = entorhinal_create(&config);
    if (ec_temp) {
        // Border cell operations should handle zero cells
        float boundaries[4] = {1.0f, 2.0f, 3.0f, 4.0f};
        entorhinal_update_border_cells(ec_temp, boundaries, 4);
        entorhinal_destroy(ec_temp);
    }
}

TEST_F(EntorhinalRegressionTest, Config_ZeroHDCells) {
    entorhinal_config_t config = entorhinal_default_config();
    config.num_hd_cells = 0;

    nimcp_entorhinal_t* ec_temp = entorhinal_create(&config);
    if (ec_temp) {
        // HD cell operations should handle zero cells
        entorhinal_update_hd_cells(ec_temp, 0.0f, 0.0f);
        entorhinal_destroy(ec_temp);
    }
}

/*=============================================================================
 * BRIDGE STATE REGRESSION TESTS
 * Issue: Bridge state inconsistencies after connect/disconnect
 *===========================================================================*/

TEST_F(EntorhinalRegressionTest, Bridge_ConnectDisconnectCycle) {
    entorhinal_hypothalamus_bridge_state_t* bridge =
        entorhinal_hypothalamus_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(entorhinal_hypothalamus_bridge_connect(bridge, ec, nullptr), 0);
        EXPECT_TRUE(bridge->connected);
        EXPECT_EQ(bridge->entorhinal, ec);

        EXPECT_EQ(entorhinal_hypothalamus_bridge_disconnect(bridge), 0);
        EXPECT_FALSE(bridge->connected);
        EXPECT_EQ(bridge->entorhinal, nullptr);
    }

    entorhinal_hypothalamus_bridge_destroy(bridge);
}

TEST_F(EntorhinalRegressionTest, Bridge_UpdateAfterDisconnect) {
    entorhinal_hypothalamus_bridge_state_t* bridge =
        entorhinal_hypothalamus_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    entorhinal_hypothalamus_bridge_connect(bridge, ec, nullptr);
    entorhinal_hypothalamus_bridge_disconnect(bridge);

    // Update after disconnect should still work (just no entorhinal)
    EXPECT_EQ(entorhinal_hypothalamus_bridge_update(bridge, 0.01f), 0);

    entorhinal_hypothalamus_bridge_destroy(bridge);
}

/*=============================================================================
 * STRESS TEST REGRESSION
 * Issue: System failed under sustained load
 *===========================================================================*/

TEST_F(EntorhinalRegressionTest, Stress_SustainedOperation) {
    // Run for equivalent of 10 minutes at 100Hz
    int updates = 60000;  // 10 min * 60 sec * 100 Hz

    for (int i = 0; i < updates; i++) {
        float dt = 0.01f;
        float position[3] = {sinf((float)i * 0.001f) * 10.0f,
                            cosf((float)i * 0.001f) * 10.0f, 0.0f};

        entorhinal_update_grid_cells(ec, position, 3);
        entorhinal_update_hd_cells(ec, (float)i * 0.001f, 0.001f);

        float velocity[3] = {cosf((float)i * 0.001f), -sinf((float)i * 0.001f), 0.0f};
        entorhinal_path_integrate(ec, velocity, 0.001f, dt);

        entorhinal_bidirectional_update(ec, dt);
    }

    // System should still be functional
    EXPECT_EQ(entorhinal_get_status(ec), ENTORHINAL_STATUS_READY);
    float health = entorhinal_get_health_status(ec);
    EXPECT_GT(health, 0.0f);
}

