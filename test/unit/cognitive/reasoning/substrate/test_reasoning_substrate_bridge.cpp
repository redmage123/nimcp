/**
 * @file test_reasoning_substrate_bridge.cpp
 * @brief Unit tests for Reasoning-Neural Substrate Bridge
 * @date 2025-12-19
 *
 * Tests bidirectional substrate-reasoning integration including inference depth,
 * logical accuracy, processing speed, and abstraction capacity.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/reasoning/nimcp_reasoning_substrate_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class ReasoningSubstrateBridgeTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    symbolic_logic_t* reasoning = nullptr;
    reasoning_substrate_bridge_t* bridge = nullptr;
    reasoning_substrate_config_t config;

    void SetUp() override {
        substrate_config_t sub_cfg;
        substrate_default_config(&sub_cfg);
        substrate = substrate_create(&sub_cfg);
        ASSERT_NE(substrate, nullptr);

        logic_config_t reason_cfg = {
            .max_predicates = 100,
            .max_rules = 50,
            .max_kb_size = 200,
            .max_inference_depth = 10,
            .enable_forward_chaining = true,
            .enable_backward_chaining = true,
            .enable_resolution = true,
            .enable_memory_consolidation = true
        };
        reasoning = symbolic_logic_create(&reason_cfg);
        ASSERT_NE(reasoning, nullptr);

        reasoning_substrate_default_config(&config);
    }

    void TearDown() override {
        if (bridge) {
            reasoning_substrate_bridge_destroy(bridge);
            bridge = nullptr;
        }
        if (reasoning) {
            symbolic_logic_destroy(reasoning);
            reasoning = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }

    void createBridge() {
        bridge = reasoning_substrate_bridge_create(&config, reasoning, substrate);
        ASSERT_NE(bridge, nullptr);
    }

    void setSubstrateATP(float level) {
        if (substrate) {
            substrate_set_atp(substrate, level);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(ReasoningSubstrateBridgeTest, CreateWithValidInputs) {
    bridge = reasoning_substrate_bridge_create(&config, reasoning, substrate);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(ReasoningSubstrateBridgeTest, CreateWithNullConfig) {
    bridge = reasoning_substrate_bridge_create(nullptr, reasoning, substrate);
    EXPECT_NE(bridge, nullptr);
}

TEST_F(ReasoningSubstrateBridgeTest, CreateWithNullSubstrate) {
    bridge = reasoning_substrate_bridge_create(&config, reasoning, nullptr);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(ReasoningSubstrateBridgeTest, CreateWithNullReasoning) {
    bridge = reasoning_substrate_bridge_create(&config, nullptr, substrate);
    EXPECT_EQ(bridge, nullptr);
}

TEST_F(ReasoningSubstrateBridgeTest, DestroyNull) {
    reasoning_substrate_bridge_destroy(nullptr);
    SUCCEED();
}

TEST_F(ReasoningSubstrateBridgeTest, DestroyValid) {
    createBridge();
    reasoning_substrate_bridge_destroy(bridge);
    bridge = nullptr;
    SUCCEED();
}

/* ============================================================================
 * Default Configuration Tests
 * ============================================================================ */

TEST_F(ReasoningSubstrateBridgeTest, DefaultConfigEnablesModulations) {
    reasoning_substrate_config_t cfg;
    reasoning_substrate_default_config(&cfg);

    EXPECT_TRUE(cfg.enable_atp_modulation);
    EXPECT_TRUE(cfg.enable_fatigue_modulation);
    EXPECT_TRUE(cfg.enable_stress_modulation);
}

TEST_F(ReasoningSubstrateBridgeTest, DefaultConfigHasReasonableSensitivity) {
    reasoning_substrate_config_t cfg;
    reasoning_substrate_default_config(&cfg);

    EXPECT_GT(cfg.atp_sensitivity, 0.0f);
    EXPECT_LE(cfg.atp_sensitivity, 2.0f);
}

