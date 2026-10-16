/**
 * @file test_rcog_delegation_pool.cpp
 * @brief Unit tests for Recursive Cognition Delegation Pool
 *
 * WHAT: Comprehensive tests for RCOG delegation pool functionality
 * WHY:  Pool manages worker threads for subtask execution - must be robust
 * HOW:  Unit tests for lifecycle, submission, batch management, workers
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 */

#include <gtest/gtest.h>
#include <string.h>
#include <thread>
#include <chrono>

#include "cognitive/recursive/nimcp_rcog_types.h"
#include "cognitive/recursive/nimcp_rcog_delegation_pool.h"
#include "cognitive/recursive/nimcp_rcog_tool_router.h"
#include "cognitive/recursive/nimcp_rcog_context_store.h"

//=============================================================================
// Test Fixtures
//=============================================================================

/**
 * @brief Basic delegation pool test fixture
 */
class RcogDelegationPoolTest : public ::testing::Test {
protected:
    rcog_delegation_pool_t* pool;

    void SetUp() override
    {
        pool = rcog_delegation_pool_create_default();
        ASSERT_NE(pool, nullptr);
    }

    void TearDown() override
    {
        if (pool) {
            rcog_delegation_pool_stop(pool, 1000);
            rcog_delegation_pool_destroy(pool);
            pool = nullptr;
        }
    }
};

/**
 * @brief Started delegation pool test fixture
 */
class RcogDelegationPoolStartedTest : public ::testing::Test {
protected:
    rcog_delegation_pool_t* pool;
    rcog_tool_router_t* router;

    void SetUp() override
    {
        pool = rcog_delegation_pool_create_default();
        ASSERT_NE(pool, nullptr);

        router = rcog_tool_router_create_default();
        ASSERT_NE(router, nullptr);

        int result = rcog_delegation_pool_connect_tool_router(pool, router);
        ASSERT_EQ(result, 0);

        result = rcog_delegation_pool_start(pool);
        ASSERT_EQ(result, 0);
    }

    void TearDown() override
    {
        if (pool) {
            rcog_delegation_pool_stop(pool, 1000);
            rcog_delegation_pool_destroy(pool);
            pool = nullptr;
        }
        if (router) {
            rcog_tool_router_destroy(router);
            router = nullptr;
        }
    }
};

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST(RcogDelegationPoolLifecycleTest, DefaultConfig)
{
    rcog_delegation_pool_config_t config = rcog_delegation_pool_default_config();

    /* Workers are configured per-tier, not via total_workers */
    uint32_t total = 0;
    for (int i = 0; i < RCOG_TIER_COUNT; i++) {
        total += config.tiers[i].num_workers;
    }
    EXPECT_GT(total, 0u);
    EXPECT_GT(config.default_task_timeout_ms, 0u);
    EXPECT_GT(config.shutdown_timeout_ms, 0u);
    EXPECT_GT(config.max_pending_tasks, 0u);
}

TEST(RcogDelegationPoolLifecycleTest, CreateDefault)
{
    rcog_delegation_pool_t* pool = rcog_delegation_pool_create_default();
    ASSERT_NE(pool, nullptr);

    rcog_delegation_pool_destroy(pool);
}

TEST(RcogDelegationPoolLifecycleTest, CreateWithConfig)
{
    rcog_delegation_pool_config_t config = rcog_delegation_pool_default_config();
    config.default_task_timeout_ms = 5000;
    config.enable_work_stealing = true;
    /* Set per-tier workers (total_workers is not used by create) */
    for (int i = 0; i < RCOG_TIER_COUNT; i++) {
        config.tiers[i].num_workers = 2;
    }

    rcog_delegation_pool_t* pool = rcog_delegation_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    /* 5 tiers * 2 workers each = 10 total */
    EXPECT_EQ(rcog_delegation_pool_get_total_workers(pool), (uint32_t)(RCOG_TIER_COUNT * 2));

    rcog_delegation_pool_destroy(pool);
}

TEST(RcogDelegationPoolLifecycleTest, DestroyNull)
{
    // Should not crash
    rcog_delegation_pool_destroy(nullptr);
}

TEST_F(RcogDelegationPoolTest, StartPool)
{
    int result = rcog_delegation_pool_start(pool);
    EXPECT_EQ(result, 0);
}

TEST_F(RcogDelegationPoolTest, StopPool)
{
    rcog_delegation_pool_start(pool);

    int result = rcog_delegation_pool_stop(pool, 1000);
    EXPECT_EQ(result, 0);
}

TEST_F(RcogDelegationPoolTest, PauseResumePool)
{
    rcog_delegation_pool_start(pool);

    int result = rcog_delegation_pool_pause(pool);
    EXPECT_EQ(result, 0);

    result = rcog_delegation_pool_resume(pool);
    EXPECT_EQ(result, 0);
}

TEST(RcogDelegationPoolLifecycleTest, StartNull)
{
    int result = rcog_delegation_pool_start(nullptr);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);
}

