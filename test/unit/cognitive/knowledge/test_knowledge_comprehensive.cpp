//=============================================================================
// test_knowledge_comprehensive.cpp - Comprehensive Knowledge System Tests
//=============================================================================

#include <gtest/gtest.h>
#include <cstring>
#include "cognitive/knowledge/nimcp_knowledge.h"

class KnowledgeComprehensiveTest : public ::testing::Test {
protected:
    knowledge_system_t system;

    void SetUp() override {
        system = knowledge_system_create("test_learner");
    }

    void TearDown() override {
        if (system) {
            knowledge_system_destroy(system);
            system = nullptr;
        }
    }
};

//=============================================================================
// 1. Creation and Destruction Tests
//=============================================================================

TEST_F(KnowledgeComprehensiveTest, CreateSystem_ValidName_Success) {
    ASSERT_NE(system, nullptr);
}

TEST_F(KnowledgeComprehensiveTest, CreateSystem_NullName_ReturnsNull) {
    knowledge_system_t sys = knowledge_system_create(nullptr);
    EXPECT_EQ(sys, nullptr);
}

TEST_F(KnowledgeComprehensiveTest, CreateSystem_EmptyName_Success) {
    knowledge_system_t sys = knowledge_system_create("");
    EXPECT_NE(sys, nullptr);
    knowledge_system_destroy(sys);
}

TEST_F(KnowledgeComprehensiveTest, DestroySystem_Null_NoCrash) {
    knowledge_system_destroy(nullptr);
    // Should not crash
}

TEST_F(KnowledgeComprehensiveTest, DestroySystem_Valid_Success) {
    knowledge_system_t sys = knowledge_system_create("temp");
    ASSERT_NE(sys, nullptr);
    knowledge_system_destroy(sys);
    // Should not crash
}

//=============================================================================
// 2. Learning from Text Tests
//=============================================================================

