/**
 * @file test_glial_immune_bridges.cpp
 * @brief Unit tests for Glial-Immune Bridge modules
 * @version 1.0.0
 * @date 2025-12-12
 *
 * Tests for:
 * - Astrocyte-Immune Bridge
 * - Microglia-Immune Bridge
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "glial/immune/nimcp_astrocyte_immune_bridge.h"
#include "glial/immune/nimcp_microglia_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Mock Structures (minimal implementations for testing)
 * ============================================================================ */

/* Mock astrocyte network for testing */
static float mock_avg_calcium = 0.5f;
static float mock_max_calcium = 1.0f;
static float mock_avg_glutamate = 0.3f;

// Headers have their own extern "C" guards
/* Mock function for astrocyte network stats */
void astrocyte_network_get_stats(
    astrocyte_network_t* network,
    float* avg_calcium,
    float* max_calcium,
    float* avg_glutamate
) {
    if (!network) return;
    if (avg_calcium) *avg_calcium = mock_avg_calcium;
    if (max_calcium) *max_calcium = mock_max_calcium;
    if (avg_glutamate) *avg_glutamate = mock_avg_glutamate;
}

/* ============================================================================
 * Astrocyte-Immune Bridge Test Fixture
 * ============================================================================ */

class AstrocyteImmuneBridgeTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    astrocyte_network_t mock_astrocyte_network;
    astro_network_bridge_t* bridge = nullptr;

    void SetUp() override {
        /* Create brain immune system */
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        immune_system = brain_immune_create(&config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        /* Initialize mock network */
        memset(&mock_astrocyte_network, 0, sizeof(mock_astrocyte_network));

        /* Create bridge */
        astro_network_config_t bridge_config;
        astro_network_default_config(&bridge_config);
        bridge = astro_network_bridge_create(
            &bridge_config,
            immune_system,
            &mock_astrocyte_network
        );
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            astro_network_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
    }
};

/* ============================================================================
 * Astrocyte-Immune Bridge Lifecycle Tests
 * ============================================================================ */

TEST_F(AstrocyteImmuneBridgeTest, DefaultConfigIsValid) {
    astro_network_config_t config;
    int result = astro_network_default_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_reactivity);
    EXPECT_TRUE(config.enable_inflammation_astrogliosis);
    EXPECT_FLOAT_EQ(config.cytokine_sensitivity, 1.0f);
}

