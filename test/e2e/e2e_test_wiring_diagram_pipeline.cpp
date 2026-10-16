/**
 * @file e2e_test_wiring_diagram_pipeline.cpp
 * @brief E2E Tests for KG-Based Wiring Diagram Pipeline
 *
 * WHAT: Complete wiring diagram pipelines from KG loading to module activation
 * WHY:  Verify the entire self-assembling module topology works end-to-end
 * HOW:  Test wiring loading, orchestrator integration, handler discovery, and message routing
 *
 * TEST PIPELINES:
 * - WiringLoadPipeline: Load wiring from JSONL files
 * - ModuleDiscoveryPipeline: Discover module configurations from KG
 * - HandlerRegistrationPipeline: Register handlers based on wiring
 * - FullStackPipeline: Complete wiring -> registration -> messaging flow
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <chrono>

#include "async/nimcp_wiring_diagram.h"
#include "async/nimcp_wiring_helpers.h"
#include "async/nimcp_bio_async_orchestrator.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WiringDiagramE2ETest : public ::testing::Test {
protected:
    wiring_diagram_t* wd_ = nullptr;
    bio_async_orchestrator_t* orch_ = nullptr;
    std::string temp_dir_;

    // Message tracking
    std::atomic<int> brain_state_queries_{0};
    std::atomic<int> health_checks_{0};
    std::atomic<int> attention_shifts_{0};

    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = true;
        bio_config.enable_logging = false;
        nimcp_error_t err = nimcp_bio_async_init(&bio_config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Bio-async initialization failed";

        // Initialize router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = true;
        router_config.enable_logging = false;
        err = bio_router_init(&router_config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Router initialization failed";

        // Create temporary directory for test wiring files
        temp_dir_ = "/tmp/nimcp_wiring_e2e_" + std::to_string(getpid());
        std::filesystem::create_directories(temp_dir_);
        std::filesystem::create_directories(temp_dir_ + "/subsystems");
        std::filesystem::create_directories(temp_dir_ + "/platforms");
        std::filesystem::create_directories(temp_dir_ + "/hardware");
        std::filesystem::create_directories(temp_dir_ + "/custom");
    }

    void TearDown() override {
        if (orch_) {
            bio_orchestrator_destroy(orch_);
            orch_ = nullptr;
        }

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

    // Helper: Create a complete wiring ecosystem
    void CreateFullWiringEcosystem() {
        // Master wiring file
        std::ofstream master(temp_dir_ + "/master.jsonl");
        master << R"({"type":"entity","name":"CoreBrain","entityType":"CognitiveModule","subsystem":"core","min_tier":0,"enabled":true})" << "\n";
        master << R"({"type":"entity","name":"AttentionModule","entityType":"CognitiveModule","subsystem":"cognition","min_tier":1,"enabled":true})" << "\n";
        master << R"({"type":"entity","name":"MemoryModule","entityType":"CognitiveModule","subsystem":"cognition","min_tier":1,"enabled":true})" << "\n";
        master << R"({"type":"relation","from":"AttentionModule","to":"CoreBrain","relationType":"DEPENDS_ON"})" << "\n";
        master << R"({"type":"relation","from":"MemoryModule","to":"CoreBrain","relationType":"DEPENDS_ON"})" << "\n";
        master.close();

        // Core subsystem with handlers
        std::ofstream core(temp_dir_ + "/subsystems/core.jsonl");
        core << R"({"type":"entity","name":"CoreBrain","entityType":"CognitiveModule","subsystem":"core"})" << "\n";
        core << R"({"type":"relation","from":"CoreBrain","to":"BIO_MSG_BRAIN_STATE_QUERY","relationType":"HANDLES_MESSAGE","message_type":1})" << "\n";
        core << R"({"type":"relation","from":"CoreBrain","to":"BIO_MSG_HEALTH_CHECK","relationType":"HANDLES_MESSAGE","message_type":2})" << "\n";
        core.close();

        // Cognition subsystem with handlers
        std::ofstream cognition(temp_dir_ + "/subsystems/cognition.jsonl");
        cognition << R"({"type":"entity","name":"AttentionModule","entityType":"CognitiveModule","subsystem":"cognition"})" << "\n";
        cognition << R"({"type":"relation","from":"AttentionModule","to":"BIO_MSG_ATTENTION_SHIFT","relationType":"HANDLES_MESSAGE","message_type":3})" << "\n";
        cognition << R"({"type":"entity","name":"MemoryModule","entityType":"CognitiveModule","subsystem":"cognition"})" << "\n";
        cognition << R"({"type":"relation","from":"MemoryModule","to":"BIO_MSG_MEMORY_QUERY","relationType":"HANDLES_MESSAGE","message_type":4})" << "\n";
        cognition.close();

        // Platform-specific wiring for FULL tier
        std::ofstream full(temp_dir_ + "/platforms/full.jsonl");
        full << R"({"type":"entity","name":"GPUAcceleratedModule","entityType":"CognitiveModule","subsystem":"core","required_hw":1})" << "\n";
        full.close();
    }

    // Helper: Initialize wiring diagram and orchestrator
    void InitializeWiringSystem() {
        wd_ = wiring_diagram_create(temp_dir_.c_str());
        ASSERT_NE(wd_, nullptr);
        wiring_diagram_set_auto_persist(wd_, false);

        bio_orchestrator_config_t orch_config;
        bio_orchestrator_default_config(&orch_config);
        orch_config.enable_statistics = true;
        orch_config.enable_auto_health_check = false;
        orch_ = bio_orchestrator_create(&orch_config);
        ASSERT_NE(orch_, nullptr);

        ASSERT_EQ(bio_orchestrator_set_wiring_diagram(orch_, wd_), 0);
    }

    // Helper: Register module with orchestrator using correct API
    int RegisterModule(bio_module_id_t id, const char* name,
                       bio_module_category_t category, uint32_t phase) {
        return bio_orchestrator_register_module(
            orch_, id, name, category, nullptr, phase);
    }
};

//=============================================================================
// E2E: Wiring Load Pipeline
//=============================================================================

TEST_F(WiringDiagramE2ETest, E2E_WiringLoadPipeline) {
    CreateFullWiringEcosystem();
    InitializeWiringSystem();

    // Step 1: Load master wiring
    int result = wiring_diagram_load_master(wd_);
    EXPECT_EQ(result, 0) << "Master wiring load should succeed";

    // Step 2: Verify core modules are loaded (get_module_config returns shallow copy)
    wiring_module_config_t config;
    memset(&config, 0, sizeof(config));
    EXPECT_EQ(wiring_diagram_get_module_config(wd_, "CoreBrain", &config), 0);
    EXPECT_EQ(wiring_diagram_get_module_config(wd_, "AttentionModule", &config), 0);
    EXPECT_EQ(wiring_diagram_get_module_config(wd_, "MemoryModule", &config), 0);
    // Note: Don't call cleanup - shallow copy, memory owned by diagram

    // Step 3: Load subsystem wiring
    EXPECT_EQ(wiring_diagram_load_subsystem(wd_, WIRING_SUBSYSTEM_CORE), 0);
    EXPECT_EQ(wiring_diagram_load_subsystem(wd_, WIRING_SUBSYSTEM_COGNITION), 0);

    // Step 4: Verify handlers are loaded (shallow copy - no cleanup needed)
    memset(&config, 0, sizeof(config));
    ASSERT_EQ(wiring_diagram_get_module_config(wd_, "CoreBrain", &config), 0);
    EXPECT_GE(config.handles_message_count, 1u) << "CoreBrain should have handlers";

    // Step 5: Check statistics
    uint32_t total, enabled, relations;
    EXPECT_EQ(wiring_diagram_get_stats(wd_, &total, &enabled, &relations), 0);
    EXPECT_GE(total, 3u) << "Should have at least 3 modules";
    EXPECT_GE(enabled, 3u) << "All modules should be enabled";
}

//=============================================================================
// E2E: Module Discovery Pipeline
//=============================================================================

TEST_F(WiringDiagramE2ETest, E2E_ModuleDiscoveryPipeline) {
    CreateFullWiringEcosystem();
    InitializeWiringSystem();

    // Load wiring
    ASSERT_EQ(wiring_diagram_load_master(wd_), 0);
    ASSERT_EQ(wiring_diagram_load_all_subsystems(wd_), 0);

    // Register modules with orchestrator
    ASSERT_EQ(RegisterModule(BIO_MODULE_BRAIN, "CoreBrain", BIO_MODULE_CATEGORY_CORE, 1), 0);
    ASSERT_EQ(RegisterModule(BIO_MODULE_ATTENTION, "AttentionModule", BIO_MODULE_CATEGORY_COGNITIVE, 2), 0);
    ASSERT_EQ(RegisterModule(BIO_MODULE_MEMORY, "MemoryModule", BIO_MODULE_CATEGORY_COGNITIVE, 2), 0);

    // Discover wiring for all modules
    int discovered = bio_orchestrator_discover_all_wiring(orch_);
    EXPECT_GE(discovered, 2) << "Should discover wiring for at least 2 modules";

    // Verify discovery worked for specific module
    int result = bio_orchestrator_discover_module_wiring(orch_, BIO_MODULE_BRAIN);
    EXPECT_EQ(result, 0) << "Should successfully discover CoreBrain wiring";
}

//=============================================================================
// E2E: Handler Registration Pipeline
//=============================================================================

TEST_F(WiringDiagramE2ETest, E2E_HandlerRegistrationPipeline) {
    CreateFullWiringEcosystem();
    InitializeWiringSystem();

    // Load wiring
    ASSERT_EQ(wiring_diagram_load_master(wd_), 0);
    ASSERT_EQ(wiring_diagram_load_all_subsystems(wd_), 0);

    // Register module
    ASSERT_EQ(RegisterModule(BIO_MODULE_BRAIN, "CoreBrain", BIO_MODULE_CATEGORY_CORE, 1), 0);

    // Track handler callback invocation
    static std::atomic<int> callback_invoked{0};
    static std::atomic<int> handlers_registered{0};

    auto callback = [](bio_module_context_t ctx,
                       const bio_message_type_t* msg_types,
                       uint32_t msg_count,
                       void* user_data) -> int {
        callback_invoked++;
        handlers_registered += msg_count;
        return 0;
    };

    // Register callback
    ASSERT_EQ(bio_orchestrator_register_handler_callback(
        orch_, BIO_MODULE_BRAIN, callback, nullptr), 0);

    // Discover wiring
    bio_orchestrator_discover_all_wiring(orch_);

    // Invoke callbacks
    int result = bio_orchestrator_invoke_handler_callbacks(orch_);
    EXPECT_GE(result, 0);

    // Verify callback was invoked
    EXPECT_GE(callback_invoked.load(), 1) << "Handler callback should be invoked";
}

//=============================================================================
// E2E: Hardware Profile-Based Selection
//=============================================================================

TEST_F(WiringDiagramE2ETest, E2E_HardwareProfileSelection) {
    CreateFullWiringEcosystem();
    InitializeWiringSystem();

    // Test 1: FULL tier with CUDA
    {
        wiring_hardware_profile_t profile;
        wiring_get_default_profile(&profile);
        profile.tier = PLATFORM_TIER_FULL;
        profile.hw_flags = WIRING_HW_CUDA;
        profile.memory_mb = 32768;
        profile.cpu_cores = 16;

        ASSERT_EQ(wiring_diagram_load_for_profile(wd_, &profile), 0);

        // GPU module should be available
        bool available = wiring_diagram_module_available(wd_, "GPUAcceleratedModule", &profile);
        EXPECT_TRUE(available) << "GPU module should be available on FULL tier with CUDA";

        // All modules should be available
        EXPECT_TRUE(wiring_diagram_module_available(wd_, "CoreBrain", &profile));
        EXPECT_TRUE(wiring_diagram_module_available(wd_, "AttentionModule", &profile));
    }

    // Recreate for test 2
    wiring_diagram_destroy(wd_);
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    // Test 2: CONSTRAINED tier without GPU
    {
        wiring_hardware_profile_t profile;
        wiring_get_default_profile(&profile);
        profile.tier = PLATFORM_TIER_CONSTRAINED;
        profile.hw_flags = WIRING_HW_NONE;
        profile.memory_mb = 2048;
        profile.cpu_cores = 2;

        ASSERT_EQ(wiring_diagram_load_for_profile(wd_, &profile), 0);

        // GPU module should NOT be available
        bool available = wiring_diagram_module_available(wd_, "GPUAcceleratedModule", &profile);
        EXPECT_FALSE(available) << "GPU module should NOT be available without CUDA";

        // Core modules should still be available
        EXPECT_TRUE(wiring_diagram_module_available(wd_, "CoreBrain", &profile));
    }
}

//=============================================================================
// E2E: Full Stack Pipeline
//=============================================================================

TEST_F(WiringDiagramE2ETest, E2E_FullStackPipeline) {
    CreateFullWiringEcosystem();
    InitializeWiringSystem();

    // Step 1: Detect hardware profile
    wiring_hardware_profile_t profile;
    int detect_result = wiring_detect_hardware_profile(&profile);
    EXPECT_EQ(detect_result, 0) << "Hardware detection should succeed";
    EXPECT_GE(profile.cpu_cores, 1u);
    EXPECT_GE(profile.memory_mb, 64u);

    // Step 2: Load wiring for detected profile
    ASSERT_EQ(wiring_diagram_load_for_profile(wd_, &profile), 0);

    // Step 3: Get available modules
    const char* all_modules[32];
    uint32_t total_count = wiring_diagram_get_all_modules(wd_, all_modules, 32);
    EXPECT_GE(total_count, 1u) << "Should have loaded modules";

    // Step 4: Register available modules with orchestrator
    int registered = 0;
    for (uint32_t i = 0; i < total_count && i < 32; i++) {
        // Check if module is available for our profile
        if (wiring_diagram_module_available(wd_, all_modules[i], &profile)) {
            if (RegisterModule((bio_module_id_t)(BIO_MODULE_BRAIN + i),
                               all_modules[i], BIO_MODULE_CATEGORY_CORE, 1) == 0) {
                registered++;
            }
        }
    }
    EXPECT_GE(registered, 1) << "Should register at least 1 module";

    // Step 5: Discover wiring for all registered modules
    int discovered = bio_orchestrator_discover_all_wiring(orch_);
    EXPECT_GE(discovered, 1) << "Should discover wiring for at least 1 module";

    // Step 6: Get wiring statistics
    uint32_t total, enabled, relations;
    EXPECT_EQ(wiring_diagram_get_stats(wd_, &total, &enabled, &relations), 0);

    // Verify the pipeline worked
    EXPECT_GE(total, 1u);
    EXPECT_GE(enabled, 1u);
}

//=============================================================================
// E2E: Dynamic Wiring Update Pipeline
//=============================================================================

TEST_F(WiringDiagramE2ETest, E2E_DynamicWiringUpdate) {
    InitializeWiringSystem();

    // Step 1: Start with empty wiring
    uint32_t initial_total = 0, initial_enabled = 0, initial_relations = 0;
    EXPECT_EQ(wiring_diagram_get_stats(wd_, &initial_total, &initial_enabled, &initial_relations), 0);
    EXPECT_EQ(initial_total, 0u);

    // Step 2: Dynamically add a module
    wiring_module_config_t config;
    wiring_module_config_init(&config);
    strcpy(config.module_name, "DynamicModule");
    config.subsystem = WIRING_SUBSYSTEM_CORE;
    config.enabled = true;

    ASSERT_EQ(wiring_diagram_add_module(wd_, "DynamicModule", &config), 0);

    // Step 3: Add handlers
    ASSERT_EQ(wiring_diagram_add_handler(wd_, "DynamicModule", BIO_MSG_HEALTH_CHECK), 0);
    ASSERT_EQ(wiring_diagram_add_handler(wd_, "DynamicModule", BIO_MSG_BRAIN_STATE_QUERY), 0);

    // Step 4: Verify module exists (shallow copy - no cleanup needed)
    wiring_module_config_t retrieved;
    memset(&retrieved, 0, sizeof(retrieved));
    ASSERT_EQ(wiring_diagram_get_module_config(wd_, "DynamicModule", &retrieved), 0);
    EXPECT_EQ(retrieved.handles_message_count, 2u);

    // Step 5: Register with orchestrator
    ASSERT_EQ(RegisterModule(BIO_MODULE_BRAIN, "DynamicModule", BIO_MODULE_CATEGORY_CORE, 1), 0);

    // Step 6: Discover wiring
    int discovered = bio_orchestrator_discover_all_wiring(orch_);
    EXPECT_GE(discovered, 1);

    // Step 7: Get handlers
    bio_message_type_t handlers[16];
    uint32_t handler_count = bio_orchestrator_get_module_handlers(
        orch_, BIO_MODULE_BRAIN, handlers, 16);
    EXPECT_EQ(handler_count, 2u) << "Should have 2 handlers";

    // Step 8: Disable module
    ASSERT_EQ(wiring_diagram_set_enabled(wd_, "DynamicModule", false), 0);

    // Step 9: Verify disabled (shallow copy - no cleanup needed)
    memset(&retrieved, 0, sizeof(retrieved));
    ASSERT_EQ(wiring_diagram_get_module_config(wd_, "DynamicModule", &retrieved), 0);
    EXPECT_FALSE(retrieved.enabled);

    // Step 10: Remove module
    ASSERT_EQ(wiring_diagram_remove_module(wd_, "DynamicModule"), 0);

    // Step 11: Verify removed
    wiring_module_config_init(&retrieved);
    EXPECT_EQ(wiring_diagram_get_module_config(wd_, "DynamicModule", &retrieved), -1);
}

//=============================================================================
// E2E: Persistence Round-Trip Pipeline
//=============================================================================

TEST_F(WiringDiagramE2ETest, E2E_PersistenceRoundTrip) {
    InitializeWiringSystem();

    // Step 1: Add multiple modules
    for (int i = 0; i < 5; i++) {
        wiring_module_config_t config;
        wiring_module_config_init(&config);
        std::string name = "PersistModule_" + std::to_string(i);
        strcpy(config.module_name, name.c_str());
        config.subsystem = WIRING_SUBSYSTEM_CORE;
        config.enabled = (i % 2 == 0);  // Alternate enabled/disabled

        ASSERT_EQ(wiring_diagram_add_module(wd_, name.c_str(), &config), 0);
        ASSERT_EQ(wiring_diagram_add_handler(wd_, name.c_str(), (bio_message_type_t)(BIO_MSG_HEALTH_CHECK + i)), 0);
    }

    // Step 2: Persist
    ASSERT_EQ(wiring_diagram_persist(wd_), 0);

    // Step 3: Destroy
    wiring_diagram_destroy(wd_);
    wd_ = nullptr;

    // Step 4: Recreate
    wd_ = wiring_diagram_create(temp_dir_.c_str());
    ASSERT_NE(wd_, nullptr);

    // Step 5: Reload
    ASSERT_EQ(wiring_diagram_load_subsystem(wd_, WIRING_SUBSYSTEM_CORE), 0);

    // Step 6: Verify all modules are restored (shallow copy - no cleanup needed)
    for (int i = 0; i < 5; i++) {
        std::string name = "PersistModule_" + std::to_string(i);
        wiring_module_config_t retrieved;
        memset(&retrieved, 0, sizeof(retrieved));
        int result = wiring_diagram_get_module_config(wd_, name.c_str(), &retrieved);
        EXPECT_EQ(result, 0) << "Module " << name << " should be restored";
        if (result == 0) {
            EXPECT_EQ(retrieved.enabled, (i % 2 == 0)) << "Enabled state should be preserved";
        }
    }
}

//=============================================================================
// E2E: Concurrent Wiring Operations
//=============================================================================

TEST_F(WiringDiagramE2ETest, E2E_ConcurrentOperations) {
    InitializeWiringSystem();

    std::atomic<int> add_success{0};
    std::atomic<int> query_success{0};
    std::atomic<int> stats_success{0};
    std::vector<std::thread> threads;

    // Writer threads
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, t, &add_success]() {
            for (int i = 0; i < 20; i++) {
                std::string name = "Concurrent_" + std::to_string(t) + "_" + std::to_string(i);
                wiring_module_config_t config;
                wiring_module_config_init(&config);
                config.subsystem = (wiring_subsystem_t)(t % WIRING_SUBSYSTEM_COUNT);
                config.enabled = true;
                if (wiring_diagram_add_module(wd_, name.c_str(), &config) == 0) {
                    add_success++;
                    wiring_diagram_add_handler(wd_, name.c_str(), BIO_MSG_HEALTH_CHECK);
                }
            }
        });
    }

    // Reader threads
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, &query_success]() {
            for (int i = 0; i < 50; i++) {
                const char* modules[200];
                uint32_t count = wiring_diagram_get_all_modules(wd_, modules, 200);
                if (count >= 0) {
                    query_success++;
                }
                std::this_thread::yield();
            }
        });
    }

    // Stats threads
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, &stats_success]() {
            for (int i = 0; i < 30; i++) {
                uint32_t total, enabled, relations;
                if (wiring_diagram_get_stats(wd_, &total, &enabled, &relations) == 0) {
                    stats_success++;
                }
                std::this_thread::yield();
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(add_success.load(), 80) << "All adds should succeed";
    EXPECT_EQ(query_success.load(), 100) << "All queries should succeed";
    EXPECT_EQ(stats_success.load(), 60) << "All stats should succeed";

    // Verify final state
    const char* modules[100];
    uint32_t final_count = wiring_diagram_get_all_modules(wd_, modules, 100);
    EXPECT_EQ(final_count, 80u) << "Should have 80 modules after concurrent adds";
}

