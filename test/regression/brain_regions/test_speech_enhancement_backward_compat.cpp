/**
 * @file test_speech_enhancement_backward_compat.cpp
 * @brief Backward compatibility regression tests for Phase B4 Speech Enhancement modules
 *
 * WHAT: Ensures speech enhancement APIs remain stable across refactorings
 * WHY:  Speech production and comprehension are critical human interaction features
 * HOW:  Test API signatures, behavioral consistency, memory management
 *
 * LINGUISTIC CONTEXT:
 * - Pragmatics: Speech acts, Gricean maxims, implicature
 * - Discourse: Anaphora resolution, topic tracking, coherence
 * - Prosody: Pitch, rate, intensity mapping from emotion
 * - Incremental: Real-time processing with revision capability
 * - Repair: Disfluency detection and self-correction
 * - Multimodal: Gesture, expression, gaze coordination
 *
 * TEST CATEGORIES:
 * 1. API Stability - Function signatures unchanged
 * 2. Behavioral Consistency - Processing patterns preserved
 * 3. Memory Management - No leaks, proper lifecycle
 * 4. Error Handling - Edge cases handled consistently
 * 5. Performance Baselines - No significant regressions
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <cstring>

#include "core/brain/regions/broca/nimcp_pragmatics_processor.h"
#include "core/brain/regions/broca/nimcp_discourse_manager.h"
#include "core/brain/regions/broca/nimcp_emotional_prosody.h"
#include "core/brain/regions/broca/nimcp_incremental_processor.h"
#include "core/brain/regions/broca/nimcp_speech_repair.h"
#include "core/brain/regions/broca/nimcp_multimodal_language.h"

/* ============================================================================
 * Test Fixture
 * ========================================================================== */

class SpeechEnhancementBackwardCompatTest : public ::testing::Test {
protected:
    pragmatics_processor_t* pragmatics = nullptr;
    discourse_manager_t* discourse = nullptr;
    emotional_prosody_t* prosody = nullptr;
    incremental_processor_t* incremental = nullptr;
    speech_repair_t* repair = nullptr;
    multimodal_language_t* multimodal = nullptr;

    static constexpr int WARMUP_ITERATIONS = 10;
    static constexpr int BENCHMARK_ITERATIONS = 100;
    static constexpr int MEMORY_TEST_CYCLES = 500;

    void SetUp() override {
        // Components created per-test as needed
    }

    void TearDown() override {
        if (pragmatics) { pragmatics_destroy(pragmatics); pragmatics = nullptr; }
        if (discourse) { discourse_destroy(discourse); discourse = nullptr; }
        if (prosody) { emotional_prosody_destroy(prosody); prosody = nullptr; }
        if (incremental) { incremental_destroy(incremental); incremental = nullptr; }
        if (repair) { speech_repair_destroy(repair); repair = nullptr; }
        if (multimodal) { multimodal_lang_destroy(multimodal); multimodal = nullptr; }
    }

    template<typename Func>
    long long measure_ns(Func func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }
};

/* ============================================================================
 * CATEGORY 1: Pragmatics API Stability Tests
 * ========================================================================== */

TEST_F(SpeechEnhancementBackwardCompatTest, Pragmatics_APIStability_DefaultConfig) {
    pragmatics_config_t config = pragmatics_default_config();

    // Default config values should remain stable
    EXPECT_TRUE(config.enable_indirect_speech);
    EXPECT_TRUE(config.enable_grice_analysis);
    EXPECT_TRUE(config.enable_scalar_implicature);
    EXPECT_GT(config.max_speech_acts, 0u);
}

TEST_F(SpeechEnhancementBackwardCompatTest, Pragmatics_APIStability_CreateDestroy) {
    pragmatics_config_t config = pragmatics_default_config();
    pragmatics = pragmatics_create(&config);
    ASSERT_NE(nullptr, pragmatics);

    // Verify status returns expected enum values
    EXPECT_EQ(pragmatics_get_status(pragmatics), PRAGMATICS_STATUS_IDLE);

    pragmatics_destroy(pragmatics);
    pragmatics = nullptr;
}

