/**
 * @file test_wernicke_gpu.cpp
 * @brief Unit tests for GPU-accelerated Wernicke's region
 *
 * WHAT: Unit tests for GPU Wernicke kernels and API
 * WHY:  Verify correctness of GPU-accelerated language comprehension
 * HOW:  Test individual operations: phoneme recognition, word recognition, semantic activation
 *
 * @version 1.0
 * @date 2026-01-30
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>

// GPU headers outside extern "C"
#include "gpu/cognitive/nimcp_wernicke_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"

//=============================================================================
// Test Fixture
//=============================================================================

class WernickeGPUUnitTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* gpu_ctx;
    wernicke_gpu_context_t* wernicke_ctx;
    std::vector<wernicke_gpu_lexical_entry_t> test_lexicon;
    std::vector<float> test_phoneme_embeddings;
    std::mt19937 rng;

    void SetUp() override {
        gpu_ctx = nullptr;
        wernicke_ctx = nullptr;
        rng.seed(42);

        // Initialize kernel backend to detect GPU
        nimcp_kernel_backend_init(NIMCP_BACKEND_AUTO);

        // Try to create GPU context
        if (nimcp_cuda_backend_available()) {
            gpu_ctx = nimcp_gpu_context_create(0);
        }

        if (gpu_ctx) {
            wernicke_gpu_config_t config = wernicke_gpu_default_config();
            config.max_lexicon_size = 1000;
            config.max_batch_size = 64;
            config.max_cohort_size = 100;
            config.max_concepts = 500;
            config.spreading_iterations = 3;
            config.spreading_decay = 0.5f;
            wernicke_ctx = wernicke_gpu_create(gpu_ctx, &config);
        }

        // Setup test data
        setupTestLexicon();
        setupTestPhonemeEmbeddings();
    }

    void TearDown() override {
        if (wernicke_ctx) {
            wernicke_gpu_destroy(wernicke_ctx);
            wernicke_ctx = nullptr;
        }
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }
        nimcp_kernel_backend_shutdown();
    }

    void setupTestLexicon() {
        test_lexicon.clear();

        // Word 1: "hello" -> h,e,l,o phoneme IDs: 8,5,12,15
        wernicke_gpu_lexical_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 1;
        entry.phonemes[0] = 8; entry.phonemes[1] = 5;
        entry.phonemes[2] = 12; entry.phonemes[3] = 15;
        entry.phoneme_count = 4;
        entry.frequency = 0.9f;
        entry.activation = 0.0f;
        entry.concept_id = 100;
        test_lexicon.push_back(entry);

        // Word 2: "help" -> h,e,l,p phoneme IDs: 8,5,12,16
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 2;
        entry.phonemes[0] = 8; entry.phonemes[1] = 5;
        entry.phonemes[2] = 12; entry.phonemes[3] = 16;
        entry.phoneme_count = 4;
        entry.frequency = 0.85f;
        entry.activation = 0.0f;
        entry.concept_id = 101;
        test_lexicon.push_back(entry);

        // Word 3: "cat" -> k,a,t phoneme IDs: 11,1,20
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 3;
        entry.phonemes[0] = 11; entry.phonemes[1] = 1;
        entry.phonemes[2] = 20;
        entry.phoneme_count = 3;
        entry.frequency = 0.8f;
        entry.activation = 0.0f;
        entry.concept_id = 102;
        test_lexicon.push_back(entry);

        // Word 4: "car" -> k,a,r phoneme IDs: 11,1,18
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 4;
        entry.phonemes[0] = 11; entry.phonemes[1] = 1;
        entry.phonemes[2] = 18;
        entry.phoneme_count = 3;
        entry.frequency = 0.75f;
        entry.activation = 0.0f;
        entry.concept_id = 103;
        test_lexicon.push_back(entry);

        // Word 5: "dog" -> d,o,g phoneme IDs: 4,15,7
        memset(&entry, 0, sizeof(entry));
        entry.word_id = 5;
        entry.phonemes[0] = 4; entry.phonemes[1] = 15;
        entry.phonemes[2] = 7;
        entry.phoneme_count = 3;
        entry.frequency = 0.7f;
        entry.activation = 0.0f;
        entry.concept_id = 104;
        test_lexicon.push_back(entry);
    }

    void setupTestPhonemeEmbeddings() {
        // Create random phoneme embeddings for 44 phonemes x 32 dimensions
        const uint32_t num_phonemes = 44;
        const uint32_t embed_dim = 32;
        test_phoneme_embeddings.resize(num_phonemes * embed_dim);

        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (size_t i = 0; i < test_phoneme_embeddings.size(); i++) {
            test_phoneme_embeddings[i] = dist(rng);
        }
    }

    wernicke_gpu_spectral_frame_t createTestFrame(uint8_t target_phoneme) {
        wernicke_gpu_spectral_frame_t frame;
        memset(&frame, 0, sizeof(frame));

        // Create spectral features that would ideally match the target phoneme
        std::uniform_real_distribution<float> noise(-0.1f, 0.1f);
        for (int i = 0; i < 40; i++) {
            frame.mel_bands[i] = 0.5f + noise(rng) + (target_phoneme % 40 == i ? 0.3f : 0.0f);
        }
        for (int i = 0; i < 13; i++) {
            frame.mfcc[i] = noise(rng);
            frame.delta[i] = noise(rng);
            frame.delta_delta[i] = noise(rng);
        }
        frame.energy = 0.7f + noise(rng);
        frame.pitch = 150.0f + 10.0f * noise(rng);
        frame.voicing = (target_phoneme < 22) ? 0.9f : 0.1f; // Voiced vs unvoiced
        return frame;
    }

    bool hasGPU() const {
        return gpu_ctx != nullptr && wernicke_ctx != nullptr;
    }

    bool uploadLexicon() {
        if (!wernicke_ctx || test_lexicon.empty()) return false;
        return wernicke_gpu_upload_lexicon(wernicke_ctx, test_lexicon.data(), test_lexicon.size());
    }

    bool uploadPhonemeEmbeddings() {
        if (!wernicke_ctx || test_phoneme_embeddings.empty()) return false;
        return wernicke_gpu_upload_phoneme_embeddings(wernicke_ctx, test_phoneme_embeddings.data(), 44, 32);
    }
};

//=============================================================================
// Configuration Tests
//=============================================================================

TEST_F(WernickeGPUUnitTest, DefaultConfig_HasSaneValues) {
    wernicke_gpu_config_t config = wernicke_gpu_default_config();

    EXPECT_GT(config.num_phoneme_categories, 0u);
    EXPECT_GT(config.phoneme_embedding_dim, 0u);
    EXPECT_GT(config.max_lexicon_size, 0u);
    EXPECT_GT(config.max_cohort_size, 0u);
    EXPECT_GT(config.max_concepts, 0u);
    EXPECT_GT(config.spreading_iterations, 0u);
    EXPECT_GT(config.spreading_decay, 0.0f);
    EXPECT_LE(config.spreading_decay, 1.0f);
    EXPECT_GT(config.working_memory_slots, 0u);
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

TEST_F(WernickeGPUUnitTest, Create_WithNullContext_ReturnsNull) {
    wernicke_gpu_context_t* ctx = wernicke_gpu_create(nullptr, nullptr);
    EXPECT_EQ(ctx, nullptr);
}

TEST_F(WernickeGPUUnitTest, Create_WithValidContext_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_NE(wernicke_ctx, nullptr);
}

TEST_F(WernickeGPUUnitTest, Destroy_WithNull_DoesNotCrash) {
    wernicke_gpu_destroy(nullptr);
    SUCCEED();
}

//=============================================================================
// Lexicon Management Tests
//=============================================================================

TEST_F(WernickeGPUUnitTest, UploadLexicon_Succeeds) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    EXPECT_TRUE(uploadLexicon());
    EXPECT_EQ(wernicke_gpu_get_lexicon_size(wernicke_ctx), test_lexicon.size());
}

TEST_F(WernickeGPUUnitTest, ClearLexicon_ResetsSize) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();
    EXPECT_GT(wernicke_gpu_get_lexicon_size(wernicke_ctx), 0u);

    EXPECT_TRUE(wernicke_gpu_clear_lexicon(wernicke_ctx));
    EXPECT_EQ(wernicke_gpu_get_lexicon_size(wernicke_ctx), 0u);
}

//=============================================================================
// Phoneme Recognition Tests
//=============================================================================

TEST_F(WernickeGPUUnitTest, RecognizePhonemes_ProducesResults) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadPhonemeEmbeddings();

    // Create test spectral frames
    std::vector<wernicke_gpu_spectral_frame_t> frames;
    frames.push_back(createTestFrame(8));  // 'h'
    frames.push_back(createTestFrame(5));  // 'e'
    frames.push_back(createTestFrame(12)); // 'l'
    frames.push_back(createTestFrame(15)); // 'o'

    std::vector<wernicke_gpu_phoneme_result_t> results(frames.size());

    EXPECT_TRUE(wernicke_gpu_recognize_phonemes(
        wernicke_ctx, frames.data(), frames.size(), results.data()
    ));

    // Check that we got valid results
    for (size_t i = 0; i < results.size(); i++) {
        EXPECT_GE(results[i].confidence, 0.0f);
        EXPECT_LE(results[i].confidence, 1.0f);
    }
}

TEST_F(WernickeGPUUnitTest, ComputePosteriors_ProducesValidDistribution) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadPhonemeEmbeddings();

    std::vector<wernicke_gpu_spectral_frame_t> frames;
    frames.push_back(createTestFrame(10));

    const uint32_t num_phonemes = 44;
    std::vector<float> posteriors(frames.size() * num_phonemes);

    EXPECT_TRUE(wernicke_gpu_compute_posteriors(
        wernicke_ctx, frames.data(), frames.size(), posteriors.data()
    ));

    // Check that posteriors sum to approximately 1
    float sum = 0.0f;
    for (uint32_t i = 0; i < num_phonemes; i++) {
        EXPECT_GE(posteriors[i], 0.0f);
        sum += posteriors[i];
    }
    EXPECT_NEAR(sum, 1.0f, 0.01f);
}

//=============================================================================
// Word Recognition Tests (Cohort Model)
//=============================================================================

TEST_F(WernickeGPUUnitTest, RecognizeWords_FindsMatchingCohort) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    // Input phoneme sequence for "hel" - should match "hello" and "help"
    uint8_t phonemes[] = {8, 5, 12}; // h, e, l
    wernicke_gpu_word_candidate_t candidates[10];
    uint32_t num_candidates = 0;

    EXPECT_TRUE(wernicke_gpu_recognize_words(
        wernicke_ctx, phonemes, 3, candidates, 10, &num_candidates
    ));

    // Should find at least "hello" and "help" as cohort candidates
    EXPECT_GE(num_candidates, 2u);

    // Check that candidates have valid probabilities
    for (uint32_t i = 0; i < num_candidates; i++) {
        EXPECT_GE(candidates[i].cohort_probability, 0.0f);
        EXPECT_LE(candidates[i].cohort_probability, 1.0f);
    }
}

TEST_F(WernickeGPUUnitTest, RecognizeWords_CompletesRecognition) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    // Complete phoneme sequence for "cat"
    uint8_t phonemes[] = {11, 1, 20}; // k, a, t
    wernicke_gpu_word_candidate_t candidates[10];
    uint32_t num_candidates = 0;

    EXPECT_TRUE(wernicke_gpu_recognize_words(
        wernicke_ctx, phonemes, 3, candidates, 10, &num_candidates
    ));

    // Should find "cat" as a complete match
    bool found_cat = false;
    for (uint32_t i = 0; i < num_candidates; i++) {
        if (candidates[i].word_id == 3) {
            found_cat = true;
            EXPECT_TRUE(candidates[i].recognition_complete);
            EXPECT_EQ(candidates[i].matched_phonemes, 3u);
        }
    }
    EXPECT_TRUE(found_cat);
}

TEST_F(WernickeGPUUnitTest, RecognizeWords_MatchesCPU) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    uint8_t phonemes[] = {11, 1}; // k, a - should match "cat" and "car"
    wernicke_gpu_word_candidate_t gpu_candidates[10];
    wernicke_gpu_word_candidate_t cpu_candidates[10];
    uint32_t gpu_count = 0, cpu_count = 0;

    // GPU recognition
    EXPECT_TRUE(wernicke_gpu_recognize_words(
        wernicke_ctx, phonemes, 2, gpu_candidates, 10, &gpu_count
    ));

    // CPU recognition (reference)
    EXPECT_TRUE(wernicke_cpu_recognize_words(
        test_lexicon.data(), test_lexicon.size(),
        phonemes, 2, cpu_candidates, 10, &cpu_count
    ));

    // Compare results
    EXPECT_EQ(gpu_count, cpu_count);
}

//=============================================================================
// Cohort Update Tests
//=============================================================================

TEST_F(WernickeGPUUnitTest, UpdateCohort_NarrowsCandidates) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();

    // Reset cohort and start fresh
    EXPECT_TRUE(wernicke_gpu_reset_cohort(wernicke_ctx));

    // Add first phoneme 'h' (8)
    EXPECT_TRUE(wernicke_gpu_update_cohort(wernicke_ctx, 8, 0.95f));

    wernicke_gpu_word_candidate_t candidates[10];
    uint32_t num_candidates = 0;
    EXPECT_TRUE(wernicke_gpu_get_cohort(wernicke_ctx, candidates, 10, &num_candidates));

    uint32_t after_h = num_candidates;

    // Add second phoneme 'e' (5)
    EXPECT_TRUE(wernicke_gpu_update_cohort(wernicke_ctx, 5, 0.9f));
    EXPECT_TRUE(wernicke_gpu_get_cohort(wernicke_ctx, candidates, 10, &num_candidates));

    // Cohort should narrow or stay same (can't grow)
    EXPECT_LE(num_candidates, after_h);
}

//=============================================================================
// Semantic Spreading Activation Tests
//=============================================================================

TEST_F(WernickeGPUUnitTest, SpreadActivation_ProducesOutput) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Seed concepts with activation
    uint32_t seed_concepts[] = {100, 102};
    float seed_activations[] = {1.0f, 0.8f};

    wernicke_gpu_activation_result_t results[50];
    uint32_t num_results = 0;

    EXPECT_TRUE(wernicke_gpu_spread_activation(
        wernicke_ctx, seed_concepts, seed_activations, 2,
        results, 50, &num_results
    ));

    // Should have at least the seed concepts in results
    EXPECT_GE(num_results, 2u);

    // Check that activations are valid
    for (uint32_t i = 0; i < num_results; i++) {
        EXPECT_GE(results[i].activation, 0.0f);
        EXPECT_LE(results[i].activation, 1.0f);
    }
}

TEST_F(WernickeGPUUnitTest, GetTopActivated_ReturnsOrderedResults) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Seed with different activation levels
    uint32_t seed_concepts[] = {100, 101, 102};
    float seed_activations[] = {0.5f, 1.0f, 0.3f};

    wernicke_gpu_spread_activation(
        wernicke_ctx, seed_concepts, seed_activations, 3,
        nullptr, 0, nullptr
    );

    wernicke_gpu_activation_result_t results[3];
    uint32_t actual_count = 0;

    EXPECT_TRUE(wernicke_gpu_get_top_activated(
        wernicke_ctx, 3, results, &actual_count
    ));

    EXPECT_GT(actual_count, 0u);

    // Results should be in descending order
    for (uint32_t i = 1; i < actual_count; i++) {
        EXPECT_GE(results[i-1].activation, results[i].activation);
    }
}

TEST_F(WernickeGPUUnitTest, SemanticSimilarity_ReturnsValidScore) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // Same concept should have similarity 1.0
    float self_sim = wernicke_gpu_semantic_similarity(wernicke_ctx, 100, 100);
    EXPECT_NEAR(self_sim, 1.0f, 0.01f);

    // Different concepts should have similarity in [0, 1]
    float diff_sim = wernicke_gpu_semantic_similarity(wernicke_ctx, 100, 104);
    EXPECT_GE(diff_sim, 0.0f);
    EXPECT_LE(diff_sim, 1.0f);
}

//=============================================================================
// Working Memory Tests
//=============================================================================

TEST_F(WernickeGPUUnitTest, WMPush_AddsPhonemes) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint8_t phonemes[] = {8, 5, 12, 15}; // h, e, l, o
    EXPECT_TRUE(wernicke_gpu_wm_push(wernicke_ctx, phonemes, 4));

    uint8_t retrieved[16];
    float activations[16];
    uint32_t actual_count = 0;

    EXPECT_TRUE(wernicke_gpu_wm_get_contents(
        wernicke_ctx, retrieved, activations, 16, &actual_count
    ));

    EXPECT_EQ(actual_count, 4u);
    EXPECT_EQ(retrieved[0], 8u);
    EXPECT_EQ(retrieved[1], 5u);
    EXPECT_EQ(retrieved[2], 12u);
    EXPECT_EQ(retrieved[3], 15u);
}

TEST_F(WernickeGPUUnitTest, WMRehearsal_MaintainsActivation) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint8_t phonemes[] = {8, 5};
    wernicke_gpu_wm_push(wernicke_ctx, phonemes, 2);

    // Apply decay first
    wernicke_gpu_wm_apply_decay(wernicke_ctx, 0.5f, 0.0f);

    uint8_t retrieved[16];
    float activations_before[16];
    uint32_t count_before = 0;
    wernicke_gpu_wm_get_contents(wernicke_ctx, retrieved, activations_before, 16, &count_before);

    // Rehearse
    EXPECT_TRUE(wernicke_gpu_wm_rehearse(wernicke_ctx));

    float activations_after[16];
    uint32_t count_after = 0;
    wernicke_gpu_wm_get_contents(wernicke_ctx, retrieved, activations_after, 16, &count_after);

    // After rehearsal, activations should be boosted
    EXPECT_EQ(count_before, count_after);
    for (uint32_t i = 0; i < count_after; i++) {
        EXPECT_GE(activations_after[i], activations_before[i]);
    }
}

TEST_F(WernickeGPUUnitTest, WMClear_RemovesAll) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint8_t phonemes[] = {1, 2, 3};
    wernicke_gpu_wm_push(wernicke_ctx, phonemes, 3);

    EXPECT_TRUE(wernicke_gpu_wm_clear(wernicke_ctx));

    uint8_t retrieved[16];
    uint32_t actual_count = 16;
    EXPECT_TRUE(wernicke_gpu_wm_get_contents(wernicke_ctx, retrieved, nullptr, 16, &actual_count));
    EXPECT_EQ(actual_count, 0u);
}

TEST_F(WernickeGPUUnitTest, WMDecay_ReducesActivations) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uint8_t phonemes[] = {10};
    wernicke_gpu_wm_push(wernicke_ctx, phonemes, 1);

    // Apply 50% decay
    EXPECT_TRUE(wernicke_gpu_wm_apply_decay(wernicke_ctx, 0.5f, 0.0f));

    uint8_t retrieved[16];
    float activations[16];
    uint32_t actual_count = 0;
    EXPECT_TRUE(wernicke_gpu_wm_get_contents(wernicke_ctx, retrieved, activations, 16, &actual_count));

    // Activation should be reduced (started at 1.0, now ~0.5)
    EXPECT_NEAR(activations[0], 0.5f, 0.1f);
}

//=============================================================================
// Full Comprehension Pipeline Tests
//=============================================================================

TEST_F(WernickeGPUUnitTest, Comprehend_RunsFullPipeline) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();
    uploadPhonemeEmbeddings();

    // Create test frames for "cat"
    std::vector<wernicke_gpu_spectral_frame_t> frames;
    frames.push_back(createTestFrame(11)); // k
    frames.push_back(createTestFrame(1));  // a
    frames.push_back(createTestFrame(20)); // t

    wernicke_gpu_word_candidate_t word_candidates[10];
    wernicke_gpu_activation_result_t semantic_activations[50];
    uint32_t num_words = 0, num_activations = 0;

    EXPECT_TRUE(wernicke_gpu_comprehend(
        wernicke_ctx, frames.data(), frames.size(),
        word_candidates, 10, &num_words,
        semantic_activations, 50, &num_activations
    ));

    // Should have produced some candidates
    EXPECT_GT(num_words, 0u);
}

//=============================================================================
// Statistics Tests
//=============================================================================

TEST_F(WernickeGPUUnitTest, Stats_TracksOperations) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadLexicon();
    uploadPhonemeEmbeddings();

    // Reset stats
    wernicke_gpu_reset_stats(wernicke_ctx);

    // Perform operations
    std::vector<wernicke_gpu_spectral_frame_t> frames;
    frames.push_back(createTestFrame(10));
    std::vector<wernicke_gpu_phoneme_result_t> results(1);
    wernicke_gpu_recognize_phonemes(wernicke_ctx, frames.data(), 1, results.data());

    uint8_t phonemes[] = {11, 1};
    wernicke_gpu_word_candidate_t candidates[10];
    uint32_t num_candidates = 0;
    wernicke_gpu_recognize_words(wernicke_ctx, phonemes, 2, candidates, 10, &num_candidates);

    // Check stats
    wernicke_gpu_stats_t stats;
    EXPECT_TRUE(wernicke_gpu_get_stats(wernicke_ctx, &stats));

    EXPECT_GT(stats.phoneme_recognitions, 0u);
    EXPECT_GT(stats.word_recognitions, 0u);
}

//=============================================================================
// GPU Recovery Tests
//=============================================================================

TEST_F(WernickeGPUUnitTest, Recovery_InitializedOnCreate) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // GPU recovery should be initialized when Wernicke GPU context is created
    EXPECT_TRUE(nimcp_gpu_recovery_is_initialized());
}

TEST_F(WernickeGPUUnitTest, Recovery_HandlesNullInputGracefully) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    // These should fail gracefully due to recovery handling
    EXPECT_FALSE(wernicke_gpu_recognize_phonemes(wernicke_ctx, nullptr, 0, nullptr));
    EXPECT_FALSE(wernicke_gpu_recognize_words(wernicke_ctx, nullptr, 0, nullptr, 0, nullptr));
    EXPECT_FALSE(wernicke_gpu_spread_activation(wernicke_ctx, nullptr, nullptr, 0, nullptr, 0, nullptr));
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(WernickeGPUUnitTest, NullSafety_AllFunctionsHandleNull) {
    // These should not crash
    wernicke_gpu_destroy(nullptr);
    EXPECT_FALSE(wernicke_gpu_synchronize(nullptr));
    EXPECT_FALSE(wernicke_gpu_upload_lexicon(nullptr, nullptr, 0));
    EXPECT_FALSE(wernicke_gpu_clear_lexicon(nullptr));
    EXPECT_EQ(wernicke_gpu_get_lexicon_size(nullptr), 0u);
    EXPECT_FALSE(wernicke_gpu_recognize_phonemes(nullptr, nullptr, 0, nullptr));
    EXPECT_FALSE(wernicke_gpu_recognize_words(nullptr, nullptr, 0, nullptr, 0, nullptr));
    EXPECT_FALSE(wernicke_gpu_spread_activation(nullptr, nullptr, nullptr, 0, nullptr, 0, nullptr));
    EXPECT_FALSE(wernicke_gpu_wm_push(nullptr, nullptr, 0));
    EXPECT_FALSE(wernicke_gpu_wm_get_contents(nullptr, nullptr, nullptr, 0, nullptr));
    EXPECT_FALSE(wernicke_gpu_get_stats(nullptr, nullptr));

    SUCCEED();
}

//=============================================================================
// CPU Fallback Equivalence Tests
//=============================================================================

TEST_F(WernickeGPUUnitTest, CPUFallback_PhonemeRecognition_MatchesGPU) {
    if (!hasGPU()) {
        GTEST_SKIP() << "GPU not available";
    }

    uploadPhonemeEmbeddings();

    std::vector<wernicke_gpu_spectral_frame_t> frames;
    for (int i = 0; i < 5; i++) {
        frames.push_back(createTestFrame(i * 3));
    }

    std::vector<wernicke_gpu_phoneme_result_t> gpu_results(frames.size());
    std::vector<wernicke_gpu_phoneme_result_t> cpu_results(frames.size());

    // GPU recognition
    EXPECT_TRUE(wernicke_gpu_recognize_phonemes(
        wernicke_ctx, frames.data(), frames.size(), gpu_results.data()
    ));

    // CPU recognition (reference)
    EXPECT_TRUE(wernicke_cpu_recognize_phonemes(
        frames.data(), frames.size(),
        test_phoneme_embeddings.data(), 44, 32,
        cpu_results.data()
    ));

    // Compare results (allowing for floating point differences)
    for (size_t i = 0; i < frames.size(); i++) {
        EXPECT_EQ(gpu_results[i].phoneme_id, cpu_results[i].phoneme_id)
            << "Phoneme mismatch at frame " << i;
        EXPECT_NEAR(gpu_results[i].confidence, cpu_results[i].confidence, 0.05f)
            << "Confidence mismatch at frame " << i;
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
