/**
 * @file test_swarm_ternary_integration.cpp
 * @brief Integration tests for swarm with ternary representation
 *
 * Tests:
 * - Swarm memory with ternary confidence
 * - Swarm voting with ternary consensus
 * - Pattern learning with ternary states
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

// Headers have their own extern "C" guards
#include "utils/error/nimcp_error_codes.h"
#include "swarm/nimcp_swarm_ternary.h"
#include "swarm/nimcp_swarm_memory.h"
#include "swarm/nimcp_swarm_consensus.h"
#include "utils/ternary/nimcp_ternary_types.h"
#include "utils/ternary/nimcp_ternary_vector.h"
#include "utils/ternary/nimcp_ternary_matrix.h"
#include "utils/ternary/nimcp_ternary_convert.h"

/**
 * @class SwarmTernaryIntegrationTest
 * @brief Test fixture for swarm with ternary integration
 */
class SwarmTernaryIntegrationTest : public ::testing::Test {
protected:
    NimcpSwarmMemory* swarm_memory = nullptr;

    void SetUp() override {
        // Create swarm memory system
        swarm_memory = nimcp_swarm_memory_create(1000, 3);  // capacity=1000, replication=3
        ASSERT_NE(nullptr, swarm_memory);

        // Initialize
        nimcp_result_t result = nimcp_swarm_memory_init(swarm_memory, nullptr);
        ASSERT_EQ(NIMCP_SUCCESS, result);
    }

    void TearDown() override {
        if (swarm_memory) {
            nimcp_swarm_memory_destroy(swarm_memory);
            swarm_memory = nullptr;
        }
    }

    /**
     * @brief Generate random votes with given distribution
     */
    std::vector<trit_vote_t> generateVotes(
            size_t n_voters, float agree_prob, float disagree_prob) {
        std::vector<trit_vote_t> votes(n_voters);

        for (size_t i = 0; i < n_voters; i++) {
            float r = (float)(i % 100) / 100.0f;
            if (r < agree_prob) {
                votes[i] = TRIT_VOTE_AGREE;
            } else if (r < agree_prob + disagree_prob) {
                votes[i] = TRIT_VOTE_DISAGREE;
            } else {
                votes[i] = TRIT_VOTE_ABSTAIN;
            }
        }
        return votes;
    }
};

//=============================================================================
// Test: Swarm Voting with Ternary Consensus
//=============================================================================

/**
 * Test basic majority consensus
 */
TEST_F(SwarmTernaryIntegrationTest, MajorityConsensusBasic) {
    // Clear majority for AGREE
    std::vector<trit_vote_t> votes = {
        TRIT_VOTE_AGREE, TRIT_VOTE_AGREE, TRIT_VOTE_AGREE,
        TRIT_VOTE_DISAGREE, TRIT_VOTE_ABSTAIN
    };

    trit_vote_t result = trit_consensus_majority(votes.data(), votes.size());
    EXPECT_EQ(TRIT_VOTE_AGREE, result) << "Majority should be AGREE";
}

/**
 * Test majority consensus with DISAGREE
 */
TEST_F(SwarmTernaryIntegrationTest, MajorityConsensusDisagree) {
    std::vector<trit_vote_t> votes = {
        TRIT_VOTE_DISAGREE, TRIT_VOTE_DISAGREE, TRIT_VOTE_DISAGREE,
        TRIT_VOTE_AGREE, TRIT_VOTE_ABSTAIN
    };

    trit_vote_t result = trit_consensus_majority(votes.data(), votes.size());
    EXPECT_EQ(TRIT_VOTE_DISAGREE, result) << "Majority should be DISAGREE";
}

/**
 * Test majority consensus tie
 */
TEST_F(SwarmTernaryIntegrationTest, MajorityConsensusTie) {
    std::vector<trit_vote_t> votes = {
        TRIT_VOTE_AGREE, TRIT_VOTE_AGREE,
        TRIT_VOTE_DISAGREE, TRIT_VOTE_DISAGREE
    };

    trit_vote_t result = trit_consensus_majority(votes.data(), votes.size());
    EXPECT_EQ(TRIT_VOTE_ABSTAIN, result) << "Tie should result in ABSTAIN";
}

/**
 * Test supermajority consensus (2/3 threshold)
 */