TEST_F(SpeechEnhancementBackwardCompatTest, Pragmatics_APIStability_SpeechActEnums) {
    // Speech act enum values must remain stable for saved/loaded models
    EXPECT_EQ(static_cast<int>(SPEECH_ACT_UNKNOWN), 0);
    EXPECT_EQ(static_cast<int>(SPEECH_ACT_ASSERT), 1);
    EXPECT_EQ(static_cast<int>(SPEECH_ACT_CLAIM), 2);
    EXPECT_EQ(static_cast<int>(SPEECH_ACT_CONCLUDE), 3);
    EXPECT_EQ(static_cast<int>(SPEECH_ACT_REPORT), 4);
}

TEST_F(SpeechEnhancementBackwardCompatTest, Pragmatics_APIStability_AnalyzeSignature) {
    pragmatics_config_t config = pragmatics_default_config();
    pragmatics = pragmatics_create(&config);

    pragmatic_analysis_t analysis;
    // API: pragmatics_analyze(processor, utterance, speaker_id, timestamp_ms, result)
    bool result = pragmatics_analyze(pragmatics, "Please help me", 1, 1000, &analysis);
    EXPECT_TRUE(result);
    EXPECT_EQ(analysis.speech_act.primary_act, SPEECH_ACT_REQUEST);
}

/* ============================================================================
 * CATEGORY 2: Discourse API Stability Tests
 * ========================================================================== */

TEST_F(SpeechEnhancementBackwardCompatTest, Discourse_APIStability_DefaultConfig) {
    discourse_config_t config = discourse_default_config();

    EXPECT_GT(config.max_referents, 0u);
    EXPECT_GT(config.salience_decay, 0.0f);
    // enable_zero_anaphora defaults to false (elided references disabled by default)
}

TEST_F(SpeechEnhancementBackwardCompatTest, Discourse_APIStability_ReferentTypes) {
    // Referent type enums must remain stable
    EXPECT_EQ(static_cast<int>(REFERENT_TYPE_UNKNOWN), 0);
    EXPECT_EQ(static_cast<int>(REFERENT_TYPE_PERSON), 1);
    EXPECT_EQ(static_cast<int>(REFERENT_TYPE_OBJECT), 2);
    EXPECT_EQ(static_cast<int>(REFERENT_TYPE_LOCATION), 3);
    EXPECT_EQ(static_cast<int>(REFERENT_TYPE_TIME), 4);
}

TEST_F(SpeechEnhancementBackwardCompatTest, Discourse_APIStability_IntroduceReferent) {
    discourse_config_t config = discourse_default_config();
    discourse = discourse_create(&config);

    // API: discourse_introduce_referent(manager, name, type, gender, number)
    // gender: 1=male, 2=female, 3=neuter; number: 1=singular, 2=plural
    uint32_t id = discourse_introduce_referent(discourse, "John",
        REFERENT_TYPE_PERSON, 1, 1);  // male, singular

    EXPECT_GT(id, 0u);
}

TEST_F(SpeechEnhancementBackwardCompatTest, Discourse_APIStability_ResolveAnaphora) {
    discourse_config_t config = discourse_default_config();
    discourse = discourse_create(&config);

    // Introduce Mary (female, singular)
    uint32_t id = discourse_introduce_referent(discourse, "Mary",
        REFERENT_TYPE_PERSON, 2, 1);  // female, singular

    anaphora_resolution_t resolution;
    // API: discourse_resolve_anaphora(manager, expression, context, result)
    bool result = discourse_resolve_anaphora(discourse, "she", NULL, &resolution);

    EXPECT_TRUE(result);
    EXPECT_TRUE(resolution.is_resolved);
    EXPECT_EQ(resolution.antecedent_id, id);
}

/* ============================================================================
 * CATEGORY 3: Prosody API Stability Tests
 * ========================================================================== */

TEST_F(SpeechEnhancementBackwardCompatTest, Prosody_APIStability_DefaultConfig) {
    emotional_prosody_config_t config = emotional_prosody_default_config();

    EXPECT_GT(config.base_pitch_hz, 0.0f);
    EXPECT_GT(config.base_rate_wpm, 0.0f);
    EXPECT_GT(config.max_contour_points, 0u);
}

