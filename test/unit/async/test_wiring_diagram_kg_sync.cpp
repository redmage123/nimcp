/**
 * @file test_wiring_diagram_kg_sync.cpp
 * @brief Unit tests for Phase 8: Wiring Diagram to Brain KG Sync
 *
 * Tests the bidirectional sync between wiring diagram module configurations
 * and the brain_kg message-type handler index:
 * - wiring_diagram_sync_to_brain_kg: Sync wiring handlers to KG
 * - wiring_diagram_sync_from_brain_kg: Sync KG handlers to wiring
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <filesystem>
#include <fstream>

#include "async/nimcp_wiring_diagram.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WiringDiagramKGSyncTest : public ::testing::Test {
protected:
    wiring_diagram_t* wd_ = nullptr;
    brain_kg_t* kg_ = nullptr;
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
        temp_dir_ = "/tmp/nimcp_wiring_kg_sync_test_" + std::to_string(getpid());
        std::filesystem::create_directories(temp_dir_);
        std::filesystem::create_directories(temp_dir_ + "/subsystems");

        // Create wiring diagram
        wd_ = wiring_diagram_create(temp_dir_.c_str());
        ASSERT_NE(wd_, nullptr);
        wiring_diagram_set_auto_persist(wd_, false);

        // Create brain KG with security disabled for testing
        brain_kg_config_t kg_config;
        brain_kg_default_config(&kg_config);
        kg_config.enable_security = false;
        kg_config.enable_access_control = false;
        kg_config.enable_integrity_checks = false;
        kg_ = brain_kg_create(&kg_config);
        ASSERT_NE(kg_, nullptr);
    }

    void TearDown() override {
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

        // Clean up temp directory
        if (!temp_dir_.empty()) {
            std::filesystem::remove_all(temp_dir_);
        }
    }

    // Helper: Add a module to wiring diagram with handlers
    int AddWiringModule(bio_module_id_t id, const char* name,
                        const std::vector<uint32_t>& handlers) {
        wiring_module_config_t config;
        wiring_module_config_init(&config);
        strncpy(config.module_name, name, sizeof(config.module_name) - 1);
        config.module_id = id;
        config.subsystem = WIRING_SUBSYSTEM_CORE;
        config.min_tier = PLATFORM_TIER_MINIMAL;
        config.enabled = true;

        // Add handlers
        if (!handlers.empty()) {
            config.handles_messages = (bio_message_type_t*)
                nimcp_calloc(handlers.size(), sizeof(bio_message_type_t));
            if (config.handles_messages) {
                for (size_t i = 0; i < handlers.size(); i++) {
                    config.handles_messages[i] = static_cast<bio_message_type_t>(handlers[i]);
                }
                config.handles_message_count = handlers.size();
                config.handles_message_capacity = handlers.size();
            }
        }

        int result = wiring_diagram_add_module(wd_, name, &config);
        return result;
    }

    // Helper: Add disabled module
    int AddDisabledWiringModule(bio_module_id_t id, const char* name,
                                 const std::vector<uint32_t>& handlers) {
        wiring_module_config_t config;
        wiring_module_config_init(&config);
        strncpy(config.module_name, name, sizeof(config.module_name) - 1);
        config.module_id = id;
        config.subsystem = WIRING_SUBSYSTEM_CORE;
        config.min_tier = PLATFORM_TIER_MINIMAL;
        config.enabled = false;  // Disabled!

        if (!handlers.empty()) {
            config.handles_messages = (bio_message_type_t*)
                nimcp_calloc(handlers.size(), sizeof(bio_message_type_t));
            if (config.handles_messages) {
                for (size_t i = 0; i < handlers.size(); i++) {
                    config.handles_messages[i] = static_cast<bio_message_type_t>(handlers[i]);
                }
                config.handles_message_count = handlers.size();
                config.handles_message_capacity = handlers.size();
            }
        }

        return wiring_diagram_add_module(wd_, name, &config);
    }
};

//=============================================================================
// SYNC TO BRAIN_KG TESTS
//=============================================================================

/**
 * @test Sync null parameters returns error
 */
TEST_F(WiringDiagramKGSyncTest, SyncToKG_NullParams) {
    EXPECT_EQ(wiring_diagram_sync_to_brain_kg(nullptr, kg_), -1);
    EXPECT_EQ(wiring_diagram_sync_to_brain_kg(wd_, nullptr), -1);
    EXPECT_EQ(wiring_diagram_sync_to_brain_kg(nullptr, nullptr), -1);
}

