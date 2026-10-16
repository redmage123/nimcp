/**
 * @file test_omni_wm_memory_cognitive_integration.cpp
 * @brief Integration tests for World Model Memory + Cognitive bridge integration
 * @version 1.0.0
 * @date 2026-01-24
 *
 * Tests cross-bridge data flow between Memory and Cognitive bridges:
 * - Memory replay -> WM training -> Prediction -> Cognitive planning
 * - Goal conditioning from Cognitive -> WM -> Memory encoding
 * - Hippocampal pattern completion for working memory context
 * - Consolidation sync with meta-learning adaptation
 * - Episodic context enriching executive predictions
 * - Attention focus guiding memory retrieval
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <algorithm>

/* World Model core */
#include "cognitive/omni/nimcp_omni_world_model.h"

/* Memory and Cognitive bridges */
#include "cognitive/omni/bridges/nimcp_omni_wm_memory_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_cognitive_bridge.h"

/* Logging for audit trail */
#include "cognitive/omni/bridges/nimcp_omni_wm_logging_bridge.h"

/* Bio-async for message routing */
#include "async/nimcp_bio_messages.h"

/* Memory utilities */
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture for Memory-Cognitive Bridge Integration
 * ============================================================================ */

class WMMemoryCognitiveTest : public ::testing::Test {
protected:
    static constexpr uint32_t STATE_DIM = 64;
    static constexpr uint32_t ACTION_DIM = 16;
    static constexpr uint32_t OBS_DIM = 64;
    static constexpr uint32_t CONTEXT_DIM = 128;
    static constexpr float DT = 0.016f;

    /* Core components */
    omni_world_model_t* wm = nullptr;

    /* Bridges */
    omni_wm_memory_bridge_t* memory_bridge = nullptr;
    omni_wm_cognitive_bridge_t* cognitive_bridge = nullptr;
    omni_wm_logging_bridge_t* logging_bridge = nullptr;

    void SetUp() override {
        /* Create world model with RSSM for dynamics learning */
        omni_wm_config_t wm_config;
        omni_wm_get_default_config(&wm_config);
        wm_config.state_dim = STATE_DIM;
        wm_config.action_dim = ACTION_DIM;
        wm_config.obs_dim = OBS_DIM;
        wm_config.rssm_h_dim = STATE_DIM;
        wm_config.rssm_z_dim = STATE_DIM / 2;
        wm_config.use_rssm = true;
        wm_config.enable_dreaming = true;
        wm_config.replay_buffer_size = 1000;
        wm_config.batch_size = 32;

        wm = omni_wm_create(&wm_config);
    }

    void TearDown() override {
        if (logging_bridge) omni_wm_logging_bridge_destroy(logging_bridge);
        if (cognitive_bridge) omni_wm_cognitive_bridge_destroy(cognitive_bridge);
        if (memory_bridge) omni_wm_memory_bridge_destroy(memory_bridge);
        if (wm) omni_wm_destroy(wm);
    }

    void CreateBridges() {
        /* Memory Bridge */
        omni_wm_memory_bridge_config_t mem_config;
        omni_wm_memory_bridge_default_config(&mem_config);
        mem_config.enable_replay_training = true;
        mem_config.enable_engram_encoding = true;
        mem_config.enable_context_retrieval = true;
        mem_config.enable_consolidation_sync = true;
        mem_config.replay_batch_size = 32;
        mem_config.encoding_threshold = 0.3f;

        memory_bridge = omni_wm_memory_bridge_create(&mem_config);
        if (memory_bridge && wm) {
            omni_wm_memory_bridge_connect_world_model(memory_bridge, wm);
        }

        /* Cognitive Bridge */
        omni_wm_cognitive_bridge_config_t cog_config = omni_wm_cognitive_bridge_default_config();
        cog_config.enable_goal_conditioning = true;
        cog_config.enable_attention_modulation = true;
        cog_config.enable_executive_integration = true;
        cog_config.enable_salience_integration = true;
        cog_config.enable_working_memory_context = true;
        cog_config.enable_meta_learning = true;

        cognitive_bridge = omni_wm_cognitive_bridge_create(&cog_config);
        if (cognitive_bridge && wm) {
            omni_wm_cognitive_bridge_connect_world_model(cognitive_bridge, wm);
        }

        /* Logging Bridge for audit */
        omni_wm_logging_bridge_config_t log_config;
        omni_wm_logging_bridge_default_config(&log_config);
        log_config.log_predictions = true;
        log_config.log_training = true;
        log_config.log_anomalies = true;

        logging_bridge = omni_wm_logging_bridge_create(&log_config);
        if (logging_bridge && wm) {
            omni_wm_logging_bridge_connect_world_model(logging_bridge, wm);
        }
    }