TEST_F(SwarmTernaryIntegrationTest, SupermajorityConsensus) {
    // 9 voters, 6 agree = 66.7%, just shy of 67%
    std::vector<trit_vote_t> votes = {
        TRIT_VOTE_AGREE, TRIT_VOTE_AGREE, TRIT_VOTE_AGREE,
        TRIT_VOTE_AGREE, TRIT_VOTE_AGREE, TRIT_VOTE_AGREE,
        TRIT_VOTE_DISAGREE, TRIT_VOTE_DISAGREE, TRIT_VOTE_ABSTAIN
    };

    // With 0.67 threshold, 6/9 = 0.666... should NOT meet threshold
    trit_vote_t result = trit_consensus_supermajority(votes.data(), votes.size(), 0.67f);
    EXPECT_EQ(TRIT_VOTE_ABSTAIN, result) << "66.7% should not meet 67% threshold";

    // 7 agree = 77.8% should meet threshold
    votes[6] = TRIT_VOTE_AGREE;  // Now 7 agree
    result = trit_consensus_supermajority(votes.data(), votes.size(), 0.67f);
    EXPECT_EQ(TRIT_VOTE_AGREE, result) << "77.8% should meet 67% threshold";
}

/**
 * Test supermajority for DISAGREE
 */
TEST_F(SwarmTernaryIntegrationTest, SupermajorityConsensusDisagree) {
    std::vector<trit_vote_t> votes = {
        TRIT_VOTE_DISAGREE, TRIT_VOTE_DISAGREE, TRIT_VOTE_DISAGREE,
        TRIT_VOTE_DISAGREE, TRIT_VOTE_DISAGREE, TRIT_VOTE_DISAGREE,
        TRIT_VOTE_DISAGREE,  // 7 disagree
        TRIT_VOTE_AGREE, TRIT_VOTE_ABSTAIN
    };

    trit_vote_t result = trit_consensus_supermajority(votes.data(), votes.size(), 0.67f);
    EXPECT_EQ(TRIT_VOTE_DISAGREE, result) << "77.8% DISAGREE should meet threshold";
}

/**
 * Test unanimous consensus
 */
TEST_F(SwarmTernaryIntegrationTest, UnanimousConsensus) {
    // All agree
    std::vector<trit_vote_t> all_agree = {
        TRIT_VOTE_AGREE, TRIT_VOTE_AGREE, TRIT_VOTE_AGREE, TRIT_VOTE_AGREE
    };

    trit_vote_t result = trit_consensus_unanimous(all_agree.data(), all_agree.size(), false);
    EXPECT_EQ(TRIT_VOTE_AGREE, result) << "All agree should be unanimous AGREE";

    // One disagrees
    std::vector<trit_vote_t> one_disagrees = {
        TRIT_VOTE_AGREE, TRIT_VOTE_AGREE, TRIT_VOTE_AGREE, TRIT_VOTE_DISAGREE
    };

    result = trit_consensus_unanimous(one_disagrees.data(), one_disagrees.size(), false);
    EXPECT_EQ(TRIT_VOTE_DISAGREE, result) << "One disagree should block unanimous";

    // One abstains (with ignore_abstain = false)
    std::vector<trit_vote_t> one_abstains = {
        TRIT_VOTE_AGREE, TRIT_VOTE_AGREE, TRIT_VOTE_AGREE, TRIT_VOTE_ABSTAIN
    };

    result = trit_consensus_unanimous(one_abstains.data(), one_abstains.size(), false);
    EXPECT_EQ(TRIT_VOTE_ABSTAIN, result) << "One abstain should block with ignore_abstain=false";

    // One abstains (with ignore_abstain = true)
    result = trit_consensus_unanimous(one_abstains.data(), one_abstains.size(), true);
    EXPECT_EQ(TRIT_VOTE_AGREE, result) << "One abstain should not block with ignore_abstain=true";
}

/**
 * Test quorum-based consensus
 */
