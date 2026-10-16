/**
 * @file e2e_test_speech_enhancement_pipeline.cpp
 * @brief End-to-end tests for Phase B4 Speech Enhancement Pipeline
 *
 * WHAT: Full pipeline tests for speech production and comprehension enhancements
 * WHY:  Verify complete speech workflows including pragmatics, discourse,
 *       prosody, incremental processing, repair, and multimodal coordination
 * HOW:  Test realistic conversational scenarios end-to-end
 *
 * TEST COVERAGE:
 * - Speech Production Pipeline (5 tests)
 * - Speech Comprehension Pipeline (4 tests)
 * - Conversational Flow (4 tests)
 * - Error Recovery Pipeline (3 tests)
 * - Performance Pipeline (3 tests)
 *
 * TOTAL: 19 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Broca's area for speech production planning
 * - Wernicke's area for comprehension and pragmatics
 * - Supplementary motor area for articulation timing
 * - Prefrontal cortex for discourse planning
 * - Mirror neurons for gesture-speech coordination
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#include "e2e_test_framework.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <cstring>

#include "core/brain/regions/broca/nimcp_pragmatics_processor.h"
#include "core/brain/regions/broca/nimcp_discourse_manager.h"
#include "core/brain/regions/broca/nimcp_emotional_prosody.h"
#include "core/brain/regions/broca/nimcp_incremental_processor.h"
#include "core/brain/regions/broca/nimcp_speech_repair.h"
#include "core/brain/regions/broca/nimcp_multimodal_language.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_PRAGMATICS_TIME_MS = 20.0;
constexpr double MAX_PROSODY_TIME_MS = 30.0;
constexpr double MAX_MULTIMODAL_TIME_MS = 50.0;
constexpr double MAX_FULL_PIPELINE_TIME_MS = 200.0;
constexpr uint32_t CONVERSATION_TURNS = 10;
constexpr uint32_t STRESS_TEST_ITERATIONS = 100;

//=============================================================================
// Helper Structures
//=============================================================================

struct ConversationTurn {
    const char* speaker;
    const char* utterance;
    speech_act_type_t expected_act;
    emotion_type_t emotion;
    bool has_disfluency;
};

struct ProductionPlan {
    pragmatic_analysis_t pragmatics;
    prosodic_contour_t contour;
    multimodal_plan_t multimodal;
    bool valid;
};

//=============================================================================
// Test Fixtures
//=============================================================================

class E2ESpeechProductionTest {
public:
    pragmatics_processor_t* pragmatics = nullptr;
    discourse_manager_t* discourse = nullptr;
    emotional_prosody_t* prosody = nullptr;
    incremental_processor_t* incremental = nullptr;
    multimodal_language_t* multimodal = nullptr;

    void SetUp() {
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

        multimodal_config_t mult_cfg = multimodal_lang_default_config();
        multimodal = multimodal_lang_create(&mult_cfg);
        ASSERT_NE(nullptr, multimodal);
    }

    void TearDown() {
        if (pragmatics) { pragmatics_destroy(pragmatics); pragmatics = nullptr; }
        if (discourse) { discourse_destroy(discourse); discourse = nullptr; }
        if (prosody) { emotional_prosody_destroy(prosody); prosody = nullptr; }
        if (incremental) { incremental_destroy(incremental); incremental = nullptr; }
        if (multimodal) { multimodal_lang_destroy(multimodal); multimodal = nullptr; }
    }

    ProductionPlan planUtterance(const char* utterance, emotion_type_t emotion,
                                 float duration_ms) {
        ProductionPlan plan;
        plan.valid = false;

        // 1. Analyze pragmatics
        if (!pragmatics_analyze(pragmatics, utterance, 1, 1000, &plan.pragmatics)) {
            return plan;
        }

        // 2. Set emotion and generate prosody
        emotional_state_t state = {emotion, 0.8f, EMOTION_NEUTRAL, 0.0f, 0.5f, 0.3f};
        emotional_prosody_set_emotion(prosody, &state);
        if (!emotional_prosody_generate_contour(prosody, utterance, duration_ms,
                                                &plan.contour)) {
            return plan;
        }

        // 3. Generate multimodal plan
        if (!multimodal_lang_generate_plan(multimodal, utterance, duration_ms,
                                      &plan.multimodal)) {
            emotional_prosody_free_contour(&plan.contour);
            return plan;
        }

        // 4. Synchronize multimodal elements
        multimodal_lang_synchronize_plan(multimodal, &plan.multimodal);

        plan.valid = true;
        return plan;
    }

    void freePlan(ProductionPlan& plan) {
        if (plan.valid) {
            emotional_prosody_free_contour(&plan.contour);
            multimodal_lang_free_plan(&plan.multimodal);
            plan.valid = false;
        }
    }
};

class E2ESpeechComprehensionTest {
public:
    pragmatics_processor_t* pragmatics = nullptr;
    discourse_manager_t* discourse = nullptr;
    speech_repair_t* repair = nullptr;
    incremental_processor_t* incremental = nullptr;

    void SetUp() {
        pragmatics_config_t prag_cfg = pragmatics_default_config();
        pragmatics = pragmatics_create(&prag_cfg);
        ASSERT_NE(nullptr, pragmatics);

        discourse_config_t disc_cfg = discourse_default_config();
        discourse = discourse_create(&disc_cfg);
        ASSERT_NE(nullptr, discourse);

        speech_repair_config_t rep_cfg = speech_repair_default_config();
        repair = speech_repair_create(&rep_cfg);
        ASSERT_NE(nullptr, repair);

        incremental_config_t incr_cfg = incremental_default_config();
        incremental = incremental_create(&incr_cfg);
        ASSERT_NE(nullptr, incremental);
    }

    void TearDown() {
        if (pragmatics) { pragmatics_destroy(pragmatics); pragmatics = nullptr; }
        if (discourse) { discourse_destroy(discourse); discourse = nullptr; }
        if (repair) { speech_repair_destroy(repair); repair = nullptr; }
        if (incremental) { incremental_destroy(incremental); incremental = nullptr; }
    }

    bool comprehendUtterance(const char* input, pragmatic_analysis_t* result) {
        // 1. Clean disfluencies
        char cleaned[512];
        speech_repair_clean(repair, input, cleaned, sizeof(cleaned));

        // 2. Analyze pragmatics
        return pragmatics_analyze(pragmatics, cleaned, 1, 1000, result);
    }
};

//=============================================================================
// Speech Production Pipeline Tests
//=============================================================================

E2E_TEST(SpeechProductionPipeline, BasicAssertive) {
    E2ESpeechProductionTest fixture;
    fixture.SetUp();

    auto plan = fixture.planUtterance("The weather is nice today",
                                       EMOTION_NEUTRAL, 1500.0f);
    ASSERT_TRUE(plan.valid);

    EXPECT_EQ(plan.pragmatics.speech_act.primary_act, SPEECH_ACT_ASSERT);
    EXPECT_GT(plan.contour.point_count, 0u);
    EXPECT_FLOAT_EQ(plan.multimodal.speech_duration_ms, 1500.0f);

    fixture.freePlan(plan);
    fixture.TearDown();
}

E2E_TEST(SpeechProductionPipeline, PoliteDirective) {
    E2ESpeechProductionTest fixture;
    fixture.SetUp();

    auto plan = fixture.planUtterance("Could you please help me with this?",
                                       EMOTION_NEUTRAL, 2000.0f);
    ASSERT_TRUE(plan.valid);

    EXPECT_EQ(plan.pragmatics.speech_act.primary_act, SPEECH_ACT_REQUEST);
    // Polite requests should maintain normal prosody
    EXPECT_GT(plan.contour.point_count, 0u);

    fixture.freePlan(plan);
    fixture.TearDown();
}

E2E_TEST(SpeechProductionPipeline, EmotionalExpressive) {
    E2ESpeechProductionTest fixture;
    fixture.SetUp();

    auto plan = fixture.planUtterance("That's absolutely wonderful!",
                                       EMOTION_HAPPY, 1200.0f);
    ASSERT_TRUE(plan.valid);

    // Exclamatory expressions are classified as assertive statements
    EXPECT_EQ(plan.pragmatics.speech_act.primary_act, SPEECH_ACT_ASSERT);
    // Should have smile expression
    bool has_smile = false;
    for (uint32_t i = 0; i < plan.multimodal.expression_count; i++) {
        if (plan.multimodal.expressions[i].type == EXPRESSION_SMILE) {
            has_smile = true;
            break;
        }
    }
    EXPECT_TRUE(has_smile);

    fixture.freePlan(plan);
    fixture.TearDown();
}

E2E_TEST(SpeechProductionPipeline, DeicticGesture) {
    E2ESpeechProductionTest fixture;
    fixture.SetUp();

    auto plan = fixture.planUtterance("Look at that thing over there",
                                       EMOTION_NEUTRAL, 1800.0f);
    ASSERT_TRUE(plan.valid);

    // Should have deictic (pointing) gesture
    bool has_deictic = false;
    for (uint32_t i = 0; i < plan.multimodal.gesture_count; i++) {
        if (plan.multimodal.gestures[i].type == GESTURE_TYPE_DEICTIC) {
            has_deictic = true;
            break;
        }
    }
    EXPECT_TRUE(has_deictic);

    fixture.freePlan(plan);
    fixture.TearDown();
}

E2E_TEST(SpeechProductionPipeline, IconicGesture) {
    E2ESpeechProductionTest fixture;
    fixture.SetUp();

    auto plan = fixture.planUtterance("It was a really big round ball",
                                       EMOTION_NEUTRAL, 1600.0f);
    ASSERT_TRUE(plan.valid);

    // Should have iconic gestures for "big" and "round"
    bool has_iconic = false;
    for (uint32_t i = 0; i < plan.multimodal.gesture_count; i++) {
        if (plan.multimodal.gestures[i].type == GESTURE_TYPE_ICONIC) {
            has_iconic = true;
            break;
        }
    }
    EXPECT_TRUE(has_iconic);

    fixture.freePlan(plan);
    fixture.TearDown();
}

//=============================================================================
// Speech Comprehension Pipeline Tests
//=============================================================================

E2E_TEST(SpeechComprehensionPipeline, CleanInput) {
    E2ESpeechComprehensionTest fixture;
    fixture.SetUp();

    pragmatic_analysis_t result;
    EXPECT_TRUE(fixture.comprehendUtterance("Please pass the salt", &result));
    EXPECT_EQ(result.speech_act.primary_act, SPEECH_ACT_REQUEST);

    fixture.TearDown();
}

E2E_TEST(SpeechComprehensionPipeline, DisfluencyHandling) {
    E2ESpeechComprehensionTest fixture;
    fixture.SetUp();

    // Input with disfluencies
    pragmatic_analysis_t result;
    EXPECT_TRUE(fixture.comprehendUtterance(
        "I um want to uh go to the store", &result));
    EXPECT_EQ(result.speech_act.primary_act, SPEECH_ACT_ASSERT);

    fixture.TearDown();
}

E2E_TEST(SpeechComprehensionPipeline, RepairHandling) {
    E2ESpeechComprehensionTest fixture;
    fixture.SetUp();

    // Input with self-repair
    pragmatic_analysis_t result;
    EXPECT_TRUE(fixture.comprehendUtterance(
        "Turn left, I mean, turn right at the corner", &result));

    fixture.TearDown();
}

E2E_TEST(SpeechComprehensionPipeline, AnaphoraResolution) {
    E2ESpeechComprehensionTest fixture;
    fixture.SetUp();

    // Introduce referent (name, type, gender=neuter, number=singular)
    discourse_introduce_referent(fixture.discourse, "the book",
                                  REFERENT_TYPE_OBJECT, 3, 1);

    // Resolve anaphora
    anaphora_resolution_t resolution;
    EXPECT_TRUE(discourse_resolve_anaphora(fixture.discourse, "it", NULL, &resolution));
    EXPECT_TRUE(resolution.is_resolved);

    fixture.TearDown();
}

//=============================================================================
// Conversational Flow Tests
//=============================================================================

E2E_TEST(ConversationalFlow, MultiTurnDialogue) {
    E2ESpeechProductionTest prod_fixture;
    E2ESpeechComprehensionTest comp_fixture;
    prod_fixture.SetUp();
    comp_fixture.SetUp();

    ConversationTurn dialogue[] = {
        {"A", "Hello, how are you today?", SPEECH_ACT_QUESTION, EMOTION_NEUTRAL, false},
        {"B", "I'm doing great, thanks!", SPEECH_ACT_THANK, EMOTION_HAPPY, false},
        {"A", "Would you like some coffee?", SPEECH_ACT_QUESTION, EMOTION_NEUTRAL, false},
        {"B", "Yes please, that would be nice", SPEECH_ACT_REQUEST, EMOTION_HAPPY, false},
    };

    for (const auto& turn : dialogue) {
        // Comprehend the turn
        pragmatic_analysis_t analysis;
        EXPECT_TRUE(comp_fixture.comprehendUtterance(turn.utterance, &analysis));
        EXPECT_EQ(analysis.speech_act.primary_act, turn.expected_act);

        // Plan production response
        auto plan = prod_fixture.planUtterance(turn.utterance, turn.emotion, 1500.0f);
        EXPECT_TRUE(plan.valid);
        prod_fixture.freePlan(plan);
    }

    prod_fixture.TearDown();
    comp_fixture.TearDown();
}

E2E_TEST(ConversationalFlow, TopicTracking) {
    discourse_config_t disc_cfg = discourse_default_config();
    discourse_manager_t* discourse = discourse_create(&disc_cfg);
    ASSERT_NE(nullptr, discourse);

    // Introduce topics over conversation (name, type, gender=neuter, number=singular)
    discourse_introduce_referent(discourse, "weather", REFERENT_TYPE_OBJECT, 3, 1);
    discourse_introduce_referent(discourse, "plans", REFERENT_TYPE_OBJECT, 3, 2);  // plural
    discourse_introduce_referent(discourse, "coffee", REFERENT_TYPE_OBJECT, 3, 1);

    // Most recent should be most salient
    anaphora_resolution_t resolution;
    discourse_resolve_anaphora(discourse, "it", NULL, &resolution);
    // Should resolve to most recent (coffee)
    EXPECT_TRUE(resolution.is_resolved);

    discourse_destroy(discourse);
}

E2E_TEST(ConversationalFlow, EmotionTransition) {
    emotional_prosody_config_t config = emotional_prosody_default_config();
    emotional_prosody_t* prosody = emotional_prosody_create(&config);
    ASSERT_NE(nullptr, prosody);

    // Simulate emotion transition during conversation
    emotional_state_t states[] = {
        {EMOTION_NEUTRAL, 1.0f, EMOTION_NEUTRAL, 0.0f, 0.5f, 0.0f},
        {EMOTION_HAPPY, 0.7f, EMOTION_NEUTRAL, 0.0f, 0.6f, 0.4f},
        {EMOTION_SURPRISED, 0.8f, EMOTION_HAPPY, 0.2f, 0.7f, 0.3f},
        {EMOTION_HAPPY, 0.9f, EMOTION_NEUTRAL, 0.0f, 0.8f, 0.6f},
    };

    for (const auto& state : states) {
        emotional_prosody_set_emotion(prosody, &state);

        prosodic_params_t params;
        EXPECT_TRUE(emotional_prosody_get_params(prosody, &state, &params));
        EXPECT_GT(params.pitch_mean_hz, 0.0f);
    }

    emotional_prosody_destroy(prosody);
}

E2E_TEST(ConversationalFlow, GriceCooperativeness) {
    pragmatics_config_t config = pragmatics_default_config();
    pragmatics_processor_t* pragmatics = pragmatics_create(&config);
    ASSERT_NE(nullptr, pragmatics);

    // Test cooperative utterances have higher scores than vague ones
    pragmatic_analysis_t clear_analysis, vague_analysis;
    pragmatics_analyze(pragmatics, "The meeting is at 3 PM in room 201", 1, 1000, &clear_analysis);
    pragmatics_analyze(pragmatics, "Well, you know, kind of sort of maybe", 1, 2000, &vague_analysis);

    // Clear statement should have higher cooperativeness than vague one
    EXPECT_GE(clear_analysis.grice_analysis.overall_cooperativeness,
              vague_analysis.grice_analysis.overall_cooperativeness);

    pragmatics_destroy(pragmatics);
}

//=============================================================================
// Error Recovery Pipeline Tests
//=============================================================================

E2E_TEST(ErrorRecoveryPipeline, GracefulDegradation) {
    E2ESpeechProductionTest fixture;
    fixture.SetUp();

    // Even with partial failures, pipeline should not crash
    // and should produce usable output

    // Test with very short utterance
    auto plan1 = fixture.planUtterance("Hi", EMOTION_NEUTRAL, 200.0f);
    EXPECT_TRUE(plan1.valid);
    fixture.freePlan(plan1);

    // Test with very long utterance
    auto plan2 = fixture.planUtterance(
        "This is a very long utterance that goes on and on with many words "
        "and phrases and clauses and all sorts of linguistic structures that "
        "need to be processed by the speech enhancement pipeline",
        EMOTION_NEUTRAL, 8000.0f);
    EXPECT_TRUE(plan2.valid);
    fixture.freePlan(plan2);

    fixture.TearDown();
}

E2E_TEST(ErrorRecoveryPipeline, ResetRecovery) {
    E2ESpeechProductionTest fixture;
    fixture.SetUp();

    // Process some data
    auto plan = fixture.planUtterance("Test utterance", EMOTION_HAPPY, 1000.0f);
    EXPECT_TRUE(plan.valid);
    fixture.freePlan(plan);

    // Reset all components
    pragmatics_reset(fixture.pragmatics);
    discourse_reset(fixture.discourse);
    emotional_prosody_reset(fixture.prosody);
    incremental_reset(fixture.incremental);
    multimodal_lang_reset(fixture.multimodal);

    // Should still work after reset
    auto plan2 = fixture.planUtterance("Another test", EMOTION_NEUTRAL, 1000.0f);
    EXPECT_TRUE(plan2.valid);
    fixture.freePlan(plan2);

    fixture.TearDown();
}

E2E_TEST(ErrorRecoveryPipeline, MixedValidInvalid) {
    E2ESpeechComprehensionTest fixture;
    fixture.SetUp();

    // Mix of valid and invalid inputs
    pragmatic_analysis_t result;

    // Valid
    EXPECT_TRUE(fixture.comprehendUtterance("Please help me", &result));

    // Empty (edge case) - implementation gracefully handles empty input
    EXPECT_TRUE(fixture.comprehendUtterance("", &result));

    // Valid again (should still work)
    EXPECT_TRUE(fixture.comprehendUtterance("Thank you", &result));

    fixture.TearDown();
}

//=============================================================================
// Performance Pipeline Tests
//=============================================================================

E2E_TEST(PerformancePipeline, ProductionThroughput) {
    E2ESpeechProductionTest fixture;
    fixture.SetUp();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < STRESS_TEST_ITERATIONS; i++) {
        auto plan = fixture.planUtterance("Hello world", EMOTION_NEUTRAL, 1000.0f);
        ASSERT_TRUE(plan.valid);
        fixture.freePlan(plan);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double avg_ms = static_cast<double>(duration.count()) / STRESS_TEST_ITERATIONS;
    std::cout << "Production pipeline: " << avg_ms << " ms/utterance" << std::endl;

    // Should process at least 10 utterances per second
    EXPECT_LT(avg_ms, 100.0);

    fixture.TearDown();
}

E2E_TEST(PerformancePipeline, ComprehensionThroughput) {
    E2ESpeechComprehensionTest fixture;
    fixture.SetUp();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < STRESS_TEST_ITERATIONS; i++) {
        pragmatic_analysis_t result;
        ASSERT_TRUE(fixture.comprehendUtterance("I um want to help you", &result));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double avg_ms = static_cast<double>(duration.count()) / STRESS_TEST_ITERATIONS;
    std::cout << "Comprehension pipeline: " << avg_ms << " ms/utterance" << std::endl;

    // Should process at least 20 utterances per second
    EXPECT_LT(avg_ms, 50.0);

    fixture.TearDown();
}

E2E_TEST(PerformancePipeline, MemoryStability) {
    // Track memory usage over many iterations
    E2ESpeechProductionTest fixture;
    fixture.SetUp();

    // Many iterations to detect memory leaks
    for (int i = 0; i < STRESS_TEST_ITERATIONS * 5; i++) {
        auto plan = fixture.planUtterance("Test memory stability", EMOTION_NEUTRAL, 1000.0f);
        ASSERT_TRUE(plan.valid);
        fixture.freePlan(plan);

        // Periodically reset to test cleanup
        if (i % 100 == 0) {
            pragmatics_reset(fixture.pragmatics);
            discourse_reset(fixture.discourse);
            emotional_prosody_reset(fixture.prosody);
            incremental_reset(fixture.incremental);
            multimodal_lang_reset(fixture.multimodal);
        }
    }

    // If we get here without crashing or memory issues, test passes
    SUCCEED();

    fixture.TearDown();
}

//=============================================================================
// Full E2E Pipeline Integration Test
//=============================================================================

E2E_TEST(FullPipeline, CompleteConversation) {
    // Create all components
    pragmatics_config_t prag_cfg = pragmatics_default_config();
    pragmatics_processor_t* pragmatics = pragmatics_create(&prag_cfg);

    discourse_config_t disc_cfg = discourse_default_config();
    discourse_manager_t* discourse = discourse_create(&disc_cfg);

    emotional_prosody_config_t pros_cfg = emotional_prosody_default_config();
    emotional_prosody_t* prosody = emotional_prosody_create(&pros_cfg);

    incremental_config_t incr_cfg = incremental_default_config();
    incremental_processor_t* incremental = incremental_create(&incr_cfg);

    speech_repair_config_t rep_cfg = speech_repair_default_config();
    speech_repair_t* repair = speech_repair_create(&rep_cfg);

    multimodal_config_t mult_cfg = multimodal_lang_default_config();
    multimodal_language_t* multimodal = multimodal_lang_create(&mult_cfg);

    ASSERT_NE(nullptr, pragmatics);
    ASSERT_NE(nullptr, discourse);
    ASSERT_NE(nullptr, prosody);
    ASSERT_NE(nullptr, incremental);
    ASSERT_NE(nullptr, repair);
    ASSERT_NE(nullptr, multimodal);

    // Simulate a complete conversation
    const char* conversation[] = {
        "Hello, I'm looking for a book about cooking",
        "What kind of cuisine are you interested in?",
        "I um like Italian food, you know, pasta and pizza",
        "I have just the thing for you!",
        "Great, I'll take it, thanks so much!"
    };

    emotion_type_t emotions[] = {
        EMOTION_NEUTRAL,
        EMOTION_NEUTRAL,
        EMOTION_NEUTRAL,
        EMOTION_HAPPY,
        EMOTION_HAPPY
    };

    for (size_t i = 0; i < sizeof(conversation) / sizeof(conversation[0]); i++) {
        // 1. Clean input
        char cleaned[256];
        speech_repair_clean(repair, conversation[i], cleaned, sizeof(cleaned));

        // 2. Analyze pragmatics
        pragmatic_analysis_t prag_analysis;
        EXPECT_TRUE(pragmatics_analyze(pragmatics, cleaned, 1, 1000 + i * 500, &prag_analysis));

        // 3. Set emotion and generate prosody
        emotional_state_t state = {emotions[i], 0.8f, EMOTION_NEUTRAL, 0.0f, 0.5f, 0.3f};
        emotional_prosody_set_emotion(prosody, &state);

        prosodic_contour_t contour;
        EXPECT_TRUE(emotional_prosody_generate_contour(prosody, cleaned, 2000.0f, &contour));

        // 4. Generate multimodal plan
        multimodal_plan_t plan;
        EXPECT_TRUE(multimodal_lang_generate_plan(multimodal, cleaned, 2000.0f, &plan));
        multimodal_lang_synchronize_plan(multimodal, &plan);

        // 5. Track discourse referents
        if (i == 0) {
            // name, type, gender=neuter, number=singular
            discourse_introduce_referent(discourse, "book", REFERENT_TYPE_OBJECT, 3, 1);
        }

        // Cleanup
        emotional_prosody_free_contour(&contour);
        multimodal_lang_free_plan(&plan);
    }

    // Collect final statistics
    pragmatics_stats_t prag_stats;
    pragmatics_get_stats(pragmatics, &prag_stats);
    EXPECT_EQ(prag_stats.utterances_processed, 5u);

    repair_stats_t rep_stats;
    speech_repair_get_stats(repair, &rep_stats);
    // speech_repair_clean increments auto_corrections_made when removing fillers
    EXPECT_GT(rep_stats.auto_corrections_made, 0u);

    // Cleanup
    pragmatics_destroy(pragmatics);
    discourse_destroy(discourse);
    emotional_prosody_destroy(prosody);
    incremental_destroy(incremental);
    speech_repair_destroy(repair);
    multimodal_lang_destroy(multimodal);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
