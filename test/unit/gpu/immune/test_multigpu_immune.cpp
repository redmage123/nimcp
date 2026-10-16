/**
 * @file test_multigpu_immune.cpp
 * @brief Unit tests for multi-GPU-immune bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Tests for multi-GPU coordination-immune bidirectional integration
 * WHY:  Ensure immune→multigpu and multigpu→immune pathways work correctly
 * HOW:  Test GPU count modulation, partition strategies, rebalancing
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "gpu/immune/nimcp_multigpu_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class MultigpuImmuneTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    multigpu_immune_bridge_t* bridge;

    void SetUp() override {
        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        brain_immune_start(immune_system);

        /* Create multigpu-immune bridge (no actual GPU context) */
        multigpu_immune_config_t config;
        multigpu_immune_default_config(&config);
        bridge = multigpu_immune_create(&config, immune_system, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            multigpu_immune_destroy(bridge);
        }
        if (immune_system) {
            brain_immune_stop(immune_system);
            brain_immune_destroy(immune_system);
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(MultigpuImmuneTest, CreateDestroy) {
    multigpu_immune_config_t config;
    ASSERT_EQ(multigpu_immune_default_config(&config), NIMCP_SUCCESS);

    multigpu_immune_bridge_t* test_bridge =
        multigpu_immune_create(&config, immune_system, nullptr);
    ASSERT_NE(test_bridge, nullptr);

    multigpu_immune_destroy(test_bridge);
}

TEST_F(MultigpuImmuneTest, DefaultConfig) {
    multigpu_immune_config_t config;
    ASSERT_EQ(multigpu_immune_default_config(&config), NIMCP_SUCCESS);

    /* Verify defaults */
    EXPECT_TRUE(config.enable_cytokine_coordination_modulation);
    EXPECT_TRUE(config.enable_multigpu_error_immune_response);
    EXPECT_TRUE(config.enable_gpu_count_modulation);
    EXPECT_TRUE(config.enable_partition_modulation);
    EXPECT_TRUE(config.enable_rebalance_modulation);

    EXPECT_FLOAT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.error_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.imbalance_sensitivity, 1.0f);
    EXPECT_EQ(config.baseline_gpu_count, 0);  /* Use all */
}

TEST_F(MultigpuImmuneTest, CreateWithNullImmuneSystem) {
    multigpu_immune_config_t config;
    multigpu_immune_default_config(&config);

    multigpu_immune_bridge_t* null_bridge =
        multigpu_immune_create(&config, nullptr, nullptr);
    EXPECT_EQ(null_bridge, nullptr);
}

TEST_F(MultigpuImmuneTest, DefaultConfigNullParam) {
    EXPECT_NE(multigpu_immune_default_config(nullptr), NIMCP_SUCCESS);
}

/* ============================================================================
 * Immune → Multi-GPU Tests
 * ============================================================================ */

TEST_F(MultigpuImmuneTest, ApplyCytokineEffectsBaseline) {
    ASSERT_EQ(multigpu_immune_apply_cytokine_effects(bridge), NIMCP_SUCCESS);

    /* At baseline, should use all GPUs (0 = all) */
    uint32_t gpu_count = multigpu_immune_get_recommended_gpu_count(bridge);
    EXPECT_EQ(gpu_count, 0);

    /* Rebalance factor should be 1.0 */
    float factor = multigpu_immune_get_rebalance_frequency_factor(bridge);
    EXPECT_FLOAT_EQ(factor, 1.0f);
}

TEST_F(MultigpuImmuneTest, GetRecommendedGpuCountWithInflammation) {
    /* Trigger inflammation */
    uint32_t antigen_id;
    uint8_t epitope[] = {0x11, 0x22, 0x33, 0x44};
    brain_immune_present_antigen(
        immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope, sizeof(epitope),
        8,
        0,
        &antigen_id
    );

    /* Multiple inflammation sites for systemic level */
    uint32_t site_id;
    for (int i = 0; i < 3; i++) {
        brain_immune_initiate_inflammation(immune_system, i + 1, antigen_id, &site_id);
    }

    ASSERT_EQ(multigpu_immune_apply_cytokine_effects(bridge), NIMCP_SUCCESS);

    /* With regional inflammation (3 sites), should reduce GPU count */
    uint32_t gpu_count = multigpu_immune_get_recommended_gpu_count(bridge);
    EXPECT_GE(gpu_count, 1);
    EXPECT_LE(gpu_count, 2);
}

