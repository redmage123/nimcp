/**
 * @file test_fep_orchestrator.cpp
 * @brief Unit tests for FEP Orchestrator
 * @version 1.0.0
 * @date 2025-12-15
 *
 * Tests for the FEP Orchestrator including:
 * - Lifecycle (create, destroy, start, stop, pause, resume)
 * - Bridge registration and unregistration
 * - Update cycles and category-based updates
 * - Bio-async integration
 * - Immune system integration
 * - Statistics tracking
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_time.h"

/* ============================================================================
 * Mock Bridge for Testing
 * ============================================================================ */

static int g_mock_update_count = 0;
static int g_mock_update_return = 0;

static int mock_bridge_update(void* bridge) {
    (void)bridge;
    g_mock_update_count++;
    return g_mock_update_return;
}

static void mock_bridge_destroy(void* bridge) {
    (void)bridge;
    /* No-op for mock */
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class FepOrchestratorTest : public ::testing::Test {
protected:
    fep_orchestrator_t* orchestrator = nullptr;
    fep_orchestrator_config_t config;

    void SetUp() override {
        g_mock_update_count = 0;
        g_mock_update_return = 0;
        fep_orchestrator_default_config(&config);
        config.enable_logging = false;  /* Reduce test output */
        orchestrator = fep_orchestrator_create(&config);
        ASSERT_NE(orchestrator, nullptr);
    }

    void TearDown() override {
        if (orchestrator) {
            fep_orchestrator_destroy(orchestrator);
            orchestrator = nullptr;
        }
    }
};

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(FepOrchestratorTest, DefaultConfigIsValid) {
    fep_orchestrator_config_t cfg;
    int result = fep_orchestrator_default_config(&cfg);
    EXPECT_EQ(result, 0);
    EXPECT_GT(cfg.max_bridges, 0u);
    EXPECT_TRUE(cfg.enable_auto_update);
    EXPECT_TRUE(cfg.enable_bio_async);
    EXPECT_TRUE(cfg.enable_brain_immune);
    EXPECT_TRUE(cfg.enable_statistics);
    
    /* Check category intervals */
    EXPECT_TRUE(cfg.categories[FEP_BRIDGE_CATEGORY_COGNITIVE].enabled);
    EXPECT_EQ(cfg.categories[FEP_BRIDGE_CATEGORY_COGNITIVE].update_interval_ms, FEP_UPDATE_INTERVAL_COGNITIVE);
}

TEST_F(FepOrchestratorTest, DefaultConfigNullFails) {
    int result = fep_orchestrator_default_config(nullptr);
    EXPECT_NE(result, 0);  /* Returns NIMCP_ERROR_NULL_POINTER */
}

TEST_F(FepOrchestratorTest, CreateWithNullConfigUsesDefaults) {
    fep_orchestrator_t* orch = fep_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);
    EXPECT_EQ(orch->config.max_bridges, FEP_ORCHESTRATOR_MAX_BRIDGES);
    fep_orchestrator_destroy(orch);
}

TEST_F(FepOrchestratorTest, CreateWithCustomConfig) {
    fep_orchestrator_config_t custom_cfg;
    fep_orchestrator_default_config(&custom_cfg);
    custom_cfg.max_bridges = 64;
    custom_cfg.enable_logging = false;
    
    fep_orchestrator_t* orch = fep_orchestrator_create(&custom_cfg);
    ASSERT_NE(orch, nullptr);
    EXPECT_EQ(orch->bridge_capacity, 64u);
    fep_orchestrator_destroy(orch);
}

TEST_F(FepOrchestratorTest, DestroyNullIsSafe) {
    fep_orchestrator_destroy(nullptr);
    /* Should not crash */
}

TEST_F(FepOrchestratorTest, StartSucceeds) {
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_STOPPED);
    int result = fep_orchestrator_start(orchestrator);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_RUNNING);
}

TEST_F(FepOrchestratorTest, StartNullFails) {
    int result = fep_orchestrator_start(nullptr);
    EXPECT_NE(result, 0);  /* Returns NIMCP_ERROR_NULL_POINTER */
}

TEST_F(FepOrchestratorTest, StopSucceeds) {
    fep_orchestrator_start(orchestrator);
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_RUNNING);
    
    int result = fep_orchestrator_stop(orchestrator);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_STOPPED);
}

TEST_F(FepOrchestratorTest, PauseAndResumeSucceeds) {
    fep_orchestrator_start(orchestrator);
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_RUNNING);
    
    int result = fep_orchestrator_pause(orchestrator);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_PAUSED);
    
    result = fep_orchestrator_resume(orchestrator);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_RUNNING);
}

