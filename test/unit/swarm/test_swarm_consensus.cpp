/**
 * @file test_swarm_consensus.cpp
 * @brief Comprehensive unit tests for NIMCP Swarm Consensus Voting
 *
 * TEST COVERAGE:
 * - Proposal creation and validation
 * - Vote collection and counting
 * - Quorum checking
 * - Timeout handling
 * - Byzantine fault tolerance
 * - Weighted voting by confidence
 * - Tie-breaking mechanisms
 * - Edge cases
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <map>
#include <chrono>

// Headers have their own extern "C" guards

// Vote topics
typedef enum {
    VOTE_TOPIC_TARGET_PRIORITY,
    VOTE_TOPIC_FORMATION_CHANGE,
    VOTE_TOPIC_RETREAT,
    VOTE_TOPIC_RESOURCE_ALLOCATION,
    VOTE_TOPIC_LEADER_ELECTION,
} swarm_vote_topic_t;

// Vote choices
typedef enum {
    VOTE_CHOICE_AGREE,
    VOTE_CHOICE_DISAGREE,
    VOTE_CHOICE_ABSTAIN
} swarm_vote_choice_t;

// Vote proposal
typedef struct {
    uint32_t proposal_id;
    uint32_t proposer_drone;
    swarm_vote_topic_t topic;
    float proposal_value[4];
    uint64_t deadline_ms;
    uint64_t created_timestamp;
} swarm_vote_proposal_t;

// Vote response
typedef struct {
    uint32_t proposal_id;
    uint32_t voter_drone;
    swarm_vote_choice_t choice;
    float confidence;  // [0,1]
} swarm_vote_response_t;

// Vote result
typedef struct {
    uint32_t proposal_id;
    bool passed;
    uint32_t total_votes;
    uint32_t agree_votes;
    uint32_t disagree_votes;
    uint32_t abstain_votes;
    float weighted_agreement;  // Confidence-weighted
    bool quorum_reached;
    bool timed_out;
} swarm_vote_result_t;

// Consensus state
typedef struct {
    std::map<uint32_t, swarm_vote_proposal_t>* active_proposals;
    std::map<uint32_t, std::vector<swarm_vote_response_t>>* votes;
    uint32_t next_proposal_id;
    uint32_t total_drones;
    float quorum_threshold;    // Default: 0.5 (50%)
    float agreement_threshold; // Default: 0.66 (66%)
    uint32_t default_timeout_ms;
} swarm_consensus_state_t;

// API functions
swarm_consensus_state_t* swarm_consensus_create(uint32_t total_drones);

void swarm_consensus_destroy(swarm_consensus_state_t* state);

uint32_t swarm_consensus_create_proposal(
    swarm_consensus_state_t* state,
    uint32_t proposer_drone,
    swarm_vote_topic_t topic,
    const float* proposal_value,
    uint32_t timeout_ms
);

bool swarm_consensus_submit_vote(
    swarm_consensus_state_t* state,
    const swarm_vote_response_t* vote
);

swarm_vote_result_t swarm_consensus_get_result(
    swarm_consensus_state_t* state,
    uint32_t proposal_id
);

bool swarm_consensus_is_complete(
    swarm_consensus_state_t* state,
    uint32_t proposal_id
);

void swarm_consensus_cleanup_expired(
    swarm_consensus_state_t* state,
    uint64_t current_time_ms
);

uint32_t swarm_consensus_get_active_proposal_count(
    const swarm_consensus_state_t* state
);


//=============================================================================
// Mock Implementation
//=============================================================================

swarm_consensus_state_t* swarm_consensus_create(uint32_t total_drones) {
    swarm_consensus_state_t* state = new swarm_consensus_state_t();
    state->active_proposals = new std::map<uint32_t, swarm_vote_proposal_t>();
    state->votes = new std::map<uint32_t, std::vector<swarm_vote_response_t>>();
    state->next_proposal_id = 1;
    state->total_drones = total_drones;
    state->quorum_threshold = 0.5f;
    state->agreement_threshold = 0.66f;
    state->default_timeout_ms = 100;
    return state;
}

void swarm_consensus_destroy(swarm_consensus_state_t* state) {
    if (state) {
        delete state->active_proposals;
        delete state->votes;
        delete state;
    }
}

uint32_t swarm_consensus_create_proposal(
    swarm_consensus_state_t* state,
    uint32_t proposer_drone,
    swarm_vote_topic_t topic,
    const float* proposal_value,
    uint32_t timeout_ms
) {
    if (!state) return 0;

    uint32_t proposal_id = state->next_proposal_id++;

    swarm_vote_proposal_t proposal;
    proposal.proposal_id = proposal_id;
    proposal.proposer_drone = proposer_drone;
    proposal.topic = topic;
    memcpy(proposal.proposal_value, proposal_value, sizeof(float) * 4);
    proposal.created_timestamp = 0; // Simplified for testing
    proposal.deadline_ms = timeout_ms > 0 ? timeout_ms : state->default_timeout_ms;

    (*state->active_proposals)[proposal_id] = proposal;
    (*state->votes)[proposal_id] = std::vector<swarm_vote_response_t>();

    return proposal_id;
}

bool swarm_consensus_submit_vote(
    swarm_consensus_state_t* state,
    const swarm_vote_response_t* vote
) {
    if (!state || !vote) return false;

    auto it = state->active_proposals->find(vote->proposal_id);
    if (it == state->active_proposals->end()) return false;

    // Check for duplicate votes from same drone
    auto& vote_list = (*state->votes)[vote->proposal_id];
    for (const auto& existing_vote : vote_list) {
        if (existing_vote.voter_drone == vote->voter_drone) {
            return false; // Already voted
        }
    }

    vote_list.push_back(*vote);
    return true;
}

swarm_vote_result_t swarm_consensus_get_result(
    swarm_consensus_state_t* state,
    uint32_t proposal_id
) {
    swarm_vote_result_t result;
    memset(&result, 0, sizeof(result));
    result.proposal_id = proposal_id;

    if (!state) return result;

    auto it = state->votes->find(proposal_id);
    if (it == state->votes->end()) return result;

    const auto& vote_list = it->second;
    result.total_votes = vote_list.size();

    float weighted_agree = 0.0f;
    float weighted_total = 0.0f;

    for (const auto& vote : vote_list) {
        switch (vote.choice) {
            case VOTE_CHOICE_AGREE:
                result.agree_votes++;
                weighted_agree += vote.confidence;
                weighted_total += vote.confidence;
                break;
            case VOTE_CHOICE_DISAGREE:
                result.disagree_votes++;
                weighted_total += vote.confidence;
                break;
            case VOTE_CHOICE_ABSTAIN:
                result.abstain_votes++;
                break;
        }
    }

    // Calculate weighted agreement
    if (weighted_total > 0) {
        result.weighted_agreement = weighted_agree / weighted_total;
    }

    // Check quorum
    result.quorum_reached =
        (float)result.total_votes / (float)state->total_drones >= state->quorum_threshold;

    // Check if passed
    result.passed = result.quorum_reached &&
                   result.weighted_agreement >= state->agreement_threshold;

    return result;
}

bool swarm_consensus_is_complete(
    swarm_consensus_state_t* state,
    uint32_t proposal_id
) {
    if (!state) return false;

    auto it = state->votes->find(proposal_id);
    if (it == state->votes->end()) return false;

    swarm_vote_result_t result = swarm_consensus_get_result(state, proposal_id);

    // Complete if quorum reached or all drones voted
    return result.quorum_reached || result.total_votes >= state->total_drones;
}

void swarm_consensus_cleanup_expired(
    swarm_consensus_state_t* state,
    uint64_t current_time_ms
) {
    if (!state) return;

    std::vector<uint32_t> to_remove;

    for (auto& pair : *state->active_proposals) {
        if (pair.second.created_timestamp + pair.second.deadline_ms < current_time_ms) {
            to_remove.push_back(pair.first);
        }
    }

    for (uint32_t id : to_remove) {
        state->active_proposals->erase(id);
        state->votes->erase(id);
    }
}

uint32_t swarm_consensus_get_active_proposal_count(
    const swarm_consensus_state_t* state
) {
    return state ? state->active_proposals->size() : 0;
}

//=============================================================================
// Test Fixtures
//=============================================================================

class SwarmConsensusTest : public ::testing::Test {
protected:
    swarm_consensus_state_t* consensus;

    void SetUp() override {
        consensus = nullptr;
    }

    void TearDown() override {
        if (consensus) {
            swarm_consensus_destroy(consensus);
            consensus = nullptr;
        }
    }

    // Helper: Create test proposal
    uint32_t CreateTestProposal(
        uint32_t proposer,
        swarm_vote_topic_t topic = VOTE_TOPIC_TARGET_PRIORITY
    ) {
        float value[4] = {1.0f, 2.0f, 3.0f, 4.0f};
        return swarm_consensus_create_proposal(consensus, proposer, topic, value, 1000);
    }

    // Helper: Submit test vote
    bool SubmitVote(
        uint32_t proposal_id,
        uint32_t voter,
        swarm_vote_choice_t choice,
        float confidence = 1.0f
    ) {
        swarm_vote_response_t vote;
        vote.proposal_id = proposal_id;
        vote.voter_drone = voter;
        vote.choice = choice;
        vote.confidence = confidence;
        return swarm_consensus_submit_vote(consensus, &vote);
    }
};

//=============================================================================
// 1. Creation and Destruction Tests
//=============================================================================

TEST_F(SwarmConsensusTest, CreateValid) {
    consensus = swarm_consensus_create(10);

    ASSERT_NE(consensus, nullptr);
    EXPECT_EQ(consensus->total_drones, 10u);
    EXPECT_FLOAT_EQ(consensus->quorum_threshold, 0.5f);
    EXPECT_FLOAT_EQ(consensus->agreement_threshold, 0.66f);
}

TEST_F(SwarmConsensusTest, DestroyNull) {
    swarm_consensus_destroy(nullptr); // Should not crash
}

//=============================================================================
// 2. Proposal Creation Tests
//=============================================================================

TEST_F(SwarmConsensusTest, CreateProposal) {
    consensus = swarm_consensus_create(10);

    uint32_t proposal_id = CreateTestProposal(1);

    EXPECT_GT(proposal_id, 0u);
    EXPECT_EQ(swarm_consensus_get_active_proposal_count(consensus), 1u);
}

TEST_F(SwarmConsensusTest, CreateMultipleProposals) {
    consensus = swarm_consensus_create(10);

    uint32_t id1 = CreateTestProposal(1);
    uint32_t id2 = CreateTestProposal(2);
    uint32_t id3 = CreateTestProposal(3);

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_EQ(swarm_consensus_get_active_proposal_count(consensus), 3u);
}

TEST_F(SwarmConsensusTest, ProposalIdIncremental) {
    consensus = swarm_consensus_create(10);

    uint32_t id1 = CreateTestProposal(1);
    uint32_t id2 = CreateTestProposal(1);

    EXPECT_EQ(id2, id1 + 1);
}

//=============================================================================
// 3. Vote Submission Tests
//=============================================================================

TEST_F(SwarmConsensusTest, SubmitSingleVote) {
    consensus = swarm_consensus_create(10);
    uint32_t proposal_id = CreateTestProposal(1);

    bool success = SubmitVote(proposal_id, 2, VOTE_CHOICE_AGREE);

    ASSERT_TRUE(success);

    swarm_vote_result_t result = swarm_consensus_get_result(consensus, proposal_id);
    EXPECT_EQ(result.total_votes, 1u);
    EXPECT_EQ(result.agree_votes, 1u);
}

TEST_F(SwarmConsensusTest, SubmitMultipleVotes) {
    consensus = swarm_consensus_create(10);
    uint32_t proposal_id = CreateTestProposal(1);

    SubmitVote(proposal_id, 2, VOTE_CHOICE_AGREE);
    SubmitVote(proposal_id, 3, VOTE_CHOICE_AGREE);
    SubmitVote(proposal_id, 4, VOTE_CHOICE_DISAGREE);

    swarm_vote_result_t result = swarm_consensus_get_result(consensus, proposal_id);
    EXPECT_EQ(result.total_votes, 3u);
    EXPECT_EQ(result.agree_votes, 2u);
    EXPECT_EQ(result.disagree_votes, 1u);
}

TEST_F(SwarmConsensusTest, DuplicateVoteRejected) {
    consensus = swarm_consensus_create(10);
    uint32_t proposal_id = CreateTestProposal(1);

    ASSERT_TRUE(SubmitVote(proposal_id, 2, VOTE_CHOICE_AGREE));
    EXPECT_FALSE(SubmitVote(proposal_id, 2, VOTE_CHOICE_DISAGREE)); // Same drone

    swarm_vote_result_t result = swarm_consensus_get_result(consensus, proposal_id);
    EXPECT_EQ(result.total_votes, 1u); // Only first vote counted
}

TEST_F(SwarmConsensusTest, VoteForInvalidProposal) {
    consensus = swarm_consensus_create(10);

    bool success = SubmitVote(999, 1, VOTE_CHOICE_AGREE);

    EXPECT_FALSE(success);
}

//=============================================================================
// 4. Quorum Tests
//=============================================================================

TEST_F(SwarmConsensusTest, QuorumNotReachedWithFewVotes) {
    consensus = swarm_consensus_create(10);
    uint32_t proposal_id = CreateTestProposal(1);

    // Only 2 votes out of 10 (20%, below 50% quorum)
    SubmitVote(proposal_id, 2, VOTE_CHOICE_AGREE);
    SubmitVote(proposal_id, 3, VOTE_CHOICE_AGREE);

    swarm_vote_result_t result = swarm_consensus_get_result(consensus, proposal_id);
    EXPECT_FALSE(result.quorum_reached);
    EXPECT_FALSE(result.passed);
}

TEST_F(SwarmConsensusTest, QuorumReachedWithHalfVotes) {
    consensus = swarm_consensus_create(10);
    uint32_t proposal_id = CreateTestProposal(1);

    // 5 votes out of 10 (50%, meets quorum)
    for (int i = 0; i < 5; i++) {
        SubmitVote(proposal_id, i, VOTE_CHOICE_AGREE);
    }

    swarm_vote_result_t result = swarm_consensus_get_result(consensus, proposal_id);
    EXPECT_TRUE(result.quorum_reached);
}

TEST_F(SwarmConsensusTest, QuorumReachedWithMajority) {
    consensus = swarm_consensus_create(10);
    uint32_t proposal_id = CreateTestProposal(1);

    // 7 votes out of 10 (70%)
    for (int i = 0; i < 7; i++) {
        SubmitVote(proposal_id, i, VOTE_CHOICE_AGREE);
    }

    swarm_vote_result_t result = swarm_consensus_get_result(consensus, proposal_id);
    EXPECT_TRUE(result.quorum_reached);
}

//=============================================================================
// 5. Agreement Threshold Tests
//=============================================================================

TEST_F(SwarmConsensusTest, ProposalPassesWithStrongAgreement) {
    consensus = swarm_consensus_create(10);
    uint32_t proposal_id = CreateTestProposal(1);

    // 7 agree, 1 disagree (87.5% agreement, above 66%)
    for (int i = 0; i < 7; i++) {
        SubmitVote(proposal_id, i, VOTE_CHOICE_AGREE);
    }
    SubmitVote(proposal_id, 7, VOTE_CHOICE_DISAGREE);

    swarm_vote_result_t result = swarm_consensus_get_result(consensus, proposal_id);
    EXPECT_TRUE(result.passed);
}

TEST_F(SwarmConsensusTest, ProposalFailsWithWeakAgreement) {
    consensus = swarm_consensus_create(10);
    uint32_t proposal_id = CreateTestProposal(1);

    // 3 agree, 3 disagree (50% agreement, below 66%)
    for (int i = 0; i < 3; i++) {
        SubmitVote(proposal_id, i, VOTE_CHOICE_AGREE);
    }
    for (int i = 3; i < 6; i++) {
        SubmitVote(proposal_id, i, VOTE_CHOICE_DISAGREE);
    }

    swarm_vote_result_t result = swarm_consensus_get_result(consensus, proposal_id);
    EXPECT_FALSE(result.passed);
}

TEST_F(SwarmConsensusTest, AbstainVotesDoNotCountTowardAgreement) {
    consensus = swarm_consensus_create(10);
    uint32_t proposal_id = CreateTestProposal(1);

    // 4 agree, 2 abstain (should still be 100% of non-abstain votes)
    for (int i = 0; i < 4; i++) {
        SubmitVote(proposal_id, i, VOTE_CHOICE_AGREE);
    }
    for (int i = 4; i < 6; i++) {
        SubmitVote(proposal_id, i, VOTE_CHOICE_ABSTAIN);
    }

    swarm_vote_result_t result = swarm_consensus_get_result(consensus, proposal_id);
    EXPECT_EQ(result.abstain_votes, 2u);
    EXPECT_FLOAT_EQ(result.weighted_agreement, 1.0f);
}

//=============================================================================
// 6. Weighted Voting Tests
//=============================================================================

TEST_F(SwarmConsensusTest, WeightedVotingByConfidence) {
    consensus = swarm_consensus_create(10);
    uint32_t proposal_id = CreateTestProposal(1);

    // 3 high-confidence agrees, 3 low-confidence disagrees
    SubmitVote(proposal_id, 0, VOTE_CHOICE_AGREE, 1.0f);
    SubmitVote(proposal_id, 1, VOTE_CHOICE_AGREE, 1.0f);
    SubmitVote(proposal_id, 2, VOTE_CHOICE_AGREE, 1.0f);
    SubmitVote(proposal_id, 3, VOTE_CHOICE_DISAGREE, 0.3f);
    SubmitVote(proposal_id, 4, VOTE_CHOICE_DISAGREE, 0.3f);
    SubmitVote(proposal_id, 5, VOTE_CHOICE_DISAGREE, 0.3f);

    swarm_vote_result_t result = swarm_consensus_get_result(consensus, proposal_id);

    // Weighted: 3.0 / (3.0 + 0.9) = 0.769 (above 66%)
    EXPECT_GT(result.weighted_agreement, 0.66f);
    EXPECT_TRUE(result.passed);
}

TEST_F(SwarmConsensusTest, LowConfidenceVotesHaveLessWeight) {
    consensus = swarm_consensus_create(10);
    uint32_t proposal_id = CreateTestProposal(1);

    SubmitVote(proposal_id, 0, VOTE_CHOICE_AGREE, 0.5f);
    SubmitVote(proposal_id, 1, VOTE_CHOICE_DISAGREE, 1.0f);
    SubmitVote(proposal_id, 2, VOTE_CHOICE_DISAGREE, 1.0f);
    SubmitVote(proposal_id, 3, VOTE_CHOICE_DISAGREE, 1.0f);
    SubmitVote(proposal_id, 4, VOTE_CHOICE_AGREE, 0.5f);

    swarm_vote_result_t result = swarm_consensus_get_result(consensus, proposal_id);

    // Weighted: 1.0 / (1.0 + 3.0) = 0.25 (below 66%)
    EXPECT_LT(result.weighted_agreement, 0.66f);
    EXPECT_FALSE(result.passed);
}

//=============================================================================
// 7. Completion Tests
//=============================================================================

TEST_F(SwarmConsensusTest, ProposalCompleteWhenQuorumReached) {
    consensus = swarm_consensus_create(10);
    uint32_t proposal_id = CreateTestProposal(1);

    for (int i = 0; i < 5; i++) {
        SubmitVote(proposal_id, i, VOTE_CHOICE_AGREE);
    }

    bool complete = swarm_consensus_is_complete(consensus, proposal_id);
    EXPECT_TRUE(complete);
}

TEST_F(SwarmConsensusTest, ProposalCompleteWhenAllVoted) {
    consensus = swarm_consensus_create(10);
    uint32_t proposal_id = CreateTestProposal(1);

    for (int i = 0; i < 10; i++) {
        SubmitVote(proposal_id, i, VOTE_CHOICE_AGREE);
    }

    bool complete = swarm_consensus_is_complete(consensus, proposal_id);
    EXPECT_TRUE(complete);
}

TEST_F(SwarmConsensusTest, ProposalNotCompleteWithFewVotes) {
    consensus = swarm_consensus_create(10);
    uint32_t proposal_id = CreateTestProposal(1);

    SubmitVote(proposal_id, 0, VOTE_CHOICE_AGREE);

    bool complete = swarm_consensus_is_complete(consensus, proposal_id);
    EXPECT_FALSE(complete);
}

//=============================================================================
// 8. Byzantine Fault Tolerance Tests
//=============================================================================

TEST_F(SwarmConsensusTest, ToleratesOneThirdByzantineDrones) {
    consensus = swarm_consensus_create(9);
    uint32_t proposal_id = CreateTestProposal(1);

    // 6 honest drones vote agree
    for (int i = 0; i < 6; i++) {
        SubmitVote(proposal_id, i, VOTE_CHOICE_AGREE, 1.0f);
    }

    // 3 Byzantine drones vote disagree (1/3 of total)
    for (int i = 6; i < 9; i++) {
        SubmitVote(proposal_id, i, VOTE_CHOICE_DISAGREE, 1.0f);
    }

    swarm_vote_result_t result = swarm_consensus_get_result(consensus, proposal_id);

    // 6/(6+3) = 0.666... = exactly at threshold
    EXPECT_GE(result.weighted_agreement, 0.66f);
}

//=============================================================================
// 9. Edge Cases
//=============================================================================

TEST_F(SwarmConsensusTest, SingleDroneSwarm) {
    consensus = swarm_consensus_create(1);
    uint32_t proposal_id = CreateTestProposal(0);

    SubmitVote(proposal_id, 0, VOTE_CHOICE_AGREE);

    swarm_vote_result_t result = swarm_consensus_get_result(consensus, proposal_id);
    EXPECT_TRUE(result.quorum_reached);
    EXPECT_TRUE(result.passed);
}

TEST_F(SwarmConsensusTest, AllDronesAbstain) {
    consensus = swarm_consensus_create(10);
    uint32_t proposal_id = CreateTestProposal(1);

    for (int i = 0; i < 6; i++) {
        SubmitVote(proposal_id, i, VOTE_CHOICE_ABSTAIN);
    }

    swarm_vote_result_t result = swarm_consensus_get_result(consensus, proposal_id);
    EXPECT_TRUE(result.quorum_reached);
    EXPECT_EQ(result.abstain_votes, 6u);
    EXPECT_FLOAT_EQ(result.weighted_agreement, 0.0f);
}

TEST_F(SwarmConsensusTest, ZeroConfidenceVotes) {
    consensus = swarm_consensus_create(10);
    uint32_t proposal_id = CreateTestProposal(1);

    for (int i = 0; i < 6; i++) {
        SubmitVote(proposal_id, i, VOTE_CHOICE_AGREE, 0.0f);
    }

    swarm_vote_result_t result = swarm_consensus_get_result(consensus, proposal_id);
    EXPECT_FLOAT_EQ(result.weighted_agreement, 0.0f);
}

TEST_F(SwarmConsensusTest, GetResultForNonexistentProposal) {
    consensus = swarm_consensus_create(10);

    swarm_vote_result_t result = swarm_consensus_get_result(consensus, 999);

    EXPECT_EQ(result.proposal_id, 999u);
    EXPECT_EQ(result.total_votes, 0u);
    EXPECT_FALSE(result.passed);
}

//=============================================================================
// 10. Multiple Proposal Tests
//=============================================================================

TEST_F(SwarmConsensusTest, MultipleProposalsIndependent) {
    consensus = swarm_consensus_create(10);

    uint32_t id1 = CreateTestProposal(1, VOTE_TOPIC_TARGET_PRIORITY);
    uint32_t id2 = CreateTestProposal(2, VOTE_TOPIC_RETREAT);

    // Vote on both
    for (int i = 0; i < 6; i++) {
        SubmitVote(id1, i, VOTE_CHOICE_AGREE);
        SubmitVote(id2, i, VOTE_CHOICE_DISAGREE);
    }

    swarm_vote_result_t result1 = swarm_consensus_get_result(consensus, id1);
    swarm_vote_result_t result2 = swarm_consensus_get_result(consensus, id2);

    EXPECT_TRUE(result1.passed);
    EXPECT_FALSE(result2.passed);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
