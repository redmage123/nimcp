/**
 * @file test_quantum_consensus.cpp
 * @brief Unit tests for quantum-enhanced swarm consensus
 *
 * Tests cover:
 * - Context lifecycle
 * - Proposal creation and voting
 * - Grover-inspired consensus algorithm
 * - Byzantine fault tolerance
 * - Collusion detection
 * - Statistics tracking
 */

#include <gtest/gtest.h>
#include <cmath>

// Headers have their own extern "C" guards
#include "swarm/nimcp_quantum_consensus.h"

//=============================================================================
// Lifecycle Tests
//=============================================================================

class QuantumConsensusLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx = nullptr;
    }

    void TearDown() override {
        if (ctx) {
            quantum_consensus_destroy(ctx);
            ctx = nullptr;
        }
    }

    quantum_consensus_t ctx;
};

TEST_F(QuantumConsensusLifecycleTest, CreateDestroy) {
    ctx = quantum_consensus_create(nullptr);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->magic, QUANTUM_CONSENSUS_MAGIC);
    EXPECT_NE(ctx->proposals, nullptr);
    EXPECT_NE(ctx->amplitudes, nullptr);
    EXPECT_NE(ctx->phases, nullptr);
    EXPECT_NE(ctx->vote_states, nullptr);
}

TEST_F(QuantumConsensusLifecycleTest, CreateWithConfig) {
    quantum_consensus_config_t config = quantum_consensus_default_config();
    config.max_voters = 128;
    config.algorithm = QCONSENSUS_HYBRID;
    config.agreement_threshold = 0.75f;

    ctx = quantum_consensus_create(&config);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->config.max_voters, 128u);
    EXPECT_EQ(ctx->config.algorithm, QCONSENSUS_HYBRID);
    EXPECT_FLOAT_EQ(ctx->config.agreement_threshold, 0.75f);
}

TEST_F(QuantumConsensusLifecycleTest, DefaultConfig) {
    quantum_consensus_config_t config = quantum_consensus_default_config();

    EXPECT_EQ(config.algorithm, QCONSENSUS_GROVER);
    EXPECT_EQ(config.max_voters, 256u);
    EXPECT_EQ(config.grover_iterations, 0u);  /* Auto */
    EXPECT_FLOAT_EQ(config.agreement_threshold, QUANTUM_CONSENSUS_DEFAULT_THRESHOLD);
    EXPECT_TRUE(config.enable_amplitude_weighting);
    EXPECT_TRUE(config.enable_collusion_detection);
}

TEST_F(QuantumConsensusLifecycleTest, DestroyNull) {
    /* Should not crash */
    quantum_consensus_destroy(nullptr);
}

//=============================================================================
// Proposal Tests
//=============================================================================

class QuantumConsensusProposalTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx = quantum_consensus_create(nullptr);
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        if (ctx) {
            quantum_consensus_destroy(ctx);
            ctx = nullptr;
        }
    }

    quantum_consensus_t ctx;
};

TEST_F(QuantumConsensusProposalTest, CreateProposal) {
    uint32_t proposal_id;
    int result = quantum_consensus_propose(ctx, 1, "Test Proposal", 1.0f, 0, &proposal_id);

    EXPECT_EQ(result, QCONSENSUS_OK);
    EXPECT_EQ(proposal_id, 0u);
    EXPECT_EQ(quantum_consensus_get_proposal_count(ctx), 1u);
}

TEST_F(QuantumConsensusProposalTest, CreateMultipleProposals) {
    uint32_t id1, id2, id3;

    EXPECT_EQ(quantum_consensus_propose(ctx, 1, "Proposal 1", 1.0f, 0, &id1), QCONSENSUS_OK);
    EXPECT_EQ(quantum_consensus_propose(ctx, 2, "Proposal 2", 2.0f, 0, &id2), QCONSENSUS_OK);
    EXPECT_EQ(quantum_consensus_propose(ctx, 3, "Proposal 3", 3.0f, 0, &id3), QCONSENSUS_OK);

    EXPECT_EQ(id1, 0u);
    EXPECT_EQ(id2, 1u);
    EXPECT_EQ(id3, 2u);
    EXPECT_EQ(quantum_consensus_get_proposal_count(ctx), 3u);
}

TEST_F(QuantumConsensusProposalTest, ProposalStatus) {
    uint32_t proposal_id;
    quantum_consensus_propose(ctx, 1, "Test", 1.0f, 0, &proposal_id);

    EXPECT_EQ(quantum_consensus_get_status(ctx, proposal_id), QPROPOSAL_PENDING);
}