TEST_F(FepOrchestratorTest, PauseWhenNotRunningFails) {
    /* Orchestrator is stopped by default */
    int result = fep_orchestrator_pause(orchestrator);
    EXPECT_NE(result, 0);  /* Returns NIMCP_ERROR_INVALID_STATE */
}

TEST_F(FepOrchestratorTest, ResumeWhenNotPausedFails) {
    fep_orchestrator_start(orchestrator);
    int result = fep_orchestrator_resume(orchestrator);
    EXPECT_NE(result, 0);  /* Returns NIMCP_ERROR_INVALID_STATE */
}

/* ============================================================================
 * Bridge Registration Tests
 * ============================================================================ */

TEST_F(FepOrchestratorTest, RegisterBridgeSucceeds) {
    int mock_bridge = 42;
    uint32_t bridge_id = 0;
    
    int result = fep_orchestrator_register_bridge(
        orchestrator,
        "test_bridge",
        FEP_BRIDGE_CATEGORY_COGNITIVE,
        &mock_bridge,
        mock_bridge_update,
        mock_bridge_destroy,
        &bridge_id
    );
    
    EXPECT_EQ(result, 0);
    EXPECT_GT(bridge_id, 0u);
    EXPECT_EQ(orchestrator->bridge_count, 1u);
}

TEST_F(FepOrchestratorTest, RegisterMultipleBridges) {
    int bridges[5];
    uint32_t ids[5];
    
    for (int i = 0; i < 5; i++) {
        bridges[i] = i;
        char name[32];
        snprintf(name, sizeof(name), "bridge_%d", i);
        
        int result = fep_orchestrator_register_bridge(
            orchestrator,
            name,
            (fep_bridge_category_t)(i % FEP_BRIDGE_CATEGORY_COUNT),
            &bridges[i],
            mock_bridge_update,
            nullptr,
            &ids[i]
        );
        EXPECT_EQ(result, 0);
    }
    
    EXPECT_EQ(orchestrator->bridge_count, 5u);
}

TEST_F(FepOrchestratorTest, RegisterBridgeNullParamsFail) {
    int mock_bridge = 42;
    uint32_t bridge_id = 0;
    
    /* Null orchestrator */
    EXPECT_EQ(fep_orchestrator_register_bridge(
        nullptr, "test", FEP_BRIDGE_CATEGORY_COGNITIVE, &mock_bridge,
        mock_bridge_update, nullptr, &bridge_id
    ), NIMCP_ERROR_NULL_POINTER);
    
    /* Null name */
    EXPECT_EQ(fep_orchestrator_register_bridge(
        orchestrator, nullptr, FEP_BRIDGE_CATEGORY_COGNITIVE, &mock_bridge,
        mock_bridge_update, nullptr, &bridge_id
    ), NIMCP_ERROR_NULL_POINTER);
    
    /* Null handle */
    EXPECT_EQ(fep_orchestrator_register_bridge(
        orchestrator, "test", FEP_BRIDGE_CATEGORY_COGNITIVE, nullptr,
        mock_bridge_update, nullptr, &bridge_id
    ), NIMCP_ERROR_NULL_POINTER);
    
    /* Null update function */
    EXPECT_EQ(fep_orchestrator_register_bridge(
        orchestrator, "test", FEP_BRIDGE_CATEGORY_COGNITIVE, &mock_bridge,
        nullptr, nullptr, &bridge_id
    ), NIMCP_ERROR_NULL_POINTER);
}

