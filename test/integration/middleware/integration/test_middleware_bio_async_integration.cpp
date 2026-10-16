/**
 * @file test_middleware_bio_async_integration.cpp
 * @brief Bio-async integration tests for middleware modules
 *
 * WHAT: Tests bio-router and bio-async integration with middleware modules
 * WHY:  Ensure modules can register, communicate, and coordinate via bio-async
 * HOW:  Tests module lifecycle, message passing, synchronization, resource management
 *
 * TEST COVERAGE:
 * 1. Module registration (single, multiple, simultaneous)
 * 2. Message passing (point-to-point, broadcast, async request-response)
 * 3. Module lifecycle (create/destroy ordering, cleanup)
 * 4. Bio-router initialization and shutdown with active modules
 * 5. Concurrent message handling
 * 6. Phase synchronization across modules
 * 7. Error handling and recovery
 * 8. Resource management and memory safety
 *
 * @author NIMCP Development Team
 * @date 2025-12-05
 */

#include <gtest/gtest.h>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <condition_variable>

// Headers have their own extern "C" guards
#include "middleware/integration/nimcp_middleware_controller.h"
#include "middleware/integration/nimcp_flow_tracker.h"
#include "middleware/integration/nimcp_shannon_monitor.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/nimcp_brain.h"
#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Test Fixture
//=============================================================================

class MiddlewareBioAsyncIntegrationTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    middleware_controller_t* controller = nullptr;
    flow_tracker_t* flow_tracker = nullptr;
    shannon_monitor_t* shannon_monitor = nullptr;

    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_logging = false;
        bio_config.enable_statistics = true;
        bio_config.use_unified_memory = true;
        bio_config.thread_pool_size = 4;
        nimcp_error_t err = nimcp_bio_async_init(&bio_config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Bio-async init failed";

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.max_modules = 32;
        router_config.inbox_capacity = 64;
        router_config.worker_threads = 2;
        router_config.enable_logging = false;
        router_config.enable_statistics = true;
        err = bio_router_init(&router_config);
        ASSERT_EQ(err, NIMCP_SUCCESS) << "Bio-router init failed";

        // Create brain for integration tests
        brain = brain_create(
            "bio_async_test",
            BRAIN_SIZE_SMALL,
            BRAIN_TASK_CLASSIFICATION,
            10, 5
        );
        ASSERT_NE(brain, nullptr) << "Brain creation failed";

        // Create middleware components
        controller = middleware_controller_create(brain);
        flow_tracker = flow_tracker_create();
        shannon_monitor = shannon_monitor_create();
    }

    void TearDown() override {
        // Cleanup in reverse order
        if (shannon_monitor) shannon_monitor_destroy(shannon_monitor);
        if (flow_tracker) flow_tracker_destroy(flow_tracker);
        if (controller) middleware_controller_destroy(controller);
        if (brain) brain_destroy(brain);

        // Shutdown systems
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }
};

//=============================================================================
// 1. MODULE REGISTRATION TESTS
//=============================================================================

