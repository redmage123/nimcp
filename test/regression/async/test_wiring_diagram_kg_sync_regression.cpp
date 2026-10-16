/**
 * @file test_wiring_diagram_kg_sync_regression.cpp
 * @brief Regression tests for Phase 8: Wiring Diagram KG Sync
 *
 * Tests ensure wiring-KG sync behavior remains consistent across versions:
 * - API stability tests
 * - Return value consistency
 * - Edge case behavior preservation
 * - Performance regression tests
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <filesystem>

#include "async/nimcp_wiring_diagram.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WiringKGSyncRegression : public ::testing::Test {
protected:
    wiring_diagram_t* wd_ = nullptr;
    brain_kg_t* kg_ = nullptr;
    std::string temp_dir_;

    void SetUp() override {
        // Initialize bio-async
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = false;
        bio_config.enable_logging = false;
        nimcp_bio_async_init(&bio_config);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = false;
        router_config.enable_logging = false;
        bio_router_init(&router_config);

        // Create temporary directory
        temp_dir_ = "/tmp/nimcp_wiring_kg_regression_" + std::to_string(getpid());
        std::filesystem::create_directories(temp_dir_);

        // Create wiring diagram
        wd_ = wiring_diagram_create(temp_dir_.c_str());
        ASSERT_NE(wd_, nullptr);
        wiring_diagram_set_auto_persist(wd_, false);

        // Create brain KG
        brain_kg_config_t kg_config;
        brain_kg_default_config(&kg_config);
        kg_config.enable_security = false;
        kg_config.enable_access_control = false;
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

        if (!temp_dir_.empty()) {
            std::filesystem::remove_all(temp_dir_);
        }
    }

    // Helper: Add wiring module
    void AddWiringModule(bio_module_id_t id, const char* name,
                         const std::vector<uint32_t>& handlers) {
        wiring_module_config_t config;
        wiring_module_config_init(&config);
        strncpy(config.module_name, name, sizeof(config.module_name) - 1);
        config.module_id = id;
        config.subsystem = WIRING_SUBSYSTEM_CORE;
        config.min_tier = PLATFORM_TIER_MINIMAL;
        config.enabled = true;

        if (!handlers.empty()) {
            config.handles_messages = (bio_message_type_t*)
                nimcp_calloc(handlers.size(), sizeof(bio_message_type_t));
            for (size_t i = 0; i < handlers.size(); i++) {
                config.handles_messages[i] = static_cast<bio_message_type_t>(handlers[i]);
            }
            config.handles_message_count = handlers.size();
            config.handles_message_capacity = handlers.size();
        }

        wiring_diagram_add_module(wd_, name, &config);
    }

    // Helper: Add disabled wiring module
    void AddDisabledWiringModule(bio_module_id_t id, const char* name,
                                  const std::vector<uint32_t>& handlers) {
        wiring_module_config_t config;
        wiring_module_config_init(&config);
        strncpy(config.module_name, name, sizeof(config.module_name) - 1);
        config.module_id = id;
        config.subsystem = WIRING_SUBSYSTEM_CORE;
        config.min_tier = PLATFORM_TIER_MINIMAL;
        config.enabled = false;  // Disabled

        if (!handlers.empty()) {
            config.handles_messages = (bio_message_type_t*)
                nimcp_calloc(handlers.size(), sizeof(bio_message_type_t));
            for (size_t i = 0; i < handlers.size(); i++) {
                config.handles_messages[i] = static_cast<bio_message_type_t>(handlers[i]);
            }
            config.handles_message_count = handlers.size();
            config.handles_message_capacity = handlers.size();
        }

        wiring_diagram_add_module(wd_, name, &config);
    }
};

//=============================================================================
// API STABILITY REGRESSION TESTS
//=============================================================================

/**
 * @test wiring_diagram_sync_to_brain_kg null parameters return -1
 * @regression Error return value must remain -1 for compatibility
 */
TEST_F(WiringKGSyncRegression, API_SyncToKG_NullReturnsMinusOne) {
    EXPECT_EQ(wiring_diagram_sync_to_brain_kg(nullptr, kg_), -1);
    EXPECT_EQ(wiring_diagram_sync_to_brain_kg(wd_, nullptr), -1);
    EXPECT_EQ(wiring_diagram_sync_to_brain_kg(nullptr, nullptr), -1);
}

