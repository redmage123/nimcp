/**
 * @file e2e_test_quantum_language_processing.cpp
 * @brief End-to-end tests for quantum language processing pipelines
 *
 * Tests cover:
 * - Language production with Broca quantum bridge
 * - Lexical-semantic-phonological pipeline
 * - Quantum-accelerated word retrieval
 * - Syntax planning and optimization
 * - Integrated language generation
 * - Error recovery in language production
 *
 * @version Phase C2: Quantum Reasoning Integration
 * @date 2025-12-30
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

/* Implementation defines */
#define NIMCP_ATTENTION_QUANTUM_BRIDGE_IMPLEMENTATION
#define NIMCP_THALAMIC_QUANTUM_BRIDGE_IMPLEMENTATION
#define NIMCP_EXECUTIVE_QUANTUM_BRIDGE_IMPLEMENTATION

// Headers have their own extern "C" guards
#include "core/brain/regions/broca/nimcp_broca_quantum_bridge.h"
#include "cognitive/reasoning/nimcp_quantum_reasoning.h"
#include "plasticity/attention/nimcp_attention_quantum_bridge.h"
#include "middleware/routing/nimcp_thalamic_quantum_bridge.h"
#include "cognitive/executive/nimcp_executive_quantum_bridge.h"

//=============================================================================
// Test Fixture
//=============================================================================

class QuantumLanguageProcessingE2E : public ::testing::Test {
protected:
    broca_quantum_bridge_t* broca = nullptr;
    qreason_t reasoner = nullptr;
    attention_quantum_bridge_t* attention = nullptr;
    thalamic_quantum_bridge_t* thalamic = nullptr;
    executive_quantum_bridge_t* executive = nullptr;

    void SetUp() override {
        broca_quantum_config_t broca_config = broca_quantum_default_config();
        broca_config.lexicon_search_depth = 500;
        broca = broca_quantum_bridge_create(nullptr, &broca_config);

        qreason_config_t qr_config = qreason_default_config();
        reasoner = qreason_create(&qr_config);

        attention_quantum_config_t attn_config = attention_quantum_default_config();
        attention = attention_quantum_bridge_create(&attn_config);

        thalamic_quantum_config_t thal_config = thalamic_quantum_default_config();
        thalamic = thalamic_quantum_bridge_create(&thal_config);

        executive_quantum_config_t exec_config = executive_quantum_default_config();
        executive = executive_quantum_bridge_create(&exec_config);
    }

    void TearDown() override {
        if (broca) broca_quantum_bridge_destroy(broca);
        if (reasoner) qreason_destroy(reasoner);
        if (attention) attention_quantum_bridge_destroy(attention);
        if (thalamic) thalamic_quantum_bridge_destroy(thalamic);
        if (executive) executive_quantum_bridge_destroy(executive);
    }

    /**
     * @brief Generate semantic embedding for a concept
     */
    void generate_concept_embedding(float* embedding, uint32_t dim, const char* concept_str) {
        /* Simple hash-based embedding */
        uint32_t hash = 0;
        for (size_t i = 0; i < strlen(concept_str); i++) {
            hash = hash * 31 + concept_str[i];
        }

        for (uint32_t i = 0; i < dim; i++) {
            embedding[i] = sinf((float)hash + (float)i * 0.3f) * 0.5f + 0.5f;
        }
    }
};

//=============================================================================
// E2E Test Cases: Basic Language Production
//=============================================================================

TEST_F(QuantumLanguageProcessingE2E, WordRetrieval) {
    /**
     * SCENARIO: Retrieve a word from semantic concept
     *
     * 1. Generate semantic embedding for concept
     * 2. Search lexicon using quantum Grover
     * 3. Return best matching word
     */

    float semantic_embedding[8];
    generate_concept_embedding(semantic_embedding, 8, "happiness");

    quantum_lexical_result_t result;
    int ret = broca_quantum_search_lexicon(broca, semantic_embedding, 8, 200, &result);

    EXPECT_EQ(ret, 0);
    ASSERT_NE(result.best_candidate, nullptr);
    EXPECT_GT(result.candidates_evaluated, 0u);
    EXPECT_GT(result.search_speedup, 1.0f);
}

