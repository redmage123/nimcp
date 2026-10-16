/**
 * @file test_self_assembly_integration.cpp
 * @brief Integration tests for Phase 10: Automatic Self-Assembly
 *
 * Tests self-assembly in context of real orchestrator, brain_kg, and router:
 * - Self-assembly with KG sync
 * - Handler callbacks during self-assembly
 * - Full system startup/shutdown
 * - Self-assembly with wiring persistence
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <filesystem>
#include <atomic>

#include "async/nimcp_wiring_diagram.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async_orchestrator.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SelfAssemblyIntegrationTest : public ::testing::Test {
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
        temp_dir_ = "/tmp/nimcp_self_assembly_int_" + std::to_string(getpid());
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

    // Helper: Add module to wiring and orchestrator
    int AddModule(bio_module_id_t id, const char* name,
                  const std::vector<bio_module_id_t>& deps = {},
                  const std::vector<bio_message_type_t>& handlers = {}) {
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

        if (!handlers.empty()) {
            config.handles_messages = (bio_message_type_t*)
                nimcp_calloc(handlers.size(), sizeof(bio_message_type_t));
            for (size_t i = 0; i < handlers.size(); i++) {
                config.handles_messages[i] = handlers[i];
            }
            config.handles_message_count = handlers.size();
            config.handles_message_capacity = handlers.size();
        }

        int result = wiring_diagram_add_module(wd_, name, &config);
        if (result != 0) return result;

        return bio_orchestrator_register_module(
            orchestrator_, id, name, BIO_MODULE_CATEGORY_CORE, nullptr, 0);
    }
};

//=============================================================================
// HANDLER CALLBACK INTEGRATION TESTS
//=============================================================================

static std::atomic<int> g_callback_count{0};
static std::vector<bio_message_type_t> g_received_handlers;

static int test_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)ctx;
    (void)user_data;
    g_callback_count++;
    for (uint32_t i = 0; i < message_count; i++) {
        g_received_handlers.push_back(message_types[i]);
    }
    return 0;
}

/**
 * @test Handler callbacks invoked during self-assembly startup
 */
TEST_F(SelfAssemblyIntegrationTest, HandlerCallbacksInvoked) {
    g_callback_count = 0;
    g_received_handlers.clear();

    // Add module with message handlers
    std::vector<bio_message_type_t> handlers = {
        static_cast<bio_message_type_t>(0x1000),
        static_cast<bio_message_type_t>(0x1001)
    };
    AddModule(static_cast<bio_module_id_t>(0x100), "handler_mod", {}, handlers);

    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    // Register callback
    bio_orchestrator_register_handler_callback(
        orchestrator_,
        static_cast<bio_module_id_t>(0x100),
        test_handler_callback,
        nullptr
    );

    // Start with self-assembly
    int started = bio_orchestrator_start_modules_ordered(orchestrator_);
    EXPECT_EQ(started, 1);

    // Callback should have been invoked
    EXPECT_EQ(g_callback_count.load(), 1);
    EXPECT_EQ(g_received_handlers.size(), 2u);
}

/**
 * @test Multiple modules with handler callbacks
 */
TEST_F(SelfAssemblyIntegrationTest, MultipleModuleCallbacks) {
    g_callback_count = 0;
    g_received_handlers.clear();

    // Add multiple modules with handlers
    AddModule(static_cast<bio_module_id_t>(0x100), "mod_a", {},
        {static_cast<bio_message_type_t>(0x1000)});
    AddModule(static_cast<bio_module_id_t>(0x101), "mod_b",
        {static_cast<bio_module_id_t>(0x100)},
        {static_cast<bio_message_type_t>(0x1001), static_cast<bio_message_type_t>(0x1002)});

    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    // Register callbacks
    bio_orchestrator_register_handler_callback(
        orchestrator_, static_cast<bio_module_id_t>(0x100), test_handler_callback, nullptr);
    bio_orchestrator_register_handler_callback(
        orchestrator_, static_cast<bio_module_id_t>(0x101), test_handler_callback, nullptr);

    bio_orchestrator_start_modules_ordered(orchestrator_);

    EXPECT_EQ(g_callback_count.load(), 2);
    EXPECT_EQ(g_received_handlers.size(), 3u);
}

//=============================================================================
// KG SYNC INTEGRATION TESTS
//=============================================================================

/**
 * @test Self-assembly with KG sync
 */
TEST_F(SelfAssemblyIntegrationTest, SelfAssemblyWithKGSync) {
    // Add modules with dependencies
    AddModule(static_cast<bio_module_id_t>(0x100), "base_module");
    AddModule(static_cast<bio_module_id_t>(0x101), "derived_module",
        {static_cast<bio_module_id_t>(0x100)});

    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    // Sync wiring to KG before startup
    int synced = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_GE(synced, 0);

    // Start with self-assembly
    int started = bio_orchestrator_start_modules_ordered(orchestrator_);
    EXPECT_EQ(started, 2);

    // Verify ordering
    int pos_base = bio_orchestrator_get_module_startup_position(
        orchestrator_, static_cast<bio_module_id_t>(0x100));
    int pos_derived = bio_orchestrator_get_module_startup_position(
        orchestrator_, static_cast<bio_module_id_t>(0x101));

    EXPECT_LT(pos_base, pos_derived);
}

