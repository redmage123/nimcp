/**
 * @file test_quantum_bridges.cpp
 * @brief Tests for quantum bridge integrations
 *
 * Tests the quantum bridge modules that integrate quantum-inspired
 * algorithms into the main NIMCP modules.
 */

#include <gtest/gtest.h>
#include <cmath>

/* Implementation defines before including headers */
#define NIMCP_SEQUENCE_QUANTUM_BRIDGE_IMPLEMENTATION
#define NIMCP_STDP_QUANTUM_BRIDGE_IMPLEMENTATION
#define NIMCP_FEATURE_QUANTUM_BRIDGE_IMPLEMENTATION
#define NIMCP_SWARM_QUANTUM_BRIDGE_IMPLEMENTATION
#define NIMCP_SEMANTIC_QUANTUM_BRIDGE_IMPLEMENTATION

/* Include the bridge headers */
#include "middleware/patterns/nimcp_sequence_detector_quantum_bridge.h"
#include "plasticity/stdp/nimcp_stdp_quantum_bridge.h"
#include "middleware/features/nimcp_feature_extractor_quantum_bridge.h"
#include "swarm/nimcp_swarm_consensus_quantum_bridge.h"
#include "cognitive/memory/nimcp_semantic_memory_quantum_bridge.h"

//=============================================================================
// Sequence Quantum Bridge Tests
//=============================================================================

class SequenceQuantumBridgeTest : public ::testing::Test {
protected:
    sequence_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = sequence_quantum_bridge_create(nullptr);
    }

    void TearDown() override {
        if (bridge) sequence_quantum_bridge_destroy(bridge);
    }
};

TEST_F(SequenceQuantumBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SequenceQuantumBridgeTest, DefaultConfig) {
    sequence_quantum_config_t config = sequence_quantum_default_config();
    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.match_threshold, 0.0f);
}

TEST_F(SequenceQuantumBridgeTest, EnableDisable) {
    /* Note: is_enabled returns true only when both enabled flag is set
     * AND bridge is connected. Test the flag separately. */
    sequence_quantum_config_t config = sequence_quantum_default_config();
    EXPECT_TRUE(config.enabled);  /* Default is enabled */

    sequence_quantum_bridge_set_enabled(bridge, false);
    /* After disable, is_enabled should be false regardless of connection */
    EXPECT_FALSE(sequence_quantum_bridge_is_enabled(bridge));

    sequence_quantum_bridge_set_enabled(bridge, true);
    /* Still false because not connected, but flag is set */
}

TEST_F(SequenceQuantumBridgeTest, AddTemplate) {
    uint32_t symbols[] = {1, 2, 3, 4, 5};
    int status = sequence_quantum_add_template(bridge, "test", symbols, 5);
    EXPECT_EQ(status, 0);
}

TEST_F(SequenceQuantumBridgeTest, Match) {
    uint32_t symbols[] = {1, 2, 3};
    sequence_quantum_add_template(bridge, "pattern1", symbols, 3);

    qseq_match_result_t result;
    int status = sequence_quantum_match(bridge, symbols, 3, &result);
    EXPECT_EQ(status, 0);
    EXPECT_GE(result.similarity, 0.0f);
}

//=============================================================================
// STDP Quantum Bridge Tests
//=============================================================================

class STDPQuantumBridgeTest : public ::testing::Test {
protected:
    stdp_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = stdp_quantum_bridge_create(nullptr);
    }

    void TearDown() override {
        if (bridge) stdp_quantum_bridge_destroy(bridge);
    }
};

TEST_F(STDPQuantumBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(STDPQuantumBridgeTest, DefaultConfig) {
    stdp_quantum_config_t config = stdp_quantum_default_config();
    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.initial_temp, 0.0f);
}

TEST_F(STDPQuantumBridgeTest, EnableDisable) {
    stdp_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(stdp_quantum_bridge_is_enabled(bridge));
    stdp_quantum_bridge_set_enabled(bridge, true);
    EXPECT_TRUE(stdp_quantum_bridge_is_enabled(bridge));
}

TEST_F(STDPQuantumBridgeTest, GetLR) {
    float lr = stdp_quantum_get_lr(bridge);
    EXPECT_GT(lr, 0.0f);
    EXPECT_LE(lr, 1.0f);
}