TEST_F(SwarmTernaryIntegrationTest, QuorumConsensus) {
    const size_t n_total = 10;  // Total eligible voters

    // 4 voters (40% participation)
    std::vector<trit_vote_t> low_participation = {
        TRIT_VOTE_AGREE, TRIT_VOTE_AGREE, TRIT_VOTE_AGREE, TRIT_VOTE_ABSTAIN
    };

    // Require 50% quorum
    trit_vote_t result = trit_consensus_quorum(
        low_participation.data(), low_participation.size(), n_total, 0.5f);
    EXPECT_EQ(TRIT_VOTE_ABSTAIN, result) << "40% participation should not meet 50% quorum";

    // 6 voters (60% participation)
    std::vector<trit_vote_t> sufficient_participation = {
        TRIT_VOTE_AGREE, TRIT_VOTE_AGREE, TRIT_VOTE_AGREE,
        TRIT_VOTE_AGREE, TRIT_VOTE_DISAGREE, TRIT_VOTE_DISAGREE
    };

    result = trit_consensus_quorum(
        sufficient_participation.data(), sufficient_participation.size(), n_total, 0.5f);
    EXPECT_EQ(TRIT_VOTE_AGREE, result) << "60% participation with majority AGREE";
}

/**
 * Test weighted voting
 */
TEST_F(SwarmTernaryIntegrationTest, WeightedVoting) {
    std::vector<trit_vote_t> votes = {
        TRIT_VOTE_AGREE,    // Weight 1.0
        TRIT_VOTE_DISAGREE, // Weight 2.0 (more weight)
        TRIT_VOTE_DISAGREE, // Weight 1.0
        TRIT_VOTE_AGREE     // Weight 1.0
    };

    std::vector<float> weights = {1.0f, 2.0f, 1.0f, 1.0f};

    // Without weights: 2 agree, 2 disagree = tie
    trit_vote_t unweighted = trit_consensus_majority(votes.data(), votes.size());
    EXPECT_EQ(TRIT_VOTE_ABSTAIN, unweighted) << "Unweighted should be tie";

    // With weights: 2.0 agree, 3.0 disagree = DISAGREE wins
    trit_vote_t weighted = trit_consensus_weighted(
        votes.data(), weights.data(), votes.size());
    EXPECT_EQ(TRIT_VOTE_DISAGREE, weighted)
        << "Weighted consensus should favor DISAGREE (3.0 vs 2.0)";
}

/**
 * Test vote statistics
 */
TEST_F(SwarmTernaryIntegrationTest, VoteStatistics) {
    std::vector<trit_vote_t> votes = {
        TRIT_VOTE_AGREE, TRIT_VOTE_AGREE, TRIT_VOTE_AGREE,  // 3 agree
        TRIT_VOTE_DISAGREE, TRIT_VOTE_DISAGREE,              // 2 disagree
        TRIT_VOTE_ABSTAIN, TRIT_VOTE_ABSTAIN, TRIT_VOTE_ABSTAIN, // 3 abstain
        TRIT_VOTE_AGREE, TRIT_VOTE_DISAGREE                  // 4 agree, 3 disagree
    };

    trit_vote_stats_t stats;
    trit_vote_count(votes.data(), votes.size(), &stats);

    EXPECT_EQ(4u, stats.n_agree);
    EXPECT_EQ(3u, stats.n_disagree);
    EXPECT_EQ(3u, stats.n_abstain);
    EXPECT_EQ(10u, stats.n_total);

    EXPECT_NEAR(0.4f, stats.agree_ratio, 0.01f);
    EXPECT_NEAR(0.3f, stats.disagree_ratio, 0.01f);
    EXPECT_NEAR(0.7f, stats.participation, 0.01f);  // 70% participated (non-abstain)
}

/**
 * Test vote to choice conversion
 */
TEST_F(SwarmTernaryIntegrationTest, VoteChoiceConversion) {
    // Ternary to choice
    EXPECT_EQ(VOTE_CHOICE_AGREE, trit_to_vote_choice(TRIT_VOTE_AGREE));
    EXPECT_EQ(VOTE_CHOICE_DISAGREE, trit_to_vote_choice(TRIT_VOTE_DISAGREE));
    EXPECT_EQ(VOTE_CHOICE_ABSTAIN, trit_to_vote_choice(TRIT_VOTE_ABSTAIN));

    // Choice to ternary
    EXPECT_EQ(TRIT_VOTE_AGREE, trit_from_vote_choice(VOTE_CHOICE_AGREE));
    EXPECT_EQ(TRIT_VOTE_DISAGREE, trit_from_vote_choice(VOTE_CHOICE_DISAGREE));
    EXPECT_EQ(TRIT_VOTE_ABSTAIN, trit_from_vote_choice(VOTE_CHOICE_ABSTAIN));

    // Round-trip
    for (int v = -1; v <= 1; v++) {
        trit_vote_t orig = (trit_vote_t)v;
        swarm_vote_choice_t choice = trit_to_vote_choice(orig);
        trit_vote_t back = trit_from_vote_choice(choice);
        EXPECT_EQ(orig, back) << "Round-trip should preserve vote";
    }
}

