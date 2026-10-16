/**
 * @file test_self_assembly_regression.cpp
 * @brief Regression tests for Phase 10: Automatic Self-Assembly
 *
 * Tests to prevent regressions in self-assembly functionality:
 * - Edge cases and boundary conditions
 * - Error handling and recovery
 * - Performance under load
 * - Backwards compatibility with phase-based ordering
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <filesystem>
#include <chrono>
#include <random>

#include "async/nimcp_wiring_diagram.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async_orchestrator.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SelfAssemblyRegressionTest : public ::testing::Test {
protected:
    wiring_diagram_t* wd_ = nullptr;
    brain_kg_t* kg_ = nullptr;
    bio_async_orchestrator_t* orchestrator_ = nullptr;
    std::string temp_dir_;

    void SetUp() override {
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = false;
        bio_config.enable_logging = false;
        nimcp_bio_async_init(&bio_config);

        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = false;
        router_config.enable_logging = false;
        bio_router_init(&router_config);

        temp_dir_ = "/tmp/nimcp_self_assembly_reg_" + std::to_string(getpid());
        std::filesystem::create_directories(temp_dir_);
        std::filesystem::create_directories(temp_dir_ + "/subsystems");

        wd_ = wiring_diagram_create(temp_dir_.c_str());
        ASSERT_NE(wd_, nullptr);
        wiring_diagram_set_auto_persist(wd_, false);

        brain_kg_config_t kg_config;
        brain_kg_default_config(&kg_config);
        kg_config.enable_security = false;
        kg_ = brain_kg_create(&kg_config);
        ASSERT_NE(kg_, nullptr);

        bio_orchestrator_config_t orch_config;
        bio_orchestrator_default_config(&orch_config);
        orch_config.enable_logging = false;
        orchestrator_ = bio_orchestrator_create(&orch_config);
        ASSERT_NE(orchestrator_, nullptr);
    }

    void TearDown() override {
        if (orchestrator_) {
            bio_orchestrator_destroy(orchestrator_);
            orchestrator_ = nullptr;
        }
        if (wd_) {
            wiring_diagram_destroy(wd_);
            wd_ = nullptr;
        }
        if (kg_) {
            brain_kg_destroy(kg_);
            kg_ = nullptr;
        }

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        if (!temp_dir_.empty()) {
            std::filesystem::remove_all(temp_dir_);
        }
    }

    int AddModule(bio_module_id_t id, const char* name,
                  const std::vector<bio_module_id_t>& deps = {}) {
        wiring_module_config_t config;
        wiring_module_config_init(&config);
        strncpy(config.module_name, name, sizeof(config.module_name) - 1);
        config.module_id = id;
        config.subsystem = WIRING_SUBSYSTEM_CORE;
        config.min_tier = PLATFORM_TIER_MINIMAL;
        config.enabled = true;

        if (!deps.empty()) {
            config.depends_on = (bio_module_id_t*)
                nimcp_calloc(deps.size(), sizeof(bio_module_id_t));
            for (size_t i = 0; i < deps.size(); i++) {
                config.depends_on[i] = deps[i];
            }
            config.depends_on_count = deps.size();
            config.depends_on_capacity = deps.size();
        }

        int result = wiring_diagram_add_module(wd_, name, &config);
        if (result != 0) return result;

        return bio_orchestrator_register_module(
            orchestrator_, id, name, BIO_MODULE_CATEGORY_CORE, nullptr, 0);
    }
};

//=============================================================================
// EDGE CASE REGRESSION TESTS
//=============================================================================

/**
 * @test Regression: Single module with no dependencies
 */
TEST_F(SelfAssemblyRegressionTest, SingleModuleNoDeps) {
    AddModule(static_cast<bio_module_id_t>(0x100), "lonely");
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    bio_module_id_t order[10];
    int count = bio_orchestrator_compute_startup_order(orchestrator_, order, 10);
    EXPECT_EQ(count, 1);

    int started = bio_orchestrator_start_modules_ordered(orchestrator_);
    EXPECT_EQ(started, 1);

    int stopped = bio_orchestrator_stop_modules_ordered(orchestrator_);
    EXPECT_EQ(stopped, 1);
}

/**
 * @test Regression: Many independent modules (no dependencies)
 */
TEST_F(SelfAssemblyRegressionTest, ManyIndependentModules) {
    const int NUM_MODULES = 50;

    for (int i = 0; i < NUM_MODULES; i++) {
        char name[32];
        snprintf(name, sizeof(name), "independent_%d", i);
        AddModule(static_cast<bio_module_id_t>(0x100 + i), name);
    }

    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    bio_module_id_t order[100];
    int count = bio_orchestrator_compute_startup_order(orchestrator_, order, 100);
    EXPECT_EQ(count, NUM_MODULES);

    int started = bio_orchestrator_start_modules_ordered(orchestrator_);
    EXPECT_EQ(started, NUM_MODULES);
}

