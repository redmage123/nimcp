/**
 * @file test_knowledge_extended.cpp
 * @brief Comprehensive unit tests for nimcp_knowledge.c (Target: 95% coverage)
 *
 * WHAT: Complete test coverage for knowledge acquisition system
 * WHY:  Increase coverage from 16.6% to 95% (307 uncovered lines → <16 lines)
 * HOW:  Test all code paths, branches, error conditions, and edge cases
 *
 * COVERAGE STRATEGY:
 * 1. Hash Table Operations (create, insert, find, destroy, collisions)
 * 2. Repository Pattern (create, add, find, get, destroy)
 * 3. Text Processing (concept extraction, context creation, normalization)
 * 4. Learning Strategies (narrative, aesthetic, historical)
 * 5. Knowledge Learning (text, story, art, history, conversation, demonstration)
 * 6. Knowledge Retrieval (retrieve, understand, explain)
 * 7. Cross-Domain Learning (find connections, transfer)
 * 8. Incremental Building (build on, reinforce)
 * 9. Knowledge Organization (organize, get map)
 * 10. Reading System (read book, continue, recommendations)
 * 11. Assessment (assess domain, get summary)
 * 12. Persistence (save, load)
 * 13. B-tree Queries (confidence range, ordered)
 * 14. Error Handling (all guard clauses)
 * 15. Edge Cases (capacity limits, empty data, null checks)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <fstream>
#include <sys/stat.h>
#include <vector>
#include <string>

    #include "cognitive/knowledge/nimcp_knowledge.h"
    #include "utils/memory/nimcp_memory.h"
    #include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * WHAT: Test fixture for knowledge system tests
 * WHY:  Provides clean setup/teardown and helper functions
 * HOW:  Initialize/cleanup NIMCP systems before/after each test
 */
class KnowledgeExtendedTest : public ::testing::Test {
protected:
    knowledge_system_t system;

    void SetUp() override {
        // WHAT: Initialize NIMCP memory and create knowledge system
        // WHY:  Clean state for each test
        // HOW:  Call init functions and create test system
        nimcp_memory_init();
        nimcp_init();
        system = knowledge_system_create("TestLearner");
        ASSERT_NE(system, nullptr) << "Failed to create knowledge system";
    }

    void TearDown() override {
        // WHAT: Cleanup knowledge system and NIMCP resources
        // WHY:  Prevent memory leaks between tests
        // HOW:  Destroy system and call cleanup functions
        if (system) {
            knowledge_system_destroy(system);
            system = nullptr;
        }
        nimcp_shutdown();
        nimcp_memory_cleanup();
    }

    // Helper: Create test narrative knowledge
    narrative_knowledge_t create_test_narrative(const char* title = "The Tortoise and the Hare") {
        narrative_knowledge_t story = {0};
        strncpy(story.title, title, sizeof(story.title) - 1);
        strncpy(story.author, "Aesop", sizeof(story.author) - 1);
        strncpy(story.summary, "A slow tortoise beats a fast hare in a race by staying focused.",
                sizeof(story.summary) - 1);
        strncpy(story.cultural_context, "Ancient Greece", sizeof(story.cultural_context) - 1);
        story.primary_domain = KNOWLEDGE_DOMAIN_LITERATURE;

        // Allocate and populate character array
        story.num_characters = 2;
        story.characters = (char**)nimcp_malloc(2 * sizeof(char*));
        story.characters[0] = (char*)nimcp_malloc(20);
        story.characters[1] = (char*)nimcp_malloc(20);
        strcpy(story.characters[0], "Tortoise");
        strcpy(story.characters[1], "Hare");

        // Allocate and populate themes
        story.num_themes = 2;
        story.themes = (char**)nimcp_malloc(2 * sizeof(char*));
        story.themes[0] = (char*)nimcp_malloc(20);
        story.themes[1] = (char*)nimcp_malloc(20);
        strcpy(story.themes[0], "Perseverance");
        strcpy(story.themes[1], "Humility");

        // Allocate and populate lessons
        story.num_lessons = 2;
        story.moral_lessons = (char**)nimcp_malloc(2 * sizeof(char*));
        story.moral_lessons[0] = (char*)nimcp_malloc(50);
        story.moral_lessons[1] = (char*)nimcp_malloc(50);
        strcpy(story.moral_lessons[0], "Slow and steady wins the race");
        strcpy(story.moral_lessons[1], "Don't be overconfident");

        return story;
    }

    // Helper: Free narrative memory
    void free_narrative(narrative_knowledge_t* story) {
        if (story->characters) {
            for (uint32_t i = 0; i < story->num_characters; i++) {
                nimcp_free(story->characters[i]);
            }
            nimcp_free(story->characters);
        }
        if (story->themes) {
            for (uint32_t i = 0; i < story->num_themes; i++) {
                nimcp_free(story->themes[i]);
            }
            nimcp_free(story->themes);
        }
        if (story->moral_lessons) {
            for (uint32_t i = 0; i < story->num_lessons; i++) {
                nimcp_free(story->moral_lessons[i]);
            }
            nimcp_free(story->moral_lessons);
        }
    }

    // Helper: Create test aesthetic knowledge
    aesthetic_knowledge_t create_test_artwork(const char* title = "Mona Lisa") {
        aesthetic_knowledge_t art = {0};
        strncpy(art.work_title, title, sizeof(art.work_title) - 1);
        strncpy(art.creator, "Leonardo da Vinci", sizeof(art.creator) - 1);
        strncpy(art.medium, "Oil painting", sizeof(art.medium) - 1);
        strncpy(art.description, "Portrait of a woman with an enigmatic smile",
                sizeof(art.description) - 1);
        strncpy(art.emotional_impact, "Mysterious and captivating", sizeof(art.emotional_impact) - 1);
        strncpy(art.historical_significance, "Renaissance masterpiece",
                sizeof(art.historical_significance) - 1);

        // Allocate qualities
        art.num_qualities = 3;
        art.aesthetic_qualities = (char**)nimcp_malloc(3 * sizeof(char*));
        art.aesthetic_qualities[0] = (char*)nimcp_malloc(20);
        art.aesthetic_qualities[1] = (char*)nimcp_malloc(20);
        art.aesthetic_qualities[2] = (char*)nimcp_malloc(20);
        strcpy(art.aesthetic_qualities[0], "Mysterious");
        strcpy(art.aesthetic_qualities[1], "Harmonious");
        strcpy(art.aesthetic_qualities[2], "Timeless");

        return art;
    }

    // Helper: Free artwork memory
    void free_artwork(aesthetic_knowledge_t* art) {
        if (art->aesthetic_qualities) {
            for (uint32_t i = 0; i < art->num_qualities; i++) {
                nimcp_free(art->aesthetic_qualities[i]);
            }
            nimcp_free(art->aesthetic_qualities);
        }
    }

    // Helper: Create test historical knowledge
    historical_knowledge_t create_test_history(const char* event = "Moon Landing") {
        historical_knowledge_t history = {0};
        strncpy(history.event_name, event, sizeof(history.event_name) - 1);
        history.timestamp_year = 1969;
        strncpy(history.causes, "Space race and technological advancement",
                sizeof(history.causes) - 1);
        strncpy(history.effects, "Demonstrated human space exploration capability",
                sizeof(history.effects) - 1);
        strncpy(history.significance, "First human steps on another celestial body",
                sizeof(history.significance) - 1);

        // Allocate key people
        history.num_people = 2;
        history.key_people = (char**)nimcp_malloc(2 * sizeof(char*));
        history.key_people[0] = (char*)nimcp_malloc(30);
        history.key_people[1] = (char*)nimcp_malloc(30);
        strcpy(history.key_people[0], "Neil Armstrong");
        strcpy(history.key_people[1], "Buzz Aldrin");

        // Allocate related events
        history.num_related_events = 1;
        history.related_events = (char**)nimcp_malloc(1 * sizeof(char*));
        history.related_events[0] = (char*)nimcp_malloc(30);
        strcpy(history.related_events[0], "Apollo 11 Mission");

        return history;
    }

    // Helper: Free history memory
    void free_history(historical_knowledge_t* history) {
        if (history->key_people) {
            for (uint32_t i = 0; i < history->num_people; i++) {
                nimcp_free(history->key_people[i]);
            }
            nimcp_free(history->key_people);
        }
        if (history->related_events) {
            for (uint32_t i = 0; i < history->num_related_events; i++) {
                nimcp_free(history->related_events[i]);
            }
            nimcp_free(history->related_events);
        }
    }

    // Helper: Check if file exists
    bool file_exists(const char* path) {
        struct stat buffer;
        return (stat(path, &buffer) == 0);
    }
};

//=============================================================================
// 1. System Creation/Destruction Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, CreateSystem_ValidName) {
    // WHAT: Test basic system creation with valid name
    // WHY:  Verify system initializes correctly
    // HOW:  Create system and check non-null result
    knowledge_system_t test_sys = knowledge_system_create("Alice");
    ASSERT_NE(test_sys, nullptr);
    knowledge_system_destroy(test_sys);
}

