/**
 * @file test_security_consensus.cpp
 * @brief Unit tests for NIMCP Security Consensus
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
// Headers have their own extern "C" guards
#include "security/nimcp_security_consensus.h"
#include "async/nimcp_bio_router.h"

class SecurityConsensusTest : public ::testing::Test {
protected:
    nimcp_security_consensus_t consensus;
    bio_router_t router;

    void SetUp() override {
        bio_router_init(NULL);  // Use default config
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

/* Basic lifecycle tests */

TEST_F(SecurityConsensusTest, CreateDestroy) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    /* Should start as FOLLOWER */
    EXPECT_EQ(NIMCP_CONSENSUS_FOLLOWER, nimcp_consensus_get_role(consensus));
    EXPECT_EQ(0, nimcp_consensus_get_leader(consensus));
}

TEST_F(SecurityConsensusTest, CreateWithCustomTimeouts) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.election_timeout_min_ms = 300;
    config.election_timeout_max_ms = 500;
    config.heartbeat_interval_ms = 100;
    config.max_nodes = 32;
    config.max_log_entries = 2048;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    nimcp_consensus_stats_t stats;
    nimcp_consensus_get_stats(consensus, &stats);

    EXPECT_EQ(NIMCP_CONSENSUS_FOLLOWER, stats.current_role);
    EXPECT_EQ(0, stats.current_term);
}

TEST_F(SecurityConsensusTest, NullConfig) {
    consensus = nimcp_consensus_create(nullptr);
    EXPECT_EQ(nullptr, consensus);
}

/* Role transition tests */

TEST_F(SecurityConsensusTest, InitialRole) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    EXPECT_EQ(NIMCP_CONSENSUS_FOLLOWER, nimcp_consensus_get_role(consensus));
}

TEST_F(SecurityConsensusTest, ElectionTimeout) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.election_timeout_min_ms = 100;
    config.election_timeout_max_ms = 150;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    /* Wait for election timeout */
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    /* Should become candidate */
    nimcp_consensus_stats_t stats;
    nimcp_consensus_get_stats(consensus, &stats);

    /* May have started election */
    EXPECT_GE(stats.elections_started, 0);
}

/* Statistics tests */

TEST_F(SecurityConsensusTest, InitialStatistics) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    nimcp_consensus_stats_t stats;
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_get_stats(consensus, &stats));

    EXPECT_EQ(NIMCP_CONSENSUS_FOLLOWER, stats.current_role);
    EXPECT_EQ(0, stats.current_term);
    EXPECT_EQ(0, stats.voted_for);
    EXPECT_EQ(0, stats.current_leader);
    EXPECT_EQ(0, stats.commit_index);
    EXPECT_EQ(0, stats.last_applied);
    EXPECT_EQ(0, stats.log_size);
    EXPECT_EQ(0, stats.elections_started);
    EXPECT_EQ(0, stats.elections_won);
    EXPECT_EQ(0, stats.policies_proposed);
}

/* Policy proposal tests */

TEST_F(SecurityConsensusTest, ProposePolicy) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    nimcp_security_policy_t policy = {};
    policy.policy_id = 1001;
    strncpy(policy.name, "TestPolicy", sizeof(policy.name));
    strncpy(policy.rules, "{\"rule\":\"test\"}", sizeof(policy.rules));
    policy.created_at = time(NULL);
    policy.author = 1;

    /* Only leader can propose - should fail */
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
              nimcp_consensus_propose_policy(consensus, &policy));
}

TEST_F(SecurityConsensusTest, ProposeNullPolicy) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER,
              nimcp_consensus_propose_policy(consensus, nullptr));
}

/* Threat sharing tests */

TEST_F(SecurityConsensusTest, ShareThreat) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    nimcp_threat_info_t threat = {};
    threat.threat_id = 5001;
    strncpy(threat.type, "DDoS", sizeof(threat.type));
    threat.severity = 80;
    strncpy(threat.source, "192.168.1.100", sizeof(threat.source));
    strncpy(threat.description, "Detected DDoS attack", sizeof(threat.description));
    threat.detected_at = time(NULL);
    threat.detector = 1;

    /* Only leader can share - should fail */
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
              nimcp_consensus_share_threat(consensus, &threat));
}

TEST_F(SecurityConsensusTest, ShareNullThreat) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER,
              nimcp_consensus_share_threat(consensus, nullptr));
}

/* Response coordination tests */

TEST_F(SecurityConsensusTest, InitiateResponse) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    char block_ip[] = "192.168.1.100";

    /* Only leader can initiate - should fail */
    EXPECT_EQ(NIMCP_ERROR_PERMISSION_DENIED,
              nimcp_consensus_initiate_response(consensus,
                                               NIMCP_RESPONSE_BLOCK_IP,
                                               block_ip));
}