TEST_F(SpeechEnhancementBackwardCompatTest, Prosody_APIStability_EmotionEnums) {
    // Emotion enums must remain stable for compatibility
    EXPECT_EQ(static_cast<int>(EMOTION_NEUTRAL), 0);
    EXPECT_EQ(static_cast<int>(EMOTION_HAPPY), 1);
    EXPECT_EQ(static_cast<int>(EMOTION_SAD), 2);
    EXPECT_EQ(static_cast<int>(EMOTION_ANGRY), 3);
    EXPECT_EQ(static_cast<int>(EMOTION_FEARFUL), 4);
    EXPECT_EQ(static_cast<int>(EMOTION_SURPRISED), 5);
    EXPECT_EQ(static_cast<int>(EMOTION_DISGUSTED), 6);
}

TEST_F(SpeechEnhancementBackwardCompatTest, Prosody_APIStability_VoiceQualityEnums) {
    EXPECT_EQ(static_cast<int>(VOICE_QUALITY_NORMAL), 0);
    EXPECT_EQ(static_cast<int>(VOICE_QUALITY_BREATHY), 1);
    EXPECT_EQ(static_cast<int>(VOICE_QUALITY_CREAKY), 2);
    EXPECT_EQ(static_cast<int>(VOICE_QUALITY_TENSE), 3);
}

TEST_F(SpeechEnhancementBackwardCompatTest, Prosody_APIStability_GetParams) {
    emotional_prosody_config_t config = emotional_prosody_default_config();
    prosody = emotional_prosody_create(&config);

    emotional_state_t state = {EMOTION_HAPPY, 0.8f, EMOTION_NEUTRAL, 0.0f, 0.7f, 0.6f};
    prosodic_params_t params;

    bool result = emotional_prosody_get_params(prosody, &state, &params);
    EXPECT_TRUE(result);
    EXPECT_GT(params.pitch_mean_hz, 0.0f);
    EXPECT_GT(params.rate_factor, 0.0f);
}

/* ============================================================================
 * CATEGORY 4: Incremental Processing API Stability Tests
 * ========================================================================== */

TEST_F(SpeechEnhancementBackwardCompatTest, Incremental_APIStability_DefaultConfig) {
    incremental_config_t config = incremental_default_config();

    EXPECT_GT(config.input_buffer_size, 0u);
    EXPECT_GT(config.output_buffer_size, 0u);
    EXPECT_GT(config.commit_delay_ms, 0u);
    EXPECT_TRUE(config.enable_revision);
}

TEST_F(SpeechEnhancementBackwardCompatTest, Incremental_APIStability_UnitTypeEnums) {
    EXPECT_EQ(static_cast<int>(UNIT_TYPE_PHONEME), 0);
    EXPECT_EQ(static_cast<int>(UNIT_TYPE_SYLLABLE), 1);
    EXPECT_EQ(static_cast<int>(UNIT_TYPE_WORD), 2);
    EXPECT_EQ(static_cast<int>(UNIT_TYPE_PHRASE), 3);
}

TEST_F(SpeechEnhancementBackwardCompatTest, Incremental_APIStability_AddProcess) {
    incremental_config_t config = incremental_default_config();
    incremental = incremental_create(&config);

    uint32_t id = incremental_add_word(incremental, "hello", 1000);
    EXPECT_GT(id, 0u);

    bool result = incremental_process(incremental, 1100);
    EXPECT_TRUE(result);
}

/* ============================================================================
 * CATEGORY 5: Speech Repair API Stability Tests
 * ========================================================================== */

TEST_F(SpeechEnhancementBackwardCompatTest, Repair_APIStability_DefaultConfig) {
    speech_repair_config_t config = speech_repair_default_config();

    EXPECT_GT(config.max_repairs, 0u);
    EXPECT_GT(config.history_size, 0u);
    EXPECT_TRUE(config.enable_auto_correction);
}

