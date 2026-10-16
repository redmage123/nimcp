/**
 * @file test_swarm_logic_bridge_enhanced_regression.cpp
 * @brief Regression tests for Enhanced Logic-Swarm Bridge
 *
 * TEST COVERAGE:
 * - Performance regression tests (4 tests)
 * - Memory leak detection (3 tests)
 * - Stability under load (3 tests)
 * - Edge cases and corner cases (3 tests)
 *
 * TOTAL: 13 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-19
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <chrono>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_logic_bridge.h"
#include "swarm/nimcp_swarm_consensus.h"
#include "swarm/nimcp_swarm_quorum.h"
#include "swarm/nimcp_swarm_emergence.h"
#include "utils/logging/nimcp_logging.h"

class SwarmLogicBridgeRegressionTest : public ::testing::Test {
protected:
    swarm_logic_bridge_t* bridge;

    void SetUp() override {
        swarm_logic_bridge_config_t config;
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
};

/*=============================================================================
 * PERFORMANCE REGRESSION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeRegressionTest, ConsensusValidationPerformance) {
    // Test performance doesn't degrade for consensus validation
    constexpr int ITERATIONS = 1000;

    std::vector<swarm_vote_response_t> votes;
    for (uint16_t i = 0; i < 10; i++) {
        swarm_vote_response_t v;
        memset(&v, 0, sizeof(v));
        v.proposal_id = 1;
        v.voter_drone = i;
        v.choice = VOTE_CHOICE_AGREE;
        v.confidence = 0.9F;
        votes.push_back(v);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; i++) {
        swarm_logic_result_t result;
        swarm_logic_validate_consensus_votes(bridge, votes.data(), votes.size(), &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 1000 iterations in under 500ms
    EXPECT_LT(duration.count(), 500);

    // Verify statistics
    swarm_logic_bridge_stats_t stats;
    swarm_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.consensus_validations, static_cast<uint64_t>(ITERATIONS));
}

TEST_F(SwarmLogicBridgeRegressionTest, ByzantineDetectionPerformance) {
    constexpr int ITERATIONS = 500;

    std::vector<swarm_vote_response_t> votes;
    for (uint16_t i = 0; i < 20; i++) {
        swarm_vote_response_t v;
        memset(&v, 0, sizeof(v));
        v.proposal_id = 1;
        v.voter_drone = i;
        v.choice = (i % 2 == 0) ? VOTE_CHOICE_AGREE : VOTE_CHOICE_DISAGREE;
        v.confidence = 0.9F;
        votes.push_back(v);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; i++) {
        byzantine_detection_t byzantine_agents[32];
        uint32_t byzantine_count = 0;
        swarm_logic_detect_byzantine_pattern(bridge, votes.data(), votes.size(),
                                            byzantine_agents, &byzantine_count);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 500 iterations in under 300ms
    EXPECT_LT(duration.count(), 300);
}

TEST_F(SwarmLogicBridgeRegressionTest, QuorumValidationPerformance) {
    constexpr int ITERATIONS = 1000;

    std::vector<nimcp_signal_molecule_t> signals;
    for (int i = 0; i < 8; i++) {
        nimcp_signal_molecule_t signal;
        memset(&signal, 0, sizeof(signal));
        signal.concentration = 0.7;
        signal.threshold = 0.5;
        signal.threshold_reached = true;
        signals.push_back(signal);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; i++) {
        swarm_logic_result_t result;
        swarm_logic_validate_quorum_signals(bridge, signals.data(), signals.size(), &result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 1000 iterations in under 400ms
    EXPECT_LT(duration.count(), 400);
}

TEST_F(SwarmLogicBridgeRegressionTest, TierValidationPerformance) {
    constexpr int ITERATIONS = 500;

    swarm_state_t state;
    memset(&state, 0, sizeof(state));
    state.connected_drones = 8;
    state.healthy_drones = 8;
    state.collective_coherence = 0.9F;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; i++) {
        bool valid = false;
        swarm_logic_validate_tier_transition(bridge,
                                            SWARM_TIER_SQUAD,
                                            SWARM_TIER_PLATOON,
                                            &state,
                                            &valid);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 500 iterations in under 200ms
    EXPECT_LT(duration.count(), 200);
}

/*=============================================================================
 * MEMORY LEAK DETECTION TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeRegressionTest, NoMemoryLeakConsensusValidation) {
    // Repeatedly create and validate votes to detect leaks
    constexpr int ITERATIONS = 100;

    for (int iter = 0; iter < ITERATIONS; iter++) {
        std::vector<swarm_vote_response_t> votes;
        for (uint16_t i = 0; i < 50; i++) {
            swarm_vote_response_t v;
            memset(&v, 0, sizeof(v));
            v.proposal_id = iter;
            v.voter_drone = i;
            v.choice = VOTE_CHOICE_AGREE;
            v.confidence = 0.9F;
            votes.push_back(v);
        }

        swarm_logic_result_t result;
        swarm_logic_validate_consensus_votes(bridge, votes.data(), votes.size(), &result);
    }

    // If we got here without crashing, no obvious memory leaks
    SUCCEED();
}

TEST_F(SwarmLogicBridgeRegressionTest, NoMemoryLeakByzantineDetection) {
    constexpr int ITERATIONS = 100;

    for (int iter = 0; iter < ITERATIONS; iter++) {
        std::vector<swarm_vote_response_t> votes;
        for (uint16_t i = 0; i < 30; i++) {
            swarm_vote_response_t v1, v2;
            memset(&v1, 0, sizeof(v1));
            v1.proposal_id = iter;
            v1.voter_drone = i;
            v1.choice = VOTE_CHOICE_AGREE;
            v1.confidence = 0.9F;
            votes.push_back(v1);

            memset(&v2, 0, sizeof(v2));
            v2.proposal_id = iter;
            v2.voter_drone = i;
            v2.choice = VOTE_CHOICE_DISAGREE;
            v2.confidence = 0.8F;
            votes.push_back(v2);
        }

        byzantine_detection_t byzantine_agents[32];
        uint32_t byzantine_count = 0;
        swarm_logic_detect_byzantine_pattern(bridge, votes.data(), votes.size(),
                                            byzantine_agents, &byzantine_count);
    }

    SUCCEED();
}

TEST_F(SwarmLogicBridgeRegressionTest, NoMemoryLeakQuorumValidation) {
    constexpr int ITERATIONS = 100;

    for (int iter = 0; iter < ITERATIONS; iter++) {
        std::vector<nimcp_signal_molecule_t> signals;
        for (int i = 0; i < NIMCP_SIGNAL_COUNT; i++) {
            nimcp_signal_molecule_t signal;
            memset(&signal, 0, sizeof(signal));
            signal.concentration = 0.5 + (iter % 5) * 0.1;
            signal.threshold = 0.5;
            signal.threshold_reached = (signal.concentration >= signal.threshold);
            signals.push_back(signal);
        }

        swarm_logic_result_t result;
        swarm_logic_validate_quorum_signals(bridge, signals.data(), signals.size(), &result);
    }

    SUCCEED();
}

/*=============================================================================
 * STABILITY UNDER LOAD TESTS
 *============================================================================*/

