/**
 * @file test_fep_orchestrator_integration.cpp
 * @brief Integration tests for FEP Orchestrator
 * @version 1.0.0
 * @date 2025-12-15
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_time.h"

/* ============================================================================
 * Mock Bridge Implementations
 * ============================================================================ */

typedef struct {
    float precision;
    float free_energy;
    int update_count;
} mock_fep_bridge_t;

static int mock_cognitive_update(void* bridge) {
    mock_fep_bridge_t* b = (mock_fep_bridge_t*)bridge;
    b->update_count++;
    b->free_energy *= 0.95f;  /* Decrease FE over time */
    return 0;
}

static int mock_swarm_update(void* bridge) {
    mock_fep_bridge_t* b = (mock_fep_bridge_t*)bridge;
    b->update_count++;
    b->precision += 0.01f;  /* Increase precision */
    return 0;
}

/* ============================================================================
 * Integration Test Fixture
 * ============================================================================ */

class FepOrchestratorIntegrationTest : public ::testing::Test {
protected:
    fep_orchestrator_t* orchestrator = nullptr;
    brain_immune_system_t* immune = nullptr;
    mock_fep_bridge_t cognitive_bridge;
    mock_fep_bridge_t swarm_bridge;
    
    void SetUp() override {
        fep_orchestrator_config_t config;
        fep_orchestrator_default_config(&config);
        config.enable_logging = false;
        orchestrator = fep_orchestrator_create(&config);
        ASSERT_NE(orchestrator, nullptr);
        
        /* Create immune system */
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune_cfg.enable_logging = false;
        immune = brain_immune_create(&immune_cfg);
        
        /* Initialize mock bridges */
        cognitive_bridge = {.precision = 1.0f, .free_energy = 1.0f, .update_count = 0};
        swarm_bridge = {.precision = 0.5f, .free_energy = 0.8f, .update_count = 0};
    }
    
    void TearDown() override {
        if (orchestrator) {
            fep_orchestrator_destroy(orchestrator);
            orchestrator = nullptr;
        }
        if (immune) {
            brain_immune_destroy(immune);
            immune = nullptr;
        }
    }
};

/* ============================================================================
 * Multi-Bridge Coordination Tests
 * ============================================================================ */

TEST_F(FepOrchestratorIntegrationTest, MultipleBridgesUpdateInOrder) {
    uint32_t cog_id, swarm_id;
    
    fep_orchestrator_register_bridge(
        orchestrator, "cognitive_bridge", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &cognitive_bridge, mock_cognitive_update, nullptr, &cog_id
    );
    fep_orchestrator_register_bridge(
        orchestrator, "swarm_bridge", FEP_BRIDGE_CATEGORY_SWARM,
        &swarm_bridge, mock_swarm_update, nullptr, &swarm_id
    );
    
    fep_orchestrator_start(orchestrator);
    
    /* Force update all */
    int updated = fep_orchestrator_force_update_all(orchestrator);
    
    EXPECT_EQ(updated, 2);
    EXPECT_EQ(cognitive_bridge.update_count, 1);
    EXPECT_EQ(swarm_bridge.update_count, 1);
    EXPECT_LT(cognitive_bridge.free_energy, 1.0f);  /* FE decreased */
    EXPECT_GT(swarm_bridge.precision, 0.5f);  /* Precision increased */
}

TEST_F(FepOrchestratorIntegrationTest, CategorySpecificUpdates) {
    fep_orchestrator_register_bridge(
        orchestrator, "cognitive", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &cognitive_bridge, mock_cognitive_update, nullptr, nullptr
    );
    fep_orchestrator_register_bridge(
        orchestrator, "swarm", FEP_BRIDGE_CATEGORY_SWARM,
        &swarm_bridge, mock_swarm_update, nullptr, nullptr
    );
    
    fep_orchestrator_start(orchestrator);
    
    uint64_t now = nimcp_platform_time_monotonic_ms() + 100;
    
    /* Update only cognitive */
    fep_orchestrator_update_category(orchestrator, FEP_BRIDGE_CATEGORY_COGNITIVE, now);
    
    EXPECT_EQ(cognitive_bridge.update_count, 1);
    EXPECT_EQ(swarm_bridge.update_count, 0);
    
    /* Update only swarm */
    fep_orchestrator_update_category(orchestrator, FEP_BRIDGE_CATEGORY_SWARM, now + 50);
    
    EXPECT_EQ(cognitive_bridge.update_count, 1);
    EXPECT_EQ(swarm_bridge.update_count, 1);
}

