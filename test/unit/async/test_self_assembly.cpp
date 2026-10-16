/**
 * @file test_self_assembly.cpp
 * @brief Unit tests for Phase 10: Automatic Self-Assembly
 *
 * Tests the KG-driven automatic startup ordering system:
 * - bio_orchestrator_compute_startup_order()
 * - bio_orchestrator_start_modules_ordered()
 * - bio_orchestrator_stop_modules_ordered()
 * - bio_orchestrator_self_assembly_available()
 * - bio_orchestrator_get_module_startup_position()
 * - bio_orchestrator_validate_self_assembly()
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <filesystem>

#include "async/nimcp_wiring_diagram.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async_orchestrator.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SelfAssemblyUnitTest : public ::testing::Test {
protected:
    wiring_diagram_t* wd_ = nullptr;
    bio_async_orchestrator_t* orchestrator_ = nullptr;
    std::string temp_dir_;

    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = false;
        bio_config.enable_logging = false;
        nimcp_bio_async_init(&bio_config);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = false;
        router_config.enable_logging = false;
        bio_router_init(&router_config);

        // Create temp directory
        temp_dir_ = "/tmp/nimcp_self_assembly_unit_" + std::to_string(getpid());
        std::filesystem::create_directories(temp_dir_);
        std::filesystem::create_directories(temp_dir_ + "/subsystems");

        // Create wiring diagram
        wd_ = wiring_diagram_create(temp_dir_.c_str());
        ASSERT_NE(wd_, nullptr);
        wiring_diagram_set_auto_persist(wd_, false);

        // Create orchestrator
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

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        if (!temp_dir_.empty()) {
            std::filesystem::remove_all(temp_dir_);
        }
    }

    // Helper: Add module to both wiring diagram and orchestrator
    int AddModule(bio_module_id_t id, const char* name,
                  const std::vector<bio_module_id_t>& deps = {}) {
        // Add to wiring diagram
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
            if (config.depends_on) {
                for (size_t i = 0; i < deps.size(); i++) {
                    config.depends_on[i] = deps[i];
                }
                config.depends_on_count = deps.size();
                config.depends_on_capacity = deps.size();
            }
        }

        int result = wiring_diagram_add_module(wd_, name, &config);
        if (result != 0) return result;

        // Also register with orchestrator
        return bio_orchestrator_register_module(
            orchestrator_, id, name, BIO_MODULE_CATEGORY_CORE, nullptr, 0);
    }
};

//=============================================================================
// SELF-ASSEMBLY AVAILABILITY TESTS
//=============================================================================

/**
 * @test Self-assembly not available without wiring diagram
 */
TEST_F(SelfAssemblyUnitTest, SelfAssemblyNotAvailableWithoutWiring) {
    // Orchestrator without wiring diagram
    EXPECT_FALSE(bio_orchestrator_self_assembly_available(orchestrator_));
}

/**
 * @test Self-assembly not available with empty wiring diagram
 */
TEST_F(SelfAssemblyUnitTest, SelfAssemblyNotAvailableWithEmptyWiring) {
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);
    EXPECT_FALSE(bio_orchestrator_self_assembly_available(orchestrator_));
}

/**
 * @test Self-assembly available with populated wiring diagram
 */
TEST_F(SelfAssemblyUnitTest, SelfAssemblyAvailableWithModules) {
    AddModule(static_cast<bio_module_id_t>(0x100), "test_module");
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);
    EXPECT_TRUE(bio_orchestrator_self_assembly_available(orchestrator_));
}

/**
 * @test Null orchestrator returns false
 */
TEST_F(SelfAssemblyUnitTest, SelfAssemblyNullCheck) {
    EXPECT_FALSE(bio_orchestrator_self_assembly_available(nullptr));
}

//=============================================================================
// COMPUTE STARTUP ORDER TESTS
//=============================================================================

/**
 * @test Compute startup order with single module
 */
TEST_F(SelfAssemblyUnitTest, ComputeStartupOrderSingleModule) {
    AddModule(static_cast<bio_module_id_t>(0x100), "single");
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    bio_module_id_t order[10];
    int count = bio_orchestrator_compute_startup_order(orchestrator_, order, 10);

    EXPECT_EQ(count, 1);
    EXPECT_EQ(order[0], static_cast<bio_module_id_t>(0x100));
}

/**
 * @test Compute startup order with linear dependency chain
 */
