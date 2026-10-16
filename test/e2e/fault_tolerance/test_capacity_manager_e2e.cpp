/**
 * @file test_capacity_manager_e2e.cpp
 * @brief End-to-end tests for Phase 5.8 Dynamic Capacity Management
 * @date 2026-01-18
 *
 * Tests the complete capacity management workflow from creation
 * through monitoring, expansion, and health agent integration.
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

extern "C" {
#include "utils/fault_tolerance/nimcp_capacity_manager.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"
#include "utils/logging/nimcp_logging.h"
}

/**
 * @brief Test fixture for capacity manager E2E tests
 */
class CapacityManagerE2ETest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    capacity_manager_t* cm = nullptr;

    void SetUp() override {
        health_agent_config_t agent_config = {};
        agent_config.heartbeat_interval_ms = 50;
        agent_config.message_queue_depth = 128;
        agent_config.watchdog_timeout_ms = 5000;
        agent_config.enable_auto_recovery = true;

        agent = nimcp_health_agent_create(&agent_config);
        ASSERT_NE(agent, nullptr);

        capacity_config_t cm_config = {};
        capacity_config_default(&cm_config);
        cm_config.initial_capacity = 100;
        cm_config.max_capacity = 1000;
        cm_config.enable_auto_expand = true;
        cm_config.enable_immune_cleanup = true;

        ASSERT_EQ(capacity_manager_create(&cm, &cm_config, "e2e_test_module"), 0);
    }

    void TearDown() override {
        if (agent) {
            nimcp_health_agent_stop(agent);
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
        if (cm) {
            capacity_manager_destroy(cm);
            cm = nullptr;
        }
    }
};

/* ============================================================================
 * Complete Workflow E2E Tests
 * ============================================================================ */

TEST_F(CapacityManagerE2ETest, FullCapacityManagementWorkflow) {
    /* Step 1: Register with health agent */
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm), 0);

    /* Step 2: Start health agent */
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Step 3: Simulate workload - fill to 50% */
    for (int i = 0; i < 50; i++) {
        EXPECT_EQ(capacity_manager_request_slot(cm), 0);
    }

    /* Step 4: Check health metrics */
    capacity_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_managers, 1u);
    EXPECT_FLOAT_EQ(metrics.overall_pressure, 0.5f);
    EXPECT_FALSE(nimcp_health_agent_capacity_needs_attention(agent));

    /* Step 5: Fill to warning level (90%) */
    for (int i = 50; i < 90; i++) {
        EXPECT_EQ(capacity_manager_request_slot(cm), 0);
    }

    EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.managers_at_warning, 1u);
    EXPECT_TRUE(nimcp_health_agent_capacity_needs_attention(agent));

    /* Step 6: Fill to capacity - should trigger auto-expand */
    for (int i = 90; i < 100; i++) {
        EXPECT_EQ(capacity_manager_request_slot(cm), 0);
    }

    /* Request one more - triggers expansion */
    EXPECT_EQ(capacity_manager_request_slot(cm), 0);

    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_GT(stats.capacity, 100u);  /* Should have expanded */
    EXPECT_GE(stats.expansions, 1u);

    /* Step 7: Release some slots */
    for (int i = 0; i < 50; i++) {
        EXPECT_EQ(capacity_manager_release_slot(cm), 0);
    }

    /* Step 8: Verify final state */
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.current_count, 51u);

    /* Step 9: Stop agent */
    nimcp_health_agent_stop(agent);
}

TEST_F(CapacityManagerE2ETest, LongRunningCapacityMonitoring) {
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    /* Run for extended period with periodic operations */
    for (int cycle = 0; cycle < 10; cycle++) {
        /* Add some items */
        for (int i = 0; i < 10; i++) {
            capacity_manager_request_slot(cm);
        }

        /* Check metrics */
        capacity_health_metrics_t metrics = {};
        EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);
        EXPECT_GE(metrics.overall_pressure, 0.0f);
        EXPECT_LE(metrics.overall_pressure, 1.0f);

        /* Release some items */
        for (int i = 0; i < 5; i++) {
            capacity_manager_release_slot(cm);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    nimcp_health_agent_stop(agent);
}

/* ============================================================================
 * Auto-Expansion E2E Tests
 * ============================================================================ */

TEST_F(CapacityManagerE2ETest, AutoExpansionSequence) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    /* Fill to capacity multiple times, triggering multiple expansions */
    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    uint32_t original_capacity = stats.capacity;

    /* Fill to capacity */
    for (uint32_t i = 0; i < original_capacity; i++) {
        EXPECT_EQ(capacity_manager_request_slot(cm), 0);
    }

    /* Request more - triggers first expansion */
    for (int i = 0; i < 50; i++) {
        EXPECT_EQ(capacity_manager_request_slot(cm), 0);
    }

    capacity_manager_get_stats(cm, &stats);
    EXPECT_GT(stats.capacity, original_capacity);
    EXPECT_GE(stats.expansions, 1u);

    /* Fill to new capacity */
    uint32_t current = stats.current_count;
    uint32_t new_capacity = stats.capacity;
    for (uint32_t i = current; i < new_capacity; i++) {
        EXPECT_EQ(capacity_manager_request_slot(cm), 0);
    }

    /* Request more - triggers second expansion */
    EXPECT_EQ(capacity_manager_request_slot(cm), 0);

    capacity_manager_get_stats(cm, &stats);
    EXPECT_GT(stats.capacity, new_capacity);
    EXPECT_GE(stats.expansions, 2u);

    nimcp_health_agent_stop(agent);
}