/**
 * @test wiring_diagram_sync_from_brain_kg null parameters return -1
 * @regression Error return value must remain -1 for compatibility
 */
TEST_F(WiringKGSyncRegression, API_SyncFromKG_NullReturnsMinusOne) {
    EXPECT_EQ(wiring_diagram_sync_from_brain_kg(nullptr, kg_), -1);
    EXPECT_EQ(wiring_diagram_sync_from_brain_kg(wd_, nullptr), -1);
    EXPECT_EQ(wiring_diagram_sync_from_brain_kg(nullptr, nullptr), -1);
}

/**
 * @test Empty wiring diagram sync returns 0 (not -1)
 * @regression Empty diagram is valid, not an error
 */
TEST_F(WiringKGSyncRegression, API_EmptyDiagramReturnsZero) {
    int result = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(result, 0);  // MUST be 0, not -1
}

/**
 * @test Sync return value equals handler count
 * @regression Return value contract must be maintained
 */
TEST_F(WiringKGSyncRegression, API_ReturnValueEqualsHandlerCount) {
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {0x100, 0x101, 0x102});
    AddWiringModule(BIO_MODULE_MEMORY, "memory", {0x200, 0x201});

    int result = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(result, 5);  // 3 + 2
}

//=============================================================================
// BEHAVIOR CONSISTENCY REGRESSION TESTS
//=============================================================================

/**
 * @test Disabled modules are skipped during sync
 * @regression Disabled modules must never be synced
 */
TEST_F(WiringKGSyncRegression, Behavior_DisabledModulesNeverSynced) {
    // Add enabled module
    AddWiringModule(BIO_MODULE_ATTENTION, "enabled", {0x100});

    // Add disabled module
    AddDisabledWiringModule(BIO_MODULE_MEMORY, "disabled", {0x200});

    int result = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(result, 1);  // Only enabled module

    // Verify disabled handler NOT in KG
    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(kg_, 0x200);
    ASSERT_NE(handlers, nullptr);
    EXPECT_EQ(handlers->count, 0u);
    brain_kg_handler_list_destroy(handlers);
}

/**
 * @test Sync is additive (doesn't clear existing handlers)
 * @regression Sync adds handlers, doesn't replace
 */
TEST_F(WiringKGSyncRegression, Behavior_SyncIsAdditive) {
    // Pre-add handler directly to KG
    brain_kg_add_message_handler(kg_, (brain_kg_node_id_t)BIO_MODULE_EMOTIONS, 0x300);

    // Sync wiring with different handler
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {0x100});
    wiring_diagram_sync_to_brain_kg(wd_, kg_);

    // Both handlers should exist
    brain_kg_handler_list_t* h1 = brain_kg_get_handlers_for_message_type(kg_, 0x300);
    ASSERT_NE(h1, nullptr);
    EXPECT_EQ(h1->count, 1u);  // Pre-existing
    brain_kg_handler_list_destroy(h1);

    brain_kg_handler_list_t* h2 = brain_kg_get_handlers_for_message_type(kg_, 0x100);
    ASSERT_NE(h2, nullptr);
    EXPECT_EQ(h2->count, 1u);  // From wiring
    brain_kg_handler_list_destroy(h2);
}

/**
 * @test Duplicate handler registration is idempotent
 * @regression Multiple syncs must not duplicate handlers
 */
TEST_F(WiringKGSyncRegression, Behavior_DuplicateRegistrationIdempotent) {
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {0x100});

    // Sync multiple times
    wiring_diagram_sync_to_brain_kg(wd_, kg_);
    wiring_diagram_sync_to_brain_kg(wd_, kg_);
    wiring_diagram_sync_to_brain_kg(wd_, kg_);

    // Handler should appear only once
    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(kg_, 0x100);
    ASSERT_NE(handlers, nullptr);
    EXPECT_EQ(handlers->count, 1u);
    brain_kg_handler_list_destroy(handlers);
}

/**
 * @test Module ID is used as handler ID (not node ID)
 * @regression bio_module_id_t must be used directly as handler
 */
TEST_F(WiringKGSyncRegression, Behavior_ModuleIDAsHandlerID) {
    const bio_module_id_t MODULE_ID = (bio_module_id_t)0x1234;
    AddWiringModule(MODULE_ID, "test_module", {0x100});

    wiring_diagram_sync_to_brain_kg(wd_, kg_);

    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(kg_, 0x100);
    ASSERT_NE(handlers, nullptr);
    ASSERT_EQ(handlers->count, 1u);
    EXPECT_EQ(handlers->handlers[0], (brain_kg_node_id_t)MODULE_ID);
    brain_kg_handler_list_destroy(handlers);
}