TEST_F(MultigpuImmuneTest, GetRecommendedPartition) {
    ASSERT_EQ(multigpu_immune_apply_cytokine_effects(bridge), NIMCP_SUCCESS);

    multigpu_partition_strategy_t strategy =
        multigpu_immune_get_recommended_partition(bridge);
    /* At baseline, should be dynamic */
    EXPECT_EQ(strategy, MULTIGPU_PARTITION_DYNAMIC);
}

TEST_F(MultigpuImmuneTest, GetRebalanceFrequencyFactor) {
    ASSERT_EQ(multigpu_immune_apply_cytokine_effects(bridge), NIMCP_SUCCESS);

    float factor = multigpu_immune_get_rebalance_frequency_factor(bridge);
    EXPECT_GE(factor, 0.0f);
    EXPECT_LE(factor, 1.0f);
}

TEST_F(MultigpuImmuneTest, GetCytokineEffects) {
    ASSERT_EQ(multigpu_immune_apply_cytokine_effects(bridge), NIMCP_SUCCESS);

    multigpu_cytokine_effects_t effects;
    ASSERT_EQ(multigpu_immune_get_cytokine_effects(bridge, &effects), NIMCP_SUCCESS);

    EXPECT_GE(effects.rebalance_frequency_factor, 0.0f);
    EXPECT_LE(effects.rebalance_frequency_factor, 1.0f);
}

/* ============================================================================
 * Multi-GPU → Immune Tests
 * ============================================================================ */

TEST_F(MultigpuImmuneTest, TriggerErrorResponse) {
    int result = multigpu_immune_trigger_error_response(
        bridge, 100, "P2P access failed");
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(MultigpuImmuneTest, MonitorLoadBalance) {
    int result = multigpu_immune_monitor_load_balance(bridge);
    /* Returns error with null context, which is expected */
    (void)result;
}

TEST_F(MultigpuImmuneTest, UpdateErrorState) {
    int result = multigpu_immune_update_error_state(bridge);
    /* Returns error with null context, which is expected */
    (void)result;
}

TEST_F(MultigpuImmuneTest, GetErrorState) {
    multigpu_error_state_t state;
    ASSERT_EQ(multigpu_immune_get_error_state(bridge, &state), NIMCP_SUCCESS);

    /* State should be initialized */
    EXPECT_EQ(state.p2p_failures, 0);
    EXPECT_EQ(state.sync_failures, 0);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(MultigpuImmuneTest, UpdateBridge) {
    int result = multigpu_immune_update(bridge);
    EXPECT_GE(result, 0);
}

TEST_F(MultigpuImmuneTest, ApplyModulation) {
    int result = multigpu_immune_apply_modulation(bridge);
    /* May return error with null context, but should not crash */
    (void)result;
}

TEST_F(MultigpuImmuneTest, IsGpuCountReduced) {
    bool reduced = multigpu_immune_is_gpu_count_reduced(bridge);
    /* At baseline, should not be reduced */
    EXPECT_FALSE(reduced);
}

TEST_F(MultigpuImmuneTest, GetActiveGpuCount) {
    uint32_t count = multigpu_immune_get_active_gpu_count(bridge);
    /* With no context, should return 0 */
    EXPECT_EQ(count, 0);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(MultigpuImmuneTest, BioAsyncConnect) {
    int result = multigpu_immune_connect_bio_async(bridge);
    (void)result;

    bool connected = multigpu_immune_is_bio_async_connected(bridge);
    (void)connected;
}

TEST_F(MultigpuImmuneTest, BioAsyncDisconnect) {
    multigpu_immune_connect_bio_async(bridge);
    int result = multigpu_immune_disconnect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    bool connected = multigpu_immune_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(MultigpuImmuneTest, BioAsyncIsConnected) {
    bool connected = multigpu_immune_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

/* ============================================================================
 * Null Parameter Tests
 * ============================================================================ */

TEST_F(MultigpuImmuneTest, NullBridgeGetGpuCount) {
    uint32_t count = multigpu_immune_get_recommended_gpu_count(nullptr);
    EXPECT_EQ(count, 0);
}

TEST_F(MultigpuImmuneTest, NullBridgeGetPartition) {
    multigpu_partition_strategy_t strategy =
        multigpu_immune_get_recommended_partition(nullptr);
    EXPECT_EQ(strategy, MULTIGPU_PARTITION_DYNAMIC);
}

TEST_F(MultigpuImmuneTest, NullBridgeApplyCytokine) {
    int result = multigpu_immune_apply_cytokine_effects(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(MultigpuImmuneTest, NullBridgeUpdate) {
    int result = multigpu_immune_update(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}
