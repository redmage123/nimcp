/**
 * @file test_portia_attention.cpp
 * @brief Unit tests for Portia attention-based resource allocation
 *
 * Tests cover:
 * - Initialization and configuration
 * - Salience updates and decay
 * - Resource allocation algorithm
 * - Preemption and priority handling
 * - Smooth transitions and hysteresis
 * - Thread safety
 * - Statistics tracking
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>

// Headers have their own extern "C" guards
#include "portia/nimcp_portia_attention.h"
#include "security/nimcp_bbb_helpers.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Test Fixture
//=============================================================================

class PortiaAttentionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize BBB helpers (required for Portia attention tests)
        if (!bbb_helpers_is_initialized()) {
            bbb_helpers_init();
        }

        // Initialize logging with console-only config (avoids mkdir_p failures)
        nimcp_log_config_t log_config = nimcp_log_default_config();
        log_config.level = LOG_LEVEL_DEBUG;
        log_config.destinations = NIMCP_LOG_DEST_CONSOLE;
        nimcp_log_init(&log_config);
    }

    void TearDown() override {
        nimcp_log_shutdown();
        bbb_helpers_shutdown();
    }
};

//=============================================================================
// Initialization Tests
//=============================================================================

TEST_F(PortiaAttentionTest, DefaultConfiguration) {
    portia_attention_config_t config = portia_attention_default_config();

    EXPECT_GT(config.reallocation_threshold, 0.0f);
    EXPECT_LT(config.reallocation_threshold, 1.0f);
    EXPECT_GT(config.decay_rate_per_second, 0.0f);
    EXPECT_LT(config.decay_rate_per_second, 1.0f);
    EXPECT_GT(config.update_interval_ms, 0u);
    EXPECT_GE(config.hysteresis_factor, 0.0f);
    EXPECT_LE(config.hysteresis_factor, 1.0f);
    EXPECT_GE(config.smoothing_alpha, 0.0f);
    EXPECT_LE(config.smoothing_alpha, 1.0f);
}

TEST_F(PortiaAttentionTest, InitializationBasic) {
    portia_attention_state_t state = portia_attention_init(nullptr, 5, 1.0f);
    ASSERT_NE(state, nullptr);

    // Verify initial allocations
    for (int i = 0; i < 5; i++) {
        float alloc = portia_attention_get_allocation(state, (attention_target_t)i);
        EXPECT_GT(alloc, 0.0f);
        EXPECT_LT(alloc, 1.0f);
    }

    portia_attention_destroy(state);
}

TEST_F(PortiaAttentionTest, InitializationWithCustomConfig) {
    portia_attention_config_t config = {
        .reallocation_threshold = 0.1f,
        .decay_rate_per_second = 0.2f,
        .update_interval_ms = 50,
        .enable_preemption = false,
        .preemption_threshold = 0.5f,
        .hysteresis_factor = 0.1f,
        .smoothing_alpha = 0.5f
    };

    portia_attention_state_t state = portia_attention_init(&config, 3, 0.9f);
    ASSERT_NE(state, nullptr);

    portia_attention_destroy(state);
}

TEST_F(PortiaAttentionTest, InitializationInvalidParams) {
    // Zero resources
    portia_attention_state_t state1 = portia_attention_init(nullptr, 0, 1.0f);
    EXPECT_EQ(state1, nullptr);

    // Invalid budget
    portia_attention_state_t state2 = portia_attention_init(nullptr, 5, 0.0f);
    EXPECT_EQ(state2, nullptr);

    portia_attention_state_t state3 = portia_attention_init(nullptr, 5, 1.5f);
    EXPECT_EQ(state3, nullptr);
}

//=============================================================================
// Salience Management Tests
//=============================================================================

TEST_F(PortiaAttentionTest, SalienceUpdate) {
    portia_attention_state_t state = portia_attention_init(nullptr, 5, 1.0f);
    ASSERT_NE(state, nullptr);

    // Update salience
    int result = portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 0.8f);
    EXPECT_EQ(result, 0);

    // Verify update
    float salience = portia_attention_get_salience(state, ATTENTION_TARGET_NEURONS);
    EXPECT_FLOAT_EQ(salience, 0.8f);

    portia_attention_destroy(state);
}

TEST_F(PortiaAttentionTest, SalienceRange) {
    portia_attention_state_t state = portia_attention_init(nullptr, 5, 1.0f);
    ASSERT_NE(state, nullptr);

    // Valid range
    EXPECT_EQ(portia_attention_update_salience(state, ATTENTION_TARGET_MEMORY, 0.0f), 0);
    EXPECT_EQ(portia_attention_update_salience(state, ATTENTION_TARGET_MEMORY, 1.0f), 0);
    EXPECT_EQ(portia_attention_update_salience(state, ATTENTION_TARGET_MEMORY, 0.5f), 0);

    // Invalid range
    EXPECT_NE(portia_attention_update_salience(state, ATTENTION_TARGET_MEMORY, -0.1f), 0);
    EXPECT_NE(portia_attention_update_salience(state, ATTENTION_TARGET_MEMORY, 1.1f), 0);

    portia_attention_destroy(state);
}

TEST_F(PortiaAttentionTest, SalienceDecay) {
    portia_attention_config_t config = portia_attention_default_config();
    config.decay_rate_per_second = 0.5f;  // 50% decay per second

    portia_attention_state_t state = portia_attention_init(&config, 5, 1.0f);
    ASSERT_NE(state, nullptr);

    // Set high salience
    portia_attention_update_salience(state, ATTENTION_TARGET_PROCESSING, 1.0f);

    // Get initial time
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t start_ms = ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;

    // Wait 1 second
    std::this_thread::sleep_for(std::chrono::seconds(1));

    uint64_t end_ms = start_ms + 1000;

    // Apply decay
    portia_attention_decay(state, end_ms);

    // Verify decay occurred
    float decayed = portia_attention_get_salience(state, ATTENTION_TARGET_PROCESSING);
    EXPECT_LT(decayed, 1.0f);
    EXPECT_GT(decayed, 0.0f);

    // Should be approximately exp(-0.5 * 1) ≈ 0.606
    EXPECT_NEAR(decayed, 0.606f, 0.1f);

    portia_attention_destroy(state);
}

//=============================================================================
// Resource Allocation Tests
//=============================================================================

TEST_F(PortiaAttentionTest, BasicAllocation) {
    portia_attention_state_t state = portia_attention_init(nullptr, 5, 1.0f);
    ASSERT_NE(state, nullptr);

    // Update saliences to create clear priority
    portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 1.0f);
    portia_attention_update_salience(state, ATTENTION_TARGET_MEMORY, 0.5f);
    portia_attention_update_salience(state, ATTENTION_TARGET_PROCESSING, 0.3f);
    portia_attention_update_salience(state, ATTENTION_TARGET_SENSORS, 0.2f);
    portia_attention_update_salience(state, ATTENTION_TARGET_COMMUNICATION, 0.1f);

    // Force reallocation
    int result = portia_attention_reallocate(state, true);
    EXPECT_EQ(result, 0);

    // Verify allocations favor high salience
    float neurons_alloc = portia_attention_get_allocation(state, ATTENTION_TARGET_NEURONS);
    float memory_alloc = portia_attention_get_allocation(state, ATTENTION_TARGET_MEMORY);
    float comm_alloc = portia_attention_get_allocation(state, ATTENTION_TARGET_COMMUNICATION);

    EXPECT_GT(neurons_alloc, memory_alloc);
    EXPECT_GT(memory_alloc, comm_alloc);

    // Verify total allocation
    float total = 0.0f;
    for (int i = 0; i < 5; i++) {
        total += portia_attention_get_allocation(state, (attention_target_t)i);
    }
    EXPECT_NEAR(total, 1.0f, 0.1f);  // Allow some tolerance

    portia_attention_destroy(state);
}

TEST_F(PortiaAttentionTest, AllocationWithMinMax) {
    portia_attention_state_t state = portia_attention_init(nullptr, 3, 1.0f);
    ASSERT_NE(state, nullptr);

    // Get all allocations (returns a copy - no constraint setter API exists)
    attention_resource_t resources[3];
    int count = portia_attention_get_all_allocations(state, resources, 3);
    EXPECT_EQ(count, 3);

    // Set high salience for resource 0
    portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 1.0f);
    portia_attention_update_salience(state, ATTENTION_TARGET_MEMORY, 0.3f);
    portia_attention_update_salience(state, ATTENTION_TARGET_PROCESSING, 0.1f);

    portia_attention_reallocate(state, true);

    // Verify allocations are proportional to salience
    float alloc0 = portia_attention_get_allocation(state, ATTENTION_TARGET_NEURONS);
    float alloc1 = portia_attention_get_allocation(state, ATTENTION_TARGET_MEMORY);
    float alloc2 = portia_attention_get_allocation(state, ATTENTION_TARGET_PROCESSING);

    // Higher salience should get more allocation
    EXPECT_GT(alloc0, alloc1);
    EXPECT_GT(alloc1, alloc2);

    // All allocations should be positive and within [0,1]
    EXPECT_GT(alloc0, 0.0f);
    EXPECT_LE(alloc0, 1.0f);
    EXPECT_GT(alloc1, 0.0f);
    EXPECT_LE(alloc1, 1.0f);
    EXPECT_GT(alloc2, 0.0f);
    EXPECT_LE(alloc2, 1.0f);

    // Total should approximately equal budget
    float total = alloc0 + alloc1 + alloc2;
    EXPECT_NEAR(total, 1.0f, 0.1f);

    portia_attention_destroy(state);
}

TEST_F(PortiaAttentionTest, ResourceRequest) {
    portia_attention_state_t state = portia_attention_init(nullptr, 5, 1.0f);
    ASSERT_NE(state, nullptr);

    float initial = portia_attention_get_allocation(state, ATTENTION_TARGET_NEURONS);

    // Request more resources
    int result = portia_attention_request(state, ATTENTION_TARGET_NEURONS, 0.5f);
    EXPECT_EQ(result, 0);

    portia_attention_destroy(state);
}

TEST_F(PortiaAttentionTest, ResourceRelease) {
    portia_attention_state_t state = portia_attention_init(nullptr, 5, 1.0f);
    ASSERT_NE(state, nullptr);

    float initial = portia_attention_get_allocation(state, ATTENTION_TARGET_MEMORY);

    // Release some resources
    float release_amount = initial * 0.3f;
    int result = portia_attention_release(state, ATTENTION_TARGET_MEMORY, release_amount);
    EXPECT_EQ(result, 0);

    float after_release = portia_attention_get_allocation(state, ATTENTION_TARGET_MEMORY);
    EXPECT_LT(after_release, initial);

    portia_attention_destroy(state);
}

//=============================================================================
// Hysteresis and Smoothing Tests
//=============================================================================

TEST_F(PortiaAttentionTest, Hysteresis) {
    portia_attention_config_t config = portia_attention_default_config();
    config.reallocation_threshold = 0.1f;  // 10% threshold
    config.hysteresis_factor = 0.2f;

    portia_attention_state_t state = portia_attention_init(&config, 3, 1.0f);
    ASSERT_NE(state, nullptr);

    // Get initial allocation
    portia_attention_reallocate(state, true);
    float initial = portia_attention_get_allocation(state, ATTENTION_TARGET_NEURONS);

    // Small change below threshold
    portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 0.52f);
    portia_attention_reallocate(state, false);

    float after_small = portia_attention_get_allocation(state, ATTENTION_TARGET_NEURONS);

    // Should not change significantly due to hysteresis
    EXPECT_NEAR(initial, after_small, 0.05f);

    portia_attention_destroy(state);
}

TEST_F(PortiaAttentionTest, ExponentialSmoothing) {
    portia_attention_config_t config = portia_attention_default_config();
    config.smoothing_alpha = 0.5f;  // 50% new, 50% old

    portia_attention_state_t state = portia_attention_init(&config, 3, 1.0f);
    ASSERT_NE(state, nullptr);

    // Force multiple reallocations with changing salience
    for (int i = 0; i < 5; i++) {
        float salience = 0.3f + (i * 0.1f);
        portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, salience);
        portia_attention_reallocate(state, true);

        // Wait briefly
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Allocation should have changed smoothly
    float final = portia_attention_get_allocation(state, ATTENTION_TARGET_NEURONS);
    EXPECT_GT(final, 0.0f);

    portia_attention_destroy(state);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(PortiaAttentionTest, Statistics) {
    portia_attention_state_t state = portia_attention_init(nullptr, 5, 1.0f);
    ASSERT_NE(state, nullptr);

    // Perform some operations
    portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 0.8f);
    portia_attention_update_salience(state, ATTENTION_TARGET_MEMORY, 0.6f);
    portia_attention_reallocate(state, true);
    portia_attention_request(state, ATTENTION_TARGET_PROCESSING, 0.3f);
    portia_attention_release(state, ATTENTION_TARGET_SENSORS, 0.1f);

    // Get statistics
    portia_attention_stats_t stats;
    int result = portia_attention_get_stats(state, &stats);
    EXPECT_EQ(result, 0);

    EXPECT_GE(stats.salience_updates, 2u);
    EXPECT_GE(stats.reallocations, 1u);
    EXPECT_GE(stats.requests, 1u);
    EXPECT_GE(stats.releases, 1u);
    EXPECT_GT(stats.avg_salience, 0.0f);
    EXPECT_LE(stats.avg_salience, 1.0f);

    // Reset statistics
    portia_attention_reset_stats(state);

    result = portia_attention_get_stats(state, &stats);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(stats.salience_updates, 0u);
    EXPECT_EQ(stats.reallocations, 0u);

    portia_attention_destroy(state);
}

//=============================================================================
// Thread Safety Tests
//=============================================================================

TEST_F(PortiaAttentionTest, ConcurrentSalienceUpdates) {
    portia_attention_state_t state = portia_attention_init(nullptr, 5, 1.0f);
    ASSERT_NE(state, nullptr);

    const int num_threads = 4;
    const int updates_per_thread = 100;

    std::vector<std::thread> threads;

    // Launch threads that update salience
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([state, t, updates_per_thread]() {
            for (int i = 0; i < updates_per_thread; i++) {
                attention_target_t target = (attention_target_t)(i % 5);
                float salience = 0.1f + (i % 10) * 0.09f;
                portia_attention_update_salience(state, target, salience);
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify statistics
    portia_attention_stats_t stats;
    portia_attention_get_stats(state, &stats);
    EXPECT_EQ(stats.salience_updates, num_threads * updates_per_thread);

    portia_attention_destroy(state);
}

TEST_F(PortiaAttentionTest, ConcurrentReallocation) {
    portia_attention_state_t state = portia_attention_init(nullptr, 5, 1.0f);
    ASSERT_NE(state, nullptr);

    const int num_threads = 4;
    const int ops_per_thread = 50;

    std::vector<std::thread> threads;

    // Launch threads with mixed operations
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([state, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; i++) {
                attention_target_t target = (attention_target_t)(i % 5);

                if (i % 3 == 0) {
                    float salience = 0.2f + (i % 8) * 0.1f;
                    portia_attention_update_salience(state, target, salience);
                } else if (i % 3 == 1) {
                    portia_attention_reallocate(state, false);
                } else {
                    portia_attention_get_allocation(state, target);
                }
            }
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    // System should still be consistent
    float total = 0.0f;
    for (int i = 0; i < 5; i++) {
        float alloc = portia_attention_get_allocation(state, (attention_target_t)i);
        EXPECT_GE(alloc, 0.0f);
        EXPECT_LE(alloc, 1.0f);
        total += alloc;
    }

    portia_attention_destroy(state);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(PortiaAttentionTest, TargetNames) {
    EXPECT_STREQ(portia_attention_target_name(ATTENTION_TARGET_NEURONS), "NEURONS");
    EXPECT_STREQ(portia_attention_target_name(ATTENTION_TARGET_MEMORY), "MEMORY");
    EXPECT_STREQ(portia_attention_target_name(ATTENTION_TARGET_PROCESSING), "PROCESSING");
    EXPECT_STREQ(portia_attention_target_name(ATTENTION_TARGET_SENSORS), "SENSORS");
    EXPECT_STREQ(portia_attention_target_name(ATTENTION_TARGET_COMMUNICATION), "COMMUNICATION");
}

TEST_F(PortiaAttentionTest, EventNames) {
    EXPECT_STREQ(portia_attention_event_name(ATTENTION_EVENT_SALIENCE_UPDATED),
                 "SALIENCE_UPDATED");
    EXPECT_STREQ(portia_attention_event_name(ATTENTION_EVENT_ALLOCATION_CHANGED),
                 "ALLOCATION_CHANGED");
    EXPECT_STREQ(portia_attention_event_name(ATTENTION_EVENT_RESOURCES_REQUESTED),
                 "RESOURCES_REQUESTED");
}

TEST_F(PortiaAttentionTest, PrintState) {
    portia_attention_state_t state = portia_attention_init(nullptr, 5, 1.0f);
    ASSERT_NE(state, nullptr);

    // Update some values
    portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 0.9f);
    portia_attention_update_salience(state, ATTENTION_TARGET_MEMORY, 0.6f);
    portia_attention_reallocate(state, true);

    // Print state (should not crash)
    portia_attention_print_state(state);

    portia_attention_destroy(state);
}

//=============================================================================
// Integration Test
//=============================================================================

TEST_F(PortiaAttentionTest, CompleteWorkflow) {
    // Create system with custom config
    portia_attention_config_t config = portia_attention_default_config();
    config.reallocation_threshold = 0.05f;
    config.decay_rate_per_second = 0.1f;
    config.enable_preemption = true;

    portia_attention_state_t state = portia_attention_init(&config, 5, 1.0f);
    ASSERT_NE(state, nullptr);

    // Simulate hunting behavior (high priority task)
    portia_attention_update_salience(state, ATTENTION_TARGET_NEURONS, 1.0f);
    portia_attention_update_salience(state, ATTENTION_TARGET_PROCESSING, 0.9f);
    portia_attention_update_salience(state, ATTENTION_TARGET_SENSORS, 0.8f);
    portia_attention_reallocate(state, true);

    // Check allocations favor hunting
    float neurons = portia_attention_get_allocation(state, ATTENTION_TARGET_NEURONS);
    float processing = portia_attention_get_allocation(state, ATTENTION_TARGET_PROCESSING);
    float sensors = portia_attention_get_allocation(state, ATTENTION_TARGET_SENSORS);

    EXPECT_GT(neurons, 0.2f);
    EXPECT_GT(processing, 0.15f);
    EXPECT_GT(sensors, 0.1f);

    // Simulate task completion and decay
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t current_ms = ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;

    portia_attention_decay(state, current_ms);

    // Verify decay occurred
    float neurons_after = portia_attention_get_salience(state, ATTENTION_TARGET_NEURONS);
    EXPECT_LT(neurons_after, 1.0f);

    // Get final statistics
    portia_attention_stats_t stats;
    portia_attention_get_stats(state, &stats);

    EXPECT_GT(stats.salience_updates, 0u);
    EXPECT_GT(stats.reallocations, 0u);

    // Print final state
    portia_attention_print_state(state);

    portia_attention_destroy(state);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
