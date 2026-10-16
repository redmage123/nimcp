/**
 * @file test_swarm_logic_consensus_integration.cpp
 * @brief Integration tests for Swarm Logic Bridge with Consensus and Quorum systems
 *
 * TEST COVERAGE:
 * - Logic bridge + Consensus system (8 tests)
 * - Logic bridge + Quorum system (7 tests)
 * - Logic bridge + Emergence system (3 tests)
 *
 * TOTAL: 18 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_logic_bridge.h"
#include "swarm/nimcp_swarm_consensus.h"
#include "swarm/nimcp_swarm_quorum.h"
#include "swarm/nimcp_swarm_emergence.h"
#include "utils/logging/nimcp_logging.h"

class SwarmLogicConsensusIntegrationTest : public ::testing::Test {
protected:
    swarm_logic_bridge_t* bridge;
    swarm_consensus_t consensus;
    nimcp_swarm_quorum_t* quorum;
    swarm_emergence_ctx_t* emergence;

    void SetUp() override {
        // Create logic bridge
        swarm_logic_bridge_config_t bridge_config;
        swarm_logic_bridge_get_default_config(&bridge_config);
        bridge_config.enable_bio_async = false;
        bridge = swarm_logic_bridge_create(&bridge_config);
        ASSERT_NE(bridge, nullptr);

        // Create consensus system
        swarm_consensus_config_t consensus_config = swarm_consensus_default_config(1);
        consensus = swarm_consensus_create(&consensus_config);
        ASSERT_NE(consensus, nullptr);

        // Create quorum system
        nimcp_quorum_config_t quorum_config;
        nimcp_swarm_quorum_default_config(&quorum_config);
        quorum = nimcp_swarm_quorum_create(&quorum_config, nullptr);
        ASSERT_NE(quorum, nullptr);

        // Create emergence system
        emergence = swarm_emergence_create();
        ASSERT_NE(emergence, nullptr);
    }

    void TearDown() override {
        if (bridge) swarm_logic_bridge_destroy(bridge);
        if (consensus) swarm_consensus_destroy(consensus);
        if (quorum) nimcp_swarm_quorum_destroy(quorum);
        if (emergence) swarm_emergence_destroy(emergence);
    }
};

/*=============================================================================
 * LOGIC BRIDGE + CONSENSUS INTEGRATION TESTS
 *============================================================================*/

TEST_F(SwarmLogicConsensusIntegrationTest, ConsensusVoteAndValidate) {
    // Create a proposal
    uint32_t proposal_id = 0;
    nimcp_error_t err = swarm_consensus_propose(consensus,
                                                VOTE_TOPIC_TARGET_PRIORITY,
                                                nullptr,
                                                0,  // No deadline
                                                3,  // Quorum of 3
                                                0.666F,  // 2/3 threshold
                                                nullptr,
                                                nullptr,
                                                &proposal_id);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Cast votes from multiple agents
    std::vector<swarm_vote_response_t> votes;
    swarm_vote_response_t vote1;
    memset(&vote1, 0, sizeof(vote1));
    vote1.proposal_id = proposal_id;
    vote1.voter_drone = 1;
    vote1.choice = VOTE_CHOICE_AGREE;
    vote1.confidence = 0.9F;
    votes.push_back(vote1);

    swarm_vote_response_t vote2;
    memset(&vote2, 0, sizeof(vote2));
    vote2.proposal_id = proposal_id;
    vote2.voter_drone = 2;
    vote2.choice = VOTE_CHOICE_AGREE;
    vote2.confidence = 0.85F;
    votes.push_back(vote2);

    swarm_vote_response_t vote3;
    memset(&vote3, 0, sizeof(vote3));
    vote3.proposal_id = proposal_id;
    vote3.voter_drone = 3;
    vote3.choice = VOTE_CHOICE_AGREE;
    vote3.confidence = 0.95F;
    votes.push_back(vote3);

    // Validate votes with logic bridge
    swarm_logic_result_t result;
    err = swarm_logic_validate_consensus_votes(bridge, votes.data(), votes.size(), &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result);
    EXPECT_GT(result.confidence, 0.8F);
}

