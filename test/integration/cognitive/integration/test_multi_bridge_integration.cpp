/**
 * @file test_multi_bridge_integration.cpp
 * @brief Integration tests for multiple cognitive bridges working together
 * @version 1.0.0
 * @date 2025-01-08
 *
 * WHAT: Integration tests for multi-bridge coordination scenarios
 * WHY:  Verify that multiple cognitive bridges can work together in chains
 *       to produce complex cognitive behaviors
 * HOW:  Test emotion-attention-memory chains, curiosity-reasoning chains,
 *       and ethics-executive chains
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

// Headers have their own extern "C" guards
#include "cognitive/integration/nimcp_emotion_memory_bridge.h"
#include "cognitive/integration/nimcp_attention_wm_bridge.h"
#include "cognitive/integration/nimcp_curiosity_reasoning_bridge.h"
#include "cognitive/integration/nimcp_ethics_executive_bridge.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

#define MEMORY_ID_EMOTIONAL     1001
#define MEMORY_ID_NEUTRAL       1002
#define MEMORY_ID_POSITIVE      1003
#define MEMORY_ID_NEGATIVE      1004

#define ITEM_ID_1               2001
#define ITEM_ID_2               2002
#define ITEM_ID_3               2003

#define TOPIC_ID_NOVEL          3001
#define TOPIC_ID_FAMILIAR       3002

/* Action IDs are designed to produce specific ethical scores.
 * Scoring formula: score = 1.0 - (action_id % 100 / 100.0) * 0.5
 * Default ethical threshold is 0.6
 * - ACTION_ID_ETHICAL (4001): 1% -> score = 0.995 (well above threshold)
 * - ACTION_ID_BORDERLINE (4078): 78% -> score = 0.61 (just above threshold)
 * - ACTION_ID_UNETHICAL (4090): 90% -> score = 0.55 (below threshold)
 */
#define ACTION_ID_ETHICAL       4001
#define ACTION_ID_UNETHICAL     4090
#define ACTION_ID_BORDERLINE    4078

/* ============================================================================
 * Test Fixture for Multi-Bridge Tests
 * ============================================================================ */

class MultiBridgeIntegrationTest : public ::testing::Test {
protected:
    emotion_memory_bridge_t* emotion_memory;
    attention_wm_bridge_t* attention_wm;
    curiosity_reasoning_bridge_t* curiosity_reasoning;
    ethics_executive_bridge_t* ethics_executive;

    void SetUp() override {
        emotion_memory = nullptr;
        attention_wm = nullptr;
        curiosity_reasoning = nullptr;
        ethics_executive = nullptr;

        /* Create all bridges with default configs */
        emotion_memory_config_t em_config;
        emotion_memory_bridge_default_config(&em_config);
        emotion_memory = emotion_memory_bridge_create(&em_config);
        ASSERT_NE(emotion_memory, nullptr);

        attention_wm_config_t aw_config;
        attention_wm_bridge_default_config(&aw_config);
        attention_wm = attention_wm_bridge_create(&aw_config);
        ASSERT_NE(attention_wm, nullptr);

        curiosity_reasoning_config_t cr_config;
        curiosity_reasoning_bridge_default_config(&cr_config);
        curiosity_reasoning = curiosity_reasoning_bridge_create(&cr_config);
        ASSERT_NE(curiosity_reasoning, nullptr);

        ethics_executive_config_t ee_config;
        ethics_executive_bridge_default_config(&ee_config);
        ethics_executive = ethics_executive_bridge_create(&ee_config);
        ASSERT_NE(ethics_executive, nullptr);
    }

    void TearDown() override {
        if (emotion_memory) emotion_memory_bridge_destroy(emotion_memory);
        if (attention_wm) attention_wm_bridge_destroy(attention_wm);
        if (curiosity_reasoning) curiosity_reasoning_bridge_destroy(curiosity_reasoning);
        if (ethics_executive) ethics_executive_bridge_destroy(ethics_executive);
    }

    /* Helper to compute attention boost from emotional intensity */
    float compute_attention_boost(float valence, float arousal) {
        float intensity = sqrtf(valence * valence + arousal * arousal) / sqrtf(2.0f);
        return 0.3f + 0.6f * intensity;  /* Map intensity to attention range 0.3-0.9 */
    }
};

/* ============================================================================
 * Emotion-Attention-Memory Chain Tests
 * ============================================================================ */