TEST_F(QuantumConsensusProposalTest, VoteCount) {
    uint32_t proposal_id;
    quantum_consensus_propose(ctx, 1, "Test", 1.0f, 0, &proposal_id);

    EXPECT_EQ(quantum_consensus_get_vote_count(ctx, proposal_id), 0u);

    quantum_consensus_vote(ctx, proposal_id, 1, QVOTE_AGREE, 0.9f);
    EXPECT_EQ(quantum_consensus_get_vote_count(ctx, proposal_id), 1u);

    quantum_consensus_vote(ctx, proposal_id, 2, QVOTE_DISAGREE, 0.8f);
    EXPECT_EQ(quantum_consensus_get_vote_count(ctx, proposal_id), 2u);
}

//=============================================================================
// Voting Tests
//=============================================================================

class QuantumConsensusVotingTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx = quantum_consensus_create(nullptr);
        ASSERT_NE(ctx, nullptr);

        quantum_consensus_propose(ctx, 1, "Test Proposal", 1.0f, 0, &proposal_id);
    }

    void TearDown() override {
        if (ctx) {
            quantum_consensus_destroy(ctx);
            ctx = nullptr;
        }
    }

    quantum_consensus_t ctx;
    uint32_t proposal_id;
};

TEST_F(QuantumConsensusVotingTest, SingleAgreeVote) {
    int result = quantum_consensus_vote(ctx, proposal_id, 1, QVOTE_AGREE, 1.0f);
    EXPECT_EQ(result, QCONSENSUS_OK);
    EXPECT_EQ(quantum_consensus_get_vote_count(ctx, proposal_id), 1u);
}

TEST_F(QuantumConsensusVotingTest, SingleDisagreeVote) {
    int result = quantum_consensus_vote(ctx, proposal_id, 1, QVOTE_DISAGREE, 1.0f);
    EXPECT_EQ(result, QCONSENSUS_OK);
    EXPECT_EQ(quantum_consensus_get_vote_count(ctx, proposal_id), 1u);
}

TEST_F(QuantumConsensusVotingTest, SingleAbstainVote) {
    int result = quantum_consensus_vote(ctx, proposal_id, 1, QVOTE_ABSTAIN, 1.0f);
    EXPECT_EQ(result, QCONSENSUS_OK);
    EXPECT_EQ(quantum_consensus_get_vote_count(ctx, proposal_id), 1u);
}

TEST_F(QuantumConsensusVotingTest, MultipleVotes) {
    quantum_consensus_vote(ctx, proposal_id, 1, QVOTE_AGREE, 0.9f);
    quantum_consensus_vote(ctx, proposal_id, 2, QVOTE_AGREE, 0.8f);
    quantum_consensus_vote(ctx, proposal_id, 3, QVOTE_DISAGREE, 0.7f);
    quantum_consensus_vote(ctx, proposal_id, 4, QVOTE_ABSTAIN, 0.5f);

    EXPECT_EQ(quantum_consensus_get_vote_count(ctx, proposal_id), 4u);
}

TEST_F(QuantumConsensusVotingTest, DuplicateVoteUpdates) {
    quantum_consensus_vote(ctx, proposal_id, 1, QVOTE_AGREE, 0.9f);
    EXPECT_EQ(quantum_consensus_get_vote_count(ctx, proposal_id), 1u);

    /* Same voter, different vote */
    quantum_consensus_vote(ctx, proposal_id, 1, QVOTE_DISAGREE, 0.8f);
    EXPECT_EQ(quantum_consensus_get_vote_count(ctx, proposal_id), 1u);  /* Still 1 */
}

TEST_F(QuantumConsensusVotingTest, LowConfidenceIgnored) {
    /* Vote below minimum confidence threshold */
    quantum_consensus_vote(ctx, proposal_id, 1, QVOTE_AGREE, 0.05f);
    EXPECT_EQ(quantum_consensus_get_vote_count(ctx, proposal_id), 0u);
}

TEST_F(QuantumConsensusVotingTest, ConfidenceClamping) {
    /* Negative confidence */
    quantum_consensus_vote(ctx, proposal_id, 1, QVOTE_AGREE, -0.5f);
    /* Should be clamped to 0, which is below threshold */
    EXPECT_EQ(quantum_consensus_get_vote_count(ctx, proposal_id), 0u);

    /* High confidence */
    quantum_consensus_vote(ctx, proposal_id, 2, QVOTE_AGREE, 1.5f);
    /* Should be clamped to 1.0 */
    EXPECT_EQ(quantum_consensus_get_vote_count(ctx, proposal_id), 1u);
}

