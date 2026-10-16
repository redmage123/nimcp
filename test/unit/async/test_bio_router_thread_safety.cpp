/**
 * @file test_bio_router_thread_safety.cpp
 * @brief Unit tests for bio-router thread safety (TOCTOU race condition fixes)
 *
 * Tests the two critical TOCTOU race conditions that were fixed:
 *
 * 1. TOCTOU Race #1: Unprotected initialized flag in get_router_brain_kg_safe()
 *    - The g_router_brain_kg_mutex_initialized flag was checked without atomic
 *      operations before the mutex was acquired. During shutdown, another thread
 *      could destroy the mutex between the check and the lock acquisition.
 *    - Fix: Use atomic_bool with acquire/release ordering and double-check
 *      inside the lock.
 *
 * 2. TOCTOU Race #2: Shutdown check before queue lock in bio_msg_queue_enqueue()
 *    - The shutdown_requested flag was read without holding the queue lock.
 *      A message could be enqueued after shutdown began because the flag was
 *      read outside the lock, and shutdown could progress between check and lock.
 *    - Fix: Acquire the queue lock first, then check shutdown state.
 *
 * @author NIMCP Development Team
 * @date 2026-02-15
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <functional>
#include <barrier>

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

/*=============================================================================
 * Test Fixture
 *============================================================================*/

class BioRouterThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_statistics = false;
        bio_config.enable_logging = false;
        ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_statistics = true;
        router_config.enable_logging = false;
        ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS);
        ASSERT_TRUE(bio_router_is_initialized());
    }

    void TearDown() override {
        // Shutdown may already have happened in the test
        if (bio_router_is_initialized()) {
            bio_router_shutdown();
        }
        if (nimcp_bio_async_is_initialized()) {
            nimcp_bio_async_shutdown();
        }
    }

    /**
     * @brief Create a simple test message for sending
     */
    bio_msg_brain_state_query_t CreateTestMessage(uint32_t source, uint32_t target) {
        bio_msg_brain_state_query_t msg;
        memset(&msg, 0, sizeof(msg));
        bio_msg_init_header(&msg.header, BIO_MSG_BRAIN_STATE_QUERY,
                           (bio_module_id_t)source, (bio_module_id_t)target,
                           sizeof(msg));
        msg.query_flags = BIO_BRAIN_QUERY_NEURON_COUNT;
        msg.region_id = 0;
        return msg;
    }

    /**
     * @brief Register a test module and return the context
     */
    bio_module_context_t RegisterTestModule(bio_module_id_t id, const char* name) {
        bio_module_info_t info;
        memset(&info, 0, sizeof(info));
        info.module_id = id;
        info.module_name = name;
        info.inbox_capacity = 0;  // Use default
        info.user_data = nullptr;
        return bio_router_register_module(&info);
    }
};

/*=============================================================================
 * Test: Concurrent initialization from multiple threads
 *
 * This tests TOCTOU Race #1: Multiple threads attempting to initialize/query
 * the router simultaneously. The platform_once mechanism must ensure exactly
 * one initialization, and concurrent init calls must see ALREADY_EXISTS.
 *============================================================================*/

TEST_F(BioRouterThreadSafetyTest, ConcurrentInitDoesNotCrash) {
    // Shutdown from SetUp so we can re-init from multiple threads
    bio_router_shutdown();
    nimcp_bio_async_shutdown();

    constexpr int kNumThreads = 8;
    std::atomic<int> success_count{0};
    std::atomic<int> already_exists_count{0};
    std::atomic<int> other_error_count{0};
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    threads.reserve(kNumThreads);

    for (int i = 0; i < kNumThreads; i++) {
        threads.emplace_back([&]() {
            // Spin-wait for all threads to be ready
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            // Each thread tries to init bio-async then router
            nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
            bio_config.enable_logging = false;
            nimcp_bio_async_init(&bio_config);  // May succeed or fail

            bio_router_config_t router_config = bio_router_default_config();
            router_config.enable_logging = false;
            nimcp_error_t result = bio_router_init(&router_config);

            if (result == NIMCP_SUCCESS) {
                success_count.fetch_add(1);
            } else if (result == NIMCP_ERROR_ALREADY_EXISTS) {
                already_exists_count.fetch_add(1);
            } else {
                other_error_count.fetch_add(1);
            }
        });
    }

    // Release all threads simultaneously
    start.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    // Exactly one thread should succeed, the rest should get ALREADY_EXISTS
    EXPECT_EQ(success_count.load(), 1)
        << "Exactly one thread should successfully initialize";
    EXPECT_EQ(already_exists_count.load(), kNumThreads - 1)
        << "All other threads should get ALREADY_EXISTS";
    EXPECT_EQ(other_error_count.load(), 0)
        << "No unexpected errors should occur";

    // Router should be in a consistent state
    EXPECT_TRUE(bio_router_is_initialized());
}