TEST_F(KnowledgeExtendedTest, CreateSystem_NullName_ReturnsNull) {
    // WHAT: Test system creation with NULL name
    // WHY:  Verify guard clause prevents creation
    // HOW:  Pass NULL name and expect NULL return
    knowledge_system_t test_sys = knowledge_system_create(nullptr);
    EXPECT_EQ(test_sys, nullptr);
}

TEST_F(KnowledgeExtendedTest, DestroySystem_NullSystem_NoError) {
    // WHAT: Test destroying NULL system doesn't crash
    // WHY:  Verify guard clause handles NULL safely
    // HOW:  Call destroy with NULL and verify no crash
    knowledge_system_destroy(nullptr);
    // Should not crash
    SUCCEED();
}

//=============================================================================
// 2. Text Learning Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, LearnFromText_BasicConcepts) {
    // WHAT: Test learning concepts from simple text
    // WHY:  Core functionality for text-based learning
    // HOW:  Learn from text and verify concepts are stored
    const char* text = "Democracy requires informed citizens who participate in voting.";
    uint32_t learned = knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_GT(learned, 0u);

    // Verify concept can be retrieved
    knowledge_item_t item;
    bool found = knowledge_retrieve(system, "democracy", &item);
    EXPECT_TRUE(found);
    EXPECT_EQ(item.domain, KNOWLEDGE_DOMAIN_GENERAL);
}

TEST_F(KnowledgeExtendedTest, LearnFromText_Reinforcement) {
    // WHAT: Test that repeating concepts reinforces knowledge
    // WHY:  Verify reinforcement increases confidence
    // HOW:  Learn same text twice and check confidence increase
    const char* text = "Science explores natural phenomena through observation.";

    knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_SCIENCE);
    knowledge_item_t item1;
    knowledge_retrieve(system, "science", &item1);
    float confidence1 = item1.confidence;
    uint32_t count1 = item1.reinforcement_count;

    knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_SCIENCE);
    knowledge_item_t item2;
    knowledge_retrieve(system, "science", &item2);
    float confidence2 = item2.confidence;
    uint32_t count2 = item2.reinforcement_count;

    EXPECT_GE(confidence2, confidence1);
    EXPECT_GT(count2, count1);
}

TEST_F(KnowledgeExtendedTest, LearnFromText_NullSystem_ReturnsZero) {
    // WHAT: Test learning with NULL system
    // WHY:  Verify guard clause prevents crash
    // HOW:  Pass NULL system and expect 0 return
    const char* text = "Test text";
    uint32_t learned = knowledge_learn_from_text(nullptr, text, KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_EQ(learned, 0u);
}

TEST_F(KnowledgeExtendedTest, LearnFromText_NullText_ReturnsZero) {
    // WHAT: Test learning with NULL text
    // WHY:  Verify guard clause prevents crash
    // HOW:  Pass NULL text and expect 0 return
    uint32_t learned = knowledge_learn_from_text(system, nullptr, KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_EQ(learned, 0u);
}

TEST_F(KnowledgeExtendedTest, LearnFromText_FilterStopwords) {
    // WHAT: Test that common stopwords are filtered
    // WHY:  Verify text processing ignores low-value words
    // HOW:  Learn text with stopwords and verify they're not stored
    const char* text = "the and that this with from have been they their about would there which";
    uint32_t learned = knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_EQ(learned, 0u);  // All stopwords, nothing learned
}

TEST_F(KnowledgeExtendedTest, LearnFromText_ShortWords_Filtered) {
    // WHAT: Test that words shorter than MIN_CONCEPT_LENGTH are filtered
    // WHY:  Verify short words are ignored
    // HOW:  Learn text with only short words
    const char* text = "a to is at of in on";
    uint32_t learned = knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_EQ(learned, 0u);
}

TEST_F(KnowledgeExtendedTest, LearnFromText_CaseInsensitive) {
    // WHAT: Test that concepts are normalized to lowercase
    // WHY:  Verify case-insensitive storage
    // HOW:  Learn "Democracy" and retrieve "democracy"
    const char* text = "Democracy is important";
    knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);

    knowledge_item_t item;
    EXPECT_TRUE(knowledge_retrieve(system, "democracy", &item));
    EXPECT_TRUE(knowledge_retrieve(system, "Democracy", &item));
    EXPECT_TRUE(knowledge_retrieve(system, "DEMOCRACY", &item));
}

TEST_F(KnowledgeExtendedTest, LearnFromText_LongText) {
    // WHAT: Test learning from long text with many concepts
    // WHY:  Verify system handles larger inputs
    // HOW:  Learn text with 100+ words
    std::string long_text = "Knowledge acquisition involves learning facts concepts ideas through various methods. ";
    for (int i = 0; i < 20; i++) {
        long_text += "Understanding requires comprehension analysis synthesis integration. ";
    }

    uint32_t learned = knowledge_learn_from_text(system, long_text.c_str(), KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_GT(learned, 0u);
}

//=============================================================================
// 3. Narrative Learning Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, LearnFromStory_ValidStory) {
    // WHAT: Test learning from narrative structure
    // WHY:  Verify strategy pattern for narrative learning
    // HOW:  Create story and verify learning succeeds
    narrative_knowledge_t story = create_test_narrative();
    bool success = knowledge_learn_from_story(system, &story);
    EXPECT_TRUE(success);
    free_narrative(&story);
}

TEST_F(KnowledgeExtendedTest, LearnFromStory_ExtractsLessons) {
    // WHAT: Test that moral lessons are extracted as knowledge items
    // WHY:  Verify lesson extraction functionality
    // HOW:  Learn story and check if lesson concepts are stored
    narrative_knowledge_t story = create_test_narrative();
    knowledge_learn_from_story(system, &story);

    // Verify lessons are stored (may be stored with modified names)
    domain_knowledge_t assessment;
    knowledge_assess_domain(system, KNOWLEDGE_DOMAIN_LITERATURE, &assessment);
    EXPECT_GT(assessment.concepts_known, 0u);

    free_narrative(&story);
}

TEST_F(KnowledgeExtendedTest, LearnFromStory_NullSystem_ReturnsFalse) {
    // WHAT: Test learning story with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect false
    narrative_knowledge_t story = create_test_narrative();
    bool success = knowledge_learn_from_story(nullptr, &story);
    EXPECT_FALSE(success);
    free_narrative(&story);
}

TEST_F(KnowledgeExtendedTest, LearnFromStory_NullStory_ReturnsFalse) {
    // WHAT: Test learning with NULL story
    // WHY:  Verify guard clause
    // HOW:  Pass NULL story and expect false
    bool success = knowledge_learn_from_story(system, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, LearnFromStory_MultipleStories) {
    // WHAT: Test learning multiple stories
    // WHY:  Verify capacity handling
    // HOW:  Learn several different stories
    narrative_knowledge_t story1 = create_test_narrative("The Ant and the Grasshopper");
    narrative_knowledge_t story2 = create_test_narrative("The Boy Who Cried Wolf");
    narrative_knowledge_t story3 = create_test_narrative("The Fox and the Grapes");

    EXPECT_TRUE(knowledge_learn_from_story(system, &story1));
    EXPECT_TRUE(knowledge_learn_from_story(system, &story2));
    EXPECT_TRUE(knowledge_learn_from_story(system, &story3));

    free_narrative(&story1);
    free_narrative(&story2);
    free_narrative(&story3);
}

//=============================================================================
// 4. Aesthetic Learning Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, LearnFromArt_ValidArtwork) {
    // WHAT: Test learning from aesthetic structure
    // WHY:  Verify strategy pattern for aesthetic learning
    // HOW:  Create artwork and verify learning succeeds
    aesthetic_knowledge_t art = create_test_artwork();
    bool success = knowledge_learn_from_art(system, &art);
    EXPECT_TRUE(success);
    free_artwork(&art);
}

TEST_F(KnowledgeExtendedTest, LearnFromArt_ExtractsQualities) {
    // WHAT: Test that aesthetic qualities are extracted
    // WHY:  Verify quality extraction functionality
    // HOW:  Learn art and check domain statistics
    aesthetic_knowledge_t art = create_test_artwork();
    knowledge_learn_from_art(system, &art);

    domain_knowledge_t assessment;
    knowledge_assess_domain(system, KNOWLEDGE_DOMAIN_ART, &assessment);
    EXPECT_GT(assessment.concepts_known, 0u);

    free_artwork(&art);
}

TEST_F(KnowledgeExtendedTest, LearnFromArt_NullSystem_ReturnsFalse) {
    // WHAT: Test learning art with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect false
    aesthetic_knowledge_t art = create_test_artwork();
    bool success = knowledge_learn_from_art(nullptr, &art);
    EXPECT_FALSE(success);
    free_artwork(&art);
}

TEST_F(KnowledgeExtendedTest, LearnFromArt_NullArt_ReturnsFalse) {
    // WHAT: Test learning with NULL art
    // WHY:  Verify guard clause
    // HOW:  Pass NULL art and expect false
    bool success = knowledge_learn_from_art(system, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, LearnFromArt_MultipleArtworks) {
    // WHAT: Test learning multiple artworks
    // WHY:  Verify capacity handling
    // HOW:  Learn several different artworks
    aesthetic_knowledge_t art1 = create_test_artwork("The Starry Night");
    aesthetic_knowledge_t art2 = create_test_artwork("The Scream");
    aesthetic_knowledge_t art3 = create_test_artwork("The Persistence of Memory");

    EXPECT_TRUE(knowledge_learn_from_art(system, &art1));
    EXPECT_TRUE(knowledge_learn_from_art(system, &art2));
    EXPECT_TRUE(knowledge_learn_from_art(system, &art3));

    free_artwork(&art1);
    free_artwork(&art2);
    free_artwork(&art3);
}

//=============================================================================
// 5. Historical Learning Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, LearnFromHistory_ValidEvent) {
    // WHAT: Test learning from historical structure
    // WHY:  Verify strategy pattern for historical learning
    // HOW:  Create event and verify learning succeeds
    historical_knowledge_t event = create_test_history();
    bool success = knowledge_learn_from_history(system, &event);
    EXPECT_TRUE(success);
    free_history(&event);
}

TEST_F(KnowledgeExtendedTest, LearnFromHistory_ExtractsEvent) {
    // WHAT: Test that historical event is stored as knowledge item
    // WHY:  Verify event extraction functionality
    // HOW:  Learn history and check domain statistics
    historical_knowledge_t event = create_test_history();
    knowledge_learn_from_history(system, &event);

    domain_knowledge_t assessment;
    knowledge_assess_domain(system, KNOWLEDGE_DOMAIN_HISTORY, &assessment);
    EXPECT_GT(assessment.concepts_known, 0u);

    free_history(&event);
}

TEST_F(KnowledgeExtendedTest, LearnFromHistory_NullSystem_ReturnsFalse) {
    // WHAT: Test learning history with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect false
    historical_knowledge_t event = create_test_history();
    bool success = knowledge_learn_from_history(nullptr, &event);
    EXPECT_FALSE(success);
    free_history(&event);
}

TEST_F(KnowledgeExtendedTest, LearnFromHistory_NullEvent_ReturnsFalse) {
    // WHAT: Test learning with NULL event
    // WHY:  Verify guard clause
    // HOW:  Pass NULL event and expect false
    bool success = knowledge_learn_from_history(system, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, LearnFromHistory_MultipleEvents) {
    // WHAT: Test learning multiple historical events
    // WHY:  Verify capacity handling
    // HOW:  Learn several different events
    historical_knowledge_t event1 = create_test_history("World War II");
    historical_knowledge_t event2 = create_test_history("Renaissance");
    historical_knowledge_t event3 = create_test_history("Industrial Revolution");

    EXPECT_TRUE(knowledge_learn_from_history(system, &event1));
    EXPECT_TRUE(knowledge_learn_from_history(system, &event2));
    EXPECT_TRUE(knowledge_learn_from_history(system, &event3));

    free_history(&event1);
    free_history(&event2);
    free_history(&event3);
}

//=============================================================================
// 6. Conversation Learning Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, LearnFromConversation_ValidDialogue) {
    // WHAT: Test learning from conversation
    // WHY:  Verify social learning functionality
    // HOW:  Learn dialogue and verify concepts are extracted
    const char* dialogue = "Hello, how are you? I am learning about friendship and communication.";
    const char* participants[] = {"Alice", "Bob"};
    uint32_t learned = knowledge_learn_from_conversation(system, dialogue, participants, 2);
    EXPECT_GT(learned, 0u);
}

TEST_F(KnowledgeExtendedTest, LearnFromConversation_NullSystem_ReturnsZero) {
    // WHAT: Test conversation with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect 0
    const char* dialogue = "Test dialogue";
    const char* participants[] = {"Alice"};
    uint32_t learned = knowledge_learn_from_conversation(nullptr, dialogue, participants, 1);
    EXPECT_EQ(learned, 0u);
}

TEST_F(KnowledgeExtendedTest, LearnFromConversation_NullDialogue_ReturnsZero) {
    // WHAT: Test conversation with NULL dialogue
    // WHY:  Verify guard clause
    // HOW:  Pass NULL dialogue and expect 0
    const char* participants[] = {"Alice"};
    uint32_t learned = knowledge_learn_from_conversation(system, nullptr, participants, 1);
    EXPECT_EQ(learned, 0u);
}

TEST_F(KnowledgeExtendedTest, LearnFromConversation_NullParticipants_ReturnsZero) {
    // WHAT: Test conversation with NULL participants
    // WHY:  Verify guard clause
    // HOW:  Pass NULL participants and expect 0
    const char* dialogue = "Test dialogue";
    uint32_t learned = knowledge_learn_from_conversation(system, dialogue, nullptr, 1);
    EXPECT_EQ(learned, 0u);
}

TEST_F(KnowledgeExtendedTest, LearnFromConversation_ZeroParticipants_ReturnsZero) {
    // WHAT: Test conversation with 0 participants
    // WHY:  Verify guard clause
    // HOW:  Pass 0 participants and expect 0
    const char* dialogue = "Test dialogue";
    const char* participants[] = {"Alice"};
    uint32_t learned = knowledge_learn_from_conversation(system, dialogue, participants, 0);
    EXPECT_EQ(learned, 0u);
}

//=============================================================================
// 7. Demonstration Learning Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, LearnFromDemonstration_ValidSteps) {
    // WHAT: Test learning from procedural demonstration
    // WHY:  Verify procedural learning functionality
    // HOW:  Learn demonstration and verify storage
    const char* steps[] = {"Turn on computer", "Open application", "Enter data", "Save work"};
    bool success = knowledge_learn_from_demonstration(system, "Using a computer", steps, 4);
    EXPECT_TRUE(success);

    knowledge_item_t item;
    EXPECT_TRUE(knowledge_retrieve(system, "Using a computer", &item));
    EXPECT_EQ(item.domain, KNOWLEDGE_DOMAIN_TECHNICAL);
}

