/**
 * @file test_knowledge.cpp
 * @brief Comprehensive unit tests for NIMCP Knowledge System
 */

#include "test_helpers.h"

#include "cognitive/knowledge/nimcp_knowledge.h"

#include <cstring>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class KnowledgeTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        system = knowledge_system_create("test_learner");
        ASSERT_NE(system, nullptr);
    }

    void TearDown() override
    {
        if (system) {
            knowledge_system_destroy(system);
            system = nullptr;
        }
    }

    // Helper to create test narrative
    narrative_knowledge_t create_test_narrative()
    {
        narrative_knowledge_t story = {0};
        strncpy(story.title, "The Tortoise and the Hare", sizeof(story.title) - 1);
        strncpy(story.author, "Aesop", sizeof(story.author) - 1);
        strncpy(story.summary, "A slow tortoise wins a race against a fast hare",
                sizeof(story.summary) - 1);

        // Allocate characters
        story.num_characters = 2;
        story.characters = (char**) calloc(2, sizeof(char*));
        story.characters[0] = strdup("Tortoise");
        story.characters[1] = strdup("Hare");

        // Allocate themes
        story.num_themes = 2;
        story.themes = (char**) calloc(2, sizeof(char*));
        story.themes[0] = strdup("perseverance");
        story.themes[1] = strdup("overconfidence");

        // Allocate moral lessons
        story.num_lessons = 1;
        story.moral_lessons = (char**) calloc(1, sizeof(char*));
        story.moral_lessons[0] = strdup("Slow and steady wins the race");

        story.primary_domain = KNOWLEDGE_DOMAIN_LITERATURE;
        strncpy(story.cultural_context, "Ancient Greece", sizeof(story.cultural_context) - 1);

        return story;
    }

    // Helper to create test art
    aesthetic_knowledge_t create_test_art()
    {
        aesthetic_knowledge_t art = {0};
        strncpy(art.work_title, "Starry Night", sizeof(art.work_title) - 1);
        strncpy(art.creator, "Vincent van Gogh", sizeof(art.creator) - 1);
        strncpy(art.medium, "oil painting", sizeof(art.medium) - 1);
        strncpy(art.description, "Swirling night sky over a village", sizeof(art.description) - 1);

        // Allocate aesthetic qualities
        art.num_qualities = 3;
        art.aesthetic_qualities = (char**) calloc(3, sizeof(char*));
        art.aesthetic_qualities[0] = strdup("dreamlike");
        art.aesthetic_qualities[1] = strdup("expressive");
        art.aesthetic_qualities[2] = strdup("turbulent");

        strncpy(art.emotional_impact, "Evokes wonder and turbulence",
                sizeof(art.emotional_impact) - 1);
        strncpy(art.historical_significance, "Post-impressionist masterpiece",
                sizeof(art.historical_significance) - 1);

        return art;
    }

    // Helper to create test history
    historical_knowledge_t create_test_history()
    {
        historical_knowledge_t event = {0};
        strncpy(event.event_name, "Moon Landing", sizeof(event.event_name) - 1);
        event.timestamp_year = 1969;

        // Allocate people
        event.num_people = 2;
        event.key_people = (char**) calloc(2, sizeof(char*));
        event.key_people[0] = strdup("Neil Armstrong");
        event.key_people[1] = strdup("Buzz Aldrin");

        strncpy(event.causes, "Space race during Cold War", sizeof(event.causes) - 1);
        strncpy(event.effects, "Advancement in space technology", sizeof(event.effects) - 1);
        strncpy(event.significance, "First humans on another celestial body",
                sizeof(event.significance) - 1);

        // Allocate related events
        event.num_related_events = 1;
        event.related_events = (char**) calloc(1, sizeof(char*));
        event.related_events[0] = strdup("Apollo 11 mission");

        return event;
    }

    // Helper to free narrative
    void free_narrative(narrative_knowledge_t* story)
    {
        if (!story)
            return;
        if (story->characters) {
            for (uint32_t i = 0; i < story->num_characters; i++) {
                free(story->characters[i]);
            }
            free(story->characters);
        }
        if (story->themes) {
            for (uint32_t i = 0; i < story->num_themes; i++) {
                free(story->themes[i]);
            }
            free(story->themes);
        }
        if (story->moral_lessons) {
            for (uint32_t i = 0; i < story->num_lessons; i++) {
                free(story->moral_lessons[i]);
            }
            free(story->moral_lessons);
        }
    }

    // Helper to free art
    void free_art(aesthetic_knowledge_t* art)
    {
        if (!art)
            return;
        if (art->aesthetic_qualities) {
            for (uint32_t i = 0; i < art->num_qualities; i++) {
                free(art->aesthetic_qualities[i]);
            }
            free(art->aesthetic_qualities);
        }
    }

    // Helper to free history
    void free_history(historical_knowledge_t* event)
    {
        if (!event)
            return;
        if (event->key_people) {
            for (uint32_t i = 0; i < event->num_people; i++) {
                free(event->key_people[i]);
            }
            free(event->key_people);
        }
        if (event->related_events) {
            for (uint32_t i = 0; i < event->num_related_events; i++) {
                free(event->related_events[i]);
            }
            free(event->related_events);
        }
    }

    knowledge_system_t system;
};