TEST_F(QuantumLanguageProcessingE2E, MultiWordRetrieval) {
    /**
     * SCENARIO: Retrieve multiple words for a sentence
     */

    const char* concepts[] = {"dog", "run", "fast", "park"};
    const int NUM_WORDS = 4;

    quantum_lexical_result_t results[NUM_WORDS];

    for (int i = 0; i < NUM_WORDS; i++) {
        float embedding[8];
        generate_concept_embedding(embedding, 8, concepts[i]);

        int ret = broca_quantum_search_lexicon(broca, embedding, 8, 150, &results[i]);
        EXPECT_EQ(ret, 0);
        EXPECT_NE(results[i].best_candidate, nullptr);
    }

    /* Verify statistics */
    broca_quantum_stats_t stats;
    broca_quantum_get_stats(broca, &stats);
    EXPECT_EQ(stats.lexical_searches, (uint64_t)NUM_WORDS);
}

TEST_F(QuantumLanguageProcessingE2E, SyntaxPlanning) {
    /**
     * SCENARIO: Plan sentence syntax
     *
     * 1. Given semantic content
     * 2. Find optimal syntactic structure
     * 3. Return arrangement with fluency score
     */

    quantum_syntax_result_t result;
    int ret = broca_quantum_optimize_syntax(broca, "The quick brown fox jumps", 5, 0.7f, &result);

    EXPECT_EQ(ret, 0);
    ASSERT_NE(result.best_structure, nullptr);
    EXPECT_TRUE(result.best_structure->is_grammatical);
    EXPECT_GT(result.best_structure->fluency_score, 0.0f);
}

TEST_F(QuantumLanguageProcessingE2E, PhonemeSequencing) {
    /**
     * SCENARIO: Optimize phoneme sequence for articulation
     */

    /* Sample phoneme sequence */
    uint8_t phonemes[] = {10, 22, 15, 8, 33, 20, 12, 5};
    quantum_phoneme_result_t result;

    int ret = broca_quantum_optimize_phonemes(broca, phonemes, 8, &result);

    EXPECT_EQ(ret, 0);
    ASSERT_NE(result.best_sequence, nullptr);
    EXPECT_GE(result.best_sequence->coarticulation_score, 0.0f);
    EXPECT_LE(result.best_sequence->coarticulation_score, 1.0f);
}

//=============================================================================
// E2E Test Cases: Full Language Pipeline
//=============================================================================

TEST_F(QuantumLanguageProcessingE2E, SemanticToSpeechPipeline) {
    /**
     * SCENARIO: Full semantic-to-speech pipeline
     *
     * 1. Semantic concept activation
     * 2. Lexical retrieval (Grover search)
     * 3. Syntax optimization
     * 4. Phoneme sequencing
     */

    /* Step 1: Semantic concept */
    float semantic_target[8];
    generate_concept_embedding(semantic_target, 8, "communicate");

    /* Step 2: Lexical retrieval */
    quantum_lexical_result_t lex_result;
    int ret = broca_quantum_search_lexicon(broca, semantic_target, 8, 300, &lex_result);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(lex_result.best_candidate, nullptr);

    /* Step 3: Syntax optimization */
    quantum_syntax_result_t syn_result;
    ret = broca_quantum_optimize_syntax(broca, "Selected word in context", 4, 0.6f, &syn_result);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(syn_result.best_structure, nullptr);

    /* Step 4: Phoneme sequencing */
    uint8_t phonemes[] = {15, 22, 10, 33, 20, 8};
    quantum_phoneme_result_t phon_result;
    ret = broca_quantum_optimize_phonemes(broca, phonemes, 6, &phon_result);
    ASSERT_EQ(ret, 0);
    ASSERT_NE(phon_result.best_sequence, nullptr);

    /* Verify complete pipeline */
    broca_quantum_stats_t stats;
    broca_quantum_get_stats(broca, &stats);
    EXPECT_EQ(stats.lexical_searches, 1u);
    EXPECT_EQ(stats.syntax_optimizations, 1u);
    EXPECT_EQ(stats.phoneme_optimizations, 1u);
}

