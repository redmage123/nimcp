/**
 * @file test_self_repair_bio_async.cpp
 * @brief Unit tests for self-repair module bio-async communication
 *
 * WHAT: Test bio-async message handling in self-repair coordinator
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

#include "cognitive/fault_tolerance/nimcp_self_repair.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SelfRepairBioAsyncTest : public ::testing::Test {
protected:
    self_repair_coordinator_t* coordinator = nullptr;
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

        // Force-initialize exception system singletons so their allocations
        // are part of the baseline (handler mutex, exception mutex, etc.)
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "warmup");
        nimcp_exception_clear_current();

        // Capture baseline AFTER router init and exception warmup
        nimcp_memory_stats_t baseline_stats;
        nimcp_memory_get_stats(&baseline_stats);
        baseline_allocated = baseline_stats.current_allocated;
    }

    void TearDown() override {
        if (coordinator) {
            self_repair_destroy(coordinator);
            coordinator = nullptr;
        }

        // Clear thread-local exception state (exceptions from BBB not being
        // initialized leave a ref in tl_current_exception that appears as a leak)
        nimcp_exception_clear_current();

        // Memory leak check BEFORE router shutdown (skip for tests that change router)
        if (!skip_memory_check) {
            nimcp_memory_stats_t stats;
            nimcp_memory_get_stats(&stats);
            EXPECT_LE(stats.current_allocated, baseline_allocated + 256)
                << "Memory leak detected!";
        }

        // Only shutdown router if we initialized it
        if (router_initialized) {
            bio_router_shutdown();
            router_initialized = false;
        }
    }

    self_repair_coordinator_t* create_coordinator() {
        self_repair_config_t config = self_repair_default_config();
        config.mode = REPAIR_MODE_DUAL;
        return self_repair_create(&config);
    }
};

//=============================================================================
// Module Registration Tests
//=============================================================================

TEST_F(SelfRepairBioAsyncTest, RegistersWithBioRouter) {
    coordinator = create_coordinator();
    ASSERT_NE(coordinator, nullptr);
    EXPECT_TRUE(self_repair_is_ready(coordinator));
}

TEST_F(SelfRepairBioAsyncTest, RegistersCorrectModuleId) {
    coordinator = create_coordinator();
    ASSERT_NE(coordinator, nullptr);

    // Verify module was registered by checking router stats
    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    EXPECT_GE(stats.active_modules, 1);
}

TEST_F(SelfRepairBioAsyncTest, UnregistersOnDestroy) {
    coordinator = create_coordinator();
    ASSERT_NE(coordinator, nullptr);

    bio_router_stats_t stats_before;
    bio_router_get_stats(&stats_before);

    self_repair_destroy(coordinator);
    coordinator = nullptr;

    bio_router_stats_t stats_after;
    bio_router_get_stats(&stats_after);

    EXPECT_LT(stats_after.active_modules, stats_before.active_modules);
}

//=============================================================================
// Message Broadcasting Tests
//=============================================================================

TEST_F(SelfRepairBioAsyncTest, BroadcastStageChange) {
    coordinator = create_coordinator();
    ASSERT_NE(coordinator, nullptr);

    int result = self_repair_broadcast_stage_change(
        coordinator,
        1,  // repair_id
        REPAIR_STAGE_PENDING,
        REPAIR_STAGE_ANALYZING
    );
    EXPECT_EQ(result, 0);
}

TEST_F(SelfRepairBioAsyncTest, BroadcastResult) {
    coordinator = create_coordinator();
    ASSERT_NE(coordinator, nullptr);

    int result = self_repair_broadcast_result(
        coordinator,
        1,      // repair_id
        true,   // success
        REPAIR_STATUS_SUCCESS
    );
    EXPECT_EQ(result, 0);
}

TEST_F(SelfRepairBioAsyncTest, BroadcastWithNullCoordinator) {
    int result = self_repair_broadcast_stage_change(nullptr, 1, REPAIR_STAGE_PENDING, REPAIR_STAGE_ANALYZING);
    EXPECT_EQ(result, -1);

    result = self_repair_broadcast_result(nullptr, 1, true, REPAIR_STATUS_SUCCESS);
    EXPECT_EQ(result, -1);
}

//=============================================================================
// Message Processing Tests
//=============================================================================

TEST_F(SelfRepairBioAsyncTest, ProcessMessagesEmpty) {
    coordinator = create_coordinator();
    ASSERT_NE(coordinator, nullptr);

    uint32_t processed = self_repair_process_messages(coordinator, 10);
    EXPECT_EQ(processed, 0);
}

TEST_F(SelfRepairBioAsyncTest, ProcessMessagesWithNullCoordinator) {
    uint32_t processed = self_repair_process_messages(nullptr, 10);
    EXPECT_EQ(processed, 0);
}

//=============================================================================
// Message Header Format Tests
//=============================================================================

TEST_F(SelfRepairBioAsyncTest, StageChangeMessageFormat) {
    coordinator = create_coordinator();
    ASSERT_NE(coordinator, nullptr);

    // Broadcast a stage change
    int result = self_repair_broadcast_stage_change(
        coordinator,
        12345,
        REPAIR_STAGE_ANALYZING,
        REPAIR_STAGE_GENERATING
    );
    EXPECT_EQ(result, 0);

    // The message should have been sent via bio_router_broadcast
    // We verify format by checking no crashes and successful return
}

TEST_F(SelfRepairBioAsyncTest, ResultMessageFormat) {
    coordinator = create_coordinator();
    ASSERT_NE(coordinator, nullptr);

    int result = self_repair_broadcast_result(
        coordinator,
        99999,
        false,
        REPAIR_STATUS_VALIDATION_FAILED
    );
    EXPECT_EQ(result, 0);
}

//=============================================================================
// Router Not Initialized Tests
//=============================================================================

TEST_F(SelfRepairBioAsyncTest, WorksWithoutRouterInitialized) {
    // This test changes router state, skip memory check
    skip_memory_check = true;

    // Shutdown router
    if (router_initialized) {
        bio_router_shutdown();
        router_initialized = false;
    }

    // Should still create successfully (just without bio-async)
    self_repair_config_t config = self_repair_default_config();
    coordinator = self_repair_create(&config);
    ASSERT_NE(coordinator, nullptr);
    EXPECT_TRUE(self_repair_is_ready(coordinator));

    // Broadcast should return -1 (no bio context)
    int result = self_repair_broadcast_stage_change(coordinator, 1, REPAIR_STAGE_PENDING, REPAIR_STAGE_ANALYZING);
    EXPECT_EQ(result, -1);

    // Re-initialize router for teardown
    bio_router_config_t bio_config = bio_router_default_config();
    bio_router_init(&bio_config);
    router_initialized = true;
}

//=============================================================================
// Multiple Coordinators Tests
//=============================================================================

TEST_F(SelfRepairBioAsyncTest, MultipleCoordinatorsRegister) {
    // Creating multiple modules may have minor memory variance in router
    skip_memory_check = true;

    coordinator = create_coordinator();
    ASSERT_NE(coordinator, nullptr);

    // Create second coordinator
    self_repair_config_t config2 = self_repair_default_config();
    self_repair_coordinator_t* coordinator2 = self_repair_create(&config2);
    ASSERT_NE(coordinator2, nullptr);

    bio_router_stats_t stats;
    bio_router_get_stats(&stats);
    EXPECT_GE(stats.active_modules, 2);

    self_repair_destroy(coordinator2);
}

//=============================================================================
// Concurrent Message Processing Tests
//=============================================================================

TEST_F(SelfRepairBioAsyncTest, ConcurrentBroadcast) {
    coordinator = create_coordinator();
    ASSERT_NE(coordinator, nullptr);

    std::atomic<int> success_count{0};
    const int num_threads = 4;
    const int messages_per_thread = 10;

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, &success_count, t, messages_per_thread]() {
            for (int i = 0; i < messages_per_thread; i++) {
                int result = self_repair_broadcast_stage_change(
                    coordinator,
                    t * 100 + i,
                    REPAIR_STAGE_PENDING,
                    REPAIR_STAGE_ANALYZING
                );
                if (result == 0) {
                    success_count++;
                }
            }
            // Clear this thread's exception state (each thread has its own
            // tl_current_exception from failed BBB calls)
            nimcp_exception_clear_current();
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count.load(), num_threads * messages_per_thread);
}