/**
 * @test Sync from KG only updates existing modules
 * @regression sync_from must not create new modules
 */
TEST_F(WiringKGSyncRegression, Behavior_SyncFromOnlyUpdatesExisting) {
    // Add handler for non-existent module in KG
    brain_kg_add_message_handler(kg_, (brain_kg_node_id_t)0x9999, 0x100);

    // Sync from KG to empty wiring diagram
    int result = wiring_diagram_sync_from_brain_kg(wd_, kg_);
    EXPECT_EQ(result, 0);  // No modules to update

    // Module 0x9999 should NOT be in wiring diagram
    wiring_module_config_t config;
    int get_result = wiring_diagram_get_module_config_by_id(wd_, (bio_module_id_t)0x9999, &config);
    EXPECT_EQ(get_result, -1);  // Not found
}

//=============================================================================
// EDGE CASE REGRESSION TESTS
//=============================================================================

/**
 * @test Zero-handler module syncs without error
 * @regression Modules with no handlers are valid
 */
TEST_F(WiringKGSyncRegression, Edge_ZeroHandlerModule) {
    // Add module with no handlers
    wiring_module_config_t config;
    wiring_module_config_init(&config);
    strncpy(config.module_name, "no_handlers", sizeof(config.module_name) - 1);
    config.module_id = BIO_MODULE_BRAIN;
    config.enabled = true;
    config.handles_message_count = 0;
    wiring_diagram_add_module(wd_, "no_handlers", &config);

    int result = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(result, 0);  // 0 handlers synced
}

/**
 * @test Very long module name is handled
 * @regression Module name length edge case
 */
TEST_F(WiringKGSyncRegression, Edge_LongModuleName) {
    wiring_module_config_t config;
    wiring_module_config_init(&config);

    // Fill name to maximum
    memset(config.module_name, 'x', sizeof(config.module_name) - 1);
    config.module_name[sizeof(config.module_name) - 1] = '\0';
    config.module_id = BIO_MODULE_BRAIN;
    config.enabled = true;
    config.handles_messages = (bio_message_type_t*)nimcp_calloc(1, sizeof(bio_message_type_t));
    config.handles_messages[0] = static_cast<bio_message_type_t>(0x100);
    config.handles_message_count = 1;
    config.handles_message_capacity = 1;
    wiring_diagram_add_module(wd_, config.module_name, &config);

    // Should sync without error
    int result = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(result, 1);
}

/**
 * @test Maximum message type value is handled
 * @regression 0xFFFF message type must work
 */
TEST_F(WiringKGSyncRegression, Edge_MaxMessageType) {
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {0xFFFF});

    int result = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(result, 1);

    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(kg_, 0xFFFF);
    ASSERT_NE(handlers, nullptr);
    EXPECT_EQ(handlers->count, 1u);
    brain_kg_handler_list_destroy(handlers);
}

/**
 * @test Zero message type is handled
 * @regression 0x0000 message type must work
 */
TEST_F(WiringKGSyncRegression, Edge_ZeroMessageType) {
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {0x0000});

    int result = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(result, 1);

    brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(kg_, 0x0000);
    ASSERT_NE(handlers, nullptr);
    EXPECT_EQ(handlers->count, 1u);
    brain_kg_handler_list_destroy(handlers);
}

//=============================================================================
// PERFORMANCE REGRESSION TESTS
//=============================================================================

/**
 * @test Sync performance baseline
 * @regression Sync must complete in reasonable time
 */
TEST_F(WiringKGSyncRegression, Performance_SyncBaseline) {
    const int MODULE_COUNT = 50;
    const int HANDLERS_PER_MODULE = 10;

    for (int i = 0; i < MODULE_COUNT; i++) {
        std::string name = "module_" + std::to_string(i);
        std::vector<uint32_t> handlers;
        for (int j = 0; j < HANDLERS_PER_MODULE; j++) {
            handlers.push_back(0x1000 + i * 20 + j);
        }
        AddWiringModule((bio_module_id_t)(BIO_MODULE_BRAIN + i), name.c_str(), handlers);
    }

    auto start = std::chrono::high_resolution_clock::now();

    int synced = wiring_diagram_sync_to_brain_kg(wd_, kg_);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    EXPECT_EQ(synced, MODULE_COUNT * HANDLERS_PER_MODULE);

    printf("  [Regression] Sync %d handlers: %ld us (%.2f us/handler)\n",
           synced, elapsed_us, (float)elapsed_us / synced);

    // Baseline: should complete in < 50ms for 500 handlers
    EXPECT_LT(elapsed_us, 50000);
}