//=============================================================================
// Creation/Destruction Tests
//=============================================================================

TEST_F(KnowledgeTest, SystemCreation)
{
    EXPECT_NE(system, nullptr);
}

TEST_F(KnowledgeTest, SystemCreationNullName)
{
    knowledge_system_t null_system = knowledge_system_create(nullptr);
    EXPECT_EQ(null_system, nullptr);
}

TEST_F(KnowledgeTest, SystemDestructionNullSafe)
{
    knowledge_system_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// Text Learning Tests
//=============================================================================

TEST_F(KnowledgeTest, LearnFromTextBasic)
{
    const char* text = "Democracy is a system of government by the people.";
    uint32_t learned = knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_GT(learned, 0);
}

TEST_F(KnowledgeTest, LearnFromTextNull)
{
    uint32_t learned = knowledge_learn_from_text(nullptr, "test", KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_EQ(learned, 0);

    learned = knowledge_learn_from_text(system, nullptr, KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_EQ(learned, 0);
}

TEST_F(KnowledgeTest, LearnFromTextMultipleConcepts)
{
    const char* text = "Science explores nature through observation and experiment. "
                       "Mathematics provides tools for logical reasoning.";
    uint32_t learned = knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_SCIENCE);
    EXPECT_GT(learned, 0);
}

TEST_F(KnowledgeTest, LearnFromTextReinforcementIncreasesConfidence)
{
    const char* text = "Democracy is a form of government.";

    // Learn once
    knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);

    knowledge_item_t item1;
    ASSERT_TRUE(knowledge_retrieve(system, "democracy", &item1));
    float confidence1 = item1.confidence;

    // Learn again (reinforcement)
    knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);

    knowledge_item_t item2;
    ASSERT_TRUE(knowledge_retrieve(system, "democracy", &item2));
    float confidence2 = item2.confidence;

    EXPECT_GT(confidence2, confidence1);
}

//=============================================================================
// Narrative Learning Tests
//=============================================================================

TEST_F(KnowledgeTest, LearnFromStory)
{
    narrative_knowledge_t story = create_test_narrative();

    bool success = knowledge_learn_from_story(system, &story);
    EXPECT_TRUE(success);

    free_narrative(&story);
}

TEST_F(KnowledgeTest, LearnFromStoryNull)
{
    EXPECT_FALSE(knowledge_learn_from_story(nullptr, nullptr));

    narrative_knowledge_t story = create_test_narrative();
    EXPECT_FALSE(knowledge_learn_from_story(nullptr, &story));
    EXPECT_FALSE(knowledge_learn_from_story(system, nullptr));

    free_narrative(&story);
}

TEST_F(KnowledgeTest, LearnFromStoryExtractsLessons)
{
    narrative_knowledge_t story = create_test_narrative();

    knowledge_learn_from_story(system, &story);

    // Verify lesson was learned (stored with pattern "lesson_from_<title>_<index>")
    domain_knowledge_t assessment;
    ASSERT_TRUE(knowledge_assess_domain(system, KNOWLEDGE_DOMAIN_LITERATURE, &assessment));
    EXPECT_GT(assessment.concepts_known, 0);

    free_narrative(&story);
}

//=============================================================================
// Art Learning Tests
//=============================================================================

