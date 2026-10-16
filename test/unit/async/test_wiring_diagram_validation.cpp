/**
 * @file test_wiring_diagram_validation.cpp
 * @brief Unit tests for Phase 9: Wiring Diagram Validation and Dependency Resolution
 *
 * Tests the validation and dependency resolution functions:
 * - wiring_diagram_validate: Configuration validation
 * - wiring_diagram_check_circular_deps: Cycle detection
 * - wiring_diagram_get_startup_order: Topological sort
 * - wiring_diagram_get_dependency_chain: Transitive dependencies
 * - wiring_diagram_get_dependents: Reverse dependencies
 * - wiring_diagram_can_disable_module: Safe disable check
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <filesystem>
#include <algorithm>

#include "async/nimcp_wiring_diagram.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WiringDiagramValidationTest : public ::testing::Test {
protected:
    wiring_diagram_t* wd_ = nullptr;
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

        // Create temporary directory for test wiring files
        temp_dir_ = "/tmp/nimcp_wiring_validation_test_" + std::to_string(getpid());
        std::filesystem::create_directories(temp_dir_);
        std::filesystem::create_directories(temp_dir_ + "/subsystems");

        // Create wiring diagram
        wd_ = wiring_diagram_create(temp_dir_.c_str());
        ASSERT_NE(wd_, nullptr);
        wiring_diagram_set_auto_persist(wd_, false);
    }

    void TearDown() override {
        if (wd_) {
            wiring_diagram_destroy(wd_);
            wd_ = nullptr;
        }

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        // Clean up temp directory
        if (!temp_dir_.empty()) {
            std::filesystem::remove_all(temp_dir_);
        }
    }

    // Helper: Add a module with dependencies
    int AddModuleWithDeps(bio_module_id_t id, const char* name,
                          const std::vector<bio_module_id_t>& deps) {
        wiring_module_config_t config;
        wiring_module_config_init(&config);
        strncpy(config.module_name, name, sizeof(config.module_name) - 1);
        config.module_id = id;
        config.subsystem = WIRING_SUBSYSTEM_CORE;
        config.min_tier = PLATFORM_TIER_MINIMAL;
        config.enabled = true;

        // Add dependencies
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

        return wiring_diagram_add_module(wd_, name, &config);
    }

    // Helper: Add simple module without dependencies
    int AddSimpleModule(bio_module_id_t id, const char* name, bool enabled = true) {
        wiring_module_config_t config;
        wiring_module_config_init(&config);
        strncpy(config.module_name, name, sizeof(config.module_name) - 1);
        config.module_id = id;
        config.subsystem = WIRING_SUBSYSTEM_CORE;
        config.min_tier = PLATFORM_TIER_MINIMAL;
        config.enabled = enabled;

        return wiring_diagram_add_module(wd_, name, &config);
    }
};

//=============================================================================
// VALIDATION TESTS
//=============================================================================

/**
 * @test Validate null parameters
 */
TEST_F(WiringDiagramValidationTest, Validate_NullParams) {
    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(nullptr, &result), -1);
    EXPECT_EQ(wiring_diagram_validate(wd_, nullptr), -1);
}

/**
 * @test Validate empty diagram
 */
TEST_F(WiringDiagramValidationTest, Validate_EmptyDiagram) {
    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), 0);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.error_count, 0u);
    EXPECT_EQ(result.warning_count, 0u);
}

/**
 * @test Validate single module
 */
TEST_F(WiringDiagramValidationTest, Validate_SingleModule) {
    AddSimpleModule(BIO_MODULE_ATTENTION, "attention");

    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), 0);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.error_count, 0u);
}

/**
 * @test Validate detects duplicate module IDs
 */
TEST_F(WiringDiagramValidationTest, Validate_DuplicateModuleIDs) {
    AddSimpleModule(BIO_MODULE_ATTENTION, "attention1");

    // Add another module with same ID but different name
    wiring_module_config_t config;
    wiring_module_config_init(&config);
    strncpy(config.module_name, "attention2", sizeof(config.module_name) - 1);
    config.module_id = BIO_MODULE_ATTENTION;  // Duplicate!
    config.enabled = true;
    wiring_diagram_add_module(wd_, "attention2", &config);

    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), -1);
    EXPECT_FALSE(result.valid);
    EXPECT_GT(result.error_count, 0u);
}

/**
 * @test Validate detects missing dependencies
 */
