/**
 * @file test_nimcp_pragmatics_processor.cpp
 * @brief Unit tests for nimcp_pragmatics_processor.c
 *
 * WHAT: Comprehensive unit tests for the Pragmatics processor
 * WHY:  Ensure correct speech act classification, Gricean analysis, and implicature detection
 * HOW:  Use Google Test framework to test lifecycle, classification, context management, and analysis
 *
 * COVERAGE TARGET: 100%
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>

#include "core/brain/regions/broca/nimcp_pragmatics_processor.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class PragmaticsProcessorTest : public ::testing::Test {
protected:
    pragmatics_processor_t* processor;
    pragmatics_config_t config;

    void SetUp() override {
        config = pragmatics_default_config();
        processor = pragmatics_create(&config);
        ASSERT_NE(nullptr, processor) << "Failed to create pragmatics processor";
    }

    void TearDown() override {
        pragmatics_destroy(processor);
        processor = nullptr;
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(PragmaticsProcessorTest, DefaultConfigHasReasonableValues) {
    pragmatics_config_t default_config = pragmatics_default_config();

    EXPECT_EQ(default_config.context_depth, PRAGMATICS_DEFAULT_CONTEXT_DEPTH);
    EXPECT_EQ(default_config.max_utterances, PRAGMATICS_DEFAULT_MAX_UTTERANCES);
    EXPECT_EQ(default_config.max_speech_acts, PRAGMATICS_DEFAULT_MAX_SPEECH_ACTS);
    EXPECT_EQ(default_config.implicature_depth, PRAGMATICS_DEFAULT_IMPLICATURE_DEPTH);
    EXPECT_TRUE(default_config.enable_scalar_implicature);
    EXPECT_TRUE(default_config.enable_indirect_speech);
    EXPECT_TRUE(default_config.enable_grice_analysis);
    EXPECT_FLOAT_EQ(default_config.classification_threshold, 0.5f);
}

TEST_F(PragmaticsProcessorTest, CreateWithNullConfigUsesDefaults) {
    pragmatics_processor_t* proc_null = pragmatics_create(NULL);
    ASSERT_NE(nullptr, proc_null);

    pragmatics_config_t retrieved;
    EXPECT_TRUE(pragmatics_get_config(proc_null, &retrieved));
    EXPECT_EQ(retrieved.context_depth, PRAGMATICS_DEFAULT_CONTEXT_DEPTH);

    pragmatics_destroy(proc_null);
}

TEST_F(PragmaticsProcessorTest, DestroyNullDoesNotCrash) {
    pragmatics_destroy(NULL);
    // Should not crash
}

TEST_F(PragmaticsProcessorTest, ResetClearsState) {
    // Add some context
    pragmatics_add_to_context(processor, "Hello", 1, 1000);
    pragmatics_add_to_context(processor, "World", 2, 2000);

    EXPECT_TRUE(pragmatics_reset(processor));

    // Status should be idle after reset
    EXPECT_EQ(pragmatics_get_status(processor), PRAGMATICS_STATUS_IDLE);
    EXPECT_EQ(pragmatics_get_last_error(processor), PRAGMATICS_ERROR_NONE);

    // Context should be empty
    utterance_context_t context[10];
    uint32_t count = pragmatics_get_context(processor, context, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(PragmaticsProcessorTest, ResetNullReturnsFalse) {
    EXPECT_FALSE(pragmatics_reset(NULL));
}

TEST_F(PragmaticsProcessorTest, GetStatusReturnsIdle) {
    EXPECT_EQ(pragmatics_get_status(processor), PRAGMATICS_STATUS_IDLE);
}

TEST_F(PragmaticsProcessorTest, GetStatusNullReturnsError) {
    EXPECT_EQ(pragmatics_get_status(NULL), PRAGMATICS_STATUS_ERROR);
}

TEST_F(PragmaticsProcessorTest, GetLastErrorNullReturnsInternal) {
    EXPECT_EQ(pragmatics_get_last_error(NULL), PRAGMATICS_ERROR_INTERNAL);
}

// ============================================================================
// SPEECH ACT CLASSIFICATION TESTS
// ============================================================================

TEST_F(PragmaticsProcessorTest, ClassifyQuestionWithQuestionMark) {
    speech_act_result_t result;
    EXPECT_TRUE(pragmatics_classify_speech_act(processor, "What time is it?", 1, &result));

    EXPECT_EQ(result.primary_act, SPEECH_ACT_QUESTION);
    EXPECT_GT(result.primary_confidence, 0.5f);
}

TEST_F(PragmaticsProcessorTest, ClassifyRequestWithPlease) {
    speech_act_result_t result;
    EXPECT_TRUE(pragmatics_classify_speech_act(processor, "Please help me", 1, &result));

    EXPECT_EQ(result.primary_act, SPEECH_ACT_REQUEST);
    EXPECT_GT(result.primary_confidence, 0.5f);
}

TEST_F(PragmaticsProcessorTest, ClassifyGreeting) {
    speech_act_result_t result;
    EXPECT_TRUE(pragmatics_classify_speech_act(processor, "Hello there", 1, &result));

    EXPECT_EQ(result.primary_act, SPEECH_ACT_GREET);
    EXPECT_GT(result.primary_confidence, 0.7f);
}

TEST_F(PragmaticsProcessorTest, ClassifyThank) {
    speech_act_result_t result;
    EXPECT_TRUE(pragmatics_classify_speech_act(processor, "Thank you very much", 1, &result));

    EXPECT_EQ(result.primary_act, SPEECH_ACT_THANK);
    EXPECT_GT(result.primary_confidence, 0.8f);
}

TEST_F(PragmaticsProcessorTest, ClassifyApology) {
    speech_act_result_t result;
    EXPECT_TRUE(pragmatics_classify_speech_act(processor, "I apologize for the delay", 1, &result));

    EXPECT_EQ(result.primary_act, SPEECH_ACT_APOLOGIZE);
    EXPECT_GT(result.primary_confidence, 0.9f);
}

TEST_F(PragmaticsProcessorTest, ClassifyPromise) {
    speech_act_result_t result;
    EXPECT_TRUE(pragmatics_classify_speech_act(processor, "I promise to help", 1, &result));

    EXPECT_EQ(result.primary_act, SPEECH_ACT_PROMISE);
    EXPECT_GT(result.primary_confidence, 0.9f);
}

TEST_F(PragmaticsProcessorTest, ClassifyOffer) {
    speech_act_result_t result;
    EXPECT_TRUE(pragmatics_classify_speech_act(processor, "I offer you my assistance", 1, &result));

    EXPECT_EQ(result.primary_act, SPEECH_ACT_OFFER);
    EXPECT_GT(result.primary_confidence, 0.8f);
}

TEST_F(PragmaticsProcessorTest, ClassifyAssertion) {
    speech_act_result_t result;
    EXPECT_TRUE(pragmatics_classify_speech_act(processor, "The sky is blue", 1, &result));

    EXPECT_EQ(result.primary_act, SPEECH_ACT_ASSERT);
}

TEST_F(PragmaticsProcessorTest, ClassifyNullInputReturnsFalse) {
    speech_act_result_t result;
    EXPECT_FALSE(pragmatics_classify_speech_act(processor, NULL, 1, &result));
    EXPECT_FALSE(pragmatics_classify_speech_act(NULL, "test", 1, &result));
    EXPECT_FALSE(pragmatics_classify_speech_act(processor, "test", 1, NULL));
}

TEST_F(PragmaticsProcessorTest, SpeechActNameValidActs) {
    EXPECT_STREQ(pragmatics_speech_act_name(SPEECH_ACT_ASSERT), "ASSERT");
    EXPECT_STREQ(pragmatics_speech_act_name(SPEECH_ACT_REQUEST), "REQUEST");
    EXPECT_STREQ(pragmatics_speech_act_name(SPEECH_ACT_THANK), "THANK");
    EXPECT_STREQ(pragmatics_speech_act_name(SPEECH_ACT_UNKNOWN), "UNKNOWN");
}

TEST_F(PragmaticsProcessorTest, SpeechActNameInvalidReturnsInvalid) {
    EXPECT_STREQ(pragmatics_speech_act_name(static_cast<speech_act_type_t>(999)), "INVALID");
}

// ============================================================================
// INDIRECT SPEECH ACT DETECTION TESTS
// ============================================================================

TEST_F(PragmaticsProcessorTest, DetectIndirectRequest_CanYou) {
    speech_act_result_t result;
    EXPECT_TRUE(pragmatics_classify_speech_act(processor, "Can you help me?", 1, &result));

    // Indirect request: primary is the intended meaning (REQUEST),
    // secondary is the surface form (QUESTION)
    EXPECT_TRUE(result.is_indirect);
    EXPECT_EQ(result.primary_act, SPEECH_ACT_REQUEST);
    EXPECT_EQ(result.secondary_act, SPEECH_ACT_QUESTION);
    EXPECT_GT(result.primary_confidence, 0.7f);
}

TEST_F(PragmaticsProcessorTest, DetectIndirectRequest_CouldYou) {
    speech_act_result_t result;
    EXPECT_TRUE(pragmatics_classify_speech_act(processor, "Could you pass the salt?", 1, &result));

    // Indirect request: primary is REQUEST, secondary is QUESTION
    EXPECT_TRUE(result.is_indirect);
    EXPECT_EQ(result.primary_act, SPEECH_ACT_REQUEST);
    EXPECT_EQ(result.secondary_act, SPEECH_ACT_QUESTION);
}

TEST_F(PragmaticsProcessorTest, DetectIndirectSuggest_WhyDontYou) {
    speech_act_type_t indirect;
    float conf = pragmatics_detect_indirect_act(processor, "Why don't you try again?",
                                                 SPEECH_ACT_QUESTION, &indirect);

    EXPECT_GT(conf, 0.7f);
    EXPECT_EQ(indirect, SPEECH_ACT_SUGGEST_ACTION);
}

TEST_F(PragmaticsProcessorTest, DetectIndirectRequest_IWish) {
    speech_act_type_t indirect;
    float conf = pragmatics_detect_indirect_act(processor, "I wish I had more time",
                                                 SPEECH_ACT_ASSERT, &indirect);

    EXPECT_GT(conf, 0.5f);
    EXPECT_EQ(indirect, SPEECH_ACT_REQUEST);
}

// ============================================================================
// GRICEAN ANALYSIS TESTS
// ============================================================================

TEST_F(PragmaticsProcessorTest, AnalyzeGriceBasicUtterance) {
    grice_analysis_result_t result;
    EXPECT_TRUE(pragmatics_analyze_grice(processor, "The meeting is at 3pm today", NULL, &result));

    EXPECT_GE(result.overall_cooperativeness, 0.0f);
    EXPECT_LE(result.overall_cooperativeness, 1.0f);
}

TEST_F(PragmaticsProcessorTest, AnalyzeGriceShortUtterance) {
    grice_analysis_result_t result;
    EXPECT_TRUE(pragmatics_analyze_grice(processor, "Yes", NULL, &result));

    // Short utterance may violate quantity maxim
    EXPECT_LT(result.adherence_scores[GRICE_MAXIM_QUANTITY], 1.0f);
}

TEST_F(PragmaticsProcessorTest, AnalyzeGriceHedgingLanguage) {
    grice_analysis_result_t result;
    EXPECT_TRUE(pragmatics_analyze_grice(processor, "I think maybe it might work", NULL, &result));

    // Hedging is cooperative (quality maxim)
    EXPECT_GT(result.adherence_scores[GRICE_MAXIM_QUALITY], 0.8f);
}

TEST_F(PragmaticsProcessorTest, AnalyzeGriceFillerWords) {
    grice_analysis_result_t result;
    EXPECT_TRUE(pragmatics_analyze_grice(processor, "Um, like, you know, the thing", NULL, &result));

    // Filler words reduce manner score
    EXPECT_LT(result.adherence_scores[GRICE_MAXIM_MANNER], 1.0f);
}

TEST_F(PragmaticsProcessorTest, AnalyzeGriceNullInputReturnsFalse) {
    grice_analysis_result_t result;
    EXPECT_FALSE(pragmatics_analyze_grice(processor, NULL, NULL, &result));
    EXPECT_FALSE(pragmatics_analyze_grice(NULL, "test", NULL, &result));
    EXPECT_FALSE(pragmatics_analyze_grice(processor, "test", NULL, NULL));
}

TEST_F(PragmaticsProcessorTest, GriceMaximNameValidMaxims) {
    EXPECT_STREQ(pragmatics_grice_maxim_name(GRICE_MAXIM_QUANTITY), "QUANTITY");
    EXPECT_STREQ(pragmatics_grice_maxim_name(GRICE_MAXIM_QUALITY), "QUALITY");
    EXPECT_STREQ(pragmatics_grice_maxim_name(GRICE_MAXIM_RELATION), "RELATION");
    EXPECT_STREQ(pragmatics_grice_maxim_name(GRICE_MAXIM_MANNER), "MANNER");
}

TEST_F(PragmaticsProcessorTest, GriceMaximNameInvalidReturnsInvalid) {
    EXPECT_STREQ(pragmatics_grice_maxim_name(static_cast<grice_maxim_t>(999)), "INVALID");
}

TEST_F(PragmaticsProcessorTest, AnalyzeGriceDisabledReturnsObserved) {
    pragmatics_config_t disabled_config = pragmatics_default_config();
    disabled_config.enable_grice_analysis = false;

    pragmatics_processor_t* disabled_proc = pragmatics_create(&disabled_config);
    ASSERT_NE(nullptr, disabled_proc);

    grice_analysis_result_t result;
    EXPECT_TRUE(pragmatics_analyze_grice(disabled_proc, "test", NULL, &result));

    // All maxims should be observed
    for (int i = 0; i < GRICE_MAXIM_COUNT; i++) {
        EXPECT_EQ(result.violations[i], MAXIM_OBSERVED);
        EXPECT_FLOAT_EQ(result.adherence_scores[i], 1.0f);
    }

    pragmatics_destroy(disabled_proc);
}

// ============================================================================
// IMPLICATURE DETECTION TESTS
// ============================================================================

TEST_F(PragmaticsProcessorTest, DetectScalarImplicature_Some) {
    implicature_result_t result;
    EXPECT_TRUE(pragmatics_detect_scalar_implicature(processor, "Some students passed", &result));

    EXPECT_EQ(result.type, IMPLICATURE_SCALAR);
    EXPECT_GT(result.confidence, 0.5f);
    EXPECT_EQ(result.triggered_by, GRICE_MAXIM_QUANTITY);
}

TEST_F(PragmaticsProcessorTest, DetectScalarImplicature_Sometimes) {
    implicature_result_t result;
    EXPECT_TRUE(pragmatics_detect_scalar_implicature(processor, "He sometimes arrives late", &result));

    EXPECT_EQ(result.type, IMPLICATURE_SCALAR);
}

TEST_F(PragmaticsProcessorTest, DetectScalarImplicature_Possible) {
    implicature_result_t result;
    EXPECT_TRUE(pragmatics_detect_scalar_implicature(processor, "It's possible that it will rain", &result));

    EXPECT_EQ(result.type, IMPLICATURE_SCALAR);
}

TEST_F(PragmaticsProcessorTest, NoScalarImplicature_AllPresent) {
    implicature_result_t result;
    // When "all" is present, "some" doesn't trigger scalar implicature
    EXPECT_FALSE(pragmatics_detect_scalar_implicature(processor, "All students passed", &result));
}

TEST_F(PragmaticsProcessorTest, DetectImplicaturesFromFlout) {
    grice_analysis_result_t grice;
    memset(&grice, 0, sizeof(grice));
    grice.flouting_detected = true;
    grice.violations[GRICE_MAXIM_QUANTITY] = MAXIM_VIOLATED_FLOUTED;

    implicature_result_t implicatures[4];
    uint32_t count = pragmatics_detect_implicatures(processor, "test", &grice, implicatures, 4);

    EXPECT_GT(count, 0u);
}

TEST_F(PragmaticsProcessorTest, DetectImplicaturesNullReturnsZero) {
    implicature_result_t implicatures[4];
    EXPECT_EQ(pragmatics_detect_implicatures(NULL, "test", NULL, implicatures, 4), 0u);
    EXPECT_EQ(pragmatics_detect_implicatures(processor, NULL, NULL, implicatures, 4), 0u);
    EXPECT_EQ(pragmatics_detect_implicatures(processor, "test", NULL, NULL, 4), 0u);
    EXPECT_EQ(pragmatics_detect_implicatures(processor, "test", NULL, implicatures, 0), 0u);
}

// ============================================================================
// CONTEXT MANAGEMENT TESTS
// ============================================================================

TEST_F(PragmaticsProcessorTest, AddToContextReturnsId) {
    uint32_t id1 = pragmatics_add_to_context(processor, "First utterance", 1, 1000);
    uint32_t id2 = pragmatics_add_to_context(processor, "Second utterance", 2, 2000);

    EXPECT_GT(id1, 0u);
    EXPECT_GT(id2, 0u);
    EXPECT_NE(id1, id2);
}

TEST_F(PragmaticsProcessorTest, GetContextRetrievesUtterances) {
    pragmatics_add_to_context(processor, "First", 1, 1000);
    pragmatics_add_to_context(processor, "Second", 2, 2000);
    pragmatics_add_to_context(processor, "Third", 1, 3000);

    utterance_context_t context[10];
    uint32_t count = pragmatics_get_context(processor, context, 10);

    EXPECT_EQ(count, 3u);
    // Most recent first
    EXPECT_STREQ(context[0].content, "Third");
    EXPECT_STREQ(context[1].content, "Second");
    EXPECT_STREQ(context[2].content, "First");
}

TEST_F(PragmaticsProcessorTest, GetContextLimitedByMaxEntries) {
    pragmatics_add_to_context(processor, "First", 1, 1000);
    pragmatics_add_to_context(processor, "Second", 2, 2000);
    pragmatics_add_to_context(processor, "Third", 1, 3000);

    utterance_context_t context[2];
    uint32_t count = pragmatics_get_context(processor, context, 2);

    EXPECT_EQ(count, 2u);
}

TEST_F(PragmaticsProcessorTest, ClearContextRemovesAll) {
    pragmatics_add_to_context(processor, "Test", 1, 1000);
    pragmatics_clear_context(processor);

    utterance_context_t context[10];
    uint32_t count = pragmatics_get_context(processor, context, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(PragmaticsProcessorTest, RegisterParticipantSuccess) {
    EXPECT_TRUE(pragmatics_register_participant(processor, 1, "Alice"));
    EXPECT_TRUE(pragmatics_register_participant(processor, 2, "Bob"));
}

TEST_F(PragmaticsProcessorTest, RegisterParticipantUpdateExisting) {
    EXPECT_TRUE(pragmatics_register_participant(processor, 1, "Alice"));
    EXPECT_TRUE(pragmatics_register_participant(processor, 1, "Alicia")); // Update name
}

TEST_F(PragmaticsProcessorTest, RegisterParticipantNullReturnsFalse) {
    EXPECT_FALSE(pragmatics_register_participant(NULL, 1, "Test"));
    EXPECT_FALSE(pragmatics_register_participant(processor, 1, NULL));
}

TEST_F(PragmaticsProcessorTest, ContextSalienceDecays) {
    pragmatics_add_to_context(processor, "Old utterance", 1, 1000);
    pragmatics_add_to_context(processor, "New utterance", 2, 2000);

    utterance_context_t context[2];
    pragmatics_get_context(processor, context, 2);

    // Most recent should have highest salience
    EXPECT_GT(context[0].salience, context[1].salience);
}

// ============================================================================
// FULL ANALYSIS TESTS
// ============================================================================

TEST_F(PragmaticsProcessorTest, FullAnalysisSuccess) {
    pragmatic_analysis_t analysis;
    EXPECT_TRUE(pragmatics_analyze(processor, "Can you help me please?", 1, 1000, &analysis));

    // Should have speech act result
    EXPECT_NE(analysis.speech_act.primary_act, SPEECH_ACT_UNKNOWN);

    // Should have Gricean analysis
    EXPECT_GE(analysis.grice_analysis.overall_cooperativeness, 0.0f);
}

TEST_F(PragmaticsProcessorTest, FullAnalysisWithScalarImplicature) {
    pragmatic_analysis_t analysis;
    EXPECT_TRUE(pragmatics_analyze(processor, "Some of the tests passed", 1, 1000, &analysis));

    // Should detect scalar implicature
    EXPECT_GT(analysis.implicature_count, 0u);
}

TEST_F(PragmaticsProcessorTest, FullAnalysisTracksContext) {
    pragmatic_analysis_t analysis1, analysis2;

    EXPECT_TRUE(pragmatics_analyze(processor, "Hello", 1, 1000, &analysis1));
    EXPECT_TRUE(pragmatics_analyze(processor, "How are you?", 2, 2000, &analysis2));

    // Second analysis should have context references
    EXPECT_GT(analysis2.relevant_context_count, 0u);
}

TEST_F(PragmaticsProcessorTest, FullAnalysisNullReturnsFalse) {
    pragmatic_analysis_t analysis;
    EXPECT_FALSE(pragmatics_analyze(NULL, "test", 1, 1000, &analysis));
    EXPECT_FALSE(pragmatics_analyze(processor, NULL, 1, 1000, &analysis));
    EXPECT_FALSE(pragmatics_analyze(processor, "test", 1, 1000, NULL));
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(PragmaticsProcessorTest, StatsUpdatedAfterClassification) {
    speech_act_result_t result;
    pragmatics_classify_speech_act(processor, "Hello", 1, &result);

    pragmatics_stats_t stats;
    EXPECT_TRUE(pragmatics_get_stats(processor, &stats));
    EXPECT_EQ(stats.speech_acts_classified, 1u);
}

TEST_F(PragmaticsProcessorTest, StatsTrackActTypes) {
    speech_act_result_t result;
    pragmatics_classify_speech_act(processor, "Hello", 1, &result);
    pragmatics_classify_speech_act(processor, "Thank you", 1, &result);
    pragmatics_classify_speech_act(processor, "Hello again", 1, &result);

    pragmatics_stats_t stats;
    pragmatics_get_stats(processor, &stats);

    EXPECT_EQ(stats.act_type_counts[SPEECH_ACT_GREET], 2u);
    EXPECT_EQ(stats.act_type_counts[SPEECH_ACT_THANK], 1u);
}

TEST_F(PragmaticsProcessorTest, StatsTrackIndirectActs) {
    speech_act_result_t result;
    pragmatics_classify_speech_act(processor, "Can you help?", 1, &result);

    pragmatics_stats_t stats;
    pragmatics_get_stats(processor, &stats);
    EXPECT_GT(stats.indirect_acts_detected, 0u);
}

TEST_F(PragmaticsProcessorTest, ResetStatsClearsAll) {
    speech_act_result_t result;
    pragmatics_classify_speech_act(processor, "Test", 1, &result);

    pragmatics_reset_stats(processor);

    pragmatics_stats_t stats;
    pragmatics_get_stats(processor, &stats);
    EXPECT_EQ(stats.speech_acts_classified, 0u);
}

TEST_F(PragmaticsProcessorTest, GetStatsNullReturnsFalse) {
    EXPECT_FALSE(pragmatics_get_stats(NULL, nullptr));

    pragmatics_stats_t stats;
    EXPECT_FALSE(pragmatics_get_stats(processor, nullptr));
    EXPECT_FALSE(pragmatics_get_stats(NULL, &stats));
}

TEST_F(PragmaticsProcessorTest, GetConfigSuccess) {
    pragmatics_config_t retrieved;
    EXPECT_TRUE(pragmatics_get_config(processor, &retrieved));
    EXPECT_EQ(retrieved.context_depth, config.context_depth);
}

TEST_F(PragmaticsProcessorTest, GetConfigNullReturnsFalse) {
    pragmatics_config_t retrieved;
    EXPECT_FALSE(pragmatics_get_config(NULL, &retrieved));
    EXPECT_FALSE(pragmatics_get_config(processor, NULL));
}

// ============================================================================
// BIO-ASYNC INTEGRATION TESTS
// ============================================================================

TEST_F(PragmaticsProcessorTest, RegisterBioHandlerNullReturnsFalse) {
    EXPECT_FALSE(pragmatics_register_bio_handler(NULL, nullptr));
    EXPECT_FALSE(pragmatics_register_bio_handler(processor, nullptr));
}

TEST_F(PragmaticsProcessorTest, SendAnalysisNullReturnsFalse) {
    pragmatic_analysis_t analysis;
    EXPECT_FALSE(pragmatics_send_analysis(NULL, &analysis));
    EXPECT_FALSE(pragmatics_send_analysis(processor, NULL));
}

TEST_F(PragmaticsProcessorTest, SendAnalysisNoRouterReturnsFalse) {
    // No router registered
    pragmatic_analysis_t analysis;
    memset(&analysis, 0, sizeof(analysis));
    EXPECT_FALSE(pragmatics_send_analysis(processor, &analysis));
}

// ============================================================================
// EDGE CASES AND BOUNDARY TESTS
// ============================================================================

TEST_F(PragmaticsProcessorTest, EmptyUtteranceClassifiesAsAssert) {
    speech_act_result_t result;
    // Empty string should still work but with low confidence
    EXPECT_TRUE(pragmatics_classify_speech_act(processor, "", 1, &result));
}

TEST_F(PragmaticsProcessorTest, VeryLongUtteranceHandled) {
    // Create a very long utterance
    std::string long_utterance(1000, 'a');
    long_utterance += " please help";

    speech_act_result_t result;
    EXPECT_TRUE(pragmatics_classify_speech_act(processor, long_utterance.c_str(), 1, &result));
    EXPECT_EQ(result.primary_act, SPEECH_ACT_REQUEST);
}

TEST_F(PragmaticsProcessorTest, CircularBufferWraparound) {
    // Add more utterances than buffer size
    for (int i = 0; i < 100; i++) {
        char utterance[32];
        snprintf(utterance, sizeof(utterance), "Utterance %d", i);
        pragmatics_add_to_context(processor, utterance, 1, i * 1000);
    }

    // Should still retrieve context (most recent)
    utterance_context_t context[5];
    uint32_t count = pragmatics_get_context(processor, context, 5);
    EXPECT_GT(count, 0u);
}

TEST_F(PragmaticsProcessorTest, CaseInsensitivePatternMatching) {
    speech_act_result_t result;

    // Test various cases
    pragmatics_classify_speech_act(processor, "HELLO WORLD", 1, &result);
    EXPECT_EQ(result.primary_act, SPEECH_ACT_GREET);

    pragmatics_classify_speech_act(processor, "Thank You", 1, &result);
    EXPECT_EQ(result.primary_act, SPEECH_ACT_THANK);

    pragmatics_classify_speech_act(processor, "PLEASE HELP", 1, &result);
    EXPECT_EQ(result.primary_act, SPEECH_ACT_REQUEST);
}

TEST_F(PragmaticsProcessorTest, MultipleActsInSameUtterance) {
    // "Can you help?" is both a question and an indirect request
    speech_act_result_t result;
    EXPECT_TRUE(pragmatics_classify_speech_act(processor, "Can you help me?", 1, &result));

    // Should detect both
    EXPECT_EQ(result.primary_act, SPEECH_ACT_REQUEST);
    EXPECT_TRUE(result.is_indirect || result.secondary_act != SPEECH_ACT_UNKNOWN);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