TEST_F(QuantumLanguageProcessingE2E, AttentionGuidedWordSelection) {
    /**
     * SCENARIO: Attention guides word selection
     *
     * 1. Retrieve multiple word candidates
     * 2. Score candidates with attention
     * 3. Select best candidate based on attention
     */

    /* Retrieve candidates for different concepts */
    const int NUM_CANDIDATES = 4;
    quantum_lexical_result_t candidates[NUM_CANDIDATES];

    const char* concepts[] = {"happy", "joyful", "content", "pleased"};
    for (int i = 0; i < NUM_CANDIDATES; i++) {
        float embedding[6];
        generate_concept_embedding(embedding, 6, concepts[i]);
        broca_quantum_search_lexicon(broca, embedding, 6, 100, &candidates[i]);
    }

    /* Score candidates */
    float attention_scores[NUM_CANDIDATES];
    for (int i = 0; i < NUM_CANDIDATES; i++) {
        if (candidates[i].best_candidate) {
            attention_scores[i] = candidates[i].best_candidate->combined_score;
        } else {
            attention_scores[i] = 0.0f;
        }
    }

    /* Attention selection */
    uint32_t selected[NUM_CANDIDATES];
    int n_selected = attention_quantum_select_heads(attention, attention_scores, NUM_CANDIDATES, 1, selected);

    EXPECT_GE(n_selected, 0);
}

TEST_F(QuantumLanguageProcessingE2E, ThalamicRoutedLanguage) {
    /**
     * SCENARIO: Thalamic routing for language signals
     *
     * 1. Generate language production signal
     * 2. Thalamus routes to appropriate modules
     * 3. Broca processes routed signal
     */

    /* Generate signal features from semantic content */
    float signal_features[8];
    generate_concept_embedding(signal_features, 8, "utterance");

    /* Thalamic routing decision */
    uint32_t modules[] = {1, 2, 3, 4, 5};  /* Broca, Wernicke, Motor, Memory, Executive */
    uint32_t routed[5];
    uint32_t num_routed = 0;

    int ret = thalamic_quantum_route(thalamic, 0, modules, 5, signal_features, 8, routed, &num_routed);
    EXPECT_GE(ret, 0);

    /* If Broca module (1) was selected, process language */
    bool broca_selected = false;
    for (uint32_t i = 0; i < num_routed; i++) {
        if (routed[i] == 1) {
            broca_selected = true;
            break;
        }
    }

    if (broca_selected || num_routed == 0) {
        /* Process in Broca */
        quantum_lexical_result_t result;
        broca_quantum_search_lexicon(broca, signal_features, 8, 100, &result);
        EXPECT_NE(result.best_candidate, nullptr);
    }
}

//=============================================================================
// E2E Test Cases: Complex Language Scenarios
//=============================================================================

TEST_F(QuantumLanguageProcessingE2E, SentenceGeneration) {
    /**
     * SCENARIO: Generate a complete sentence
     *
     * 1. Plan sentence structure
     * 2. Retrieve words for each slot
     * 3. Optimize phoneme sequences
     * 4. Produce complete utterance
     */

    /* Plan syntax for 5-word sentence */
    quantum_syntax_result_t syn_result;
    broca_quantum_optimize_syntax(broca, "subject verb object modifier end", 5, 0.8f, &syn_result);
    ASSERT_NE(syn_result.best_structure, nullptr);

    /* Retrieve words for each syntactic slot */
    const char* slot_concepts[] = {"person", "action", "thing", "description", "punctuation"};
    const int NUM_SLOTS = 5;

    std::vector<quantum_lexical_result_t> words(NUM_SLOTS);
    for (int i = 0; i < NUM_SLOTS; i++) {
        float embedding[6];
        generate_concept_embedding(embedding, 6, slot_concepts[i]);
        broca_quantum_search_lexicon(broca, embedding, 6, 80, &words[i]);
    }

    /* Optimize phonemes for complete utterance */
    uint8_t full_phoneme_sequence[32];
    for (int i = 0; i < 32; i++) {
        full_phoneme_sequence[i] = (uint8_t)((i * 7 + 3) % 50);
    }

    quantum_phoneme_result_t phon_result;
    broca_quantum_optimize_phonemes(broca, full_phoneme_sequence, 32, &phon_result);

    /* Verify complete generation */
    broca_quantum_stats_t stats;
    broca_quantum_get_stats(broca, &stats);
    EXPECT_EQ(stats.lexical_searches, (uint64_t)NUM_SLOTS);
    EXPECT_EQ(stats.syntax_optimizations, 1u);
    EXPECT_EQ(stats.phoneme_optimizations, 1u);
}

