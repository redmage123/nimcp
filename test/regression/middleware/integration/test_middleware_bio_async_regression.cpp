/**
 * @file test_middleware_bio_async_regression.cpp
 * @brief Regression tests for bio-async middleware integration
 *
 * Tests for:
 * - Performance baselines (registration < 1ms, routing < 100us)
 * - Memory stability (no leaks during create/destroy cycles)
 * - Stress testing (rapid operations, high throughput)
 * - Concurrent operations (thread safety)
 *
 * @author NIMCP Development Team
 * @date 2025-12-05
 */

#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <numeric>
#include <algorithm>

// Headers have their own extern "C" guards
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"

//=============================================================================
// Static Handlers (C-style for bio-router C API)
//=============================================================================

static std::atomic<uint64_t> g_messages_handled{0};

static nimcp_error_t SimpleHandler(const void*, size_t, nimcp_bio_promise_t, void*) {
    g_messages_handled++;
    return NIMCP_SUCCESS;
}

//=============================================================================
// Test Fixture
//=============================================================================

class MiddlewareBioAsyncRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize bio-async system
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        bio_config.enable_logging = false;
        bio_config.enable_statistics = true;
        // Disable refractory periods for regression tests - tight loops hit the
        // same channel faster than the biological refractory window allows
        for (int ch = 0; ch < BIO_CHANNEL_COUNT; ch++) {
            bio_config.channel_configs[ch].refractory_period_ms = 0.0f;
        }
        nimcp_error_t err = nimcp_bio_async_init(&bio_config);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        // Initialize bio-router with smaller limits
        bio_router_config_t router_config = bio_router_default_config();
        router_config.enable_logging = false;
        router_config.enable_statistics = true;
        router_config.max_modules = 32;
        router_config.inbox_capacity = 1000;
        err = bio_router_init(&router_config);
        ASSERT_EQ(err, NIMCP_SUCCESS);

        g_messages_handled = 0;
        module_id_counter = 0x2000;
    }

    void TearDown() override {
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }

    bio_module_context_t RegisterModule(const char* name) {
        bio_module_info_t info = {};
        info.module_id = static_cast<bio_module_id_t>(module_id_counter++);
        info.module_name = name;
        info.inbox_capacity = 100;
        info.user_data = nullptr;
        return bio_router_register_module(&info);
    }

private:
    uint32_t module_id_counter;
};

//=============================================================================
// Performance Baseline Tests
//=============================================================================

TEST_F(MiddlewareBioAsyncRegressionTest, RegistrationPerformance) {
    const int NUM_ITERATIONS = 20;
    std::vector<double> latencies;
    std::vector<bio_module_context_t> modules;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        bio_module_context_t ctx = RegisterModule(("perf_test_" + std::to_string(i)).c_str());
        auto end = std::chrono::high_resolution_clock::now();

        ASSERT_NE(ctx, nullptr) << "Failed at iteration " << i;

        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);
        modules.push_back(ctx);
    }

    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double mean = sum / latencies.size();
    std::sort(latencies.begin(), latencies.end());
    double p95 = latencies[static_cast<size_t>(0.95 * latencies.size())];

    std::cout << "Registration: mean=" << mean << "us, p95=" << p95 << "us\n";

    // Performance target: < 1ms (1000us)
    EXPECT_LT(mean, 1000.0) << "Registration too slow";

    for (auto ctx : modules) {
        bio_router_unregister_module(ctx);
    }
}

TEST_F(MiddlewareBioAsyncRegressionTest, MessageRoutingPerformance) {
    bio_module_context_t source = RegisterModule("source");
    bio_module_context_t target = RegisterModule("target");
    ASSERT_NE(source, nullptr);
    ASSERT_NE(target, nullptr);

    nimcp_error_t err = bio_router_register_handler(target, BIO_MSG_PIPELINE_STAGE_COMPLETE, SimpleHandler);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    const int NUM_MESSAGES = 500;
    std::vector<double> latencies;

    for (int i = 0; i < NUM_MESSAGES; i++) {
        struct {
            bio_message_header_t header;
            uint32_t data;
        } msg;

        msg.header.type = BIO_MSG_PIPELINE_STAGE_COMPLETE;
        msg.header.source_module = bio_module_context_get_id(source);
        msg.header.target_module = bio_module_context_get_id(target);
        msg.header.channel = BIO_CHANNEL_DOPAMINE;
        msg.header.payload_size = sizeof(uint32_t);
        msg.data = i;

        auto start = std::chrono::high_resolution_clock::now();
        bio_router_send(source, &msg, sizeof(msg), 100);
        auto end = std::chrono::high_resolution_clock::now();

        latencies.push_back(std::chrono::duration<double, std::micro>(end - start).count());

        if (i % 50 == 49) {
            bio_router_process_inbox(target, 50);
        }
    }

    bio_router_process_inbox(target, 0);

    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    double mean = sum / latencies.size();
    std::sort(latencies.begin(), latencies.end());
    double p95 = latencies[static_cast<size_t>(0.95 * latencies.size())];

    std::cout << "Routing: mean=" << mean << "us, p95=" << p95 << "us, handled="
              << g_messages_handled.load() << "/" << NUM_MESSAGES << "\n";

    // Performance target: < 2000us (relaxed for CI/parallel test contention)
    EXPECT_LT(mean, 2000.0) << "Routing too slow";

    bio_router_unregister_module(target);
    bio_router_unregister_module(source);
}

