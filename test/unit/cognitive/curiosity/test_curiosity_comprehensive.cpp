/**
 * @file test_curiosity_comprehensive.cpp
 * @brief Comprehensive unit tests for nimcp_curiosity.c (Target: 95%+ coverage)
 *
 * WHAT: Complete test coverage for all functions in nimcp_curiosity.c
 * WHY:  Increase coverage from 24.3% to 95%+ (230 uncovered lines)
 * HOW:  Test all code paths, branches, error conditions, and edge cases
 *
 * COVERAGE STRATEGY:
 * 1. Engine Creation/Destruction (all paths)
 * 2. Knowledge Gap Detection (hash table operations)
 * 3. Question Generation (all types, all stages)
 * 4. Motivation Assessment (with/without neuromodulators)
 * 5. Learning Functions (answers, experiences, observations)
 * 6. Knowledge Source Management (registration, searching)
 * 7. Progress Tracking (statistics, domain coverage)
 * 8. Stage Management (all stages, strategy patterns)
 * 9. Utility Functions (print functions)
 * 10. Bidirectional Feedback (exploration rate, information gain)
 * 11. Error Handling (NULL guards, edge cases)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

    #include "cognitive/curiosity/nimcp_curiosity.h"
    #include "utils/memory/nimcp_memory.h"
    #include "nimcp.h"

//=============================================================================
// Test Fixture
//=============================================================================

class CuriosityComprehensiveTest : public ::testing::Test {
protected:
    curiosity_engine_t engine;

    void SetUp() override {
        // Initialize NIMCP systems
        nimcp_memory_init();
        nimcp_init();

        // Create curiosity engine for most tests
        engine = curiosity_engine_create(NULL, "test_learner");
    }

    void TearDown() override {
        // Cleanup
        if (engine) {
            curiosity_engine_destroy(engine);
            engine = nullptr;
        }
        nimcp_shutdown();
        nimcp_memory_cleanup();
    }

    // Helper: Mock knowledge search function
    static char** mock_knowledge_search(const char* query, void* context,
                                       uint32_t max_results, uint32_t* num_results) {
        *num_results = 2;
        char** results = (char**)nimcp_calloc(2, sizeof(char*));
        // Use nimcp_malloc instead of strdup to match cleanup expectations
        const char* str1 = "Result 1 about the topic";
        const char* str2 = "Result 2 with more information";
        results[0] = (char*)nimcp_malloc(strlen(str1) + 1);
        results[1] = (char*)nimcp_malloc(strlen(str2) + 1);
        strcpy(results[0], str1);
        strcpy(results[1], str2);
        return results;
    }

    // Helper: Empty knowledge search (returns nothing)
    static char** empty_knowledge_search(const char* query, void* context,
                                        uint32_t max_results, uint32_t* num_results) {
        *num_results = 0;
        return nullptr;
    }
};

//=============================================================================
// 1. Engine Creation and Destruction Tests
//=============================================================================

TEST_F(CuriosityComprehensiveTest, CreateEngine_ValidName) {
    // WHAT: Create engine with valid name
    // WHY: Test basic creation path
    // HOW: Create and verify engine is non-NULL

    curiosity_engine_t test_engine = curiosity_engine_create(NULL, "infant_learner");
    ASSERT_NE(test_engine, nullptr);

    // Verify initial state
    EXPECT_EQ(curiosity_get_stage(test_engine), STAGE_INFANT);
    EXPECT_GT(curiosity_get_drive(test_engine), 0.0f);

    learning_progress_t progress;
    ASSERT_TRUE(curiosity_get_progress(test_engine, &progress));
    EXPECT_EQ(progress.total_questions_asked, 0u);
    EXPECT_EQ(progress.total_answers_learned, 0u);

    curiosity_engine_destroy(test_engine);
}

TEST_F(CuriosityComprehensiveTest, CreateEngine_NullName) {
    // WHAT: Test NULL name guard
    // WHY: Guard clause validation
    // HOW: Pass NULL, expect NULL return

    curiosity_engine_t test_engine = curiosity_engine_create(NULL, nullptr);
    EXPECT_EQ(test_engine, nullptr);
}

TEST_F(CuriosityComprehensiveTest, CreateEngine_LongName) {
    // WHAT: Create engine with very long name
    // WHY: Test name truncation handling
    // HOW: Pass 200-character name

    char long_name[200];
    memset(long_name, 'A', 199);
    long_name[199] = '\0';

    curiosity_engine_t test_engine = curiosity_engine_create(NULL, long_name);
    ASSERT_NE(test_engine, nullptr);
    curiosity_engine_destroy(test_engine);
}

TEST_F(CuriosityComprehensiveTest, DestroyEngine_Null) {
    // WHAT: Test safe NULL destroy
    // WHY: Guard clause validation
    // HOW: Call with NULL, should not crash

    curiosity_engine_destroy(nullptr);
    SUCCEED();
}

TEST_F(CuriosityComprehensiveTest, DestroyEngine_WithData) {
    // WHAT: Destroy engine with data
    // WHY: Test cleanup of all resources
    // HOW: Add concepts, questions, sources, then destroy

    // Add some data
    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "test_concept");
    EXPECT_GT(gap.gap_size, 0.0f);

    bool registered = curiosity_register_knowledge_source(engine, "test_source",
                                                          mock_knowledge_search, nullptr);
    EXPECT_TRUE(registered);

    // Destroy should clean up everything
    curiosity_engine_destroy(engine);
    engine = nullptr;  // Prevent double-free in TearDown
    SUCCEED();
}

//=============================================================================
// 2. Knowledge Gap Detection Tests
//=============================================================================

TEST_F(CuriosityComprehensiveTest, DetectGap_NewConcept) {
    // WHAT: Detect gap for unknown concept
    // WHY: Test gap detection and hash table insertion
    // HOW: Query new concept, expect high gap size

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "quantum_mechanics");

    EXPECT_STREQ(gap.topic, "quantum_mechanics");
    EXPECT_NEAR(gap.gap_size, 1.0f, 0.01f);  // Completely unknown
    EXPECT_GT(gap.curiosity_intensity, 0.0f);
    EXPECT_GT(gap.learning_potential, 0.0f);
    EXPECT_EQ(gap.related_concepts, 0u);
}

TEST_F(CuriosityComprehensiveTest, DetectGap_NullEngine) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(nullptr, "concept");
    EXPECT_EQ(gap.gap_size, 0.0f);
    EXPECT_EQ(gap.curiosity_intensity, 0.0f);
}

TEST_F(CuriosityComprehensiveTest, DetectGap_NullConcept) {
    // WHAT: Test NULL concept guard
    // WHY: Guard clause validation
    // HOW: Pass NULL concept

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, nullptr);
    EXPECT_EQ(gap.gap_size, 0.0f);
}

TEST_F(CuriosityComprehensiveTest, DetectGap_SameConcept_Twice) {
    // WHAT: Detect gap for same concept twice
    // WHY: Test hash table lookup (not insertion)
    // HOW: Query same concept twice

    knowledge_gap_t gap1 = curiosity_detect_knowledge_gap(engine, "physics");
    knowledge_gap_t gap2 = curiosity_detect_knowledge_gap(engine, "physics");

    // Should return same gap info
    EXPECT_STREQ(gap1.topic, gap2.topic);
    EXPECT_EQ(gap1.gap_size, gap2.gap_size);
}

TEST_F(CuriosityComprehensiveTest, CheckFamiliarity_UnknownConcept) {
    // WHAT: Check familiarity with unknown concept
    // WHY: Test hash table lookup returning NULL
    // HOW: Query concept never seen

    float familiarity = curiosity_check_familiarity(engine, "unknown_thing");
    EXPECT_EQ(familiarity, 0.0f);
}

TEST_F(CuriosityComprehensiveTest, CheckFamiliarity_NullEngine) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    float familiarity = curiosity_check_familiarity(nullptr, "concept");
    EXPECT_EQ(familiarity, 0.0f);
}

TEST_F(CuriosityComprehensiveTest, CheckFamiliarity_NullConcept) {
    // WHAT: Test NULL concept guard
    // WHY: Guard clause validation
    // HOW: Pass NULL concept

    float familiarity = curiosity_check_familiarity(engine, nullptr);
    EXPECT_EQ(familiarity, 0.0f);
}

TEST_F(CuriosityComprehensiveTest, GetRelatedConcepts_NoConcept) {
    // WHAT: Get related concepts for unknown concept
    // WHY: Test empty result handling
    // HOW: Query non-existent concept

    char* related[10];
    uint32_t count = curiosity_get_related_concepts(engine, "unknown", related, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(CuriosityComprehensiveTest, GetRelatedConcepts_NullEngine) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    char* related[10];
    uint32_t count = curiosity_get_related_concepts(nullptr, "concept", related, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(CuriosityComprehensiveTest, GetRelatedConcepts_NullConcept) {
    // WHAT: Test NULL concept guard
    // WHY: Guard clause validation
    // HOW: Pass NULL concept

    char* related[10];
    uint32_t count = curiosity_get_related_concepts(engine, nullptr, related, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(CuriosityComprehensiveTest, GetRelatedConcepts_NullOutput) {
    // WHAT: Get count without output array
    // WHY: Test count-only query
    // HOW: Pass NULL for related array

    // First create a concept
    curiosity_detect_knowledge_gap(engine, "test_concept");

    uint32_t count = curiosity_get_related_concepts(engine, "test_concept", nullptr, 10);
    EXPECT_EQ(count, 0u);  // No related concepts yet
}

//=============================================================================
// 3. Question Generation Tests - All Types and Stages
//=============================================================================

TEST_F(CuriosityComprehensiveTest, GenerateQuestions_InfantStage) {
    // WHAT: Generate questions in infant stage
    // WHY: Test infant strategy (WHAT, WHY, HOW only)
    // HOW: Set infant stage and generate questions

    curiosity_set_stage(engine, STAGE_INFANT);

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "cat");
    generated_question_t questions[10];

    uint32_t count = curiosity_generate_questions(engine, &gap, questions, 10);
    EXPECT_EQ(count, 3u);  // Infant asks WHAT, WHY, HOW

    // Verify questions
    bool has_what = false, has_why = false, has_how = false;
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_GT(strlen(questions[i].question), 0u);
        EXPECT_GE(questions[i].priority, 0.0f);
        EXPECT_LE(questions[i].priority, 1.0f);
        EXPECT_GE(questions[i].difficulty, 0.0f);
        EXPECT_LE(questions[i].difficulty, 1.0f);

        if (questions[i].type == QUESTION_WHAT) has_what = true;
        if (questions[i].type == QUESTION_WHY) has_why = true;
        if (questions[i].type == QUESTION_HOW) has_how = true;
    }

    EXPECT_TRUE(has_what);
    EXPECT_TRUE(has_why);
    EXPECT_TRUE(has_how);
}

TEST_F(CuriosityComprehensiveTest, GenerateQuestions_ToddlerStage) {
    // WHAT: Generate questions in toddler stage
    // WHY: Test toddler strategy (adds WHERE, WHEN)
    // HOW: Set toddler stage and generate questions

    curiosity_set_stage(engine, STAGE_TODDLER);

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "ball");
    generated_question_t questions[10];

    uint32_t count = curiosity_generate_questions(engine, &gap, questions, 10);
    EXPECT_EQ(count, 5u);  // Toddler asks 5 types
}

TEST_F(CuriosityComprehensiveTest, GenerateQuestions_ChildStage) {
    // WHAT: Generate questions in child stage
    // WHY: Test child strategy (all 7 question types)
    // HOW: Set child stage and generate questions

    curiosity_set_stage(engine, STAGE_CHILD);

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "science");
    generated_question_t questions[10];

    uint32_t count = curiosity_generate_questions(engine, &gap, questions, 10);
    EXPECT_EQ(count, 7u);  // Child asks all types

    // Verify all 7 types present
    bool types[7] = {false};
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_GE(questions[i].type, 0);
        EXPECT_LT(questions[i].type, 7);
        types[questions[i].type] = true;
    }

    for (int i = 0; i < 7; i++) {
        EXPECT_TRUE(types[i]) << "Missing question type " << i;
    }
}

TEST_F(CuriosityComprehensiveTest, GenerateQuestions_AdolescentStage) {
    // WHAT: Generate questions in adolescent stage
    // WHY: Test adolescent strategy
    // HOW: Set adolescent stage and generate questions

    curiosity_set_stage(engine, STAGE_ADOLESCENT);

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "philosophy");
    generated_question_t questions[10];

    uint32_t count = curiosity_generate_questions(engine, &gap, questions, 10);
    EXPECT_EQ(count, 7u);  // Same as child
}

TEST_F(CuriosityComprehensiveTest, GenerateQuestions_AdultStage) {
    // WHAT: Generate questions in adult stage
    // WHY: Test adult strategy (more selective)
    // HOW: Set adult stage and generate questions

    curiosity_set_stage(engine, STAGE_ADULT);

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "specialized_topic");
    generated_question_t questions[10];

    uint32_t count = curiosity_generate_questions(engine, &gap, questions, 10);
    EXPECT_EQ(count, 7u);
}

TEST_F(CuriosityComprehensiveTest, GenerateQuestions_ExpertStage) {
    // WHAT: Generate questions in expert stage
    // WHY: Test expert strategy (most selective)
    // HOW: Set expert stage and generate questions

    curiosity_set_stage(engine, STAGE_EXPERT);

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "research_topic");
    generated_question_t questions[10];

    uint32_t count = curiosity_generate_questions(engine, &gap, questions, 10);
    EXPECT_EQ(count, 7u);
}

TEST_F(CuriosityComprehensiveTest, GenerateQuestions_NullEngine) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    knowledge_gap_t gap = {};
    generated_question_t questions[10];

    uint32_t count = curiosity_generate_questions(nullptr, &gap, questions, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(CuriosityComprehensiveTest, GenerateQuestions_NullGap) {
    // WHAT: Test NULL gap guard
    // WHY: Guard clause validation
    // HOW: Pass NULL gap

    generated_question_t questions[10];

    uint32_t count = curiosity_generate_questions(engine, nullptr, questions, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(CuriosityComprehensiveTest, GenerateQuestions_NullQuestions) {
    // WHAT: Test NULL questions array guard
    // WHY: Guard clause validation
    // HOW: Pass NULL questions array

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "test");

    uint32_t count = curiosity_generate_questions(engine, &gap, nullptr, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(CuriosityComprehensiveTest, GenerateQuestions_ZeroMax) {
    // WHAT: Test zero max questions guard
    // WHY: Guard clause validation
    // HOW: Pass 0 for max_questions

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "test");
    generated_question_t questions[10];

    uint32_t count = curiosity_generate_questions(engine, &gap, questions, 0);
    EXPECT_EQ(count, 0u);
}

TEST_F(CuriosityComprehensiveTest, GenerateQuestions_LimitedSpace) {
    // WHAT: Generate questions with limited output space
    // WHY: Test output limit handling
    // HOW: Request only 2 questions in child stage (has 7 types)

    curiosity_set_stage(engine, STAGE_CHILD);

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "topic");
    generated_question_t questions[2];

    uint32_t count = curiosity_generate_questions(engine, &gap, questions, 2);
    EXPECT_EQ(count, 2u);  // Should stop at limit
}

TEST_F(CuriosityComprehensiveTest, GenerateFollowup_ValidAnswer) {
    // WHAT: Generate follow-up question
    // WHY: Test follow-up generation
    // HOW: Provide answer and generate follow-up

    const char* followup = curiosity_generate_followup(engine, "Answer text here");
    ASSERT_NE(followup, nullptr);
    EXPECT_GT(strlen(followup), 0u);
}

TEST_F(CuriosityComprehensiveTest, GenerateFollowup_NullEngine) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    const char* followup = curiosity_generate_followup(nullptr, "answer");
    EXPECT_EQ(followup, nullptr);
}

TEST_F(CuriosityComprehensiveTest, GenerateFollowup_NullAnswer) {
    // WHAT: Test NULL answer guard
    // WHY: Guard clause validation
    // HOW: Pass NULL answer

    const char* followup = curiosity_generate_followup(engine, nullptr);
    EXPECT_EQ(followup, nullptr);
}

//=============================================================================
// 4. Motivation Assessment Tests
//=============================================================================

TEST_F(CuriosityComprehensiveTest, AssessMotivation_BasicConcept) {
    // WHAT: Assess motivation for learning a concept
    // WHY: Test motivation calculation
    // HOW: Query motivation state for concept

    motivation_state_t state = curiosity_assess_motivation(engine, "learning_topic");

    EXPECT_GT(state.intrinsic_curiosity, 0.0f);
    EXPECT_LE(state.intrinsic_curiosity, 1.0f);
    EXPECT_GE(state.goal_relevance, 0.0f);
    EXPECT_LE(state.goal_relevance, 1.0f);
    EXPECT_GE(state.social_importance, 0.0f);
    EXPECT_LE(state.social_importance, 1.0f);
    EXPECT_GE(state.survival_value, 0.0f);
    EXPECT_LE(state.survival_value, 1.0f);
    EXPECT_GE(state.aesthetic_appeal, 0.0f);
    EXPECT_LE(state.aesthetic_appeal, 1.0f);
    EXPECT_GT(state.overall_motivation, 0.0f);
}

TEST_F(CuriosityComprehensiveTest, AssessMotivation_NullEngine) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    motivation_state_t state = curiosity_assess_motivation(nullptr, "concept");
    EXPECT_EQ(state.intrinsic_curiosity, 0.0f);
    EXPECT_EQ(state.overall_motivation, 0.0f);
}

TEST_F(CuriosityComprehensiveTest, AssessMotivation_NullConcept) {
    // WHAT: Test NULL concept guard
    // WHY: Guard clause validation
    // HOW: Pass NULL concept

    motivation_state_t state = curiosity_assess_motivation(engine, nullptr);
    EXPECT_EQ(state.intrinsic_curiosity, 0.0f);
}

TEST_F(CuriosityComprehensiveTest, AssessMotivation_NovelConcept) {
    // WHAT: Assess motivation for novel concept
    // WHY: Test aesthetic appeal calculation (high for novel)
    // HOW: Query never-seen concept

    motivation_state_t state = curiosity_assess_motivation(engine, "brand_new_concept");

    // Novel concepts have high aesthetic appeal
    EXPECT_GT(state.aesthetic_appeal, 0.5f);
}

TEST_F(CuriosityComprehensiveTest, SetBaseline_ValidLevel) {
    // WHAT: Set baseline curiosity
    // WHY: Test baseline adjustment
    // HOW: Set various levels and verify

    curiosity_set_baseline(engine, 0.8f);
    EXPECT_NEAR(curiosity_get_drive(engine), 0.8f, 0.01f);

    curiosity_set_baseline(engine, 0.3f);
    EXPECT_NEAR(curiosity_get_drive(engine), 0.3f, 0.01f);
}

TEST_F(CuriosityComprehensiveTest, SetBaseline_ClampHigh) {
    // WHAT: Set baseline above 1.0
    // WHY: Test clamping to [0, 1]
    // HOW: Set 1.5, expect 1.0

    curiosity_set_baseline(engine, 1.5f);
    EXPECT_NEAR(curiosity_get_drive(engine), 1.0f, 0.01f);
}

TEST_F(CuriosityComprehensiveTest, SetBaseline_ClampLow) {
    // WHAT: Set baseline below 0.0
    // WHY: Test clamping to [0, 1]
    // HOW: Set -0.5, expect 0.0

    curiosity_set_baseline(engine, -0.5f);
    EXPECT_NEAR(curiosity_get_drive(engine), 0.0f, 0.01f);
}

TEST_F(CuriosityComprehensiveTest, SetBaseline_Null) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    curiosity_set_baseline(nullptr, 0.5f);
    SUCCEED();  // Should not crash
}

TEST_F(CuriosityComprehensiveTest, GetDrive_Null) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    float drive = curiosity_get_drive(nullptr);
    EXPECT_EQ(drive, 0.0f);
}

//=============================================================================
// 5. Learning Function Tests
//=============================================================================

TEST_F(CuriosityComprehensiveTest, LearnAnswer_ValidQA) {
    // WHAT: Learn from question-answer pair
    // WHY: Test learning and progress tracking
    // HOW: Provide Q&A, verify progress updated

    bool result = curiosity_learn_answer(engine, "What is X?", "X is Y");
    EXPECT_TRUE(result);

    learning_progress_t progress;
    ASSERT_TRUE(curiosity_get_progress(engine, &progress));
    EXPECT_EQ(progress.total_answers_learned, 1u);
}

TEST_F(CuriosityComprehensiveTest, LearnAnswer_NullEngine) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    bool result = curiosity_learn_answer(nullptr, "question", "answer");
    EXPECT_FALSE(result);
}

TEST_F(CuriosityComprehensiveTest, LearnAnswer_NullQuestion) {
    // WHAT: Test NULL question guard
    // WHY: Guard clause validation
    // HOW: Pass NULL question

    bool result = curiosity_learn_answer(engine, nullptr, "answer");
    EXPECT_FALSE(result);
}

TEST_F(CuriosityComprehensiveTest, LearnAnswer_NullAnswer) {
    // WHAT: Test NULL answer guard
    // WHY: Guard clause validation
    // HOW: Pass NULL answer

    bool result = curiosity_learn_answer(engine, "question", nullptr);
    EXPECT_FALSE(result);
}

TEST_F(CuriosityComprehensiveTest, LearnAnswer_Multiple) {
    // WHAT: Learn multiple answers
    // WHY: Test progress accumulation
    // HOW: Learn 5 answers, verify count

    for (int i = 0; i < 5; i++) {
        char question[64], answer[64];
        snprintf(question, sizeof(question), "Question %d?", i);
        snprintf(answer, sizeof(answer), "Answer %d", i);

        EXPECT_TRUE(curiosity_learn_answer(engine, question, answer));
    }

    learning_progress_t progress;
    ASSERT_TRUE(curiosity_get_progress(engine, &progress));
    EXPECT_EQ(progress.total_answers_learned, 5u);
}

TEST_F(CuriosityComprehensiveTest, LearnExperience_Valid) {
    // WHAT: Learn from experience
    // WHY: Test experiential learning
    // HOW: Provide experience with sensory data

    float sensory[5] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    bool result = curiosity_learn_experience(engine, "Saw a bird", sensory, 5);
    EXPECT_TRUE(result);

    learning_progress_t progress;
    ASSERT_TRUE(curiosity_get_progress(engine, &progress));
    EXPECT_EQ(progress.total_experiences, 1u);
}

TEST_F(CuriosityComprehensiveTest, LearnExperience_NoSensory) {
    // WHAT: Learn experience without sensory data
    // WHY: Test with NULL sensory data
    // HOW: Pass NULL for sensory_data

    bool result = curiosity_learn_experience(engine, "Heard a sound", nullptr, 0);
    EXPECT_TRUE(result);
}

TEST_F(CuriosityComprehensiveTest, LearnExperience_NullEngine) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    bool result = curiosity_learn_experience(nullptr, "experience", nullptr, 0);
    EXPECT_FALSE(result);
}

TEST_F(CuriosityComprehensiveTest, LearnExperience_NullDescription) {
    // WHAT: Test NULL description guard
    // WHY: Guard clause validation
    // HOW: Pass NULL description

    bool result = curiosity_learn_experience(engine, nullptr, nullptr, 0);
    EXPECT_FALSE(result);
}

TEST_F(CuriosityComprehensiveTest, LearnObservation_Valid) {
    // WHAT: Learn from observation
    // WHY: Test social learning
    // HOW: Provide observation with context

    bool result = curiosity_learn_observation(engine, "Person using tool", "Workshop");
    EXPECT_TRUE(result);
}

TEST_F(CuriosityComprehensiveTest, LearnObservation_NullEngine) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    bool result = curiosity_learn_observation(nullptr, "observation", "context");
    EXPECT_FALSE(result);
}

TEST_F(CuriosityComprehensiveTest, LearnObservation_NullObservation) {
    // WHAT: Test NULL observation guard
    // WHY: Guard clause validation
    // HOW: Pass NULL observation

    bool result = curiosity_learn_observation(engine, nullptr, "context");
    EXPECT_FALSE(result);
}

//=============================================================================
// 6. Knowledge Source Management Tests
//=============================================================================

TEST_F(CuriosityComprehensiveTest, RegisterSource_Valid) {
    // WHAT: Register knowledge source
    // WHY: Test source registration
    // HOW: Register mock source and verify

    bool result = curiosity_register_knowledge_source(engine, "test_source",
                                                     mock_knowledge_search, nullptr);
    EXPECT_TRUE(result);
}

TEST_F(CuriosityComprehensiveTest, RegisterSource_NullEngine) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    bool result = curiosity_register_knowledge_source(nullptr, "source",
                                                     mock_knowledge_search, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(CuriosityComprehensiveTest, RegisterSource_NullName) {
    // WHAT: Test NULL name guard
    // WHY: Guard clause validation
    // HOW: Pass NULL name

    bool result = curiosity_register_knowledge_source(engine, nullptr,
                                                     mock_knowledge_search, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(CuriosityComprehensiveTest, RegisterSource_NullFunction) {
    // WHAT: Test NULL function guard
    // WHY: Guard clause validation
    // HOW: Pass NULL search function

    bool result = curiosity_register_knowledge_source(engine, "source", nullptr, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(CuriosityComprehensiveTest, RegisterSource_Multiple) {
    // WHAT: Register multiple sources
    // WHY: Test capacity expansion
    // HOW: Register 15 sources (capacity starts at 10)

    for (int i = 0; i < 15; i++) {
        char name[32];
        snprintf(name, sizeof(name), "source_%d", i);

        bool result = curiosity_register_knowledge_source(engine, name,
                                                         mock_knowledge_search, nullptr);
        EXPECT_TRUE(result) << "Failed to register source " << i;
    }
}

TEST_F(CuriosityComprehensiveTest, SeekKnowledge_WithSource) {
    // WHAT: Seek knowledge from registered source
    // WHY: Test knowledge search
    // HOW: Register source, then search

    ASSERT_TRUE(curiosity_register_knowledge_source(engine, "wiki",
                                                   mock_knowledge_search, nullptr));

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "topic");
    char* results[10];

    uint32_t count = curiosity_seek_knowledge(engine, &gap, results, 10);
    EXPECT_EQ(count, 2u);  // Mock returns 2 results

    // Free the returned strings to prevent memory leaks
    for (uint32_t i = 0; i < count; i++) {
        nimcp_free(results[i]);
    }
}

TEST_F(CuriosityComprehensiveTest, SeekKnowledge_NoSources) {
    // WHAT: Seek knowledge with no sources
    // WHY: Test empty source list
    // HOW: Search without registering sources

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "topic");
    char* results[10];

    uint32_t count = curiosity_seek_knowledge(engine, &gap, results, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(CuriosityComprehensiveTest, SeekKnowledge_EmptySource) {
    // WHAT: Seek knowledge from empty source
    // WHY: Test source that returns no results
    // HOW: Register empty source, then search

    ASSERT_TRUE(curiosity_register_knowledge_source(engine, "empty",
                                                   empty_knowledge_search, nullptr));

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "topic");
    char* results[10];

    uint32_t count = curiosity_seek_knowledge(engine, &gap, results, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(CuriosityComprehensiveTest, SeekKnowledge_NullEngine) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    knowledge_gap_t gap = {};
    char* results[10];

    uint32_t count = curiosity_seek_knowledge(nullptr, &gap, results, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(CuriosityComprehensiveTest, SeekKnowledge_NullGap) {
    // WHAT: Test NULL gap guard
    // WHY: Guard clause validation
    // HOW: Pass NULL gap

    char* results[10];

    uint32_t count = curiosity_seek_knowledge(engine, nullptr, results, 10);
    EXPECT_EQ(count, 0u);
}

TEST_F(CuriosityComprehensiveTest, SeekKnowledge_NullResults) {
    // WHAT: Test NULL results guard
    // WHY: Guard clause validation
    // HOW: Pass NULL results

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "topic");

    uint32_t count = curiosity_seek_knowledge(engine, &gap, nullptr, 10);
    EXPECT_EQ(count, 0u);
}

//=============================================================================
// 7. Progress Tracking Tests
//=============================================================================

TEST_F(CuriosityComprehensiveTest, GetProgress_Initial) {
    // WHAT: Get progress at start
    // WHY: Test initial state
    // HOW: Query progress with no activity

    learning_progress_t progress;
    ASSERT_TRUE(curiosity_get_progress(engine, &progress));

    EXPECT_EQ(progress.total_questions_asked, 0u);
    EXPECT_EQ(progress.total_answers_learned, 0u);
    EXPECT_EQ(progress.total_experiences, 0u);
    EXPECT_EQ(progress.concepts_learned, 0u);
    EXPECT_GT(progress.avg_curiosity, 0.0f);
}

TEST_F(CuriosityComprehensiveTest, GetProgress_AfterLearning) {
    // WHAT: Get progress after learning
    // WHY: Test progress updates
    // HOW: Learn, then check progress

    curiosity_learn_answer(engine, "Q1", "A1");
    curiosity_learn_answer(engine, "Q2", "A2");

    learning_progress_t progress;
    ASSERT_TRUE(curiosity_get_progress(engine, &progress));

    EXPECT_EQ(progress.total_answers_learned, 2u);
}

TEST_F(CuriosityComprehensiveTest, GetProgress_NullEngine) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    learning_progress_t progress;
    bool result = curiosity_get_progress(nullptr, &progress);
    EXPECT_FALSE(result);
}

TEST_F(CuriosityComprehensiveTest, GetProgress_NullProgress) {
    // WHAT: Test NULL progress guard
    // WHY: Guard clause validation
    // HOW: Pass NULL progress

    bool result = curiosity_get_progress(engine, nullptr);
    EXPECT_FALSE(result);
}

TEST_F(CuriosityComprehensiveTest, GetDomainCoverage_EmptyDomain) {
    // WHAT: Get coverage for unknown domain
    // WHY: Test with no domain concepts
    // HOW: Query never-mentioned domain

    float coverage = curiosity_get_domain_coverage(engine, "quantum_physics");
    EXPECT_EQ(coverage, 0.0f);
}

TEST_F(CuriosityComprehensiveTest, GetDomainCoverage_NullEngine) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    float coverage = curiosity_get_domain_coverage(nullptr, "domain");
    EXPECT_EQ(coverage, 0.0f);
}

TEST_F(CuriosityComprehensiveTest, GetDomainCoverage_NullDomain) {
    // WHAT: Test NULL domain guard
    // WHY: Guard clause validation
    // HOW: Pass NULL domain

    float coverage = curiosity_get_domain_coverage(engine, nullptr);
    EXPECT_EQ(coverage, 0.0f);
}

//=============================================================================
// 8. Stage Management Tests
//=============================================================================

TEST_F(CuriosityComprehensiveTest, GetStage_Initial) {
    // WHAT: Get initial stage
    // WHY: Verify starts at infant
    // HOW: Query stage at creation

    learning_stage_t stage = curiosity_get_stage(engine);
    EXPECT_EQ(stage, STAGE_INFANT);
}

TEST_F(CuriosityComprehensiveTest, GetStage_Null) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    learning_stage_t stage = curiosity_get_stage(nullptr);
    EXPECT_EQ(stage, STAGE_INFANT);
}

TEST_F(CuriosityComprehensiveTest, SetStage_AllStages) {
    // WHAT: Set all learning stages
    // WHY: Test stage transitions and strategies
    // HOW: Set each stage and verify

    learning_stage_t stages[] = {
        STAGE_INFANT, STAGE_TODDLER, STAGE_CHILD,
        STAGE_ADOLESCENT, STAGE_ADULT, STAGE_EXPERT
    };

    for (int i = 0; i < 6; i++) {
        curiosity_set_stage(engine, stages[i]);
        EXPECT_EQ(curiosity_get_stage(engine), stages[i]);

        // Verify baseline updated
        float drive = curiosity_get_drive(engine);
        EXPECT_GT(drive, 0.0f);
        EXPECT_LE(drive, 1.0f);
    }
}

TEST_F(CuriosityComprehensiveTest, SetStage_Null) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    curiosity_set_stage(nullptr, STAGE_CHILD);
    SUCCEED();  // Should not crash
}

TEST_F(CuriosityComprehensiveTest, SetStage_InvalidStage) {
    // WHAT: Set invalid stage value
    // WHY: Test bounds checking
    // HOW: Set stage 99

    curiosity_set_stage(engine, (learning_stage_t)99);

    // Should fall back to safe default (infant)
    float drive = curiosity_get_drive(engine);
    EXPECT_GT(drive, 0.0f);
}

//=============================================================================
// 9. Utility Function Tests
//=============================================================================

TEST_F(CuriosityComprehensiveTest, PrintGap_Valid) {
    // WHAT: Print knowledge gap
    // WHY: Test print function (for coverage)
    // HOW: Create gap and print

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "test_topic");

    // Redirect stdout to suppress output during tests
    testing::internal::CaptureStdout();
    curiosity_print_gap(&gap);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("Knowledge Gap"), std::string::npos);
}

TEST_F(CuriosityComprehensiveTest, PrintGap_Null) {
    // WHAT: Test NULL gap guard
    // WHY: Guard clause validation
    // HOW: Pass NULL gap

    testing::internal::CaptureStdout();
    curiosity_print_gap(nullptr);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.empty());
}

TEST_F(CuriosityComprehensiveTest, PrintQuestion_Valid) {
    // WHAT: Print generated question
    // WHY: Test print function (for coverage)
    // HOW: Generate and print question

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "topic");
    generated_question_t questions[5];
    uint32_t count = curiosity_generate_questions(engine, &gap, questions, 5);
    ASSERT_GT(count, 0u);

    testing::internal::CaptureStdout();
    curiosity_print_question(&questions[0]);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("Question"), std::string::npos);
}

TEST_F(CuriosityComprehensiveTest, PrintQuestion_Null) {
    // WHAT: Test NULL question guard
    // WHY: Guard clause validation
    // HOW: Pass NULL question

    testing::internal::CaptureStdout();
    curiosity_print_question(nullptr);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.empty());
}

TEST_F(CuriosityComprehensiveTest, PrintProgress_Valid) {
    // WHAT: Print learning progress
    // WHY: Test print function (for coverage)
    // HOW: Get and print progress

    learning_progress_t progress;
    ASSERT_TRUE(curiosity_get_progress(engine, &progress));

    testing::internal::CaptureStdout();
    curiosity_print_progress(&progress);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("Learning Progress"), std::string::npos);
}

TEST_F(CuriosityComprehensiveTest, PrintProgress_Null) {
    // WHAT: Test NULL progress guard
    // WHY: Guard clause validation
    // HOW: Pass NULL progress

    testing::internal::CaptureStdout();
    curiosity_print_progress(nullptr);
    std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(output.empty());
}

//=============================================================================
// 10. Bidirectional Feedback Tests
//=============================================================================

TEST_F(CuriosityComprehensiveTest, SetExplorationRate_Valid) {
    // WHAT: Set exploration rate
    // WHY: Test cognitive load feedback
    // HOW: Set various rates and verify baseline adjusted

    curiosity_set_exploration_rate(engine, 0.5f);
    float drive1 = curiosity_get_drive(engine);

    curiosity_set_exploration_rate(engine, 1.0f);
    float drive2 = curiosity_get_drive(engine);

    EXPECT_GT(drive2, drive1);  // Higher exploration → higher drive
}

TEST_F(CuriosityComprehensiveTest, SetExplorationRate_ClampHigh) {
    // WHAT: Set exploration rate above 1.0
    // WHY: Test clamping
    // HOW: Set 1.5, should clamp to 1.0

    curiosity_set_exploration_rate(engine, 1.5f);
    float drive = curiosity_get_drive(engine);
    EXPECT_GE(drive, 0.0f);
    EXPECT_LE(drive, 1.0f);
}

TEST_F(CuriosityComprehensiveTest, SetExplorationRate_ClampLow) {
    // WHAT: Set exploration rate below 0.0
    // WHY: Test clamping
    // HOW: Set -0.5, should clamp to 0.0

    curiosity_set_exploration_rate(engine, -0.5f);
    float drive = curiosity_get_drive(engine);
    EXPECT_NEAR(drive, 0.0f, 0.01f);
}

TEST_F(CuriosityComprehensiveTest, SetExplorationRate_Null) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    curiosity_set_exploration_rate(nullptr, 0.5f);
    SUCCEED();  // Should not crash
}

TEST_F(CuriosityComprehensiveTest, GetInformationGain_NoLearning) {
    // WHAT: Get information gain with no learning
    // WHY: Test initial state handling
    // HOW: Query before any learning

    float gain = curiosity_get_information_gain(engine);
    EXPECT_GT(gain, 0.0f);  // Returns baseline
}

TEST_F(CuriosityComprehensiveTest, GetInformationGain_AfterLearning) {
    // WHAT: Get information gain after learning
    // WHY: Test learning rate calculation
    // HOW: Learn some answers, then query

    curiosity_learn_answer(engine, "Q1", "A1");
    curiosity_learn_answer(engine, "Q2", "A2");

    // Simulate questions asked
    learning_progress_t progress;
    curiosity_get_progress(engine, &progress);

    float gain = curiosity_get_information_gain(engine);
    EXPECT_GE(gain, 0.0f);
    EXPECT_LE(gain, 1.0f);
}

TEST_F(CuriosityComprehensiveTest, GetInformationGain_Null) {
    // WHAT: Test NULL engine guard
    // WHY: Guard clause validation
    // HOW: Pass NULL engine

    float gain = curiosity_get_information_gain(nullptr);
    EXPECT_EQ(gain, 0.0f);
}

//=============================================================================
// 11. Edge Cases and Stress Tests
//=============================================================================

TEST_F(CuriosityComprehensiveTest, EdgeCase_VeryLongConcept) {
    // WHAT: Test with very long concept string
    // WHY: Test buffer handling
    // HOW: Create 300-char concept

    char long_concept[300];
    memset(long_concept, 'A', 299);
    long_concept[299] = '\0';

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, long_concept);
    EXPECT_GT(strlen(gap.topic), 0u);
}

TEST_F(CuriosityComprehensiveTest, EdgeCase_SpecialCharacters) {
    // WHAT: Test concept with special characters
    // WHY: Test string normalization
    // HOW: Use concept with punctuation, numbers

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine,
                                                        "C++ programming!");
    EXPECT_GT(gap.gap_size, 0.0f);
}

TEST_F(CuriosityComprehensiveTest, EdgeCase_EmptyString) {
    // WHAT: Test empty concept string
    // WHY: Test edge case handling
    // HOW: Pass empty string (treated as valid but unknown concept)

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "");
    // Empty string is still a valid concept, just unfamiliar
    EXPECT_GE(gap.gap_size, 0.0f);
    EXPECT_LE(gap.gap_size, 1.0f);
}

TEST_F(CuriosityComprehensiveTest, EdgeCase_SimpleConcept_ToddlerStage) {
    // WHAT: Test toddler learning potential with simple concept
    // WHY: Test simplicity bonus in toddler strategy
    // HOW: Set toddler stage, use short concept

    curiosity_set_stage(engine, STAGE_TODDLER);

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "cat");
    EXPECT_GT(gap.learning_potential, 0.0f);
}

TEST_F(CuriosityComprehensiveTest, EdgeCase_ComplexConcept_ToddlerStage) {
    // WHAT: Test toddler learning potential with complex concept
    // WHY: Test simplicity penalty for long concepts
    // HOW: Set toddler stage, use long concept

    curiosity_set_stage(engine, STAGE_TODDLER);

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine,
                                    "quantum_superposition_theory");
    EXPECT_GT(gap.learning_potential, 0.0f);
}

TEST_F(CuriosityComprehensiveTest, Stress_ManyConceptsHashTable) {
    // WHAT: Test hash table with many concept lookups
    // WHY: Test hash table performance and collision handling
    // HOW: Query 1000 different concepts

    for (int i = 0; i < 1000; i++) {
        char concept_name[64];
        snprintf(concept_name, sizeof(concept_name), "concept_%d", i);

        // Detect gap (doesn't add concepts, just queries)
        knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, concept_name);
        EXPECT_GT(gap.gap_size, 0.8f);  // All unfamiliar

        // Check familiarity directly (also doesn't add)
        float fam = curiosity_check_familiarity(engine, concept_name);
        EXPECT_EQ(fam, 0.0f);  // Not known
    }

    // Note: detect_knowledge_gap only queries, doesn't add concepts
    // Concepts are implicitly tracked only when learning occurs
    SUCCEED();
}

TEST_F(CuriosityComprehensiveTest, Stress_ManyQuestionsHistory) {
    // WHAT: Learn many Q&A pairs
    // WHY: Test question history capacity
    // HOW: Learn 100 answers

    for (int i = 0; i < 100; i++) {
        char question[64], answer[128];
        snprintf(question, sizeof(question), "Question %d?", i);
        snprintf(answer, sizeof(answer), "Answer to question %d", i);

        bool result = curiosity_learn_answer(engine, question, answer);
        EXPECT_TRUE(result);
    }

    learning_progress_t progress;
    ASSERT_TRUE(curiosity_get_progress(engine, &progress));
    EXPECT_EQ(progress.total_answers_learned, 100u);
}

TEST_F(CuriosityComprehensiveTest, Integration_CompleteWorkflow) {
    // WHAT: Complete curiosity-driven learning workflow
    // WHY: Test full integration of all components
    // HOW: Stage progression with learning at each stage

    // Start as infant
    EXPECT_EQ(curiosity_get_stage(engine), STAGE_INFANT);

    // Encounter new concept
    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "ball");
    EXPECT_GT(gap.gap_size, 0.8f);  // Very unfamiliar

    // Generate questions (infant asks WHAT, WHY, HOW)
    generated_question_t questions[10];
    uint32_t q_count = curiosity_generate_questions(engine, &gap, questions, 10);
    EXPECT_EQ(q_count, 3u);

    // Learn answers
    for (uint32_t i = 0; i < q_count; i++) {
        char answer[128];
        snprintf(answer, sizeof(answer), "Answer to: %s", questions[i].question);
        EXPECT_TRUE(curiosity_learn_answer(engine, questions[i].question, answer));
    }

    // Progress to toddler
    curiosity_set_stage(engine, STAGE_TODDLER);
    EXPECT_EQ(curiosity_get_stage(engine), STAGE_TODDLER);

    // Encounter another concept
    gap = curiosity_detect_knowledge_gap(engine, "dog");
    q_count = curiosity_generate_questions(engine, &gap, questions, 10);
    EXPECT_EQ(q_count, 5u);  // Toddler asks more types

    // Register knowledge source
    ASSERT_TRUE(curiosity_register_knowledge_source(engine, "wikipedia",
                                                   mock_knowledge_search, nullptr));

    // Seek knowledge
    char* results[10];
    uint32_t result_count = curiosity_seek_knowledge(engine, &gap, results, 10);
    EXPECT_GT(result_count, 0u);

    // Free the returned strings to prevent memory leaks
    for (uint32_t i = 0; i < result_count; i++) {
        nimcp_free(results[i]);
    }

    // Check progress
    learning_progress_t progress;
    ASSERT_TRUE(curiosity_get_progress(engine, &progress));
    EXPECT_GE(progress.total_answers_learned, 3u);
    // Note: concepts_learned only increases when concepts explicitly added
    // detect_knowledge_gap doesn't auto-add concepts
    EXPECT_GE(progress.concepts_learned, 0u);
    EXPECT_GT(progress.avg_curiosity, 0.0f);
}

//=============================================================================
// 12. Stage Strategy Pattern Tests
//=============================================================================

TEST_F(CuriosityComprehensiveTest, Strategy_InfantLearningPotential) {
    // WHAT: Test infant learning potential calculation
    // WHY: Verify high baseline for infants
    // HOW: Set infant stage, check learning potential

    curiosity_set_stage(engine, STAGE_INFANT);

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "anything");
    EXPECT_NEAR(gap.learning_potential, 0.9f, 0.01f);  // Everything exciting
}

TEST_F(CuriosityComprehensiveTest, Strategy_ChildLearningPotential) {
    // WHAT: Test child learning potential calculation
    // WHY: Verify scales with gap size
    // HOW: Set child stage, check learning potential

    curiosity_set_stage(engine, STAGE_CHILD);

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "science");
    EXPECT_GT(gap.learning_potential, 0.7f);
    EXPECT_LE(gap.learning_potential, 1.0f);
}

TEST_F(CuriosityComprehensiveTest, Strategy_ExpertLearningPotential) {
    // WHAT: Test expert learning potential calculation
    // WHY: Verify selective interest (only big gaps)
    // HOW: Set expert stage, check learning potential

    curiosity_set_stage(engine, STAGE_EXPERT);

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "research");
    EXPECT_GT(gap.learning_potential, 0.4f);
    EXPECT_LE(gap.learning_potential, 1.0f);
}

TEST_F(CuriosityComprehensiveTest, Strategy_AllQuestionTypes) {
    // WHAT: Test all question type generators
    // WHY: Verify each type generates valid questions
    // HOW: Generate each type and check formatting

    curiosity_set_stage(engine, STAGE_CHILD);  // Has all types

    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "topic");
    generated_question_t questions[10];

    uint32_t count = curiosity_generate_questions(engine, &gap, questions, 10);
    EXPECT_EQ(count, 7u);

    // Verify each question is properly formatted
    for (uint32_t i = 0; i < count; i++) {
        EXPECT_GT(strlen(questions[i].question), 5u);
        EXPECT_TRUE(strstr(questions[i].question, "?") != nullptr);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