TEST_F(SelfAssemblyUnitTest, ComputeStartupOrderLinearChain) {
    // Create chain: A <- B <- C
    AddModule(static_cast<bio_module_id_t>(0x100), "module_a");
    AddModule(static_cast<bio_module_id_t>(0x101), "module_b", {static_cast<bio_module_id_t>(0x100)});
    AddModule(static_cast<bio_module_id_t>(0x102), "module_c", {static_cast<bio_module_id_t>(0x101)});

    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    bio_module_id_t order[10];
    int count = bio_orchestrator_compute_startup_order(orchestrator_, order, 10);

    EXPECT_EQ(count, 3);
    // A must come before B, B must come before C
    int pos_a = -1, pos_b = -1, pos_c = -1;
    for (int i = 0; i < count; i++) {
        if (order[i] == static_cast<bio_module_id_t>(0x100)) pos_a = i;
        if (order[i] == static_cast<bio_module_id_t>(0x101)) pos_b = i;
        if (order[i] == static_cast<bio_module_id_t>(0x102)) pos_c = i;
    }
    EXPECT_LT(pos_a, pos_b);
    EXPECT_LT(pos_b, pos_c);
}

/**
 * @test Compute startup order with diamond dependency
 */
TEST_F(SelfAssemblyUnitTest, ComputeStartupOrderDiamondDependency) {
    // Diamond: A <- B, A <- C, B <- D, C <- D
    AddModule(static_cast<bio_module_id_t>(0x100), "root_a");
    AddModule(static_cast<bio_module_id_t>(0x101), "branch_b", {static_cast<bio_module_id_t>(0x100)});
    AddModule(static_cast<bio_module_id_t>(0x102), "branch_c", {static_cast<bio_module_id_t>(0x100)});
    AddModule(static_cast<bio_module_id_t>(0x103), "top_d", {static_cast<bio_module_id_t>(0x101), static_cast<bio_module_id_t>(0x102)});

    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    bio_module_id_t order[10];
    int count = bio_orchestrator_compute_startup_order(orchestrator_, order, 10);

    EXPECT_EQ(count, 4);

    // Find positions
    int pos_a = -1, pos_b = -1, pos_c = -1, pos_d = -1;
    for (int i = 0; i < count; i++) {
        if (order[i] == static_cast<bio_module_id_t>(0x100)) pos_a = i;
        if (order[i] == static_cast<bio_module_id_t>(0x101)) pos_b = i;
        if (order[i] == static_cast<bio_module_id_t>(0x102)) pos_c = i;
        if (order[i] == static_cast<bio_module_id_t>(0x103)) pos_d = i;
    }

    // A must be first
    EXPECT_EQ(pos_a, 0);
    // D must be last
    EXPECT_EQ(pos_d, 3);
    // B and C must be between A and D
    EXPECT_LT(pos_a, pos_b);
    EXPECT_LT(pos_a, pos_c);
    EXPECT_LT(pos_b, pos_d);
    EXPECT_LT(pos_c, pos_d);
}

/**
 * @test Null parameter handling
 */
TEST_F(SelfAssemblyUnitTest, ComputeStartupOrderNullParams) {
    bio_module_id_t order[10];

    EXPECT_EQ(bio_orchestrator_compute_startup_order(nullptr, order, 10), -1);
    EXPECT_EQ(bio_orchestrator_compute_startup_order(orchestrator_, nullptr, 10), -1);
    EXPECT_EQ(bio_orchestrator_compute_startup_order(orchestrator_, order, 0), -1);
}

/**
 * @test Fallback to phase-based ordering without wiring diagram
 */
TEST_F(SelfAssemblyUnitTest, ComputeStartupOrderFallbackToPhases) {
    // Register modules directly with orchestrator (no wiring diagram)
    bio_orchestrator_register_module(orchestrator_,
        static_cast<bio_module_id_t>(0x100), "phase2", BIO_MODULE_CATEGORY_CORE, nullptr, 2);
    bio_orchestrator_register_module(orchestrator_,
        static_cast<bio_module_id_t>(0x101), "phase0", BIO_MODULE_CATEGORY_CORE, nullptr, 0);
    bio_orchestrator_register_module(orchestrator_,
        static_cast<bio_module_id_t>(0x102), "phase1", BIO_MODULE_CATEGORY_CORE, nullptr, 1);

    bio_module_id_t order[10];
    int count = bio_orchestrator_compute_startup_order(orchestrator_, order, 10);

    EXPECT_EQ(count, 3);
    // Should be ordered by phase
    EXPECT_EQ(order[0], static_cast<bio_module_id_t>(0x101)); // phase 0
    EXPECT_EQ(order[1], static_cast<bio_module_id_t>(0x102)); // phase 1
    EXPECT_EQ(order[2], static_cast<bio_module_id_t>(0x100)); // phase 2
}