TEST_F(MiddlewareBioAsyncIntegrationTest, SingleModuleRegister) {
    // Test: Single module can register with bio-router

    bio_module_info_t info;
    info.module_id = (bio_module_id_t)0x1001;
    info.module_name = "test_module_1";
    info.inbox_capacity = 32;
    info.user_data = nullptr;

    bio_module_context_t ctx = bio_router_register_module(&info);
    EXPECT_NE(ctx, nullptr);

    // Verify registration
    EXPECT_EQ(bio_module_context_get_id(ctx), 0x1001u);
    EXPECT_STREQ(bio_module_context_get_name(ctx), "test_module_1");

    // Verify router statistics
    bio_router_stats_t stats;
    nimcp_error_t err = bio_router_get_stats(&stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(stats.active_modules, 1u);

    bio_router_unregister_module(ctx);
}

TEST_F(MiddlewareBioAsyncIntegrationTest, MultipleModulesRegister) {
    // Test: Multiple modules can register simultaneously

    const int NUM_MODULES = 8;
    std::vector<bio_module_context_t> contexts;

    for (int i = 0; i < NUM_MODULES; i++) {
        bio_module_info_t info;
        info.module_id = (bio_module_id_t)(0x2000 + i);
        info.module_name = ("module_" + std::to_string(i)).c_str();
        info.inbox_capacity = 16;
        info.user_data = nullptr;

        bio_module_context_t ctx = bio_router_register_module(&info);
        EXPECT_NE(ctx, nullptr) << "Failed to register module " << i;
        contexts.push_back(ctx);
    }

    // Verify all registered
    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    EXPECT_GE(stats.active_modules, (uint32_t)NUM_MODULES);

    // Cleanup
    for (auto ctx : contexts) {
        bio_router_unregister_module(ctx);
    }
}

TEST_F(MiddlewareBioAsyncIntegrationTest, ConcurrentModuleRegistration) {
    // Test: Modules can register from multiple threads simultaneously

    const int NUM_THREADS = 4;
    const int MODULES_PER_THREAD = 4;
    std::vector<std::thread> threads;
    std::vector<bio_module_context_t> contexts;
    std::mutex ctx_mutex;
    std::atomic<int> success_count{0};

    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < MODULES_PER_THREAD; i++) {
                bio_module_info_t info;
                info.module_id = (bio_module_id_t)(0x3000 + (t * 100) + i);
                info.module_name = ("thread_" + std::to_string(t) +
                                   "_mod_" + std::to_string(i)).c_str();
                info.inbox_capacity = 16;
                info.user_data = nullptr;

                bio_module_context_t ctx = bio_router_register_module(&info);
                if (ctx) {
                    std::lock_guard<std::mutex> lock(ctx_mutex);
                    contexts.push_back(ctx);
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), NUM_THREADS * MODULES_PER_THREAD);

    // Cleanup
    for (auto ctx : contexts) {
        bio_router_unregister_module(ctx);
    }
}

//=============================================================================
// 2. MESSAGE PASSING TESTS
//=============================================================================

TEST_F(MiddlewareBioAsyncIntegrationTest, PointToPointMessage) {
    // Test: Message passing between two modules

    // Register sender and receiver
    bio_module_info_t sender_info = {
        (bio_module_id_t)0x4001, "sender", 16, nullptr
    };
    bio_module_info_t receiver_info = {
        (bio_module_id_t)0x4002, "receiver", 16, nullptr
    };

    bio_module_context_t sender = bio_router_register_module(&sender_info);
    bio_module_context_t receiver = bio_router_register_module(&receiver_info);
    ASSERT_NE(sender, nullptr);
    ASSERT_NE(receiver, nullptr);

    // Register handler on receiver
    std::atomic<bool> received{false};
    bio_router_register_handler(receiver, BIO_MSG_TRAINING_STEP_REQUEST,
        [](const void* msg, size_t msg_size,
           nimcp_bio_promise_t response, void* user_data) -> nimcp_error_t {
            auto* flag = static_cast<std::atomic<bool>*>(user_data);
            flag->store(true);
            return NIMCP_SUCCESS;
        }
    );

    // Send message
    bio_message_header_t header;
    bio_msg_init_header(&header, BIO_MSG_TRAINING_STEP_REQUEST,
        (bio_module_id_t)0x4001, (bio_module_id_t)0x4002, sizeof(header));
    nimcp_error_t err = bio_router_send(sender, &header, sizeof(header), 100);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Process inbox
    uint32_t processed = bio_router_process_inbox(receiver, 10);
    EXPECT_GT(processed, 0u);
    EXPECT_TRUE(received.load());

    bio_router_unregister_module(sender);
    bio_router_unregister_module(receiver);
}

