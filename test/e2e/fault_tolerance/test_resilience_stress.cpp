/**
 * @file test_resilience_stress.cpp
 * @brief Stress tests for system resilience (Phase 6)
 * @version 1.0.0
 * @date 2026-01-19
 *
 * WHAT: Stress tests for fault tolerance under extreme conditions
 * WHY:  Verify system stability under high load and continuous anomaly injection
 * HOW:  Multi-threaded stress tests, rapid operations, and anomaly simulation
 *
 * Test Scenarios:
 * 1. High-frequency health checks
 * 2. Concurrent multi-threaded operations
 * 3. Rapid connect/disconnect cycles
 * 4. Continuous anomaly injection
 * 5. Memory pressure tests
 * 6. Long-running stability tests
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <atomic>
#include <vector>
#include <random>
#include <mutex>
#include <condition_variable>

// Health agent header
#include "utils/fault_tolerance/nimcp_health_agent.h"

// Brain immune system for full integration
#include "cognitive/immune/nimcp_brain_immune.h"

/* ============================================================================
 * Test Utilities
 * ============================================================================ */

static std::atomic<uint64_t> g_total_operations{0};
static std::atomic<uint64_t> g_successful_operations{0};
static std::atomic<uint64_t> g_failed_operations{0};
static std::atomic<uint64_t> g_anomaly_injections{0};

static void reset_stress_counters() {
    g_total_operations = 0;
    g_successful_operations = 0;
    g_failed_operations = 0;
    g_anomaly_injections = 0;
}

// Mock cognitive modules for stress testing
static void* create_mock_module(int id) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(0x1000 + id * 0x100));
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class ResilienceStressTest : public ::testing::Test {
protected:
    nimcp_health_agent_t* agent = nullptr;
    health_agent_config_t config;

    void SetUp() override {
        reset_stress_counters();
        nimcp_health_agent_default_config(&config);
        strncpy(config.agent_name, "StressTestAgent", sizeof(config.agent_name) - 1);
        config.check_interval_ms = 10;  // Fast checks for stress testing
        config.heartbeat_interval_ms = 50;
        config.watchdog_timeout_ms = 500;
        config.enable_auto_recovery = true;

        agent = nimcp_health_agent_create(&config);
        ASSERT_NE(agent, nullptr) << "Failed to create health agent";
    }

    void TearDown() override {
        if (agent) {
            if (nimcp_health_agent_is_running(agent)) {
                nimcp_health_agent_stop(agent);
            }
            nimcp_health_agent_destroy(agent);
            agent = nullptr;
        }
    }
};

/* ============================================================================
 * Test 1: High-Frequency Health Checks
 * ============================================================================ */

TEST_F(ResilienceStressTest, HighFrequencyHealthChecks) {
    printf("=== Test: High-Frequency Health Checks ===\n");

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    const int NUM_CHECKS = 10000;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_CHECKS; i++) {
        nimcp_health_agent_heartbeat(agent);
        g_total_operations++;

        // Occasional score check
        if (i % 100 == 0) {
            float score = nimcp_health_agent_get_neural_health_score(agent);
            EXPECT_GE(score, 0.0f);
            EXPECT_LE(score, 100.0f);  // Neural health score is 0-100
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    printf("  Completed %d health checks in %ld ms\n", NUM_CHECKS, duration.count());
    printf("  Rate: %.2f checks/second\n", (NUM_CHECKS * 1000.0) / duration.count());

    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_health_agent_get_stats(agent, &stats);
    printf("  Total heartbeats received: %lu\n", (unsigned long)stats.heartbeats_received);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: High-frequency health checks completed\n\n");
}

TEST_F(ResilienceStressTest, BurstHealthChecks) {
    printf("=== Test: Burst Health Checks ===\n");

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    const int NUM_BURSTS = 100;
    const int CHECKS_PER_BURST = 100;

    for (int burst = 0; burst < NUM_BURSTS; burst++) {
        // Burst of rapid checks
        for (int i = 0; i < CHECKS_PER_BURST; i++) {
            nimcp_health_agent_heartbeat(agent);
            g_total_operations++;
        }

        // Brief pause between bursts
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        if (burst % 20 == 0) {
            printf("  Burst %d/%d completed\n", burst + 1, NUM_BURSTS);
        }
    }

    printf("  Total operations: %lu\n", g_total_operations.load());

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Burst health checks completed\n\n");
}

/* ============================================================================
 * Test 2: Concurrent Multi-Threaded Operations
 * ============================================================================ */

TEST_F(ResilienceStressTest, ConcurrentHeartbeats) {
    printf("=== Test: Concurrent Heartbeats ===\n");

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    const int NUM_THREADS = 8;
    const int OPS_PER_THREAD = 1000;
    std::vector<std::thread> threads;
    std::atomic<bool> start_flag{false};

    // Create threads
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &start_flag, OPS_PER_THREAD]() {
            // Wait for start signal
            while (!start_flag) {
                std::this_thread::yield();
            }

            for (int i = 0; i < OPS_PER_THREAD; i++) {
                nimcp_health_agent_heartbeat(agent);
                g_total_operations++;
            }
        });
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    start_flag = true;

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    printf("  %d threads x %d ops = %lu total in %ld ms\n",
           NUM_THREADS, OPS_PER_THREAD, g_total_operations.load(), duration.count());
    printf("  Rate: %.2f ops/second\n", (g_total_operations.load() * 1000.0) / duration.count());

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Concurrent heartbeats completed\n\n");
}