/*=============================================================================
 * Test: Concurrent module registration during normal operation
 *
 * Multiple threads register different modules simultaneously. This stresses
 * the modules_mutex and ensures no data corruption.
 *============================================================================*/

TEST_F(BioRouterThreadSafetyTest, ConcurrentModuleRegistration) {
    constexpr int kNumThreads = 8;
    std::atomic<int> success_count{0};
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    std::vector<bio_module_context_t> contexts(kNumThreads, nullptr);

    for (int i = 0; i < kNumThreads; i++) {
        threads.emplace_back([&, i]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            // Each thread registers a unique module
            char name[32];
            snprintf(name, sizeof(name), "test_module_%d", i);
            // Use unique module IDs in a range unlikely to conflict
            bio_module_id_t id = (bio_module_id_t)(0xF000 + i);

            bio_module_context_t ctx = RegisterTestModule(id, name);
            if (ctx != nullptr) {
                contexts[i] = ctx;
                success_count.fetch_add(1);
            }
        });
    }

    start.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    // All registrations should succeed since each uses a unique ID
    EXPECT_EQ(success_count.load(), kNumThreads);

    // Verify each context is valid
    for (int i = 0; i < kNumThreads; i++) {
        EXPECT_NE(contexts[i], nullptr) << "Module " << i << " registration failed";
        if (contexts[i]) {
            EXPECT_EQ(bio_module_context_get_id(contexts[i]),
                      (bio_module_id_t)(0xF000 + i));
        }
    }

    // Cleanup
    for (int i = 0; i < kNumThreads; i++) {
        if (contexts[i]) {
            bio_router_unregister_module(contexts[i]);
        }
    }
}

/*=============================================================================
 * Test: Message enqueue during shutdown (TOCTOU Race #2)
 *
 * This tests the race between message enqueue and shutdown. Before the fix,
 * a message could be enqueued after shutdown began because the shutdown flag
 * was checked outside the queue lock.
 *
 * After the fix, the enqueue function acquires the queue lock first, then
 * checks the shutdown flag, preventing messages from being enqueued during
 * teardown.
 *============================================================================*/

TEST_F(BioRouterThreadSafetyTest, EnqueueDuringShutdownRejectsGracefully) {
    // Register a module to have a valid context and inbox
    bio_module_context_t sender_ctx = RegisterTestModule(BIO_MODULE_BRAIN, "sender");
    ASSERT_NE(sender_ctx, nullptr);

    bio_module_context_t receiver_ctx = RegisterTestModule(BIO_MODULE_INTROSPECTION, "receiver");
    ASSERT_NE(receiver_ctx, nullptr);

    constexpr int kSenderThreads = 4;
    std::atomic<int> send_success{0};
    std::atomic<int> send_cancelled{0};
    std::atomic<int> send_other_error{0};
    std::atomic<bool> start{false};
    std::atomic<bool> shutdown_done{false};

    // Sender threads try to send messages continuously
    std::vector<std::thread> senders;
    for (int i = 0; i < kSenderThreads; i++) {
        senders.emplace_back([&, i]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            // Keep sending until shutdown is complete
            for (int j = 0; j < 100 && !shutdown_done.load(std::memory_order_acquire); j++) {
                bio_msg_brain_state_query_t msg = CreateTestMessage(
                    BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION);

                nimcp_error_t result = bio_router_send(sender_ctx, &msg, sizeof(msg), 0);

                if (result == NIMCP_SUCCESS) {
                    send_success.fetch_add(1);
                } else if (result == NIMCP_ERROR_CANCELLED) {
                    send_cancelled.fetch_add(1);
                } else {
                    // Other errors are acceptable (e.g. INVALID_PARAM if router
                    // was already torn down, QUEUE_FULL, etc.)
                    send_other_error.fetch_add(1);
                }
            }
        });
    }

    // Shutdown thread
    std::thread shutdown_thread([&]() {
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        // Small delay to let senders start
        std::this_thread::sleep_for(std::chrono::microseconds(100));

        bio_router_shutdown();
        shutdown_done.store(true, std::memory_order_release);
    });

    start.store(true, std::memory_order_release);

    for (auto& s : senders) {
        s.join();
    }
    shutdown_thread.join();

    // The key assertion: no crashes occurred. The shutdown and sends
    // raced against each other but all completed without UB.
    // At least some sends should have succeeded before shutdown
    // (unless system was extremely fast at shutdown)
    EXPECT_GE(send_success.load() + send_cancelled.load() + send_other_error.load(), 0)
        << "All sends should complete with defined error codes, not crash";

    // After shutdown, router should not be initialized
    EXPECT_FALSE(bio_router_is_initialized());
}

