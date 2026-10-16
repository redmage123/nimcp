/**
 * @file test_thalamic_middleware_health_e2e.cpp
 * @brief End-to-end tests for Phase 5.11: Thalamic/Middleware Health Integration
 *
 * Tests full workflows for thalamic and middleware health monitoring.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

extern "C" {
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_thalamic_bridge.h"
#include "middleware/training/nimcp_brain_training_integration.h"
#include "core/brain/nimcp_brain.h"
}

/**
 * @brief E2E test fixture for Thalamic/Middleware Health
 */
class ThalamicMiddlewareHealthE2ETest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;

    void SetUp() override {
        health_agent_config_t config;
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 25;
        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);
    }

    void TearDown() override {
        if (agent) {
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }

    // Helper to create mock training context pointers
    nimcp_brain_training_ctx_t* mock_ctx(uintptr_t id) {
        return reinterpret_cast<nimcp_brain_training_ctx_t*>(0x10000 + id);
    }
};

/* ============================================================================
 * Full Thalamic Workflow Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthE2ETest, ThalamicFullWorkflow) {
    // Step 1: Configure monitoring
    health_agent_thalamic_config_t config;
    nimcp_health_agent_thalamic_config_default(&config);
    config.enable_gating_monitoring = true;
    config.enable_prediction_monitoring = true;
    config.enable_trn_monitoring = true;
    config.enable_timing_monitoring = true;
    config.enable_auto_recovery = true;

    // Step 2: Connect multiple bridges
    std::vector<omni_wm_thalamic_bridge_t> bridges(4);
    for (size_t i = 0; i < bridges.size(); i++) {
        int result = nimcp_health_agent_connect_thalamic(agent, &bridges[i],
            (i == 0) ? &config : nullptr);
        EXPECT_EQ(result, 0);
    }

    // Step 3: Run health monitoring cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        // Get metrics
        thalamic_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_thalamic_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_bridges, bridges.size());

        // Check health score
        float score = nimcp_health_agent_get_thalamic_health_score(agent);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);

        // Check if needs attention
        bool needs_attention = nimcp_health_agent_thalamic_needs_attention(agent);
        (void)needs_attention;  // May or may not need attention

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    // Step 4: Update configuration at runtime
    config.max_update_time_ms = 50.0;
    EXPECT_EQ(nimcp_health_agent_update_thalamic_config(agent, &config), 0);

    // Step 5: Continue monitoring with new config
    for (int cycle = 0; cycle < 5; cycle++) {
        thalamic_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_thalamic_metrics(agent, &metrics), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    // Step 6: Disconnect bridges
    for (size_t i = 0; i < bridges.size(); i++) {
        EXPECT_EQ(nimcp_health_agent_disconnect_thalamic(agent, &bridges[i]), 0);
    }

    // Step 7: Verify clean state
    thalamic_health_metrics_t final_metrics;
    EXPECT_EQ(nimcp_health_agent_get_thalamic_metrics(agent, &final_metrics), 0);
    EXPECT_EQ(final_metrics.num_bridges, 0u);
    EXPECT_FLOAT_EQ(final_metrics.overall_thalamic_health, 100.0f);
}

/* ============================================================================
 * Full Middleware Workflow Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthE2ETest, MiddlewareFullWorkflow) {
    // Step 1: Configure monitoring
    health_agent_middleware_config_t config;
    nimcp_health_agent_middleware_config_default(&config);
    config.enable_loss_monitoring = true;
    config.enable_gradient_monitoring = true;
    config.enable_lr_monitoring = true;
    config.enable_timing_monitoring = true;
    config.enable_auto_recovery = true;

    // Step 2: Connect training contexts using mock pointers
    nimcp_brain_training_ctx_t* ctx1 = mock_ctx(1);
    nimcp_brain_training_ctx_t* ctx2 = mock_ctx(2);

    EXPECT_EQ(nimcp_health_agent_connect_middleware(agent, ctx1, &config), 0);
    EXPECT_EQ(nimcp_health_agent_connect_middleware(agent, ctx2, nullptr), 0);

    // Step 3: Run health monitoring cycles
    for (int cycle = 0; cycle < 10; cycle++) {
        // Get metrics
        middleware_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_middleware_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_contexts, 2u);

        // Check health score
        float score = nimcp_health_agent_get_middleware_health_score(agent);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);

        // Check if needs attention
        bool needs_attention = nimcp_health_agent_middleware_needs_attention(agent);
        (void)needs_attention;  // May or may not need attention

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    // Step 4: Update configuration at runtime
    config.max_batch_time_ms = 2000.0;
    EXPECT_EQ(nimcp_health_agent_update_middleware_config(agent, &config), 0);

    // Step 5: Continue monitoring with new config
    for (int cycle = 0; cycle < 5; cycle++) {
        middleware_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_middleware_metrics(agent, &metrics), 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    // Step 6: Disconnect contexts
    EXPECT_EQ(nimcp_health_agent_disconnect_middleware(agent, ctx1), 0);
    EXPECT_EQ(nimcp_health_agent_disconnect_middleware(agent, ctx2), 0);

    // Step 7: Verify clean state
    middleware_health_metrics_t final_metrics;
    EXPECT_EQ(nimcp_health_agent_get_middleware_metrics(agent, &final_metrics), 0);
    EXPECT_EQ(final_metrics.num_contexts, 0u);
    EXPECT_FLOAT_EQ(final_metrics.overall_middleware_health, 100.0f);
}

/* ============================================================================
 * Combined Thalamic + Middleware Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthE2ETest, CombinedMonitoring) {
    // Connect thalamic bridges
    std::vector<omni_wm_thalamic_bridge_t> bridges(3);
    for (size_t i = 0; i < bridges.size(); i++) {
        EXPECT_EQ(nimcp_health_agent_connect_thalamic(agent, &bridges[i], nullptr), 0);
    }

    // Connect middleware contexts using mock pointers
    nimcp_brain_training_ctx_t* ctx1 = mock_ctx(1);
    nimcp_brain_training_ctx_t* ctx2 = mock_ctx(2);
    EXPECT_EQ(nimcp_health_agent_connect_middleware(agent, ctx1, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_middleware(agent, ctx2, nullptr), 0);

    // Run combined monitoring
    for (int cycle = 0; cycle < 20; cycle++) {
        // Check thalamic health
        thalamic_health_metrics_t thal_metrics;
        EXPECT_EQ(nimcp_health_agent_get_thalamic_metrics(agent, &thal_metrics), 0);
        EXPECT_EQ(thal_metrics.num_bridges, bridges.size());

        // Check middleware health
        middleware_health_metrics_t mw_metrics;
        EXPECT_EQ(nimcp_health_agent_get_middleware_metrics(agent, &mw_metrics), 0);
        EXPECT_EQ(mw_metrics.num_contexts, 2u);

        // Get health scores
        float thal_score = nimcp_health_agent_get_thalamic_health_score(agent);
        float mw_score = nimcp_health_agent_get_middleware_health_score(agent);

        EXPECT_GE(thal_score, 0.0f);
        EXPECT_LE(thal_score, 100.0f);
        EXPECT_GE(mw_score, 0.0f);
        EXPECT_LE(mw_score, 100.0f);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Cleanup
    for (auto& bridge : bridges) {
        nimcp_health_agent_disconnect_thalamic(agent, &bridge);
    }
    nimcp_health_agent_disconnect_middleware(agent, ctx1);
    nimcp_health_agent_disconnect_middleware(agent, ctx2);
}

/* ============================================================================
 * Long-Running Monitoring Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthE2ETest, LongRunningThalamicMonitoring) {
    omni_wm_thalamic_bridge_t bridge;
    EXPECT_EQ(nimcp_health_agent_connect_thalamic(agent, &bridge, nullptr), 0);

    // Run for extended period
    auto start_time = std::chrono::steady_clock::now();
    int cycle_count = 0;

    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2)) {
        thalamic_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_thalamic_metrics(agent, &metrics), 0);

        float score = nimcp_health_agent_get_thalamic_health_score(agent);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);

        cycle_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_GT(cycle_count, 50);  // Should have run many cycles

    nimcp_health_agent_disconnect_thalamic(agent, &bridge);
}

TEST_F(ThalamicMiddlewareHealthE2ETest, LongRunningMiddlewareMonitoring) {
    nimcp_brain_training_ctx_t* ctx = mock_ctx(1);
    EXPECT_EQ(nimcp_health_agent_connect_middleware(agent, ctx, nullptr), 0);

    // Run for extended period
    auto start_time = std::chrono::steady_clock::now();
    int cycle_count = 0;

    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2)) {
        middleware_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_middleware_metrics(agent, &metrics), 0);

        float score = nimcp_health_agent_get_middleware_health_score(agent);
        EXPECT_GE(score, 0.0f);
        EXPECT_LE(score, 100.0f);

        cycle_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_GT(cycle_count, 50);  // Should have run many cycles

    nimcp_health_agent_disconnect_middleware(agent, ctx);
}

/* ============================================================================
 * Concurrent Stress Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthE2ETest, ConcurrentThalamicStress) {
    std::vector<omni_wm_thalamic_bridge_t> bridges(6);
    for (auto& bridge : bridges) {
        EXPECT_EQ(nimcp_health_agent_connect_thalamic(agent, &bridge, nullptr), 0);
    }

    std::atomic<int> total_operations{0};
    std::vector<std::thread> threads;

    // Launch multiple concurrent monitoring threads
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &total_operations]() {
            for (int i = 0; i < 50; i++) {
                thalamic_health_metrics_t metrics;
                nimcp_health_agent_get_thalamic_metrics(agent, &metrics);
                total_operations++;

                nimcp_health_agent_get_thalamic_health_score(agent);
                total_operations++;

                nimcp_health_agent_thalamic_needs_attention(agent);
                total_operations++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_operations.load(), 4 * 50 * 3);

    for (auto& bridge : bridges) {
        nimcp_health_agent_disconnect_thalamic(agent, &bridge);
    }
}

TEST_F(ThalamicMiddlewareHealthE2ETest, ConcurrentMiddlewareStress) {
    // Create mock context pointers
    nimcp_brain_training_ctx_t* ctx1 = mock_ctx(1);
    nimcp_brain_training_ctx_t* ctx2 = mock_ctx(2);
    nimcp_brain_training_ctx_t* ctx3 = mock_ctx(3);

    EXPECT_EQ(nimcp_health_agent_connect_middleware(agent, ctx1, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_middleware(agent, ctx2, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_middleware(agent, ctx3, nullptr), 0);

    std::atomic<int> total_operations{0};
    std::vector<std::thread> threads;

    // Launch multiple concurrent monitoring threads
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &total_operations]() {
            for (int i = 0; i < 50; i++) {
                middleware_health_metrics_t metrics;
                nimcp_health_agent_get_middleware_metrics(agent, &metrics);
                total_operations++;

                nimcp_health_agent_get_middleware_health_score(agent);
                total_operations++;

                nimcp_health_agent_middleware_needs_attention(agent);
                total_operations++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_operations.load(), 4 * 50 * 3);

    nimcp_health_agent_disconnect_middleware(agent, ctx1);
    nimcp_health_agent_disconnect_middleware(agent, ctx2);
    nimcp_health_agent_disconnect_middleware(agent, ctx3);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthE2ETest, ThalamicLifecycleTest) {
    // Multiple create/destroy cycles
    for (int round = 0; round < 3; round++) {
        omni_wm_thalamic_bridge_t bridge;

        // Connect
        EXPECT_EQ(nimcp_health_agent_connect_thalamic(agent, &bridge, nullptr), 0);

        // Use
        for (int i = 0; i < 10; i++) {
            thalamic_health_metrics_t metrics;
            EXPECT_EQ(nimcp_health_agent_get_thalamic_metrics(agent, &metrics), 0);
            EXPECT_EQ(metrics.num_bridges, 1u);
        }

        // Disconnect
        EXPECT_EQ(nimcp_health_agent_disconnect_thalamic(agent, &bridge), 0);

        // Verify clean state
        thalamic_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_thalamic_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_bridges, 0u);
    }
}

TEST_F(ThalamicMiddlewareHealthE2ETest, MiddlewareLifecycleTest) {
    // Multiple create/destroy cycles
    for (int round = 0; round < 3; round++) {
        nimcp_brain_training_ctx_t* ctx = mock_ctx(round + 100);

        // Connect
        EXPECT_EQ(nimcp_health_agent_connect_middleware(agent, ctx, nullptr), 0);

        // Use
        for (int i = 0; i < 10; i++) {
            middleware_health_metrics_t metrics;
            EXPECT_EQ(nimcp_health_agent_get_middleware_metrics(agent, &metrics), 0);
            EXPECT_EQ(metrics.num_contexts, 1u);
        }

        // Disconnect
        EXPECT_EQ(nimcp_health_agent_disconnect_middleware(agent, ctx), 0);

        // Verify clean state
        middleware_health_metrics_t metrics;
        EXPECT_EQ(nimcp_health_agent_get_middleware_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_contexts, 0u);
    }
}

/* ============================================================================
 * Runtime Config Change Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthE2ETest, ThalamicRuntimeConfigChanges) {
    omni_wm_thalamic_bridge_t bridge;
    EXPECT_EQ(nimcp_health_agent_connect_thalamic(agent, &bridge, nullptr), 0);

    // Get initial metrics
    thalamic_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_thalamic_metrics(agent, &metrics), 0);

    // Apply multiple config changes
    for (double threshold = 5.0; threshold <= 50.0; threshold += 5.0) {
        health_agent_thalamic_config_t config;
        nimcp_health_agent_thalamic_config_default(&config);
        config.max_update_time_ms = threshold;
        EXPECT_EQ(nimcp_health_agent_update_thalamic_config(agent, &config), 0);

        // Verify monitoring continues
        EXPECT_EQ(nimcp_health_agent_get_thalamic_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_bridges, 1u);
    }

    nimcp_health_agent_disconnect_thalamic(agent, &bridge);
}

TEST_F(ThalamicMiddlewareHealthE2ETest, MiddlewareRuntimeConfigChanges) {
    nimcp_brain_training_ctx_t* ctx = mock_ctx(1);
    EXPECT_EQ(nimcp_health_agent_connect_middleware(agent, ctx, nullptr), 0);

    // Get initial metrics
    middleware_health_metrics_t metrics;
    EXPECT_EQ(nimcp_health_agent_get_middleware_metrics(agent, &metrics), 0);

    // Apply multiple config changes
    for (float threshold = 5.0f; threshold <= 50.0f; threshold += 5.0f) {
        health_agent_middleware_config_t config;
        nimcp_health_agent_middleware_config_default(&config);
        config.loss_explosion_threshold = threshold;
        EXPECT_EQ(nimcp_health_agent_update_middleware_config(agent, &config), 0);

        // Verify monitoring continues
        EXPECT_EQ(nimcp_health_agent_get_middleware_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_contexts, 1u);
    }

    nimcp_health_agent_disconnect_middleware(agent, ctx);
}
