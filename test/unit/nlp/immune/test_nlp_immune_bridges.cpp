/**
 * @file test_nlp_immune_bridges.cpp
 * @brief Unit tests for NLP-Immune Bridge modules
 * @version 1.0.0
 * @date 2025-12-12
 *
 * Tests for:
 * - NLP-Immune Bridge (language processing)
 * - Spike NLP-Immune Bridge (spike-based NLP)
 * - Multimodal NLP-Immune Bridge (multimodal integration)
 */

#include <gtest/gtest.h>
#include <cstring>

// Headers have their own extern "C" guards
#include "nlp/immune/nimcp_nlp_immune_bridge.h"
#include "nlp/immune/nimcp_spike_nlp_immune_bridge.h"
#include "nlp/immune/nimcp_multimodal_nlp_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * NLP-Immune Bridge Test Fixture
 * ============================================================================ */

class NLPImmuneBridgeTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    nlp_context_t mock_nlp_context;
    nlp_immune_bridge_t* bridge = nullptr;

    void SetUp() override {
        /* Create brain immune system */
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        immune_system = brain_immune_create(&config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        /* Initialize mock NLP context */
        memset(&mock_nlp_context, 0, sizeof(mock_nlp_context));

        /* Create bridge */
        nlp_immune_config_t bridge_config;
        nlp_immune_default_config(&bridge_config);
        bridge = nlp_immune_bridge_create(
            &bridge_config,
            immune_system,
            &mock_nlp_context
        );
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            nlp_immune_bridge_destroy(bridge);
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
 * NLP-Immune Bridge Lifecycle Tests
 * ============================================================================ */

TEST_F(NLPImmuneBridgeTest, DefaultConfigIsValid) {
    nlp_immune_config_t config;
    int result = nlp_immune_default_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_language_impairment);
    EXPECT_TRUE(config.enable_comprehension_stress_response);
}

TEST_F(NLPImmuneBridgeTest, DefaultConfigNullFails) {
    int result = nlp_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(NLPImmuneBridgeTest, CreateWithNullImmuneFails) {
    nlp_immune_bridge_t* b = nlp_immune_bridge_create(
        nullptr, nullptr, &mock_nlp_context
    );
    EXPECT_EQ(b, nullptr);
}

TEST_F(NLPImmuneBridgeTest, CreateWithNullContextFails) {
    nlp_immune_bridge_t* b = nlp_immune_bridge_create(
        nullptr, immune_system, nullptr
    );
    EXPECT_EQ(b, nullptr);
}

TEST_F(NLPImmuneBridgeTest, CreateWithDefaultConfig) {
    nlp_immune_bridge_t* b = nlp_immune_bridge_create(
        nullptr, immune_system, &mock_nlp_context
    );
    ASSERT_NE(b, nullptr);
    nlp_immune_bridge_destroy(b);
}

TEST_F(NLPImmuneBridgeTest, DestroyNull) {
    nlp_immune_bridge_destroy(nullptr);
}

/* ============================================================================
 * NLP-Immune Bridge Update Tests
 * ============================================================================ */

TEST_F(NLPImmuneBridgeTest, BridgeUpdate) {
    int result = nlp_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(bridge->total_updates, 0u);
}

TEST_F(NLPImmuneBridgeTest, BridgeUpdateNull) {
    int result = nlp_immune_bridge_update(nullptr, 100);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(NLPImmuneBridgeTest, MultipleUpdates) {
    for (int i = 0; i < 5; i++) {
        int result = nlp_immune_bridge_update(bridge, 20);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
    EXPECT_EQ(bridge->total_updates, 5u);
}

/* ============================================================================
 * NLP-Immune Bridge Query Tests
 * ============================================================================ */

TEST_F(NLPImmuneBridgeTest, GetCytokineEffects) {
    cytokine_nlp_effects_t effects;
    int result = nlp_immune_get_cytokine_effects(bridge, &effects);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(NLPImmuneBridgeTest, GetInflammationState) {
    inflammation_nlp_state_t state;
    int result = nlp_immune_get_inflammation_state(bridge, &state);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(NLPImmuneBridgeTest, GetLanguageCapacity) {
    float capacity = nlp_immune_get_language_capacity(bridge);
    EXPECT_GE(capacity, 0.0f);
    EXPECT_LE(capacity, 1.0f);
}

TEST_F(NLPImmuneBridgeTest, GetLanguageCapacityNull) {
    float capacity = nlp_immune_get_language_capacity(nullptr);
    EXPECT_FLOAT_EQ(capacity, 1.0f); /* Default full capacity */
}

/* ============================================================================
 * NLP-Immune Bridge Bio-Async Tests
 * ============================================================================ */

TEST_F(NLPImmuneBridgeTest, ConnectBioAsync) {
    int result = nlp_immune_connect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(nlp_immune_is_bio_async_connected(bridge));
}

TEST_F(NLPImmuneBridgeTest, DisconnectBioAsync) {
    nlp_immune_connect_bio_async(bridge);
    int result = nlp_immune_disconnect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(nlp_immune_is_bio_async_connected(bridge));
}

TEST_F(NLPImmuneBridgeTest, IsBioAsyncConnectedNull) {
    bool connected = nlp_immune_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

/* ============================================================================
 * Spike NLP-Immune Bridge Test Fixture
 * ============================================================================ */

class SpikeNLPImmuneBridgeTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    spike_nlp_context_t mock_spike_nlp;
    spike_nlp_immune_bridge_t* bridge = nullptr;

    void SetUp() override {
        /* Create brain immune system */
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        immune_system = brain_immune_create(&config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        /* Initialize mock spike NLP context */
        memset(&mock_spike_nlp, 0, sizeof(mock_spike_nlp));

        /* Create bridge */
        spike_nlp_immune_config_t bridge_config;
        spike_nlp_immune_default_config(&bridge_config);
        bridge = spike_nlp_immune_bridge_create(
            &bridge_config,
            immune_system,
            &mock_spike_nlp
        );
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            spike_nlp_immune_bridge_destroy(bridge);
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
 * Spike NLP-Immune Bridge Lifecycle Tests
 * ============================================================================ */

TEST_F(SpikeNLPImmuneBridgeTest, DefaultConfigIsValid) {
    spike_nlp_immune_config_t config;
    int result = spike_nlp_immune_default_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_spike_rate_modulation);
}

TEST_F(SpikeNLPImmuneBridgeTest, DefaultConfigNullFails) {
    int result = spike_nlp_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(SpikeNLPImmuneBridgeTest, CreateWithDefaultConfig) {
    spike_nlp_immune_bridge_t* b = spike_nlp_immune_bridge_create(
        nullptr, immune_system, &mock_spike_nlp
    );
    ASSERT_NE(b, nullptr);
    spike_nlp_immune_bridge_destroy(b);
}

TEST_F(SpikeNLPImmuneBridgeTest, DestroyNull) {
    spike_nlp_immune_bridge_destroy(nullptr);
}

/* ============================================================================
 * Spike NLP-Immune Bridge Update Tests
 * ============================================================================ */

TEST_F(SpikeNLPImmuneBridgeTest, BridgeUpdate) {
    int result = spike_nlp_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(bridge->total_updates, 0u);
}

TEST_F(SpikeNLPImmuneBridgeTest, BridgeUpdateNull) {
    int result = spike_nlp_immune_bridge_update(nullptr, 100);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Spike NLP-Immune Bridge Bio-Async Tests
 * ============================================================================ */

TEST_F(SpikeNLPImmuneBridgeTest, ConnectBioAsync) {
    int result = spike_nlp_immune_connect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(spike_nlp_immune_is_bio_async_connected(bridge));
}

TEST_F(SpikeNLPImmuneBridgeTest, DisconnectBioAsync) {
    spike_nlp_immune_connect_bio_async(bridge);
    int result = spike_nlp_immune_disconnect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(spike_nlp_immune_is_bio_async_connected(bridge));
}

/* ============================================================================
 * Multimodal NLP-Immune Bridge Test Fixture
 * ============================================================================ */

class MultimodalNLPImmuneBridgeTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;
    multimodal_nlp_context_t mock_multimodal;
    multimodal_nlp_immune_bridge_t* bridge = nullptr;

    void SetUp() override {
        /* Create brain immune system */
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        immune_system = brain_immune_create(&config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);

        /* Initialize mock multimodal NLP context */
        memset(&mock_multimodal, 0, sizeof(mock_multimodal));

        /* Create bridge */
        multimodal_nlp_immune_config_t bridge_config;
        multimodal_nlp_immune_default_config(&bridge_config);
        bridge = multimodal_nlp_immune_bridge_create(
            &bridge_config,
            immune_system,
            &mock_multimodal
        );
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            multimodal_nlp_immune_bridge_destroy(bridge);
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
 * Multimodal NLP-Immune Bridge Lifecycle Tests
 * ============================================================================ */

TEST_F(MultimodalNLPImmuneBridgeTest, DefaultConfigIsValid) {
    multimodal_nlp_immune_config_t config;
    int result = multimodal_nlp_immune_default_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_modality_integration_impairment);
}

TEST_F(MultimodalNLPImmuneBridgeTest, DefaultConfigNullFails) {
    int result = multimodal_nlp_immune_default_config(nullptr);
    EXPECT_EQ(result, -1);
}

TEST_F(MultimodalNLPImmuneBridgeTest, CreateWithDefaultConfig) {
    multimodal_nlp_immune_bridge_t* b = multimodal_nlp_immune_bridge_create(
        nullptr, immune_system, &mock_multimodal
    );
    ASSERT_NE(b, nullptr);
    multimodal_nlp_immune_bridge_destroy(b);
}

TEST_F(MultimodalNLPImmuneBridgeTest, DestroyNull) {
    multimodal_nlp_immune_bridge_destroy(nullptr);
}

/* ============================================================================
 * Multimodal NLP-Immune Bridge Update Tests
 * ============================================================================ */

TEST_F(MultimodalNLPImmuneBridgeTest, BridgeUpdate) {
    int result = multimodal_nlp_immune_bridge_update(bridge, 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(bridge->total_updates, 0u);
}

TEST_F(MultimodalNLPImmuneBridgeTest, BridgeUpdateNull) {
    int result = multimodal_nlp_immune_bridge_update(nullptr, 100);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Multimodal NLP-Immune Bridge Bio-Async Tests
 * ============================================================================ */

TEST_F(MultimodalNLPImmuneBridgeTest, ConnectBioAsync) {
    int result = multimodal_nlp_immune_connect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_TRUE(multimodal_nlp_immune_is_bio_async_connected(bridge));
}

TEST_F(MultimodalNLPImmuneBridgeTest, DisconnectBioAsync) {
    multimodal_nlp_immune_connect_bio_async(bridge);
    int result = multimodal_nlp_immune_disconnect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_FALSE(multimodal_nlp_immune_is_bio_async_connected(bridge));
}