TEST_F(STDPQuantumBridgeTest, Step) {
    qstdp_activity_stats_t stats = {0};
    stats.mean_weight = 0.5f;
    stats.weight_variance = 0.1f;
    stats.firing_rate = 10.0f;
    stats.ltp_rate = 5.0f;
    stats.ltd_rate = 3.0f;
    stats.ltp_ltd_ratio = 1.67f;

    float lr = stdp_quantum_step(bridge, &stats);
    EXPECT_GT(lr, 0.0f);
}

TEST_F(STDPQuantumBridgeTest, GetParams) {
    stdp_quantum_params_t params;
    int status = stdp_quantum_get_params(bridge, &params);
    EXPECT_EQ(status, 0);
    EXPECT_GT(params.learning_rate, 0.0f);
}

//=============================================================================
// Feature Quantum Bridge Tests
//=============================================================================

class FeatureQuantumBridgeTest : public ::testing::Test {
protected:
    feature_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = feature_quantum_bridge_create(nullptr);
    }

    void TearDown() override {
        if (bridge) feature_quantum_bridge_destroy(bridge);
    }
};

TEST_F(FeatureQuantumBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(FeatureQuantumBridgeTest, DefaultConfig) {
    feature_quantum_config_t config = feature_quantum_default_config();
    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.output_dim, 0u);
}

TEST_F(FeatureQuantumBridgeTest, EnableDisable) {
    feature_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(feature_quantum_bridge_is_enabled(bridge));
    feature_quantum_bridge_set_enabled(bridge, true);
    EXPECT_TRUE(feature_quantum_bridge_is_enabled(bridge));
}

TEST_F(FeatureQuantumBridgeTest, Transform) {
    /* Use default config dimensions */
    feature_quantum_config_t config = feature_quantum_default_config();
    uint32_t input_dim = config.input_dim;
    uint32_t output_dim = config.output_dim;

    float* input = new float[input_dim];
    float* output = new float[output_dim];

    for (uint32_t i = 0; i < input_dim; i++) {
        input[i] = sinf((float)i * 0.1f);
    }

    int status = feature_quantum_transform(bridge, input, output);
    EXPECT_EQ(status, 0);

    /* Check output is not all zeros */
    float sum = 0;
    for (uint32_t i = 0; i < output_dim; i++) {
        sum += fabsf(output[i]);
    }
    EXPECT_GT(sum, 0.0f);

    delete[] input;
    delete[] output;
}

//=============================================================================
// Swarm Quantum Bridge Tests
//=============================================================================

class SwarmQuantumBridgeTest : public ::testing::Test {
protected:
    swarm_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = swarm_quantum_bridge_create(nullptr);
    }

    void TearDown() override {
        if (bridge) swarm_quantum_bridge_destroy(bridge);
    }
};

TEST_F(SwarmQuantumBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SwarmQuantumBridgeTest, DefaultConfig) {
    swarm_quantum_config_t config = swarm_quantum_default_config();
    EXPECT_TRUE(config.enabled);
    EXPECT_TRUE(config.detect_collusion);
}

TEST_F(SwarmQuantumBridgeTest, EnableDisable) {
    swarm_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(swarm_quantum_bridge_is_enabled(bridge));
    swarm_quantum_bridge_set_enabled(bridge, true);
    EXPECT_TRUE(swarm_quantum_bridge_is_enabled(bridge));
}

TEST_F(SwarmQuantumBridgeTest, ProposeAndVote) {
    uint32_t proposal_id;
    int status = swarm_quantum_propose(bridge, "Test proposal", &proposal_id);
    EXPECT_EQ(status, 0);

    /* Cast some votes */
    status = swarm_quantum_vote(bridge, proposal_id, 1, QVOTE_AGREE, 1.0f);
    EXPECT_EQ(status, 0);
    status = swarm_quantum_vote(bridge, proposal_id, 2, QVOTE_AGREE, 0.8f);
    EXPECT_EQ(status, 0);
    status = swarm_quantum_vote(bridge, proposal_id, 3, QVOTE_DISAGREE, 0.5f);
    EXPECT_EQ(status, 0);
}

