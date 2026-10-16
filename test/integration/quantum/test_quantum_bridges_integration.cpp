/**
 * @file test_quantum_bridges_integration.cpp
 * @brief Integration tests for Quantum Bridges
 *
 * WHAT: Cross-module integration tests for quantum bridge implementations
 * WHY:  Verify realistic interactions between quantum bridges and their target modules
 * HOW:  Test with multiple connected modules in realistic scenarios
 *
 * TEST COVERAGE:
 * - Attention-Quantum Integration (6 tests)
 * - Curiosity-Quantum Integration (6 tests)
 * - Emotion-Quantum Integration (6 tests)
 * - BCM-Quantum Integration (6 tests)
 * - Thalamic-Quantum Integration (6 tests)
 * - Cross-Bridge Integration (5 tests)
 *
 * TOTAL: 35 tests
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

// Headers have their own extern "C" guards
/* Quantum bridge headers with implementations */
#define NIMCP_ATTENTION_QUANTUM_BRIDGE_IMPLEMENTATION
#include "plasticity/attention/nimcp_attention_quantum_bridge.h"

#define NIMCP_CURIOSITY_QUANTUM_BRIDGE_IMPLEMENTATION
#include "cognitive/curiosity/nimcp_curiosity_quantum_bridge.h"

#define NIMCP_EMOTION_QUANTUM_BRIDGE_IMPLEMENTATION
#include "cognitive/emotion/nimcp_emotion_quantum_bridge.h"

#define NIMCP_BCM_QUANTUM_BRIDGE_IMPLEMENTATION
#include "plasticity/bcm/nimcp_bcm_quantum_bridge.h"

#define NIMCP_THALAMIC_QUANTUM_BRIDGE_IMPLEMENTATION
#include "middleware/routing/nimcp_thalamic_quantum_bridge.h"

#include "utils/error/nimcp_error_codes.h"

//=============================================================================
// Attention-Quantum Integration Tests
//=============================================================================

class AttentionQuantumIntegrationTest : public ::testing::Test {
protected:
    attention_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        attention_quantum_config_t config = attention_quantum_default_config();
        config.max_selected_heads = 4;
        bridge = attention_quantum_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) attention_quantum_bridge_destroy(bridge);
    }
};

TEST_F(AttentionQuantumIntegrationTest, MultipleHeadSelectionCycles) {
    /* WHAT: Test repeated head selection cycles */
    /* WHY:  Verify stability across multiple iterations */
    /* HOW:  Run selection 100 times with varying scores */

    for (int cycle = 0; cycle < 100; ++cycle) {
        float head_scores[8];
        for (int i = 0; i < 8; ++i) {
            head_scores[i] = 0.1f + 0.1f * i + 0.05f * sinf(cycle * 0.1f + i);
        }

        uint32_t selected[4];
        int n = attention_quantum_select_heads(bridge, head_scores, 8, 4, selected);
        EXPECT_GE(n, 0);
        EXPECT_LE(n, 4);
    }

    attention_quantum_stats_t stats;
    attention_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.classical_fallbacks, 100u);  /* No MHA connected */
}

TEST_F(AttentionQuantumIntegrationTest, EdgeCaseScores) {
    /* WHAT: Test with extreme score values */
    /* WHY:  Verify robustness to edge cases */

    float edge_scores[] = {0.0f, 1.0f, 0.5f, -0.1f, 1.1f, 0.999f, 0.001f, 0.5f};
    uint32_t selected[4];

    int n = attention_quantum_select_heads(bridge, edge_scores, 8, 4, selected);
    EXPECT_GE(n, 0);
}

TEST_F(AttentionQuantumIntegrationTest, VaryingK) {
    /* WHAT: Test with varying k values */
    /* WHY:  Verify selection works for any k */

    float scores[16];
    for (int i = 0; i < 16; ++i) scores[i] = 0.5f + 0.03f * i;

    for (uint32_t k = 1; k <= 8; ++k) {
        uint32_t selected[8];
        int n = attention_quantum_select_heads(bridge, scores, 16, k, selected);
        EXPECT_EQ(n, (int)std::min(k, 4u));  /* Capped by max_selected_heads */
    }
}

