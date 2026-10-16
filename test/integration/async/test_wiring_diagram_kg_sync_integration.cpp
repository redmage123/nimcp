/**
 * @file test_wiring_diagram_kg_sync_integration.cpp
 * @brief Integration tests for Phase 8: Wiring Diagram KG Sync with Bio-Router
 *
 * Tests the integration of wiring diagram to brain_kg sync with the
 * bio-router's KG dispatch functionality (Phase 7):
 * - End-to-end wiring -> KG -> dispatch flow
 * - Orchestrator integration
 * - Runtime reconfiguration scenarios
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
#include <fstream>

#include "async/nimcp_wiring_diagram.h"
#include "async/nimcp_bio_async_orchestrator.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WiringKGSyncIntegrationTest : public ::testing::Test {
protected:
    wiring_diagram_t* wd_ = nullptr;
    brain_kg_t* kg_ = nullptr;
    bio_async_orchestrator_t* orch_ = nullptr;
    bio_module_context_t sender_ctx_ = nullptr;
    bio_module_context_t receiver_ctx_ = nullptr;
    std::string temp_dir_;

    static std::atomic<int> s_handler_invocations;

    void SetUp() override {
        s_handler_invocations = 0;

        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = true;
        bio_config.enable_logging = false;
        ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = true;
        router_config.enable_logging = false;
        ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS);

        // Create temporary directory
        temp_dir_ = "/tmp/nimcp_wiring_kg_integration_" + std::to_string(getpid());
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

        // Create orchestrator
        bio_orchestrator_config_t orch_config;
        bio_orchestrator_default_config(&orch_config);
        orch_config.enable_statistics = false;
        orch_config.enable_auto_health_check = false;
        orch_ = bio_orchestrator_create(&orch_config);
        ASSERT_NE(orch_, nullptr);

        // Register test modules with bio-router
        bio_module_info_t sender_info = {
            .module_id = BIO_MODULE_BRAIN,
            .module_name = "test_sender",
            .inbox_capacity = 32,
            .user_data = nullptr
        };
        sender_ctx_ = bio_router_register_module(&sender_info);
        ASSERT_NE(sender_ctx_, nullptr);

        bio_module_info_t receiver_info = {
            .module_id = BIO_MODULE_ATTENTION,
            .module_name = "test_receiver",
            .inbox_capacity = 32,
            .user_data = nullptr
        };
        receiver_ctx_ = bio_router_register_module(&receiver_info);
        ASSERT_NE(receiver_ctx_, nullptr);

        // Register handler for test messages
        bio_router_register_handler(receiver_ctx_, BIO_MSG_BRAIN_STATE_QUERY, test_handler);
    }

    void TearDown() override {
        // Disconnect brain_kg from router
        bio_router_set_brain_kg(nullptr);

        if (receiver_ctx_) bio_router_unregister_module(receiver_ctx_);
        if (sender_ctx_) bio_router_unregister_module(sender_ctx_);

        if (orch_) {
            bio_orchestrator_destroy(orch_);
            orch_ = nullptr;
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

    static nimcp_error_t test_handler(const void* msg, size_t msg_size,
                                       nimcp_bio_promise_t promise, void* user_data) {
        (void)msg; (void)msg_size; (void)promise; (void)user_data;
        s_handler_invocations++;
        return NIMCP_SUCCESS;
    }

    // Helper: Add wiring module with handlers
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

    // Helper: Create a KG dispatch message
    bio_msg_brain_state_query_t CreateKGDispatchMessage() {
        bio_msg_brain_state_query_t msg;
        memset(&msg, 0, sizeof(msg));
        bio_msg_init_header(&msg.header, BIO_MSG_BRAIN_STATE_QUERY,
                           BIO_MODULE_BRAIN, BIO_MODULE_KG_DISPATCH,
                           sizeof(msg));
        msg.query_flags = BIO_BRAIN_QUERY_NEURON_COUNT;
        return msg;
    }
};

std::atomic<int> WiringKGSyncIntegrationTest::s_handler_invocations{0};

//=============================================================================
// END-TO-END WIRING -> KG -> DISPATCH TESTS
//=============================================================================

/**
 * @test Complete flow: wiring diagram -> sync to KG -> KG dispatch
 *
 * This is the main integration test verifying the Phase 8 functionality
 * works with Phase 7 KG dispatch.
 */
TEST_F(WiringKGSyncIntegrationTest, EndToEnd_WiringToKGDispatch) {
    // 1. Configure wiring diagram with handler
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {(uint32_t)BIO_MSG_BRAIN_STATE_QUERY});

    // 2. Sync wiring to brain_kg
    int synced = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(synced, 1);

    // 3. Connect brain_kg to router
    EXPECT_EQ(bio_router_set_brain_kg(kg_), NIMCP_SUCCESS);
    EXPECT_TRUE(bio_router_kg_dispatch_available());

    // 4. Send message via KG dispatch
    auto msg = CreateKGDispatchMessage();
    nimcp_error_t result = bio_router_send(sender_ctx_, &msg, sizeof(msg), 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // 5. Process and verify handler invoked
    bio_router_process_inbox(receiver_ctx_, 10);
    EXPECT_EQ(s_handler_invocations.load(), 1);
}

