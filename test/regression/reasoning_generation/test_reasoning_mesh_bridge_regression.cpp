/**
 * @file test_reasoning_mesh_bridge_regression.cpp
 * @brief Regression tests for the Reasoning-Mesh bridge
 *
 * WHAT: Regression tests guarding against specific edge-case bugs
 * WHY:  Prevent regressions in NULL handling, buffer overflow in summary,
 *       consensus threshold capping, evidence filtering, and apply semantics
 * HOW:  GTest fixture exercising edge cases and invariants
 *
 * @author NIMCP Development Team
 * @date 2026-02-26
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_mesh_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/training/nimcp_training_integration.h"
}

// =============================================================================
// Test Fixture
// =============================================================================

class ReasoningMeshBridgeRegression : public ::testing::Test {};

// =============================================================================
// Tests: Repeated empty result stability
// =============================================================================

TEST_F(ReasoningMeshBridgeRegression, RepeatedEmptyResultStable) {
    reasoning_mesh_result_t first = reasoning_mesh_empty_result();

    for (int i = 0; i < 100; i++) {
        reasoning_mesh_result_t result = reasoning_mesh_empty_result();
        EXPECT_EQ(result.mesh_available, first.mesh_available) << "Iteration " << i;
        EXPECT_EQ(result.evidence_count, first.evidence_count) << "Iteration " << i;
        EXPECT_FLOAT_EQ(result.consensus_confidence, first.consensus_confidence) << "Iteration " << i;
        EXPECT_FLOAT_EQ(result.coherence, first.coherence) << "Iteration " << i;
        EXPECT_EQ(result.endorsements_received, first.endorsements_received) << "Iteration " << i;
        EXPECT_EQ(result.endorsements_approved, first.endorsements_approved) << "Iteration " << i;
        EXPECT_FLOAT_EQ(result.gather_time_ms, first.gather_time_ms) << "Iteration " << i;
        EXPECT_EQ(result.channel_participant_count, first.channel_participant_count) << "Iteration " << i;
    }
}

// =============================================================================
// Tests: Repeated gather with NULL query stability
// =============================================================================

TEST_F(ReasoningMeshBridgeRegression, RepeatedGatherNullQueryStable) {
    for (int i = 0; i < 100; i++) {
        reasoning_mesh_result_t result = reasoning_mesh_gather_evidence(NULL, NULL, 0);
        EXPECT_FALSE(result.mesh_available) << "Iteration " << i;
        EXPECT_EQ(result.evidence_count, 0u) << "Iteration " << i;
        EXPECT_FLOAT_EQ(result.consensus_confidence, 0.0f) << "Iteration " << i;
        EXPECT_FLOAT_EQ(result.coherence, 0.0f) << "Iteration " << i;
    }
}

// =============================================================================
// Tests: Apply evidence both NULL returns error
// =============================================================================

TEST_F(ReasoningMeshBridgeRegression, ApplyBothNullReturnsError) {
    EXPECT_EQ(reasoning_mesh_apply_evidence(NULL, NULL), -1);
}

// =============================================================================
// Tests: Apply consensus both NULL returns error
// =============================================================================

TEST_F(ReasoningMeshBridgeRegression, ApplyConsensusBothNullReturnsError) {
    EXPECT_EQ(reasoning_mesh_apply_consensus(NULL, NULL), -1);
}

// =============================================================================
// Tests: Buffer overflow protection in summary — tiny buffer
// =============================================================================

TEST_F(ReasoningMeshBridgeRegression, SummaryTinyBufferNoOverflow) {
    reasoning_mesh_result_t result = reasoning_mesh_empty_result();
    char buf[1];
    buf[0] = 'X';

    /* Buffer size 1: snprintf writes NUL only */
    int written = reasoning_mesh_result_summary(&result, buf, 1);
    /* Should succeed (snprintf returns chars that would have been written) */
    EXPECT_GE(written, 0);
    EXPECT_EQ(buf[0], '\0');
}

// =============================================================================
// Tests: Buffer overflow protection in summary — small buffer truncates
// =============================================================================

TEST_F(ReasoningMeshBridgeRegression, SummarySmallBufferTruncates) {
    reasoning_mesh_result_t result = reasoning_mesh_empty_result();
    char buf[16];
    memset(buf, 'X', sizeof(buf));

    int written = reasoning_mesh_result_summary(&result, buf, sizeof(buf));
    EXPECT_GE(written, 0);
    /* Must be NUL-terminated */
    EXPECT_EQ(buf[15], '\0');
}

// =============================================================================
// Tests: Consensus threshold never exceeds 0.95
// =============================================================================

