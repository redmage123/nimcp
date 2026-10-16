/**
 * @file test_imagination_engine.cpp
 * @brief Unit tests for Imagination Engine
 *
 * WHAT: Comprehensive unit tests for imagination module
 * WHY:  Ensure imagination engine lifecycle, scenarios, and modes work correctly
 * HOW:  Test with GoogleTest framework
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

// Headers have their own extern "C" guards
#include "utils/nimcp_test_base.h"

// Include imagination headers after test base to avoid typedef conflicts
#include "cognitive/imagination/nimcp_imagination_engine.h"
#include "cognitive/imagination/nimcp_imagination_workspace.h"

/* ============================================================================
 * Imagination Engine Core Tests
 * ============================================================================ */

class ImaginationEngineTest : public NimcpTestBase {
protected:
    imagination_engine_t* engine = nullptr;
    imagination_workspace_t* workspace = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        if (engine) {
            imagination_engine_destroy(engine);
            engine = nullptr;
        }
        if (workspace) {
            imagination_workspace_destroy(workspace);
            workspace = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

/* ============================================================================
 * Configuration Tests
 * ============================================================================ */

TEST_F(ImaginationEngineTest, DefaultConfigHasSensibleValues) {
    // WHAT: Verify default configuration has reasonable values
    // WHY:  Ensure users get working system without manual config
    // HOW:  Call default_config, check all values

    imagination_engine_config_t config = imagination_engine_default_config();

    // Capacity should be positive
    EXPECT_GT(config.max_concurrent_scenarios, 0u);
    EXPECT_GT(config.workspace_capacity, 0u);
    EXPECT_GT(config.latent_dim, 0u);
    EXPECT_LE(config.latent_dim, IMAGINATION_MAX_LATENT_DIM);

    // Vividness and noise levels should be in range [0, 1]
    EXPECT_GE(config.default_vividness, 0.0f);
    EXPECT_LE(config.default_vividness, 1.0f);
    EXPECT_GE(config.creativity_noise_level, 0.0f);
    EXPECT_LE(config.creativity_noise_level, 1.0f);
    EXPECT_GE(config.coherence_threshold, 0.0f);
    EXPECT_LE(config.coherence_threshold, 1.0f);

    // Timeouts should be reasonable
    EXPECT_GT(config.default_timeout_ms, 0u);
    EXPECT_GT(config.step_timeout_ms, 0u);
}

TEST_F(ImaginationEngineTest, ValidateConfigNull) {
    // WHAT: Verify validate_config handles NULL
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    bool result = imagination_engine_validate_config(nullptr, nullptr, 0);
    EXPECT_FALSE(result);
}

TEST_F(ImaginationEngineTest, ValidateConfigValid) {
    // WHAT: Verify valid config passes validation
    // WHY:  Ensure validation works for good configs
    // HOW:  Use default config

    imagination_engine_config_t config = imagination_engine_default_config();
    char error_msg[256] = {0};

    bool result = imagination_engine_validate_config(&config, error_msg, sizeof(error_msg));
    EXPECT_TRUE(result) << "Error: " << error_msg;
}

TEST_F(ImaginationEngineTest, ValidateConfigInvalidLatentDim) {
    // WHAT: Verify config validation catches invalid latent_dim
    // WHY:  Prevent misconfigured engines
    // HOW:  Set latent_dim beyond max

    imagination_engine_config_t config = imagination_engine_default_config();
    config.latent_dim = IMAGINATION_MAX_LATENT_DIM + 1;
    char error_msg[256] = {0};

    bool result = imagination_engine_validate_config(&config, error_msg, sizeof(error_msg));
    EXPECT_FALSE(result);
}

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_F(ImaginationEngineTest, CreateWithDefaultConfig) {
    // WHAT: Create engine with default config
    // WHY:  Basic lifecycle test
    // HOW:  Create and verify

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    // Check configuration was applied
    EXPECT_EQ(engine->config.latent_dim, config.latent_dim);
    EXPECT_EQ(engine->config.max_concurrent_scenarios, config.max_concurrent_scenarios);
}

TEST_F(ImaginationEngineTest, CreateWithNullConfig) {
    // WHAT: Verify create with NULL uses defaults
    // WHY:  Convenience API
    // HOW:  Pass NULL config

    engine = imagination_engine_create(nullptr);
    ASSERT_NE(engine, nullptr);

    // Should have sensible defaults
    EXPECT_GT(engine->config.latent_dim, 0u);
    EXPECT_GT(engine->config.max_concurrent_scenarios, 0u);
}

TEST_F(ImaginationEngineTest, DestroyNull) {
    // WHAT: Verify destroying NULL is safe
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    imagination_engine_destroy(nullptr);
    SUCCEED();
}

TEST_F(ImaginationEngineTest, Reset) {
    // WHAT: Verify reset works
    // WHY:  Return to initial state
    // HOW:  Create, potentially use, reset

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    int ret = imagination_engine_reset(engine);
    EXPECT_EQ(ret, 0);
}

TEST_F(ImaginationEngineTest, ResetNull) {
    // WHAT: Verify reset handles NULL
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    int ret = imagination_engine_reset(nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Scenario Management Tests
 * ============================================================================ */

TEST_F(ImaginationEngineTest, BeginScenarioPassive) {
    // WHAT: Begin a passive imagination scenario
    // WHY:  Test basic scenario creation
    // HOW:  Create scenario with passive mode

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_PASSIVE, nullptr);
    ASSERT_NE(scenario, nullptr);

    EXPECT_EQ(scenario->mode, IMAGINATION_MODE_PASSIVE);
    EXPECT_TRUE(scenario->is_active);
    EXPECT_FALSE(scenario->is_paused);
    EXPECT_GT(scenario->id, 0u);

    // Cleanup
    int ret = imagination_end_scenario(engine, scenario);
    EXPECT_EQ(ret, 0);
}

TEST_F(ImaginationEngineTest, BeginScenarioDirected) {
    // WHAT: Begin a directed imagination scenario with goal
    // WHY:  Test goal-directed scenario creation
    // HOW:  Create scenario with directed mode and goal

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    imagination_goal_t goal;
    memset(&goal, 0, sizeof(goal));
    goal.mode = IMAGINATION_MODE_DIRECTED;
    goal.priority = 0.8f;

    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_DIRECTED, &goal);
    ASSERT_NE(scenario, nullptr);

    EXPECT_EQ(scenario->mode, IMAGINATION_MODE_DIRECTED);
    EXPECT_TRUE(scenario->is_active);

    // Cleanup
    imagination_end_scenario(engine, scenario);
}

TEST_F(ImaginationEngineTest, BeginScenarioNullEngine) {
    // WHAT: Verify begin_scenario handles NULL engine
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    imagination_scenario_t* scenario = imagination_begin_scenario(
        nullptr, IMAGINATION_MODE_PASSIVE, nullptr);
    EXPECT_EQ(scenario, nullptr);
}

TEST_F(ImaginationEngineTest, StepScenario) {
    // WHAT: Step scenario forward
    // WHY:  Test scenario evolution
    // HOW:  Create and step multiple times

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_PASSIVE, nullptr);
    ASSERT_NE(scenario, nullptr);

    // Step several times
    for (int i = 0; i < 5; i++) {
        int ret = imagination_step_scenario(engine, scenario);
        EXPECT_EQ(ret, 0);
    }

    // Check metrics updated
    EXPECT_GE(scenario->coherence, 0.0f);
    EXPECT_LE(scenario->coherence, 1.0f);
    EXPECT_GE(scenario->vividness, 0.0f);
    EXPECT_LE(scenario->vividness, 1.0f);

    imagination_end_scenario(engine, scenario);
}

TEST_F(ImaginationEngineTest, PauseResumeScenario) {
    // WHAT: Pause and resume scenario
    // WHY:  Test scenario lifecycle states
    // HOW:  Create, pause, verify, resume, verify

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_PASSIVE, nullptr);
    ASSERT_NE(scenario, nullptr);