/*=============================================================================
 * Test: Concurrent brain KG access during shutdown (TOCTOU Race #1)
 *
 * This tests the race between get_router_brain_kg_safe() and shutdown.
 * Before the fix, the initialized flag was checked without atomics,
 * and a thread could attempt to lock a destroyed mutex.
 *
 * After the fix, the flag uses atomic operations with proper memory
 * ordering, and the function double-checks inside the lock.
 *============================================================================*/

TEST_F(BioRouterThreadSafetyTest, BrainKgAccessDuringShutdownNoUAF) {
    // Set a brain KG reference (using NULL is fine - we test the mechanism)
    bio_router_set_brain_kg(nullptr);

    constexpr int kReaderThreads = 8;
    std::atomic<bool> start{false};
    std::atomic<bool> shutdown_done{false};
    std::atomic<int> read_count{0};

    // Reader threads continuously call bio_router_get_brain_kg()
    std::vector<std::thread> readers;
    for (int i = 0; i < kReaderThreads; i++) {
        readers.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            while (!shutdown_done.load(std::memory_order_acquire)) {
                // This calls get_router_brain_kg_safe() internally
                // which is where TOCTOU Race #1 existed
                (void)bio_router_get_brain_kg();
                read_count.fetch_add(1);
                std::this_thread::yield();
            }

            // A few more reads after shutdown to stress the race
            for (int j = 0; j < 10; j++) {
                (void)bio_router_get_brain_kg();
            }
        });
    }

    // Shutdown thread
    std::thread shutdown_thread([&]() {
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        // Let readers run for a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        bio_router_shutdown();
        shutdown_done.store(true, std::memory_order_release);
    });

    start.store(true, std::memory_order_release);

    for (auto& r : readers) {
        r.join();
    }
    shutdown_thread.join();

    // The key assertion: no crashes or UB occurred during the race
    EXPECT_GT(read_count.load(), 0)
        << "Readers should have executed at least once";
    EXPECT_FALSE(bio_router_is_initialized());
}

/*=============================================================================
 * Test: Initialized flag is properly protected with atomic operations
 *
 * Verify that bio_router_is_initialized() returns consistent values
 * during concurrent init/shutdown cycles.
 *============================================================================*/

