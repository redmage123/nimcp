/**
 * @file test_curiosity.cpp
 * @brief Comprehensive unit tests for NIMCP Curiosity Engine
 */

#include "test_helpers.h"

#include "cognitive/curiosity/nimcp_curiosity.h"

#include <cstring>

namespace {

//=============================================================================
// Test Fixture
//=============================================================================

class CuriosityTest : public ::testing::Test {
   protected:
    void SetUp() override
    {
        engine = curiosity_engine_create(NULL, "test_learner");
        ASSERT_NE(engine, nullptr);
    }

    void TearDown() override
    {
        if (engine) {
            curiosity_engine_destroy(engine);
            engine = nullptr;
        }
    }

    // Helper to create test knowledge gap
    knowledge_gap_t create_test_gap(const char* topic)
    {
        knowledge_gap_t gap;
        memset(&gap, 0, sizeof(gap));
        strncpy(gap.topic, topic, sizeof(gap.topic) - 1);
        gap.gap_size = 0.8f;
        gap.curiosity_intensity = 0.7f;
        gap.learning_potential = 0.9f;
        gap.related_concepts = 0;
        gap.prerequisites = nullptr;
        gap.num_prerequisites = 0;
        return gap;
    }

    curiosity_engine_t engine;
};

//=============================================================================
// Creation/Destruction Tests
//=============================================================================

TEST_F(CuriosityTest, EngineCreation)
{
    EXPECT_NE(engine, nullptr);
}

TEST_F(CuriosityTest, EngineCreationNullName)
{
    curiosity_engine_t null_engine = curiosity_engine_create(NULL, nullptr);
    EXPECT_EQ(null_engine, nullptr);
}

TEST_F(CuriosityTest, EngineDestructionNullSafe)
{
    curiosity_engine_destroy(nullptr);
    // Should not crash
}

//=============================================================================
// Knowledge Gap Detection Tests
//=============================================================================

TEST_F(CuriosityTest, DetectKnowledgeGap)
{
    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "astronomy");

    EXPECT_STREQ(gap.topic, "astronomy");
    EXPECT_GE(gap.gap_size, 0.0f);
    EXPECT_LE(gap.gap_size, 1.0f);
    EXPECT_GE(gap.curiosity_intensity, 0.0f);
    EXPECT_LE(gap.curiosity_intensity, 1.0f);
    EXPECT_GE(gap.learning_potential, 0.0f);
    EXPECT_LE(gap.learning_potential, 1.0f);
}

TEST_F(CuriosityTest, DetectKnowledgeGapNull)
{
    knowledge_gap_t gap = curiosity_detect_knowledge_gap(nullptr, "test");
    EXPECT_EQ(gap.topic[0], '\0');

    gap = curiosity_detect_knowledge_gap(engine, nullptr);
    EXPECT_EQ(gap.topic[0], '\0');
}

TEST_F(CuriosityTest, DetectKnowledgeGapUnknownConcept)
{
    // First check: should be unknown
    knowledge_gap_t gap1 = curiosity_detect_knowledge_gap(engine, "quantum_physics");
    float gap_size1 = gap1.gap_size;

    // Learn about it
    curiosity_learn_answer(engine, "What is quantum physics?",
                           "Quantum physics is the study of matter at atomic scales.");

    // Second check: gap should be smaller
    knowledge_gap_t gap2 = curiosity_detect_knowledge_gap(engine, "quantum_physics");
    float gap_size2 = gap2.gap_size;

    EXPECT_LE(gap_size2, gap_size1);
}

//=============================================================================
// Familiarity Check Tests
//=============================================================================

TEST_F(CuriosityTest, CheckFamiliarityUnknown)
{
    float familiarity = curiosity_check_familiarity(engine, "unknown_concept_xyz");
    EXPECT_EQ(familiarity, 0.0f);
}

TEST_F(CuriosityTest, CheckFamiliarityNull)
{
    EXPECT_EQ(curiosity_check_familiarity(nullptr, "test"), 0.0f);
    EXPECT_EQ(curiosity_check_familiarity(engine, nullptr), 0.0f);
}

//=============================================================================
// Related Concepts Tests
//=============================================================================

TEST_F(CuriosityTest, GetRelatedConcepts)
{
    char* related[10];
    uint32_t num_related = curiosity_get_related_concepts(engine, "science", related, 10);

    EXPECT_GE(num_related, 0);
    EXPECT_LE(num_related, 10);
}