    void generate_random_pattern(float* pattern, uint32_t dim) {
        for (uint32_t i = 0; i < dim; i++) {
            pattern[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        }
    }

    void normalize_pattern(float* pattern, uint32_t dim) {
        float sum_sq = 0.0f;
        for (uint32_t i = 0; i < dim; i++) {
            sum_sq += pattern[i] * pattern[i];
        }
        if (sum_sq > 0.0f) {
            float norm = sqrtf(sum_sq);
            for (uint32_t i = 0; i < dim; i++) {
                pattern[i] /= norm;
            }
        }
    }
};

/* ============================================================================
 * Basic Bridge Setup Tests
 * ============================================================================ */

TEST_F(WMMemoryCognitiveTest, BridgeCreation) {
    ASSERT_NE(wm, nullptr);

    CreateBridges();

    EXPECT_NE(memory_bridge, nullptr);
    EXPECT_NE(cognitive_bridge, nullptr);
    EXPECT_NE(logging_bridge, nullptr);
}

TEST_F(WMMemoryCognitiveTest, BridgeConnections) {
    if (!wm) GTEST_SKIP();

    CreateBridges();

    if (memory_bridge) {
        EXPECT_TRUE(omni_wm_memory_bridge_is_connected(memory_bridge));
    }
    if (cognitive_bridge) {
        EXPECT_TRUE(omni_wm_cognitive_bridge_is_connected(cognitive_bridge));
    }
    if (logging_bridge) {
        EXPECT_TRUE(omni_wm_logging_bridge_is_connected(logging_bridge));
    }
}

/* ============================================================================
 * Cross-Bridge Data Flow Tests: Memory -> WM -> Cognitive
 * ============================================================================ */

TEST_F(WMMemoryCognitiveTest, MemoryReplayToWMTraining) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!memory_bridge) GTEST_SKIP();

    /* Simulate replay sequence */
    const uint32_t SEQ_LEN = 10;
    float** states = (float**)nimcp_malloc(SEQ_LEN * sizeof(float*));
    float** actions = (float**)nimcp_malloc(SEQ_LEN * sizeof(float*));
    float* rewards = (float*)nimcp_malloc(SEQ_LEN * sizeof(float));

    ASSERT_NE(states, nullptr);
    ASSERT_NE(actions, nullptr);
    ASSERT_NE(rewards, nullptr);

    for (uint32_t i = 0; i < SEQ_LEN; i++) {
        states[i] = (float*)nimcp_malloc(STATE_DIM * sizeof(float));
        actions[i] = (float*)nimcp_malloc(ACTION_DIM * sizeof(float));
        generate_random_pattern(states[i], STATE_DIM);
        generate_random_pattern(actions[i], ACTION_DIM);
        rewards[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
    }

    /* Train from replay */
    nimcp_error_t ret = omni_wm_memory_bridge_train_from_replay(
        memory_bridge,
        (const float**)states,
        (const float**)actions,
        rewards,
        SEQ_LEN,
        false  /* not reverse replay */
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Get memory bridge stats to verify training happened */
    omni_wm_memory_bridge_stats_t mem_stats;
    ret = omni_wm_memory_bridge_get_stats(memory_bridge, &mem_stats);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(mem_stats.replay_sequences_received, 1u);

    /* Clean up */
    for (uint32_t i = 0; i < SEQ_LEN; i++) {
        nimcp_free(states[i]);
        nimcp_free(actions[i]);
    }
    nimcp_free(states);
    nimcp_free(actions);
    nimcp_free(rewards);
}

TEST_F(WMMemoryCognitiveTest, CognitiveGoalRegistration) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!cognitive_bridge) GTEST_SKIP();

    /* Register a goal */
    float target_state[STATE_DIM];
    generate_random_pattern(target_state, STATE_DIM);
    normalize_pattern(target_state, STATE_DIM);

    uint32_t goal_id;
    nimcp_error_t ret = omni_wm_cognitive_bridge_register_goal(
        cognitive_bridge,
        target_state,
        STATE_DIM,
        0.8f,  /* priority */
        0,     /* no deadline */
        "Test goal",
        &goal_id
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(goal_id, 0u);

    /* Check goal count */
    uint32_t num_goals = omni_wm_cognitive_bridge_get_num_goals(cognitive_bridge);
    EXPECT_GE(num_goals, 1u);

    /* Get goal and verify */
    const wm_cognitive_goal_t* goal = omni_wm_cognitive_bridge_get_goal(cognitive_bridge, goal_id);
    EXPECT_NE(goal, nullptr);
    if (goal) {
        EXPECT_NEAR(goal->priority, 0.8f, 0.01f);
        EXPECT_TRUE(goal->is_active);
    }
}