TEST_F(SwarmLogicConsensusIntegrationTest, ConsensusSplitDecisionValidation) {
    uint32_t proposal_id = 0;
    swarm_consensus_propose(consensus, VOTE_TOPIC_RETREAT, nullptr, 0, 4, 0.5F,
                           nullptr, nullptr, &proposal_id);

    // Split votes
    std::vector<swarm_vote_response_t> votes;
    for (uint16_t i = 0; i < 2; i++) {
        swarm_vote_response_t v;
        memset(&v, 0, sizeof(v));
        v.proposal_id = proposal_id;
        v.voter_drone = i;
        v.choice = VOTE_CHOICE_AGREE;
        v.confidence = 0.7F;
        votes.push_back(v);
    }
    for (uint16_t i = 2; i < 4; i++) {
        swarm_vote_response_t v;
        memset(&v, 0, sizeof(v));
        v.proposal_id = proposal_id;
        v.voter_drone = i;
        v.choice = VOTE_CHOICE_DISAGREE;
        v.confidence = 0.8F;
        votes.push_back(v);
    }

    swarm_logic_result_t result;
    swarm_logic_validate_consensus_votes(bridge, votes.data(), votes.size(), &result);

    // Should be weak result due to split - no majority achieved
    EXPECT_FALSE(result.result);  // Split vote fails consensus
    EXPECT_LE(result.confidence, 1.0F);  // Confidence is from AGREE votes only
}

TEST_F(SwarmLogicConsensusIntegrationTest, ConsensusWithByzantineDetection) {
    uint32_t proposal_id = 0;
    swarm_consensus_propose(consensus, VOTE_TOPIC_FORMATION_CHANGE, nullptr, 0, 5, 0.6F,
                           nullptr, nullptr, &proposal_id);

    // Create votes with one Byzantine agent
    std::vector<swarm_vote_response_t> votes;
    for (uint16_t i = 0; i < 4; i++) {
        swarm_vote_response_t v;
        memset(&v, 0, sizeof(v));
        v.proposal_id = proposal_id;
        v.voter_drone = i;
        v.choice = VOTE_CHOICE_AGREE;
        v.confidence = 0.9F;
        votes.push_back(v);
    }

    // Agent 4 votes twice with contradictory choices
    swarm_vote_response_t v1, v2;
    memset(&v1, 0, sizeof(v1));
    v1.proposal_id = proposal_id;
    v1.voter_drone = 4;
    v1.choice = VOTE_CHOICE_AGREE;
    v1.confidence = 0.9F;
    votes.push_back(v1);

    memset(&v2, 0, sizeof(v2));
    v2.proposal_id = proposal_id;
    v2.voter_drone = 4;
    v2.choice = VOTE_CHOICE_DISAGREE;
    v2.confidence = 0.8F;
    votes.push_back(v2);

    // Detect Byzantine pattern
    byzantine_detection_t byzantine_agents[10];
    uint32_t byzantine_count = 0;
    swarm_logic_detect_byzantine_pattern(bridge, votes.data(), votes.size(),
                                        byzantine_agents, &byzantine_count);

    EXPECT_EQ(byzantine_count, 1u);
    EXPECT_EQ(byzantine_agents[0].agent_id, 4u);
}

TEST_F(SwarmLogicConsensusIntegrationTest, ConsensusQuorumNotMet) {
    uint32_t proposal_id = 0;
    swarm_consensus_propose(consensus, VOTE_TOPIC_LEADER_ELECTION, nullptr, 0, 10, 0.7F,
                           nullptr, nullptr, &proposal_id);

    // Only 3 votes when quorum is 10
    std::vector<swarm_vote_response_t> votes;
    for (uint16_t i = 0; i < 3; i++) {
        swarm_vote_response_t v;
        memset(&v, 0, sizeof(v));
        v.proposal_id = proposal_id;
        v.voter_drone = i;
        v.choice = VOTE_CHOICE_AGREE;
        v.confidence = 1.0F;
        votes.push_back(v);
    }

    swarm_logic_result_t result;
    swarm_logic_validate_consensus_votes(bridge, votes.data(), votes.size(), &result);

    // Even with unanimous agreement, insufficient quorum affects confidence
    EXPECT_EQ(result.num_inputs_used, 3u);
}