TEST_F(QuantumConsensusVotingTest, VoteOnInvalidProposal) {
    int result = quantum_consensus_vote(ctx, 999, 1, QVOTE_AGREE, 1.0f);
    EXPECT_EQ(result, QCONSENSUS_ERR_NOT_FOUND);
}

//=============================================================================
// Consensus Algorithm Tests
//=============================================================================

class QuantumConsensusAlgorithmTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx = quantum_consensus_create(nullptr);
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        if (ctx) {
            quantum_consensus_destroy(ctx);
            ctx = nullptr;
        }
    }

    quantum_consensus_t ctx;
};

TEST_F(QuantumConsensusAlgorithmTest, UnanimousAgree) {
    uint32_t proposal_id;
    quantum_consensus_propose(ctx, 1, "Unanimous", 1.0f, 0, &proposal_id);

    for (uint32_t i = 0; i < 10; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_AGREE, 1.0f);
    }

    quantum_consensus_result_t result;
    int status = quantum_consensus_run(ctx, proposal_id, &result);

    EXPECT_EQ(status, QCONSENSUS_OK);
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.agree_count, 10u);
    EXPECT_EQ(result.disagree_count, 0u);
    EXPECT_EQ(result.abstain_count, 0u);
    EXPECT_FLOAT_EQ(result.weighted_agreement, 1.0f);
    EXPECT_GE(result.amplitude_agreement, 0.9f);  /* Should be high */
}

TEST_F(QuantumConsensusAlgorithmTest, UnanimousDisagree) {
    uint32_t proposal_id;
    quantum_consensus_propose(ctx, 1, "Unanimous Disagree", 1.0f, 0, &proposal_id);

    for (uint32_t i = 0; i < 10; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_DISAGREE, 1.0f);
    }

    quantum_consensus_result_t result;
    int status = quantum_consensus_run(ctx, proposal_id, &result);

    EXPECT_EQ(status, QCONSENSUS_OK);
    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.agree_count, 0u);
    EXPECT_EQ(result.disagree_count, 10u);
    EXPECT_FLOAT_EQ(result.weighted_agreement, 0.0f);
}

TEST_F(QuantumConsensusAlgorithmTest, MajorityAgree) {
    uint32_t proposal_id;
    quantum_consensus_propose(ctx, 1, "Majority", 1.0f, 0, &proposal_id);

    /* 7 agree, 3 disagree = 70% agree */
    for (uint32_t i = 0; i < 7; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_AGREE, 1.0f);
    }
    for (uint32_t i = 7; i < 10; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_DISAGREE, 1.0f);
    }

    quantum_consensus_result_t result;
    quantum_consensus_run(ctx, proposal_id, &result);

    EXPECT_TRUE(result.passed);  /* 70% > 66.7% threshold */
    EXPECT_EQ(result.agree_count, 7u);
    EXPECT_EQ(result.disagree_count, 3u);
}

TEST_F(QuantumConsensusAlgorithmTest, MinorityAgree) {
    uint32_t proposal_id;
    quantum_consensus_propose(ctx, 1, "Minority", 1.0f, 0, &proposal_id);

    /* 3 agree, 7 disagree = 30% agree */
    for (uint32_t i = 0; i < 3; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_AGREE, 1.0f);
    }
    for (uint32_t i = 3; i < 10; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_DISAGREE, 1.0f);
    }

    quantum_consensus_result_t result;
    quantum_consensus_run(ctx, proposal_id, &result);

    EXPECT_FALSE(result.passed);  /* 30% < 66.7% threshold */
}

TEST_F(QuantumConsensusAlgorithmTest, WeightedVotes) {
    uint32_t proposal_id;
    quantum_consensus_propose(ctx, 1, "Weighted", 1.0f, 0, &proposal_id);

    /* 3 high-confidence agrees, 7 low-confidence disagrees */
    for (uint32_t i = 0; i < 3; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_AGREE, 1.0f);
    }
    for (uint32_t i = 3; i < 10; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_DISAGREE, 0.2f);
    }

    quantum_consensus_result_t result;
    quantum_consensus_run(ctx, proposal_id, &result);

    /* Weight: 3*1.0 = 3.0 agree, 7*0.2 = 1.4 disagree */
    /* Weighted agreement = 3.0 / 4.4 ≈ 0.68 > 0.667 */
    EXPECT_TRUE(result.passed);
    EXPECT_GT(result.weighted_agreement, 0.6f);
}