TEST_F(QuantumLanguageProcessingE2E, DialogueTurn) {
    /**
     * SCENARIO: Generate a dialogue response
     *
     * 1. Understand input (attention)
     * 2. Reason about response (reasoning)
     * 3. Generate response (Broca)
     */

    /* Simulate understanding input */
    float input_features[] = {0.8f, 0.6f, 0.4f, 0.3f};
    uint32_t selected_features[4];
    attention_quantum_select_heads(attention, input_features, 4, 2, selected_features);

    /* Reason about appropriate response */
    qreason_set_fact(reasoner, 0, QREASON_TRUE, 0.9f);  /* Question detected */
    qreason_set_fact(reasoner, 1, QREASON_TRUE, 0.8f);  /* Topic understood */

    uint32_t ant[] = {0, 1};
    qreason_add_rule(reasoner, ant, 2, 2, 0.9f);  /* Can answer */

    qreason_result_t reason_result;
    qreason_forward_chain(reasoner, &reason_result);

    float answer_conf;
    qreason_truth_t can_answer = qreason_get_fact(reasoner, 2, &answer_conf);

    /* Generate response if reasoning permits */
    if (can_answer == QREASON_TRUE) {
        /* Generate semantically appropriate response */
        float response_embedding[6];
        generate_concept_embedding(response_embedding, 6, "answer");

        quantum_lexical_result_t lex_result;
        broca_quantum_search_lexicon(broca, response_embedding, 6, 100, &lex_result);

        quantum_syntax_result_t syn_result;
        broca_quantum_optimize_syntax(broca, "Response content", 3, 0.6f, &syn_result);

        EXPECT_NE(lex_result.best_candidate, nullptr);
        EXPECT_NE(syn_result.best_structure, nullptr);
    }
}

TEST_F(QuantumLanguageProcessingE2E, ErrorRecoveryInProduction) {
    /**
     * SCENARIO: Recover from production error
     *
     * 1. Attempt word retrieval
     * 2. If low confidence, try alternative
     * 3. Select best available option
     */

    /* First attempt - might have low confidence */
    float embedding1[6] = {0.9f, 0.1f, 0.8f, 0.2f, 0.7f, 0.3f};
    quantum_lexical_result_t result1;
    broca_quantum_search_lexicon(broca, embedding1, 6, 100, &result1);

    /* Check if we need recovery */
    bool needs_recovery = (result1.best_candidate == nullptr ||
                          result1.satisfaction_probability < 0.5f);

    if (needs_recovery) {
        /* Try alternative semantic target */
        float embedding2[6] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        quantum_lexical_result_t result2;
        broca_quantum_search_lexicon(broca, embedding2, 6, 150, &result2);

        EXPECT_NE(result2.best_candidate, nullptr);
    }

    /* Verify recovery attempts tracked */
    broca_quantum_stats_t stats;
    broca_quantum_get_stats(broca, &stats);
    EXPECT_GE(stats.lexical_searches, 1u);
}

//=============================================================================
// E2E Test Cases: Executive-Guided Language
//=============================================================================

TEST_F(QuantumLanguageProcessingE2E, ExecutiveDecisionInLanguage) {
    /**
     * SCENARIO: Executive function decides language style
     *
     * 1. Present style options to executive
     * 2. Executive selects style
     * 3. Generate language with selected style
     */

    /* Style decision */
    decision_option_t style_options[3] = {
        {.option_id = 0, .expected_reward = 0.7f, .risk_level = 0.1f},
        {.option_id = 1, .expected_reward = 0.8f, .risk_level = 0.2f},
        {.option_id = 2, .expected_reward = 0.6f, .risk_level = 0.3f}
    };
    strncpy(style_options[0].description, "Formal", sizeof(style_options[0].description));
    strncpy(style_options[1].description, "Casual", sizeof(style_options[1].description));
    strncpy(style_options[2].description, "Technical", sizeof(style_options[2].description));

    quantum_decision_result_t decision;
    executive_quantum_evaluate_options(executive, style_options, 3, &decision);

    /* Generate content based on selected style */
    float style_embedding[6];
    switch (decision.selected_option_id) {
        case 0: generate_concept_embedding(style_embedding, 6, "formal"); break;
        case 1: generate_concept_embedding(style_embedding, 6, "casual"); break;
        case 2: generate_concept_embedding(style_embedding, 6, "technical"); break;
        default: generate_concept_embedding(style_embedding, 6, "neutral"); break;
    }

    quantum_lexical_result_t lex_result;
    broca_quantum_search_lexicon(broca, style_embedding, 6, 120, &lex_result);

    EXPECT_NE(lex_result.best_candidate, nullptr);
}

//=============================================================================
// E2E Test Cases: Performance and Stress
//=============================================================================