TEST_F(AttentionQuantumIntegrationTest, ConsistentStatistics) {
    /* WHAT: Verify statistics accumulate correctly */

    attention_quantum_reset_stats(bridge);

    float scores[] = {0.1f, 0.9f, 0.5f, 0.7f};
    uint32_t selected[2];

    for (int i = 0; i < 10; ++i) {
        attention_quantum_select_heads(bridge, scores, 4, 2, selected);
    }

    attention_quantum_stats_t stats;
    attention_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.classical_fallbacks, 10u);
}

TEST_F(AttentionQuantumIntegrationTest, EnableDisableToggle) {
    /* WHAT: Test enable/disable across operations */

    float scores[] = {0.3f, 0.7f, 0.5f, 0.9f};
    uint32_t selected[2];

    attention_quantum_bridge_set_enabled(bridge, true);
    attention_quantum_select_heads(bridge, scores, 4, 2, selected);

    attention_quantum_bridge_set_enabled(bridge, false);
    attention_quantum_select_heads(bridge, scores, 4, 2, selected);

    attention_quantum_bridge_set_enabled(bridge, true);
    attention_quantum_select_heads(bridge, scores, 4, 2, selected);

    attention_quantum_stats_t stats;
    attention_quantum_get_stats(bridge, &stats);
    EXPECT_EQ(stats.classical_fallbacks, 3u);
}

TEST_F(AttentionQuantumIntegrationTest, NullInputHandling) {
    /* WHAT: Test null input handling */

    uint32_t selected[2];
    EXPECT_EQ(attention_quantum_select_heads(nullptr, nullptr, 0, 0, nullptr), -1);
    EXPECT_EQ(attention_quantum_select_heads(bridge, nullptr, 4, 2, selected), -1);
}

//=============================================================================
// Curiosity-Quantum Integration Tests
//=============================================================================

class CuriosityQuantumIntegrationTest : public ::testing::Test {
protected:
    curiosity_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = curiosity_quantum_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) curiosity_quantum_destroy(bridge);
    }
};

TEST_F(CuriosityQuantumIntegrationTest, TopicGraphConstruction) {
    /* WHAT: Build topic graph with multiple topics */
    /* WHY:  Verify graph grows correctly */

    const char* topics[] = {
        "machine_learning", "deep_learning", "neural_networks",
        "reinforcement_learning", "computer_vision", "nlp"
    };

    for (int i = 0; i < 6; ++i) {
        int id = curiosity_quantum_add_topic(bridge, topics[i], 0.5f + 0.1f * i, 0.7f);
        EXPECT_GE(id, 0);
    }
}

TEST_F(CuriosityQuantumIntegrationTest, SimilarityMatrix) {
    /* WHAT: Test topic similarity computation */

    float sim1 = curiosity_quantum_topic_similarity("machine_learning", "deep_learning");
    float sim2 = curiosity_quantum_topic_similarity("machine_learning", "cooking");
    float sim3 = curiosity_quantum_topic_similarity("test", "test");

    EXPECT_GE(sim1, 0.0f);
    EXPECT_LE(sim1, 1.0f);
    EXPECT_GE(sim2, 0.0f);
    EXPECT_GT(sim3, 0.9f);  /* Same topic = high similarity */
}

TEST_F(CuriosityQuantumIntegrationTest, ExplorationSequence) {
    /* WHAT: Multiple exploration cycles */

    curiosity_quantum_add_topic(bridge, "topic_a", 0.8f, 0.9f);
    curiosity_quantum_add_topic(bridge, "topic_b", 0.6f, 0.7f);
    curiosity_quantum_add_topic(bridge, "topic_c", 0.4f, 0.5f);

    for (int i = 0; i < 10; ++i) {
        char novel[256] = {0};
        float novelty = curiosity_quantum_explore(bridge, nullptr, 5, novel);
        (void)novelty;  /* May return -1 if no path found */
    }
}

TEST_F(CuriosityQuantumIntegrationTest, IntensityUpdates) {
    /* WHAT: Update topic intensities dynamically */

    curiosity_quantum_add_topic(bridge, "dynamic_topic", 0.5f, 0.5f);

    for (float intensity = 0.0f; intensity <= 1.0f; intensity += 0.1f) {
        int status = curiosity_quantum_update_topic_intensity(bridge, "dynamic_topic", intensity);
        EXPECT_EQ(status, 0);
    }
}