TEST_F(WMMemoryCognitiveTest, GoalProgressUpdate) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!cognitive_bridge) GTEST_SKIP();

    /* Register goal */
    float target_state[STATE_DIM];
    generate_random_pattern(target_state, STATE_DIM);

    uint32_t goal_id;
    omni_wm_cognitive_bridge_register_goal(
        cognitive_bridge,
        target_state,
        STATE_DIM,
        0.8f,
        0,
        "Progress test goal",
        &goal_id
    );

    /* Update progress */
    nimcp_error_t ret = omni_wm_cognitive_bridge_update_goal_progress(
        cognitive_bridge,
        goal_id,
        0.5f
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Verify progress */
    const wm_cognitive_goal_t* goal = omni_wm_cognitive_bridge_get_goal(cognitive_bridge, goal_id);
    if (goal) {
        EXPECT_NEAR(goal->progress, 0.5f, 0.01f);
    }

    /* Mark achieved */
    ret = omni_wm_cognitive_bridge_goal_achieved(cognitive_bridge, goal_id);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

/* ============================================================================
 * Attention and Memory Interaction Tests
 * ============================================================================ */

TEST_F(WMMemoryCognitiveTest, AttentionFocusSetting) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!cognitive_bridge) GTEST_SKIP();

    /* Set attention focus */
    float focus_location[STATE_DIM];
    generate_random_pattern(focus_location, STATE_DIM);
    normalize_pattern(focus_location, STATE_DIM);

    nimcp_error_t ret = omni_wm_cognitive_bridge_set_attention_focus(
        cognitive_bridge,
        focus_location,
        STATE_DIM,
        0.9f,   /* strength */
        0.2f    /* bandwidth */
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Update cognitive bridge - should use attention */
    ret = omni_wm_cognitive_bridge_update(cognitive_bridge, DT);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Get stats */
    omni_wm_cognitive_bridge_stats_t stats;
    ret = omni_wm_cognitive_bridge_get_stats(cognitive_bridge, &stats);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(stats.attention_focus_events, 1u);
}

TEST_F(WMMemoryCognitiveTest, AttentionShift) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!cognitive_bridge) GTEST_SKIP();

    /* Set initial focus */
    float focus1[STATE_DIM], focus2[STATE_DIM];
    generate_random_pattern(focus1, STATE_DIM);
    generate_random_pattern(focus2, STATE_DIM);

    omni_wm_cognitive_bridge_set_attention_focus(
        cognitive_bridge, focus1, STATE_DIM, 0.8f, 0.2f);

    omni_wm_cognitive_bridge_update(cognitive_bridge, DT);

    /* Shift attention */
    nimcp_error_t ret = omni_wm_cognitive_bridge_attention_shift(
        cognitive_bridge,
        focus2,
        STATE_DIM,
        0.9f
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    omni_wm_cognitive_bridge_update(cognitive_bridge, DT);

    /* Check stats */
    omni_wm_cognitive_bridge_stats_t stats;
    omni_wm_cognitive_bridge_get_stats(cognitive_bridge, &stats);
    EXPECT_GE(stats.attention_shifts, 1u);
}

