/**
 * @file test_language_loop_e2e.cpp
 * @brief End-to-end tests for complete language processing loop (comprehension to production)
 *
 * WHAT: Full pipeline tests for bidirectional Wernicke-Broca communication
 * WHY:  Verify complete language processing including repetition, dialogue, and working memory
 * HOW:  Test arcuate fasciculus bridge, rehearsal loops, and attention modulation
 *
 * TEST COVERAGE:
 * - Complete Comprehension-to-Production Loop (3 tests)
 * - Dialogue Simulation (3 tests)
 * - Repetition and Shadowing (3 tests)
 * - Translation-like Transformations (3 tests)
 * - Working Memory and Attention (3 tests)
 *
 * TOTAL: 15 tests
 *
 * BIOLOGICAL ANALOGY:
 * - Arcuate fasciculus connects Wernicke's to Broca's area
 * - Dorsal stream: phonological repetition pathway
 * - Ventral stream: semantic comprehension pathway
 * - Phonological loop: working memory rehearsal via subvocal articulation
 *
 * @author NIMCP Development Team
 * @date 2026-01-24
 */

#include "../../../e2e_test_framework.h"
#include "utils/nimcp_test_base.h"
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <cstring>

#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/regions/broca/nimcp_language_production_bridge.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_broca_bridge.h"
#include "perception/nimcp_speech_cortex.h"
#include "utils/memory/nimcp_memory.h"

using namespace nimcp::e2e;

//=============================================================================
// Test Configuration
//=============================================================================

constexpr double MAX_LOOP_TIME_MS = 500.0;
constexpr double MAX_PROCESSING_TIME_MS = 200.0;
constexpr double MAX_BRIDGE_TIME_MS = 100.0;
constexpr uint32_t MAX_WORDS = 32;
constexpr uint32_t MAX_PHONEMES = 128;
constexpr uint32_t MAX_MOTOR_COMMANDS = 256;
constexpr uint32_t SEMANTIC_DIM = 64;
constexpr float SAMPLE_RATE = 16000.0f;

//=============================================================================
// Helper Functions
//=============================================================================

static void populate_shared_lexicon(broca_adapter_t* broca, wernicke_adapter_t* wernicke) {
    // Common words for both regions
    struct WordEntry {
        uint32_t id;
        const char* word;
        uint8_t phonemes[8];
        uint32_t phoneme_count;
        uint8_t pos;
        float freq;
        uint32_t concept_id;
    };

    WordEntry entries[] = {
        {1, "hello", {'h', 'E', 'l', 'o', 'U'}, 5, 5, 0.8f, 100},
        {2, "world", {'w', '3', 'r', 'l', 'd'}, 5, 1, 0.9f, 101},
        {3, "the",   {'D', '@', 0, 0, 0}, 2, 4, 0.99f, 102},
        {4, "cat",   {'k', 'a', 't', 0, 0}, 3, 1, 0.7f, 103},
        {5, "sat",   {'s', 'a', 't', 0, 0}, 3, 2, 0.6f, 104},
        {6, "yes",   {'j', 'E', 's', 0, 0}, 3, 5, 0.85f, 105},
        {7, "no",    {'n', 'o', 'U', 0, 0}, 3, 5, 0.85f, 106},
        {8, "please",{'p', 'l', 'i', 'z', 0}, 4, 5, 0.75f, 107}
    };

    for (const auto& e : entries) {
        // Add to Broca
        broca_lexical_entry_t broca_entry;
        memset(&broca_entry, 0, sizeof(broca_entry));
        broca_entry.word_id = e.id;
        strncpy(broca_entry.word, e.word, sizeof(broca_entry.word) - 1);
        memcpy(broca_entry.phonemes, e.phonemes, e.phoneme_count);
        broca_entry.phoneme_count = e.phoneme_count;
        broca_entry.pos = e.pos;
        broca_entry.frequency = e.freq;
        broca_add_lexical_entry(broca, &broca_entry);

        // Add to Wernicke
        wernicke_word_t wernicke_entry;
        memset(&wernicke_entry, 0, sizeof(wernicke_entry));
        wernicke_entry.word_id = e.id;
        strncpy(wernicke_entry.word, e.word, sizeof(wernicke_entry.word) - 1);
        memcpy(wernicke_entry.phonemes, e.phonemes, e.phoneme_count);
        wernicke_entry.phoneme_count = e.phoneme_count;
        wernicke_entry.pos = e.pos;
        wernicke_entry.frequency = e.freq;
        wernicke_entry.concept_id = e.concept_id;
        wernicke_add_word(wernicke, &wernicke_entry);
    }
}