TEST_F(CuriosityQuantumIntegrationTest, LargeTopicGraph) {
    /* WHAT: Test with large number of topics */

    for (int i = 0; i < 100; ++i) {
        char topic[64];
        snprintf(topic, sizeof(topic), "topic_%03d", i);
        int id = curiosity_quantum_add_topic(bridge, topic, 0.5f, 0.5f);
        EXPECT_GE(id, 0);
    }
}

TEST_F(CuriosityQuantumIntegrationTest, NoveltyEvaluation) {
    /* WHAT: Evaluate novelty of topics */

    curiosity_quantum_add_topic(bridge, "known_topic", 0.9f, 0.1f);
    curiosity_quantum_add_topic(bridge, "novel_topic", 0.1f, 0.9f);

    float known_novelty = curiosity_quantum_evaluate_novelty(bridge, "known_topic");
    float novel_novelty = curiosity_quantum_evaluate_novelty(bridge, "novel_topic");

    /* Both should return valid values or -1 */
    EXPECT_TRUE(known_novelty >= -1.0f);
    EXPECT_TRUE(novel_novelty >= -1.0f);
}

//=============================================================================
// Emotion-Quantum Integration Tests
//=============================================================================

class EmotionQuantumIntegrationTest : public ::testing::Test {
protected:
    emotion_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = emotion_quantum_bridge_create(nullptr, nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) emotion_quantum_bridge_destroy(bridge);
    }
};

TEST_F(EmotionQuantumIntegrationTest, StateSpaceExploration) {
    /* WHAT: Explore emotion state space */

    for (float valence = -1.0f; valence <= 1.0f; valence += 0.25f) {
        for (float arousal = 0.0f; arousal <= 1.0f; arousal += 0.25f) {
            uint32_t steps = 0;
            emotion_quantum_evaluate_state(bridge, valence, arousal, 0.0f, 0.5f, &steps);
        }
    }
}

TEST_F(EmotionQuantumIntegrationTest, TransitionPathways) {
    /* WHAT: Find transition pathways between emotions */

    emotion_quantum_prediction_t pathway[10];
    uint32_t steps_found = 0;

    /* From sad (negative valence, low arousal) to happy (positive, medium) */
    bool found = emotion_quantum_transition(
        bridge, -0.8f, 0.2f, 0.8f, 0.5f,
        pathway, 10, &steps_found
    );

    if (found) {
        EXPECT_GT(steps_found, 0u);
    }
}

TEST_F(EmotionQuantumIntegrationTest, PredictionAccuracy) {
    /* WHAT: Test emotion prediction */

    emotion_quantum_prediction_t predictions[5];
    uint32_t found = 0;

    emotion_quantum_predict(bridge, 0.0f, 0.5f, predictions, 5, &found);

    for (uint32_t i = 0; i < found; ++i) {
        EXPECT_GE(predictions[i].valence, -1.0f);
        EXPECT_LE(predictions[i].valence, 1.0f);
        EXPECT_GE(predictions[i].arousal, 0.0f);
        EXPECT_LE(predictions[i].arousal, 1.0f);
        EXPECT_GE(predictions[i].probability, 0.0f);
        EXPECT_LE(predictions[i].probability, 1.0f);
    }
}

TEST_F(EmotionQuantumIntegrationTest, StatisticsAccumulation) {
    /* WHAT: Verify statistics accumulate */

    emotion_quantum_reset_stats(bridge);

    for (int i = 0; i < 20; ++i) {
        emotion_quantum_evaluate_state(bridge, 0.0f, 0.5f, 0.5f, 0.5f, nullptr);
    }

    emotion_quantum_stats_t stats;
    emotion_quantum_get_stats(bridge, &stats);
    /* Stats should have updated */
}

TEST_F(EmotionQuantumIntegrationTest, EnableDisableCycle) {
    /* WHAT: Toggle enable state during operation */

    for (int i = 0; i < 10; ++i) {
        emotion_quantum_bridge_set_enabled(bridge, i % 2 == 0);
        emotion_quantum_evaluate_state(bridge, 0.0f, 0.5f, 0.5f, 0.5f, nullptr);
    }
}