/**
 * @test Regression: Deep dependency chain
 */
TEST_F(SelfAssemblyRegressionTest, DeepDependencyChain) {
    const int DEPTH = 20;

    AddModule(static_cast<bio_module_id_t>(0x100), "level_0");

    for (int i = 1; i < DEPTH; i++) {
        char name[32];
        snprintf(name, sizeof(name), "level_%d", i);
        AddModule(static_cast<bio_module_id_t>(0x100 + i), name,
            {static_cast<bio_module_id_t>(0x100 + i - 1)});
    }

    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    bio_module_id_t order[50];
    int count = bio_orchestrator_compute_startup_order(orchestrator_, order, 50);
    EXPECT_EQ(count, DEPTH);

    // Verify strict ordering
    for (int i = 0; i < DEPTH; i++) {
        EXPECT_EQ(order[i], static_cast<bio_module_id_t>(0x100 + i));
    }
}

/**
 * @test Regression: Wide dependency tree
 */
TEST_F(SelfAssemblyRegressionTest, WideDependencyTree) {
    const int WIDTH = 30;

    AddModule(static_cast<bio_module_id_t>(0x100), "root");

    for (int i = 0; i < WIDTH; i++) {
        char name[32];
        snprintf(name, sizeof(name), "child_%d", i);
        AddModule(static_cast<bio_module_id_t>(0x101 + i), name,
            {static_cast<bio_module_id_t>(0x100)});
    }

    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    bio_module_id_t order[100];
    int count = bio_orchestrator_compute_startup_order(orchestrator_, order, 100);
    EXPECT_EQ(count, WIDTH + 1);

    // Root must be first
    EXPECT_EQ(order[0], static_cast<bio_module_id_t>(0x100));
}

//=============================================================================
// ERROR HANDLING REGRESSION TESTS
//=============================================================================

/**
 * @test Regression: Circular dependency detection
 */
TEST_F(SelfAssemblyRegressionTest, CircularDependencyDetection) {
    // A -> B -> C -> A
    wiring_module_config_t config_a;
    wiring_module_config_init(&config_a);
    strncpy(config_a.module_name, "cycle_a", sizeof(config_a.module_name) - 1);
    config_a.module_id = static_cast<bio_module_id_t>(0x100);
    config_a.enabled = true;
    config_a.depends_on = (bio_module_id_t*)nimcp_calloc(1, sizeof(bio_module_id_t));
    config_a.depends_on[0] = static_cast<bio_module_id_t>(0x102);
    config_a.depends_on_count = 1;
    config_a.depends_on_capacity = 1;
    wiring_diagram_add_module(wd_, "cycle_a", &config_a);

    wiring_module_config_t config_b;
    wiring_module_config_init(&config_b);
    strncpy(config_b.module_name, "cycle_b", sizeof(config_b.module_name) - 1);
    config_b.module_id = static_cast<bio_module_id_t>(0x101);
    config_b.enabled = true;
    config_b.depends_on = (bio_module_id_t*)nimcp_calloc(1, sizeof(bio_module_id_t));
    config_b.depends_on[0] = static_cast<bio_module_id_t>(0x100);
    config_b.depends_on_count = 1;
    config_b.depends_on_capacity = 1;
    wiring_diagram_add_module(wd_, "cycle_b", &config_b);

    wiring_module_config_t config_c;
    wiring_module_config_init(&config_c);
    strncpy(config_c.module_name, "cycle_c", sizeof(config_c.module_name) - 1);
    config_c.module_id = static_cast<bio_module_id_t>(0x102);
    config_c.enabled = true;
    config_c.depends_on = (bio_module_id_t*)nimcp_calloc(1, sizeof(bio_module_id_t));
    config_c.depends_on[0] = static_cast<bio_module_id_t>(0x101);
    config_c.depends_on_count = 1;
    config_c.depends_on_capacity = 1;
    wiring_diagram_add_module(wd_, "cycle_c", &config_c);

    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    wiring_validation_result_t result;
    int status = bio_orchestrator_validate_self_assembly(orchestrator_, &result);

    EXPECT_EQ(status, -1);
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.has_circular_deps);
}

/**
 * @test Regression: Self-dependency detection
 */