//=============================================================================
// START MODULES ORDERED TESTS
//=============================================================================

/**
 * @test Start modules in order with valid wiring
 */
TEST_F(SelfAssemblyUnitTest, StartModulesOrderedBasic) {
    AddModule(static_cast<bio_module_id_t>(0x100), "base");
    AddModule(static_cast<bio_module_id_t>(0x101), "derived", {static_cast<bio_module_id_t>(0x100)});
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    int started = bio_orchestrator_start_modules_ordered(orchestrator_);
    EXPECT_EQ(started, 2);
    EXPECT_EQ(bio_orchestrator_get_state(orchestrator_), BIO_ORCHESTRATOR_RUNNING);
}

/**
 * @test Start with no modules
 */
TEST_F(SelfAssemblyUnitTest, StartModulesOrderedEmpty) {
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);
    int started = bio_orchestrator_start_modules_ordered(orchestrator_);
    EXPECT_EQ(started, 0);
}

/**
 * @test Null orchestrator handling
 */
TEST_F(SelfAssemblyUnitTest, StartModulesOrderedNull) {
    EXPECT_EQ(bio_orchestrator_start_modules_ordered(nullptr), -1);
}

/**
 * @test Modules marked healthy after start
 */
TEST_F(SelfAssemblyUnitTest, StartModulesMarksHealthy) {
    AddModule(static_cast<bio_module_id_t>(0x100), "test_mod");
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    // Health should be unknown before start
    EXPECT_EQ(bio_orchestrator_get_module_health(orchestrator_,
        static_cast<bio_module_id_t>(0x100)), BIO_MODULE_HEALTH_UNKNOWN);

    bio_orchestrator_start_modules_ordered(orchestrator_);

    // Health should be healthy after start
    EXPECT_EQ(bio_orchestrator_get_module_health(orchestrator_,
        static_cast<bio_module_id_t>(0x100)), BIO_MODULE_HEALTH_HEALTHY);
}

//=============================================================================
// STOP MODULES ORDERED TESTS
//=============================================================================

/**
 * @test Stop modules in reverse order
 */
TEST_F(SelfAssemblyUnitTest, StopModulesOrderedBasic) {
    AddModule(static_cast<bio_module_id_t>(0x100), "base");
    AddModule(static_cast<bio_module_id_t>(0x101), "derived", {static_cast<bio_module_id_t>(0x100)});
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    // Start first
    bio_orchestrator_start_modules_ordered(orchestrator_);
    EXPECT_EQ(bio_orchestrator_get_state(orchestrator_), BIO_ORCHESTRATOR_RUNNING);

    // Then stop
    int stopped = bio_orchestrator_stop_modules_ordered(orchestrator_);
    EXPECT_EQ(stopped, 2);
    EXPECT_EQ(bio_orchestrator_get_state(orchestrator_), BIO_ORCHESTRATOR_STOPPED);
}

/**
 * @test Stop with no modules
 */
TEST_F(SelfAssemblyUnitTest, StopModulesOrderedEmpty) {
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);
    int stopped = bio_orchestrator_stop_modules_ordered(orchestrator_);
    EXPECT_EQ(stopped, 0);
}

/**
 * @test Null orchestrator handling
 */
TEST_F(SelfAssemblyUnitTest, StopModulesOrderedNull) {
    EXPECT_EQ(bio_orchestrator_stop_modules_ordered(nullptr), -1);
}

/**
 * @test Modules marked unknown health after stop
 */
TEST_F(SelfAssemblyUnitTest, StopModulesMarksUnknown) {
    AddModule(static_cast<bio_module_id_t>(0x100), "test_mod");
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    bio_orchestrator_start_modules_ordered(orchestrator_);
    EXPECT_EQ(bio_orchestrator_get_module_health(orchestrator_,
        static_cast<bio_module_id_t>(0x100)), BIO_MODULE_HEALTH_HEALTHY);

    bio_orchestrator_stop_modules_ordered(orchestrator_);
    EXPECT_EQ(bio_orchestrator_get_module_health(orchestrator_,
        static_cast<bio_module_id_t>(0x100)), BIO_MODULE_HEALTH_UNKNOWN);
}