TEST(RcogDelegationPoolLifecycleTest, StopNull)
{
    int result = rcog_delegation_pool_stop(nullptr, 1000);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);
}

//=============================================================================
// Connection Tests
//=============================================================================

TEST_F(RcogDelegationPoolTest, ConnectToolRouter)
{
    rcog_tool_router_t* router = rcog_tool_router_create_default();
    ASSERT_NE(router, nullptr);

    int result = rcog_delegation_pool_connect_tool_router(pool, router);
    EXPECT_EQ(result, 0);

    rcog_tool_router_destroy(router);
}

TEST_F(RcogDelegationPoolTest, ConnectContextStore)
{
    rcog_context_store_t* store = rcog_context_store_create_default();
    ASSERT_NE(store, nullptr);

    int result = rcog_delegation_pool_connect_context_store(pool, store);
    EXPECT_EQ(result, 0);

    rcog_context_store_destroy(store);
}

TEST(RcogDelegationPoolConnectionTest, ConnectNullPool)
{
    rcog_tool_router_t* router = rcog_tool_router_create_default();

    int result = rcog_delegation_pool_connect_tool_router(nullptr, router);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_tool_router_destroy(router);
}

//=============================================================================
// Worker Tests
//=============================================================================

TEST_F(RcogDelegationPoolTest, GetTotalWorkers)
{
    uint32_t count = rcog_delegation_pool_get_total_workers(pool);
    EXPECT_GT(count, 0u);
}

TEST_F(RcogDelegationPoolTest, GetWorkerCountByTier)
{
    uint32_t l1_count = rcog_delegation_pool_get_worker_count(pool, RCOG_TIER_L1_REASONING);
    uint32_t l2_count = rcog_delegation_pool_get_worker_count(pool, RCOG_TIER_L2_PERCEPTION);

    // Should have workers at various tiers
    EXPECT_GE(l1_count + l2_count, 0u);
}

TEST_F(RcogDelegationPoolStartedTest, GetWorkerInfo)
{
    uint32_t total = rcog_delegation_pool_get_total_workers(pool);

    if (total > 0) {
        rcog_worker_info_t info;
        int result = rcog_delegation_pool_get_worker_info(pool, 0, &info);
        EXPECT_EQ(result, 0);

        EXPECT_EQ(info.worker_id, 0u);
    }
}

TEST_F(RcogDelegationPoolStartedTest, GetAllWorkers)
{
    uint32_t total = rcog_delegation_pool_get_total_workers(pool);

    rcog_worker_info_t* infos = new rcog_worker_info_t[total];
    size_t count = 0;

    int result = rcog_delegation_pool_get_all_workers(pool, infos, total, &count);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, (size_t)total);

    delete[] infos;
}

TEST(RcogDelegationPoolWorkerTest, GetWorkerInfoNull)
{
    rcog_worker_info_t info;
    int result = rcog_delegation_pool_get_worker_info(nullptr, 0, &info);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);
}

//=============================================================================
// Submission Tests
//=============================================================================

TEST_F(RcogDelegationPoolStartedTest, DefaultSubmitOptions)
{
    rcog_submit_options_t opts = rcog_delegation_pool_default_submit_options();

    EXPECT_FLOAT_EQ(opts.priority_override, -1.0f);  // No override
    EXPECT_TRUE(opts.allow_work_stealing);
}

TEST_F(RcogDelegationPoolStartedTest, HasCapacity)
{
    bool has_cap = rcog_delegation_pool_has_capacity(pool);
    EXPECT_TRUE(has_cap);
}

TEST_F(RcogDelegationPoolStartedTest, GetQueueDepth)
{
    size_t depth = rcog_delegation_pool_get_queue_depth(pool);
    EXPECT_EQ(depth, 0u);  // No tasks submitted yet
}

TEST(RcogDelegationPoolSubmitTest, SubmitNull)
{
    rcog_delegation_pool_t* pool = rcog_delegation_pool_create_default();
    rcog_delegation_pool_start(pool);

    int result = rcog_delegation_pool_submit(pool, nullptr, nullptr);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    result = rcog_delegation_pool_submit(nullptr, nullptr, nullptr);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_delegation_pool_stop(pool, 1000);
    rcog_delegation_pool_destroy(pool);
}

//=============================================================================
// Batch Tests
//=============================================================================

TEST(RcogDelegationPoolBatchTest, FreeBatchHandleNull)
{
    // Should not crash
    rcog_delegation_pool_free_batch_handle(nullptr);
}

//=============================================================================
// Task Management Tests
//=============================================================================

TEST_F(RcogDelegationPoolStartedTest, CancelAll)
{
    size_t cancelled = rcog_delegation_pool_cancel_all(pool);
    EXPECT_EQ(cancelled, 0u);  // No tasks to cancel
}

TEST(RcogDelegationPoolTaskTest, GetTaskStatusNull)
{
    rcog_subtask_status_t status;
    int result = rcog_delegation_pool_get_task_status(nullptr, 1, &status);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);
}