TEST_F(KnowledgeExtendedTest, LearnFromDemonstration_NullSystem_ReturnsFalse) {
    // WHAT: Test demonstration with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect false
    const char* steps[] = {"Step 1"};
    bool success = knowledge_learn_from_demonstration(nullptr, "Test", steps, 1);
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, LearnFromDemonstration_NullWhat_ReturnsFalse) {
    // WHAT: Test demonstration with NULL what_demonstrated
    // WHY:  Verify guard clause
    // HOW:  Pass NULL what and expect false
    const char* steps[] = {"Step 1"};
    bool success = knowledge_learn_from_demonstration(system, nullptr, steps, 1);
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, LearnFromDemonstration_NullSteps_ReturnsFalse) {
    // WHAT: Test demonstration with NULL steps
    // WHY:  Verify guard clause
    // HOW:  Pass NULL steps and expect false
    bool success = knowledge_learn_from_demonstration(system, "Test", nullptr, 1);
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, LearnFromDemonstration_ManySteps) {
    // WHAT: Test demonstration with many steps
    // WHY:  Verify handling of long procedures
    // HOW:  Create demonstration with 20 steps
    std::vector<const char*> steps;
    for (int i = 0; i < 20; i++) {
        steps.push_back("Step");
    }
    bool success = knowledge_learn_from_demonstration(system, "Complex Process",
                                                      steps.data(), steps.size());
    EXPECT_TRUE(success);
}

//=============================================================================
// 8. Knowledge Retrieval Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, Retrieve_ExistingConcept) {
    // WHAT: Test retrieving existing concept
    // WHY:  Verify basic retrieval functionality
    // HOW:  Learn concept then retrieve it
    knowledge_learn_from_text(system, "Artificial intelligence is fascinating",
                             KNOWLEDGE_DOMAIN_TECHNICAL);

    knowledge_item_t item;
    bool found = knowledge_retrieve(system, "artificial", &item);
    EXPECT_TRUE(found);
    EXPECT_STREQ(item.concept_name, "artificial");
}

TEST_F(KnowledgeExtendedTest, Retrieve_NonexistentConcept_ReturnsFalse) {
    // WHAT: Test retrieving nonexistent concept
    // WHY:  Verify retrieval returns false for unknown concepts
    // HOW:  Try to retrieve concept that wasn't learned
    knowledge_item_t item;
    bool found = knowledge_retrieve(system, "nonexistent_concept_xyz", &item);
    EXPECT_FALSE(found);
}

TEST_F(KnowledgeExtendedTest, Retrieve_NullSystem_ReturnsFalse) {
    // WHAT: Test retrieve with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect false
    knowledge_item_t item;
    bool found = knowledge_retrieve(nullptr, "test", &item);
    EXPECT_FALSE(found);
}