TEST_F(FepOrchestratorIntegrationTest, IntervalBasedScheduling) {
    /* Set different intervals */
    fep_orchestrator_set_update_interval(orchestrator, FEP_BRIDGE_CATEGORY_COGNITIVE, 10);
    fep_orchestrator_set_update_interval(orchestrator, FEP_BRIDGE_CATEGORY_SWARM, 50);
    
    fep_orchestrator_register_bridge(
        orchestrator, "cognitive", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &cognitive_bridge, mock_cognitive_update, nullptr, nullptr
    );
    fep_orchestrator_register_bridge(
        orchestrator, "swarm", FEP_BRIDGE_CATEGORY_SWARM,
        &swarm_bridge, mock_swarm_update, nullptr, nullptr
    );
    
    fep_orchestrator_start(orchestrator);
    
    uint64_t base = nimcp_platform_time_monotonic_ms();
    
    /* At t=15, cognitive should update but not swarm */
    fep_orchestrator_update(orchestrator, base + 15);
    EXPECT_EQ(cognitive_bridge.update_count, 1);
    EXPECT_EQ(swarm_bridge.update_count, 0);
    
    /* At t=55, both should have updated */
    fep_orchestrator_update(orchestrator, base + 55);
    EXPECT_GE(cognitive_bridge.update_count, 2);
    EXPECT_EQ(swarm_bridge.update_count, 1);
}

/* ============================================================================
 * Brain Immune Integration Tests
 * ============================================================================ */

TEST_F(FepOrchestratorIntegrationTest, ImmuneSystemConnection) {
    ASSERT_NE(immune, nullptr);
    
    int result = fep_orchestrator_connect_brain_immune(orchestrator, immune);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(orchestrator->immune_connected);
    
    result = fep_orchestrator_disconnect_brain_immune(orchestrator);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(orchestrator->immune_connected);
}

/* ============================================================================
 * Statistics Tracking Tests
 * ============================================================================ */

TEST_F(FepOrchestratorIntegrationTest, StatisticsAccumulateCorrectly) {
    mock_fep_bridge_t bridges[10];
    
    for (int i = 0; i < 10; i++) {
        bridges[i] = {.precision = 1.0f, .free_energy = 1.0f, .update_count = 0};
        fep_orchestrator_register_bridge(
            orchestrator, "bridge", FEP_BRIDGE_CATEGORY_COGNITIVE,
            &bridges[i], mock_cognitive_update, nullptr, nullptr
        );
    }
    
    fep_orchestrator_start(orchestrator);
    
    /* Perform multiple update cycles */
    uint64_t base = nimcp_platform_time_monotonic_ms();
    for (int cycle = 0; cycle < 5; cycle++) {
        fep_orchestrator_force_update_all(orchestrator);
    }
    
    fep_orchestrator_stats_t stats;
    fep_orchestrator_get_stats(orchestrator, &stats);
    
    EXPECT_EQ(stats.total_bridges, 10u);
    EXPECT_EQ(stats.active_bridges, 10u);
    EXPECT_EQ(stats.total_bridge_updates, 50u);  /* 10 bridges * 5 cycles */
    EXPECT_EQ(stats.categories[FEP_BRIDGE_CATEGORY_COGNITIVE].total_updates, 50u);
}