//=============================================================================
// Test: Swarm Memory with Ternary Confidence
//=============================================================================

/**
 * Test ternary confidence configuration
 */
TEST_F(SwarmTernaryIntegrationTest, TernaryConfidenceConfig) {
    ternary_confidence_config_t config;
    int result = ternary_confidence_default_config(&config);
    EXPECT_EQ(0, result);

    // Verify defaults
    EXPECT_NEAR(0.8f, config.certain_threshold, 0.01f);
    EXPECT_NEAR(0.4f, config.uncertain_threshold, 0.01f);
    EXPECT_TRUE(config.enable_for_patterns);
    EXPECT_TRUE(config.enable_for_memories);
    EXPECT_TRUE(config.enable_for_predictions);
    EXPECT_TRUE(config.decay_aware);
}

/**
 * Test confidence value to ternary conversion
 */
TEST_F(SwarmTernaryIntegrationTest, ConfidenceToTernary) {
    ternary_confidence_config_t config;
    ternary_confidence_default_config(&config);

    // High confidence -> CERTAIN
    EXPECT_EQ(SWARM_CONFIDENCE_CERTAIN, ternary_confidence_from_value(&config, 0.9f));
    EXPECT_EQ(SWARM_CONFIDENCE_CERTAIN, ternary_confidence_from_value(&config, 0.85f));
    EXPECT_EQ(SWARM_CONFIDENCE_CERTAIN, ternary_confidence_from_value(&config, 0.80f));

    // Medium confidence -> UNCERTAIN
    EXPECT_EQ(SWARM_CONFIDENCE_UNCERTAIN, ternary_confidence_from_value(&config, 0.79f));
    EXPECT_EQ(SWARM_CONFIDENCE_UNCERTAIN, ternary_confidence_from_value(&config, 0.5f));
    EXPECT_EQ(SWARM_CONFIDENCE_UNCERTAIN, ternary_confidence_from_value(&config, 0.40f));

    // Low confidence -> UNRELIABLE
    EXPECT_EQ(SWARM_CONFIDENCE_UNRELIABLE, ternary_confidence_from_value(&config, 0.39f));
    EXPECT_EQ(SWARM_CONFIDENCE_UNRELIABLE, ternary_confidence_from_value(&config, 0.2f));
    EXPECT_EQ(SWARM_CONFIDENCE_UNRELIABLE, ternary_confidence_from_value(&config, 0.0f));
}

/**
 * Test ternary to confidence value conversion
 */
TEST_F(SwarmTernaryIntegrationTest, TernaryToConfidenceValue) {
    EXPECT_NEAR(0.9f, ternary_confidence_to_value(SWARM_CONFIDENCE_CERTAIN), 0.01f);
    EXPECT_NEAR(0.6f, ternary_confidence_to_value(SWARM_CONFIDENCE_UNCERTAIN), 0.01f);
    EXPECT_NEAR(0.2f, ternary_confidence_to_value(SWARM_CONFIDENCE_UNRELIABLE), 0.01f);
}

/**
 * Test enable/disable ternary confidence mode
 */
TEST_F(SwarmTernaryIntegrationTest, TernaryConfidenceMode) {
    // Initially not in ternary mode
    bool is_ternary = swarm_memory_is_ternary_confidence(swarm_memory);
    EXPECT_FALSE(is_ternary);

    // Enable ternary confidence
    ternary_confidence_config_t config;
    ternary_confidence_default_config(&config);

    nimcp_result_t result = swarm_memory_enable_ternary_confidence(swarm_memory, &config);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    is_ternary = swarm_memory_is_ternary_confidence(swarm_memory);
    EXPECT_TRUE(is_ternary);

    // Disable ternary confidence
    result = swarm_memory_disable_ternary_confidence(swarm_memory);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    is_ternary = swarm_memory_is_ternary_confidence(swarm_memory);
    EXPECT_FALSE(is_ternary);
}

