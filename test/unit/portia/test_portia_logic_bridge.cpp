/**
 * @file test_portia_logic_bridge.cpp
 * @brief Comprehensive unit tests for Portia-Logic Bridge
 *
 * TEST COVERAGE:
 * - Bridge creation and destruction (8 tests)
 * - Configuration management (5 tests)
 * - Decision evaluation - tier upgrade (8 tests)
 * - Decision evaluation - tier downgrade (8 tests)
 * - Decision evaluation - feature degradation (6 tests)
 * - Decision evaluation - resource allocation (7 tests)
 * - Custom gate management (6 tests)
 * - Resource condition management (8 tests)
 * - Integration (brain, immune, UMM) (6 tests)
 * - Bio-async communication (5 tests)
 * - Statistics tracking (6 tests)
 * - Error handling (5 tests)
 * - Thread safety (3 tests)
 *
 * TOTAL: 81 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_logic_bridge.h"
#include "portia/nimcp_portia.h"
#include "utils/logging/nimcp_logging.h"

class PortiaLogicBridgeTest : public ::testing::Test {
protected:
    portia_logic_bridge_t* bridge;
    portia_logic_config_t config;
    portia_context_t* portia;

    void SetUp() override {
        /* Initialize Portia */
        portia_config_t portia_cfg = portia_get_default_config();
        portia_cfg.enable_bio_async = false;  /* Disable for unit tests */
        portia_init(&portia_cfg);
        portia = portia_get_context();
        ASSERT_NE(portia, nullptr);

        /* Get default config */
        portia_logic_bridge_get_default_config(&config);
        config.enable_bio_async = false;  /* Disable for unit tests */
        config.disable_auto_update = true;  /* Allow manual condition control in tests */

        /* Create bridge */
        bridge = portia_logic_bridge_create(&config, portia);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            portia_logic_bridge_destroy(bridge);
        }
        portia_destroy();
    }
};

/*=============================================================================
 * LIFECYCLE TESTS (8 tests)
 *============================================================================*/

TEST_F(PortiaLogicBridgeTest, CreateValidBridge) {
    EXPECT_NE(bridge, nullptr);
}

TEST_F(PortiaLogicBridgeTest, CreateWithNullConfig) {
    portia_logic_bridge_t* test_bridge = portia_logic_bridge_create(nullptr, portia);
    EXPECT_NE(test_bridge, nullptr);
    portia_logic_bridge_destroy(test_bridge);
}

TEST_F(PortiaLogicBridgeTest, CreateWithNullPortia) {
    portia_logic_bridge_t* test_bridge = portia_logic_bridge_create(&config, nullptr);
    EXPECT_EQ(test_bridge, nullptr);
}

TEST_F(PortiaLogicBridgeTest, DestroyNullBridge) {
    portia_logic_bridge_destroy(nullptr);
    SUCCEED();  /* Should not crash */
}