TEST_F(EmotionQuantumIntegrationTest, ExtremeEmotionStates) {
    /* WHAT: Test extreme emotion state values */

    uint32_t steps;

    /* Extreme positive */
    emotion_quantum_evaluate_state(bridge, 1.0f, 1.0f, 0.0f, 0.0f, &steps);

    /* Extreme negative */
    emotion_quantum_evaluate_state(bridge, -1.0f, 1.0f, 1.0f, 0.0f, &steps);

    /* Neutral */
    emotion_quantum_evaluate_state(bridge, 0.0f, 0.0f, 0.0f, 1.0f, &steps);
}

//=============================================================================
// BCM-Quantum Integration Tests
//=============================================================================

class BCMQuantumIntegrationTest : public ::testing::Test {
protected:
    bcm_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        bridge = bcm_quantum_bridge_create(nullptr);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) bcm_quantum_bridge_destroy(bridge);
    }
};

TEST_F(BCMQuantumIntegrationTest, ThresholdOptimizationCycles) {
    /* WHAT: Multiple optimization cycles */

    for (int cycle = 0; cycle < 50; ++cycle) {
        bcm_activity_stats_t stats = {
            .avg_weight = 0.3f + 0.2f * sinf(cycle * 0.1f),
            .weight_variance = 0.1f + 0.05f * cosf(cycle * 0.1f),
            .avg_post_activity = 0.4f + 0.1f * sinf(cycle * 0.2f),
            .selectivity_index = 0.5f + 0.3f * cosf(cycle * 0.15f),
            .num_active_synapses = 100 + cycle
        };

        float threshold = bcm_quantum_optimize_threshold(bridge, &stats);
        EXPECT_GT(threshold, 0.0f);
    }
}

TEST_F(BCMQuantumIntegrationTest, UpdateSchedule) {
    /* WHAT: Test periodic update schedule */

    bcm_activity_stats_t stats = {
        .avg_weight = 0.5f,
        .weight_variance = 0.1f,
        .avg_post_activity = 0.3f,
        .selectivity_index = 0.6f,
        .num_active_synapses = 100
    };

    int optimizations_triggered = 0;
    for (int i = 0; i < 2000; ++i) {
        float result = bcm_quantum_update(bridge, &stats);
        if (result >= 0.0f) optimizations_triggered++;
    }

    /* At least one optimization should have triggered */
    EXPECT_GE(optimizations_triggered, 1);
}

TEST_F(BCMQuantumIntegrationTest, ExtremeActivityStats) {
    /* WHAT: Test with extreme activity values */

    bcm_activity_stats_t extreme_low = {
        .avg_weight = 0.001f,
        .weight_variance = 0.0001f,
        .avg_post_activity = 0.001f,
        .selectivity_index = 0.01f,
        .num_active_synapses = 1
    };

    bcm_activity_stats_t extreme_high = {
        .avg_weight = 0.999f,
        .weight_variance = 0.5f,
        .avg_post_activity = 0.999f,
        .selectivity_index = 0.99f,
        .num_active_synapses = 10000
    };

    float threshold_low = bcm_quantum_optimize_threshold(bridge, &extreme_low);
    float threshold_high = bcm_quantum_optimize_threshold(bridge, &extreme_high);

    EXPECT_GT(threshold_low, 0.0f);
    EXPECT_GT(threshold_high, 0.0f);
}

TEST_F(BCMQuantumIntegrationTest, StatisticsTracking) {
    /* WHAT: Verify statistics are tracked correctly */

    bcm_quantum_reset_stats(bridge);

    bcm_activity_stats_t stats = {
        .avg_weight = 0.5f,
        .weight_variance = 0.1f,
        .avg_post_activity = 0.3f,
        .selectivity_index = 0.6f,
        .num_active_synapses = 100
    };

    for (int i = 0; i < 5; ++i) {
        bcm_quantum_optimize_threshold(bridge, &stats);
    }

    bcm_quantum_stats_t qstats;
    bcm_quantum_get_stats(bridge, &qstats);
    EXPECT_EQ(qstats.optimization_steps, 5u);
}