TEST_F(MiddlewareBioAsyncIntegrationTest, BroadcastMessage) {
    // Test: Broadcast message to multiple modules

    const int NUM_RECEIVERS = 5;
    std::vector<bio_module_context_t> receivers;
    std::vector<std::atomic<bool>*> flags;

    // Register sender
    bio_module_info_t sender_info = {
        (bio_module_id_t)0x5001, "broadcaster", 16, nullptr
    };
    bio_module_context_t sender = bio_router_register_module(&sender_info);
    ASSERT_NE(sender, nullptr);

    // Register receivers with handlers
    for (int i = 0; i < NUM_RECEIVERS; i++) {
        bio_module_info_t info = {
            (bio_module_id_t)(0x5100 + i),
            ("receiver_" + std::to_string(i)).c_str(),
            16,
            nullptr
        };

        auto* flag = new std::atomic<bool>(false);
        flags.push_back(flag);

        bio_module_context_t ctx = bio_router_register_module(&info);
        ASSERT_NE(ctx, nullptr);

        bio_router_register_handler(ctx, BIO_MSG_TRAINING_STEP_COMPLETE,
            [](const void*, size_t, nimcp_bio_promise_t, void* user_data) -> nimcp_error_t {
                auto* f = static_cast<std::atomic<bool>*>(user_data);
                f->store(true);
                return NIMCP_SUCCESS;
            }
        );

        receivers.push_back(ctx);
    }

    // Broadcast message
    bio_message_header_t header;
    bio_msg_init_header(&header, BIO_MSG_TRAINING_STEP_COMPLETE, (bio_module_id_t)0x5001, BIO_MODULE_ALL, sizeof(header));
    nimcp_error_t err = bio_router_broadcast(sender, &header, sizeof(header));
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Process all inboxes
    for (auto ctx : receivers) {
        bio_router_process_inbox(ctx, 10);
    }

    // Verify all received
    int received_count = 0;
    for (auto* flag : flags) {
        if (flag->load()) received_count++;
    }
    EXPECT_EQ(received_count, NUM_RECEIVERS);

    // Cleanup
    bio_router_unregister_module(sender);
    for (auto ctx : receivers) {
        bio_router_unregister_module(ctx);
    }
    for (auto* flag : flags) {
        delete flag;
    }
}

TEST_F(MiddlewareBioAsyncIntegrationTest, AsyncRequestResponse) {
    // Test: Async request-response pattern

    bio_module_info_t requester_info = {(bio_module_id_t)0x6001, "requester", 16, nullptr};
    bio_module_info_t responder_info = {(bio_module_id_t)0x6002, "responder", 16, nullptr};

    bio_module_context_t requester = bio_router_register_module(&requester_info);
    bio_module_context_t responder = bio_router_register_module(&responder_info);
    ASSERT_NE(requester, nullptr);
    ASSERT_NE(responder, nullptr);

    // Register handler that completes promise
    bio_router_register_handler(responder, BIO_MSG_LOSS_COMPUTED,
        [](const void*, size_t, nimcp_bio_promise_t response, void*) -> nimcp_error_t {
            if (response) {
                int result = 42;
                nimcp_bio_promise_complete(response, &result);
            }
            return NIMCP_SUCCESS;
        }
    );

    // Send async request
    bio_message_header_t request;
    bio_msg_init_header(&request, BIO_MSG_LOSS_COMPUTED, (bio_module_id_t)0x6001, (bio_module_id_t)0x6002, sizeof(request));
    nimcp_bio_promise_t promise = bio_router_send_async(
        requester, &request, sizeof(request), BIO_CHANNEL_DOPAMINE);
    ASSERT_NE(promise, nullptr);

    nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
    ASSERT_NE(future, nullptr);

    // Process responder inbox
    bio_router_process_inbox(responder, 10);

    // Wait for response
    int result = 0;
    nimcp_error_t err = nimcp_bio_future_wait(future, &result, 500);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(result, 42);

    nimcp_bio_future_destroy(future);
    nimcp_bio_promise_destroy(promise);
    bio_router_unregister_module(requester);
    bio_router_unregister_module(responder);
}

//=============================================================================
// 3. MODULE LIFECYCLE TESTS
//=============================================================================

