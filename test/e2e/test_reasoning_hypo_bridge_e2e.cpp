/**
 * @file test_reasoning_hypo_bridge_e2e.cpp
 * @brief E2E tests: Hypothalamus-Reasoning bridge full pipeline
 *
 * WHAT: Full pipeline test exercising hypothalamus bridge from modulation
 *       computation through config application, reasoning engine execution,
 *       and training integration wrappers
 * WHY:  Verify that the hypothalamus bridge integrates correctly with the
 *       complete reasoning pipeline — modulation, config mutation, engine
 *       lifecycle, and TI wrappers all working together
 * HOW:  Creates brain, connects reasoning engine, computes modulation, applies
 *       it, runs reasoning with the modified config, and validates E2E behavior
 *
 * @author NIMCP Development Team
 * @date 2026-02-25
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
#include "cognitive/reasoning/nimcp_reasoning_hypo_bridge.h"
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

class ReasoningHypoBridgeE2E : public ::testing::Test {
protected:
    brain_t brain = nullptr;
    PipelineTracker* tracker = nullptr;

    void SetUp() override {
        tracker = new PipelineTracker("Hypo-Reasoning Bridge E2E");

        tracker->begin_stage("Brain Creation", 10000);
        brain = brain_create("e2e_hypo_bridge", BRAIN_SIZE_SMALL,
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
// E2E: Full Pipeline — Compute Modulation, Apply, Create Engine, Reason
// =============================================================================

TEST_F(ReasoningHypoBridgeE2E, FullPipelineModulationAndReason) {
    /* Stage 1: Compute hypo modulation */
    tracker->begin_stage("Compute Modulation", 5000);
    reasoning_hypo_modulation_t mod = reasoning_hypo_compute_modulation(brain);
    EXPECT_GE(mod.cognitive_capacity, 0.0f);
    EXPECT_LE(mod.cognitive_capacity, 1.0f);
    tracker->end_stage();

    /* Stage 2: Apply modulation to config */
    tracker->begin_stage("Apply Modulation", 1000);
    reasoning_engine_config_t config = reasoning_engine_default_config();
    int rc = reasoning_hypo_apply_modulation(&config, &mod);
    EXPECT_EQ(rc, 0);
    EXPECT_GT(config.max_steps, 0u);
    tracker->end_stage();

    /* Stage 3: Create reasoning engine with modified config */
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
    rc = reasoning_engine_reason(engine, "What is the current cognitive state?", &chain);
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

TEST_F(ReasoningHypoBridgeE2E, TrainingIntegrationAPIsEndToEnd) {
    tracker->begin_stage("TI Cognitive Capacity", 5000);
    float cap = brain_ti_get_cognitive_capacity(brain);
    EXPECT_GE(cap, 0.0f);
    EXPECT_LE(cap, 1.0f);
    tracker->end_stage();

    tracker->begin_stage("TI Urgency Mode", 5000);
    int urgency = brain_ti_get_urgency_mode(brain);
    EXPECT_GE(urgency, 0);
    EXPECT_LE(urgency, 3);
    tracker->end_stage();

    tracker->begin_stage("TI Stress Level", 5000);
    float stress = brain_ti_get_stress_level(brain);
    EXPECT_GE(stress, 0.0f);
    EXPECT_LE(stress, 1.0f);
    tracker->end_stage();

    tracker->print_summary();
    EXPECT_TRUE(tracker->is_successful());
}

// =============================================================================
// E2E: Modulation Summary end-to-end
// =============================================================================

TEST_F(ReasoningHypoBridgeE2E, ModulationSummaryEndToEnd) {
    tracker->begin_stage("Compute Modulation", 5000);
    reasoning_hypo_modulation_t mod = reasoning_hypo_compute_modulation(brain);
    tracker->end_stage();

    tracker->begin_stage("Format Summary", 1000);
    char buf[512];
    int written = reasoning_hypo_modulation_summary(&mod, buf, sizeof(buf));
    EXPECT_GT(written, 0);
    EXPECT_NE(strstr(buf, "capacity="), nullptr);
    tracker->end_stage();

    tracker->print_summary();
    EXPECT_TRUE(tracker->is_successful());
}

// =============================================================================
// E2E: Urgency Ladder Walk — test all 4 urgency levels
// =============================================================================

TEST_F(ReasoningHypoBridgeE2E, UrgencyLadderWalk) {
    static const struct {
        reasoning_urgency_mode_t mode;
        const char* name;
        float stress;
        float alertness;
        uint32_t max_steps;
        bool sequential;
    } urgency_levels[] = {
        { REASONING_URGENCY_RELAXED,          "RELAXED",          0.0f, 1.0f, 0,  false },
        { REASONING_URGENCY_NORMAL,           "NORMAL",           0.1f, 0.9f, 0,  false },
        { REASONING_URGENCY_ALERT,            "ALERT",            0.4f, 0.8f, 30, false },
        { REASONING_URGENCY_FIGHT_OR_FLIGHT,  "FIGHT_OR_FLIGHT",  0.9f, 1.0f, 10, true  },
    };

    for (size_t i = 0; i < sizeof(urgency_levels) / sizeof(urgency_levels[0]); i++) {
        std::string stage_name = std::string("Urgency: ") + urgency_levels[i].name;
        tracker->begin_stage(stage_name, 5000);

        /* Construct modulation with this urgency level */
        reasoning_hypo_modulation_t mod = reasoning_hypo_neutral_modulation();
        mod.urgency_mode = urgency_levels[i].mode;
        mod.stress_level = urgency_levels[i].stress;
        mod.alertness = urgency_levels[i].alertness;
        mod.recommended_max_steps = urgency_levels[i].max_steps;
        mod.force_sequential = urgency_levels[i].sequential;
        mod.hypothalamus_available = true;

        /* Format summary and verify it contains the urgency name */
        char buf[512];
        int written = reasoning_hypo_modulation_summary(&mod, buf, sizeof(buf));
        EXPECT_GT(written, 0) << "Failed for urgency " << urgency_levels[i].name;
        EXPECT_NE(strstr(buf, urgency_levels[i].name), nullptr)
            << "Summary missing urgency name '" << urgency_levels[i].name
            << "' — got: " << buf;

        tracker->end_stage();
    }

    tracker->print_summary();
    EXPECT_TRUE(tracker->is_successful());
}