//=============================================================================
// GET MODULE STARTUP POSITION TESTS
//=============================================================================

/**
 * @test Get startup position for valid module
 */
TEST_F(SelfAssemblyUnitTest, GetModuleStartupPosition) {
    AddModule(static_cast<bio_module_id_t>(0x100), "first");
    AddModule(static_cast<bio_module_id_t>(0x101), "second", {static_cast<bio_module_id_t>(0x100)});
    AddModule(static_cast<bio_module_id_t>(0x102), "third", {static_cast<bio_module_id_t>(0x101)});
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    int pos_first = bio_orchestrator_get_module_startup_position(
        orchestrator_, static_cast<bio_module_id_t>(0x100));
    int pos_second = bio_orchestrator_get_module_startup_position(
        orchestrator_, static_cast<bio_module_id_t>(0x101));
    int pos_third = bio_orchestrator_get_module_startup_position(
        orchestrator_, static_cast<bio_module_id_t>(0x102));

    EXPECT_EQ(pos_first, 0);
    EXPECT_EQ(pos_second, 1);
    EXPECT_EQ(pos_third, 2);
}

/**
 * @test Get startup position for non-existent module
 */
TEST_F(SelfAssemblyUnitTest, GetModuleStartupPositionNotFound) {
    AddModule(static_cast<bio_module_id_t>(0x100), "only");
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    int pos = bio_orchestrator_get_module_startup_position(
        orchestrator_, static_cast<bio_module_id_t>(0x999));
    EXPECT_EQ(pos, -1);
}

/**
 * @test Null orchestrator handling
 */
TEST_F(SelfAssemblyUnitTest, GetModuleStartupPositionNull) {
    EXPECT_EQ(bio_orchestrator_get_module_startup_position(nullptr,
        static_cast<bio_module_id_t>(0x100)), -1);
}

//=============================================================================
// VALIDATE SELF-ASSEMBLY TESTS
//=============================================================================

/**
 * @test Validate valid self-assembly configuration
 */
TEST_F(SelfAssemblyUnitTest, ValidateSelfAssemblyValid) {
    AddModule(static_cast<bio_module_id_t>(0x100), "base");
    AddModule(static_cast<bio_module_id_t>(0x101), "derived", {static_cast<bio_module_id_t>(0x100)});
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    wiring_validation_result_t result;
    int status = bio_orchestrator_validate_self_assembly(orchestrator_, &result);

    EXPECT_EQ(status, 0);
    EXPECT_TRUE(result.valid);
    EXPECT_FALSE(result.has_circular_deps);
    EXPECT_FALSE(result.has_missing_deps);
}

/**
 * @test Validate with circular dependency
 */
TEST_F(SelfAssemblyUnitTest, ValidateSelfAssemblyCircular) {
    // Create circular: A -> B -> A
    wiring_module_config_t config_a;
    wiring_module_config_init(&config_a);
    strncpy(config_a.module_name, "mod_a", sizeof(config_a.module_name) - 1);
    config_a.module_id = static_cast<bio_module_id_t>(0x100);
    config_a.enabled = true;
    config_a.depends_on = (bio_module_id_t*)nimcp_calloc(1, sizeof(bio_module_id_t));
    config_a.depends_on[0] = static_cast<bio_module_id_t>(0x101);
    config_a.depends_on_count = 1;
    config_a.depends_on_capacity = 1;

    wiring_module_config_t config_b;
    wiring_module_config_init(&config_b);
    strncpy(config_b.module_name, "mod_b", sizeof(config_b.module_name) - 1);
    config_b.module_id = static_cast<bio_module_id_t>(0x101);
    config_b.enabled = true;
    config_b.depends_on = (bio_module_id_t*)nimcp_calloc(1, sizeof(bio_module_id_t));
    config_b.depends_on[0] = static_cast<bio_module_id_t>(0x100);
    config_b.depends_on_count = 1;
    config_b.depends_on_capacity = 1;

    wiring_diagram_add_module(wd_, "mod_a", &config_a);
    wiring_diagram_add_module(wd_, "mod_b", &config_b);
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    wiring_validation_result_t result;
    int status = bio_orchestrator_validate_self_assembly(orchestrator_, &result);

    EXPECT_EQ(status, -1);
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.has_circular_deps);
}

