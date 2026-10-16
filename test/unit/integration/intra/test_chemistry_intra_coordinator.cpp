/**
 * @file test_chemistry_intra_coordinator.cpp
 * @brief Unit tests for Chemistry Intra-Layer Coordinator
 *
 * WHAT: Test suite for nimcp_chemistry_intra
 * WHY:  Verify correct coordination of chemistry layer modules
 * HOW:  Unit tests for create, init, update, and lifecycle operations
 *
 * @author NIMCP Development Team
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cstdlib>

extern "C" {
#include "integration/intra/chemistry/nimcp_chemistry_intra_coordinator.h"
#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ChemistryIntraCoordinatorTest : public ::testing::Test {
protected:
    nimcp_chemistry_intra_t coordinator = nullptr;
    nimcp_layer_registry_t registry = nullptr;

    void SetUp() override {
        /* Create registry for module registration */
        nimcp_layer_registry_config_t reg_config = nimcp_layer_registry_default_config();
        registry = nimcp_layer_registry_create(&reg_config);
        ASSERT_NE(registry, nullptr);

        /* Create chemistry intra-layer coordinator */
        nimcp_chemistry_intra_config_t config = nimcp_chemistry_intra_default_config();
        coordinator = nimcp_chemistry_intra_create(&config);
        ASSERT_NE(coordinator, nullptr);
    }

    void TearDown() override {
        if (coordinator) {
            nimcp_chemistry_intra_destroy(coordinator);
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

TEST(ChemistryIntraCreateTest, CreateWithDefaultConfig) {
    nimcp_chemistry_intra_t coord = nimcp_chemistry_intra_create(nullptr);
    ASSERT_NE(coord, nullptr);
    nimcp_chemistry_intra_destroy(coord);
}

TEST(ChemistryIntraCreateTest, CreateWithCustomConfig) {
    nimcp_chemistry_intra_config_t config = nimcp_chemistry_intra_default_config();
    config.enable_ph = true;
    config.enable_no_signaling = true;
    config.enable_neurovascular = true;
    config.ph_no_coupling = 0.8f;
    config.ph_vascular_coupling = 0.7f;
    config.baseline_ph = 7.4f;

    nimcp_chemistry_intra_t coord = nimcp_chemistry_intra_create(&config);
    ASSERT_NE(coord, nullptr);
    nimcp_chemistry_intra_destroy(coord);
}

TEST(ChemistryIntraCreateTest, CreateWithAllModulesDisabled) {
    nimcp_chemistry_intra_config_t config = nimcp_chemistry_intra_default_config();
    config.enable_ph = false;
    config.enable_no_signaling = false;
    config.enable_neurovascular = false;

    nimcp_chemistry_intra_t coord = nimcp_chemistry_intra_create(&config);
    ASSERT_NE(coord, nullptr);
    nimcp_chemistry_intra_destroy(coord);
}

TEST(ChemistryIntraCreateTest, DestroyNull) {
    /* Should not crash */
    nimcp_chemistry_intra_destroy(nullptr);
}

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(ChemistryIntraCoordinatorTest, InitSuccess) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(ChemistryIntraCoordinatorTest, InitNullCoordinator) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(nullptr, registry);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryIntraCoordinatorTest, InitNullRegistry) {
    /* May succeed or fail depending on implementation */
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, nullptr);
    /* Registry may be optional for standalone use */
    (void)err;
}

TEST_F(ChemistryIntraCoordinatorTest, DoubleInit) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_ALREADY_REGISTERED);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(ChemistryIntraCoordinatorTest, UpdateSuccess) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Update with 10ms timestep */
    err = nimcp_chemistry_intra_update(coordinator, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(ChemistryIntraCoordinatorTest, UpdateMultipleTimes) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Multiple updates */
    for (int i = 0; i < 10; i++) {
        err = nimcp_chemistry_intra_update(coordinator, 0.01f);
        EXPECT_EQ(err, NIMCP_LAYER_OK);
    }
}

TEST_F(ChemistryIntraCoordinatorTest, UpdateNotInitialized) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_update(coordinator, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

TEST_F(ChemistryIntraCoordinatorTest, UpdateNull) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_update(nullptr, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryIntraCoordinatorTest, UpdateZeroDt) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Update with zero dt */
    err = nimcp_chemistry_intra_update(coordinator, 0.0f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

//=============================================================================
// Module Connection Tests
//=============================================================================

TEST_F(ChemistryIntraCoordinatorTest, ConnectPhNull) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* NULL module should fail */
    err = nimcp_chemistry_intra_connect_ph(coordinator, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryIntraCoordinatorTest, ConnectNoSignalingNull) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_intra_connect_no_signaling(coordinator, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryIntraCoordinatorTest, ConnectNeurovascularNull) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_intra_connect_neurovascular(coordinator, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

//=============================================================================
// State and Stats Tests
//=============================================================================

TEST_F(ChemistryIntraCoordinatorTest, GetState) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_chemistry_intra_state_t state;
    err = nimcp_chemistry_intra_get_state(coordinator, &state);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_GE(state.layer_coherence, 0.0f);
}

TEST_F(ChemistryIntraCoordinatorTest, GetStateNull) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_intra_get_state(coordinator, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryIntraCoordinatorTest, GetStats) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_chemistry_intra_stats_t stats;
    err = nimcp_chemistry_intra_get_stats(coordinator, &stats);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_EQ(stats.messages_sent, 0u);
}

TEST_F(ChemistryIntraCoordinatorTest, GetStatsNull) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_intra_get_stats(coordinator, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryIntraCoordinatorTest, GetCoherence) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    float coherence = nimcp_chemistry_intra_get_coherence(coordinator);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(ChemistryIntraCoordinatorTest, GetCoherenceNull) {
    float coherence = nimcp_chemistry_intra_get_coherence(nullptr);
    EXPECT_EQ(coherence, -1.0f);
}

TEST_F(ChemistryIntraCoordinatorTest, ResetStats) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_intra_reset_stats(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(ChemistryIntraCoordinatorTest, ResetStatsNull) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_reset_stats(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

//=============================================================================
// Sync Tests
//=============================================================================

TEST_F(ChemistryIntraCoordinatorTest, Sync) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_intra_sync(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(ChemistryIntraCoordinatorTest, SyncNull) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_sync(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryIntraCoordinatorTest, SyncNotInitialized) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_sync(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

//=============================================================================
// Messaging Tests
//=============================================================================

TEST_F(ChemistryIntraCoordinatorTest, SendNull) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_intra_send(coordinator, CHEMISTRY_MODULE_PH, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryIntraCoordinatorTest, BroadcastNull) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_intra_broadcast(coordinator, CHEMISTRY_MODULE_PH, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

//=============================================================================
// Shutdown Tests
//=============================================================================

TEST_F(ChemistryIntraCoordinatorTest, Shutdown) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_intra_shutdown(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(ChemistryIntraCoordinatorTest, ShutdownNotInitialized) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_shutdown(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

TEST_F(ChemistryIntraCoordinatorTest, ShutdownNull) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_shutdown(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryIntraCoordinatorTest, DoubleShutdown) {
    nimcp_layer_error_t err = nimcp_chemistry_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_intra_shutdown(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_intra_shutdown(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
