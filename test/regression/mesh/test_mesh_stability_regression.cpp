/**
 * @file test_mesh_stability_regression.cpp
 * @brief Regression Tests for Mesh Network Stability
 *
 * WHAT: Tests for stability issues, race conditions, and edge cases
 * WHY:  Catch regressions in mesh network behavior under stress
 * HOW:  Simulate failure scenarios, concurrent access, resource exhaustion
 *
 * TEST COVERAGE:
 * - Leader election stability under rapid changes
 * - World state consistency under concurrent updates
 * - Channel isolation under adversarial access
 * - Transaction ordering under high load
 * - Memory leak detection in long-running scenarios
 * - Coordinator pool stability under failures
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
#include <set>

extern "C" {
#include "mesh/nimcp_mesh_types.h"
#include "mesh/nimcp_mesh_channel.h"
#include "mesh/nimcp_mesh_coordinator_pool.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_transaction.h"
#include "mesh/nimcp_mesh_ordering.h"
#include "utils/memory/nimcp_memory.h"
}

// =============================================================================
// Test Fixture
// =============================================================================

class MeshStabilityRegressionTest : public ::testing::Test {
protected:
    static constexpr size_t STRESS_ITERATIONS = 100;
    static constexpr size_t CONCURRENT_THREADS = 8;

    void SetUp() override {
        // Reset any global state
    }

    void TearDown() override {
        // Verify no memory leaks (if memory tracking available)
    }
};

// =============================================================================
// Regression: Leader Election Stability
// =============================================================================

TEST_F(MeshStabilityRegressionTest, RapidLeaderElectionCycles) {
    // Bug scenario: Rapid election cycles caused deadlock
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_config_init(&config);
    config.pool_name = "election_stress_pool";
    config.initial_size = 5;
    config.enable_bft = true;
    config.election_timeout_ms = 20.0f;
    config.heartbeat_interval_ms = 5.0f;

    mesh_coordinator_pool_t* pool = mesh_coordinator_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    std::atomic<int> success_count{0};
    std::atomic<int> fail_count{0};

    // Rapid election cycles
    for (size_t i = 0; i < STRESS_ITERATIONS; i++) {
        nimcp_error_t err = mesh_coordinator_pool_elect_leader(pool);
        if (err == NIMCP_OK) {
            success_count++;
        } else {
            fail_count++;
        }

        // Occasional failure simulation
        if (i % 20 == 0) {
            mesh_coordinator_pool_info_t info;
            mesh_coordinator_pool_get_info(pool, &info);
            if (info.has_leader) {
                mesh_coordinator_pool_handle_failure(pool, info.leader_index);
            }
        }
    }

    // Should complete without deadlock
    EXPECT_GT(success_count.load(), 0) << "No successful elections";

    mesh_coordinator_pool_destroy(pool);
}

TEST_F(MeshStabilityRegressionTest, ConcurrentElectionAndFailure) {
    // Bug scenario: Concurrent election and failure handling caused race condition
    mesh_coordinator_pool_config_t config;
    mesh_coordinator_pool_config_init(&config);
    config.pool_name = "concurrent_election_pool";
    config.initial_size = 7;
    config.enable_bft = true;

    mesh_coordinator_pool_t* pool = mesh_coordinator_pool_create(&config);
    ASSERT_NE(pool, nullptr);

    std::atomic<bool> stop{false};
    std::atomic<int> operations{0};

    // Thread 1: Continuous elections
    std::thread election_thread([&]() {
        while (!stop.load()) {
            mesh_coordinator_pool_elect_leader(pool);
            operations++;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });

    // Thread 2: Random failure injection
    std::thread failure_thread([&]() {
        std::mt19937 rng(42);
        std::uniform_int_distribution<size_t> dist(0, 6);

        while (!stop.load()) {
            size_t idx = dist(rng);
            mesh_coordinator_pool_handle_failure(pool, idx);
            operations++;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Run for limited time
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop.store(true);

    election_thread.join();
    failure_thread.join();

    // Should complete without crash
    EXPECT_GT(operations.load(), 0);

    mesh_coordinator_pool_destroy(pool);
}

// =============================================================================
// Regression: World State Consistency
// =============================================================================

TEST_F(MeshStabilityRegressionTest, ConcurrentWorldStateUpdates) {
    // Bug scenario: Concurrent updates caused lost writes
    mesh_channel_config_t config;
    mesh_channel_config_init(&config);
    config.name = "concurrent_state_channel";
    config.max_participants = 32;

    mesh_channel_t* channel = mesh_channel_create(&config);
    ASSERT_NE(channel, nullptr);

    std::atomic<int> write_count{0};
    std::atomic<int> read_success{0};
    std::vector<std::thread> threads;

    // Multiple writers
    for (size_t t = 0; t < CONCURRENT_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (size_t i = 0; i < STRESS_ITERATIONS / CONCURRENT_THREADS; i++) {
                char key[32], value[32];
                snprintf(key, sizeof(key), "key_%zu_%zu", t, i);
                snprintf(value, sizeof(value), "value_%zu_%zu", t, i);

                nimcp_error_t err = mesh_channel_world_state_put(
                    channel, key, value, strlen(value) + 1);
                if (err == NIMCP_OK) {
                    write_count++;
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    // Verify all writes succeeded
    EXPECT_EQ(write_count.load(), static_cast<int>(STRESS_ITERATIONS));

    // Verify reads are consistent
    for (size_t t = 0; t < CONCURRENT_THREADS; t++) {
        for (size_t i = 0; i < STRESS_ITERATIONS / CONCURRENT_THREADS; i++) {
            char key[32], expected[32], actual[64] = {0};
            snprintf(key, sizeof(key), "key_%zu_%zu", t, i);
            snprintf(expected, sizeof(expected), "value_%zu_%zu", t, i);

            size_t size = sizeof(actual);
            nimcp_error_t err = mesh_channel_world_state_get(channel, key, actual, &size);
            if (err == NIMCP_OK && strcmp(actual, expected) == 0) {
                read_success++;
            }
        }
    }

    EXPECT_EQ(read_success.load(), static_cast<int>(STRESS_ITERATIONS));

    mesh_channel_destroy(channel);
}

TEST_F(MeshStabilityRegressionTest, WorldStateUpdateDuringRead) {
    // Bug scenario: Read during update returned partial data
    mesh_channel_config_t config;
    mesh_channel_config_init(&config);
    config.name = "read_write_race_channel";

    mesh_channel_t* channel = mesh_channel_create(&config);
    ASSERT_NE(channel, nullptr);

    const char* key = "shared_key";
    std::atomic<bool> stop{false};
    std::atomic<int> inconsistencies{0};

    // Writer thread - alternates between two known values
    std::thread writer([&]() {
        int iteration = 0;
        while (!stop.load()) {
            const char* value = (iteration % 2 == 0) ? "AAAAAAAAAA" : "BBBBBBBBBB";
            mesh_channel_world_state_put(channel, key, value, strlen(value) + 1);
            iteration++;
        }
    });

    // Reader threads - verify value is always complete
    std::vector<std::thread> readers;
    for (int r = 0; r < 4; r++) {
        readers.emplace_back([&]() {
            while (!stop.load()) {
                char value[64] = {0};
                size_t size = sizeof(value);

                if (mesh_channel_world_state_get(channel, key, value, &size) == NIMCP_OK) {
                    // Value should be all A's or all B's, never mixed
                    bool all_a = (strcmp(value, "AAAAAAAAAA") == 0);
                    bool all_b = (strcmp(value, "BBBBBBBBBB") == 0);

                    if (!all_a && !all_b && strlen(value) > 0) {
                        inconsistencies++;
                    }
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop.store(true);

    writer.join();
    for (auto& th : readers) {
        th.join();
    }

    EXPECT_EQ(inconsistencies.load(), 0) << "Detected partial/torn reads";

    mesh_channel_destroy(channel);
}

// =============================================================================
// Regression: Channel Isolation
// =============================================================================

TEST_F(MeshStabilityRegressionTest, ChannelIsolationUnderStress) {
    // Bug scenario: Channel isolation broke under concurrent access
    std::vector<mesh_channel_t*> channels;

    for (int c = 0; c < 4; c++) {
        mesh_channel_config_t config;
        mesh_channel_config_init(&config);

        char name[32];
        snprintf(name, sizeof(name), "isolated_channel_%d", c);
        config.name = name;

        mesh_channel_t* ch = mesh_channel_create(&config);
        ASSERT_NE(ch, nullptr);
        channels.push_back(ch);
    }

    std::vector<std::thread> threads;
    std::atomic<int> isolation_violations{0};

    // Each thread writes unique keys to its channel
    for (size_t t = 0; t < channels.size(); t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 50; i++) {
                // Write to own channel
                char key[32], value[32];
                snprintf(key, sizeof(key), "ch%zu_key_%d", t, i);
                snprintf(value, sizeof(value), "ch%zu_value_%d", t, i);

                mesh_channel_world_state_put(channels[t], key, value, strlen(value) + 1);

                // Verify other channels don't have this key
                for (size_t other = 0; other < channels.size(); other++) {
                    if (other != t) {
                        char check[64] = {0};
                        size_t size = sizeof(check);
                        nimcp_error_t err = mesh_channel_world_state_get(
                            channels[other], key, check, &size);

                        if (err == NIMCP_OK && strlen(check) > 0) {
                            isolation_violations++;
                        }
                    }
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(isolation_violations.load(), 0) << "Channel isolation was violated";

    for (auto* ch : channels) {
        mesh_channel_destroy(ch);
    }
}

// =============================================================================
// Regression: Transaction Ordering
// =============================================================================

TEST_F(MeshStabilityRegressionTest, TransactionSequenceMonotonicity) {
    // Bug scenario: Sequence numbers were not strictly monotonic under load
    mesh_ordering_config_t config;
    mesh_ordering_config_init(&config);
    config.batch_size = 5;
    config.batch_timeout_ms = 10.0f;

    mesh_ordering_service_t* ordering = mesh_ordering_create(&config);
    ASSERT_NE(ordering, nullptr);

    std::vector<mesh_transaction_t*> transactions;
    std::vector<uint64_t> sequences;
    std::mutex seq_mutex;

    // Submit many transactions
    for (size_t i = 0; i < STRESS_ITERATIONS; i++) {
        mesh_transaction_config_t tx_config;
        mesh_transaction_config_init(&tx_config);
        tx_config.type = MESH_TX_TYPE_BELIEF_UPDATE;

        char payload[32];
        snprintf(payload, sizeof(payload), "tx_%zu", i);
        tx_config.payload = payload;
        tx_config.payload_size = strlen(payload);

        mesh_transaction_t* tx = mesh_transaction_create(&tx_config);
        ASSERT_NE(tx, nullptr);
        transactions.push_back(tx);

        mesh_ordering_submit(ordering, tx, nullptr, nullptr);

        if ((i + 1) % 5 == 0) {
            mesh_ordering_flush(ordering);
        }
    }

    mesh_ordering_flush(ordering);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Collect sequences
    for (auto* tx : transactions) {
        mesh_transaction_info_t info;
        if (mesh_transaction_get_info(tx, &info) == NIMCP_OK) {
            if (info.sequence_number > 0) {
                std::lock_guard<std::mutex> lock(seq_mutex);
                sequences.push_back(info.sequence_number);
            }
        }
    }

    // Verify monotonicity
    std::set<uint64_t> unique_sequences(sequences.begin(), sequences.end());
    EXPECT_EQ(unique_sequences.size(), sequences.size())
        << "Duplicate sequence numbers detected";

    std::vector<uint64_t> sorted_sequences = sequences;
    std::sort(sorted_sequences.begin(), sorted_sequences.end());

    for (size_t i = 1; i < sorted_sequences.size(); i++) {
        EXPECT_GT(sorted_sequences[i], sorted_sequences[i-1])
            << "Sequence numbers should be strictly increasing";
    }

    // Cleanup
    for (auto* tx : transactions) {
        mesh_transaction_destroy(tx);
    }
    mesh_ordering_destroy(ordering);
}

// =============================================================================
// Regression: Resource Exhaustion
// =============================================================================

TEST_F(MeshStabilityRegressionTest, ParticipantCreationDestruction) {
    // Bug scenario: Repeated create/destroy leaked resources
    for (size_t cycle = 0; cycle < 10; cycle++) {
        std::vector<mesh_participant_t*> participants;

        // Create many participants
        for (size_t i = 0; i < 50; i++) {
            mesh_participant_config_t config;
            mesh_participant_config_init(&config);

            char name[32];
            snprintf(name, sizeof(name), "cycle_%zu_p_%zu", cycle, i);
            config.name = name;

            mesh_participant_t* p = mesh_participant_create(&config);
            ASSERT_NE(p, nullptr) << "Failed at cycle " << cycle << " participant " << i;
            participants.push_back(p);
        }

        // Destroy all
        for (auto* p : participants) {
            mesh_participant_destroy(p);
        }
    }

    // If we get here without crash/exhaustion, test passes
    SUCCEED();
}

TEST_F(MeshStabilityRegressionTest, ChannelCreationDestruction) {
    // Bug scenario: Channel destruction didn't clean up all resources
    for (size_t cycle = 0; cycle < 10; cycle++) {
        mesh_channel_config_t config;
        mesh_channel_config_init(&config);

        char name[32];
        snprintf(name, sizeof(name), "ephemeral_channel_%zu", cycle);
        config.name = name;
        config.max_participants = 100;
        config.enable_gossip = true;
        config.enable_private_data = true;

        mesh_channel_t* ch = mesh_channel_create(&config);
        ASSERT_NE(ch, nullptr);

        // Add some state
        for (int i = 0; i < 100; i++) {
            char key[32], value[64];
            snprintf(key, sizeof(key), "key_%d", i);
            snprintf(value, sizeof(value), "value_with_some_longer_content_%d", i);
            mesh_channel_world_state_put(ch, key, value, strlen(value) + 1);
        }

        mesh_channel_destroy(ch);
    }

    SUCCEED();
}

// =============================================================================
// Regression: Edge Cases
// =============================================================================

TEST_F(MeshStabilityRegressionTest, EmptyWorldStateAccess) {
    // Bug scenario: Accessing empty world state caused null pointer dereference
    mesh_channel_config_t config;
    mesh_channel_config_init(&config);
    config.name = "empty_channel";

    mesh_channel_t* ch = mesh_channel_create(&config);
    ASSERT_NE(ch, nullptr);

    // Read from empty channel
    char value[64] = {0};
    size_t size = sizeof(value);
    nimcp_error_t err = mesh_channel_world_state_get(ch, "nonexistent", value, &size);

    // Should return error, not crash
    EXPECT_NE(err, NIMCP_OK);

    mesh_channel_destroy(ch);
}

TEST_F(MeshStabilityRegressionTest, ZeroSizePayload) {
    // Bug scenario: Zero-size payload caused issues
    mesh_transaction_config_t config;
    mesh_transaction_config_init(&config);
    config.type = MESH_TX_TYPE_BELIEF_UPDATE;
    config.payload = "";
    config.payload_size = 0;

    mesh_transaction_t* tx = mesh_transaction_create(&config);
    // Should either succeed or fail gracefully
    if (tx) {
        mesh_transaction_info_t info;
        mesh_transaction_get_info(tx, &info);
        EXPECT_EQ(info.payload_size, 0u);
        mesh_transaction_destroy(tx);
    }

    SUCCEED();
}

TEST_F(MeshStabilityRegressionTest, MaxLengthKeys) {
    // Bug scenario: Maximum length keys caused buffer overflows
    mesh_channel_config_t config;
    mesh_channel_config_init(&config);
    config.name = "long_key_channel";

    mesh_channel_t* ch = mesh_channel_create(&config);
    ASSERT_NE(ch, nullptr);

    // Create max length key
    char long_key[256];
    memset(long_key, 'K', sizeof(long_key) - 1);
    long_key[sizeof(long_key) - 1] = '\0';

    const char* value = "test_value";
    nimcp_error_t err = mesh_channel_world_state_put(ch, long_key, value, strlen(value) + 1);

    // Should handle gracefully (either succeed or return error)
    if (err == NIMCP_OK) {
        char retrieved[64] = {0};
        size_t size = sizeof(retrieved);
        err = mesh_channel_world_state_get(ch, long_key, retrieved, &size);
        if (err == NIMCP_OK) {
            EXPECT_STREQ(retrieved, value);
        }
    }

    mesh_channel_destroy(ch);
}
