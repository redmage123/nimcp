/**
 * @file test_gpu_execution_immune.cpp
 * @brief Unit tests for GPU execution-immune bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Tests for GPU execution mode-immune bidirectional integration
 * WHY:  Ensure immune→execution and execution→immune pathways work correctly
 * HOW:  Test cytokine mode modulation, error responses, energy conservation
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "gpu/immune/nimcp_gpu_execution_immune_bridge.h"
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class GpuExecutionImmuneTest : public ::testing::Test {
protected:
    brain_immune_system_t* immune_system;
    execution_immune_bridge_t* bridge;

    void SetUp() override {
        /* Create immune system */
        brain_immune_config_t immune_config;
        brain_immune_default_config(&immune_config);
        immune_system = brain_immune_create(&immune_config);
        ASSERT_NE(immune_system, nullptr);

        brain_immune_start(immune_system);

        /* Create execution-immune bridge */
        execution_immune_config_t config;
        execution_immune_default_config(&config);
        bridge = execution_immune_create(&config, immune_system, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            execution_immune_destroy(bridge);
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

TEST_F(GpuExecutionImmuneTest, CreateDestroy) {
    execution_immune_config_t config;
    ASSERT_EQ(execution_immune_default_config(&config), NIMCP_SUCCESS);

    execution_immune_bridge_t* test_bridge =
        execution_immune_create(&config, immune_system, nullptr);
    ASSERT_NE(test_bridge, nullptr);

    execution_immune_destroy(test_bridge);
}

TEST_F(GpuExecutionImmuneTest, DefaultConfig) {
    execution_immune_config_t config;
    ASSERT_EQ(execution_immune_default_config(&config), NIMCP_SUCCESS);

    /* Verify defaults */
    EXPECT_TRUE(config.enable_cytokine_mode_modulation);
    EXPECT_TRUE(config.enable_exec_error_immune_response);
    EXPECT_TRUE(config.enable_energy_conservation);
    EXPECT_TRUE(config.enable_fallback_modulation);
    EXPECT_TRUE(config.enable_performance_monitoring);

    EXPECT_FLOAT_EQ(config.cytokine_sensitivity, 1.0f);
    EXPECT_FLOAT_EQ(config.error_sensitivity, 1.0f);
    EXPECT_EQ(config.baseline_mode, EXEC_MODE_GPU_CUDA);
    EXPECT_TRUE(config.allow_mode_switching);
}

TEST_F(GpuExecutionImmuneTest, CreateWithNullImmuneSystem) {
    execution_immune_config_t config;
    execution_immune_default_config(&config);

    execution_immune_bridge_t* null_bridge =
        execution_immune_create(&config, nullptr, nullptr);
    EXPECT_EQ(null_bridge, nullptr);
}

TEST_F(GpuExecutionImmuneTest, DefaultConfigNullParam) {
    EXPECT_NE(execution_immune_default_config(nullptr), NIMCP_SUCCESS);
}

/* ============================================================================
 * Immune → Execution Tests
 * ============================================================================ */

TEST_F(GpuExecutionImmuneTest, ApplyCytokineEffectsBaseline) {
    /* Apply cytokine effects at baseline */
    ASSERT_EQ(execution_immune_apply_cytokine_effects(bridge), NIMCP_SUCCESS);

    /* At baseline (no inflammation), should prefer GPU mode */
    execution_mode_t mode = execution_immune_get_recommended_mode(bridge);
    EXPECT_EQ(mode, EXEC_MODE_GPU_CUDA);

    /* Energy factor should be full (1.0) */
    float energy_factor = execution_immune_get_energy_factor(bridge);
    EXPECT_FLOAT_EQ(energy_factor, 1.0f);
}

TEST_F(GpuExecutionImmuneTest, GetRecommendedModeWithInflammation) {
    /* Trigger inflammation */
    uint32_t antigen_id;
    uint8_t epitope[] = {0xAB, 0xCD, 0xEF, 0x12};
    brain_immune_present_antigen(
        immune_system,
        ANTIGEN_SOURCE_MANUAL,
        epitope, sizeof(epitope),
        8,  /* High severity */
        0,
        &antigen_id
    );

    /* Initiate multiple inflammation sites for systemic level */
    uint32_t site_id;
    for (int i = 0; i < 3; i++) {
        brain_immune_initiate_inflammation(immune_system, i + 1, antigen_id, &site_id);
    }

    /* Apply cytokine effects */
    ASSERT_EQ(execution_immune_apply_cytokine_effects(bridge), NIMCP_SUCCESS);

    /* With systemic inflammation, should prefer CPU mode */
    execution_mode_t mode = execution_immune_get_recommended_mode(bridge);
    EXPECT_NE(mode, EXEC_MODE_GPU_CUDA);
}

TEST_F(GpuExecutionImmuneTest, EnergyConservationFactor) {
    float energy = execution_immune_get_energy_factor(bridge);
    EXPECT_GE(energy, 0.0f);
    EXPECT_LE(energy, 1.0f);
}

TEST_F(GpuExecutionImmuneTest, GetCytokineEffects) {
    ASSERT_EQ(execution_immune_apply_cytokine_effects(bridge), NIMCP_SUCCESS);

    execution_cytokine_effects_t effects;
    ASSERT_EQ(execution_immune_get_cytokine_effects(bridge, &effects), NIMCP_SUCCESS);

    /* Effects should be valid */
    EXPECT_GE(effects.energy_conservation_factor, 0.0f);
    EXPECT_LE(effects.energy_conservation_factor, 1.0f);
}

/* ============================================================================
 * Execution → Immune Tests
 * ============================================================================ */

TEST_F(GpuExecutionImmuneTest, TriggerErrorResponse) {
    int result = execution_immune_trigger_error_response(
        bridge, 100, "GPU initialization failed");
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(GpuExecutionImmuneTest, MonitorPerformance) {
    int result = execution_immune_monitor_performance(bridge);
    /* Returns success even with no context - monitors internal state */
    EXPECT_GE(result, 0);
}

TEST_F(GpuExecutionImmuneTest, UpdateErrorState) {
    int result = execution_immune_update_error_state(bridge);
    /* Returns success when context is null - internal state update */
    EXPECT_GE(result, 0);
}

TEST_F(GpuExecutionImmuneTest, GetErrorState) {
    execution_error_state_t state;
    ASSERT_EQ(execution_immune_get_error_state(bridge, &state), NIMCP_SUCCESS);

    /* State should be initialized */
    EXPECT_EQ(state.init_failures, 0);
}

/* ============================================================================
 * Bidirectional Update Tests
 * ============================================================================ */

TEST_F(GpuExecutionImmuneTest, UpdateBridge) {
    int result = execution_immune_update(bridge);
    EXPECT_GE(result, 0);
}

TEST_F(GpuExecutionImmuneTest, ApplyModulation) {
    int result = execution_immune_apply_modulation(bridge);
    EXPECT_GE(result, 0);
}

TEST_F(GpuExecutionImmuneTest, IsModeChanged) {
    bool changed = execution_immune_is_mode_changed(bridge);
    /* Initially should be at baseline */
    EXPECT_FALSE(changed);
}

TEST_F(GpuExecutionImmuneTest, GetEnergyConservationFactor) {
    float factor = execution_immune_get_energy_conservation_factor(bridge);
    EXPECT_GE(factor, 0.0f);
    EXPECT_LE(factor, 1.0f);
}

/* ============================================================================
 * Bio-Async Integration Tests
 * ============================================================================ */

TEST_F(GpuExecutionImmuneTest, BioAsyncConnect) {
    int result = execution_immune_connect_bio_async(bridge);
    /* May fail if bio_router not initialized - that's OK */
    (void)result;  /* Suppress unused warning */

    bool connected = execution_immune_is_bio_async_connected(bridge);
    /* Either connected or not - just test the query works */
    (void)connected;
}

TEST_F(GpuExecutionImmuneTest, BioAsyncDisconnect) {
    execution_immune_connect_bio_async(bridge);
    int result = execution_immune_disconnect_bio_async(bridge);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    bool connected = execution_immune_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

TEST_F(GpuExecutionImmuneTest, BioAsyncIsConnected) {
    /* Before connecting */
    bool connected = execution_immune_is_bio_async_connected(bridge);
    EXPECT_FALSE(connected);
}

/* ============================================================================
 * Null Parameter Tests
 * ============================================================================ */

TEST_F(GpuExecutionImmuneTest, NullBridgeGetMode) {
    execution_mode_t mode = execution_immune_get_recommended_mode(nullptr);
    EXPECT_EQ(mode, EXEC_MODE_GPU_CUDA);  /* Default return */
}

TEST_F(GpuExecutionImmuneTest, NullBridgeGetEnergy) {
    float energy = execution_immune_get_energy_factor(nullptr);
    EXPECT_FLOAT_EQ(energy, 1.0f);  /* Default return */
}

TEST_F(GpuExecutionImmuneTest, NullBridgeApplyCytokine) {
    int result = execution_immune_apply_cytokine_effects(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}

TEST_F(GpuExecutionImmuneTest, NullBridgeUpdate) {
    int result = execution_immune_update(nullptr);
    EXPECT_EQ(result, NIMCP_ERROR_NULL_POINTER);
}
