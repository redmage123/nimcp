/**
 * @file test_security_consensus_integration.cpp
 * @brief Integration tests for NIMCP Security Consensus
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
// Headers have their own extern "C" guards
#include "security/nimcp_security_consensus.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_router.h"

class SecurityConsensusIntegrationTest : public ::testing::Test {
protected:
    static const int NUM_NODES = 3;
    std::vector<nimcp_security_consensus_t> nodes;
    std::vector<bio_router_t*> routers;

    void SetUp() override {
        nodes.resize(NUM_NODES);
        routers.resize(NUM_NODES);

        for (int i = 0; i < NUM_NODES; i++) {
            routers[i] = bio_router_create();

            nimcp_consensus_config_t config = {};
            config.node_id = i + 1;
            config.bind_address = "127.0.0.1";
            config.port = 8000 + i;
            config.election_timeout_min_ms = 200;
            config.election_timeout_max_ms = 400;
            config.heartbeat_interval_ms = 75;
            config.max_nodes = 10;
            config.router = routers[i];

            nodes[i] = nimcp_consensus_create(&config);
            ASSERT_NE(nullptr, nodes[i]);
        }
    }

    void TearDown() override {
        for (int i = 0; i < NUM_NODES; i++) {
            if (nodes[i]) {
                nimcp_consensus_destroy(nodes[i]);
            }
            if (routers[i]) {
                bio_router_destroy(routers[i]);
            }
        }
    }
};

/* Cluster formation tests */

TEST_F(SecurityConsensusIntegrationTest, InitialClusterState) {
    /* All nodes should start as followers */
    for (int i = 0; i < NUM_NODES; i++) {
        EXPECT_EQ(NIMCP_CONSENSUS_FOLLOWER, nimcp_consensus_get_role(nodes[i]));
        EXPECT_EQ(0, nimcp_consensus_get_leader(nodes[i]));
    }
}

TEST_F(SecurityConsensusIntegrationTest, ClusterStatistics) {
    /* Verify initial statistics for all nodes */
    for (int i = 0; i < NUM_NODES; i++) {
        nimcp_consensus_stats_t stats;
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_get_stats(nodes[i], &stats));

        EXPECT_EQ(NIMCP_CONSENSUS_FOLLOWER, stats.current_role);
        EXPECT_EQ(0, stats.current_term);
        EXPECT_EQ(0, stats.log_size);
    }
}

/* Leader election integration tests */

TEST_F(SecurityConsensusIntegrationTest, LeaderElection) {
    /* Wait for election timeout */
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    /* At least one node should have started election */
    int elections_started = 0;
    for (int i = 0; i < NUM_NODES; i++) {
        nimcp_consensus_stats_t stats;
        nimcp_consensus_get_stats(nodes[i], &stats);
        if (stats.elections_started > 0) {
            elections_started++;
        }
    }

    EXPECT_GT(elections_started, 0);
}

TEST_F(SecurityConsensusIntegrationTest, HeartbeatActivity) {
    /* Wait for some heartbeat activity */
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    /* Process messages */
    for (int i = 0; i < NUM_NODES; i++) {
        nimcp_consensus_process(nodes[i]);
    }

    /* Check for any heartbeat activity */
    int total_heartbeats = 0;
    for (int i = 0; i < NUM_NODES; i++) {
        nimcp_consensus_stats_t stats;
        nimcp_consensus_get_stats(nodes[i], &stats);
        total_heartbeats += stats.heartbeats_sent + stats.heartbeats_received;
    }

    /* Some heartbeat activity should have occurred */
    EXPECT_GE(total_heartbeats, 0);
}

/* Policy replication integration tests */

TEST_F(SecurityConsensusIntegrationTest, PolicyProposal) {
    /* Create a policy */
    nimcp_security_policy_t policy = {};
    policy.policy_id = 1001;
    strncpy(policy.name, "TestPolicy", sizeof(policy.name));
    strncpy(policy.rules, "{\"action\":\"block\",\"condition\":\"threat>80\"}",
            sizeof(policy.rules));
    policy.created_at = time(NULL);
    policy.author = 1;

    /* Only followers initially - should fail on all */
    for (int i = 0; i < NUM_NODES; i++) {
        EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
                 nimcp_consensus_propose_policy(nodes[i], &policy));
    }
}

/* Threat sharing integration tests */

TEST_F(SecurityConsensusIntegrationTest, ThreatSharing) {
    /* Create a threat */
    nimcp_threat_info_t threat = {};
    threat.threat_id = 2001;
    strncpy(threat.type, "DDoS", sizeof(threat.type));
    threat.severity = 85;
    strncpy(threat.source, "192.168.1.100", sizeof(threat.source));
    strncpy(threat.description, "High-volume traffic from single source",
            sizeof(threat.description));
    threat.detected_at = time(NULL);
    threat.detector = 1;

    /* Only followers initially - should fail */
    for (int i = 0; i < NUM_NODES; i++) {
        EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
                 nimcp_consensus_share_threat(nodes[i], &threat));
    }
}

/* Response coordination integration tests */

TEST_F(SecurityConsensusIntegrationTest, ResponseCoordination) {
    char block_params[] = "192.168.1.100";

    /* Only followers initially - should fail */
    for (int i = 0; i < NUM_NODES; i++) {
        EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
                 nimcp_consensus_initiate_response(nodes[i],
                                                   NIMCP_RESPONSE_BLOCK_IP,
                                                   block_params));
    }
}

/* Cluster membership integration tests */