TEST_F(WMMemoryCognitiveTest, ClearAttention) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!cognitive_bridge) GTEST_SKIP();

    float focus[STATE_DIM];
    generate_random_pattern(focus, STATE_DIM);

    omni_wm_cognitive_bridge_set_attention_focus(
        cognitive_bridge, focus, STATE_DIM, 0.9f, 0.2f);

    omni_wm_cognitive_bridge_update(cognitive_bridge, DT);

    nimcp_error_t ret = omni_wm_cognitive_bridge_clear_attention(cognitive_bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

/* ============================================================================
 * Memory Encoding and Retrieval Tests
 * ============================================================================ */

TEST_F(WMMemoryCognitiveTest, EngramEncoding) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!memory_bridge) GTEST_SKIP();

    /* Encode engram from WM state */
    uint64_t engram_id;
    nimcp_error_t ret = omni_wm_memory_bridge_encode_engram(
        memory_bridge,
        0.7f,    /* emotional tag */
        true,    /* force encode */
        &engram_id
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Get stats */
    omni_wm_memory_bridge_stats_t stats;
    ret = omni_wm_memory_bridge_get_stats(memory_bridge, &stats);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(stats.engrams_encoded, 1u);
}

TEST_F(WMMemoryCognitiveTest, EpisodicContextRetrieval) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!memory_bridge) GTEST_SKIP();

    /* First encode some engrams to have context */
    for (int i = 0; i < 3; i++) {
        uint64_t engram_id;
        omni_wm_memory_bridge_encode_engram(
            memory_bridge,
            0.5f + (float)i * 0.1f,
            true,
            &engram_id
        );
        omni_wm_memory_bridge_update(memory_bridge, DT);
    }

    /* Retrieve episodic context */
    float context[CONTEXT_DIM];
    float confidence;
    nimcp_error_t ret = omni_wm_memory_bridge_retrieve_episodic_context(
        memory_bridge,
        context,
        CONTEXT_DIM,
        &confidence
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);

    /* Check stats */
    omni_wm_memory_bridge_stats_t stats;
    omni_wm_memory_bridge_get_stats(memory_bridge, &stats);
    EXPECT_GE(stats.engram_retrievals, 1u);
}

/* ============================================================================
 * Pattern Operations Tests
 * ============================================================================ */

TEST_F(WMMemoryCognitiveTest, PatternCompletion) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!memory_bridge) GTEST_SKIP();

    /* Create partial pattern */
    float partial[STATE_DIM / 2];
    generate_random_pattern(partial, STATE_DIM / 2);

    /* Request pattern completion */
    float completed[STATE_DIM];
    float confidence;
    nimcp_error_t ret = omni_wm_memory_bridge_pattern_complete(
        memory_bridge,
        partial,
        STATE_DIM / 2,
        completed,
        STATE_DIM,
        &confidence
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Get stats */
    omni_wm_memory_bridge_stats_t stats;
    omni_wm_memory_bridge_get_stats(memory_bridge, &stats);
    EXPECT_GE(stats.pattern_completions, 1u);
}

TEST_F(WMMemoryCognitiveTest, PatternSeparation) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!memory_bridge) GTEST_SKIP();

    /* Create input pattern */
    float input[STATE_DIM];
    generate_random_pattern(input, STATE_DIM);

    /* Request pattern separation */
    float separated[STATE_DIM];
    float separation_strength;
    nimcp_error_t ret = omni_wm_memory_bridge_pattern_separate(
        memory_bridge,
        input,
        STATE_DIM,
        separated,
        STATE_DIM,
        &separation_strength
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Get stats */
    omni_wm_memory_bridge_stats_t stats;
    omni_wm_memory_bridge_get_stats(memory_bridge, &stats);
    EXPECT_GE(stats.pattern_separations, 1u);
}

/* ============================================================================
 * Salience and Prediction Error Tests
 * ============================================================================ */

TEST_F(WMMemoryCognitiveTest, SalienceUpdate) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!cognitive_bridge) GTEST_SKIP();

    nimcp_error_t ret = omni_wm_cognitive_bridge_update_salience(
        cognitive_bridge,
        0.8f,  /* novelty */
        0.6f,  /* surprise */
        0.4f   /* urgency */
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Update bridge */
    ret = omni_wm_cognitive_bridge_update(cognitive_bridge, DT);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Check effects */
    const cognitive_to_omni_wm_effects_t* effects =
        omni_wm_cognitive_bridge_get_cognitive_effects(cognitive_bridge);
    EXPECT_NE(effects, nullptr);
    if (effects) {
        /* High novelty should trigger high salience event */
        EXPECT_TRUE(effects->high_salience_event || effects->salience.novelty > 0.5f);
    }
}