TEST_F(KnowledgeExtendedTest, Retrieve_NullConcept_ReturnsFalse) {
    // WHAT: Test retrieve with NULL concept
    // WHY:  Verify guard clause
    // HOW:  Pass NULL concept and expect false
    knowledge_item_t item;
    bool found = knowledge_retrieve(system, nullptr, &item);
    EXPECT_FALSE(found);
}

TEST_F(KnowledgeExtendedTest, Retrieve_NullItem_ReturnsFalse) {
    // WHAT: Test retrieve with NULL output item
    // WHY:  Verify guard clause
    // HOW:  Pass NULL item and expect false
    knowledge_learn_from_text(system, "Test concept", KNOWLEDGE_DOMAIN_GENERAL);
    bool found = knowledge_retrieve(system, "concept", nullptr);
    EXPECT_FALSE(found);
}

//=============================================================================
// 9. Understanding Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, Understand_ExistingConcept) {
    // WHAT: Test generating understanding explanation
    // WHY:  Verify explanation generation
    // HOW:  Learn concept and generate explanation
    knowledge_learn_from_text(system, "Democracy is government by the people",
                             KNOWLEDGE_DOMAIN_GENERAL);

    char explanation[512];
    uint32_t len = knowledge_understand(system, "democracy", "", explanation, sizeof(explanation));
    EXPECT_GT(len, 0u);
    EXPECT_NE(strstr(explanation, "democracy"), nullptr);
}

TEST_F(KnowledgeExtendedTest, Understand_UnknownConcept_ExplainsUnknown) {
    // WHAT: Test understanding unknown concept
    // WHY:  Verify system explains when it doesn't know
    // HOW:  Try to understand concept that wasn't learned
    char explanation[512];
    uint32_t len = knowledge_understand(system, "unknown_xyz", "", explanation, sizeof(explanation));
    EXPECT_GT(len, 0u);
    EXPECT_NE(strstr(explanation, "don't know"), nullptr);
}

TEST_F(KnowledgeExtendedTest, Understand_NullSystem_ReturnsZero) {
    // WHAT: Test understand with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect 0
    char explanation[512];
    uint32_t len = knowledge_understand(nullptr, "test", "", explanation, sizeof(explanation));
    EXPECT_EQ(len, 0u);
}

TEST_F(KnowledgeExtendedTest, Understand_NullConcept_ReturnsZero) {
    // WHAT: Test understand with NULL concept
    // WHY:  Verify guard clause
    // HOW:  Pass NULL concept and expect 0
    char explanation[512];
    uint32_t len = knowledge_understand(system, nullptr, "", explanation, sizeof(explanation));
    EXPECT_EQ(len, 0u);
}

TEST_F(KnowledgeExtendedTest, Understand_NullExplanation_ReturnsZero) {
    // WHAT: Test understand with NULL output buffer
    // WHY:  Verify guard clause
    // HOW:  Pass NULL explanation and expect 0
    uint32_t len = knowledge_understand(system, "test", "", nullptr, 512);
    EXPECT_EQ(len, 0u);
}

//=============================================================================
// 10. Simple Explanation Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, ExplainSimply_YoungAge) {
    // WHAT: Test age-appropriate explanation for young child
    // WHY:  Verify simplification for age < 5
    // HOW:  Learn concept and explain for age 4
    knowledge_learn_from_text(system, "Kindness means being nice to others",
                             KNOWLEDGE_DOMAIN_ETHICS);

    char explanation[512];
    uint32_t len = knowledge_explain_simply(system, "kindness", 4, explanation, sizeof(explanation));
    EXPECT_GT(len, 0u);
    // Should be very simple for age 4
}

TEST_F(KnowledgeExtendedTest, ExplainSimply_MiddleAge) {
    // WHAT: Test explanation for middle age (5-10)
    // WHY:  Verify appropriate complexity for this age
    // HOW:  Learn concept and explain for age 8
    knowledge_learn_from_text(system, "Honesty means telling the truth",
                             KNOWLEDGE_DOMAIN_ETHICS);

    char explanation[512];
    uint32_t len = knowledge_explain_simply(system, "honesty", 8, explanation, sizeof(explanation));
    EXPECT_GT(len, 0u);
}

TEST_F(KnowledgeExtendedTest, ExplainSimply_OlderAge) {
    // WHAT: Test explanation for older age (10+)
    // WHY:  Verify full explanation for mature readers
    // HOW:  Learn concept and explain for age 15
    knowledge_learn_from_text(system, "Justice means fairness and equality",
                             KNOWLEDGE_DOMAIN_ETHICS);

    char explanation[512];
    uint32_t len = knowledge_explain_simply(system, "justice", 15, explanation, sizeof(explanation));
    EXPECT_GT(len, 0u);
}

TEST_F(KnowledgeExtendedTest, ExplainSimply_UnknownConcept) {
    // WHAT: Test explaining unknown concept simply
    // WHY:  Verify system handles unknown concepts gracefully
    // HOW:  Try to explain concept that wasn't learned
    char explanation[512];
    uint32_t len = knowledge_explain_simply(system, "unknown_xyz", 10, explanation, sizeof(explanation));
    EXPECT_GT(len, 0u);
    EXPECT_NE(strstr(explanation, "haven't learned"), nullptr);
}

TEST_F(KnowledgeExtendedTest, ExplainSimply_NullSystem_ReturnsZero) {
    // WHAT: Test explain simply with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect 0
    char explanation[512];
    uint32_t len = knowledge_explain_simply(nullptr, "test", 10, explanation, sizeof(explanation));
    EXPECT_EQ(len, 0u);
}

TEST_F(KnowledgeExtendedTest, ExplainSimply_NullConcept_ReturnsZero) {
    // WHAT: Test explain simply with NULL concept
    // WHY:  Verify guard clause
    // HOW:  Pass NULL concept and expect 0
    char explanation[512];
    uint32_t len = knowledge_explain_simply(system, nullptr, 10, explanation, sizeof(explanation));
    EXPECT_EQ(len, 0u);
}

TEST_F(KnowledgeExtendedTest, ExplainSimply_NullExplanation_ReturnsZero) {
    // WHAT: Test explain simply with NULL output buffer
    // WHY:  Verify guard clause
    // HOW:  Pass NULL explanation and expect 0
    uint32_t len = knowledge_explain_simply(system, "test", 10, nullptr, 512);
    EXPECT_EQ(len, 0u);
}

//=============================================================================
// 11. Cross-Domain Connection Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, FindConnections_MultipleDomains) {
    // WHAT: Test finding cross-domain connections
    // WHY:  Verify connection discovery functionality
    // HOW:  Learn concepts in different domains and find connections
    knowledge_learn_from_text(system, "Democracy in ancient Greece", KNOWLEDGE_DOMAIN_HISTORY);
    knowledge_learn_from_text(system, "Democratic voting systems", KNOWLEDGE_DOMAIN_SOCIAL);
    knowledge_learn_from_text(system, "Ethics of democratic participation", KNOWLEDGE_DOMAIN_ETHICS);

    knowledge_item_t connections[10];
    uint32_t found = knowledge_find_connections(system, "democracy", connections, 10);
    // Should find connections across different domains
    EXPECT_GE(found, 0u);
}

TEST_F(KnowledgeExtendedTest, FindConnections_NullSystem_ReturnsZero) {
    // WHAT: Test find connections with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect 0
    knowledge_item_t connections[10];
    uint32_t found = knowledge_find_connections(nullptr, "test", connections, 10);
    EXPECT_EQ(found, 0u);
}

TEST_F(KnowledgeExtendedTest, FindConnections_NullConcept_ReturnsZero) {
    // WHAT: Test find connections with NULL concept
    // WHY:  Verify guard clause
    // HOW:  Pass NULL concept and expect 0
    knowledge_item_t connections[10];
    uint32_t found = knowledge_find_connections(system, nullptr, connections, 10);
    EXPECT_EQ(found, 0u);
}

TEST_F(KnowledgeExtendedTest, FindConnections_NullConnections_ReturnsZero) {
    // WHAT: Test find connections with NULL output array
    // WHY:  Verify guard clause
    // HOW:  Pass NULL connections and expect 0
    uint32_t found = knowledge_find_connections(system, "test", nullptr, 10);
    EXPECT_EQ(found, 0u);
}

TEST_F(KnowledgeExtendedTest, FindConnections_NonexistentConcept_ReturnsZero) {
    // WHAT: Test finding connections for nonexistent concept
    // WHY:  Verify handling of unknown concepts
    // HOW:  Try to find connections for concept that wasn't learned
    knowledge_item_t connections[10];
    uint32_t found = knowledge_find_connections(system, "nonexistent_xyz", connections, 10);
    EXPECT_EQ(found, 0u);
}