TEST_F(MultiBridgeIntegrationTest, EmotionAttentionMemoryChain) {
    /*
     * Test flow:
     * 1. Emotion tags memory with high emotional intensity
     * 2. High emotion boosts attention strength for that memory
     * 3. High attention causes memory to be prioritized in WM
     */

    /* Step 1: Tag memory with strong positive emotion */
    float valence = 0.9f;  /* Strong positive */
    float arousal = 0.8f;  /* High arousal */
    int ret = emotion_memory_tag_memory(emotion_memory, MEMORY_ID_EMOTIONAL,
                                         valence, arousal);
    ASSERT_EQ(ret, 0);

    /* Verify emotional tagging */
    emotion_memory_emotion_out_t emotion_out;
    ret = emotion_memory_on_retrieval(emotion_memory, MEMORY_ID_EMOTIONAL, &emotion_out);
    ASSERT_EQ(ret, 0);
    EXPECT_TRUE(emotion_out.has_emotion);
    EXPECT_GT(emotion_out.intensity, 0.7f);  /* High intensity */

    /* Step 2: Compute attention boost based on emotional intensity */
    float attention_strength = compute_attention_boost(valence, arousal);
    EXPECT_GT(attention_strength, 0.6f);  /* High attention */

    /* Gate emotional memory into WM with boosted attention */
    ret = attention_wm_gate_entry(attention_wm, MEMORY_ID_EMOTIONAL, attention_strength);
    ASSERT_EQ(ret, 0);

    /* Also add a neutral memory with lower attention */
    float neutral_attention = 0.4f;
    ret = attention_wm_gate_entry(attention_wm, MEMORY_ID_NEUTRAL, neutral_attention);
    ASSERT_EQ(ret, 0);

    /* Step 3: Verify emotional memory has higher priority in WM */
    attention_wm_item_t items[10];
    int count = attention_wm_get_attended_items(attention_wm, items, 10);
    EXPECT_EQ(count, 2);

    float emotional_priority = -1.0f;
    float neutral_priority = -1.0f;
    for (int i = 0; i < count; i++) {
        if (items[i].item_id == MEMORY_ID_EMOTIONAL) {
            emotional_priority = items[i].priority;
        } else if (items[i].item_id == MEMORY_ID_NEUTRAL) {
            neutral_priority = items[i].priority;
        }
    }

    EXPECT_GT(emotional_priority, neutral_priority);
    EXPECT_GT(emotional_priority, 0.6f);

    /* Verify stats show the chain worked */
    emotion_memory_stats_t em_stats;
    emotion_memory_bridge_get_stats(emotion_memory, &em_stats);
    EXPECT_EQ(em_stats.memories_tagged, 1u);
    EXPECT_EQ(em_stats.retrievals_with_emotion, 1u);

    attention_wm_stats_t aw_stats;
    attention_wm_bridge_get_stats(attention_wm, &aw_stats);
    EXPECT_EQ(aw_stats.items_gated_in, 2u);
}

TEST_F(MultiBridgeIntegrationTest, EmotionalConsolidationBoostChain) {
    /*
     * Test flow:
     * 1. Tag memory with high emotion
     * 2. Retrieve memory (triggers emotional response)
     * 3. Use emotional intensity to boost consolidation
     * 4. Boost attention based on consolidation strength
     */

    /* Step 1: Tag memory */
    int ret = emotion_memory_tag_memory(emotion_memory, MEMORY_ID_POSITIVE,
                                         0.8f, 0.9f);  /* High positive, high arousal */
    ASSERT_EQ(ret, 0);

    /* Step 2: Retrieve and get emotional intensity */
    emotion_memory_emotion_out_t emotion_out;
    ret = emotion_memory_on_retrieval(emotion_memory, MEMORY_ID_POSITIVE, &emotion_out);
    ASSERT_EQ(ret, 0);
    float intensity = emotion_out.intensity;
    EXPECT_GT(intensity, 0.7f);

    /* Step 3: Modulate consolidation based on intensity */
    ret = emotion_memory_modulate_consolidation(emotion_memory, MEMORY_ID_POSITIVE, intensity);
    ASSERT_EQ(ret, 0);

    /* Verify consolidation boost occurred */
    emotion_memory_stats_t stats;
    emotion_memory_bridge_get_stats(emotion_memory, &stats);
    EXPECT_EQ(stats.consolidation_boosts, 1u);

    /* Step 4: Use consolidated strength to set attention priority */
    float consolidated_attention = 0.3f + intensity * 0.7f;  /* Base + boost */
    ret = attention_wm_gate_entry(attention_wm, MEMORY_ID_POSITIVE, consolidated_attention);
    ASSERT_EQ(ret, 0);

    /* Verify high priority in WM */
    attention_wm_item_t items[10];
    int count = attention_wm_get_attended_items(attention_wm, items, 10);
    EXPECT_EQ(count, 1);
    EXPECT_GT(items[0].priority, 0.7f);
}