TEST_F(CuriosityTest, GetRelatedConceptsNull)
{
    char* related[10];

    EXPECT_EQ(curiosity_get_related_concepts(nullptr, "test", related, 10), 0);
    EXPECT_EQ(curiosity_get_related_concepts(engine, nullptr, related, 10), 0);
}

TEST_F(CuriosityTest, GetRelatedConceptsCountOnly)
{
    // Passing NULL for related array should return count only
    uint32_t count = curiosity_get_related_concepts(engine, "science", nullptr, 10);
    EXPECT_GE(count, 0);
}

//=============================================================================
// Question Generation Tests
//=============================================================================

TEST_F(CuriosityTest, GenerateQuestions)
{
    knowledge_gap_t gap = create_test_gap("planets");
    generated_question_t questions[10];

    uint32_t num_generated = curiosity_generate_questions(engine, &gap, questions, 10);

    EXPECT_GT(num_generated, 0);
    EXPECT_LE(num_generated, 10);

    // Verify question properties
    for (uint32_t i = 0; i < num_generated; i++) {
        EXPECT_GT(strlen(questions[i].question), 0);
        EXPECT_GE(questions[i].priority, 0.0f);
        EXPECT_LE(questions[i].priority, 1.0f);
        EXPECT_GE(questions[i].difficulty, 0.0f);
        EXPECT_LE(questions[i].difficulty, 1.0f);
    }
}

TEST_F(CuriosityTest, GenerateQuestionsNull)
{
    knowledge_gap_t gap = create_test_gap("test");
    generated_question_t questions[10];

    EXPECT_EQ(curiosity_generate_questions(nullptr, &gap, questions, 10), 0);
    EXPECT_EQ(curiosity_generate_questions(engine, nullptr, questions, 10), 0);
    EXPECT_EQ(curiosity_generate_questions(engine, &gap, nullptr, 10), 0);
    EXPECT_EQ(curiosity_generate_questions(engine, &gap, questions, 0), 0);
}

TEST_F(CuriosityTest, GenerateQuestionsTypes)
{
    knowledge_gap_t gap = create_test_gap("universe");
    generated_question_t questions[10];

    uint32_t num_generated = curiosity_generate_questions(engine, &gap, questions, 10);
    ASSERT_GT(num_generated, 0);

    // Should generate different question types (WHAT, WHY, HOW, etc.)
    bool has_what = false;
    bool has_why = false;
    bool has_how = false;

    for (uint32_t i = 0; i < num_generated; i++) {
        if (questions[i].type == QUESTION_WHAT)
            has_what = true;
        if (questions[i].type == QUESTION_WHY)
            has_why = true;
        if (questions[i].type == QUESTION_HOW)
            has_how = true;
    }

    // At least one question type should be generated
    EXPECT_TRUE(has_what || has_why || has_how);
}

//=============================================================================
// Follow-up Question Tests
//=============================================================================

TEST_F(CuriosityTest, GenerateFollowup)
{
    const char* answer = "Stars are giant balls of hot gas.";
    const char* followup = curiosity_generate_followup(engine, answer);

    EXPECT_NE(followup, nullptr);
    if (followup) {
        EXPECT_GT(strlen(followup), 0);
    }
}

TEST_F(CuriosityTest, GenerateFollowupNull)
{
    EXPECT_EQ(curiosity_generate_followup(nullptr, "answer"), nullptr);
    EXPECT_EQ(curiosity_generate_followup(engine, nullptr), nullptr);
}

//=============================================================================
// Motivation Assessment Tests
//=============================================================================

TEST_F(CuriosityTest, AssessMotivation)
{
    motivation_state_t state = curiosity_assess_motivation(engine, "learning");

    EXPECT_GE(state.intrinsic_curiosity, 0.0f);
    EXPECT_LE(state.intrinsic_curiosity, 1.0f);
    EXPECT_GE(state.goal_relevance, 0.0f);
    EXPECT_LE(state.goal_relevance, 1.0f);
    EXPECT_GE(state.social_importance, 0.0f);
    EXPECT_LE(state.social_importance, 1.0f);
    EXPECT_GE(state.survival_value, 0.0f);
    EXPECT_LE(state.survival_value, 1.0f);
    EXPECT_GE(state.aesthetic_appeal, 0.0f);
    EXPECT_LE(state.aesthetic_appeal, 1.0f);
    EXPECT_GE(state.overall_motivation, 0.0f);
    EXPECT_LE(state.overall_motivation, 1.0f);
}