TEST_F(FepOrchestratorTest, RegisterBridgeInvalidCategoryFails) {
    int mock_bridge = 42;
    uint32_t bridge_id = 0;
    
    int result = fep_orchestrator_register_bridge(
        orchestrator,
        "test_bridge",
        (fep_bridge_category_t)999,  /* Invalid category */
        &mock_bridge,
        mock_bridge_update,
        nullptr,
        &bridge_id
    );
    
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(FepOrchestratorTest, UnregisterBridgeSucceeds) {
    int mock_bridge = 42;
    uint32_t bridge_id = 0;
    
    fep_orchestrator_register_bridge(
        orchestrator, "test", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &mock_bridge, mock_bridge_update, nullptr, &bridge_id
    );
    EXPECT_EQ(orchestrator->bridge_count, 1u);
    
    int result = fep_orchestrator_unregister_bridge(orchestrator, bridge_id);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(orchestrator->bridge_count, 0u);
}

TEST_F(FepOrchestratorTest, UnregisterNonExistentBridgeFails) {
    int result = fep_orchestrator_unregister_bridge(orchestrator, 9999);
    EXPECT_EQ(result, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(FepOrchestratorTest, SetBridgeEnabledSucceeds) {
    int mock_bridge = 42;
    uint32_t bridge_id = 0;
    
    fep_orchestrator_register_bridge(
        orchestrator, "test", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &mock_bridge, mock_bridge_update, nullptr, &bridge_id
    );
    
    /* Disable */
    int result = fep_orchestrator_set_bridge_enabled(orchestrator, bridge_id, false);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(orchestrator->stats.active_bridges, 0u);
    
    /* Re-enable */
    result = fep_orchestrator_set_bridge_enabled(orchestrator, bridge_id, true);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(orchestrator->stats.active_bridges, 1u);
}

TEST_F(FepOrchestratorTest, GetBridgeSucceeds) {
    int mock_bridge = 42;
    uint32_t bridge_id = 0;
    
    fep_orchestrator_register_bridge(
        orchestrator, "test_bridge", FEP_BRIDGE_CATEGORY_SWARM,
        &mock_bridge, mock_bridge_update, nullptr, &bridge_id
    );
    
    const fep_bridge_entry_t* entry = fep_orchestrator_get_bridge(orchestrator, bridge_id);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->bridge_id, bridge_id);
    EXPECT_STREQ(entry->bridge_name, "test_bridge");
    EXPECT_EQ(entry->category, FEP_BRIDGE_CATEGORY_SWARM);
}

TEST_F(FepOrchestratorTest, GetBridgeNonExistentReturnsNull) {
    const fep_bridge_entry_t* entry = fep_orchestrator_get_bridge(orchestrator, 9999);
    EXPECT_EQ(entry, nullptr);
}

/* ============================================================================
 * Update Tests
 * ============================================================================ */

TEST_F(FepOrchestratorTest, UpdateWhenNotRunningDoesNothing) {
    int mock_bridge = 42;
    uint32_t bridge_id = 0;
    
    fep_orchestrator_register_bridge(
        orchestrator, "test", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &mock_bridge, mock_bridge_update, nullptr, &bridge_id
    );
    
    /* Orchestrator is stopped */
    uint64_t now = nimcp_platform_time_monotonic_ms();
    int updated = fep_orchestrator_update(orchestrator, now);
    EXPECT_EQ(updated, 0);
    EXPECT_EQ(g_mock_update_count, 0);
}

TEST_F(FepOrchestratorTest, UpdateCallsBridges) {
    int mock_bridge = 42;
    uint32_t bridge_id = 0;
    
    fep_orchestrator_register_bridge(
        orchestrator, "test", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &mock_bridge, mock_bridge_update, nullptr, &bridge_id
    );
    
    fep_orchestrator_start(orchestrator);
    
    /* Wait for update interval and call update */
    uint64_t start = nimcp_platform_time_monotonic_ms();
    uint64_t now = start + FEP_UPDATE_INTERVAL_COGNITIVE + 1;
    
    int updated = fep_orchestrator_update(orchestrator, now);
    EXPECT_GE(updated, 1);
    EXPECT_EQ(g_mock_update_count, 1);
}

TEST_F(FepOrchestratorTest, UpdateRespectsInterval) {
    int mock_bridge = 42;
    uint32_t bridge_id = 0;
    
    fep_orchestrator_register_bridge(
        orchestrator, "test", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &mock_bridge, mock_bridge_update, nullptr, &bridge_id
    );
    
    fep_orchestrator_start(orchestrator);
    
    uint64_t start = nimcp_platform_time_monotonic_ms();
    
    /* First update - triggers immediately after start (interval passed) */
    int updated = fep_orchestrator_update(orchestrator, start + FEP_UPDATE_INTERVAL_COGNITIVE + 1);
    EXPECT_GE(updated, 1);
    
    /* Second update too soon - should not trigger */
    g_mock_update_count = 0;
    updated = fep_orchestrator_update(orchestrator, start + FEP_UPDATE_INTERVAL_COGNITIVE + 2);
    EXPECT_EQ(updated, 0);
    EXPECT_EQ(g_mock_update_count, 0);
}

TEST_F(FepOrchestratorTest, ForceUpdateAllUpdatesBridges) {
    int bridges[3];
    
    for (int i = 0; i < 3; i++) {
        bridges[i] = i;
        fep_orchestrator_register_bridge(
            orchestrator, "test", FEP_BRIDGE_CATEGORY_COGNITIVE,
            &bridges[i], mock_bridge_update, nullptr, nullptr
        );
    }
    
    fep_orchestrator_start(orchestrator);
    
    int updated = fep_orchestrator_force_update_all(orchestrator);
    EXPECT_EQ(updated, 3);
    EXPECT_EQ(g_mock_update_count, 3);
}

TEST_F(FepOrchestratorTest, UpdateCategoryUpdatesOnlyMatchingBridges) {
    int cognitive_bridge = 1;
    int swarm_bridge = 2;
    
    fep_orchestrator_register_bridge(
        orchestrator, "cognitive", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &cognitive_bridge, mock_bridge_update, nullptr, nullptr
    );
    fep_orchestrator_register_bridge(
        orchestrator, "swarm", FEP_BRIDGE_CATEGORY_SWARM,
        &swarm_bridge, mock_bridge_update, nullptr, nullptr
    );
    
    fep_orchestrator_start(orchestrator);
    
    uint64_t now = nimcp_platform_time_monotonic_ms() + 100;
    int updated = fep_orchestrator_update_category(orchestrator, FEP_BRIDGE_CATEGORY_COGNITIVE, now);
    EXPECT_EQ(updated, 1);
    EXPECT_EQ(g_mock_update_count, 1);
}

TEST_F(FepOrchestratorTest, UpdateBridgeUpdatesSpecificBridge) {
    int mock_bridge = 42;
    uint32_t bridge_id = 0;
    
    fep_orchestrator_register_bridge(
        orchestrator, "test", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &mock_bridge, mock_bridge_update, nullptr, &bridge_id
    );
    
    fep_orchestrator_start(orchestrator);
    
    int result = fep_orchestrator_update_bridge(orchestrator, bridge_id);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(g_mock_update_count, 1);
}

TEST_F(FepOrchestratorTest, UpdateBridgeFailedUpdatesTrackErrors) {
    int mock_bridge = 42;
    uint32_t bridge_id = 0;
    
    fep_orchestrator_register_bridge(
        orchestrator, "test", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &mock_bridge, mock_bridge_update, nullptr, &bridge_id
    );
    
    fep_orchestrator_start(orchestrator);
    
    /* Make update fail */
    g_mock_update_return = -1;
    
    int result = fep_orchestrator_update_bridge(orchestrator, bridge_id);
    EXPECT_NE(result, 0);  /* Returns NIMCP_ERROR_OPERATION_FAILED */
    EXPECT_EQ(orchestrator->stats.update_errors, 1u);
}

/* ============================================================================
 * Category Configuration Tests
 * ============================================================================ */

TEST_F(FepOrchestratorTest, SetUpdateIntervalSucceeds) {
    int result = fep_orchestrator_set_update_interval(
        orchestrator, FEP_BRIDGE_CATEGORY_COGNITIVE, 100
    );
    EXPECT_EQ(result, 0);
    EXPECT_EQ(orchestrator->config.categories[FEP_BRIDGE_CATEGORY_COGNITIVE].update_interval_ms, 100u);
}

TEST_F(FepOrchestratorTest, SetCategoryEnabledSucceeds) {
    int result = fep_orchestrator_set_category_enabled(
        orchestrator, FEP_BRIDGE_CATEGORY_PLASTICITY, false
    );
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(orchestrator->config.categories[FEP_BRIDGE_CATEGORY_PLASTICITY].enabled);
}

TEST_F(FepOrchestratorTest, GetCategoryConfigSucceeds) {
    fep_category_config_t cat_cfg;
    int result = fep_orchestrator_get_category_config(
        orchestrator, FEP_BRIDGE_CATEGORY_COGNITIVE, &cat_cfg
    );
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(cat_cfg.enabled);
    EXPECT_EQ(cat_cfg.update_interval_ms, FEP_UPDATE_INTERVAL_COGNITIVE);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(FepOrchestratorTest, GetStatsSucceeds) {
    fep_orchestrator_stats_t stats;
    int result = fep_orchestrator_get_stats(orchestrator, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.total_bridges, 0u);
}

TEST_F(FepOrchestratorTest, ResetStatsPreservesBridgeCounts) {
    int mock_bridge = 42;
    fep_orchestrator_register_bridge(
        orchestrator, "test", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &mock_bridge, mock_bridge_update, nullptr, nullptr
    );
    
    fep_orchestrator_start(orchestrator);
    fep_orchestrator_force_update_all(orchestrator);
    
    EXPECT_GT(orchestrator->stats.total_bridge_updates, 0u);
    
    fep_orchestrator_reset_stats(orchestrator);
    
    EXPECT_EQ(orchestrator->stats.total_bridge_updates, 0u);
    EXPECT_EQ(orchestrator->stats.total_bridges, 1u);  /* Preserved */
}

TEST_F(FepOrchestratorTest, GetLoadReturnsZeroWithoutBudget) {
    float load = fep_orchestrator_get_load(orchestrator);
    EXPECT_EQ(load, 0.0f);  /* No budget set */
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

TEST_F(FepOrchestratorTest, ConnectBrainImmuneSucceeds) {
    /* Note: We don't have a real brain_immune_system_t for unit tests */
    int result = fep_orchestrator_connect_brain_immune(orchestrator, nullptr);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(orchestrator->immune_connected);  /* NULL immune = not connected */
}

TEST_F(FepOrchestratorTest, DisconnectBrainImmuneSucceeds) {
    int result = fep_orchestrator_disconnect_brain_immune(orchestrator);
    EXPECT_EQ(result, 0);
}

TEST_F(FepOrchestratorTest, ConnectBioAsyncSucceeds) {
    /* Bio-async may not be initialized in unit test environment */
    int result = fep_orchestrator_connect_bio_async(orchestrator);
    EXPECT_EQ(result, 0);  /* Should succeed even if router not initialized */
}

TEST_F(FepOrchestratorTest, DisconnectBioAsyncSucceeds) {
    int result = fep_orchestrator_disconnect_bio_async(orchestrator);
    EXPECT_EQ(result, 0);
}

/* ============================================================================
 * String Conversion Tests
 * ============================================================================ */

TEST_F(FepOrchestratorTest, CategoryToStringIsValid) {
    EXPECT_STREQ(fep_bridge_category_to_string(FEP_BRIDGE_CATEGORY_COGNITIVE), "cognitive");
    EXPECT_STREQ(fep_bridge_category_to_string(FEP_BRIDGE_CATEGORY_SWARM), "swarm");
    EXPECT_STREQ(fep_bridge_category_to_string(FEP_BRIDGE_CATEGORY_SECURITY), "security");
    EXPECT_STREQ(fep_bridge_category_to_string(FEP_BRIDGE_CATEGORY_PLASTICITY), "plasticity");
    EXPECT_STREQ(fep_bridge_category_to_string((fep_bridge_category_t)999), "unknown");
}

TEST_F(FepOrchestratorTest, StateToStringIsValid) {
    EXPECT_STREQ(fep_orchestrator_state_to_string(FEP_ORCHESTRATOR_STOPPED), "stopped");
    EXPECT_STREQ(fep_orchestrator_state_to_string(FEP_ORCHESTRATOR_RUNNING), "running");
    EXPECT_STREQ(fep_orchestrator_state_to_string(FEP_ORCHESTRATOR_PAUSED), "paused");
    EXPECT_STREQ(fep_orchestrator_state_to_string((fep_orchestrator_state_t)999), "unknown");
}

/* ============================================================================
 * Edge Case Tests
 * ============================================================================ */

TEST_F(FepOrchestratorTest, DisabledBridgeNotUpdated) {
    int mock_bridge = 42;
    uint32_t bridge_id = 0;
    
    fep_orchestrator_register_bridge(
        orchestrator, "test", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &mock_bridge, mock_bridge_update, nullptr, &bridge_id
    );
    
    /* Disable the bridge */
    fep_orchestrator_set_bridge_enabled(orchestrator, bridge_id, false);
    
    fep_orchestrator_start(orchestrator);
    fep_orchestrator_force_update_all(orchestrator);
    
    EXPECT_EQ(g_mock_update_count, 0);  /* Bridge was disabled */
}

TEST_F(FepOrchestratorTest, DisabledCategoryNotUpdated) {
    int mock_bridge = 42;
    
    fep_orchestrator_register_bridge(
        orchestrator, "test", FEP_BRIDGE_CATEGORY_PLASTICITY,
        &mock_bridge, mock_bridge_update, nullptr, nullptr
    );
    
    /* Disable the category */
    fep_orchestrator_set_category_enabled(orchestrator, FEP_BRIDGE_CATEGORY_PLASTICITY, false);
    
    fep_orchestrator_start(orchestrator);
    
    uint64_t now = nimcp_platform_time_monotonic_ms() + 2000;  /* Well past interval */
    fep_orchestrator_update(orchestrator, now);
    
    EXPECT_EQ(g_mock_update_count, 0);  /* Category was disabled */
}

TEST_F(FepOrchestratorTest, DoubleStartIsSafe) {
    fep_orchestrator_start(orchestrator);
    int result = fep_orchestrator_start(orchestrator);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_RUNNING);
}

TEST_F(FepOrchestratorTest, DoubleStopIsSafe) {
    fep_orchestrator_start(orchestrator);
    fep_orchestrator_stop(orchestrator);
    int result = fep_orchestrator_stop(orchestrator);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_STOPPED);
}

/* ============================================================================
 * Continuous Scheduling Tests
 * ============================================================================ */

TEST_F(FepOrchestratorTest, ComputeUpdateIntervalNullParamsFail) {
    fep_continuous_schedule_config_t sched;
    float interval = 0.0f;

    /* Null config */
    EXPECT_EQ(fep_compute_update_interval(0.0f, 0.0f, nullptr, &interval), -1);

    /* Null output */
    EXPECT_EQ(fep_compute_update_interval(0.0f, 0.0f, &sched, nullptr), -1);
}

TEST_F(FepOrchestratorTest, ComputeUpdateIntervalZeroMetricsReturnsMax) {
    fep_continuous_schedule_config_t sched = {
        .enabled = true,
        .min_interval_ms = 5.0f,
        .max_interval_ms = 2000.0f,
        .decay_rate = 3.0f,
        .fe_scale = 2.0f
    };
    float interval = 0.0f;

    /* Zero prediction error and zero free energy → exp(0)=1 → max_interval */
    int result = fep_compute_update_interval(0.0f, 0.0f, &sched, &interval);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(interval, 2000.0f);
}

TEST_F(FepOrchestratorTest, ComputeUpdateIntervalHighPredictionErrorShortensInterval) {
    fep_continuous_schedule_config_t sched = {
        .enabled = true,
        .min_interval_ms = 5.0f,
        .max_interval_ms = 2000.0f,
        .decay_rate = 3.0f,
        .fe_scale = 0.0f  /* Disable free energy effect */
    };
    float interval_low = 0.0f;
    float interval_high = 0.0f;

    fep_compute_update_interval(0.1f, 0.0f, &sched, &interval_low);
    fep_compute_update_interval(2.0f, 0.0f, &sched, &interval_high);

    /* Higher prediction error should produce shorter interval */
    EXPECT_LT(interval_high, interval_low);
    EXPECT_GT(interval_low, 0.0f);
    EXPECT_GT(interval_high, 0.0f);
}

TEST_F(FepOrchestratorTest, ComputeUpdateIntervalHighFreeEnergyShortensInterval) {
    fep_continuous_schedule_config_t sched = {
        .enabled = true,
        .min_interval_ms = 5.0f,
        .max_interval_ms = 2000.0f,
        .decay_rate = 0.0f,  /* Disable prediction error effect */
        .fe_scale = 2.0f
    };
    float interval_low = 0.0f;
    float interval_high = 0.0f;

    fep_compute_update_interval(0.0f, 0.1f, &sched, &interval_low);
    fep_compute_update_interval(0.0f, 10.0f, &sched, &interval_high);

    /* Higher free energy should produce shorter interval */
    EXPECT_LT(interval_high, interval_low);
}

TEST_F(FepOrchestratorTest, ComputeUpdateIntervalClampedToMinMax) {
    fep_continuous_schedule_config_t sched = {
        .enabled = true,
        .min_interval_ms = 10.0f,
        .max_interval_ms = 500.0f,
        .decay_rate = 100.0f,  /* Very aggressive — drives interval near zero */
        .fe_scale = 100.0f
    };
    float interval = 0.0f;

    /* Extreme metrics should not go below min */
    fep_compute_update_interval(100.0f, 100.0f, &sched, &interval);
    EXPECT_GE(interval, 10.0f);
    EXPECT_LE(interval, 500.0f);
}

TEST_F(FepOrchestratorTest, ComputeUpdateIntervalNegativeInputsClamped) {
    fep_continuous_schedule_config_t sched = {
        .enabled = true,
        .min_interval_ms = 5.0f,
        .max_interval_ms = 1000.0f,
        .decay_rate = 3.0f,
        .fe_scale = 2.0f
    };
    float interval = 0.0f;

    /* Negative inputs should be clamped to zero → same as zero metrics */
    int result = fep_compute_update_interval(-5.0f, -10.0f, &sched, &interval);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(interval, 1000.0f);
}

TEST_F(FepOrchestratorTest, ComputeUpdateIntervalSmoothCurve) {
    fep_continuous_schedule_config_t sched = {
        .enabled = true,
        .min_interval_ms = 5.0f,
        .max_interval_ms = 1000.0f,
        .decay_rate = 2.0f,
        .fe_scale = 1.0f
    };

    /* Sample at multiple points and verify monotonic decrease */
    float prev_interval = 1e9f;
    for (float pe = 0.0f; pe <= 3.0f; pe += 0.5f) {
        float interval = 0.0f;
        fep_compute_update_interval(pe, 0.0f, &sched, &interval);
        EXPECT_LE(interval, prev_interval);
        prev_interval = interval;
    }
}

TEST_F(FepOrchestratorTest, SchedulingTierLabelFast) {
    EXPECT_STREQ(fep_scheduling_tier_label(5.0f), "fast");
    EXPECT_STREQ(fep_scheduling_tier_label(10.0f), "fast");
    EXPECT_STREQ(fep_scheduling_tier_label(15.0f), "fast");
}

TEST_F(FepOrchestratorTest, SchedulingTierLabelMedium) {
    EXPECT_STREQ(fep_scheduling_tier_label(16.0f), "medium");
    EXPECT_STREQ(fep_scheduling_tier_label(50.0f), "medium");
    EXPECT_STREQ(fep_scheduling_tier_label(75.0f), "medium");
}

TEST_F(FepOrchestratorTest, SchedulingTierLabelSlow) {
    EXPECT_STREQ(fep_scheduling_tier_label(76.0f), "slow");
    EXPECT_STREQ(fep_scheduling_tier_label(500.0f), "slow");
    EXPECT_STREQ(fep_scheduling_tier_label(2000.0f), "slow");
}

TEST_F(FepOrchestratorTest, SetFepMetricsSucceeds) {
    fep_scheduling_metrics_t metrics = {
        .prediction_error = 0.5f,
        .free_energy = 1.0f,
        .surprise = 0.3f
    };

    int result = fep_orchestrator_set_fep_metrics(orchestrator, &metrics);
    EXPECT_EQ(result, 0);

    /* Verify metrics are stored */
    fep_scheduling_metrics_t out = {};
    result = fep_orchestrator_get_fep_metrics(orchestrator, &out);
    EXPECT_EQ(result, 0);
    EXPECT_FLOAT_EQ(out.prediction_error, 0.5f);
    EXPECT_FLOAT_EQ(out.free_energy, 1.0f);
    EXPECT_FLOAT_EQ(out.surprise, 0.3f);
}

TEST_F(FepOrchestratorTest, SetFepMetricsNullFails) {
    fep_scheduling_metrics_t metrics = {};
    EXPECT_NE(fep_orchestrator_set_fep_metrics(nullptr, &metrics), 0);
    EXPECT_NE(fep_orchestrator_set_fep_metrics(orchestrator, nullptr), 0);
}

TEST_F(FepOrchestratorTest, GetFepMetricsNullFails) {
    fep_scheduling_metrics_t metrics = {};
    EXPECT_NE(fep_orchestrator_get_fep_metrics(nullptr, &metrics), 0);
    EXPECT_NE(fep_orchestrator_get_fep_metrics(orchestrator, nullptr), 0);
}

TEST_F(FepOrchestratorTest, ContinuousSchedulingDisabledByDefault) {
    /* Default config should have continuous scheduling disabled */
    fep_orchestrator_config_t cfg;
    fep_orchestrator_default_config(&cfg);
    EXPECT_FALSE(cfg.continuous_schedule.enabled);

    /* Fixed intervals should still work normally */
    EXPECT_EQ(cfg.categories[FEP_BRIDGE_CATEGORY_COGNITIVE].update_interval_ms,
              FEP_UPDATE_INTERVAL_COGNITIVE);
}

TEST_F(FepOrchestratorTest, ContinuousSchedulingDefaultParams) {
    fep_orchestrator_config_t cfg;
    fep_orchestrator_default_config(&cfg);

    EXPECT_FLOAT_EQ(cfg.continuous_schedule.min_interval_ms, FEP_CONTINUOUS_MIN_INTERVAL_MS);
    EXPECT_FLOAT_EQ(cfg.continuous_schedule.max_interval_ms, FEP_CONTINUOUS_MAX_INTERVAL_MS);
    EXPECT_FLOAT_EQ(cfg.continuous_schedule.decay_rate, FEP_CONTINUOUS_DECAY_RATE);
    EXPECT_FLOAT_EQ(cfg.continuous_schedule.fe_scale, FEP_CONTINUOUS_FE_SCALE);
}

TEST_F(FepOrchestratorTest, ContinuousSchedulingRecomputesIntervals) {
    /* Enable continuous scheduling */
    orchestrator->config.continuous_schedule.enabled = true;
    orchestrator->config.continuous_schedule.min_interval_ms = 5.0f;
    orchestrator->config.continuous_schedule.max_interval_ms = 1000.0f;
    orchestrator->config.continuous_schedule.decay_rate = 3.0f;
    orchestrator->config.continuous_schedule.fe_scale = 2.0f;

    fep_scheduling_metrics_t metrics = {
        .prediction_error = 1.0f,
        .free_energy = 0.5f,
        .surprise = 0.0f
    };

    fep_orchestrator_set_fep_metrics(orchestrator, &metrics);

    /* Verify continuous intervals were computed for all categories */
    for (int i = 0; i < FEP_BRIDGE_CATEGORY_COUNT; i++) {
        float ci = orchestrator->config.categories[i].continuous_interval_ms;
        EXPECT_GT(ci, 0.0f);
        EXPECT_LE(ci, 1000.0f);
        EXPECT_GE(ci, 5.0f);
    }
}

TEST_F(FepOrchestratorTest, ContinuousSchedulingAffectsUpdateTiming) {
    /* Enable continuous scheduling with very short interval */
    orchestrator->config.continuous_schedule.enabled = true;
    orchestrator->config.continuous_schedule.min_interval_ms = 5.0f;
    orchestrator->config.continuous_schedule.max_interval_ms = 1000.0f;
    orchestrator->config.continuous_schedule.decay_rate = 10.0f;
    orchestrator->config.continuous_schedule.fe_scale = 0.0f;

    int mock_bridge = 42;
    fep_orchestrator_register_bridge(
        orchestrator, "test", FEP_BRIDGE_CATEGORY_PLASTICITY,
        &mock_bridge, mock_bridge_update, nullptr, nullptr
    );

    fep_orchestrator_start(orchestrator);

    /* Set high prediction error → very short continuous interval */
    fep_scheduling_metrics_t metrics = {
        .prediction_error = 5.0f,
        .free_energy = 0.0f,
        .surprise = 0.0f
    };
    fep_orchestrator_set_fep_metrics(orchestrator, &metrics);

    /* Plasticity normally has 1000ms interval, but continuous should make it ~5ms */
    float ci = orchestrator->config.categories[FEP_BRIDGE_CATEGORY_PLASTICITY].continuous_interval_ms;
    EXPECT_LT(ci, 100.0f);  /* Should be much less than fixed 1000ms */

    /* Update with small elapsed time (10ms) — should trigger with continuous
       scheduling even though fixed interval is 1000ms */
    uint64_t start = nimcp_platform_time_monotonic_ms();
    int updated = fep_orchestrator_update(orchestrator, start + 10);
    EXPECT_GE(updated, 1);
    EXPECT_EQ(g_mock_update_count, 1);
}

TEST_F(FepOrchestratorTest, ContinuousSchedulingFallsBackWhenDisabled) {
    /* Continuous scheduling disabled (default) — fixed intervals should apply */
    int mock_bridge = 42;
    fep_orchestrator_register_bridge(
        orchestrator, "test", FEP_BRIDGE_CATEGORY_PLASTICITY,
        &mock_bridge, mock_bridge_update, nullptr, nullptr
    );

    fep_orchestrator_start(orchestrator);

    /* Even with metrics set, fixed interval should be used since disabled */
    fep_scheduling_metrics_t metrics = {
        .prediction_error = 5.0f,
        .free_energy = 10.0f,
        .surprise = 0.0f
    };
    fep_orchestrator_set_fep_metrics(orchestrator, &metrics);

    /* Update with small elapsed time — fixed 1000ms should NOT trigger */
    uint64_t start = nimcp_platform_time_monotonic_ms();
    int updated = fep_orchestrator_update(orchestrator, start + 10);
    EXPECT_EQ(updated, 0);
    EXPECT_EQ(g_mock_update_count, 0);
}

TEST_F(FepOrchestratorTest, ContinuousSchedulingCalmSystemSlowsDown) {
    orchestrator->config.continuous_schedule.enabled = true;
    orchestrator->config.continuous_schedule.min_interval_ms = 5.0f;
    orchestrator->config.continuous_schedule.max_interval_ms = 2000.0f;
    orchestrator->config.continuous_schedule.decay_rate = 3.0f;
    orchestrator->config.continuous_schedule.fe_scale = 2.0f;

    /* Calm system: zero metrics → maximum interval */
    fep_scheduling_metrics_t calm = {
        .prediction_error = 0.0f,
        .free_energy = 0.0f,
        .surprise = 0.0f
    };
    fep_orchestrator_set_fep_metrics(orchestrator, &calm);

    float calm_interval = orchestrator->config.categories[FEP_BRIDGE_CATEGORY_COGNITIVE].continuous_interval_ms;
    EXPECT_FLOAT_EQ(calm_interval, 2000.0f);

    /* Urgent system: high metrics → short interval */
    fep_scheduling_metrics_t urgent = {
        .prediction_error = 2.0f,
        .free_energy = 5.0f,
        .surprise = 0.0f
    };
    fep_orchestrator_set_fep_metrics(orchestrator, &urgent);

    float urgent_interval = orchestrator->config.categories[FEP_BRIDGE_CATEGORY_COGNITIVE].continuous_interval_ms;
    EXPECT_LT(urgent_interval, calm_interval);
    EXPECT_GE(urgent_interval, 5.0f);  /* Not below min */
}