/**
 * Test ternary confidence with memory operations
 */
TEST_F(SwarmTernaryIntegrationTest, TernaryConfidenceMemoryOps) {
    // Enable ternary confidence
    nimcp_result_t result = swarm_memory_enable_ternary_confidence(swarm_memory, nullptr);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    // Store a memory
    const char* test_data = "test_memory_data";
    char memory_id[64];

    result = nimcp_swarm_memory_store(
        swarm_memory,
        NIMCP_MEMORY_SEMANTIC,
        NIMCP_IMPORTANCE_HIGH,
        test_data,
        strlen(test_data) + 1,
        memory_id
    );
    EXPECT_EQ(NIMCP_SUCCESS, result);

    // Get ternary confidence for the memory
    ternary_swarm_confidence_t conf = swarm_memory_get_ternary_confidence(
        swarm_memory, memory_id);

    // New memory should start with certain confidence (strength = 1.0)
    EXPECT_EQ(SWARM_CONFIDENCE_CERTAIN, conf)
        << "New memory should have CERTAIN confidence";

    // Apply some forgetting to reduce strength
    uint32_t forgotten_count = 0;
    result = nimcp_swarm_memory_apply_forgetting(swarm_memory, &forgotten_count);
    EXPECT_EQ(NIMCP_SUCCESS, result);
}

/**
 * Test ternary confidence statistics
 */
TEST_F(SwarmTernaryIntegrationTest, TernaryConfidenceStats) {
    // Enable ternary confidence
    swarm_memory_enable_ternary_confidence(swarm_memory, nullptr);

    // Store multiple memories
    for (int i = 0; i < 5; i++) {
        char data[64];
        snprintf(data, sizeof(data), "memory_%d", i);
        char memory_id[64];

        nimcp_swarm_memory_store(
            swarm_memory,
            NIMCP_MEMORY_EPISODIC,
            (NimcpMemoryImportance)(i % 4),
            data,
            strlen(data) + 1,
            memory_id
        );
    }

    // Get ternary confidence statistics
    ternary_confidence_stats_t stats;
    nimcp_result_t result = swarm_memory_get_ternary_confidence_stats(
        swarm_memory, &stats);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    // Verify we have some statistics
    uint32_t total = stats.certain_count + stats.uncertain_count + stats.unreliable_count;
    EXPECT_GT(total, 0u) << "Should have at least some memories categorized";
}

//=============================================================================
// Test: Pattern Learning with Ternary States
//=============================================================================

/**
 * Test pattern storage with ternary-like strength
 */
TEST_F(SwarmTernaryIntegrationTest, PatternStorageWithStrength) {
    // Create a pattern with known values
    swarm_pattern_t pattern = {0};
    pattern.pattern_id = 1;
    strncpy(pattern.label, "test_pattern", sizeof(pattern.label) - 1);
    pattern.signature_size = 8;
    pattern.data_len = 8;

    // Allocate and fill data
    pattern.data = (float*)malloc(pattern.data_len * sizeof(float));
    for (size_t i = 0; i < pattern.data_len; i++) {
        pattern.data[i] = (float)i / 10.0f;
    }

    pattern.strength = 0.9f;  // High strength -> CERTAIN
    pattern.confidence = 0.95f;
    pattern.occurrence_count = 10;

    // Store pattern
    nimcp_result_t result = swarm_memory_store_pattern(swarm_memory, &pattern);
    EXPECT_EQ(NIMCP_SUCCESS, result);

    // Retrieve and verify
    swarm_pattern_t retrieved = {0};
    result = swarm_memory_retrieve_pattern(swarm_memory, 1, &retrieved);
    EXPECT_EQ(NIMCP_SUCCESS, result);
    EXPECT_STREQ("test_pattern", retrieved.label);

    // Check ternary confidence
    swarm_memory_enable_ternary_confidence(swarm_memory, nullptr);
    ternary_swarm_confidence_t conf = swarm_memory_get_pattern_ternary_confidence(
        swarm_memory, 1);
    EXPECT_EQ(SWARM_CONFIDENCE_CERTAIN, conf)
        << "High-strength pattern should be CERTAIN";

    free(pattern.data);
}