TEST_F(SpeechEnhancementBackwardCompatTest, Repair_APIStability_DisfluencyEnums) {
    EXPECT_EQ(static_cast<int>(DISFLUENCY_NONE), 0);
    EXPECT_EQ(static_cast<int>(DISFLUENCY_FILLED_PAUSE), 1);
    EXPECT_EQ(static_cast<int>(DISFLUENCY_SILENT_PAUSE), 2);
    EXPECT_EQ(static_cast<int>(DISFLUENCY_LENGTHENING), 3);
    EXPECT_EQ(static_cast<int>(DISFLUENCY_WORD_FRAGMENT), 4);
}

TEST_F(SpeechEnhancementBackwardCompatTest, Repair_APIStability_RepairTypeEnums) {
    EXPECT_EQ(static_cast<int>(REPAIR_TYPE_NONE), 0);
    EXPECT_EQ(static_cast<int>(REPAIR_TYPE_RESTART), 1);
    EXPECT_EQ(static_cast<int>(REPAIR_TYPE_REPLACEMENT), 2);
    EXPECT_EQ(static_cast<int>(REPAIR_TYPE_INSERTION), 3);
    EXPECT_EQ(static_cast<int>(REPAIR_TYPE_DELETION), 4);
}

TEST_F(SpeechEnhancementBackwardCompatTest, Repair_APIStability_DetectDisfluencies) {
    speech_repair_config_t config = speech_repair_default_config();
    repair = speech_repair_create(&config);

    disfluency_t disfluencies[8];
    uint32_t count = speech_repair_detect_disfluencies(repair,
        "I um want to go", disfluencies, 8);

    EXPECT_GT(count, 0u);
}

/* ============================================================================
 * CATEGORY 6: Multimodal API Stability Tests
 * ========================================================================== */

TEST_F(SpeechEnhancementBackwardCompatTest, Multimodal_APIStability_DefaultConfig) {
    multimodal_config_t config = multimodal_lang_default_config();

    EXPECT_GT(config.max_gestures, 0u);
    EXPECT_GT(config.max_expressions, 0u);
    EXPECT_TRUE(config.enable_auto_gestures);
    EXPECT_TRUE(config.enable_auto_expressions);
}

TEST_F(SpeechEnhancementBackwardCompatTest, Multimodal_APIStability_GestureEnums) {
    EXPECT_EQ(static_cast<int>(GESTURE_TYPE_NONE), 0);
    EXPECT_EQ(static_cast<int>(GESTURE_TYPE_ICONIC), 1);
    EXPECT_EQ(static_cast<int>(GESTURE_TYPE_METAPHORIC), 2);
    EXPECT_EQ(static_cast<int>(GESTURE_TYPE_BEAT), 3);
    EXPECT_EQ(static_cast<int>(GESTURE_TYPE_DEICTIC), 4);
}

TEST_F(SpeechEnhancementBackwardCompatTest, Multimodal_APIStability_ExpressionEnums) {
    EXPECT_EQ(static_cast<int>(EXPRESSION_NEUTRAL), 0);
    EXPECT_EQ(static_cast<int>(EXPRESSION_SMILE), 1);
    EXPECT_EQ(static_cast<int>(EXPRESSION_FROWN), 2);
    EXPECT_EQ(static_cast<int>(EXPRESSION_RAISED_EYEBROWS), 3);
}

TEST_F(SpeechEnhancementBackwardCompatTest, Multimodal_APIStability_GeneratePlan) {
    multimodal_config_t config = multimodal_lang_default_config();
    multimodal = multimodal_lang_create(&config);

    multimodal_plan_t plan;
    bool result = multimodal_lang_generate_plan(multimodal, "Hello world", 1000.0f, &plan);

    EXPECT_TRUE(result);
    EXPECT_FLOAT_EQ(plan.speech_duration_ms, 1000.0f);

    multimodal_lang_free_plan(&plan);
}

/* ============================================================================
 * CATEGORY 7: Behavioral Consistency Tests
 * ========================================================================== */

