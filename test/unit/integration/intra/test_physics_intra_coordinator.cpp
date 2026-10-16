/**
 * @file test_physics_intra_coordinator.cpp
 * @brief Unit tests for Physics Intra-Layer Coordinator
 *
 * WHAT: Test suite for nimcp_physics_intra
 * WHY:  Verify correct coordination of physics layer modules
 * HOW:  Unit tests for create, init, update, and lifecycle operations
 *
 * @author NIMCP Development Team
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cstdlib>

extern "C" {
#include "integration/intra/physics/nimcp_physics_intra_coordinator.h"
#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class PhysicsIntraCoordinatorTest : public ::testing::Test {
protected:
    nimcp_physics_intra_t coordinator = nullptr;
    nimcp_layer_registry_t registry = nullptr;

    void SetUp() override {
        /* Create registry for module registration */
        nimcp_layer_registry_config_t reg_config = nimcp_layer_registry_default_config();
        registry = nimcp_layer_registry_create(&reg_config);
        ASSERT_NE(registry, nullptr);

        /* Create physics intra-layer coordinator */
        nimcp_physics_intra_config_t config = nimcp_physics_intra_default_config();
        coordinator = nimcp_physics_intra_create(&config);
        ASSERT_NE(coordinator, nullptr);
    }

    void TearDown() override {
        if (coordinator) {
            nimcp_physics_intra_destroy(coordinator);
            coordinator = nullptr;
        }
        if (registry) {
            nimcp_layer_registry_destroy(registry);
            registry = nullptr;
        }
    }
};

//=============================================================================
// Creation and Destruction Tests
//=============================================================================

TEST(PhysicsIntraCreateTest, CreateWithDefaultConfig) {
    nimcp_physics_intra_t coord = nimcp_physics_intra_create(nullptr);
    ASSERT_NE(coord, nullptr);
    nimcp_physics_intra_destroy(coord);
}

TEST(PhysicsIntraCreateTest, CreateWithCustomConfig) {
    nimcp_physics_intra_config_t config = nimcp_physics_intra_default_config();
    config.enable_ephaptic = true;
    config.enable_info_geometry = true;
    config.enable_hh_dynamics = true;
    config.enable_thermodynamics = true;
    config.ephaptic_hh_coupling = 0.9f;
    config.enforce_energy_conservation = true;
    config.temperature_kelvin = 310.0f;

    nimcp_physics_intra_t coord = nimcp_physics_intra_create(&config);
    ASSERT_NE(coord, nullptr);
    nimcp_physics_intra_destroy(coord);
}

TEST(PhysicsIntraCreateTest, CreateWithAllModulesDisabled) {
    nimcp_physics_intra_config_t config = nimcp_physics_intra_default_config();
    config.enable_ephaptic = false;
    config.enable_info_geometry = false;
    config.enable_hh_dynamics = false;
    config.enable_thermodynamics = false;

    nimcp_physics_intra_t coord = nimcp_physics_intra_create(&config);
    ASSERT_NE(coord, nullptr);
    nimcp_physics_intra_destroy(coord);
}

TEST(PhysicsIntraCreateTest, DestroyNull) {
    /* Should not crash */
    nimcp_physics_intra_destroy(nullptr);
}

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(PhysicsIntraCoordinatorTest, InitSuccess) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsIntraCoordinatorTest, InitNullCoordinator) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(nullptr, registry);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(PhysicsIntraCoordinatorTest, InitNullRegistry) {
    /* May succeed or fail depending on implementation */
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, nullptr);
    /* Registry may be optional for standalone use */
    (void)err;
}

TEST_F(PhysicsIntraCoordinatorTest, DoubleInit) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_ALREADY_REGISTERED);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(PhysicsIntraCoordinatorTest, UpdateSuccess) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Update with 10ms timestep */
    err = nimcp_physics_intra_update(coordinator, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsIntraCoordinatorTest, UpdateMultipleTimes) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Multiple updates */
    for (int i = 0; i < 10; i++) {
        err = nimcp_physics_intra_update(coordinator, 0.01f);
        EXPECT_EQ(err, NIMCP_LAYER_OK);
    }
}

