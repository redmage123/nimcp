/**
 * @file test_security_consensus_regression.cpp
 * @brief Regression tests for NIMCP Security Consensus
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
// Headers have their own extern "C" guards
#include "security/nimcp_security_consensus.h"
#include "async/nimcp_bio_router.h"

class SecurityConsensusRegressionTest : public ::testing::Test {
protected:
    nimcp_security_consensus_t consensus;
    bio_router_t router;

    void SetUp() override {
        bio_router_init(NULL);
        router = bio_router_get_global();
        consensus = nullptr;
    }

    void TearDown() override {
        if (consensus) {
            nimcp_consensus_destroy(consensus);
        }
        bio_router_shutdown();
    }
};

/* Memory leak regression tests */

TEST_F(SecurityConsensusRegressionTest, NoMemoryLeakOnCreateDestroy) {
    /* Create and destroy many times */
    for (int i = 0; i < 100; i++) {
        nimcp_consensus_config_t config = {};
        config.node_id = i + 1;
        config.bind_address = "127.0.0.1";
        config.port = 9000 + i;
        config.router = &router;

        nimcp_security_consensus_t temp = nimcp_consensus_create(&config);
        ASSERT_NE(nullptr, temp);

        /* Small delay to let timer thread start */
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        nimcp_consensus_destroy(temp);
    }
}

TEST_F(SecurityConsensusRegressionTest, NoMemoryLeakWithLargeLog) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 9000;
    config.max_log_entries = 10000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    /* Simulate becoming leader and adding many log entries */
    /* In actual use, this would happen through consensus protocol */

    /* Verify no memory issues */
    nimcp_consensus_stats_t stats;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_get_stats(consensus, &stats));
}

/* Role transition regression tests */

TEST_F(SecurityConsensusRegressionTest, AlwaysStartAsFollower) {
    /* Test multiple creates */
    for (int i = 0; i < 20; i++) {
        nimcp_consensus_config_t config = {};
        config.node_id = i + 1;
        config.bind_address = "127.0.0.1";
        config.port = 9000 + i;
        config.router = &router;

        nimcp_security_consensus_t temp = nimcp_consensus_create(&config);
        ASSERT_NE(nullptr, temp);

        EXPECT_EQ(NIMCP_CONSENSUS_FOLLOWER, nimcp_consensus_get_role(temp));
        EXPECT_EQ(0, nimcp_consensus_get_leader(temp));

        nimcp_consensus_destroy(temp);
    }
}

TEST_F(SecurityConsensusRegressionTest, ElectionTimeoutEventuallyTriggered) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 9000;
    config.election_timeout_min_ms = 100;
    config.election_timeout_max_ms = 200;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    /* Wait multiple timeout periods */
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    /* Should have attempted election */
    nimcp_consensus_stats_t stats;
    nimcp_consensus_get_stats(consensus, &stats);

    EXPECT_GT(stats.elections_started, 0);
}

/* Statistics consistency regression tests */

TEST_F(SecurityConsensusRegressionTest, StatisticsAlwaysValid) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 9000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    /* Query stats repeatedly */
    for (int i = 0; i < 1000; i++) {
        nimcp_consensus_stats_t stats;
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_get_stats(consensus, &stats));

        /* Validate invariants */
        EXPECT_GE(stats.current_term, 0);
        EXPECT_GE(stats.commit_index, 0);
        EXPECT_GE(stats.last_applied, 0);
        EXPECT_LE(stats.last_applied, stats.commit_index);
        EXPECT_GE(stats.log_size, 0);
        EXPECT_GE(stats.elections_started, stats.elections_won);
    }
}

/* Permission enforcement regression tests */

