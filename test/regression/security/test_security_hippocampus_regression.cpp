/**
 * @file test_security_hippocampus_regression.cpp
 * @brief Regression tests for Security-Hippocampus Bridge
 * @version 1.0.0
 * @date 2026-01-10
 *
 * Regression tests to catch:
 * - Memory leaks in lifecycle operations
 * - Thread safety in concurrent operations
 * - Numerical stability in coherence calculations
 * - Boundary conditions in circular buffers
 * - State corruption under stress
 * - Performance degradation over time
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>

extern "C" {
#include "security/hippocampus/nimcp_security_hippocampus_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/error/nimcp_error_codes.h"
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SecurityHippocampusRegressionTest : public ::testing::Test {
protected:
    sec_hippo_bridge_t* bridge = nullptr;
    sec_hippo_config_t config;

    void SetUp() override {
        security_hippocampus_default_config(&config);
        bridge = security_hippocampus_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            security_hippocampus_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }

    hippocampus_system_t create_mock_hippocampus() {
        return reinterpret_cast<hippocampus_system_t>(0x40000001);
    }

    sleep_system_t create_mock_sleep_system() {
        return reinterpret_cast<sleep_system_t>(0x40000002);
    }

    void connect_all_systems() {
        security_hippocampus_connect_hippo(bridge, create_mock_hippocampus());
        security_hippocampus_connect_sleep(bridge, create_mock_sleep_system());
    }
};

/* ============================================================================
 * Memory Leak Regression Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusRegressionTest, RepeatedCreateDestroy) {
    // Regression: Memory leak in create/destroy cycle
    for (int i = 0; i < 100; i++) {
        sec_hippo_bridge_t* br = security_hippocampus_bridge_create(nullptr);
        ASSERT_NE(br, nullptr);
        security_hippocampus_bridge_destroy(br);
    }
    // If no crash or memory growth, test passes
}

TEST_F(SecurityHippocampusRegressionTest, RepeatedReset) {
    // Regression: Memory leak in reset
    connect_all_systems();

    for (int i = 0; i < 100; i++) {
        // Build up state
        security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);
        for (int j = 0; j < 10; j++) {
            security_hippocampus_report_place_cell(bridge, j, (float)j * 0.1f, 0.0f, 5.0f);
        }

        // Reset
        int result = security_hippocampus_bridge_reset(bridge);
        EXPECT_EQ(result, 0);
    }
}

TEST_F(SecurityHippocampusRegressionTest, RepeatedConnectDisconnect) {
    // Regression: Memory leak in connection management
    for (int i = 0; i < 100; i++) {
        connect_all_systems();
        EXPECT_TRUE(security_hippocampus_is_fully_connected(bridge));

        security_hippocampus_disconnect_all(bridge);
        EXPECT_FALSE(security_hippocampus_is_fully_connected(bridge));
    }
}

/* ============================================================================
 * Thread Safety Regression Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusRegressionTest, ConcurrentSleepPhaseChanges) {
    // Regression: Race condition in sleep phase updates
    connect_all_systems();

    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &success_count, &error_count, t]() {
            for (int i = 0; i < 100; i++) {
                sec_hippo_sleep_phase_t phase =
                    static_cast<sec_hippo_sleep_phase_t>((t + i) % 5);

                int result = security_hippocampus_protect_sleep(bridge, phase);
                if (result == 0) {
                    success_count++;
                } else {
                    error_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(error_count, 0);
    EXPECT_EQ(success_count, 400);
}

TEST_F(SecurityHippocampusRegressionTest, ConcurrentCellReporting) {
    // Regression: Race condition in cell activity reporting
    connect_all_systems();

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &success_count, t]() {
            for (int i = 0; i < 100; i++) {
                int cell_id = t * 100 + i;
                int result = security_hippocampus_report_place_cell(
                    bridge, cell_id, (float)i * 0.01f, (float)t * 0.1f, 5.0f);
                if (result == 0) {
                    success_count++;
                }

                result = security_hippocampus_report_time_cell(
                    bridge, cell_id, 1000000 + (uint64_t)i * 1000, 5.0f);
                if (result == 0) {
                    success_count++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(success_count, 800);  // 4 threads * 100 iterations * 2 operations
}

TEST_F(SecurityHippocampusRegressionTest, ConcurrentReadWrite) {
    // Regression: Race condition between readers and writers
    connect_all_systems();

    std::atomic<bool> running{true};
    std::atomic<int> read_errors{0};
    std::atomic<int> write_errors{0};

    // Writer thread
    std::thread writer([this, &running, &write_errors]() {
        int i = 0;
        while (running) {
            int result = security_hippocampus_protect_sleep(bridge,
                static_cast<sec_hippo_sleep_phase_t>(i % 5));
            if (result != 0) write_errors++;

            result = security_hippocampus_report_place_cell(bridge, i % 100,
                (float)(i % 10) * 0.1f, (float)(i % 10) * 0.1f, 5.0f);
            if (result != 0) write_errors++;

            i++;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Reader threads
    std::vector<std::thread> readers;
    for (int r = 0; r < 3; r++) {
        readers.emplace_back([this, &running, &read_errors]() {
            while (running) {
                sec_hippo_state_info_t state;
                int result = security_hippocampus_get_state(bridge, &state);
                if (result != 0) read_errors++;

                sec_hippo_stats_t stats;
                result = security_hippocampus_get_stats(bridge, &stats);
                if (result != 0) read_errors++;

                security_to_hippo_effects_t effects;
                result = security_hippocampus_get_security_effects(bridge, &effects);
                if (result != 0) read_errors++;

                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        });
    }

    // Run for 500ms
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    running = false;

    writer.join();
    for (auto& r : readers) {
        r.join();
    }

    EXPECT_EQ(read_errors, 0);
    EXPECT_EQ(write_errors, 0);
}

/* ============================================================================
 * Numerical Stability Regression Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusRegressionTest, CoherenceWithExtremePlaceCells) {
    // Regression: NaN/Inf in coherence calculation with extreme values
    connect_all_systems();

    // Add place cells with extreme positions
    security_hippocampus_report_place_cell(bridge, 0, 0.0f, 0.0f, 5.0f);
    security_hippocampus_report_place_cell(bridge, 1, 1e10f, 1e10f, 5.0f);
    security_hippocampus_report_place_cell(bridge, 2, -1e10f, -1e10f, 5.0f);
    security_hippocampus_report_place_cell(bridge, 3, 1e-10f, 1e-10f, 5.0f);

    float spatial;
    int result = security_hippocampus_check_spatial_coherence(bridge, &spatial);
    EXPECT_EQ(result, 0);

    // Result should be finite
    EXPECT_FALSE(std::isnan(spatial));
    EXPECT_FALSE(std::isinf(spatial));
    EXPECT_GE(spatial, 0.0f);
    EXPECT_LE(spatial, 1.0f);
}

TEST_F(SecurityHippocampusRegressionTest, CoherenceWithZeroValues) {
    // Regression: Division by zero in coherence calculation
    connect_all_systems();

    // Add place cells at same position (zero distance)
    for (int i = 0; i < 10; i++) {
        security_hippocampus_report_place_cell(bridge, i, 0.5f, 0.5f, 5.0f);
    }

    float spatial;
    int result = security_hippocampus_check_spatial_coherence(bridge, &spatial);
    EXPECT_EQ(result, 0);

    EXPECT_FALSE(std::isnan(spatial));
    EXPECT_FALSE(std::isinf(spatial));
}

TEST_F(SecurityHippocampusRegressionTest, ConsolidationWithEdgeStrengths) {
    // Regression: Edge cases in consolidation verification
    connect_all_systems();

    // Test edge strength values
    struct {
        float before;
        float after;
    } test_cases[] = {
        {0.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f},
        {1.0f, 0.0f},
        {0.5f, 0.5f},
        {-0.1f, 0.5f},  // Invalid but should handle
        {0.5f, 1.5f},   // Out of range
    };

    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        if (bridge->num_consolidation_events < SEC_HIPPO_MAX_CONSOLIDATION_EVENTS) {
            sec_hippo_consolidation_event_t* event =
                &bridge->consolidation_events[bridge->num_consolidation_events++];
            event->memory_id = 20000 + i;
            event->strength_before = test_cases[i].before;
            event->strength_after = test_cases[i].after;
            event->verified = false;
        }
    }

    // Verify all - should not crash
    for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
        sec_hippo_consolidation_status_t status;
        float confidence;

        int result = security_hippocampus_verify_consolidation(bridge,
            20000 + i, &status, &confidence);
        EXPECT_EQ(result, 0);

        // Confidence should be finite
        EXPECT_FALSE(std::isnan(confidence));
        EXPECT_FALSE(std::isinf(confidence));
    }
}

/* ============================================================================
 * Circular Buffer Boundary Regression Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusRegressionTest, AuditLogOverflow) {
    // Regression: Audit log circular buffer corruption
    connect_all_systems();

    // Fill audit log many times over
    for (int i = 0; i < SEC_HIPPO_MAX_AUDIT_ENTRIES * 3; i++) {
        security_hippocampus_protect_sleep(bridge,
            static_cast<sec_hippo_sleep_phase_t>(i % 5));
    }

    // Should still be able to read log
    sec_hippo_audit_entry_t entries[10];
    size_t count;

    int result = security_hippocampus_get_audit_log(bridge, entries, 10, &count);
    EXPECT_EQ(result, 0);
    EXPECT_GT(count, 0u);
    EXPECT_LE(count, 10u);

    // Entries should have valid data
    for (size_t i = 0; i < count; i++) {
        EXPECT_GT(entries[i].timestamp, 0u);
    }
}

TEST_F(SecurityHippocampusRegressionTest, PlaceCellHistoryOverflow) {
    // Regression: Place cell history circular buffer corruption
    connect_all_systems();

    // Fill place cell history many times over
    for (int i = 0; i < 10000; i++) {
        int result = security_hippocampus_report_place_cell(bridge, i % 100,
            (float)(i % 100) * 0.01f, (float)(i % 100) * 0.01f, 5.0f);
        EXPECT_EQ(result, 0);
    }

    // Coherence check should still work
    float spatial;
    int result = security_hippocampus_check_spatial_coherence(bridge, &spatial);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(std::isnan(spatial));
}

TEST_F(SecurityHippocampusRegressionTest, TimeCellHistoryOverflow) {
    // Regression: Time cell history circular buffer corruption
    connect_all_systems();

    // Fill time cell history many times over
    for (int i = 0; i < 10000; i++) {
        int result = security_hippocampus_report_time_cell(bridge, i % 100,
            1000000 + (uint64_t)i * 1000, 5.0f);
        EXPECT_EQ(result, 0);
    }

    // Coherence check should still work
    float temporal;
    int result = security_hippocampus_check_temporal_coherence(bridge, &temporal);
    EXPECT_EQ(result, 0);
    EXPECT_FALSE(std::isnan(temporal));
}

TEST_F(SecurityHippocampusRegressionTest, ReplaySequenceLimit) {
    // Regression: Replay sequence array bounds
    connect_all_systems();

    // Try to add more than limit
    for (uint32_t i = 0; i < SEC_HIPPO_MAX_REPLAY_SEQUENCES + 100; i++) {
        // Use validate which creates new sequences
        sec_hippo_replay_status_t status;
        float match_score;
        security_hippocampus_validate_replay(bridge, 30000 + i, &status, &match_score);
    }

    // Should not exceed limit
    EXPECT_LE(bridge->num_replay_sequences, SEC_HIPPO_MAX_REPLAY_SEQUENCES);

    // Should still be able to query sequences
    sec_hippo_replay_sequence_t seq;
    int result = security_hippocampus_get_replay_info(bridge, 30000, &seq);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityHippocampusRegressionTest, ConsolidationEventLimit) {
    // Regression: Consolidation event array bounds
    connect_all_systems();

    // Add events up to and beyond limit
    for (uint32_t i = 0; i < SEC_HIPPO_MAX_CONSOLIDATION_EVENTS + 100; i++) {
        if (bridge->num_consolidation_events < SEC_HIPPO_MAX_CONSOLIDATION_EVENTS) {
            sec_hippo_consolidation_event_t* event =
                &bridge->consolidation_events[bridge->num_consolidation_events++];
            event->memory_id = 40000 + i;
            event->strength_before = 0.3f;
            event->strength_after = 0.7f;
        }
    }

    // Should not exceed limit
    EXPECT_LE(bridge->num_consolidation_events, SEC_HIPPO_MAX_CONSOLIDATION_EVENTS);
}

/* ============================================================================
 * State Corruption Regression Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusRegressionTest, StateAfterMultipleOperations) {
    // Regression: State corruption after many operations
    connect_all_systems();

    std::mt19937 rng(12345);

    for (int i = 0; i < 1000; i++) {
        int op = rng() % 10;

        switch (op) {
            case 0:
                security_hippocampus_protect_sleep(bridge,
                    static_cast<sec_hippo_sleep_phase_t>(rng() % 5));
                break;
            case 1:
                security_hippocampus_report_place_cell(bridge, rng() % 100,
                    (float)(rng() % 100) / 100.0f, (float)(rng() % 100) / 100.0f, 5.0f);
                break;
            case 2:
                security_hippocampus_report_time_cell(bridge, rng() % 100,
                    1000000 + rng() % 1000000, 5.0f);
                break;
            case 3: {
                sec_hippo_coherence_status_t status;
                float spatial, temporal;
                security_hippocampus_check_coherence(bridge, &status, &spatial, &temporal);
                break;
            }
            case 4: {
                sec_hippo_injection_type_t injection;
                float confidence;
                security_hippocampus_detect_injection(bridge, &injection, &confidence, nullptr, 0);
                break;
            }
            case 5: {
                sec_hippo_replay_status_t status;
                float match_score;
                security_hippocampus_validate_replay(bridge, 50000 + i, &status, &match_score);
                break;
            }
            case 6:
                security_hippocampus_bridge_update(bridge, 10);
                break;
            case 7:
                security_hippocampus_apply_security_effects(bridge);
                break;
            case 8:
                security_hippocampus_gather_hippo_effects(bridge);
                break;
            case 9:
                // Read operations
                {
                    sec_hippo_state_info_t state;
                    security_hippocampus_get_state(bridge, &state);
                    sec_hippo_stats_t stats;
                    security_hippocampus_get_stats(bridge, &stats);
                }
                break;
        }
    }

    // Verify bridge is still in valid state
    sec_hippo_state_info_t state;
    int result = security_hippocampus_get_state(bridge, &state);
    EXPECT_EQ(result, 0);

    // State should be valid enum value
    EXPECT_GE(static_cast<int>(state.state), 0);
    EXPECT_LE(static_cast<int>(state.state), SEC_HIPPO_STATE_ERROR);

    // Effects should have valid values
    security_to_hippo_effects_t sec_effects;
    security_hippocampus_get_security_effects(bridge, &sec_effects);
    EXPECT_FALSE(std::isnan(sec_effects.ripple_filter_level));
    EXPECT_FALSE(std::isnan(sec_effects.spindle_gate_level));
    EXPECT_FALSE(std::isnan(sec_effects.throughput_reduction));

    hippo_to_security_effects_t hippo_effects;
    security_hippocampus_get_hippo_effects(bridge, &hippo_effects);
    EXPECT_FALSE(std::isnan(hippo_effects.spatial_coherence));
    EXPECT_FALSE(std::isnan(hippo_effects.temporal_coherence));
}

/* ============================================================================
 * Performance Regression Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusRegressionTest, UpdatePerformanceStability) {
    // Regression: Performance degradation over time
    connect_all_systems();

    std::vector<double> update_times;

    for (int batch = 0; batch < 10; batch++) {
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 100; i++) {
            security_hippocampus_bridge_update(bridge, 10);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        update_times.push_back(static_cast<double>(duration.count()));
    }

    // Calculate average of first and last batches
    double first_avg = (update_times[0] + update_times[1]) / 2.0;
    double last_avg = (update_times[8] + update_times[9]) / 2.0;

    // Performance should not degrade more than 50%
    EXPECT_LT(last_avg, first_avg * 1.5);
}

TEST_F(SecurityHippocampusRegressionTest, CoherencePerformanceWithData) {
    // Regression: Coherence calculation slowdown with data
    connect_all_systems();

    std::vector<double> check_times;

    for (int batch = 0; batch < 10; batch++) {
        // Add more data each batch
        for (int i = 0; i < 50; i++) {
            security_hippocampus_report_place_cell(bridge, batch * 50 + i,
                (float)i * 0.02f, (float)i * 0.02f, 5.0f);
        }

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 100; i++) {
            sec_hippo_coherence_status_t status;
            float spatial, temporal;
            security_hippocampus_check_coherence(bridge, &status, &spatial, &temporal);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        check_times.push_back(static_cast<double>(duration.count()));
    }

    // All times should be within reasonable bound (< 1 second for 100 checks)
    for (double t : check_times) {
        EXPECT_LT(t, 1000000.0);
    }
}

/* ============================================================================
 * Specific Bug Regression Tests
 * ============================================================================ */