TEST_F(CuriosityTest, AssessMotivationNull)
{
    motivation_state_t state = curiosity_assess_motivation(nullptr, "test");
    EXPECT_EQ(state.overall_motivation, 0.0f);

    state = curiosity_assess_motivation(engine, nullptr);
    EXPECT_EQ(state.overall_motivation, 0.0f);
}

//=============================================================================
// Baseline Curiosity Tests
//=============================================================================

TEST_F(CuriosityTest, SetBaseline)
{
    curiosity_set_baseline(engine, 0.8f);
    float drive = curiosity_get_drive(engine);

    EXPECT_TRUE(float_equals(drive, 0.8f));
}

TEST_F(CuriosityTest, SetBaselineClampedToRange)
{
    // Test upper bound
    curiosity_set_baseline(engine, 1.5f);
    float drive1 = curiosity_get_drive(engine);
    EXPECT_LE(drive1, 1.0f);

    // Test lower bound
    curiosity_set_baseline(engine, -0.5f);
    float drive2 = curiosity_get_drive(engine);
    EXPECT_GE(drive2, 0.0f);
}

TEST_F(CuriosityTest, SetBaselineNull)
{
    curiosity_set_baseline(nullptr, 0.5f);
    // Should not crash
}

TEST_F(CuriosityTest, GetDriveNull)
{
    EXPECT_EQ(curiosity_get_drive(nullptr), 0.0f);
}

//=============================================================================
// Learning from Answers Tests
//=============================================================================

TEST_F(CuriosityTest, LearnAnswer)
{
    const char* question = "What is photosynthesis?";
    const char* answer = "Photosynthesis is how plants convert sunlight into energy.";

    bool success = curiosity_learn_answer(engine, question, answer);
    EXPECT_TRUE(success);
}

TEST_F(CuriosityTest, LearnAnswerNull)
{
    EXPECT_FALSE(curiosity_learn_answer(nullptr, "Q", "A"));
    EXPECT_FALSE(curiosity_learn_answer(engine, nullptr, "A"));
    EXPECT_FALSE(curiosity_learn_answer(engine, "Q", nullptr));
}

TEST_F(CuriosityTest, LearnAnswerUpdatesProgress)
{
    learning_progress_t progress1;
    ASSERT_TRUE(curiosity_get_progress(engine, &progress1));
    uint64_t answers_before = progress1.total_answers_learned;

    curiosity_learn_answer(engine, "What is X?", "X is Y.");

    learning_progress_t progress2;
    ASSERT_TRUE(curiosity_get_progress(engine, &progress2));
    uint64_t answers_after = progress2.total_answers_learned;

    EXPECT_GT(answers_after, answers_before);
}

//=============================================================================
// Learning from Experience Tests
//=============================================================================

TEST_F(CuriosityTest, LearnExperience)
{
    const char* description = "Saw a bird flying in the sky.";
    float sensory_data[] = {0.1f, 0.2f, 0.3f};

    bool success = curiosity_learn_experience(engine, description, sensory_data, 3);
    EXPECT_TRUE(success);
}

TEST_F(CuriosityTest, LearnExperienceNoSensoryData)
{
    const char* description = "Heard a sound.";

    bool success = curiosity_learn_experience(engine, description, nullptr, 0);
    EXPECT_TRUE(success);
}

TEST_F(CuriosityTest, LearnExperienceNull)
{
    float sensory_data[] = {0.1f, 0.2f};

    EXPECT_FALSE(curiosity_learn_experience(nullptr, "test", sensory_data, 2));
    EXPECT_FALSE(curiosity_learn_experience(engine, nullptr, sensory_data, 2));
}

//=============================================================================
// Learning from Observation Tests
//=============================================================================

TEST_F(CuriosityTest, LearnObservation)
{
    const char* observed = "People greeting each other with handshakes.";
    const char* context = "Social interaction";

    bool success = curiosity_learn_observation(engine, observed, context);
    EXPECT_TRUE(success);
}

TEST_F(CuriosityTest, LearnObservationNull)
{
    EXPECT_FALSE(curiosity_learn_observation(nullptr, "observed", "context"));
    EXPECT_FALSE(curiosity_learn_observation(engine, nullptr, "context"));
}