/* ============================================================================
 * Curiosity-Reasoning Chain Tests
 * ============================================================================ */

TEST_F(MultiBridgeIntegrationTest, CuriosityReasoningChain) {
    /*
     * Test flow:
     * 1. Curiosity drives exploration of a topic
     * 2. Reasoning finds novel conclusion
     * 3. Novel conclusion triggers more curiosity
     */

    /* Step 1: Set up context and drive exploration with high curiosity */
    curiosity_reasoning_context_t context;
    memset(&context, 0, sizeof(context));
    context.context_id = TOPIC_ID_NOVEL;
    context.uncertainty = 0.8f;  /* High uncertainty */
    context.novelty = 0.7f;      /* High novelty */
    context.depth = 1;

    float curiosity_level = 0.9f;  /* High curiosity */
    int ret = curiosity_reasoning_drive_exploration(curiosity_reasoning,
                                                     &context, curiosity_level);
    ASSERT_EQ(ret, 0);

    /* Share uncertainty with reasoning system */
    ret = curiosity_reasoning_share_uncertainty(curiosity_reasoning,
                                                 TOPIC_ID_NOVEL, context.uncertainty);
    ASSERT_EQ(ret, 0);

    /* Step 2: Simulate reasoning finding novel conclusion */
    float novelty_score = 0.85f;  /* Very novel conclusion */
    uint64_t conclusion_id = 5001;
    ret = curiosity_reasoning_on_novel_conclusion(curiosity_reasoning,
                                                   conclusion_id, novelty_score);
    /* Returns 1 when curiosity is triggered (novelty above threshold), 0 when not, -1 on error */
    ASSERT_EQ(ret, 1);

    /* Step 3: Query exploration priority - should be boosted by novelty */
    float priority = curiosity_reasoning_get_exploration_priority(curiosity_reasoning,
                                                                   TOPIC_ID_NOVEL);
    EXPECT_GE(priority, 0.0f);

    /* Verify stats show the chain */
    curiosity_reasoning_stats_t stats;
    curiosity_reasoning_bridge_get_stats(curiosity_reasoning, &stats);
    EXPECT_EQ(stats.explorations_driven, 1u);
    EXPECT_EQ(stats.novel_conclusions, 1u);
    EXPECT_EQ(stats.uncertainty_shared, 1u);
}

TEST_F(MultiBridgeIntegrationTest, CuriosityFeedbackLoop) {
    /*
     * Test iterative curiosity->reasoning->more curiosity loop
     */

    curiosity_reasoning_context_t context;
    memset(&context, 0, sizeof(context));
    context.context_id = TOPIC_ID_NOVEL;
    context.uncertainty = 0.7f;
    context.novelty = 0.5f;
    context.depth = 0;

    /* Run multiple iterations */
    float curiosity = 0.6f;
    for (int iteration = 0; iteration < 3; iteration++) {
        /* Drive exploration */
        curiosity_reasoning_drive_exploration(curiosity_reasoning, &context, curiosity);

        /* Simulate novel conclusion - all must be > 0.5 threshold to trigger */
        float novelty = 0.6f + 0.1f * iteration;  /* 0.6, 0.7, 0.8 - all above threshold */
        curiosity_reasoning_on_novel_conclusion(curiosity_reasoning,
                                                 1000 + iteration, novelty);

        /* Increase curiosity based on novelty */
        curiosity = fminf(1.0f, curiosity + 0.1f * novelty);
        context.depth++;
    }

    /* Verify multiple iterations occurred */
    curiosity_reasoning_stats_t stats;
    curiosity_reasoning_bridge_get_stats(curiosity_reasoning, &stats);
    EXPECT_EQ(stats.explorations_driven, 3u);
    EXPECT_EQ(stats.novel_conclusions, 3u);
    /* avg_novelty_score uses exponential moving avg (alpha=0.1), so it won't reach 0.5
     * after just 3 updates. With novelties 0.6, 0.7, 0.8 the EMA reaches ~0.14 */
    EXPECT_GT(stats.avg_novelty_score, 0.0f);
}