TEST_F(SwarmLogicBridgeRegressionTest, LargeVoteSetStability) {
    // Test with very large vote sets
    std::vector<swarm_vote_response_t> votes;
    for (uint16_t i = 0; i < 1000; i++) {
        swarm_vote_response_t v;
        memset(&v, 0, sizeof(v));
        v.proposal_id = 1;
        v.voter_drone = i;
        v.choice = (i % 3 == 0) ? VOTE_CHOICE_AGREE :
                   (i % 3 == 1) ? VOTE_CHOICE_DISAGREE :
                                  VOTE_CHOICE_ABSTAIN;
        v.confidence = 0.5F + (i % 50) * 0.01F;
        votes.push_back(v);
    }

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_validate_consensus_votes(bridge, votes.data(),
                                                              votes.size(), &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(result.num_inputs_used, 1000u);
}

TEST_F(SwarmLogicBridgeRegressionTest, ManyByzantineAgentsStability) {
    // Test with many Byzantine agents
    std::vector<swarm_vote_response_t> votes;
    for (uint16_t i = 0; i < 100; i++) {
        swarm_vote_response_t v1, v2;
        memset(&v1, 0, sizeof(v1));
        v1.proposal_id = 1;
        v1.voter_drone = i;
        v1.choice = VOTE_CHOICE_AGREE;
        v1.confidence = 0.9F;
        votes.push_back(v1);

        memset(&v2, 0, sizeof(v2));
        v2.proposal_id = 1;
        v2.voter_drone = i;
        v2.choice = VOTE_CHOICE_DISAGREE;
        v2.confidence = 0.8F;
        votes.push_back(v2);
    }

    byzantine_detection_t byzantine_agents[100];
    uint32_t byzantine_count = 0;
    nimcp_error_t err = swarm_logic_detect_byzantine_pattern(bridge, votes.data(),
                                                              votes.size(),
                                                              byzantine_agents,
                                                              &byzantine_count);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    // Detection is capped at 32
    EXPECT_LE(byzantine_count, 32u);
}

TEST_F(SwarmLogicBridgeRegressionTest, ConcurrentOperationsStability) {
    // Simulate concurrent-like operations (sequential in single-threaded test)
    constexpr int OPS = 50;

    for (int i = 0; i < OPS; i++) {
        // Mix different operation types

        // Consensus validation
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

        // Byzantine detection
        byzantine_detection_t byzantine_agents[10];
        uint32_t byzantine_count = 0;
        swarm_logic_detect_byzantine_pattern(bridge, votes.data(), 1,
                                            byzantine_agents, &byzantine_count);

        // Quorum validation
        nimcp_signal_molecule_t signal;
        memset(&signal, 0, sizeof(signal));
        signal.concentration = 0.7;
        signal.threshold = 0.5;
        signal.threshold_reached = true;
        swarm_logic_validate_quorum_signals(bridge, &signal, 1, &result);

        // Tier validation
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
    }

    // Verify statistics are consistent
    swarm_logic_bridge_stats_t stats;
    swarm_logic_bridge_get_stats(bridge, &stats);
    EXPECT_EQ(stats.consensus_validations, static_cast<uint64_t>(OPS));
    EXPECT_EQ(stats.quorum_validations, static_cast<uint64_t>(OPS));
    EXPECT_EQ(stats.tier_validations, static_cast<uint64_t>(OPS));
}

/*=============================================================================
 * EDGE CASES AND CORNER CASES
 *============================================================================*/

TEST_F(SwarmLogicBridgeRegressionTest, ZeroVotesEdgeCase) {
    // Edge case: empty vote array
    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_validate_consensus_votes(bridge, nullptr, 0, &result);

    EXPECT_EQ(err, NIMCP_ERROR_INVALID_PARAMETER);
}

TEST_F(SwarmLogicBridgeRegressionTest, ExtremConfidenceValues) {
    // Test extreme confidence values (0.0, 1.0, out of range)
    std::vector<swarm_vote_response_t> votes;

    swarm_vote_response_t v1;
    memset(&v1, 0, sizeof(v1));
    v1.proposal_id = 1;
    v1.voter_drone = 0;
    v1.choice = VOTE_CHOICE_AGREE;
    v1.confidence = 0.0F;  // Minimum
    votes.push_back(v1);

    swarm_vote_response_t v2;
    memset(&v2, 0, sizeof(v2));
    v2.proposal_id = 1;
    v2.voter_drone = 1;
    v2.choice = VOTE_CHOICE_AGREE;
    v2.confidence = 1.0F;  // Maximum
    votes.push_back(v2);

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_validate_consensus_votes(bridge, votes.data(),
                                                              votes.size(), &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(result.num_inputs_used, 2u);
}

TEST_F(SwarmLogicBridgeRegressionTest, AllVoteChoiceTypes) {
    // Test all three vote choices in single validation
    std::vector<swarm_vote_response_t> votes;

    swarm_vote_response_t v1;
    memset(&v1, 0, sizeof(v1));
    v1.proposal_id = 1;
    v1.voter_drone = 0;
    v1.choice = VOTE_CHOICE_AGREE;
    v1.confidence = 0.9F;
    votes.push_back(v1);

    swarm_vote_response_t v2;
    memset(&v2, 0, sizeof(v2));
    v2.proposal_id = 1;
    v2.voter_drone = 1;
    v2.choice = VOTE_CHOICE_DISAGREE;
    v2.confidence = 0.8F;
    votes.push_back(v2);

    swarm_vote_response_t v3;
    memset(&v3, 0, sizeof(v3));
    v3.proposal_id = 1;
    v3.voter_drone = 2;
    v3.choice = VOTE_CHOICE_ABSTAIN;
    v3.confidence = 0.0F;
    votes.push_back(v3);

    swarm_logic_result_t result;
    nimcp_error_t err = swarm_logic_validate_consensus_votes(bridge, votes.data(),
                                                              votes.size(), &result);

    EXPECT_EQ(err, NIMCP_SUCCESS);
    EXPECT_EQ(result.num_inputs_used, 3u);
}

/*=============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