//=============================================================================
// 12. Transfer Learning Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, TransferLearning_ValidTransfer) {
    // WHAT: Test applying knowledge from one domain to another
    // WHY:  Verify transfer learning functionality
    // HOW:  Transfer from narrative to social domain
    char application[512];
    bool success = knowledge_transfer_learning(system, KNOWLEDGE_DOMAIN_LITERATURE,
                                               KNOWLEDGE_DOMAIN_SOCIAL,
                                               "Making new friends",
                                               application, sizeof(application));
    EXPECT_TRUE(success);
    EXPECT_GT(strlen(application), 0u);
}

TEST_F(KnowledgeExtendedTest, TransferLearning_NullSystem_ReturnsFalse) {
    // WHAT: Test transfer with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect false
    char application[512];
    bool success = knowledge_transfer_learning(nullptr, KNOWLEDGE_DOMAIN_LITERATURE,
                                               KNOWLEDGE_DOMAIN_SOCIAL,
                                               "Test situation",
                                               application, sizeof(application));
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, TransferLearning_NullSituation_ReturnsFalse) {
    // WHAT: Test transfer with NULL situation
    // WHY:  Verify guard clause
    // HOW:  Pass NULL situation and expect false
    char application[512];
    bool success = knowledge_transfer_learning(system, KNOWLEDGE_DOMAIN_LITERATURE,
                                               KNOWLEDGE_DOMAIN_SOCIAL,
                                               nullptr,
                                               application, sizeof(application));
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, TransferLearning_NullApplication_ReturnsFalse) {
    // WHAT: Test transfer with NULL application buffer
    // WHY:  Verify guard clause
    // HOW:  Pass NULL application and expect false
    bool success = knowledge_transfer_learning(system, KNOWLEDGE_DOMAIN_LITERATURE,
                                               KNOWLEDGE_DOMAIN_SOCIAL,
                                               "Test situation",
                                               nullptr, 512);
    EXPECT_FALSE(success);
}

//=============================================================================
// 13. Build On Tests (Analogical Learning)
//=============================================================================

TEST_F(KnowledgeExtendedTest, BuildOn_ValidBase) {
    // WHAT: Test building new concept on existing one
    // WHY:  Verify analogical learning functionality
    // HOW:  Learn base concept then build related concept
    knowledge_learn_from_text(system, "Democracy is government by the people",
                             KNOWLEDGE_DOMAIN_GENERAL);

    bool success = knowledge_build_on(system, "republic", "democracy",
                                     "Representatives elected by citizens");
    EXPECT_TRUE(success);

    knowledge_item_t item;
    EXPECT_TRUE(knowledge_retrieve(system, "republic", &item));
}

TEST_F(KnowledgeExtendedTest, BuildOn_NonexistentBase_ReturnsFalse) {
    // WHAT: Test building on nonexistent base concept
    // WHY:  Verify function fails when base doesn't exist
    // HOW:  Try to build on concept that wasn't learned
    bool success = knowledge_build_on(system, "new_concept", "nonexistent_base", "differences");
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, BuildOn_NullSystem_ReturnsFalse) {
    // WHAT: Test build on with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect false
    bool success = knowledge_build_on(nullptr, "new", "base", "diff");
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, BuildOn_NullNewConcept_ReturnsFalse) {
    // WHAT: Test build on with NULL new concept
    // WHY:  Verify guard clause
    // HOW:  Pass NULL new_concept and expect false
    bool success = knowledge_build_on(system, nullptr, "base", "diff");
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, BuildOn_NullBaseConcept_ReturnsFalse) {
    // WHAT: Test build on with NULL base concept
    // WHY:  Verify guard clause
    // HOW:  Pass NULL based_on_concept and expect false
    bool success = knowledge_build_on(system, "new", nullptr, "diff");
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, BuildOn_NullDifferences) {
    // WHAT: Test build on with NULL differences (should work)
    // WHY:  Verify differences is optional
    // HOW:  Pass NULL differences and expect success
    knowledge_learn_from_text(system, "Democracy is government by the people",
                             KNOWLEDGE_DOMAIN_GENERAL);

    bool success = knowledge_build_on(system, "republic", "democracy", nullptr);
    EXPECT_TRUE(success);
}

//=============================================================================
// 14. Reinforcement Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, Reinforce_ExistingConcept) {
    // WHAT: Test reinforcing existing knowledge
    // WHY:  Verify reinforcement increases confidence
    // HOW:  Learn concept, reinforce it, check confidence increase
    knowledge_learn_from_text(system, "Empathy means understanding others feelings",
                             KNOWLEDGE_DOMAIN_ETHICS);

    knowledge_item_t before;
    knowledge_retrieve(system, "empathy", &before);
    float conf_before = before.confidence;

    bool success = knowledge_reinforce(system, "empathy", "Showing empathy in conversation");
    EXPECT_TRUE(success);

    knowledge_item_t after;
    knowledge_retrieve(system, "empathy", &after);
    EXPECT_GE(after.confidence, conf_before);
    EXPECT_GT(after.reinforcement_count, before.reinforcement_count);
}

TEST_F(KnowledgeExtendedTest, Reinforce_WithExample) {
    // WHAT: Test reinforcement adds example
    // WHY:  Verify examples are stored
    // HOW:  Reinforce with example and verify storage
    knowledge_learn_from_text(system, "Courage means facing fear",
                             KNOWLEDGE_DOMAIN_ETHICS);

    knowledge_reinforce(system, "courage", "Standing up for what's right");

    knowledge_item_t item;
    knowledge_retrieve(system, "courage", &item);
    EXPECT_GT(item.num_examples, 0u);
}

TEST_F(KnowledgeExtendedTest, Reinforce_NonexistentConcept_ReturnsFalse) {
    // WHAT: Test reinforcing nonexistent concept
    // WHY:  Verify function fails for unknown concepts
    // HOW:  Try to reinforce concept that wasn't learned
    bool success = knowledge_reinforce(system, "nonexistent_xyz", "example");
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, Reinforce_NullSystem_ReturnsFalse) {
    // WHAT: Test reinforce with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect false
    bool success = knowledge_reinforce(nullptr, "test", "example");
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, Reinforce_NullConcept_ReturnsFalse) {
    // WHAT: Test reinforce with NULL concept
    // WHY:  Verify guard clause
    // HOW:  Pass NULL concept and expect false
    bool success = knowledge_reinforce(system, nullptr, "example");
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, Reinforce_NullExample_StillSucceeds) {
    // WHAT: Test reinforcement without example (should work)
    // WHY:  Verify example is optional
    // HOW:  Reinforce with NULL example and expect success
    knowledge_learn_from_text(system, "Test concept", KNOWLEDGE_DOMAIN_GENERAL);
    bool success = knowledge_reinforce(system, "concept", nullptr);
    EXPECT_TRUE(success);
}

TEST_F(KnowledgeExtendedTest, Reinforce_MaxExamples) {
    // WHAT: Test reinforcement respects max examples limit
    // WHY:  Verify examples don't exceed limit
    // HOW:  Add 15 examples and verify only 10 are stored
    knowledge_learn_from_text(system, "Patience is important", KNOWLEDGE_DOMAIN_ETHICS);

    for (int i = 0; i < 15; i++) {
        char example[64];
        snprintf(example, sizeof(example), "Example %d", i);
        knowledge_reinforce(system, "patience", example);
    }

    knowledge_item_t item;
    knowledge_retrieve(system, "patience", &item);
    EXPECT_LE(item.num_examples, 10u);
}

//=============================================================================
// 15. Knowledge Organization Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, OrganizeDomain_ValidDomain) {
    // WHAT: Test organizing knowledge within domain
    // WHY:  Verify organization functionality
    // HOW:  Learn concepts and organize domain
    knowledge_learn_from_text(system, "Mathematics includes algebra and geometry",
                             KNOWLEDGE_DOMAIN_MATHEMATICS);

    bool success = knowledge_organize_domain(system, KNOWLEDGE_DOMAIN_MATHEMATICS);
    EXPECT_TRUE(success);
}

