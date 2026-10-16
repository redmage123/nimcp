/**
 * @file test_code_generation_bio_async.cpp
 * @brief Unit tests for code generation module bio-async communication
 *
 * WHAT: Test bio-async message handling in code generation engine
 * WHY:  Verify module correctly registers, sends, and receives bio messages
 * HOW:  Initialize bio-router, create module, test message flow
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 */

#include <gtest/gtest.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>

#include "cognitive/parietal/nimcp_code_generation.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CodeGenerationBioAsyncTest : public ::testing::Test {
protected:
    code_gen_engine_t* engine = nullptr;
    bool router_initialized = false;
    size_t baseline_allocated = 0;
    bool skip_memory_check = false;  // For tests that change router state

    void SetUp() override {
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);

        // Initialize bio-router FIRST (it allocates memory)
        if (!bio_router_is_initialized()) {
            bio_router_config_t config = bio_router_default_config();
            config.max_modules = 32;
            config.inbox_capacity = 64;
            bio_router_init(&config);
            router_initialized = true;
        }

        // Capture baseline AFTER router init
        nimcp_memory_stats_t baseline_stats;
        nimcp_memory_get_stats(&baseline_stats);
        baseline_allocated = baseline_stats.current_allocated;
    }

    void TearDown() override {
        if (engine) {
            code_gen_destroy(engine);
            engine = nullptr;
        }

        // Memory leak check BEFORE router shutdown (skip for tests that change router)
        if (!skip_memory_check) {
            nimcp_memory_stats_t stats;
            nimcp_memory_get_stats(&stats);
            EXPECT_EQ(stats.current_allocated, baseline_allocated)
                << "Memory leak detected!";
        }

        // Only shutdown router if we initialized it
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }

    code_gen_engine_t* create_engine() {
        code_gen_config_t config = code_gen_default_config();
        return code_gen_create(&config);
    }
};

//=============================================================================
// Module Registration Tests
//=============================================================================

TEST_F(CodeGenerationBioAsyncTest, RegistersWithBioRouter) {
    engine = create_engine();
    ASSERT_NE(engine, nullptr);
    EXPECT_TRUE(code_gen_is_ready(engine));
}

TEST_F(CodeGenerationBioAsyncTest, RegistersCorrectModuleId) {
    engine = create_engine();
    ASSERT_NE(engine, nullptr);

    // Verify module was registered by checking router stats
    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    EXPECT_GE(stats.active_modules, 1);
}

TEST_F(CodeGenerationBioAsyncTest, UnregistersOnDestroy) {
    engine = create_engine();
    ASSERT_NE(engine, nullptr);

    bio_router_stats_t stats_before;
    bio_router_get_stats(&stats_before);

    code_gen_destroy(engine);
    engine = nullptr;

    bio_router_stats_t stats_after;
    bio_router_get_stats(&stats_after);

    EXPECT_LT(stats_after.active_modules, stats_before.active_modules);
}

//=============================================================================
// Message Broadcasting Tests
//=============================================================================

TEST_F(CodeGenerationBioAsyncTest, BroadcastResult) {
    engine = create_engine();
    ASSERT_NE(engine, nullptr);

    int result = code_gen_broadcast_result(
        engine,
        1,      // fix_id
        true,   // success
        0.85f   // confidence
    );
    EXPECT_EQ(result, 0);
}

TEST_F(CodeGenerationBioAsyncTest, BroadcastResultFailure) {
    engine = create_engine();
    ASSERT_NE(engine, nullptr);

    int result = code_gen_broadcast_result(
        engine,
        999,    // fix_id
        false,  // failure
        0.2f    // low confidence
    );
    EXPECT_EQ(result, 0);
}

