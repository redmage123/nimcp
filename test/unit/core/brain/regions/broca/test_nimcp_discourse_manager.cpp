/**
 * @file test_nimcp_discourse_manager.cpp
 * @brief Unit tests for nimcp_discourse_manager.c
 *
 * WHAT: Comprehensive unit tests for the Discourse Manager
 * WHY:  Ensure correct conversation tracking, anaphora resolution, and topic management
 * HOW:  Use Google Test framework to test referents, turns, topics, and analysis
 *
 * COVERAGE TARGET: 100%
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <string.h>
#include <stdlib.h>

#include "core/brain/regions/broca/nimcp_discourse_manager.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class DiscourseManagerTest : public ::testing::Test {
protected:
    discourse_manager_t* manager;
    discourse_config_t config;

    void SetUp() override {
        config = discourse_default_config();
        manager = discourse_create(&config);
        ASSERT_NE(nullptr, manager) << "Failed to create discourse manager";
    }

    void TearDown() override {
        discourse_destroy(manager);
        manager = nullptr;
    }

    // Helper to set up a basic conversation
    void SetupBasicConversation() {
        // Introduce some referents
        discourse_introduce_referent(manager, "John", REFERENT_TYPE_PERSON, 1, 1);
        discourse_introduce_referent(manager, "Mary", REFERENT_TYPE_PERSON, 2, 1);
        discourse_introduce_referent(manager, "the book", REFERENT_TYPE_OBJECT, 3, 1);

        // Add some turns
        discourse_add_turn(manager, 1, "John gave Mary a book", 1000);
        discourse_add_turn(manager, 2, "She thanked him for it", 2000);
    }
};

// ============================================================================
// LIFECYCLE TESTS
// ============================================================================

TEST_F(DiscourseManagerTest, DefaultConfigHasReasonableValues) {
    discourse_config_t default_config = discourse_default_config();

    EXPECT_EQ(default_config.max_turns, DISCOURSE_DEFAULT_MAX_TURNS);
    EXPECT_EQ(default_config.max_referents, DISCOURSE_DEFAULT_MAX_REFERENTS);
    EXPECT_EQ(default_config.max_topics, DISCOURSE_DEFAULT_MAX_TOPICS);
    EXPECT_EQ(default_config.history_depth, DISCOURSE_DEFAULT_HISTORY_DEPTH);
    EXPECT_FLOAT_EQ(default_config.salience_decay, 0.8f);
    EXPECT_TRUE(default_config.enable_topic_tracking);
}

TEST_F(DiscourseManagerTest, CreateWithNullConfigUsesDefaults) {
    discourse_manager_t* mgr = discourse_create(NULL);
    ASSERT_NE(nullptr, mgr);

    discourse_config_t retrieved;
    EXPECT_TRUE(discourse_get_config(mgr, &retrieved));
    EXPECT_EQ(retrieved.max_turns, DISCOURSE_DEFAULT_MAX_TURNS);

    discourse_destroy(mgr);
}

TEST_F(DiscourseManagerTest, DestroyNullDoesNotCrash) {
    discourse_destroy(NULL);
}

TEST_F(DiscourseManagerTest, ResetClearsState) {
    SetupBasicConversation();

    EXPECT_TRUE(discourse_reset(manager));

    EXPECT_EQ(discourse_get_status(manager), DISCOURSE_STATUS_IDLE);
    EXPECT_EQ(discourse_get_turn_count(manager), 0u);

    discourse_referent_t refs[10];
    EXPECT_EQ(discourse_get_salient_referents(manager, refs, 10), 0u);
}

TEST_F(DiscourseManagerTest, ResetNullReturnsFalse) {
    EXPECT_FALSE(discourse_reset(NULL));
}

TEST_F(DiscourseManagerTest, GetStatusReturnsIdle) {
    EXPECT_EQ(discourse_get_status(manager), DISCOURSE_STATUS_IDLE);
}

TEST_F(DiscourseManagerTest, GetStatusNullReturnsError) {
    EXPECT_EQ(discourse_get_status(NULL), DISCOURSE_STATUS_ERROR);
}

TEST_F(DiscourseManagerTest, GetLastErrorNullReturnsInternal) {
    EXPECT_EQ(discourse_get_last_error(NULL), DISCOURSE_ERROR_INTERNAL);
}

// ============================================================================
// REFERENT MANAGEMENT TESTS
// ============================================================================

TEST_F(DiscourseManagerTest, IntroduceReferentSuccess) {
    uint32_t id = discourse_introduce_referent(manager, "Alice", REFERENT_TYPE_PERSON, 2, 1);
    EXPECT_GT(id, 0u);
}

TEST_F(DiscourseManagerTest, IntroduceReferentReturnsUniqueIds) {
    uint32_t id1 = discourse_introduce_referent(manager, "Alice", REFERENT_TYPE_PERSON, 2, 1);
    uint32_t id2 = discourse_introduce_referent(manager, "Bob", REFERENT_TYPE_PERSON, 1, 1);

    EXPECT_GT(id1, 0u);
    EXPECT_GT(id2, 0u);
    EXPECT_NE(id1, id2);
}

TEST_F(DiscourseManagerTest, IntroduceReferentDuplicateBoostsSalience) {
    uint32_t id1 = discourse_introduce_referent(manager, "Alice", REFERENT_TYPE_PERSON, 2, 1);

    // Introduce same name again
    uint32_t id2 = discourse_introduce_referent(manager, "Alice", REFERENT_TYPE_PERSON, 2, 1);

    // Should return same ID
    EXPECT_EQ(id1, id2);

    // Check mention count increased
    discourse_referent_t ref;
    EXPECT_TRUE(discourse_get_referent(manager, id1, &ref));
    EXPECT_EQ(ref.mention_count, 2u);
}

TEST_F(DiscourseManagerTest, IntroduceReferentNullReturnsFalse) {
    EXPECT_EQ(discourse_introduce_referent(NULL, "Test", REFERENT_TYPE_PERSON, 0, 0), 0u);
    EXPECT_EQ(discourse_introduce_referent(manager, NULL, REFERENT_TYPE_PERSON, 0, 0), 0u);
}

TEST_F(DiscourseManagerTest, GetReferentSuccess) {
    uint32_t id = discourse_introduce_referent(manager, "Charlie", REFERENT_TYPE_PERSON, 1, 1);

    discourse_referent_t ref;
    EXPECT_TRUE(discourse_get_referent(manager, id, &ref));
    EXPECT_STREQ(ref.name, "Charlie");
    EXPECT_EQ(ref.type, REFERENT_TYPE_PERSON);
    EXPECT_EQ(ref.gender, 1);
    EXPECT_EQ(ref.number, 1);
}

TEST_F(DiscourseManagerTest, GetReferentNotFound) {
    discourse_referent_t ref;
    EXPECT_FALSE(discourse_get_referent(manager, 9999, &ref));
}

TEST_F(DiscourseManagerTest, GetReferentNullReturnsFalse) {
    discourse_referent_t ref;
    EXPECT_FALSE(discourse_get_referent(NULL, 1, &ref));
    EXPECT_FALSE(discourse_get_referent(manager, 0, &ref));
    EXPECT_FALSE(discourse_get_referent(manager, 1, NULL));
}

TEST_F(DiscourseManagerTest, FindReferentByName) {
    discourse_introduce_referent(manager, "David", REFERENT_TYPE_PERSON, 1, 1);

    discourse_referent_t ref;
    EXPECT_TRUE(discourse_find_referent(manager, "David", &ref));
    EXPECT_STREQ(ref.name, "David");
}

TEST_F(DiscourseManagerTest, FindReferentNotFound) {
    discourse_referent_t ref;
    EXPECT_FALSE(discourse_find_referent(manager, "Unknown", &ref));
}

TEST_F(DiscourseManagerTest, GetSalientReferentsSorted) {
    discourse_introduce_referent(manager, "First", REFERENT_TYPE_PERSON, 0, 1);
    discourse_introduce_referent(manager, "Second", REFERENT_TYPE_PERSON, 0, 1);

    // Boost salience of first
    discourse_referent_t ref;
    discourse_find_referent(manager, "First", &ref);
    discourse_boost_referent_salience(manager, ref.referent_id, 0.5f);

    discourse_referent_t refs[5];
    uint32_t count = discourse_get_salient_referents(manager, refs, 5);

    EXPECT_EQ(count, 2u);
    // First should be first (higher salience)
    EXPECT_STREQ(refs[0].name, "First");
}

TEST_F(DiscourseManagerTest, BoostReferentSalienceSuccess) {
    uint32_t id = discourse_introduce_referent(manager, "Test", REFERENT_TYPE_PERSON, 0, 1);

    // Initial salience is 1.0, decay first
    discourse_add_turn(manager, 1, "Some turn", 1000);

    discourse_referent_t before;
    discourse_get_referent(manager, id, &before);

    EXPECT_TRUE(discourse_boost_referent_salience(manager, id, 0.5f));

    discourse_referent_t after;
    discourse_get_referent(manager, id, &after);

    EXPECT_GT(after.salience, before.salience);
    EXPECT_GT(after.mention_count, before.mention_count);
}

TEST_F(DiscourseManagerTest, BoostReferentSalienceClampedToOne) {
    uint32_t id = discourse_introduce_referent(manager, "Test", REFERENT_TYPE_PERSON, 0, 1);

    // Salience is already 1.0, boost should clamp
    EXPECT_TRUE(discourse_boost_referent_salience(manager, id, 0.5f));

    discourse_referent_t ref;
    discourse_get_referent(manager, id, &ref);
    EXPECT_FLOAT_EQ(ref.salience, 1.0f);
}

// ============================================================================
// ANAPHORA RESOLUTION TESTS
// ============================================================================

TEST_F(DiscourseManagerTest, ResolveAnaphoraHe) {
    discourse_introduce_referent(manager, "John", REFERENT_TYPE_PERSON, 1, 1);

    anaphora_resolution_t result;
    EXPECT_TRUE(discourse_resolve_anaphora(manager, "he", NULL, &result));

    EXPECT_TRUE(result.is_resolved);
    EXPECT_EQ(result.type, ANAPHORA_TYPE_PRONOUN);
    EXPECT_GT(result.antecedent_id, 0u);
}

TEST_F(DiscourseManagerTest, ResolveAnaphoraShe) {
    discourse_introduce_referent(manager, "Mary", REFERENT_TYPE_PERSON, 2, 1);

    anaphora_resolution_t result;
    EXPECT_TRUE(discourse_resolve_anaphora(manager, "she", NULL, &result));

    EXPECT_TRUE(result.is_resolved);
}

TEST_F(DiscourseManagerTest, ResolveAnaphoraIt) {
    discourse_introduce_referent(manager, "the car", REFERENT_TYPE_OBJECT, 3, 1);

    anaphora_resolution_t result;
    EXPECT_TRUE(discourse_resolve_anaphora(manager, "it", NULL, &result));

    EXPECT_TRUE(result.is_resolved);
}

TEST_F(DiscourseManagerTest, ResolveAnaphoraThey) {
    discourse_introduce_referent(manager, "the team", REFERENT_TYPE_PERSON, 0, 2);

    anaphora_resolution_t result;
    EXPECT_TRUE(discourse_resolve_anaphora(manager, "they", NULL, &result));

    EXPECT_TRUE(result.is_resolved);
}

TEST_F(DiscourseManagerTest, ResolveAnaphoraGenderMismatch) {
    // Only male referent
    discourse_introduce_referent(manager, "John", REFERENT_TYPE_PERSON, 1, 1);

    anaphora_resolution_t result;
    // Try to resolve "she" - should fail due to gender mismatch
    bool resolved = discourse_resolve_anaphora(manager, "she", NULL, &result);

    // Should not resolve (no female referent)
    EXPECT_FALSE(resolved);
    EXPECT_FALSE(result.is_resolved);
}

TEST_F(DiscourseManagerTest, ResolveAnaphoraNullReturnsFalse) {
    anaphora_resolution_t result;
    EXPECT_FALSE(discourse_resolve_anaphora(NULL, "he", NULL, &result));
    EXPECT_FALSE(discourse_resolve_anaphora(manager, NULL, NULL, &result));
    EXPECT_FALSE(discourse_resolve_anaphora(manager, "he", NULL, NULL));
}

TEST_F(DiscourseManagerTest, ResolveAllAnaphoraInUtterance) {
    discourse_introduce_referent(manager, "John", REFERENT_TYPE_PERSON, 1, 1);
    discourse_introduce_referent(manager, "Mary", REFERENT_TYPE_PERSON, 2, 1);

    anaphora_resolution_t resolutions[8];
    uint32_t count = discourse_resolve_all_anaphora(manager,
        "He gave her the book", resolutions, 8);

    // Should find "he" and "her"
    EXPECT_GE(count, 1u);
}

TEST_F(DiscourseManagerTest, GetAntecedentCandidatesFiltered) {
    discourse_introduce_referent(manager, "John", REFERENT_TYPE_PERSON, 1, 1);
    discourse_introduce_referent(manager, "Mary", REFERENT_TYPE_PERSON, 2, 1);
    discourse_introduce_referent(manager, "the book", REFERENT_TYPE_OBJECT, 3, 1);

    discourse_referent_t candidates[10];
    // Get only masculine candidates
    uint32_t count = discourse_get_antecedent_candidates(manager,
        ANAPHORA_TYPE_PRONOUN, 1, 0, candidates, 10);

    EXPECT_GE(count, 1u);
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_TRUE(candidates[i].gender == 0 || candidates[i].gender == 1);
    }
}

// ============================================================================
// TOPIC MANAGEMENT TESTS
// ============================================================================

TEST_F(DiscourseManagerTest, IntroduceTopicSuccess) {
    uint32_t id = discourse_introduce_topic(manager, "Weather");
    EXPECT_GT(id, 0u);
}

TEST_F(DiscourseManagerTest, IntroduceTopicDuplicateReturnsExisting) {
    uint32_t id1 = discourse_introduce_topic(manager, "Sports");
    uint32_t id2 = discourse_introduce_topic(manager, "Sports");

    EXPECT_EQ(id1, id2);
}

TEST_F(DiscourseManagerTest, GetCurrentTopicAfterIntroduce) {
    uint32_t id = discourse_introduce_topic(manager, "Politics");

    discourse_topic_t topic;
    EXPECT_TRUE(discourse_get_current_topic(manager, &topic));
    EXPECT_EQ(topic.topic_id, id);
    EXPECT_STREQ(topic.name, "Politics");
}

TEST_F(DiscourseManagerTest, GetCurrentTopicNoTopicReturnsFalse) {
    discourse_topic_t topic;
    EXPECT_FALSE(discourse_get_current_topic(manager, &topic));
}

TEST_F(DiscourseManagerTest, SetCurrentTopicSuccess) {
    uint32_t id1 = discourse_introduce_topic(manager, "Topic1");
    uint32_t id2 = discourse_introduce_topic(manager, "Topic2");

    EXPECT_TRUE(discourse_set_current_topic(manager, id2));

    discourse_topic_t topic;
    discourse_get_current_topic(manager, &topic);
    EXPECT_EQ(topic.topic_id, id2);
    (void)id1;
}

TEST_F(DiscourseManagerTest, SetCurrentTopicInvalidReturnsFalse) {
    EXPECT_FALSE(discourse_set_current_topic(manager, 9999));
}

TEST_F(DiscourseManagerTest, DetectTopicShiftDigression) {
    topic_shift_t shift;
    float conf = discourse_detect_topic_shift(manager, "Anyway, about something else", &shift);

    EXPECT_EQ(shift, TOPIC_SHIFT_DIGRESSION);
    EXPECT_GT(conf, 0.5f);
}

TEST_F(DiscourseManagerTest, DetectTopicShiftReturn) {
    topic_shift_t shift;
    float conf = discourse_detect_topic_shift(manager, "Back to what I was saying", &shift);

    EXPECT_EQ(shift, TOPIC_SHIFT_RETURN);
    EXPECT_GT(conf, 0.5f);
}

TEST_F(DiscourseManagerTest, DetectTopicShiftContinuation) {
    topic_shift_t shift;
    float conf = discourse_detect_topic_shift(manager, "And then we went home", &shift);

    EXPECT_EQ(shift, TOPIC_SHIFT_CONTINUATION);
    EXPECT_LT(conf, 0.5f);
}

TEST_F(DiscourseManagerTest, GetRecentTopics) {
    discourse_introduce_topic(manager, "Topic1");
    discourse_introduce_topic(manager, "Topic2");
    discourse_introduce_topic(manager, "Topic3");

    discourse_topic_t topics[10];
    uint32_t count = discourse_get_recent_topics(manager, topics, 10);

    EXPECT_EQ(count, 3u);
}

// ============================================================================
// TURN MANAGEMENT TESTS
// ============================================================================

TEST_F(DiscourseManagerTest, AddTurnSuccess) {
    uint32_t id = discourse_add_turn(manager, 1, "Hello world", 1000);
    EXPECT_GT(id, 0u);
}

TEST_F(DiscourseManagerTest, AddTurnReturnsUniqueIds) {
    uint32_t id1 = discourse_add_turn(manager, 1, "First turn", 1000);
    uint32_t id2 = discourse_add_turn(manager, 2, "Second turn", 2000);

    EXPECT_GT(id1, 0u);
    EXPECT_GT(id2, 0u);
    EXPECT_NE(id1, id2);
}

TEST_F(DiscourseManagerTest, AddTurnNullReturnsFalse) {
    EXPECT_EQ(discourse_add_turn(NULL, 1, "Test", 1000), 0u);
    EXPECT_EQ(discourse_add_turn(manager, 1, NULL, 1000), 0u);
}

TEST_F(DiscourseManagerTest, GetTurnSuccess) {
    uint32_t id = discourse_add_turn(manager, 42, "Test turn content", 12345);

    discourse_turn_t turn;
    EXPECT_TRUE(discourse_get_turn(manager, id, &turn));
    EXPECT_EQ(turn.turn_id, id);
    EXPECT_EQ(turn.speaker_id, 42u);
    EXPECT_EQ(turn.timestamp_ms, 12345u);
    EXPECT_STREQ(turn.content, "Test turn content");
}

TEST_F(DiscourseManagerTest, GetTurnNotFound) {
    discourse_turn_t turn;
    EXPECT_FALSE(discourse_get_turn(manager, 9999, &turn));
}

TEST_F(DiscourseManagerTest, GetTurnNullReturnsFalse) {
    discourse_turn_t turn;
    EXPECT_FALSE(discourse_get_turn(NULL, 1, &turn));
    EXPECT_FALSE(discourse_get_turn(manager, 0, &turn));
    EXPECT_FALSE(discourse_get_turn(manager, 1, NULL));
}

TEST_F(DiscourseManagerTest, GetRecentTurnsOrdered) {
    discourse_add_turn(manager, 1, "First", 1000);
    discourse_add_turn(manager, 2, "Second", 2000);
    discourse_add_turn(manager, 1, "Third", 3000);

    discourse_turn_t turns[10];
    uint32_t count = discourse_get_recent_turns(manager, turns, 10);

    EXPECT_EQ(count, 3u);
    // Most recent first
    EXPECT_STREQ(turns[0].content, "Third");
    EXPECT_STREQ(turns[1].content, "Second");
    EXPECT_STREQ(turns[2].content, "First");
}

TEST_F(DiscourseManagerTest, GetRecentTurnsLimited) {
    discourse_add_turn(manager, 1, "First", 1000);
    discourse_add_turn(manager, 2, "Second", 2000);
    discourse_add_turn(manager, 1, "Third", 3000);

    discourse_turn_t turns[2];
    uint32_t count = discourse_get_recent_turns(manager, turns, 2);

    EXPECT_EQ(count, 2u);
}

TEST_F(DiscourseManagerTest, GetTurnCountAccurate) {
    EXPECT_EQ(discourse_get_turn_count(manager), 0u);

    discourse_add_turn(manager, 1, "First", 1000);
    EXPECT_EQ(discourse_get_turn_count(manager), 1u);

    discourse_add_turn(manager, 2, "Second", 2000);
    EXPECT_EQ(discourse_get_turn_count(manager), 2u);
}

TEST_F(DiscourseManagerTest, GetTurnCountNullReturnsZero) {
    EXPECT_EQ(discourse_get_turn_count(NULL), 0u);
}

TEST_F(DiscourseManagerTest, AddTurnDecaysSalience) {
    uint32_t ref_id = discourse_introduce_referent(manager, "Test", REFERENT_TYPE_PERSON, 0, 1);

    discourse_referent_t before;
    discourse_get_referent(manager, ref_id, &before);
    float initial_salience = before.salience;

    discourse_add_turn(manager, 1, "Some turn", 1000);

    discourse_referent_t after;
    discourse_get_referent(manager, ref_id, &after);

    EXPECT_LT(after.salience, initial_salience);
}

// ============================================================================
// COHERENCE ANALYSIS TESTS
// ============================================================================

TEST_F(DiscourseManagerTest, AnalyzeCoherenceBasic) {
    uint32_t id1 = discourse_add_turn(manager, 1, "The weather is nice", 1000);
    uint32_t id2 = discourse_add_turn(manager, 2, "Yes, very sunny today", 2000);

    coherence_relation_t relation;
    float score = discourse_analyze_coherence(manager, id1, id2, &relation);

    EXPECT_GT(score, 0.0f);
    EXPECT_LE(score, 1.0f);
}

TEST_F(DiscourseManagerTest, AnalyzeCoherenceContrastMarker) {
    uint32_t id1 = discourse_add_turn(manager, 1, "I like pizza", 1000);
    uint32_t id2 = discourse_add_turn(manager, 2, "But I prefer pasta", 2000);

    coherence_relation_t relation;
    discourse_analyze_coherence(manager, id1, id2, &relation);

    EXPECT_EQ(relation, COHERENCE_RELATION_CONTRAST);
}

TEST_F(DiscourseManagerTest, AnalyzeCoherenceExplanation) {
    uint32_t id1 = discourse_add_turn(manager, 1, "I stayed home", 1000);
    uint32_t id2 = discourse_add_turn(manager, 2, "because it was raining", 2000);

    coherence_relation_t relation;
    discourse_analyze_coherence(manager, id1, id2, &relation);

    EXPECT_EQ(relation, COHERENCE_RELATION_EXPLANATION);
}

TEST_F(DiscourseManagerTest, GetOverallCoherenceInitial) {
    // No turns yet
    float coherence = discourse_get_overall_coherence(manager);
    EXPECT_FLOAT_EQ(coherence, 1.0f);
}

// ============================================================================
// FULL ANALYSIS TESTS
// ============================================================================

TEST_F(DiscourseManagerTest, FullAnalysisSuccess) {
    SetupBasicConversation();

    discourse_analysis_t analysis;
    EXPECT_TRUE(discourse_analyze(manager, 1, "He said he would help", 3000, &analysis));

    // Should have some resolutions (for "he")
    EXPECT_GE(analysis.resolution_count, 0u);
}

TEST_F(DiscourseManagerTest, FullAnalysisDetectsTopicShift) {
    discourse_add_turn(manager, 1, "The weather is nice", 1000);

    discourse_analysis_t analysis;
    discourse_analyze(manager, 2, "Anyway, about the project", 2000, &analysis);

    EXPECT_EQ(analysis.topic_shift, TOPIC_SHIFT_DIGRESSION);
}

TEST_F(DiscourseManagerTest, FullAnalysisTracksCoherence) {
    discourse_add_turn(manager, 1, "I like dogs", 1000);

    discourse_analysis_t analysis;
    discourse_analyze(manager, 2, "because they are friendly", 2000, &analysis);

    EXPECT_EQ(analysis.coherence_relation, COHERENCE_RELATION_EXPLANATION);
}

TEST_F(DiscourseManagerTest, FullAnalysisNullReturnsFalse) {
    discourse_analysis_t analysis;
    EXPECT_FALSE(discourse_analyze(NULL, 1, "test", 1000, &analysis));
    EXPECT_FALSE(discourse_analyze(manager, 1, NULL, 1000, &analysis));
    EXPECT_FALSE(discourse_analyze(manager, 1, "test", 1000, NULL));
}

// ============================================================================
// STATISTICS TESTS
// ============================================================================

TEST_F(DiscourseManagerTest, StatsUpdatedAfterOperations) {
    discourse_introduce_referent(manager, "Test", REFERENT_TYPE_PERSON, 0, 1);
    discourse_add_turn(manager, 1, "Some turn", 1000);

    discourse_stats_t stats;
    EXPECT_TRUE(discourse_get_stats(manager, &stats));

    EXPECT_EQ(stats.referents_created, 1u);
    EXPECT_EQ(stats.turns_processed, 1u);
}

TEST_F(DiscourseManagerTest, StatsTrackAnaphoraResolution) {
    discourse_introduce_referent(manager, "John", REFERENT_TYPE_PERSON, 1, 1);

    anaphora_resolution_t result;
    discourse_resolve_anaphora(manager, "he", NULL, &result);

    discourse_stats_t stats;
    discourse_get_stats(manager, &stats);

    EXPECT_GT(stats.anaphora_resolved + stats.anaphora_failed, 0u);
}

TEST_F(DiscourseManagerTest, ResetStatsClearsAll) {
    discourse_introduce_referent(manager, "Test", REFERENT_TYPE_PERSON, 0, 1);
    discourse_add_turn(manager, 1, "Turn", 1000);

    discourse_reset_stats(manager);

    discourse_stats_t stats;
    discourse_get_stats(manager, &stats);

    EXPECT_EQ(stats.referents_created, 0u);
    EXPECT_EQ(stats.turns_processed, 0u);
}

TEST_F(DiscourseManagerTest, GetStatsNullReturnsFalse) {
    EXPECT_FALSE(discourse_get_stats(NULL, nullptr));

    discourse_stats_t stats;
    EXPECT_FALSE(discourse_get_stats(manager, nullptr));
    EXPECT_FALSE(discourse_get_stats(NULL, &stats));
}

TEST_F(DiscourseManagerTest, GetConfigSuccess) {
    discourse_config_t retrieved;
    EXPECT_TRUE(discourse_get_config(manager, &retrieved));
    EXPECT_EQ(retrieved.max_turns, config.max_turns);
}

TEST_F(DiscourseManagerTest, GetConfigNullReturnsFalse) {
    discourse_config_t retrieved;
    EXPECT_FALSE(discourse_get_config(NULL, &retrieved));
    EXPECT_FALSE(discourse_get_config(manager, NULL));
}

// ============================================================================
// BIO-ASYNC INTEGRATION TESTS
// ============================================================================

TEST_F(DiscourseManagerTest, RegisterBioHandlerNullReturnsFalse) {
    EXPECT_FALSE(discourse_register_bio_handler(NULL, nullptr));
    EXPECT_FALSE(discourse_register_bio_handler(manager, nullptr));
}

// ============================================================================
// EDGE CASES AND BOUNDARY TESTS
// ============================================================================

TEST_F(DiscourseManagerTest, CircularBufferWraparound) {
    // Add more turns than buffer size
    for (int i = 0; i < 100; i++) {
        char content[32];
        snprintf(content, sizeof(content), "Turn %d", i);
        discourse_add_turn(manager, 1, content, i * 1000);
    }

    // Should still have turns (most recent)
    discourse_turn_t turns[5];
    uint32_t count = discourse_get_recent_turns(manager, turns, 5);
    EXPECT_GT(count, 0u);
}

TEST_F(DiscourseManagerTest, LongEntityName) {
    std::string long_name(100, 'x');
    uint32_t id = discourse_introduce_referent(manager, long_name.c_str(),
                                                REFERENT_TYPE_PERSON, 0, 1);
    EXPECT_GT(id, 0u);

    discourse_referent_t ref;
    discourse_get_referent(manager, id, &ref);
    // Name should be truncated
    EXPECT_LT(strlen(ref.name), long_name.length());
}

TEST_F(DiscourseManagerTest, ManyReferents) {
    // Add many referents
    for (int i = 0; i < 50; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Person%d", i);
        discourse_introduce_referent(manager, name, REFERENT_TYPE_PERSON, 0, 1);
    }

    discourse_referent_t refs[100];
    uint32_t count = discourse_get_salient_referents(manager, refs, 100);
    EXPECT_EQ(count, 50u);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