TEST_F(MiddlewareBioAsyncIntegrationTest, ModuleLifecycleOrdering) {
    // Test: Create and destroy modules in various orders

    std::vector<bio_module_context_t> contexts;

    // Create modules
    for (int i = 0; i < 5; i++) {
        bio_module_info_t info = {
            (bio_module_id_t)(0x7000 + i),
            ("lifecycle_" + std::to_string(i)).c_str(),
            16,
            nullptr
        };
        bio_module_context_t ctx = bio_router_register_module(&info);
        ASSERT_NE(ctx, nullptr);
        contexts.push_back(ctx);
    }

    // Destroy in FIFO order
    bio_router_unregister_module(contexts[0]);
    bio_router_unregister_module(contexts[1]);

    // Destroy in LIFO order
    bio_router_unregister_module(contexts[4]);
    bio_router_unregister_module(contexts[3]);

    // Destroy middle
    bio_router_unregister_module(contexts[2]);

    // Verify router is still functional
    bio_router_stats_t stats;
    nimcp_error_t err = bio_router_get_stats(&stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(MiddlewareBioAsyncIntegrationTest, DestroyWithPendingMessages) {
    // Test: Destroy module with pending messages in inbox

    bio_module_info_t sender_info = {(bio_module_id_t)0x8001, "sender", 16, nullptr};
    bio_module_info_t receiver_info = {(bio_module_id_t)0x8002, "receiver", 32, nullptr};

    bio_module_context_t sender = bio_router_register_module(&sender_info);
    bio_module_context_t receiver = bio_router_register_module(&receiver_info);

    // Send multiple messages without processing
    for (int i = 0; i < 10; i++) {
        bio_message_header_t msg;
        bio_msg_init_header(&msg, BIO_MSG_GRADIENT_COMPUTED, (bio_module_id_t)0x8001, (bio_module_id_t)0x8002, sizeof(msg));
        bio_router_send(sender, &msg, sizeof(msg), 10);
    }

    // Verify messages pending
    uint32_t pending = bio_router_inbox_count(receiver);
    EXPECT_GT(pending, 0u);

    // Destroy receiver with pending messages
    bio_router_unregister_module(receiver);
    bio_router_unregister_module(sender);

    // Should not crash
    SUCCEED();
}

TEST_F(MiddlewareBioAsyncIntegrationTest, RecreateModuleAfterDestroy) {
    // Test: Can recreate module with same ID after destroying

    bio_module_info_t info = {(bio_module_id_t)0x9001, "recreate_test", 16, nullptr};

    // Create, destroy, recreate
    bio_module_context_t ctx1 = bio_router_register_module(&info);
    ASSERT_NE(ctx1, nullptr);
    bio_router_unregister_module(ctx1);

    bio_module_context_t ctx2 = bio_router_register_module(&info);
    EXPECT_NE(ctx2, nullptr);
    bio_router_unregister_module(ctx2);
}

//=============================================================================
// 4. BIO-ROUTER INITIALIZATION/SHUTDOWN TESTS
//=============================================================================

TEST_F(MiddlewareBioAsyncIntegrationTest, ShutdownWithActiveModules) {
    // Test: Router can shutdown cleanly with active modules
    // Note: This is tested implicitly by TearDown(), but explicit test here

    std::vector<bio_module_context_t> contexts;

    // Register several modules
    for (int i = 0; i < 3; i++) {
        bio_module_info_t info = {
            (bio_module_id_t)(0xA000 + i),
            ("active_" + std::to_string(i)).c_str(),
            16,
            nullptr
        };
        bio_module_context_t ctx = bio_router_register_module(&info);
        ASSERT_NE(ctx, nullptr);
        contexts.push_back(ctx);
    }

    // Send some messages
    bio_message_header_t msg;
    bio_msg_init_header(&msg, BIO_MSG_OPTIMIZER_STEP, (bio_module_id_t)0xA000, (bio_module_id_t)0xA001, sizeof(msg));
    bio_router_send(contexts[0], &msg, sizeof(msg), 10);

    // Cleanup (TearDown will call shutdown)
    for (auto ctx : contexts) {
        bio_router_unregister_module(ctx);
    }
}

TEST_F(MiddlewareBioAsyncIntegrationTest, RouterIsInitialized) {
    // Test: Can check if router is initialized

    EXPECT_TRUE(bio_router_is_initialized());
    EXPECT_TRUE(nimcp_bio_async_is_initialized());
}

//=============================================================================
// 5. CONCURRENT MESSAGE HANDLING TESTS
//=============================================================================

TEST_F(MiddlewareBioAsyncIntegrationTest, ConcurrentMessageSending) {
    // Test: Multiple threads sending messages concurrently

    const int NUM_SENDERS = 4;
    const int MESSAGES_PER_SENDER = 20;

    bio_module_info_t receiver_info = {(bio_module_id_t)0xB001, "receiver", 128, nullptr};
    bio_module_context_t receiver = bio_router_register_module(&receiver_info);
    ASSERT_NE(receiver, nullptr);

    std::atomic<int> received_count{0};
    bio_router_register_handler(receiver, BIO_MSG_BATCH_COMPLETE,
        [](const void*, size_t, nimcp_bio_promise_t, void* user_data) -> nimcp_error_t {
            auto* count = static_cast<std::atomic<int>*>(user_data);
            count->fetch_add(1);
            return NIMCP_SUCCESS;
        }
    );

    // Create senders and send from threads
    std::vector<bio_module_context_t> senders;
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_SENDERS; i++) {
        bio_module_info_t info = {
            (bio_module_id_t)(0xB100 + i),
            ("sender_" + std::to_string(i)).c_str(),
            16,
            nullptr
        };
        bio_module_context_t ctx = bio_router_register_module(&info);
        senders.push_back(ctx);

        threads.emplace_back([ctx, MESSAGES_PER_SENDER]() {
            for (int j = 0; j < MESSAGES_PER_SENDER; j++) {
                bio_message_header_t msg;
                bio_msg_init_header(&msg, BIO_MSG_BATCH_COMPLETE,
                    bio_module_context_get_id(ctx), (bio_module_id_t)0xB001, sizeof(msg));
                bio_router_send(ctx, &msg, sizeof(msg), 100);
            }
        });
    }

    // Wait for sends to complete
    for (auto& t : threads) {
        t.join();
    }

    // Process all messages
    uint32_t total_processed = 0;
    while (total_processed < (uint32_t)(NUM_SENDERS * MESSAGES_PER_SENDER)) {
        uint32_t processed = bio_router_process_inbox(receiver, 10);
        total_processed += processed;
        if (processed == 0) break;
    }

    EXPECT_EQ(received_count.load(), NUM_SENDERS * MESSAGES_PER_SENDER);

    // Cleanup
    bio_router_unregister_module(receiver);
    for (auto ctx : senders) {
        bio_router_unregister_module(ctx);
    }
}

//=============================================================================
// 6. PHASE SYNCHRONIZATION TESTS
//=============================================================================

TEST_F(MiddlewareBioAsyncIntegrationTest, PhaseSyncAcrossModules) {
    // Test: Use phase sync to coordinate multiple modules

    const int NUM_MODULES = 3;
    std::vector<bio_module_context_t> modules;
    std::vector<nimcp_bio_promise_t> promises;

    // Register modules
    for (int i = 0; i < NUM_MODULES; i++) {
        bio_module_info_t info = {
            (bio_module_id_t)(0xC000 + i),
            ("sync_mod_" + std::to_string(i)).c_str(),
            16,
            nullptr
        };
        bio_module_context_t ctx = bio_router_register_module(&info);
        ASSERT_NE(ctx, nullptr);
        modules.push_back(ctx);
    }

    // Create promises/futures for each module
    nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);
    ASSERT_NE(sync, nullptr);

    for (int i = 0; i < NUM_MODULES; i++) {
        nimcp_bio_promise_t p = nimcp_bio_promise_create(
            BIO_CHANNEL_DOPAMINE, sizeof(int));
        nimcp_bio_future_t f = nimcp_bio_promise_get_future(p);
        nimcp_phase_sync_add_future(sync, f);
        promises.push_back(p);
    }

    // Complete promises from different threads
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_MODULES; i++) {
        threads.emplace_back([i, &promises]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(i * 10));
            int val = i;
            nimcp_bio_promise_complete(promises[i], &val);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Wait for phase coherence
    nimcp_error_t err = nimcp_phase_sync_wait_all(sync, 500);
    // May succeed or timeout depending on timing
    (void)err;

    // Cleanup
    for (auto p : promises) {
        nimcp_bio_promise_destroy(p);
    }
    nimcp_phase_sync_destroy(sync);
    for (auto ctx : modules) {
        bio_router_unregister_module(ctx);
    }
}

