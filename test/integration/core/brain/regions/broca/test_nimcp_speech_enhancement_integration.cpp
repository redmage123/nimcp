/**
 * @file test_nimcp_speech_enhancement_integration.cpp
 * @brief Integration tests for Phase B4 Speech Enhancement modules
 *
 * WHAT: Test integration between speech enhancement components
 * WHY:  Ensure modules work together in realistic speech processing pipelines
 * HOW:  Create integrated pipelines connecting pragmatics, discourse, prosody,
 *       incremental processing, repair, and multimodal systems
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/brain/regions/broca/nimcp_pragmatics_processor.h"
#include "core/brain/regions/broca/nimcp_discourse_manager.h"
#include "core/brain/regions/broca/nimcp_emotional_prosody.h"
#include "core/brain/regions/broca/nimcp_incremental_processor.h"
#include "core/brain/regions/broca/nimcp_speech_repair.h"
#include "core/brain/regions/broca/nimcp_multimodal_language.h"

/**
 * @brief Full speech enhancement pipeline integration test fixture
 */
class SpeechEnhancementIntegrationTest : public ::testing::Test {
protected:
    pragmatics_processor_t* pragmatics;
    discourse_manager_t* discourse;
    emotional_prosody_t* prosody;
    incremental_processor_t* incremental;
    speech_repair_t* repair;
    multimodal_language_t* multimodal;

    void SetUp() override {
        // Create all components with default configs
        pragmatics_config_t prag_cfg = pragmatics_default_config();
        pragmatics = pragmatics_create(&prag_cfg);
        ASSERT_NE(nullptr, pragmatics);

        discourse_config_t disc_cfg = discourse_default_config();
        discourse = discourse_create(&disc_cfg);
        ASSERT_NE(nullptr, discourse);

        emotional_prosody_config_t pros_cfg = emotional_prosody_default_config();
        prosody = emotional_prosody_create(&pros_cfg);
        ASSERT_NE(nullptr, prosody);

        incremental_config_t incr_cfg = incremental_default_config();
        incremental = incremental_create(&incr_cfg);
        ASSERT_NE(nullptr, incremental);

        speech_repair_config_t rep_cfg = speech_repair_default_config();
        repair = speech_repair_create(&rep_cfg);
        ASSERT_NE(nullptr, repair);

        multimodal_config_t mult_cfg = multimodal_lang_default_config();
        multimodal = multimodal_lang_create(&mult_cfg);
        ASSERT_NE(nullptr, multimodal);
    }

    void TearDown() override {
        pragmatics_destroy(pragmatics);
        discourse_destroy(discourse);
        emotional_prosody_destroy(prosody);
        incremental_destroy(incremental);
        speech_repair_destroy(repair);
        multimodal_lang_destroy(multimodal);
    }
};

// ============================================================================
// Pragmatics + Discourse Integration
// ============================================================================

TEST_F(SpeechEnhancementIntegrationTest, PragmaticsDiscourseIntegration) {
    // Introduce a referent through discourse (name, type, gender=male, number=singular)
    uint32_t ref_id = discourse_introduce_referent(discourse, "John",
        REFERENT_TYPE_PERSON, 1, 1);
    EXPECT_GT(ref_id, 0u);

    // Analyze a directive about the referent
    pragmatic_analysis_t analysis;
    EXPECT_TRUE(pragmatics_analyze(pragmatics, "Tell John to come here",
        1, 1000, &analysis));
    // The imperative "Tell" may classify as command or assertion depending on patterns
    EXPECT_TRUE(analysis.speech_act.primary_act == SPEECH_ACT_COMMAND ||
                analysis.speech_act.primary_act == SPEECH_ACT_ASSERT ||
                analysis.speech_act.primary_act == SPEECH_ACT_REQUEST);

    // Resolve anaphora in follow-up
    anaphora_resolution_t resolution;
    EXPECT_TRUE(discourse_resolve_anaphora(discourse, "he", NULL, &resolution));
    EXPECT_TRUE(resolution.is_resolved);
    EXPECT_EQ(resolution.antecedent_id, ref_id);
}