TEST_F(PhysicsIntraCoordinatorTest, UpdateNotInitialized) {
    nimcp_layer_error_t err = nimcp_physics_intra_update(coordinator, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

TEST_F(PhysicsIntraCoordinatorTest, UpdateNull) {
    nimcp_layer_error_t err = nimcp_physics_intra_update(nullptr, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(PhysicsIntraCoordinatorTest, UpdateZeroDt) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Update with zero dt */
    err = nimcp_physics_intra_update(coordinator, 0.0f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

//=============================================================================
// Module Connection Tests
//=============================================================================

TEST_F(PhysicsIntraCoordinatorTest, ConnectEphapticNull) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* NULL module should fail */
    err = nimcp_physics_intra_connect_ephaptic(coordinator, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(PhysicsIntraCoordinatorTest, ConnectInfoGeometryNull) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_physics_intra_connect_info_geometry(coordinator, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(PhysicsIntraCoordinatorTest, ConnectHHDynamicsNull) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_physics_intra_connect_hh_dynamics(coordinator, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(PhysicsIntraCoordinatorTest, ConnectThermodynamicsNull) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_physics_intra_connect_thermodynamics(coordinator, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

//=============================================================================
// Physical Constraint Tests
//=============================================================================

TEST_F(PhysicsIntraCoordinatorTest, UpdateEnergy) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_physics_intra_update_energy(coordinator, 0.1f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsIntraCoordinatorTest, UpdateEntropy) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Positive entropy change should succeed */
    err = nimcp_physics_intra_update_entropy(coordinator, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsIntraCoordinatorTest, CheckConstraints) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    uint32_t violations = 0;
    err = nimcp_physics_intra_check_constraints(coordinator, &violations);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_EQ(violations, 0u);  /* No violations initially */
}

//=============================================================================
// State and Stats Tests
//=============================================================================

TEST_F(PhysicsIntraCoordinatorTest, GetState) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_physics_intra_state_t state;
    err = nimcp_physics_intra_get_state(coordinator, &state);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_GE(state.layer_coherence, 0.0f);
}

TEST_F(PhysicsIntraCoordinatorTest, GetStateNull) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_physics_intra_get_state(coordinator, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(PhysicsIntraCoordinatorTest, GetStats) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_physics_intra_stats_t stats;
    err = nimcp_physics_intra_get_stats(coordinator, &stats);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_EQ(stats.messages_sent, 0u);
}

TEST_F(PhysicsIntraCoordinatorTest, GetStatsNull) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_physics_intra_get_stats(coordinator, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(PhysicsIntraCoordinatorTest, GetCoherence) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    float coherence = nimcp_physics_intra_get_coherence(coordinator);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(PhysicsIntraCoordinatorTest, GetCoherenceNull) {
    float coherence = nimcp_physics_intra_get_coherence(nullptr);
    EXPECT_EQ(coherence, -1.0f);
}

TEST_F(PhysicsIntraCoordinatorTest, ResetStats) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_physics_intra_reset_stats(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsIntraCoordinatorTest, ResetStatsNull) {
    nimcp_layer_error_t err = nimcp_physics_intra_reset_stats(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

//=============================================================================
// Sync Tests
//=============================================================================

TEST_F(PhysicsIntraCoordinatorTest, Sync) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_physics_intra_sync(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsIntraCoordinatorTest, SyncNull) {
    nimcp_layer_error_t err = nimcp_physics_intra_sync(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(PhysicsIntraCoordinatorTest, SyncNotInitialized) {
    nimcp_layer_error_t err = nimcp_physics_intra_sync(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

//=============================================================================
// Messaging Tests
//=============================================================================

TEST_F(PhysicsIntraCoordinatorTest, SendNull) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_physics_intra_send(coordinator, PHYSICS_MODULE_EPHAPTIC, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(PhysicsIntraCoordinatorTest, BroadcastNull) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_physics_intra_broadcast(coordinator, PHYSICS_MODULE_EPHAPTIC, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

//=============================================================================
// Shutdown Tests
//=============================================================================

TEST_F(PhysicsIntraCoordinatorTest, Shutdown) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_physics_intra_shutdown(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(PhysicsIntraCoordinatorTest, ShutdownNotInitialized) {
    nimcp_layer_error_t err = nimcp_physics_intra_shutdown(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

TEST_F(PhysicsIntraCoordinatorTest, ShutdownNull) {
    nimcp_layer_error_t err = nimcp_physics_intra_shutdown(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(PhysicsIntraCoordinatorTest, DoubleShutdown) {
    nimcp_layer_error_t err = nimcp_physics_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_physics_intra_shutdown(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_physics_intra_shutdown(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