TEST_F(PortiaLogicBridgeTest, StartBridge) {
    int result = portia_logic_bridge_start(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, StartNullBridge) {
    int result = portia_logic_bridge_start(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, StopBridge) {
    portia_logic_bridge_start(bridge);
    int result = portia_logic_bridge_stop(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, StopNullBridge) {
    int result = portia_logic_bridge_stop(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/*=============================================================================
 * CONFIGURATION TESTS (5 tests)
 *============================================================================*/

TEST_F(PortiaLogicBridgeTest, DefaultConfig) {
    portia_logic_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    portia_logic_bridge_get_default_config(&cfg);

    EXPECT_GT(cfg.max_gates, 0u);
    EXPECT_GT(cfg.max_custom_rules, 0u);
    EXPECT_GT(cfg.decision_threshold, 0.0f);
    EXPECT_LE(cfg.decision_threshold, 1.0f);
    EXPECT_GT(cfg.evaluation_timeout_ms, 0u);
}

TEST_F(PortiaLogicBridgeTest, DefaultConfigNullPointer) {
    portia_logic_bridge_get_default_config(nullptr);
    SUCCEED();  /* Should not crash */
}

TEST_F(PortiaLogicBridgeTest, CustomMaxGates) {
    portia_logic_config_t custom_config;
    portia_logic_bridge_get_default_config(&custom_config);
    custom_config.max_gates = 200;

    portia_logic_bridge_t* custom_bridge = portia_logic_bridge_create(&custom_config, portia);
    EXPECT_NE(custom_bridge, nullptr);
    portia_logic_bridge_destroy(custom_bridge);
}

TEST_F(PortiaLogicBridgeTest, CustomDecisionThreshold) {
    portia_logic_config_t custom_config;
    portia_logic_bridge_get_default_config(&custom_config);
    custom_config.decision_threshold = 0.9f;

    portia_logic_bridge_t* custom_bridge = portia_logic_bridge_create(&custom_config, portia);
    EXPECT_NE(custom_bridge, nullptr);
    portia_logic_bridge_destroy(custom_bridge);
}

TEST_F(PortiaLogicBridgeTest, EnableAllIntegrations) {
    portia_logic_config_t custom_config;
    portia_logic_bridge_get_default_config(&custom_config);
    custom_config.enable_brain_integration = true;
    custom_config.enable_immune_integration = true;
    custom_config.enable_umm_integration = true;

    portia_logic_bridge_t* custom_bridge = portia_logic_bridge_create(&custom_config, portia);
    EXPECT_NE(custom_bridge, nullptr);
    portia_logic_bridge_destroy(custom_bridge);
}

/*=============================================================================
 * TIER UPGRADE DECISION TESTS (8 tests)
 *============================================================================*/

TEST_F(PortiaLogicBridgeTest, TierUpgradeAllConditionsOk) {
    /* Set all conditions to OK */
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);

    bool can_upgrade = portia_logic_can_upgrade_tier(bridge, 0, 1);
    EXPECT_TRUE(can_upgrade);
}

TEST_F(PortiaLogicBridgeTest, TierUpgradeMemoryNotOk) {
    portia_logic_set_condition(bridge, "memory_ok", false);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);

    bool can_upgrade = portia_logic_can_upgrade_tier(bridge, 0, 1);
    EXPECT_FALSE(can_upgrade);
}

TEST_F(PortiaLogicBridgeTest, TierUpgradeThermalNotOk) {
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", false);
    portia_logic_set_condition(bridge, "battery_ok", true);

    bool can_upgrade = portia_logic_can_upgrade_tier(bridge, 0, 1);
    EXPECT_FALSE(can_upgrade);
}

TEST_F(PortiaLogicBridgeTest, TierUpgradeBatteryNotOk) {
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", false);

    bool can_upgrade = portia_logic_can_upgrade_tier(bridge, 0, 1);
    EXPECT_FALSE(can_upgrade);
}

TEST_F(PortiaLogicBridgeTest, TierUpgradeAllConditionsNotOk) {
    portia_logic_set_condition(bridge, "memory_ok", false);
    portia_logic_set_condition(bridge, "thermal_ok", false);
    portia_logic_set_condition(bridge, "battery_ok", false);

    bool can_upgrade = portia_logic_can_upgrade_tier(bridge, 0, 1);
    EXPECT_FALSE(can_upgrade);
}

TEST_F(PortiaLogicBridgeTest, TierUpgradeNullBridge) {
    bool can_upgrade = portia_logic_can_upgrade_tier(nullptr, 0, 1);
    EXPECT_FALSE(can_upgrade);
}

TEST_F(PortiaLogicBridgeTest, TierUpgradeInvalidTiers) {
    /* Target tier <= current tier */
    bool can_upgrade = portia_logic_can_upgrade_tier(bridge, 2, 1);
    EXPECT_FALSE(can_upgrade);
}

TEST_F(PortiaLogicBridgeTest, TierUpgradeMultipleLevels) {
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);

    /* Upgrade from tier 0 to tier 3 */
    bool can_upgrade = portia_logic_can_upgrade_tier(bridge, 0, 3);
    EXPECT_TRUE(can_upgrade);
}

/*=============================================================================
 * TIER DOWNGRADE DECISION TESTS (8 tests)
 *============================================================================*/

TEST_F(PortiaLogicBridgeTest, TierDowngradeAllConditionsOk) {
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);

    bool must_downgrade = portia_logic_must_downgrade_tier(bridge, 2);
    EXPECT_FALSE(must_downgrade);
}

TEST_F(PortiaLogicBridgeTest, TierDowngradeMemoryCritical) {
    portia_logic_set_condition(bridge, "memory_ok", false);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);

    bool must_downgrade = portia_logic_must_downgrade_tier(bridge, 2);
    EXPECT_TRUE(must_downgrade);
}