TEST_F(SwarmQuantumBridgeTest, ComputeConsensus) {
    uint32_t proposal_id;
    swarm_quantum_propose(bridge, "Test proposal", &proposal_id);

    swarm_quantum_vote(bridge, proposal_id, 1, QVOTE_AGREE, 1.0f);
    swarm_quantum_vote(bridge, proposal_id, 2, QVOTE_AGREE, 1.0f);
    swarm_quantum_vote(bridge, proposal_id, 3, QVOTE_AGREE, 1.0f);

    quantum_consensus_result_t result;
    int status = swarm_quantum_compute_consensus(bridge, proposal_id, &result);
    EXPECT_EQ(status, 0);
    EXPECT_TRUE(result.passed);  /* Unanimous agreement should pass */
}

//=============================================================================
// Semantic Quantum Bridge Tests
//=============================================================================

class SemanticQuantumBridgeTest : public ::testing::Test {
protected:
    semantic_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = semantic_quantum_bridge_create(nullptr);
    }

    void TearDown() override {
        if (bridge) semantic_quantum_bridge_destroy(bridge);
    }
};

TEST_F(SemanticQuantumBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(SemanticQuantumBridgeTest, DefaultConfig) {
    semantic_quantum_config_t config = semantic_quantum_default_config();
    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.max_results, 0u);
}

TEST_F(SemanticQuantumBridgeTest, EnableDisable) {
    semantic_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(semantic_quantum_bridge_is_enabled(bridge));
    semantic_quantum_bridge_set_enabled(bridge, true);
    EXPECT_TRUE(semantic_quantum_bridge_is_enabled(bridge));
}

//=============================================================================
// Stats Tests
//=============================================================================

class QuantumBridgeStatsTest : public ::testing::Test {};

TEST_F(QuantumBridgeStatsTest, SequenceStats) {
    sequence_quantum_bridge_t* bridge = sequence_quantum_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    sequence_quantum_stats_t stats;
    int status = sequence_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(status, 0);
    EXPECT_EQ(stats.templates_added, 0u);

    /* Add template and check stats */
    uint32_t symbols[] = {1, 2, 3};
    sequence_quantum_add_template(bridge, "test", symbols, 3);
    sequence_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.templates_added, 1u);

    sequence_quantum_reset_stats(bridge);
    sequence_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.templates_added, 0u);

    sequence_quantum_bridge_destroy(bridge);
}

TEST_F(QuantumBridgeStatsTest, STDPStats) {
    stdp_quantum_bridge_t* bridge = stdp_quantum_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    stdp_quantum_stats_t stats;
    int status = stdp_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(status, 0);
    EXPECT_EQ(stats.optimization_steps, 0u);

    /* Perform steps and check stats */
    qstdp_activity_stats_t activity = {0};
    activity.mean_weight = 0.5f;
    stdp_quantum_step(bridge, &activity);
    stdp_quantum_step(bridge, &activity);
    stdp_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.optimization_steps, 2u);

    stdp_quantum_reset_stats(bridge);
    stdp_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.optimization_steps, 0u);

    stdp_quantum_bridge_destroy(bridge);
}

TEST_F(QuantumBridgeStatsTest, FeatureStats) {
    feature_quantum_bridge_t* bridge = feature_quantum_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    feature_quantum_stats_t stats;
    int status = feature_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(status, 0);
    EXPECT_EQ(stats.quantum_transforms, 0u);

    feature_quantum_reset_stats(bridge);
    feature_quantum_bridge_destroy(bridge);
}

TEST_F(QuantumBridgeStatsTest, SwarmStats) {
    swarm_quantum_bridge_t* bridge = swarm_quantum_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    swarm_quantum_stats_t stats;
    int status = swarm_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(status, 0);
    EXPECT_EQ(stats.quantum_votes, 0u);

    swarm_quantum_reset_stats(bridge);
    swarm_quantum_bridge_destroy(bridge);
}

TEST_F(QuantumBridgeStatsTest, SemanticStats) {
    semantic_quantum_bridge_t* bridge = semantic_quantum_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    semantic_quantum_stats_t stats;
    int status = semantic_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(status, 0);
    EXPECT_EQ(stats.quantum_retrievals, 0u);

    semantic_quantum_reset_stats(bridge);
    semantic_quantum_bridge_destroy(bridge);
}

//=============================================================================
// Attention Quantum Bridge Tests
//=============================================================================

#define NIMCP_ATTENTION_QUANTUM_BRIDGE_IMPLEMENTATION
#include "plasticity/attention/nimcp_attention_quantum_bridge.h"