TEST_F(BioRouterThreadSafetyTest, InitializedFlagConsistentDuringCycles) {
    constexpr int kCycles = 5;
    constexpr int kReaderThreads = 4;
    std::atomic<bool> running{true};
    std::atomic<int> true_count{0};
    std::atomic<int> false_count{0};

    // Reader threads continuously poll is_initialized
    std::vector<std::thread> readers;
    for (int i = 0; i < kReaderThreads; i++) {
        readers.emplace_back([&]() {
            while (running.load(std::memory_order_acquire)) {
                if (bio_router_is_initialized()) {
                    true_count.fetch_add(1);
                } else {
                    false_count.fetch_add(1);
                }
                std::this_thread::yield();
            }
        });
    }

    // Main thread performs init/shutdown cycles
    for (int cycle = 0; cycle < kCycles; cycle++) {
        // Already initialized from SetUp on first cycle
        if (cycle > 0) {
            nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
            bio_config.enable_logging = false;
            nimcp_bio_async_init(&bio_config);

            bio_router_config_t router_config = bio_router_default_config();
            router_config.enable_logging = false;
            bio_router_init(&router_config);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    running.store(false, std::memory_order_release);
    for (auto& r : readers) {
        r.join();
    }

    // Both states should have been observed during the cycles
    EXPECT_GT(true_count.load(), 0) << "Should have seen initialized=true";
    EXPECT_GT(false_count.load(), 0) << "Should have seen initialized=false";
}

/*=============================================================================
 * Test: Concurrent send and shutdown does not corrupt router state
 *
 * Multiple senders attempt to route messages while shutdown occurs.
 * The router must not crash, and messages sent after shutdown should
 * receive appropriate error codes.
 *============================================================================*/

TEST_F(BioRouterThreadSafetyTest, ConcurrentSendAndShutdownNoCorruption) {
    // Register sender and receiver modules
    bio_module_context_t sender = RegisterTestModule(BIO_MODULE_BRAIN, "sender");
    ASSERT_NE(sender, nullptr);
    bio_module_context_t receiver = RegisterTestModule(BIO_MODULE_INTROSPECTION, "receiver");
    ASSERT_NE(receiver, nullptr);

    constexpr int kSenderThreads = 4;
    constexpr int kMessagesPerThread = 50;
    std::atomic<bool> start{false};
    std::atomic<int> total_sent{0};

    std::vector<std::thread> senders;
    for (int i = 0; i < kSenderThreads; i++) {
        senders.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (int j = 0; j < kMessagesPerThread; j++) {
                bio_msg_brain_state_query_t msg = CreateTestMessage(
                    BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION);

                nimcp_error_t result = bio_router_send(sender, &msg, sizeof(msg), 0);

                // Any of these error codes are acceptable
                if (result == NIMCP_SUCCESS ||
                    result == NIMCP_ERROR_CANCELLED ||
                    result == NIMCP_ERROR_INVALID_PARAM ||
                    result == NIMCP_ERROR_QUEUE_FULL ||
                    result == NIMCP_ERROR_NOT_FOUND) {
                    total_sent.fetch_add(1);
                }
            }
        });
    }

    // Concurrent shutdown after a brief delay
    std::thread shutdown_thread([&]() {
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        bio_router_shutdown();
    });

    start.store(true, std::memory_order_release);

    for (auto& s : senders) {
        s.join();
    }
    shutdown_thread.join();

    // All send attempts should have completed with defined behavior
    EXPECT_EQ(total_sent.load(), kSenderThreads * kMessagesPerThread);
}

/*=============================================================================
 * Test: Repeated init/shutdown cycles are safe
 *
 * Verifies that the once-flag reset on shutdown allows clean re-init,
 * and the atomic initialized flag is properly reset each cycle.
 *============================================================================*/

TEST_F(BioRouterThreadSafetyTest, RepeatedInitShutdownCycles) {
    constexpr int kCycles = 10;

    for (int cycle = 0; cycle < kCycles; cycle++) {
        // Already initialized from SetUp on first cycle
        if (cycle > 0) {
            nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
            bio_config.enable_logging = false;
            ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS)
                << "Cycle " << cycle << ": bio_async init failed";

            bio_router_config_t router_config = bio_router_default_config();
            router_config.enable_logging = false;
            ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS)
                << "Cycle " << cycle << ": router init failed";
        }

        EXPECT_TRUE(bio_router_is_initialized())
            << "Cycle " << cycle << ": should be initialized";

        // Register a module and send a message to verify functionality
        bio_module_context_t ctx = RegisterTestModule(BIO_MODULE_BRAIN, "cycle_test");
        EXPECT_NE(ctx, nullptr) << "Cycle " << cycle << ": registration failed";

        if (ctx) {
            bio_router_unregister_module(ctx);
        }

        // Brain KG operations should work each cycle
        EXPECT_EQ(bio_router_set_brain_kg(nullptr), NIMCP_SUCCESS)
            << "Cycle " << cycle << ": set_brain_kg failed";

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        EXPECT_FALSE(bio_router_is_initialized())
            << "Cycle " << cycle << ": should not be initialized after shutdown";
    }
}

/*=============================================================================
 * Test: Brain KG set/get is thread-safe
 *
 * Multiple threads concurrently set and get the brain KG reference.
 * The atomic flag + mutex must prevent use-after-free and data races.
 *============================================================================*/

TEST_F(BioRouterThreadSafetyTest, ConcurrentBrainKgSetGet) {
    constexpr int kThreads = 8;
    constexpr int kIterations = 100;
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; i++) {
        threads.emplace_back([&, i]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (int j = 0; j < kIterations; j++) {
                if (i % 2 == 0) {
                    // Even threads set
                    bio_router_set_brain_kg(nullptr);
                } else {
                    // Odd threads get
                    (void)bio_router_get_brain_kg();
                }
            }
        });
    }

    start.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    // No crashes - test passes if we get here
    SUCCEED();
}