TEST_F(SelfAssemblyRegressionTest, SelfDependencyDetection) {
    wiring_module_config_t config;
    wiring_module_config_init(&config);
    strncpy(config.module_name, "self_dep", sizeof(config.module_name) - 1);
    config.module_id = static_cast<bio_module_id_t>(0x100);
    config.enabled = true;
    config.depends_on = (bio_module_id_t*)nimcp_calloc(1, sizeof(bio_module_id_t));
    config.depends_on[0] = static_cast<bio_module_id_t>(0x100);  // Self!
    config.depends_on_count = 1;
    config.depends_on_capacity = 1;
    wiring_diagram_add_module(wd_, "self_dep", &config);

    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    wiring_validation_result_t result;
    int status = bio_orchestrator_validate_self_assembly(orchestrator_, &result);

    EXPECT_EQ(status, -1);
    EXPECT_FALSE(result.valid);
}

//=============================================================================
// BACKWARDS COMPATIBILITY REGRESSION TESTS
//=============================================================================

/**
 * @test Regression: Phase-based fallback works without wiring diagram
 */
TEST_F(SelfAssemblyRegressionTest, PhaseFallbackWithoutWiring) {
    // Register modules directly with phases, no wiring diagram
    bio_orchestrator_register_module(orchestrator_,
        static_cast<bio_module_id_t>(0x100), "phase3", BIO_MODULE_CATEGORY_CORE, nullptr, 3);
    bio_orchestrator_register_module(orchestrator_,
        static_cast<bio_module_id_t>(0x101), "phase1", BIO_MODULE_CATEGORY_CORE, nullptr, 1);
    bio_orchestrator_register_module(orchestrator_,
        static_cast<bio_module_id_t>(0x102), "phase0", BIO_MODULE_CATEGORY_CORE, nullptr, 0);
    bio_orchestrator_register_module(orchestrator_,
        static_cast<bio_module_id_t>(0x103), "phase2", BIO_MODULE_CATEGORY_CORE, nullptr, 2);

    // No wiring diagram set

    bio_module_id_t order[10];
    int count = bio_orchestrator_compute_startup_order(orchestrator_, order, 10);
    EXPECT_EQ(count, 4);

    // Verify phase ordering
    EXPECT_EQ(order[0], static_cast<bio_module_id_t>(0x102));  // phase 0
    EXPECT_EQ(order[1], static_cast<bio_module_id_t>(0x101));  // phase 1
    EXPECT_EQ(order[2], static_cast<bio_module_id_t>(0x103));  // phase 2
    EXPECT_EQ(order[3], static_cast<bio_module_id_t>(0x100));  // phase 3
}

/**
 * @test Regression: Legacy orchestrator start still works
 */
TEST_F(SelfAssemblyRegressionTest, LegacyStartStillWorks) {
    // Use legacy bio_orchestrator_start instead of start_modules_ordered
    AddModule(static_cast<bio_module_id_t>(0x100), "legacy_mod");
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    int result = bio_orchestrator_start(orchestrator_);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bio_orchestrator_get_state(orchestrator_), BIO_ORCHESTRATOR_RUNNING);

    result = bio_orchestrator_stop(orchestrator_);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(bio_orchestrator_get_state(orchestrator_), BIO_ORCHESTRATOR_STOPPED);
}

//=============================================================================
// PERFORMANCE REGRESSION TESTS
//=============================================================================

/**
 * @test Regression: Startup order computation performance
 */
TEST_F(SelfAssemblyRegressionTest, StartupOrderPerformance) {
    const int NUM_MODULES = 100;

    // Create modules with random dependencies
    std::mt19937 rng(42);  // Fixed seed for reproducibility

    AddModule(static_cast<bio_module_id_t>(0x100), "root");

    for (int i = 1; i < NUM_MODULES; i++) {
        char name[32];
        snprintf(name, sizeof(name), "perf_%d", i);

        // Each module depends on a random earlier module
        std::uniform_int_distribution<int> dist(0, i - 1);
        int dep_idx = dist(rng);

        AddModule(static_cast<bio_module_id_t>(0x100 + i), name,
            {static_cast<bio_module_id_t>(0x100 + dep_idx)});
    }

    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    // Measure time
    auto start = std::chrono::high_resolution_clock::now();

    bio_module_id_t order[200];
    int count = bio_orchestrator_compute_startup_order(orchestrator_, order, 200);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(count, NUM_MODULES);
    EXPECT_LT(duration.count(), 1000);  // Should complete in under 1 second
}

/**
 * @test Regression: Repeated start/stop cycles don't leak memory
 */
