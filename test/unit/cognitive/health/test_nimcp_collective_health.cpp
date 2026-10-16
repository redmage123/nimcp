/**
 * @file test_nimcp_collective_health.cpp
 * @brief Unit tests for collective health monitoring system
 *
 * WHAT: Tests for distributed health monitoring across brain instances
 * WHY:  Validate collective anomaly consensus, swarm actions, health aggregation
 * HOW:  Test lifecycle, consensus, summary, swarm actions, statistics
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "cognitive/health/nimcp_collective_health.h"
#include "utils/fault_tolerance/nimcp_health_agent.h"

/*=============================================================================
 * Test Fixture
 *===========================================================================*/

class CollectiveHealthTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = collective_health_default_config();

        // Create health agent for tests (required by collective health)
        health_agent_config_t agent_config;
        nimcp_health_agent_default_config(&agent_config);
        agent_ = nimcp_health_agent_create(&agent_config);
    }

    void TearDown() override {
        if (monitor_) {
            collective_health_monitor_destroy(monitor_);
            monitor_ = nullptr;
        }
        if (agent_) {
            nimcp_health_agent_destroy(agent_);
            agent_ = nullptr;
        }
    }

    collective_health_config_t config_;
    collective_health_monitor_t* monitor_ = nullptr;
    nimcp_health_agent_t* agent_ = nullptr;
};

/*=============================================================================
 * Configuration Tests
 *===========================================================================*/

TEST_F(CollectiveHealthTest, DefaultConfigValues) {
    // Verify default configuration values
    EXPECT_TRUE(config_.enable_hyperscanning);
    EXPECT_TRUE(config_.enable_collective_phi);
    EXPECT_TRUE(config_.enable_shared_goals);
    // enable_we_mode_recovery defaults to false for safety

    EXPECT_FLOAT_EQ(config_.anomaly_consensus_threshold, COLLECTIVE_HEALTH_DEFAULT_ANOMALY_THRESHOLD);
    EXPECT_FLOAT_EQ(config_.recovery_quorum_threshold, COLLECTIVE_HEALTH_DEFAULT_RECOVERY_QUORUM);
    EXPECT_EQ(config_.max_consensus_time_ms, COLLECTIVE_HEALTH_DEFAULT_CONSENSUS_TIMEOUT_MS);

    EXPECT_TRUE(config_.aggregate_health_scores);
    EXPECT_TRUE(config_.share_failure_predictions);
    // propagate_immune_memory defaults to false for safety

    EXPECT_GT(config_.local_weight, 0.0f);
    EXPECT_LE(config_.local_weight, 1.0f);
}

/*=============================================================================
 * Lifecycle Tests
 *===========================================================================*/

TEST_F(CollectiveHealthTest, CreateWithNullAgentReturnsNull) {
    // Creating with null agent returns NULL (agent is required)
    collective_health_monitor_t* m = collective_health_monitor_create(nullptr, nullptr, &config_);
    EXPECT_EQ(m, nullptr);
}

TEST_F(CollectiveHealthTest, CreateWithAgentAndConfig) {
    // Can create with valid agent and config
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);
}

TEST_F(CollectiveHealthTest, CreateWithNullConfig) {
    // Can create with null config (uses defaults)
    monitor_ = collective_health_monitor_create(agent_, nullptr, nullptr);
    ASSERT_NE(monitor_, nullptr);
}

TEST_F(CollectiveHealthTest, DestroyNull) {
    // Should not crash
    collective_health_monitor_destroy(nullptr);
}

TEST_F(CollectiveHealthTest, StartStop) {
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_monitor_start(monitor_), 0);
    EXPECT_TRUE(collective_health_monitor_is_running(monitor_));

    EXPECT_EQ(collective_health_monitor_stop(monitor_), 0);
    EXPECT_FALSE(collective_health_monitor_is_running(monitor_));
}