TEST_F(CapacityManagerE2ETest, MaxCapacityEnforcement) {
    /* Create manager with small max capacity */
    capacity_config_t config = {};
    capacity_config_default(&config);
    config.initial_capacity = 50;
    config.max_capacity = 100;
    config.enable_auto_expand = true;

    capacity_manager_t* limited_cm = nullptr;
    ASSERT_EQ(capacity_manager_create(&limited_cm, &config, "limited_module"), 0);

    /* Fill to initial capacity */
    for (int i = 0; i < 50; i++) {
        EXPECT_EQ(capacity_manager_request_slot(limited_cm), 0);
    }

    /* Request more - triggers expansion */
    EXPECT_EQ(capacity_manager_request_slot(limited_cm), 0);

    capacity_stats_t stats;
    capacity_manager_get_stats(limited_cm, &stats);
    EXPECT_EQ(stats.capacity, 100u);  /* Should hit max */

    /* Fill to max */
    for (uint32_t i = stats.current_count; i < 100; i++) {
        EXPECT_EQ(capacity_manager_request_slot(limited_cm), 0);
    }

    /* Request more - should fail (at max) */
    EXPECT_EQ(capacity_manager_request_slot(limited_cm), -1);

    capacity_manager_get_stats(limited_cm, &stats);
    EXPECT_GE(stats.failed_allocations, 1u);

    capacity_manager_destroy(limited_cm);
}

/* ============================================================================
 * Concurrent Access E2E Tests
 * ============================================================================ */

TEST_F(CapacityManagerE2ETest, ConcurrentSlotRequests) {
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm), 0);
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};

    /* Launch multiple threads requesting slots */
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &success_count, &failure_count]() {
            for (int i = 0; i < 50; i++) {
                if (capacity_manager_request_slot(cm) == 0) {
                    success_count++;
                } else {
                    failure_count++;
                }
            }
        });
    }

    /* Wait for all threads */
    for (auto& t : threads) {
        t.join();
    }

    /* Verify total count matches */
    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.current_count, static_cast<uint32_t>(success_count.load()));

    nimcp_health_agent_stop(agent);
}

TEST_F(CapacityManagerE2ETest, ConcurrentRequestAndRelease) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    /* Pre-fill with some items */
    for (int i = 0; i < 50; i++) {
        capacity_manager_request_slot(cm);
    }

    std::atomic<bool> running{true};
    std::atomic<int> requests{0};
    std::atomic<int> releases{0};

    /* Launch request threads */
    std::vector<std::thread> threads;
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, &running, &requests]() {
            while (running) {
                if (capacity_manager_request_slot(cm) == 0) {
                    requests++;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    /* Launch release threads */
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, &running, &releases]() {
            while (running) {
                if (capacity_manager_release_slot(cm) == 0) {
                    releases++;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    /* Run for a bit */
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    /* Verify count is consistent */
    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    int expected = 50 + requests.load() - releases.load();
    EXPECT_EQ(stats.current_count, static_cast<uint32_t>(std::max(0, expected)));

    nimcp_health_agent_stop(agent);
}

/* ============================================================================
 * Multiple Managers E2E Tests
 * ============================================================================ */

TEST_F(CapacityManagerE2ETest, MultipleManagersWorkflow) {
    /* Create additional managers */
    capacity_config_t config = {};
    capacity_config_default(&config);
    config.initial_capacity = 200;
    config.max_capacity = 2000;

    capacity_manager_t* cm2 = nullptr;
    capacity_manager_t* cm3 = nullptr;
    ASSERT_EQ(capacity_manager_create(&cm2, &config, "module_2"), 0);

    config.initial_capacity = 300;
    ASSERT_EQ(capacity_manager_create(&cm3, &config, "module_3"), 0);

    /* Register all managers */
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm), 0);
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm2), 0);
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm3), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Apply different loads to each */
    for (int i = 0; i < 50; i++) capacity_manager_request_slot(cm);   /* 50% */
    for (int i = 0; i < 180; i++) capacity_manager_request_slot(cm2);  /* 90% */
    for (int i = 0; i < 150; i++) capacity_manager_request_slot(cm3);  /* 50% */

    /* Check aggregate metrics */
    capacity_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_managers, 3u);
    EXPECT_EQ(metrics.managers_at_warning, 1u);  /* cm2 at 90% */
    EXPECT_TRUE(nimcp_health_agent_capacity_needs_attention(agent));

    /* Average pressure: (50/100 + 180/200 + 150/300) / 3 = (0.5 + 0.9 + 0.5) / 3 = 0.633 */
    EXPECT_GT(metrics.overall_pressure, 0.5f);
    EXPECT_LT(metrics.overall_pressure, 0.8f);

    nimcp_health_agent_stop(agent);

    capacity_manager_destroy(cm2);
    capacity_manager_destroy(cm3);
}