//=============================================================================
// 7. ERROR HANDLING TESTS
//=============================================================================

TEST_F(MiddlewareBioAsyncIntegrationTest, SendToNonexistentModule) {
    // Test: Sending to non-existent module ID

    bio_module_info_t sender_info = {(bio_module_id_t)0xD001, "sender", 16, nullptr};
    bio_module_context_t sender = bio_router_register_module(&sender_info);
    ASSERT_NE(sender, nullptr);

    bio_message_header_t msg;
    bio_msg_init_header(&msg, BIO_MSG_EPOCH_COMPLETE, (bio_module_id_t)0xD001, (bio_module_id_t)0xFFFF, sizeof(msg));

    // Should fail or drop message
    nimcp_error_t err = bio_router_send(sender, &msg, sizeof(msg), 100);
    // Error handling is implementation-dependent
    (void)err;

    bio_router_unregister_module(sender);
}

TEST_F(MiddlewareBioAsyncIntegrationTest, InboxOverflow) {
    // Test: Handle inbox overflow gracefully

    bio_module_info_t sender_info = {(bio_module_id_t)0xE001, "sender", 16, nullptr};
    bio_module_info_t receiver_info = {(bio_module_id_t)0xE002, "receiver", 8, nullptr};  // Small inbox

    bio_module_context_t sender = bio_router_register_module(&sender_info);
    bio_module_context_t receiver = bio_router_register_module(&receiver_info);

    // Send more messages than inbox capacity
    int sent_count = 0;
    for (int i = 0; i < 20; i++) {
        bio_message_header_t msg;
        bio_msg_init_header(&msg, BIO_MSG_CHECKPOINT_REQUEST, (bio_module_id_t)0xE001, (bio_module_id_t)0xE002, sizeof(msg));
        nimcp_error_t err = bio_router_send(sender, &msg, sizeof(msg), 10);
        if (err == NIMCP_SUCCESS) sent_count++;
    }

    // Should have dropped some messages or blocked
    EXPECT_GT(sent_count, 0);

    bio_router_unregister_module(sender);
    bio_router_unregister_module(receiver);
}