TEST_F(CollectiveHealthTest, StartNull) {
    EXPECT_EQ(collective_health_monitor_start(nullptr), -1);
}

TEST_F(CollectiveHealthTest, StopNull) {
    EXPECT_EQ(collective_health_monitor_stop(nullptr), -1);
}

TEST_F(CollectiveHealthTest, IsRunningNull) {
    EXPECT_FALSE(collective_health_monitor_is_running(nullptr));
}

TEST_F(CollectiveHealthTest, DoubleStart) {
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_monitor_start(monitor_), 0);
    EXPECT_EQ(collective_health_monitor_start(monitor_), 0);  // Should be idempotent
    EXPECT_TRUE(collective_health_monitor_is_running(monitor_));

    EXPECT_EQ(collective_health_monitor_stop(monitor_), 0);
}

TEST_F(CollectiveHealthTest, DoubleStop) {
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_monitor_start(monitor_), 0);
    EXPECT_EQ(collective_health_monitor_stop(monitor_), 0);
    EXPECT_EQ(collective_health_monitor_stop(monitor_), 0);  // Should be idempotent
}

/*=============================================================================
 * Consensus Tests
 *===========================================================================*/

TEST_F(CollectiveHealthTest, ProposeAnomalyNull) {
    collective_anomaly_proposal_t proposal;
    collective_anomaly_consensus_t consensus;

    collective_health_init_proposal(&proposal);

    EXPECT_EQ(collective_health_propose_anomaly(nullptr, &proposal, &consensus), -1);

    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_propose_anomaly(monitor_, nullptr, &consensus), -1);
    EXPECT_EQ(collective_health_propose_anomaly(monitor_, &proposal, nullptr), -1);
}

TEST_F(CollectiveHealthTest, ProposeAnomalyBasic) {
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_monitor_start(monitor_), 0);

    collective_anomaly_proposal_t proposal;
    collective_health_init_proposal(&proposal);
    proposal.anomaly_type = HEALTH_MSG_MEMORY_CORRUPTION;
    proposal.source = HEALTH_SOURCE_MEMORY;
    proposal.severity = HEALTH_SEVERITY_WARNING;
    proposal.instance_id = 1;
    proposal.local_confidence = 0.85f;

    collective_anomaly_consensus_t consensus;
    memset(&consensus, 0, sizeof(consensus));

    // With no other instances, consensus should be based on local vote only
    int result = collective_health_propose_anomaly(monitor_, &proposal, &consensus);
    EXPECT_EQ(result, 0);
    // Consensus should complete (even if just from local instance)
    EXPECT_GE(consensus.total_instances, 1u);

    EXPECT_EQ(collective_health_monitor_stop(monitor_), 0);
}

TEST_F(CollectiveHealthTest, ProposeAnomalyAsync) {
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_monitor_start(monitor_), 0);

    collective_anomaly_proposal_t proposal;
    collective_health_init_proposal(&proposal);
    proposal.anomaly_type = HEALTH_MSG_DEADLOCK_DETECTED;
    proposal.severity = HEALTH_SEVERITY_CRITICAL;

    uint64_t request_id = 0;
    EXPECT_EQ(collective_health_propose_anomaly_async(monitor_, &proposal, &request_id), 0);
    EXPECT_NE(request_id, 0u);

    // Check status
    collective_anomaly_consensus_t consensus;
    int status = collective_health_check_consensus(monitor_, request_id, &consensus);
    EXPECT_GE(status, 0);  // Either pending (0) or complete (1)

    EXPECT_EQ(collective_health_monitor_stop(monitor_), 0);
}

TEST_F(CollectiveHealthTest, VoteAnomalyNull) {
    collective_anomaly_proposal_t proposal;
    collective_health_init_proposal(&proposal);

    EXPECT_EQ(collective_health_vote_anomaly(nullptr, &proposal, true, 0.9f), -1);

    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_vote_anomaly(monitor_, nullptr, true, 0.9f), -1);
}

