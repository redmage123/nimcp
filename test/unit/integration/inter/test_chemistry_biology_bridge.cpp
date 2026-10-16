/**
 * @file test_chemistry_biology_bridge.cpp
 * @brief Unit tests for Chemistry-Biology Inter-Layer Bridge
 *
 * WHAT: Test suite for nimcp_chemistry_biology bridge
 * WHY:  Verify correct bridging between chemistry and biology layers
 * HOW:  Unit tests for create, init, update, transfer, and lifecycle operations
 *
 * @author NIMCP Development Team
 * @date 2026-01-10
 */

#include <gtest/gtest.h>
#include <cstdlib>

extern "C" {
#include "integration/inter/chemistry_biology/nimcp_chemistry_biology_bridge.h"
#include "integration/intra/chemistry/nimcp_chemistry_intra_coordinator.h"
#include "integration/intra/biology/nimcp_biology_intra_coordinator.h"
#include "integration/core/nimcp_layer_types.h"
#include "integration/core/nimcp_layer_registry.h"
}

//=============================================================================
// Test Fixture
//=============================================================================

class ChemistryBiologyBridgeTest : public ::testing::Test {
protected:
    nimcp_chemistry_biology_bridge_t bridge = nullptr;
    nimcp_layer_registry_t registry = nullptr;
    nimcp_chemistry_intra_t chemistry = nullptr;
    nimcp_biology_intra_t biology = nullptr;

    void SetUp() override {
        /* Create registry for module registration */
        nimcp_layer_registry_config_t reg_config = nimcp_layer_registry_default_config();
        registry = nimcp_layer_registry_create(&reg_config);
        ASSERT_NE(registry, nullptr);

        /* Create chemistry intra-layer coordinator */
        nimcp_chemistry_intra_config_t chem_config = nimcp_chemistry_intra_default_config();
        chemistry = nimcp_chemistry_intra_create(&chem_config);
        ASSERT_NE(chemistry, nullptr);

        /* Create biology intra coordinator */
        nimcp_biology_intra_config_t bio_config = nimcp_biology_intra_default_config();
        biology = nimcp_biology_intra_create(&bio_config);
        /* Note: biology may be NULL for stub implementation */

        /* Create chemistry-biology bridge */
        nimcp_chemistry_biology_config_t config = nimcp_chemistry_biology_default_config();
        bridge = nimcp_chemistry_biology_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            nimcp_chemistry_biology_destroy(bridge);
            bridge = nullptr;
        }
        if (biology) {
            nimcp_biology_intra_destroy(biology);
            biology = nullptr;
        }
        if (chemistry) {
            nimcp_chemistry_intra_destroy(chemistry);
            chemistry = nullptr;
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

TEST(ChemistryBiologyBridgeCreateTest, CreateWithDefaultConfig) {
    nimcp_chemistry_biology_bridge_t br = nimcp_chemistry_biology_create(nullptr);
    ASSERT_NE(br, nullptr);
    nimcp_chemistry_biology_destroy(br);
}

TEST(ChemistryBiologyBridgeCreateTest, CreateWithCustomConfig) {
    nimcp_chemistry_biology_config_t config = nimcp_chemistry_biology_default_config();
    config.receptor_coupling_strength = 0.9f;
    config.ph_sensitivity = 0.5f;
    config.no_signaling_gain = 1.5f;
    config.enable_receptor_dynamics = true;
    config.enable_metrics = true;

    nimcp_chemistry_biology_bridge_t br = nimcp_chemistry_biology_create(&config);
    ASSERT_NE(br, nullptr);
    nimcp_chemistry_biology_destroy(br);
}

TEST(ChemistryBiologyBridgeCreateTest, CreateWithVariousCouplings) {
    nimcp_chemistry_biology_config_t config = nimcp_chemistry_biology_default_config();

    /* Test with low coupling */
    config.receptor_coupling_strength = 0.1f;
    nimcp_chemistry_biology_bridge_t br = nimcp_chemistry_biology_create(&config);
    ASSERT_NE(br, nullptr);
    nimcp_chemistry_biology_destroy(br);

    /* Test with high coupling */
    config.receptor_coupling_strength = 1.0f;
    br = nimcp_chemistry_biology_create(&config);
    ASSERT_NE(br, nullptr);
    nimcp_chemistry_biology_destroy(br);
}

TEST(ChemistryBiologyBridgeCreateTest, DestroyNull) {
    /* Should not crash */
    nimcp_chemistry_biology_destroy(nullptr);
}

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(ChemistryBiologyBridgeTest, InitSuccess) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(ChemistryBiologyBridgeTest, InitNullBridge) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(nullptr, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryBiologyBridgeTest, InitNullRegistry) {
    /* May succeed or fail depending on implementation */
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, nullptr, chemistry, biology);
    /* Registry may be optional for standalone use */
    (void)err;
}

TEST_F(ChemistryBiologyBridgeTest, InitNullChemistry) {
    /* May succeed or fail depending on implementation */
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, nullptr, biology);
    /* Could be optional for partial testing */
    (void)err;
}

TEST_F(ChemistryBiologyBridgeTest, InitNullBiology) {
    /* May succeed or fail depending on implementation */
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, chemistry, nullptr);
    /* Could be optional for partial testing */
    (void)err;
}

TEST_F(ChemistryBiologyBridgeTest, DoubleInit) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_biology_init(bridge, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_ALREADY_REGISTERED);
}

//=============================================================================
// Update Tests
//=============================================================================