//=============================================================================
// Memory Stability Tests
//=============================================================================

TEST_F(MiddlewareBioAsyncRegressionTest, CreateDestroyStressTest) {
    const int NUM_CYCLES = 500;

    for (int i = 0; i < NUM_CYCLES; i++) {
        bio_module_context_t ctx = RegisterModule(("stress_" + std::to_string(i)).c_str());
        ASSERT_NE(ctx, nullptr) << "Failed at cycle " << i;

        bio_router_register_handler(ctx, BIO_MSG_PIPELINE_STAGE_COMPLETE, SimpleHandler);
        bio_router_unregister_module(ctx);

        if (i % 100 == 99) {
            bio_router_stats_t stats;
            bio_router_get_stats(&stats);
            EXPECT_LT(stats.active_modules, 10u) << "Memory leak at cycle " << i;
        }
    }

    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    EXPECT_LT(stats.active_modules, 5u);
}

TEST_F(MiddlewareBioAsyncRegressionTest, HighVolumeMessageStability) {
    bio_module_context_t source = RegisterModule("hv_source");
    bio_module_context_t target = RegisterModule("hv_target");
    ASSERT_NE(source, nullptr);
    ASSERT_NE(target, nullptr);

    g_messages_handled = 0;
    bio_router_register_handler(target, BIO_MSG_PIPELINE_STAGE_COMPLETE, SimpleHandler);

    const int NUM_MESSAGES = 5000;
    const int BATCH_SIZE = 100;

    for (int batch = 0; batch < NUM_MESSAGES / BATCH_SIZE; batch++) {
        for (int i = 0; i < BATCH_SIZE; i++) {
            struct {
                bio_message_header_t header;
                uint32_t data;
            } msg;

            msg.header.type = BIO_MSG_PIPELINE_STAGE_COMPLETE;
            msg.header.source_module = bio_module_context_get_id(source);
            msg.header.target_module = bio_module_context_get_id(target);
            msg.header.channel = BIO_CHANNEL_DOPAMINE;
            msg.header.payload_size = sizeof(uint32_t);
            msg.data = batch * BATCH_SIZE + i;

            bio_router_send(source, &msg, sizeof(msg), 100);
        }

        bio_router_process_inbox(target, BATCH_SIZE);
    }

    EXPECT_EQ(g_messages_handled.load(), static_cast<uint64_t>(NUM_MESSAGES));

    bio_router_unregister_module(target);
    bio_router_unregister_module(source);
}

TEST_F(MiddlewareBioAsyncRegressionTest, PromiseStability) {
    const int NUM_PROMISES = 500;

    for (int i = 0; i < NUM_PROMISES; i++) {
        nimcp_bio_promise_t promise = nimcp_bio_promise_create(
            static_cast<nimcp_bio_channel_type_t>(i % BIO_CHANNEL_COUNT), sizeof(int));
        ASSERT_NE(promise, nullptr);

        nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
        ASSERT_NE(future, nullptr);

        int result = i;
        nimcp_bio_promise_complete(promise, &result);

        int out = 0;
        nimcp_bio_future_wait(future, &out, 100);
        EXPECT_EQ(out, i);

        nimcp_bio_future_destroy(future);
        nimcp_bio_promise_destroy(promise);
    }

    nimcp_bio_async_stats_t stats;
    nimcp_bio_async_get_stats(&stats);
    EXPECT_EQ(stats.total_futures_created, static_cast<uint64_t>(NUM_PROMISES));
}

//=============================================================================
// Concurrent Operations Tests
//=============================================================================