TEST_F(SpeechEnhancementBackwardCompatTest, Behavioral_PragmaticsClassification) {
    pragmatics_config_t config = pragmatics_default_config();
    pragmatics = pragmatics_create(&config);

    // These classifications must remain consistent
    // API: pragmatics_classify_speech_act(processor, utterance, speaker_id, result)
    speech_act_result_t result;

    pragmatics_classify_speech_act(pragmatics, "It is raining", 1, &result);
    EXPECT_EQ(result.primary_act, SPEECH_ACT_ASSERT);

    pragmatics_classify_speech_act(pragmatics, "Please help me", 1, &result);
    EXPECT_EQ(result.primary_act, SPEECH_ACT_REQUEST);

    pragmatics_classify_speech_act(pragmatics, "I promise to help", 1, &result);
    EXPECT_EQ(result.primary_act, SPEECH_ACT_PROMISE);

    pragmatics_classify_speech_act(pragmatics, "Thank you so much!", 1, &result);
    EXPECT_EQ(result.primary_act, SPEECH_ACT_THANK);
}

TEST_F(SpeechEnhancementBackwardCompatTest, Behavioral_FillerDetection) {
    // Filler words must be consistently detected
    EXPECT_TRUE(speech_repair_is_filler("um"));
    EXPECT_TRUE(speech_repair_is_filler("uh"));
    EXPECT_TRUE(speech_repair_is_filler("er"));
    EXPECT_TRUE(speech_repair_is_filler("like"));

    EXPECT_FALSE(speech_repair_is_filler("hello"));
    EXPECT_FALSE(speech_repair_is_filler("the"));
    EXPECT_FALSE(speech_repair_is_filler("is"));
}

TEST_F(SpeechEnhancementBackwardCompatTest, Behavioral_EmotionProsodyMapping) {
    emotional_prosody_config_t config = emotional_prosody_default_config();
    prosody = emotional_prosody_create(&config);

    prosodic_params_t neutral_params, happy_params, sad_params;

    emotional_state_t neutral = {EMOTION_NEUTRAL, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.5f, 0.0f};
    emotional_state_t happy = {EMOTION_HAPPY, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.8f, 0.7f};
    emotional_state_t sad = {EMOTION_SAD, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.2f, -0.7f};

    emotional_prosody_get_params(prosody, &neutral, &neutral_params);
    emotional_prosody_get_params(prosody, &happy, &happy_params);
    emotional_prosody_get_params(prosody, &sad, &sad_params);

    // Happy should have higher pitch and rate than neutral
    EXPECT_GT(happy_params.pitch_mean_hz, neutral_params.pitch_mean_hz);
    EXPECT_GT(happy_params.rate_factor, neutral_params.rate_factor);

    // Sad should have lower pitch and rate than neutral
    EXPECT_LT(sad_params.pitch_mean_hz, neutral_params.pitch_mean_hz);
    EXPECT_LT(sad_params.rate_factor, neutral_params.rate_factor);
}

/* ============================================================================
 * CATEGORY 8: Memory Management Tests
 * ========================================================================== */

TEST_F(SpeechEnhancementBackwardCompatTest, Memory_CreateDestroyLoop) {
    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        pragmatics_config_t prag_cfg = pragmatics_default_config();
        pragmatics_processor_t* p = pragmatics_create(&prag_cfg);
        ASSERT_NE(nullptr, p);
        pragmatics_destroy(p);

        discourse_config_t disc_cfg = discourse_default_config();
        discourse_manager_t* d = discourse_create(&disc_cfg);
        ASSERT_NE(nullptr, d);
        discourse_destroy(d);

        emotional_prosody_config_t pros_cfg = emotional_prosody_default_config();
        emotional_prosody_t* pr = emotional_prosody_create(&pros_cfg);
        ASSERT_NE(nullptr, pr);
        emotional_prosody_destroy(pr);

        incremental_config_t incr_cfg = incremental_default_config();
        incremental_processor_t* inc = incremental_create(&incr_cfg);
        ASSERT_NE(nullptr, inc);
        incremental_destroy(inc);

        speech_repair_config_t rep_cfg = speech_repair_default_config();
        speech_repair_t* r = speech_repair_create(&rep_cfg);
        ASSERT_NE(nullptr, r);
        speech_repair_destroy(r);

        multimodal_config_t mult_cfg = multimodal_lang_default_config();
        multimodal_language_t* m = multimodal_lang_create(&mult_cfg);
        ASSERT_NE(nullptr, m);
        multimodal_lang_destroy(m);
    }
}