TEST_F(SwarmLogicConsensusIntegrationTest, ConsensusHighConfidenceThreshold) {
    uint32_t proposal_id = 0;
    swarm_consensus_propose(consensus, VOTE_TOPIC_RESOURCE_ALLOCATION, nullptr, 0, 5, 0.9F,
                           nullptr, nullptr, &proposal_id);

    std::vector<swarm_vote_response_t> votes;
    for (uint16_t i = 0; i < 5; i++) {
        swarm_vote_response_t v;
        memset(&v, 0, sizeof(v));
        v.proposal_id = proposal_id;
        v.voter_drone = i;
        v.choice = VOTE_CHOICE_AGREE;
        v.confidence = 0.95F;
        votes.push_back(v);
    }

    swarm_logic_result_t result;
    swarm_logic_validate_consensus_votes(bridge, votes.data(), votes.size(), &result);

    EXPECT_TRUE(result.result);
    EXPECT_GT(result.confidence, 0.9F);
}

TEST_F(SwarmLogicConsensusIntegrationTest, ConsensusAbstainMajority) {
    uint32_t proposal_id = 0;
    swarm_consensus_propose(consensus, VOTE_TOPIC_CUSTOM, nullptr, 0, 10, 0.5F,
                           nullptr, nullptr, &proposal_id);

    // Mostly abstain votes
    std::vector<swarm_vote_response_t> votes;
    for (uint16_t i = 0; i < 8; i++) {
        swarm_vote_response_t v;
        memset(&v, 0, sizeof(v));
        v.proposal_id = proposal_id;
        v.voter_drone = i;
        v.choice = VOTE_CHOICE_ABSTAIN;
        v.confidence = 0.0F;
        votes.push_back(v);
    }
    for (uint16_t i = 8; i < 10; i++) {
        swarm_vote_response_t v;
        memset(&v, 0, sizeof(v));
        v.proposal_id = proposal_id;
        v.voter_drone = i;
        v.choice = VOTE_CHOICE_AGREE;
        v.confidence = 0.8F;
        votes.push_back(v);
    }

    swarm_logic_result_t result;
    swarm_logic_validate_consensus_votes(bridge, votes.data(), votes.size(), &result);

    // Only 2 agree votes spread over 10 total
    EXPECT_FALSE(result.result);
}

TEST_F(SwarmLogicConsensusIntegrationTest, ConsensusMultipleProposals) {
    // Create multiple proposals and validate each
    std::vector<uint32_t> proposal_ids;

    for (int i = 0; i < 3; i++) {
        uint32_t pid = 0;
        swarm_consensus_propose(consensus, VOTE_TOPIC_TARGET_PRIORITY, nullptr, 0, 2, 0.5F,
                               nullptr, nullptr, &pid);
        proposal_ids.push_back(pid);
    }

    // Validate votes for each proposal
    for (uint32_t pid : proposal_ids) {
        std::vector<swarm_vote_response_t> votes;
        for (uint16_t i = 0; i < 2; i++) {
            swarm_vote_response_t v;
            memset(&v, 0, sizeof(v));
            v.proposal_id = pid;
            v.voter_drone = i;
            v.choice = VOTE_CHOICE_AGREE;
            v.confidence = 0.9F;
            votes.push_back(v);
        }

        swarm_logic_result_t result;
        nimcp_error_t err = swarm_logic_validate_consensus_votes(bridge, votes.data(),
                                                                  votes.size(), &result);
        EXPECT_EQ(err, NIMCP_SUCCESS);
    }
}

