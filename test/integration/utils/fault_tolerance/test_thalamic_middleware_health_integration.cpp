/**
 * @file test_thalamic_middleware_health_integration.cpp
 * @brief Integration tests for Phase 5.11: Thalamic/Middleware Health Integration
 *
 * Tests the integration of health agent with actual thalamic bridges and
 * middleware training contexts.
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
 * @brief Integration test fixture for Thalamic/Middleware Health tests
 */
class ThalamicMiddlewareHealthIntegrationTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;

    void SetUp() override {
        health_agent_config_t config;
        nimcp_health_agent_default_config(&config);
        config.check_interval_ms = 50;
        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr);
    }

    void TearDown() override {
        if (agent) {
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }

    // Helper to create mock context pointers
    nimcp_brain_training_ctx_t* mock_ctx(uintptr_t id) {
        return reinterpret_cast<nimcp_brain_training_ctx_t*>(0x10000 + id);
    }
};

/* ============================================================================
 * Multi-Bridge Registration Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthIntegrationTest, RegisterMultipleThalamicBridges) {
    // Create multiple mock bridge pointers
    omni_wm_thalamic_bridge_t bridge1, bridge2, bridge3;

    // Register first bridge
    int result = nimcp_health_agent_connect_thalamic(agent, &bridge1, nullptr);
    EXPECT_EQ(result, 0);

    // Register second bridge
    result = nimcp_health_agent_connect_thalamic(agent, &bridge2, nullptr);
    EXPECT_EQ(result, 0);

    // Register third bridge
    result = nimcp_health_agent_connect_thalamic(agent, &bridge3, nullptr);
    EXPECT_EQ(result, 0);

    // Get metrics to verify count
    thalamic_health_metrics_t metrics;
    nimcp_health_agent_get_thalamic_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_bridges, 3u);

    // Disconnect all
    nimcp_health_agent_disconnect_thalamic(agent, &bridge1);
    nimcp_health_agent_disconnect_thalamic(agent, &bridge2);
    nimcp_health_agent_disconnect_thalamic(agent, &bridge3);

    // Verify all disconnected
    nimcp_health_agent_get_thalamic_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_bridges, 0u);
}

TEST_F(ThalamicMiddlewareHealthIntegrationTest, RegisterMultipleTrainingContexts) {
    // Create mock context pointers
    nimcp_brain_training_ctx_t* ctx1 = mock_ctx(1);
    nimcp_brain_training_ctx_t* ctx2 = mock_ctx(2);

    // Register first context
    int result = nimcp_health_agent_connect_middleware(agent, ctx1, nullptr);
    EXPECT_EQ(result, 0);

    // Register second context
    result = nimcp_health_agent_connect_middleware(agent, ctx2, nullptr);
    EXPECT_EQ(result, 0);

    // Get metrics to verify count
    middleware_health_metrics_t metrics;
    nimcp_health_agent_get_middleware_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_contexts, 2u);

    // Disconnect all
    nimcp_health_agent_disconnect_middleware(agent, ctx1);
    nimcp_health_agent_disconnect_middleware(agent, ctx2);

    // Verify all disconnected
    nimcp_health_agent_get_middleware_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_contexts, 0u);
}

/* ============================================================================
 * Duplicate Registration Prevention Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthIntegrationTest, PreventDuplicateThalamicRegistration) {
    omni_wm_thalamic_bridge_t bridge;

    // Register bridge
    int result = nimcp_health_agent_connect_thalamic(agent, &bridge, nullptr);
    EXPECT_EQ(result, 0);

    // Try to register same bridge again - should return 0 (already registered)
    result = nimcp_health_agent_connect_thalamic(agent, &bridge, nullptr);
    EXPECT_EQ(result, 0);

    // Verify only one bridge is registered
    thalamic_health_metrics_t metrics;
    nimcp_health_agent_get_thalamic_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_bridges, 1u);

    nimcp_health_agent_disconnect_thalamic(agent, &bridge);
}

TEST_F(ThalamicMiddlewareHealthIntegrationTest, PreventDuplicateMiddlewareRegistration) {
    nimcp_brain_training_ctx_t* ctx = mock_ctx(1);

    // Register context
    int result = nimcp_health_agent_connect_middleware(agent, ctx, nullptr);
    EXPECT_EQ(result, 0);

    // Try to register same context again - should return 0 (already registered)
    result = nimcp_health_agent_connect_middleware(agent, ctx, nullptr);
    EXPECT_EQ(result, 0);

    // Verify only one context is registered
    middleware_health_metrics_t metrics;
    nimcp_health_agent_get_middleware_metrics(agent, &metrics);
    EXPECT_EQ(metrics.num_contexts, 1u);

    nimcp_health_agent_disconnect_middleware(agent, ctx);
}

/* ============================================================================
 * Health Check Cycle Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthIntegrationTest, ThalamicHealthCheckCycle) {
    omni_wm_thalamic_bridge_t bridge;
    nimcp_health_agent_connect_thalamic(agent, &bridge, nullptr);

    // Run multiple health check cycles
    for (int i = 0; i < 5; i++) {
        thalamic_health_metrics_t metrics;
        int result = nimcp_health_agent_get_thalamic_metrics(agent, &metrics);
        EXPECT_EQ(result, 0);
        EXPECT_EQ(metrics.num_bridges, 1u);
        EXPECT_GT(metrics.last_check_timestamp_us, 0u);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    nimcp_health_agent_disconnect_thalamic(agent, &bridge);
}

TEST_F(ThalamicMiddlewareHealthIntegrationTest, MiddlewareHealthCheckCycle) {
    nimcp_brain_training_ctx_t* ctx = mock_ctx(1);
    nimcp_health_agent_connect_middleware(agent, ctx, nullptr);

    // Run multiple health check cycles
    for (int i = 0; i < 5; i++) {
        middleware_health_metrics_t metrics;
        int result = nimcp_health_agent_get_middleware_metrics(agent, &metrics);
        EXPECT_EQ(result, 0);
        EXPECT_EQ(metrics.num_contexts, 1u);
        EXPECT_GT(metrics.last_check_timestamp_us, 0u);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    nimcp_health_agent_disconnect_middleware(agent, ctx);
}

/* ============================================================================
 * Configuration Update Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthIntegrationTest, RuntimeThalamicConfigUpdate) {
    omni_wm_thalamic_bridge_t bridge;
    nimcp_health_agent_connect_thalamic(agent, &bridge, nullptr);

    // Get initial metrics
    thalamic_health_metrics_t metrics1;
    nimcp_health_agent_get_thalamic_metrics(agent, &metrics1);

    // Update configuration
    health_agent_thalamic_config_t new_config;
    nimcp_health_agent_thalamic_config_default(&new_config);
    new_config.max_update_time_ms = 50.0;
    new_config.attention_imbalance_threshold = 0.8f;
    EXPECT_EQ(nimcp_health_agent_update_thalamic_config(agent, &new_config), 0);

    // Get metrics after config update
    thalamic_health_metrics_t metrics2;
    nimcp_health_agent_get_thalamic_metrics(agent, &metrics2);
    EXPECT_GE(metrics2.last_check_timestamp_us, metrics1.last_check_timestamp_us);

    nimcp_health_agent_disconnect_thalamic(agent, &bridge);
}

TEST_F(ThalamicMiddlewareHealthIntegrationTest, RuntimeMiddlewareConfigUpdate) {
    nimcp_brain_training_ctx_t* ctx = mock_ctx(1);
    nimcp_health_agent_connect_middleware(agent, ctx, nullptr);

    // Get initial metrics
    middleware_health_metrics_t metrics1;
    nimcp_health_agent_get_middleware_metrics(agent, &metrics1);

    // Update configuration
    health_agent_middleware_config_t new_config;
    nimcp_health_agent_middleware_config_default(&new_config);
    new_config.max_batch_time_ms = 5000.0;
    new_config.loss_explosion_threshold = 100.0f;
    EXPECT_EQ(nimcp_health_agent_update_middleware_config(agent, &new_config), 0);

    // Get metrics after config update
    middleware_health_metrics_t metrics2;
    nimcp_health_agent_get_middleware_metrics(agent, &metrics2);
    EXPECT_GE(metrics2.last_check_timestamp_us, metrics1.last_check_timestamp_us);

    nimcp_health_agent_disconnect_middleware(agent, ctx);
}

/* ============================================================================
 * Concurrent Access Tests
 * ============================================================================ */