TEST_F(WMMemoryCognitiveTest, PredictionErrorMap) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!cognitive_bridge) GTEST_SKIP();

    /* Run some updates to accumulate prediction errors */
    for (int i = 0; i < 10; i++) {
        omni_wm_cognitive_bridge_update(cognitive_bridge, DT);
    }

    /* Get prediction error map */
    float pe_map[STATE_DIM];
    float max_pe;
    nimcp_error_t ret = omni_wm_cognitive_bridge_get_prediction_error_map(
        cognitive_bridge,
        pe_map,
        STATE_DIM,
        &max_pe
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(max_pe, 0.0f);
}

/* ============================================================================
 * State Prediction Tests
 * ============================================================================ */

TEST_F(WMMemoryCognitiveTest, StatePrediction) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!cognitive_bridge) GTEST_SKIP();

    /* Create current state and action */
    float current_state[STATE_DIM];
    float action[ACTION_DIM];
    generate_random_pattern(current_state, STATE_DIM);
    generate_random_pattern(action, ACTION_DIM);

    /* Predict next state */
    float predicted_state[STATE_DIM];
    float confidence;
    nimcp_error_t ret = omni_wm_cognitive_bridge_predict_state(
        cognitive_bridge,
        current_state,
        STATE_DIM,
        action,
        ACTION_DIM,
        predicted_state,
        &confidence
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);

    /* Get stats */
    omni_wm_cognitive_bridge_stats_t stats;
    omni_wm_cognitive_bridge_get_stats(cognitive_bridge, &stats);
    EXPECT_GE(stats.state_predictions, 1u);
}

/* ============================================================================
 * Working Memory Context Tests
 * ============================================================================ */

TEST_F(WMMemoryCognitiveTest, WorkingMemoryContextUpdate) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!cognitive_bridge) GTEST_SKIP();

    /* Update working memory context */
    nimcp_error_t ret = omni_wm_cognitive_bridge_update_wm_context(cognitive_bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Get context */
    float context[CONTEXT_DIM];
    float utilization;
    ret = omni_wm_cognitive_bridge_get_wm_context(
        cognitive_bridge,
        context,
        CONTEXT_DIM,
        &utilization
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GE(utilization, 0.0f);
    EXPECT_LE(utilization, 1.0f);
}

/* ============================================================================
 * Meta-Learning Integration Tests
 * ============================================================================ */

TEST_F(WMMemoryCognitiveTest, RecommendedLearningRate) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!cognitive_bridge) GTEST_SKIP();

    /* Run updates to establish baseline */
    for (int i = 0; i < 20; i++) {
        omni_wm_cognitive_bridge_update(cognitive_bridge, DT);
    }

    /* Get recommended learning rate */
    float recommended_lr;
    nimcp_error_t ret = omni_wm_cognitive_bridge_get_recommended_lr(
        cognitive_bridge,
        &recommended_lr
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(recommended_lr, 0.0f);
}

TEST_F(WMMemoryCognitiveTest, TaskAdaptation) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!cognitive_bridge) GTEST_SKIP();

    /* Trigger adaptation with high prediction error */
    nimcp_error_t ret = omni_wm_cognitive_bridge_trigger_adaptation(
        cognitive_bridge,
        2.0f  /* high prediction error */
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Get stats */
    omni_wm_cognitive_bridge_stats_t stats;
    omni_wm_cognitive_bridge_get_stats(cognitive_bridge, &stats);
    EXPECT_GE(stats.adaptation_triggers, 1u);
}

/* ============================================================================
 * Sleep and Consolidation Tests
 * ============================================================================ */

