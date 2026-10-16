/**
 * @file test_reasoning_mesh_bridge.cpp
 * @brief Unit tests for the Reasoning-Mesh Bridge
 *
 * WHAT: Tests mesh evidence gathering, application, consensus, and summary in isolation
 * WHY:  Verify mesh bridge logic independently of actual mesh network state.
 *       In unit tests no mesh bootstrap is set, so gather_evidence() returns empty
 *       and training integration functions return safe defaults — that is expected.
 * HOW:  GTest fixture exercising each function with crafted results
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

class ReasoningMeshBridgeUnit : public ::testing::Test {};

// =============================================================================
// Tests: Empty Result Defaults
// =============================================================================

TEST_F(ReasoningMeshBridgeUnit, EmptyResultDefaults) {
    reasoning_mesh_result_t result = reasoning_mesh_empty_result();

    EXPECT_FALSE(result.mesh_available);
    EXPECT_EQ(result.evidence_count, 0u);
    EXPECT_FLOAT_EQ(result.consensus_confidence, 0.0f);
    EXPECT_FLOAT_EQ(result.coherence, 0.0f);
}

// =============================================================================
// Tests: Gather Evidence NULL/empty guards
// =============================================================================

TEST_F(ReasoningMeshBridgeUnit, GatherNullQueryReturnsEmpty) {
    reasoning_mesh_result_t result = reasoning_mesh_gather_evidence(NULL, NULL, 0);

    EXPECT_FALSE(result.mesh_available);
    EXPECT_EQ(result.evidence_count, 0u);
    EXPECT_FLOAT_EQ(result.consensus_confidence, 0.0f);
    EXPECT_FLOAT_EQ(result.coherence, 0.0f);
}

TEST_F(ReasoningMeshBridgeUnit, GatherNoBrainReturnsResults) {
    /* No mesh bootstrap set, so mesh is unavailable — returns empty */
    reasoning_mesh_result_t result = reasoning_mesh_gather_evidence(NULL, "test query", 0);

    EXPECT_FALSE(result.mesh_available);
    EXPECT_EQ(result.evidence_count, 0u);
}

// =============================================================================
// Tests: Apply Evidence NULL guards
// =============================================================================

TEST_F(ReasoningMeshBridgeUnit, ApplyEvidenceNullChainReturnsError) {
    reasoning_mesh_result_t result = reasoning_mesh_empty_result();
    EXPECT_EQ(reasoning_mesh_apply_evidence(NULL, &result), -1);
}

TEST_F(ReasoningMeshBridgeUnit, ApplyEvidenceNullResultReturnsError) {
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    EXPECT_EQ(reasoning_mesh_apply_evidence(&chain, NULL), -1);
    reasoning_chain_cleanup(&chain);
}

// =============================================================================
// Tests: Apply Evidence with unavailable mesh / no evidence
// =============================================================================