TEST_F(ReasoningMeshBridgeRegression, ConsensusThresholdNeverExceeds095) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.confidence_threshold = 0.94f;

    /* Craft result with 100% approval and very high confidence */
    reasoning_mesh_result_t result;
    memset(&result, 0, sizeof(result));
    result.mesh_available = true;
    result.endorsements_received = 100;
    result.endorsements_approved = 100;   /* 100% approval rate */
    result.consensus_confidence = 0.99f;  /* Very high confidence */

    int rc = reasoning_mesh_apply_consensus(&config, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_LE(config.confidence_threshold, 0.95f);
}

// =============================================================================
// Tests: Evidence count accuracy — exactly 5 endorsed items
// =============================================================================

TEST_F(ReasoningMeshBridgeRegression, EvidenceCountAccuracy) {
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    reasoning_mesh_result_t result;
    memset(&result, 0, sizeof(result));
    result.mesh_available = true;
    result.evidence_count = 5;

    /* Craft exactly 5 endorsed evidence items */
    for (uint32_t i = 0; i < 5; i++) {
        result.evidence[i].source = REASONING_EVIDENCE_MEMORY;
        result.evidence[i].confidence = 0.8f;
        result.evidence[i].relevance = 0.9f;
        result.evidence[i].endorsed = true;
        result.evidence[i].source_id = 100 + i;
        snprintf(result.evidence[i].description,
                 sizeof(result.evidence[i].description),
                 "Evidence item %u", i);
    }

    uint32_t steps_before = chain.num_steps;
    int added = reasoning_mesh_apply_evidence(&chain, &result);
    EXPECT_EQ(added, 5);
    EXPECT_EQ(chain.num_steps, steps_before + 5);

    reasoning_chain_cleanup(&chain);
}

// =============================================================================
// Tests: Unendorsed evidence never added
// =============================================================================

TEST_F(ReasoningMeshBridgeRegression, UnendorsedEvidenceNeverAdded) {
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    reasoning_mesh_result_t result;
    memset(&result, 0, sizeof(result));
    result.mesh_available = true;
    result.evidence_count = 3;

    /* All three evidence items are NOT endorsed */
    for (uint32_t i = 0; i < 3; i++) {
        result.evidence[i].source = REASONING_EVIDENCE_KNOWLEDGE;
        result.evidence[i].confidence = 0.6f;
        result.evidence[i].relevance = 0.7f;
        result.evidence[i].endorsed = false;
        result.evidence[i].source_id = 200 + i;
        snprintf(result.evidence[i].description,
                 sizeof(result.evidence[i].description),
                 "Unendorsed item %u", i);
    }

    int added = reasoning_mesh_apply_evidence(&chain, &result);
    EXPECT_EQ(added, 0);
    EXPECT_EQ(chain.num_steps, 0u);

    reasoning_chain_cleanup(&chain);
}

// =============================================================================
// Tests: Apply consensus low approval no boost
// =============================================================================

TEST_F(ReasoningMeshBridgeRegression, ApplyConsensusLowApprovalNoBoost) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    float original_threshold = config.confidence_threshold;

    reasoning_mesh_result_t result;
    memset(&result, 0, sizeof(result));
    result.mesh_available = true;
    result.endorsements_received = 10;
    result.endorsements_approved = 7;   /* 70% approval — below 0.8 threshold */
    result.consensus_confidence = 0.9f; /* High confidence, but approval too low */

    int rc = reasoning_mesh_apply_consensus(&config, &result);
    EXPECT_EQ(rc, 0);
    /* Threshold should NOT have been boosted */
    EXPECT_FLOAT_EQ(config.confidence_threshold, original_threshold);
}

// =============================================================================
// Tests: Apply consensus low confidence no boost
// =============================================================================

TEST_F(ReasoningMeshBridgeRegression, ApplyConsensusLowConfidenceNoBoost) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    float original_threshold = config.confidence_threshold;

    reasoning_mesh_result_t result;
    memset(&result, 0, sizeof(result));
    result.mesh_available = true;
    result.endorsements_received = 10;
    result.endorsements_approved = 9;   /* 90% approval — above 0.8 */
    result.consensus_confidence = 0.5f; /* Low confidence — below 0.7 threshold */

    int rc = reasoning_mesh_apply_consensus(&config, &result);
    EXPECT_EQ(rc, 0);
    /* Threshold should NOT have been boosted */
    EXPECT_FLOAT_EQ(config.confidence_threshold, original_threshold);
}

// =============================================================================
// Tests: Summary both NULL returns error
// =============================================================================

TEST_F(ReasoningMeshBridgeRegression, SummaryBothNullReturnsError) {
    EXPECT_EQ(reasoning_mesh_result_summary(NULL, NULL, 0), -1);
}