TEST_F(WMMemoryCognitiveTest, SleepStateTransition) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!memory_bridge) GTEST_SKIP();

    /* Enter sleep state */
    nimcp_error_t ret = omni_wm_memory_bridge_set_sleep_state(
        memory_bridge,
        true,  /* is_sleeping */
        1.0f   /* SWS stage */
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Run some updates in sleep mode */
    for (int i = 0; i < 10; i++) {
        omni_wm_memory_bridge_update(memory_bridge, DT);
    }

    /* Exit sleep */
    ret = omni_wm_memory_bridge_set_sleep_state(
        memory_bridge,
        false, /* awake */
        0.0f   /* awake stage */
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(WMMemoryCognitiveTest, ConsolidationSync) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!memory_bridge) GTEST_SKIP();

    /* Enter sleep */
    omni_wm_memory_bridge_set_sleep_state(memory_bridge, true, 1.0f);

    /* Sync with consolidation signal */
    nimcp_error_t ret = omni_wm_memory_bridge_consolidation_sync(
        memory_bridge,
        0.8f  /* consolidation signal */
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Get stats */
    omni_wm_memory_bridge_stats_t stats;
    omni_wm_memory_bridge_get_stats(memory_bridge, &stats);
    EXPECT_GE(stats.consolidation_cycles, 1u);

    /* Exit sleep */
    omni_wm_memory_bridge_set_sleep_state(memory_bridge, false, 0.0f);
}

TEST_F(WMMemoryCognitiveTest, SemanticExtraction) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!memory_bridge) GTEST_SKIP();

    /* Extract semantic features */
    float features[STATE_DIM];
    nimcp_error_t ret = omni_wm_memory_bridge_extract_semantics(
        memory_bridge,
        features,
        STATE_DIM
    );
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Get stats */
    omni_wm_memory_bridge_stats_t stats;
    omni_wm_memory_bridge_get_stats(memory_bridge, &stats);
    EXPECT_GE(stats.semantic_transfers, 1u);
}

/* ============================================================================
 * Combined Memory-Cognitive Pipeline Tests
 * ============================================================================ */

TEST_F(WMMemoryCognitiveTest, FullPipelineTest) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!memory_bridge || !cognitive_bridge) GTEST_SKIP();

    /* 1. Register a goal in cognitive bridge */
    float target_state[STATE_DIM];
    generate_random_pattern(target_state, STATE_DIM);
    normalize_pattern(target_state, STATE_DIM);

    uint32_t goal_id;
    omni_wm_cognitive_bridge_register_goal(
        cognitive_bridge,
        target_state,
        STATE_DIM,
        0.9f,
        0,
        "Full pipeline goal",
        &goal_id
    );

    /* 2. Set attention focus */
    omni_wm_cognitive_bridge_set_attention_focus(
        cognitive_bridge,
        target_state,
        STATE_DIM,
        0.9f,
        0.2f
    );

    /* 3. Simulate experience and train from replay */
    const uint32_t SEQ_LEN = 8;
    float** states = (float**)nimcp_malloc(SEQ_LEN * sizeof(float*));
    float** actions = (float**)nimcp_malloc(SEQ_LEN * sizeof(float*));
    float* rewards = (float*)nimcp_malloc(SEQ_LEN * sizeof(float));

    for (uint32_t i = 0; i < SEQ_LEN; i++) {
        states[i] = (float*)nimcp_malloc(STATE_DIM * sizeof(float));
        actions[i] = (float*)nimcp_malloc(ACTION_DIM * sizeof(float));
        generate_random_pattern(states[i], STATE_DIM);
        generate_random_pattern(actions[i], ACTION_DIM);
        rewards[i] = (float)i / SEQ_LEN;
    }

    omni_wm_memory_bridge_train_from_replay(
        memory_bridge,
        (const float**)states,
        (const float**)actions,
        rewards,
        SEQ_LEN,
        false
    );

    /* 4. Encode engrams from significant states */
    for (int i = 0; i < 3; i++) {
        uint64_t engram_id;
        omni_wm_memory_bridge_encode_engram(
            memory_bridge,
            0.5f + (float)i * 0.2f,
            true,
            &engram_id
        );
    }

    /* 5. Run integrated update cycle */
    for (int i = 0; i < 50; i++) {
        omni_wm_memory_bridge_update(memory_bridge, DT);
        omni_wm_cognitive_bridge_update(cognitive_bridge, DT);

        /* Update goal progress */
        float progress = (float)(i + 1) / 50.0f;
        omni_wm_cognitive_bridge_update_goal_progress(cognitive_bridge, goal_id, progress);
    }

    /* 6. Verify cross-bridge effects */
    const omni_wm_to_cognitive_effects_t* cog_effects =
        omni_wm_cognitive_bridge_get_wm_effects(cognitive_bridge);
    const memory_to_omni_wm_effects_t* mem_effects =
        omni_wm_memory_bridge_get_memory_effects(memory_bridge);

    EXPECT_NE(cog_effects, nullptr);
    EXPECT_NE(mem_effects, nullptr);

    /* 7. Complete goal */
    omni_wm_cognitive_bridge_goal_achieved(cognitive_bridge, goal_id);

    /* Get final stats */
    omni_wm_memory_bridge_stats_t mem_stats;
    omni_wm_cognitive_bridge_stats_t cog_stats;
    omni_wm_memory_bridge_get_stats(memory_bridge, &mem_stats);
    omni_wm_cognitive_bridge_get_stats(cognitive_bridge, &cog_stats);

    EXPECT_GT(mem_stats.total_updates, 0u);
    EXPECT_GT(cog_stats.total_updates, 0u);
    EXPECT_GE(cog_stats.goals_achieved, 1u);

    /* Clean up */
    for (uint32_t i = 0; i < SEQ_LEN; i++) {
        nimcp_free(states[i]);
        nimcp_free(actions[i]);
    }
    nimcp_free(states);
    nimcp_free(actions);
    nimcp_free(rewards);
}