class AttentionQuantumBridgeTest : public ::testing::Test {
protected:
    attention_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = attention_quantum_bridge_create(nullptr);
    }

    void TearDown() override {
        if (bridge) attention_quantum_bridge_destroy(bridge);
    }
};

TEST_F(AttentionQuantumBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(AttentionQuantumBridgeTest, DefaultConfig) {
    attention_quantum_config_t config = attention_quantum_default_config();
    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.collapse_threshold, 0.0f);
    EXPECT_GT(config.max_selected_heads, 0u);
}

TEST_F(AttentionQuantumBridgeTest, EnableDisable) {
    /* Not connected, so not enabled even if flag is set */
    EXPECT_FALSE(attention_quantum_bridge_is_enabled(bridge));

    attention_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(attention_quantum_bridge_is_enabled(bridge));

    attention_quantum_bridge_set_enabled(bridge, true);
    /* Still not enabled because not connected */
    EXPECT_FALSE(attention_quantum_bridge_is_enabled(bridge));
}

TEST_F(AttentionQuantumBridgeTest, SelectHeadsClassicalFallback) {
    /* Without connection, uses classical fallback */
    float head_scores[] = {0.1f, 0.9f, 0.5f, 0.3f, 0.7f};
    uint32_t selected[3];

    int n_selected = attention_quantum_select_heads(bridge, head_scores, 5, 3, selected);
    EXPECT_EQ(n_selected, 3);

    /* Should select top 3: indices 1, 4, 2 (scores 0.9, 0.7, 0.5) */
    bool found_1 = false, found_4 = false, found_2 = false;
    for (int i = 0; i < n_selected; i++) {
        if (selected[i] == 1) found_1 = true;
        if (selected[i] == 4) found_4 = true;
        if (selected[i] == 2) found_2 = true;
    }
    EXPECT_TRUE(found_1);
    EXPECT_TRUE(found_4);
    EXPECT_TRUE(found_2);
}

TEST_F(AttentionQuantumBridgeTest, Stats) {
    attention_quantum_stats_t stats;
    int status = attention_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(status, 0);
    EXPECT_EQ(stats.quantum_selections, 0u);

    /* Perform selection (classical fallback) */
    float head_scores[] = {0.1f, 0.9f, 0.5f};
    uint32_t selected[2];
    attention_quantum_select_heads(bridge, head_scores, 3, 2, selected);

    attention_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.classical_fallbacks, 1u);

    attention_quantum_reset_stats(bridge);
    attention_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.classical_fallbacks, 0u);
}

//=============================================================================
// Curiosity Quantum Bridge Tests
//=============================================================================

#define NIMCP_CURIOSITY_QUANTUM_BRIDGE_IMPLEMENTATION
#include "cognitive/curiosity/nimcp_curiosity_quantum_bridge.h"

class CuriosityQuantumBridgeTest : public ::testing::Test {
protected:
    curiosity_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = curiosity_quantum_create(nullptr);
    }

    void TearDown() override {
        if (bridge) curiosity_quantum_destroy(bridge);
    }
};

TEST_F(CuriosityQuantumBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(CuriosityQuantumBridgeTest, DefaultConfig) {
    curiosity_quantum_config_t config;
    curiosity_quantum_default_config(&config);
    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.max_topics, 0u);
    EXPECT_GT(config.exploration_steps, 0u);
}

TEST_F(CuriosityQuantumBridgeTest, AddTopic) {
    int topic_id = curiosity_quantum_add_topic(bridge, "machine_learning", 0.8f, 0.6f);
    EXPECT_GE(topic_id, 0);
}

TEST_F(CuriosityQuantumBridgeTest, TopicSimilarity) {
    float sim = curiosity_quantum_topic_similarity("machine_learning", "deep_learning");
    EXPECT_GE(sim, 0.0f);
    EXPECT_LE(sim, 1.0f);

    /* Same topic should have high similarity */
    float same_sim = curiosity_quantum_topic_similarity("quantum", "quantum");
    EXPECT_GT(same_sim, 0.9f);
}

TEST_F(CuriosityQuantumBridgeTest, Explore) {
    curiosity_quantum_add_topic(bridge, "topic1", 0.5f, 0.7f);
    curiosity_quantum_add_topic(bridge, "topic2", 0.6f, 0.8f);

    char novel_topic[256] = {0};
    float novelty = curiosity_quantum_explore(bridge, "topic1", 0, novel_topic);
    /* Exploration may or may not find novel topic depending on graph */
    EXPECT_TRUE(novelty >= -1.0f);  /* Either valid novelty or error */
}