TEST_F(FepOrchestratorIntegrationTest, UpdateTimingTracked) {
    fep_orchestrator_register_bridge(
        orchestrator, "bridge", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &cognitive_bridge, mock_cognitive_update, nullptr, nullptr
    );
    
    fep_orchestrator_start(orchestrator);
    
    for (int i = 0; i < 10; i++) {
        fep_orchestrator_force_update_all(orchestrator);
    }
    
    fep_orchestrator_stats_t stats;
    fep_orchestrator_get_stats(orchestrator, &stats);
    
    EXPECT_GT(stats.avg_cycle_time_us, 0.0f);
    EXPECT_GE(stats.max_cycle_time_us, stats.avg_cycle_time_us);
}

/* ============================================================================
 * Dynamic Reconfiguration Tests
 * ============================================================================ */

TEST_F(FepOrchestratorIntegrationTest, DynamicBridgeAddRemove) {
    fep_orchestrator_start(orchestrator);
    
    /* Add bridges dynamically */
    uint32_t id1, id2;
    fep_orchestrator_register_bridge(
        orchestrator, "bridge1", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &cognitive_bridge, mock_cognitive_update, nullptr, &id1
    );
    
    EXPECT_EQ(orchestrator->bridge_count, 1u);
    
    fep_orchestrator_register_bridge(
        orchestrator, "bridge2", FEP_BRIDGE_CATEGORY_SWARM,
        &swarm_bridge, mock_swarm_update, nullptr, &id2
    );
    
    EXPECT_EQ(orchestrator->bridge_count, 2u);
    
    fep_orchestrator_force_update_all(orchestrator);
    EXPECT_EQ(cognitive_bridge.update_count, 1);
    EXPECT_EQ(swarm_bridge.update_count, 1);
    
    /* Remove first bridge */
    fep_orchestrator_unregister_bridge(orchestrator, id1);
    EXPECT_EQ(orchestrator->bridge_count, 1u);
    
    fep_orchestrator_force_update_all(orchestrator);
    EXPECT_EQ(cognitive_bridge.update_count, 1);  /* Not updated */
    EXPECT_EQ(swarm_bridge.update_count, 2);  /* Updated */
}

TEST_F(FepOrchestratorIntegrationTest, EnableDisableBridgesAtRuntime) {
    uint32_t id;
    fep_orchestrator_register_bridge(
        orchestrator, "bridge", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &cognitive_bridge, mock_cognitive_update, nullptr, &id
    );
    
    fep_orchestrator_start(orchestrator);
    
    /* Update while enabled */
    fep_orchestrator_force_update_all(orchestrator);
    EXPECT_EQ(cognitive_bridge.update_count, 1);
    
    /* Disable and update */
    fep_orchestrator_set_bridge_enabled(orchestrator, id, false);
    fep_orchestrator_force_update_all(orchestrator);
    EXPECT_EQ(cognitive_bridge.update_count, 1);  /* Not updated */
    
    /* Re-enable and update */
    fep_orchestrator_set_bridge_enabled(orchestrator, id, true);
    fep_orchestrator_force_update_all(orchestrator);
    EXPECT_EQ(cognitive_bridge.update_count, 2);
}

/* ============================================================================
 * Pause/Resume Behavior Tests
 * ============================================================================ */

TEST_F(FepOrchestratorIntegrationTest, PauseStopsUpdates) {
    fep_orchestrator_register_bridge(
        orchestrator, "bridge", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &cognitive_bridge, mock_cognitive_update, nullptr, nullptr
    );
    
    fep_orchestrator_start(orchestrator);
    fep_orchestrator_force_update_all(orchestrator);
    EXPECT_EQ(cognitive_bridge.update_count, 1);
    
    fep_orchestrator_pause(orchestrator);
    
    /* Updates should not happen while paused */
    uint64_t now = nimcp_platform_time_monotonic_ms() + 1000;
    fep_orchestrator_update(orchestrator, now);
    EXPECT_EQ(cognitive_bridge.update_count, 1);
    
    /* Resume and update */
    fep_orchestrator_resume(orchestrator);
    fep_orchestrator_force_update_all(orchestrator);
    EXPECT_EQ(cognitive_bridge.update_count, 2);
}