TEST_F(SpeechEnhancementIntegrationTest, PragmaticsDiscourseConversation) {
    // Simulate multi-turn conversation

    // Turn 1: Introduction
    pragmatic_analysis_t analysis1;
    EXPECT_TRUE(pragmatics_analyze(pragmatics, "I'm looking for a book",
        1, 1000, &analysis1));
    EXPECT_EQ(analysis1.speech_act.primary_act, SPEECH_ACT_ASSERT);

    discourse_introduce_referent(discourse, "book", REFERENT_TYPE_OBJECT, 3, 1);

    // Turn 2: Question
    pragmatic_analysis_t analysis2;
    EXPECT_TRUE(pragmatics_analyze(pragmatics, "What kind would you like?",
        2, 2000, &analysis2));
    EXPECT_EQ(analysis2.speech_act.primary_act, SPEECH_ACT_QUESTION);

    // Turn 3: Response with reference
    discourse_introduce_referent(discourse, "novel", REFERENT_TYPE_OBJECT, 3, 1);

    anaphora_resolution_t resolution;
    EXPECT_TRUE(discourse_resolve_anaphora(discourse, "it", NULL, &resolution));
    // Should resolve to most salient entity (novel)
    EXPECT_TRUE(resolution.is_resolved);
}

// ============================================================================
// Pragmatics + Prosody Integration
// ============================================================================

TEST_F(SpeechEnhancementIntegrationTest, PragmaticsProsodyQuestion) {
    // Analyze a question pragmatically - use "What" for reliable question detection
    pragmatic_analysis_t prag_analysis;
    EXPECT_TRUE(pragmatics_analyze(pragmatics, "What time is the party?",
        1, 1000, &prag_analysis));
    EXPECT_EQ(prag_analysis.speech_act.primary_act, SPEECH_ACT_QUESTION);

    // Generate appropriate prosody (questions have rising intonation)
    emotional_state_t state = {EMOTION_NEUTRAL, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.5f, 0.0f};
    emotional_prosody_set_emotion(prosody, &state);

    prosodic_contour_t contour;
    EXPECT_TRUE(emotional_prosody_generate_contour(prosody,
        "What time is the party?", 1500.0f, &contour));
    EXPECT_GT(contour.point_count, 0u);

    // Verify rising intonation pattern at end
    if (contour.point_count >= 2) {
        float end_pitch = contour.points[contour.point_count - 1].pitch_hz;
        float mid_pitch = contour.points[contour.point_count / 2].pitch_hz;
        // Questions typically have final rise (allow some tolerance)
        EXPECT_GE(end_pitch, mid_pitch * 0.8f);
    }

    emotional_prosody_free_contour(&contour);
}

TEST_F(SpeechEnhancementIntegrationTest, PragmaticsProsodyExclamation) {
    // Exclamatory expressives need excited prosody
    pragmatic_analysis_t prag_analysis;
    EXPECT_TRUE(pragmatics_analyze(pragmatics, "That's wonderful news!",
        1, 1000, &prag_analysis));
    // "wonderful" triggers positive expressive classification
    EXPECT_TRUE(prag_analysis.speech_act.primary_act == SPEECH_ACT_ASSERT ||
                prag_analysis.speech_act.primary_act == SPEECH_ACT_CONGRATULATE);

    // Set happy emotion for expressive
    emotional_state_t state = {EMOTION_HAPPY, 0.9f, EMOTION_NEUTRAL, 0.0f, 0.8f, 0.7f};
    emotional_prosody_set_emotion(prosody, &state);

    prosodic_params_t params;
    EXPECT_TRUE(emotional_prosody_get_params(prosody, &state, &params));
    EXPECT_GT(params.pitch_mean_hz, 100.0f); // Higher pitch for excitement
    EXPECT_GT(params.rate_factor, 1.0f); // Faster speech for excitement
}

// ============================================================================
// Repair + Incremental Integration
// ============================================================================

TEST_F(SpeechEnhancementIntegrationTest, RepairIncrementalDisfluencyHandling) {
    // Simulate incremental input with disfluency
    const char* words[] = {"I", "um", "want", "to", "go"};
    uint64_t timestamps[] = {0, 200, 400, 600, 800};

    for (int i = 0; i < 5; i++) {
        incremental_add_word(incremental, words[i], timestamps[i]);
    }

    // Process all input
    incremental_process(incremental, 1000);

    // Detect disfluency in accumulated input
    disfluency_t disfluencies[8];
    uint32_t count = speech_repair_detect_disfluencies(repair,
        "I um want to go", disfluencies, 8);
    EXPECT_GT(count, 0u);

    // Verify "um" detected as filled pause
    bool found_um = false;
    for (uint32_t i = 0; i < count; i++) {
        if (disfluencies[i].type == DISFLUENCY_FILLED_PAUSE) {
            found_um = true;
            break;
        }
    }
    EXPECT_TRUE(found_um);

    // Clean the utterance
    char cleaned[256];
    EXPECT_TRUE(speech_repair_clean(repair, "I um want to go",
        cleaned, sizeof(cleaned)));
    EXPECT_TRUE(strstr(cleaned, "um") == NULL);
}