TEST_F(WiringDiagramValidationTest, Validate_MissingDependencies) {
    // Add module with non-existent dependency
    AddModuleWithDeps(BIO_MODULE_ATTENTION, "attention", {static_cast<bio_module_id_t>(0x9999)});  // Non-existent

    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), -1);
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.has_missing_deps);
}

/**
 * @test Validate warns on disabled dependencies
 */
TEST_F(WiringDiagramValidationTest, Validate_DisabledDependencies) {
    AddSimpleModule(BIO_MODULE_MEMORY, "memory", false);  // Disabled
    AddModuleWithDeps(BIO_MODULE_ATTENTION, "attention", {BIO_MODULE_MEMORY});

    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), 0);  // Still valid
    EXPECT_TRUE(result.valid);
    EXPECT_GT(result.warning_count, 0u);  // But has warnings
}

/**
 * @test Validate detects circular dependencies
 */
TEST_F(WiringDiagramValidationTest, Validate_CircularDependencies) {
    // A -> B -> C -> A
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x100), "moduleA", {static_cast<bio_module_id_t>(0x102)});  // A depends on C
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x101), "moduleB", {static_cast<bio_module_id_t>(0x100)});  // B depends on A
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x102), "moduleC", {static_cast<bio_module_id_t>(0x101)});  // C depends on B

    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), -1);
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.has_circular_deps);
}

/**
 * @test Validate passes when all modules have valid names
 * Note: wiring_diagram_add_module may reject empty names at add time
 */
TEST_F(WiringDiagramValidationTest, Validate_ValidModuleNames) {
    // Add modules with valid names
    AddSimpleModule(static_cast<bio_module_id_t>(0x100), "module_a");
    AddSimpleModule(static_cast<bio_module_id_t>(0x101), "module_b");

    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), 0);
    EXPECT_TRUE(result.valid);
}

//=============================================================================
// CIRCULAR DEPENDENCY DETECTION TESTS
//=============================================================================

/**
 * @test Check circular deps - null params
 */
TEST_F(WiringDiagramValidationTest, CircularDeps_NullParams) {
    EXPECT_EQ(wiring_diagram_check_circular_deps(nullptr, nullptr, 0, nullptr), -1);
}

/**
 * @test Check circular deps - empty diagram
 */
TEST_F(WiringDiagramValidationTest, CircularDeps_EmptyDiagram) {
    EXPECT_EQ(wiring_diagram_check_circular_deps(wd_, nullptr, 0, nullptr), 0);
}

/**
 * @test Check circular deps - no cycles
 */
TEST_F(WiringDiagramValidationTest, CircularDeps_NoCycles) {
    // Linear chain: A -> B -> C
    AddSimpleModule(static_cast<bio_module_id_t>(0x100), "moduleA");
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x101), "moduleB", {static_cast<bio_module_id_t>(0x100)});
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x102), "moduleC", {static_cast<bio_module_id_t>(0x101)});

    uint32_t cycle_count = 0;
    EXPECT_EQ(wiring_diagram_check_circular_deps(wd_, nullptr, 0, &cycle_count), 0);
    EXPECT_EQ(cycle_count, 0u);
}

/**
 * @test Check circular deps - simple cycle (A -> B -> A)
 */
TEST_F(WiringDiagramValidationTest, CircularDeps_SimpleCycle) {
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x100), "moduleA", {static_cast<bio_module_id_t>(0x101)});  // A depends on B
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x101), "moduleB", {static_cast<bio_module_id_t>(0x100)});  // B depends on A

    uint32_t cycle_count = 0;
    bio_module_id_t cycle_modules[16];
    EXPECT_EQ(wiring_diagram_check_circular_deps(wd_, cycle_modules, 16, &cycle_count), -1);
    EXPECT_GT(cycle_count, 0u);
}

/**
 * @test Check circular deps - self-dependency
 */
TEST_F(WiringDiagramValidationTest, CircularDeps_SelfDependency) {
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x100), "moduleA", {static_cast<bio_module_id_t>(0x100)});  // A depends on itself

    EXPECT_EQ(wiring_diagram_check_circular_deps(wd_, nullptr, 0, nullptr), -1);
}

/**
 * @test Check circular deps - complex graph with no cycle
 */
TEST_F(WiringDiagramValidationTest, CircularDeps_ComplexNoCycle) {
    // Diamond pattern: A depends on B and C, both depend on D
    AddSimpleModule(static_cast<bio_module_id_t>(0x100), "moduleD");
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x101), "moduleB", {static_cast<bio_module_id_t>(0x100)});
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x102), "moduleC", {static_cast<bio_module_id_t>(0x100)});
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x103), "moduleA", {static_cast<bio_module_id_t>(0x101), static_cast<bio_module_id_t>(0x102)});

    EXPECT_EQ(wiring_diagram_check_circular_deps(wd_, nullptr, 0, nullptr), 0);
}