    // Pause
    int ret = imagination_pause_scenario(engine, scenario);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(scenario->is_paused);

    // Resume
    ret = imagination_resume_scenario(engine, scenario);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(scenario->is_paused);

    imagination_end_scenario(engine, scenario);
}

TEST_F(ImaginationEngineTest, EndScenarioNullEngine) {
    // WHAT: Verify end_scenario handles NULL engine
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    int ret = imagination_end_scenario(nullptr, nullptr);
    EXPECT_NE(ret, 0);
}

/* ============================================================================
 * Multiple Scenarios Tests
 * ============================================================================ */

TEST_F(ImaginationEngineTest, MultipleConcurrentScenarios) {
    // WHAT: Test multiple concurrent scenarios
    // WHY:  Verify concurrent scenario management
    // HOW:  Create multiple scenarios, verify all active

    imagination_engine_config_t config = imagination_engine_default_config();
    config.max_concurrent_scenarios = 4;
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    imagination_scenario_t* scenarios[4] = {nullptr};

    // Create multiple scenarios
    for (int i = 0; i < 4; i++) {
        scenarios[i] = imagination_begin_scenario(
            engine, IMAGINATION_MODE_PASSIVE, nullptr);
        ASSERT_NE(scenarios[i], nullptr) << "Failed to create scenario " << i;
    }

    // Verify all have unique IDs
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            EXPECT_NE(scenarios[i]->id, scenarios[j]->id);
        }
    }

    // Cleanup
    for (int i = 0; i < 4; i++) {
        imagination_end_scenario(engine, scenarios[i]);
    }
}