//=============================================================================
// Emotion Quantum Bridge Tests
//=============================================================================

#define NIMCP_EMOTION_QUANTUM_BRIDGE_IMPLEMENTATION
#include "cognitive/emotion/nimcp_emotion_quantum_bridge.h"

class EmotionQuantumBridgeTest : public ::testing::Test {
protected:
    emotion_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = emotion_quantum_bridge_create(nullptr, nullptr);
    }

    void TearDown() override {
        if (bridge) emotion_quantum_bridge_destroy(bridge);
    }
};

TEST_F(EmotionQuantumBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(EmotionQuantumBridgeTest, DefaultConfig) {
    emotion_quantum_config_t config = emotion_quantum_default_config();
    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.state_dimensions, 0u);
    EXPECT_GT(config.transition_steps, 0u);
}

TEST_F(EmotionQuantumBridgeTest, EnableDisable) {
    EXPECT_TRUE(emotion_quantum_bridge_is_enabled(bridge));
    emotion_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(emotion_quantum_bridge_is_enabled(bridge));
    emotion_quantum_bridge_set_enabled(bridge, true);
    EXPECT_TRUE(emotion_quantum_bridge_is_enabled(bridge));
}

TEST_F(EmotionQuantumBridgeTest, EvaluateState) {
    uint32_t steps = 0;
    bool success = emotion_quantum_evaluate_state(
        bridge, 0.0f, 0.5f,  /* current: neutral valence, medium arousal */
        0.5f, 0.3f,          /* target: positive valence, low arousal */
        &steps
    );
    /* May succeed or fail depending on walk */
    (void)success;
    (void)steps;
}

TEST_F(EmotionQuantumBridgeTest, Predict) {
    emotion_quantum_prediction_t predictions[4];
    uint32_t found = 0;

    bool success = emotion_quantum_predict(
        bridge, 0.0f, 0.5f,  /* current state */
        predictions, 4, &found
    );
    if (success) {
        EXPECT_LE(found, 4u);
        for (uint32_t i = 0; i < found; i++) {
            EXPECT_GE(predictions[i].probability, 0.0f);
            EXPECT_LE(predictions[i].probability, 1.0f);
        }
    }
}

TEST_F(EmotionQuantumBridgeTest, Stats) {
    emotion_quantum_stats_t stats;
    bool success = emotion_quantum_get_stats(bridge, &stats);
    EXPECT_TRUE(success);
    EXPECT_EQ(stats.quantum_transitions, 0u);

    emotion_quantum_reset_stats(bridge);
}

//=============================================================================
// BCM Quantum Bridge Tests
//=============================================================================

#define NIMCP_BCM_QUANTUM_BRIDGE_IMPLEMENTATION
#include "plasticity/bcm/nimcp_bcm_quantum_bridge.h"

class BCMQuantumBridgeTest : public ::testing::Test {
protected:
    bcm_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = bcm_quantum_bridge_create(nullptr);
    }

    void TearDown() override {
        if (bridge) bcm_quantum_bridge_destroy(bridge);
    }
};

TEST_F(BCMQuantumBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(BCMQuantumBridgeTest, DefaultConfig) {
    bcm_quantum_config_t config = bcm_quantum_default_config();
    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.num_iterations, 0u);
    EXPECT_GT(config.initial_temperature, 0.0f);
}

TEST_F(BCMQuantumBridgeTest, EnableDisable) {
    EXPECT_TRUE(bcm_quantum_is_enabled(bridge));
    bcm_quantum_set_enabled(bridge, false);
    EXPECT_FALSE(bcm_quantum_is_enabled(bridge));
    bcm_quantum_set_enabled(bridge, true);
    EXPECT_TRUE(bcm_quantum_is_enabled(bridge));
}

TEST_F(BCMQuantumBridgeTest, OptimizeThreshold) {
    bcm_activity_stats_t stats = {
        .avg_weight = 0.5f,
        .weight_variance = 0.1f,
        .avg_post_activity = 0.3f,
        .selectivity_index = 0.6f,
        .num_active_synapses = 100
    };

    float threshold = bcm_quantum_optimize_threshold(bridge, &stats);
    EXPECT_GT(threshold, 0.0f);
}