//=============================================================================
// Knowledge Source Registration Tests
//=============================================================================

// Mock search function for testing
static char** mock_search_fn(const char* query, void* context, uint32_t max_results,
                             uint32_t* num_results)
{
    *num_results = 0;
    return nullptr;
}

TEST_F(CuriosityTest, RegisterKnowledgeSource)
{
    bool success =
        curiosity_register_knowledge_source(engine, "test_source", mock_search_fn, nullptr);
    EXPECT_TRUE(success);
}

TEST_F(CuriosityTest, RegisterKnowledgeSourceNull)
{
    EXPECT_FALSE(curiosity_register_knowledge_source(nullptr, "source", mock_search_fn, nullptr));
    EXPECT_FALSE(curiosity_register_knowledge_source(engine, nullptr, mock_search_fn, nullptr));
    EXPECT_FALSE(curiosity_register_knowledge_source(engine, "source", nullptr, nullptr));
}

//=============================================================================
// Knowledge Seeking Tests
//=============================================================================

TEST_F(CuriosityTest, SeekKnowledge)
{
    // Register a source first
    curiosity_register_knowledge_source(engine, "test_source", mock_search_fn, nullptr);

    knowledge_gap_t gap = create_test_gap("stars");
    char* results[10];

    uint32_t num_results = curiosity_seek_knowledge(engine, &gap, results, 10);

    // Mock function returns 0, so we expect 0
    EXPECT_EQ(num_results, 0);
}

TEST_F(CuriosityTest, SeekKnowledgeNull)
{
    knowledge_gap_t gap = create_test_gap("test");
    char* results[10];

    EXPECT_EQ(curiosity_seek_knowledge(nullptr, &gap, results, 10), 0);
    EXPECT_EQ(curiosity_seek_knowledge(engine, nullptr, results, 10), 0);
    EXPECT_EQ(curiosity_seek_knowledge(engine, &gap, nullptr, 10), 0);
}

//=============================================================================
// Learning Progress Tests
//=============================================================================

TEST_F(CuriosityTest, GetProgress)
{
    learning_progress_t progress;
    bool success = curiosity_get_progress(engine, &progress);

    EXPECT_TRUE(success);
    EXPECT_GE(progress.total_questions_asked, 0);
    EXPECT_GE(progress.total_answers_learned, 0);
    EXPECT_GE(progress.total_experiences, 0);
    EXPECT_GE(progress.concepts_learned, 0);
    EXPECT_GE(progress.avg_curiosity, 0.0f);
    EXPECT_LE(progress.avg_curiosity, 1.0f);
}

TEST_F(CuriosityTest, GetProgressNull)
{
    learning_progress_t progress;

    EXPECT_FALSE(curiosity_get_progress(nullptr, &progress));
    EXPECT_FALSE(curiosity_get_progress(engine, nullptr));
}

TEST_F(CuriosityTest, GetProgressTracksLearning)
{
    learning_progress_t progress1;
    ASSERT_TRUE(curiosity_get_progress(engine, &progress1));

    // Learn something
    curiosity_learn_answer(engine, "What is AI?", "AI is artificial intelligence.");

    learning_progress_t progress2;
    ASSERT_TRUE(curiosity_get_progress(engine, &progress2));

    EXPECT_GT(progress2.total_answers_learned, progress1.total_answers_learned);
}

//=============================================================================
// Domain Coverage Tests
//=============================================================================

TEST_F(CuriosityTest, GetDomainCoverage)
{
    float coverage = curiosity_get_domain_coverage(engine, "science");

    EXPECT_GE(coverage, 0.0f);
    EXPECT_LE(coverage, 1.0f);
}

TEST_F(CuriosityTest, GetDomainCoverageNull)
{
    EXPECT_EQ(curiosity_get_domain_coverage(nullptr, "science"), 0.0f);
    EXPECT_EQ(curiosity_get_domain_coverage(engine, nullptr), 0.0f);
}

//=============================================================================
// Learning Stage Tests
//=============================================================================

TEST_F(CuriosityTest, GetStageDefault)
{
    learning_stage_t stage = curiosity_get_stage(engine);
    EXPECT_EQ(stage, STAGE_INFANT);
}

TEST_F(CuriosityTest, SetStage)
{
    curiosity_set_stage(engine, STAGE_CHILD);
    learning_stage_t stage = curiosity_get_stage(engine);
    EXPECT_EQ(stage, STAGE_CHILD);
}