TEST_F(KnowledgeTest, LearnFromArt)
{
    aesthetic_knowledge_t art = create_test_art();

    bool success = knowledge_learn_from_art(system, &art);
    EXPECT_TRUE(success);

    free_art(&art);
}

TEST_F(KnowledgeTest, LearnFromArtNull)
{
    EXPECT_FALSE(knowledge_learn_from_art(nullptr, nullptr));

    aesthetic_knowledge_t art = create_test_art();
    EXPECT_FALSE(knowledge_learn_from_art(nullptr, &art));
    EXPECT_FALSE(knowledge_learn_from_art(system, nullptr));

    free_art(&art);
}

//=============================================================================
// History Learning Tests
//=============================================================================

TEST_F(KnowledgeTest, LearnFromHistory)
{
    historical_knowledge_t event = create_test_history();

    bool success = knowledge_learn_from_history(system, &event);
    EXPECT_TRUE(success);

    free_history(&event);
}

TEST_F(KnowledgeTest, LearnFromHistoryNull)
{
    EXPECT_FALSE(knowledge_learn_from_history(nullptr, nullptr));

    historical_knowledge_t event = create_test_history();
    EXPECT_FALSE(knowledge_learn_from_history(nullptr, &event));
    EXPECT_FALSE(knowledge_learn_from_history(system, nullptr));

    free_history(&event);
}

//=============================================================================
// Conversation Learning Tests
//=============================================================================

TEST_F(KnowledgeTest, LearnFromConversation)
{
    const char* dialogue = "Hello! How are you? I'm learning about social interaction.";
    const char* participants[] = {"Alice", "Bob"};

    uint32_t learned = knowledge_learn_from_conversation(system, dialogue, participants, 2);
    EXPECT_GT(learned, 0);
}

TEST_F(KnowledgeTest, LearnFromConversationNull)
{
    const char* participants[] = {"Alice", "Bob"};

    EXPECT_EQ(knowledge_learn_from_conversation(nullptr, "test", participants, 2), 0);
    EXPECT_EQ(knowledge_learn_from_conversation(system, nullptr, participants, 2), 0);
}

//=============================================================================
// Demonstration Learning Tests
//=============================================================================

TEST_F(KnowledgeTest, LearnFromDemonstration)
{
    const char* steps[] = {"Turn on the computer", "Open the application",
                           "Click the start button"};

    bool success = knowledge_learn_from_demonstration(system, "Starting software", steps, 3);
    EXPECT_TRUE(success);
}

TEST_F(KnowledgeTest, LearnFromDemonstrationNull)
{
    const char* steps[] = {"Step 1", "Step 2"};

    EXPECT_FALSE(knowledge_learn_from_demonstration(nullptr, "test", steps, 2));
    EXPECT_FALSE(knowledge_learn_from_demonstration(system, nullptr, steps, 2));
    EXPECT_FALSE(knowledge_learn_from_demonstration(system, "test", nullptr, 2));
}

//=============================================================================
// Retrieval Tests
//=============================================================================

TEST_F(KnowledgeTest, RetrieveKnowledge)
{
    const char* text = "Democracy is government by the people.";
    knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);

    knowledge_item_t item;
    bool found = knowledge_retrieve(system, "democracy", &item);
    EXPECT_TRUE(found);

    if (found) {
        EXPECT_STREQ(item.concept_name, "democracy");
        EXPECT_GT(item.confidence, 0.0f);
        EXPECT_LE(item.confidence, 1.0f);
    }
}

TEST_F(KnowledgeTest, RetrieveUnknownConcept)
{
    knowledge_item_t item;
    bool found = knowledge_retrieve(system, "nonexistent_concept_xyz", &item);
    EXPECT_FALSE(found);
}

TEST_F(KnowledgeTest, RetrieveNull)
{
    knowledge_item_t item;

    EXPECT_FALSE(knowledge_retrieve(nullptr, "test", &item));
    EXPECT_FALSE(knowledge_retrieve(system, nullptr, &item));
    EXPECT_FALSE(knowledge_retrieve(system, "test", nullptr));
}

//=============================================================================
// Understanding Tests
//=============================================================================