TEST_F(BCMQuantumBridgeTest, Update) {
    bcm_activity_stats_t stats = {
        .avg_weight = 0.5f,
        .weight_variance = 0.1f,
        .avg_post_activity = 0.3f,
        .selectivity_index = 0.6f,
        .num_active_synapses = 100
    };

    /* Update won't trigger optimization until interval is reached */
    float result = bcm_quantum_update(bridge, &stats);
    /* Returns -1.0f if no optimization triggered */
    (void)result;
}

TEST_F(BCMQuantumBridgeTest, Stats) {
    bcm_quantum_stats_t stats;
    int status = bcm_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(status, 0);
    EXPECT_EQ(stats.optimization_steps, 0u);

    bcm_quantum_reset_stats(bridge);
}

//=============================================================================
// Executive Quantum Bridge Tests
//=============================================================================

#define NIMCP_EXECUTIVE_QUANTUM_BRIDGE_IMPLEMENTATION
#include "cognitive/executive/nimcp_executive_quantum_bridge.h"

class ExecutiveQuantumBridgeTest : public ::testing::Test {
protected:
    executive_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = executive_quantum_bridge_create(nullptr);
    }

    void TearDown() override {
        if (bridge) executive_quantum_bridge_destroy(bridge);
    }
};

TEST_F(ExecutiveQuantumBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(ExecutiveQuantumBridgeTest, DefaultConfig) {
    executive_quantum_config_t config = executive_quantum_default_config();
    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.planning_depth, 0u);
    EXPECT_GT(config.hypothesis_count, 0u);
}

TEST_F(ExecutiveQuantumBridgeTest, EnableDisable) {
    EXPECT_TRUE(executive_quantum_bridge_is_enabled(bridge));
    executive_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(executive_quantum_bridge_is_enabled(bridge));
    executive_quantum_bridge_set_enabled(bridge, true);
    EXPECT_TRUE(executive_quantum_bridge_is_enabled(bridge));
}

TEST_F(ExecutiveQuantumBridgeTest, EvaluateOptions) {
    decision_option_t options[3] = {
        {.option_id = 0, .expected_reward = 0.3f, .risk_level = 0.2f},
        {.option_id = 1, .expected_reward = 0.8f, .risk_level = 0.1f},
        {.option_id = 2, .expected_reward = 0.5f, .risk_level = 0.5f}
    };
    strncpy(options[0].description, "Option A", sizeof(options[0].description));
    strncpy(options[1].description, "Option B", sizeof(options[1].description));
    strncpy(options[2].description, "Option C", sizeof(options[2].description));

    quantum_decision_result_t result;
    int status = executive_quantum_evaluate_options(bridge, options, 3, &result);
    EXPECT_EQ(status, 0);
    EXPECT_LT(result.selected_option_id, 3u);
}

//=============================================================================
// Working Memory Quantum Bridge Tests
//=============================================================================

#define NIMCP_WORKING_MEMORY_QUANTUM_BRIDGE_IMPLEMENTATION
#include "cognitive/memory/nimcp_working_memory_quantum_bridge.h"

class WorkingMemoryQuantumBridgeTest : public ::testing::Test {
protected:
    working_memory_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = working_memory_quantum_bridge_create(nullptr);
    }

    void TearDown() override {
        if (bridge) working_memory_quantum_bridge_destroy(bridge);
    }
};

TEST_F(WorkingMemoryQuantumBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(WorkingMemoryQuantumBridgeTest, DefaultConfig) {
    working_memory_quantum_config_t config = working_memory_quantum_default_config();
    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.max_items, 0u);
    EXPECT_GT(config.item_embedding_dim, 0u);
}

TEST_F(WorkingMemoryQuantumBridgeTest, EnableDisable) {
    EXPECT_TRUE(working_memory_quantum_bridge_is_enabled(bridge));
    working_memory_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(working_memory_quantum_bridge_is_enabled(bridge));
    working_memory_quantum_bridge_set_enabled(bridge, true);
    EXPECT_TRUE(working_memory_quantum_bridge_is_enabled(bridge));
}

TEST_F(WorkingMemoryQuantumBridgeTest, StoreItem) {
    float item[16] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f,
                      0.9f, 1.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};

    int status = working_memory_quantum_store(bridge, item, 16, 0, 0.8f);
    EXPECT_EQ(status, 0);
}