/**
 * @test Repeated sync performance
 * @regression Re-sync should not degrade significantly
 */
TEST_F(WiringKGSyncRegression, Performance_RepeatedSync) {
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {0x100, 0x101, 0x102, 0x103, 0x104});

    const int SYNC_COUNT = 100;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < SYNC_COUNT; i++) {
        wiring_diagram_sync_to_brain_kg(wd_, kg_);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    printf("  [Regression] %d syncs: %ld us (%.2f us/sync)\n",
           SYNC_COUNT, elapsed_us, (float)elapsed_us / SYNC_COUNT);

    // Should complete in < 100ms for 100 syncs
    EXPECT_LT(elapsed_us, 100000);
}

/**
 * @test Handler lookup after sync is fast
 * @regression Lookup performance must not degrade after sync
 */
TEST_F(WiringKGSyncRegression, Performance_LookupAfterSync) {
    // Add many handlers
    for (int i = 0; i < 100; i++) {
        std::string name = "module_" + std::to_string(i);
        AddWiringModule((bio_module_id_t)(BIO_MODULE_BRAIN + i), name.c_str(),
                        {static_cast<uint32_t>(0x1000 + i)});
    }

    wiring_diagram_sync_to_brain_kg(wd_, kg_);

    const int LOOKUP_COUNT = 10000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < LOOKUP_COUNT; i++) {
        uint32_t msg = 0x1000 + (i % 100);
        brain_kg_handler_list_t* handlers = brain_kg_get_handlers_for_message_type(kg_, msg);
        brain_kg_handler_list_destroy(handlers);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    printf("  [Regression] %d lookups: %ld us (%.3f us/lookup)\n",
           LOOKUP_COUNT, elapsed_us, (float)elapsed_us / LOOKUP_COUNT);

    // Should complete in < 100ms for 10000 lookups
    EXPECT_LT(elapsed_us, 100000);
}

//=============================================================================
// THREAD SAFETY REGRESSION TESTS
//=============================================================================

/**
 * @test Concurrent sync is safe
 * @regression No crashes or data corruption under concurrent access
 */
TEST_F(WiringKGSyncRegression, ThreadSafety_ConcurrentSync) {
    for (int i = 0; i < 10; i++) {
        std::string name = "module_" + std::to_string(i);
        AddWiringModule((bio_module_id_t)(BIO_MODULE_BRAIN + i), name.c_str(),
                        {static_cast<uint32_t>(0x100 + i)});
    }

    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &error_count]() {
            for (int i = 0; i < 50; i++) {
                int result = wiring_diagram_sync_to_brain_kg(wd_, kg_);
                if (result < 0) {
                    error_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(error_count.load(), 0);
}

/**
 * @test Concurrent sync and lookup is safe
 * @regression Sync and lookup can run concurrently
 */
TEST_F(WiringKGSyncRegression, ThreadSafety_ConcurrentSyncAndLookup) {
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {0x100});
    wiring_diagram_sync_to_brain_kg(wd_, kg_);

    std::atomic<bool> running{true};
    std::atomic<int> lookup_count{0};
    std::atomic<int> sync_count{0};

    // Lookup thread
    std::thread lookup_thread([this, &running, &lookup_count]() {
        while (running) {
            brain_kg_handler_list_t* h = brain_kg_get_handlers_for_message_type(kg_, 0x100);
            if (h) {
                brain_kg_handler_list_destroy(h);
                lookup_count++;
            }
        }
    });

    // Sync thread
    std::thread sync_thread([this, &running, &sync_count]() {
        while (running) {
            wiring_diagram_sync_to_brain_kg(wd_, kg_);
            sync_count++;
        }
    });

    // Run for 100ms
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    lookup_thread.join();
    sync_thread.join();

    printf("  [Regression] Concurrent: %d lookups, %d syncs\n",
           lookup_count.load(), sync_count.load());

    EXPECT_GT(lookup_count.load(), 0);
    EXPECT_GT(sync_count.load(), 0);
}