TEST_F(PortiaLogicBridgeTest, TierDowngradeThermalCritical) {
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", false);
    portia_logic_set_condition(bridge, "battery_ok", true);

    bool must_downgrade = portia_logic_must_downgrade_tier(bridge, 2);
    EXPECT_TRUE(must_downgrade);
}

TEST_F(PortiaLogicBridgeTest, TierDowngradeBatteryCritical) {
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", false);

    bool must_downgrade = portia_logic_must_downgrade_tier(bridge, 2);
    EXPECT_TRUE(must_downgrade);
}

TEST_F(PortiaLogicBridgeTest, TierDowngradeAllCritical) {
    portia_logic_set_condition(bridge, "memory_ok", false);
    portia_logic_set_condition(bridge, "thermal_ok", false);
    portia_logic_set_condition(bridge, "battery_ok", false);

    bool must_downgrade = portia_logic_must_downgrade_tier(bridge, 2);
    EXPECT_TRUE(must_downgrade);
}

TEST_F(PortiaLogicBridgeTest, TierDowngradeNullBridge) {
    bool must_downgrade = portia_logic_must_downgrade_tier(nullptr, 2);
    EXPECT_FALSE(must_downgrade);
}

TEST_F(PortiaLogicBridgeTest, TierDowngradeFromLowestTier) {
    portia_logic_set_condition(bridge, "memory_ok", false);

    bool must_downgrade = portia_logic_must_downgrade_tier(bridge, 0);
    EXPECT_TRUE(must_downgrade);  /* Still returns true even at lowest tier */
}

TEST_F(PortiaLogicBridgeTest, TierDowngradePartialCritical) {
    /* Memory critical, but thermal and battery OK */
    portia_logic_set_condition(bridge, "memory_ok", false);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);

    bool must_downgrade = portia_logic_must_downgrade_tier(bridge, 2);
    EXPECT_TRUE(must_downgrade);  /* OR gate: any critical triggers downgrade */
}

/*=============================================================================
 * FEATURE DEGRADATION TESTS (6 tests)
 *============================================================================*/

TEST_F(PortiaLogicBridgeTest, CanDisableFeatureResourcesOk) {
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);
    portia_logic_set_condition(bridge, "cpu_ok", true);

    bool can_disable = portia_logic_can_disable_feature(bridge, 100);
    EXPECT_FALSE(can_disable);  /* Resources OK, no need to disable */
}

TEST_F(PortiaLogicBridgeTest, CanDisableFeatureResourcesCritical) {
    portia_logic_set_condition(bridge, "memory_ok", false);
    portia_logic_set_condition(bridge, "thermal_ok", false);
    portia_logic_set_condition(bridge, "battery_ok", false);
    portia_logic_set_condition(bridge, "cpu_ok", false);

    bool can_disable = portia_logic_can_disable_feature(bridge, 100);
    EXPECT_TRUE(can_disable);  /* Resources critical, can disable */
}

TEST_F(PortiaLogicBridgeTest, CanDisableFeatureNullBridge) {
    bool can_disable = portia_logic_can_disable_feature(nullptr, 100);
    EXPECT_FALSE(can_disable);
}

TEST_F(PortiaLogicBridgeTest, CanDisableFeatureDifferentIds) {
    portia_logic_set_condition(bridge, "memory_ok", false);

    /* Test multiple feature IDs */
    bool can_disable_1 = portia_logic_can_disable_feature(bridge, 1);
    bool can_disable_2 = portia_logic_can_disable_feature(bridge, 2);
    bool can_disable_3 = portia_logic_can_disable_feature(bridge, 3);

    EXPECT_TRUE(can_disable_1);
    EXPECT_TRUE(can_disable_2);
    EXPECT_TRUE(can_disable_3);
}

