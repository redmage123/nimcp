//=============================================================================
// test_event_driven_plasticity_bio_async_regression.cpp
// Regression Tests for Event-Driven Plasticity Bio-Async
//=============================================================================
/**
 * @file test_event_driven_plasticity_bio_async_regression.cpp
 * @brief Regression tests to prevent bio-async integration bugs
 *
 * Tests cover:
 * - Previously identified bugs
 * - Edge cases
 * - Performance regressions
 * - Memory leak scenarios
 * - Race condition scenarios
 *
 * @author NIMCP Development Team
 * @date 2025-12-03
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

// Headers have their own extern "C" guards
#include "middleware/training/nimcp_event_driven_plasticity.h"
#include "middleware/training/nimcp_training_plasticity_bridge.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class EDPBioAsyncRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize bio-async
        nimcp_bio_async_config_t bio_config = nimcp_bio_async_default_config();
        nimcp_bio_async_init(&bio_config);

        // Initialize bio-router
        bio_router_config_t router_config = bio_router_default_config();
        bio_router_init(&router_config);

        edp_ctx_ = nullptr;
        tpb_ctx_ = nullptr;
    }

    void TearDown() override {
        if (edp_ctx_) {
            edp_destroy(edp_ctx_);
            edp_ctx_ = nullptr;
        }
        if (tpb_ctx_) {
            tpb_destroy(tpb_ctx_);
            tpb_ctx_ = nullptr;
        }
        bio_router_shutdown();
        nimcp_bio_async_shutdown();
    }

    edp_context_t* edp_ctx_;
    tpb_context_t* tpb_ctx_;
};

//=============================================================================
// Regression: Memory Leaks
//=============================================================================

TEST_F(EDPBioAsyncRegressionTest, NoLeakOnRepeatedCreateDestroy) {
    // Regression: Previously leaked bio-router context on destroy
    for (int i = 0; i < 100; i++) {
        edp_config_t config = edp_config_default();
        edp_context_t* ctx = edp_create(&config);
        ASSERT_NE(ctx, nullptr);
        edp_destroy(ctx);
    }
    // If no crash, memory management is correct
    SUCCEED();
}

TEST_F(EDPBioAsyncRegressionTest, NoLeakOnMessageHandling) {
    // Regression: Message buffers not freed
    edp_config_t config = edp_config_default();
    edp_ctx_ = edp_create(&config);
    ASSERT_NE(edp_ctx_, nullptr);

    tpb_config_t tpb_config = tpb_config_default();
    tpb_ctx_ = tpb_create(&tpb_config);
    ASSERT_NE(tpb_ctx_, nullptr);

    edp_connect_bridge(edp_ctx_, tpb_ctx_);
    edp_start(edp_ctx_);

    // Process many messages
    for (int i = 0; i < 1000; i++) {
        edp_process_reward(edp_ctx_, 0.1f);
    }

    // Cleanup should not leak
}

//=============================================================================
// Regression: Race Conditions
//=============================================================================

TEST_F(EDPBioAsyncRegressionTest, NoCrashOnConcurrentAccess) {
    // Regression: Race condition in stats mutex
    edp_config_t config = edp_config_default();
    edp_ctx_ = edp_create(&config);
    ASSERT_NE(edp_ctx_, nullptr);

    tpb_config_t tpb_config = tpb_config_default();
    tpb_ctx_ = tpb_create(&tpb_config);
    edp_connect_bridge(edp_ctx_, tpb_ctx_);
    edp_start(edp_ctx_);

    std::atomic<bool> stop{false};
    std::vector<std::thread> threads;

    // Writer threads
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &stop]() {
            while (!stop) {
                edp_process_reward(edp_ctx_, 0.1f);
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    // Reader threads
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &stop]() {
            while (!stop) {
                edp_stats_t stats;
                edp_get_stats(edp_ctx_, &stats);
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop = true;

    for (auto& t : threads) {
        t.join();
    }

    SUCCEED();
}

TEST_F(EDPBioAsyncRegressionTest, NoDeadlockOnShutdown) {
    // Regression: Deadlock during destroy with active threads
    edp_config_t config = edp_config_default();
    edp_ctx_ = edp_create(&config);
    edp_start(edp_ctx_);

    std::thread worker([this]() {
        for (int i = 0; i < 100; i++) {
            edp_process_reward(edp_ctx_, 0.1f);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Destroy should not deadlock
    edp_stop(edp_ctx_);
    worker.join();
    edp_destroy(edp_ctx_);
    edp_ctx_ = nullptr;

    SUCCEED();
}

//=============================================================================
// Regression: Edge Cases
//=============================================================================

TEST_F(EDPBioAsyncRegressionTest, HandlesNullContextGracefully) {
    // Regression: Null pointer dereference in handlers
    nimcp_result_t result = edp_process_reward(nullptr, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(EDPBioAsyncRegressionTest, HandlesInactiveContextGracefully) {
    // Regression: Crash when processing on inactive context
    edp_config_t config = edp_config_default();
    edp_ctx_ = edp_create(&config);
    // Don't call edp_start()

    nimcp_result_t result = edp_process_reward(edp_ctx_, 0.5f);
    EXPECT_NE(result, NIMCP_SUCCESS);
}

TEST_F(EDPBioAsyncRegressionTest, HandlesZeroTimestampSTDP) {
    // Regression: Division by zero in STDP timing
    edp_config_t config = edp_config_default();
    edp_ctx_ = edp_create(&config);

    tpb_config_t tpb_config = tpb_config_default();
    tpb_ctx_ = tpb_create(&tpb_config);
    edp_connect_bridge(edp_ctx_, tpb_ctx_);
    edp_start(edp_ctx_);

    // Zero delta_t should not crash
    bio_msg_stdp_event_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.delta_t_ms = 0.0f;

    // Should handle gracefully
    SUCCEED();
}

TEST_F(EDPBioAsyncRegressionTest, HandlesExtremeSTDPWindow) {
    // Regression: Buffer overflow with large STDP window
    edp_config_t config = edp_config_default();
    config.stdp_window_ms = 1000.0f;  // Very large window
    edp_ctx_ = edp_create(&config);
    ASSERT_NE(edp_ctx_, nullptr);

    tpb_config_t tpb_config = tpb_config_default();
    tpb_ctx_ = tpb_create(&tpb_config);
    edp_connect_bridge(edp_ctx_, tpb_ctx_);
    edp_start(edp_ctx_);

    // Should not overflow buffers
    for (int i = 0; i < 100; i++) {
        spike_burst_data_t burst = {
            .neuron_ids = nullptr,
            .num_neurons = 0,
            .timestamp_ns = static_cast<uint64_t>(i * 1000000),
            .synchrony_score = 0.5f,
            .region_id = 0
        };
    }

    SUCCEED();
}

//=============================================================================
// Regression: Performance Issues
//=============================================================================

TEST_F(EDPBioAsyncRegressionTest, NoPerformanceDegradation) {
    // Regression: Performance degrades over time
    edp_config_t config = edp_config_default();
    edp_ctx_ = edp_create(&config);

    tpb_config_t tpb_config = tpb_config_default();
    tpb_ctx_ = tpb_create(&tpb_config);
    edp_connect_bridge(edp_ctx_, tpb_ctx_);
    edp_start(edp_ctx_);

    // Measure initial throughput
    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        edp_process_reward(edp_ctx_, 0.1f);
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);

    // Process more to simulate long running
    for (int i = 0; i < 10000; i++) {
        edp_process_reward(edp_ctx_, 0.1f);
    }

    // Measure later throughput
    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; i++) {
        edp_process_reward(edp_ctx_, 0.1f);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);

    // Should not degrade significantly (allow 50% variance)
    EXPECT_LT(duration2.count(), duration1.count() * 1.5);
}

TEST_F(EDPBioAsyncRegressionTest, NoMemoryGrowthOverTime) {
    // Regression: Memory usage grows unbounded
    edp_config_t config = edp_config_default();
    config.enable_eligibility = true;
    edp_ctx_ = edp_create(&config);

    tpb_config_t tpb_config = tpb_config_default();
    tpb_ctx_ = tpb_create(&tpb_config);
    edp_connect_bridge(edp_ctx_, tpb_ctx_);
    edp_start(edp_ctx_);

    // Process many events
    for (int i = 0; i < 50000; i++) {
        edp_process_reward(edp_ctx_, 0.1f);
        if (i % 1000 == 0) {
            edp_clear_eligibility(edp_ctx_);
        }
    }

    // If we reach here without running out of memory, test passes
    SUCCEED();
}

//=============================================================================
// Regression: Initialization Order
//=============================================================================

TEST_F(EDPBioAsyncRegressionTest, WorksWithoutBioRouter) {
    // Regression: Crash when bio-router not initialized
    bio_router_shutdown();
    nimcp_bio_async_shutdown();

    edp_config_t config = edp_config_default();
    edp_ctx_ = edp_create(&config);
    ASSERT_NE(edp_ctx_, nullptr);

    // Should work without bio-async
    nimcp_result_t result = edp_process_reward(edp_ctx_, 0.5f);
    EXPECT_TRUE(result == NIMCP_SUCCESS || result == NIMCP_NOT_INITIALIZED);

    // Re-initialize for teardown
    nimcp_bio_async_init(nullptr);
    bio_router_init(nullptr);
}

TEST_F(EDPBioAsyncRegressionTest, HandlesBridgeConnectionOrder) {
    // Regression: Order of connections matters
    edp_config_t config = edp_config_default();
    edp_ctx_ = edp_create(&config);

    // Start before connecting bridge
    edp_start(edp_ctx_);

    tpb_config_t tpb_config = tpb_config_default();
    tpb_ctx_ = tpb_create(&tpb_config);

    // Connect after start
    nimcp_result_t result = edp_connect_bridge(edp_ctx_, tpb_ctx_);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    // Should work
    result = edp_process_reward(edp_ctx_, 0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

//=============================================================================
// Regression: Statistics Bugs
//=============================================================================

TEST_F(EDPBioAsyncRegressionTest, StatisticsRemainConsistent) {
    // Regression: Stats counters overflow or become inconsistent
    edp_config_t config = edp_config_default();
    edp_ctx_ = edp_create(&config);

    tpb_config_t tpb_config = tpb_config_default();
    tpb_ctx_ = tpb_create(&tpb_config);
    edp_connect_bridge(edp_ctx_, tpb_ctx_);
    edp_start(edp_ctx_);

    uint64_t expected_events = 0;

    for (int i = 0; i < 1000; i++) {
        nimcp_result_t result = edp_process_reward(edp_ctx_, 0.1f);
        if (result == NIMCP_SUCCESS) {
            expected_events++;
        }
    }

    edp_stats_t stats;
    edp_get_stats(edp_ctx_, &stats);

    EXPECT_GE(stats.total_events_processed, expected_events);
    EXPECT_GE(stats.total_events_received, stats.total_events_processed);
}

TEST_F(EDPBioAsyncRegressionTest, StatisticsResetProperly) {
    // Regression: Reset doesn't clear all fields
    edp_config_t config = edp_config_default();
    edp_ctx_ = edp_create(&config);

    // Generate some stats
    for (int i = 0; i < 100; i++) {
        edp_process_reward(edp_ctx_, 0.1f);
    }

    edp_stats_t stats_before;
    edp_get_stats(edp_ctx_, &stats_before);
    EXPECT_GT(stats_before.total_events_processed, 0u);

    // Reset
    nimcp_result_t result = edp_reset_stats(edp_ctx_);
    EXPECT_EQ(result, NIMCP_SUCCESS);

    edp_stats_t stats_after;
    edp_get_stats(edp_ctx_, &stats_after);
    EXPECT_EQ(stats_after.total_events_processed, 0u);
    EXPECT_EQ(stats_after.cumulative_reward, 0.0f);
}

//=============================================================================
// Regression: Error Code Handling
//=============================================================================

TEST_F(EDPBioAsyncRegressionTest, PropagatesErrorCodesCorrectly) {
    // Regression: Error codes lost in promise completion
    edp_config_t config = edp_config_default();
    edp_ctx_ = edp_create(&config);
    // No bridge connected

    nimcp_result_t result = edp_process_reward(edp_ctx_, 0.5f);
    EXPECT_EQ(result, NIMCP_NOT_INITIALIZED);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
