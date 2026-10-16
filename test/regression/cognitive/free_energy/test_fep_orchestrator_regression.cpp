/**
 * @file test_fep_orchestrator_regression.cpp
 * @brief Regression tests for FEP Orchestrator
 * @version 1.0.0
 * @date 2025-12-15
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/free_energy/nimcp_fep_orchestrator.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_time.h"

static int noop_update(void* bridge) { (void)bridge; return 0; }

class FepOrchestratorRegressionTest : public ::testing::Test {
protected:
    fep_orchestrator_t* orchestrator = nullptr;
    
    void SetUp() override {
        fep_orchestrator_config_t config;
        fep_orchestrator_default_config(&config);
        config.enable_logging = false;
        orchestrator = fep_orchestrator_create(&config);
    }
    
    void TearDown() override {
        if (orchestrator) {
            fep_orchestrator_destroy(orchestrator);
        }
    }
};

/* REG-001: Bridge count remains consistent after multiple add/remove cycles */
TEST_F(FepOrchestratorRegressionTest, REG001_BridgeCountConsistency) {
    int bridges[100];
    uint32_t ids[100];
    
    for (int cycle = 0; cycle < 5; cycle++) {
        /* Add bridges */
        for (int i = 0; i < 100; i++) {
            bridges[i] = i;
            fep_orchestrator_register_bridge(
                orchestrator, "bridge", FEP_BRIDGE_CATEGORY_COGNITIVE,
                &bridges[i], noop_update, nullptr, &ids[i]
            );
        }
        EXPECT_EQ(orchestrator->bridge_count, 100u);
        
        /* Remove all bridges */
        for (int i = 0; i < 100; i++) {
            fep_orchestrator_unregister_bridge(orchestrator, ids[i]);
        }
        EXPECT_EQ(orchestrator->bridge_count, 0u);
    }
}

/* REG-002: Statistics don't overflow with many updates */
TEST_F(FepOrchestratorRegressionTest, REG002_StatisticsNoOverflow) {
    int bridge = 42;
    fep_orchestrator_register_bridge(
        orchestrator, "bridge", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &bridge, noop_update, nullptr, nullptr
    );
    
    fep_orchestrator_start(orchestrator);
    
    for (int i = 0; i < 10000; i++) {
        fep_orchestrator_force_update_all(orchestrator);
    }
    
    fep_orchestrator_stats_t stats;
    fep_orchestrator_get_stats(orchestrator, &stats);
    
    EXPECT_EQ(stats.total_bridge_updates, 10000u);
    EXPECT_GT(stats.avg_cycle_time_us, 0.0f);
    EXPECT_FALSE(std::isnan(stats.avg_cycle_time_us));
    EXPECT_FALSE(std::isinf(stats.avg_cycle_time_us));
}

/* REG-003: All categories can be updated independently */
TEST_F(FepOrchestratorRegressionTest, REG003_AllCategoriesWork) {
    int bridges[FEP_BRIDGE_CATEGORY_COUNT];
    
    for (int cat = 0; cat < FEP_BRIDGE_CATEGORY_COUNT; cat++) {
        bridges[cat] = cat;
        fep_orchestrator_register_bridge(
            orchestrator, "bridge", (fep_bridge_category_t)cat,
            &bridges[cat], noop_update, nullptr, nullptr
        );
    }
    
    fep_orchestrator_start(orchestrator);
    
    for (int cat = 0; cat < FEP_BRIDGE_CATEGORY_COUNT; cat++) {
        uint64_t now = nimcp_platform_time_monotonic_ms() + 100;
        int updated = fep_orchestrator_update_category(
            orchestrator, (fep_bridge_category_t)cat, now
        );
        EXPECT_EQ(updated, 1) << "Category " << cat << " failed";
    }
}

/* REG-004: Destroy after registering many bridges doesn't leak */
TEST_F(FepOrchestratorRegressionTest, REG004_DestroyCleanup) {
    int bridges[256];
    
    for (int i = 0; i < 256; i++) {
        bridges[i] = i;
        fep_orchestrator_register_bridge(
            orchestrator, "bridge", FEP_BRIDGE_CATEGORY_COGNITIVE,
            &bridges[i], noop_update, nullptr, nullptr
        );
    }
    
    /* Destroy should clean up all bridges */
    fep_orchestrator_destroy(orchestrator);
    orchestrator = nullptr;
    /* If this doesn't crash or leak, test passes */
}

/* REG-005: Category enable/disable state persists correctly */
TEST_F(FepOrchestratorRegressionTest, REG005_CategoryStatePersistence) {
    for (int cat = 0; cat < FEP_BRIDGE_CATEGORY_COUNT; cat++) {
        fep_orchestrator_set_category_enabled(
            orchestrator, (fep_bridge_category_t)cat, cat % 2 == 0
        );
    }
    
    for (int cat = 0; cat < FEP_BRIDGE_CATEGORY_COUNT; cat++) {
        fep_category_config_t cfg;
        fep_orchestrator_get_category_config(
            orchestrator, (fep_bridge_category_t)cat, &cfg
        );
        EXPECT_EQ(cfg.enabled, cat % 2 == 0) << "Category " << cat;
    }
}