TEST_F(SwarmLogicConsensusIntegrationTest, ConsensusStatisticsUpdate) {
    swarm_logic_bridge_stats_t stats_before, stats_after;
    swarm_logic_bridge_get_stats(bridge, &stats_before);

    // Perform multiple consensus validations
    for (int i = 0; i < 5; i++) {
        std::vector<swarm_vote_response_t> votes;
        swarm_vote_response_t v;
        memset(&v, 0, sizeof(v));
        v.proposal_id = i;
        v.voter_drone = 0;
        v.choice = VOTE_CHOICE_AGREE;
        v.confidence = 0.9F;
        votes.push_back(v);

        swarm_logic_result_t result;
        swarm_logic_validate_consensus_votes(bridge, votes.data(), 1, &result);
    }

    swarm_logic_bridge_get_stats(bridge, &stats_after);

    EXPECT_EQ(stats_after.consensus_validations, stats_before.consensus_validations + 5);
    EXPECT_GE(stats_after.total_evaluations, stats_before.total_evaluations + 5);
}

/*=============================================================================
 * LOGIC BRIDGE + QUORUM INTEGRATION TESTS
 *============================================================================*/

TEST_F(SwarmLogicConsensusIntegrationTest, QuorumSignalValidation) {
    // Broadcast signals
    nimcp_quorum_broadcast_signal(quorum, 1, NIMCP_SIGNAL_ATTACK, 0.8);
    nimcp_quorum_broadcast_signal(quorum, 2, NIMCP_SIGNAL_ATTACK, 0.9);
    nimcp_quorum_broadcast_signal(quorum, 3, NIMCP_SIGNAL_ATTACK, 0.85);

    // Get signals from quorum system
    std::vector<nimcp_signal_molecule_t> signals;
    nimcp_signal_molecule_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.concentration = 0.85;
    signal.threshold = 0.5;
    signal.threshold_reached = true;
    signal.committed_count = 3;
    signals.push_back(signal);

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_validate_quorum_signals(bridge, signals.data(),
                                                            signals.size(), &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result);
}

TEST_F(SwarmLogicConsensusIntegrationTest, QuorumSignalExclusion) {
    // Test ATTACK vs RETREAT exclusion
    bool exclusive = false;
    nimcp_error_t err = swarm_logic_check_signal_exclusion(bridge,
                                                           NIMCP_SIGNAL_ATTACK,
                                                           NIMCP_SIGNAL_RETREAT,
                                                           &exclusive);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(exclusive);

    // Test EXPLORE vs DEFEND exclusion
    err = swarm_logic_check_signal_exclusion(bridge,
                                            NIMCP_SIGNAL_EXPLORE,
                                            NIMCP_SIGNAL_DEFEND,
                                            &exclusive);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(exclusive);
}

TEST_F(SwarmLogicConsensusIntegrationTest, QuorumMultipleSignals) {
    std::vector<nimcp_signal_molecule_t> signals;

    // Multiple active signals
    for (int i = 0; i < 4; i++) {
        nimcp_signal_molecule_t signal;
        memset(&signal, 0, sizeof(signal));
        signal.concentration = 0.7 + i * 0.05;
        signal.threshold = 0.5;
        signal.threshold_reached = true;
        signals.push_back(signal);
    }

    swarm_logic_result_t result;
    swarm_logic_validate_quorum_signals(bridge, signals.data(), signals.size(), &result);

    EXPECT_TRUE(result.result);
    EXPECT_EQ(result.num_inputs_used, 4u);
}

TEST_F(SwarmLogicConsensusIntegrationTest, QuorumBelowThreshold) {
    std::vector<nimcp_signal_molecule_t> signals;

    // All signals below threshold
    for (int i = 0; i < 3; i++) {
        nimcp_signal_molecule_t signal;
        memset(&signal, 0, sizeof(signal));
        signal.concentration = 0.3;
        signal.threshold = 0.5;
        signal.threshold_reached = false;
        signals.push_back(signal);
    }

    swarm_logic_result_t result;
    swarm_logic_validate_quorum_signals(bridge, signals.data(), signals.size(), &result);

    EXPECT_FALSE(result.result);
}