/**
 * @test Sync empty wiring diagram returns 0 handlers
 */
TEST_F(WiringDiagramKGSyncTest, SyncToKG_EmptyDiagram) {
    int result = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(result, 0);
}

/**
 * @test Sync single module with single handler
 */
TEST_F(WiringDiagramKGSyncTest, SyncToKG_SingleModuleSingleHandler) {
    AddWiringModule(BIO_MODULE_ATTENTION, "attention_module", {0x100});

    int handlers_synced = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(handlers_synced, 1);

    // Verify handler is in brain_kg
    brain_kg_handler_list_t* list = brain_kg_get_handlers_for_message_type(kg_, 0x100);
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->count, 1u);
    EXPECT_EQ(list->handlers[0], (brain_kg_node_id_t)BIO_MODULE_ATTENTION);
    brain_kg_handler_list_destroy(list);
}

/**
 * @test Sync single module with multiple handlers
 */
TEST_F(WiringDiagramKGSyncTest, SyncToKG_SingleModuleMultipleHandlers) {
    AddWiringModule(BIO_MODULE_MEMORY, "memory_module", {0x200, 0x201, 0x202});

    int handlers_synced = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(handlers_synced, 3);

    // Verify each handler
    for (uint32_t msg = 0x200; msg <= 0x202; msg++) {
        brain_kg_handler_list_t* list = brain_kg_get_handlers_for_message_type(kg_, msg);
        ASSERT_NE(list, nullptr);
        EXPECT_EQ(list->count, 1u);
        brain_kg_handler_list_destroy(list);
    }
}

/**
 * @test Sync multiple modules with handlers
 */
TEST_F(WiringDiagramKGSyncTest, SyncToKG_MultipleModules) {
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {0x100, 0x101});
    AddWiringModule(BIO_MODULE_MEMORY, "memory", {0x200});
    AddWiringModule(BIO_MODULE_EMOTIONS, "emotion", {0x300, 0x301, 0x302});

    int handlers_synced = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(handlers_synced, 6);  // 2 + 1 + 3
}

/**
 * @test Sync multiple modules handling same message type
 */
TEST_F(WiringDiagramKGSyncTest, SyncToKG_SharedMessageType) {
    const uint32_t SHARED_MSG = 0x500;

    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {SHARED_MSG});
    AddWiringModule(BIO_MODULE_MEMORY, "memory", {SHARED_MSG});
    AddWiringModule(BIO_MODULE_EMOTIONS, "emotion", {SHARED_MSG});

    int handlers_synced = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(handlers_synced, 3);

    // All three modules should handle the shared message
    brain_kg_handler_list_t* list = brain_kg_get_handlers_for_message_type(kg_, SHARED_MSG);
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->count, 3u);
    brain_kg_handler_list_destroy(list);
}

/**
 * @test Disabled modules are not synced
 */
TEST_F(WiringDiagramKGSyncTest, SyncToKG_DisabledModulesSkipped) {
    // Add enabled module
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {0x100});

    // Add disabled module
    AddDisabledWiringModule(BIO_MODULE_MEMORY, "disabled_module", {0x200});

    int handlers_synced = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(handlers_synced, 1);  // Only enabled module

    // Verify disabled module's handler was NOT synced
    brain_kg_handler_list_t* list = brain_kg_get_handlers_for_message_type(kg_, 0x200);
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->count, 0u);
    brain_kg_handler_list_destroy(list);
}

/**
 * @test Sync is idempotent (re-sync produces same result)
 */
TEST_F(WiringDiagramKGSyncTest, SyncToKG_Idempotent) {
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {0x100, 0x101});

    // First sync
    int first_sync = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(first_sync, 2);

    // Second sync should also succeed
    int second_sync = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(second_sync, 2);

    // Handler should appear only once (not duplicated)
    brain_kg_handler_list_t* list = brain_kg_get_handlers_for_message_type(kg_, 0x100);
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(list->count, 1u);
    brain_kg_handler_list_destroy(list);
}

/**
 * @test Sync modules with no handlers
 */
TEST_F(WiringDiagramKGSyncTest, SyncToKG_ModulesWithNoHandlers) {
    // Add module with no handlers
    wiring_module_config_t config;
    wiring_module_config_init(&config);
    strncpy(config.module_name, "no_handlers", sizeof(config.module_name) - 1);
    config.module_id = BIO_MODULE_BRAIN;
    config.enabled = true;
    config.handles_message_count = 0;
    wiring_diagram_add_module(wd_, "no_handlers", &config);

    int handlers_synced = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(handlers_synced, 0);
}

