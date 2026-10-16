/**
 * @file test_layer_coordinator.cpp
 * @brief Unit tests for Layer Coordinator
 *
 * WHAT: Test suite for nimcp_layer_coordinator
 * WHY:  Verify correct coordination of all layers
 * HOW:  Unit tests for create, init, update, and lifecycle operations
 *
 * @author NIMCP Development Team
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cstdlib>

extern "C" {
#include "integration/core/nimcp_layer_coordinator.h"
#include "integration/core/nimcp_layer_types.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class LayerCoordinatorTest : public ::testing::Test {
protected:
    nimcp_layer_coordinator_t coordinator = nullptr;

    void SetUp() override {
        nimcp_layer_coordinator_config_t config = nimcp_layer_coordinator_default_config();
        coordinator = nimcp_layer_coordinator_create(&config, nullptr);
        ASSERT_NE(coordinator, nullptr);
    }

    void TearDown() override {
        if (coordinator) {
            nimcp_layer_coordinator_destroy(coordinator);
            coordinator = nullptr;
        }
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST(LayerCoordinatorCreateTest, CreateWithDefaultConfig) {
    nimcp_layer_coordinator_t coord = nimcp_layer_coordinator_create(nullptr, nullptr);
    ASSERT_NE(coord, nullptr);
    nimcp_layer_coordinator_destroy(coord);
}

TEST(LayerCoordinatorCreateTest, CreateWithCustomConfig) {
    nimcp_layer_coordinator_config_t config = nimcp_layer_coordinator_default_config();
    config.update_interval_ms = 5;
    config.parallel_layer_update = true;

    nimcp_layer_coordinator_t coord = nimcp_layer_coordinator_create(&config, nullptr);
    ASSERT_NE(coord, nullptr);
    nimcp_layer_coordinator_destroy(coord);
}

TEST(LayerCoordinatorCreateTest, DestroyNull) {
    /* Should not crash */
    nimcp_layer_coordinator_destroy(nullptr);
}

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(LayerCoordinatorTest, InitAllSuccess) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_init_all(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(LayerCoordinatorTest, InitAllNull) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_init_all(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(LayerCoordinatorTest, DoubleInitAll) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_init_all(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_layer_coordinator_init_all(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_ALREADY_REGISTERED);
}

//=============================================================================
// Layer Registration Tests
//=============================================================================

TEST_F(LayerCoordinatorTest, RegisterStandardLayers) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_register_standard_layers(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(LayerCoordinatorTest, RegisterStandardConnections) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_register_standard_layers(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_layer_coordinator_register_standard_connections(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(LayerCoordinatorTest, RegisterLayerWithConfig) {
    nimcp_layer_config_t layer_config = nimcp_layer_default_config(NIMCP_LAYER_PHYSICS);
    nimcp_layer_error_t err = nimcp_layer_coordinator_register_layer(coordinator, &layer_config);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(LayerCoordinatorTest, Update) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_init_all(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Update with 10ms timestep */
    err = nimcp_layer_coordinator_update(coordinator, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(LayerCoordinatorTest, UpdateNotInitialized) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_update(coordinator, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

TEST_F(LayerCoordinatorTest, UpdateNull) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_update(nullptr, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(LayerCoordinatorTest, UpdateSpecificLayer) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_register_standard_layers(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_layer_coordinator_init_all(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_layer_coordinator_update_layer(coordinator, NIMCP_LAYER_PHYSICS, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

//=============================================================================
// State and Stats Tests
//=============================================================================

TEST_F(LayerCoordinatorTest, GetState) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_init_all(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_layer_coordinator_state_t state = nimcp_layer_coordinator_get_state(coordinator);
    EXPECT_EQ(state, NIMCP_COORD_STATE_RUNNING);
}

TEST_F(LayerCoordinatorTest, GetStats) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_init_all(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_layer_coordinator_stats_t stats;
    err = nimcp_layer_coordinator_get_stats(coordinator, &stats);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_GE(stats.global_coherence, 0.0f);
}

TEST_F(LayerCoordinatorTest, GetStatsNull) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_get_stats(coordinator, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(LayerCoordinatorTest, ResetStats) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_init_all(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_layer_coordinator_reset_stats(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(LayerCoordinatorTest, GetCoherence) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_init_all(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    float coherence = nimcp_layer_coordinator_get_coherence(coordinator);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(LayerCoordinatorTest, GetLayerCoherence) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_register_standard_layers(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_layer_coordinator_init_all(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    float coherence = nimcp_layer_coordinator_get_layer_coherence(coordinator, NIMCP_LAYER_PHYSICS);
    EXPECT_GE(coherence, -1.0f);  /* -1 indicates error or not available */
}

//=============================================================================
// Registry and Router Access Tests
//=============================================================================

TEST_F(LayerCoordinatorTest, GetRegistry) {
    nimcp_layer_registry_t registry = nimcp_layer_coordinator_get_registry(coordinator);
    EXPECT_NE(registry, nullptr);
}

TEST_F(LayerCoordinatorTest, GetRouter) {
    nimcp_inter_layer_router_t router = nimcp_layer_coordinator_get_router(coordinator);
    EXPECT_NE(router, nullptr);
}

//=============================================================================
// Pause and Resume Tests
//=============================================================================

TEST_F(LayerCoordinatorTest, PauseAndResume) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_init_all(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_layer_coordinator_pause(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_layer_coordinator_state_t state = nimcp_layer_coordinator_get_state(coordinator);
    EXPECT_EQ(state, NIMCP_COORD_STATE_PAUSED);

    err = nimcp_layer_coordinator_resume(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    state = nimcp_layer_coordinator_get_state(coordinator);
    EXPECT_EQ(state, NIMCP_COORD_STATE_RUNNING);
}

//=============================================================================
// Sync Tests
//=============================================================================

TEST_F(LayerCoordinatorTest, Sync) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_init_all(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_layer_coordinator_sync(coordinator, 1000);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

//=============================================================================
// Reset Tests
//=============================================================================

TEST_F(LayerCoordinatorTest, Reset) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_init_all(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_layer_coordinator_reset(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

//=============================================================================
// Shutdown Tests
//=============================================================================

TEST_F(LayerCoordinatorTest, Shutdown) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_init_all(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_layer_coordinator_shutdown(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(LayerCoordinatorTest, ShutdownNotInitialized) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_shutdown(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

//=============================================================================
// Error Handling Tests
//=============================================================================

TEST_F(LayerCoordinatorTest, GetLastError) {
    nimcp_layer_error_t err = nimcp_layer_coordinator_get_last_error(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);  /* No error initially */
}

TEST_F(LayerCoordinatorTest, GetLastErrorMsg) {
    const char* msg = nimcp_layer_coordinator_get_last_error_msg(coordinator);
    EXPECT_NE(msg, nullptr);
}

TEST_F(LayerCoordinatorTest, ClearError) {
    /* Should not crash */
    nimcp_layer_coordinator_clear_error(coordinator);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