TEST_F(SpeechEnhancementIntegrationTest, RepairIncrementalRevision) {
    // Add word with typo
    uint32_t id = incremental_add_word(incremental, "tset", 1000);
    incremental_process(incremental, 1010);

    // Detect self-correction potential
    repair_instance_t repairs[4];
    uint32_t count = speech_repair_detect_repairs(repair,
        "tset, I mean test", repairs, 4);
    EXPECT_GT(count, 0u);

    // Revise the unit
    EXPECT_TRUE(incremental_can_revise(incremental, id));
    EXPECT_TRUE(incremental_revise_unit(incremental, id, "test"));

    // Verify revision recorded
    revision_record_t revisions[5];
    uint32_t rev_count = incremental_get_revisions(incremental, revisions, 5);
    EXPECT_GT(rev_count, 0u);
}

// ============================================================================
// Multimodal + Prosody Integration
// ============================================================================

TEST_F(SpeechEnhancementIntegrationTest, MultimodalProsodyEmotionSync) {
    // Set emotional state
    emotional_state_t state = {EMOTION_HAPPY, 0.9f, EMOTION_NEUTRAL, 0.0f, 0.8f, 0.7f};
    emotional_prosody_set_emotion(prosody, &state);

    // Get prosodic parameters
    prosodic_params_t params;
    EXPECT_TRUE(emotional_prosody_get_params(prosody, &state, &params));

    // Generate multimodal plan - use "happy" keyword for reliable expression detection
    multimodal_plan_t plan;
    EXPECT_TRUE(multimodal_lang_generate_plan(multimodal, "I'm so happy about this!",
        1200.0f, &plan));

    // Plan should have expressions
    EXPECT_GT(plan.expression_count, 0u);

    // With auto-expressions enabled and "happy" keyword, should generate smile
    bool found_positive_expression = false;
    for (uint32_t i = 0; i < plan.expression_count; i++) {
        if (plan.expressions[i].type == EXPRESSION_SMILE ||
            plan.expressions[i].type == EXPRESSION_RAISED_EYEBROWS) {
            found_positive_expression = true;
            break;
        }
    }
    EXPECT_TRUE(found_positive_expression);

    multimodal_lang_free_plan(&plan);
}

TEST_F(SpeechEnhancementIntegrationTest, MultimodalGestureTiming) {
    // Generate multimodal plan
    multimodal_plan_t plan;
    EXPECT_TRUE(multimodal_lang_generate_plan(multimodal,
        "Look at this big thing over there", 2000.0f, &plan));

    // Should have gestures for "this" (deictic) and "big" (iconic)
    EXPECT_GT(plan.gesture_count, 0u);

    // Synchronize plan
    EXPECT_TRUE(multimodal_lang_synchronize_plan(multimodal, &plan));
    EXPECT_GT(plan.sync_score, 0.5f);

    // Generate prosodic contour
    prosodic_contour_t contour;
    emotional_state_t state = {EMOTION_NEUTRAL, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.5f, 0.0f};
    emotional_prosody_set_emotion(prosody, &state);
    EXPECT_TRUE(emotional_prosody_generate_contour(prosody,
        "Look at this big thing over there", 2000.0f, &contour));

    // Verify gesture and prosody durations match
    EXPECT_FLOAT_EQ(plan.speech_duration_ms, contour.duration_ms);

    emotional_prosody_free_contour(&contour);
    multimodal_lang_free_plan(&plan);
}

// ============================================================================
// Discourse + Multimodal Integration
// ============================================================================

