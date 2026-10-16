/**
 * @file test_wiring_diagram_validation_integration.cpp
 * @brief Integration tests for Phase 9: Wiring Diagram Validation
 *
 * Tests validation in context of real orchestrator and brain_kg usage:
 * - Validation before orchestrator startup
 * - Startup ordering with real modules
 * - Dependency resolution with KG sync
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
#include "core/brain/nimcp_brain_kg.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WiringValidationIntegrationTest : public ::testing::Test {
protected:
    wiring_diagram_t* wd_ = nullptr;
    brain_kg_t* kg_ = nullptr;
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
        temp_dir_ = "/tmp/nimcp_wiring_valid_int_" + std::to_string(getpid());
        std::filesystem::create_directories(temp_dir_);
        std::filesystem::create_directories(temp_dir_ + "/subsystems");

        // Create wiring diagram
        wd_ = wiring_diagram_create(temp_dir_.c_str());
        ASSERT_NE(wd_, nullptr);
        wiring_diagram_set_auto_persist(wd_, false);

        // Create brain KG
        brain_kg_config_t kg_config;
        brain_kg_default_config(&kg_config);
        kg_config.enable_security = false;
        kg_ = brain_kg_create(&kg_config);
        ASSERT_NE(kg_, nullptr);

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

    // Helper: Add module with deps
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
};

//=============================================================================
// ORCHESTRATOR INTEGRATION TESTS
//=============================================================================

/**
 * @test Validation before orchestrator startup
 */
TEST_F(WiringValidationIntegrationTest, ValidateBeforeOrchestratorStart) {
    // Add valid module hierarchy
    AddModule(static_cast<bio_module_id_t>(0x100), "base");
    AddModule(static_cast<bio_module_id_t>(0x101), "layer1", {static_cast<bio_module_id_t>(0x100)});
    AddModule(static_cast<bio_module_id_t>(0x102), "layer2", {static_cast<bio_module_id_t>(0x101)});

    // Validate before connecting to orchestrator
    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), 0);
    EXPECT_TRUE(result.valid);
    EXPECT_FALSE(result.has_circular_deps);
    EXPECT_FALSE(result.has_missing_deps);

    // Connect wiring to orchestrator
    EXPECT_EQ(bio_orchestrator_set_wiring_diagram(orchestrator_, wd_), 0);
}

/**
 * @test Startup order matches dependency order
 */
TEST_F(WiringValidationIntegrationTest, StartupOrderMatchesDependencies) {
    // Create dependency chain
    AddModule(static_cast<bio_module_id_t>(0x100), "foundation");
    AddModule(static_cast<bio_module_id_t>(0x101), "middleware", {static_cast<bio_module_id_t>(0x100)});
    AddModule(static_cast<bio_module_id_t>(0x102), "application", {static_cast<bio_module_id_t>(0x101)});

    // Get startup order
    bio_module_id_t order[10];
    int count = wiring_diagram_get_startup_order(wd_, order, 10);
    EXPECT_EQ(count, 3);

    // Verify order: foundation -> middleware -> application
    EXPECT_EQ(order[0], static_cast<bio_module_id_t>(0x100));
    EXPECT_EQ(order[1], static_cast<bio_module_id_t>(0x101));
    EXPECT_EQ(order[2], static_cast<bio_module_id_t>(0x102));
}

/**
 * @test Validation with brain_kg sync
 */
TEST_F(WiringValidationIntegrationTest, ValidateWithKGSync) {
    // Add modules
    AddModule(static_cast<bio_module_id_t>(0x100), "sensor");
    AddModule(static_cast<bio_module_id_t>(0x101), "processor", {static_cast<bio_module_id_t>(0x100)});

    // Validate before sync
    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), 0);

    // Sync to KG
    int synced = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_GE(synced, 0);

    // Validate after sync (should still be valid)
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), 0);
    EXPECT_TRUE(result.valid);
}

/**
 * @test Dependency chain with KG-registered handlers
 */
TEST_F(WiringValidationIntegrationTest, DependencyChainWithHandlers) {
    // Add modules with handlers
    wiring_module_config_t config;
    wiring_module_config_init(&config);
    strncpy(config.module_name, "handler_module", sizeof(config.module_name) - 1);
    config.module_id = static_cast<bio_module_id_t>(0x100);
    config.enabled = true;

    // Add message handlers
    config.handles_messages = (bio_message_type_t*)nimcp_calloc(3, sizeof(bio_message_type_t));
    config.handles_messages[0] = static_cast<bio_message_type_t>(0x1000);
    config.handles_messages[1] = static_cast<bio_message_type_t>(0x1001);
    config.handles_messages[2] = static_cast<bio_message_type_t>(0x1002);
    config.handles_message_count = 3;
    config.handles_message_capacity = 3;

    wiring_diagram_add_module(wd_, "handler_module", &config);

    // Add dependent module
    AddModule(static_cast<bio_module_id_t>(0x101), "consumer", {static_cast<bio_module_id_t>(0x100)});

    // Validate
    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), 0);

    // Get dependency chain
    bio_module_id_t deps[10];
    int count = wiring_diagram_get_dependency_chain(wd_, static_cast<bio_module_id_t>(0x101), deps, 10);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(deps[0], static_cast<bio_module_id_t>(0x100));

    // Sync handlers to KG
    int synced = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(synced, 3);  // 3 handlers registered
}