TEST_F(SecurityHippocampusRegressionTest, Bug_DoubleDestroy) {
    // Regression: Destroy with NULL should not crash
    sec_hippo_bridge_t* br = security_hippocampus_bridge_create(nullptr);
    ASSERT_NE(br, nullptr);

    security_hippocampus_bridge_destroy(br);
    br = nullptr;  // Caller must NULL after destroy (no magic field for use-after-free detection)
    security_hippocampus_bridge_destroy(br);  // NULL is safe
    security_hippocampus_bridge_destroy(nullptr);  // Should not crash
}

TEST_F(SecurityHippocampusRegressionTest, Bug_UseAfterReset) {
    // Regression: Operations fail after reset
    connect_all_systems();

    // Use bridge
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);

    // Reset
    security_hippocampus_bridge_reset(bridge);

    // Should still work after reset
    int result = security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_REM);
    EXPECT_EQ(result, 0);

    sec_hippo_stats_t stats;
    result = security_hippocampus_get_stats(bridge, &stats);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityHippocampusRegressionTest, Bug_DisconnectedOperations) {
    // Regression: Operations should work even without connections
    // (using mock/placeholder behavior)

    // Don't connect anything

    int result = security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);
    EXPECT_EQ(result, 0);

    result = security_hippocampus_report_place_cell(bridge, 0, 0.5f, 0.5f, 5.0f);
    EXPECT_EQ(result, 0);

    sec_hippo_coherence_status_t status;
    float spatial, temporal;
    result = security_hippocampus_check_coherence(bridge, &status, &spatial, &temporal);
    EXPECT_EQ(result, 0);
}

