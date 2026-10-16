/**
 * @file test_portia_immune_bridges.cpp
 * @brief Unit tests for Portia-Immune Bridge modules
 *
 * WHAT: Tests for Portia immune integration bridges
 * WHY:  Verify lifecycle, NULL handling, and basic API contracts
 * HOW:  Test with real systems where possible, NULL safety tests
 *
 * NOTE: All bridge create functions require 3 parameters:
 *       (config, immune_system, module_system)
 *       Without real module instances, we test NULL handling.
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "portia/immune/nimcp_portia_attention_immune_bridge.h"
#include "portia/immune/nimcp_portia_learning_immune_bridge.h"
#include "portia/immune/nimcp_portia_sensor_fusion_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"

class PortiaImmuneBridgesTestBase : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system = nullptr;

    void SetUp() override {
        brain_immune_config_t config;
        brain_immune_default_config(&config);
        immune_system = brain_immune_create(&config);
        ASSERT_NE(immune_system, nullptr);
        brain_immune_start(immune_system);
    }

    void TearDown() override {
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
            immune_system = nullptr;
        }
    }
};

/* ============================================================================
 * Portia Attention-Immune Bridge Tests
 * API: portia_attention_immune_create(config, immune_system, attention_state)
 * ============================================================================ */

TEST_F(PortiaImmuneBridgesTestBase, AttentionDefaultConfig) {
    portia_attention_immune_config_t config;
    int result = portia_attention_immune_default_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_budget_reduction);
    EXPECT_TRUE(config.enable_inflammation_allocation_impairment);
}

TEST_F(PortiaImmuneBridgesTestBase, AttentionDefaultConfigNull) {
    EXPECT_EQ(portia_attention_immune_default_config(nullptr), -1);
}

TEST_F(PortiaImmuneBridgesTestBase, AttentionCreateWithNullConfig) {
    /* NULL config should use defaults, but NULL immune_system or attention should fail */
    portia_attention_immune_bridge_t* br = portia_attention_immune_create(nullptr, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(PortiaImmuneBridgesTestBase, AttentionCreateWithNullImmune) {
    portia_attention_immune_config_t config;
    portia_attention_immune_default_config(&config);
    portia_attention_immune_bridge_t* br = portia_attention_immune_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(PortiaImmuneBridgesTestBase, AttentionDestroyNull) {
    portia_attention_immune_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Portia Learning-Immune Bridge Tests
 * API: portia_learning_immune_create(config, immune_system, learning_state)
 * ============================================================================ */

TEST_F(PortiaImmuneBridgesTestBase, LearningDefaultConfig) {
    portia_learning_immune_config_t config;
    int result = portia_learning_immune_default_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_learning_impairment);
}

TEST_F(PortiaImmuneBridgesTestBase, LearningDefaultConfigNull) {
    EXPECT_EQ(portia_learning_immune_default_config(nullptr), -1);
}

TEST_F(PortiaImmuneBridgesTestBase, LearningCreateWithNullConfig) {
    portia_learning_immune_bridge_t* br = portia_learning_immune_create(nullptr, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(PortiaImmuneBridgesTestBase, LearningCreateWithNullImmune) {
    portia_learning_immune_config_t config;
    portia_learning_immune_default_config(&config);
    portia_learning_immune_bridge_t* br = portia_learning_immune_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(PortiaImmuneBridgesTestBase, LearningDestroyNull) {
    portia_learning_immune_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Portia Sensor Fusion-Immune Bridge Tests
 * API: portia_sensor_fusion_immune_create(config, immune_system, fusion_ctx)
 * ============================================================================ */

TEST_F(PortiaImmuneBridgesTestBase, SensorFusionDefaultConfig) {
    portia_sensor_fusion_immune_config_t config;
    int result = portia_sensor_fusion_immune_default_config(&config);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(config.enable_cytokine_sensor_impairment);
}

TEST_F(PortiaImmuneBridgesTestBase, SensorFusionDefaultConfigNull) {
    EXPECT_EQ(portia_sensor_fusion_immune_default_config(nullptr), -1);
}

TEST_F(PortiaImmuneBridgesTestBase, SensorFusionCreateWithNullConfig) {
    portia_sensor_fusion_immune_bridge_t* br = portia_sensor_fusion_immune_create(nullptr, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(PortiaImmuneBridgesTestBase, SensorFusionCreateWithNullImmune) {
    portia_sensor_fusion_immune_config_t config;
    portia_sensor_fusion_immune_default_config(&config);
    portia_sensor_fusion_immune_bridge_t* br = portia_sensor_fusion_immune_create(&config, nullptr, nullptr);
    EXPECT_EQ(br, nullptr);
}

TEST_F(PortiaImmuneBridgesTestBase, SensorFusionDestroyNull) {
    portia_sensor_fusion_immune_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Update API NULL Safety Tests
 * ============================================================================ */

TEST_F(PortiaImmuneBridgesTestBase, AttentionUpdateNull) {
    EXPECT_NE(portia_attention_immune_update(nullptr, 0), 0);
}

TEST_F(PortiaImmuneBridgesTestBase, LearningUpdateNull) {
    EXPECT_NE(portia_learning_immune_update(nullptr, 0), 0);
}

TEST_F(PortiaImmuneBridgesTestBase, SensorFusionUpdateNull) {
    EXPECT_NE(portia_sensor_fusion_immune_update(nullptr, 0), 0);
}

/* ============================================================================
 * Query API NULL Safety Tests
 * ============================================================================ */

TEST_F(PortiaImmuneBridgesTestBase, AttentionGetBudgetFactorNull) {
    float factor = portia_attention_immune_get_budget_factor(nullptr);
    EXPECT_FLOAT_EQ(factor, 1.0f);  /* Default factor when NULL */
}

TEST_F(PortiaImmuneBridgesTestBase, AttentionGetEfficiencyLossNull) {
    float loss = portia_attention_immune_get_efficiency_loss(nullptr);
    EXPECT_FLOAT_EQ(loss, 0.0f);  /* No loss when NULL */
}

TEST_F(PortiaImmuneBridgesTestBase, AttentionHasResourceDeficitNull) {
    EXPECT_FALSE(portia_attention_immune_has_resource_deficit(nullptr));
}

/* ============================================================================
 * Bio-Async NULL Safety Tests
 * ============================================================================ */

TEST_F(PortiaImmuneBridgesTestBase, AttentionBioAsyncConnectNull) {
    EXPECT_NE(portia_attention_immune_connect_bio_async(nullptr), 0);
}

TEST_F(PortiaImmuneBridgesTestBase, AttentionBioAsyncDisconnectNull) {
    /* Disconnect on NULL returns error code */
    EXPECT_EQ(portia_attention_immune_disconnect_bio_async(nullptr), -1);
}

TEST_F(PortiaImmuneBridgesTestBase, AttentionBioAsyncIsConnectedNull) {
    EXPECT_FALSE(portia_attention_immune_is_bio_async_connected(nullptr));
}

TEST_F(PortiaImmuneBridgesTestBase, LearningBioAsyncConnectNull) {
    EXPECT_NE(portia_learning_immune_connect_bio_async(nullptr), 0);
}

TEST_F(PortiaImmuneBridgesTestBase, SensorFusionBioAsyncConnectNull) {
    EXPECT_NE(portia_sensor_fusion_immune_connect_bio_async(nullptr), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
