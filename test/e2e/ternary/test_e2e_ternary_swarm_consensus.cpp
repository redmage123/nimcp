/**
 * @file test_e2e_ternary_swarm_consensus.cpp
 * @brief End-to-end tests for swarm consensus with ternary voting and memory
 *
 * WHAT: Full swarm simulation with ternary voting and memory
 * WHY:  Verify ternary Byzantine consensus in swarm coordination
 * HOW:  Test voting, memory sharing, Byzantine tolerance
 *
 * TEST COVERAGE:
 * - Full swarm simulation with ternary voting
 * - Byzantine consensus with ternary states
 * - Swarm convergence verification
 * - Distributed memory with ternary encoding
 * - Fault tolerance under Byzantine failures
 *
 * BIOLOGICAL/SWARM BASIS:
 * - Votes: agree (+1), abstain (0), disagree (-1)
 * - Memory states: remember (+1), uncertain (0), forget (-1)
 * - Byzantine tolerance: up to 1/3 faulty nodes
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>
#include <set>

// Headers have their own extern "C" guards
#include "utils/ternary/nimcp_ternary.h"
#include "utils/ternary/nimcp_ternary_vector.h"
#include "utils/ternary/nimcp_ternary_matrix.h"
#include "utils/ternary/nimcp_ternary_logic.h"
#include "swarm/nimcp_swarm_ternary.h"
#include "swarm/nimcp_swarm_consensus.h"

//=============================================================================
// Test Fixture
//=============================================================================

class TernarySwarmConsensusE2ETest : public ::testing::Test {
protected:
    static constexpr size_t SWARM_SIZE = 32;
    static constexpr size_t MEMORY_SIZE = 64;
    static constexpr size_t NUM_PROPOSALS = 10;
    static constexpr float BYZANTINE_RATIO = 0.25f;  // 25% faulty
    static constexpr float CONSENSUS_THRESHOLD = 0.667f;  // 2/3 majority

    struct SwarmNode {
        uint16_t id;
        bool byzantine;  // Faulty node
        trit_vector_t* memory;
        float confidence;
        std::vector<trit_t> pending_votes;
    };

    struct Proposal {
        uint32_t id;
        trit_t value;  // Proposed value
        std::vector<std::pair<uint16_t, trit_t>> votes;  // (node_id, vote)
        bool resolved;
        trit_t outcome;
    };

    std::vector<SwarmNode> nodes;
    std::vector<Proposal> proposals;
    trit_matrix_t* shared_memory = nullptr;  // Distributed shared memory

    std::mt19937 rng;

    void SetUp() override {
        rng.seed(42);

        // Create swarm nodes
        std::uniform_real_distribution<float> conf_dist(0.5f, 1.0f);
        size_t num_byzantine = (size_t)(SWARM_SIZE * BYZANTINE_RATIO);

        for (size_t i = 0; i < SWARM_SIZE; i++) {
            SwarmNode node;
            node.id = (uint16_t)i;
            node.byzantine = (i < num_byzantine);  // First N nodes are Byzantine
            node.memory = trit_vector_create(MEMORY_SIZE, TERNARY_PACK_2BIT);
            ASSERT_NE(node.memory, nullptr);
            node.confidence = conf_dist(rng);

            // Initialize memory with uncertain state
            for (size_t m = 0; m < MEMORY_SIZE; m++) {
                trit_vector_set(node.memory, m, TRIT_UNKNOWN);
            }

            nodes.push_back(node);
        }

        // Create shared memory matrix (nodes x memory_slots)
        shared_memory = trit_matrix_create(SWARM_SIZE, MEMORY_SIZE, TERNARY_PACK_BASE243);
        ASSERT_NE(shared_memory, nullptr);

        // Initialize with uncertain
        for (size_t n = 0; n < SWARM_SIZE; n++) {
            for (size_t m = 0; m < MEMORY_SIZE; m++) {
                trit_matrix_set(shared_memory, n, m, TRIT_UNKNOWN);
            }
        }

        // Create proposals
        GenerateProposals();
    }

    void TearDown() override {
        for (auto& node : nodes) {
            if (node.memory) trit_vector_destroy(node.memory);
        }
        if (shared_memory) trit_matrix_destroy(shared_memory);
    }

    void GenerateProposals() {
        proposals.clear();
        std::uniform_int_distribution<int> val_dist(-1, 1);

        for (size_t i = 0; i < NUM_PROPOSALS; i++) {
            Proposal prop;
            prop.id = (uint32_t)i;
            prop.value = (trit_t)val_dist(rng);
            prop.resolved = false;
            prop.outcome = TRIT_UNKNOWN;
            proposals.push_back(prop);
        }
    }

    // Honest node voting behavior
    trit_t HonestVote(const SwarmNode& node, const Proposal& proposal) {
        // Honest nodes vote based on proposal value and their confidence
        std::uniform_real_distribution<float> vote_dist(0.0f, 1.0f);

        if (vote_dist(rng) < node.confidence) {
            // Vote same as proposal
            return proposal.value;
        } else if (vote_dist(rng) < 0.5f) {
            // Abstain
            return TRIT_UNKNOWN;
        } else {
            // Vote opposite (minority dissent)
            return (trit_t)(-proposal.value);
        }
    }

    // Byzantine node voting behavior (malicious/faulty)
    trit_t ByzantineVote(const SwarmNode& node, const Proposal& proposal) {
        std::uniform_int_distribution<int> random_vote(-1, 1);
        std::uniform_real_distribution<float> behavior(0.0f, 1.0f);

        float b = behavior(rng);
        if (b < 0.3f) {
            // Vote opposite to confuse
            return (trit_t)(-proposal.value);
        } else if (b < 0.6f) {
            // Random vote
            return (trit_t)random_vote(rng);
        } else {
            // Appear honest to avoid detection
            return HonestVote(node, proposal);
        }
    }

    // Collect votes for a proposal
    void CollectVotes(Proposal& proposal) {
        proposal.votes.clear();
        for (const auto& node : nodes) {
            trit_t vote;
            if (node.byzantine) {
                vote = ByzantineVote(node, proposal);
            } else {
                vote = HonestVote(node, proposal);
            }
            proposal.votes.push_back({node.id, vote});
        }
    }

    // Resolve proposal using weighted voting
    void ResolveProposal(Proposal& proposal) {
        float agree_weight = 0.0f;
        float disagree_weight = 0.0f;
        float total_weight = 0.0f;

        for (const auto& vote : proposal.votes) {
            float weight = nodes[vote.first].confidence;
            total_weight += weight;

            if (vote.second == TRIT_POSITIVE) {
                agree_weight += weight;
            } else if (vote.second == TRIT_NEGATIVE) {
                disagree_weight += weight;
            }
            // Abstain votes don't contribute
        }

        float agree_ratio = agree_weight / total_weight;
        float disagree_ratio = disagree_weight / total_weight;

        if (agree_ratio >= CONSENSUS_THRESHOLD) {
            proposal.outcome = TRIT_POSITIVE;
            proposal.resolved = true;
        } else if (disagree_ratio >= CONSENSUS_THRESHOLD) {
            proposal.outcome = TRIT_NEGATIVE;
            proposal.resolved = true;
        } else {
            proposal.outcome = TRIT_UNKNOWN;
            proposal.resolved = false;  // No consensus
        }
    }

    // Count votes by type
    struct VoteCounts {
        size_t agree;
        size_t abstain;
        size_t disagree;
    };

    VoteCounts CountVotes(const Proposal& proposal) {
        VoteCounts counts = {0, 0, 0};
        for (const auto& vote : proposal.votes) {
            if (vote.second == TRIT_POSITIVE) counts.agree++;
            else if (vote.second == TRIT_NEGATIVE) counts.disagree++;
            else counts.abstain++;
        }
        return counts;
    }

    // Update shared memory based on consensus
    void UpdateSharedMemory(size_t slot, trit_t value) {
        for (size_t n = 0; n < SWARM_SIZE; n++) {
            trit_matrix_set(shared_memory, n, slot, value);
            trit_vector_set(nodes[n].memory, slot, value);
        }
    }

    // Check memory consistency across swarm
    bool CheckMemoryConsistency(size_t slot) {
        trit_t first = trit_vector_get(nodes[0].memory, slot);
        for (size_t n = 1; n < SWARM_SIZE; n++) {
            if (!nodes[n].byzantine) {  // Only check honest nodes
                if (trit_vector_get(nodes[n].memory, slot) != first) {
                    return false;
                }
            }
        }
        return true;
    }
};

//=============================================================================
// E2E Test: Basic Consensus Voting
//=============================================================================

TEST_F(TernarySwarmConsensusE2ETest, BasicConsensusVoting) {
    // Simple proposal with expected consensus
    Proposal& prop = proposals[0];
    prop.value = TRIT_POSITIVE;  // Propose agreement

    CollectVotes(prop);

    VoteCounts counts = CountVotes(prop);
    EXPECT_EQ(counts.agree + counts.abstain + counts.disagree, SWARM_SIZE);

    ResolveProposal(prop);

    // Log results (may or may not reach consensus depending on Byzantine nodes)
    SUCCEED() << "Votes: " << counts.agree << " agree, "
              << counts.abstain << " abstain, "
              << counts.disagree << " disagree. "
              << "Outcome: " << (int)prop.outcome;
}

//=============================================================================
// E2E Test: Byzantine Fault Tolerance
//=============================================================================

TEST_F(TernarySwarmConsensusE2ETest, ByzantineFaultTolerance) {
    // With 25% Byzantine nodes, consensus should still be achievable
    // if honest nodes are in agreement

    // Force all honest nodes to agree
    for (auto& node : nodes) {
        if (!node.byzantine) {
            node.confidence = 0.99f;  // High confidence means consistent voting
        }
    }

    // Run multiple proposals
    size_t consensus_reached = 0;
    for (auto& prop : proposals) {
        prop.value = TRIT_POSITIVE;
        CollectVotes(prop);
        ResolveProposal(prop);

        if (prop.resolved) {
            consensus_reached++;
        }
    }

    // With 25% Byzantine (< 1/3), most proposals should reach consensus
    EXPECT_GE(consensus_reached, NUM_PROPOSALS / 2)
        << "Too few proposals reached consensus with BFT";
}

//=============================================================================
// E2E Test: Swarm Convergence
//=============================================================================

TEST_F(TernarySwarmConsensusE2ETest, SwarmConvergence) {
    constexpr size_t NUM_ROUNDS = 20;

    // Track convergence over multiple rounds
    std::vector<float> convergence_scores;

    for (size_t round = 0; round < NUM_ROUNDS; round++) {
        // Each round: propose, vote, resolve
        Proposal prop;
        prop.id = (uint32_t)round;
        prop.value = (round % 2 == 0) ? TRIT_POSITIVE : TRIT_NEGATIVE;

        // Collect votes
        prop.votes.clear();
        for (auto& node : nodes) {
            trit_t vote;
            if (node.byzantine) {
                vote = ByzantineVote(node, prop);
            } else {
                vote = HonestVote(node, prop);
            }
            prop.votes.push_back({node.id, vote});
        }

        // Resolve
        ResolveProposal(prop);

        // Update memories if resolved
        if (prop.resolved && round < MEMORY_SIZE) {
            UpdateSharedMemory(round, prop.outcome);
        }

        // Compute convergence score (agreement among honest nodes)
        size_t honest_count = 0;
        size_t agree_with_outcome = 0;
        for (const auto& vote : prop.votes) {
            if (!nodes[vote.first].byzantine) {
                honest_count++;
                if (vote.second == prop.outcome) {
                    agree_with_outcome++;
                }
            }
        }

        float convergence = (honest_count > 0) ?
            (float)agree_with_outcome / honest_count : 0.0f;
        convergence_scores.push_back(convergence);
    }

    // Average convergence should be reasonable
    float avg_convergence = 0.0f;
    for (float c : convergence_scores) avg_convergence += c;
    avg_convergence /= convergence_scores.size();

    EXPECT_GT(avg_convergence, 0.3f) << "Swarm failed to converge";
}

//=============================================================================
// E2E Test: Ternary Memory Distribution
//=============================================================================

TEST_F(TernarySwarmConsensusE2ETest, TernaryMemoryDistribution) {
    // Store values in distributed memory through consensus
    constexpr size_t NUM_MEMORY_OPS = 10;

    for (size_t op = 0; op < NUM_MEMORY_OPS; op++) {
        size_t slot = op;
        trit_t value = (op % 3 == 0) ? TRIT_POSITIVE :
                       (op % 3 == 1) ? TRIT_NEGATIVE : TRIT_UNKNOWN;

        // Create proposal for memory update
        Proposal prop;
        prop.id = (uint32_t)(1000 + op);
        prop.value = value;

        CollectVotes(prop);
        ResolveProposal(prop);

        if (prop.resolved) {
            UpdateSharedMemory(slot, prop.outcome);
        }
    }

    // Verify memory consistency among honest nodes
    size_t consistent_slots = 0;
    for (size_t slot = 0; slot < NUM_MEMORY_OPS; slot++) {
        if (CheckMemoryConsistency(slot)) {
            consistent_slots++;
        }
    }

    // Most slots should be consistent
    EXPECT_GE(consistent_slots, NUM_MEMORY_OPS * 0.8)
        << "Memory inconsistent across swarm";
}

//=============================================================================
// E2E Test: Majority Voting with Ternary Logic
//=============================================================================

TEST_F(TernarySwarmConsensusE2ETest, MajorityVotingWithTernaryLogic) {
    // Use ternary majority function for voting
    std::vector<trit_t> votes(SWARM_SIZE);

    for (size_t trial = 0; trial < 10; trial++) {
        // Generate votes
        std::uniform_int_distribution<int> vote_dist(-1, 1);
        for (size_t i = 0; i < SWARM_SIZE; i++) {
            if (nodes[i].byzantine) {
                // Byzantine: random vote
                votes[i] = (trit_t)vote_dist(rng);
            } else {
                // Honest: vote based on trial parity
                votes[i] = (trial % 2 == 0) ? TRIT_POSITIVE : TRIT_NEGATIVE;
            }
        }

        // Use ternary majority
        trit_t majority_result = trit_majority(votes.data(), votes.size());

        // Expected: honest majority should win
        trit_t expected = (trial % 2 == 0) ? TRIT_POSITIVE : TRIT_NEGATIVE;

        // Count actual votes
        size_t pos = 0, neg = 0;
        for (trit_t v : votes) {
            if (v == TRIT_POSITIVE) pos++;
            else if (v == TRIT_NEGATIVE) neg++;
        }

        // Verify majority function works correctly
        if (pos > SWARM_SIZE / 2) {
            EXPECT_EQ(majority_result, TRIT_POSITIVE);
        } else if (neg > SWARM_SIZE / 2) {
            EXPECT_EQ(majority_result, TRIT_NEGATIVE);
        }
        // Otherwise UNKNOWN is valid
    }
}

//=============================================================================
// E2E Test: Unanimous Agreement Detection
//=============================================================================

TEST_F(TernarySwarmConsensusE2ETest, UnanimousAgreementDetection) {
    std::vector<trit_t> votes(SWARM_SIZE);

    // Test unanimous positive
    std::fill(votes.begin(), votes.end(), TRIT_POSITIVE);
    trit_t result = trit_unanimous(votes.data(), votes.size());
    EXPECT_EQ(result, TRIT_POSITIVE);

    // Test unanimous negative
    std::fill(votes.begin(), votes.end(), TRIT_NEGATIVE);
    result = trit_unanimous(votes.data(), votes.size());
    EXPECT_EQ(result, TRIT_NEGATIVE);

    // Test non-unanimous (one dissent)
    votes[0] = TRIT_UNKNOWN;
    result = trit_unanimous(votes.data(), votes.size());
    EXPECT_EQ(result, TRIT_UNKNOWN) << "Non-unanimous should return UNKNOWN";
}

//=============================================================================
// E2E Test: Weighted Confidence Voting
//=============================================================================

TEST_F(TernarySwarmConsensusE2ETest, WeightedConfidenceVoting) {
    // High-confidence nodes should have more influence
    Proposal prop;
    prop.id = 100;
    prop.value = TRIT_POSITIVE;

    // Set up: first half low confidence, second half high confidence
    for (size_t i = 0; i < SWARM_SIZE / 2; i++) {
        nodes[i].confidence = 0.3f;
    }
    for (size_t i = SWARM_SIZE / 2; i < SWARM_SIZE; i++) {
        nodes[i].confidence = 0.95f;
    }

    // Disable Byzantine behavior for this test
    for (auto& node : nodes) {
        node.byzantine = false;
    }

    // Force low-confidence to vote negative, high-confidence to vote positive
    prop.votes.clear();
    for (size_t i = 0; i < SWARM_SIZE; i++) {
        trit_t vote = (i < SWARM_SIZE / 2) ? TRIT_NEGATIVE : TRIT_POSITIVE;
        prop.votes.push_back({(uint16_t)i, vote});
    }

    ResolveProposal(prop);

    // High-confidence votes (POSITIVE) should win despite equal count
    EXPECT_EQ(prop.outcome, TRIT_POSITIVE)
        << "Weighted voting should favor high-confidence nodes";
}

//=============================================================================
// E2E Test: Multi-Round Consensus Protocol
//=============================================================================

TEST_F(TernarySwarmConsensusE2ETest, MultiRoundConsensusProtocol) {
    // Simulate multi-round consensus (like PBFT)
    constexpr size_t MAX_ROUNDS = 5;

    Proposal prop;
    prop.id = 200;
    prop.value = TRIT_POSITIVE;

    for (size_t round = 0; round < MAX_ROUNDS; round++) {
        // Pre-prepare: leader proposes
        // Prepare: nodes vote
        CollectVotes(prop);

        // Check if prepare threshold reached
        VoteCounts counts = CountVotes(prop);
        float agreement = (float)(counts.agree) / SWARM_SIZE;

        if (agreement >= 0.667f) {
            // Commit: consensus reached
            prop.resolved = true;
            prop.outcome = TRIT_POSITIVE;
            break;
        }

        // Increase confidence for next round (nodes that agreed before
        // are more likely to agree again)
        for (auto& vote : prop.votes) {
            if (vote.second == TRIT_POSITIVE) {
                nodes[vote.first].confidence =
                    std::min(1.0f, nodes[vote.first].confidence + 0.1f);
            }
        }
    }

    SUCCEED() << "Multi-round consensus completed. Resolved: " << prop.resolved;
}

//=============================================================================
// E2E Test: Fault Detection
//=============================================================================

TEST_F(TernarySwarmConsensusE2ETest, FaultDetection) {
    // Track voting history to detect Byzantine behavior
    std::vector<std::vector<trit_t>> voting_history(SWARM_SIZE);

    // Run multiple proposals
    for (size_t p = 0; p < 20; p++) {
        Proposal prop;
        prop.id = (uint32_t)(300 + p);
        prop.value = TRIT_POSITIVE;

        CollectVotes(prop);

        for (const auto& vote : prop.votes) {
            voting_history[vote.first].push_back(vote.second);
        }
    }

    // Analyze voting patterns to detect Byzantine nodes
    std::set<uint16_t> suspected_byzantine;

    for (size_t n = 0; n < SWARM_SIZE; n++) {
        const auto& history = voting_history[n];

        // Byzantine indicators:
        // 1. High inconsistency (many state changes)
        size_t changes = 0;
        for (size_t i = 1; i < history.size(); i++) {
            if (history[i] != history[i-1]) changes++;
        }
        float inconsistency = (float)changes / (history.size() - 1);

        // 2. Voting against majority often
        size_t against_majority = 0;
        for (size_t i = 0; i < history.size(); i++) {
            // Simplified: if voted negative when proposal was positive
            if (history[i] == TRIT_NEGATIVE) {
                against_majority++;
            }
        }
        float contrarian_ratio = (float)against_majority / history.size();

        // Flag as suspicious
        if (inconsistency > 0.6f || contrarian_ratio > 0.6f) {
            suspected_byzantine.insert((uint16_t)n);
        }
    }

    // Check detection accuracy
    size_t true_positives = 0;
    size_t false_positives = 0;

    for (uint16_t id : suspected_byzantine) {
        if (nodes[id].byzantine) {
            true_positives++;
        } else {
            false_positives++;
        }
    }

    // Log detection results
    size_t actual_byzantine = (size_t)(SWARM_SIZE * BYZANTINE_RATIO);
    float detection_rate = (actual_byzantine > 0) ?
        (float)true_positives / actual_byzantine : 0.0f;

    SUCCEED() << "Byzantine detection: " << true_positives << "/" << actual_byzantine
              << " detected, " << false_positives << " false positives";
}

//=============================================================================
// E2E Test: Long-Running Stability
//=============================================================================

TEST_F(TernarySwarmConsensusE2ETest, LongRunningStability) {
    constexpr size_t NUM_ITERATIONS = 500;

    size_t proposals_resolved = 0;
    size_t memory_updates = 0;
    size_t errors = 0;

    for (size_t iter = 0; iter < NUM_ITERATIONS; iter++) {
        Proposal prop;
        prop.id = (uint32_t)(iter);
        prop.value = (iter % 3 == 0) ? TRIT_POSITIVE :
                     (iter % 3 == 1) ? TRIT_NEGATIVE : TRIT_UNKNOWN;

        CollectVotes(prop);
        ResolveProposal(prop);

        if (prop.resolved) {
            proposals_resolved++;

            // Update memory
            size_t slot = iter % MEMORY_SIZE;
            UpdateSharedMemory(slot, prop.outcome);
            memory_updates++;

            // Verify memory
            if (!CheckMemoryConsistency(slot)) {
                errors++;
            }
        }
    }

    EXPECT_EQ(errors, 0u) << "Memory consistency errors detected";
    EXPECT_GT(proposals_resolved, NUM_ITERATIONS / 4)
        << "Too few proposals resolved";

    SUCCEED() << "Completed " << NUM_ITERATIONS << " iterations, "
              << proposals_resolved << " resolved, "
              << memory_updates << " memory updates";
}

//=============================================================================
// E2E Test: Memory Efficiency
//=============================================================================

TEST_F(TernarySwarmConsensusE2ETest, MemoryEfficiency) {
    size_t ternary_bytes = 0;

    for (const auto& node : nodes) {
        ternary_bytes += trit_vector_memory_size(node.memory);
    }
    ternary_bytes += trit_matrix_memory_size(shared_memory);

    // Float equivalent
    size_t float_bytes = SWARM_SIZE * MEMORY_SIZE * sizeof(float) +  // Node memories
                         SWARM_SIZE * MEMORY_SIZE * sizeof(float);   // Shared memory

    float compression = (float)float_bytes / (float)ternary_bytes;

    EXPECT_GT(compression, 3.0f) << "Expected at least 3x compression";

    SUCCEED() << "Memory compression: " << compression << "x ("
              << ternary_bytes << " ternary bytes vs "
              << float_bytes << " float bytes)";
}

//=============================================================================
// E2E Test: Performance Benchmark
//=============================================================================

TEST_F(TernarySwarmConsensusE2ETest, PerformanceBenchmark) {
    constexpr size_t BENCHMARK_PROPOSALS = 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < BENCHMARK_PROPOSALS; i++) {
        Proposal prop;
        prop.id = (uint32_t)i;
        prop.value = TRIT_POSITIVE;

        CollectVotes(prop);
        ResolveProposal(prop);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    float proposals_per_second = (float)BENCHMARK_PROPOSALS * 1e6f / duration.count();

    EXPECT_GT(proposals_per_second, 1000.0f)
        << "Performance: " << proposals_per_second << " proposals/second";

    SUCCEED() << "Swarm consensus: " << proposals_per_second << " proposals/second";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