TEST_F(PortiaLogicBridgeTest, CanDisableFeaturePartialCritical) {
    /* Only memory critical - but 3/4 conditions OK means score = 0.75 (not critical) */
    portia_logic_set_condition(bridge, "memory_ok", false);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);
    portia_logic_set_condition(bridge, "cpu_ok", true);

    bool can_disable = portia_logic_can_disable_feature(bridge, 100);
    EXPECT_FALSE(can_disable);  /* Resource score = 0.75 >= 0.5, not critical */
}

TEST_F(PortiaLogicBridgeTest, CanDisableFeatureBorderlineResources) {
    /* Exactly 50% resources OK */
    portia_logic_set_condition(bridge, "memory_ok", false);
    portia_logic_set_condition(bridge, "thermal_ok", false);
    portia_logic_set_condition(bridge, "battery_ok", true);
    portia_logic_set_condition(bridge, "cpu_ok", true);

    bool can_disable = portia_logic_can_disable_feature(bridge, 100);
    EXPECT_FALSE(can_disable);  /* Resource score = 0.5, not < 0.5 */
}

/*=============================================================================
 * RESOURCE ALLOCATION TESTS (7 tests)
 *============================================================================*/

TEST_F(PortiaLogicBridgeTest, CanAllocateResourceSufficientBudget) {
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);
    portia_logic_set_condition(bridge, "cpu_ok", true);

    /* Request small allocation (25%) */
    bool can_allocate = portia_logic_can_allocate_resource(bridge, 1, 0.25f);
    EXPECT_TRUE(can_allocate);  /* Resource score = 1.0, can allocate 0.25 */
}

TEST_F(PortiaLogicBridgeTest, CanAllocateResourceInsufficientBudget) {
    portia_logic_set_condition(bridge, "memory_ok", false);
    portia_logic_set_condition(bridge, "thermal_ok", false);
    portia_logic_set_condition(bridge, "battery_ok", false);
    portia_logic_set_condition(bridge, "cpu_ok", false);

    /* Request large allocation (50%) */
    bool can_allocate = portia_logic_can_allocate_resource(bridge, 1, 0.5f);
    EXPECT_FALSE(can_allocate);  /* Resource score = 0, cannot allocate */
}

TEST_F(PortiaLogicBridgeTest, CanAllocateResourceNullBridge) {
    bool can_allocate = portia_logic_can_allocate_resource(nullptr, 1, 0.5f);
    EXPECT_FALSE(can_allocate);
}

TEST_F(PortiaLogicBridgeTest, CanAllocateResourceInvalidAmount) {
    /* Negative amount */
    bool can_allocate_neg = portia_logic_can_allocate_resource(bridge, 1, -0.5f);
    EXPECT_FALSE(can_allocate_neg);

    /* Amount > 1.0 */
    bool can_allocate_high = portia_logic_can_allocate_resource(bridge, 1, 1.5f);
    EXPECT_FALSE(can_allocate_high);
}

TEST_F(PortiaLogicBridgeTest, CanAllocateResourceZeroAmount) {
    portia_logic_set_condition(bridge, "memory_ok", true);

    bool can_allocate = portia_logic_can_allocate_resource(bridge, 1, 0.0f);
    EXPECT_TRUE(can_allocate);  /* Zero allocation always allowed */
}

TEST_F(PortiaLogicBridgeTest, CanAllocateResourceFullBudget) {
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);
    portia_logic_set_condition(bridge, "cpu_ok", true);

    /* Request 100% allocation */
    bool can_allocate = portia_logic_can_allocate_resource(bridge, 1, 1.0f);
    EXPECT_TRUE(can_allocate);  /* Resource score = 1.0, can allocate all */
}

TEST_F(PortiaLogicBridgeTest, CanAllocateResourceBorderline) {
    /* Resource score = 0.5 */
    portia_logic_set_condition(bridge, "memory_ok", false);
    portia_logic_set_condition(bridge, "thermal_ok", false);
    portia_logic_set_condition(bridge, "battery_ok", true);
    portia_logic_set_condition(bridge, "cpu_ok", true);

    /* Request exactly 0.5 */
    bool can_allocate_exact = portia_logic_can_allocate_resource(bridge, 1, 0.5f);
    EXPECT_TRUE(can_allocate_exact);

    /* Request slightly more */
    bool can_allocate_over = portia_logic_can_allocate_resource(bridge, 1, 0.51f);
    EXPECT_FALSE(can_allocate_over);
}

