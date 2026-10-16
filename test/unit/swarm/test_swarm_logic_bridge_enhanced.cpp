/**
 * @file test_swarm_logic_bridge_enhanced.cpp
 * @brief Comprehensive unit tests for Enhanced Logic-Swarm Bridge
 *
 * TEST COVERAGE:
 * - Enhanced consensus validation (10 tests)
 * - Byzantine detection (8 tests)
 * - Quorum signal validation (7 tests)
 * - Emergence tier validation (8 tests)
 * - Brain/Immune/UMM integration (5 tests)
 * - Neuromodulation effects (3 tests)
 * - Statistics tracking (2 tests)
 *
 * TOTAL: 43 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_logic_bridge.h"
#include "swarm/nimcp_swarm_consensus.h"
#include "swarm/nimcp_swarm_quorum.h"
#include "swarm/nimcp_swarm_emergence.h"
#include "utils/logging/nimcp_logging.h"

class SwarmLogicBridgeEnhancedTest : public ::testing::Test {
protected:
    swarm_logic_bridge_t* bridge;
    swarm_logic_bridge_config_t config;

    void SetUp() override {
        swarm_logic_bridge_get_default_config(&config);
        config.enable_bio_async = false;
        bridge = swarm_logic_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            swarm_logic_bridge_destroy(bridge);
        }
    }

    // Helper: Create vote response
    swarm_vote_response_t create_vote(uint32_t proposal_id, uint16_t voter,
                                     swarm_vote_choice_t choice, float confidence) {
        swarm_vote_response_t vote;
        memset(&vote, 0, sizeof(vote));
        vote.proposal_id = proposal_id;
        vote.voter_drone = voter;
        vote.choice = choice;
        vote.confidence = confidence;
        return vote;
    }

    // Helper: Create signal molecule
    nimcp_signal_molecule_t create_signal(double concentration, double threshold, bool reached) {
        nimcp_signal_molecule_t signal;
        memset(&signal, 0, sizeof(signal));
        signal.concentration = concentration;
        signal.threshold = threshold;
        signal.threshold_reached = reached;
        signal.decay_rate = 0.1;
        signal.amplification = 1.2;
        signal.inhibition = 0.8;
        signal.last_update_time = 0;
        signal.committed_count = 0;
        signal.hysteresis_low = threshold * 0.8;
        signal.hysteresis_high = threshold * 1.2;
        return signal;
    }

    // Helper: Create swarm state
    swarm_state_t create_swarm_state(uint32_t connected, uint32_t healthy, float coherence) {
        swarm_state_t state;
        memset(&state, 0, sizeof(state));
        state.connected_drones = connected;
        state.healthy_drones = healthy;
        state.collective_coherence = coherence;
        state.timestamp = 0;
        return state;
    }
};

/*=============================================================================
 * ENHANCED CONSENSUS VALIDATION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateConsensusUnanimousAgree) {
    std::vector<swarm_vote_response_t> votes;
    for (uint16_t i = 0; i < 5; i++) {
        votes.push_back(create_vote(1, i, VOTE_CHOICE_AGREE, 0.9F));
    }

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_validate_consensus_votes(bridge, votes.data(),
                                                              votes.size(), &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result);
    EXPECT_GT(result.confidence, 0.8F);
    EXPECT_EQ(result.num_inputs_used, 5u);
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateConsensusMajorityAgree) {
    std::vector<swarm_vote_response_t> votes;
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_AGREE, 0.8F));
    votes.push_back(create_vote(1, 1, VOTE_CHOICE_AGREE, 0.9F));
    votes.push_back(create_vote(1, 2, VOTE_CHOICE_AGREE, 0.7F));
    votes.push_back(create_vote(1, 3, VOTE_CHOICE_DISAGREE, 0.6F));
    votes.push_back(create_vote(1, 4, VOTE_CHOICE_DISAGREE, 0.5F));

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_validate_consensus_votes(bridge, votes.data(),
                                                              votes.size(), &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result);  // 3/5 agree with high confidence
    EXPECT_EQ(result.num_inputs_used, 5u);
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateConsensusSplitVote) {
    std::vector<swarm_vote_response_t> votes;
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_AGREE, 0.5F));
    votes.push_back(create_vote(1, 1, VOTE_CHOICE_AGREE, 0.5F));
    votes.push_back(create_vote(1, 2, VOTE_CHOICE_DISAGREE, 0.6F));
    votes.push_back(create_vote(1, 3, VOTE_CHOICE_DISAGREE, 0.7F));

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_validate_consensus_votes(bridge, votes.data(),
                                                              votes.size(), &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(result.result);  // Split vote - no majority (2/4)
    EXPECT_LE(result.confidence, 0.5F);  // Low/borderline confidence
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateConsensusWithAbstain) {
    std::vector<swarm_vote_response_t> votes;
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_AGREE, 0.9F));
    votes.push_back(create_vote(1, 1, VOTE_CHOICE_AGREE, 0.8F));
    votes.push_back(create_vote(1, 2, VOTE_CHOICE_ABSTAIN, 0.0F));
    votes.push_back(create_vote(1, 3, VOTE_CHOICE_ABSTAIN, 0.0F));

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_validate_consensus_votes(bridge, votes.data(),
                                                              votes.size(), &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    // Only 2 agree votes, but spread over 4 total
    EXPECT_EQ(result.num_inputs_used, 4u);
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateConsensusNullParameters) {
    std::vector<swarm_vote_response_t> votes;
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_AGREE, 0.9F));

    swarm_logic_result_t result;

    EXPECT_EQ(swarm_logic_validate_consensus_votes(nullptr, votes.data(), 1, &result),
              NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(swarm_logic_validate_consensus_votes(bridge, nullptr, 1, &result),
              NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(swarm_logic_validate_consensus_votes(bridge, votes.data(), 0, &result),
              NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(swarm_logic_validate_consensus_votes(bridge, votes.data(), 1, nullptr),
              NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateConsensusSingleVote) {
    std::vector<swarm_vote_response_t> votes;
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_AGREE, 1.0F));

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_validate_consensus_votes(bridge, votes.data(), 1, &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result);
    EXPECT_FLOAT_EQ(result.confidence, 1.0F);
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateConsensusLargeVoteSet) {
    std::vector<swarm_vote_response_t> votes;
    for (uint16_t i = 0; i < 100; i++) {
        votes.push_back(create_vote(1, i, VOTE_CHOICE_AGREE, 0.85F));
    }

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_validate_consensus_votes(bridge, votes.data(),
                                                              votes.size(), &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result);
    EXPECT_EQ(result.num_inputs_used, 100u);
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateConsensusLowConfidence) {
    std::vector<swarm_vote_response_t> votes;
    for (uint16_t i = 0; i < 10; i++) {
        votes.push_back(create_vote(1, i, VOTE_CHOICE_AGREE, 0.2F));  // Very low confidence
    }

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_validate_consensus_votes(bridge, votes.data(),
                                                              votes.size(), &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(result.result);  // Low confidence should fail
    EXPECT_LT(result.confidence, 0.5F);
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateConsensusAllDisagree) {
    std::vector<swarm_vote_response_t> votes;
    for (uint16_t i = 0; i < 5; i++) {
        votes.push_back(create_vote(1, i, VOTE_CHOICE_DISAGREE, 0.9F));
    }

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_validate_consensus_votes(bridge, votes.data(),
                                                              votes.size(), &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(result.result);  // No agree votes
    EXPECT_FLOAT_EQ(result.confidence, 0.0F);
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateConsensusStatisticsUpdate) {
    std::vector<swarm_vote_response_t> votes;
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_AGREE, 0.9F));

    swarm_logic_result_t result;
    swarm_logic_bridge_stats_t stats_before, stats_after;

    swarm_logic_bridge_get_stats(bridge, &stats_before);
    swarm_logic_validate_consensus_votes(bridge, votes.data(), 1, &result);
    swarm_logic_bridge_get_stats(bridge, &stats_after);

    EXPECT_EQ(stats_after.consensus_validations, stats_before.consensus_validations + 1);
    EXPECT_EQ(stats_after.total_evaluations, stats_before.total_evaluations + 1);
}

/*=============================================================================
 * BYZANTINE DETECTION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeEnhancedTest, DetectByzantineContradictoryVotes) {
    std::vector<swarm_vote_response_t> votes;
    // Agent 0 votes twice with different choices
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_AGREE, 0.9F));
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_DISAGREE, 0.8F));  // Contradicts
    votes.push_back(create_vote(1, 1, VOTE_CHOICE_AGREE, 0.9F));

    byzantine_detection_t byzantine_agents[10];
    uint32_t byzantine_count = 0;

    nimcp_error_t err = swarm_logic_detect_byzantine_pattern(bridge, votes.data(),
                                                              votes.size(),
                                                              byzantine_agents,
                                                              &byzantine_count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(byzantine_count, 1u);
    EXPECT_EQ(byzantine_agents[0].agent_id, 0u);
    EXPECT_FLOAT_EQ(byzantine_agents[0].suspicion_score, 1.0F);
    EXPECT_EQ(byzantine_agents[0].contradiction_count, 1u);
}

TEST_F(SwarmLogicBridgeEnhancedTest, DetectByzantineMultipleAgents) {
    std::vector<swarm_vote_response_t> votes;
    // Agent 0 contradicts itself
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_AGREE, 0.9F));
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_DISAGREE, 0.8F));
    // Agent 1 contradicts itself
    votes.push_back(create_vote(1, 1, VOTE_CHOICE_AGREE, 0.9F));
    votes.push_back(create_vote(1, 1, VOTE_CHOICE_DISAGREE, 0.8F));
    // Agent 2 is honest
    votes.push_back(create_vote(1, 2, VOTE_CHOICE_AGREE, 0.9F));

    byzantine_detection_t byzantine_agents[10];
    uint32_t byzantine_count = 0;

    nimcp_error_t err = swarm_logic_detect_byzantine_pattern(bridge, votes.data(),
                                                              votes.size(),
                                                              byzantine_agents,
                                                              &byzantine_count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(byzantine_count, 2u);
}

TEST_F(SwarmLogicBridgeEnhancedTest, DetectByzantineNoDetection) {
    std::vector<swarm_vote_response_t> votes;
    // All honest votes
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_AGREE, 0.9F));
    votes.push_back(create_vote(1, 1, VOTE_CHOICE_AGREE, 0.9F));
    votes.push_back(create_vote(1, 2, VOTE_CHOICE_DISAGREE, 0.8F));

    byzantine_detection_t byzantine_agents[10];
    uint32_t byzantine_count = 0;

    nimcp_error_t err = swarm_logic_detect_byzantine_pattern(bridge, votes.data(),
                                                              votes.size(),
                                                              byzantine_agents,
                                                              &byzantine_count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(byzantine_count, 0u);
}

TEST_F(SwarmLogicBridgeEnhancedTest, DetectByzantineDuplicateNonContradictory) {
    std::vector<swarm_vote_response_t> votes;
    // Agent 0 votes twice with SAME choice (not Byzantine)
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_AGREE, 0.9F));
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_AGREE, 0.9F));

    byzantine_detection_t byzantine_agents[10];
    uint32_t byzantine_count = 0;

    nimcp_error_t err = swarm_logic_detect_byzantine_pattern(bridge, votes.data(),
                                                              votes.size(),
                                                              byzantine_agents,
                                                              &byzantine_count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(byzantine_count, 0u);  // Same vote twice is not contradictory
}

TEST_F(SwarmLogicBridgeEnhancedTest, DetectByzantineNullParameters) {
    std::vector<swarm_vote_response_t> votes;
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_AGREE, 0.9F));

    byzantine_detection_t byzantine_agents[10];
    uint32_t byzantine_count = 0;

    EXPECT_EQ(swarm_logic_detect_byzantine_pattern(nullptr, votes.data(), 1,
                                                    byzantine_agents, &byzantine_count),
              NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(swarm_logic_detect_byzantine_pattern(bridge, nullptr, 1,
                                                    byzantine_agents, &byzantine_count),
              NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(swarm_logic_detect_byzantine_pattern(bridge, votes.data(), 1,
                                                    nullptr, &byzantine_count),
              NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(swarm_logic_detect_byzantine_pattern(bridge, votes.data(), 1,
                                                    byzantine_agents, nullptr),
              NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(SwarmLogicBridgeEnhancedTest, DetectByzantineStatisticsUpdate) {
    std::vector<swarm_vote_response_t> votes;
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_AGREE, 0.9F));
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_DISAGREE, 0.8F));

    byzantine_detection_t byzantine_agents[10];
    uint32_t byzantine_count = 0;

    swarm_logic_bridge_stats_t stats_before, stats_after;
    swarm_logic_bridge_get_stats(bridge, &stats_before);

    swarm_logic_detect_byzantine_pattern(bridge, votes.data(), votes.size(),
                                        byzantine_agents, &byzantine_count);

    swarm_logic_bridge_get_stats(bridge, &stats_after);

    EXPECT_GT(stats_after.byzantine_detections, stats_before.byzantine_detections);
}

TEST_F(SwarmLogicBridgeEnhancedTest, DetectByzantineAbstainVsAgree) {
    std::vector<swarm_vote_response_t> votes;
    // Agent votes abstain then agree - is this Byzantine?
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_ABSTAIN, 0.0F));
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_AGREE, 0.9F));

    byzantine_detection_t byzantine_agents[10];
    uint32_t byzantine_count = 0;

    nimcp_error_t err = swarm_logic_detect_byzantine_pattern(bridge, votes.data(),
                                                              votes.size(),
                                                              byzantine_agents,
                                                              &byzantine_count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(byzantine_count, 1u);  // Different choices = Byzantine
}

TEST_F(SwarmLogicBridgeEnhancedTest, DetectByzantineMaxDetections) {
    std::vector<swarm_vote_response_t> votes;
    // Create 50 Byzantine agents (but function only detects up to 32)
    for (uint16_t i = 0; i < 50; i++) {
        votes.push_back(create_vote(1, i, VOTE_CHOICE_AGREE, 0.9F));
        votes.push_back(create_vote(1, i, VOTE_CHOICE_DISAGREE, 0.8F));
    }

    byzantine_detection_t byzantine_agents[40];
    uint32_t byzantine_count = 0;

    nimcp_error_t err = swarm_logic_detect_byzantine_pattern(bridge, votes.data(),
                                                              votes.size(),
                                                              byzantine_agents,
                                                              &byzantine_count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_LE(byzantine_count, 32u);  // Capped at 32
}

/*=============================================================================
 * QUORUM SIGNAL VALIDATION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateQuorumSignalsAllActive) {
    std::vector<nimcp_signal_molecule_t> signals;
    signals.push_back(create_signal(0.8, 0.5, true));
    signals.push_back(create_signal(0.9, 0.5, true));
    signals.push_back(create_signal(0.7, 0.5, true));

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_validate_quorum_signals(bridge, signals.data(),
                                                            signals.size(), &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(result.result);
    EXPECT_EQ(result.num_inputs_used, 3u);
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateQuorumSignalsMixed) {
    std::vector<nimcp_signal_molecule_t> signals;
    signals.push_back(create_signal(0.8, 0.5, true));
    signals.push_back(create_signal(0.3, 0.5, false));
    signals.push_back(create_signal(0.6, 0.5, true));

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_validate_quorum_signals(bridge, signals.data(),
                                                            signals.size(), &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(result.num_inputs_used, 3u);
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateQuorumSignalsNoneActive) {
    std::vector<nimcp_signal_molecule_t> signals;
    signals.push_back(create_signal(0.2, 0.5, false));
    signals.push_back(create_signal(0.1, 0.5, false));

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_validate_quorum_signals(bridge, signals.data(),
                                                            signals.size(), &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(result.result);  // No active signals
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateQuorumSignalsNullParameters) {
    std::vector<nimcp_signal_molecule_t> signals;
    signals.push_back(create_signal(0.8, 0.5, true));

    swarm_logic_result_t result;

    EXPECT_EQ(swarm_logic_validate_quorum_signals(nullptr, signals.data(), 1, &result),
              NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(swarm_logic_validate_quorum_signals(bridge, nullptr, 1, &result),
              NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(swarm_logic_validate_quorum_signals(bridge, signals.data(), 0, &result),
              NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(SwarmLogicBridgeEnhancedTest, CheckSignalExclusionAttackRetreat) {
    bool mutually_exclusive = false;

    nimcp_error_t err = swarm_logic_check_signal_exclusion(bridge,
                                                           NIMCP_SIGNAL_ATTACK,
                                                           NIMCP_SIGNAL_RETREAT,
                                                           &mutually_exclusive);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(mutually_exclusive);  // ATTACK and RETREAT are exclusive
}

TEST_F(SwarmLogicBridgeEnhancedTest, CheckSignalExclusionExploreDefend) {
    bool mutually_exclusive = false;

    nimcp_error_t err = swarm_logic_check_signal_exclusion(bridge,
                                                           NIMCP_SIGNAL_EXPLORE,
                                                           NIMCP_SIGNAL_DEFEND,
                                                           &mutually_exclusive);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(mutually_exclusive);
}

TEST_F(SwarmLogicBridgeEnhancedTest, CheckSignalExclusionCompatible) {
    bool mutually_exclusive = false;

    nimcp_error_t err = swarm_logic_check_signal_exclusion(bridge,
                                                           NIMCP_SIGNAL_ATTACK,
                                                           NIMCP_SIGNAL_ALERT,
                                                           &mutually_exclusive);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(mutually_exclusive);  // ATTACK and ALERT are compatible
}

/*=============================================================================
 * EMERGENCE TIER VALIDATION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateTierTransitionValid) {
    swarm_state_t state = create_swarm_state(4, 4, 0.9F);
    bool valid = false;

    nimcp_error_t err = swarm_logic_validate_tier_transition(bridge,
                                                             SWARM_TIER_PAIR,
                                                             SWARM_TIER_SQUAD,
                                                             &state,
                                                             &valid);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(valid);  // 4 healthy drones with high coherence
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateTierTransitionInsufficientDrones) {
    swarm_state_t state = create_swarm_state(2, 2, 0.9F);  // Only 2 drones
    bool valid = false;

    nimcp_error_t err = swarm_logic_validate_tier_transition(bridge,
                                                             SWARM_TIER_PAIR,
                                                             SWARM_TIER_SQUAD,
                                                             &state,
                                                             &valid);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(valid);  // Need 4+ drones for SQUAD tier
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateTierTransitionLowCoherence) {
    swarm_state_t state = create_swarm_state(4, 4, 0.3F);  // Low coherence
    bool valid = false;

    nimcp_error_t err = swarm_logic_validate_tier_transition(bridge,
                                                             SWARM_TIER_PAIR,
                                                             SWARM_TIER_SQUAD,
                                                             &state,
                                                             &valid);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(valid);  // Coherence too low
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateTierTransitionPoorHealth) {
    swarm_state_t state = create_swarm_state(4, 1, 0.9F);  // Only 1 healthy
    bool valid = false;

    nimcp_error_t err = swarm_logic_validate_tier_transition(bridge,
                                                             SWARM_TIER_PAIR,
                                                             SWARM_TIER_SQUAD,
                                                             &state,
                                                             &valid);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(valid);  // Health ratio too low (1/4 = 25%)
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateTierTransitionSkipTier) {
    swarm_state_t state = create_swarm_state(8, 8, 0.9F);
    bool valid = false;

    // Try to skip from INDIVIDUAL to SQUAD (skipping PAIR)
    nimcp_error_t err = swarm_logic_validate_tier_transition(bridge,
                                                             SWARM_TIER_INDIVIDUAL,
                                                             SWARM_TIER_SQUAD,
                                                             &state,
                                                             &valid);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_FALSE(valid);  // Cannot skip tiers
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateTierTransitionDowngrade) {
    swarm_state_t state = create_swarm_state(4, 4, 0.9F);
    bool valid = false;

    // Downgrade from PLATOON to SQUAD
    nimcp_error_t err = swarm_logic_validate_tier_transition(bridge,
                                                             SWARM_TIER_PLATOON,
                                                             SWARM_TIER_SQUAD,
                                                             &state,
                                                             &valid);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(valid);  // Sequential downgrade is valid
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateTierTransitionMaxTier) {
    swarm_state_t state = create_swarm_state(50, 48, 0.95F);
    bool valid = false;

    nimcp_error_t err = swarm_logic_validate_tier_transition(bridge,
                                                             SWARM_TIER_COMPANY,
                                                             SWARM_TIER_BATTALION,
                                                             &state,
                                                             &valid);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_TRUE(valid);  // 50 drones with high coherence and health
}

TEST_F(SwarmLogicBridgeEnhancedTest, ValidateTierTransitionNullParameters) {
    swarm_state_t state = create_swarm_state(4, 4, 0.9F);
    bool valid = false;

    EXPECT_EQ(swarm_logic_validate_tier_transition(nullptr, SWARM_TIER_PAIR,
                                                   SWARM_TIER_SQUAD, &state, &valid),
              NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(swarm_logic_validate_tier_transition(bridge, SWARM_TIER_PAIR,
                                                   SWARM_TIER_SQUAD, nullptr, &valid),
              NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(swarm_logic_validate_tier_transition(bridge, SWARM_TIER_PAIR,
                                                   SWARM_TIER_SQUAD, &state, nullptr),
              NIMCP_ERROR_INVALID_PARAMETER);
}

/*=============================================================================
 * BRAIN/IMMUNE/UMM INTEGRATION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeEnhancedTest, ConnectBrain) {
    void* dummy_brain = (void*)0x12345678;

    nimcp_error_t err = swarm_logic_connect_brain(bridge, dummy_brain);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SwarmLogicBridgeEnhancedTest, ConnectImmune) {
    void* dummy_immune = (void*)0x87654321;

    nimcp_error_t err = swarm_logic_connect_immune(bridge, dummy_immune);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SwarmLogicBridgeEnhancedTest, ConnectUMM) {
    void* dummy_umm = (void*)0xABCDEF00;

    nimcp_error_t err = swarm_logic_connect_umm(bridge, dummy_umm);
    EXPECT_EQ(err, NIMCP_SUCCESS);
}

TEST_F(SwarmLogicBridgeEnhancedTest, ConnectNullBridge) {
    void* dummy_brain = (void*)0x12345678;

    EXPECT_EQ(swarm_logic_connect_brain(nullptr, dummy_brain),
              NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(swarm_logic_connect_immune(nullptr, dummy_brain),
              NIMCP_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(swarm_logic_connect_umm(nullptr, dummy_brain),
              NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(SwarmLogicBridgeEnhancedTest, ConnectAllSystems) {
    void* dummy_brain = (void*)0x1;
    void* dummy_immune = (void*)0x2;
    void* dummy_umm = (void*)0x3;

    EXPECT_EQ(swarm_logic_connect_brain(bridge, dummy_brain), NIMCP_SUCCESS);
    EXPECT_EQ(swarm_logic_connect_immune(bridge, dummy_immune), NIMCP_SUCCESS);
    EXPECT_EQ(swarm_logic_connect_umm(bridge, dummy_umm), NIMCP_SUCCESS);
}

/*=============================================================================
 * NEUROMODULATION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeEnhancedTest, EvaluateWithModulationHighDopamine) {
    // Add a rule first
    uint32_t agent_ids[] = {0, 1};
    swarm_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.rule_id = 1;
    rule.gate_type = LOGIC_GATE_AND;
    rule.input_agent_ids = agent_ids;
    rule.num_inputs = 2;
    rule.threshold = 0.5F;
    rule.confidence_weight = 1.0F;

    swarm_logic_bridge_add_rule(bridge, &rule);

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_evaluate_with_modulation(bridge, 1,
                                                              0.9F,  // High DA
                                                              0.0F,  // No ACh
                                                              &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(result.rule_id, 1u);
}

TEST_F(SwarmLogicBridgeEnhancedTest, EvaluateWithModulationHighAcetylcholine) {
    // Add a rule first
    uint32_t agent_ids[] = {0, 1};
    swarm_logic_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    rule.rule_id = 2;
    rule.gate_type = LOGIC_GATE_OR;
    rule.input_agent_ids = agent_ids;
    rule.num_inputs = 2;
    rule.threshold = 0.5F;
    rule.confidence_weight = 1.0F;

    swarm_logic_bridge_add_rule(bridge, &rule);

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_evaluate_with_modulation(bridge, 2,
                                                              0.0F,  // No DA
                                                              0.8F,  // High ACh
                                                              &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(result.rule_id, 2u);
}

TEST_F(SwarmLogicBridgeEnhancedTest, EvaluateWithModulationInvalidRule) {
    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_evaluate_with_modulation(bridge, 999,  // Non-existent
                                                              0.5F, 0.5F, &result);

    EXPECT_EQ(err, NIMCP_ERROR_INVALID_PARAMETER);
}

/*=============================================================================
 * STATISTICS TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeEnhancedTest, StatisticsTrackConsensusValidations) {
    std::vector<swarm_vote_response_t> votes;
    votes.push_back(create_vote(1, 0, VOTE_CHOICE_AGREE, 0.9F));

    swarm_logic_bridge_stats_t stats_before, stats_after;
    swarm_logic_bridge_get_stats(bridge, &stats_before);

    swarm_logic_result_t result;
    swarm_logic_validate_consensus_votes(bridge, votes.data(), 1, &result);

    swarm_logic_bridge_get_stats(bridge, &stats_after);

    EXPECT_EQ(stats_after.consensus_validations, stats_before.consensus_validations + 1);
}

TEST_F(SwarmLogicBridgeEnhancedTest, StatisticsTrackQuorumValidations) {
    std::vector<nimcp_signal_molecule_t> signals;
    signals.push_back(create_signal(0.8, 0.5, true));

    swarm_logic_bridge_stats_t stats_before, stats_after;
    swarm_logic_bridge_get_stats(bridge, &stats_before);

    swarm_logic_result_t result;
    swarm_logic_validate_quorum_signals(bridge, signals.data(), 1, &result);

    swarm_logic_bridge_get_stats(bridge, &stats_after);

    EXPECT_EQ(stats_after.quorum_validations, stats_before.quorum_validations + 1);
}

/*=============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