//=============================================================================
// SYNC FROM BRAIN_KG TESTS
//=============================================================================

/**
 * @test Sync from null parameters returns error
 */
TEST_F(WiringDiagramKGSyncTest, SyncFromKG_NullParams) {
    EXPECT_EQ(wiring_diagram_sync_from_brain_kg(nullptr, kg_), -1);
    EXPECT_EQ(wiring_diagram_sync_from_brain_kg(wd_, nullptr), -1);
    EXPECT_EQ(wiring_diagram_sync_from_brain_kg(nullptr, nullptr), -1);
}

/**
 * @test Sync from KG with no handlers
 */
TEST_F(WiringDiagramKGSyncTest, SyncFromKG_NoHandlers) {
    // Add module to wiring diagram without handlers
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {});

    int handlers_synced = wiring_diagram_sync_from_brain_kg(wd_, kg_);
    EXPECT_EQ(handlers_synced, 0);
}

/**
 * @test Sync from KG with handlers updates wiring diagram
 */
TEST_F(WiringDiagramKGSyncTest, SyncFromKG_UpdatesWiringModule) {
    // Add module to wiring diagram without handlers
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {});

    // Add handler directly to brain_kg
    brain_kg_add_message_handler(kg_, (brain_kg_node_id_t)BIO_MODULE_ATTENTION, 0x100);
    brain_kg_add_message_handler(kg_, (brain_kg_node_id_t)BIO_MODULE_ATTENTION, 0x101);

    int handlers_synced = wiring_diagram_sync_from_brain_kg(wd_, kg_);
    EXPECT_EQ(handlers_synced, 2);

    // Verify wiring module now has handlers
    // Note: wiring_diagram_get_module_config_by_id does a shallow copy,
    // so we do NOT call wiring_module_config_cleanup on the result
    wiring_module_config_t config;
    int result = wiring_diagram_get_module_config_by_id(wd_, BIO_MODULE_ATTENTION, &config);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(config.handles_message_count, 2u);
}

/**
 * @test Sync from KG only affects modules in wiring diagram
 */
TEST_F(WiringDiagramKGSyncTest, SyncFromKG_OnlyAffectsExistingModules) {
    // Add one module to wiring diagram
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {});

    // Add handlers for both ATTENTION and MEMORY in brain_kg
    brain_kg_add_message_handler(kg_, (brain_kg_node_id_t)BIO_MODULE_ATTENTION, 0x100);
    brain_kg_add_message_handler(kg_, (brain_kg_node_id_t)BIO_MODULE_MEMORY, 0x200);

    int handlers_synced = wiring_diagram_sync_from_brain_kg(wd_, kg_);
    EXPECT_EQ(handlers_synced, 1);  // Only ATTENTION is in wiring

    // MEMORY module should NOT be in wiring diagram
    wiring_module_config_t config;
    int result = wiring_diagram_get_module_config_by_id(wd_, BIO_MODULE_MEMORY, &config);
    EXPECT_EQ(result, -1);  // Not found
}

/**
 * @test Sync from KG replaces existing handlers
 */
TEST_F(WiringDiagramKGSyncTest, SyncFromKG_ReplacesExistingHandlers) {
    // Add module with initial handlers
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {0x100, 0x101});

    // Add different handlers to brain_kg
    brain_kg_add_message_handler(kg_, (brain_kg_node_id_t)BIO_MODULE_ATTENTION, 0x200);
    brain_kg_add_message_handler(kg_, (brain_kg_node_id_t)BIO_MODULE_ATTENTION, 0x201);
    brain_kg_add_message_handler(kg_, (brain_kg_node_id_t)BIO_MODULE_ATTENTION, 0x202);

    int handlers_synced = wiring_diagram_sync_from_brain_kg(wd_, kg_);
    EXPECT_EQ(handlers_synced, 3);

    // Verify new handlers replaced old ones
    wiring_module_config_t config;
    int result = wiring_diagram_get_module_config_by_id(wd_, BIO_MODULE_ATTENTION, &config);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(config.handles_message_count, 3u);
}

//=============================================================================
// BIDIRECTIONAL SYNC TESTS
//=============================================================================

/**
 * @test Round-trip sync preserves handlers
 */