/**
 * @test Multiple modules receive KG-dispatched message
 */
TEST_F(WiringKGSyncIntegrationTest, EndToEnd_MultipleReceivers) {
    // Register second receiver
    bio_module_info_t receiver2_info = {
        .module_id = BIO_MODULE_MEMORY,
        .module_name = "test_receiver2",
        .inbox_capacity = 32,
        .user_data = nullptr
    };
    bio_module_context_t receiver2_ctx = bio_router_register_module(&receiver2_info);
    ASSERT_NE(receiver2_ctx, nullptr);
    bio_router_register_handler(receiver2_ctx, BIO_MSG_BRAIN_STATE_QUERY, test_handler);

    // Configure both modules in wiring diagram
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {(uint32_t)BIO_MSG_BRAIN_STATE_QUERY});
    AddWiringModule(BIO_MODULE_MEMORY, "memory", {(uint32_t)BIO_MSG_BRAIN_STATE_QUERY});

    // Sync and connect
    wiring_diagram_sync_to_brain_kg(wd_, kg_);
    bio_router_set_brain_kg(kg_);

    // Send message
    auto msg = CreateKGDispatchMessage();
    bio_router_send(sender_ctx_, &msg, sizeof(msg), 100);

    // Process both receivers
    bio_router_process_inbox(receiver_ctx_, 10);
    bio_router_process_inbox(receiver2_ctx, 10);

    // Both handlers should be invoked
    EXPECT_EQ(s_handler_invocations.load(), 2);

    bio_router_unregister_module(receiver2_ctx);
}

/**
 * @test Dynamic reconfiguration: add handler at runtime
 */
TEST_F(WiringKGSyncIntegrationTest, Dynamic_AddHandlerAtRuntime) {
    // Initial configuration without handler
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {});

    wiring_diagram_sync_to_brain_kg(wd_, kg_);
    bio_router_set_brain_kg(kg_);

    // Send message - should not be delivered (no handler in KG)
    auto msg1 = CreateKGDispatchMessage();
    bio_router_send(sender_ctx_, &msg1, sizeof(msg1), 100);
    bio_router_process_inbox(receiver_ctx_, 10);
    EXPECT_EQ(s_handler_invocations.load(), 0);

    // Add handler dynamically to brain_kg
    brain_kg_add_message_handler(kg_, (brain_kg_node_id_t)BIO_MODULE_ATTENTION,
                                  (uint32_t)BIO_MSG_BRAIN_STATE_QUERY);

    // Now message should be delivered
    auto msg2 = CreateKGDispatchMessage();
    bio_router_send(sender_ctx_, &msg2, sizeof(msg2), 100);
    bio_router_process_inbox(receiver_ctx_, 10);
    EXPECT_EQ(s_handler_invocations.load(), 1);
}

/**
 * @test Dynamic reconfiguration: remove handler at runtime
 */
TEST_F(WiringKGSyncIntegrationTest, Dynamic_RemoveHandlerAtRuntime) {
    // Configure with handler
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {(uint32_t)BIO_MSG_BRAIN_STATE_QUERY});

    wiring_diagram_sync_to_brain_kg(wd_, kg_);
    bio_router_set_brain_kg(kg_);

    // First message should be delivered
    auto msg1 = CreateKGDispatchMessage();
    bio_router_send(sender_ctx_, &msg1, sizeof(msg1), 100);
    bio_router_process_inbox(receiver_ctx_, 10);
    EXPECT_EQ(s_handler_invocations.load(), 1);

    // Remove handler from brain_kg
    brain_kg_remove_message_handler(kg_, (brain_kg_node_id_t)BIO_MODULE_ATTENTION,
                                     (uint32_t)BIO_MSG_BRAIN_STATE_QUERY);

    // Second message should NOT be delivered
    auto msg2 = CreateKGDispatchMessage();
    bio_router_send(sender_ctx_, &msg2, sizeof(msg2), 100);
    bio_router_process_inbox(receiver_ctx_, 10);
    EXPECT_EQ(s_handler_invocations.load(), 1);  // Still 1, not 2
}

//=============================================================================
// ORCHESTRATOR INTEGRATION TESTS
//=============================================================================

/**
 * @test Orchestrator wiring diagram is synced to brain_kg
 */
TEST_F(WiringKGSyncIntegrationTest, Orchestrator_WiringKGSync) {
    // Set wiring diagram on orchestrator
    bio_orchestrator_set_wiring_diagram(orch_, wd_);

    // Add module to wiring
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {(uint32_t)BIO_MSG_BRAIN_STATE_QUERY});

    // Get wiring from orchestrator and sync
    wiring_diagram_t* orch_wd = bio_orchestrator_get_wiring_diagram(orch_);
    EXPECT_EQ(orch_wd, wd_);

    int synced = wiring_diagram_sync_to_brain_kg(orch_wd, kg_);
    EXPECT_EQ(synced, 1);

    // Verify handler in KG
    brain_kg_handler_list_t* handlers =
        brain_kg_get_handlers_for_message_type(kg_, (uint32_t)BIO_MSG_BRAIN_STATE_QUERY);
    ASSERT_NE(handlers, nullptr);
    EXPECT_EQ(handlers->count, 1u);
    brain_kg_handler_list_destroy(handlers);
}