//=============================================================================
// STARTUP ORDER TESTS
//=============================================================================

/**
 * @test Startup order - null params
 */
TEST_F(WiringDiagramValidationTest, StartupOrder_NullParams) {
    bio_module_id_t order[10];
    EXPECT_EQ(wiring_diagram_get_startup_order(nullptr, order, 10), -1);
    EXPECT_EQ(wiring_diagram_get_startup_order(wd_, nullptr, 10), -1);
    EXPECT_EQ(wiring_diagram_get_startup_order(wd_, order, 0), -1);
}

/**
 * @test Startup order - empty diagram
 */
TEST_F(WiringDiagramValidationTest, StartupOrder_EmptyDiagram) {
    bio_module_id_t order[10];
    EXPECT_EQ(wiring_diagram_get_startup_order(wd_, order, 10), 0);
}

/**
 * @test Startup order - single module
 */
TEST_F(WiringDiagramValidationTest, StartupOrder_SingleModule) {
    AddSimpleModule(static_cast<bio_module_id_t>(0x100), "moduleA");

    bio_module_id_t order[10];
    int count = wiring_diagram_get_startup_order(wd_, order, 10);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(order[0], static_cast<bio_module_id_t>(0x100));
}

/**
 * @test Startup order - linear chain
 */
TEST_F(WiringDiagramValidationTest, StartupOrder_LinearChain) {
    // A -> B -> C (C depends on B depends on A)
    AddSimpleModule(static_cast<bio_module_id_t>(0x100), "moduleA");
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x101), "moduleB", {static_cast<bio_module_id_t>(0x100)});
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x102), "moduleC", {static_cast<bio_module_id_t>(0x101)});

    bio_module_id_t order[10];
    int count = wiring_diagram_get_startup_order(wd_, order, 10);
    EXPECT_EQ(count, 3);

    // Find positions
    int pos_a = -1, pos_b = -1, pos_c = -1;
    for (int i = 0; i < count; i++) {
        if (order[i] == static_cast<bio_module_id_t>(0x100)) pos_a = i;
        if (order[i] == static_cast<bio_module_id_t>(0x101)) pos_b = i;
        if (order[i] == static_cast<bio_module_id_t>(0x102)) pos_c = i;
    }

    // A must start before B, B before C
    EXPECT_LT(pos_a, pos_b);
    EXPECT_LT(pos_b, pos_c);
}

/**
 * @test Startup order - diamond pattern
 */
TEST_F(WiringDiagramValidationTest, StartupOrder_DiamondPattern) {
    // D <- B <- A
    //   \- C <-/
    AddSimpleModule(static_cast<bio_module_id_t>(0x100), "moduleD");
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x101), "moduleB", {static_cast<bio_module_id_t>(0x100)});
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x102), "moduleC", {static_cast<bio_module_id_t>(0x100)});
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x103), "moduleA", {static_cast<bio_module_id_t>(0x101), static_cast<bio_module_id_t>(0x102)});

    bio_module_id_t order[10];
    int count = wiring_diagram_get_startup_order(wd_, order, 10);
    EXPECT_EQ(count, 4);

    // D must start first, A must start last
    EXPECT_EQ(order[0], static_cast<bio_module_id_t>(0x100));  // D first (no deps)
    EXPECT_EQ(order[3], static_cast<bio_module_id_t>(0x103));  // A last (depends on B and C)
}

/**
 * @test Startup order - fails on cycle
 */
TEST_F(WiringDiagramValidationTest, StartupOrder_FailsOnCycle) {
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x100), "moduleA", {static_cast<bio_module_id_t>(0x101)});
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x101), "moduleB", {static_cast<bio_module_id_t>(0x100)});

    bio_module_id_t order[10];
    EXPECT_EQ(wiring_diagram_get_startup_order(wd_, order, 10), -1);
}

//=============================================================================
// DEPENDENCY CHAIN TESTS
//=============================================================================

/**
 * @test Dependency chain - null params
 */
TEST_F(WiringDiagramValidationTest, DependencyChain_NullParams) {
    bio_module_id_t deps[10];
    EXPECT_EQ(wiring_diagram_get_dependency_chain(nullptr, static_cast<bio_module_id_t>(0x100), deps, 10), -1);
    EXPECT_EQ(wiring_diagram_get_dependency_chain(wd_, static_cast<bio_module_id_t>(0x100), nullptr, 10), -1);
    EXPECT_EQ(wiring_diagram_get_dependency_chain(wd_, static_cast<bio_module_id_t>(0x100), deps, 0), -1);
}