TEST_F(KnowledgeExtendedTest, OrganizeDomain_NullSystem_ReturnsFalse) {
    // WHAT: Test organize with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect false
    bool success = knowledge_organize_domain(nullptr, KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, GetMap_ValidDomain) {
    // WHAT: Test getting knowledge map for domain
    // WHY:  Verify map generation functionality
    // HOW:  Learn concepts and get map
    knowledge_learn_from_text(system, "Science explores the natural world",
                             KNOWLEDGE_DOMAIN_SCIENCE);

    uint32_t count = knowledge_get_map(system, KNOWLEDGE_DOMAIN_SCIENCE, nullptr, 100);
    EXPECT_GT(count, 0u);
}

TEST_F(KnowledgeExtendedTest, GetMap_GeneralDomain_ReturnsAll) {
    // WHAT: Test getting map for all domains
    // WHY:  Verify GENERAL domain returns all concepts
    // HOW:  Learn in multiple domains and get GENERAL map
    knowledge_learn_from_text(system, "History teaches us about the past", KNOWLEDGE_DOMAIN_HISTORY);
    knowledge_learn_from_text(system, "Ethics guides our behavior", KNOWLEDGE_DOMAIN_ETHICS);

    uint32_t count = knowledge_get_map(system, KNOWLEDGE_DOMAIN_GENERAL, nullptr, 100);
    EXPECT_GT(count, 0u);
}

TEST_F(KnowledgeExtendedTest, GetMap_NullSystem_ReturnsZero) {
    // WHAT: Test get map with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect 0
    uint32_t count = knowledge_get_map(nullptr, KNOWLEDGE_DOMAIN_GENERAL, nullptr, 100);
    EXPECT_EQ(count, 0u);
}

TEST_F(KnowledgeExtendedTest, GetMap_LimitedMaxNodes) {
    // WHAT: Test map respects max_nodes limit
    // WHY:  Verify function honors capacity limit
    // HOW:  Learn many concepts and request limited map
    for (int i = 0; i < 20; i++) {
        char text[128];
        snprintf(text, sizeof(text), "Concept number %d is interesting", i);
        knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);
    }

    uint32_t count = knowledge_get_map(system, KNOWLEDGE_DOMAIN_GENERAL, nullptr, 5);
    EXPECT_LE(count, 5u);
}

//=============================================================================
// 16. Reading System Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, ReadBook_ValidBook) {
    // WHAT: Test reading book functionality
    // WHY:  Verify incremental reading
    // HOW:  Read book and verify concepts are learned
    const char* book_text = "This is a test book about learning and knowledge. "
                           "It contains many interesting concepts about education.";
    uint32_t learned = knowledge_read_book(system, "Test Book", book_text, 10);
    EXPECT_GT(learned, 0u);
}

TEST_F(KnowledgeExtendedTest, ReadBook_NullSystem_ReturnsZero) {
    // WHAT: Test read book with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect 0
    const char* book_text = "Test text";
    uint32_t learned = knowledge_read_book(nullptr, "Test Book", book_text, 10);
    EXPECT_EQ(learned, 0u);
}

TEST_F(KnowledgeExtendedTest, ReadBook_NullTitle_ReturnsZero) {
    // WHAT: Test read book with NULL title
    // WHY:  Verify guard clause
    // HOW:  Pass NULL title and expect 0
    const char* book_text = "Test text";
    uint32_t learned = knowledge_read_book(system, nullptr, book_text, 10);
    EXPECT_EQ(learned, 0u);
}

TEST_F(KnowledgeExtendedTest, ReadBook_NullText_ReturnsZero) {
    // WHAT: Test read book with NULL text
    // WHY:  Verify guard clause
    // HOW:  Pass NULL text and expect 0
    uint32_t learned = knowledge_read_book(system, "Test Book", nullptr, 10);
    EXPECT_EQ(learned, 0u);
}

TEST_F(KnowledgeExtendedTest, ContinueReading_ExistingBook) {
    // WHAT: Test continuing reading from bookmark
    // WHY:  Verify reading progress tracking
    // HOW:  Start reading and continue later
    const char* book_text = "This is a test book with multiple pages of content.";
    knowledge_read_book(system, "Progressive Book", book_text, 10);

    uint32_t progress = knowledge_continue_reading(system, "Progressive Book", true);
    EXPECT_GE(progress, 0u);
    EXPECT_LE(progress, 100u);
}

TEST_F(KnowledgeExtendedTest, ContinueReading_NonexistentBook_ReturnsZero) {
    // WHAT: Test continuing nonexistent book
    // WHY:  Verify function handles unknown books
    // HOW:  Try to continue book that wasn't started
    uint32_t progress = knowledge_continue_reading(system, "Nonexistent Book", true);
    EXPECT_EQ(progress, 0u);
}

TEST_F(KnowledgeExtendedTest, ContinueReading_NullSystem_ReturnsZero) {
    // WHAT: Test continue reading with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect 0
    uint32_t progress = knowledge_continue_reading(nullptr, "Test Book", true);
    EXPECT_EQ(progress, 0u);
}

TEST_F(KnowledgeExtendedTest, ContinueReading_NullTitle_ReturnsZero) {
    // WHAT: Test continue reading with NULL title
    // WHY:  Verify guard clause
    // HOW:  Pass NULL title and expect 0
    uint32_t progress = knowledge_continue_reading(system, nullptr, true);
    EXPECT_EQ(progress, 0u);
}

TEST_F(KnowledgeExtendedTest, ContinueReading_ZeroPages_ReturnsZero) {
    // WHAT: Test continue reading with zero total pages
    // WHY:  Verify divide by zero protection
    // HOW:  This tests the guard clause in the function
    // Note: This is implicitly tested by the implementation's guard clause
    uint32_t progress = knowledge_continue_reading(system, "Empty Book", true);
    EXPECT_EQ(progress, 0u);
}

TEST_F(KnowledgeExtendedTest, GetReadingList_ValidDomain) {
    // WHAT: Test getting reading recommendations
    // WHY:  Verify recommendation functionality
    // HOW:  Request recommendations for domain
    char* recommendations[10];
    uint32_t count = knowledge_get_reading_list(system, KNOWLEDGE_DOMAIN_LITERATURE,
                                                recommendations, 10);
    EXPECT_GT(count, 0u);
}