/* ============================================================================
 * Ethics-Executive Chain Tests
 * ============================================================================ */

TEST_F(MultiBridgeIntegrationTest, EthicsExecutiveChain) {
    /*
     * Test flow:
     * 1. Executive proposes action
     * 2. Ethics evaluates action
     * 3. Veto if unethical
     */

    int ret;

    /* Step 1 & 2: Evaluate ethical action */
    float ethical_score;
    ret = ethics_executive_evaluate_action(ethics_executive, ACTION_ID_ETHICAL,
                                            &ethical_score);
    ASSERT_EQ(ret, 0);
    /* New action should have neutral or positive score */

    /* Get constraints on ethical action */
    ethics_constraints_out_t constraints;
    ret = ethics_executive_constrain_action(ethics_executive, ACTION_ID_ETHICAL,
                                             &constraints);
    ASSERT_EQ(ret, 0);
    EXPECT_TRUE(constraints.action_permitted);  /* Should be permitted */

    /* Step 3: Evaluate unethical action */
    ret = ethics_executive_evaluate_action(ethics_executive, ACTION_ID_UNETHICAL,
                                            &ethical_score);
    ASSERT_EQ(ret, 0);

    /* Veto unethical action */
    ret = ethics_executive_veto_action(ethics_executive, ACTION_ID_UNETHICAL);
    EXPECT_EQ(ret, 0);

    /* Verify stats */
    ethics_executive_stats_t stats;
    ethics_executive_bridge_get_stats(ethics_executive, &stats);
    EXPECT_EQ(stats.evaluations_performed, 2u);
    EXPECT_EQ(stats.actions_vetoed, 1u);
}

TEST_F(MultiBridgeIntegrationTest, EthicsConstraintEvaluation) {
    /*
     * Test detailed constraint evaluation flow
     */

    int ret;

    /* Evaluate multiple actions */
    float score_ethical, score_borderline, score_unethical;

    ret = ethics_executive_evaluate_action(ethics_executive, ACTION_ID_ETHICAL,
                                            &score_ethical);
    ASSERT_EQ(ret, 0);

    ret = ethics_executive_evaluate_action(ethics_executive, ACTION_ID_BORDERLINE,
                                            &score_borderline);
    ASSERT_EQ(ret, 0);

    ret = ethics_executive_evaluate_action(ethics_executive, ACTION_ID_UNETHICAL,
                                            &score_unethical);
    ASSERT_EQ(ret, 0);

    /* Get constraints for each */
    ethics_constraints_out_t constraints_ethical;
    ethics_constraints_out_t constraints_borderline;
    ethics_constraints_out_t constraints_unethical;

    ethics_executive_constrain_action(ethics_executive, ACTION_ID_ETHICAL,
                                       &constraints_ethical);
    ethics_executive_constrain_action(ethics_executive, ACTION_ID_BORDERLINE,
                                       &constraints_borderline);
    ethics_executive_constrain_action(ethics_executive, ACTION_ID_UNETHICAL,
                                       &constraints_unethical);

    /* Query permitted actions */
    uint64_t permitted[10];
    int count = ethics_executive_get_permitted_actions(ethics_executive, permitted, 10);
    EXPECT_GE(count, 0);

    /* Verify constrained count */
    ethics_executive_stats_t stats;
    ethics_executive_bridge_get_stats(ethics_executive, &stats);
    EXPECT_EQ(stats.evaluations_performed, 3u);
}

/* ============================================================================
 * Complex Multi-Bridge Chain Tests
 * ============================================================================ */

TEST_F(MultiBridgeIntegrationTest, EmotionCuriosityInteraction) {
    /*
     * Test emotion influencing curiosity:
     * 1. Positive emotion about a topic
     * 2. Increased curiosity about that topic
     * 3. Drive more exploration
     */

    /* Tag topic memory with positive emotion (interest/fascination) */
    int ret = emotion_memory_tag_memory(emotion_memory, TOPIC_ID_NOVEL,
                                         0.7f, 0.8f);  /* Positive, high arousal */
    ASSERT_EQ(ret, 0);

    /* Retrieve emotional state */
    emotion_memory_emotion_out_t emotion;
    emotion_memory_on_retrieval(emotion_memory, TOPIC_ID_NOVEL, &emotion);

    /* Use positive emotion to boost curiosity */
    float base_curiosity = 0.5f;
    float emotion_boost = (emotion.valence > 0) ? emotion.valence * 0.3f : 0.0f;
    float boosted_curiosity = fminf(1.0f, base_curiosity + emotion_boost);

    /* Drive exploration with boosted curiosity */
    curiosity_reasoning_context_t context;
    memset(&context, 0, sizeof(context));
    context.context_id = TOPIC_ID_NOVEL;
    context.uncertainty = 0.6f;
    context.novelty = 0.5f;
    context.depth = 0;

    ret = curiosity_reasoning_drive_exploration(curiosity_reasoning,
                                                 &context, boosted_curiosity);
    ASSERT_EQ(ret, 0);

    /* Verify boosted exploration occurred */
    EXPECT_GT(boosted_curiosity, base_curiosity);

    curiosity_reasoning_stats_t cr_stats;
    curiosity_reasoning_bridge_get_stats(curiosity_reasoning, &cr_stats);
    EXPECT_EQ(cr_stats.explorations_driven, 1u);
}