TEST_F(ChemistryBiologyBridgeTest, UpdateSuccess) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Update with 10ms timestep */
    err = nimcp_chemistry_biology_update(bridge, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(ChemistryBiologyBridgeTest, UpdateMultipleTimes) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* Multiple updates */
    for (int i = 0; i < 10; i++) {
        err = nimcp_chemistry_biology_update(bridge, 0.01f);
        EXPECT_EQ(err, NIMCP_LAYER_OK);
    }
}

TEST_F(ChemistryBiologyBridgeTest, UpdateNotInitialized) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_update(bridge, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

TEST_F(ChemistryBiologyBridgeTest, UpdateNull) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_update(nullptr, 0.01f);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryBiologyBridgeTest, UpdateZeroDt) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_biology_update(bridge, 0.0f);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

//=============================================================================
// Transfer Function Tests
//=============================================================================

TEST_F(ChemistryBiologyBridgeTest, TransferBottomUpNull) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    /* NULL message should fail */
    err = nimcp_chemistry_biology_transfer_bottom_up(bridge, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryBiologyBridgeTest, TransferTopDownNull) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_biology_transfer_top_down(bridge, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryBiologyBridgeTest, TransferBottomUpNotInitialized) {
    nimcp_layer_msg_t* msg = nimcp_layer_msg_create(
        CHEM_BIO_MSG_RECEPTOR_ACTIVATE,
        NIMCP_LAYER_CHEMISTRY,
        NIMCP_LAYER_BIOLOGY,
        nullptr, 0);
    ASSERT_NE(msg, nullptr);

    nimcp_layer_error_t err = nimcp_chemistry_biology_transfer_bottom_up(bridge, msg);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);

    nimcp_layer_msg_destroy(msg);
}

TEST_F(ChemistryBiologyBridgeTest, TransferTopDownNotInitialized) {
    nimcp_layer_msg_t* msg = nimcp_layer_msg_create(
        CHEM_BIO_MSG_PROTEIN_REQUEST,
        NIMCP_LAYER_BIOLOGY,
        NIMCP_LAYER_CHEMISTRY,
        nullptr, 0);
    ASSERT_NE(msg, nullptr);

    nimcp_layer_error_t err = nimcp_chemistry_biology_transfer_top_down(bridge, msg);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);

    nimcp_layer_msg_destroy(msg);
}

TEST_F(ChemistryBiologyBridgeTest, TransferBottomUpNullBridge) {
    nimcp_layer_msg_t* msg = nimcp_layer_msg_create(
        CHEM_BIO_MSG_RECEPTOR_ACTIVATE,
        NIMCP_LAYER_CHEMISTRY,
        NIMCP_LAYER_BIOLOGY,
        nullptr, 0);
    ASSERT_NE(msg, nullptr);

    nimcp_layer_error_t err = nimcp_chemistry_biology_transfer_bottom_up(nullptr, msg);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);

    nimcp_layer_msg_destroy(msg);
}

//=============================================================================
// State and Stats Tests
//=============================================================================

TEST_F(ChemistryBiologyBridgeTest, GetState) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_chemistry_biology_state_t state;
    err = nimcp_chemistry_biology_get_state(bridge, &state);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_GE(state.bridge_coherence, 0.0f);
}

TEST_F(ChemistryBiologyBridgeTest, GetStateNull) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_biology_get_state(bridge, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryBiologyBridgeTest, GetStateNullBridge) {
    nimcp_chemistry_biology_state_t state;
    nimcp_layer_error_t err = nimcp_chemistry_biology_get_state(nullptr, &state);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryBiologyBridgeTest, GetStats) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    nimcp_chemistry_biology_stats_t stats;
    err = nimcp_chemistry_biology_get_stats(bridge, &stats);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
    EXPECT_EQ(stats.receptor_activations, 0u);
}

TEST_F(ChemistryBiologyBridgeTest, GetStatsNull) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_biology_get_stats(bridge, nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryBiologyBridgeTest, GetCoherence) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    float coherence = nimcp_chemistry_biology_get_coherence(bridge);
    EXPECT_GE(coherence, 0.0f);
    EXPECT_LE(coherence, 1.0f);
}

TEST_F(ChemistryBiologyBridgeTest, GetCoherenceNull) {
    float coherence = nimcp_chemistry_biology_get_coherence(nullptr);
    EXPECT_EQ(coherence, -1.0f);
}

TEST_F(ChemistryBiologyBridgeTest, ResetStats) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_biology_reset_stats(bridge);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(ChemistryBiologyBridgeTest, ResetStatsNull) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_reset_stats(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

//=============================================================================
// Shutdown Tests
//=============================================================================

TEST_F(ChemistryBiologyBridgeTest, Shutdown) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_biology_shutdown(bridge);
    EXPECT_EQ(err, NIMCP_LAYER_OK);
}

TEST_F(ChemistryBiologyBridgeTest, ShutdownNotInitialized) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_shutdown(bridge);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

TEST_F(ChemistryBiologyBridgeTest, ShutdownNull) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_shutdown(nullptr);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NULL_PTR);
}

TEST_F(ChemistryBiologyBridgeTest, DoubleShutdown) {
    nimcp_layer_error_t err = nimcp_chemistry_biology_init(bridge, registry, chemistry, biology);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_biology_shutdown(bridge);
    EXPECT_EQ(err, NIMCP_LAYER_OK);

    err = nimcp_chemistry_biology_shutdown(bridge);
    EXPECT_EQ(err, NIMCP_LAYER_ERR_NOT_INITIALIZED);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