TEST_F(SpeechEnhancementBackwardCompatTest, Memory_ContourAllocationFree) {
    emotional_prosody_config_t config = emotional_prosody_default_config();
    prosody = emotional_prosody_create(&config);

    emotional_state_t state = {EMOTION_NEUTRAL, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.5f, 0.0f};
    emotional_prosody_set_emotion(prosody, &state);

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        prosodic_contour_t contour;
        bool result = emotional_prosody_generate_contour(prosody, "Test", 500.0f, &contour);
        ASSERT_TRUE(result);
        emotional_prosody_free_contour(&contour);
    }
}

TEST_F(SpeechEnhancementBackwardCompatTest, Memory_PlanAllocationFree) {
    multimodal_config_t config = multimodal_lang_default_config();
    multimodal = multimodal_lang_create(&config);

    for (int i = 0; i < MEMORY_TEST_CYCLES; i++) {
        multimodal_plan_t plan;
        bool result = multimodal_lang_generate_plan(multimodal, "Test utterance", 1000.0f, &plan);
        ASSERT_TRUE(result);
        multimodal_lang_free_plan(&plan);
    }
}

/* ============================================================================
 * CATEGORY 9: Error Handling Consistency Tests
 * ========================================================================== */

TEST_F(SpeechEnhancementBackwardCompatTest, ErrorHandling_NullInputs) {
    // All modules should handle null inputs consistently

    // Pragmatics
    pragmatics_config_t prag_cfg = pragmatics_default_config();
    pragmatics = pragmatics_create(&prag_cfg);
    pragmatic_analysis_t prag_analysis;
    EXPECT_FALSE(pragmatics_analyze(pragmatics, NULL, 1, 1000, &prag_analysis));
    EXPECT_FALSE(pragmatics_analyze(pragmatics, "test", 1, 1000, NULL));

    // Discourse
    discourse_config_t disc_cfg = discourse_default_config();
    discourse = discourse_create(&disc_cfg);
    EXPECT_EQ(discourse_introduce_referent(discourse, NULL, REFERENT_TYPE_PERSON, 1, 1), 0u);

    // Prosody
    emotional_prosody_config_t pros_cfg = emotional_prosody_default_config();
    prosody = emotional_prosody_create(&pros_cfg);
    prosodic_params_t params;
    EXPECT_FALSE(emotional_prosody_get_params(prosody, NULL, &params));

    // Incremental
    incremental_config_t incr_cfg = incremental_default_config();
    incremental = incremental_create(&incr_cfg);
    EXPECT_EQ(incremental_add_unit(incremental, NULL, UNIT_TYPE_WORD, 1000), 0u);

    // Repair
    speech_repair_config_t rep_cfg = speech_repair_default_config();
    repair = speech_repair_create(&rep_cfg);
    disfluency_t disf[4];
    EXPECT_EQ(speech_repair_detect_disfluencies(repair, NULL, disf, 4), 0u);

    // Multimodal
    multimodal_config_t mult_cfg = multimodal_lang_default_config();
    multimodal = multimodal_lang_create(&mult_cfg);
    multimodal_plan_t plan;
    EXPECT_FALSE(multimodal_lang_generate_plan(multimodal, NULL, 1000.0f, &plan));
}

TEST_F(SpeechEnhancementBackwardCompatTest, ErrorHandling_NullProcessors) {
    // All functions should handle null processors

    EXPECT_EQ(pragmatics_get_status(NULL), PRAGMATICS_STATUS_ERROR);
    EXPECT_EQ(discourse_get_status(NULL), DISCOURSE_STATUS_ERROR);
    EXPECT_EQ(emotional_prosody_get_status(NULL), PROSODY_STATUS_ERROR);
    EXPECT_EQ(incremental_get_status(NULL), INCREMENTAL_STATUS_ERROR);
    EXPECT_EQ(speech_repair_get_status(NULL), REPAIR_STATUS_ERROR);
    EXPECT_EQ(multimodal_lang_get_status(NULL), MULTIMODAL_STATUS_ERROR);
}