/*=============================================================================
 * Health Summary Tests
 *===========================================================================*/

TEST_F(CollectiveHealthTest, GetSummaryNull) {
    collective_health_summary_t summary;

    EXPECT_EQ(collective_health_get_summary(nullptr, &summary), -1);

    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_get_summary(monitor_, nullptr), -1);
}

TEST_F(CollectiveHealthTest, GetSummaryBasic) {
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    collective_health_summary_t summary;
    memset(&summary, 0xFF, sizeof(summary));

    EXPECT_EQ(collective_health_get_summary(monitor_, &summary), 0);

    // Verify summary is populated
    EXPECT_GE(summary.collective_health_score, 0.0f);
    EXPECT_LE(summary.collective_health_score, 1.0f);
    EXPECT_NE(summary.timestamp_us, 0u);
}

TEST_F(CollectiveHealthTest, GetInstanceReportNull) {
    instance_health_report_t report;

    EXPECT_EQ(collective_health_get_instance_report(nullptr, 1, &report), -1);

    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_get_instance_report(monitor_, 1, nullptr), -1);
}

TEST_F(CollectiveHealthTest, GetAllReportsNull) {
    instance_health_report_t reports[8];
    uint32_t num_reports = 0;

    EXPECT_EQ(collective_health_get_all_reports(nullptr, reports, 8, &num_reports), -1);

    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_get_all_reports(monitor_, nullptr, 8, &num_reports), -1);
    EXPECT_EQ(collective_health_get_all_reports(monitor_, reports, 8, nullptr), -1);
}

TEST_F(CollectiveHealthTest, GetAllReportsBasic) {
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    instance_health_report_t reports[COLLECTIVE_HEALTH_MAX_INSTANCES];
    uint32_t num_reports = 0;

    EXPECT_EQ(collective_health_get_all_reports(monitor_, reports, COLLECTIVE_HEALTH_MAX_INSTANCES, &num_reports), 0);
    EXPECT_LE(num_reports, (uint32_t)COLLECTIVE_HEALTH_MAX_INSTANCES);
}

/*=============================================================================
 * Swarm Immune Tests
 *===========================================================================*/

TEST_F(CollectiveHealthTest, RequestSwarmActionNull) {
    swarm_immune_request_t request;
    swarm_immune_response_t response;

    collective_health_init_swarm_request(&request);

    EXPECT_EQ(collective_health_request_swarm_action(nullptr, &request, &response), -1);

    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_request_swarm_action(monitor_, nullptr, &response), -1);
    EXPECT_EQ(collective_health_request_swarm_action(monitor_, &request, nullptr), -1);
}

TEST_F(CollectiveHealthTest, RequestSwarmActionBasic) {
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_monitor_start(monitor_), 0);

    swarm_immune_request_t request;
    collective_health_init_swarm_request(&request);
    request.action = SWARM_IMMUNE_MEMORY_SYNC;
    request.urgency = 0.7f;
    snprintf(request.reason, sizeof(request.reason), "Test memory sync");

    swarm_immune_response_t response;
    memset(&response, 0, sizeof(response));

    EXPECT_EQ(collective_health_request_swarm_action(monitor_, &request, &response), 0);
    EXPECT_GE(response.total_instances, 1u);

    EXPECT_EQ(collective_health_monitor_stop(monitor_), 0);
}

TEST_F(CollectiveHealthTest, RequestSwarmActionAsync) {
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_monitor_start(monitor_), 0);

    swarm_immune_request_t request;
    collective_health_init_swarm_request(&request);
    request.action = SWARM_IMMUNE_COLLECTIVE_GC;

    uint64_t request_id = 0;
    EXPECT_EQ(collective_health_request_swarm_action_async(monitor_, &request, &request_id), 0);
    EXPECT_NE(request_id, 0u);

    swarm_immune_response_t response;
    int status = collective_health_check_swarm_action(monitor_, request_id, &response);
    EXPECT_GE(status, 0);

    EXPECT_EQ(collective_health_monitor_stop(monitor_), 0);
}