TEST_F(ImaginationEngineTest, ExceedMaxScenarios) {
    // WHAT: Test exceeding max concurrent scenarios
    // WHY:  Verify engine respects limits
    // HOW:  Try to create more scenarios than allowed

    imagination_engine_config_t config = imagination_engine_default_config();
    config.max_concurrent_scenarios = 2;
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    imagination_scenario_t* s1 = imagination_begin_scenario(
        engine, IMAGINATION_MODE_PASSIVE, nullptr);
    imagination_scenario_t* s2 = imagination_begin_scenario(
        engine, IMAGINATION_MODE_PASSIVE, nullptr);

    ASSERT_NE(s1, nullptr);
    ASSERT_NE(s2, nullptr);

    // Third should fail or return NULL (depending on implementation)
    imagination_scenario_t* s3 = imagination_begin_scenario(
        engine, IMAGINATION_MODE_PASSIVE, nullptr);
    EXPECT_EQ(s3, nullptr);

    imagination_end_scenario(engine, s1);
    imagination_end_scenario(engine, s2);
}

/* ============================================================================
 * Imagination Mode Tests
 * ============================================================================ */

TEST_F(ImaginationEngineTest, AllModesCanBeCreated) {
    // WHAT: Verify all imagination modes can create scenarios
    // WHY:  Ensure mode system works for all modes
    // HOW:  Loop through all modes

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    for (int mode = 0; mode < IMAGINATION_MODE_COUNT; mode++) {
        imagination_scenario_t* scenario = imagination_begin_scenario(
            engine, static_cast<imagination_mode_t>(mode), nullptr);
        ASSERT_NE(scenario, nullptr) << "Failed for mode " << mode;
        EXPECT_EQ(scenario->mode, static_cast<imagination_mode_t>(mode));
        imagination_end_scenario(engine, scenario);
    }
}

TEST_F(ImaginationEngineTest, ModeToStringConversion) {
    // WHAT: Verify mode-to-string conversion
    // WHY:  Debugging and logging support
    // HOW:  Check all modes return non-null strings

    for (int mode = 0; mode < IMAGINATION_MODE_COUNT; mode++) {
        const char* str = imagination_mode_to_string(
            static_cast<imagination_mode_t>(mode));
        EXPECT_NE(str, nullptr) << "Null string for mode " << mode;
        EXPECT_GT(strlen(str), 0u) << "Empty string for mode " << mode;
    }
}