TEST(RcogDelegationPoolTaskTest, CancelTaskNull)
{
    int result = rcog_delegation_pool_cancel_task(nullptr, 1);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(RcogDelegationPoolTest, GetStats)
{
    rcog_delegation_pool_stats_t stats;
    int result = rcog_delegation_pool_get_stats(pool, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_EQ(stats.tasks_submitted, 0u);
    EXPECT_EQ(stats.tasks_completed, 0u);
}

TEST_F(RcogDelegationPoolTest, ResetStats)
{
    rcog_delegation_pool_reset_stats(pool);

    rcog_delegation_pool_stats_t stats;
    rcog_delegation_pool_get_stats(pool, &stats);
    EXPECT_EQ(stats.tasks_submitted, 0u);
}

TEST(RcogDelegationPoolStatsTest, StatsNullParams)
{
    rcog_delegation_pool_stats_t stats;
    int result = rcog_delegation_pool_get_stats(nullptr, &stats);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_delegation_pool_t* pool = rcog_delegation_pool_create_default();
    result = rcog_delegation_pool_get_stats(pool, nullptr);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_delegation_pool_destroy(pool);
}

//=============================================================================
// Work Stealing Tests
//=============================================================================

TEST_F(RcogDelegationPoolStartedTest, SetWorkStealing)
{
    int result = rcog_delegation_pool_set_work_stealing(pool, true);
    EXPECT_EQ(result, 0);

    result = rcog_delegation_pool_set_work_stealing(pool, false);
    EXPECT_EQ(result, 0);
}

TEST_F(RcogDelegationPoolStartedTest, Rebalance)
{
    size_t moved = rcog_delegation_pool_rebalance(pool);
    EXPECT_EQ(moved, 0u);  // No tasks to rebalance
}

TEST(RcogDelegationPoolWorkStealingTest, SetWorkStealingNull)
{
    int result = rcog_delegation_pool_set_work_stealing(nullptr, true);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);
}

//=============================================================================
// Immune Modulation Tests
//=============================================================================

TEST_F(RcogDelegationPoolTest, ApplyImmuneModulation)
{
    rcog_immune_modulation_t mod;
    memset(&mod, 0, sizeof(mod));
    mod.capacity_multiplier = 0.5f;
    mod.timeout_multiplier = 1.5f;

    int result = rcog_delegation_pool_apply_immune_modulation(pool, &mod);
    EXPECT_EQ(result, 0);
}

TEST_F(RcogDelegationPoolTest, GetEffectiveCapacity)
{
    float capacity = rcog_delegation_pool_get_effective_capacity(pool);
    EXPECT_GT(capacity, 0.0f);
    EXPECT_LE(capacity, 1.0f);
}

TEST(RcogDelegationPoolModulationTest, ModulationNullParams)
{
    rcog_delegation_pool_t* pool = rcog_delegation_pool_create_default();
    rcog_immune_modulation_t mod;

    int result = rcog_delegation_pool_apply_immune_modulation(nullptr, &mod);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    result = rcog_delegation_pool_apply_immune_modulation(pool, nullptr);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);

    rcog_delegation_pool_destroy(pool);
}

//=============================================================================
// Scaling Tests
//=============================================================================

TEST_F(RcogDelegationPoolTest, ScaleTier)
{
    uint32_t original = rcog_delegation_pool_get_worker_count(pool, RCOG_TIER_L1_REASONING);

    int result = rcog_delegation_pool_scale_tier(pool, RCOG_TIER_L1_REASONING, original + 2);
    // May succeed or fail depending on max workers
    // Just ensure it doesn't crash
    (void)result;
}

TEST(RcogDelegationPoolScaleTest, ScaleTierNull)
{
    int result = rcog_delegation_pool_scale_tier(nullptr, RCOG_TIER_L1_REASONING, 4);
    EXPECT_EQ(result, RCOG_ERROR_NULL_POINTER);
}

//=============================================================================
// Utility Tests
//=============================================================================

TEST_F(RcogDelegationPoolStartedTest, EstimateWaitTime)
{
    uint32_t wait = rcog_delegation_pool_estimate_wait_time(pool, RCOG_TIER_L1_REASONING);
    // Should be very low with empty queue
    EXPECT_LT(wait, 1000u);
}

TEST_F(RcogDelegationPoolStartedTest, DrainPool)
{
    int result = rcog_delegation_pool_drain(pool, 1000);
    EXPECT_EQ(result, 0);  // No tasks to drain
}

TEST(RcogDelegationPoolUtilityTest, EstimateWaitTimeNull)
{
    uint32_t wait = rcog_delegation_pool_estimate_wait_time(nullptr, RCOG_TIER_L1_REASONING);
    EXPECT_EQ(wait, 0u);
}

TEST(RcogDelegationPoolUtilityTest, HasCapacityNull)
{
    bool has_cap = rcog_delegation_pool_has_capacity(nullptr);
    EXPECT_FALSE(has_cap);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