TEST_F(ThalamicMiddlewareHealthIntegrationTest, ConcurrentThalamicAccess) {
    omni_wm_thalamic_bridge_t bridge;
    nimcp_health_agent_connect_thalamic(agent, &bridge, nullptr);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // Launch multiple threads accessing thalamic health
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &success_count]() {
            for (int j = 0; j < 25; j++) {
                thalamic_health_metrics_t metrics;
                if (nimcp_health_agent_get_thalamic_metrics(agent, &metrics) == 0) {
                    success_count++;
                }

                float score = nimcp_health_agent_get_thalamic_health_score(agent);
                if (score >= 0.0f && score <= 100.0f) {
                    success_count++;
                }

                bool needs_attention = nimcp_health_agent_thalamic_needs_attention(agent);
                (void)needs_attention;
                success_count++;
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // All operations should succeed
    EXPECT_EQ(success_count.load(), 4 * 25 * 3);

    nimcp_health_agent_disconnect_thalamic(agent, &bridge);
}

TEST_F(ThalamicMiddlewareHealthIntegrationTest, ConcurrentMiddlewareAccess) {
    nimcp_brain_training_ctx_t* ctx = mock_ctx(1);
    nimcp_health_agent_connect_middleware(agent, ctx, nullptr);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // Launch multiple threads accessing middleware health
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &success_count]() {
            for (int j = 0; j < 25; j++) {
                middleware_health_metrics_t metrics;
                if (nimcp_health_agent_get_middleware_metrics(agent, &metrics) == 0) {
                    success_count++;
                }

                float score = nimcp_health_agent_get_middleware_health_score(agent);
                if (score >= 0.0f && score <= 100.0f) {
                    success_count++;
                }

                bool needs_attention = nimcp_health_agent_middleware_needs_attention(agent);
                (void)needs_attention;
                success_count++;
            }
        });
    }

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    // All operations should succeed
    EXPECT_EQ(success_count.load(), 4 * 25 * 3);

    nimcp_health_agent_disconnect_middleware(agent, ctx);
}