TEST_F(QuantumConsensusAlgorithmTest, NoVotes) {
    uint32_t proposal_id;
    quantum_consensus_propose(ctx, 1, "Empty", 1.0f, 0, &proposal_id);

    quantum_consensus_result_t result;
    quantum_consensus_run(ctx, proposal_id, &result);

    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.agree_count, 0u);
}

TEST_F(QuantumConsensusAlgorithmTest, GroverIterations) {
    uint32_t proposal_id;
    quantum_consensus_propose(ctx, 1, "Grover Test", 1.0f, 0, &proposal_id);

    for (uint32_t i = 0; i < 16; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_AGREE, 0.8f);
    }

    quantum_consensus_result_t result;
    quantum_consensus_run(ctx, proposal_id, &result);

    /* Should have used Grover iterations */
    EXPECT_GT(result.grover_rounds, 0u);
}

//=============================================================================
// Byzantine Fault Tolerance Tests
//=============================================================================

class QuantumConsensusBFTTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx = quantum_consensus_create(nullptr);
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        if (ctx) {
            quantum_consensus_destroy(ctx);
            ctx = nullptr;
        }
    }

    quantum_consensus_t ctx;
};

TEST_F(QuantumConsensusBFTTest, TooManyAbstentions) {
    uint32_t proposal_id;
    quantum_consensus_propose(ctx, 1, "BFT Test", 1.0f, 0, &proposal_id);

    /* More than 1/3 abstain */
    for (uint32_t i = 0; i < 5; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_AGREE, 1.0f);
    }
    for (uint32_t i = 5; i < 10; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_ABSTAIN, 1.0f);
    }

    quantum_consensus_result_t result;
    int status = quantum_consensus_run(ctx, proposal_id, &result);

    EXPECT_EQ(status, QCONSENSUS_ERR_BFT_VIOLATION);
    EXPECT_FALSE(result.passed);
}

TEST_F(QuantumConsensusBFTTest, AcceptableAbstentions) {
    uint32_t proposal_id;
    quantum_consensus_propose(ctx, 1, "BFT OK", 1.0f, 0, &proposal_id);

    /* Less than 1/3 abstain */
    for (uint32_t i = 0; i < 7; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_AGREE, 1.0f);
    }
    for (uint32_t i = 7; i < 9; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_ABSTAIN, 1.0f);
    }
    quantum_consensus_vote(ctx, proposal_id, 9, QVOTE_DISAGREE, 1.0f);

    quantum_consensus_result_t result;
    int status = quantum_consensus_run(ctx, proposal_id, &result);

    EXPECT_EQ(status, QCONSENSUS_OK);
    EXPECT_TRUE(result.passed);  /* 7/8 real votes agree */
}

TEST_F(QuantumConsensusBFTTest, BFTScoreCalculation) {
    uint32_t proposal_id;
    quantum_consensus_propose(ctx, 1, "BFT Score", 1.0f, 0, &proposal_id);

    for (uint32_t i = 0; i < 8; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_AGREE, 1.0f);
    }
    for (uint32_t i = 8; i < 10; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_ABSTAIN, 1.0f);
    }

    quantum_consensus_result_t result;
    quantum_consensus_run(ctx, proposal_id, &result);

    /* 2/10 abstain = 0.2 fault ratio, score = 0.8 */
    EXPECT_NEAR(result.bft_score, 0.8f, 0.01f);
}

//=============================================================================
// Collusion Detection Tests
//=============================================================================

class QuantumConsensusCollusionTest : public ::testing::Test {
protected:
    void SetUp() override {
        quantum_consensus_config_t config = quantum_consensus_default_config();
        config.enable_collusion_detection = true;
        ctx = quantum_consensus_create(&config);
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        if (ctx) {
            quantum_consensus_destroy(ctx);
            ctx = nullptr;
        }
    }

    quantum_consensus_t ctx;
};

TEST_F(QuantumConsensusCollusionTest, IdenticalConfidences) {
    uint32_t proposal_id;
    quantum_consensus_propose(ctx, 1, "Collusion Test", 1.0f, 0, &proposal_id);

    /* All votes with identical confidence = suspicious */
    for (uint32_t i = 0; i < 10; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_AGREE, 0.75f);
    }

    quantum_consensus_result_t result;
    quantum_consensus_run(ctx, proposal_id, &result);

    EXPECT_TRUE(result.collusion_detected);
}