/* ============================================================================
 * Error Propagation Tests
 * ============================================================================ */

TEST_F(WMMemoryCognitiveTest, NullPointerHandling) {
    /* Memory bridge null handling */
    EXPECT_NE(omni_wm_memory_bridge_update(nullptr, DT), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_memory_bridge_reset(nullptr), NIMCP_SUCCESS);

    /* Cognitive bridge null handling */
    EXPECT_NE(omni_wm_cognitive_bridge_update(nullptr, DT), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_cognitive_bridge_reset(nullptr), NIMCP_SUCCESS);
}

TEST_F(WMMemoryCognitiveTest, InvalidGoalIdHandling) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!cognitive_bridge) GTEST_SKIP();

    /* Try to update non-existent goal */
    nimcp_error_t ret = omni_wm_cognitive_bridge_update_goal_progress(
        cognitive_bridge,
        999999,  /* invalid ID */
        0.5f
    );
    EXPECT_NE(ret, NIMCP_SUCCESS);

    /* Get non-existent goal */
    const wm_cognitive_goal_t* goal = omni_wm_cognitive_bridge_get_goal(
        cognitive_bridge,
        999999
    );
    EXPECT_EQ(goal, nullptr);
}

/* ============================================================================
 * Logging Bridge Verification
 * ============================================================================ */

TEST_F(WMMemoryCognitiveTest, LoggingBridgeCapture) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!logging_bridge || !cognitive_bridge) GTEST_SKIP();

    /* Run operations that should be logged */
    for (int i = 0; i < 10; i++) {
        omni_wm_cognitive_bridge_update(cognitive_bridge, DT);
        omni_wm_logging_bridge_update(logging_bridge, DT);
    }

    /* Get logging stats */
    omni_wm_logging_bridge_stats_t log_stats;
    nimcp_error_t ret = omni_wm_logging_bridge_get_stats(logging_bridge, &log_stats);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(log_stats.total_updates, 0u);
}

/* ============================================================================
 * Stress Test
 * ============================================================================ */

TEST_F(WMMemoryCognitiveTest, StressTestPipeline) {
    if (!wm) GTEST_SKIP();

    CreateBridges();
    if (!memory_bridge || !cognitive_bridge) GTEST_SKIP();

    const int NUM_ITERATIONS = 500;
    int successful_iterations = 0;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        bool success = true;

        /* Update both bridges */
        if (omni_wm_memory_bridge_update(memory_bridge, DT) != NIMCP_SUCCESS) {
            success = false;
        }
        if (omni_wm_cognitive_bridge_update(cognitive_bridge, DT) != NIMCP_SUCCESS) {
            success = false;
        }

        /* Periodically register and complete goals */
        if (i % 50 == 0) {
            float target[STATE_DIM];
            generate_random_pattern(target, STATE_DIM);
            uint32_t goal_id;
            if (omni_wm_cognitive_bridge_register_goal(
                    cognitive_bridge, target, STATE_DIM, 0.5f, 0, "Stress goal", &goal_id
                ) == NIMCP_SUCCESS) {
                omni_wm_cognitive_bridge_goal_achieved(cognitive_bridge, goal_id);
            }
        }

        /* Periodically encode engrams */
        if (i % 25 == 0) {
            uint64_t engram_id;
            omni_wm_memory_bridge_encode_engram(memory_bridge, 0.5f, true, &engram_id);
        }

        if (success) successful_iterations++;
    }

    EXPECT_EQ(successful_iterations, NUM_ITERATIONS);
}