/**
 * @test Dependency chain - module not found
 */
TEST_F(WiringDiagramValidationTest, DependencyChain_NotFound) {
    bio_module_id_t deps[10];
    EXPECT_EQ(wiring_diagram_get_dependency_chain(wd_, static_cast<bio_module_id_t>(0x9999), deps, 10), -1);
}

/**
 * @test Dependency chain - no dependencies
 */
TEST_F(WiringDiagramValidationTest, DependencyChain_NoDeps) {
    AddSimpleModule(static_cast<bio_module_id_t>(0x100), "moduleA");

    bio_module_id_t deps[10];
    int count = wiring_diagram_get_dependency_chain(wd_, static_cast<bio_module_id_t>(0x100), deps, 10);
    EXPECT_EQ(count, 0);
}

/**
 * @test Dependency chain - transitive dependencies
 */
TEST_F(WiringDiagramValidationTest, DependencyChain_Transitive) {
    // A -> B -> C -> D
    AddSimpleModule(static_cast<bio_module_id_t>(0x100), "moduleD");
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x101), "moduleC", {static_cast<bio_module_id_t>(0x100)});
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x102), "moduleB", {static_cast<bio_module_id_t>(0x101)});
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x103), "moduleA", {static_cast<bio_module_id_t>(0x102)});

    bio_module_id_t deps[10];
    int count = wiring_diagram_get_dependency_chain(wd_, static_cast<bio_module_id_t>(0x103), deps, 10);
    EXPECT_EQ(count, 3);  // B, C, D are all dependencies of A

    // Verify all deps are present
    std::vector<bio_module_id_t> dep_vec(deps, deps + count);
    EXPECT_TRUE(std::find(dep_vec.begin(), dep_vec.end(), static_cast<bio_module_id_t>(0x100)) != dep_vec.end());
    EXPECT_TRUE(std::find(dep_vec.begin(), dep_vec.end(), static_cast<bio_module_id_t>(0x101)) != dep_vec.end());
    EXPECT_TRUE(std::find(dep_vec.begin(), dep_vec.end(), static_cast<bio_module_id_t>(0x102)) != dep_vec.end());
}

//=============================================================================
// DEPENDENTS TESTS
//=============================================================================

/**
 * @test Get dependents - null params
 */
TEST_F(WiringDiagramValidationTest, Dependents_NullParams) {
    bio_module_id_t deps[10];
    EXPECT_EQ(wiring_diagram_get_dependents(nullptr, static_cast<bio_module_id_t>(0x100), deps, 10), -1);
    EXPECT_EQ(wiring_diagram_get_dependents(wd_, static_cast<bio_module_id_t>(0x100), nullptr, 10), -1);
    EXPECT_EQ(wiring_diagram_get_dependents(wd_, static_cast<bio_module_id_t>(0x100), deps, 0), -1);
}

/**
 * @test Get dependents - no dependents
 */
TEST_F(WiringDiagramValidationTest, Dependents_NoDependents) {
    AddSimpleModule(static_cast<bio_module_id_t>(0x100), "moduleA");

    bio_module_id_t deps[10];
    int count = wiring_diagram_get_dependents(wd_, static_cast<bio_module_id_t>(0x100), deps, 10);
    EXPECT_EQ(count, 0);
}

/**
 * @test Get dependents - multiple dependents
 */
TEST_F(WiringDiagramValidationTest, Dependents_MultipleDependents) {
    // A is depended on by B and C
    AddSimpleModule(static_cast<bio_module_id_t>(0x100), "moduleA");
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x101), "moduleB", {static_cast<bio_module_id_t>(0x100)});
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x102), "moduleC", {static_cast<bio_module_id_t>(0x100)});

    bio_module_id_t deps[10];
    int count = wiring_diagram_get_dependents(wd_, static_cast<bio_module_id_t>(0x100), deps, 10);
    EXPECT_EQ(count, 2);

    std::vector<bio_module_id_t> dep_vec(deps, deps + count);
    EXPECT_TRUE(std::find(dep_vec.begin(), dep_vec.end(), static_cast<bio_module_id_t>(0x101)) != dep_vec.end());
    EXPECT_TRUE(std::find(dep_vec.begin(), dep_vec.end(), static_cast<bio_module_id_t>(0x102)) != dep_vec.end());
}