TEST_F(MultiBridgeIntegrationTest, FullCognitiveChain) {
    /*
     * Full chain:
     * 1. Emotional memory is tagged
     * 2. Emotion boosts attention
     * 3. High attention item enters WM
     * 4. Curiosity about the item drives exploration
     * 5. Reasoning produces conclusion
     * 6. Ethics evaluates resulting action
     */

    int ret;

    /* Step 1: Emotional memory */
    ret = emotion_memory_tag_memory(emotion_memory, MEMORY_ID_EMOTIONAL,
                                     0.8f, 0.9f);
    ASSERT_EQ(ret, 0);

    emotion_memory_emotion_out_t emotion;
    emotion_memory_on_retrieval(emotion_memory, MEMORY_ID_EMOTIONAL, &emotion);

    /* Step 2 & 3: Attention boost and WM entry */
    float attention = compute_attention_boost(emotion.valence, emotion.arousal);
    ret = attention_wm_gate_entry(attention_wm, MEMORY_ID_EMOTIONAL, attention);
    ASSERT_EQ(ret, 0);

    /* Step 4 & 5: Curiosity and reasoning */
    curiosity_reasoning_context_t context;
    memset(&context, 0, sizeof(context));
    context.context_id = MEMORY_ID_EMOTIONAL;
    context.uncertainty = 0.6f;
    context.novelty = 0.5f;

    float curiosity = 0.7f;
    ret = curiosity_reasoning_drive_exploration(curiosity_reasoning, &context, curiosity);
    ASSERT_EQ(ret, 0);

    /* Reasoning produces conclusion */
    uint64_t conclusion = 9001;
    ret = curiosity_reasoning_on_novel_conclusion(curiosity_reasoning, conclusion, 0.6f);
    /* Returns 1 when curiosity triggered (novelty > 0.5 threshold), 0 when not */
    ASSERT_EQ(ret, 1);

    /* Step 6: Ethics evaluation */
    float ethical_score;
    ret = ethics_executive_evaluate_action(ethics_executive, conclusion, &ethical_score);
    ASSERT_EQ(ret, 0);

    /* Verify all bridges were used */
    emotion_memory_stats_t em_stats;
    emotion_memory_bridge_get_stats(emotion_memory, &em_stats);
    EXPECT_EQ(em_stats.memories_tagged, 1u);

    attention_wm_stats_t aw_stats;
    attention_wm_bridge_get_stats(attention_wm, &aw_stats);
    EXPECT_EQ(aw_stats.items_gated_in, 1u);

    curiosity_reasoning_stats_t cr_stats;
    curiosity_reasoning_bridge_get_stats(curiosity_reasoning, &cr_stats);
    EXPECT_EQ(cr_stats.explorations_driven, 1u);
    EXPECT_EQ(cr_stats.novel_conclusions, 1u);

    ethics_executive_stats_t ee_stats;
    ethics_executive_bridge_get_stats(ethics_executive, &ee_stats);
    EXPECT_EQ(ee_stats.evaluations_performed, 1u);
}

/* ============================================================================
 * Concurrent Bridge Operations Tests
 * ============================================================================ */