TEST_F(QuantumConsensusCollusionTest, VariedConfidences) {
    uint32_t proposal_id;
    quantum_consensus_propose(ctx, 1, "No Collusion", 1.0f, 0, &proposal_id);

    /* Varied confidences = not suspicious */
    quantum_consensus_vote(ctx, proposal_id, 0, QVOTE_AGREE, 0.9f);
    quantum_consensus_vote(ctx, proposal_id, 1, QVOTE_AGREE, 0.8f);
    quantum_consensus_vote(ctx, proposal_id, 2, QVOTE_AGREE, 0.7f);
    quantum_consensus_vote(ctx, proposal_id, 3, QVOTE_DISAGREE, 0.6f);
    quantum_consensus_vote(ctx, proposal_id, 4, QVOTE_AGREE, 0.95f);

    quantum_consensus_result_t result;
    quantum_consensus_run(ctx, proposal_id, &result);

    EXPECT_FALSE(result.collusion_detected);
}

TEST_F(QuantumConsensusCollusionTest, PartialIdentical) {
    uint32_t proposal_id;
    quantum_consensus_propose(ctx, 1, "Partial Collusion", 1.0f, 0, &proposal_id);

    /* 4 identical (40%), 6 varied = not enough to trigger */
    for (uint32_t i = 0; i < 4; i++) {
        quantum_consensus_vote(ctx, proposal_id, i, QVOTE_AGREE, 0.75f);
    }
    quantum_consensus_vote(ctx, proposal_id, 4, QVOTE_AGREE, 0.9f);
    quantum_consensus_vote(ctx, proposal_id, 5, QVOTE_AGREE, 0.85f);
    quantum_consensus_vote(ctx, proposal_id, 6, QVOTE_AGREE, 0.8f);
    quantum_consensus_vote(ctx, proposal_id, 7, QVOTE_DISAGREE, 0.6f);
    quantum_consensus_vote(ctx, proposal_id, 8, QVOTE_AGREE, 0.95f);
    quantum_consensus_vote(ctx, proposal_id, 9, QVOTE_AGREE, 0.7f);

    quantum_consensus_result_t result;
    quantum_consensus_run(ctx, proposal_id, &result);

    /* 4 identical out of 10 = 40% > 33%, should detect */
    EXPECT_TRUE(result.collusion_detected);
}

//=============================================================================
// Statistics Tests
//=============================================================================

class QuantumConsensusStatsTest : public ::testing::Test {
protected:
    void SetUp() override {
        ctx = quantum_consensus_create(nullptr);
        ASSERT_NE(ctx, nullptr);
    }

    void TearDown() override {
        if (ctx) {
            quantum_consensus_destroy(ctx);
            ctx = nullptr;
        }
    }

    quantum_consensus_t ctx;
};

TEST_F(QuantumConsensusStatsTest, InitialStats) {
    quantum_consensus_stats_t stats;
    quantum_consensus_get_stats(ctx, &stats);

    EXPECT_EQ(stats.total_proposals, 0u);
    EXPECT_EQ(stats.proposals_passed, 0u);
    EXPECT_EQ(stats.proposals_failed, 0u);
    EXPECT_EQ(stats.total_votes, 0u);
}

TEST_F(QuantumConsensusStatsTest, ProposalStats) {
    uint32_t id1, id2;
    quantum_consensus_propose(ctx, 1, "Test 1", 1.0f, 0, &id1);
    quantum_consensus_propose(ctx, 2, "Test 2", 2.0f, 0, &id2);

    /* First proposal passes */
    for (uint32_t i = 0; i < 10; i++) {
        quantum_consensus_vote(ctx, id1, i, QVOTE_AGREE, 1.0f);
    }
    quantum_consensus_result_t result;
    quantum_consensus_run(ctx, id1, &result);

    /* Second proposal fails */
    for (uint32_t i = 0; i < 10; i++) {
        quantum_consensus_vote(ctx, id2, i, QVOTE_DISAGREE, 1.0f);
    }
    quantum_consensus_run(ctx, id2, &result);

    quantum_consensus_stats_t stats;
    quantum_consensus_get_stats(ctx, &stats);

    EXPECT_EQ(stats.total_proposals, 2u);
    EXPECT_EQ(stats.proposals_passed, 1u);
    EXPECT_EQ(stats.proposals_failed, 1u);
    EXPECT_EQ(stats.total_votes, 20u);
}

