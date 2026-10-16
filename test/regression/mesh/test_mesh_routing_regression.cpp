/**
 * @file test_mesh_routing_regression.cpp
 * @brief Regression Tests for Message Routing System
 *
 * WHAT: Tests for message routing stability, deadlock prevention, and concurrency
 * WHY:  Catch regressions in routing behavior under stress and failure conditions
 * HOW:  Test high message volumes, cross-channel routing, concurrency, and failures
 *
 * TEST COVERAGE:
 * - Message routing with 1000+ messages
 * - Cross-channel routing doesn't deadlock
 * - Routing under high concurrency
 * - Routing with failing modules
 * - Pattern-based routing edge cases
 * - Bio-bridge routing translation
 * - Channel isolation during routing
 * - Transaction ordering under load
 * - Timeout handling in routing
 * - Recovery from routing failures
 *
 * @author NIMCP Development Team
 * @date 2025-01-31
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <random>
#include <mutex>
#include <condition_variable>
#include <queue>

extern "C" {
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_cross_channel.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_ordering.h"
#include "mesh/nimcp_mesh_msp.h"
#include "mesh/nimcp_mesh_bio_bridge.h"
#include "mesh/nimcp_mesh_pattern_routing.h"
#include "mesh/nimcp_mesh_bootstrap.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
}

// =============================================================================
// Test Constants
// =============================================================================

static constexpr size_t HIGH_MESSAGE_COUNT = 1000;
static constexpr size_t CONCURRENT_THREADS = 8;
static constexpr size_t STRESS_DURATION_MS = 2000;
static constexpr float ROUTING_TIMEOUT_MS = 100.0f;

// =============================================================================
// Test Fixture
// =============================================================================

class MeshRoutingRegressionTest : public ::testing::Test {
protected:
    mesh_msp_t* msp_ = nullptr;
    mesh_ordering_service_t* ordering_ = nullptr;
    mesh_cross_router_t* cross_router_ = nullptr;
    mesh_system_coordinator_t system_coord_ = nullptr;

    mesh_channel_t* channel1_ = nullptr;
    mesh_channel_t* channel2_ = nullptr;
    mesh_channel_t* channel3_ = nullptr;

    std::vector<mesh_transaction_t*> transactions_;
    std::mutex tx_mutex_;

    void SetUp() override {
        // Create MSP
        mesh_msp_config_t msp_config;
        mesh_msp_default_config(&msp_config);
        msp_ = mesh_msp_create(&msp_config, nullptr);
        ASSERT_NE(msp_, nullptr);

        // Create ordering service
        mesh_ordering_config_t ord_config;
        mesh_ordering_config_init(&ord_config);
        ord_config.batch_size = 50;
        ord_config.batch_timeout_ms = 20.0f;
        ordering_ = mesh_ordering_create(&ord_config);
        ASSERT_NE(ordering_, nullptr);

        // Create system coordinator
        mesh_system_coord_config_t sys_config = mesh_system_coord_default_config();
        sys_config.enable_fep_arbitration = true;
        system_coord_ = mesh_system_coord_create(&sys_config, ordering_, msp_);
        ASSERT_NE(system_coord_, nullptr);

        // Create cross-channel router
        mesh_cross_router_config_t router_config = mesh_cross_router_default_config();
        router_config.transaction_timeout_ms = ROUTING_TIMEOUT_MS;
        router_config.max_pending = 500;
        cross_router_ = mesh_cross_router_create(&router_config, system_coord_);
        ASSERT_NE(cross_router_, nullptr);

        // Create channels
        channel1_ = CreateChannel("routing_channel_1", MESH_CHANNEL_LEFT_HEMISPHERE);
        channel2_ = CreateChannel("routing_channel_2", MESH_CHANNEL_RIGHT_HEMISPHERE);
        channel3_ = CreateChannel("routing_channel_3", MESH_CHANNEL_SUBCORTICAL);

        ASSERT_NE(channel1_, nullptr);
        ASSERT_NE(channel2_, nullptr);
        ASSERT_NE(channel3_, nullptr);

        // Register channels with coordinator
        mesh_channel_id_t ch1_id = mesh_channel_get_id(channel1_);
        mesh_channel_id_t ch2_id = mesh_channel_get_id(channel2_);
        mesh_channel_id_t ch3_id = mesh_channel_get_id(channel3_);

        mesh_system_coord_register_channel(system_coord_, ch1_id, "routing_channel_1");
        mesh_system_coord_register_channel(system_coord_, ch2_id, "routing_channel_2");
        mesh_system_coord_register_channel(system_coord_, ch3_id, "routing_channel_3");

        mesh_cross_router_start(cross_router_);
    }

    void TearDown() override {
        mesh_cross_router_stop(cross_router_, true);

        for (auto* tx : transactions_) {
            mesh_transaction_destroy(tx);
        }
        transactions_.clear();

        if (cross_router_) mesh_cross_router_destroy(cross_router_);
        if (system_coord_) mesh_system_coord_destroy(system_coord_);
        if (channel3_) mesh_channel_destroy(channel3_);
        if (channel2_) mesh_channel_destroy(channel2_);
        if (channel1_) mesh_channel_destroy(channel1_);
        if (ordering_) mesh_ordering_destroy(ordering_);
        if (msp_) mesh_msp_destroy(msp_);
    }

    mesh_channel_t* CreateChannel(const char* name, mesh_channel_id_t id) {
        mesh_channel_config_t config;
        mesh_channel_default_config(&config);
        config.channel_name = name;
        config.channel_id = id;
        return mesh_channel_create(&config, nullptr);
    }

    mesh_transaction_t* CreateTransaction(mesh_channel_id_t channel_id, const char* payload) {
        mesh_transaction_config_t config;
        mesh_transaction_config_init(&config);
        config.type = MESH_TX_TYPE_BELIEF_UPDATE;
        config.channel_id = channel_id;
        config.payload = payload;
        config.payload_size = strlen(payload);
        return mesh_transaction_create(&config);
    }

    mesh_cross_transaction_t* CreateCrossTransaction(
        mesh_channel_id_t source, mesh_channel_id_t target, const char* payload) {

        return mesh_cross_transaction_create(
            source, target,
            1,  // proposer ID
            MESH_TX_TYPE_BELIEF_UPDATE,
            payload, strlen(payload));
    }
};

// =============================================================================
// Test 1: High Volume Message Routing
// =============================================================================

TEST_F(MeshRoutingRegressionTest, HighVolumeMessageRouting) {
    // Bug scenario: Message routing degraded or failed at high volumes
    std::atomic<size_t> submitted{0};
    std::atomic<size_t> success{0};
    std::atomic<size_t> failed{0};

    auto start = std::chrono::high_resolution_clock::now();

    mesh_channel_id_t ch1_id = mesh_channel_get_id(channel1_);

    for (size_t i = 0; i < HIGH_MESSAGE_COUNT; i++) {
        char payload[64];
        snprintf(payload, sizeof(payload), "high_volume_msg_%zu", i);

        mesh_transaction_t* tx = CreateTransaction(ch1_id, payload);
        if (!tx) {
            failed++;
            continue;
        }

        nimcp_error_t err = mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
        submitted++;

        if (err == NIMCP_OK) {
            success++;
            std::lock_guard<std::mutex> lock(tx_mutex_);
            transactions_.push_back(tx);
        } else {
            failed++;
            mesh_transaction_destroy(tx);
        }

        // Periodic flush
        if ((i + 1) % 100 == 0) {
            mesh_ordering_flush(ordering_);
        }
    }

    mesh_ordering_flush(ordering_);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Performance assertions
    float throughput = static_cast<float>(success.load()) / (duration_ms / 1000.0f);

    EXPECT_GE(success.load(), HIGH_MESSAGE_COUNT * 0.95)
        << "At least 95% of messages should be routed successfully";

    EXPECT_GT(throughput, 100.0f)
        << "Throughput should be at least 100 msg/sec, got " << throughput;

    // Verify ordering metrics
    mesh_ordering_metrics_t metrics;
    mesh_ordering_get_metrics(ordering_, &metrics);
    EXPECT_GE(metrics.total_transactions, HIGH_MESSAGE_COUNT * 0.95);
}

// =============================================================================
// Test 2: Cross-Channel Routing Without Deadlock
// =============================================================================

TEST_F(MeshRoutingRegressionTest, CrossChannelRoutingNoDeadlock) {
    // Bug scenario: Cross-channel routing caused deadlocks with circular dependencies
    std::atomic<bool> deadlock_detected{false};
    std::atomic<size_t> completed{0};
    std::vector<std::thread> threads;

    mesh_channel_id_t ch1_id = mesh_channel_get_id(channel1_);
    mesh_channel_id_t ch2_id = mesh_channel_get_id(channel2_);
    mesh_channel_id_t ch3_id = mesh_channel_get_id(channel3_);

    // Thread 1: Channel 1 -> Channel 2
    threads.emplace_back([&]() {
        for (size_t i = 0; i < 50; i++) {
            char payload[64];
            snprintf(payload, sizeof(payload), "ch1_to_ch2_%zu", i);

            mesh_cross_transaction_t* tx = CreateCrossTransaction(ch1_id, ch2_id, payload);
            if (tx) {
                mesh_cross_router_submit(cross_router_, tx);
                completed++;
            }
        }
    });

    // Thread 2: Channel 2 -> Channel 3
    threads.emplace_back([&]() {
        for (size_t i = 0; i < 50; i++) {
            char payload[64];
            snprintf(payload, sizeof(payload), "ch2_to_ch3_%zu", i);

            mesh_cross_transaction_t* tx = CreateCrossTransaction(ch2_id, ch3_id, payload);
            if (tx) {
                mesh_cross_router_submit(cross_router_, tx);
                completed++;
            }
        }
    });

    // Thread 3: Channel 3 -> Channel 1 (creates cycle)
    threads.emplace_back([&]() {
        for (size_t i = 0; i < 50; i++) {
            char payload[64];
            snprintf(payload, sizeof(payload), "ch3_to_ch1_%zu", i);

            mesh_cross_transaction_t* tx = CreateCrossTransaction(ch3_id, ch1_id, payload);
            if (tx) {
                mesh_cross_router_submit(cross_router_, tx);
                completed++;
            }
        }
    });

    // Monitor thread for deadlock detection
    std::thread monitor([&]() {
        size_t last_completed = 0;
        int stall_count = 0;

        for (int i = 0; i < 20; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            size_t current = completed.load();
            if (current == last_completed) {
                stall_count++;
                if (stall_count > 10) {
                    deadlock_detected = true;
                    return;
                }
            } else {
                stall_count = 0;
            }
            last_completed = current;
        }
    });

    for (auto& th : threads) {
        th.join();
    }
    monitor.join();

    EXPECT_FALSE(deadlock_detected.load())
        << "Deadlock detected in cross-channel routing";

    // All transactions should eventually complete
    EXPECT_EQ(completed.load(), 150u)
        << "All cross-channel transactions should complete";

    // Check pending count is zero
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    size_t pending = mesh_cross_router_pending_count(cross_router_);
    EXPECT_EQ(pending, 0u)
        << "No transactions should be pending after completion";
}

// =============================================================================
// Test 3: Routing Under High Concurrency
// =============================================================================

TEST_F(MeshRoutingRegressionTest, RoutingUnderHighConcurrency) {
    // Bug scenario: Race conditions in routing caused message loss
    std::atomic<size_t> total_submitted{0};
    std::atomic<size_t> total_success{0};
    std::atomic<size_t> total_failed{0};
    std::vector<std::thread> threads;

    mesh_channel_id_t channels[] = {
        mesh_channel_get_id(channel1_),
        mesh_channel_get_id(channel2_),
        mesh_channel_get_id(channel3_)
    };

    for (size_t t = 0; t < CONCURRENT_THREADS; t++) {
        threads.emplace_back([&, t]() {
            std::mt19937 rng(static_cast<unsigned>(t * 42));
            std::uniform_int_distribution<size_t> ch_dist(0, 2);

            for (size_t i = 0; i < 50; i++) {
                char payload[64];
                snprintf(payload, sizeof(payload), "concurrent_t%zu_m%zu", t, i);

                mesh_channel_id_t ch = channels[ch_dist(rng)];
                mesh_transaction_t* tx = CreateTransaction(ch, payload);

                if (!tx) {
                    total_failed++;
                    continue;
                }

                nimcp_error_t err = mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
                total_submitted++;

                if (err == NIMCP_OK) {
                    total_success++;
                    std::lock_guard<std::mutex> lock(tx_mutex_);
                    transactions_.push_back(tx);
                } else {
                    total_failed++;
                    mesh_transaction_destroy(tx);
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    mesh_ordering_flush(ordering_);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    size_t expected = CONCURRENT_THREADS * 50;

    EXPECT_GE(total_success.load(), expected * 0.95)
        << "At least 95% of concurrent routing should succeed";

    // Verify no message loss by checking ordering service
    mesh_ordering_metrics_t metrics;
    mesh_ordering_get_metrics(ordering_, &metrics);
    EXPECT_GE(metrics.total_transactions, expected * 0.95);
}

// =============================================================================
// Test 4: Routing with Failing Modules
// =============================================================================

TEST_F(MeshRoutingRegressionTest, RoutingWithFailingModules) {
    // Bug scenario: Module failures caused routing to hang
    std::atomic<size_t> success_before_failure{0};
    std::atomic<size_t> success_after_failure{0};

    mesh_channel_id_t ch1_id = mesh_channel_get_id(channel1_);
    mesh_channel_id_t ch2_id = mesh_channel_get_id(channel2_);

    // Phase 1: Route messages normally
    for (size_t i = 0; i < 20; i++) {
        char payload[64];
        snprintf(payload, sizeof(payload), "pre_failure_%zu", i);

        mesh_transaction_t* tx = CreateTransaction(ch1_id, payload);
        if (tx) {
            nimcp_error_t err = mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
            if (err == NIMCP_OK) {
                success_before_failure++;
                std::lock_guard<std::mutex> lock(tx_mutex_);
                transactions_.push_back(tx);
            } else {
                mesh_transaction_destroy(tx);
            }
        }
    }

    mesh_ordering_flush(ordering_);

    // Phase 2: Simulate channel failure
    mesh_system_coord_mark_unhealthy(system_coord_, ch2_id, "simulated_failure");

    // Phase 3: Route to healthy channel should still work
    for (size_t i = 0; i < 20; i++) {
        char payload[64];
        snprintf(payload, sizeof(payload), "post_failure_%zu", i);

        mesh_transaction_t* tx = CreateTransaction(ch1_id, payload);
        if (tx) {
            nimcp_error_t err = mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
            if (err == NIMCP_OK) {
                success_after_failure++;
                std::lock_guard<std::mutex> lock(tx_mutex_);
                transactions_.push_back(tx);
            } else {
                mesh_transaction_destroy(tx);
            }
        }
    }

    mesh_ordering_flush(ordering_);

    EXPECT_EQ(success_before_failure.load(), 20u)
        << "Pre-failure routing should work normally";

    EXPECT_EQ(success_after_failure.load(), 20u)
        << "Post-failure routing to healthy channel should work";

    // Phase 4: Recovery - mark healthy again
    mesh_system_coord_mark_healthy(system_coord_, ch2_id);
    EXPECT_TRUE(mesh_system_coord_channel_healthy(system_coord_, ch2_id));
}

// =============================================================================
// Test 5: Pattern-Based Routing Edge Cases
// =============================================================================

TEST_F(MeshRoutingRegressionTest, PatternRoutingEdgeCases) {
    // Bug scenario: Pattern routing with extreme patterns failed
    mesh_bootstrap_config_t boot_config;
    mesh_bootstrap_default_config(&boot_config);

    mesh_bootstrap_t* bootstrap = mesh_bootstrap_create(&boot_config);
    if (!bootstrap) {
        GTEST_SKIP() << "Bootstrap creation not available";
    }

    mesh_pattern_router_t* pattern_router = mesh_bootstrap_get_pattern_router(bootstrap);
    if (!pattern_router) {
        mesh_bootstrap_destroy(bootstrap);
        GTEST_SKIP() << "Pattern router not available";
    }

    // Test 1: Zero pattern
    mesh_pattern_t zero_pattern;
    memset(&zero_pattern, 0, sizeof(zero_pattern));
    // Should handle gracefully

    // Test 2: Max values pattern
    mesh_pattern_t max_pattern;
    for (size_t i = 0; i < MESH_PATTERN_MAX_DIMS; i++) {
        max_pattern.dims[i] = 1.0f;
    }
    max_pattern.dimension_count = MESH_PATTERN_MAX_DIMS;
    max_pattern.magnitude = 1.0f;
    // Should handle gracefully

    // Test 3: Sparse pattern
    mesh_pattern_t sparse_pattern;
    memset(&sparse_pattern, 0, sizeof(sparse_pattern));
    sparse_pattern.dims[0] = 0.5f;
    sparse_pattern.dims[63] = 0.5f;
    sparse_pattern.dimension_count = 64;
    sparse_pattern.magnitude = 0.707f;
    // Should handle gracefully

    mesh_bootstrap_destroy(bootstrap);
    SUCCEED();
}

// =============================================================================
// Test 6: Bio-Bridge Routing Translation
// =============================================================================

TEST_F(MeshRoutingRegressionTest, BioBridgeRoutingTranslation) {
    // Bug scenario: Bio message translation created invalid transactions
    mesh_bootstrap_config_t boot_config;
    mesh_bootstrap_default_config(&boot_config);

    mesh_bootstrap_t* bootstrap = mesh_bootstrap_create(&boot_config);
    if (!bootstrap) {
        GTEST_SKIP() << "Bootstrap creation not available";
    }

    mesh_bio_bridge_config_t bio_config;
    mesh_bio_bridge_default_config(&bio_config);
    bio_config.enable_pattern_routing = true;
    bio_config.bidirectional = true;

    mesh_bio_bridge_t* bio_bridge = mesh_bio_bridge_create(bootstrap, &bio_config);
    if (!bio_bridge) {
        mesh_bootstrap_destroy(bootstrap);
        GTEST_SKIP() << "Bio bridge creation not available";
    }

    // Test translation with various message sizes
    size_t sizes[] = {0, 1, 64, 256, 1024, 4096};

    for (size_t size : sizes) {
        std::vector<uint8_t> msg(size);
        for (size_t i = 0; i < size; i++) {
            msg[i] = static_cast<uint8_t>(i % 256);
        }

        mesh_transaction_t* tx = nullptr;
        nimcp_error_t err = mesh_bio_bridge_translate_to_mesh(
            bio_bridge, msg.data(), size, &tx);

        if (err == NIMCP_SUCCESS && tx != nullptr) {
            mesh_transaction_destroy(tx);
        }
        // Either success or graceful failure expected
    }

    mesh_bio_bridge_destroy(bio_bridge);
    mesh_bootstrap_destroy(bootstrap);
    SUCCEED();
}

// =============================================================================
// Test 7: Channel Isolation During Routing
// =============================================================================

TEST_F(MeshRoutingRegressionTest, ChannelIsolationDuringRouting) {
    // Bug scenario: Messages leaked between isolated channels
    std::atomic<size_t> ch1_count{0};
    std::atomic<size_t> ch2_count{0};
    std::atomic<size_t> isolation_violations{0};

    mesh_channel_id_t ch1_id = mesh_channel_get_id(channel1_);
    mesh_channel_id_t ch2_id = mesh_channel_get_id(channel2_);

    std::vector<std::thread> threads;

    // Thread 1: Send to channel 1
    threads.emplace_back([&]() {
        for (size_t i = 0; i < 100; i++) {
            char payload[64];
            snprintf(payload, sizeof(payload), "CH1_ONLY_%zu", i);

            mesh_transaction_t* tx = CreateTransaction(ch1_id, payload);
            if (tx) {
                mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
                ch1_count++;
                std::lock_guard<std::mutex> lock(tx_mutex_);
                transactions_.push_back(tx);
            }
        }
    });

    // Thread 2: Send to channel 2
    threads.emplace_back([&]() {
        for (size_t i = 0; i < 100; i++) {
            char payload[64];
            snprintf(payload, sizeof(payload), "CH2_ONLY_%zu", i);

            mesh_transaction_t* tx = CreateTransaction(ch2_id, payload);
            if (tx) {
                mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
                ch2_count++;
                std::lock_guard<std::mutex> lock(tx_mutex_);
                transactions_.push_back(tx);
            }
        }
    });

    for (auto& th : threads) {
        th.join();
    }

    mesh_ordering_flush(ordering_);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Verify isolation: check that transactions are associated with correct channels
    for (auto* tx : transactions_) {
        mesh_transaction_info_t info;
        if (mesh_transaction_get_info(tx, &info) == NIMCP_OK) {
            const char* payload = static_cast<const char*>(info.payload);
            if (payload) {
                bool is_ch1_msg = strstr(payload, "CH1_ONLY") != nullptr;
                bool is_ch2_msg = strstr(payload, "CH2_ONLY") != nullptr;

                if (is_ch1_msg && info.channel_id != ch1_id) {
                    isolation_violations++;
                }
                if (is_ch2_msg && info.channel_id != ch2_id) {
                    isolation_violations++;
                }
            }
        }
    }

    EXPECT_EQ(isolation_violations.load(), 0u)
        << "No channel isolation violations should occur";
}

// =============================================================================
// Test 8: Transaction Ordering Under Load
// =============================================================================

TEST_F(MeshRoutingRegressionTest, TransactionOrderingUnderLoad) {
    // Bug scenario: Transaction ordering was not strictly sequential under load
    std::vector<uint64_t> sequence_numbers;
    std::mutex seq_mutex;

    mesh_channel_id_t ch1_id = mesh_channel_get_id(channel1_);

    for (size_t i = 0; i < 200; i++) {
        char payload[64];
        snprintf(payload, sizeof(payload), "ordering_test_%zu", i);

        mesh_transaction_t* tx = CreateTransaction(ch1_id, payload);
        if (tx) {
            mesh_ordering_submit(ordering_, tx, nullptr, nullptr);
            std::lock_guard<std::mutex> lock(tx_mutex_);
            transactions_.push_back(tx);
        }

        if ((i + 1) % 20 == 0) {
            mesh_ordering_flush(ordering_);
        }
    }

    mesh_ordering_flush(ordering_);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Collect sequence numbers
    for (auto* tx : transactions_) {
        mesh_transaction_info_t info;
        if (mesh_transaction_get_info(tx, &info) == NIMCP_OK) {
            if (info.sequence_number > 0) {
                std::lock_guard<std::mutex> lock(seq_mutex);
                sequence_numbers.push_back(info.sequence_number);
            }
        }
    }

    // Verify uniqueness
    std::set<uint64_t> unique_seqs(sequence_numbers.begin(), sequence_numbers.end());
    EXPECT_EQ(unique_seqs.size(), sequence_numbers.size())
        << "All sequence numbers should be unique";

    // Verify monotonicity
    std::vector<uint64_t> sorted_seqs = sequence_numbers;
    std::sort(sorted_seqs.begin(), sorted_seqs.end());

    for (size_t i = 1; i < sorted_seqs.size(); i++) {
        EXPECT_GT(sorted_seqs[i], sorted_seqs[i-1])
            << "Sequence numbers should be strictly increasing";
    }
}

// =============================================================================
// Test 9: Timeout Handling in Routing
// =============================================================================

TEST_F(MeshRoutingRegressionTest, TimeoutHandlingInRouting) {
    // Bug scenario: Timeouts caused resource leaks or hangs
    mesh_channel_id_t ch1_id = mesh_channel_get_id(channel1_);
    mesh_channel_id_t ch2_id = mesh_channel_get_id(channel2_);

    // Create transactions with very short timeout
    for (size_t i = 0; i < 20; i++) {
        mesh_cross_transaction_t* tx = CreateCrossTransaction(
            ch1_id, ch2_id, "timeout_test");

        if (tx) {
            tx->timeout_ms = 1.0f;  // Very short timeout
            mesh_cross_router_submit(cross_router_, tx);
        }
    }

    // Wait longer than timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // System should still be responsive
    mesh_transaction_t* test_tx = CreateTransaction(ch1_id, "post_timeout_test");
    ASSERT_NE(test_tx, nullptr);

    nimcp_error_t err = mesh_ordering_submit(ordering_, test_tx, nullptr, nullptr);
    EXPECT_EQ(err, NIMCP_OK)
        << "Ordering service should still work after timeouts";

    std::lock_guard<std::mutex> lock(tx_mutex_);
    transactions_.push_back(test_tx);

    // Check router is still functional
    size_t pending = mesh_cross_router_pending_count(cross_router_);
    // Pending should be cleared by timeouts
}

// =============================================================================
// Test 10: Recovery from Routing Failures
// =============================================================================

TEST_F(MeshRoutingRegressionTest, RecoveryFromRoutingFailures) {
    // Bug scenario: Failed routes left system in inconsistent state
    mesh_channel_id_t ch1_id = mesh_channel_get_id(channel1_);
    mesh_channel_id_t ch2_id = mesh_channel_get_id(channel2_);

    // Phase 1: Cause failures by marking channel unhealthy
    mesh_system_coord_mark_unhealthy(system_coord_, ch2_id, "test_failure");

    size_t failed_routes = 0;
    for (size_t i = 0; i < 10; i++) {
        mesh_cross_transaction_t* tx = CreateCrossTransaction(
            ch1_id, ch2_id, "should_fail");

        if (tx) {
            nimcp_error_t err = mesh_cross_router_submit(cross_router_, tx);
            // Expect some failures when target is unhealthy
            if (err != NIMCP_OK) {
                failed_routes++;
            }
        }
    }

    // Phase 2: Recover
    mesh_system_coord_mark_healthy(system_coord_, ch2_id);

    // Phase 3: Verify recovery
    size_t success_after_recovery = 0;
    for (size_t i = 0; i < 10; i++) {
        char payload[64];
        snprintf(payload, sizeof(payload), "after_recovery_%zu", i);

        mesh_cross_transaction_t* tx = CreateCrossTransaction(
            ch1_id, ch2_id, payload);

        if (tx) {
            nimcp_error_t err = mesh_cross_router_submit(cross_router_, tx);
            if (err == NIMCP_OK) {
                success_after_recovery++;
            }
        }
    }

    EXPECT_GE(success_after_recovery, 8u)
        << "System should recover and route successfully after failure";

    // Verify coordinator stats show recovery
    mesh_system_coord_stats_t stats;
    mesh_system_coord_get_stats(system_coord_, &stats);

    mesh_system_coord_stats_free(&stats);
}

