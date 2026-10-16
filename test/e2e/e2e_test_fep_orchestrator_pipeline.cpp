/**
 * @file e2e_test_fep_orchestrator_pipeline.cpp
 * @brief End-to-end tests for FEP Orchestrator Pipeline
 * @version 1.0.0
 * @date 2025-12-15
 *
 * Tests full FEP orchestrator pipeline including:
 * - Multiple bridge types operating together
 * - Real FEP system integration
 * - Brain immune system integration
 * - Full processing cycles
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_time.h"

/* ============================================================================
 * Mock Bridges Simulating Real Module Behavior
 * ============================================================================ */

typedef struct {
    const char* name;
    fep_bridge_category_t category;
    float precision;
    float free_energy;
    int update_count;
    bool bio_async_connected;
} simulated_bridge_t;

static int simulated_bridge_update(void* bridge) {
    simulated_bridge_t* b = (simulated_bridge_t*)bridge;
    b->update_count++;
    
    /* Simulate FE minimization over time */
    b->free_energy = b->free_energy * 0.98f + 0.01f;
    
    /* Precision increases as FE decreases (inverse relationship) */
    b->precision = 1.0f / (1.0f + b->free_energy);
    
    return 0;
}

/* ============================================================================
 * E2E Test Fixture
 * ============================================================================ */

class FepOrchestratorE2ETest : public ::testing::Test {
protected:
    fep_orchestrator_t* orchestrator = nullptr;
    fep_system_t* fep = nullptr;
    brain_immune_system_t* immune = nullptr;
    
    static constexpr int NUM_BRIDGES = 20;
    simulated_bridge_t bridges[NUM_BRIDGES];
    
    void SetUp() override {
        /* Create FEP system */
        fep_config_t fep_cfg;
        fep_default_config(&fep_cfg);
        fep = fep_create(&fep_cfg, 64, 16);
        
        /* Create immune system */
        brain_immune_config_t immune_cfg;
        brain_immune_default_config(&immune_cfg);
        immune_cfg.enable_logging = false;
        immune = brain_immune_create(&immune_cfg);
        
        /* Create orchestrator */
        fep_orchestrator_config_t orch_cfg;
        fep_orchestrator_default_config(&orch_cfg);
        orch_cfg.enable_logging = false;
        orchestrator = fep_orchestrator_create(&orch_cfg);
        ASSERT_NE(orchestrator, nullptr);
        
        /* Connect integrations */
        if (immune) {
            fep_orchestrator_connect_brain_immune(orchestrator, immune);
        }
        fep_orchestrator_connect_bio_async(orchestrator);
        
        /* Initialize simulated bridges across categories */
        const char* category_names[] = {
            "attention", "memory", "reasoning", "executive",
            "swarm_brain", "consensus", "emergence", "flocking",
            "bbb_security", "anomaly", "rate_limit",
            "stdp", "bcm", "homeostatic",
            "router", "feature_extractor",
            "visual", "audio",
            "astrocyte", "oscillations"
        };
        
        fep_bridge_category_t categories[] = {
            FEP_BRIDGE_CATEGORY_COGNITIVE, FEP_BRIDGE_CATEGORY_COGNITIVE,
            FEP_BRIDGE_CATEGORY_COGNITIVE, FEP_BRIDGE_CATEGORY_COGNITIVE,
            FEP_BRIDGE_CATEGORY_SWARM, FEP_BRIDGE_CATEGORY_SWARM,
            FEP_BRIDGE_CATEGORY_SWARM, FEP_BRIDGE_CATEGORY_SWARM,
            FEP_BRIDGE_CATEGORY_SECURITY, FEP_BRIDGE_CATEGORY_SECURITY,
            FEP_BRIDGE_CATEGORY_SECURITY,
            FEP_BRIDGE_CATEGORY_PLASTICITY, FEP_BRIDGE_CATEGORY_PLASTICITY,
            FEP_BRIDGE_CATEGORY_PLASTICITY,
            FEP_BRIDGE_CATEGORY_MIDDLEWARE, FEP_BRIDGE_CATEGORY_MIDDLEWARE,
            FEP_BRIDGE_CATEGORY_PERCEPTION, FEP_BRIDGE_CATEGORY_PERCEPTION,
            FEP_BRIDGE_CATEGORY_GLIAL, FEP_BRIDGE_CATEGORY_CORE
        };
        
        for (int i = 0; i < NUM_BRIDGES; i++) {
            bridges[i].name = category_names[i];
            bridges[i].category = categories[i];
            bridges[i].precision = 0.5f;
            bridges[i].free_energy = 1.0f;
            bridges[i].update_count = 0;
            bridges[i].bio_async_connected = false;
            
            fep_orchestrator_register_bridge(
                orchestrator,
                bridges[i].name,
                bridges[i].category,
                &bridges[i],
                simulated_bridge_update,
                nullptr,
                nullptr
            );
        }
    }
    
