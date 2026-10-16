/**
 * @file test_bio_router_toctou_regression.cpp
 * @brief Regression tests for bio-router TOCTOU race conditions
 *
 * These tests verify that two specific TOCTOU (Time-of-Check-Time-of-Use)
 * race conditions in bio_router.c remain fixed:
 *
 * REGRESSION #1 (was at ~line 190):
 *   get_router_brain_kg_safe() checked g_router_brain_kg_mutex_initialized
 *   without atomic operations before acquiring the mutex. A concurrent
 *   shutdown could destroy the mutex between the check and the lock.
 *   Fix: atomic_bool with acquire/release ordering + double-check after lock.
 *
 * REGRESSION #2 (was at ~line 393):
 *   bio_msg_queue_enqueue() checked shutdown_requested without holding the
 *   queue lock. A message could be enqueued after shutdown began.
 *   Fix: acquire queue lock first, then check shutdown flag.
 *
 * @author NIMCP Development Team
 * @date 2026-02-15
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

/*=============================================================================
 * Test Fixture
 *============================================================================*/

class BioRouterTOCTOURegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_logging = false;
        ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS);

        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_logging = false;
        router_config.enable_statistics = true;
        ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (bio_router_is_initialized()) {
            bio_router_shutdown();
        }
        if (nimcp_bio_async_is_initialized()) {
            nimcp_bio_async_shutdown();
        }
    }

    bio_module_context_t RegisterModule(bio_module_id_t id, const char* name) {
        bio_module_info_t info;
        memset(&info, 0, sizeof(info));
        info.module_id = id;
        info.module_name = name;
        info.inbox_capacity = 0;
        info.user_data = nullptr;
        return bio_router_register_module(&info);
    }

    bio_msg_brain_state_query_t MakeMessage(uint32_t source, uint32_t target) {
        bio_msg_brain_state_query_t msg;
        memset(&msg, 0, sizeof(msg));
        bio_msg_init_header(&msg.header, BIO_MSG_BRAIN_STATE_QUERY,
                           (bio_module_id_t)source, (bio_module_id_t)target,
                           sizeof(msg));
        msg.query_flags = BIO_BRAIN_QUERY_NEURON_COUNT;
        msg.region_id = 0;
        return msg;
    }
};

/*=============================================================================
 * REGRESSION #1: Brain KG mutex initialized flag TOCTOU
 *
 * Before fix: get_router_brain_kg_safe() used plain bool check, allowing
 * a thread to see initialized=true, then shutdown destroys mutex, then
 * thread tries to lock destroyed mutex -> undefined behavior.
 *
 * After fix: atomic_bool with acquire/release ordering prevents this race.
 *============================================================================*/

/**
 * @test Verify brain KG accessor does not crash during concurrent shutdown
 *
 * Strategy: Launch many reader threads that continuously call
 * bio_router_get_brain_kg() while a shutdown thread tears down the router.
 * Before the fix, this would occasionally crash with use-after-free on the
 * mutex. After the fix, the atomic flag ensures readers bail out safely.
 */
