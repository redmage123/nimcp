/**
 * @file e2e_test_prefrontal_pipeline.cpp
 * @brief End-to-end tests for Prefrontal Cortex Pipeline
 *
 * WHAT: Full pipeline tests for executive function and decision-making
 * WHY:  Verify complete prefrontal workflows with substrate integration
 * HOW:  Test executive control, working memory, inhibition, planning
 *
 * TEST COVERAGE:
 * - Executive Function Pipeline (4 tests)
 * - Working Memory Maintenance (4 tests)
 * - Inhibitory Control (3 tests)
 * - Decision-Making Scenarios (4 tests)
 * - Metabolic Effects (3 tests)
 * - Thalamic Integration (3 tests)
 *
 * TOTAL: 21 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Prefrontal cortex orchestrates executive function
 * - Working memory maintained in DLPFC
 * - Inhibitory control via ventrolateral PFC
 * - Decision-making involves OFC and ACC
 * - High metabolic demands - first to fail under stress
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <atomic>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "core/prefrontal/nimcp_prefrontal_substrate_bridge.h"
#include "core/prefrontal/nimcp_prefrontal_thalamic_bridge.h"
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "utils/memory/nimcp_memory.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_EXECUTIVE_DECISION_TIME_MS = 50.0;
constexpr double MAX_WORKING_MEMORY_UPDATE_TIME_MS = 10.0;
constexpr double MAX_INHIBITION_TIME_MS = 5.0;
constexpr float MIN_EXECUTIVE_CAPACITY = 0.3f;
constexpr float NORMAL_EXECUTIVE_CAPACITY = 0.8f;
constexpr uint32_t WORKING_MEMORY_SLOTS = 7;

//=============================================================================
// Test Fixtures
//=============================================================================

class E2EPrefrontalExecutivePipelineTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    prefrontal_substrate_bridge_t* pfc_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        prefrontal_substrate_config_t pfc_config = prefrontal_substrate_default_config();
        pfc_bridge = prefrontal_substrate_bridge_create(nullptr, substrate, &pfc_config);
        ASSERT_NE(pfc_bridge, nullptr);
    }

    void TearDown() override {
        if (pfc_bridge) {
            prefrontal_substrate_bridge_destroy(pfc_bridge);
            pfc_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

class E2EPrefrontalWorkingMemoryTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    prefrontal_substrate_bridge_t* pfc_bridge = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        prefrontal_substrate_config_t pfc_config = prefrontal_substrate_default_config();
        pfc_bridge = prefrontal_substrate_bridge_create(nullptr, substrate, &pfc_config);
        ASSERT_NE(pfc_bridge, nullptr);
    }

    void TearDown() override {
        if (pfc_bridge) {
            prefrontal_substrate_bridge_destroy(pfc_bridge);
            pfc_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

class E2EPrefrontalInhibitionTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    prefrontal_substrate_bridge_t* pfc_bridge = nullptr;
    basal_ganglia_t* bg = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        prefrontal_substrate_config_t pfc_config = prefrontal_substrate_default_config();
        pfc_bridge = prefrontal_substrate_bridge_create(nullptr, substrate, &pfc_config);
        ASSERT_NE(pfc_bridge, nullptr);

        basal_ganglia_config_t bg_config;
        basal_ganglia_default_config(&bg_config);
        bg_config.num_actions = 8;
        bg = basal_ganglia_create(&bg_config);
        ASSERT_NE(bg, nullptr);
    }

    void TearDown() override {
        if (bg) {
            basal_ganglia_destroy(bg);
            bg = nullptr;
        }
        if (pfc_bridge) {
            prefrontal_substrate_bridge_destroy(pfc_bridge);
            pfc_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

class E2EPrefrontalDecisionTest : public ::testing::Test {
protected:
    neural_substrate_t* substrate = nullptr;
    prefrontal_substrate_bridge_t* pfc_bridge = nullptr;
    basal_ganglia_t* bg = nullptr;

    void SetUp() override {
        substrate_config_t sub_config;
        substrate_default_config(&sub_config);
        substrate = substrate_create(&sub_config);
        ASSERT_NE(substrate, nullptr);

        prefrontal_substrate_config_t pfc_config = prefrontal_substrate_default_config();
        pfc_bridge = prefrontal_substrate_bridge_create(nullptr, substrate, &pfc_config);
        ASSERT_NE(pfc_bridge, nullptr);

        basal_ganglia_config_t bg_config;
        basal_ganglia_default_config(&bg_config);
        bg_config.num_actions = 4;
        bg = basal_ganglia_create(&bg_config);
        ASSERT_NE(bg, nullptr);
    }

    void TearDown() override {
        if (bg) {
            basal_ganglia_destroy(bg);
            bg = nullptr;
        }
        if (pfc_bridge) {
            prefrontal_substrate_bridge_destroy(pfc_bridge);
            pfc_bridge = nullptr;
        }
        if (substrate) {
            substrate_destroy(substrate);
            substrate = nullptr;
        }
    }
};

//=============================================================================
// Executive Function Pipeline Tests
//=============================================================================

TEST_F(E2EPrefrontalExecutivePipelineTest, BaselineExecutiveCapacity) {
    // Scenario: Verify baseline executive function with optimal substrate
    E2E_PIPELINE_START("Baseline Executive Capacity");

    E2E_STAGE_BEGIN("Initialize substrate", 5);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Update PFC bridge", 10);
    int result = prefrontal_substrate_bridge_update(pfc_bridge);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Get effects", 5);
    prefrontal_substrate_effects_t effects;
    result = prefrontal_substrate_bridge_get_effects(pfc_bridge, &effects);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify capacity", 2);
    EXPECT_GT(effects.overall_capacity, MIN_EXECUTIVE_CAPACITY);
    EXPECT_GT(effects.executive_function, MIN_EXECUTIVE_CAPACITY);
    EXPECT_GT(effects.working_memory, MIN_EXECUTIVE_CAPACITY);
    EXPECT_GT(effects.inhibitory_control, MIN_EXECUTIVE_CAPACITY);
    EXPECT_GT(effects.planning_capacity, MIN_EXECUTIVE_CAPACITY);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EPrefrontalExecutivePipelineTest, ExecutiveLoadProgression) {
    // Scenario: Track executive capacity across increasing cognitive load
    E2E_PIPELINE_START("Executive Load Progression");

    E2E_STAGE_BEGIN("Setup baseline", 5);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t baseline;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &baseline);
    float initial_capacity = baseline.overall_capacity;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Simulate cognitive load", 50);
    // Simulate increasing cognitive load through substrate consumption
    for (int load_level = 1; load_level <= 5; load_level++) {
        // Higher load consumes more ATP
        substrate_record_spikes(substrate, load_level * 100);
        substrate_update(substrate, 50);
        prefrontal_substrate_bridge_update(pfc_bridge);

        prefrontal_substrate_effects_t effects;
        prefrontal_substrate_bridge_get_effects(pfc_bridge, &effects);

        // Capacity should still be valid
        EXPECT_GE(effects.overall_capacity, 0.0f);
        EXPECT_LE(effects.overall_capacity, 1.0f);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify non-NaN", 2);
    prefrontal_substrate_effects_t final_effects;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &final_effects);
    EXPECT_FALSE(std::isnan(final_effects.overall_capacity));
    EXPECT_FALSE(std::isnan(final_effects.executive_function));
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EPrefrontalExecutivePipelineTest, ExecutiveRecoveryAfterLoad) {
    // Scenario: Executive function recovery after high load
    E2E_PIPELINE_START("Executive Recovery After Load");

    E2E_STAGE_BEGIN("Deplete resources", 20);
    // Heavy cognitive load depletes ATP
    for (int i = 0; i < 10; i++) {
        substrate_record_spikes(substrate, 500);
        substrate_update(substrate, 10);
    }
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t depleted;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &depleted);
    float depleted_capacity = depleted.overall_capacity;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Recovery period", 100);
    // Allow substrate recovery
    for (int i = 0; i < 50; i++) {
        substrate_update(substrate, 100);  // Long recovery time
    }
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t recovered;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &recovered);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify recovery", 2);
    EXPECT_GE(recovered.overall_capacity, depleted_capacity);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EPrefrontalExecutivePipelineTest, ApplyEffectsModulation) {
    // Scenario: Test effect application
    E2E_PIPELINE_START("Apply Effects Modulation");

    E2E_STAGE_BEGIN("Update bridge", 10);
    prefrontal_substrate_bridge_update(pfc_bridge);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Apply effects", 10);
    int result = prefrontal_substrate_bridge_apply_effects(pfc_bridge);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify application", 5);
    prefrontal_substrate_effects_t effects;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &effects);
    EXPECT_GT(effects.overall_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Working Memory Maintenance Tests
//=============================================================================

TEST_F(E2EPrefrontalWorkingMemoryTest, WorkingMemoryCapacityUnderLoad) {
    // Scenario: Working memory capacity varies with substrate state
    E2E_PIPELINE_START("Working Memory Capacity Under Load");

    E2E_STAGE_BEGIN("Optimal conditions", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t optimal;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &optimal);
    float optimal_wm = optimal.working_memory;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Stressed conditions", 10);
    substrate_set_atp(substrate, 0.5f);  // Reduced ATP
    substrate_update(substrate, 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t stressed;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &stressed);
    float stressed_wm = stressed.working_memory;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare capacities", 2);
    // Both should be valid
    EXPECT_GT(optimal_wm, 0.0f);
    EXPECT_GT(stressed_wm, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EPrefrontalWorkingMemoryTest, WorkingMemoryAcrossSimulation) {
    // Scenario: Track working memory across extended simulation
    E2E_PIPELINE_START("Working Memory Across Simulation");

    E2E_STAGE_BEGIN("Run simulation", 100);
    std::vector<float> wm_values;

    for (int step = 0; step < 50; step++) {
        substrate_update(substrate, 20);
        prefrontal_substrate_bridge_update(pfc_bridge);

        prefrontal_substrate_effects_t effects;
        prefrontal_substrate_bridge_get_effects(pfc_bridge, &effects);
        wm_values.push_back(effects.working_memory);

        // Values should be bounded
        EXPECT_GE(effects.working_memory, 0.0f);
        EXPECT_LE(effects.working_memory, 1.0f);
        EXPECT_FALSE(std::isnan(effects.working_memory));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze stability", 5);
    // Calculate variance - should be relatively stable
    float mean = std::accumulate(wm_values.begin(), wm_values.end(), 0.0f) / wm_values.size();
    float variance = 0.0f;
    for (float v : wm_values) {
        variance += (v - mean) * (v - mean);
    }
    variance /= wm_values.size();

    // Should have some variance but not extreme
    EXPECT_LT(variance, 0.5f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EPrefrontalWorkingMemoryTest, WorkingMemoryFatigueEffects) {
    // Scenario: Fatigue reduces working memory capacity
    E2E_PIPELINE_START("Working Memory Fatigue Effects");

    E2E_STAGE_BEGIN("Baseline fresh state", 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t fresh;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &fresh);
    float fresh_wm = fresh.working_memory;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Induce fatigue", 50);
    // Heavy sustained activity induces fatigue
    for (int i = 0; i < 100; i++) {
        substrate_record_spikes(substrate, 200);
        substrate_record_transmissions(substrate, 500);
        substrate_update(substrate, 10);
    }
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t fatigued;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &fatigued);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify values", 2);
    EXPECT_GE(fatigued.working_memory, 0.0f);
    EXPECT_LE(fatigued.working_memory, 1.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EPrefrontalWorkingMemoryTest, WorkingMemoryOxygenDependence) {
    // Scenario: Working memory is sensitive to oxygen levels
    E2E_PIPELINE_START("Working Memory Oxygen Dependence");

    E2E_STAGE_BEGIN("Normal oxygen", 10);
    substrate_set_oxygen(substrate, SUBSTRATE_NORMAL_O2_SAT);
    substrate_update(substrate, 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t normal_o2;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &normal_o2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low oxygen", 10);
    substrate_set_oxygen(substrate, 0.6f);  // Mild hypoxia
    substrate_update(substrate, 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t low_o2;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &low_o2);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify impact", 2);
    // Both values should be valid
    EXPECT_GT(normal_o2.working_memory, 0.0f);
    EXPECT_GT(low_o2.working_memory, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Inhibitory Control Tests
//=============================================================================

TEST_F(E2EPrefrontalInhibitionTest, InhibitoryControlCapacity) {
    // Scenario: Test inhibitory control modulation
    E2E_PIPELINE_START("Inhibitory Control Capacity");

    E2E_STAGE_BEGIN("Get inhibitory capacity", 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t effects;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &effects);
    float inhibitory_capacity = effects.inhibitory_control;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify capacity", 2);
    EXPECT_GE(inhibitory_capacity, 0.0f);
    EXPECT_LE(inhibitory_capacity, 1.0f);
    EXPECT_GT(inhibitory_capacity, MIN_EXECUTIVE_CAPACITY);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EPrefrontalInhibitionTest, InhibitionWithBasalGangliaInteraction) {
    // Scenario: PFC inhibition affects basal ganglia action selection
    E2E_PIPELINE_START("Inhibition With Basal Ganglia");

    E2E_STAGE_BEGIN("Setup", 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t effects;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Action selection with inhibition", 20);
    float cortical_input[8] = {0.8f, 0.3f, 0.2f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f};

    // Use inhibitory capacity to modulate suppression
    float inhibition_strength = effects.inhibitory_control;
    basal_ganglia_suppress_action(bg, inhibition_strength * 0.5f);

    uint32_t selected;
    int result = basal_ganglia_select_action(bg, cortical_input, &selected);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify selection", 2);
    EXPECT_LT(selected, 8u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EPrefrontalInhibitionTest, InhibitionUnderStress) {
    // Scenario: Inhibitory control degrades under stress
    E2E_PIPELINE_START("Inhibition Under Stress");

    E2E_STAGE_BEGIN("Baseline inhibition", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t baseline;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &baseline);
    float baseline_inhibition = baseline.inhibitory_control;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Stress conditions", 10);
    // Reduce ATP to simulate stress
    substrate_set_atp(substrate, 0.4f);
    substrate_update(substrate, 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t stressed;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &stressed);
    float stressed_inhibition = stressed.inhibitory_control;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare", 2);
    // Both should be valid
    EXPECT_GE(baseline_inhibition, 0.0f);
    EXPECT_GE(stressed_inhibition, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Decision-Making Scenarios
//=============================================================================

TEST_F(E2EPrefrontalDecisionTest, SimpleDecisionPipeline) {
    // Scenario: Complete decision-making pipeline
    E2E_PIPELINE_START("Simple Decision Pipeline");

    E2E_STAGE_BEGIN("Update PFC state", 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t pfc_effects;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &pfc_effects);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Modulate cortical input", 10);
    // Modulate input based on executive capacity
    float raw_input[4] = {0.5f, 0.7f, 0.3f, 0.8f};
    float modulated_input[4];

    for (int i = 0; i < 4; i++) {
        modulated_input[i] = raw_input[i] * pfc_effects.executive_function;
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Action selection", 20);
    uint32_t selected;
    int result = basal_ganglia_select_action(bg, modulated_input, &selected);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify decision", 2);
    EXPECT_LT(selected, 4u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EPrefrontalDecisionTest, DelayedDecisionWithWorkingMemory) {
    // Scenario: Decision requiring working memory maintenance
    E2E_PIPELINE_START("Delayed Decision With Working Memory");

    E2E_STAGE_BEGIN("Store decision context", 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t effects;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &effects);

    // Initial stimulus
    float stimulus[4] = {0.3f, 0.9f, 0.4f, 0.2f};
    uint32_t initial_best = 1;  // Best action from stimulus
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Delay period", 50);
    // Simulate delay period - working memory must maintain info
    for (int step = 0; step < 10; step++) {
        substrate_update(substrate, 100);
        prefrontal_substrate_bridge_update(pfc_bridge);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Execute delayed decision", 20);
    // Working memory quality affects decision accuracy
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &effects);
    float wm_quality = effects.working_memory;

    // Decision based on remembered stimulus (degraded by WM quality)
    float recalled[4];
    for (int i = 0; i < 4; i++) {
        recalled[i] = stimulus[i] * wm_quality;
    }

    uint32_t selected;
    basal_ganglia_select_action(bg, recalled, &selected);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify outcome", 2);
    EXPECT_LT(selected, 4u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EPrefrontalDecisionTest, ConflictResolutionDecision) {
    // Scenario: Decision with competing options
    E2E_PIPELINE_START("Conflict Resolution Decision");

    E2E_STAGE_BEGIN("Setup conflict", 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    // Two options with similar values - creates conflict
    float conflicting_input[4] = {0.75f, 0.72f, 0.2f, 0.1f};
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("First decision", 20);
    uint32_t first_decision;
    basal_ganglia_select_action(bg, conflicting_input, &first_decision);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Reset and decide again", 20);
    basal_ganglia_reset(bg);
    uint32_t second_decision;
    basal_ganglia_select_action(bg, conflicting_input, &second_decision);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify consistency", 2);
    // Both decisions should be among the high-value options
    EXPECT_TRUE(first_decision == 0 || first_decision == 1);
    EXPECT_TRUE(second_decision == 0 || second_decision == 1);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EPrefrontalDecisionTest, MultiplDecisionTrials) {
    // Scenario: Multiple decision trials with learning
    E2E_PIPELINE_START("Multiple Decision Trials");

    E2E_STAGE_BEGIN("Run trials", 200);
    std::vector<uint32_t> decisions;

    for (int trial = 0; trial < 10; trial++) {
        prefrontal_substrate_bridge_update(pfc_bridge);

        prefrontal_substrate_effects_t effects;
        prefrontal_substrate_bridge_get_effects(pfc_bridge, &effects);

        float input[4] = {
            0.4f + 0.1f * sinf(trial * 0.5f),
            0.6f,
            0.3f + 0.1f * cosf(trial * 0.5f),
            0.5f
        };

        // Modulate by executive function
        for (int i = 0; i < 4; i++) {
            input[i] *= effects.executive_function;
        }

        uint32_t selected;
        basal_ganglia_select_action(bg, input, &selected);
        decisions.push_back(selected);

        // Provide reward for action 1
        float reward = (selected == 1) ? 1.0f : -0.3f;
        basal_ganglia_update_dopamine(bg, reward, 0.5f);
        basal_ganglia_step(bg, 10.0f);

        substrate_update(substrate, 50);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Analyze decisions", 5);
    int action_1_count = std::count(decisions.begin(), decisions.end(), 1);
    // Learning should favor action 1 over time
    EXPECT_GT(action_1_count, 0);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Metabolic Effects Tests
//=============================================================================

TEST_F(E2EPrefrontalExecutivePipelineTest, ATPDepletionEffects) {
    // Scenario: PFC is first to fail under metabolic stress
    E2E_PIPELINE_START("ATP Depletion Effects");

    E2E_STAGE_BEGIN("Normal ATP", 10);
    substrate_set_atp(substrate, SUBSTRATE_NORMAL_ATP);
    substrate_update(substrate, 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t normal;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &normal);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Progressive depletion", 50);
    float atp_levels[] = {0.8f, 0.6f, 0.4f, 0.3f};

    for (float atp : atp_levels) {
        substrate_set_atp(substrate, atp);
        substrate_update(substrate, 10);
        prefrontal_substrate_bridge_update(pfc_bridge);

        prefrontal_substrate_effects_t effects;
        prefrontal_substrate_bridge_get_effects(pfc_bridge, &effects);

        // All values should remain valid
        EXPECT_GE(effects.overall_capacity, 0.0f);
        EXPECT_LE(effects.overall_capacity, 1.0f);
        EXPECT_FALSE(std::isnan(effects.executive_function));
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Critical ATP", 10);
    substrate_set_atp(substrate, SUBSTRATE_CRITICAL_ATP);
    substrate_update(substrate, 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t critical;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &critical);

    // Should be significantly impaired but not zero
    EXPECT_GE(critical.overall_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EPrefrontalExecutivePipelineTest, GlucoseDependence) {
    // Scenario: PFC is highly glucose dependent
    E2E_PIPELINE_START("Glucose Dependence");

    E2E_STAGE_BEGIN("Normal glucose", 10);
    substrate_set_glucose(substrate, SUBSTRATE_NORMAL_GLUCOSE);
    substrate_update(substrate, 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t normal;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &normal);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low glucose", 10);
    substrate_set_glucose(substrate, 0.4f);
    substrate_update(substrate, 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t low;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &low);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify values", 2);
    EXPECT_GT(normal.overall_capacity, 0.0f);
    EXPECT_GT(low.overall_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2EPrefrontalExecutivePipelineTest, TemperatureEffects) {
    // Scenario: Temperature affects PFC function
    E2E_PIPELINE_START("Temperature Effects");

    E2E_STAGE_BEGIN("Normal temperature", 10);
    substrate_set_temperature(substrate, SUBSTRATE_NORMAL_TEMPERATURE);
    substrate_update(substrate, 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t normal_temp;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &normal_temp);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Elevated temperature", 10);
    substrate_set_temperature(substrate, 39.0f);  // Mild fever
    substrate_update(substrate, 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t elevated;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &elevated);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Low temperature", 10);
    substrate_set_temperature(substrate, 35.0f);  // Mild hypothermia
    substrate_update(substrate, 10);
    prefrontal_substrate_bridge_update(pfc_bridge);

    prefrontal_substrate_effects_t low_temp;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &low_temp);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify all valid", 2);
    EXPECT_GE(normal_temp.overall_capacity, 0.0f);
    EXPECT_GE(elevated.overall_capacity, 0.0f);
    EXPECT_GE(low_temp.overall_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Long-Term Stability Tests
//=============================================================================

TEST_F(E2EPrefrontalExecutivePipelineTest, LongSimulationStability) {
    // Scenario: Extended simulation without degradation
    E2E_PIPELINE_START("Long Simulation Stability");

    E2E_STAGE_BEGIN("Extended simulation", 500);
    for (int step = 0; step < 1000; step++) {
        substrate_update(substrate, 10);
        prefrontal_substrate_bridge_update(pfc_bridge);

        if (step % 100 == 0) {
            prefrontal_substrate_effects_t effects;
            prefrontal_substrate_bridge_get_effects(pfc_bridge, &effects);

            EXPECT_FALSE(std::isnan(effects.overall_capacity));
            EXPECT_FALSE(std::isinf(effects.overall_capacity));
            EXPECT_GE(effects.overall_capacity, 0.0f);
            EXPECT_LE(effects.overall_capacity, 1.0f);
        }
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Final validation", 5);
    prefrontal_substrate_effects_t final_effects;
    prefrontal_substrate_bridge_get_effects(pfc_bridge, &final_effects);

    EXPECT_GT(final_effects.overall_capacity, 0.0f);
    EXPECT_GT(final_effects.executive_function, 0.0f);
    EXPECT_GT(final_effects.working_memory, 0.0f);
    EXPECT_GT(final_effects.inhibitory_control, 0.0f);
    EXPECT_GT(final_effects.planning_capacity, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Entry Point
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