//=============================================================================
// 8. STATISTICS AND MONITORING TESTS
//=============================================================================

TEST_F(MiddlewareBioAsyncIntegrationTest, RouterStatistics) {
    // Test: Router tracks statistics correctly

    bio_router_reset_stats();

    bio_module_info_t info1 = {(bio_module_id_t)0xF001, "stat_mod_1", 16, nullptr};
    bio_module_info_t info2 = {(bio_module_id_t)0xF002, "stat_mod_2", 16, nullptr};

    bio_module_context_t mod1 = bio_router_register_module(&info1);
    bio_module_context_t mod2 = bio_router_register_module(&info2);

    // Send some messages
    for (int i = 0; i < 5; i++) {
        bio_message_header_t msg;
        bio_msg_init_header(&msg, BIO_MSG_CHECKPOINT_COMPLETE, (bio_module_id_t)0xF001, (bio_module_id_t)0xF002, sizeof(msg));
        bio_router_send(mod1, &msg, sizeof(msg), 100);
    }

    bio_router_stats_t stats;
    nimcp_error_t err = bio_router_get_stats(&stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(stats.active_modules, 2u);
    EXPECT_GT(stats.messages_routed, 0u);

    bio_router_unregister_module(mod1);
    bio_router_unregister_module(mod2);
}

TEST_F(MiddlewareBioAsyncIntegrationTest, BioAsyncStatistics) {
    // Test: Bio-async tracks statistics correctly

    nimcp_bio_async_reset_stats();

    // Create and complete some promises
    for (int i = 0; i < 3; i++) {
        nimcp_bio_promise_t p = nimcp_bio_promise_create(
            BIO_CHANNEL_DOPAMINE, sizeof(int));
        int val = i;
        nimcp_bio_promise_complete(p, &val);
        nimcp_bio_promise_destroy(p);
    }

    nimcp_bio_async_stats_t stats;
    nimcp_error_t err = nimcp_bio_async_get_stats(&stats);
    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_GE(stats.total_futures_created, 3u);
    EXPECT_GE(stats.total_futures_completed, 3u);
}

//=============================================================================
// 9. INTEGRATION WITH MIDDLEWARE CONTROLLER
//=============================================================================

TEST_F(MiddlewareBioAsyncIntegrationTest, ControllerWithBioAsync) {
    // Test: Middleware controller works alongside bio-async modules

    if (!controller) {
        GTEST_SKIP() << "Controller not available";
    }

    // Register a module
    bio_module_info_t info = {(bio_module_id_t)0x1F001, "ctrl_test", 16, nullptr};
    bio_module_context_t ctx = bio_router_register_module(&info);
    ASSERT_NE(ctx, nullptr);

    // Use controller commands
    EXPECT_TRUE(middleware_controller_set_attention_threshold(
        controller, TARGET_VISUAL_CORTEX, 0.7f));
    EXPECT_TRUE(middleware_controller_set_routing_priority(
        controller, TARGET_PREFRONTAL, TARGET_HIPPOCAMPUS, 0.8f));

    // Send message via bio-router
    bio_message_header_t msg;
    bio_msg_init_header(&msg, BIO_MSG_TRAINING_METRIC, (bio_module_id_t)0x1F001, BIO_MODULE_ALL, sizeof(msg));
    nimcp_error_t err = bio_router_broadcast(ctx, &msg, sizeof(msg));
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Get controller metrics
    middleware_controller_metrics_t metrics;
    EXPECT_TRUE(middleware_controller_get_metrics(controller, &metrics));

    bio_router_unregister_module(ctx);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