TEST_F(KnowledgeTest, UnderstandConcept)
{
    const char* text = "Democracy is government by the people.";
    knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);

    char explanation[512];
    uint32_t len = knowledge_understand(system, "democracy", "", explanation, sizeof(explanation));

    EXPECT_GT(len, 0);
    EXPECT_LT(len, sizeof(explanation));
}

TEST_F(KnowledgeTest, UnderstandUnknownConcept)
{
    char explanation[512];
    uint32_t len =
        knowledge_understand(system, "unknown_concept", "", explanation, sizeof(explanation));

    EXPECT_GT(len, 0);
    // Should contain message about not knowing
    EXPECT_NE(strstr(explanation, "don't know"), nullptr);
}

TEST_F(KnowledgeTest, UnderstandNull)
{
    char explanation[512];

    EXPECT_EQ(knowledge_understand(nullptr, "test", "", explanation, sizeof(explanation)), 0);
    EXPECT_EQ(knowledge_understand(system, nullptr, "", explanation, sizeof(explanation)), 0);
    EXPECT_EQ(knowledge_understand(system, "test", "", nullptr, sizeof(explanation)), 0);
}

//=============================================================================
// Simple Explanation Tests
//=============================================================================

TEST_F(KnowledgeTest, ExplainSimplyYoungAge)
{
    const char* text = "Democracy is government by the people.";
    knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);

    char explanation[512];
    uint32_t len =
        knowledge_explain_simply(system, "democracy", 5, explanation, sizeof(explanation));

    EXPECT_GT(len, 0);
    // For age < 5, should be very simple
}

TEST_F(KnowledgeTest, ExplainSimplyOlderAge)
{
    const char* text = "Democracy is government by the people.";
    knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);

    char explanation[512];
    uint32_t len =
        knowledge_explain_simply(system, "democracy", 15, explanation, sizeof(explanation));

    EXPECT_GT(len, 0);
}

TEST_F(KnowledgeTest, ExplainSimplyNull)
{
    char explanation[512];

    EXPECT_EQ(knowledge_explain_simply(nullptr, "test", 10, explanation, sizeof(explanation)), 0);
    EXPECT_EQ(knowledge_explain_simply(system, nullptr, 10, explanation, sizeof(explanation)), 0);
    EXPECT_EQ(knowledge_explain_simply(system, "test", 10, nullptr, sizeof(explanation)), 0);
}

//=============================================================================
// Connection Finding Tests
//=============================================================================

TEST_F(KnowledgeTest, FindConnections)
{
    const char* text = "Democracy requires voting and representation.";
    knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);

    knowledge_item_t connections[10];
    uint32_t num_found = knowledge_find_connections(system, "democracy", connections, 10);

    // May or may not find connections depending on implementation
    EXPECT_GE(num_found, 0);
    EXPECT_LE(num_found, 10);
}

TEST_F(KnowledgeTest, FindConnectionsNull)
{
    knowledge_item_t connections[10];

    EXPECT_EQ(knowledge_find_connections(nullptr, "test", connections, 10), 0);
    EXPECT_EQ(knowledge_find_connections(system, nullptr, connections, 10), 0);
    EXPECT_EQ(knowledge_find_connections(system, "test", nullptr, 10), 0);
}

//=============================================================================
// Transfer Learning Tests
//=============================================================================

TEST_F(KnowledgeTest, TransferLearning)
{
    char application[512];
    bool success =
        knowledge_transfer_learning(system, KNOWLEDGE_DOMAIN_LITERATURE, KNOWLEDGE_DOMAIN_SOCIAL,
                                    "Making new friends", application, sizeof(application));

    EXPECT_TRUE(success);
    EXPECT_GT(strlen(application), 0);
}

TEST_F(KnowledgeTest, TransferLearningNull)
{
    char application[512];

    EXPECT_FALSE(knowledge_transfer_learning(nullptr, KNOWLEDGE_DOMAIN_LITERATURE,
                                             KNOWLEDGE_DOMAIN_SOCIAL, "test", application,
                                             sizeof(application)));

    EXPECT_FALSE(knowledge_transfer_learning(system, KNOWLEDGE_DOMAIN_LITERATURE,
                                             KNOWLEDGE_DOMAIN_SOCIAL, nullptr, application,
                                             sizeof(application)));

    EXPECT_FALSE(knowledge_transfer_learning(system, KNOWLEDGE_DOMAIN_LITERATURE,
                                             KNOWLEDGE_DOMAIN_SOCIAL, "test", nullptr,
                                             sizeof(application)));
}

