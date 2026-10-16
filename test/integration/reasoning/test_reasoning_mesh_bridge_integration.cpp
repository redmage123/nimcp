/**
 * @file test_reasoning_mesh_bridge_integration.cpp
 * @brief Integration tests for the Reasoning-Mesh Bridge with a live brain
 *
 * WHAT: Integration tests verifying mesh bridge works with real brain
 * WHY:  Verify that gather_evidence and TI wrappers produce valid results
 *       when connected to a live brain instance (even without mesh bootstrap,
 *       the functions should gracefully degrade — mesh_available=false,
 *       empty results, no crashes)
 * HOW:  GTest fixture with brain_create/destroy lifecycle
 *
 * @author NIMCP Development Team
 * @date 2026-02-26
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "core/brain/nimcp_brain.h"
#include "cognitive/reasoning/nimcp_reasoning_mesh_bridge.h"
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/training/nimcp_training_integration.h"
}

// =============================================================================
// Test Fixture
// =============================================================================

class ReasoningMeshBridgeIntegration : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create("mesh_bridge_test", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, 32, 8);
        ASSERT_NE(brain, nullptr);
    }

    void TearDown() override {
        if (brain) brain_destroy(brain);
    }
};

// =============================================================================
// Tests: Gather evidence with real brain — graceful degradation
// =============================================================================

TEST_F(ReasoningMeshBridgeIntegration, GatherWithRealBrainGracefulDegradation) {
    reasoning_mesh_result_t result =
        reasoning_mesh_gather_evidence(brain, "test query", 100);

    /* Mesh bootstrap is not set, so mesh_available must be false */
    EXPECT_FALSE(result.mesh_available);
    EXPECT_EQ(result.evidence_count, 0u);
    EXPECT_EQ(result.endorsements_received, 0u);
    EXPECT_EQ(result.endorsements_approved, 0u);
    EXPECT_FLOAT_EQ(result.consensus_confidence, 0.0f);
    EXPECT_FLOAT_EQ(result.coherence, 0.0f);
}

// =============================================================================
// Tests: Gather summary with brain
// =============================================================================

TEST_F(ReasoningMeshBridgeIntegration, GatherSummaryWithBrain) {
    reasoning_mesh_result_t result =
        reasoning_mesh_gather_evidence(brain, "test query", 100);

    char buf[512];
    int written = reasoning_mesh_result_summary(&result, buf, sizeof(buf));
    EXPECT_GT(written, 0);
    EXPECT_GT(strlen(buf), 0u);

    /* Mesh is not initialized, so summary should contain "UNAVAILABLE" */
    EXPECT_NE(strstr(buf, "UNAVAILABLE"), nullptr)
        << "Summary should indicate mesh is UNAVAILABLE, got: " << buf;
}

// =============================================================================
// Tests: Apply evidence to chain with brain — empty evidence adds 0 steps
// =============================================================================

TEST_F(ReasoningMeshBridgeIntegration, ApplyEvidenceToChainWithBrain) {
    reasoning_mesh_result_t result =
        reasoning_mesh_gather_evidence(brain, "test query", 100);

    reasoning_chain_t chain;
    reasoning_chain_init(&chain);

    int steps_added = reasoning_mesh_apply_evidence(&chain, &result);
    /* Empty evidence (mesh_available=false) should add 0 steps */
    EXPECT_EQ(steps_added, 0);
    EXPECT_EQ(reasoning_chain_get_num_steps(&chain), 0u);

    reasoning_chain_cleanup(&chain);
}

// =============================================================================
// Tests: Apply consensus with brain — empty evidence leaves config unchanged
// =============================================================================

TEST_F(ReasoningMeshBridgeIntegration, ApplyConsensusWithBrain) {
    reasoning_mesh_result_t result =
        reasoning_mesh_gather_evidence(brain, "test query", 100);

    reasoning_engine_config_t config = reasoning_engine_default_config();
    float original_threshold = config.confidence_threshold;

    int rc = reasoning_mesh_apply_consensus(&config, &result);
    EXPECT_EQ(rc, 0);
    /* No mesh means no consensus boost — threshold unchanged */
    EXPECT_FLOAT_EQ(config.confidence_threshold, original_threshold);
}

// =============================================================================
// Tests: Training integration mesh wrappers with real brain
// =============================================================================

TEST_F(ReasoningMeshBridgeIntegration, TrainingIntegrationMeshWithBrain) {
    /* Mesh bootstrap is not set, so TI wrappers should report unavailable */
    bool available = brain_ti_mesh_is_available();
    EXPECT_FALSE(available);

    uint32_t participants = brain_ti_mesh_get_participant_count();
    EXPECT_EQ(participants, 0u);

    float coherence = brain_ti_mesh_get_coherence();
    EXPECT_FLOAT_EQ(coherence, 0.0f);
}

// =============================================================================
// Tests: Channel stats without mesh
// =============================================================================

TEST_F(ReasoningMeshBridgeIntegration, ChannelStatsWithoutMesh) {
    uint32_t participants = 999;
    float coherence = 999.0f;

    int rc = reasoning_mesh_get_channel_stats(&participants, &coherence);
    /* Should return -1 when mesh is unavailable */
    EXPECT_EQ(rc, -1);
    /* Output parameters should be zeroed */
    EXPECT_EQ(participants, 0u);
    EXPECT_FLOAT_EQ(coherence, 0.0f);
}

// =============================================================================
// Tests: Repeated gather stability — no crashes over multiple calls
// =============================================================================

TEST_F(ReasoningMeshBridgeIntegration, RepeatedGatherStability) {
    for (int i = 0; i < 10; i++) {
        reasoning_mesh_result_t result =
            reasoning_mesh_gather_evidence(brain, "stability test", 100);

        EXPECT_FALSE(result.mesh_available) << "Iteration " << i;
        EXPECT_EQ(result.evidence_count, 0u) << "Iteration " << i;
        EXPECT_FLOAT_EQ(result.consensus_confidence, 0.0f) << "Iteration " << i;
    }
}