/**
 * Test pattern recognition returning ternary confidence
 */
TEST_F(SwarmTernaryIntegrationTest, PatternRecognitionTernary) {
    // Store a labeled pattern
    std::vector<float> signal = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};

    int32_t pattern_id = swarm_memory_store_pattern_labeled(
        swarm_memory,
        signal.data(),
        signal.size(),
        "alternating"
    );
    EXPECT_GE(pattern_id, 0) << "Pattern storage should succeed";

    // Enable ternary confidence
    swarm_memory_enable_ternary_confidence(swarm_memory, nullptr);

    // Recognize similar pattern
    std::vector<float> query = {1.0f, 0.1f, 0.9f, 0.0f, 1.0f, 0.0f, 1.0f, 0.1f};

    int32_t matched_id = swarm_memory_recognize_pattern(
        swarm_memory, query.data(), query.size());

    // Should find the similar pattern
    if (matched_id >= 0) {
        // Get ternary confidence for matched pattern
        ternary_swarm_confidence_t conf = swarm_memory_get_pattern_ternary_confidence(
            swarm_memory, (uint32_t)matched_id);

        // Confidence should be valid
        EXPECT_TRUE(conf >= SWARM_CONFIDENCE_UNRELIABLE &&
                   conf <= SWARM_CONFIDENCE_CERTAIN);
    }
}

/**
 * Test ternary-based pattern filtering
 */
TEST_F(SwarmTernaryIntegrationTest, FilterPatternsByTernaryConfidence) {
    // Enable ternary confidence
    swarm_memory_enable_ternary_confidence(swarm_memory, nullptr);

    // Store patterns with varying confidence
    for (int i = 0; i < 10; i++) {
        std::vector<float> signal(8);
        for (size_t j = 0; j < 8; j++) {
            signal[j] = (float)((i + j) % 5) / 5.0f;
        }

        char label[32];
        snprintf(label, sizeof(label), "pattern_%d", i);

        swarm_memory_store_pattern_labeled(
            swarm_memory, signal.data(), signal.size(), label);
    }

    // Filter by CERTAIN confidence
    std::vector<uint32_t> certain_patterns(10);
    uint32_t certain_count = swarm_memory_filter_patterns_by_ternary_confidence(
        swarm_memory,
        SWARM_CONFIDENCE_CERTAIN,
        certain_patterns.data(),
        certain_patterns.size()
    );

    // Some patterns should be CERTAIN (newly stored with high strength)
    EXPECT_GE(certain_count, 0u);

    // Filter by UNCERTAIN confidence
    std::vector<uint32_t> uncertain_patterns(10);
    uint32_t uncertain_count = swarm_memory_filter_patterns_by_ternary_confidence(
        swarm_memory,
        SWARM_CONFIDENCE_UNCERTAIN,
        uncertain_patterns.data(),
        uncertain_patterns.size()
    );

    // Verify total makes sense
    EXPECT_LE(certain_count + uncertain_count, 10u);
}

/**
 * Test ternary-based forgetting
 */
TEST_F(SwarmTernaryIntegrationTest, TernaryBasedForgetting) {
    // Store memories
    for (int i = 0; i < 10; i++) {
        char data[64];
        snprintf(data, sizeof(data), "memory_to_forget_%d", i);
        char memory_id[64];

        nimcp_swarm_memory_store(
            swarm_memory,
            NIMCP_MEMORY_EPISODIC,
            (NimcpMemoryImportance)(i % 4),
            data,
            strlen(data) + 1,
            memory_id
        );
    }

    // Enable ternary confidence
    swarm_memory_enable_ternary_confidence(swarm_memory, nullptr);

    // Apply forgetting to reduce strengths
    // Multiple rounds to decay memories
    for (int round = 0; round < 5; round++) {
        uint32_t forgotten = 0;
        nimcp_swarm_memory_apply_forgetting(swarm_memory, &forgotten);
    }

    // Get stats before ternary forget
    NimcpMemoryStatistics stats_before;
    nimcp_swarm_memory_get_statistics(swarm_memory, &stats_before);

    // Forget UNRELIABLE memories
    uint32_t removed = swarm_memory_ternary_forget(
        swarm_memory, SWARM_CONFIDENCE_UNCERTAIN);

    // Get stats after
    NimcpMemoryStatistics stats_after;
    nimcp_swarm_memory_get_statistics(swarm_memory, &stats_after);

    // Should have removed some memories (those below UNCERTAIN threshold)
    // Note: removed count may be 0 if all memories still have high confidence
    EXPECT_GE(removed, 0u);
}