TEST_F(WiringDiagramKGSyncTest, Bidirectional_RoundTrip) {
    // Add module with handlers
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {0x100, 0x101, 0x102});

    // Sync to brain_kg
    int to_kg = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(to_kg, 3);

    // Create new wiring diagram in a separate directory (avoid file contention)
    std::string temp_dir2 = temp_dir_ + "_wd2";
    std::filesystem::create_directories(temp_dir2);
    std::filesystem::create_directories(temp_dir2 + "/subsystems");
    wiring_diagram_t* wd2 = wiring_diagram_create(temp_dir2.c_str());
    ASSERT_NE(wd2, nullptr);
    wiring_diagram_set_auto_persist(wd2, false);

    // Add module without handlers to second diagram
    wiring_module_config_t config;
    wiring_module_config_init(&config);
    strncpy(config.module_name, "attention", sizeof(config.module_name) - 1);
    config.module_id = BIO_MODULE_ATTENTION;
    config.enabled = true;
    wiring_diagram_add_module(wd2, "attention", &config);

    // Sync from brain_kg
    int from_kg = wiring_diagram_sync_from_brain_kg(wd2, kg_);
    EXPECT_EQ(from_kg, 3);

    // Verify handlers match original
    wiring_module_config_t retrieved;
    int result = wiring_diagram_get_module_config_by_id(wd2, BIO_MODULE_ATTENTION, &retrieved);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(retrieved.handles_message_count, 3u);

    wiring_diagram_destroy(wd2);
    std::filesystem::remove_all(temp_dir2);
}

/**
 * @test Sync with large number of modules
 */
TEST_F(WiringDiagramKGSyncTest, Bidirectional_ManyModules) {
    const int MODULE_COUNT = 50;
    int total_handlers = 0;

    // Add many modules with varying handlers
    for (int i = 0; i < MODULE_COUNT; i++) {
        std::string name = "module_" + std::to_string(i);
        std::vector<uint32_t> handlers;
        int handler_count = (i % 5) + 1;  // 1-5 handlers each

        for (int j = 0; j < handler_count; j++) {
            handlers.push_back(0x1000 + i * 10 + j);
        }

        AddWiringModule((bio_module_id_t)(BIO_MODULE_BRAIN + i), name.c_str(), handlers);
        total_handlers += handler_count;
    }

    // Sync to brain_kg
    int synced = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(synced, total_handlers);
}

/**
 * @test Sync after dynamic handler changes via brain_kg API
 */
TEST_F(WiringDiagramKGSyncTest, Bidirectional_DynamicChanges) {
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {0x100});

    // Initial sync
    wiring_diagram_sync_to_brain_kg(wd_, kg_);

    // Add handlers dynamically via brain_kg API
    brain_kg_add_message_handler(kg_, (brain_kg_node_id_t)BIO_MODULE_ATTENTION, 0x101);
    brain_kg_add_message_handler(kg_, (brain_kg_node_id_t)BIO_MODULE_ATTENTION, 0x102);

    // Sync back to wiring diagram
    int synced = wiring_diagram_sync_from_brain_kg(wd_, kg_);
    EXPECT_EQ(synced, 3);  // Original + 2 new

    // Verify all handlers in wiring
    wiring_module_config_t config;
    int result = wiring_diagram_get_module_config_by_id(wd_, BIO_MODULE_ATTENTION, &config);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(config.handles_message_count, 3u);
}

//=============================================================================
// THREAD SAFETY TESTS
//=============================================================================

/**
 * @test Concurrent sync operations are safe
 */
TEST_F(WiringDiagramKGSyncTest, ThreadSafety_ConcurrentSync) {
    // Add some modules
    for (int i = 0; i < 10; i++) {
        std::string name = "module_" + std::to_string(i);
        AddWiringModule((bio_module_id_t)(BIO_MODULE_BRAIN + i), name.c_str(),
                        {static_cast<uint32_t>(0x100 + i)});
    }

    std::atomic<int> sync_count{0};
    std::atomic<bool> error_detected{false};

    // Run concurrent syncs
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &sync_count, &error_detected]() {
            for (int i = 0; i < 25; i++) {
                int result = wiring_diagram_sync_to_brain_kg(wd_, kg_);
                if (result < 0) {
                    error_detected = true;
                }
                sync_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(error_detected.load());
    EXPECT_EQ(sync_count.load(), 100);  // 4 threads * 25 iterations
}