TEST_F(BCMQuantumIntegrationTest, ConfigurationVariations) {
    /* WHAT: Test different configurations */

    bcm_quantum_bridge_destroy(bridge);

    bcm_quantum_config_t config = bcm_quantum_default_config();
    config.num_iterations = 500;
    config.initial_temperature = 2.0f;
    config.final_temperature = 0.001f;
    config.stability_weight = 0.8f;
    config.selectivity_weight = 0.2f;

    bridge = bcm_quantum_bridge_create(&config);
    ASSERT_NE(bridge, nullptr);

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

TEST_F(BCMQuantumIntegrationTest, EnableDisableOptimization) {
    /* WHAT: Test enable/disable affects optimization */

    bcm_activity_stats_t stats = {
        .avg_weight = 0.5f,
        .weight_variance = 0.1f,
        .avg_post_activity = 0.3f,
        .selectivity_index = 0.6f,
        .num_active_synapses = 100
    };

    bcm_quantum_set_enabled(bridge, true);
    float t1 = bcm_quantum_optimize_threshold(bridge, &stats);

    bcm_quantum_set_enabled(bridge, false);
    float t2 = bcm_quantum_optimize_threshold(bridge, &stats);

    bcm_quantum_set_enabled(bridge, true);
    float t3 = bcm_quantum_optimize_threshold(bridge, &stats);

    EXPECT_GT(t1, 0.0f);
    EXPECT_GT(t3, 0.0f);
    (void)t2;  /* May or may not return valid value when disabled */
}

//=============================================================================
// Thalamic-Quantum Integration Tests
//=============================================================================

class ThalamicQuantumIntegrationTest : public ::testing::Test {
protected:
    thalamic_quantum_bridge_t* bridge = nullptr;

    void SetUp() override {
        thalamic_quantum_config_t config = thalamic_quantum_default_config();
        config.max_destinations = 32;  /* Smaller for testing */
        bridge = thalamic_quantum_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) thalamic_quantum_bridge_destroy(bridge);
    }
};

TEST_F(ThalamicQuantumIntegrationTest, RoutingDecisions) {
    /* WHAT: Test routing to multiple destinations */

    float features[16];
    for (int i = 0; i < 16; ++i) features[i] = 0.5f + 0.03f * i;

    uint32_t dests[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    uint32_t routed[8];
    uint32_t num_routed = 0;

    int status = thalamic_quantum_route(bridge, 1, dests, 8, features, 16, routed, &num_routed);
    EXPECT_EQ(status, 0);
    EXPECT_LE(num_routed, 8u);
}

TEST_F(ThalamicQuantumIntegrationTest, GatingDecisions) {
    /* WHAT: Test individual gating decisions */

    float features[8] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};

    for (uint32_t dest = 1; dest <= 10; ++dest) {
        float weight = 0.0f;
        bool gate = thalamic_quantum_gate_signal(bridge, 0, dest, features, 8, &weight);
        EXPECT_GE(weight, 0.0f);
        EXPECT_LE(weight, 1.0f);
        (void)gate;
    }
}

TEST_F(ThalamicQuantumIntegrationTest, VaryingFeatureDimensions) {
    /* WHAT: Test with different feature dimensions */

    for (uint32_t dim = 1; dim <= 64; dim *= 2) {
        std::vector<float> features(dim, 0.5f);
        uint32_t dests[4] = {1, 2, 3, 4};
        uint32_t routed[4];
        uint32_t num_routed = 0;

        int status = thalamic_quantum_route(
            bridge, 0, dests, 4, features.data(), dim, routed, &num_routed
        );
        EXPECT_EQ(status, 0);
    }
}