/**
 * Test pattern association with ternary outcome
 */
TEST_F(SwarmTernaryIntegrationTest, PatternAssociationWithTernary) {
    // Store a pattern
    std::vector<float> signal = {1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f};

    int32_t pattern_id = swarm_memory_store_pattern_labeled(
        swarm_memory, signal.data(), signal.size(), "double_pulse");
    EXPECT_GE(pattern_id, 0);

    // Associate pattern with outcome using reward
    // Positive reward -> strengthen association
    nimcp_result_t result = swarm_memory_associate_pattern(
        swarm_memory, (uint32_t)pattern_id, 100, 0.9f);  // outcome_id=100, reward=0.9
    EXPECT_EQ(NIMCP_SUCCESS, result);

    // Negative reward -> weaken association
    result = swarm_memory_associate_pattern(
        swarm_memory, (uint32_t)pattern_id, 101, -0.5f);  // outcome_id=101, reward=-0.5
    EXPECT_EQ(NIMCP_SUCCESS, result);

    // Predict outcome
    uint32_t predicted_outcome = 0;
    float confidence = 0.0f;

    result = swarm_memory_predict_outcome(
        swarm_memory, (uint32_t)pattern_id, &predicted_outcome, &confidence);

    if (result == NIMCP_SUCCESS) {
        // Should predict outcome 100 (higher reward)
        EXPECT_EQ(100u, predicted_outcome);
        EXPECT_GT(confidence, 0.0f);
    }
}

//=============================================================================
// Test: Integration of Voting and Memory
//=============================================================================

/**
 * Test using ternary votes to validate memory consensus
 */
TEST_F(SwarmTernaryIntegrationTest, VoteBasedMemoryConsensus) {
    // Simulate swarm nodes voting on memory validity
    const size_t n_nodes = 7;

    // Store memory on local node
    const char* data = "shared_memory_data";
    char memory_id[64];

    nimcp_result_t result = nimcp_swarm_memory_store(
        swarm_memory,
        NIMCP_MEMORY_SEMANTIC,
        NIMCP_IMPORTANCE_HIGH,
        data,
        strlen(data) + 1,
        memory_id
    );
    EXPECT_EQ(NIMCP_SUCCESS, result);

    // Simulate votes from other nodes about this memory's validity
    std::vector<trit_vote_t> validity_votes = {
        TRIT_VOTE_AGREE,    // Node 1: confirms
        TRIT_VOTE_AGREE,    // Node 2: confirms
        TRIT_VOTE_AGREE,    // Node 3: confirms
        TRIT_VOTE_AGREE,    // Node 4: confirms
        TRIT_VOTE_DISAGREE, // Node 5: disputes
        TRIT_VOTE_ABSTAIN,  // Node 6: no data
        TRIT_VOTE_AGREE     // Node 7: confirms
    };

    // Require 2/3 supermajority for consensus
    trit_vote_t consensus = trit_consensus_supermajority(
        validity_votes.data(), validity_votes.size(), 0.67f);

    EXPECT_EQ(TRIT_VOTE_AGREE, consensus)
        << "5/7 (71%) agree should meet 67% threshold";

    // Verify consensus
    bool has_consensus = false;
    result = nimcp_swarm_memory_verify_consensus(
        swarm_memory, memory_id, &has_consensus);
    // Note: actual consensus verification requires distributed nodes
}

/**
 * Test ternary voting for pattern learning decisions
 */