TEST_F(ResilienceStressTest, ConcurrentMixedOperations) {
    printf("=== Test: Concurrent Mixed Operations ===\n");

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    const int NUM_THREADS = 6;
    const int DURATION_MS = 2000;
    std::atomic<bool> running{true};
    std::vector<std::thread> threads;

    // Heartbeat threads
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, &running]() {
            while (running) {
                nimcp_health_agent_heartbeat(agent);
                g_total_operations++;
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    // Score check threads
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, &running]() {
            while (running) {
                float score = nimcp_health_agent_get_neural_health_score(agent);
                (void)score;
                g_total_operations++;
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
        });
    }

    // Stats check threads
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, &running]() {
            while (running) {
                health_agent_stats_t stats;
                memset(&stats, 0, sizeof(stats));
                nimcp_health_agent_get_stats(agent, &stats);
                g_total_operations++;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    // Let it run
    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    running = false;

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    printf("  Total mixed operations: %lu in %d ms\n", g_total_operations.load(), DURATION_MS);
    printf("  Rate: %.2f ops/second\n", (g_total_operations.load() * 1000.0) / DURATION_MS);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Concurrent mixed operations completed\n\n");
}

/* ============================================================================
 * Test 3: Rapid Connect/Disconnect Cycles
 * ============================================================================ */

TEST_F(ResilienceStressTest, RapidModuleConnectDisconnect) {
    printf("=== Test: Rapid Module Connect/Disconnect ===\n");

    const int NUM_CYCLES = 500;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_CYCLES; i++) {
        // Connect modules (reconnecting replaces existing connection)
        nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr);
        nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr);
        nimcp_health_agent_connect_ethics(agent, nullptr, nullptr);
        g_total_operations += 3;

        // Connect world model components (can disconnect these)
        void* mock_jepa = create_mock_module(i);
        nimcp_health_agent_connect_jepa(agent, (jepa_predictor_t*)mock_jepa, nullptr);
        nimcp_health_agent_disconnect_jepa(agent);
        g_total_operations += 2;

        if (i % 100 == 0) {
            printf("  Cycle %d/%d completed\n", i, NUM_CYCLES);
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    printf("  Completed %d cycles (%lu ops) in %ld ms\n",
           NUM_CYCLES, g_total_operations.load(), duration.count());

    printf("Test passed: Rapid connect/disconnect completed\n\n");
}