TEST_F(AstrocyteImmuneBridgeTest, DefaultConfigNullFails) {
    int result = astro_network_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(AstrocyteImmuneBridgeTest, CreateWithNullImmuneFails) {
    astro_network_bridge_t* b = astro_network_bridge_create(
        nullptr, nullptr, &mock_astrocyte_network
    );
    EXPECT_EQ(b, nullptr);
}

TEST_F(AstrocyteImmuneBridgeTest, CreateWithNullNetworkFails) {
    astro_network_bridge_t* b = astro_network_bridge_create(
        nullptr, immune_system, nullptr
    );
    EXPECT_EQ(b, nullptr);
}

TEST_F(AstrocyteImmuneBridgeTest, CreateWithDefaultConfig) {
    astro_network_bridge_t* b = astro_network_bridge_create(
        nullptr, immune_system, &mock_astrocyte_network
    );
    ASSERT_NE(b, nullptr);
    astro_network_bridge_destroy(b);
}

TEST_F(AstrocyteImmuneBridgeTest, DestroyNull) {
    /* Should not crash */
    astro_network_bridge_destroy(nullptr);
}

/* ============================================================================
 * Astrocyte-Immune Bridge Cytokine Effects Tests
 * ============================================================================ */

TEST_F(AstrocyteImmuneBridgeTest, ApplyCytokineEffects) {
    int result = astro_network_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(AstrocyteImmuneBridgeTest, ApplyCytokineEffectsNullBridge) {
    int result = astro_network_apply_cytokine_effects(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(AstrocyteImmuneBridgeTest, ApplyInflammationEffects) {
    int result = astro_network_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(AstrocyteImmuneBridgeTest, ComputeReactivity) {
    float reactivity = astro_network_compute_reactivity(bridge);
    EXPECT_GE(reactivity, 0.0f);
    EXPECT_LE(reactivity, 1.0f);
}

TEST_F(AstrocyteImmuneBridgeTest, ComputeReactivityNull) {
    float reactivity = astro_network_compute_reactivity(nullptr);
    EXPECT_FLOAT_EQ(reactivity, 0.0f);
}

TEST_F(AstrocyteImmuneBridgeTest, ComputeGlutamateClearance) {
    float clearance = astro_network_compute_glutamate_clearance(bridge);
    EXPECT_GE(clearance, 0.0f);
    EXPECT_LE(clearance, 1.0f);
}

/* ============================================================================
 * Astrocyte-Immune Bridge Update Tests
 * ============================================================================ */

TEST_F(AstrocyteImmuneBridgeTest, BridgeUpdate) {
    int result = astro_network_bridge_update(bridge, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(bridge->total_updates, 0u);
}

TEST_F(AstrocyteImmuneBridgeTest, BridgeUpdateNull) {
    int result = astro_network_bridge_update(nullptr, 100);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Astrocyte-Immune Bridge Query Tests
 * ============================================================================ */

TEST_F(AstrocyteImmuneBridgeTest, GetCytokineEffects) {
    cytokine_astro_network_effects_t effects;
    int result = astro_network_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(AstrocyteImmuneBridgeTest, GetInflammationState) {
    inflammation_astro_network_state_t state;
    int result = astro_network_get_inflammation_state(bridge, &state);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(AstrocyteImmuneBridgeTest, HasAstrogliosis) {
    bool has_astrogliosis = astro_network_has_astrogliosis(bridge);
    /* Initially false */
    EXPECT_FALSE(has_astrogliosis);
}

TEST_F(AstrocyteImmuneBridgeTest, GetReactivityFactor) {
    float factor = astro_network_get_reactivity_factor(bridge);
    EXPECT_GE(factor, 0.0f);
}

TEST_F(AstrocyteImmuneBridgeTest, GetBBBPermeability) {
    float permeability = astro_network_get_bbb_permeability(bridge);
    EXPECT_GE(permeability, 0.0f);
}

/* ============================================================================
 * Astrocyte-Immune Bridge Bio-Async Tests
 * ============================================================================ */

TEST_F(AstrocyteImmuneBridgeTest, ConnectBioAsync) {
    int result = astro_network_connect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    /* Note: bio_async_enabled only becomes true when bio_router is initialized */
    /* In unit tests without bio_router, connection succeeds but enabled stays false */
}

TEST_F(AstrocyteImmuneBridgeTest, DisconnectBioAsync) {
    astro_network_connect_bio_async(bridge);
    int result = astro_network_disconnect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(astro_network_is_bio_async_connected(bridge));
}

TEST_F(AstrocyteImmuneBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = astro_network_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

/* ============================================================================
 * Microglia-Immune Bridge Test Fixture
 * ============================================================================ */

class MicrogliaImmuneBridgeTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    microglia_network_t mock_microglia_network;
    microglia_immune_bridge_t* bridge = nullptr;

    void SetUp() override {
        /* Create brain immune system */
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        immune_system = brain_immune_create(&config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        /* Initialize mock network */
        memset(&mock_microglia_network, 0, sizeof(mock_microglia_network));

        /* Create bridge */
        microglia_immune_config_t bridge_config;
        microglia_immune_default_config(&bridge_config);
        bridge = microglia_immune_bridge_create(
            &bridge_config,
            immune_system,
            &mock_microglia_network
        );
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            microglia_immune_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
    }
};

/* ============================================================================
 * Microglia-Immune Bridge Lifecycle Tests
 * ============================================================================ */

TEST_F(MicrogliaImmuneBridgeTest, DefaultConfigIsValid) {
    microglia_immune_config_t config;
    int result = microglia_immune_default_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_polarization);
    EXPECT_TRUE(config.enable_inflammation_activation);
    EXPECT_TRUE(config.enable_m1_cytokine_production);
    EXPECT_TRUE(config.enable_m2_cytokine_production);
    EXPECT_FLOAT_EQ(config.cytokine_sensitivity, 1.0f);
}

TEST_F(MicrogliaImmuneBridgeTest, DefaultConfigNullFails) {
    int result = microglia_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MicrogliaImmuneBridgeTest, CreateWithNullImmuneFails) {
    microglia_immune_bridge_t* b = microglia_immune_bridge_create(
        nullptr, nullptr, &mock_microglia_network
    );
    EXPECT_EQ(b, nullptr);
}

TEST_F(MicrogliaImmuneBridgeTest, CreateWithNullNetworkFails) {
    microglia_immune_bridge_t* b = microglia_immune_bridge_create(
        nullptr, immune_system, nullptr
    );
    EXPECT_EQ(b, nullptr);
}

TEST_F(MicrogliaImmuneBridgeTest, CreateWithDefaultConfig) {
    microglia_immune_bridge_t* b = microglia_immune_bridge_create(
        nullptr, immune_system, &mock_microglia_network
    );
    ASSERT_NE(b, nullptr);
    microglia_immune_bridge_destroy(b);
}

TEST_F(MicrogliaImmuneBridgeTest, DestroyNull) {
    /* Should not crash */
    microglia_immune_bridge_destroy(nullptr);
}

/* ============================================================================
 * Microglia-Immune Bridge Cytokine Effects Tests
 * ============================================================================ */

TEST_F(MicrogliaImmuneBridgeTest, ApplyCytokineEffects) {
    int result = microglia_immune_apply_cytokine_effects(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(MicrogliaImmuneBridgeTest, ApplyCytokineEffectsNullBridge) {
    int result = microglia_immune_apply_cytokine_effects(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MicrogliaImmuneBridgeTest, ApplyInflammationEffects) {
    int result = microglia_immune_apply_inflammation_effects(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(MicrogliaImmuneBridgeTest, ComputeActivation) {
    float activation = microglia_immune_compute_activation(bridge);
    EXPECT_GE(activation, 0.0f);
    EXPECT_LE(activation, 1.0f);
}

TEST_F(MicrogliaImmuneBridgeTest, ComputeActivationNull) {
    float activation = microglia_immune_compute_activation(nullptr);
    EXPECT_FLOAT_EQ(activation, 0.0f);
}

TEST_F(MicrogliaImmuneBridgeTest, ComputePolarization) {
    microglia_polarization_t pol = microglia_immune_compute_polarization(bridge);
    EXPECT_GE(pol, MICROGLIA_POLARIZATION_NONE);
    EXPECT_LE(pol, MICROGLIA_POLARIZATION_MIXED);
}

TEST_F(MicrogliaImmuneBridgeTest, ComputePolarizationNull) {
    microglia_polarization_t pol = microglia_immune_compute_polarization(nullptr);
    EXPECT_EQ(pol, MICROGLIA_POLARIZATION_NONE);
}

/* ============================================================================
 * Microglia -> Immune Direction Tests
 * ============================================================================ */

TEST_F(MicrogliaImmuneBridgeTest, ReleaseM1Cytokines) {
    int result = microglia_immune_release_m1_cytokines(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(MicrogliaImmuneBridgeTest, ReleaseM2Cytokines) {
    int result = microglia_immune_release_m2_cytokines(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(MicrogliaImmuneBridgeTest, PresentDAMPAntigens) {
    int result = microglia_immune_present_damp_antigens(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(MicrogliaImmuneBridgeTest, ReportComplementPruning) {
    int result = microglia_immune_report_complement_pruning(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(MicrogliaImmuneBridgeTest, ReportPhagocytosis) {
    int result = microglia_immune_report_phagocytosis(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Microglia-Immune Bridge Update Tests
 * ============================================================================ */

TEST_F(MicrogliaImmuneBridgeTest, BridgeUpdate) {
    int result = microglia_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(bridge->total_updates, 0u);
}

TEST_F(MicrogliaImmuneBridgeTest, BridgeUpdateNull) {
    int result = microglia_immune_bridge_update(nullptr, 100);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MicrogliaImmuneBridgeTest, MultipleUpdates) {
    for (int i = 0; i < 10; i++) {
        int result = microglia_immune_bridge_update(bridge, 10);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
    EXPECT_EQ(bridge->total_updates, 10u);
}

/* ============================================================================
 * Microglia-Immune Bridge Query Tests
 * ============================================================================ */

TEST_F(MicrogliaImmuneBridgeTest, GetCytokineEffects) {
    cytokine_microglia_effects_t effects;
    int result = microglia_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(MicrogliaImmuneBridgeTest, GetCytokineEffectsNull) {
    int result = microglia_immune_get_cytokine_effects(nullptr, nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MicrogliaImmuneBridgeTest, GetInflammationState) {
    inflammation_microglia_state_t state;
    int result = microglia_immune_get_inflammation_state(bridge, &state);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(MicrogliaImmuneBridgeTest, HasStormToxicity) {
    bool has_toxicity = microglia_immune_has_storm_toxicity(bridge);
    /* Initially false */
    EXPECT_FALSE(has_toxicity);
}

TEST_F(MicrogliaImmuneBridgeTest, HasStormToxicityNull) {
    bool has_toxicity = microglia_immune_has_storm_toxicity(nullptr);
    EXPECT_FALSE(has_toxicity);
}

TEST_F(MicrogliaImmuneBridgeTest, GetActivationFactor) {
    float factor = microglia_immune_get_activation_factor(bridge);
    EXPECT_GE(factor, 0.0f);
}

TEST_F(MicrogliaImmuneBridgeTest, GetActivationFactorNull) {
    float factor = microglia_immune_get_activation_factor(nullptr);
    EXPECT_FLOAT_EQ(factor, 0.0f);
}

TEST_F(MicrogliaImmuneBridgeTest, GetM1Fraction) {
    float fraction = microglia_immune_get_m1_fraction(bridge);
    EXPECT_GE(fraction, 0.0f);
    EXPECT_LE(fraction, 1.0f);
}

TEST_F(MicrogliaImmuneBridgeTest, GetM2Fraction) {
    float fraction = microglia_immune_get_m2_fraction(bridge);
    EXPECT_GE(fraction, 0.0f);
    EXPECT_LE(fraction, 1.0f);
}

/* ============================================================================
 * Microglia-Immune Bridge Bio-Async Tests
 * ============================================================================ */

TEST_F(MicrogliaImmuneBridgeTest, ConnectBioAsync) {
    int result = microglia_immune_connect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    /* Note: bio_async_enabled only becomes true when bio_router is initialized */
}

TEST_F(MicrogliaImmuneBridgeTest, ConnectBioAsyncNull) {
    int result = microglia_immune_connect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MicrogliaImmuneBridgeTest, DisconnectBioAsync) {
    microglia_immune_connect_bio_async(bridge);
    int result = microglia_immune_disconnect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(microglia_immune_is_bio_async_connected(bridge));
}

TEST_F(MicrogliaImmuneBridgeTest, DisconnectBioAsyncNull) {
    int result = microglia_immune_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MicrogliaImmuneBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = microglia_immune_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(MicrogliaImmuneBridgeTest, ReconnectBioAsync) {
    /* Connect - returns success even if router not available */
    int result = microglia_immune_connect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    /* Disconnect */
    result = microglia_immune_disconnect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(microglia_immune_is_bio_async_connected(bridge));

    /* Reconnect */
    result = microglia_immune_connect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(MicrogliaImmuneBridgeTest, FullUpdateCycle) {
    /* Connect bio-async */
    microglia_immune_connect_bio_async(bridge);

    /* Run multiple update cycles */
    for (int i = 0; i < 5; i++) {
        int result = microglia_immune_bridge_update(bridge, 100);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }

    /* Query final state */
    cytokine_microglia_effects_t effects;
    microglia_immune_get_cytokine_effects(bridge, &effects);

    inflammation_microglia_state_t state;
    microglia_immune_get_inflammation_state(bridge, &state);

    /* State should be valid */
    EXPECT_GE(effects.total_activation, 0.0f);
    EXPECT_LE(effects.total_activation, 1.0f);
}