/*=============================================================================
 * CUSTOM GATE MANAGEMENT TESTS (6 tests)
 *============================================================================*/

TEST_F(PortiaLogicBridgeTest, AddCustomGateAND) {
    uint32_t gate_id = 0;
    int result = portia_logic_add_custom_gate(bridge, "A AND B", &gate_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(gate_id, 0u);
}

TEST_F(PortiaLogicBridgeTest, AddCustomGateOR) {
    uint32_t gate_id = 0;
    int result = portia_logic_add_custom_gate(bridge, "A OR B", &gate_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(gate_id, 0u);
}

TEST_F(PortiaLogicBridgeTest, AddCustomGateNOT) {
    uint32_t gate_id = 0;
    int result = portia_logic_add_custom_gate(bridge, "NOT A", &gate_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(gate_id, 0u);
}

TEST_F(PortiaLogicBridgeTest, AddCustomGateIMPLIES) {
    uint32_t gate_id = 0;
    int result = portia_logic_add_custom_gate(bridge, "A IMPLIES B", &gate_id);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_GT(gate_id, 0u);
}

TEST_F(PortiaLogicBridgeTest, AddCustomGateInvalidExpression) {
    uint32_t gate_id = 0;
    int result = portia_logic_add_custom_gate(bridge, "INVALID", &gate_id);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, AddCustomGateNullPointers) {
    uint32_t gate_id = 0;

    int result1 = portia_logic_add_custom_gate(nullptr, "A AND B", &gate_id);
    EXPECT_NE(result1, NIMCP_SUCCESS);

    int result2 = portia_logic_add_custom_gate(bridge, nullptr, &gate_id);
    EXPECT_NE(result2, NIMCP_SUCCESS);

    int result3 = portia_logic_add_custom_gate(bridge, "A AND B", nullptr);
    EXPECT_NE(result3, NIMCP_SUCCESS);
}

/*=============================================================================
 * RESOURCE CONDITION TESTS (8 tests)
 *============================================================================*/

TEST_F(PortiaLogicBridgeTest, UpdateConditions) {
    int result = portia_logic_update_conditions(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, GetConditions) {
    portia_resource_condition_t conditions;
    int result = portia_logic_get_conditions(bridge, &conditions);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, SetConditionMemoryOk) {
    int result = portia_logic_set_condition(bridge, "memory_ok", true);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    portia_resource_condition_t conditions;
    portia_logic_get_conditions(bridge, &conditions);
    EXPECT_TRUE(conditions.memory_ok);
}

TEST_F(PortiaLogicBridgeTest, SetConditionThermalOk) {
    int result = portia_logic_set_condition(bridge, "thermal_ok", false);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    portia_resource_condition_t conditions;
    portia_logic_get_conditions(bridge, &conditions);
    EXPECT_FALSE(conditions.thermal_ok);
}

TEST_F(PortiaLogicBridgeTest, SetConditionBatteryOk) {
    int result = portia_logic_set_condition(bridge, "battery_ok", true);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    portia_resource_condition_t conditions;
    portia_logic_get_conditions(bridge, &conditions);
    EXPECT_TRUE(conditions.battery_ok);
}

TEST_F(PortiaLogicBridgeTest, SetConditionEmergencyMode) {
    int result = portia_logic_set_condition(bridge, "emergency_mode", true);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    portia_resource_condition_t conditions;
    portia_logic_get_conditions(bridge, &conditions);
    EXPECT_TRUE(conditions.emergency_mode);
}

TEST_F(PortiaLogicBridgeTest, SetConditionInvalidName) {
    int result = portia_logic_set_condition(bridge, "invalid_condition", true);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, SetConditionNullPointers) {
    int result1 = portia_logic_set_condition(nullptr, "memory_ok", true);
    EXPECT_NE(result1, NIMCP_SUCCESS);

    int result2 = portia_logic_set_condition(bridge, nullptr, true);
    EXPECT_NE(result2, NIMCP_SUCCESS);
}

/*=============================================================================
 * INTEGRATION TESTS (6 tests)
 *============================================================================*/

TEST_F(PortiaLogicBridgeTest, ConnectBrain) {
    brain_t mock_brain = (brain_t)0x1234;  /* Mock pointer */
    int result = portia_logic_connect_brain(bridge, mock_brain);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, ConnectBrainNullBridge) {
    brain_t mock_brain = (brain_t)0x1234;
    int result = portia_logic_connect_brain(nullptr, mock_brain);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, ConnectImmune) {
    void* mock_immune = (void*)0x5678;  /* Mock pointer */
    int result = portia_logic_connect_immune(bridge, mock_immune);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, ConnectImmuneNullBridge) {
    void* mock_immune = (void*)0x5678;
    int result = portia_logic_connect_immune(nullptr, mock_immune);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, ConnectUMM) {
    void* mock_umm = (void*)0x9ABC;  /* Mock pointer */
    int result = portia_logic_connect_umm(bridge, mock_umm);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, ConnectUMMNullBridge) {
    void* mock_umm = (void*)0x9ABC;
    int result = portia_logic_connect_umm(nullptr, mock_umm);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

/*=============================================================================
 * BIO-ASYNC TESTS (5 tests)
 *============================================================================*/

TEST_F(PortiaLogicBridgeTest, IsBioAsyncConnected) {
    bool connected = portia_logic_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);  /* Disabled in test setup */
}

TEST_F(PortiaLogicBridgeTest, IsBioAsyncConnectedNullBridge) {
    bool connected = portia_logic_is_bio_async_connected(nullptr);
    EXPECT_FALSE(connected);
}

TEST_F(PortiaLogicBridgeTest, ProcessInbox) {
    int count = portia_logic_process_inbox(bridge);
    EXPECT_GE(count, 0);  /* Should return 0 (no bio-async enabled) */
}

TEST_F(PortiaLogicBridgeTest, ProcessInboxNullBridge) {
    int count = portia_logic_process_inbox(nullptr);
    EXPECT_EQ(count, NIMCP_ERROR_NULL_POINTER);  /* Error code for null bridge */
}

TEST_F(PortiaLogicBridgeTest, DisconnectBioAsyncNotConnected) {
    int result = portia_logic_disconnect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);  /* Should succeed even if not connected */
}

/*=============================================================================
 * STATISTICS TESTS (6 tests)
 *============================================================================*/

TEST_F(PortiaLogicBridgeTest, GetStats) {
    portia_logic_stats_t stats;
    int result = portia_logic_get_stats(bridge, &stats);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, GetStatsNullPointers) {
    portia_logic_stats_t stats;

    int result1 = portia_logic_get_stats(nullptr, &stats);
    EXPECT_NE(result1, NIMCP_SUCCESS);

    int result2 = portia_logic_get_stats(bridge, nullptr);
    EXPECT_NE(result2, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, ResetStats) {
    /* Perform some evaluations to generate stats */
    portia_logic_can_upgrade_tier(bridge, 0, 1);
    portia_logic_must_downgrade_tier(bridge, 2);

    /* Reset stats */
    int result = portia_logic_reset_stats(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    /* Verify stats are reset */
    portia_logic_stats_t stats;
    portia_logic_get_stats(bridge, &stats);
    EXPECT_EQ(stats.total_evaluations, 0u);
    EXPECT_EQ(stats.tier_upgrade_decisions, 0u);
    EXPECT_EQ(stats.tier_downgrade_decisions, 0u);
}

TEST_F(PortiaLogicBridgeTest, GetGateCount) {
    uint32_t count = portia_logic_get_gate_count(bridge);
    EXPECT_GE(count, 5u);  /* At least 5 pre-built gates */
}

TEST_F(PortiaLogicBridgeTest, GetGateCountNullBridge) {
    uint32_t count = portia_logic_get_gate_count(nullptr);
    EXPECT_EQ(count, 0u);
}

TEST_F(PortiaLogicBridgeTest, DumpGates) {
    portia_logic_dump_gates(bridge);
    SUCCEED();  /* Should not crash */
}

/*=============================================================================
 * ERROR HANDLING TESTS (5 tests)
 *============================================================================*/

TEST_F(PortiaLogicBridgeTest, EvaluateGateNullBridge) {
    bool result = portia_logic_evaluate_gate(nullptr, 1);
    EXPECT_FALSE(result);
}

TEST_F(PortiaLogicBridgeTest, GetGateDecisionNullPointers) {
    portia_logic_decision_t decision;

    int result1 = portia_logic_get_gate_decision(nullptr, 1, &decision);
    EXPECT_NE(result1, NIMCP_SUCCESS);

    int result2 = portia_logic_get_gate_decision(bridge, 1, nullptr);
    EXPECT_NE(result2, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, UpdateConditionsNullBridge) {
    int result = portia_logic_update_conditions(nullptr);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, GetConditionsNullPointers) {
    portia_resource_condition_t conditions;

    int result1 = portia_logic_get_conditions(nullptr, &conditions);
    EXPECT_NE(result1, NIMCP_SUCCESS);

    int result2 = portia_logic_get_conditions(bridge, nullptr);
    EXPECT_NE(result2, NIMCP_SUCCESS);
}

TEST_F(PortiaLogicBridgeTest, BroadcastDecisionNotConnected) {
    portia_logic_decision_t decision;
    memset(&decision, 0, sizeof(decision));
    decision.gate_id = 1;
    decision.result = true;
    decision.confidence = 0.9f;

    int result = portia_logic_broadcast_decision(bridge, &decision);
    EXPECT_NE(result, NIMCP_SUCCESS);  /* Bio-async not connected */
}

/*=============================================================================
 * THREAD SAFETY TESTS (3 tests)
 *============================================================================*/

TEST_F(PortiaLogicBridgeTest, ConcurrentTierUpgradeEvaluations) {
    const int num_threads = 10;
    std::thread threads[num_threads];

    /* Set all conditions to OK */
    portia_logic_set_condition(bridge, "memory_ok", true);
    portia_logic_set_condition(bridge, "thermal_ok", true);
    portia_logic_set_condition(bridge, "battery_ok", true);

    /* Launch concurrent evaluations */
    for (int i = 0; i < num_threads; i++) {
        threads[i] = std::thread([this]() {
            for (int j = 0; j < 100; j++) {
                portia_logic_can_upgrade_tier(this->bridge, 0, 1);
            }
        });
    }

    /* Wait for all threads */
    for (int i = 0; i < num_threads; i++) {
        threads[i].join();
    }

    /* Check stats */
    portia_logic_stats_t stats;
    portia_logic_get_stats(bridge, &stats);
    EXPECT_EQ(stats.tier_upgrade_decisions, 1000u);  /* 10 threads * 100 evals */
}

TEST_F(PortiaLogicBridgeTest, ConcurrentConditionUpdates) {
    const int num_threads = 10;
    std::thread threads[num_threads];

    /* Launch concurrent condition updates */
    for (int i = 0; i < num_threads; i++) {
        threads[i] = std::thread([this, i]() {
            for (int j = 0; j < 100; j++) {
                bool value = ((i + j) % 2 == 0);
                portia_logic_set_condition(this->bridge, "memory_ok", value);
            }
        });
    }

    /* Wait for all threads */
    for (int i = 0; i < num_threads; i++) {
        threads[i].join();
    }

    SUCCEED();  /* Should not crash */
}

TEST_F(PortiaLogicBridgeTest, ConcurrentCustomGateCreation) {
    const int num_threads = 5;
    std::thread threads[num_threads];

    /* Launch concurrent gate creation */
    for (int i = 0; i < num_threads; i++) {
        threads[i] = std::thread([this, i]() {
            for (int j = 0; j < 5; j++) {
                uint32_t gate_id = 0;
                const char* expressions[] = {"A AND B", "A OR B", "NOT A", "A XOR B", "A IMPLIES B"};
                portia_logic_add_custom_gate(this->bridge, expressions[j], &gate_id);
            }
        });
    }

    /* Wait for all threads */
    for (int i = 0; i < num_threads; i++) {
        threads[i].join();
    }

    /* Check gate count */
    uint32_t count = portia_logic_get_gate_count(bridge);
    EXPECT_GT(count, 5u);  /* At least pre-built gates + some custom ones */
}

/*=============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