    void TearDown() override {
        if (orchestrator) {
            fep_orchestrator_destroy(orchestrator);
        }
        if (immune) {
            brain_immune_destroy(immune);
        }
        if (fep) {
            fep_destroy(fep);
        }
    }
};

/* ============================================================================
 * E2E Pipeline Tests
 * ============================================================================ */

TEST_F(FepOrchestratorE2ETest, FullSystemStartup) {
    EXPECT_EQ(orchestrator->bridge_count, (uint32_t)NUM_BRIDGES);
    EXPECT_TRUE(orchestrator->immune_connected);
    
    int result = fep_orchestrator_start(orchestrator);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_RUNNING);
}

TEST_F(FepOrchestratorE2ETest, MultiCycleFreeEnergyMinimization) {
    fep_orchestrator_start(orchestrator);
    
    float initial_avg_fe = 0.0f;
    for (int i = 0; i < NUM_BRIDGES; i++) {
        initial_avg_fe += bridges[i].free_energy;
    }
    initial_avg_fe /= NUM_BRIDGES;
    
    /* Run 100 update cycles */
    for (int cycle = 0; cycle < 100; cycle++) {
        fep_orchestrator_force_update_all(orchestrator);
    }
    
    float final_avg_fe = 0.0f;
    for (int i = 0; i < NUM_BRIDGES; i++) {
        final_avg_fe += bridges[i].free_energy;
    }
    final_avg_fe /= NUM_BRIDGES;
    
    /* Free energy should have decreased (minimized) */
    EXPECT_LT(final_avg_fe, initial_avg_fe);
    
    /* All bridges should have been updated */
    for (int i = 0; i < NUM_BRIDGES; i++) {
        EXPECT_EQ(bridges[i].update_count, 100) << "Bridge: " << bridges[i].name;
    }
}

TEST_F(FepOrchestratorE2ETest, PrecisionIncreasesWithFEMinimization) {
    fep_orchestrator_start(orchestrator);
    
    float initial_avg_precision = 0.0f;
    for (int i = 0; i < NUM_BRIDGES; i++) {
        initial_avg_precision += bridges[i].precision;
    }
    initial_avg_precision /= NUM_BRIDGES;
    
    /* Run update cycles */
    for (int cycle = 0; cycle < 50; cycle++) {
        fep_orchestrator_force_update_all(orchestrator);
    }
    
    float final_avg_precision = 0.0f;
    for (int i = 0; i < NUM_BRIDGES; i++) {
        final_avg_precision += bridges[i].precision;
    }
    final_avg_precision /= NUM_BRIDGES;
    
    /* Precision should have increased */
    EXPECT_GT(final_avg_precision, initial_avg_precision);
}

TEST_F(FepOrchestratorE2ETest, IntervalBasedProcessing) {
    /* Set very different intervals for each category */
    fep_orchestrator_set_update_interval(orchestrator, FEP_BRIDGE_CATEGORY_COGNITIVE, 10);
    fep_orchestrator_set_update_interval(orchestrator, FEP_BRIDGE_CATEGORY_SWARM, 50);
    fep_orchestrator_set_update_interval(orchestrator, FEP_BRIDGE_CATEGORY_SECURITY, 20);
    fep_orchestrator_set_update_interval(orchestrator, FEP_BRIDGE_CATEGORY_PLASTICITY, 100);
    fep_orchestrator_set_update_interval(orchestrator, FEP_BRIDGE_CATEGORY_MIDDLEWARE, 30);
    fep_orchestrator_set_update_interval(orchestrator, FEP_BRIDGE_CATEGORY_PERCEPTION, 15);
    fep_orchestrator_set_update_interval(orchestrator, FEP_BRIDGE_CATEGORY_GLIAL, 80);
    fep_orchestrator_set_update_interval(orchestrator, FEP_BRIDGE_CATEGORY_CORE, 60);
    
    fep_orchestrator_start(orchestrator);
    
    uint64_t base = nimcp_platform_time_monotonic_ms();
    
    /* Simulate 500ms of operation with 10ms ticks */
    for (int tick = 0; tick <= 50; tick++) {
        fep_orchestrator_update(orchestrator, base + tick * 10);
    }
    
    /* Cognitive (10ms) should have most updates */
    int cognitive_updates = 0;
    int plasticity_updates = 0;
    for (int i = 0; i < NUM_BRIDGES; i++) {
        if (bridges[i].category == FEP_BRIDGE_CATEGORY_COGNITIVE) {
            cognitive_updates += bridges[i].update_count;
        }
        if (bridges[i].category == FEP_BRIDGE_CATEGORY_PLASTICITY) {
            plasticity_updates += bridges[i].update_count;
        }
    }
    
    /* Cognitive bridges update more frequently than plasticity */
    EXPECT_GT(cognitive_updates, plasticity_updates);
}