/* REG-006: Rapid start/stop cycles don't corrupt state */
TEST_F(FepOrchestratorRegressionTest, REG006_RapidStartStop) {
    int bridge = 42;
    fep_orchestrator_register_bridge(
        orchestrator, "bridge", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &bridge, noop_update, nullptr, nullptr
    );
    
    for (int i = 0; i < 100; i++) {
        fep_orchestrator_start(orchestrator);
        EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_RUNNING);
        fep_orchestrator_stop(orchestrator);
        EXPECT_EQ(fep_orchestrator_get_state(orchestrator), FEP_ORCHESTRATOR_STOPPED);
    }
    
    EXPECT_EQ(orchestrator->bridge_count, 1u);
}

/* REG-007: Bridge IDs are unique across add/remove cycles */
TEST_F(FepOrchestratorRegressionTest, REG007_UniqueIDs) {
    std::set<uint32_t> seen_ids;
    
    for (int cycle = 0; cycle < 10; cycle++) {
        int bridges[10];
        uint32_t ids[10];
        
        for (int i = 0; i < 10; i++) {
            bridges[i] = i;
            fep_orchestrator_register_bridge(
                orchestrator, "bridge", FEP_BRIDGE_CATEGORY_COGNITIVE,
                &bridges[i], noop_update, nullptr, &ids[i]
            );
            
            EXPECT_EQ(seen_ids.count(ids[i]), 0u) << "Duplicate ID: " << ids[i];
            seen_ids.insert(ids[i]);
        }
        
        for (int i = 0; i < 10; i++) {
            fep_orchestrator_unregister_bridge(orchestrator, ids[i]);
        }
    }
}

/* REG-008: Update intervals are respected correctly */
TEST_F(FepOrchestratorRegressionTest, REG008_IntervalRespected) {
    static int update_count = 0;
    auto counting_update = [](void* b) -> int { 
        (void)b; 
        update_count++; 
        return 0; 
    };
    
    fep_orchestrator_set_update_interval(orchestrator, FEP_BRIDGE_CATEGORY_COGNITIVE, 100);
    
    int bridge = 42;
    update_count = 0;
    fep_orchestrator_register_bridge(
        orchestrator, "bridge", FEP_BRIDGE_CATEGORY_COGNITIVE,
        &bridge, counting_update, nullptr, nullptr
    );
    
    fep_orchestrator_start(orchestrator);
    
    uint64_t base = nimcp_platform_time_monotonic_ms();
    
    /* Update at intervals less than 100ms should not trigger */
    for (int t = 10; t < 100; t += 10) {
        fep_orchestrator_update(orchestrator, base + t);
    }
    EXPECT_EQ(update_count, 0);
    
    /* Update at 100ms should trigger */
    fep_orchestrator_update(orchestrator, base + 101);
    EXPECT_EQ(update_count, 1);
}

/* REG-009: Active bridge count matches enabled state */
TEST_F(FepOrchestratorRegressionTest, REG009_ActiveCountAccurate) {
    int bridges[10];
    uint32_t ids[10];
    
    for (int i = 0; i < 10; i++) {
        bridges[i] = i;
        fep_orchestrator_register_bridge(
            orchestrator, "bridge", FEP_BRIDGE_CATEGORY_COGNITIVE,
            &bridges[i], noop_update, nullptr, &ids[i]
        );
    }
    
    EXPECT_EQ(orchestrator->stats.active_bridges, 10u);
    
    /* Disable half */
    for (int i = 0; i < 5; i++) {
        fep_orchestrator_set_bridge_enabled(orchestrator, ids[i], false);
    }
    EXPECT_EQ(orchestrator->stats.active_bridges, 5u);
    
    /* Re-enable all */
    for (int i = 0; i < 5; i++) {
        fep_orchestrator_set_bridge_enabled(orchestrator, ids[i], true);
    }
    EXPECT_EQ(orchestrator->stats.active_bridges, 10u);
}

/* REG-010: Category bridge counts are accurate */
TEST_F(FepOrchestratorRegressionTest, REG010_CategoryCountsAccurate) {
    int bridges[FEP_BRIDGE_CATEGORY_COUNT * 3];
    
    for (int cat = 0; cat < FEP_BRIDGE_CATEGORY_COUNT; cat++) {
        for (int i = 0; i < 3; i++) {
            int idx = cat * 3 + i;
            bridges[idx] = idx;
            fep_orchestrator_register_bridge(
                orchestrator, "bridge", (fep_bridge_category_t)cat,
                &bridges[idx], noop_update, nullptr, nullptr
            );
        }
    }
    
    for (int cat = 0; cat < FEP_BRIDGE_CATEGORY_COUNT; cat++) {
        EXPECT_EQ(orchestrator->stats.categories[cat].bridge_count, 3u) 
            << "Category " << cat;
    }
}