TEST_F(SelfAssemblyRegressionTest, RepeatedCyclesNoLeak) {
    AddModule(static_cast<bio_module_id_t>(0x100), "base");
    AddModule(static_cast<bio_module_id_t>(0x101), "derived",
        {static_cast<bio_module_id_t>(0x100)});
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    for (int i = 0; i < 100; i++) {
        int started = bio_orchestrator_start_modules_ordered(orchestrator_);
        EXPECT_EQ(started, 2);

        int stopped = bio_orchestrator_stop_modules_ordered(orchestrator_);
        EXPECT_EQ(stopped, 2);
    }
}

//=============================================================================
// STATE CONSISTENCY REGRESSION TESTS
//=============================================================================

/**
 * @test Regression: Health status consistent across start/stop
 */
TEST_F(SelfAssemblyRegressionTest, HealthStatusConsistency) {
    AddModule(static_cast<bio_module_id_t>(0x100), "health_mod");
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    // Initial: unknown
    EXPECT_EQ(bio_orchestrator_get_module_health(orchestrator_,
        static_cast<bio_module_id_t>(0x100)), BIO_MODULE_HEALTH_UNKNOWN);

    // After start: healthy
    bio_orchestrator_start_modules_ordered(orchestrator_);
    EXPECT_EQ(bio_orchestrator_get_module_health(orchestrator_,
        static_cast<bio_module_id_t>(0x100)), BIO_MODULE_HEALTH_HEALTHY);

    // After stop: unknown
    bio_orchestrator_stop_modules_ordered(orchestrator_);
    EXPECT_EQ(bio_orchestrator_get_module_health(orchestrator_,
        static_cast<bio_module_id_t>(0x100)), BIO_MODULE_HEALTH_UNKNOWN);

    // After restart: healthy again
    bio_orchestrator_start_modules_ordered(orchestrator_);
    EXPECT_EQ(bio_orchestrator_get_module_health(orchestrator_,
        static_cast<bio_module_id_t>(0x100)), BIO_MODULE_HEALTH_HEALTHY);
}

/**
 * @test Regression: Orchestrator state transitions
 */
TEST_F(SelfAssemblyRegressionTest, StateTransitions) {
    AddModule(static_cast<bio_module_id_t>(0x100), "state_mod");
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    EXPECT_EQ(bio_orchestrator_get_state(orchestrator_), BIO_ORCHESTRATOR_STOPPED);

    bio_orchestrator_start_modules_ordered(orchestrator_);
    EXPECT_EQ(bio_orchestrator_get_state(orchestrator_), BIO_ORCHESTRATOR_RUNNING);

    bio_orchestrator_stop_modules_ordered(orchestrator_);
    EXPECT_EQ(bio_orchestrator_get_state(orchestrator_), BIO_ORCHESTRATOR_STOPPED);
}

/**
 * @test Regression: Self-assembly available flag
 */
TEST_F(SelfAssemblyRegressionTest, SelfAssemblyAvailableFlag) {
    // Not available without wiring
    EXPECT_FALSE(bio_orchestrator_self_assembly_available(orchestrator_));

    // Not available with empty wiring
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);
    EXPECT_FALSE(bio_orchestrator_self_assembly_available(orchestrator_));

    // Available after adding module
    AddModule(static_cast<bio_module_id_t>(0x100), "makes_available");
    EXPECT_TRUE(bio_orchestrator_self_assembly_available(orchestrator_));

    // Still available after clearing wiring
    bio_orchestrator_set_wiring_diagram(orchestrator_, nullptr);
    EXPECT_FALSE(bio_orchestrator_self_assembly_available(orchestrator_));
}

//=============================================================================
// VALIDATION RESULT REGRESSION TESTS
//=============================================================================

/**
 * @test Regression: Validation result details
 */
TEST_F(SelfAssemblyRegressionTest, ValidationResultDetails) {
    AddModule(static_cast<bio_module_id_t>(0x100), "valid_a");
    AddModule(static_cast<bio_module_id_t>(0x101), "valid_b",
        {static_cast<bio_module_id_t>(0x100)});
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    wiring_validation_result_t result;
    bio_orchestrator_validate_self_assembly(orchestrator_, &result);

    EXPECT_TRUE(result.valid);
    EXPECT_FALSE(result.has_circular_deps);
    EXPECT_FALSE(result.has_missing_deps);
    EXPECT_EQ(result.error_count, 0u);
}

/**
 * @test Regression: Multiple validation calls don't corrupt state
 */
TEST_F(SelfAssemblyRegressionTest, MultipleValidationCalls) {
    AddModule(static_cast<bio_module_id_t>(0x100), "multi_val");
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    for (int i = 0; i < 10; i++) {
        wiring_validation_result_t result;
        int status = bio_orchestrator_validate_self_assembly(orchestrator_, &result);
        EXPECT_EQ(status, 0);
        EXPECT_TRUE(result.valid);
    }
}