/* ============================================================================
 * CATEGORY 10: Performance Baseline Tests
 * ========================================================================== */

TEST_F(SpeechEnhancementBackwardCompatTest, Performance_PragmaticsAnalysis) {
    pragmatics_config_t config = pragmatics_default_config();
    pragmatics = pragmatics_create(&config);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        pragmatic_analysis_t analysis;
        pragmatics_analyze(pragmatics, "Please help me", 1, 1000, &analysis);
    }

    // Benchmark
    long long total_ns = 0;
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        pragmatic_analysis_t analysis;
        total_ns += measure_ns([&]() {
            pragmatics_analyze(pragmatics, "Please help me", 1, 1000, &analysis);
        });
    }

    double avg_us = total_ns / BENCHMARK_ITERATIONS / 1000.0;
    std::cout << "Pragmatics analysis: " << avg_us << " us/call" << std::endl;
    EXPECT_LT(avg_us, 10000.0); // Should complete in < 10ms
}

TEST_F(SpeechEnhancementBackwardCompatTest, Performance_DisfluencyDetection) {
    speech_repair_config_t config = speech_repair_default_config();
    repair = speech_repair_create(&config);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        disfluency_t disf[8];
        speech_repair_detect_disfluencies(repair, "I um want to uh go", disf, 8);
    }

    // Benchmark
    long long total_ns = 0;
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        disfluency_t disf[8];
        total_ns += measure_ns([&]() {
            speech_repair_detect_disfluencies(repair, "I um want to uh go", disf, 8);
        });
    }

    double avg_us = total_ns / BENCHMARK_ITERATIONS / 1000.0;
    std::cout << "Disfluency detection: " << avg_us << " us/call" << std::endl;
    EXPECT_LT(avg_us, 5000.0); // Should complete in < 5ms
}

TEST_F(SpeechEnhancementBackwardCompatTest, Performance_ContourGeneration) {
    emotional_prosody_config_t config = emotional_prosody_default_config();
    prosody = emotional_prosody_create(&config);

    emotional_state_t state = {EMOTION_NEUTRAL, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.5f, 0.0f};
    emotional_prosody_set_emotion(prosody, &state);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        prosodic_contour_t contour;
        emotional_prosody_generate_contour(prosody, "Hello world", 1000.0f, &contour);
        emotional_prosody_free_contour(&contour);
    }

    // Benchmark
    long long total_ns = 0;
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        prosodic_contour_t contour;
        total_ns += measure_ns([&]() {
            emotional_prosody_generate_contour(prosody, "Hello world", 1000.0f, &contour);
        });
        emotional_prosody_free_contour(&contour);
    }

    double avg_us = total_ns / BENCHMARK_ITERATIONS / 1000.0;
    std::cout << "Contour generation: " << avg_us << " us/call" << std::endl;
    EXPECT_LT(avg_us, 10000.0); // Should complete in < 10ms
}

TEST_F(SpeechEnhancementBackwardCompatTest, Performance_MultimodalPlanGeneration) {
    multimodal_config_t config = multimodal_lang_default_config();
    multimodal = multimodal_lang_create(&config);

    // Warmup
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        multimodal_plan_t plan;
        multimodal_lang_generate_plan(multimodal, "Look at this big thing", 1500.0f, &plan);
        multimodal_lang_free_plan(&plan);
    }

    // Benchmark
    long long total_ns = 0;
    for (int i = 0; i < BENCHMARK_ITERATIONS; i++) {
        multimodal_plan_t plan;
        total_ns += measure_ns([&]() {
            multimodal_lang_generate_plan(multimodal, "Look at this big thing", 1500.0f, &plan);
        });
        multimodal_lang_free_plan(&plan);
    }

    double avg_us = total_ns / BENCHMARK_ITERATIONS / 1000.0;
    std::cout << "Multimodal plan generation: " << avg_us << " us/call" << std::endl;
    EXPECT_LT(avg_us, 20000.0); // Should complete in < 20ms
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