TEST_F(FepOrchestratorE2ETest, CategoryDisableStopsUpdates) {
    fep_orchestrator_start(orchestrator);
    
    /* Disable swarm category */
    fep_orchestrator_set_category_enabled(orchestrator, FEP_BRIDGE_CATEGORY_SWARM, false);
    
    /* Run updates */
    for (int cycle = 0; cycle < 10; cycle++) {
        fep_orchestrator_force_update_all(orchestrator);
    }
    
    /* Check swarm bridges were not updated */
    for (int i = 0; i < NUM_BRIDGES; i++) {
        if (bridges[i].category == FEP_BRIDGE_CATEGORY_SWARM) {
            EXPECT_EQ(bridges[i].update_count, 0) << "Swarm bridge updated: " << bridges[i].name;
        } else {
            EXPECT_GT(bridges[i].update_count, 0) << "Non-swarm bridge not updated: " << bridges[i].name;
        }
    }
}

TEST_F(FepOrchestratorE2ETest, StatisticsReflectOperation) {
    fep_orchestrator_start(orchestrator);
    
    for (int cycle = 0; cycle < 25; cycle++) {
        fep_orchestrator_force_update_all(orchestrator);
    }
    
    fep_orchestrator_stats_t stats;
    fep_orchestrator_get_stats(orchestrator, &stats);
    
    EXPECT_EQ(stats.total_bridges, (uint32_t)NUM_BRIDGES);
    EXPECT_EQ(stats.active_bridges, (uint32_t)NUM_BRIDGES);
    EXPECT_EQ(stats.total_bridge_updates, 25u * NUM_BRIDGES);
    EXPECT_GT(stats.avg_cycle_time_us, 0.0f);
    
    /* Check category distribution */
    uint32_t total_category_updates = 0;
    for (int cat = 0; cat < FEP_BRIDGE_CATEGORY_COUNT; cat++) {
        total_category_updates += stats.categories[cat].total_updates;
    }
    EXPECT_EQ(total_category_updates, 25u * NUM_BRIDGES);
}

TEST_F(FepOrchestratorE2ETest, GracefulShutdown) {
    fep_orchestrator_start(orchestrator);
    
    /* Run some cycles */
    for (int cycle = 0; cycle < 10; cycle++) {
        fep_orchestrator_force_update_all(orchestrator);
    }
    
    /* Stop */
    int result = fep_orchestrator_stop(orchestrator);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_STOPPED);
    
    /* Further updates should not process */
    int updates_before = bridges[0].update_count;
    uint64_t now = nimcp_platform_time_monotonic_ms() + 1000;
    fep_orchestrator_update(orchestrator, now);
    EXPECT_EQ(bridges[0].update_count, updates_before);
}

TEST_F(FepOrchestratorE2ETest, PauseResumePreservesState) {
    fep_orchestrator_start(orchestrator);
    
    /* Run some cycles */
    for (int cycle = 0; cycle < 5; cycle++) {
        fep_orchestrator_force_update_all(orchestrator);
    }
    
    float fe_before_pause = bridges[0].free_energy;
    int updates_before_pause = bridges[0].update_count;
    
    /* Pause */
    fep_orchestrator_pause(orchestrator);
    EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_PAUSED);
    
    /* Try to update (should not happen) */
    fep_orchestrator_force_update_all(orchestrator);
    EXPECT_EQ(bridges[0].free_energy, fe_before_pause);
    EXPECT_EQ(bridges[0].update_count, updates_before_pause);
    
    /* Resume and continue */
    fep_orchestrator_resume(orchestrator);
    fep_orchestrator_force_update_all(orchestrator);
    
    EXPECT_NE(bridges[0].free_energy, fe_before_pause);
    EXPECT_GT(bridges[0].update_count, updates_before_pause);
}

TEST_F(FepOrchestratorE2ETest, DynamicBridgeAdditionDuringOperation) {
    fep_orchestrator_start(orchestrator);
    
    /* Run initial cycles */
    for (int cycle = 0; cycle < 5; cycle++) {
        fep_orchestrator_force_update_all(orchestrator);
    }
    
    /* Add a new bridge */
    simulated_bridge_t new_bridge = {
        .name = "new_bridge",
        .category = FEP_BRIDGE_CATEGORY_COGNITIVE,
        .precision = 0.5f,
        .free_energy = 1.0f,
        .update_count = 0,
        .bio_async_connected = false
    };
    
    uint32_t new_id;
    fep_orchestrator_register_bridge(
        orchestrator, new_bridge.name, new_bridge.category,
        &new_bridge, simulated_bridge_update, nullptr, &new_id
    );
    
    EXPECT_EQ(orchestrator->bridge_count, (uint32_t)(NUM_BRIDGES + 1));
    
    /* Continue operation */
    for (int cycle = 0; cycle < 5; cycle++) {
        fep_orchestrator_force_update_all(orchestrator);
    }
    
    EXPECT_EQ(new_bridge.update_count, 5);
}