/*=============================================================================
 * Hyperscanning Tests
 *===========================================================================*/

TEST_F(CollectiveHealthTest, GetSyncStateNull) {
    hyperscan_state_t state;

    EXPECT_EQ(collective_health_get_sync_state(nullptr, &state), -1);

    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_get_sync_state(monitor_, nullptr), -1);
}

TEST_F(CollectiveHealthTest, ForceSyncNull) {
    EXPECT_EQ(collective_health_force_sync(nullptr), -1);
}

TEST_F(CollectiveHealthTest, ForceSyncBasic) {
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_monitor_start(monitor_), 0);
    EXPECT_EQ(collective_health_force_sync(monitor_), 0);
    EXPECT_EQ(collective_health_monitor_stop(monitor_), 0);
}

/*=============================================================================
 * Threat Detection Tests
 *===========================================================================*/

static int threat_callback_count = 0;
static void test_threat_callback(const collective_threat_t* threat, void* user_data) {
    (void)threat;
    (void)user_data;
    threat_callback_count++;
}

TEST_F(CollectiveHealthTest, RegisterThreatCallbackNull) {
    // Null monitor should fail
    EXPECT_EQ(collective_health_register_threat_callback(nullptr, test_threat_callback, nullptr), -1);

    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    // Null callback is allowed (unregisters the callback)
    EXPECT_EQ(collective_health_register_threat_callback(monitor_, nullptr, nullptr), 0);
}

TEST_F(CollectiveHealthTest, RegisterThreatCallbackBasic) {
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    threat_callback_count = 0;
    EXPECT_EQ(collective_health_register_threat_callback(monitor_, test_threat_callback, nullptr), 0);
}

/*=============================================================================
 * Prediction Sharing Tests
 *===========================================================================*/

TEST_F(CollectiveHealthTest, SharePredictionNull) {
    EXPECT_EQ(collective_health_share_prediction(nullptr, 0.5f, 1000, HEALTH_SOURCE_MEMORY), -1);
}

TEST_F(CollectiveHealthTest, SharePredictionBasic) {
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_monitor_start(monitor_), 0);
    EXPECT_EQ(collective_health_share_prediction(monitor_, 0.3f, 5000, HEALTH_SOURCE_NEURAL), 0);
    EXPECT_EQ(collective_health_monitor_stop(monitor_), 0);
}

TEST_F(CollectiveHealthTest, GetWorstPredictionNull) {
    uint32_t instance_id;
    float probability;
    uint32_t time_to_failure;

    EXPECT_EQ(collective_health_get_worst_prediction(nullptr, &instance_id, &probability, &time_to_failure), -1);

    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_get_worst_prediction(monitor_, nullptr, &probability, &time_to_failure), -1);
}

/*=============================================================================
 * Statistics Tests
 *===========================================================================*/

TEST_F(CollectiveHealthTest, GetStatsNull) {
    collective_health_stats_t stats;

    EXPECT_EQ(collective_health_get_stats(nullptr, &stats), -1);

    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_get_stats(monitor_, nullptr), -1);
}

TEST_F(CollectiveHealthTest, GetStatsBasic) {
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    collective_health_stats_t stats;
    memset(&stats, 0xFF, sizeof(stats));

    EXPECT_EQ(collective_health_get_stats(monitor_, &stats), 0);

    // Initial stats should be zero or positive
    EXPECT_GE(stats.anomalies_proposed, 0u);
    EXPECT_GE(stats.consensus_reached, 0u);
}

TEST_F(CollectiveHealthTest, ResetStatsNull) {
    // Should not crash
    collective_health_reset_stats(nullptr);
}