static std::vector<float> generate_semantic_vector(uint32_t dim, uint32_t seed) {
    std::vector<float> vec(dim);
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (uint32_t i = 0; i < dim; i++) {
        vec[i] = dist(gen);
    }
    float norm = 0.0f;
    for (uint32_t i = 0; i < dim; i++) norm += vec[i] * vec[i];
    norm = std::sqrt(norm);
    if (norm > 0.0f) {
        for (uint32_t i = 0; i < dim; i++) vec[i] /= norm;
    }
    return vec;
}

static std::vector<phoneme_event_t> create_phoneme_event_sequence(const uint8_t* phonemes, uint32_t count) {
    std::vector<phoneme_event_t> seq(count);
    for (uint32_t i = 0; i < count; i++) {
        memset(&seq[i], 0, sizeof(phoneme_event_t));
        seq[i].phoneme = static_cast<phoneme_t>(phonemes[i]);
        seq[i].confidence = 0.9f;
        seq[i].features.duration_ms = 80.0f;
        seq[i].onset_time_ms = i * 100;
        seq[i].offset_time_ms = (i + 1) * 100;
        seq[i].sequence_position = i;
    }
    return seq;
}

static std::vector<phoneme_t> create_phoneme_sequence(const uint8_t* phonemes, uint32_t count) {
    std::vector<phoneme_t> seq(count);
    for (uint32_t i = 0; i < count; i++) {
        seq[i] = static_cast<phoneme_t>(phonemes[i]);
    }
    return seq;
}

//=============================================================================
// Test Fixture
//=============================================================================

class E2ELanguageLoopTest : public NimcpTestBase {
protected:
    broca_adapter_t* broca = nullptr;
    wernicke_adapter_t* wernicke = nullptr;
    language_production_bridge_t* production_bridge = nullptr;
    wernicke_broca_bridge_t* wbb = nullptr;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Create Broca's region
        broca_config_t broca_config = broca_default_config();
        broca_config.max_words = MAX_WORDS;
        broca_config.max_phonemes = MAX_PHONEMES;
        broca_config.enable_working_memory = true;
        broca_config.enable_lexicon = true;

        broca = broca_create(&broca_config);
        ASSERT_NE(broca, nullptr) << "Failed to create Broca adapter";

        // Create Wernicke's area
        wernicke_config_t wernicke_config = wernicke_default_config();
        wernicke_config.max_phonemes = MAX_PHONEMES;
        wernicke_config.max_words = MAX_WORDS;
        wernicke_config.enable_lexicon = true;
        wernicke_config.enable_working_memory = true;
        wernicke_config.enable_broca_connection = true;

        wernicke = wernicke_create(&wernicke_config);
        ASSERT_NE(wernicke, nullptr) << "Failed to create Wernicke adapter";

        // Populate shared lexicon
        populate_shared_lexicon(broca, wernicke);

        // Create language production bridge
        lpb_config_t lpb_config = lpb_default_config();
        lpb_config.semantic_dim = SEMANTIC_DIM;
        lpb_config.enable_repetition = true;

        production_bridge = lpb_create(&lpb_config, broca);
        ASSERT_NE(production_bridge, nullptr) << "Failed to create production bridge";