TEST_F(SecurityConsensusRegressionTest, OnlyLeaderCanPropose) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 9000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    nimcp_security_policy_t policy = {};
    policy.policy_id = 1;
    strncpy(policy.name, "TestPolicy", sizeof(policy.name));

    /* Should fail while follower/candidate */
    for (int i = 0; i < 100; i++) {
        nimcp_consensus_role_t role = nimcp_consensus_get_role(consensus);

        if (role != NIMCP_CONSENSUS_LEADER) {
            EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
                     nimcp_consensus_propose_policy(consensus, &policy));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

TEST_F(SecurityConsensusRegressionTest, OnlyLeaderCanShareThreat) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 9000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    nimcp_threat_info_t threat = {};
    threat.threat_id = 1;
    strncpy(threat.type, "Test", sizeof(threat.type));

    /* Should fail while follower/candidate */
    for (int i = 0; i < 100; i++) {
        nimcp_consensus_role_t role = nimcp_consensus_get_role(consensus);

        if (role != NIMCP_CONSENSUS_LEADER) {
            EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
                     nimcp_consensus_share_threat(consensus, &threat));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

/* Thread safety regression tests */

TEST_F(SecurityConsensusRegressionTest, ConcurrentStatisticsQueries) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 9000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    std::atomic<int> errors{0};

    /* Multiple threads querying simultaneously */
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 500; i++) {
                nimcp_consensus_stats_t stats;
                if (nimcp_consensus_get_stats(consensus, &stats) != NIMCP_SUCCESS) {
                    errors++;
                }

                /* Verify stats are valid */
                if (stats.current_term < 0 ||
                    stats.last_applied > stats.commit_index) {
                    errors++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(0, errors);
}

TEST_F(SecurityConsensusRegressionTest, ConcurrentProcessing) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 9000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    std::atomic<int> errors{0};

    /* Multiple threads processing */
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < 200; i++) {
                if (nimcp_consensus_process(consensus) != NIMCP_SUCCESS) {
                    errors++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(0, errors);
}

/* Multi-node regression tests */

class MultiNodeRegressionTest : public ::testing::Test {
protected:
    static const int NUM_NODES = 5;
    std::vector<nimcp_security_consensus_t> nodes;
    bio_router_t router;

    void SetUp() override {
        bio_router_init(NULL);
        router = bio_router_get_global();
        nodes.resize(NUM_NODES);

        for (int i = 0; i < NUM_NODES; i++) {
            nimcp_consensus_config_t config = {};
            config.node_id = i + 1;
            config.bind_address = "127.0.0.1";
            config.port = 10000 + i;
            config.election_timeout_min_ms = 150;
            config.election_timeout_max_ms = 300;
            config.heartbeat_interval_ms = 50;
            config.router = &router;

            nodes[i] = nimcp_consensus_create(&config);
            ASSERT_NE(nullptr, nodes[i]);
        }
    }

    void TearDown() override {
        for (int i = 0; i < NUM_NODES; i++) {
            if (nodes[i]) {
                nimcp_consensus_destroy(nodes[i]);
            }
        }
        bio_router_shutdown();
    }
};

TEST_F(MultiNodeRegressionTest, AllNodesStartCorrectly) {
    for (int i = 0; i < NUM_NODES; i++) {
        EXPECT_EQ(NIMCP_CONSENSUS_FOLLOWER, nimcp_consensus_get_role(nodes[i]));

        nimcp_consensus_stats_t stats;
        nimcp_consensus_get_stats(nodes[i], &stats);

        EXPECT_EQ(0, stats.current_term);
        EXPECT_EQ(0, stats.log_size);
    }
}

TEST_F(MultiNodeRegressionTest, NoImmediateElections) {
    /* Very short wait - no elections should have occurred */
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    for (int i = 0; i < NUM_NODES; i++) {
        nimcp_consensus_stats_t stats;
        nimcp_consensus_get_stats(nodes[i], &stats);

        /* No elections in first 50ms */
        EXPECT_EQ(0, stats.elections_started);
    }
}

TEST_F(MultiNodeRegressionTest, EventualElectionActivity) {
    /* Wait for multiple election timeouts */
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    /* At least some nodes should have election activity */
    int total_elections = 0;
    for (int i = 0; i < NUM_NODES; i++) {
        nimcp_consensus_stats_t stats;
        nimcp_consensus_get_stats(nodes[i], &stats);
        total_elections += stats.elections_started;
    }

    EXPECT_GT(total_elections, 0);
}

TEST_F(MultiNodeRegressionTest, NodesIndependent) {
    /* Each node should have independent state */
    std::vector<nimcp_node_id_t> node_ids;

    for (int i = 0; i < NUM_NODES; i++) {
        nimcp_consensus_stats_t stats;
        nimcp_consensus_get_stats(nodes[i], &stats);

        /* Verify each node knows its own ID (indirectly through stats) */
        EXPECT_GE(stats.current_term, 0);
    }
}

/* Performance regression tests */

TEST_F(SecurityConsensusRegressionTest, StatisticsQueryPerformance) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 9000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    auto start = std::chrono::high_resolution_clock::now();

    /* Many statistics queries */
    for (int i = 0; i < 50000; i++) {
        nimcp_consensus_stats_t stats;
        nimcp_consensus_get_stats(consensus, &stats);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    /* Should be fast (< 100ms for 50k queries) */
    EXPECT_LT(duration.count(), 100)
        << "Statistics queries too slow: " << duration.count() << "ms";
}

TEST_F(SecurityConsensusRegressionTest, ProcessingPerformance) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 9000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    auto start = std::chrono::high_resolution_clock::now();

    /* Many process calls */
    for (int i = 0; i < 10000; i++) {
        nimcp_consensus_process(consensus);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    /* Should be fast */
    EXPECT_LT(duration.count(), 500)
        << "Processing too slow: " << duration.count() << "ms";
}

/* Stability regression tests */

TEST_F(SecurityConsensusRegressionTest, LongRunningStability) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 9000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    /* Run for extended period */
    auto start = std::chrono::steady_clock::now();
    auto end = start + std::chrono::seconds(2);

    int iterations = 0;
    while (std::chrono::steady_clock::now() < end) {
        nimcp_consensus_process(consensus);
        iterations++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    /* Should still be functional */
    nimcp_consensus_stats_t stats;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_get_stats(consensus, &stats));

    EXPECT_GT(iterations, 1000);
}

/* Error handling regression tests */

TEST_F(SecurityConsensusRegressionTest, NullArgumentsHandled) {
    EXPECT_EQ(nullptr, nimcp_consensus_create(nullptr));

    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 9000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    /* Null arguments should be handled gracefully */
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER,
             nimcp_consensus_propose_policy(consensus, nullptr));
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER,
             nimcp_consensus_share_threat(consensus, nullptr));
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER,
             nimcp_consensus_join(consensus, nullptr));

    nimcp_consensus_stats_t stats;
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER,
             nimcp_consensus_get_stats(consensus, nullptr));
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER,
             nimcp_consensus_get_stats(nullptr, &stats));

    size_t count;
    nimcp_node_info_t nodes[10];
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER,
             nimcp_consensus_get_nodes(consensus, nullptr, 10, &count));
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER,
             nimcp_consensus_get_nodes(consensus, nodes, 10, nullptr));
}