TEST_F(ImaginationEngineTest, QualityToStringConversion) {
    // WHAT: Verify quality-to-string conversion
    // WHY:  Debugging and logging support
    // HOW:  Check all quality levels

    imagination_quality_t qualities[] = {
        IMAGINATION_QUALITY_DRAFT,
        IMAGINATION_QUALITY_NORMAL,
        IMAGINATION_QUALITY_HIGH,
        IMAGINATION_QUALITY_VIVID
    };

    for (auto quality : qualities) {
        const char* str = imagination_quality_to_string(quality);
        EXPECT_NE(str, nullptr);
        EXPECT_GT(strlen(str), 0u);
    }
}

/* ============================================================================
 * Generation Tests
 * ============================================================================ */

TEST_F(ImaginationEngineTest, GenerateVisual) {
    // WHAT: Test visual generation
    // WHY:  Verify latent-to-visual pipeline
    // HOW:  Create scenario, generate visual, check buffer

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_DIRECTED, nullptr);
    ASSERT_NE(scenario, nullptr);

    int ret = imagination_generate_visual(engine, scenario);
    EXPECT_EQ(ret, 0);

    // Visual buffer should be allocated after generation
    EXPECT_NE(scenario->visual_buffer, nullptr);

    imagination_end_scenario(engine, scenario);
}

TEST_F(ImaginationEngineTest, GenerateAudio) {
    // WHAT: Test audio generation
    // WHY:  Verify latent-to-audio pipeline
    // HOW:  Create scenario, generate audio, check buffer

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_DIRECTED, nullptr);
    ASSERT_NE(scenario, nullptr);

    int ret = imagination_generate_audio(engine, scenario);
    EXPECT_EQ(ret, 0);

    // Audio buffer should be allocated after generation
    EXPECT_NE(scenario->audio_buffer, nullptr);

    imagination_end_scenario(engine, scenario);
}

TEST_F(ImaginationEngineTest, GenerateMultimodal) {
    // WHAT: Test multimodal generation
    // WHY:  Verify combined visual+audio generation
    // HOW:  Create scenario, generate multimodal

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_DIRECTED, nullptr);
    ASSERT_NE(scenario, nullptr);

    int ret = imagination_generate_multimodal(engine, scenario);
    EXPECT_EQ(ret, 0);

    // Both buffers should be allocated
    EXPECT_NE(scenario->visual_buffer, nullptr);
    EXPECT_NE(scenario->audio_buffer, nullptr);

    imagination_end_scenario(engine, scenario);
}

/* ============================================================================
 * Evaluation Tests
 * ============================================================================ */

TEST_F(ImaginationEngineTest, EvaluateScenario) {
    // WHAT: Test scenario evaluation
    // WHY:  Verify coherence/plausibility computation
    // HOW:  Create scenario, evaluate, check result

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_DIRECTED, nullptr);
    ASSERT_NE(scenario, nullptr);

    // Step a few times to evolve scenario
    for (int i = 0; i < 3; i++) {
        imagination_step_scenario(engine, scenario);
    }

    imagination_evaluation_t eval = {0};
    int ret = imagination_evaluate(engine, scenario, &eval);
    EXPECT_EQ(ret, 0);

    // Check evaluation values are in valid range
    EXPECT_GE(eval.coherence, 0.0f);
    EXPECT_LE(eval.coherence, 1.0f);
    EXPECT_GE(eval.plausibility, 0.0f);
    EXPECT_LE(eval.plausibility, 1.0f);
    EXPECT_GE(eval.novelty, 0.0f);
    EXPECT_LE(eval.novelty, 1.0f);

    imagination_end_scenario(engine, scenario);
}