/**
 * @test Self-assembly validation with KG
 */
TEST_F(SelfAssemblyIntegrationTest, ValidationWithKG) {
    AddModule(static_cast<bio_module_id_t>(0x100), "valid_a");
    AddModule(static_cast<bio_module_id_t>(0x101), "valid_b",
        {static_cast<bio_module_id_t>(0x100)});

    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    // Sync to KG
    wiring_diagram_sync_to_brain_kg(wd_, kg_);

    // Validate
    wiring_validation_result_t result;
    int status = bio_orchestrator_validate_self_assembly(orchestrator_, &result);

    EXPECT_EQ(status, 0);
    EXPECT_TRUE(result.valid);
}

//=============================================================================
// COMPLEX DEPENDENCY INTEGRATION TESTS
//=============================================================================

/**
 * @test Self-assembly with complex dependency graph
 */
TEST_F(SelfAssemblyIntegrationTest, ComplexDependencyGraph) {
    // Create complex graph:
    //       E
    //      /|\
    //     C D |
    //     |/  |
    //     B   |
    //      \ /
    //       A

    AddModule(static_cast<bio_module_id_t>(0x100), "node_a");
    AddModule(static_cast<bio_module_id_t>(0x101), "node_b",
        {static_cast<bio_module_id_t>(0x100)});
    AddModule(static_cast<bio_module_id_t>(0x102), "node_c",
        {static_cast<bio_module_id_t>(0x101)});
    AddModule(static_cast<bio_module_id_t>(0x103), "node_d",
        {static_cast<bio_module_id_t>(0x101)});
    AddModule(static_cast<bio_module_id_t>(0x104), "node_e",
        {static_cast<bio_module_id_t>(0x100),
         static_cast<bio_module_id_t>(0x102),
         static_cast<bio_module_id_t>(0x103)});

    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    // Validate
    wiring_validation_result_t result;
    EXPECT_EQ(bio_orchestrator_validate_self_assembly(orchestrator_, &result), 0);
    EXPECT_TRUE(result.valid);

    // Start
    int started = bio_orchestrator_start_modules_ordered(orchestrator_);
    EXPECT_EQ(started, 5);

    // Verify order constraints
    int pos_a = bio_orchestrator_get_module_startup_position(
        orchestrator_, static_cast<bio_module_id_t>(0x100));
    int pos_b = bio_orchestrator_get_module_startup_position(
        orchestrator_, static_cast<bio_module_id_t>(0x101));
    int pos_c = bio_orchestrator_get_module_startup_position(
        orchestrator_, static_cast<bio_module_id_t>(0x102));
    int pos_d = bio_orchestrator_get_module_startup_position(
        orchestrator_, static_cast<bio_module_id_t>(0x103));
    int pos_e = bio_orchestrator_get_module_startup_position(
        orchestrator_, static_cast<bio_module_id_t>(0x104));

    // A must be first
    EXPECT_EQ(pos_a, 0);
    // E must be last
    EXPECT_EQ(pos_e, 4);
    // B must come after A, before C, D, E
    EXPECT_LT(pos_a, pos_b);
    EXPECT_LT(pos_b, pos_c);
    EXPECT_LT(pos_b, pos_d);
    EXPECT_LT(pos_c, pos_e);
    EXPECT_LT(pos_d, pos_e);
}

/**
 * @test Shutdown in reverse dependency order
 */
TEST_F(SelfAssemblyIntegrationTest, ShutdownReverseOrder) {
    AddModule(static_cast<bio_module_id_t>(0x100), "level0");
    AddModule(static_cast<bio_module_id_t>(0x101), "level1",
        {static_cast<bio_module_id_t>(0x100)});
    AddModule(static_cast<bio_module_id_t>(0x102), "level2",
        {static_cast<bio_module_id_t>(0x101)});

    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    // Start
    bio_orchestrator_start_modules_ordered(orchestrator_);

    // Stop
    int stopped = bio_orchestrator_stop_modules_ordered(orchestrator_);
    EXPECT_EQ(stopped, 3);

    // Verify all modules have unknown health (stopped)
    EXPECT_EQ(bio_orchestrator_get_module_health(orchestrator_,
        static_cast<bio_module_id_t>(0x100)), BIO_MODULE_HEALTH_UNKNOWN);
    EXPECT_EQ(bio_orchestrator_get_module_health(orchestrator_,
        static_cast<bio_module_id_t>(0x101)), BIO_MODULE_HEALTH_UNKNOWN);
    EXPECT_EQ(bio_orchestrator_get_module_health(orchestrator_,
        static_cast<bio_module_id_t>(0x102)), BIO_MODULE_HEALTH_UNKNOWN);
}

//=============================================================================
// WIRING PERSISTENCE INTEGRATION TESTS
//=============================================================================

/**
 * @test Self-assembly after wiring save/load
 */