TEST_F(ReasoningMeshBridgeUnit, ApplyEvidenceUnavailableMeshReturnsZero) {
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    reasoning_mesh_result_t result = reasoning_mesh_empty_result();
    /* mesh_available is false */

    int rc = reasoning_mesh_apply_evidence(&chain, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(chain.num_steps, 0u);

    reasoning_chain_cleanup(&chain);
}

TEST_F(ReasoningMeshBridgeUnit, ApplyEvidenceNoEvidence) {
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    reasoning_mesh_result_t result;
    memset(&result, 0, sizeof(result));
    result.mesh_available = true;
    result.evidence_count = 0;

    int rc = reasoning_mesh_apply_evidence(&chain, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(chain.num_steps, 0u);

    reasoning_chain_cleanup(&chain);
}

// =============================================================================
// Tests: Apply Evidence with crafted endorsed/unendorsed items
// =============================================================================

TEST_F(ReasoningMeshBridgeUnit, ApplyEvidenceEndorsedAddsSteps) {
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    EXPECT_EQ(chain.num_steps, 0u);

    reasoning_mesh_result_t result;
    memset(&result, 0, sizeof(result));
    result.mesh_available = true;
    result.evidence_count = 3;

    /* Craft 3 endorsed evidence items */
    for (uint32_t i = 0; i < 3; i++) {
        result.evidence[i].source = REASONING_EVIDENCE_MEMORY;
        result.evidence[i].confidence = 0.8f;
        result.evidence[i].relevance = 0.7f;
        result.evidence[i].endorsed = true;
        result.evidence[i].source_id = 0x100 + i;
        snprintf(result.evidence[i].description,
                 sizeof(result.evidence[i].description),
                 "Endorsed evidence %u", i);
    }

    int steps_added = reasoning_mesh_apply_evidence(&chain, &result);
    EXPECT_EQ(steps_added, 3);
    EXPECT_EQ(chain.num_steps, 3u);

    reasoning_chain_cleanup(&chain);
}

TEST_F(ReasoningMeshBridgeUnit, ApplyEvidenceUnendorsedSkipped) {
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    reasoning_mesh_result_t result;
    memset(&result, 0, sizeof(result));
    result.mesh_available = true;
    result.evidence_count = 2;

    /* Craft 2 unendorsed evidence items */
    for (uint32_t i = 0; i < 2; i++) {
        result.evidence[i].source = REASONING_EVIDENCE_KNOWLEDGE;
        result.evidence[i].confidence = 0.6f;
        result.evidence[i].relevance = 0.5f;
        result.evidence[i].endorsed = false;
        snprintf(result.evidence[i].description,
                 sizeof(result.evidence[i].description),
                 "Unendorsed evidence %u", i);
    }

    int steps_added = reasoning_mesh_apply_evidence(&chain, &result);
    EXPECT_EQ(steps_added, 0);
    EXPECT_EQ(chain.num_steps, 0u);

    reasoning_chain_cleanup(&chain);
}

TEST_F(ReasoningMeshBridgeUnit, ApplyEvidenceMixedEndorsement) {
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    reasoning_mesh_result_t result;
    memset(&result, 0, sizeof(result));
    result.mesh_available = true;
    result.evidence_count = 5;

    /* Items 0, 2, 4 endorsed; items 1, 3 not endorsed */
    for (uint32_t i = 0; i < 5; i++) {
        result.evidence[i].source = REASONING_EVIDENCE_PLANNING;
        result.evidence[i].confidence = 0.75f;
        result.evidence[i].relevance = 0.6f;
        result.evidence[i].endorsed = (i % 2 == 0);
        snprintf(result.evidence[i].description,
                 sizeof(result.evidence[i].description),
                 "Mixed evidence %u", i);
    }

    int steps_added = reasoning_mesh_apply_evidence(&chain, &result);
    EXPECT_EQ(steps_added, 3);  /* Only items 0, 2, 4 endorsed */
    EXPECT_EQ(chain.num_steps, 3u);

    reasoning_chain_cleanup(&chain);
}

// =============================================================================
// Tests: Apply Consensus NULL guards
// =============================================================================

TEST_F(ReasoningMeshBridgeUnit, ApplyConsensusNullConfigReturnsError) {
    reasoning_mesh_result_t result = reasoning_mesh_empty_result();
    EXPECT_EQ(reasoning_mesh_apply_consensus(NULL, &result), -1);
}

TEST_F(ReasoningMeshBridgeUnit, ApplyConsensusNullResultReturnsError) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    EXPECT_EQ(reasoning_mesh_apply_consensus(&config, NULL), -1);
}

// =============================================================================
// Tests: Apply Consensus behavior
// =============================================================================

TEST_F(ReasoningMeshBridgeUnit, ApplyConsensusUnavailableMeshNoChange) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    float original_threshold = config.confidence_threshold;

    reasoning_mesh_result_t result = reasoning_mesh_empty_result();

    int rc = reasoning_mesh_apply_consensus(&config, &result);
    EXPECT_EQ(rc, 0);
    EXPECT_FLOAT_EQ(config.confidence_threshold, original_threshold);
}

TEST_F(ReasoningMeshBridgeUnit, ApplyConsensusHighApprovalBoosts) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    float original_threshold = config.confidence_threshold;

    reasoning_mesh_result_t result;
    memset(&result, 0, sizeof(result));
    result.mesh_available = true;
    result.endorsements_received = 10;
    result.endorsements_approved = 9;    /* approval_rate = 0.9 > 0.8 */
    result.consensus_confidence = 0.85f; /* > 0.7 */

    int rc = reasoning_mesh_apply_consensus(&config, &result);
    EXPECT_EQ(rc, 0);

    /* Boost = (0.9 - 0.8) * 0.25 = 0.025 */
    float expected_threshold = original_threshold + 0.025f;
    EXPECT_FLOAT_EQ(config.confidence_threshold, expected_threshold);
    EXPECT_GT(config.confidence_threshold, original_threshold);
}