/**
 * @test Full stack: orchestrator -> wiring -> KG -> router dispatch
 */
TEST_F(WiringKGSyncIntegrationTest, FullStack_OrchestratorToDispatch) {
    // Configure orchestrator with wiring
    bio_orchestrator_set_wiring_diagram(orch_, wd_);

    // Register module with orchestrator
    bio_orchestrator_register_module(orch_, BIO_MODULE_ATTENTION, "attention",
                                      BIO_MODULE_CATEGORY_COGNITIVE, nullptr, 1);

    // Add handlers via wiring
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {(uint32_t)BIO_MSG_BRAIN_STATE_QUERY});

    // Sync wiring to KG and connect to router
    wiring_diagram_sync_to_brain_kg(wd_, kg_);
    bio_router_set_brain_kg(kg_);

    // Send via KG dispatch
    auto msg = CreateKGDispatchMessage();
    bio_router_send(sender_ctx_, &msg, sizeof(msg), 100);
    bio_router_process_inbox(receiver_ctx_, 10);

    EXPECT_EQ(s_handler_invocations.load(), 1);
}

//=============================================================================
// ERROR HANDLING TESTS
//=============================================================================

/**
 * @test Sync to disconnected KG fails gracefully
 */
TEST_F(WiringKGSyncIntegrationTest, Error_SyncAfterKGDestroy) {
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {0x100});

    // Destroy KG
    brain_kg_destroy(kg_);
    kg_ = nullptr;

    // Sync should fail with null KG
    int result = wiring_diagram_sync_to_brain_kg(wd_, kg_);
    EXPECT_EQ(result, -1);
}

/**
 * @test KG dispatch without sync returns no handlers
 */
TEST_F(WiringKGSyncIntegrationTest, NoSync_KGDispatchEmpty) {
    // Configure wiring but DON'T sync
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {(uint32_t)BIO_MSG_BRAIN_STATE_QUERY});

    // Connect empty KG to router
    bio_router_set_brain_kg(kg_);

    // Send via KG dispatch
    auto msg = CreateKGDispatchMessage();
    nimcp_error_t result = bio_router_send(sender_ctx_, &msg, sizeof(msg), 100);
    EXPECT_EQ(result, NIMCP_SUCCESS);  // Success but no delivery

    bio_router_process_inbox(receiver_ctx_, 10);
    EXPECT_EQ(s_handler_invocations.load(), 0);  // No handler in KG
}

//=============================================================================
// PERFORMANCE TESTS
//=============================================================================

/**
 * @test Sync large wiring diagram performance
 */
TEST_F(WiringKGSyncIntegrationTest, Performance_LargeWiringSync) {
    const int MODULE_COUNT = 100;
    const int HANDLERS_PER_MODULE = 5;

    // Add many modules
    for (int i = 0; i < MODULE_COUNT; i++) {
        std::string name = "module_" + std::to_string(i);
        std::vector<uint32_t> handlers;
        for (int j = 0; j < HANDLERS_PER_MODULE; j++) {
            handlers.push_back(0x1000 + i * 10 + j);
        }
        AddWiringModule((bio_module_id_t)(BIO_MODULE_BRAIN + i), name.c_str(), handlers);
    }

    auto start = std::chrono::high_resolution_clock::now();

    int synced = wiring_diagram_sync_to_brain_kg(wd_, kg_);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    EXPECT_EQ(synced, MODULE_COUNT * HANDLERS_PER_MODULE);

    printf("  [Performance] Synced %d handlers in %ld us (%.2f handlers/us)\n",
           synced, elapsed, (float)synced / elapsed);

    // Should complete quickly (< 100ms)
    EXPECT_LT(elapsed, 100000);
}

/**
 * @test End-to-end dispatch latency after sync
 */
TEST_F(WiringKGSyncIntegrationTest, Performance_DispatchLatencyAfterSync) {
    AddWiringModule(BIO_MODULE_ATTENTION, "attention", {(uint32_t)BIO_MSG_BRAIN_STATE_QUERY});
    wiring_diagram_sync_to_brain_kg(wd_, kg_);
    bio_router_set_brain_kg(kg_);

    const int MSG_COUNT = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < MSG_COUNT; i++) {
        auto msg = CreateKGDispatchMessage();
        bio_router_send(sender_ctx_, &msg, sizeof(msg), 100);
    }

    bio_router_process_inbox(receiver_ctx_, MSG_COUNT + 100);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    EXPECT_EQ(s_handler_invocations.load(), MSG_COUNT);

    printf("  [Performance] %d messages dispatched in %ld us (%.2f msg/us)\n",
           MSG_COUNT, elapsed, (float)MSG_COUNT / elapsed);
}