TEST_F(ImaginationEngineTest, CheckPlausibility) {
    // WHAT: Test plausibility check
    // WHY:  Verify plausibility computation
    // HOW:  Create scenario, check plausibility

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_DIRECTED, nullptr);
    ASSERT_NE(scenario, nullptr);

    float plausibility = imagination_check_plausibility(engine, scenario);
    EXPECT_GE(plausibility, 0.0f);
    EXPECT_LE(plausibility, 1.0f);

    imagination_end_scenario(engine, scenario);
}

TEST_F(ImaginationEngineTest, RealityDistance) {
    // WHAT: Test reality distance computation
    // WHY:  Verify distance-from-reality metric
    // HOW:  Create scenario, measure distance

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_CREATIVE, nullptr);
    ASSERT_NE(scenario, nullptr);

    // Step to evolve away from reality
    for (int i = 0; i < 5; i++) {
        imagination_step_scenario(engine, scenario);
    }

    float distance = imagination_reality_distance(engine, scenario);
    EXPECT_GE(distance, 0.0f);

    imagination_end_scenario(engine, scenario);
}

/* ============================================================================
 * Statistics Tests
 * ============================================================================ */

TEST_F(ImaginationEngineTest, GetStats) {
    // WHAT: Test statistics retrieval
    // WHY:  Verify stats tracking
    // HOW:  Create scenarios, get stats

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    // Create and end a scenario
    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_PASSIVE, nullptr);
    ASSERT_NE(scenario, nullptr);
    imagination_end_scenario(engine, scenario);

    imagination_stats_t stats = {0};
    int ret = imagination_get_stats(engine, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.scenarios_created, 1u);
}

TEST_F(ImaginationEngineTest, ResetStats) {
    // WHAT: Test statistics reset
    // WHY:  Verify stats can be cleared
    // HOW:  Create scenarios, reset stats, verify

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    // Create and end a scenario
    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_PASSIVE, nullptr);
    ASSERT_NE(scenario, nullptr);
    imagination_end_scenario(engine, scenario);

    // Reset stats
    int ret = imagination_reset_stats(engine);
    EXPECT_EQ(ret, 0);

    // Stats should be zero after reset
    imagination_stats_t stats = {0};
    imagination_get_stats(engine, &stats);
    EXPECT_EQ(stats.scenarios_created, 0u);
    EXPECT_EQ(stats.scenarios_completed, 0u);
}

/* ============================================================================
 * Advanced Imagination Tests
 * ============================================================================ */

TEST_F(ImaginationEngineTest, CounterfactualImagination) {
    // WHAT: Test counterfactual reasoning
    // WHY:  Verify "what if" scenario generation
    // HOW:  Create counterfactual scenario

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    // Create a simple memory state
    uint32_t dims[] = {(uint32_t)engine->config.latent_dim};
    nimcp_tensor_t* memory = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_NE(memory, nullptr);

    // Fill with some values
    float* data = (float*)nimcp_tensor_data(memory);
    for (size_t i = 0; i < engine->config.latent_dim; i++) {
        data[i] = (float)i / engine->config.latent_dim;
    }

    counterfactual_query_t query = {0};
    query.original_state = memory;
    query.steps_forward = 5;
    query.preserve_agents = true;

    imagination_scenario_t* scenario = imagination_counterfactual(engine, memory, &query);
    // May return NULL if counterfactual requires additional setup
    if (scenario) {
        EXPECT_EQ(scenario->mode, IMAGINATION_MODE_COUNTERFACTUAL);
        imagination_end_scenario(engine, scenario);
    }

    nimcp_tensor_destroy(memory);
}