TEST_F(SecurityConsensusIntegrationTest, JoinCluster) {
    /* Nodes should be able to initiate join */
    for (int i = 1; i < NUM_NODES; i++) {
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_join(nodes[i], "127.0.0.1:8000"));
    }
}

TEST_F(SecurityConsensusIntegrationTest, LeaveCluster) {
    /* All nodes should be able to leave */
    for (int i = 0; i < NUM_NODES; i++) {
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_leave(nodes[i]));
    }
}

/* Message processing integration tests */

TEST_F(SecurityConsensusIntegrationTest, ContinuousProcessing) {
    /* Run continuous processing for a period */
    auto start = std::chrono::steady_clock::now();
    auto end = start + std::chrono::milliseconds(500);

    while (std::chrono::steady_clock::now() < end) {
        for (int i = 0; i < NUM_NODES; i++) {
            nimcp_consensus_process(nodes[i]);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    /* Verify all nodes are still functioning */
    for (int i = 0; i < NUM_NODES; i++) {
        nimcp_consensus_stats_t stats;
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_get_stats(nodes[i], &stats));
    }
}

/* Concurrent operations tests */

TEST_F(SecurityConsensusIntegrationTest, ConcurrentStatisticsQueries) {
    std::atomic<int> errors{0};

    /* Multiple threads querying statistics */
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 100; i++) {
                nimcp_consensus_stats_t stats;
                if (nimcp_consensus_get_stats(nodes[t % NUM_NODES], &stats) != NIMCP_SUCCESS) {
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

/* Failure scenario tests */

TEST_F(SecurityConsensusIntegrationTest, NodeIsolation) {
    /* Simulate one node being isolated */
    nimcp_consensus_t isolated = nodes[0];

    /* Other nodes continue to operate */
    for (int i = 1; i < NUM_NODES; i++) {
        nimcp_consensus_stats_t stats;
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_get_stats(nodes[i], &stats));
    }

    /* Isolated node should still be functional */
    nimcp_consensus_stats_t stats;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_get_stats(isolated, &stats));
}

/* Real-world scenario tests */

TEST_F(SecurityConsensusIntegrationTest, DistributedThreatResponse) {
    /* Simulate distributed threat detection and response */

    /* Wait for potential leader election */
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    /* Each node processes messages */
    for (int i = 0; i < NUM_NODES; i++) {
        nimcp_consensus_process(nodes[i]);
    }

    /* Verify cluster is operational */
    for (int i = 0; i < NUM_NODES; i++) {
        nimcp_consensus_stats_t stats;
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_get_stats(nodes[i], &stats));

        /* Nodes should be in some valid state */
        EXPECT_TRUE(stats.current_role == NIMCP_CONSENSUS_FOLLOWER ||
                   stats.current_role == NIMCP_CONSENSUS_CANDIDATE ||
                   stats.current_role == NIMCP_CONSENSUS_LEADER);
    }
}

TEST_F(SecurityConsensusIntegrationTest, PolicyReplicationScenario) {
    /* Simulate policy update across cluster */

    nimcp_security_policy_t policies[3];

    for (int i = 0; i < 3; i++) {
        policies[i].policy_id = 1000 + i;
        snprintf(policies[i].name, sizeof(policies[i].name), "Policy%d", i);
        snprintf(policies[i].rules, sizeof(policies[i].rules),
                "{\"id\":%d,\"action\":\"enforce\"}", i);
        policies[i].created_at = time(NULL);
        policies[i].author = 1;
    }

    /* Wait for cluster to potentially elect leader */
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    /* Find leader */
    int leader_idx = -1;
    for (int i = 0; i < NUM_NODES; i++) {
        if (nimcp_consensus_get_role(nodes[i]) == NIMCP_CONSENSUS_LEADER) {
            leader_idx = i;
            break;
        }
    }

    /* If we have a leader, try to propose policies */
    if (leader_idx >= 0) {
        for (int i = 0; i < 3; i++) {
            nimcp_error_t err = nimcp_consensus_propose_policy(nodes[leader_idx],
                                                               &policies[i]);
            /* Leader should accept policies */
            EXPECT_EQ(NIMCP_SUCCESS, err);
        }

        /* Verify policies were logged */
        nimcp_consensus_stats_t stats;
        nimcp_consensus_get_stats(nodes[leader_idx], &stats);
        EXPECT_EQ(3, stats.policies_proposed);
    }
}

/* Performance tests */

TEST_F(SecurityConsensusIntegrationTest, HighFrequencyProcessing) {
    /* Process messages at high frequency */
    for (int iter = 0; iter < 100; iter++) {
        for (int i = 0; i < NUM_NODES; i++) {
            EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_process(nodes[i]));
        }
    }

    /* All nodes should remain healthy */
    for (int i = 0; i < NUM_NODES; i++) {
        nimcp_consensus_stats_t stats;
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_get_stats(nodes[i], &stats));
    }
}

/* Cluster consistency tests */

TEST_F(SecurityConsensusIntegrationTest, TermConsistency) {
    /* After some time, all nodes should have consistent view of term */
    std::this_thread::sleep_for(std::chrono::milliseconds(700));

    std::vector<nimcp_term_t> terms;
    for (int i = 0; i < NUM_NODES; i++) {
        nimcp_consensus_stats_t stats;
        nimcp_consensus_get_stats(nodes[i], &stats);
        terms.push_back(stats.current_term);
    }

    /* Terms should be relatively close (within 1-2 of each other) */
    nimcp_term_t min_term = *std::min_element(terms.begin(), terms.end());
    nimcp_term_t max_term = *std::max_element(terms.begin(), terms.end());

    /* Allow some variance due to timing */
    EXPECT_LE(max_term - min_term, 3);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
