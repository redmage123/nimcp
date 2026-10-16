/**
 * @file test_biology_intra_coordinator.cpp
 * @brief Unit tests for Biology Intra-Layer Coordinator
 *
 * WHAT: Test suite for nimcp_biology_intra
 * WHY:  Verify correct coordination of biology layer modules
 * HOW:  Unit tests for create, init, update, and lifecycle operations
 *
 * @author NIMCP Development Team
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cstdlib>

extern "C" {
#include "integration/intra/biology/nimcp_biology_intra_coordinator.h"
#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class BiologyIntraCoordinatorTest : public ::testing::Test {
protected:
    nimcp_biology_intra_t coordinator = nullptr;
    nimcp_layer_registry_t registry = nullptr;

    void SetUp() override {
        /* Create registry for module registration */
        nimcp_layer_registry_config_t reg_config = nimcp_layer_registry_default_config();
        registry = nimcp_layer_registry_create(&reg_config);
        ASSERT_NE(registry, nullptr);

        /* Create biology intra-layer coordinator */
        nimcp_biology_intra_config_t config = nimcp_biology_intra_default_config();
        coordinator = nimcp_biology_intra_create(&config);
        ASSERT_NE(coordinator, nullptr);
    }

    void TearDown() override {
        if (coordinator) {
            nimcp_biology_intra_destroy(coordinator);
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

TEST(BiologyIntraCreateTest, CreateWithDefaultConfig) {
    nimcp_biology_intra_t coord = nimcp_biology_intra_create(nullptr);
    ASSERT_NE(coord, nullptr);
    nimcp_biology_intra_destroy(coord);
}

TEST(BiologyIntraCreateTest, CreateWithCustomConfig) {
    nimcp_biology_intra_config_t config = nimcp_biology_intra_default_config();
    config.enable_epigenetics = true;
    config.enable_neurogenesis = true;
    config.enable_gene_expression = true;
    config.epigenetics_genesis_coupling = 0.8f;
    config.epigenetics_expression_coupling = 0.7f;
    config.genesis_expression_coupling = 0.6f;

    nimcp_biology_intra_t coord = nimcp_biology_intra_create(&config);
    ASSERT_NE(coord, nullptr);
    nimcp_biology_intra_destroy(coord);
}

TEST(BiologyIntraCreateTest, CreateWithAllModulesDisabled) {
    nimcp_biology_intra_config_t config = nimcp_biology_intra_default_config();
    config.enable_epigenetics = false;
    config.enable_neurogenesis = false;
    config.enable_gene_expression = false;

    nimcp_biology_intra_t coord = nimcp_biology_intra_create(&config);
    ASSERT_NE(coord, nullptr);
    nimcp_biology_intra_destroy(coord);
}

TEST(BiologyIntraCreateTest, DestroyNull) {
    /* Should not crash */
    nimcp_biology_intra_destroy(nullptr);
}

TEST(BiologyIntraCreateTest, DefaultConfigValues) {
    nimcp_biology_intra_config_t config = nimcp_biology_intra_default_config();
    EXPECT_TRUE(config.enable_epigenetics);
    EXPECT_TRUE(config.enable_neurogenesis);
    EXPECT_TRUE(config.enable_gene_expression);
    EXPECT_GT(config.epigenetics_genesis_coupling, 0.0f);
    EXPECT_GT(config.coherence_threshold, 0.0f);
}

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(BiologyIntraCoordinatorTest, InitSuccess) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(BiologyIntraCoordinatorTest, InitNullCoordinator) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(nullptr, registry);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(BiologyIntraCoordinatorTest, InitNullRegistry) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(BiologyIntraCoordinatorTest, DoubleInit) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_ALREADY_REGISTERED);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(BiologyIntraCoordinatorTest, UpdateSuccess) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Update with 10ms timestep */
    err = nimcp_biology_intra_update(coordinator, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(BiologyIntraCoordinatorTest, UpdateMultipleTimes) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Multiple updates */
    for (int i = 0; i < 10; i++) {
        err = nimcp_biology_intra_update(coordinator, 0.01f);
        EXPECT_EQ(err, NIMCP_LAYER_OK);
    }
}

TEST_F(BiologyIntraCoordinatorTest, UpdateNotInitialized) {
    nimcp_layer_error_t err = nimcp_biology_intra_update(coordinator, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

TEST_F(BiologyIntraCoordinatorTest, UpdateNull) {
    nimcp_layer_error_t err = nimcp_biology_intra_update(nullptr, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(BiologyIntraCoordinatorTest, UpdateZeroDt) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Update with zero dt */
    err = nimcp_biology_intra_update(coordinator, 0.0f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

//=============================================================================
// Module Connection Tests
//=============================================================================

TEST_F(BiologyIntraCoordinatorTest, ConnectEpigeneticsNull) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* NULL module should fail */
    err = nimcp_biology_intra_connect_epigenetics(coordinator, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(BiologyIntraCoordinatorTest, ConnectNeurogenesisNull) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_biology_intra_connect_neurogenesis(coordinator, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(BiologyIntraCoordinatorTest, ConnectGeneExpressionNull) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_biology_intra_connect_gene_expression(coordinator, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(BiologyIntraCoordinatorTest, ConnectEpigeneticsSuccess) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Create a dummy module interface */
    int dummy_module = 42;
    nimcp_module_interface_t interface = {0};

    err = nimcp_biology_intra_connect_epigenetics(coordinator, &dummy_module, &interface);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Verify state reflects connection */
    nimcp_biology_intra_state_t state;
    nimcp_biology_intra_get_state(coordinator, &state);
    EXPECT_TRUE(state.epigenetics_active);
}

TEST_F(BiologyIntraCoordinatorTest, ConnectNeurogenesisSuccess) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    int dummy_module = 42;
    nimcp_module_interface_t interface = {0};

    err = nimcp_biology_intra_connect_neurogenesis(coordinator, &dummy_module, &interface);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_biology_intra_state_t state;
    nimcp_biology_intra_get_state(coordinator, &state);
    EXPECT_TRUE(state.neurogenesis_active);
}

TEST_F(BiologyIntraCoordinatorTest, ConnectGeneExpressionSuccess) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    int dummy_module = 42;
    nimcp_module_interface_t interface = {0};

    err = nimcp_biology_intra_connect_gene_expression(coordinator, &dummy_module, &interface);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_biology_intra_state_t state;
    nimcp_biology_intra_get_state(coordinator, &state);
    EXPECT_TRUE(state.gene_expression_active);
}

//=============================================================================
// State and Stats Tests
//=============================================================================

TEST_F(BiologyIntraCoordinatorTest, GetState) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_biology_intra_state_t state;
    err = nimcp_biology_intra_get_state(coordinator, &state);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_GE(state.layer_coherence, 0.0f);
}

TEST_F(BiologyIntraCoordinatorTest, GetStateNull) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_biology_intra_get_state(coordinator, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(BiologyIntraCoordinatorTest, GetStateNullCoordinator) {
    nimcp_biology_intra_state_t state;
    nimcp_layer_error_t err = nimcp_biology_intra_get_state(nullptr, &state);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(BiologyIntraCoordinatorTest, GetStats) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_biology_intra_stats_t stats;
    err = nimcp_biology_intra_get_stats(coordinator, &stats);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_EQ(stats.messages_sent, 0u);
}

TEST_F(BiologyIntraCoordinatorTest, GetStatsNull) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_biology_intra_get_stats(coordinator, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(BiologyIntraCoordinatorTest, GetStatsNullCoordinator) {
    nimcp_biology_intra_stats_t stats;
    nimcp_layer_error_t err = nimcp_biology_intra_get_stats(nullptr, &stats);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(BiologyIntraCoordinatorTest, GetCoherence) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    float coherence = nimcp_biology_intra_get_coherence(coordinator);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(BiologyIntraCoordinatorTest, GetCoherenceNull) {
    float coherence = nimcp_biology_intra_get_coherence(nullptr);
    EXPECT_EQ(coherence, -1.0f);
}

TEST_F(BiologyIntraCoordinatorTest, ResetStats) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_biology_intra_reset_stats(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(BiologyIntraCoordinatorTest, ResetStatsNull) {
    nimcp_layer_error_t err = nimcp_biology_intra_reset_stats(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

//=============================================================================
// Sync Tests
//=============================================================================

TEST_F(BiologyIntraCoordinatorTest, Sync) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_biology_intra_sync(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(BiologyIntraCoordinatorTest, SyncNull) {
    nimcp_layer_error_t err = nimcp_biology_intra_sync(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

//=============================================================================
// Messaging Tests
//=============================================================================

TEST_F(BiologyIntraCoordinatorTest, SendNull) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_biology_intra_send(coordinator, BIOLOGY_MODULE_EPIGENETICS, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(BiologyIntraCoordinatorTest, SendNullCoordinator) {
    nimcp_layer_msg_t msg = {0};
    nimcp_layer_error_t err = nimcp_biology_intra_send(nullptr, BIOLOGY_MODULE_EPIGENETICS, &msg);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(BiologyIntraCoordinatorTest, SendSuccess) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_layer_msg_t msg = {0};
    msg.header.msg_type = BIOLOGY_MSG_METHYLATION;
    msg.header.source_layer = NIMCP_LAYER_BIOLOGY;
    msg.header.target_layer = NIMCP_LAYER_BIOLOGY;

    err = nimcp_biology_intra_send(coordinator, BIOLOGY_MODULE_EPIGENETICS, &msg);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Verify stats updated */
    nimcp_biology_intra_stats_t stats;
    nimcp_biology_intra_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.messages_sent, 1u);
}

TEST_F(BiologyIntraCoordinatorTest, BroadcastNull) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_biology_intra_broadcast(coordinator, BIOLOGY_MODULE_EPIGENETICS, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(BiologyIntraCoordinatorTest, BroadcastNullCoordinator) {
    nimcp_layer_msg_t msg = {0};
    nimcp_layer_error_t err = nimcp_biology_intra_broadcast(nullptr, BIOLOGY_MODULE_EPIGENETICS, &msg);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(BiologyIntraCoordinatorTest, BroadcastSuccess) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_layer_msg_t msg = {0};
    msg.header.msg_type = BIOLOGY_MSG_CELL_DIVISION;
    msg.header.source_layer = NIMCP_LAYER_BIOLOGY;

    err = nimcp_biology_intra_broadcast(coordinator, BIOLOGY_MODULE_NEUROGENESIS, &msg);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Verify stats updated (broadcast sends to all modules) */
    nimcp_biology_intra_stats_t stats;
    nimcp_biology_intra_get_stats(coordinator, &stats);
    EXPECT_EQ(stats.messages_sent, BIOLOGY_MODULE_COUNT);
}

//=============================================================================
// Shutdown Tests
//=============================================================================

TEST_F(BiologyIntraCoordinatorTest, Shutdown) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_biology_intra_shutdown(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(BiologyIntraCoordinatorTest, ShutdownNotInitialized) {
    nimcp_layer_error_t err = nimcp_biology_intra_shutdown(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

TEST_F(BiologyIntraCoordinatorTest, ShutdownNull) {
    nimcp_layer_error_t err = nimcp_biology_intra_shutdown(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(BiologyIntraCoordinatorTest, DoubleShutdown) {
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_biology_intra_shutdown(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_biology_intra_shutdown(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

//=============================================================================
// Integration Scenario Tests
//=============================================================================

TEST_F(BiologyIntraCoordinatorTest, FullLifecycleWithModules) {
    /* Init coordinator */
    nimcp_layer_error_t err = nimcp_biology_intra_init(coordinator, registry);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Connect all three modules */
    int epi_module = 1, neuro_module = 2, gene_module = 3;
    nimcp_module_interface_t epi_iface = {0}, neuro_iface = {0}, gene_iface = {0};

    err = nimcp_biology_intra_connect_epigenetics(coordinator, &epi_module, &epi_iface);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    err = nimcp_biology_intra_connect_neurogenesis(coordinator, &neuro_module, &neuro_iface);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    err = nimcp_biology_intra_connect_gene_expression(coordinator, &gene_module, &gene_iface);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Verify all modules active */
    nimcp_biology_intra_state_t state;
    nimcp_biology_intra_get_state(coordinator, &state);
    EXPECT_TRUE(state.epigenetics_active);
    EXPECT_TRUE(state.neurogenesis_active);
    EXPECT_TRUE(state.gene_expression_active);

    /* Run updates */
    for (int i = 0; i < 100; i++) {
        err = nimcp_biology_intra_update(coordinator, 0.01f);
        EXPECT_EQ(err, NIMCP_LAYER_OK);
    }

    /* Check coherence remains valid */
    float coherence = nimcp_biology_intra_get_coherence(coordinator);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);

    /* Shutdown */
    err = nimcp_biology_intra_shutdown(coordinator);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(BiologyIntraCoordinatorTest, MessageTypesValid) {
    /* Verify message type constants are properly defined */
    EXPECT_GT(BIOLOGY_MSG_METHYLATION, 0u);
    EXPECT_GT(BIOLOGY_MSG_HISTONE_MOD, 0u);
    EXPECT_GT(BIOLOGY_MSG_CELL_DIVISION, 0u);
    EXPECT_GT(BIOLOGY_MSG_DIFFERENTIATION, 0u);
    EXPECT_GT(BIOLOGY_MSG_TRANSCRIPTION, 0u);
    EXPECT_GT(BIOLOGY_MSG_TRANSLATION, 0u);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