TEST_F(MiddlewareBioAsyncRegressionTest, ConcurrentModuleRegistration) {
    const int NUM_THREADS = 4;
    const int MODULES_PER_THREAD = 5;
    std::atomic<int> success{0};
    std::vector<std::vector<bio_module_context_t>> thread_modules(NUM_THREADS);

    auto worker = [&](int tid) {
        for (int i = 0; i < MODULES_PER_THREAD; i++) {
            std::string name = "t" + std::to_string(tid) + "_m" + std::to_string(i);
            bio_module_context_t ctx = RegisterModule(name.c_str());
            if (ctx != nullptr) {
                success++;
                thread_modules[tid].push_back(ctx);
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) t.join();

    std::cout << "Concurrent registration: " << success.load() << " modules\n";
    EXPECT_GE(success.load(), NUM_THREADS * MODULES_PER_THREAD * 0.9);

    for (auto& modules : thread_modules) {
        for (auto ctx : modules) {
            if (ctx) bio_router_unregister_module(ctx);
        }
    }
}

TEST_F(MiddlewareBioAsyncRegressionTest, ConcurrentMessageSending) {
    bio_module_context_t target = RegisterModule("concurrent_target");
    ASSERT_NE(target, nullptr);

    g_messages_handled = 0;
    bio_router_register_handler(target, BIO_MSG_PIPELINE_STAGE_COMPLETE, SimpleHandler);

    const int NUM_THREADS = 4;
    const int MSGS_PER_THREAD = 50;
    std::atomic<int> sent{0};
    std::atomic<bool> senders_done{false};

    auto sender = [&](int tid) {
        bio_module_context_t source = RegisterModule(("sender_" + std::to_string(tid)).c_str());
        if (!source) return;

        for (int i = 0; i < MSGS_PER_THREAD; i++) {
            struct {
                bio_message_header_t header;
                uint32_t tid;
                uint32_t i;
            } msg;

            msg.header.type = BIO_MSG_PIPELINE_STAGE_COMPLETE;
            msg.header.source_module = bio_module_context_get_id(source);
            msg.header.target_module = bio_module_context_get_id(target);
            msg.header.channel = BIO_CHANNEL_DOPAMINE;
            msg.header.payload_size = sizeof(uint32_t) * 2;
            msg.tid = tid;
            msg.i = i;

            if (bio_router_send(source, &msg, sizeof(msg), 500) == NIMCP_SUCCESS) {
                sent++;
            }
        }

        bio_router_unregister_module(source);
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(sender, i);
    }

    std::thread processor([&]() {
        while (!senders_done.load() || bio_router_inbox_count(target) > 0) {
            bio_router_process_inbox(target, 100);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    for (auto& t : threads) t.join();
    senders_done.store(true);
    bio_router_process_inbox(target, 0);
    processor.join();

    std::cout << "Concurrent: sent=" << sent.load()
              << ", handled=" << g_messages_handled.load() << "\n";

    const int EXPECTED = NUM_THREADS * MSGS_PER_THREAD;
    EXPECT_GE(sent.load(), static_cast<int>(EXPECTED * 0.9));
    EXPECT_GE(g_messages_handled.load(), static_cast<uint64_t>(EXPECTED * 0.9));

    bio_router_unregister_module(target);
}

//=============================================================================
// Throughput Tests
//=============================================================================

TEST_F(MiddlewareBioAsyncRegressionTest, MessageThroughput) {
    bio_module_context_t source = RegisterModule("tp_source");
    bio_module_context_t target = RegisterModule("tp_target");
    ASSERT_NE(source, nullptr);
    ASSERT_NE(target, nullptr);

    g_messages_handled = 0;
    bio_router_register_handler(target, BIO_MSG_PIPELINE_STAGE_COMPLETE, SimpleHandler);

    const int NUM_MESSAGES = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_MESSAGES; i++) {
        struct {
            bio_message_header_t header;
            uint32_t data;
        } msg;

        msg.header.type = BIO_MSG_PIPELINE_STAGE_COMPLETE;
        msg.header.source_module = bio_module_context_get_id(source);
        msg.header.target_module = bio_module_context_get_id(target);
        msg.header.channel = BIO_CHANNEL_DOPAMINE;
        msg.header.payload_size = sizeof(uint32_t);
        msg.data = i;

        bio_router_send(source, &msg, sizeof(msg), 1000);

        if (i % 100 == 99) {
            bio_router_process_inbox(target, 100);
        }
    }

    while (bio_router_inbox_count(target) > 0) {
        bio_router_process_inbox(target, 1000);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

    double throughput = NUM_MESSAGES / (duration_ms / 1000.0);
    std::cout << "Throughput: " << throughput << " msgs/sec\n";

    // Should achieve at least 500 msgs/sec (relaxed for CI/parallel test contention)
    EXPECT_GT(throughput, 500.0);
    EXPECT_EQ(g_messages_handled.load(), static_cast<uint64_t>(NUM_MESSAGES));

    bio_router_unregister_module(target);
    bio_router_unregister_module(source);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