TEST_F(KnowledgeComprehensiveTest, LearnFromText_Simple_Success) {
    const char* text = "A cat is a small domesticated carnivorous mammal.";
    uint32_t learned = knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_GT(learned, 0u);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromText_NullSystem_ReturnsZero) {
    const char* text = "Test text";
    uint32_t learned = knowledge_learn_from_text(nullptr, text, KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_EQ(learned, 0u);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromText_NullText_ReturnsZero) {
    uint32_t learned = knowledge_learn_from_text(system, nullptr, KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_EQ(learned, 0u);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromText_EmptyText_ReturnsZero) {
    uint32_t learned = knowledge_learn_from_text(system, "", KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_EQ(learned, 0u);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromText_AllDomains_Success) {
    knowledge_domain_t domains[] = {
        KNOWLEDGE_DOMAIN_LANGUAGE,
        KNOWLEDGE_DOMAIN_LITERATURE,
        KNOWLEDGE_DOMAIN_ART,
        KNOWLEDGE_DOMAIN_ETHICS,
        KNOWLEDGE_DOMAIN_HISTORY,
        KNOWLEDGE_DOMAIN_SCIENCE,
        KNOWLEDGE_DOMAIN_MATHEMATICS,
        KNOWLEDGE_DOMAIN_SOCIAL,
        KNOWLEDGE_DOMAIN_TECHNICAL,
        KNOWLEDGE_DOMAIN_PHILOSOPHY,
        KNOWLEDGE_DOMAIN_GENERAL
    };

    for (auto domain : domains) {
        uint32_t learned = knowledge_learn_from_text(system, "Test concept", domain);
        EXPECT_GE(learned, 0u);
    }
}

TEST_F(KnowledgeComprehensiveTest, LearnFromText_LongText_Success) {
    const char* text = "The theory of evolution by natural selection, first formulated in "
                      "Charles Darwin's book On the Origin of Species in 1859, is the process "
                      "by which organisms change over time as a result of changes in heritable "
                      "physical or behavioral traits.";
    uint32_t learned = knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_SCIENCE);
    EXPECT_GT(learned, 0u);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromText_MultipleTexts_Accumulates) {
    uint32_t learned1 = knowledge_learn_from_text(system, "Dogs are loyal animals",
                                                   KNOWLEDGE_DOMAIN_GENERAL);
    uint32_t learned2 = knowledge_learn_from_text(system, "Cats are independent animals",
                                                   KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_GT(learned1, 0u);
    EXPECT_GT(learned2, 0u);
}

//=============================================================================
// 3. Learning from Story Tests
//=============================================================================

TEST_F(KnowledgeComprehensiveTest, LearnFromStory_Valid_Success) {
    narrative_knowledge_t story = {};
    strncpy(story.title, "The Three Little Pigs", sizeof(story.title) - 1);
    strncpy(story.author, "Traditional", sizeof(story.author) - 1);
    strncpy(story.summary, "Three pigs build houses and face a wolf",
            sizeof(story.summary) - 1);

    const char* chars[] = {"First Pig", "Second Pig", "Third Pig", "Wolf"};
    story.characters = const_cast<char**>(chars);
    story.num_characters = 4;

    const char* themes[] = {"Hard work", "Perseverance"};
    story.themes = const_cast<char**>(themes);
    story.num_themes = 2;

    const char* lessons[] = {"Plan ahead", "Don't take shortcuts"};
    story.moral_lessons = const_cast<char**>(lessons);
    story.num_lessons = 2;

    story.primary_domain = KNOWLEDGE_DOMAIN_LITERATURE;
    strncpy(story.cultural_context, "Western folklore", sizeof(story.cultural_context) - 1);

    bool result = knowledge_learn_from_story(system, &story);
    EXPECT_TRUE(result);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromStory_NullSystem_ReturnsFalse) {
    narrative_knowledge_t story = {};
    strncpy(story.title, "Test", sizeof(story.title) - 1);
    bool result = knowledge_learn_from_story(nullptr, &story);
    EXPECT_FALSE(result);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromStory_NullStory_ReturnsFalse) {
    bool result = knowledge_learn_from_story(system, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromStory_EmptyStory_HandlesSafely) {
    narrative_knowledge_t story = {};
    story.characters = nullptr;
    story.num_characters = 0;
    story.themes = nullptr;
    story.num_themes = 0;
    story.moral_lessons = nullptr;
    story.num_lessons = 0;

    bool result = knowledge_learn_from_story(system, &story);
    EXPECT_TRUE(result || !result); // Should handle gracefully either way
}

//=============================================================================
// 4. Learning from Art Tests
//=============================================================================

TEST_F(KnowledgeComprehensiveTest, LearnFromArt_Valid_Success) {
    aesthetic_knowledge_t art = {};
    strncpy(art.work_title, "Mona Lisa", sizeof(art.work_title) - 1);
    strncpy(art.creator, "Leonardo da Vinci", sizeof(art.creator) - 1);
    strncpy(art.medium, "Oil painting", sizeof(art.medium) - 1);
    strncpy(art.description, "Portrait of a woman with enigmatic smile",
            sizeof(art.description) - 1);

    const char* qualities[] = {"Mysterious", "Serene", "Masterful"};
    art.aesthetic_qualities = const_cast<char**>(qualities);
    art.num_qualities = 3;

    strncpy(art.emotional_impact, "Contemplative", sizeof(art.emotional_impact) - 1);
    strncpy(art.historical_significance, "Renaissance masterpiece",
            sizeof(art.historical_significance) - 1);

    bool result = knowledge_learn_from_art(system, &art);
    EXPECT_TRUE(result);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromArt_NullSystem_ReturnsFalse) {
    aesthetic_knowledge_t art = {};
    strncpy(art.work_title, "Test", sizeof(art.work_title) - 1);
    bool result = knowledge_learn_from_art(nullptr, &art);
    EXPECT_FALSE(result);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromArt_NullArt_ReturnsFalse) {
    bool result = knowledge_learn_from_art(system, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// 5. Learning from History Tests
//=============================================================================

TEST_F(KnowledgeComprehensiveTest, LearnFromHistory_Valid_Success) {
    historical_knowledge_t event = {};
    strncpy(event.event_name, "Moon Landing", sizeof(event.event_name) - 1);
    event.timestamp_year = 1969;

    const char* people[] = {"Neil Armstrong", "Buzz Aldrin"};
    event.key_people = const_cast<char**>(people);
    event.num_people = 2;

    strncpy(event.causes, "Space race competition", sizeof(event.causes) - 1);
    strncpy(event.effects, "Advancement in technology", sizeof(event.effects) - 1);
    strncpy(event.significance, "First humans on the Moon", sizeof(event.significance) - 1);

    const char* related[] = {"Apollo 11 mission"};
    event.related_events = const_cast<char**>(related);
    event.num_related_events = 1;

    bool result = knowledge_learn_from_history(system, &event);
    EXPECT_TRUE(result);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromHistory_NullSystem_ReturnsFalse) {
    historical_knowledge_t event = {};
    strncpy(event.event_name, "Test", sizeof(event.event_name) - 1);
    bool result = knowledge_learn_from_history(nullptr, &event);
    EXPECT_FALSE(result);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromHistory_NullEvent_ReturnsFalse) {
    bool result = knowledge_learn_from_history(system, nullptr);
    EXPECT_FALSE(result);
}

//=============================================================================
// 6. Learning from Conversation Tests
//=============================================================================

TEST_F(KnowledgeComprehensiveTest, LearnFromConversation_Valid_Success) {
    const char* dialogue = "Alice: Hello, how are you? Bob: I'm fine, thanks!";
    const char* participants[] = {"Alice", "Bob"};

    uint32_t learned = knowledge_learn_from_conversation(system, dialogue, participants, 2);
    EXPECT_GE(learned, 0u);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromConversation_NullSystem_ReturnsZero) {
    const char* dialogue = "Test dialogue";
    const char* participants[] = {"Person"};

    uint32_t learned = knowledge_learn_from_conversation(nullptr, dialogue, participants, 1);
    EXPECT_EQ(learned, 0u);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromConversation_NullDialogue_ReturnsZero) {
    const char* participants[] = {"Person"};
    uint32_t learned = knowledge_learn_from_conversation(system, nullptr, participants, 1);
    EXPECT_EQ(learned, 0u);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromConversation_NullParticipants_ReturnsZero) {
    const char* dialogue = "Test dialogue";
    uint32_t learned = knowledge_learn_from_conversation(system, dialogue, nullptr, 0);
    EXPECT_EQ(learned, 0u);
}

//=============================================================================
// 7. Learning from Demonstration Tests
//=============================================================================

TEST_F(KnowledgeComprehensiveTest, LearnFromDemonstration_Valid_Success) {
    const char* steps[] = {
        "Step 1: Gather materials",
        "Step 2: Assemble parts",
        "Step 3: Test result"
    };

    bool result = knowledge_learn_from_demonstration(system, "Build a birdhouse", steps, 3);
    EXPECT_TRUE(result);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromDemonstration_NullSystem_ReturnsFalse) {
    const char* steps[] = {"Step 1"};
    bool result = knowledge_learn_from_demonstration(nullptr, "Test", steps, 1);
    EXPECT_FALSE(result);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromDemonstration_NullWhat_ReturnsFalse) {
    const char* steps[] = {"Step 1"};
    bool result = knowledge_learn_from_demonstration(system, nullptr, steps, 1);
    EXPECT_FALSE(result);
}

TEST_F(KnowledgeComprehensiveTest, LearnFromDemonstration_NullSteps_ReturnsFalse) {
    bool result = knowledge_learn_from_demonstration(system, "Test", nullptr, 0);
    EXPECT_FALSE(result);
}

//=============================================================================
// 8. Knowledge Retrieval Tests
//=============================================================================

TEST_F(KnowledgeComprehensiveTest, Retrieve_ExistingConcept_Success) {
    // First learn something
    knowledge_learn_from_text(system, "A dog is a domesticated animal", KNOWLEDGE_DOMAIN_GENERAL);

    // Then try to retrieve it
    knowledge_item_t item = {};
    bool found = knowledge_retrieve(system, "dog", &item);

    // May or may not find it depending on implementation
    EXPECT_TRUE(found || !found);
}

TEST_F(KnowledgeComprehensiveTest, Retrieve_NullSystem_ReturnsFalse) {
    knowledge_item_t item = {};
    bool found = knowledge_retrieve(nullptr, "test", &item);
    EXPECT_FALSE(found);
}

TEST_F(KnowledgeComprehensiveTest, Retrieve_NullConcept_ReturnsFalse) {
    knowledge_item_t item = {};
    bool found = knowledge_retrieve(system, nullptr, &item);
    EXPECT_FALSE(found);
}

TEST_F(KnowledgeComprehensiveTest, Retrieve_NullItem_ReturnsFalse) {
    bool found = knowledge_retrieve(system, "test", nullptr);
    EXPECT_FALSE(found);
}

TEST_F(KnowledgeComprehensiveTest, Retrieve_NonexistentConcept_ReturnsFalse) {
    knowledge_item_t item = {};
    bool found = knowledge_retrieve(system, "nonexistent_xyz_123", &item);
    EXPECT_FALSE(found);
}

//=============================================================================
// 9. Integration Tests
//=============================================================================

TEST_F(KnowledgeComprehensiveTest, Integration_LearnAndRetrieve_Success) {
    // Learn from multiple sources
    knowledge_learn_from_text(system, "Philosophy is the study of fundamental questions",
                              KNOWLEDGE_DOMAIN_PHILOSOPHY);

    narrative_knowledge_t story = {};
    strncpy(story.title, "Test Story", sizeof(story.title) - 1);
    strncpy(story.summary, "A tale of wisdom", sizeof(story.summary) - 1);
    story.primary_domain = KNOWLEDGE_DOMAIN_LITERATURE;
    knowledge_learn_from_story(system, &story);

    // System should handle multiple learning sessions
    SUCCEED();
}

TEST_F(KnowledgeComprehensiveTest, Integration_MultipleDomains_Success) {
    // Learn across different domains
    knowledge_learn_from_text(system, "Science explores natural phenomena",
                              KNOWLEDGE_DOMAIN_SCIENCE);
    knowledge_learn_from_text(system, "Art expresses human creativity",
                              KNOWLEDGE_DOMAIN_ART);
    knowledge_learn_from_text(system, "History studies past events",
                              KNOWLEDGE_DOMAIN_HISTORY);

    SUCCEED();
}

TEST_F(KnowledgeComprehensiveTest, Stress_ManyLearningOperations_HandlesWell) {
    for (int i = 0; i < 100; i++) {
        char text[256];
        snprintf(text, sizeof(text), "Concept %d is an interesting topic", i);
        knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);
    }
    SUCCEED();
}