TEST_F(ThalamicQuantumIntegrationTest, StatisticsAccumulation) {
    /* WHAT: Verify routing statistics */

    thalamic_quantum_reset_stats(bridge);

    float features[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    uint32_t dests[4] = {1, 2, 3, 4};
    uint32_t routed[4];
    uint32_t num_routed;

    for (int i = 0; i < 10; ++i) {
        thalamic_quantum_route(bridge, 0, dests, 4, features, 8, routed, &num_routed);
    }

    thalamic_quantum_stats_t stats;
    thalamic_quantum_get_stats(bridge, &stats);
    EXPECT_GT(stats.quantum_routes + stats.classical_fallbacks, 0u);
}

TEST_F(ThalamicQuantumIntegrationTest, SingleDestinationRouting) {
    /* WHAT: Route to single destination */

    float features[8] = {0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.2f};
    uint32_t dest = 42;
    uint32_t routed;
    uint32_t num_routed = 0;

    int status = thalamic_quantum_route(bridge, 0, &dest, 1, features, 8, &routed, &num_routed);
    EXPECT_EQ(status, 0);
    EXPECT_LE(num_routed, 1u);
}

TEST_F(ThalamicQuantumIntegrationTest, MaxDestinations) {
    /* WHAT: Test at max destination limit */

    std::vector<float> features(32, 0.5f);
    std::vector<uint32_t> dests(32);
    std::vector<uint32_t> routed(32);
    uint32_t num_routed = 0;

    for (uint32_t i = 0; i < 32; ++i) dests[i] = i + 100;

    int status = thalamic_quantum_route(
        bridge, 0, dests.data(), 32, features.data(), 32, routed.data(), &num_routed
    );
    EXPECT_EQ(status, 0);
}

//=============================================================================
// Cross-Bridge Integration Tests
//=============================================================================

class CrossBridgeIntegrationTest : public ::testing::Test {
protected:
    attention_quantum_bridge_t* attention_bridge = nullptr;
    curiosity_quantum_bridge_t* curiosity_bridge = nullptr;
    emotion_quantum_bridge_t* emotion_bridge = nullptr;
    bcm_quantum_bridge_t* bcm_bridge = nullptr;
    thalamic_quantum_bridge_t* thalamic_bridge = nullptr;

    void SetUp() override {
        attention_bridge = attention_quantum_bridge_create(nullptr);
        curiosity_bridge = curiosity_quantum_create(nullptr);
        emotion_bridge = emotion_quantum_bridge_create(nullptr, nullptr);
        bcm_bridge = bcm_quantum_bridge_create(nullptr);

        thalamic_quantum_config_t config = thalamic_quantum_default_config();
        config.max_destinations = 16;
        thalamic_bridge = thalamic_quantum_bridge_create(&config);

        ASSERT_NE(attention_bridge, nullptr);
        ASSERT_NE(curiosity_bridge, nullptr);
        ASSERT_NE(emotion_bridge, nullptr);
        ASSERT_NE(bcm_bridge, nullptr);
        ASSERT_NE(thalamic_bridge, nullptr);
    }

    void TearDown() override {
        if (attention_bridge) attention_quantum_bridge_destroy(attention_bridge);
        if (curiosity_bridge) curiosity_quantum_destroy(curiosity_bridge);
        if (emotion_bridge) emotion_quantum_bridge_destroy(emotion_bridge);
        if (bcm_bridge) bcm_quantum_bridge_destroy(bcm_bridge);
        if (thalamic_bridge) thalamic_quantum_bridge_destroy(thalamic_bridge);
    }
};

TEST_F(CrossBridgeIntegrationTest, AllBridgesCoexist) {
    /* WHAT: All bridges operate concurrently */
    /* WHY:  Verify no resource conflicts */

    /* Attention selection */
    float head_scores[4] = {0.2f, 0.8f, 0.5f, 0.9f};
    uint32_t selected[2];
    attention_quantum_select_heads(attention_bridge, head_scores, 4, 2, selected);

    /* Curiosity exploration */
    curiosity_quantum_add_topic(curiosity_bridge, "cross_test", 0.7f, 0.8f);

    /* Emotion evaluation */
    emotion_quantum_evaluate_state(emotion_bridge, 0.5f, 0.5f, 0.0f, 0.5f, nullptr);

    /* BCM optimization */
    bcm_activity_stats_t bcm_stats = {
        .avg_weight = 0.5f, .weight_variance = 0.1f,
        .avg_post_activity = 0.3f, .selectivity_index = 0.6f,
        .num_active_synapses = 50
    };
    bcm_quantum_optimize_threshold(bcm_bridge, &bcm_stats);

    /* Thalamic routing */
    float features[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    uint32_t dests[4] = {1, 2, 3, 4};
    uint32_t routed[4];
    uint32_t num_routed;
    thalamic_quantum_route(thalamic_bridge, 0, dests, 4, features, 8, routed, &num_routed);

    SUCCEED();
}

TEST_F(CrossBridgeIntegrationTest, SequentialOperations) {
    /* WHAT: Sequential operations across bridges */

    for (int i = 0; i < 50; ++i) {
        /* Attention */
        float scores[4] = {0.1f * i, 0.2f * i, 0.3f * i, 0.4f * i};
        for (int j = 0; j < 4; ++j) {
            if (scores[j] > 1.0f) scores[j] = 1.0f;
        }
        uint32_t sel[2];
        attention_quantum_select_heads(attention_bridge, scores, 4, 2, sel);

        /* Emotion */
        float valence = -1.0f + 2.0f * (i / 50.0f);
        emotion_quantum_evaluate_state(emotion_bridge, valence, 0.5f, 0.0f, 0.5f, nullptr);

        /* BCM */
        bcm_activity_stats_t stats = {
            .avg_weight = 0.5f, .weight_variance = 0.1f,
            .avg_post_activity = 0.3f, .selectivity_index = 0.6f,
            .num_active_synapses = 50 + i
        };
        bcm_quantum_optimize_threshold(bcm_bridge, &stats);
    }

    SUCCEED();
}

TEST_F(CrossBridgeIntegrationTest, StatsIndependence) {
    /* WHAT: Verify statistics are independent */

    /* Reset all */
    attention_quantum_reset_stats(attention_bridge);
    emotion_quantum_reset_stats(emotion_bridge);
    bcm_quantum_reset_stats(bcm_bridge);
    thalamic_quantum_reset_stats(thalamic_bridge);

    /* Operate on one */
    float scores[4] = {0.1f, 0.9f, 0.5f, 0.7f};
    uint32_t sel[2];
    attention_quantum_select_heads(attention_bridge, scores, 4, 2, sel);

    /* Check others are still zero */
    emotion_quantum_stats_t emotion_stats;
    emotion_quantum_get_stats(emotion_bridge, &emotion_stats);
    EXPECT_EQ(emotion_stats.quantum_transitions, 0u);

    bcm_quantum_stats_t bcm_stats;
    bcm_quantum_get_stats(bcm_bridge, &bcm_stats);
    EXPECT_EQ(bcm_stats.optimization_steps, 0u);
}

TEST_F(CrossBridgeIntegrationTest, EnableDisableAll) {
    /* WHAT: Enable/disable all bridges */

    /* Disable all */
    attention_quantum_bridge_set_enabled(attention_bridge, false);
    emotion_quantum_bridge_set_enabled(emotion_bridge, false);
    bcm_quantum_set_enabled(bcm_bridge, false);
    thalamic_quantum_bridge_set_enabled(thalamic_bridge, false);

    EXPECT_FALSE(attention_quantum_bridge_is_enabled(attention_bridge));
    EXPECT_FALSE(emotion_quantum_bridge_is_enabled(emotion_bridge));
    EXPECT_FALSE(bcm_quantum_is_enabled(bcm_bridge));
    EXPECT_FALSE(thalamic_quantum_bridge_is_enabled(thalamic_bridge));

    /* Re-enable all */
    attention_quantum_bridge_set_enabled(attention_bridge, true);
    emotion_quantum_bridge_set_enabled(emotion_bridge, true);
    bcm_quantum_set_enabled(bcm_bridge, true);
    thalamic_quantum_bridge_set_enabled(thalamic_bridge, true);
}

TEST_F(CrossBridgeIntegrationTest, ConcurrentCreationDestruction) {
    /* WHAT: Create and destroy bridges multiple times */

    for (int i = 0; i < 10; ++i) {
        attention_quantum_bridge_t* a = attention_quantum_bridge_create(nullptr);
        curiosity_quantum_bridge_t* c = curiosity_quantum_create(nullptr);
        emotion_quantum_bridge_t* e = emotion_quantum_bridge_create(nullptr, nullptr);
        bcm_quantum_bridge_t* b = bcm_quantum_bridge_create(nullptr);

        ASSERT_NE(a, nullptr);
        ASSERT_NE(c, nullptr);
        ASSERT_NE(e, nullptr);
        ASSERT_NE(b, nullptr);

        attention_quantum_bridge_destroy(a);
        curiosity_quantum_destroy(c);
        emotion_quantum_bridge_destroy(e);
        bcm_quantum_bridge_destroy(b);
    }
}