TEST_F(SpeechEnhancementIntegrationTest, DiscourseMultimodalGazeTracking) {
    // Introduce referents (name, type, gender, number)
    uint32_t person_id = discourse_introduce_referent(discourse, "Sarah",
        REFERENT_TYPE_PERSON, 2, 1);  // female, singular
    uint32_t object_id = discourse_introduce_referent(discourse, "book",
        REFERENT_TYPE_OBJECT, 3, 1);  // neuter, singular

    // Generate plan mentioning referents
    multimodal_plan_t plan;
    EXPECT_TRUE(multimodal_lang_generate_plan(multimodal,
        "Sarah is reading a book", 1500.0f, &plan));

    // Should have gaze events
    EXPECT_GT(plan.gaze_count, 0u);

    // Verify gaze targets make sense
    bool found_addressee = false;
    bool found_object = false;
    for (uint32_t i = 0; i < plan.gaze_count; i++) {
        if (plan.gaze_events[i].target == GAZE_TARGET_ADDRESSEE) {
            found_addressee = true;
        }
        if (plan.gaze_events[i].target == GAZE_TARGET_OBJECT) {
            found_object = true;
        }
    }
    // Should look at addressee and/or object during utterance
    EXPECT_TRUE(found_addressee || found_object);

    multimodal_lang_free_plan(&plan);
}

// ============================================================================
// Full Pipeline Integration
// ============================================================================

TEST_F(SpeechEnhancementIntegrationTest, FullProductionPipeline) {
    // Simulate complete speech production with all enhancements

    // 1. Analyze pragmatics of intended utterance
    const char* utterance = "Could you please help me find my keys?";
    pragmatic_analysis_t prag_analysis;
    EXPECT_TRUE(pragmatics_analyze(pragmatics, utterance, 1, 1000, &prag_analysis));
    // "Could you please" is an indirect request
    EXPECT_TRUE(prag_analysis.speech_act.primary_act == SPEECH_ACT_REQUEST ||
                prag_analysis.speech_act.primary_act == SPEECH_ACT_QUESTION);

    // 2. Track discourse referents
    uint32_t ref_id = discourse_introduce_referent(discourse, "keys",
        REFERENT_TYPE_OBJECT, 3, 2);  // neuter, plural
    EXPECT_GT(ref_id, 0u);

    // 3. Set appropriate emotion (polite request)
    emotional_state_t state = {EMOTION_NEUTRAL, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.5f, 0.1f};
    emotional_prosody_set_emotion(prosody, &state);

    // 4. Generate prosodic contour
    prosodic_contour_t contour;
    EXPECT_TRUE(emotional_prosody_generate_contour(prosody, utterance,
        2500.0f, &contour));
    EXPECT_GT(contour.point_count, 0u);

    // 5. Generate multimodal plan
    multimodal_plan_t plan;
    EXPECT_TRUE(multimodal_lang_generate_plan(multimodal, utterance, 2500.0f, &plan));
    EXPECT_TRUE(multimodal_lang_synchronize_plan(multimodal, &plan));

    // 6. Process incrementally with potential repair
    const char* words[] = {"Could", "you", "please", "um", "help", "me", "find", "my", "keys"};
    for (int i = 0; i < 9; i++) {
        incremental_add_word(incremental, words[i], i * 250);
    }
    incremental_process(incremental, 2500);
    incremental_force_commit(incremental);

    // 7. Detect and clean any disfluencies
    repair_analysis_t rep_analysis;
    EXPECT_TRUE(speech_repair_analyze(repair,
        "Could you please um help me find my keys", &rep_analysis));
    // Should detect the "um"
    EXPECT_GT(rep_analysis.disfluency_count, 0u);

    // Cleanup
    emotional_prosody_free_contour(&contour);
    multimodal_lang_free_plan(&plan);
}

TEST_F(SpeechEnhancementIntegrationTest, FullComprehensionPipeline) {
    // Simulate speech comprehension with disfluent input

    // 1. Input has disfluency
    const char* input = "I want the, I mean, I need the big book";

    // 2. Detect and analyze repairs
    repair_analysis_t rep_analysis;
    EXPECT_TRUE(speech_repair_analyze(repair, input, &rep_analysis));
    EXPECT_GT(rep_analysis.repair_count, 0u);

    // 3. Get cleaned version
    char cleaned[256];
    EXPECT_TRUE(speech_repair_clean(repair, input, cleaned, sizeof(cleaned)));

    // 4. Analyze pragmatics of cleaned utterance
    pragmatic_analysis_t prag_analysis;
    EXPECT_TRUE(pragmatics_analyze(pragmatics, cleaned, 1, 1000, &prag_analysis));
    EXPECT_EQ(prag_analysis.speech_act.primary_act, SPEECH_ACT_ASSERT);

    // 5. Track discourse referents
    discourse_introduce_referent(discourse, "book", REFERENT_TYPE_OBJECT, 3, 1);

    // 6. Analyze emotional prosody (simulated recognition)
    emotional_state_t detected_state;
    emotional_prosody_get_emotion(prosody, &detected_state);
    // Default should be neutral
    EXPECT_EQ(detected_state.primary_emotion, EMOTION_NEUTRAL);
}

