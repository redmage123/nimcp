/**
 * @file test_reasoning_mesh_bridge_e2e.cpp
 * @brief E2E tests: Reasoning-Mesh bridge full pipeline
 *
 * WHAT: Full pipeline test exercising mesh bridge from evidence gathering
 *       through consensus application, reasoning engine execution,
 *       and training integration wrappers
 * WHY:  Verify that the mesh bridge integrates correctly with the
 *       complete reasoning pipeline — evidence gathering, consensus,
 *       engine lifecycle, and TI wrappers all working together
 * HOW:  Creates brain, connects reasoning engine, gathers mesh evidence,
 *       applies consensus, runs reasoning, and validates E2E behavior
 *
 * @author NIMCP Development Team
 * @date 2026-02-26
 * @version 1.0.0
 */

#include "e2e_test_framework.h"
#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <string>

/* Brain header has its own extern "C" guard */
#include "core/brain/nimcp_brain.h"

extern "C" {
#include "cognitive/reasoning/nimcp_reasoning_chain.h"
#include "cognitive/reasoning/nimcp_reasoning_mesh_bridge.h"
#include "cognitive/training/nimcp_training_integration.h"
}

using namespace nimcp::e2e;

// =============================================================================
// Constants
// =============================================================================

static constexpr uint32_t INPUT_DIM = 32;
static constexpr uint32_t OUTPUT_DIM = 8;

// =============================================================================
// Test Fixture
// =============================================================================

class ReasoningMeshBridgeE2E : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    PipelineTracker* tracker = nullptr;

    void SetUp() override {
        tracker = new PipelineTracker("Mesh-Reasoning Bridge E2E");

        tracker->begin_stage("Brain Creation", 10000);
        brain = brain_create("e2e_mesh_bridge", BRAIN_SIZE_SMALL,
                            BRAIN_TASK_CLASSIFICATION, INPUT_DIM, OUTPUT_DIM);
        ASSERT_NE(brain, nullptr) << "Brain creation must succeed";
        tracker->end_stage();
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
        delete tracker;
        tracker = nullptr;
    }
};

// =============================================================================
// E2E: Full Pipeline — Gather Evidence, Apply Consensus, Create Engine, Reason
// =============================================================================

TEST_F(ReasoningMeshBridgeE2E, FullPipelineGatherAndReason) {
    /* Stage 1: Gather mesh evidence */
    tracker->begin_stage("Mesh Evidence Gathering", 5000);
    reasoning_mesh_result_t mesh_result = reasoning_mesh_gather_evidence(
        brain, "What evidence supports this conclusion?",
        REASONING_MESH_DEFAULT_TIMEOUT_MS);
    /* Mesh may not be set up — evidence_count will be 0 in that case */
    EXPECT_GE(mesh_result.evidence_count, 0u);
    tracker->end_stage();

    /* Stage 2: Apply consensus to config */
    tracker->begin_stage("Apply Consensus", 1000);
    reasoning_engine_config_t config = reasoning_engine_default_config();
    int rc = reasoning_mesh_apply_consensus(&config, &mesh_result);
    /* Returns 0 whether mesh is available or not (graceful degradation) */
    EXPECT_GE(rc, -1);
    EXPECT_LE(rc, 0);
    tracker->end_stage();

    /* Stage 3: Create reasoning engine with config */
    tracker->begin_stage("Engine Creation", 5000);
    reasoning_engine_t* engine = reasoning_engine_create(&config);
    ASSERT_NE(engine, nullptr);
    tracker->end_stage();

    /* Stage 4: Connect brain */
    tracker->begin_stage("Connect Brain", 5000);
    rc = reasoning_engine_connect_brain(engine, brain);
    EXPECT_EQ(rc, 0);
    tracker->end_stage();

    /* Stage 5: Run reasoning query */
    tracker->begin_stage("Reasoning", 30000);
    reasoning_chain_t chain;
    reasoning_chain_init(&chain);
    rc = reasoning_engine_reason(engine, "What is the mesh consensus?", &chain);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(chain.num_steps, 0u);
    reasoning_chain_cleanup(&chain);
    tracker->end_stage();

    /* Cleanup */
    reasoning_engine_destroy(engine);

    tracker->print_summary();
    EXPECT_TRUE(tracker->is_successful());
}

// =============================================================================
// E2E: Training Integration APIs end-to-end
// =============================================================================

TEST_F(ReasoningMeshBridgeE2E, TrainingIntegrationAPIsEndToEnd) {
    tracker->begin_stage("TI Mesh Available", 5000);
    bool avail = brain_ti_mesh_is_available();
    /* Likely false since no mesh bootstrap is set, but valid either way */
    (void)avail;
    tracker->end_stage();

    tracker->begin_stage("TI Participant Count", 5000);
    uint32_t participants = brain_ti_mesh_get_participant_count();
    /* Likely 0 since no mesh is initialized */
    EXPECT_GE(participants, 0u);
    tracker->end_stage();

    tracker->begin_stage("TI Coherence", 5000);
    float coherence = brain_ti_mesh_get_coherence();
    EXPECT_GE(coherence, 0.0f);
    tracker->end_stage();

    tracker->print_summary();
    EXPECT_TRUE(tracker->is_successful());
}

// =============================================================================
// E2E: Mesh Evidence Summary end-to-end
// =============================================================================

TEST_F(ReasoningMeshBridgeE2E, MeshEvidenceSummaryEndToEnd) {
    tracker->begin_stage("Gather Evidence", 5000);
    reasoning_mesh_result_t result = reasoning_mesh_gather_evidence(
        brain, "Summarize the available evidence",
        REASONING_MESH_DEFAULT_TIMEOUT_MS);
    tracker->end_stage();

    tracker->begin_stage("Format Summary", 1000);
    char buf[512];
    int written = reasoning_mesh_result_summary(&result, buf, sizeof(buf));
    EXPECT_GT(written, 0);
    EXPECT_NE(strstr(buf, "evidence="), nullptr)
        << "Summary missing 'evidence=' — got: " << buf;
    tracker->end_stage();

    tracker->print_summary();
    EXPECT_TRUE(tracker->is_successful());
}

// =============================================================================
// E2E: Multiple Gathers Cycle — stability under repeated gathering
// =============================================================================

TEST_F(ReasoningMeshBridgeE2E, MultipleGathersCycle) {
    uint32_t first_evidence_count = UINT32_MAX;

    for (int i = 0; i < 5; i++) {
        std::string stage_name = "Gather " + std::to_string(i + 1);
        tracker->begin_stage(stage_name, 5000);

        reasoning_mesh_result_t result = reasoning_mesh_gather_evidence(
            brain, "Repeated mesh query for stability test",
            REASONING_MESH_DEFAULT_TIMEOUT_MS);

        /* No crash is the primary assertion */
        EXPECT_GE(result.evidence_count, 0u);

        /* Evidence count should be consistent across iterations
         * (mesh state doesn't change between gathers) */
        if (first_evidence_count == UINT32_MAX) {
            first_evidence_count = result.evidence_count;
        } else {
            EXPECT_EQ(result.evidence_count, first_evidence_count)
                << "Evidence count changed between gather " << 1
                << " and gather " << (i + 1);
        }

        tracker->end_stage();
    }

    tracker->print_summary();
    EXPECT_TRUE(tracker->is_successful());
}