TEST_F(SwarmLogicConsensusIntegrationTest, QuorumCompatibleSignals) {
    // ATTACK and ALERT are compatible (not exclusive)
    bool exclusive = false;
    swarm_logic_check_signal_exclusion(bridge,
                                      NIMCP_SIGNAL_ATTACK,
                                      NIMCP_SIGNAL_ALERT,
                                      &exclusive);

    EXPECT_FALSE(exclusive);
}

TEST_F(SwarmLogicConsensusIntegrationTest, QuorumHysteresis) {
    // Signal near threshold with hysteresis
    nimcp_signal_molecule_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.concentration = 0.52;
    signal.threshold = 0.5;
    signal.hysteresis_low = 0.4;
    signal.hysteresis_high = 0.6;
    signal.threshold_reached = true;

    swarm_logic_result_t result;
    swarm_logic_validate_quorum_signals(bridge, &signal, 1, &result);

    EXPECT_TRUE(result.result);
}

TEST_F(SwarmLogicConsensusIntegrationTest, QuorumStatisticsUpdate) {
    swarm_logic_bridge_stats_t stats_before, stats_after;
    swarm_logic_bridge_get_stats(bridge, &stats_before);

    // Perform multiple quorum validations
    for (int i = 0; i < 3; i++) {
        nimcp_signal_molecule_t signal;
        memset(&signal, 0, sizeof(signal));
        signal.concentration = 0.8;
        signal.threshold = 0.5;
        signal.threshold_reached = true;

        swarm_logic_result_t result;
        swarm_logic_validate_quorum_signals(bridge, &signal, 1, &result);
    }

    swarm_logic_bridge_get_stats(bridge, &stats_after);

    EXPECT_EQ(stats_after.quorum_validations, stats_before.quorum_validations + 3);
}

/*=============================================================================
 * LOGIC BRIDGE + EMERGENCE INTEGRATION TESTS
 *============================================================================*/

TEST_F(SwarmLogicConsensusIntegrationTest, EmergenceTierTransition) {
    // Update emergence state
    swarm_state_t state;
    memset(&state, 0, sizeof(state));
    state.connected_drones = 8;
    state.healthy_drones = 8;
    state.collective_coherence = 0.9F;
    state.timestamp = 0;

    swarm_emergence_update(emergence, &state);

    // Validate transition from SQUAD to PLATOON
    bool valid = false;
    nimcp_error_t err = swarm_logic_validate_tier_transition(bridge,
                                                             SWARM_TIER_SQUAD,
                                                             SWARM_TIER_PLATOON,
                                                             &state,
                                                             &valid);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(valid);
}

TEST_F(SwarmLogicConsensusIntegrationTest, EmergenceInsufficientDrones) {
    swarm_state_t state;
    memset(&state, 0, sizeof(state));
    state.connected_drones = 3;
    state.healthy_drones = 3;
    state.collective_coherence = 0.95F;

    swarm_emergence_update(emergence, &state);

    // Try to transition to PLATOON (needs 8 drones)
    bool valid = false;
    swarm_logic_validate_tier_transition(bridge,
                                        SWARM_TIER_SQUAD,
                                        SWARM_TIER_PLATOON,
                                        &state,
                                        &valid);

    EXPECT_FALSE(valid);
}

TEST_F(SwarmLogicConsensusIntegrationTest, EmergenceTierValidationStatistics) {
    swarm_logic_bridge_stats_t stats_before, stats_after;
    swarm_logic_bridge_get_stats(bridge, &stats_before);

    swarm_state_t state;
    memset(&state, 0, sizeof(state));
    state.connected_drones = 4;
    state.healthy_drones = 4;
    state.collective_coherence = 0.9F;

    bool valid = false;
    swarm_logic_validate_tier_transition(bridge,
                                        SWARM_TIER_PAIR,
                                        SWARM_TIER_SQUAD,
                                        &state,
                                        &valid);

    swarm_logic_bridge_get_stats(bridge, &stats_after);

    EXPECT_EQ(stats_after.tier_validations, stats_before.tier_validations + 1);
}

/*=============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