TEST_F(ImaginationEngineTest, CreativeRecombination) {
    // WHAT: Test creative recombination
    // WHY:  Verify dream-like creative mode
    // HOW:  Provide seed memories, recombine

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    // Create seed memories
    uint32_t dims[] = {(uint32_t)engine->config.latent_dim};
    nimcp_tensor_t* seeds[2];
    seeds[0] = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    seeds[1] = nimcp_tensor_create(dims, 1, NIMCP_DTYPE_F32);
    ASSERT_NE(seeds[0], nullptr);
    ASSERT_NE(seeds[1], nullptr);

    // Fill with different patterns
    float* data0 = (float*)nimcp_tensor_data(seeds[0]);
    float* data1 = (float*)nimcp_tensor_data(seeds[1]);
    for (size_t i = 0; i < engine->config.latent_dim; i++) {
        data0[i] = sinf((float)i * 0.1f);
        data1[i] = cosf((float)i * 0.1f);
    }

    imagination_scenario_t* scenario = imagination_creative_recombine(
        engine, seeds, 2, 0.7f);
    if (scenario) {
        EXPECT_EQ(scenario->mode, IMAGINATION_MODE_CREATIVE);
        imagination_end_scenario(engine, scenario);
    }

    nimcp_tensor_destroy(seeds[0]);
    nimcp_tensor_destroy(seeds[1]);
}

/* ============================================================================
 * Workspace Tests
 * ============================================================================ */