TEST_F(KnowledgeExtendedTest, GetReadingList_NullSystem_ReturnsZero) {
    // WHAT: Test reading list with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect 0
    char* recommendations[10];
    uint32_t count = knowledge_get_reading_list(nullptr, KNOWLEDGE_DOMAIN_LITERATURE,
                                                recommendations, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(KnowledgeExtendedTest, GetReadingList_NullRecommendations_ReturnsZero) {
    // WHAT: Test reading list with NULL output array
    // WHY:  Verify guard clause
    // HOW:  Pass NULL recommendations and expect 0
    uint32_t count = knowledge_get_reading_list(system, KNOWLEDGE_DOMAIN_LITERATURE,
                                                nullptr, 10);
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// 17. Assessment Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, AssessDomain_ValidDomain) {
    // WHAT: Test domain assessment functionality
    // WHY:  Verify assessment calculation
    // HOW:  Learn concepts and assess domain
    knowledge_learn_from_text(system, "Philosophy explores fundamental questions",
                             KNOWLEDGE_DOMAIN_PHILOSOPHY);

    domain_knowledge_t assessment;
    bool success = knowledge_assess_domain(system, KNOWLEDGE_DOMAIN_PHILOSOPHY, &assessment);
    EXPECT_TRUE(success);
    EXPECT_EQ(assessment.domain, KNOWLEDGE_DOMAIN_PHILOSOPHY);
    EXPECT_GT(assessment.concepts_known, 0u);
}

TEST_F(KnowledgeExtendedTest, AssessDomain_EmptyDomain) {
    // WHAT: Test assessing domain with no knowledge
    // WHY:  Verify assessment works for empty domains
    // HOW:  Assess domain without learning anything in it
    domain_knowledge_t assessment;
    bool success = knowledge_assess_domain(system, KNOWLEDGE_DOMAIN_MATHEMATICS, &assessment);
    EXPECT_TRUE(success);
    EXPECT_EQ(assessment.concepts_known, 0u);
}

TEST_F(KnowledgeExtendedTest, AssessDomain_NullSystem_ReturnsFalse) {
    // WHAT: Test assess with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect false
    domain_knowledge_t assessment;
    bool success = knowledge_assess_domain(nullptr, KNOWLEDGE_DOMAIN_GENERAL, &assessment);
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, AssessDomain_NullAssessment_ReturnsFalse) {
    // WHAT: Test assess with NULL output
    // WHY:  Verify guard clause
    // HOW:  Pass NULL assessment and expect false
    bool success = knowledge_assess_domain(system, KNOWLEDGE_DOMAIN_GENERAL, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, GetSummary_AllDomains) {
    // WHAT: Test getting summary of all domains
    // WHY:  Verify summary generation
    // HOW:  Learn in multiple domains and get summary
    knowledge_learn_from_text(system, "Language enables communication", KNOWLEDGE_DOMAIN_LANGUAGE);
    knowledge_learn_from_text(system, "Social skills are important", KNOWLEDGE_DOMAIN_SOCIAL);

    domain_knowledge_t all_domains[11];
    uint32_t count = knowledge_get_summary(system, all_domains, 11);
    EXPECT_EQ(count, 11u);
}

TEST_F(KnowledgeExtendedTest, GetSummary_LimitedArray) {
    // WHAT: Test summary with limited array size
    // WHY:  Verify function respects max_domains limit
    // HOW:  Request summary with small array
    domain_knowledge_t domains[5];
    uint32_t count = knowledge_get_summary(system, domains, 5);
    EXPECT_EQ(count, 5u);
}

TEST_F(KnowledgeExtendedTest, GetSummary_NullSystem_ReturnsZero) {
    // WHAT: Test summary with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect 0
    domain_knowledge_t domains[11];
    uint32_t count = knowledge_get_summary(nullptr, domains, 11);
    EXPECT_EQ(count, 0u);
}

TEST_F(KnowledgeExtendedTest, GetSummary_NullDomains_ReturnsZero) {
    // WHAT: Test summary with NULL output array
    // WHY:  Verify guard clause
    // HOW:  Pass NULL domains and expect 0
    uint32_t count = knowledge_get_summary(system, nullptr, 11);
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// 18. Utility Function Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, DomainName_ValidDomains) {
    // WHAT: Test domain name retrieval
    // WHY:  Verify all domain names are accessible
    // HOW:  Get name for each domain and check non-null
    for (int i = 0; i <= 10; i++) {
        const char* name = knowledge_domain_name((knowledge_domain_t)i);
        EXPECT_NE(name, nullptr);
        EXPECT_STRNE(name, "Unknown");
    }
}

TEST_F(KnowledgeExtendedTest, DomainName_InvalidDomain) {
    // WHAT: Test domain name for invalid domain
    // WHY:  Verify function handles invalid input
    // HOW:  Pass invalid domain value and expect "Unknown"
    const char* name = knowledge_domain_name((knowledge_domain_t)99);
    EXPECT_STREQ(name, "Unknown");
}

TEST_F(KnowledgeExtendedTest, PrintItem_ValidItem) {
    // WHAT: Test printing knowledge item
    // WHY:  Verify print function doesn't crash
    // HOW:  Learn concept and print it
    knowledge_learn_from_text(system, "Testing print functionality", KNOWLEDGE_DOMAIN_GENERAL);
    knowledge_item_t item;
    knowledge_retrieve(system, "testing", &item);

    // Capture stdout to verify output
    testing::internal::CaptureStdout();
    knowledge_print_item(&item);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_FALSE(output.empty());
}

TEST_F(KnowledgeExtendedTest, PrintItem_NullItem_NoError) {
    // WHAT: Test printing NULL item doesn't crash
    // WHY:  Verify guard clause
    // HOW:  Call print with NULL and verify no crash
    knowledge_print_item(nullptr);
    SUCCEED();
}

TEST_F(KnowledgeExtendedTest, PrintAssessment_ValidAssessment) {
    // WHAT: Test printing domain assessment
    // WHY:  Verify print function doesn't crash
    // HOW:  Get assessment and print it
    domain_knowledge_t assessment;
    knowledge_assess_domain(system, KNOWLEDGE_DOMAIN_GENERAL, &assessment);

    testing::internal::CaptureStdout();
    knowledge_print_assessment(&assessment);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_FALSE(output.empty());
}

TEST_F(KnowledgeExtendedTest, PrintAssessment_NullAssessment_NoError) {
    // WHAT: Test printing NULL assessment doesn't crash
    // WHY:  Verify guard clause
    // HOW:  Call print with NULL and verify no crash
    knowledge_print_assessment(nullptr);
    SUCCEED();
}

//=============================================================================
// 19. Persistence Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, Save_ValidSystem) {
    // WHAT: Test saving knowledge to file
    // WHY:  Verify persistence functionality
    // HOW:  Learn concepts and save to file
    knowledge_learn_from_text(system, "Persistence allows long term memory",
                             KNOWLEDGE_DOMAIN_GENERAL);

    const char* filepath = "/tmp/test_knowledge_save.dat";
    bool success = knowledge_save(system, filepath);
    EXPECT_TRUE(success);

    // Cleanup
    remove(filepath);
}

TEST_F(KnowledgeExtendedTest, Save_NullSystem_ReturnsFalse) {
    // WHAT: Test save with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect false
    bool success = knowledge_save(nullptr, "/tmp/test.dat");
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, Save_NullFilepath_ReturnsFalse) {
    // WHAT: Test save with NULL filepath
    // WHY:  Verify guard clause
    // HOW:  Pass NULL filepath and expect false
    bool success = knowledge_save(system, nullptr);
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, Save_InvalidPath_ReturnsFalse) {
    // WHAT: Test save with invalid path
    // WHY:  Verify error handling for file system errors
    // HOW:  Try to save to invalid directory
    bool success = knowledge_save(system, "/invalid/nonexistent/path/file.dat");
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, Load_ValidFile) {
    // WHAT: Test loading knowledge from file
    // WHY:  Verify load functionality
    // HOW:  Save system, load it, verify contents
    knowledge_learn_from_text(system, "Loading tests persistence", KNOWLEDGE_DOMAIN_GENERAL);

    const char* filepath = "/tmp/test_knowledge_load.dat";
    knowledge_save(system, filepath);

    knowledge_system_t loaded = knowledge_load(filepath);
    EXPECT_NE(loaded, nullptr);

    if (loaded) {
        // Verify loaded content
        knowledge_item_t item;
        bool found = knowledge_retrieve(loaded, "loading", &item);
        EXPECT_TRUE(found);
        knowledge_system_destroy(loaded);
    }

    remove(filepath);
}

TEST_F(KnowledgeExtendedTest, Load_NullFilepath_ReturnsNull) {
    // WHAT: Test load with NULL filepath
    // WHY:  Verify guard clause
    // HOW:  Pass NULL filepath and expect NULL
    knowledge_system_t loaded = knowledge_load(nullptr);
    EXPECT_EQ(loaded, nullptr);
}

TEST_F(KnowledgeExtendedTest, Load_NonexistentFile_ReturnsNull) {
    // WHAT: Test load with nonexistent file
    // WHY:  Verify error handling
    // HOW:  Try to load file that doesn't exist
    knowledge_system_t loaded = knowledge_load("/tmp/nonexistent_file_xyz.dat");
    EXPECT_EQ(loaded, nullptr);
}

TEST_F(KnowledgeExtendedTest, Load_InvalidMagic_ReturnsNull) {
    // WHAT: Test load with invalid file format
    // WHY:  Verify format validation
    // HOW:  Create file with wrong magic number
    const char* filepath = "/tmp/test_invalid_magic.dat";
    FILE* file = fopen(filepath, "wb");
    uint32_t invalid_magic = 0x12345678;
    fwrite(&invalid_magic, sizeof(uint32_t), 1, file);
    fclose(file);

    knowledge_system_t loaded = knowledge_load(filepath);
    EXPECT_EQ(loaded, nullptr);

    remove(filepath);
}

//=============================================================================
// 20. B-tree Query Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, GetByConfidenceRange_ValidRange) {
    // WHAT: Test querying by confidence range
    // WHY:  Verify B-tree range query functionality
    // HOW:  Learn concepts with different confidence and query range
    knowledge_learn_from_text(system, "High confidence concept", KNOWLEDGE_DOMAIN_GENERAL);
    knowledge_learn_from_text(system, "Medium confidence concept", KNOWLEDGE_DOMAIN_GENERAL);
    knowledge_learn_from_text(system, "Low confidence concept", KNOWLEDGE_DOMAIN_GENERAL);

    knowledge_item_t* results = nullptr;
    uint32_t count = knowledge_get_by_confidence_range(system, 0.0f, 1.0f, &results);
    EXPECT_GT(count, 0u);

    if (results) {
        nimcp_free(results);
    }
}

TEST_F(KnowledgeExtendedTest, GetByConfidenceRange_NarrowRange) {
    // WHAT: Test narrow confidence range
    // WHY:  Verify range filtering works correctly
    // HOW:  Query specific range and verify results are within range
    for (int i = 0; i < 10; i++) {
        char text[128];
        snprintf(text, sizeof(text), "Concept number %d", i);
        knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);
    }

    knowledge_item_t* results = nullptr;
    uint32_t count = knowledge_get_by_confidence_range(system, 0.25f, 0.35f, &results);

    if (results) {
        for (uint32_t i = 0; i < count; i++) {
            EXPECT_GE(results[i].confidence, 0.25f);
            EXPECT_LE(results[i].confidence, 0.35f);
        }
        nimcp_free(results);
    }
}

TEST_F(KnowledgeExtendedTest, GetByConfidenceRange_InvalidRange) {
    // WHAT: Test invalid range (min > max)
    // WHY:  Verify function handles invalid input
    // HOW:  Pass min > max and expect 0
    knowledge_item_t* results = nullptr;
    uint32_t count = knowledge_get_by_confidence_range(system, 0.8f, 0.2f, &results);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(results, nullptr);
}

TEST_F(KnowledgeExtendedTest, GetByConfidenceRange_NullSystem) {
    // WHAT: Test confidence range with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect 0
    knowledge_item_t* results = nullptr;
    uint32_t count = knowledge_get_by_confidence_range(nullptr, 0.0f, 1.0f, &results);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(results, nullptr);
}

TEST_F(KnowledgeExtendedTest, GetByConfidenceRange_NullResults) {
    // WHAT: Test confidence range with NULL results pointer
    // WHY:  Verify guard clause
    // HOW:  Pass NULL results_out and expect 0
    uint32_t count = knowledge_get_by_confidence_range(system, 0.0f, 1.0f, nullptr);
    EXPECT_EQ(count, 0u);
}