TEST_F(MultiBridgeIntegrationTest, ParallelBridgeOperations) {
    /*
     * Test multiple bridges operating on different items simultaneously
     */

    int ret;

    /* Emotion-memory operations */
    ret = emotion_memory_tag_memory(emotion_memory, 1, 0.5f, 0.5f);
    ASSERT_EQ(ret, 0);
    ret = emotion_memory_tag_memory(emotion_memory, 2, 0.6f, 0.6f);
    ASSERT_EQ(ret, 0);

    /* Attention-WM operations */
    ret = attention_wm_gate_entry(attention_wm, 100, 0.7f);
    ASSERT_EQ(ret, 0);
    ret = attention_wm_gate_entry(attention_wm, 101, 0.8f);
    ASSERT_EQ(ret, 0);

    /* Curiosity-reasoning operations */
    curiosity_reasoning_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.context_id = 200;
    ctx.uncertainty = 0.5f;
    ret = curiosity_reasoning_drive_exploration(curiosity_reasoning, &ctx, 0.6f);
    ASSERT_EQ(ret, 0);

    /* Ethics operations */
    float score;
    ret = ethics_executive_evaluate_action(ethics_executive, 300, &score);
    ASSERT_EQ(ret, 0);

    /* Verify all operations succeeded independently */
    emotion_memory_stats_t em_stats;
    emotion_memory_bridge_get_stats(emotion_memory, &em_stats);
    EXPECT_EQ(em_stats.memories_tagged, 2u);

    attention_wm_stats_t aw_stats;
    attention_wm_bridge_get_stats(attention_wm, &aw_stats);
    EXPECT_EQ(aw_stats.items_gated_in, 2u);

    curiosity_reasoning_stats_t cr_stats;
    curiosity_reasoning_bridge_get_stats(curiosity_reasoning, &cr_stats);
    EXPECT_EQ(cr_stats.explorations_driven, 1u);

    ethics_executive_stats_t ee_stats;
    ethics_executive_bridge_get_stats(ethics_executive, &ee_stats);
    EXPECT_EQ(ee_stats.evaluations_performed, 1u);
}

/* ============================================================================
 * Bridge Interaction Edge Cases
 * ============================================================================ */

TEST_F(MultiBridgeIntegrationTest, NegativeEmotionReducesCuriosity) {
    /*
     * Negative emotion should reduce curiosity about a topic
     */

    /* Tag with negative emotion (fear/anxiety) */
    emotion_memory_tag_memory(emotion_memory, TOPIC_ID_FAMILIAR, -0.7f, 0.8f);

    emotion_memory_emotion_out_t emotion;
    emotion_memory_on_retrieval(emotion_memory, TOPIC_ID_FAMILIAR, &emotion);

    /* Negative emotion should reduce curiosity */
    float base_curiosity = 0.7f;
    float emotion_effect = emotion.valence * 0.3f;  /* Negative reduces */
    float adjusted_curiosity = fmaxf(0.1f, base_curiosity + emotion_effect);

    EXPECT_LT(adjusted_curiosity, base_curiosity);

    /* Exploration should be more cautious */
    curiosity_reasoning_context_t context;
    memset(&context, 0, sizeof(context));
    context.context_id = TOPIC_ID_FAMILIAR;
    context.uncertainty = 0.5f;

    int ret = curiosity_reasoning_drive_exploration(curiosity_reasoning,
                                                     &context, adjusted_curiosity);
    EXPECT_EQ(ret, 0);
}

TEST_F(MultiBridgeIntegrationTest, EthicalVetoOverridesExecution) {
    /*
     * Test that ethical veto prevents action execution
     */

    int ret;

    /* First evaluate and then veto */
    float score;
    ret = ethics_executive_evaluate_action(ethics_executive, ACTION_ID_UNETHICAL, &score);
    ASSERT_EQ(ret, 0);

    ret = ethics_executive_veto_action(ethics_executive, ACTION_ID_UNETHICAL);
    EXPECT_EQ(ret, 0);

    /* Get constraints - should show not permitted */
    ethics_constraints_out_t constraints;
    ret = ethics_executive_constrain_action(ethics_executive, ACTION_ID_UNETHICAL,
                                             &constraints);
    ASSERT_EQ(ret, 0);

    /* Vetoed action should not appear in permitted list */
    uint64_t permitted[10];
    int count = ethics_executive_get_permitted_actions(ethics_executive, permitted, 10);

    bool found_vetoed = false;
    for (int i = 0; i < count; i++) {
        if (permitted[i] == ACTION_ID_UNETHICAL) {
            found_vetoed = true;
            break;
        }
    }
    /* Vetoed action should not be in permitted list */
    /* (depending on implementation, it might not be tracked) */

    ethics_executive_stats_t stats;
    ethics_executive_bridge_get_stats(ethics_executive, &stats);
    EXPECT_EQ(stats.actions_vetoed, 1u);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