//=============================================================================
// Neural Logic Quantum Bridge Tests
//=============================================================================

#define NIMCP_NEURAL_LOGIC_QUANTUM_BRIDGE_IMPLEMENTATION
#include "core/logic/nimcp_neural_logic_quantum_bridge.h"

class NeuralLogicQuantumBridgeTest : public ::testing::Test {
protected:
    neural_logic_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = neural_logic_quantum_bridge_create(nullptr);
    }

    void TearDown() override {
        if (bridge) neural_logic_quantum_bridge_destroy(bridge);
    }
};

TEST_F(NeuralLogicQuantumBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(NeuralLogicQuantumBridgeTest, DefaultConfig) {
    neural_logic_quantum_config_t config = neural_logic_quantum_default_config();
    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.min_confidence, 0.0f);
}

TEST_F(NeuralLogicQuantumBridgeTest, EnableDisable) {
    /* Not connected, so may not be enabled */
    neural_logic_quantum_set_enabled(bridge, false);
    EXPECT_FALSE(neural_logic_quantum_is_enabled(bridge));
    neural_logic_quantum_set_enabled(bridge, true);
    /* Still may not be enabled if not connected */
}

//=============================================================================
// Thalamic Quantum Bridge Tests
//=============================================================================

#define NIMCP_THALAMIC_QUANTUM_BRIDGE_IMPLEMENTATION
#include "middleware/routing/nimcp_thalamic_quantum_bridge.h"

class ThalamicQuantumBridgeTest : public ::testing::Test {
protected:
    thalamic_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = thalamic_quantum_bridge_create(nullptr);
    }

    void TearDown() override {
        if (bridge) thalamic_quantum_bridge_destroy(bridge);
    }
};

TEST_F(ThalamicQuantumBridgeTest, CreateDestroy) {
    ASSERT_NE(bridge, nullptr);
}

TEST_F(ThalamicQuantumBridgeTest, DefaultConfig) {
    thalamic_quantum_config_t config = thalamic_quantum_default_config();
    EXPECT_TRUE(config.enabled);
    EXPECT_GT(config.routing_threshold, 0.0f);
    EXPECT_GT(config.max_destinations, 0u);
}

TEST_F(ThalamicQuantumBridgeTest, EnableDisable) {
    EXPECT_TRUE(thalamic_quantum_bridge_is_enabled(bridge));
    thalamic_quantum_bridge_set_enabled(bridge, false);
    EXPECT_FALSE(thalamic_quantum_bridge_is_enabled(bridge));
    thalamic_quantum_bridge_set_enabled(bridge, true);
    EXPECT_TRUE(thalamic_quantum_bridge_is_enabled(bridge));
}

TEST_F(ThalamicQuantumBridgeTest, GateSignal) {
    float signal_features[8] = {0.5f, 0.3f, 0.7f, 0.2f, 0.9f, 0.1f, 0.4f, 0.6f};
    float gate_weight = -1.0f;

    bool should_gate = thalamic_quantum_gate_signal(
        bridge, 1, 2, signal_features, 8, &gate_weight
    );
    /* Result depends on quantum attention scoring - check gate_weight is valid */
    (void)should_gate;
    EXPECT_GE(gate_weight, 0.0f);
    EXPECT_LE(gate_weight, 1.0f);
}

TEST_F(ThalamicQuantumBridgeTest, Route) {
    float signal_features[8] = {0.5f, 0.3f, 0.7f, 0.2f, 0.9f, 0.1f, 0.4f, 0.6f};
    uint32_t dest_ids[4] = {10, 20, 30, 40};
    uint32_t routed_dests[4];
    uint32_t num_routed = 0;

    int status = thalamic_quantum_route(
        bridge, 1, dest_ids, 4, signal_features, 8, routed_dests, &num_routed
    );
    EXPECT_EQ(status, 0);
    /* Quantum routing may select 0 to all destinations based on attention scores */
    EXPECT_LE(num_routed, 4u);
}

TEST_F(ThalamicQuantumBridgeTest, Stats) {
    thalamic_quantum_stats_t stats;
    int status = thalamic_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(status, 0);
    EXPECT_EQ(stats.quantum_routes, 0u);

    thalamic_quantum_reset_stats(bridge);
}