TEST_F(CollectiveHealthTest, ResetStatsBasic) {
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    collective_health_reset_stats(monitor_);

    collective_health_stats_t stats;
    EXPECT_EQ(collective_health_get_stats(monitor_, &stats), 0);
    EXPECT_EQ(stats.anomalies_proposed, 0u);
    EXPECT_EQ(stats.consensus_reached, 0u);
}

/*=============================================================================
 * Utility Tests
 *===========================================================================*/

TEST_F(CollectiveHealthTest, SwarmImmuneActionName) {
    // All action types should return non-null strings
    for (int i = 0; i < SWARM_IMMUNE_ACTION_COUNT; i++) {
        const char* name = swarm_immune_action_name((swarm_immune_action_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_GT(strlen(name), 0u);
    }

    // Invalid action should still return something
    const char* invalid_name = swarm_immune_action_name((swarm_immune_action_t)999);
    EXPECT_NE(invalid_name, nullptr);
}

TEST_F(CollectiveHealthTest, InitProposalNull) {
    // Should not crash
    collective_health_init_proposal(nullptr);
}

TEST_F(CollectiveHealthTest, InitProposalBasic) {
    collective_anomaly_proposal_t proposal;
    memset(&proposal, 0xFF, sizeof(proposal));

    collective_health_init_proposal(&proposal);

    // Should be initialized to sensible defaults
    EXPECT_EQ(proposal.instance_id, 0u);
    EXPECT_FLOAT_EQ(proposal.local_confidence, 0.5f);  // Default is 0.5
    EXPECT_EQ(proposal.context_size, 0u);
}

TEST_F(CollectiveHealthTest, InitSwarmRequestNull) {
    // Should not crash
    collective_health_init_swarm_request(nullptr);
}

TEST_F(CollectiveHealthTest, InitSwarmRequestBasic) {
    swarm_immune_request_t request;
    memset(&request, 0xFF, sizeof(request));

    collective_health_init_swarm_request(&request);

    EXPECT_EQ(request.action, SWARM_IMMUNE_NONE);
    EXPECT_EQ(request.target_instance_id, 0u);
    EXPECT_FLOAT_EQ(request.urgency, 0.5f);
}

/*=============================================================================
 * Edge Case Tests
 *===========================================================================*/

TEST_F(CollectiveHealthTest, HighUrgencySwarmAction) {
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_monitor_start(monitor_), 0);

    swarm_immune_request_t request;
    collective_health_init_swarm_request(&request);
    request.action = SWARM_IMMUNE_QUARANTINE_INSTANCE;
    request.target_instance_id = 5;
    request.urgency = 1.0f;  // Maximum urgency
    snprintf(request.reason, sizeof(request.reason), "Critical security threat");

    swarm_immune_response_t response;
    EXPECT_EQ(collective_health_request_swarm_action(monitor_, &request, &response), 0);

    EXPECT_EQ(collective_health_monitor_stop(monitor_), 0);
}

TEST_F(CollectiveHealthTest, MultipleConsensusRequests) {
    monitor_ = collective_health_monitor_create(agent_, nullptr, &config_);
    ASSERT_NE(monitor_, nullptr);

    EXPECT_EQ(collective_health_monitor_start(monitor_), 0);

    // Submit multiple async requests
    uint64_t request_ids[4] = {0};
    for (int i = 0; i < 4; i++) {
        collective_anomaly_proposal_t proposal;
        collective_health_init_proposal(&proposal);
        proposal.anomaly_type = (health_agent_msg_type_t)(HEALTH_MSG_ANOMALY_DETECTED + i);
        proposal.instance_id = i + 1;

        EXPECT_EQ(collective_health_propose_anomaly_async(monitor_, &proposal, &request_ids[i]), 0);
        EXPECT_NE(request_ids[i], 0u);
    }

    // All request IDs should be unique
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            EXPECT_NE(request_ids[i], request_ids[j]);
        }
    }

    EXPECT_EQ(collective_health_monitor_stop(monitor_), 0);
}