TEST_F(ReasoningMeshBridgeUnit, ApplyConsensusThresholdCappedAt095) {
    reasoning_engine_config_t config = reasoning_engine_default_config();
    config.confidence_threshold = 0.94f;  /* Already near max */

    reasoning_mesh_result_t result;
    memset(&result, 0, sizeof(result));
    result.mesh_available = true;
    result.endorsements_received = 10;
    result.endorsements_approved = 10;   /* approval_rate = 1.0 > 0.8 */
    result.consensus_confidence = 0.9f;  /* > 0.7 */

    int rc = reasoning_mesh_apply_consensus(&config, &result);
    EXPECT_EQ(rc, 0);

    /* Boost = (1.0 - 0.8) * 0.25 = 0.05, new = 0.94 + 0.05 = 0.99 -> capped at 0.95 */
    EXPECT_LE(config.confidence_threshold, 0.95f);
}

// =============================================================================
// Tests: Summary NULL guards
// =============================================================================

TEST_F(ReasoningMeshBridgeUnit, SummaryNullResultReturnsError) {
    char buf[256];
    EXPECT_EQ(reasoning_mesh_result_summary(NULL, buf, sizeof(buf)), -1);
}

TEST_F(ReasoningMeshBridgeUnit, SummaryNullBufferReturnsError) {
    reasoning_mesh_result_t result = reasoning_mesh_empty_result();
    EXPECT_EQ(reasoning_mesh_result_summary(&result, NULL, 256), -1);
}

TEST_F(ReasoningMeshBridgeUnit, SummaryZeroSizeReturnsError) {
    reasoning_mesh_result_t result = reasoning_mesh_empty_result();
    char buf[1];
    EXPECT_EQ(reasoning_mesh_result_summary(&result, buf, 0), -1);
}

// =============================================================================
// Tests: Summary content
// =============================================================================

TEST_F(ReasoningMeshBridgeUnit, SummaryContainsStatusAndMetrics) {
    char buf[512];

    /* Unavailable mesh */
    reasoning_mesh_result_t result = reasoning_mesh_empty_result();
    int written = reasoning_mesh_result_summary(&result, buf, sizeof(buf));
    EXPECT_GT(written, 0);
    EXPECT_NE(strstr(buf, "UNAVAILABLE"), nullptr);
    EXPECT_NE(strstr(buf, "evidence="), nullptr);
    EXPECT_NE(strstr(buf, "consensus="), nullptr);

    /* Available mesh */
    memset(&result, 0, sizeof(result));
    result.mesh_available = true;
    result.evidence_count = 3;
    result.consensus_confidence = 0.75f;
    result.coherence = 0.9f;
    result.endorsements_received = 5;
    result.endorsements_approved = 4;

    written = reasoning_mesh_result_summary(&result, buf, sizeof(buf));
    EXPECT_GT(written, 0);
    EXPECT_NE(strstr(buf, "AVAILABLE"), nullptr);
    EXPECT_NE(strstr(buf, "evidence="), nullptr);
    EXPECT_NE(strstr(buf, "consensus="), nullptr);
}

// =============================================================================
// Tests: Training Integration wrappers (no bootstrap set)
// =============================================================================

TEST_F(ReasoningMeshBridgeUnit, TrainingIntegrationMeshNotAvailable) {
    /* No mesh bootstrap set — should return false */
    EXPECT_FALSE(brain_ti_mesh_is_available());
}

TEST_F(ReasoningMeshBridgeUnit, TrainingIntegrationParticipantCountZero) {
    /* No mesh bootstrap set — should return 0 */
    EXPECT_EQ(brain_ti_mesh_get_participant_count(), 0u);
}

TEST_F(ReasoningMeshBridgeUnit, TrainingIntegrationCoherenceZero) {
    /* No mesh bootstrap set — should return 0.0 */
    EXPECT_FLOAT_EQ(brain_ti_mesh_get_coherence(), 0.0f);
}