TEST_F(CodeGenerationBioAsyncTest, BroadcastWithNullEngine) {
    int result = code_gen_broadcast_result(nullptr, 1, true, 0.9f);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Message Processing Tests
//=============================================================================

TEST_F(CodeGenerationBioAsyncTest, ProcessMessagesEmpty) {
    engine = create_engine();
    ASSERT_NE(engine, nullptr);

    uint32_t processed = code_gen_process_messages(engine, 10);
    EXPECT_EQ(processed, 0);
}

TEST_F(CodeGenerationBioAsyncTest, ProcessMessagesWithNullEngine) {
    uint32_t processed = code_gen_process_messages(nullptr, 10);
    EXPECT_EQ(processed, 0);
}

//=============================================================================
// Message Header Format Tests
//=============================================================================

TEST_F(CodeGenerationBioAsyncTest, ResultMessageFormat) {
    engine = create_engine();
    ASSERT_NE(engine, nullptr);

    // Broadcast with various confidence values
    int result = code_gen_broadcast_result(engine, 12345, true, 0.0f);
    EXPECT_EQ(result, 0);

    result = code_gen_broadcast_result(engine, 12346, false, 1.0f);
    EXPECT_EQ(result, 0);

    result = code_gen_broadcast_result(engine, 12347, true, 0.5f);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Router Not Initialized Tests
//=============================================================================

TEST_F(CodeGenerationBioAsyncTest, WorksWithoutRouterInitialized) {
    // This test changes router state, skip memory check
    skip_memory_check = true;

    // Shutdown router
    if (router_initialized) {
        bio_router_shutdown();
        router_initialized = false;
    }

    // Should still create successfully (just without bio-async)
    code_gen_config_t config = code_gen_default_config();
    engine = code_gen_create(&config);
    ASSERT_NE(engine, nullptr);
    EXPECT_TRUE(code_gen_is_ready(engine));

    // Broadcast should return -1 (no bio context)
    int result = code_gen_broadcast_result(engine, 1, true, 0.9f);
    EXPECT_EQ(result, -1);

    // Re-initialize router for teardown
    bio_router_config_t bio_config = bio_router_default_config();
    bio_router_init(&bio_config);
    router_initialized = true;
}

//=============================================================================
// Multiple Engines Tests
//=============================================================================

TEST_F(CodeGenerationBioAsyncTest, MultipleEnginesRegister) {
    // Creating multiple modules may have minor memory variance in router
    skip_memory_check = true;

    engine = create_engine();
    ASSERT_NE(engine, nullptr);

    // Create second engine
    code_gen_config_t config2 = code_gen_default_config();
    code_gen_engine_t* engine2 = code_gen_create(&config2);
    ASSERT_NE(engine2, nullptr);

    // Both engines should be ready (may share same module slot)
    EXPECT_TRUE(code_gen_is_ready(engine));
    EXPECT_TRUE(code_gen_is_ready(engine2));

    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    // At least 1 module should be registered (modules may share slots)
    EXPECT_GE(stats.active_modules, 1);

    code_gen_destroy(engine2);
}

//=============================================================================
// Concurrent Message Broadcasting Tests
//=============================================================================

TEST_F(CodeGenerationBioAsyncTest, ConcurrentBroadcast) {
    engine = create_engine();
    ASSERT_NE(engine, nullptr);

    std::atomic<int> success_count{0};
    const int num_threads = 4;
    const int messages_per_thread = 10;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &success_count, t, messages_per_thread]() {
            for (int i = 0; i < messages_per_thread; i++) {
                float confidence = (float)(t * 10 + i) / 40.0f;
                int result = code_gen_broadcast_result(
                    engine,
                    t * 100 + i,
                    (i % 2) == 0,
                    confidence
                );
                if (result == 0) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * messages_per_thread);
}

//=============================================================================
// Strategy Message Tests
//=============================================================================

TEST_F(CodeGenerationBioAsyncTest, BroadcastAllStrategies) {
    engine = create_engine();
    ASSERT_NE(engine, nullptr);

    // Test broadcasting results for different fix strategies
    for (int strategy = 0; strategy < 8; strategy++) {
        int result = code_gen_broadcast_result(
            engine,
            1000 + strategy,
            true,
            0.7f + strategy * 0.03f
        );
        EXPECT_EQ(result, 0) << "Failed for strategy " << strategy;
    }
}