//=============================================================================
// CAN DISABLE MODULE TESTS
//=============================================================================

/**
 * @test Can disable - null params
 */
TEST_F(WiringDiagramValidationTest, CanDisable_NullParams) {
    EXPECT_EQ(wiring_diagram_can_disable_module(nullptr, static_cast<bio_module_id_t>(0x100)), -1);
}

/**
 * @test Can disable - no dependents
 */
TEST_F(WiringDiagramValidationTest, CanDisable_NoDependents) {
    AddSimpleModule(static_cast<bio_module_id_t>(0x100), "moduleA");

    EXPECT_EQ(wiring_diagram_can_disable_module(wd_, static_cast<bio_module_id_t>(0x100)), 1);  // Safe
}

/**
 * @test Can disable - has enabled dependents
 */
TEST_F(WiringDiagramValidationTest, CanDisable_HasEnabledDependents) {
    AddSimpleModule(static_cast<bio_module_id_t>(0x100), "moduleA");
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x101), "moduleB", {static_cast<bio_module_id_t>(0x100)});

    EXPECT_EQ(wiring_diagram_can_disable_module(wd_, static_cast<bio_module_id_t>(0x100)), 0);  // Not safe
}

/**
 * @test Can disable - has only disabled dependents
 */
TEST_F(WiringDiagramValidationTest, CanDisable_DisabledDependentsOnly) {
    AddSimpleModule(static_cast<bio_module_id_t>(0x100), "moduleA");

    // Add disabled dependent
    wiring_module_config_t config;
    wiring_module_config_init(&config);
    strncpy(config.module_name, "moduleB", sizeof(config.module_name) - 1);
    config.module_id = static_cast<bio_module_id_t>(0x101);
    config.enabled = false;  // Disabled
    config.depends_on = (bio_module_id_t*)nimcp_calloc(1, sizeof(bio_module_id_t));
    config.depends_on[0] = static_cast<bio_module_id_t>(0x100);
    config.depends_on_count = 1;
    wiring_diagram_add_module(wd_, "moduleB", &config);

    EXPECT_EQ(wiring_diagram_can_disable_module(wd_, static_cast<bio_module_id_t>(0x100)), 1);  // Safe
}

//=============================================================================
// EDGE CASE TESTS
//=============================================================================

/**
 * @test Large number of modules
 */
TEST_F(WiringDiagramValidationTest, EdgeCase_ManyModules) {
    const int MODULE_COUNT = 100;

    // Linear chain of 100 modules
    AddSimpleModule(static_cast<bio_module_id_t>(0), "module_0");
    for (int i = 1; i < MODULE_COUNT; i++) {
        std::string name = "module_" + std::to_string(i);
        AddModuleWithDeps(static_cast<bio_module_id_t>(i), name.c_str(),
            {static_cast<bio_module_id_t>(i-1)});
    }

    // Validation should pass
    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), 0);
    EXPECT_TRUE(result.valid);

    // Startup order should work
    bio_module_id_t order[MODULE_COUNT];
    int count = wiring_diagram_get_startup_order(wd_, order, MODULE_COUNT);
    EXPECT_EQ(count, MODULE_COUNT);

    // Order should be 0, 1, 2, ... MODULE_COUNT-1
    for (int i = 0; i < count; i++) {
        EXPECT_EQ(order[i], (bio_module_id_t)i);
    }
}

/**
 * @test Module with many dependencies
 */
TEST_F(WiringDiagramValidationTest, EdgeCase_ManyDependencies) {
    const int DEP_COUNT = 20;

    // Create 20 independent modules
    for (int i = 0; i < DEP_COUNT; i++) {
        std::string name = "dep_" + std::to_string(i);
        AddSimpleModule((bio_module_id_t)(static_cast<bio_module_id_t>(0x100) + i), name.c_str());
    }

    // Create one module that depends on all 20
    std::vector<bio_module_id_t> deps;
    for (int i = 0; i < DEP_COUNT; i++) {
        deps.push_back(static_cast<bio_module_id_t>(static_cast<bio_module_id_t>(0x100) + i));
    }
    AddModuleWithDeps(static_cast<bio_module_id_t>(0x200), "dependent", deps);

    // Validation should pass
    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), 0);
    EXPECT_TRUE(result.valid);

    // Dependency chain should find all 20
    bio_module_id_t chain[64];
    int count = wiring_diagram_get_dependency_chain(wd_, static_cast<bio_module_id_t>(0x200), chain, 64);
    EXPECT_EQ(count, DEP_COUNT);
}
