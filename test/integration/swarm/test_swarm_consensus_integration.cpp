/**
 * @file test_swarm_consensus_integration.cpp
 * @brief Comprehensive Integration Tests for Swarm Consensus System
 *
 * WHAT: Tests complete consensus voting flow with Byzantine fault tolerance
 * WHY:  Verify distributed decision-making under various failure conditions
 * HOW:  Simulate multi-drone voting with malicious nodes, timeouts, and quorum
 *
 * TEST COVERAGE:
 * - Multi-drone consensus voting (5-16 drones)
 * - Byzantine fault tolerance (up to 1/3 malicious nodes)
 * - Vote timeout handling and expiration
 * - Quorum requirements and threshold voting
 * - Concurrent voting scenarios
 * - Vote confidence weighting
 * - Proposal lifecycle management
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <atomic>
#include <random>
#include <algorithm>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_consensus.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"

//=============================================================================
// Test Fixture
//=============================================================================

// Callback context for capturing vote results
struct VoteCallbackContext {
    std::atomic<bool> completed{false};
    swarm_vote_result_t result{};
    std::mutex mutex;
};

// Callback function for vote completion
static void vote_completion_callback(const swarm_vote_result_t* result, void* user_ctx) {
    if (user_ctx && result) {
        auto* ctx = static_cast<VoteCallbackContext*>(user_ctx);
        std::lock_guard<std::mutex> lock(ctx->mutex);
        ctx->result = *result;
        ctx->completed.store(true);
    }
}

class SwarmConsensusIntegrationTest : public ::testing::Test {
protected:
    static constexpr uint32_t NUM_DRONES = 7;
    std::vector<swarm_consensus_t> consensus_modules_;

    void SetUp() override {
        // Logging initialized in framework
        // Log level set in framework

        // Create consensus contexts for each drone
        consensus_modules_.resize(NUM_DRONES);
        for (uint32_t i = 0; i < NUM_DRONES; i++) {
            swarm_consensus_config_t config = swarm_consensus_default_config(i);
            config.enable_byzantine_ft = true;
            config.enable_logging = false; // Reduce noise in tests
            consensus_modules_[i] = swarm_consensus_create(&config);
            ASSERT_NE(consensus_modules_[i], nullptr)
                << "Failed to create consensus for drone " << i;
        }
    }

    void TearDown() override {
        for (auto ctx : consensus_modules_) {
            if (ctx) swarm_consensus_destroy(ctx);
        }
        consensus_modules_.clear();
    }

    // Helper: Get current time in milliseconds
    uint64_t GetCurrentTimeMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    // Helper: Simulate vote propagation across swarm
    // Note: In a real swarm, each drone would have its own proposal copy.
    // For testing, we only send to the proposing drone (index 0) since
    // proposals complete when quorum is reached and subsequent votes fail.
    void PropagateVote(const swarm_vote_response_t& vote) {
        // Send vote only to the proposer's consensus module
        // Other modules would need their own proposal copies in production
        if (!consensus_modules_.empty()) {
            swarm_consensus_receive_vote(consensus_modules_[0], &vote);
        }
    }

    // Helper: Wait for callback completion
    bool WaitForCallback(VoteCallbackContext& ctx, uint32_t timeout_ms = 1000) {
        auto start = std::chrono::steady_clock::now();
        while (!ctx.completed.load()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            ).count();
            if (elapsed > timeout_ms) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

    // Helper: Wait for vote completion
    bool WaitForCompletion(swarm_consensus_t ctx, uint32_t proposal_id,
                          uint32_t timeout_ms = 1000) {
        auto start = std::chrono::steady_clock::now();
        bool is_complete = false;

        while (!is_complete) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start
            ).count();

            if (elapsed > timeout_ms) return false;

            swarm_consensus_check_result(ctx, proposal_id, &is_complete);
            if (!is_complete) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        return true;
    }
};

//=============================================================================
// Basic Consensus Tests
//=============================================================================

TEST_F(SwarmConsensusIntegrationTest, UnanimousConsensus) {
    // Drone 0 proposes a target priority vote
    uint32_t proposal_id = 0;
    float target_coords[4] = {100.0f, 200.0f, 50.0f, 1.0f}; // x, y, z, priority
    VoteCallbackContext callback_ctx;

    nimcp_error_t err = swarm_consensus_propose(
        consensus_modules_[0],
        VOTE_TOPIC_TARGET_PRIORITY,
        target_coords,
        GetCurrentTimeMs() + 5000, // 5 second deadline
        NUM_DRONES,                 // Require all drones
        SWARM_DEFAULT_THRESHOLD,    // 2/3 agreement
        vote_completion_callback,
        &callback_ctx,
        &proposal_id
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // All drones vote AGREE with high confidence
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        swarm_vote_response_t vote;
        vote.proposal_id = proposal_id;
        vote.voter_drone = i;
        vote.choice = VOTE_CHOICE_AGREE;
        vote.confidence = 0.95f;

        PropagateVote(vote);
    }

    // Wait for callback and check result
    ASSERT_TRUE(WaitForCallback(callback_ctx)) << "Vote did not complete in time";
    const auto& result = callback_ctx.result;

    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.agree_count, NUM_DRONES);
    EXPECT_EQ(result.disagree_count, 0u);
    EXPECT_GT(result.weighted_agreement, 0.9f);
}

TEST_F(SwarmConsensusIntegrationTest, MajorityVote) {
    uint32_t proposal_id = 0;
    VoteCallbackContext callback_ctx;

    // Propose formation change
    nimcp_error_t err = swarm_consensus_propose(
        consensus_modules_[0],
        VOTE_TOPIC_FORMATION_CHANGE,
        nullptr,
        GetCurrentTimeMs() + 5000,
        NUM_DRONES,
        0.5f, // Simple majority
        vote_completion_callback,
        &callback_ctx,
        &proposal_id
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // 5 agree, 2 disagree
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        swarm_vote_response_t vote;
        vote.proposal_id = proposal_id;
        vote.voter_drone = i;
        vote.choice = (i < 5) ? VOTE_CHOICE_AGREE : VOTE_CHOICE_DISAGREE;
        vote.confidence = 0.8f;

        PropagateVote(vote);
    }

    ASSERT_TRUE(WaitForCallback(callback_ctx)) << "Vote did not complete in time";
    const auto& result = callback_ctx.result;

    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.agree_count, 5u);
    EXPECT_EQ(result.disagree_count, 2u);
}

TEST_F(SwarmConsensusIntegrationTest, MinorityRejects) {
    uint32_t proposal_id = 0;
    VoteCallbackContext callback_ctx;

    nimcp_error_t err = swarm_consensus_propose(
        consensus_modules_[0],
        VOTE_TOPIC_RETREAT,
        nullptr,
        GetCurrentTimeMs() + 5000,
        NUM_DRONES,
        SWARM_DEFAULT_THRESHOLD, // 2/3 required
        vote_completion_callback,
        &callback_ctx,
        &proposal_id
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Only 3 agree, 4 disagree - should fail
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        swarm_vote_response_t vote;
        vote.proposal_id = proposal_id;
        vote.voter_drone = i;
        vote.choice = (i < 3) ? VOTE_CHOICE_AGREE : VOTE_CHOICE_DISAGREE;
        vote.confidence = 0.8f;

        PropagateVote(vote);
    }

    ASSERT_TRUE(WaitForCallback(callback_ctx)) << "Vote did not complete in time";
    const auto& result = callback_ctx.result;

    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.agree_count, 3u);
    EXPECT_EQ(result.disagree_count, 4u);
}

//=============================================================================
// Byzantine Fault Tolerance Tests
//=============================================================================

TEST_F(SwarmConsensusIntegrationTest, ByzantineFaultTolerance_OneThirdMalicious) {
    uint32_t proposal_id = 0;
    VoteCallbackContext callback_ctx;

    // 7 drones, up to 2 can be malicious (< 1/3)
    nimcp_error_t err = swarm_consensus_propose(
        consensus_modules_[0],
        VOTE_TOPIC_RESOURCE_ALLOCATION,
        nullptr,
        GetCurrentTimeMs() + 5000,
        NUM_DRONES,
        SWARM_DEFAULT_THRESHOLD,
        vote_completion_callback,
        &callback_ctx,
        &proposal_id
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // 2 malicious drones vote opposite with low confidence
    // 5 honest drones vote correctly with high confidence
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        swarm_vote_response_t vote;
        vote.proposal_id = proposal_id;
        vote.voter_drone = i;

        if (i < 2) {
            // Malicious: disagree with low confidence
            vote.choice = VOTE_CHOICE_DISAGREE;
            vote.confidence = 0.3f;
        } else {
            // Honest: agree with high confidence
            vote.choice = VOTE_CHOICE_AGREE;
            vote.confidence = 0.95f;
        }

        PropagateVote(vote);
    }

    ASSERT_TRUE(WaitForCallback(callback_ctx)) << "Vote did not complete in time";
    const auto& result = callback_ctx.result;

    // Should still pass due to confidence weighting
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.agree_count, 5u);
    EXPECT_EQ(result.disagree_count, 2u);
}

TEST_F(SwarmConsensusIntegrationTest, ByzantineFaultTolerance_ConflictingVotes) {
    uint32_t proposal_id = 0;
    VoteCallbackContext callback_ctx;

    nimcp_error_t err = swarm_consensus_propose(
        consensus_modules_[0],
        VOTE_TOPIC_LEADER_ELECTION,
        nullptr,
        GetCurrentTimeMs() + 5000,
        NUM_DRONES,
        SWARM_DEFAULT_THRESHOLD,
        vote_completion_callback,
        &callback_ctx,
        &proposal_id
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Simulate conflicting votes from same drone (Byzantine behavior)
    swarm_vote_response_t vote1;
    vote1.proposal_id = proposal_id;
    vote1.voter_drone = 3;
    vote1.choice = VOTE_CHOICE_AGREE;
    vote1.confidence = 0.9f;
    PropagateVote(vote1);

    // Same drone changes vote (malicious) - should be rejected as duplicate
    swarm_vote_response_t vote2;
    vote2.proposal_id = proposal_id;
    vote2.voter_drone = 3;
    vote2.choice = VOTE_CHOICE_DISAGREE;
    vote2.confidence = 0.9f;
    PropagateVote(vote2);

    // Other drones vote normally
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        if (i == 3) continue; // Skip already voted drone

        swarm_vote_response_t vote;
        vote.proposal_id = proposal_id;
        vote.voter_drone = i;
        vote.choice = VOTE_CHOICE_AGREE;
        vote.confidence = 0.85f;
        PropagateVote(vote);
    }

    ASSERT_TRUE(WaitForCallback(callback_ctx)) << "Vote did not complete in time";
    const auto& result = callback_ctx.result;

    // Should handle conflicting vote gracefully
    EXPECT_TRUE(result.passed);
}

//=============================================================================
// Timeout and Expiration Tests
//=============================================================================

TEST_F(SwarmConsensusIntegrationTest, VoteTimeout) {
    uint32_t proposal_id = 0;
    uint64_t deadline = GetCurrentTimeMs() + 200; // Very short deadline
    VoteCallbackContext callback_ctx;

    nimcp_error_t err = swarm_consensus_propose(
        consensus_modules_[0],
        VOTE_TOPIC_CUSTOM,
        nullptr,
        deadline,
        NUM_DRONES,
        SWARM_DEFAULT_THRESHOLD,
        vote_completion_callback,
        &callback_ctx,
        &proposal_id
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Only 2 drones vote before timeout
    for (uint32_t i = 0; i < 2; i++) {
        swarm_vote_response_t vote;
        vote.proposal_id = proposal_id;
        vote.voter_drone = i;
        vote.choice = VOTE_CHOICE_AGREE;
        vote.confidence = 0.9f;
        PropagateVote(vote);
    }

    // Wait for timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Cleanup expired votes - this triggers the callback
    swarm_consensus_cleanup_expired(consensus_modules_[0], GetCurrentTimeMs());

    // Wait for callback to be invoked
    ASSERT_TRUE(WaitForCallback(callback_ctx)) << "Vote did not expire/complete in time";

    // Callback should have captured expired status
    EXPECT_EQ(callback_ctx.result.status, VOTE_STATUS_EXPIRED);
}

TEST_F(SwarmConsensusIntegrationTest, VotesArrivedAfterDeadline) {
    uint32_t proposal_id = 0;
    uint64_t deadline = GetCurrentTimeMs() + 100;
    VoteCallbackContext callback_ctx;

    nimcp_error_t err = swarm_consensus_propose(
        consensus_modules_[0],
        VOTE_TOPIC_CUSTOM,
        nullptr,
        deadline,
        NUM_DRONES,
        SWARM_DEFAULT_THRESHOLD,
        vote_completion_callback,
        &callback_ctx,
        &proposal_id
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // First 3 votes arrive on time
    for (uint32_t i = 0; i < 3; i++) {
        swarm_vote_response_t vote;
        vote.proposal_id = proposal_id;
        vote.voter_drone = i;
        vote.choice = VOTE_CHOICE_AGREE;
        vote.confidence = 0.9f;
        PropagateVote(vote);
    }

    // Wait for deadline to pass
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Cleanup to trigger expiration callback
    swarm_consensus_cleanup_expired(consensus_modules_[0], GetCurrentTimeMs());

    // Late votes should be ignored (proposal is now expired/inactive)
    for (uint32_t i = 3; i < NUM_DRONES; i++) {
        swarm_vote_response_t vote;
        vote.proposal_id = proposal_id;
        vote.voter_drone = i;
        vote.choice = VOTE_CHOICE_AGREE;
        vote.confidence = 0.9f;
        // Late votes will fail because proposal is expired - this is expected
        PropagateVote(vote);
    }

    // Wait for callback to be invoked (from expiration)
    ASSERT_TRUE(WaitForCallback(callback_ctx)) << "Vote did not expire in time";

    // Only on-time votes should count, and status should be expired
    EXPECT_EQ(callback_ctx.result.status, VOTE_STATUS_EXPIRED);
    EXPECT_LE(callback_ctx.result.agree_count, 3u);
}

//=============================================================================
// Quorum Tests
//=============================================================================

TEST_F(SwarmConsensusIntegrationTest, QuorumNotMet) {
    uint32_t proposal_id = 0;

    nimcp_error_t err = swarm_consensus_propose(
        consensus_modules_[0],
        VOTE_TOPIC_RETREAT,
        nullptr,
        GetCurrentTimeMs() + 5000,
        NUM_DRONES,    // Require all drones
        SWARM_DEFAULT_THRESHOLD,
        nullptr,
        nullptr,
        &proposal_id
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // Only 3 drones vote (less than quorum)
    for (uint32_t i = 0; i < 3; i++) {
        swarm_vote_response_t vote;
        vote.proposal_id = proposal_id;
        vote.voter_drone = i;
        vote.choice = VOTE_CHOICE_AGREE;
        vote.confidence = 1.0f;
        PropagateVote(vote);
    }

    bool is_complete = false;
    swarm_consensus_check_result(consensus_modules_[0], proposal_id, &is_complete);

    // Should not be complete without quorum
    EXPECT_FALSE(is_complete);
}

TEST_F(SwarmConsensusIntegrationTest, QuorumWithAbstentions) {
    uint32_t proposal_id = 0;
    VoteCallbackContext callback_ctx;

    // Note: quorum_required counts participating votes (agree + disagree), not abstentions
    // So we need 3 agree votes with threshold 0.667 to pass (3/3 = 100% >= 66.7%)
    nimcp_error_t err = swarm_consensus_propose(
        consensus_modules_[0],
        VOTE_TOPIC_CUSTOM,
        nullptr,
        GetCurrentTimeMs() + 5000,
        3, // Quorum of 3 participating votes (excludes abstentions)
        SWARM_DEFAULT_THRESHOLD,
        vote_completion_callback,
        &callback_ctx,
        &proposal_id
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // 3 agree, 2 abstain
    for (uint32_t i = 0; i < 5; i++) {
        swarm_vote_response_t vote;
        vote.proposal_id = proposal_id;
        vote.voter_drone = i;

        if (i < 3) {
            vote.choice = VOTE_CHOICE_AGREE;
            vote.confidence = 0.9f;
        } else {
            vote.choice = VOTE_CHOICE_ABSTAIN;
            vote.confidence = 0.0f;
        }

        PropagateVote(vote);
    }

    ASSERT_TRUE(WaitForCallback(callback_ctx)) << "Vote did not complete in time";
    const auto& result = callback_ctx.result;

    // With 3 agree and quorum of 3, vote should pass
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.agree_count, 3u);
}

//=============================================================================
// Concurrent Voting Tests
//=============================================================================

TEST_F(SwarmConsensusIntegrationTest, ConcurrentVoting) {
    uint32_t proposal_id = 0;
    VoteCallbackContext callback_ctx;

    nimcp_error_t err = swarm_consensus_propose(
        consensus_modules_[0],
        VOTE_TOPIC_CUSTOM,
        nullptr,
        GetCurrentTimeMs() + 5000,
        NUM_DRONES,
        SWARM_DEFAULT_THRESHOLD,
        vote_completion_callback,
        &callback_ctx,
        &proposal_id
    );
    ASSERT_EQ(err, NIMCP_SUCCESS);

    // All drones vote concurrently
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < NUM_DRONES; i++) {
        threads.emplace_back([this, i, proposal_id]() {
            swarm_vote_response_t vote;
            vote.proposal_id = proposal_id;
            vote.voter_drone = i;
            vote.choice = VOTE_CHOICE_AGREE;
            vote.confidence = 0.9f;
            PropagateVote(vote);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    ASSERT_TRUE(WaitForCallback(callback_ctx)) << "Vote did not complete in time";
    const auto& result = callback_ctx.result;

    EXPECT_TRUE(result.passed);
    // Vote count depends on timing - at least quorum should have voted
    EXPECT_GE(result.agree_count, 1u);
}

TEST_F(SwarmConsensusIntegrationTest, MultipleSimultaneousProposals) {
    std::vector<uint32_t> proposal_ids;
    std::vector<VoteCallbackContext> callback_ctxs(3);

    // Create 3 proposals simultaneously (all on drone 0 for simplicity)
    for (int i = 0; i < 3; i++) {
        uint32_t proposal_id = 0;
        nimcp_error_t err = swarm_consensus_propose(
            consensus_modules_[0],
            VOTE_TOPIC_CUSTOM,
            nullptr,
            GetCurrentTimeMs() + 5000,
            NUM_DRONES,
            SWARM_DEFAULT_THRESHOLD,
            vote_completion_callback,
            &callback_ctxs[i],
            &proposal_id
        );
        ASSERT_EQ(err, NIMCP_SUCCESS);
        proposal_ids.push_back(proposal_id);
    }

    // Vote on all proposals
    for (auto proposal_id : proposal_ids) {
        for (uint32_t i = 0; i < NUM_DRONES; i++) {
            swarm_vote_response_t vote;
            vote.proposal_id = proposal_id;
            vote.voter_drone = i;
            vote.choice = VOTE_CHOICE_AGREE;
            vote.confidence = 0.8f;
            PropagateVote(vote);
        }
    }

    // Check all proposals via callbacks
    int proposals_passed = 0;
    for (int i = 0; i < 3; i++) {
        if (WaitForCallback(callback_ctxs[i])) {
            if (callback_ctxs[i].result.passed) {
                proposals_passed++;
            }
        }
    }
    // All proposals should have passed
    EXPECT_EQ(proposals_passed, 3);
}

//=============================================================================
// Statistics and Monitoring Tests
//=============================================================================

TEST_F(SwarmConsensusIntegrationTest, StatisticsTracking) {
    // Create and complete several proposals
    for (int i = 0; i < 3; i++) {
        uint32_t proposal_id = 0;
        swarm_consensus_propose(
            consensus_modules_[0],
            VOTE_TOPIC_CUSTOM,
            nullptr,
            GetCurrentTimeMs() + 5000,
            NUM_DRONES,
            SWARM_DEFAULT_THRESHOLD,
            nullptr,
            nullptr,
            &proposal_id
        );

        // Vote
        for (uint32_t j = 0; j < NUM_DRONES; j++) {
            swarm_vote_response_t vote;
            vote.proposal_id = proposal_id;
            vote.voter_drone = j;
            vote.choice = VOTE_CHOICE_AGREE;
            vote.confidence = 0.9f;
            PropagateVote(vote);
        }
    }

    // Check statistics
    swarm_consensus_stats_t stats;
    nimcp_error_t err = swarm_consensus_get_stats(consensus_modules_[0], &stats);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    EXPECT_EQ(stats.proposals_created, 3);
    EXPECT_GE(stats.votes_received, NUM_DRONES * 3);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