//=============================================================================
// Building On Knowledge Tests
//=============================================================================

TEST_F(KnowledgeTest, BuildOnExistingKnowledge)
{
    const char* text = "A dog is a domesticated animal.";
    knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);

    bool success = knowledge_build_on(system, "wolf", "dog", "wild and not domesticated");
    EXPECT_TRUE(success);
}

TEST_F(KnowledgeTest, BuildOnNonexistentBase)
{
    bool success = knowledge_build_on(system, "new", "nonexistent_base", "different");
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeTest, BuildOnNull)
{
    EXPECT_FALSE(knowledge_build_on(nullptr, "new", "base", "diff"));
    EXPECT_FALSE(knowledge_build_on(system, nullptr, "base", "diff"));
    EXPECT_FALSE(knowledge_build_on(system, "new", nullptr, "diff"));
}

//=============================================================================
// Reinforcement Tests
//=============================================================================

TEST_F(KnowledgeTest, ReinforceKnowledge)
{
    const char* text = "Democracy is government by the people.";
    knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);

    bool success = knowledge_reinforce(system, "democracy", "Ancient Greek democracy");
    EXPECT_TRUE(success);
}

TEST_F(KnowledgeTest, ReinforceUnknownConcept)
{
    bool success = knowledge_reinforce(system, "nonexistent", "example");
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeTest, ReinforceNull)
{
    EXPECT_FALSE(knowledge_reinforce(nullptr, "test", "example"));
    EXPECT_FALSE(knowledge_reinforce(system, nullptr, "example"));
}

//=============================================================================
// Domain Organization Tests
//=============================================================================

TEST_F(KnowledgeTest, OrganizeDomain)
{
    bool success = knowledge_organize_domain(system, KNOWLEDGE_DOMAIN_SCIENCE);
    EXPECT_TRUE(success);
}

TEST_F(KnowledgeTest, OrganizeDomainNull)
{
    EXPECT_FALSE(knowledge_organize_domain(nullptr, KNOWLEDGE_DOMAIN_SCIENCE));
}

//=============================================================================
// Knowledge Map Tests
//=============================================================================

TEST_F(KnowledgeTest, GetKnowledgeMap)
{
    const char* text = "Science is the study of nature.";
    knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_SCIENCE);

    uint32_t num_nodes = knowledge_get_map(system, KNOWLEDGE_DOMAIN_SCIENCE, nullptr, 100);
    EXPECT_GT(num_nodes, 0);
}

TEST_F(KnowledgeTest, GetKnowledgeMapNull)
{
    EXPECT_EQ(knowledge_get_map(nullptr, KNOWLEDGE_DOMAIN_SCIENCE, nullptr, 100), 0);
}

//=============================================================================
// Reading Tests
//=============================================================================

TEST_F(KnowledgeTest, ReadBook)
{
    const char* book_text = "Once upon a time, in a land far away, there lived a wise king.";
    uint32_t learned = knowledge_read_book(system, "The Wise King", book_text, 10);

    EXPECT_GT(learned, 0);
}

TEST_F(KnowledgeTest, ReadBookNull)
{
    EXPECT_EQ(knowledge_read_book(nullptr, "Title", "text", 10), 0);
    EXPECT_EQ(knowledge_read_book(system, nullptr, "text", 10), 0);
    EXPECT_EQ(knowledge_read_book(system, "Title", nullptr, 10), 0);
}

TEST_F(KnowledgeTest, ContinueReading)
{
    const char* book_text = "A long story with many pages.";
    knowledge_read_book(system, "Long Story", book_text, 5);

    uint32_t progress = knowledge_continue_reading(system, "Long Story", true);
    EXPECT_GE(progress, 0);
    EXPECT_LE(progress, 100);
}

TEST_F(KnowledgeTest, ContinueReadingNull)
{
    EXPECT_EQ(knowledge_continue_reading(nullptr, "Title", true), 0);
    EXPECT_EQ(knowledge_continue_reading(system, nullptr, true), 0);
}

//=============================================================================
// Reading Recommendations Tests
//=============================================================================