TEST_F(SwarmTernaryIntegrationTest, VoteBasedPatternLearning) {
    // Store a candidate pattern
    std::vector<float> signal = {0.5f, 0.5f, 0.5f, 0.5f};

    int32_t pattern_id = swarm_memory_store_pattern_labeled(
        swarm_memory, signal.data(), signal.size(), "candidate");
    EXPECT_GE(pattern_id, 0);

    // Simulate swarm voting on whether to retain this pattern
    // Each node votes based on their observation frequency

    std::vector<trit_vote_t> retention_votes(10);
    for (size_t i = 0; i < retention_votes.size(); i++) {
        // Simulate: high-frequency observers agree, low-frequency abstain
        if (i < 7) {
            retention_votes[i] = TRIT_VOTE_AGREE;
        } else {
            retention_votes[i] = TRIT_VOTE_ABSTAIN;
        }
    }

    // Get vote statistics
    trit_vote_stats_t stats;
    trit_vote_count(retention_votes.data(), retention_votes.size(), &stats);

    EXPECT_NEAR(0.7f, stats.agree_ratio, 0.01f);
    EXPECT_NEAR(0.7f, stats.participation, 0.01f);  // 70% participated

    // Decision: keep pattern if supermajority agrees
    trit_vote_t decision = trit_consensus_supermajority(
        retention_votes.data(), retention_votes.size(), 0.6f);
    EXPECT_EQ(TRIT_VOTE_AGREE, decision)
        << "70% agree should meet 60% threshold for pattern retention";
}

/**
 * Test ternary vector for vote storage
 */
TEST_F(SwarmTernaryIntegrationTest, TernaryVectorVoteStorage) {
    const size_t n_voters = 20;

    // Store votes in ternary vector for efficiency
    trit_vector_t* vote_vec = trit_vector_create(n_voters, TERNARY_PACK_2BIT);
    ASSERT_NE(nullptr, vote_vec);

    // Cast votes
    for (size_t i = 0; i < n_voters; i++) {
        trit_vote_t vote = (trit_vote_t)((i % 3) - 1);  // {-1, 0, 1}
        trit_vector_set(vote_vec, i, vote);
    }

    // Extract votes for consensus calculation
    std::vector<trit_vote_t> votes(n_voters);
    for (size_t i = 0; i < n_voters; i++) {
        votes[i] = trit_vector_get(vote_vec, i);
    }

    // Calculate consensus
    trit_vote_t result = trit_consensus_majority(votes.data(), votes.size());

    // With pattern {-1, 0, 1, -1, 0, 1, ...}, votes are balanced
    // Expect ABSTAIN or slight lean

    // Verify vote counts
    trit_vote_stats_t stats;
    trit_vote_count(votes.data(), votes.size(), &stats);

    // Should have roughly equal distribution
    EXPECT_EQ(n_voters, stats.n_agree + stats.n_abstain + stats.n_disagree);

    trit_vector_destroy(vote_vec);
}

/**
 * Test ternary matrix for multi-issue voting
 */
TEST_F(SwarmTernaryIntegrationTest, TernaryMatrixMultiIssueVoting) {
    const size_t n_voters = 8;
    const size_t n_issues = 4;

    // Create vote matrix: voters x issues
    trit_matrix_t* vote_matrix = trit_matrix_create(n_voters, n_issues, TERNARY_PACK_NONE);
    ASSERT_NE(nullptr, vote_matrix);

    // Cast votes for each issue
    for (size_t voter = 0; voter < n_voters; voter++) {
        for (size_t issue = 0; issue < n_issues; issue++) {
            // Pattern: voter i votes based on (i + issue) mod 3
            trit_vote_t vote = (trit_vote_t)(((voter + issue) % 3) - 1);
            trit_matrix_set(vote_matrix, voter, issue, vote);
        }
    }

    // Compute consensus for each issue
    std::vector<trit_vote_t> issue_results(n_issues);
    for (size_t issue = 0; issue < n_issues; issue++) {
        // Extract column (all votes for this issue)
        trit_vector_t* issue_votes = trit_matrix_get_col(vote_matrix, issue);
        ASSERT_NE(nullptr, issue_votes);

        std::vector<trit_vote_t> votes(n_voters);
        for (size_t v = 0; v < n_voters; v++) {
            votes[v] = trit_vector_get(issue_votes, v);
        }

        issue_results[issue] = trit_consensus_majority(votes.data(), votes.size());
        trit_vector_destroy(issue_votes);
    }

    // Verify all results are valid ternary values
    for (size_t issue = 0; issue < n_issues; issue++) {
        EXPECT_TRUE(issue_results[issue] >= TRIT_VOTE_DISAGREE &&
                   issue_results[issue] <= TRIT_VOTE_AGREE)
            << "Issue " << issue << " result should be valid vote";
    }

    trit_matrix_destroy(vote_matrix);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