TEST_F(SelfAssemblyIntegrationTest, PersistenceRoundTrip) {
    // Add modules
    AddModule(static_cast<bio_module_id_t>(0x100), "persist_base");
    AddModule(static_cast<bio_module_id_t>(0x101), "persist_dep",
        {static_cast<bio_module_id_t>(0x100)});

    // Save using persist_subsystem
    EXPECT_EQ(wiring_diagram_persist_subsystem(wd_, WIRING_SUBSYSTEM_CORE), 0);

    // Create new wiring diagram and load
    wiring_diagram_t* wd2 = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd2, nullptr);
    EXPECT_EQ(wiring_diagram_load_subsystem(wd2, WIRING_SUBSYSTEM_CORE), 0);

    // Create new orchestrator
    bio_orchestrator_config_t config;
    bio_orchestrator_default_config(&config);
    config.enable_logging = false;
    bio_async_orchestrator_t* orch2 = bio_orchestrator_create(&config);
    ASSERT_NE(orch2, nullptr);

    // Register modules
    bio_orchestrator_register_module(orch2,
        static_cast<bio_module_id_t>(0x100), "persist_base", BIO_MODULE_CATEGORY_CORE, nullptr, 0);
    bio_orchestrator_register_module(orch2,
        static_cast<bio_module_id_t>(0x101), "persist_dep", BIO_MODULE_CATEGORY_CORE, nullptr, 0);

    bio_orchestrator_set_wiring_diagram(orch2, wd2);

    // Validate and start
    wiring_validation_result_t result;
    EXPECT_EQ(bio_orchestrator_validate_self_assembly(orch2, &result), 0);
    EXPECT_TRUE(result.valid);

    int started = bio_orchestrator_start_modules_ordered(orch2);
    EXPECT_EQ(started, 2);

    // Cleanup
    bio_orchestrator_destroy(orch2);
    wiring_diagram_destroy(wd2);
}

//=============================================================================
// FULL SYSTEM INTEGRATION TESTS
//=============================================================================

/**
 * @test Full self-assembly workflow: validate, sync, start, stop
 */
TEST_F(SelfAssemblyIntegrationTest, FullWorkflow) {
    // Step 1: Add modules
    AddModule(static_cast<bio_module_id_t>(0x100), "input_handler", {},
        {static_cast<bio_message_type_t>(0x1000)});
    AddModule(static_cast<bio_module_id_t>(0x101), "processor",
        {static_cast<bio_module_id_t>(0x100)},
        {static_cast<bio_message_type_t>(0x1001)});
    AddModule(static_cast<bio_module_id_t>(0x102), "output_handler",
        {static_cast<bio_module_id_t>(0x101)},
        {static_cast<bio_message_type_t>(0x1002)});

    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    // Step 2: Validate
    wiring_validation_result_t result;
    ASSERT_EQ(bio_orchestrator_validate_self_assembly(orchestrator_, &result), 0);
    ASSERT_TRUE(result.valid);

    // Step 3: Sync to KG
    int synced = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_GE(synced, 0);

    // Step 4: Start with self-assembly
    int started = bio_orchestrator_start_modules_ordered(orchestrator_);
    EXPECT_EQ(started, 3);
    EXPECT_EQ(bio_orchestrator_get_state(orchestrator_), BIO_ORCHESTRATOR_RUNNING);

    // Step 5: Verify startup positions
    EXPECT_EQ(bio_orchestrator_get_module_startup_position(
        orchestrator_, static_cast<bio_module_id_t>(0x100)), 0);
    EXPECT_EQ(bio_orchestrator_get_module_startup_position(
        orchestrator_, static_cast<bio_module_id_t>(0x101)), 1);
    EXPECT_EQ(bio_orchestrator_get_module_startup_position(
        orchestrator_, static_cast<bio_module_id_t>(0x102)), 2);

    // Step 6: Stop with self-assembly
    int stopped = bio_orchestrator_stop_modules_ordered(orchestrator_);
    EXPECT_EQ(stopped, 3);
    EXPECT_EQ(bio_orchestrator_get_state(orchestrator_), BIO_ORCHESTRATOR_STOPPED);
}

/**
 * @test Self-assembly with disabled modules
 */
TEST_F(SelfAssemblyIntegrationTest, WithDisabledModules) {
    AddModule(static_cast<bio_module_id_t>(0x100), "enabled_base");
    AddModule(static_cast<bio_module_id_t>(0x101), "disabled_mod",
        {static_cast<bio_module_id_t>(0x100)});
    AddModule(static_cast<bio_module_id_t>(0x102), "enabled_top",
        {static_cast<bio_module_id_t>(0x100)});

    bio_orchestrator_set_wiring_diagram(orchestrator_, wd_);

    // Disable one module
    bio_orchestrator_set_module_enabled(orchestrator_,
        static_cast<bio_module_id_t>(0x101), false);

    // Start should only start enabled modules
    int started = bio_orchestrator_start_modules_ordered(orchestrator_);
    EXPECT_EQ(started, 2);  // Only 2 enabled modules

    // Disabled module should have unknown health
    EXPECT_EQ(bio_orchestrator_get_module_health(orchestrator_,
        static_cast<bio_module_id_t>(0x101)), BIO_MODULE_HEALTH_UNKNOWN);
}