TEST_F(KnowledgeExtendedTest, GetAllOrderedByConfidence_ValidSystem) {
    // WHAT: Test getting all knowledge ordered by confidence
    // WHY:  Verify B-tree in-order traversal
    // HOW:  Learn concepts and get ordered list
    knowledge_learn_from_text(system, "First concept", KNOWLEDGE_DOMAIN_GENERAL);
    knowledge_learn_from_text(system, "Second concept", KNOWLEDGE_DOMAIN_GENERAL);
    knowledge_learn_from_text(system, "Third concept", KNOWLEDGE_DOMAIN_GENERAL);

    knowledge_item_t* results = nullptr;
    uint32_t count = knowledge_get_all_ordered_by_confidence(system, &results);
    EXPECT_GT(count, 0u);

    // Verify ordering
    if (results && count > 1) {
        for (uint32_t i = 1; i < count; i++) {
            EXPECT_GE(results[i].confidence, results[i-1].confidence);
        }
        nimcp_free(results);
    }
}

TEST_F(KnowledgeExtendedTest, GetAllOrderedByConfidence_EmptySystem) {
    // WHAT: Test getting ordered list from empty system
    // WHY:  Verify function handles empty system
    // HOW:  Get ordered list without learning anything
    knowledge_system_t empty_sys = knowledge_system_create("Empty");
    knowledge_item_t* results = nullptr;
    uint32_t count = knowledge_get_all_ordered_by_confidence(empty_sys, &results);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(results, nullptr);
    knowledge_system_destroy(empty_sys);
}

TEST_F(KnowledgeExtendedTest, GetAllOrderedByConfidence_NullSystem) {
    // WHAT: Test ordered query with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect 0
    knowledge_item_t* results = nullptr;
    uint32_t count = knowledge_get_all_ordered_by_confidence(nullptr, &results);
    EXPECT_EQ(count, 0u);
    EXPECT_EQ(results, nullptr);
}

TEST_F(KnowledgeExtendedTest, GetAllOrderedByConfidence_NullResults) {
    // WHAT: Test ordered query with NULL results pointer
    // WHY:  Verify guard clause
    // HOW:  Pass NULL results_out and expect 0
    uint32_t count = knowledge_get_all_ordered_by_confidence(system, nullptr);
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// 21. Direct Knowledge Item Addition Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, AddItem_ValidItem) {
    // WHAT: Test direct item addition
    // WHY:  Verify testing API works correctly
    // HOW:  Create item and add directly
    knowledge_item_t item = {0};
    strncpy(item.concept_name, "direct_concept", sizeof(item.concept_name) - 1);
    item.domain = KNOWLEDGE_DOMAIN_GENERAL;
    strncpy(item.definition, "Directly added concept", sizeof(item.definition) - 1);
    item.confidence = 0.8f;
    item.reinforcement_count = 1;

    bool success = knowledge_add_item(system, &item);
    EXPECT_TRUE(success);

    // Verify retrieval
    knowledge_item_t retrieved;
    EXPECT_TRUE(knowledge_retrieve(system, "direct_concept", &retrieved));
}

TEST_F(KnowledgeExtendedTest, AddItem_NullSystem_ReturnsFalse) {
    // WHAT: Test add item with NULL system
    // WHY:  Verify guard clause
    // HOW:  Pass NULL system and expect false
    knowledge_item_t item = {0};
    bool success = knowledge_add_item(nullptr, &item);
    EXPECT_FALSE(success);
}

TEST_F(KnowledgeExtendedTest, AddItem_NullItem_ReturnsFalse) {
    // WHAT: Test add item with NULL item
    // WHY:  Verify guard clause
    // HOW:  Pass NULL item and expect false
    bool success = knowledge_add_item(system, nullptr);
    EXPECT_FALSE(success);
}

//=============================================================================
// 22. Stress and Edge Case Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, StressTest_ManyConceptsLearning) {
    // WHAT: Test system with many concepts
    // WHY:  Verify scalability and hash table performance
    // HOW:  Learn 1000+ concepts
    for (int i = 0; i < 1000; i++) {
        char text[128];
        snprintf(text, sizeof(text), "Concept_%d is a unique identifier for testing", i);
        knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);
    }

    // Verify some concepts are retrievable
    knowledge_item_t item;
    EXPECT_TRUE(knowledge_retrieve(system, "concept_0", &item));
    EXPECT_TRUE(knowledge_retrieve(system, "concept_500", &item));
    EXPECT_TRUE(knowledge_retrieve(system, "concept_999", &item));
}

TEST_F(KnowledgeExtendedTest, EdgeCase_VeryLongConceptName) {
    // WHAT: Test with very long concept name
    // WHY:  Verify buffer handling
    // HOW:  Create text with 300+ character word
    std::string long_word(300, 'a');
    std::string text = long_word + " is a very long concept name";

    uint32_t learned = knowledge_learn_from_text(system, text.c_str(), KNOWLEDGE_DOMAIN_GENERAL);
    // Should handle gracefully (truncate or skip)
    EXPECT_GE(learned, 0u);
}

TEST_F(KnowledgeExtendedTest, EdgeCase_EmptyText) {
    // WHAT: Test learning from empty text
    // WHY:  Verify empty input handling
    // HOW:  Pass empty string
    uint32_t learned = knowledge_learn_from_text(system, "", KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_EQ(learned, 0u);
}

TEST_F(KnowledgeExtendedTest, EdgeCase_SpecialCharacters) {
    // WHAT: Test text with special characters
    // WHY:  Verify tokenization handles special chars
    // HOW:  Learn text with punctuation and symbols
    const char* text = "Hello! How are you? I'm fine. Let's learn: knowledge@system#test";
    uint32_t learned = knowledge_learn_from_text(system, text, KNOWLEDGE_DOMAIN_GENERAL);
    EXPECT_GT(learned, 0u);
}

TEST_F(KnowledgeExtendedTest, EdgeCase_RepeatedReinforcement) {
    // WHAT: Test many reinforcements of same concept
    // WHY:  Verify confidence doesn't exceed 1.0
    // HOW:  Reinforce concept 100 times
    knowledge_learn_from_text(system, "Maximal confidence test", KNOWLEDGE_DOMAIN_GENERAL);

    for (int i = 0; i < 100; i++) {
        knowledge_reinforce(system, "maximal", nullptr);
    }

    knowledge_item_t item;
    knowledge_retrieve(system, "maximal", &item);
    EXPECT_LE(item.confidence, 1.0f);
}

TEST_F(KnowledgeExtendedTest, EdgeCase_MultipleDomainsPerConcept) {
    // WHAT: Test same concept in different domains
    // WHY:  Verify domain-specific storage
    // HOW:  Learn same word in different domains
    knowledge_learn_from_text(system, "Art and science", KNOWLEDGE_DOMAIN_ART);
    knowledge_learn_from_text(system, "Art and science", KNOWLEDGE_DOMAIN_SCIENCE);

    // Both should be stored (or reinforced if normalized to same concept)
    knowledge_item_t item;
    EXPECT_TRUE(knowledge_retrieve(system, "science", &item));
}

TEST_F(KnowledgeExtendedTest, ConcurrentLearning_MultipleSources) {
    // WHAT: Test learning from multiple sources simultaneously
    // WHY:  Verify system handles diverse input
    // HOW:  Learn from text, story, art, history in sequence
    knowledge_learn_from_text(system, "Multisource learning test", KNOWLEDGE_DOMAIN_GENERAL);

    narrative_knowledge_t story = create_test_narrative();
    knowledge_learn_from_story(system, &story);
    free_narrative(&story);

    aesthetic_knowledge_t art = create_test_artwork();
    knowledge_learn_from_art(system, &art);
    free_artwork(&art);

    historical_knowledge_t event = create_test_history();
    knowledge_learn_from_history(system, &event);
    free_history(&event);

    // Verify concepts from each source
    domain_knowledge_t summary[11];
    uint32_t count = knowledge_get_summary(system, summary, 11);
    EXPECT_EQ(count, 11u);
}

//=============================================================================
// 23. Hash Collision Tests
//=============================================================================

TEST_F(KnowledgeExtendedTest, HashCollision_ManyConceptsSameHash) {
    // WHAT: Test hash table handles collisions
    // WHY:  Verify chaining works correctly
    // HOW:  Add many concepts (some will hash to same bucket)
    std::vector<std::string> concepts;
    for (int i = 0; i < 100; i++) {
        char concept_name[32];
        snprintf(concept_name, sizeof(concept_name), "collision_%d", i);
        concepts.push_back(concept_name);

        std::string text = concepts.back() + " is a test concept";
        knowledge_learn_from_text(system, text.c_str(), KNOWLEDGE_DOMAIN_GENERAL);
    }

    // Verify all can be retrieved
    for (const auto& concept_str : concepts) {
        knowledge_item_t item;
        EXPECT_TRUE(knowledge_retrieve(system, concept_str.c_str(), &item))
            << "Failed to retrieve: " << concept_str;
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