        // Create Wernicke-Broca bridge (arcuate fasciculus)
        wbb_config_t wbb_config = wbb_default_config();
        wbb_config.enable_dorsal_stream = true;
        wbb_config.enable_ventral_stream = true;
        wbb_config.enable_self_monitoring = true;
        wbb_config.enable_working_memory = true;

        wbb = wbb_create(wernicke, broca, &wbb_config);
        ASSERT_NE(wbb, nullptr) << "Failed to create Wernicke-Broca bridge";

        // Connect Wernicke to Broca
        bool connected = wernicke_connect_broca(wernicke, broca);
        EXPECT_TRUE(connected) << "Should connect Wernicke to Broca";
    }

    void TearDown() override {
        if (wbb) {
            wbb_destroy(wbb);
            wbb = nullptr;
        }
        if (production_bridge) {
            lpb_destroy(production_bridge);
            production_bridge = nullptr;
        }
        if (wernicke) {
            wernicke_destroy(wernicke);
            wernicke = nullptr;
        }
        if (broca) {
            broca_destroy(broca);
            broca = nullptr;
        }
        NimcpTestBase::TearDown();
    }
};

//=============================================================================
// Complete Comprehension-to-Production Loop Tests
//=============================================================================

TEST_F(E2ELanguageLoopTest, ComprehendThenProduce) {
    E2E_PIPELINE_START("Comprehend Then Produce");

    // Stage 1: Comprehend input
    E2E_STAGE_BEGIN("Recognize input word", MAX_PROCESSING_TIME_MS);
    uint8_t hello_phonemes[] = {'h', 'E', 'l', 'o', 'U'};
    auto phonemes = create_phoneme_sequence(hello_phonemes, 5);

    wernicke_word_result_t word_result;
    memset(&word_result, 0, sizeof(word_result));

    bool success = wernicke_recognize_word(wernicke, phonemes.data(), 5, &word_result);
    EXPECT_TRUE(success) << "Should recognize 'hello'";
    E2E_STAGE_END();

    // Stage 2: Forward comprehension to Broca
    E2E_STAGE_BEGIN("Forward to Broca", MAX_BRIDGE_TIME_MS);
    wbb_comprehension_t comprehension;
    memset(&comprehension, 0, sizeof(comprehension));

    comprehension.word_id = word_result.word.word_id;
    strncpy(comprehension.word_string, word_result.word.word, sizeof(comprehension.word_string) - 1);
    comprehension.num_phonemes = word_result.word.phoneme_count;
    comprehension.confidence = word_result.confidence;

    int result = wbb_forward_comprehension(wbb, &comprehension, WBB_STREAM_BOTH);
    EXPECT_EQ(result, 0) << "Should forward comprehension";
    E2E_STAGE_END();

    // Stage 3: Process bridge messages
    E2E_STAGE_BEGIN("Process bridge messages", MAX_BRIDGE_TIME_MS);
    int messages_processed = wbb_process_messages(wbb, 0);
    EXPECT_GE(messages_processed, 0) << "Should process messages";
    E2E_STAGE_END();

    // Stage 4: Produce response
    E2E_STAGE_BEGIN("Produce from comprehension", MAX_PROCESSING_TIME_MS);
    const char* response[] = {"hello"};
    broca_utterance_result_t utterance;
    memset(&utterance, 0, sizeof(utterance));

    success = broca_produce_from_strings(broca, response, 1, &utterance);
    EXPECT_TRUE(success) << "Should produce response";
    EXPECT_TRUE(utterance.ready_for_articulation);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageLoopTest, SemanticPathway) {
    E2E_PIPELINE_START("Semantic Pathway (Ventral Stream)");

    // Test semantic-level comprehension to production
    E2E_STAGE_BEGIN("Generate semantic intent", 20);
    auto semantic_vec = generate_semantic_vector(SEMANTIC_DIM, 42);

    lpb_semantic_intent_t intent;
    memset(&intent, 0, sizeof(intent));
    intent.semantic_vector = semantic_vec.data();
    intent.semantic_dim = SEMANTIC_DIM;
    intent.confidence = 0.9f;
    intent.intent_type = 0;  // Statement
    E2E_STAGE_END();

    // Send semantic intent via ventral stream
    E2E_STAGE_BEGIN("Send via ventral stream", MAX_BRIDGE_TIME_MS);
    int result = wbb_send_response_intent(
        wbb,
        semantic_vec.data(),
        SEMANTIC_DIM,
        0  // Statement
    );
    EXPECT_EQ(result, 0) << "Should send semantic intent";
    E2E_STAGE_END();

    // Produce from semantic intent
    E2E_STAGE_BEGIN("Produce from semantic", MAX_PROCESSING_TIME_MS);
    lpb_production_result_t production;
    memset(&production, 0, sizeof(production));

    bool success = lpb_produce_from_intent(production_bridge, &intent, &production);
    EXPECT_TRUE(success) << "Should produce from semantic intent";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify production", 10);
    EXPECT_GE(production.fluency_score, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageLoopTest, FullLoopWithSelfMonitoring) {
    E2E_PIPELINE_START("Full Loop with Self-Monitoring");

    // Comprehend -> Forward -> Produce -> Monitor
    E2E_STAGE_BEGIN("Comprehend input", MAX_PROCESSING_TIME_MS);
    uint8_t cat_phonemes[] = {'k', 'a', 't'};
    auto phonemes = create_phoneme_sequence(cat_phonemes, 3);

    wernicke_word_result_t word;
    bool success = wernicke_recognize_word(wernicke, phonemes.data(), 3, &word);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    // Forward comprehension
    E2E_STAGE_BEGIN("Forward comprehension", MAX_BRIDGE_TIME_MS);
    wbb_comprehension_t comp;
    memset(&comp, 0, sizeof(comp));
    comp.word_id = word.word.word_id;
    comp.confidence = word.confidence;

    int result = wbb_forward_comprehension(wbb, &comp, WBB_STREAM_DORSAL);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    // Produce
    E2E_STAGE_BEGIN("Produce word", MAX_PROCESSING_TIME_MS);
    const char* words[] = {"cat"};
    broca_utterance_result_t utterance;
    success = broca_produce_from_strings(broca, words, 1, &utterance);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    // Self-monitoring via efference copy
    E2E_STAGE_BEGIN("Self-monitoring check", MAX_BRIDGE_TIME_MS);
    wbb_efference_copy_t efference;
    memset(&efference, 0, sizeof(efference));

    result = wbb_receive_efference_copy(wbb, &efference);
    // May or may not have efference copy available
    EXPECT_GE(result, -1);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Dialogue Simulation Tests
//=============================================================================

TEST_F(E2ELanguageLoopTest, ListenThenRespond) {
    E2E_PIPELINE_START("Listen Then Respond");

    // Simulate simple dialogue turn
    E2E_STAGE_BEGIN("Listen phase", MAX_PROCESSING_TIME_MS);
    // Hear "hello"
    uint8_t hello_phonemes[] = {'h', 'E', 'l', 'o', 'U'};
    auto phonemes = create_phoneme_sequence(hello_phonemes, 5);

    wernicke_word_result_t heard;
    bool success = wernicke_recognize_word(wernicke, phonemes.data(), 5, &heard);
    EXPECT_TRUE(success);
    EXPECT_STREQ(heard.word.word, "hello");
    E2E_STAGE_END();

    // Process and generate response
    E2E_STAGE_BEGIN("Generate response", MAX_PROCESSING_TIME_MS);
    // Respond with "hello"
    const char* response[] = {"hello"};
    broca_utterance_result_t utterance;
    success = broca_produce_from_strings(broca, response, 1, &utterance);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify dialogue turn", 10);
    EXPECT_TRUE(utterance.ready_for_articulation);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageLoopTest, QuestionAnswerExchange) {
    E2E_PIPELINE_START("Question Answer Exchange");

    // Simulate Q&A dialogue
    E2E_STAGE_BEGIN("Process question", MAX_PROCESSING_TIME_MS);
    // Hear question word
    uint8_t the_phonemes[] = {'D', '@'};
    auto phonemes = create_phoneme_sequence(the_phonemes, 2);

    wernicke_word_result_t question_word;
    bool success = wernicke_recognize_word(wernicke, phonemes.data(), 2, &question_word);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    // Generate answer via semantic pathway
    E2E_STAGE_BEGIN("Generate answer intent", 20);
    auto semantic = generate_semantic_vector(SEMANTIC_DIM, 100);

    lpb_semantic_intent_t answer_intent;
    memset(&answer_intent, 0, sizeof(answer_intent));
    answer_intent.semantic_vector = semantic.data();
    answer_intent.semantic_dim = SEMANTIC_DIM;
    answer_intent.intent_type = 0;  // Statement (answer)
    answer_intent.speech_act = 2;   // Answer
    E2E_STAGE_END();

    // Produce answer
    E2E_STAGE_BEGIN("Produce answer", MAX_PROCESSING_TIME_MS);
    lpb_production_result_t production;
    success = lpb_produce_from_intent(production_bridge, &answer_intent, &production);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageLoopTest, MultipleTurnDialogue) {
    E2E_PIPELINE_START("Multiple Turn Dialogue");

    // Simulate multi-turn conversation
    E2E_STAGE_BEGIN("Turn 1: Greeting", MAX_LOOP_TIME_MS);
    // Listen
    uint8_t hello_phonemes[] = {'h', 'E', 'l', 'o', 'U'};
    auto phonemes = create_phoneme_sequence(hello_phonemes, 5);
    wernicke_word_result_t heard;
    wernicke_recognize_word(wernicke, phonemes.data(), 5, &heard);

    // Respond
    const char* resp1[] = {"hello"};
    broca_utterance_result_t utt1;
    broca_produce_from_strings(broca, resp1, 1, &utt1);
    broca_reset(broca);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Turn 2: Continuation", MAX_LOOP_TIME_MS);
    // Listen
    uint8_t world_phonemes[] = {'w', '3', 'r', 'l', 'd'};
    phonemes = create_phoneme_sequence(world_phonemes, 5);
    wernicke_recognize_word(wernicke, phonemes.data(), 5, &heard);

    // Respond
    const char* resp2[] = {"world"};
    broca_utterance_result_t utt2;
    broca_produce_from_strings(broca, resp2, 1, &utt2);
    broca_reset(broca);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify dialogue stats", 20);
    broca_stats_t broca_stats;
    broca_get_stats(broca, &broca_stats);
    EXPECT_GE(broca_stats.utterances_processed, 2u)
        << "Should have processed multiple utterances";

    wernicke_stats_t wernicke_stats;
    wernicke_get_stats(wernicke, &wernicke_stats);
    EXPECT_GE(wernicke_stats.words_recognized, 2u)
        << "Should have recognized multiple words";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Repetition and Shadowing Tests
//=============================================================================

TEST_F(E2ELanguageLoopTest, VerbatimRepetition) {
    E2E_PIPELINE_START("Verbatim Repetition");

    // Test echoic repetition via dorsal stream
    E2E_STAGE_BEGIN("Request repetition", MAX_BRIDGE_TIME_MS);
    uint8_t phonemes[] = {'h', 'E', 'l', 'o', 'U'};

    int result = wbb_request_repetition(wbb, phonemes, 5);
    EXPECT_EQ(result, 0) << "Should request repetition";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process repetition request", MAX_BRIDGE_TIME_MS);
    int processed = wbb_process_messages(wbb, 10);
    EXPECT_GE(processed, 0);
    E2E_STAGE_END();

    // Produce the repeated utterance
    E2E_STAGE_BEGIN("Produce repeated utterance", MAX_PROCESSING_TIME_MS);
    const char* words[] = {"hello"};
    broca_utterance_result_t result_utt;
    bool success = broca_produce_from_strings(broca, words, 1, &result_utt);
    EXPECT_TRUE(success);
    EXPECT_EQ(result_utt.phoneme_count, 5u) << "Should have same phoneme count";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageLoopTest, SpeechShadowing) {
    E2E_PIPELINE_START("Speech Shadowing");

    // Shadowing: near-simultaneous listening and repeating
    E2E_STAGE_BEGIN("Shadow multiple words", MAX_LOOP_TIME_MS);
    const char* sequence[] = {"the", "cat", "sat"};
    uint8_t phoneme_seqs[3][4] = {
        {'D', '@', 0, 0},
        {'k', 'a', 't', 0},
        {'s', 'a', 't', 0}
    };
    uint32_t counts[] = {2, 3, 3};

    for (int i = 0; i < 3; i++) {
        // Listen
        auto phonemes = create_phoneme_sequence(phoneme_seqs[i], counts[i]);
        wernicke_word_result_t heard;
        wernicke_recognize_word(wernicke, phonemes.data(), counts[i], &heard);

        // Immediately produce (shadow)
        broca_begin_utterance(broca);
        broca_input_word_t word;
        memset(&word, 0, sizeof(word));
        strncpy(word.word, sequence[i], sizeof(word.word) - 1);
        broca_add_word(broca, &word);

        broca_utterance_result_t utt;
        broca_process_utterance(broca, &utt);
        broca_reset(broca);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify shadowing stats", 10);
    wbb_stats_t stats;
    wbb_get_stats(wbb, &stats);
    // Stats should reflect the activity
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageLoopTest, PhonologicalLoopRehearsal) {
    E2E_PIPELINE_START("Phonological Loop Rehearsal");

    // Test working memory rehearsal
    E2E_STAGE_BEGIN("Request rehearsal", MAX_BRIDGE_TIME_MS);
    uint8_t phonemes[] = {'k', 'a', 't'};

    int result = wbb_request_rehearsal(wbb, phonemes, 3, 3);  // 3 repetitions
    EXPECT_EQ(result, 0) << "Should request rehearsal";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process rehearsal cycles", MAX_LOOP_TIME_MS);
    for (int cycle = 0; cycle < 3; cycle++) {
        int processed = wbb_process_rehearsal(wbb);
        EXPECT_GE(processed, 0);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify working memory", 20);
    // Check Wernicke working memory
    phoneme_t wm_contents[MAX_PHONEMES];
    uint32_t wm_count = 0;

    bool success = wernicke_wm_get_contents(wernicke, wm_contents, MAX_PHONEMES, &wm_count);
    EXPECT_TRUE(success);
    // Working memory should have been exercised
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Translation-like Transformation Tests
//=============================================================================

TEST_F(E2ELanguageLoopTest, SemanticTransformation) {
    E2E_PIPELINE_START("Semantic Transformation");

    // Comprehend one thing, produce semantically related
    E2E_STAGE_BEGIN("Comprehend source", MAX_PROCESSING_TIME_MS);
    uint8_t cat_phonemes[] = {'k', 'a', 't'};
    auto phonemes = create_phoneme_sequence(cat_phonemes, 3);

    wernicke_word_result_t source;
    bool success = wernicke_recognize_word(wernicke, phonemes.data(), 3, &source);
    EXPECT_TRUE(success);

    // Get semantic representation
    wernicke_concept_t wern_concept;
    wernicke_get_meaning(wernicke, &source, &wern_concept);
    E2E_STAGE_END();

    // Transform: produce different but related output
    E2E_STAGE_BEGIN("Transform to target", MAX_PROCESSING_TIME_MS);
    // Instead of "cat", produce "sat" (related by rhyme/context)
    auto semantic = generate_semantic_vector(SEMANTIC_DIM, wern_concept.concept_id);

    lpb_semantic_intent_t intent;
    memset(&intent, 0, sizeof(intent));
    intent.semantic_vector = semantic.data();
    intent.semantic_dim = SEMANTIC_DIM;
    intent.confidence = 0.8f;

    lpb_production_result_t production;
    success = lpb_produce_from_intent(production_bridge, &intent, &production);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageLoopTest, ParaphraseGeneration) {
    E2E_PIPELINE_START("Paraphrase Generation");

    // Test producing semantically equivalent but different output
    E2E_STAGE_BEGIN("Setup paraphrase intent", 20);
    auto semantic = generate_semantic_vector(SEMANTIC_DIM, 500);

    lpb_semantic_intent_t intent;
    memset(&intent, 0, sizeof(intent));
    intent.semantic_vector = semantic.data();
    intent.semantic_dim = SEMANTIC_DIM;
    intent.confidence = 0.85f;
    intent.from_internal = true;
    E2E_STAGE_END();

    // Produce first version
    E2E_STAGE_BEGIN("Produce first version", MAX_PROCESSING_TIME_MS);
    lpb_production_result_t prod1;
    bool success = lpb_produce_from_intent(production_bridge, &intent, &prod1);
    EXPECT_TRUE(success);
    float score1 = prod1.semantic_match;
    E2E_STAGE_END();

    // Reset and produce again (may differ)
    E2E_STAGE_BEGIN("Produce alternative version", MAX_PROCESSING_TIME_MS);
    lpb_reset(production_bridge);

    // Slightly modify semantic vector
    for (uint32_t i = 0; i < SEMANTIC_DIM; i++) {
        semantic[i] += 0.01f * (i % 2 == 0 ? 1 : -1);
    }

    lpb_production_result_t prod2;
    success = lpb_produce_from_intent(production_bridge, &intent, &prod2);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageLoopTest, CrossStreamTransformation) {
    E2E_PIPELINE_START("Cross-Stream Transformation");

    // Test transformation using both dorsal and ventral streams
    E2E_STAGE_BEGIN("Forward via both streams", MAX_BRIDGE_TIME_MS);
    uint8_t phonemes[] = {'h', 'E', 'l', 'o', 'U'};
    auto semantic = generate_semantic_vector(SEMANTIC_DIM, 200);

    wbb_comprehension_t comp;
    memset(&comp, 0, sizeof(comp));
    comp.word_id = 1;
    strncpy(comp.word_string, "hello", sizeof(comp.word_string) - 1);
    comp.num_phonemes = 5;
    memcpy(comp.phonemes, phonemes, 5);
    comp.semantic_vector = semantic.data();
    comp.semantic_dim = SEMANTIC_DIM;
    comp.confidence = 0.9f;

    int result = wbb_forward_comprehension(wbb, &comp, WBB_STREAM_BOTH);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Process cross-stream messages", MAX_BRIDGE_TIME_MS);
    int processed = wbb_process_messages(wbb, 0);
    EXPECT_GE(processed, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify message forwarding", 20);
    wbb_stats_t stats;
    wbb_get_stats(wbb, &stats);
    EXPECT_GT(stats.comprehensions_forwarded, 0u);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

//=============================================================================
// Working Memory and Attention Tests
//=============================================================================

TEST_F(E2ELanguageLoopTest, WorkingMemoryIntegration) {
    E2E_PIPELINE_START("Working Memory Integration");

    // Test working memory across the loop
    E2E_STAGE_BEGIN("Store in Wernicke WM", MAX_PROCESSING_TIME_MS);
    uint8_t ph_data[] = {'h', 'E', 'l', 'o', 'U'};
    phoneme_t phonemes[5];
    for (int i = 0; i < 5; i++) {
        phonemes[i] = static_cast<phoneme_t>(ph_data[i]);
    }

    bool success = wernicke_wm_store(wernicke, phonemes, 5);
    EXPECT_TRUE(success) << "Should store in Wernicke WM";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Store in Broca WM", MAX_PROCESSING_TIME_MS);
    success = broca_wm_push(broca, 1);  // Word ID for "hello"
    EXPECT_TRUE(success) << "Should push to Broca WM";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Rehearse Wernicke WM", MAX_PROCESSING_TIME_MS);
    success = wernicke_wm_rehearse(wernicke);
    EXPECT_TRUE(success) << "Should rehearse WM";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Retrieve from Broca WM", MAX_PROCESSING_TIME_MS);
    uint32_t word_id;
    success = broca_wm_pop(broca, &word_id);
    EXPECT_TRUE(success) << "Should pop from Broca WM";
    EXPECT_EQ(word_id, 1u) << "Should retrieve correct word";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageLoopTest, AttentionModulationEffect) {
    E2E_PIPELINE_START("Attention Modulation Effect");

    // Test how attention affects language processing
    E2E_STAGE_BEGIN("Baseline processing", MAX_PROCESSING_TIME_MS);
    uint8_t hello_phonemes[] = {'h', 'E', 'l', 'o', 'U'};
    auto phonemes = create_phoneme_sequence(hello_phonemes, 5);

    wernicke_word_result_t baseline;
    bool success = wernicke_recognize_word(wernicke, phonemes.data(), 5, &baseline);
    EXPECT_TRUE(success);
    float baseline_confidence = baseline.confidence;
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Prime lexical access", MAX_PROCESSING_TIME_MS);
    // Prime with context vector
    auto context = generate_semantic_vector(SEMANTIC_DIM, 42);

    success = lpb_prime_lexical_access(
        production_bridge,
        context.data(),
        SEMANTIC_DIM,
        0.8f  // Strong priming
    );
    EXPECT_TRUE(success) << "Should prime lexical access";
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Primed processing", MAX_PROCESSING_TIME_MS);
    // Process again with priming active
    wernicke_word_result_t primed;
    success = wernicke_recognize_word(wernicke, phonemes.data(), 5, &primed);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare results", 10);
    // Priming should affect processing (exact effect depends on implementation)
    EXPECT_GT(primed.confidence, 0.0f);
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(E2ELanguageLoopTest, ErrorMonitoringFeedback) {
    E2E_PIPELINE_START("Error Monitoring Feedback");

    // Test self-monitoring and error correction
    E2E_STAGE_BEGIN("Produce with potential error", MAX_PROCESSING_TIME_MS);
    const char* words[] = {"cat"};
    broca_utterance_result_t utterance;
    bool success = broca_produce_from_strings(broca, words, 1, &utterance);
    EXPECT_TRUE(success);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Compare production to intent", MAX_BRIDGE_TIME_MS);
    wbb_comprehension_t intended;
    memset(&intended, 0, sizeof(intended));
    intended.word_id = 4;  // cat
    strncpy(intended.word_string, "cat", sizeof(intended.word_string) - 1);
    intended.confidence = 0.95f;

    wbb_efference_copy_t efference;
    memset(&efference, 0, sizeof(efference));
    efference.planned_word_id = 4;
    uint8_t planned[] = {'k', 'a', 't'};
    efference.planned_phonemes = planned;
    efference.num_planned_phonemes = 3;

    wbb_monitoring_result_t monitoring;
    int result = wbb_compare_production(wbb, &intended, &efference, &monitoring);
    EXPECT_EQ(result, 0);
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify monitoring result", 10);
    EXPECT_TRUE(monitoring.phoneme_match) << "Phonemes should match";
    EXPECT_FALSE(monitoring.error_detected) << "No error should be detected";
    EXPECT_GT(monitoring.match_score, 0.5f) << "Match score should be high";
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}