class ImaginationWorkspaceTest : public NimcpTestBase {
protected:
    imagination_workspace_t* workspace = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();
    }

    void TearDown() override {
        if (workspace) {
            imagination_workspace_destroy(workspace);
            workspace = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

TEST_F(ImaginationWorkspaceTest, DefaultConfigValues) {
    // WHAT: Verify workspace default config
    // WHY:  Ensure sensible defaults
    // HOW:  Get default config, check values

    imagination_workspace_config_t config = imagination_workspace_default_config();

    EXPECT_GT(config.max_scenarios, 0u);
    EXPECT_GT(config.latent_dim, 0u);
    EXPECT_GT(config.visual_width, 0u);
    EXPECT_GT(config.visual_height, 0u);
    EXPECT_GT(config.audio_samples, 0u);
}

TEST_F(ImaginationWorkspaceTest, CreateWithDefaults) {
    // WHAT: Create workspace with default config
    // WHY:  Basic lifecycle test
    // HOW:  Create and verify

    workspace = imagination_workspace_create(nullptr);
    ASSERT_NE(workspace, nullptr);
}

TEST_F(ImaginationWorkspaceTest, CreateWithConfig) {
    // WHAT: Create workspace with custom config
    // WHY:  Verify custom configuration
    // HOW:  Use custom config

    imagination_workspace_config_t config = imagination_workspace_default_config();
    config.max_scenarios = 4;
    config.latent_dim = 128;

    workspace = imagination_workspace_create(&config);
    ASSERT_NE(workspace, nullptr);

    EXPECT_EQ(workspace->config.max_scenarios, 4u);
    EXPECT_EQ(workspace->config.latent_dim, 128u);
}

TEST_F(ImaginationWorkspaceTest, DestroyNull) {
    // WHAT: Verify destroy handles NULL
    // WHY:  Defensive programming
    // HOW:  Call with NULL

    imagination_workspace_destroy(nullptr);
    SUCCEED();
}

TEST_F(ImaginationWorkspaceTest, AllocateScenario) {
    // WHAT: Allocate scenario slot
    // WHY:  Test scenario slot management
    // HOW:  Allocate, verify ID

    workspace = imagination_workspace_create(nullptr);
    ASSERT_NE(workspace, nullptr);

    scenario_id_t id = imagination_workspace_allocate_scenario(workspace);
    EXPECT_GT(id, 0u);

    // Should exist
    EXPECT_TRUE(imagination_workspace_has_scenario(workspace, id));
}

TEST_F(ImaginationWorkspaceTest, ReleaseScenario) {
    // WHAT: Release scenario slot
    // WHY:  Test cleanup
    // HOW:  Allocate, release, verify

    workspace = imagination_workspace_create(nullptr);
    ASSERT_NE(workspace, nullptr);

    scenario_id_t id = imagination_workspace_allocate_scenario(workspace);
    ASSERT_GT(id, 0u);

    int ret = imagination_workspace_release_scenario(workspace, id);
    EXPECT_EQ(ret, 0);

    // Should no longer exist
    EXPECT_FALSE(imagination_workspace_has_scenario(workspace, id));
}

TEST_F(ImaginationWorkspaceTest, ActiveCount) {
    // WHAT: Test active scenario counting
    // WHY:  Verify count tracking
    // HOW:  Allocate multiple, check count

    imagination_workspace_config_t config = imagination_workspace_default_config();
    config.max_scenarios = 8;
    workspace = imagination_workspace_create(&config);
    ASSERT_NE(workspace, nullptr);

    EXPECT_EQ(imagination_workspace_active_count(workspace), 0u);

    scenario_id_t id1 = imagination_workspace_allocate_scenario(workspace);
    scenario_id_t id2 = imagination_workspace_allocate_scenario(workspace);

    EXPECT_EQ(imagination_workspace_active_count(workspace), 2u);

    imagination_workspace_release_scenario(workspace, id1);
    EXPECT_EQ(imagination_workspace_active_count(workspace), 1u);

    imagination_workspace_release_scenario(workspace, id2);
    EXPECT_EQ(imagination_workspace_active_count(workspace), 0u);
}

TEST_F(ImaginationWorkspaceTest, Reset) {
    // WHAT: Test workspace reset
    // WHY:  Verify full reset clears scenarios
    // HOW:  Create scenarios, reset, verify empty

    workspace = imagination_workspace_create(nullptr);
    ASSERT_NE(workspace, nullptr);

    imagination_workspace_allocate_scenario(workspace);
    imagination_workspace_allocate_scenario(workspace);

    EXPECT_GT(imagination_workspace_active_count(workspace), 0u);

    int ret = imagination_workspace_reset(workspace);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(imagination_workspace_active_count(workspace), 0u);
}

TEST_F(ImaginationWorkspaceTest, GetStats) {
    // WHAT: Test workspace statistics
    // WHY:  Verify stats collection
    // HOW:  Use workspace, get stats

    workspace = imagination_workspace_create(nullptr);
    ASSERT_NE(workspace, nullptr);

    imagination_workspace_allocate_scenario(workspace);

    imagination_workspace_stats_t stats = {0};
    int ret = imagination_workspace_get_stats(workspace, &stats);
    EXPECT_EQ(ret, 0);
    EXPECT_GE(stats.scenarios_created, 1u);
}

/* ============================================================================
 * GPU Tests (Conditional)
 * ============================================================================ */

TEST_F(ImaginationEngineTest, GPUAvailabilityCheck) {
    // WHAT: Test GPU availability check
    // WHY:  Verify GPU detection
    // HOW:  Check availability flag

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    // Just verify the function works - GPU may or may not be available
    bool available = imagination_gpu_available(engine);
    // No assertion on value - just verify it returns without error
    (void)available;
    SUCCEED();
}

/* ============================================================================
 * Immune Modulation Tests
 * ============================================================================ */

TEST_F(ImaginationEngineTest, ImmuneModulationDefault) {
    // WHAT: Test immune modulation without immune system
    // WHY:  Verify default behavior when no immune connected
    // HOW:  Get modulation factor

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    float modulation = imagination_get_immune_modulation(engine);
    EXPECT_EQ(modulation, 1.0f);  // No modulation = 1.0 (healthy)
}

/* ============================================================================
 * Print State Test
 * ============================================================================ */

TEST_F(ImaginationEngineTest, PrintState) {
    // WHAT: Test print state function
    // WHY:  Verify debugging output doesn't crash
    // HOW:  Call print with various options

    imagination_engine_config_t config = imagination_engine_default_config();
    engine = imagination_engine_create(&config);
    ASSERT_NE(engine, nullptr);

    // Create a scenario for more interesting output
    imagination_scenario_t* scenario = imagination_begin_scenario(
        engine, IMAGINATION_MODE_PASSIVE, nullptr);
    ASSERT_NE(scenario, nullptr);

    // Just verify it doesn't crash
    imagination_print_state(engine, false);
    imagination_print_state(engine, true);

    imagination_end_scenario(engine, scenario);
    SUCCEED();
}