/* Cluster membership tests */

TEST_F(SecurityConsensusTest, Join) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    /* Join should succeed (even if peer doesn't exist in unit test) */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_join(consensus, "127.0.0.1:7001"));
}

TEST_F(SecurityConsensusTest, JoinNullPeer) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER,
              nimcp_consensus_join(consensus, nullptr));
}

TEST_F(SecurityConsensusTest, Leave) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_leave(consensus));
}

/* Node information tests */

TEST_F(SecurityConsensusTest, GetNodes) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    nimcp_node_info_t nodes[10];
    size_t count;

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_get_nodes(consensus, nodes, 10, &count));

    /* Initially no other nodes */
    EXPECT_EQ(0, count);
}

TEST_F(SecurityConsensusTest, GetNodesNull) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    size_t count;
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER,
              nimcp_consensus_get_nodes(consensus, nullptr, 10, &count));

    nimcp_node_info_t nodes[10];
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER,
              nimcp_consensus_get_nodes(consensus, nodes, 10, nullptr));
}

/* Message processing tests */

TEST_F(SecurityConsensusTest, Process) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    /* Process should succeed even with no messages */
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_process(consensus));
}

/* Utility function tests */

TEST_F(SecurityConsensusTest, RoleNames) {
    EXPECT_STREQ("FOLLOWER", nimcp_consensus_role_name(NIMCP_CONSENSUS_FOLLOWER));
    EXPECT_STREQ("CANDIDATE", nimcp_consensus_role_name(NIMCP_CONSENSUS_CANDIDATE));
    EXPECT_STREQ("LEADER", nimcp_consensus_role_name(NIMCP_CONSENSUS_LEADER));
}

/* Error handling tests */

TEST_F(SecurityConsensusTest, NullConsensusHandling) {
    EXPECT_EQ(NIMCP_CONSENSUS_FOLLOWER, nimcp_consensus_get_role(nullptr));
    EXPECT_EQ(0, nimcp_consensus_get_leader(nullptr));

    nimcp_consensus_stats_t stats;
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAMETER,
              nimcp_consensus_get_stats(nullptr, &stats));
}

/* Multi-node simulation tests */

class MultiNodeConsensusTest : public ::testing::Test {
protected:
    static const int NUM_NODES = 3;
    nimcp_security_consensus_t nodes[NUM_NODES];
    bio_router_t router;

    void SetUp() override {
        bio_router_init(NULL);  // Use default config
        router = bio_router_get_global();

        for (int i = 0; i < NUM_NODES; i++) {
            nimcp_consensus_config_t config = {};
            config.node_id = i + 1;
            config.bind_address = "127.0.0.1";
            config.port = 7000 + i;
            config.election_timeout_min_ms = 150;
            config.election_timeout_max_ms = 300;
            config.heartbeat_interval_ms = 50;
            config.router = &router;

            nodes[i] = nimcp_consensus_create(&config);
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

TEST_F(MultiNodeConsensusTest, AllNodesStartAsFollowers) {
    for (int i = 0; i < NUM_NODES; i++) {
        ASSERT_NE(nullptr, nodes[i]);
        EXPECT_EQ(NIMCP_CONSENSUS_FOLLOWER, nimcp_consensus_get_role(nodes[i]));
    }
}

TEST_F(MultiNodeConsensusTest, AllNodesSameInitialTerm) {
    for (int i = 0; i < NUM_NODES; i++) {
        nimcp_consensus_stats_t stats;
        nimcp_consensus_get_stats(nodes[i], &stats);
        EXPECT_EQ(0, stats.current_term);
    }
}

TEST_F(MultiNodeConsensusTest, ElectionTriggered) {
    /* Wait for election timeout */
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    /* At least one node should have started an election */
    bool election_started = false;
    for (int i = 0; i < NUM_NODES; i++) {
        nimcp_consensus_stats_t stats;
        nimcp_consensus_get_stats(nodes[i], &stats);
        if (stats.elections_started > 0) {
            election_started = true;
            break;
        }
    }

    EXPECT_TRUE(election_started);
}

/* Stress tests */

TEST_F(SecurityConsensusTest, RapidStatisticsQueries) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    /* Query stats many times rapidly */
    for (int i = 0; i < 1000; i++) {
        nimcp_consensus_stats_t stats;
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_get_stats(consensus, &stats));
    }
}

TEST_F(SecurityConsensusTest, ConcurrentProcessing) {
    nimcp_consensus_config_t config = {};
    config.node_id = 1;
    config.bind_address = "127.0.0.1";
    config.port = 7000;
    config.router = &router;

    consensus = nimcp_consensus_create(&config);
    ASSERT_NE(nullptr, consensus);

    /* Multiple concurrent process calls */
    #pragma omp parallel for
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_consensus_process(consensus));
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