TEST_F(KnowledgeTest, GetReadingList)
{
    char* recommendations[5];
    uint32_t num_recs =
        knowledge_get_reading_list(system, KNOWLEDGE_DOMAIN_LITERATURE, recommendations, 5);

    EXPECT_GT(num_recs, 0);
    EXPECT_LE(num_recs, 5);
}

TEST_F(KnowledgeTest, GetReadingListNull)
{
    char* recommendations[5];

    EXPECT_EQ(knowledge_get_reading_list(nullptr, KNOWLEDGE_DOMAIN_LITERATURE, recommendations, 5),
              0);
    EXPECT_EQ(knowledge_get_reading_list(system, KNOWLEDGE_DOMAIN_LITERATURE, nullptr, 5), 0);
}

//=============================================================================
// Domain Assessment Tests
//=============================================================================

TEST_F(KnowledgeTest, AssessDomain)
{
    const char* text = "Science is the study of nature through observation.";
    knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_SCIENCE);

    domain_knowledge_t assessment;
    bool success = knowledge_assess_domain(system, KNOWLEDGE_DOMAIN_SCIENCE, &assessment);

    EXPECT_TRUE(success);
    EXPECT_EQ(assessment.domain, KNOWLEDGE_DOMAIN_SCIENCE);
    EXPECT_GT(assessment.concepts_known, 0);
}

TEST_F(KnowledgeTest, AssessDomainNull)
{
    domain_knowledge_t assessment;

    EXPECT_FALSE(knowledge_assess_domain(nullptr, KNOWLEDGE_DOMAIN_SCIENCE, &assessment));
    EXPECT_FALSE(knowledge_assess_domain(system, KNOWLEDGE_DOMAIN_SCIENCE, nullptr));
}

//=============================================================================
// Summary Tests
//=============================================================================

TEST_F(KnowledgeTest, GetSummary)
{
    domain_knowledge_t all_domains[11];
    uint32_t num_domains = knowledge_get_summary(system, all_domains, 11);

    EXPECT_EQ(num_domains, 11);

    for (uint32_t i = 0; i < num_domains; i++) {
        EXPECT_EQ(all_domains[i].domain, (knowledge_domain_t) i);
    }
}

TEST_F(KnowledgeTest, GetSummaryNull)
{
    domain_knowledge_t all_domains[11];

    EXPECT_EQ(knowledge_get_summary(nullptr, all_domains, 11), 0);
    EXPECT_EQ(knowledge_get_summary(system, nullptr, 11), 0);
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(KnowledgeTest, DomainNames)
{
    EXPECT_STREQ(knowledge_domain_name(KNOWLEDGE_DOMAIN_LANGUAGE), "Language");
    EXPECT_STREQ(knowledge_domain_name(KNOWLEDGE_DOMAIN_LITERATURE), "Literature");
    EXPECT_STREQ(knowledge_domain_name(KNOWLEDGE_DOMAIN_ART), "Art");
    EXPECT_STREQ(knowledge_domain_name(KNOWLEDGE_DOMAIN_ETHICS), "Ethics");
    EXPECT_STREQ(knowledge_domain_name(KNOWLEDGE_DOMAIN_HISTORY), "History");
    EXPECT_STREQ(knowledge_domain_name(KNOWLEDGE_DOMAIN_SCIENCE), "Science");
    EXPECT_STREQ(knowledge_domain_name(KNOWLEDGE_DOMAIN_MATHEMATICS), "Mathematics");
    EXPECT_STREQ(knowledge_domain_name(KNOWLEDGE_DOMAIN_SOCIAL), "Social");
    EXPECT_STREQ(knowledge_domain_name(KNOWLEDGE_DOMAIN_TECHNICAL), "Technical");
    EXPECT_STREQ(knowledge_domain_name(KNOWLEDGE_DOMAIN_PHILOSOPHY), "Philosophy");
    EXPECT_STREQ(knowledge_domain_name(KNOWLEDGE_DOMAIN_GENERAL), "General");
}

TEST_F(KnowledgeTest, DomainNameInvalid)
{
    const char* name = knowledge_domain_name((knowledge_domain_t) 999);
    EXPECT_STREQ(name, "Unknown");
}

TEST_F(KnowledgeTest, PrintItemNull)
{
    knowledge_print_item(nullptr);
    // Should not crash
}

TEST_F(KnowledgeTest, PrintAssessmentNull)
{
    knowledge_print_assessment(nullptr);
    // Should not crash
}

//=============================================================================
// Save/Load Tests
//=============================================================================

TEST_F(KnowledgeTest, SaveKnowledge)
{
    const char* text = "Test knowledge for saving.";
    knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);

    const char* filepath = "/tmp/test_knowledge.dat";
    bool success = knowledge_save(system, filepath);
    EXPECT_TRUE(success);

    // Clean up
    unlink(filepath);
}