TEST_F(QuantumLanguageProcessingE2E, HighThroughputProduction) {
    /**
     * SCENARIO: High-throughput language production
     */

    const int NUM_UTTERANCES = 30;

    for (int u = 0; u < NUM_UTTERANCES; u++) {
        /* Generate utterance-specific embedding */
        float embedding[6];
        for (int i = 0; i < 6; i++) {
            embedding[i] = sinf((float)u * 0.2f + (float)i * 0.3f) * 0.5f + 0.5f;
        }

        quantum_lexical_result_t lex_result;
        broca_quantum_search_lexicon(broca, embedding, 6, 80, &lex_result);

        quantum_syntax_result_t syn_result;
        broca_quantum_optimize_syntax(broca, "Content", 2, 0.5f, &syn_result);

        uint8_t phonemes[8] = {(uint8_t)u, 10, 20, 30, 40, 50, 60, 70};
        quantum_phoneme_result_t phon_result;
        broca_quantum_optimize_phonemes(broca, phonemes, 8, &phon_result);
    }

    broca_quantum_stats_t stats;
    broca_quantum_get_stats(broca, &stats);
    EXPECT_EQ(stats.lexical_searches, (uint64_t)NUM_UTTERANCES);
    EXPECT_EQ(stats.syntax_optimizations, (uint64_t)NUM_UTTERANCES);
    EXPECT_EQ(stats.phoneme_optimizations, (uint64_t)NUM_UTTERANCES);
}

TEST_F(QuantumLanguageProcessingE2E, IntegratedCognitiveCycle) {
    /**
     * SCENARIO: Integrated cognitive cycle with language
     *
     * Simulates: Perceive -> Reason -> Decide -> Speak
     */

    const int NUM_CYCLES = 10;

    for (int cycle = 0; cycle < NUM_CYCLES; cycle++) {
        /* 1. Perceive (attention) */
        float percepts[4] = {0.7f + 0.1f * (float)cycle, 0.5f, 0.3f, 0.9f};
        uint32_t selected[4];
        attention_quantum_select_heads(attention, percepts, 4, 2, selected);

        /* 2. Reason */
        qreason_clear_facts(reasoner);
        for (int i = 0; i < 2; i++) {
            if (selected[i] < 4) {
                qreason_set_fact(reasoner, selected[i], QREASON_TRUE, percepts[selected[i]]);
            }
        }

        /* 3. Decide what to say */
        decision_option_t options[2] = {
            {.option_id = 0, .expected_reward = 0.7f, .risk_level = 0.2f},
            {.option_id = 1, .expected_reward = 0.5f, .risk_level = 0.1f}
        };
        strncpy(options[0].description, "Comment", sizeof(options[0].description));
        strncpy(options[1].description, "Silent", sizeof(options[1].description));

        quantum_decision_result_t decision;
        executive_quantum_evaluate_options(executive, options, 2, &decision);

        /* 4. Speak (if decided) */
        if (decision.selected_option_id == 0) {
            float embedding[4];
            for (int i = 0; i < 4; i++) {
                embedding[i] = percepts[i];
            }

            quantum_lexical_result_t lex_result;
            broca_quantum_search_lexicon(broca, embedding, 4, 60, &lex_result);
        }
    }

    /* Verify all systems processed */
    attention_quantum_stats_t attn_stats;
    attention_quantum_get_stats(attention, &attn_stats);
    EXPECT_EQ(attn_stats.quantum_selections + attn_stats.classical_fallbacks, (uint64_t)NUM_CYCLES);
}

//=============================================================================
// E2E Test Cases: Resilience
//=============================================================================

TEST_F(QuantumLanguageProcessingE2E, RecoveryAfterDisable) {
    /**
     * SCENARIO: System recovers after quantum disabled/re-enabled
     */

    /* Normal operation */
    float embedding[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    quantum_lexical_result_t result1;
    broca_quantum_search_lexicon(broca, embedding, 4, 50, &result1);
    EXPECT_NE(result1.best_candidate, nullptr);

    /* Disable and try (should fail) */
    broca_quantum_bridge_set_enabled(broca, false);
    quantum_lexical_result_t result2;
    int ret = broca_quantum_search_lexicon(broca, embedding, 4, 50, &result2);
    EXPECT_EQ(ret, -1);

    /* Re-enable and verify recovery */
    broca_quantum_bridge_set_enabled(broca, true);
    quantum_lexical_result_t result3;
    broca_quantum_search_lexicon(broca, embedding, 4, 50, &result3);
    EXPECT_NE(result3.best_candidate, nullptr);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
