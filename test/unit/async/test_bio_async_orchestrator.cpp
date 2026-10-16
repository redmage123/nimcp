/**
 * @file test_bio_async_orchestrator.cpp
 * @brief Comprehensive unit tests for Bio-Async Orchestrator
 *
 * Test Categories:
 * 1. Lifecycle Tests - Create, destroy, start, stop
 * 2. Configuration Tests - Default config, custom config
 * 3. Module Registration Tests - Register, unregister, enable/disable
 * 4. Dependency Tracking Tests - Add dependencies, validate ordering
 * 5. Startup Sequencing Tests - Phased startup, dependency ordering
 * 6. Health Monitoring Tests - Health checks, status tracking
 * 7. Discovery Tests - Query modules, category filtering
 * 8. Statistics Tests - Tracking, reset, health scores
 * 9. Integration Tests - Bio-async, brain immune connections
 * 10. Thread Safety Tests - Concurrent operations
 * 11. Edge Cases - Null pointers, capacity limits, duplicate registrations
 *
 * @author NIMCP Development Team
 * @date 2025-12-15
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_async_orchestrator.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

class BioAsyncOrchestratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize bio-async system for orchestrator integration
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = false;
        bio_config.enable_logging = false;  // Reduce noise in tests
        nimcp_bio_async_init(&bio_config);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = false;
        router_config.enable_logging = false;
        bio_router_init(&router_config);
    }

    void TearDown() override {
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }

    // Helper: Create test module context
    bio_module_context_t CreateTestModule(bio_module_id_t id, const char* name) {
        bio_module_info_t info;
        info.module_id = id;
        info.module_name = name;
        info.inbox_capacity = 16;
        info.user_data = nullptr;
        return bio_router_register_module(&info);
    }

    // Helper: Cleanup test module
    void CleanupTestModule(bio_module_context_t ctx) {
        if (ctx) {
            bio_router_unregister_module(ctx);
        }
    }
};

//=============================================================================
// 1. LIFECYCLE TESTS
//=============================================================================

TEST_F(BioAsyncOrchestratorTest, CreateDestroy) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    // Verify initial state
    EXPECT_EQ(bio_orchestrator_get_state(orch), BIO_ORCHESTRATOR_STOPPED);

    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, CreateWithConfig) {
    bio_orchestrator_config_t config;
    ASSERT_EQ(bio_orchestrator_default_config(&config), 0);

    // Customize config
    config.max_modules = 128;
    config.enable_auto_health_check = false;
    config.global_health_check_ms = 10000;

    bio_async_orchestrator_t* orch = bio_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, DestroyNullSafe) {
    bio_orchestrator_destroy(nullptr);  // Should not crash
}

TEST_F(BioAsyncOrchestratorTest, StartStop) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    // Start
    EXPECT_EQ(bio_orchestrator_start(orch), 0);
    EXPECT_EQ(bio_orchestrator_get_state(orch), BIO_ORCHESTRATOR_RUNNING);

    // Stop
    EXPECT_EQ(bio_orchestrator_stop(orch), 0);
    EXPECT_EQ(bio_orchestrator_get_state(orch), BIO_ORCHESTRATOR_STOPPED);

    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, StartAlreadyRunning) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    EXPECT_EQ(bio_orchestrator_start(orch), 0);
    EXPECT_EQ(bio_orchestrator_start(orch), 0);  // Should be idempotent

    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, StopAlreadyStopped) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    EXPECT_EQ(bio_orchestrator_stop(orch), 0);  // Should be idempotent

    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, DestroyWhileRunning) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    EXPECT_EQ(bio_orchestrator_start(orch), 0);
    bio_orchestrator_destroy(orch);  // Should stop automatically
}

//=============================================================================
// 2. CONFIGURATION TESTS
//=============================================================================

TEST_F(BioAsyncOrchestratorTest, DefaultConfigValid) {
    bio_orchestrator_config_t config;
    ASSERT_EQ(bio_orchestrator_default_config(&config), 0);

    // Verify defaults
    EXPECT_EQ(config.max_modules, BIO_ORCHESTRATOR_MAX_MODULES);
    EXPECT_TRUE(config.enable_auto_health_check);
    EXPECT_EQ(config.global_health_check_ms, BIO_ORCHESTRATOR_HEALTH_CHECK_MS);
    EXPECT_TRUE(config.enable_bio_async);
    EXPECT_FALSE(config.enable_brain_immune);
    EXPECT_TRUE(config.enable_statistics);
    EXPECT_TRUE(config.enable_logging);
    EXPECT_TRUE(config.enforce_startup_order);
    EXPECT_GT(config.startup_timeout_ms, 0u);

    // Verify category configs
    for (int i = 0; i < BIO_MODULE_CATEGORY_COUNT; i++) {
        EXPECT_TRUE(config.categories[i].enabled);
        EXPECT_EQ(config.categories[i].health_check_interval_ms, BIO_ORCHESTRATOR_HEALTH_CHECK_MS);
        EXPECT_EQ(config.categories[i].max_health_failures, 3u);
    }
}

TEST_F(BioAsyncOrchestratorTest, DefaultConfigNullPointer) {
    EXPECT_EQ(bio_orchestrator_default_config(nullptr), -1);
}

TEST_F(BioAsyncOrchestratorTest, CustomCategoryConfig) {
    bio_orchestrator_config_t config;
    ASSERT_EQ(bio_orchestrator_default_config(&config), 0);

    // Customize specific categories
    config.categories[BIO_MODULE_CATEGORY_CORE].health_check_interval_ms = 1000;
    config.categories[BIO_MODULE_CATEGORY_IMMUNE].max_health_failures = 5;
    config.categories[BIO_MODULE_CATEGORY_SWARM].enabled = false;

    bio_async_orchestrator_t* orch = bio_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    bio_orchestrator_destroy(orch);
}

//=============================================================================
// 3. MODULE REGISTRATION TESTS
//=============================================================================

TEST_F(BioAsyncOrchestratorTest, RegisterModuleBasic) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx = CreateTestModule(BIO_MODULE_BRAIN, "test_brain");
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch,
        BIO_MODULE_BRAIN,
        "test_brain",
        BIO_MODULE_CATEGORY_CORE,
        ctx,
        BIO_STARTUP_PHASE_CORE
    ), 0);

    // Verify registration
    EXPECT_TRUE(bio_orchestrator_is_module_registered(orch, BIO_MODULE_BRAIN));

    CleanupTestModule(ctx);
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, RegisterMultipleModules) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    // Register multiple modules from different categories
    bio_module_id_t ids[] = {
        BIO_MODULE_BRAIN,
        BIO_MODULE_STDP,
        BIO_MODULE_VISUAL_CORTEX,
        BIO_MODULE_ATTENTION,
        BIO_MODULE_INTROSPECTION
    };
    bio_module_category_t categories[] = {
        BIO_MODULE_CATEGORY_CORE,
        BIO_MODULE_CATEGORY_PLASTICITY,
        BIO_MODULE_CATEGORY_PERCEPTION,
        BIO_MODULE_CATEGORY_COGNITIVE,
        BIO_MODULE_CATEGORY_HIGHLEVEL
    };
    const char* names[] = {
        "brain", "stdp", "visual", "attention", "introspection"
    };

    std::vector<bio_module_context_t> contexts;

    for (size_t i = 0; i < 5; i++) {
        bio_module_context_t ctx = CreateTestModule(ids[i], names[i]);
        ASSERT_NE(ctx, nullptr);
        contexts.push_back(ctx);

        EXPECT_EQ(bio_orchestrator_register_module(
            orch, ids[i], names[i], categories[i], ctx, i
        ), 0);
    }

    // Verify all registered
    for (size_t i = 0; i < 5; i++) {
        EXPECT_TRUE(bio_orchestrator_is_module_registered(orch, ids[i]));
    }

    // Get stats
    bio_orchestrator_stats_t stats;
    ASSERT_EQ(bio_orchestrator_get_stats(orch, &stats), 0);
    EXPECT_EQ(stats.total_modules, 5u);

    for (auto ctx : contexts) {
        CleanupTestModule(ctx);
    }
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, UnregisterModule) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx = CreateTestModule(BIO_MODULE_BRAIN, "test_brain");
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "test_brain",
        BIO_MODULE_CATEGORY_CORE, ctx, BIO_STARTUP_PHASE_CORE
    ), 0);

    EXPECT_TRUE(bio_orchestrator_is_module_registered(orch, BIO_MODULE_BRAIN));

    // Unregister
    EXPECT_EQ(bio_orchestrator_unregister_module(orch, BIO_MODULE_BRAIN), 0);
    EXPECT_FALSE(bio_orchestrator_is_module_registered(orch, BIO_MODULE_BRAIN));

    CleanupTestModule(ctx);
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, UnregisterNonexistent) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    EXPECT_EQ(bio_orchestrator_unregister_module(orch, BIO_MODULE_BRAIN), -1);

    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, RegisterDuplicateModule) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx = CreateTestModule(BIO_MODULE_BRAIN, "test_brain");
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "test_brain",
        BIO_MODULE_CATEGORY_CORE, ctx, BIO_STARTUP_PHASE_CORE
    ), 0);

    // Duplicate registration should succeed (not an error)
    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "test_brain",
        BIO_MODULE_CATEGORY_CORE, ctx, BIO_STARTUP_PHASE_CORE
    ), 0);

    CleanupTestModule(ctx);
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, SetModuleEnabled) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx = CreateTestModule(BIO_MODULE_BRAIN, "test_brain");
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "test_brain",
        BIO_MODULE_CATEGORY_CORE, ctx, BIO_STARTUP_PHASE_CORE
    ), 0);

    // Disable module
    EXPECT_EQ(bio_orchestrator_set_module_enabled(orch, BIO_MODULE_BRAIN, false), 0);

    // Re-enable
    EXPECT_EQ(bio_orchestrator_set_module_enabled(orch, BIO_MODULE_BRAIN, true), 0);

    CleanupTestModule(ctx);
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, RegisterNullOrchestratorFails) {
    EXPECT_EQ(bio_orchestrator_register_module(
        nullptr, BIO_MODULE_BRAIN, "test", BIO_MODULE_CATEGORY_CORE, nullptr, 0
    ), -1);
}

TEST_F(BioAsyncOrchestratorTest, RegisterNullNameFails) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, nullptr, BIO_MODULE_CATEGORY_CORE, nullptr, 0
    ), -1);

    bio_orchestrator_destroy(orch);
}

//=============================================================================
// 4. DEPENDENCY TRACKING TESTS
//=============================================================================

TEST_F(BioAsyncOrchestratorTest, AddDependency) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx1 = CreateTestModule(BIO_MODULE_BRAIN, "brain");
    bio_module_context_t ctx2 = CreateTestModule(BIO_MODULE_ATTENTION, "attention");
    ASSERT_NE(ctx1, nullptr);
    ASSERT_NE(ctx2, nullptr);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx1, BIO_STARTUP_PHASE_CORE
    ), 0);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_ATTENTION, "attention",
        BIO_MODULE_CATEGORY_COGNITIVE, ctx2, BIO_STARTUP_PHASE_COGNITIVE
    ), 0);

    // Add dependency: attention depends on brain
    EXPECT_EQ(bio_orchestrator_add_dependency(
        orch, BIO_MODULE_ATTENTION, BIO_MODULE_BRAIN
    ), 0);

    // Verify via module info
    const bio_module_entry_t* entry = bio_orchestrator_get_module_info(
        orch, BIO_MODULE_ATTENTION
    );
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->dependency_count, 1u);
    EXPECT_EQ(entry->dependencies[0], BIO_MODULE_BRAIN);

    CleanupTestModule(ctx1);
    CleanupTestModule(ctx2);
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, AddMultipleDependencies) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx1 = CreateTestModule(BIO_MODULE_BRAIN, "brain");
    bio_module_context_t ctx2 = CreateTestModule(BIO_MODULE_STDP, "stdp");
    bio_module_context_t ctx3 = CreateTestModule(BIO_MODULE_ATTENTION, "attention");

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx1, BIO_STARTUP_PHASE_CORE
    ), 0);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_STDP, "stdp",
        BIO_MODULE_CATEGORY_PLASTICITY, ctx2, BIO_STARTUP_PHASE_PLASTICITY
    ), 0);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_ATTENTION, "attention",
        BIO_MODULE_CATEGORY_COGNITIVE, ctx3, BIO_STARTUP_PHASE_COGNITIVE
    ), 0);

    // Attention depends on both brain and stdp
    EXPECT_EQ(bio_orchestrator_add_dependency(
        orch, BIO_MODULE_ATTENTION, BIO_MODULE_BRAIN
    ), 0);
    EXPECT_EQ(bio_orchestrator_add_dependency(
        orch, BIO_MODULE_ATTENTION, BIO_MODULE_STDP
    ), 0);

    const bio_module_entry_t* entry = bio_orchestrator_get_module_info(
        orch, BIO_MODULE_ATTENTION
    );
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->dependency_count, 2u);

    CleanupTestModule(ctx1);
    CleanupTestModule(ctx2);
    CleanupTestModule(ctx3);
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, AddDependencyNonexistentModule) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    EXPECT_EQ(bio_orchestrator_add_dependency(
        orch, BIO_MODULE_BRAIN, BIO_MODULE_ATTENTION
    ), -1);

    bio_orchestrator_destroy(orch);
}

//=============================================================================
// 5. STARTUP SEQUENCING TESTS
//=============================================================================

TEST_F(BioAsyncOrchestratorTest, ExecuteStartup) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    // Register modules in different phases
    bio_module_context_t ctx1 = CreateTestModule(BIO_MODULE_BRAIN, "brain");
    bio_module_context_t ctx2 = CreateTestModule(BIO_MODULE_STDP, "stdp");

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx1, BIO_STARTUP_PHASE_CORE
    ), 0);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_STDP, "stdp",
        BIO_MODULE_CATEGORY_PLASTICITY, ctx2, BIO_STARTUP_PHASE_PLASTICITY
    ), 0);

    EXPECT_EQ(bio_orchestrator_execute_startup(orch), 0);

    CleanupTestModule(ctx1);
    CleanupTestModule(ctx2);
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, GetPhaseModules) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    // Register modules in phase 0 (CORE)
    bio_module_context_t ctx1 = CreateTestModule(BIO_MODULE_BRAIN, "brain");
    bio_module_context_t ctx2 = CreateTestModule(BIO_MODULE_SYNAPSE, "synapse");

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx1, BIO_STARTUP_PHASE_CORE
    ), 0);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_SYNAPSE, "synapse",
        BIO_MODULE_CATEGORY_CORE, ctx2, BIO_STARTUP_PHASE_CORE
    ), 0);

    // Get phase 0 modules
    bio_module_id_t module_ids[10];
    uint32_t count = bio_orchestrator_get_phase_modules(
        orch, BIO_STARTUP_PHASE_CORE, module_ids, 10
    );

    EXPECT_EQ(count, 2u);
    EXPECT_TRUE(module_ids[0] == BIO_MODULE_BRAIN || module_ids[1] == BIO_MODULE_BRAIN);
    EXPECT_TRUE(module_ids[0] == BIO_MODULE_SYNAPSE || module_ids[1] == BIO_MODULE_SYNAPSE);

    CleanupTestModule(ctx1);
    CleanupTestModule(ctx2);
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, GetPhaseModulesEmpty) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_id_t module_ids[10];
    uint32_t count = bio_orchestrator_get_phase_modules(
        orch, BIO_STARTUP_PHASE_IMMUNE, module_ids, 10
    );

    EXPECT_EQ(count, 0u);

    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, GetPhaseModulesInvalidPhase) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_id_t module_ids[10];
    uint32_t count = bio_orchestrator_get_phase_modules(
        orch, 999, module_ids, 10
    );

    EXPECT_EQ(count, 0u);

    bio_orchestrator_destroy(orch);
}

//=============================================================================
// 6. HEALTH MONITORING TESTS
//=============================================================================

TEST_F(BioAsyncOrchestratorTest, HealthCheckAll) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx1 = CreateTestModule(BIO_MODULE_BRAIN, "brain");
    bio_module_context_t ctx2 = CreateTestModule(BIO_MODULE_STDP, "stdp");

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx1, BIO_STARTUP_PHASE_CORE
    ), 0);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_STDP, "stdp",
        BIO_MODULE_CATEGORY_PLASTICITY, ctx2, BIO_STARTUP_PHASE_PLASTICITY
    ), 0);

    int healthy = bio_orchestrator_health_check_all(orch);
    EXPECT_EQ(healthy, 2);

    CleanupTestModule(ctx1);
    CleanupTestModule(ctx2);
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, HealthCheckModule) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx = CreateTestModule(BIO_MODULE_BRAIN, "brain");

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx, BIO_STARTUP_PHASE_CORE
    ), 0);

    bio_module_health_t health;
    EXPECT_EQ(bio_orchestrator_health_check_module(orch, BIO_MODULE_BRAIN, &health), 0);
    EXPECT_EQ(health, BIO_MODULE_HEALTH_HEALTHY);

    CleanupTestModule(ctx);
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, GetModuleHealth) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx = CreateTestModule(BIO_MODULE_BRAIN, "brain");

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx, BIO_STARTUP_PHASE_CORE
    ), 0);

    // Initial health should be UNKNOWN
    bio_module_health_t health = bio_orchestrator_get_module_health(orch, BIO_MODULE_BRAIN);
    EXPECT_EQ(health, BIO_MODULE_HEALTH_UNKNOWN);

    // After health check
    bio_orchestrator_health_check_module(orch, BIO_MODULE_BRAIN, nullptr);
    health = bio_orchestrator_get_module_health(orch, BIO_MODULE_BRAIN);
    EXPECT_EQ(health, BIO_MODULE_HEALTH_HEALTHY);

    CleanupTestModule(ctx);
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, GetModuleHealthNonexistent) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_health_t health = bio_orchestrator_get_module_health(orch, BIO_MODULE_BRAIN);
    EXPECT_EQ(health, BIO_MODULE_HEALTH_UNKNOWN);

    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, HealthCheckDisabledModule) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx = CreateTestModule(BIO_MODULE_BRAIN, "brain");

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx, BIO_STARTUP_PHASE_CORE
    ), 0);

    // Disable module
    EXPECT_EQ(bio_orchestrator_set_module_enabled(orch, BIO_MODULE_BRAIN, false), 0);

    // Health check should skip disabled modules
    int healthy = bio_orchestrator_health_check_all(orch);
    EXPECT_EQ(healthy, 0);

    CleanupTestModule(ctx);
    bio_orchestrator_destroy(orch);
}

//=============================================================================
// 7. DISCOVERY TESTS
//=============================================================================

TEST_F(BioAsyncOrchestratorTest, GetAllModules) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx1 = CreateTestModule(BIO_MODULE_BRAIN, "brain");
    bio_module_context_t ctx2 = CreateTestModule(BIO_MODULE_STDP, "stdp");
    bio_module_context_t ctx3 = CreateTestModule(BIO_MODULE_ATTENTION, "attention");

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx1, BIO_STARTUP_PHASE_CORE
    ), 0);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_STDP, "stdp",
        BIO_MODULE_CATEGORY_PLASTICITY, ctx2, BIO_STARTUP_PHASE_PLASTICITY
    ), 0);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_ATTENTION, "attention",
        BIO_MODULE_CATEGORY_COGNITIVE, ctx3, BIO_STARTUP_PHASE_COGNITIVE
    ), 0);

    bio_module_id_t module_ids[10];
    uint32_t count = bio_orchestrator_get_all_modules(orch, module_ids, 10);

    EXPECT_EQ(count, 3u);

    CleanupTestModule(ctx1);
    CleanupTestModule(ctx2);
    CleanupTestModule(ctx3);
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, GetModulesByCategory) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx1 = CreateTestModule(BIO_MODULE_BRAIN, "brain");
    bio_module_context_t ctx2 = CreateTestModule(BIO_MODULE_SYNAPSE, "synapse");
    bio_module_context_t ctx3 = CreateTestModule(BIO_MODULE_STDP, "stdp");

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx1, BIO_STARTUP_PHASE_CORE
    ), 0);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_SYNAPSE, "synapse",
        BIO_MODULE_CATEGORY_CORE, ctx2, BIO_STARTUP_PHASE_CORE
    ), 0);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_STDP, "stdp",
        BIO_MODULE_CATEGORY_PLASTICITY, ctx3, BIO_STARTUP_PHASE_PLASTICITY
    ), 0);

    bio_module_id_t module_ids[10];
    uint32_t count = bio_orchestrator_get_modules_by_category(
        orch, BIO_MODULE_CATEGORY_CORE, module_ids, 10
    );

    EXPECT_EQ(count, 2u);

    count = bio_orchestrator_get_modules_by_category(
        orch, BIO_MODULE_CATEGORY_PLASTICITY, module_ids, 10
    );

    EXPECT_EQ(count, 1u);
    EXPECT_EQ(module_ids[0], BIO_MODULE_STDP);

    CleanupTestModule(ctx1);
    CleanupTestModule(ctx2);
    CleanupTestModule(ctx3);
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, GetModuleInfo) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx = CreateTestModule(BIO_MODULE_BRAIN, "brain");

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx, BIO_STARTUP_PHASE_CORE
    ), 0);

    const bio_module_entry_t* entry = bio_orchestrator_get_module_info(
        orch, BIO_MODULE_BRAIN
    );

    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->module_id, BIO_MODULE_BRAIN);
    EXPECT_STREQ(entry->module_name, "brain");
    EXPECT_EQ(entry->category, BIO_MODULE_CATEGORY_CORE);
    EXPECT_EQ(entry->startup_phase, BIO_STARTUP_PHASE_CORE);
    EXPECT_TRUE(entry->registered);
    EXPECT_TRUE(entry->enabled);

    CleanupTestModule(ctx);
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, GetModuleInfoNonexistent) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    const bio_module_entry_t* entry = bio_orchestrator_get_module_info(
        orch, BIO_MODULE_BRAIN
    );

    EXPECT_EQ(entry, nullptr);

    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, IsModuleRegistered) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    EXPECT_FALSE(bio_orchestrator_is_module_registered(orch, BIO_MODULE_BRAIN));

    bio_module_context_t ctx = CreateTestModule(BIO_MODULE_BRAIN, "brain");
    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx, BIO_STARTUP_PHASE_CORE
    ), 0);

    EXPECT_TRUE(bio_orchestrator_is_module_registered(orch, BIO_MODULE_BRAIN));

    CleanupTestModule(ctx);
    bio_orchestrator_destroy(orch);
}

//=============================================================================
// 8. STATISTICS TESTS
//=============================================================================

TEST_F(BioAsyncOrchestratorTest, GetStats) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_orchestrator_stats_t stats;
    ASSERT_EQ(bio_orchestrator_get_stats(orch, &stats), 0);

    EXPECT_EQ(stats.total_modules, 0u);
    EXPECT_EQ(stats.active_modules, 0u);
    EXPECT_EQ(stats.healthy_modules, 0u);

    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, GetStatsAfterRegistration) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx = CreateTestModule(BIO_MODULE_BRAIN, "brain");
    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx, BIO_STARTUP_PHASE_CORE
    ), 0);

    bio_orchestrator_stats_t stats;
    ASSERT_EQ(bio_orchestrator_get_stats(orch, &stats), 0);

    EXPECT_EQ(stats.total_modules, 1u);
    EXPECT_EQ(stats.active_modules, 1u);

    CleanupTestModule(ctx);
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, ResetStats) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx = CreateTestModule(BIO_MODULE_BRAIN, "brain");
    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx, BIO_STARTUP_PHASE_CORE
    ), 0);

    bio_orchestrator_health_check_all(orch);

    bio_orchestrator_reset_stats(orch);

    bio_orchestrator_stats_t stats;
    ASSERT_EQ(bio_orchestrator_get_stats(orch, &stats), 0);

    EXPECT_EQ(stats.total_health_checks, 0u);
    EXPECT_EQ(stats.total_modules, 1u);  // Module count preserved

    CleanupTestModule(ctx);
    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, GetHealthScore) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    // Empty orchestrator
    float score = bio_orchestrator_get_health_score(orch);
    EXPECT_EQ(score, 0.0f);

    // Add healthy module
    bio_module_context_t ctx = CreateTestModule(BIO_MODULE_BRAIN, "brain");
    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx, BIO_STARTUP_PHASE_CORE
    ), 0);

    bio_orchestrator_health_check_module(orch, BIO_MODULE_BRAIN, nullptr);
    score = bio_orchestrator_get_health_score(orch);
    EXPECT_EQ(score, 1.0f);

    CleanupTestModule(ctx);
    bio_orchestrator_destroy(orch);
}

//=============================================================================
// 9. INTEGRATION TESTS
//=============================================================================

TEST_F(BioAsyncOrchestratorTest, ConnectBrainImmune) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    void* fake_immune = (void*)0x1234;  // Opaque pointer
    EXPECT_EQ(bio_orchestrator_connect_brain_immune(orch, fake_immune), 0);

    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, DisconnectBrainImmune) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    void* fake_immune = (void*)0x1234;
    EXPECT_EQ(bio_orchestrator_connect_brain_immune(orch, fake_immune), 0);
    EXPECT_EQ(bio_orchestrator_disconnect_brain_immune(orch), 0);

    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, ConnectBioAsync) {
    bio_orchestrator_config_t config;
    ASSERT_EQ(bio_orchestrator_default_config(&config), 0);
    config.enable_bio_async = true;

    bio_async_orchestrator_t* orch = bio_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    // Should auto-connect during creation
    bio_orchestrator_destroy(orch);
}

//=============================================================================
// 10. THREAD SAFETY TESTS
//=============================================================================

TEST_F(BioAsyncOrchestratorTest, ConcurrentRegistration) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // Create 4 threads that each register a different module
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([&, i]() {
            bio_module_id_t id = static_cast<bio_module_id_t>(BIO_MODULE_BRAIN + i);
            std::string name = "module_" + std::to_string(i);

            bio_module_context_t ctx = CreateTestModule(id, name.c_str());
            if (ctx) {
                if (bio_orchestrator_register_module(
                    orch, id, name.c_str(),
                    BIO_MODULE_CATEGORY_CORE, ctx, BIO_STARTUP_PHASE_CORE
                ) == 0) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), 4);

    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, ConcurrentHealthCheck) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx = CreateTestModule(BIO_MODULE_BRAIN, "brain");
    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx, BIO_STARTUP_PHASE_CORE
    ), 0);

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([&]() {
            bio_orchestrator_health_check_all(orch);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    CleanupTestModule(ctx);
    bio_orchestrator_destroy(orch);
}

//=============================================================================
// 11. STRING CONVERSION TESTS
//=============================================================================

TEST_F(BioAsyncOrchestratorTest, CategoryToString) {
    EXPECT_STREQ(bio_module_category_to_string(BIO_MODULE_CATEGORY_CORE), "core");
    EXPECT_STREQ(bio_module_category_to_string(BIO_MODULE_CATEGORY_PLASTICITY), "plasticity");
    EXPECT_STREQ(bio_module_category_to_string(BIO_MODULE_CATEGORY_PERCEPTION), "perception");
    EXPECT_STREQ(bio_module_category_to_string(BIO_MODULE_CATEGORY_COGNITIVE), "cognitive");
    EXPECT_STREQ(bio_module_category_to_string(BIO_MODULE_CATEGORY_HIGHLEVEL), "highlevel");
    EXPECT_STREQ(bio_module_category_to_string(BIO_MODULE_CATEGORY_IMMUNE), "immune");
    EXPECT_STREQ(bio_module_category_to_string(BIO_MODULE_CATEGORY_SWARM), "swarm");
    EXPECT_STREQ(bio_module_category_to_string(BIO_MODULE_CATEGORY_SECURITY), "security");
    EXPECT_STREQ(bio_module_category_to_string(BIO_MODULE_CATEGORY_MIDDLEWARE), "middleware");
    EXPECT_STREQ(bio_module_category_to_string(BIO_MODULE_CATEGORY_GLIAL), "glial");
    EXPECT_STREQ(bio_module_category_to_string((bio_module_category_t)999), "unknown");
}

TEST_F(BioAsyncOrchestratorTest, StateToString) {
    EXPECT_STREQ(bio_orchestrator_state_to_string(BIO_ORCHESTRATOR_STOPPED), "stopped");
    EXPECT_STREQ(bio_orchestrator_state_to_string(BIO_ORCHESTRATOR_STARTING), "starting");
    EXPECT_STREQ(bio_orchestrator_state_to_string(BIO_ORCHESTRATOR_RUNNING), "running");
    EXPECT_STREQ(bio_orchestrator_state_to_string(BIO_ORCHESTRATOR_PAUSED), "paused");
    EXPECT_STREQ(bio_orchestrator_state_to_string(BIO_ORCHESTRATOR_STOPPING), "stopping");
    EXPECT_STREQ(bio_orchestrator_state_to_string(BIO_ORCHESTRATOR_ERROR), "error");
    EXPECT_STREQ(bio_orchestrator_state_to_string((bio_orchestrator_state_t)999), "unknown");
}

TEST_F(BioAsyncOrchestratorTest, HealthToString) {
    EXPECT_STREQ(bio_module_health_to_string(BIO_MODULE_HEALTH_UNKNOWN), "unknown");
    EXPECT_STREQ(bio_module_health_to_string(BIO_MODULE_HEALTH_HEALTHY), "healthy");
    EXPECT_STREQ(bio_module_health_to_string(BIO_MODULE_HEALTH_DEGRADED), "degraded");
    EXPECT_STREQ(bio_module_health_to_string(BIO_MODULE_HEALTH_UNHEALTHY), "unhealthy");
    EXPECT_STREQ(bio_module_health_to_string(BIO_MODULE_HEALTH_FAILED), "failed");
    EXPECT_STREQ(bio_module_health_to_string((bio_module_health_t)999), "unknown");
}

//=============================================================================
// 12. EDGE CASES AND ERROR HANDLING
//=============================================================================

TEST_F(BioAsyncOrchestratorTest, NullPointerHandling) {
    EXPECT_EQ(bio_orchestrator_start(nullptr), -1);
    EXPECT_EQ(bio_orchestrator_stop(nullptr), -1);
    EXPECT_EQ(bio_orchestrator_health_check_all(nullptr), -1);
    EXPECT_EQ(bio_orchestrator_health_check_module(nullptr, BIO_MODULE_BRAIN, nullptr), -1);
    EXPECT_STREQ(bio_module_health_to_string(BIO_MODULE_HEALTH_HEALTHY), "healthy");
    EXPECT_EQ(bio_orchestrator_get_state(nullptr), BIO_ORCHESTRATOR_ERROR);
    EXPECT_EQ(bio_orchestrator_get_health_score(nullptr), 0.0f);
}

TEST_F(BioAsyncOrchestratorTest, GetStatsNullPointer) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    EXPECT_EQ(bio_orchestrator_get_stats(nullptr, nullptr), -1);
    EXPECT_EQ(bio_orchestrator_get_stats(orch, nullptr), -1);

    bio_orchestrator_stats_t stats;
    EXPECT_EQ(bio_orchestrator_get_stats(nullptr, &stats), -1);

    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, DiscoveryNullPointers) {
    bio_async_orchestrator_t* orch = bio_orchestrator_create(nullptr);
    ASSERT_NE(orch, nullptr);

    EXPECT_EQ(bio_orchestrator_get_all_modules(nullptr, nullptr, 0), 0u);
    EXPECT_EQ(bio_orchestrator_get_modules_by_category(nullptr, BIO_MODULE_CATEGORY_CORE, nullptr, 0), 0u);
    EXPECT_EQ(bio_orchestrator_get_module_info(nullptr, BIO_MODULE_BRAIN), nullptr);
    EXPECT_FALSE(bio_orchestrator_is_module_registered(nullptr, BIO_MODULE_BRAIN));

    bio_orchestrator_destroy(orch);
}

TEST_F(BioAsyncOrchestratorTest, CapacityLimit) {
    bio_orchestrator_config_t config;
    ASSERT_EQ(bio_orchestrator_default_config(&config), 0);
    config.max_modules = 2;  // Very small capacity

    bio_async_orchestrator_t* orch = bio_orchestrator_create(&config);
    ASSERT_NE(orch, nullptr);

    bio_module_context_t ctx1 = CreateTestModule(BIO_MODULE_BRAIN, "brain");
    bio_module_context_t ctx2 = CreateTestModule(BIO_MODULE_STDP, "stdp");
    bio_module_context_t ctx3 = CreateTestModule(BIO_MODULE_ATTENTION, "attention");

    // First two should succeed
    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_BRAIN, "brain",
        BIO_MODULE_CATEGORY_CORE, ctx1, BIO_STARTUP_PHASE_CORE
    ), 0);

    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_STDP, "stdp",
        BIO_MODULE_CATEGORY_PLASTICITY, ctx2, BIO_STARTUP_PHASE_PLASTICITY
    ), 0);

    // Third should fail (capacity exceeded)
    EXPECT_EQ(bio_orchestrator_register_module(
        orch, BIO_MODULE_ATTENTION, "attention",
        BIO_MODULE_CATEGORY_COGNITIVE, ctx3, BIO_STARTUP_PHASE_COGNITIVE
    ), -1);

    CleanupTestModule(ctx1);
    CleanupTestModule(ctx2);
    CleanupTestModule(ctx3);
    bio_orchestrator_destroy(orch);
}