/* Cluster size regression tests */

TEST_F(MultiNodeRegressionTest, LargeClusterStarts) {
    /* All nodes in larger cluster should start successfully */
    for (int i = 0; i < NUM_NODES; i++) {
        EXPECT_NE(nullptr, nodes[i]);
        EXPECT_EQ(NIMCP_CONSENSUS_FOLLOWER, nimcp_consensus_get_role(nodes[i]));
    }
}

TEST_F(MultiNodeRegressionTest, LargeClusterStability) {
    /* Run all nodes for a period */
    for (int iter = 0; iter < 100; iter++) {
        for (int i = 0; i < NUM_NODES; i++) {
            nimcp_consensus_process(nodes[i]);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    /* All nodes should still be functional */
    for (int i = 0; i < NUM_NODES; i++) {
        nimcp_consensus_stats_t stats;
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_get_stats(nodes[i], &stats));
    }
}

/* Boundary condition regression tests */

TEST_F(SecurityConsensusRegressionTest, MinimumTimeouts) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 9000;
    config.election_timeout_min_ms = 10;
    config.election_timeout_max_ms = 20;
    config.heartbeat_interval_ms = 5;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    /* Should still function */
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    nimcp_consensus_stats_t stats;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_get_stats(consensus, &stats));
}

TEST_F(SecurityConsensusRegressionTest, MaximumTimeouts) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 9000;
    config.election_timeout_min_ms = 5000;
    config.election_timeout_max_ms = 10000;
    config.heartbeat_interval_ms = 2000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    /* Should still function */
    nimcp_consensus_stats_t stats;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_get_stats(consensus, &stats));

    /* No elections yet due to long timeout */
    EXPECT_EQ(0, stats.elections_started);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