TEST_F(KnowledgeTest, SaveNull)
{
    EXPECT_FALSE(knowledge_save(nullptr, "/tmp/test.dat"));
    EXPECT_FALSE(knowledge_save(system, nullptr));
}

TEST_F(KnowledgeTest, LoadKnowledge)
{
    // First save some knowledge
    const char* text = "Test knowledge for loading.";
    knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);

    const char* filepath = "/tmp/test_knowledge_load.dat";
    ASSERT_TRUE(knowledge_save(system, filepath));

    // Load it back
    knowledge_system_t loaded = knowledge_load(filepath);
    EXPECT_NE(loaded, nullptr);

    if (loaded) {
        knowledge_system_destroy(loaded);
    }

    // Clean up
    unlink(filepath);
}

TEST_F(KnowledgeTest, LoadNull)
{
    knowledge_system_t loaded = knowledge_load(nullptr);
    EXPECT_EQ(loaded, nullptr);
}

TEST_F(KnowledgeTest, LoadNonexistentFile)
{
    knowledge_system_t loaded = knowledge_load("/nonexistent/path/file.dat");
    EXPECT_EQ(loaded, nullptr);
}

//=============================================================================
// TDD TESTS - B-TREE CONFIDENCE INDEXING (New Functionality)
//=============================================================================

/**
 * WHAT: Test confidence-based range queries for knowledge
 * WHY: Need efficient queries like "get all well-understood concepts" (confidence >= 0.8)
 * HOW: Add B-tree indexed by confidence alongside hash table
 *
 * TDD: This test WILL FAIL until B-tree implementation is added
 */
TEST_F(KnowledgeTest, GetKnowledgeByConfidenceRange_ReturnsFiltered)
{
    // Add knowledge items with varying confidence levels
    knowledge_item_t item1 = {0};
    strncpy(item1.concept_name, "well_understood", sizeof(item1.concept_name) - 1);
    item1.confidence = 0.95f;
    item1.domain = KNOWLEDGE_DOMAIN_GENERAL;
    knowledge_add_item(system, &item1);

    knowledge_item_t item2 = {0};
    strncpy(item2.concept_name, "somewhat_known", sizeof(item2.concept_name) - 1);
    item2.confidence = 0.5f;
    item2.domain = KNOWLEDGE_DOMAIN_GENERAL;
    knowledge_add_item(system, &item2);

    knowledge_item_t item3 = {0};
    strncpy(item3.concept_name, "very_confident", sizeof(item3.concept_name) - 1);
    item3.confidence = 0.85f;
    item3.domain = KNOWLEDGE_DOMAIN_GENERAL;
    knowledge_add_item(system, &item3);

    knowledge_item_t item4 = {0};
    strncpy(item4.concept_name, "uncertain", sizeof(item4.concept_name) - 1);
    item4.confidence = 0.2f;
    item4.domain = KNOWLEDGE_DOMAIN_GENERAL;
    knowledge_add_item(system, &item4);

    // Query for well-understood concepts (confidence >= 0.8)
    knowledge_item_t* results = nullptr;
    uint32_t count = knowledge_get_by_confidence_range(system, 0.8f, 1.0f, &results);

    ASSERT_EQ(count, 2u) << "Should find 2 items with confidence >= 0.8";
    ASSERT_NE(results, nullptr);

    // Verify both high-confidence items are returned
    bool found_well_understood = false;
    bool found_very_confident = false;
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(results[i].concept_name, "well_understood") == 0) {
            found_well_understood = true;
            EXPECT_FLOAT_EQ(results[i].confidence, 0.95f);
        }
        if (strcmp(results[i].concept_name, "very_confident") == 0) {
            found_very_confident = true;
            EXPECT_FLOAT_EQ(results[i].confidence, 0.85f);
        }
    }

    EXPECT_TRUE(found_well_understood);
    EXPECT_TRUE(found_very_confident);

    nimcp_free(results);
}