/* ============================================================================
 * Lifecycle E2E Tests
 * ============================================================================ */

TEST_F(CapacityManagerE2ETest, MultipleStartStopCycles) {
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm), 0);

    for (int cycle = 0; cycle < 5; cycle++) {
        /* Start */
        EXPECT_EQ(nimcp_health_agent_start(agent), 0);

        /* Use capacity features */
        for (int i = 0; i < 20; i++) {
            capacity_manager_request_slot(cm);
        }

        capacity_health_metrics_t metrics = {};
        EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);
        EXPECT_EQ(metrics.num_managers, 1u);

        /* Release */
        for (int i = 0; i < 20; i++) {
            capacity_manager_release_slot(cm);
        }

        /* Stop */
        nimcp_health_agent_stop(agent);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

TEST_F(CapacityManagerE2ETest, RegistrationDuringOperation) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    /* Register while running */
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm), 0);

    /* Use immediately */
    for (int i = 0; i < 50; i++) {
        EXPECT_EQ(capacity_manager_request_slot(cm), 0);
    }

    capacity_health_metrics_t metrics = {};
    EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_managers, 1u);

    /* Unregister while running */
    EXPECT_EQ(nimcp_health_agent_unregister_capacity_manager(agent, cm), 0);

    EXPECT_EQ(nimcp_health_agent_get_capacity_metrics(agent, &metrics), 0);
    EXPECT_EQ(metrics.num_managers, 0u);

    /* Re-register */
    EXPECT_EQ(nimcp_health_agent_register_capacity_manager(agent, cm), 0);

    nimcp_health_agent_stop(agent);
}

/* ============================================================================
 * Statistics Tracking E2E Tests
 * ============================================================================ */

TEST_F(CapacityManagerE2ETest, PeakTrackingWorkflow) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    /* Fill to high watermark */
    for (int i = 0; i < 80; i++) {
        capacity_manager_request_slot(cm);
    }

    /* Release some */
    for (int i = 0; i < 30; i++) {
        capacity_manager_release_slot(cm);
    }

    /* Check peak was recorded */
    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.current_count, 50u);
    EXPECT_EQ(stats.peak_count, 80u);
    EXPECT_FLOAT_EQ(stats.peak_utilization, 0.8f);

    /* Fill higher */
    for (int i = 0; i < 45; i++) {
        capacity_manager_request_slot(cm);
    }

    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.peak_count, 95u);
    EXPECT_FLOAT_EQ(stats.peak_utilization, 0.95f);

    nimcp_health_agent_stop(agent);
}

TEST_F(CapacityManagerE2ETest, ExpansionStatisticsWorkflow) {
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    capacity_stats_t stats;
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.expansions, 0u);

    /* Trigger expansions */
    capacity_manager_trigger_expand(cm);
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.expansions, 1u);
    EXPECT_GT(stats.last_expansion_time, 0u);

    capacity_manager_trigger_expand(cm);
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.expansions, 2u);

    /* Reset and verify */
    capacity_manager_reset_stats(cm);
    capacity_manager_get_stats(cm, &stats);
    EXPECT_EQ(stats.expansions, 0u);

    nimcp_health_agent_stop(agent);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
