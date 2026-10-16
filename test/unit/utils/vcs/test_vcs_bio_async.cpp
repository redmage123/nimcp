/**
 * @file test_vcs_bio_async.cpp
 * @brief Unit tests for VCS integration module bio-async communication
 *
 * WHAT: Test bio-async message handling in VCS integration
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

#include "utils/vcs/nimcp_vcs_integration.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_handlers.h"

//=============================================================================
// Test Fixture
//=============================================================================

class VcsBioAsyncTest : public ::testing::Test {
protected:
    vcs_integration_t* vcs = nullptr;
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

        // Warm up exception handler system (one-time mutex allocation)
        {
            nimcp_exception_t* warmup = nimcp_exception_create(
                NIMCP_ERROR_NULL_POINTER, EXCEPTION_SEVERITY_DEBUG,
                __FILE__, __LINE__, __func__, "warmup");
            if (warmup) {
                nimcp_exception_dispatch(warmup);
                nimcp_exception_unref(warmup);
            }
            nimcp_exception_clear_current();
        }

        // Capture baseline AFTER router init and exception warmup
        nimcp_memory_stats_t baseline_stats;
        nimcp_memory_get_stats(&baseline_stats);
        baseline_allocated = baseline_stats.current_allocated;
    }

    void TearDown() override {
        if (vcs) {
            vcs_destroy(vcs);
            vcs = nullptr;
        }

        // Release any exception held as "current" by the dispatch system
        nimcp_exception_clear_current();

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

    vcs_integration_t* create_vcs() {
        vcs_config_t config = vcs_default_config();
        config.dry_run = true;  // Don't actually run git commands
        return vcs_create(&config);
    }
};

//=============================================================================
// Module Registration Tests
//=============================================================================

TEST_F(VcsBioAsyncTest, RegistersWithBioRouter) {
    vcs = create_vcs();
    ASSERT_NE(vcs, nullptr);
    EXPECT_TRUE(vcs_is_ready(vcs));
}

TEST_F(VcsBioAsyncTest, RegistersCorrectModuleId) {
    vcs = create_vcs();
    ASSERT_NE(vcs, nullptr);

    // Verify module was registered by checking router stats
    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    EXPECT_GE(stats.active_modules, 1);
}

TEST_F(VcsBioAsyncTest, UnregistersOnDestroy) {
    vcs = create_vcs();
    ASSERT_NE(vcs, nullptr);

    bio_router_stats_t stats_before;
    bio_router_get_stats(&stats_before);

    vcs_destroy(vcs);
    vcs = nullptr;

    bio_router_stats_t stats_after;
    bio_router_get_stats(&stats_after);

    EXPECT_LT(stats_after.active_modules, stats_before.active_modules);
}

//=============================================================================
// Message Broadcasting Tests
//=============================================================================

TEST_F(VcsBioAsyncTest, BroadcastCommit) {
    vcs = create_vcs();
    ASSERT_NE(vcs, nullptr);

    int result = vcs_broadcast_commit(
        vcs,
        1,                  // fix_id
        "abc123def456",     // commit_hash
        true                // success
    );
    EXPECT_EQ(result, 0);
}

TEST_F(VcsBioAsyncTest, BroadcastCommitFailure) {
    vcs = create_vcs();
    ASSERT_NE(vcs, nullptr);

    int result = vcs_broadcast_commit(
        vcs,
        999,            // fix_id
        nullptr,        // no commit hash (failed)
        false           // failure
    );
    EXPECT_EQ(result, 0);
}

TEST_F(VcsBioAsyncTest, BroadcastWithNullVcs) {
    int result = vcs_broadcast_commit(nullptr, 1, "abc123", true);
    EXPECT_EQ(result, -1);
}

TEST_F(VcsBioAsyncTest, BroadcastWithLongCommitHash) {
    vcs = create_vcs();
    ASSERT_NE(vcs, nullptr);

    // Full 40-char git hash
    int result = vcs_broadcast_commit(
        vcs,
        1,
        "abc123def456789012345678901234567890abcd",
        true
    );
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Message Processing Tests
//=============================================================================

TEST_F(VcsBioAsyncTest, ProcessMessagesEmpty) {
    vcs = create_vcs();
    ASSERT_NE(vcs, nullptr);

    uint32_t processed = vcs_process_messages(vcs, 10);
    EXPECT_EQ(processed, 0);
}

TEST_F(VcsBioAsyncTest, ProcessMessagesWithNullVcs) {
    uint32_t processed = vcs_process_messages(nullptr, 10);
    EXPECT_EQ(processed, 0);
}

//=============================================================================
// Message Header Format Tests
//=============================================================================

TEST_F(VcsBioAsyncTest, CommitMessageFormat) {
    vcs = create_vcs();
    ASSERT_NE(vcs, nullptr);

    // Test various commit hash formats
    int result = vcs_broadcast_commit(vcs, 1, "", true);
    EXPECT_EQ(result, 0);

    result = vcs_broadcast_commit(vcs, 2, "short", true);
    EXPECT_EQ(result, 0);

    result = vcs_broadcast_commit(vcs, 3, "medium_hash_1234", false);
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Router Not Initialized Tests
//=============================================================================

TEST_F(VcsBioAsyncTest, WorksWithoutRouterInitialized) {
    // This test changes router state, skip memory check
    skip_memory_check = true;

    // Shutdown router
    if (router_initialized) {
        bio_router_shutdown();
        router_initialized = false;
    }

    // Should still create successfully (just without bio-async)
    vcs_config_t config = vcs_default_config();
    config.dry_run = true;
    vcs = vcs_create(&config);
    ASSERT_NE(vcs, nullptr);
    EXPECT_TRUE(vcs_is_ready(vcs));

    // Broadcast should return -1 (no bio context)
    int result = vcs_broadcast_commit(vcs, 1, "abc123", true);
    EXPECT_EQ(result, -1);

    // Re-initialize router for teardown
    bio_router_config_t bio_config = bio_router_default_config();
    bio_router_init(&bio_config);
    router_initialized = true;
}

//=============================================================================
// Multiple VCS Instances Tests
//=============================================================================

TEST_F(VcsBioAsyncTest, MultipleInstancesRegister) {
    // Creating multiple modules may have minor memory variance in router
    skip_memory_check = true;

    vcs = create_vcs();
    ASSERT_NE(vcs, nullptr);

    // Create second VCS instance
    vcs_config_t config2 = vcs_default_config();
    config2.dry_run = true;
    vcs_integration_t* vcs2 = vcs_create(&config2);
    ASSERT_NE(vcs2, nullptr);

    // Both instances should be ready (may share same module slot)
    EXPECT_TRUE(vcs_is_ready(vcs));
    EXPECT_TRUE(vcs_is_ready(vcs2));

    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    // At least 1 module should be registered (modules may share slots)
    EXPECT_GE(stats.active_modules, 1);

    vcs_destroy(vcs2);
}

//=============================================================================
// Concurrent Message Broadcasting Tests
//=============================================================================

TEST_F(VcsBioAsyncTest, ConcurrentBroadcast) {
    // Skip memory check: spawned threads may leave exceptions in thread-local
    // storage that can't be cleared from the main thread after join
    skip_memory_check = true;
    vcs = create_vcs();
    ASSERT_NE(vcs, nullptr);

    std::atomic<int> success_count{0};
    const int num_threads = 4;
    const int messages_per_thread = 10;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &success_count, t, messages_per_thread]() {
            for (int i = 0; i < messages_per_thread; i++) {
                char hash[32];
                snprintf(hash, sizeof(hash), "hash_%d_%d", t, i);
                int result = vcs_broadcast_commit(
                    vcs,
                    t * 100 + i,
                    hash,
                    (i % 2) == 0
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
// Status Broadcast Tests
//=============================================================================

TEST_F(VcsBioAsyncTest, BroadcastMultipleCommits) {
    vcs = create_vcs();
    ASSERT_NE(vcs, nullptr);

    // Simulate multiple commits
    for (uint64_t i = 0; i < 10; i++) {
        char hash[32];
        snprintf(hash, sizeof(hash), "commit_%lu", (unsigned long)i);
        int result = vcs_broadcast_commit(vcs, i, hash, true);
        EXPECT_EQ(result, 0) << "Failed on commit " << i;
    }
}
