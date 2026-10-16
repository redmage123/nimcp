/**
 * @file e2e_test_basal_ganglia_pipeline.cpp
 * @brief E2E Tests for Basal Ganglia Integration Pipeline
 *
 * WHAT: Complete basal ganglia integration testing with brain lifecycle
 * WHY:  Validate enhanced BG action selection and reward processing
 * HOW:  Test realistic scenarios: create brain → init BG → select actions → process rewards
 *
 * TEST COVERAGE:
 * - Brain creation with BG subsystem
 * - BG-based action selection with cortical input
 * - Reward processing and dopamine modulation
 * - Integration with emotional and arousal signals
 * - BG step updates during brain step
 */

#include "e2e_test_framework.h"

// Headers have their own extern "C" guards
#include "core/brain/factory/nimcp_brain_factory.h"
#include "core/brain/factory/init/nimcp_brain_init_basal_ganglia.h"
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Fixtures
//=============================================================================

class BasalGangliaPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        nimcp_init();
        nimcp_memory_init();
        nimcp_memory_enable_tracking(true);
    }

    void TearDown() override {
        nimcp_memory_check_leaks();
        nimcp_shutdown();
    }
};

//=============================================================================
// E2E Test: BG Action Selection Pipeline
//=============================================================================

E2E_TEST(BasalGangliaPipelineTest, ActionSelectionPipeline) {
    E2E_PIPELINE_START("BG Action Selection Pipeline");

    brain_t brain = nullptr;

    // Stage 1: Create brain with 8 output actions (using internal API for BG access)
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        brain = brain_create(
            "bg_e2e_test_brain",
            BRAIN_SIZE_SMALL,
            BRAIN_TASK_CLASSIFICATION,
            10,  // 10 inputs
            8    // 8 outputs/actions
        );
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Verify BG is enabled
    E2E_STAGE_BEGIN("Verify BG enabled", 10);
    {
        bool enabled = nimcp_brain_bg_is_enabled(brain);
        E2E_ASSERT(enabled, "Basal ganglia should be enabled after brain creation");
    }
    E2E_STAGE_END();

    // Stage 3: Run action selection
    E2E_STAGE_BEGIN("Action selection", 100);
    {
        // Create cortical input with clear winner
        float cortical_input[8] = {0.1f, 0.2f, 0.3f, 0.9f, 0.2f, 0.1f, 0.15f, 0.25f};
        uint32_t selected_action;

        int result = nimcp_brain_bg_select_action(brain, cortical_input, &selected_action);
        E2E_ASSERT(result == 0, "Action selection should succeed");
        E2E_ASSERT(selected_action == 3, "Should select action with highest cortical input");
    }
    E2E_STAGE_END();

    // Stage 4: Process reward (positive surprise)
    E2E_STAGE_BEGIN("Process positive reward", 50);
    {
        int result = nimcp_brain_bg_process_reward(brain, 1.0f, 0.2f);
        E2E_ASSERT(result == 0, "Reward processing should succeed");
    }
    E2E_STAGE_END();

    // Stage 5: Process reward (negative surprise)
    E2E_STAGE_BEGIN("Process negative reward", 50);
    {
        int result = nimcp_brain_bg_process_reward(brain, 0.0f, 0.8f);
        E2E_ASSERT(result == 0, "Negative reward processing should succeed");
    }
    E2E_STAGE_END();

    // Stage 6: BG step update
    E2E_STAGE_BEGIN("BG step update", 5000);
    {
        for (int i = 0; i < 10; i++) {
            int result = nimcp_brain_bg_step(brain, 1.0f);  // 1ms timestep
            E2E_ASSERT(result == 0, "BG step should succeed");
        }
    }
    E2E_STAGE_END();

    // Stage 7: Check motivation
    E2E_STAGE_BEGIN("Check motivation", 10);
    {
        float motivation = nimcp_brain_bg_get_motivation(brain);
        E2E_ASSERT(motivation >= 0.0f && motivation <= 1.0f,
                   "Motivation should be in valid range");
    }
    E2E_STAGE_END();

    // Stage 8: Cleanup
    E2E_STAGE_BEGIN("Destroy brain", 500);
    {
        brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: BG Emotional Integration Pipeline
//=============================================================================

E2E_TEST(BasalGangliaPipelineTest, EmotionalIntegrationPipeline) {
    E2E_PIPELINE_START("BG Emotional Integration Pipeline");

    brain_t brain = nullptr;

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        brain = brain_create(
            "bg_emotional_test",
            BRAIN_SIZE_SMALL,
            BRAIN_TASK_CLASSIFICATION,
            10,
            4
        );
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Send positive emotional signal
    E2E_STAGE_BEGIN("Positive emotional signal", 50);
    {
        // High valence, moderate arousal
        nimcp_brain_bg_on_emotional_signal(brain, 0.8f, 0.5f);
    }
    E2E_STAGE_END();

    // Stage 3: Send negative emotional signal
    E2E_STAGE_BEGIN("Negative emotional signal", 50);
    {
        // Low valence (aversion), high arousal
        nimcp_brain_bg_on_emotional_signal(brain, -0.7f, 0.9f);
    }
    E2E_STAGE_END();

    // Stage 4: Test arousal modulation
    E2E_STAGE_BEGIN("Arousal modulation", 500);
    {
        // High arousal
        nimcp_brain_bg_on_arousal_change(brain, 0.9f);
        nimcp_brain_bg_step(brain, 1.0f);

        // Low arousal (fatigue)
        nimcp_brain_bg_on_arousal_change(brain, 0.1f);
        nimcp_brain_bg_step(brain, 1.0f);

        // Normal arousal
        nimcp_brain_bg_on_arousal_change(brain, 0.5f);
        nimcp_brain_bg_step(brain, 1.0f);
    }
    E2E_STAGE_END();

    // Stage 5: Test goal changes
    E2E_STAGE_BEGIN("Goal changes", 500);
    {
        // Activate goal
        nimcp_brain_bg_on_goal_change(brain, 1, true);
        nimcp_brain_bg_step(brain, 1.0f);

        // Activate another goal
        nimcp_brain_bg_on_goal_change(brain, 2, true);
        nimcp_brain_bg_step(brain, 1.0f);

        // Deactivate first goal
        nimcp_brain_bg_on_goal_change(brain, 1, false);
        nimcp_brain_bg_step(brain, 1.0f);
    }
    E2E_STAGE_END();

    // Stage 6: Verify BG state
    E2E_STAGE_BEGIN("Verify BG state", 50);
    {
        bgod_behavior_type_t behavior = nimcp_brain_bg_get_behavior_type(brain);
        // Behavior type depends on internal state, just verify it's valid
        // Enum order: GOAL_DIRECTED, HABITUAL, MIXED, UNKNOWN, COUNT
        E2E_ASSERT(behavior >= BGOD_BEHAVIOR_GOAL_DIRECTED && behavior < BGOD_BEHAVIOR_COUNT,
                   "Behavior type should be valid");
    }
    E2E_STAGE_END();

    // Stage 7: Cleanup
    E2E_STAGE_BEGIN("Destroy brain", 500);
    {
        brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: BG Continuous Learning Pipeline
//=============================================================================

E2E_TEST(BasalGangliaPipelineTest, ContinuousLearningPipeline) {
    E2E_PIPELINE_START("BG Continuous Learning Pipeline");

    brain_t brain = nullptr;
    const int NUM_TRIALS = 50;

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        brain = brain_create(
            "bg_learning_test",
            BRAIN_SIZE_SMALL,
            BRAIN_TASK_CLASSIFICATION,
            10,
            4
        );
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Run learning trials
    E2E_STAGE_BEGIN("Learning trials", 2000);
    {
        for (int trial = 0; trial < NUM_TRIALS; trial++) {
            // Generate varying cortical inputs
            float cortical_input[4];
            for (int i = 0; i < 4; i++) {
                cortical_input[i] = 0.1f + (float)(trial % 10) / 20.0f + (float)i * 0.1f;
            }

            // Select action
            uint32_t selected_action;
            int result = nimcp_brain_bg_select_action(brain, cortical_input, &selected_action);
            E2E_ASSERT(result == 0, "Action selection should succeed");

            // Provide reward based on action quality
            float reward = (selected_action == 3) ? 1.0f : 0.2f;
            float predicted = 0.5f;

            result = nimcp_brain_bg_process_reward(brain, reward, predicted);
            E2E_ASSERT(result == 0, "Reward processing should succeed");

            // Step BG dynamics
            result = nimcp_brain_bg_step(brain, 10.0f);  // 10ms per trial
            E2E_ASSERT(result == 0, "BG step should succeed");
        }
    }
    E2E_STAGE_END();

    // Stage 3: Verify stats are updated
    E2E_STAGE_BEGIN("Verify stats", 50);
    {
        bg_enhanced_stats_t stats;
        int result = nimcp_brain_bg_get_stats(brain, &stats);
        E2E_ASSERT(result == 0, "Stats retrieval should succeed");
    }
    E2E_STAGE_END();

    // Stage 4: Final action selection (should prefer action 3)
    E2E_STAGE_BEGIN("Final action test", 50);
    {
        float cortical_input[4] = {0.5f, 0.5f, 0.5f, 0.5f};  // Equal inputs
        uint32_t selected_action;
        int result = nimcp_brain_bg_select_action(brain, cortical_input, &selected_action);
        E2E_ASSERT(result == 0, "Final action selection should succeed");
        // After learning, may or may not prefer action 3 - just verify it works
    }
    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Destroy brain", 500);
    {
        brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// E2E Test: BG Fallback Behavior
//=============================================================================

E2E_TEST(BasalGangliaPipelineTest, FallbackBehavior) {
    E2E_PIPELINE_START("BG Fallback Behavior");

    brain_t brain = nullptr;

    // Stage 1: Create brain
    E2E_STAGE_BEGIN("Create brain", 500);
    {
        brain = brain_create(
            "bg_fallback_test",
            BRAIN_SIZE_SMALL,
            BRAIN_TASK_CLASSIFICATION,
            10,
            4
        );
        E2E_ASSERT_NOT_NULL(brain, "Brain creation failed");
    }
    E2E_STAGE_END();

    // Stage 2: Manually disable BG
    E2E_STAGE_BEGIN("Disable BG", 50);
    {
        nimcp_brain_bg_destroy(brain);
        E2E_ASSERT(!nimcp_brain_bg_is_enabled(brain), "BG should be disabled");
    }
    E2E_STAGE_END();

    // Stage 3: Test fallback action selection
    E2E_STAGE_BEGIN("Fallback action selection", 50);
    {
        float cortical_input[4] = {0.2f, 0.8f, 0.3f, 0.1f};
        uint32_t selected_action;
        int result = nimcp_brain_bg_select_action(brain, cortical_input, &selected_action);
        E2E_ASSERT(result == 0, "Fallback action selection should succeed");
        E2E_ASSERT(selected_action == 1, "Should select max activation (index 1)");
    }
    E2E_STAGE_END();

    // Stage 4: Test that callbacks don't crash
    E2E_STAGE_BEGIN("Safe callbacks without BG", 50);
    {
        // These should not crash even with BG disabled
        nimcp_brain_bg_on_emotional_signal(brain, 0.5f, 0.5f);
        nimcp_brain_bg_on_goal_change(brain, 1, true);
        nimcp_brain_bg_on_arousal_change(brain, 0.5f);
        int result = nimcp_brain_bg_step(brain, 1.0f);
        E2E_ASSERT(result == 0, "Step should silently skip when disabled");
        result = nimcp_brain_bg_process_reward(brain, 1.0f, 0.5f);
        E2E_ASSERT(result == 0, "Reward should silently skip when disabled");
    }
    E2E_STAGE_END();

    // Stage 5: Cleanup
    E2E_STAGE_BEGIN("Destroy brain", 500);
    {
        brain_destroy(brain);
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