TEST_F(ResilienceStressTest, ConnectDisconnectWhileRunning) {
    printf("=== Test: Connect/Disconnect While Running ===\n");

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    const int DURATION_MS = 2000;
    std::atomic<bool> running{true};

    // Heartbeat thread
    std::thread heartbeat_thread([this, &running]() {
        while (running) {
            nimcp_health_agent_heartbeat(agent);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Connect/disconnect thread (using JEPA which has disconnect)
    std::thread connect_thread([this, &running]() {
        int i = 0;
        while (running) {
            void* mock_jepa = create_mock_module(i++);
            nimcp_health_agent_connect_jepa(agent, (jepa_predictor_t*)mock_jepa, nullptr);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            nimcp_health_agent_disconnect_jepa(agent);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            g_total_operations += 2;
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    running = false;

    heartbeat_thread.join();
    connect_thread.join();

    printf("  Connect/disconnect operations: %lu\n", g_total_operations.load());

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Connect/disconnect while running completed\n\n");
}

/* ============================================================================
 * Test 4: Continuous Anomaly Injection
 * ============================================================================ */

TEST_F(ResilienceStressTest, ContinuousEmergencyRequests) {
    printf("=== Test: Continuous Emergency Requests ===\n");

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    const int NUM_EMERGENCIES = 100;
    const int HEARTBEATS_BETWEEN = 10;

    for (int i = 0; i < NUM_EMERGENCIES; i++) {
        // Normal heartbeats
        for (int j = 0; j < HEARTBEATS_BETWEEN; j++) {
            nimcp_health_agent_heartbeat(agent);
        }

        // Inject emergency
        char reason[64];
        snprintf(reason, sizeof(reason), "Test emergency %d", i);
        nimcp_health_agent_request_emergency_checkpoint(agent, reason);
        g_anomaly_injections++;

        if (i % 20 == 0) {
            printf("  Injected %d emergencies\n", i);
        }
    }

    printf("  Total emergencies injected: %lu\n", g_anomaly_injections.load());

    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_health_agent_get_stats(agent, &stats);
    printf("  Anomalies detected by agent: %lu\n", (unsigned long)stats.anomalies_detected);

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Continuous emergency requests completed\n\n");
}

TEST_F(ResilienceStressTest, RandomizedAnomalyInjection) {
    printf("=== Test: Randomized Anomaly Injection ===\n");

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> action_dist(0, 3);
    std::uniform_int_distribution<> delay_dist(1, 20);

    const int DURATION_MS = 3000;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (true) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
        if (elapsed.count() >= DURATION_MS) break;

        int action = action_dist(gen);
        switch (action) {
            case 0:
                // Normal heartbeat
                nimcp_health_agent_heartbeat(agent);
                break;
            case 1:
                // Emergency request
                nimcp_health_agent_request_emergency_checkpoint(agent, "Random test");
                g_anomaly_injections++;
                break;
            case 2:
                // Check health score
                nimcp_health_agent_get_neural_health_score(agent);
                break;
            case 3:
                // Get stats
                {
                    health_agent_stats_t stats;
                    memset(&stats, 0, sizeof(stats));
                    nimcp_health_agent_get_stats(agent, &stats);
                }
                break;
        }

        g_total_operations++;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(gen)));
    }

    printf("  Total operations: %lu\n", g_total_operations.load());
    printf("  Anomalies injected: %lu\n", g_anomaly_injections.load());

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Randomized anomaly injection completed\n\n");
}

/* ============================================================================
 * Test 5: Memory Pressure Tests
 * ============================================================================ */

TEST_F(ResilienceStressTest, LargeNumberOfModuleConnections) {
    printf("=== Test: Large Number of Module Connections ===\n");

    // Connect many world model components
    const int NUM_JEPA = 50;
    const int NUM_WM = 50;
    const int NUM_IMAG = 50;

    for (int i = 0; i < NUM_JEPA; i++) {
        void* mock_jepa = create_mock_module(i);
        nimcp_health_agent_connect_jepa(agent, (jepa_predictor_t*)mock_jepa, nullptr);
        g_total_operations++;
    }

    for (int i = 0; i < NUM_WM; i++) {
        void* mock_wm = create_mock_module(100 + i);
        nimcp_health_agent_connect_world_model(agent, (omni_world_model_t*)mock_wm, nullptr);
        g_total_operations++;
    }

    for (int i = 0; i < NUM_IMAG; i++) {
        void* mock_imag = create_mock_module(200 + i);
        nimcp_health_agent_connect_imagination(agent, (imagination_engine_t*)mock_imag, nullptr);
        g_total_operations++;
    }

    printf("  Connected %d components\n", NUM_JEPA + NUM_WM + NUM_IMAG);

    // Start and run
    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    for (int i = 0; i < 100; i++) {
        nimcp_health_agent_heartbeat(agent);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Check health
    float score = nimcp_health_agent_get_neural_health_score(agent);
    printf("  Overall health score: %.4f\n", score);
    EXPECT_GE(score, 0.0f);
    EXPECT_LE(score, 100.0f);  // Neural health score is 0-100

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Large number of connections completed\n\n");
}

/* ============================================================================
 * Test 6: Long-Running Stability Tests
 * ============================================================================ */

TEST_F(ResilienceStressTest, ExtendedOperation) {
    printf("=== Test: Extended Operation (5 seconds) ===\n");

    // Connect modules
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    const int DURATION_SECONDS = 5;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (true) {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
        if (elapsed.count() >= DURATION_SECONDS) break;

        nimcp_health_agent_heartbeat(agent);
        g_total_operations++;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Final health check
    float final_score = nimcp_health_agent_get_neural_health_score(agent);
    printf("  Total operations: %lu\n", g_total_operations.load());
    printf("  Final health score: %.4f\n", final_score);

    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    nimcp_health_agent_get_stats(agent, &stats);
    printf("  Total checks performed: %lu\n", (unsigned long)stats.checks_performed);
    printf("  Total anomalies: %lu\n", (unsigned long)stats.anomalies_detected);

    EXPECT_GE(final_score, 0.0f);
    EXPECT_LE(final_score, 100.0f);  // Neural health score is 0-100

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Extended operation completed\n\n");
}

TEST_F(ResilienceStressTest, StabilityUnderContinuousLoad) {
    printf("=== Test: Stability Under Continuous Load ===\n");

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    const int DURATION_MS = 3000;
    std::atomic<bool> running{true};
    std::vector<float> health_samples;
    std::mutex samples_mutex;

    // Continuous load threads
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &running]() {
            while (running) {
                nimcp_health_agent_heartbeat(agent);
                g_total_operations++;
            }
        });
    }

    // Health monitoring thread
    threads.emplace_back([this, &running, &health_samples, &samples_mutex]() {
        while (running) {
            float score = nimcp_health_agent_get_neural_health_score(agent);
            {
                std::lock_guard<std::mutex> lock(samples_mutex);
                health_samples.push_back(score);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    // Analyze health stability
    float min_score = 1.0f, max_score = 0.0f, avg_score = 0.0f;
    for (float s : health_samples) {
        min_score = std::min(min_score, s);
        max_score = std::max(max_score, s);
        avg_score += s;
    }
    avg_score /= health_samples.size();

    printf("  Total operations: %lu\n", g_total_operations.load());
    printf("  Health samples: %zu\n", health_samples.size());
    printf("  Health score - min: %.4f, max: %.4f, avg: %.4f\n", min_score, max_score, avg_score);
    printf("  Health variance: %.4f\n", max_score - min_score);

    // Health should remain relatively stable
    EXPECT_GE(avg_score, 0.0f);
    EXPECT_LE(avg_score, 100.0f);  // Neural health score is 0-100

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("Test passed: Stability under continuous load completed\n\n");
}

/* ============================================================================
 * Combined Stress Test
 * ============================================================================ */

TEST_F(ResilienceStressTest, FullResilienceStressTest) {
    printf("=== Test: Full Resilience Stress Test ===\n");

    // Connect all available modules
    EXPECT_EQ(nimcp_health_agent_connect_failure_prediction(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_metacognition(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_ethics(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_emotion(agent, nullptr, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_wellbeing(agent, nullptr, nullptr), 0);
    EXPECT_EQ(nimcp_health_agent_connect_mental_health(agent, nullptr), 0);

    EXPECT_EQ(nimcp_health_agent_start(agent), 0);

    const int DURATION_MS = 5000;
    std::atomic<bool> running{true};
    std::random_device rd;

    // Heartbeat threads (high frequency)
    std::vector<std::thread> threads;
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, &running]() {
            while (running) {
                nimcp_health_agent_heartbeat(agent);
                g_total_operations++;
            }
        });
    }

    // Anomaly injection thread
    threads.emplace_back([this, &running, &rd]() {
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> delay_dist(50, 200);
        while (running) {
            nimcp_health_agent_request_emergency_checkpoint(agent, "Stress test");
            g_anomaly_injections++;
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(gen)));
        }
    });

    // Module connect/disconnect thread (using JEPA which has disconnect)
    threads.emplace_back([this, &running]() {
        int i = 0;
        while (running) {
            void* mock_jepa = create_mock_module(i++);
            nimcp_health_agent_connect_jepa(agent, (jepa_predictor_t*)mock_jepa, nullptr);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            nimcp_health_agent_disconnect_jepa(agent);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // Stats monitoring thread
    threads.emplace_back([this, &running]() {
        while (running) {
            health_agent_stats_t stats;
            memset(&stats, 0, sizeof(stats));
            nimcp_health_agent_get_stats(agent, &stats);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });

    // Let it run
    auto start_time = std::chrono::high_resolution_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(DURATION_MS));
    running = false;

    // Wait for all threads
    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Final stats
    health_agent_stats_t final_stats;
    memset(&final_stats, 0, sizeof(final_stats));
    nimcp_health_agent_get_stats(agent, &final_stats);

    printf("\n=== Final Results ===\n");
    printf("  Duration: %ld ms\n", duration.count());
    printf("  Total operations: %lu\n", g_total_operations.load());
    printf("  Anomalies injected: %lu\n", g_anomaly_injections.load());
    printf("  Operations/second: %.2f\n", (g_total_operations.load() * 1000.0) / duration.count());
    printf("  Heartbeats received: %lu\n", (unsigned long)final_stats.heartbeats_received);
    printf("  Checks performed: %lu\n", (unsigned long)final_stats.checks_performed);
    printf("  Anomalies detected: %lu\n", (unsigned long)final_stats.anomalies_detected);
    printf("  Recoveries triggered: %lu\n", (unsigned long)final_stats.recoveries_triggered);

    float final_score = nimcp_health_agent_get_neural_health_score(agent);
    printf("  Final health score: %.4f\n", final_score);

    EXPECT_GE(final_score, 0.0f);
    EXPECT_LE(final_score, 100.0f);  // Neural health score is 0-100

    EXPECT_EQ(nimcp_health_agent_stop(agent), 0);
    printf("\nTest passed: Full resilience stress test completed\n\n");
}

/* ============================================================================
 * Edge Case Stress Tests
 * ============================================================================ */

TEST_F(ResilienceStressTest, StartStopUnderLoad) {
    printf("=== Test: Start/Stop Under Load ===\n");

    const int NUM_CYCLES = 20;
    std::atomic<bool> heartbeat_running{false};
    std::thread heartbeat_thread;

    for (int cycle = 0; cycle < NUM_CYCLES; cycle++) {
        // Start heartbeat thread
        heartbeat_running = true;
        heartbeat_thread = std::thread([this, &heartbeat_running]() {
            while (heartbeat_running) {
                nimcp_health_agent_heartbeat(agent);
                g_total_operations++;
            }
        });

        // Start agent
        EXPECT_EQ(nimcp_health_agent_start(agent), 0);

        // Brief run
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Stop heartbeat
        heartbeat_running = false;
        heartbeat_thread.join();

        // Stop agent
        EXPECT_EQ(nimcp_health_agent_stop(agent), 0);

        if (cycle % 5 == 0) {
            printf("  Cycle %d/%d completed\n", cycle + 1, NUM_CYCLES);
        }
    }

    printf("  Total operations: %lu\n", g_total_operations.load());
    printf("Test passed: Start/stop under load completed\n\n");
}

TEST_F(ResilienceStressTest, NullPointerResilience) {
    printf("=== Test: Null Pointer Resilience ===\n");

    // Test that null pointers don't crash the system
    EXPECT_NE(nimcp_health_agent_start(nullptr), 0);
    EXPECT_NE(nimcp_health_agent_stop(nullptr), 0);

    nimcp_health_agent_heartbeat(nullptr);  // Should not crash

    float score = nimcp_health_agent_get_neural_health_score(nullptr);
    // Just verify it doesn't crash - return value for null may vary
    (void)score;

    health_agent_stats_t stats;
    memset(&stats, 0, sizeof(stats));
    // get_stats returns void, just verify it doesn't crash
    nimcp_health_agent_get_stats(nullptr, &stats);
    nimcp_health_agent_get_stats(agent, nullptr);

    EXPECT_NE(nimcp_health_agent_request_emergency_checkpoint(nullptr, "test"), 0);
    // Note: request with null reason may or may not be an error depending on implementation

    printf("Test passed: Null pointer resilience completed\n\n");
}