TEST_F(ReasoningSubstrateBridgeTest, DefaultConfigNullSafe) {
    reasoning_substrate_default_config(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(ReasoningSubstrateBridgeTest, BioAsyncInitiallyDisconnected) {
    createBridge();
    EXPECT_FALSE(reasoning_substrate_is_bio_async_connected(bridge));
}

TEST_F(ReasoningSubstrateBridgeTest, ConnectBioAsyncNull) {
    int result = reasoning_substrate_connect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(ReasoningSubstrateBridgeTest, DisconnectBioAsyncNull) {
    int result = reasoning_substrate_disconnect_bio_async(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Initial Effects Tests
 * ============================================================================ */

TEST_F(ReasoningSubstrateBridgeTest, InitialEffectsAreOptimal) {
    createBridge();

    EXPECT_FLOAT_EQ(reasoning_substrate_get_inference_depth(bridge), 1.0f);
    EXPECT_FLOAT_EQ(reasoning_substrate_get_logical_accuracy(bridge), 1.0f);
    EXPECT_FLOAT_EQ(reasoning_substrate_get_processing_speed(bridge), 1.0f);
    EXPECT_FLOAT_EQ(reasoning_substrate_get_abstraction_capacity(bridge), 1.0f);
}

TEST_F(ReasoningSubstrateBridgeTest, InitiallyNotImpaired) {
    createBridge();
    EXPECT_FALSE(reasoning_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Update Tests - Inference Depth
 * ============================================================================ */

TEST_F(ReasoningSubstrateBridgeTest, UpdateWithFullATPMaintainsDepth) {
    createBridge();
    setSubstrateATP(1.0f);

    int result = reasoning_substrate_update(bridge);
    EXPECT_EQ(result, 0);

    float depth = reasoning_substrate_get_inference_depth(bridge);
    EXPECT_GE(depth, 0.9f);
}

TEST_F(ReasoningSubstrateBridgeTest, UpdateWithLowATPReducesDepth) {
    createBridge();
    setSubstrateATP(0.4f);

    reasoning_substrate_update(bridge);

    float depth = reasoning_substrate_get_inference_depth(bridge);
    EXPECT_LT(depth, 0.8f);
}

/* ============================================================================
 * Update Tests - Logical Accuracy
 * ============================================================================ */

TEST_F(ReasoningSubstrateBridgeTest, UpdateWithFullATPMaintainsAccuracy) {
    createBridge();
    setSubstrateATP(1.0f);

    reasoning_substrate_update(bridge);

    float accuracy = reasoning_substrate_get_logical_accuracy(bridge);
    EXPECT_GE(accuracy, 0.9f);
}

TEST_F(ReasoningSubstrateBridgeTest, UpdateWithLowATPReducesAccuracy) {
    createBridge();
    setSubstrateATP(0.25f);

    reasoning_substrate_update(bridge);

    float accuracy = reasoning_substrate_get_logical_accuracy(bridge);
    /* Accuracy has floor clamping, with low ATP still maintains partial capacity.
     * Expect reduced but not severely impaired (~0.7-0.8 range) */
    EXPECT_LT(accuracy, 0.85f);
}

/* ============================================================================
 * Update Tests - Processing Speed
 * ============================================================================ */

TEST_F(ReasoningSubstrateBridgeTest, UpdateWithFullATPMaintainsSpeed) {
    createBridge();
    setSubstrateATP(1.0f);

    reasoning_substrate_update(bridge);

    float speed = reasoning_substrate_get_processing_speed(bridge);
    EXPECT_GE(speed, 0.9f);
}

TEST_F(ReasoningSubstrateBridgeTest, UpdateWithLowATPReducesSpeed) {
    createBridge();
    setSubstrateATP(0.3f);

    reasoning_substrate_update(bridge);

    float speed = reasoning_substrate_get_processing_speed(bridge);
    EXPECT_LT(speed, 0.7f);
}

/* ============================================================================
 * Update Tests - Abstraction Capacity
 * ============================================================================ */

TEST_F(ReasoningSubstrateBridgeTest, UpdateWithFullATPMaintainsAbstraction) {
    createBridge();
    setSubstrateATP(1.0f);

    reasoning_substrate_update(bridge);

    float abstraction = reasoning_substrate_get_abstraction_capacity(bridge);
    /* Abstraction depends on metabolic capacity (ATP + O2 + glucose).
     * With normal defaults, expect good but not maximum capacity */
    EXPECT_GE(abstraction, 0.85f);
}

TEST_F(ReasoningSubstrateBridgeTest, UpdateWithLowATPReducesAbstraction) {
    createBridge();
    setSubstrateATP(0.35f);

    reasoning_substrate_update(bridge);

    float abstraction = reasoning_substrate_get_abstraction_capacity(bridge);
    EXPECT_LT(abstraction, 0.7f);
}

/* ============================================================================
 * Impairment Detection Tests
 * ============================================================================ */

TEST_F(ReasoningSubstrateBridgeTest, ImpairedWithCriticalATP) {
    createBridge();
    setSubstrateATP(0.15f);

    reasoning_substrate_update(bridge);

    EXPECT_TRUE(reasoning_substrate_is_impaired(bridge));
}

TEST_F(ReasoningSubstrateBridgeTest, NotImpairedWithOptimalState) {
    createBridge();
    setSubstrateATP(1.0f);

    reasoning_substrate_update(bridge);

    EXPECT_FALSE(reasoning_substrate_is_impaired(bridge));
}

/* ============================================================================
 * Get Effects Tests
 * ============================================================================ */

TEST_F(ReasoningSubstrateBridgeTest, GetEffectsSuccess) {
    createBridge();

    const reasoning_substrate_effects_t* effects = reasoning_substrate_get_effects(bridge);

    ASSERT_NE(effects, nullptr);
    EXPECT_FLOAT_EQ(effects->inference_depth, 1.0f);
    EXPECT_FLOAT_EQ(effects->logical_accuracy, 1.0f);
}

TEST_F(ReasoningSubstrateBridgeTest, GetEffectsNullBridge) {
    const reasoning_substrate_effects_t* effects = reasoning_substrate_get_effects(nullptr);
    EXPECT_EQ(effects, nullptr);
}

/* ============================================================================
 * Query API Null Safety Tests
 * ============================================================================ */

TEST_F(ReasoningSubstrateBridgeTest, GetInferenceDepthNullReturnsNegative) {
    EXPECT_FLOAT_EQ(reasoning_substrate_get_inference_depth(nullptr), -1.0f);
}

TEST_F(ReasoningSubstrateBridgeTest, GetLogicalAccuracyNullReturnsNegative) {
    EXPECT_FLOAT_EQ(reasoning_substrate_get_logical_accuracy(nullptr), -1.0f);
}

TEST_F(ReasoningSubstrateBridgeTest, GetProcessingSpeedNullReturnsNegative) {
    EXPECT_FLOAT_EQ(reasoning_substrate_get_processing_speed(nullptr), -1.0f);
}

TEST_F(ReasoningSubstrateBridgeTest, GetAbstractionCapacityNullReturnsNegative) {
    EXPECT_FLOAT_EQ(reasoning_substrate_get_abstraction_capacity(nullptr), -1.0f);
}

TEST_F(ReasoningSubstrateBridgeTest, IsImpairedNullReturnsFalse) {
    EXPECT_FALSE(reasoning_substrate_is_impaired(nullptr));
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(ReasoningSubstrateBridgeTest, InitialStatsAreZero) {
    createBridge();

    const reasoning_substrate_stats_t* stats = reasoning_substrate_get_stats(bridge);

    ASSERT_NE(stats, nullptr);
    EXPECT_EQ(stats->update_count, 0u);
}

TEST_F(ReasoningSubstrateBridgeTest, UpdateIncrementsCount) {
    createBridge();

    reasoning_substrate_update(bridge);
    reasoning_substrate_update(bridge);

    const reasoning_substrate_stats_t* stats = reasoning_substrate_get_stats(bridge);

    ASSERT_NE(stats, nullptr);
    EXPECT_EQ(stats->update_count, 2u);
}

TEST_F(ReasoningSubstrateBridgeTest, GetStatsNullBridge) {
    const reasoning_substrate_stats_t* stats = reasoning_substrate_get_stats(nullptr);
    EXPECT_EQ(stats, nullptr);
}

/* ============================================================================
 * Configuration Modulation Tests
 * ============================================================================ */

TEST_F(ReasoningSubstrateBridgeTest, DisabledDepthModulationStaysOptimal) {
    config.enable_atp_modulation = false;
    createBridge();
    setSubstrateATP(0.1f);

    reasoning_substrate_update(bridge);

    EXPECT_FLOAT_EQ(reasoning_substrate_get_inference_depth(bridge), 1.0f);
}

TEST_F(ReasoningSubstrateBridgeTest, DisabledAccuracyModulationStaysOptimal) {
    config.enable_stress_modulation = false;
    createBridge();
    setSubstrateATP(0.1f);

    reasoning_substrate_update(bridge);

    EXPECT_FLOAT_EQ(reasoning_substrate_get_logical_accuracy(bridge), 1.0f);
}

/* ============================================================================
 * Thread Safety Tests
 * ============================================================================ */

TEST_F(ReasoningSubstrateBridgeTest, UpdateNullReturnsError) {
    int result = reasoning_substrate_update(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

/* ============================================================================
 * Biological Validity Tests
 * ============================================================================ */

TEST_F(ReasoningSubstrateBridgeTest, AllEffectsNeverBelowFloor) {
    createBridge();
    setSubstrateATP(0.0f);

    reasoning_substrate_update(bridge);

    EXPECT_GE(reasoning_substrate_get_inference_depth(bridge), 0.0f);
    EXPECT_GE(reasoning_substrate_get_logical_accuracy(bridge), 0.0f);
    EXPECT_GE(reasoning_substrate_get_processing_speed(bridge), 0.0f);
    EXPECT_GE(reasoning_substrate_get_abstraction_capacity(bridge), 0.0f);
}

TEST_F(ReasoningSubstrateBridgeTest, AllEffectsBoundedAbove) {
    createBridge();
    setSubstrateATP(2.0f);

    reasoning_substrate_update(bridge);

    EXPECT_LE(reasoning_substrate_get_inference_depth(bridge), 1.0f);
    EXPECT_LE(reasoning_substrate_get_logical_accuracy(bridge), 1.0f);
    EXPECT_LE(reasoning_substrate_get_processing_speed(bridge), 1.0f);
    EXPECT_LE(reasoning_substrate_get_abstraction_capacity(bridge), 1.0f);
}