TEST_F(QuantumConsensusStatsTest, ResetStats) {
    uint32_t id;
    quantum_consensus_propose(ctx, 1, "Test", 1.0f, 0, &id);
    quantum_consensus_vote(ctx, id, 1, QVOTE_AGREE, 1.0f);

    quantum_consensus_stats_t stats;
    quantum_consensus_get_stats(ctx, &stats);
    EXPECT_GT(stats.total_proposals, 0u);

    quantum_consensus_reset_stats(ctx);
    quantum_consensus_get_stats(ctx, &stats);
    EXPECT_EQ(stats.total_proposals, 0u);
    EXPECT_EQ(stats.total_votes, 0u);
}

TEST_F(QuantumConsensusStatsTest, GroverAmplifications) {
    uint32_t id;
    quantum_consensus_propose(ctx, 1, "Grover", 1.0f, 0, &id);

    for (uint32_t i = 0; i < 16; i++) {
        quantum_consensus_vote(ctx, id, i, QVOTE_AGREE, 0.8f);
    }

    quantum_consensus_result_t result;
    quantum_consensus_run(ctx, id, &result);

    quantum_consensus_stats_t stats;
    quantum_consensus_get_stats(ctx, &stats);

    EXPECT_GT(stats.grover_amplifications, 0u);
}

//=============================================================================
// Edge Cases
//=============================================================================

TEST(QuantumConsensusEdgeCases, NullContext) {
    EXPECT_EQ(quantum_consensus_get_proposal_count(nullptr), 0u);
    EXPECT_EQ(quantum_consensus_get_vote_count(nullptr, 0), 0u);
    EXPECT_EQ(quantum_consensus_get_status(nullptr, 0), QPROPOSAL_PENDING);

    quantum_consensus_stats_t stats;
    EXPECT_EQ(quantum_consensus_get_stats(nullptr, &stats), QCONSENSUS_ERR_NULL);

    quantum_consensus_result_t result;
    EXPECT_EQ(quantum_consensus_run(nullptr, 0, &result), QCONSENSUS_ERR_NULL);

    quantum_consensus_reset_stats(nullptr);  /* Should not crash */
}

TEST(QuantumConsensusEdgeCases, InvalidProposalId) {
    quantum_consensus_t ctx = quantum_consensus_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(quantum_consensus_get_vote_count(ctx, 999), 0u);

    quantum_consensus_result_t result;
    EXPECT_EQ(quantum_consensus_run(ctx, 999, &result), QCONSENSUS_ERR_NOT_FOUND);

    quantum_consensus_destroy(ctx);
}

TEST(QuantumConsensusEdgeCases, TritConversion) {
    EXPECT_EQ(qcons_choice_to_trit(QVOTE_AGREE), TRIT_POSITIVE);
    EXPECT_EQ(qcons_choice_to_trit(QVOTE_DISAGREE), TRIT_NEGATIVE);
    EXPECT_EQ(qcons_choice_to_trit(QVOTE_ABSTAIN), TRIT_UNKNOWN);

    EXPECT_EQ(qcons_trit_to_choice(TRIT_POSITIVE), QVOTE_AGREE);
    EXPECT_EQ(qcons_trit_to_choice(TRIT_NEGATIVE), QVOTE_DISAGREE);
    EXPECT_EQ(qcons_trit_to_choice(TRIT_UNKNOWN), QVOTE_ABSTAIN);
}

TEST(QuantumConsensusEdgeCases, FreeResult) {
    quantum_consensus_result_t result;
    memset(&result, 0xFF, sizeof(result));  /* Fill with garbage */

    quantum_consensus_free_result(&result);

    EXPECT_EQ(result.proposal_id, 0u);
    EXPECT_FALSE(result.passed);
}

TEST(QuantumConsensusEdgeCases, SingleVoter) {
    quantum_consensus_t ctx = quantum_consensus_create(nullptr);
    ASSERT_NE(ctx, nullptr);

    uint32_t id;
    quantum_consensus_propose(ctx, 1, "Single Voter", 1.0f, 0, &id);
    quantum_consensus_vote(ctx, id, 1, QVOTE_AGREE, 1.0f);

    quantum_consensus_result_t result;
    quantum_consensus_run(ctx, id, &result);

    /* Single agree vote with 100% confidence should pass */
    EXPECT_TRUE(result.passed);
    EXPECT_FLOAT_EQ(result.weighted_agreement, 1.0f);

    quantum_consensus_destroy(ctx);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
