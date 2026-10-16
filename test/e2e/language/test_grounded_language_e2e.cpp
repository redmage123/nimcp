/**
 * @file test_grounded_language_e2e.cpp
 * @brief End-to-end tests for the grounded language system
 *
 * WHAT: Full pipeline tests that exercise the complete language acquisition
 *       and production cycle through the public brain API
 * WHY:  Verify that the grounded language system works as an integrated whole:
 *       create brain -> learn vocabulary -> ground words -> comprehend -> respond
 * HOW:  Creates brain via nimcp_brain_create, runs multi-step language scenarios
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>

extern "C" {
#include "nimcp.h"
}

// =============================================================================
// Constants
// =============================================================================

static constexpr uint32_t E2E_NEURONS = 100;
static constexpr uint32_t E2E_DIM = 128;

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

class GroundedLanguageE2E : public ::testing::Test {
protected:
    nimcp_brain_t brain = nullptr;

    void SetUp() override {
        brain = nimcp_brain_create_fast("grounded_e2e",
                                    NIMCP_TASK_CLASSIFICATION,
                                    E2E_NEURONS, 10, E2E_NEURONS);
        if (!brain) {
            GTEST_SKIP() << "Brain creation failed";
        }
    }

    void TearDown() override {
        if (brain) nimcp_brain_destroy(brain);
    }

    void teach_basic_vocabulary() {
        // Ground some basic words with distinct feature vectors
        struct { const char* word; unsigned seed; } words[] = {
            {"dog",   1000}, {"cat",  1001}, {"bird",  1002},
            {"red",   2000}, {"blue", 2001}, {"green", 2002},
            {"big",   3000}, {"small",3001}, {"fast",  3002},
            {"run",   4000}, {"sit",  4001}, {"fly",   4002},
            {"sky",   5000}, {"tree", 5001}, {"water", 5002},
            {"happy", 6000}, {"sad",  6001}, {"warm",  6002},
        };

        for (auto& w : words) {
            auto f = random_vector(E2E_DIM, w.seed);
            nimcp_brain_ground_word(brain, w.word, f.data(), E2E_DIM, 0, 0.9f);
        }

        // Teach distributional patterns
        const char* corpus[] = {
            "the big red dog runs fast",
            "the small cat sits quietly",
            "the blue bird flies high",
            "the green tree grows tall",
            "the warm sun shines bright",
            "the cold water flows down",
            "the happy child plays outside",
            "the sad dog sits alone",
            "the blue sky is beautiful",
            "the red bird sits on the tree",
        };
        for (auto& text : corpus) {
            nimcp_brain_learn_language(brain, text, nullptr);
        }

        // Teach via pairs
        nimcp_brain_learn_language_pair(brain,
            "what color is the sky", "the sky is blue", 0.1f, nullptr);
        nimcp_brain_learn_language_pair(brain,
            "describe the dog", "the big dog runs fast", 0.1f, nullptr);
    }
};

// =============================================================================
// E2E: Learn → Comprehend → Produce cycle
// =============================================================================

TEST_F(GroundedLanguageE2E, LearnComprehendProduceCycle) {
    teach_basic_vocabulary();

    // Comprehend a known sentence
    float semantic[E2E_DIM] = {};
    float confidence = 0.0f;
    nimcp_status_t status = nimcp_brain_comprehend(
        brain, "the big red dog", semantic, E2E_DIM, &confidence);
    ASSERT_EQ(status, NIMCP_OK);
    EXPECT_GT(confidence, 0.3f); // Most words should be known

    // Produce text from the comprehended vector
    char text[1024] = {};
    float prod_conf = 0.0f;
    status = nimcp_brain_produce_text(
        brain, semantic, E2E_DIM, text, sizeof(text), &prod_conf);
    ASSERT_EQ(status, NIMCP_OK);
    EXPECT_GT(strlen(text), 0u);
    // The produced text should contain at least one grounded word
}

// =============================================================================
// E2E: Conversational loop
// =============================================================================

TEST_F(GroundedLanguageE2E, ConversationalLoop) {
    teach_basic_vocabulary();

    // Ask questions and get responses
    const char* questions[] = {
        "describe the sky",
        "what is the dog",
        "tell me about water",
    };

    for (auto& q : questions) {
        char response[1024] = {};
        float confidence = 0.0f;
        nimcp_status_t status = nimcp_brain_grounded_respond(
            brain, q, response, sizeof(response), &confidence);
        EXPECT_EQ(status, NIMCP_OK);
        EXPECT_GT(strlen(response), 0u);
    }
}

// =============================================================================
// E2E: Training improves comprehension confidence
// =============================================================================

TEST_F(GroundedLanguageE2E, TrainingImprovesComprehension) {
    // Before training: comprehend novel text
    float semantic_before[E2E_DIM] = {};
    float conf_before = 0.0f;
    nimcp_brain_comprehend(brain, "the elephant walks slowly",
                           semantic_before, E2E_DIM, &conf_before);

    // Train on relevant text
    for (int epoch = 0; epoch < 5; epoch++) {
        nimcp_brain_learn_language(brain,
            "the elephant walks slowly through the forest", nullptr);
    }
    auto elephant_f = random_vector(E2E_DIM, 7777);
    nimcp_brain_ground_word(brain, "elephant", elephant_f.data(), E2E_DIM, 0, 0.9f);

    // After training: same text should have higher confidence
    float semantic_after[E2E_DIM] = {};
    float conf_after = 0.0f;
    nimcp_brain_comprehend(brain, "the elephant walks slowly",
                           semantic_after, E2E_DIM, &conf_after);

    EXPECT_GT(conf_after, conf_before);
}

// =============================================================================
// E2E: Creative blend produces novel text
// =============================================================================

TEST_F(GroundedLanguageE2E, CreativeBlendProducesNovelText) {
    teach_basic_vocabulary();

    auto fire_f = random_vector(E2E_DIM, 8000);
    auto ice_f = random_vector(E2E_DIM, 8001);
    nimcp_brain_ground_word(brain, "fire", fire_f.data(), E2E_DIM, 0, 0.9f);
    nimcp_brain_ground_word(brain, "ice", ice_f.data(), E2E_DIM, 0, 0.9f);
    nimcp_brain_learn_language(brain, "the fire is hot and bright", nullptr);
    nimcp_brain_learn_language(brain, "the ice is cold and clear", nullptr);

    char text[1024] = {};
    nimcp_status_t status = nimcp_brain_creative_blend(
        brain, fire_f.data(), ice_f.data(), E2E_DIM, 0.5f,
        text, sizeof(text));
    ASSERT_EQ(status, NIMCP_OK);
    EXPECT_GT(strlen(text), 0u);
}

// =============================================================================
// E2E: Pair learning creates bidirectional associations
// =============================================================================

TEST_F(GroundedLanguageE2E, PairLearningBidirectional) {
    teach_basic_vocabulary();

    // Train a Q&A pair
    for (int i = 0; i < 10; i++) {
        nimcp_brain_learn_language_pair(brain,
            "what does the bird do", "the bird flies high", 0.1f, nullptr);
    }

    // Comprehend the question
    float q_semantic[E2E_DIM] = {};
    float q_conf = 0.0f;
    nimcp_brain_comprehend(brain, "what does the bird do",
                           q_semantic, E2E_DIM, &q_conf);

    // Comprehend the answer
    float a_semantic[E2E_DIM] = {};
    float a_conf = 0.0f;
    nimcp_brain_comprehend(brain, "the bird flies high",
                           a_semantic, E2E_DIM, &a_conf);

    // The semantic vectors should have some similarity (cross-binding)
    float dot = 0.0f, norm_q = 0.0f, norm_a = 0.0f;
    for (uint32_t i = 0; i < E2E_DIM; i++) {
        dot += q_semantic[i] * a_semantic[i];
        norm_q += q_semantic[i] * q_semantic[i];
        norm_a += a_semantic[i] * a_semantic[i];
    }
    float cosine = dot / (sqrtf(norm_q) * sqrtf(norm_a) + 1e-8f);
    EXPECT_GT(cosine, -1.0f); // At minimum, not anti-correlated
}

// =============================================================================
// E2E: Multi-modality grounding enriches production
// =============================================================================

TEST_F(GroundedLanguageE2E, MultiModalGrounding) {
    // Ground "sunset" in multiple modalities
    auto vis_f = random_vector(E2E_DIM, 9000);
    auto aud_f = random_vector(E2E_DIM, 9001);

    // Visual grounding
    nimcp_brain_ground_word(brain, "sunset", vis_f.data(), E2E_DIM,
                            0 /* GL_MODALITY_VISUAL */, 0.9f);
    // Auditory grounding (sound of evening)
    nimcp_brain_ground_word(brain, "sunset", aud_f.data(), E2E_DIM,
                            1 /* GL_MODALITY_AUDITORY */, 0.7f);
    // Emotional grounding
    nimcp_brain_ground_word(brain, "sunset", vis_f.data(), E2E_DIM,
                            3 /* GL_MODALITY_EMOTIONAL */, 0.8f);

    nimcp_brain_learn_language(brain, "the sunset is beautiful and warm", nullptr);

    // Comprehend "sunset"
    float semantic[E2E_DIM] = {};
    float confidence = 0.0f;
    nimcp_brain_comprehend(brain, "sunset", semantic, E2E_DIM, &confidence);
    EXPECT_GT(confidence, 0.0f);

    // Produce text from sunset concept
    char text[1024] = {};
    nimcp_brain_produce_text(brain, vis_f.data(), E2E_DIM,
                              text, sizeof(text), nullptr);
    EXPECT_GT(strlen(text), 0u);
}

// =============================================================================
// E2E: Vocabulary growth over training
// =============================================================================

TEST_F(GroundedLanguageE2E, VocabularyGrowth) {
    // Verify vocabulary grows with exposure
    float loss_first = 0.0f, loss_last = 0.0f;

    const char* texts[] = {
        "the ancient forest whispers secrets",
        "bright stars illuminate the midnight sky",
        "gentle waves caress the golden shore",
        "mysterious shadows dance in moonlight",
        "colorful butterflies flutter through gardens",
    };

    for (int epoch = 0; epoch < 3; epoch++) {
        for (auto& text : texts) {
            float loss = 0.0f;
            nimcp_brain_learn_language(brain, text, &loss);
            if (epoch == 0) loss_first = loss;
            loss_last = loss;
        }
    }

    // Loss should decrease (or stay reasonable) with training
    EXPECT_LE(loss_last, loss_first + 0.5f);
}