TEST_F(CuriosityTest, SetStageChangesBaseline)
{
    curiosity_set_stage(engine, STAGE_INFANT);
    float infant_baseline = curiosity_get_drive(engine);

    curiosity_set_stage(engine, STAGE_ADULT);
    float adult_baseline = curiosity_get_drive(engine);

    // Adult baseline should be lower than infant
    EXPECT_LT(adult_baseline, infant_baseline);
}

TEST_F(CuriosityTest, SetStageNull)
{
    curiosity_set_stage(nullptr, STAGE_CHILD);
    // Should not crash
}

TEST_F(CuriosityTest, GetStageNull)
{
    learning_stage_t stage = curiosity_get_stage(nullptr);
    EXPECT_EQ(stage, STAGE_INFANT);
}

TEST_F(CuriosityTest, SetStageAffectsQuestionGeneration)
{
    knowledge_gap_t gap = create_test_gap("physics");
    generated_question_t questions_infant[10];
    generated_question_t questions_expert[10];

    // Infant stage
    curiosity_set_stage(engine, STAGE_INFANT);
    uint32_t num_infant = curiosity_generate_questions(engine, &gap, questions_infant, 10);

    // Expert stage
    curiosity_set_stage(engine, STAGE_EXPERT);
    uint32_t num_expert = curiosity_generate_questions(engine, &gap, questions_expert, 10);

    // Both should generate some questions
    EXPECT_GT(num_infant, 0);
    EXPECT_GT(num_expert, 0);

    // Question counts or types may differ based on stage
    // (Actual behavior depends on implementation)
}

//=============================================================================
// Utility Function Tests
//=============================================================================

TEST_F(CuriosityTest, PrintGapNull)
{
    curiosity_print_gap(nullptr);
    // Should not crash
}

TEST_F(CuriosityTest, PrintQuestionNull)
{
    curiosity_print_question(nullptr);
    // Should not crash
}

TEST_F(CuriosityTest, PrintProgressNull)
{
    curiosity_print_progress(nullptr);
    // Should not crash
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(CuriosityTest, IntegrationFullLearningCycle)
{
    // 1. Detect knowledge gap
    knowledge_gap_t gap = curiosity_detect_knowledge_gap(engine, "stars");
    EXPECT_GT(gap.gap_size, 0.0f);

    // 2. Generate questions
    generated_question_t questions[5];
    uint32_t num_q = curiosity_generate_questions(engine, &gap, questions, 5);
    ASSERT_GT(num_q, 0);

    // 3. Learn answers
    bool learned = curiosity_learn_answer(engine, questions[0].question,
                                          "Stars are giant balls of burning gas.");
    EXPECT_TRUE(learned);

    // 4. Check progress
    learning_progress_t progress;
    ASSERT_TRUE(curiosity_get_progress(engine, &progress));
    EXPECT_GT(progress.total_answers_learned, 0);
}

TEST_F(CuriosityTest, IntegrationCuriosityDrivenExploration)
{
    // Set high baseline curiosity
    curiosity_set_baseline(engine, 0.9f);

    // Assess motivation for various topics
    motivation_state_t m1 = curiosity_assess_motivation(engine, "universe");
    motivation_state_t m2 = curiosity_assess_motivation(engine, "atoms");
    motivation_state_t m3 = curiosity_assess_motivation(engine, "biology");

    // All should have high intrinsic curiosity
    EXPECT_GT(m1.intrinsic_curiosity, 0.8f);
    EXPECT_GT(m2.intrinsic_curiosity, 0.8f);
    EXPECT_GT(m3.intrinsic_curiosity, 0.8f);
}

TEST_F(CuriosityTest, IntegrationProgressionThroughStages)
{
    // Start as infant
    curiosity_set_stage(engine, STAGE_INFANT);
    EXPECT_EQ(curiosity_get_stage(engine), STAGE_INFANT);

    // Progress to toddler
    curiosity_set_stage(engine, STAGE_TODDLER);
    EXPECT_EQ(curiosity_get_stage(engine), STAGE_TODDLER);

    // Progress to child
    curiosity_set_stage(engine, STAGE_CHILD);
    EXPECT_EQ(curiosity_get_stage(engine), STAGE_CHILD);

    // Each stage should maintain functionality
    learning_progress_t progress;
    EXPECT_TRUE(curiosity_get_progress(engine, &progress));
}

}  // anonymous namespace