TEST_F(BioRouterTOCTOURegressionTest, BrainKgAccessorSafeDuringShutdown) {
    // Set a brain KG (NULL value is fine - testing the accessor mechanism)
    ASSERT_EQ(bio_router_set_brain_kg(nullptr), NIMCP_SUCCESS);

    constexpr int kReaderThreads = 16;
    constexpr int kReadsPerThread = 1000;
    std::atomic<bool> start{false};
    std::atomic<int> completed_reads{0};

    std::vector<std::thread> readers;
    for (int i = 0; i < kReaderThreads; i++) {
        readers.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (int j = 0; j < kReadsPerThread; j++) {
                // This exercises get_router_brain_kg_safe() internally
                (void)bio_router_get_brain_kg();
                completed_reads.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // Shutdown thread
    std::thread shutdowner([&]() {
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        // Brief delay to let readers start
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        bio_router_shutdown();
    });

    start.store(true, std::memory_order_release);

    for (auto& r : readers) {
        r.join();
    }
    shutdowner.join();

    // If we reach here without crashing, the TOCTOU race is fixed
    EXPECT_GT(completed_reads.load(), 0)
        << "At least some brain KG reads should have completed";
}

/**
 * @test Verify brain KG accessor returns NULL after shutdown
 *
 * After shutdown, the atomic flag should be false, and all subsequent
 * calls to bio_router_get_brain_kg() should return NULL without crashing.
 */
TEST_F(BioRouterTOCTOURegressionTest, BrainKgReturnsNullAfterShutdown) {
    ASSERT_EQ(bio_router_set_brain_kg(nullptr), NIMCP_SUCCESS);

    bio_router_shutdown();

    // Multiple calls after shutdown should all return NULL safely
    for (int i = 0; i < 100; i++) {
        struct brain_kg* kg = bio_router_get_brain_kg();
        EXPECT_EQ(kg, nullptr) << "Iteration " << i;
    }
}

/**
 * @test Verify atomic flag is properly reset during init/shutdown cycles
 *
 * The atomic flag for the brain KG mutex must be set on init and cleared
 * on shutdown. This test cycles through multiple init/shutdown pairs and
 * verifies the flag state via bio_router_get_brain_kg() behavior.
 */
TEST_F(BioRouterTOCTOURegressionTest, AtomicFlagResetOnCycles) {
    constexpr int kCycles = 10;

    for (int cycle = 0; cycle < kCycles; cycle++) {
        if (cycle > 0) {
            nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
            bio_config.enable_logging = false;
            ASSERT_EQ(nimcp_bio_async_init(&bio_config), NIMCP_SUCCESS)
                << "Cycle " << cycle;

            bio_router_config_t router_config = bio_router_default_config();
            router_config.enable_logging = false;
            ASSERT_EQ(bio_router_init(&router_config), NIMCP_SUCCESS)
                << "Cycle " << cycle;
        }

        // While initialized, set_brain_kg should work
        EXPECT_EQ(bio_router_set_brain_kg(nullptr), NIMCP_SUCCESS)
            << "Cycle " << cycle << ": set_brain_kg should succeed while initialized";

        // bio_router_get_brain_kg should not crash
        (void)bio_router_get_brain_kg();

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        // After shutdown, get should return NULL (flag cleared)
        EXPECT_EQ(bio_router_get_brain_kg(), nullptr)
            << "Cycle " << cycle << ": should return NULL after shutdown";
    }
}

/*=============================================================================
 * REGRESSION #2: Enqueue shutdown check TOCTOU
 *
 * Before fix: bio_msg_queue_enqueue() checked g_router->shutdown_requested
 * WITHOUT holding the queue lock, then acquired the lock. Between the check
 * and the lock, shutdown could set the flag and begin teardown, causing a
 * message to be enqueued during or after teardown.
 *
 * After fix: The queue lock is acquired first, then shutdown is checked
 * inside the lock, making the check-and-enqueue atomic.
 *============================================================================*/

/**
 * @test Verify enqueue rejects messages during shutdown
 *
 * Strategy: Register modules, then launch sender threads that continuously
 * try to enqueue messages while a concurrent thread initiates shutdown.
 * All sends must either succeed (before shutdown) or return a defined
 * error code (during/after shutdown). No crashes should occur.
 */
TEST_F(BioRouterTOCTOURegressionTest, EnqueueRejectsDuringShutdown) {
    bio_module_context_t sender = RegisterModule(BIO_MODULE_BRAIN, "sender");
    ASSERT_NE(sender, nullptr);

    bio_module_context_t receiver = RegisterModule(BIO_MODULE_INTROSPECTION, "receiver");
    ASSERT_NE(receiver, nullptr);

    constexpr int kSenderThreads = 8;
    constexpr int kMessagesPerThread = 200;
    std::atomic<bool> start{false};
    std::atomic<int> success_count{0};
    std::atomic<int> cancelled_count{0};
    std::atomic<int> error_count{0};

    std::vector<std::thread> senders;
    for (int i = 0; i < kSenderThreads; i++) {
        senders.emplace_back([&]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (int j = 0; j < kMessagesPerThread; j++) {
                bio_msg_brain_state_query_t msg = MakeMessage(
                    BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION);

                nimcp_error_t result = bio_router_send(sender, &msg, sizeof(msg), 0);

                if (result == NIMCP_SUCCESS) {
                    success_count.fetch_add(1);
                } else if (result == NIMCP_ERROR_CANCELLED) {
                    cancelled_count.fetch_add(1);
                } else {
                    // INVALID_PARAM, NOT_FOUND, QUEUE_FULL are all acceptable
                    error_count.fetch_add(1);
                }
            }
        });
    }

    std::thread shutdowner([&]() {
        while (!start.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        // Let senders get some messages through first
        std::this_thread::sleep_for(std::chrono::microseconds(200));
        bio_router_shutdown();
    });

    start.store(true, std::memory_order_release);

    for (auto& s : senders) {
        s.join();
    }
    shutdowner.join();

    int total = success_count.load() + cancelled_count.load() + error_count.load();
    EXPECT_EQ(total, kSenderThreads * kMessagesPerThread)
        << "All send attempts should have returned a defined result";

    // Some messages should have succeeded before shutdown
    EXPECT_GT(success_count.load(), 0)
        << "Some messages should have been sent before shutdown";
}

/**
 * @test Verify no messages are enqueued after shutdown_requested is set
 *
 * This is the core regression: after shutdown sets the flag and wakes
 * all waiters, no new messages should enter the queue. We verify by
 * checking that the router is in a clean state after shutdown.
 */
TEST_F(BioRouterTOCTOURegressionTest, NoEnqueueAfterShutdownFlag) {
    bio_module_context_t sender = RegisterModule(BIO_MODULE_BRAIN, "sender");
    ASSERT_NE(sender, nullptr);
    bio_module_context_t receiver = RegisterModule(BIO_MODULE_INTROSPECTION, "receiver");
    ASSERT_NE(receiver, nullptr);

    // Send some messages first
    for (int i = 0; i < 5; i++) {
        bio_msg_brain_state_query_t msg = MakeMessage(
            BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION);
        EXPECT_EQ(bio_router_send(sender, &msg, sizeof(msg), 0), NIMCP_SUCCESS);
    }

    // Verify stats show messages routed
    bio_router_stats_t stats;
    ASSERT_EQ(bio_router_get_stats(&stats), NIMCP_SUCCESS);
    EXPECT_GE(stats.messages_routed, 5u);

    // Now shutdown
    bio_router_shutdown();
    EXPECT_FALSE(bio_router_is_initialized());

    // Attempting to send after shutdown should not crash
    bio_msg_brain_state_query_t msg = MakeMessage(
        BIO_MODULE_BRAIN, BIO_MODULE_INTROSPECTION);
    nimcp_error_t result = bio_router_send(sender, &msg, sizeof(msg), 0);
    // Should fail with some error (INVALID_PARAM since g_router is NULL)
    EXPECT_NE(result, NIMCP_SUCCESS)
        << "Sending after shutdown should fail";
}

/**
 * @test Stress test: rapid init/shutdown cycles with concurrent message traffic
 *
 * This is the most aggressive regression test. It rapidly cycles through
 * init/shutdown while sender threads try to send messages. Both TOCTOU
 * races would be triggered by this pattern.
 */
TEST_F(BioRouterTOCTOURegressionTest, StressRapidCyclesWithTraffic) {
    constexpr int kCycles = 5;
    constexpr int kSenderThreads = 4;
    std::atomic<bool> running{true};
    std::atomic<int> total_attempts{0};

    // Sender threads continuously try to send
    std::vector<std::thread> senders;
    for (int i = 0; i < kSenderThreads; i++) {
        senders.emplace_back([&, i]() {
            while (running.load(std::memory_order_acquire)) {
                if (bio_router_is_initialized()) {
                    // Try to register, send, and access brain KG
                    char name[32];
                    snprintf(name, sizeof(name), "stress_%d", i);
                    bio_module_info_t info;
                    memset(&info, 0, sizeof(info));
                    info.module_id = (bio_module_id_t)(0xE000 + i);
                    info.module_name = name;
                    bio_module_context_t ctx = bio_router_register_module(&info);

                    if (ctx) {
                        bio_msg_brain_state_query_t msg;
                        memset(&msg, 0, sizeof(msg));
                        bio_msg_init_header(&msg.header, BIO_MSG_BRAIN_STATE_QUERY,
                                           (bio_module_id_t)(0xE000 + i),
                                           BIO_MODULE_BRAIN,
                                           sizeof(msg));
                        msg.query_flags = BIO_BRAIN_QUERY_NEURON_COUNT;
                        // Send attempt - may fail, that's OK
                        (void)bio_router_send(ctx, &msg, sizeof(msg), 0);
                        bio_router_unregister_module(ctx);
                    }

                    // Also exercise brain KG accessor (TOCTOU Race #1)
                    (void)bio_router_get_brain_kg();

                    total_attempts.fetch_add(1, std::memory_order_relaxed);
                }
                std::this_thread::yield();
            }
        });
    }

    // Main thread cycles init/shutdown
    for (int cycle = 0; cycle < kCycles; cycle++) {
        if (cycle > 0) {
            nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
            bio_config.enable_logging = false;
            nimcp_bio_async_init(&bio_config);

            bio_router_config_t router_config = bio_router_default_config();
            router_config.enable_logging = false;
            bio_router_init(&router_config);
        }

        // Let workers hammer the system
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        bio_router_shutdown();
        nimcp_bio_async_shutdown();

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    running.store(false, std::memory_order_release);
    for (auto& s : senders) {
        s.join();
    }

    // If we survived all cycles without crashing, both TOCTOU races are fixed
    EXPECT_GT(total_attempts.load(), 0)
        << "Workers should have made at least some attempts";
}

/**
 * @test Verify double shutdown does not crash
 *
 * Double shutdown could expose races in flag cleanup if not handled properly.
 */
TEST_F(BioRouterTOCTOURegressionTest, DoubleShutdownSafe) {
    ASSERT_TRUE(bio_router_is_initialized());

    bio_router_shutdown();
    EXPECT_FALSE(bio_router_is_initialized());

    // Second shutdown should be a no-op, not crash
    bio_router_shutdown();
    EXPECT_FALSE(bio_router_is_initialized());

    // Brain KG access after double shutdown should be safe
    EXPECT_EQ(bio_router_get_brain_kg(), nullptr);
}