/**
 * @test Complex dependency graph validation
 */
TEST_F(WiringValidationIntegrationTest, ComplexDependencyGraph) {
    // Create complex but valid dependency graph
    //     D
    //    / \
    //   B   C
    //    \ /
    //     A (root, no deps)
    AddModule(static_cast<bio_module_id_t>(0x100), "root_a");
    AddModule(static_cast<bio_module_id_t>(0x101), "branch_b", {static_cast<bio_module_id_t>(0x100)});
    AddModule(static_cast<bio_module_id_t>(0x102), "branch_c", {static_cast<bio_module_id_t>(0x100)});
    AddModule(static_cast<bio_module_id_t>(0x103), "top_d", {static_cast<bio_module_id_t>(0x101), static_cast<bio_module_id_t>(0x102)});

    // Validate
    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), 0);
    EXPECT_TRUE(result.valid);

    // Check startup order
    bio_module_id_t order[10];
    int count = wiring_diagram_get_startup_order(wd_, order, 10);
    EXPECT_EQ(count, 4);

    // A must be first
    EXPECT_EQ(order[0], static_cast<bio_module_id_t>(0x100));

    // D must be last
    EXPECT_EQ(order[3], static_cast<bio_module_id_t>(0x103));

    // Get full dependency chain for D
    bio_module_id_t deps[10];
    int dep_count = wiring_diagram_get_dependency_chain(wd_, static_cast<bio_module_id_t>(0x103), deps, 10);
    EXPECT_EQ(dep_count, 3);  // B, C, A
}

/**
 * @test Safe module disable check with orchestrator
 */
TEST_F(WiringValidationIntegrationTest, SafeDisableWithOrchestrator) {
    AddModule(static_cast<bio_module_id_t>(0x100), "core_service");
    AddModule(static_cast<bio_module_id_t>(0x101), "optional_feature", {static_cast<bio_module_id_t>(0x100)});

    // Connect to orchestrator
    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    // Cannot disable core_service (has enabled dependent)
    EXPECT_EQ(wiring_diagram_can_disable_module(wd_, static_cast<bio_module_id_t>(0x100)), 0);

    // Can disable optional_feature (no dependents)
    EXPECT_EQ(wiring_diagram_can_disable_module(wd_, static_cast<bio_module_id_t>(0x101)), 1);
}

/**
 * @test Validation error propagation
 */
TEST_F(WiringValidationIntegrationTest, ValidationErrorDetails) {
    // Add module with circular dependency
    AddModule(static_cast<bio_module_id_t>(0x100), "module_a", {static_cast<bio_module_id_t>(0x101)});
    AddModule(static_cast<bio_module_id_t>(0x101), "module_b", {static_cast<bio_module_id_t>(0x100)});

    wiring_validation_result_t result;
    EXPECT_EQ(wiring_diagram_validate(wd_, &result), -1);
    EXPECT_FALSE(result.valid);
    EXPECT_TRUE(result.has_circular_deps);
    EXPECT_GT(result.error_count, 0u);

    // Error message should mention circular dependency
    bool found_cycle_error = false;
    for (uint32_t i = 0; i < result.error_count; i++) {
        if (strstr(result.errors[i], "ircular") != nullptr) {
            found_cycle_error = true;
            break;
        }
    }
    EXPECT_TRUE(found_cycle_error);
}

/**
 * @test Full integration: validate, order, sync, dispatch
 */
TEST_F(WiringValidationIntegrationTest, FullIntegrationWorkflow) {
    // Step 1: Add modules
    AddModule(static_cast<bio_module_id_t>(0x100), "input_handler");
    AddModule(static_cast<bio_module_id_t>(0x101), "processor", {static_cast<bio_module_id_t>(0x100)});
    AddModule(static_cast<bio_module_id_t>(0x102), "output_handler", {static_cast<bio_module_id_t>(0x101)});

    // Step 2: Validate
    wiring_validation_result_t result;
    ASSERT_EQ(wiring_diagram_validate(wd_, &result), 0);
    ASSERT_TRUE(result.valid);

    // Step 3: Get startup order
    bio_module_id_t order[10];
    int count = wiring_diagram_get_startup_order(wd_, order, 10);
    ASSERT_EQ(count, 3);

    // Step 4: Connect to orchestrator
    ASSERT_EQ(bio_orchestrator_set_wiring_diagram(orchestrator_, wd_), 0);

    // Step 5: Sync to KG
    int synced = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_GE(synced, 0);

    // Step 6: Verify dependents
    bio_module_id_t dependents[10];
    int dep_count = wiring_diagram_get_dependents(wd_, static_cast<bio_module_id_t>(0x100), dependents, 10);
    EXPECT_EQ(dep_count, 1);  // processor depends on input_handler
}