/**
 * @test Validate without wiring diagram (trivially valid)
 */
TEST_F(SelfAssemblyUnitTest, ValidateSelfAssemblyNoWiring) {
    wiring_validation_result_t result;
    int status = bio_orchestrator_validate_self_assembly(orchestrator_, &result);

    EXPECT_EQ(status, 0);
    EXPECT_TRUE(result.valid);
}

/**
 * @test Null orchestrator handling
 */
TEST_F(SelfAssemblyUnitTest, ValidateSelfAssemblyNull) {
    wiring_validation_result_t result;
    EXPECT_EQ(bio_orchestrator_validate_self_assembly(nullptr, &result), -1);
}

/**
 * @test Validate with null result (should still work)
 */
TEST_F(SelfAssemblyUnitTest, ValidateSelfAssemblyNullResult) {
    AddModule(static_cast<bio_module_id_t>(0x100), "test");
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    int status = bio_orchestrator_validate_self_assembly(orchestrator_, nullptr);
    EXPECT_EQ(status, 0);
}

//=============================================================================
// WIRING DIAGRAM MODULE COUNT TESTS
//=============================================================================

/**
 * @test Get module count from empty wiring diagram
 */
TEST_F(SelfAssemblyUnitTest, WiringDiagramModuleCountEmpty) {
    EXPECT_EQ(wiring_diagram_get_module_count(wd_), 0u);
}

/**
 * @test Get module count from populated wiring diagram
 */
TEST_F(SelfAssemblyUnitTest, WiringDiagramModuleCountPopulated) {
    wiring_module_config_t config;
    wiring_module_config_init(&config);
    strncpy(config.module_name, "test1", sizeof(config.module_name) - 1);
    config.module_id = static_cast<bio_module_id_t>(0x100);
    wiring_diagram_add_module(wd_, "test1", &config);

    wiring_module_config_init(&config);
    strncpy(config.module_name, "test2", sizeof(config.module_name) - 1);
    config.module_id = static_cast<bio_module_id_t>(0x101);
    wiring_diagram_add_module(wd_, "test2", &config);

    EXPECT_EQ(wiring_diagram_get_module_count(wd_), 2u);
}

/**
 * @test Get module count from null wiring diagram
 */
TEST_F(SelfAssemblyUnitTest, WiringDiagramModuleCountNull) {
    EXPECT_EQ(wiring_diagram_get_module_count(nullptr), 0u);
}

//=============================================================================
// START/STOP LIFECYCLE TESTS
//=============================================================================

/**
 * @test Full lifecycle: start, verify state, stop, verify state
 */
TEST_F(SelfAssemblyUnitTest, FullLifecycle) {
    AddModule(static_cast<bio_module_id_t>(0x100), "core");
    AddModule(static_cast<bio_module_id_t>(0x101), "app", {static_cast<bio_module_id_t>(0x100)});
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    // Initial state
    EXPECT_EQ(bio_orchestrator_get_state(orchestrator_), BIO_ORCHESTRATOR_STOPPED);

    // Start
    int started = bio_orchestrator_start_modules_ordered(orchestrator_);
    EXPECT_EQ(started, 2);
    EXPECT_EQ(bio_orchestrator_get_state(orchestrator_), BIO_ORCHESTRATOR_RUNNING);

    // Stop
    int stopped = bio_orchestrator_stop_modules_ordered(orchestrator_);
    EXPECT_EQ(stopped, 2);
    EXPECT_EQ(bio_orchestrator_get_state(orchestrator_), BIO_ORCHESTRATOR_STOPPED);
}

/**
 * @test Multiple start/stop cycles
 */
TEST_F(SelfAssemblyUnitTest, MultipleStartStopCycles) {
    AddModule(static_cast<bio_module_id_t>(0x100), "cyclic");
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    for (int i = 0; i < 3; i++) {
        int started = bio_orchestrator_start_modules_ordered(orchestrator_);
        EXPECT_EQ(started, 1);
        EXPECT_EQ(bio_orchestrator_get_state(orchestrator_), BIO_ORCHESTRATOR_RUNNING);

        int stopped = bio_orchestrator_stop_modules_ordered(orchestrator_);
        EXPECT_EQ(stopped, 1);
        EXPECT_EQ(bio_orchestrator_get_state(orchestrator_), BIO_ORCHESTRATOR_STOPPED);
    }
}