/**
 * WHAT: Test getting weakly-understood knowledge
 * WHY: Identify areas needing more learning/reinforcement
 * HOW: Query by confidence threshold
 */
TEST_F(KnowledgeTest, GetWeakKnowledge_ReturnsLowConfidence)
{
    // Add items with different confidence
    knowledge_item_t strong = {0};
    strncpy(strong.concept_name, "strong_concept", sizeof(strong.concept_name) - 1);
    strong.confidence = 0.9f;
    strong.domain = KNOWLEDGE_DOMAIN_SCIENCE;
    knowledge_add_item(system, &strong);

    knowledge_item_t weak1 = {0};
    strncpy(weak1.concept_name, "weak_concept_1", sizeof(weak1.concept_name) - 1);
    weak1.confidence = 0.3f;
    weak1.domain = KNOWLEDGE_DOMAIN_SCIENCE;
    knowledge_add_item(system, &weak1);

    knowledge_item_t weak2 = {0};
    strncpy(weak2.concept_name, "weak_concept_2", sizeof(weak2.concept_name) - 1);
    weak2.confidence = 0.15f;
    weak2.domain = KNOWLEDGE_DOMAIN_SCIENCE;
    knowledge_add_item(system, &weak2);

    // Get weak knowledge (confidence < 0.5)
    knowledge_item_t* results = nullptr;
    uint32_t count = knowledge_get_by_confidence_range(system, 0.0f, 0.5f, &results);

    ASSERT_GE(count, 2u) << "Should find at least 2 weak items";
    ASSERT_NE(results, nullptr);

    // Verify all returned items have low confidence
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_LE(results[i].confidence, 0.5f) << "Item " << results[i].concept_name << " should have low confidence";
    }

    nimcp_free(results);
}

/**
 * WHAT: Test ordered iteration by confidence
 * WHY: List knowledge from most to least understood
 * HOW: B-tree provides ordered traversal
 */
TEST_F(KnowledgeTest, GetAllKnowledgeOrderedByConfidence_ReturnsSorted)
{
    // Add items OUT OF ORDER by confidence
    knowledge_item_t mid = {0};
    strncpy(mid.concept_name, "mid_confidence", sizeof(mid.concept_name) - 1);
    mid.confidence = 0.5f;
    mid.domain = KNOWLEDGE_DOMAIN_GENERAL;
    knowledge_add_item(system, &mid);

    knowledge_item_t high = {0};
    strncpy(high.concept_name, "high_confidence", sizeof(high.concept_name) - 1);
    high.confidence = 0.95f;
    high.domain = KNOWLEDGE_DOMAIN_GENERAL;
    knowledge_add_item(system, &high);

    knowledge_item_t low = {0};
    strncpy(low.concept_name, "low_confidence", sizeof(low.concept_name) - 1);
    low.confidence = 0.1f;
    low.domain = KNOWLEDGE_DOMAIN_GENERAL;
    knowledge_add_item(system, &low);

    // Get all knowledge ordered by confidence
    knowledge_item_t* results = nullptr;
    uint32_t count = knowledge_get_all_ordered_by_confidence(system, &results);

    ASSERT_GE(count, 3u);
    ASSERT_NE(results, nullptr);

    // Find our test items and verify order
    int low_idx = -1, mid_idx = -1, high_idx = -1;
    for (uint32_t i = 0; i < count; i++) {
        if (strcmp(results[i].concept_name, "low_confidence") == 0) low_idx = i;
        if (strcmp(results[i].concept_name, "mid_confidence") == 0) mid_idx = i;
        if (strcmp(results[i].concept_name, "high_confidence") == 0) high_idx = i;
    }

    ASSERT_NE(low_idx, -1);
    ASSERT_NE(mid_idx, -1);
    ASSERT_NE(high_idx, -1);

    // Should be in ascending order: low < mid < high
    EXPECT_LT(low_idx, mid_idx) << "Low confidence should come before mid";
    EXPECT_LT(mid_idx, high_idx) << "Mid confidence should come before high";

    nimcp_free(results);
}

}  // anonymous namespace