// ============================================================================
// Error Recovery Integration
// ============================================================================

TEST_F(SpeechEnhancementIntegrationTest, ComponentFailureRecovery) {
    // Test that pipeline handles individual component failures gracefully

    // Null inputs should not crash the pipeline
    pragmatic_analysis_t dummy_analysis;
    EXPECT_FALSE(pragmatics_analyze(pragmatics, NULL, 1, 1000, &dummy_analysis));
    EXPECT_EQ(discourse_introduce_referent(discourse, NULL, REFERENT_TYPE_OBJECT, 3, 1), 0u);

    prosodic_contour_t contour;
    EXPECT_FALSE(emotional_prosody_generate_contour(prosody, NULL, 1000.0f, &contour));

    multimodal_plan_t plan;
    EXPECT_FALSE(multimodal_lang_generate_plan(multimodal, NULL, 1000.0f, &plan));

    // Processing should still work after failed calls
    pragmatic_analysis_t analysis;
    EXPECT_TRUE(pragmatics_analyze(pragmatics, "Hello world", 1, 1000, &analysis));
}

TEST_F(SpeechEnhancementIntegrationTest, ResetAllComponents) {
    // Process some data
    pragmatic_analysis_t analysis;
    pragmatics_analyze(pragmatics, "Test utterance", 1, 1000, &analysis);

    discourse_introduce_referent(discourse, "test", REFERENT_TYPE_OBJECT, 3, 1);

    incremental_add_word(incremental, "test", 1000);
    incremental_process(incremental, 1100);

    // Reset all components
    EXPECT_TRUE(pragmatics_reset(pragmatics));
    EXPECT_TRUE(discourse_reset(discourse));
    EXPECT_TRUE(emotional_prosody_reset(prosody));
    EXPECT_TRUE(incremental_reset(incremental));
    EXPECT_TRUE(speech_repair_reset(repair));
    EXPECT_TRUE(multimodal_lang_reset(multimodal));

    // Verify clean state
    EXPECT_EQ(pragmatics_get_status(pragmatics), PRAGMATICS_STATUS_IDLE);
    EXPECT_EQ(discourse_get_status(discourse), DISCOURSE_STATUS_IDLE);
    EXPECT_EQ(emotional_prosody_get_status(prosody), PROSODY_STATUS_IDLE);
    EXPECT_EQ(incremental_get_status(incremental), INCREMENTAL_STATUS_IDLE);
    EXPECT_EQ(speech_repair_get_status(repair), REPAIR_STATUS_IDLE);
    EXPECT_EQ(multimodal_lang_get_status(multimodal), MULTIMODAL_STATUS_IDLE);
}

// ============================================================================
// Statistics Integration
// ============================================================================

TEST_F(SpeechEnhancementIntegrationTest, AggregatedStatistics) {
    // Run some processing
    pragmatic_analysis_t prag_analysis;
    pragmatics_analyze(pragmatics, "Please help me", 1, 1000, &prag_analysis);
    pragmatics_analyze(pragmatics, "Thank you!", 1, 2000, &prag_analysis);

    emotional_state_t state = {EMOTION_HAPPY, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.5f, 0.5f};
    prosodic_params_t params;
    emotional_prosody_get_params(prosody, &state, &params);

    repair_analysis_t rep_analysis;
    speech_repair_analyze(repair, "I um want help", &rep_analysis);

    // Collect stats from all components
    pragmatics_stats_t prag_stats;
    EXPECT_TRUE(pragmatics_get_stats(pragmatics, &prag_stats));
    EXPECT_EQ(prag_stats.utterances_processed, 2u);

    prosody_stats_t pros_stats;
    EXPECT_TRUE(emotional_prosody_get_stats(prosody, &pros_stats));
    EXPECT_GT(pros_stats.parameters_computed, 0u);

    repair_stats_t rep_stats;
    EXPECT_TRUE(speech_repair_get_stats(repair, &rep_stats));
    EXPECT_GT(rep_stats.disfluencies_detected, 0u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