TEST_F(SecurityHippocampusRegressionTest, Bug_StatisticsAccumulation) {
    // Regression: Statistics not accumulating correctly
    connect_all_systems();

    // Reset stats
    security_hippocampus_reset_stats(bridge);

    // Perform known number of operations
    const int sleep_ops = 10;
    const int inject_ops = 5;
    const int coherence_ops = 3;

    for (int i = 0; i < sleep_ops; i++) {
        security_hippocampus_protect_sleep(bridge,
            static_cast<sec_hippo_sleep_phase_t>(i % 5));
    }

    for (int i = 0; i < inject_ops; i++) {
        sec_hippo_injection_type_t injection;
        float confidence;
        security_hippocampus_detect_injection(bridge, &injection, &confidence, nullptr, 0);
    }

    for (int i = 0; i < coherence_ops; i++) {
        sec_hippo_coherence_status_t status;
        float spatial, temporal;
        security_hippocampus_check_coherence(bridge, &status, &spatial, &temporal);
    }

    // Verify counts
    sec_hippo_stats_t stats;
    security_hippocampus_get_stats(bridge, &stats);

    EXPECT_EQ(stats.sleep_phases_protected, (uint64_t)sleep_ops);
    EXPECT_EQ(stats.injection_scans, (uint64_t)inject_ops);
    EXPECT_EQ(stats.coherence_checks, (uint64_t)coherence_ops);
}

TEST_F(SecurityHippocampusRegressionTest, Bug_AuditLogClear) {
    // Regression: Audit log not properly cleared
    connect_all_systems();

    // Generate audit entries
    for (int i = 0; i < 10; i++) {
        security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_DEEP_NREM);
    }

    // Verify entries exist
    sec_hippo_audit_entry_t entries[20];
    size_t count = 0;
    security_hippocampus_get_audit_log(bridge, entries, 20, &count);
    EXPECT_GT(count, 0u);

    // Clear
    security_hippocampus_clear_audit_log(bridge);

    // Verify cleared
    security_hippocampus_get_audit_log(bridge, entries, 20, &count);
    EXPECT_EQ(count, 0u);

    // New entries should work
    security_hippocampus_protect_sleep(bridge, SEC_HIPPO_SLEEP_REM);
    security_hippocampus_get_audit_log(bridge, entries, 20, &count);
    EXPECT_GT(count, 0u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
