/**
 * @file test_grounded_language_brain_integration.cpp
 * @brief Integration tests: grounded language system wired into a full brain
 *
 * WHAT: Tests that the grounded language system is properly initialized by the
 *       brain factory, connected to semantic memory, and accessible via the
 *       public NIMCP API
 * WHY:  The grounded language system must be properly wired into the brain's
 *       subsystem graph — initialization, cross-modal connections, and cleanup
 * HOW:  Creates a brain via the public API, exercises grounded language functions
 *       through nimcp_brain_* wrappers, verifies end-to-end data flow
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>

extern "C" {
#include "nimcp.h"
}

// =============================================================================
// Constants
// =============================================================================

static constexpr uint32_t BRAIN_NEURONS = 100;
static constexpr uint32_t SEMANTIC_DIM = 128;

// =============================================================================
// Helpers
// =============================================================================

static std::vector<float> random_vector(uint32_t dim, unsigned seed = 42) {
    std::vector<float> v(dim);
    srand(seed);
    for (uint32_t i = 0; i < dim; i++) {
        v[i] = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
    }
    return v;
}

// =============================================================================
// Test Fixture
// =============================================================================

class GroundedLanguageBrainIntegration : public ::testing::Test {
protected:
    nimcp_brain_t brain = nullptr;

    void SetUp() override {
        brain = nimcp_brain_create_fast("grounded_lang_test",
                                    NIMCP_TASK_CLASSIFICATION,
                                    BRAIN_NEURONS, 10, BRAIN_NEURONS);
        // Brain creation may take time; if it fails, skip
        if (!brain) {
            GTEST_SKIP() << "Brain creation failed (resource constrained)";
        }
    }

    void TearDown() override {
        if (brain) {
            nimcp_brain_destroy(brain);
        }
    }
};

// =============================================================================
// Initialization Tests
// =============================================================================

TEST_F(GroundedLanguageBrainIntegration, GroundedLangInitialized) {
    // Verify grounded language is initialized by testing API accessibility
    // If grounded_respond returns NIMCP_OK, the system is initialized
    char response[64] = {};
    float conf = 0.0f;
    nimcp_status_t s = nimcp_brain_grounded_respond(brain, "hello", response, 64, &conf);
    EXPECT_EQ(s, NIMCP_OK);
}

// =============================================================================
// Ground Word via API Tests
// =============================================================================

TEST_F(GroundedLanguageBrainIntegration, GroundWordAPI) {
    auto features = random_vector(SEMANTIC_DIM, 10);
    nimcp_status_t status = nimcp_brain_ground_word(
        brain, "elephant", features.data(), SEMANTIC_DIM, 0, 0.9f);
    EXPECT_EQ(status, NIMCP_OK);
}

TEST_F(GroundedLanguageBrainIntegration, GroundWordNullBrain) {
    auto features = random_vector(SEMANTIC_DIM);
    nimcp_status_t status = nimcp_brain_ground_word(
        nullptr, "test", features.data(), SEMANTIC_DIM, 0, 0.5f);
    EXPECT_NE(status, NIMCP_OK);
}

// =============================================================================
// Learn Language via API Tests
// =============================================================================

TEST_F(GroundedLanguageBrainIntegration, LearnLanguageAPI) {
    float loss = 0.0f;
    nimcp_status_t status = nimcp_brain_learn_language(
        brain, "the big red dog runs fast in the park", &loss);
    EXPECT_EQ(status, NIMCP_OK);
    EXPECT_GT(loss, 0.0f);
}

TEST_F(GroundedLanguageBrainIntegration, LearnLanguagePairAPI) {
    float loss = 0.0f;
    nimcp_status_t status = nimcp_brain_learn_language_pair(
        brain, "what color is the sky", "the sky is blue", 0.1f, &loss);
    EXPECT_EQ(status, NIMCP_OK);
    EXPECT_GE(loss, 0.0f);
    EXPECT_LE(loss, 2.0f);
}

// =============================================================================
// Comprehend via API Tests
// =============================================================================

TEST_F(GroundedLanguageBrainIntegration, ComprehendAPI) {
    // First teach some words
    nimcp_brain_learn_language(brain, "the cat sits on the mat", nullptr);

    auto cat_f = random_vector(SEMANTIC_DIM, 50);
    nimcp_brain_ground_word(brain, "cat", cat_f.data(), SEMANTIC_DIM, 0, 0.9f);

    float semantic[128] = {};
    float confidence = 0.0f;
    nimcp_status_t status = nimcp_brain_comprehend(
        brain, "the cat", semantic, 128, &confidence);
    EXPECT_EQ(status, NIMCP_OK);
    EXPECT_GT(confidence, 0.0f);

    // Semantic vector should be non-zero
    float norm = 0.0f;
    for (int i = 0; i < 128; i++) norm += semantic[i] * semantic[i];
    EXPECT_GT(norm, 0.01f);
}

// =============================================================================
// Produce Text via API Tests
// =============================================================================

TEST_F(GroundedLanguageBrainIntegration, ProduceTextAPI) {
    // Teach vocabulary
    auto dog_f = random_vector(SEMANTIC_DIM, 60);
    nimcp_brain_ground_word(brain, "dog", dog_f.data(), SEMANTIC_DIM, 0, 0.9f);

    nimcp_brain_learn_language(brain, "the big dog runs fast", nullptr);

    char text[1024] = {};
    float confidence = 0.0f;
    nimcp_status_t status = nimcp_brain_produce_text(
        brain, dog_f.data(), SEMANTIC_DIM, text, sizeof(text), &confidence);
    EXPECT_EQ(status, NIMCP_OK);
    EXPECT_GT(strlen(text), 0u);
}

// =============================================================================
// Grounded Respond via API Tests
// =============================================================================

TEST_F(GroundedLanguageBrainIntegration, GroundedRespondAPI) {
    // Teach vocabulary
    nimcp_brain_learn_language(brain, "the sky is blue and beautiful", nullptr);
    auto sky_f = random_vector(SEMANTIC_DIM, 70);
    nimcp_brain_ground_word(brain, "sky", sky_f.data(), SEMANTIC_DIM, 0, 0.9f);

    char response[1024] = {};
    float confidence = 0.0f;
    nimcp_status_t status = nimcp_brain_grounded_respond(
        brain, "describe the sky", response, sizeof(response), &confidence);
    EXPECT_EQ(status, NIMCP_OK);
    EXPECT_GT(strlen(response), 0u);
}

// =============================================================================
// Creative Blend via API Tests
// =============================================================================

TEST_F(GroundedLanguageBrainIntegration, CreativeBlendAPI) {
    // Teach vocabulary
    auto fire_f = random_vector(SEMANTIC_DIM, 80);
    auto water_f = random_vector(SEMANTIC_DIM, 81);
    nimcp_brain_ground_word(brain, "fire", fire_f.data(), SEMANTIC_DIM, 0, 0.9f);
    nimcp_brain_ground_word(brain, "water", water_f.data(), SEMANTIC_DIM, 0, 0.9f);
    nimcp_brain_learn_language(brain, "fire is hot and bright", nullptr);
    nimcp_brain_learn_language(brain, "water is cold and flowing", nullptr);

    char text[1024] = {};
    nimcp_status_t status = nimcp_brain_creative_blend(
        brain, fire_f.data(), water_f.data(), SEMANTIC_DIM, 0.5f,
        text, sizeof(text));
    EXPECT_EQ(status, NIMCP_OK);
    EXPECT_GT(strlen(text), 0u);
}

// =============================================================================
// Multi-step Learning + Comprehension + Production
// =============================================================================

TEST_F(GroundedLanguageBrainIntegration, FullLearningPipeline) {
    // 1. Expose to text
    const char* corpus[] = {
        "the big red dog runs fast",
        "the small cat sits quietly",
        "the blue sky is beautiful",
        "a happy child plays outside",
    };
    for (int i = 0; i < 4; i++) {
        nimcp_brain_learn_language(brain, corpus[i], nullptr);
    }

    // 2. Ground key words
    auto dog_f = random_vector(SEMANTIC_DIM, 90);
    auto cat_f = random_vector(SEMANTIC_DIM, 91);
    nimcp_brain_ground_word(brain, "dog", dog_f.data(), SEMANTIC_DIM, 0, 0.9f);
    nimcp_brain_ground_word(brain, "cat", cat_f.data(), SEMANTIC_DIM, 0, 0.9f);

    // 3. Learn pairs
    nimcp_brain_learn_language_pair(brain, "what does the dog do",
                                    "the dog runs fast", 0.1f, nullptr);
    nimcp_brain_learn_language_pair(brain, "what does the cat do",
                                    "the cat sits quietly", 0.1f, nullptr);

    // 4. Comprehend
    float semantic[128] = {};
    float confidence = 0.0f;
    nimcp_brain_comprehend(brain, "the big dog", semantic, 128, &confidence);
    EXPECT_GT(confidence, 0.0f);

    // 5. Produce from comprehended vector
    char text[1024] = {};
    float prod_conf = 0.0f;
    nimcp_brain_produce_text(brain, semantic, 128, text, sizeof(text), &prod_conf);
    // Should produce something
    EXPECT_GT(strlen(text), 0u);
}
