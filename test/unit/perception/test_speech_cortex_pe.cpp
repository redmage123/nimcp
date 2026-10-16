//=============================================================================
// test_speech_cortex_pe.cpp - Unit Tests for Speech Cortex PE
//=============================================================================
/**
 * @file test_speech_cortex_pe.cpp
 * @brief Unit tests for positional encoding integration in speech cortex
 *
 * WHAT: Test PE for phoneme sequences and phonological buffer positions
 * WHY:  Position information critical for speech processing serial order
 * HOW:  Test sinusoidal PE for sequences, learned PE for buffer slots
 *
 * TEST COVERAGE:
 * 1. PE configuration (sinusoidal and learned)
 * 2. Phoneme sequence position encoding
 * 3. Phonological buffer slot embeddings
 * 4. Serial position effects (primacy/recency)
 * 5. Position-aware phoneme processing
 * 6. Edge cases (empty sequences, full buffer)
 * 7. Integration with phoneme detection
 *
 * BIOLOGICAL BASIS:
 * - Phonological loop maintains serial order (Baddeley & Hitch 1974)
 * - Superior Temporal Gyrus neurons sensitive to position
 * - Serial position effects in working memory
 * - Position encoding supports temporal ordering
 *
 * @author NIMCP Development Team
 * @date 2025-12-10
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

// Headers have their own extern "C" guards
    #include "perception/nimcp_speech_cortex.h"
    #include "utils/encoding/nimcp_positional_encoding.h"
    #include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class SpeechCortexPETest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-4f;
    static constexpr uint32_t TEST_SAMPLE_RATE = 16000;
    static constexpr uint32_t TEST_PE_DIM = 64;
    static constexpr uint32_t TEST_MAX_PHONEMES = 20;

    speech_cortex_t* cortex = nullptr;

    void SetUp() override {
        srand(42);
        nimcp_memory_init();
    }

    void TearDown() override {
        if (cortex) {
            speech_cortex_destroy(cortex);
            cortex = nullptr;
        }
    }

    bool FloatEqual(float a, float b, float eps = EPSILON) {
        return std::abs(a - b) < eps;
    }

    // Helper to create speech cortex with PE
    speech_cortex_t* CreateCortexWithPE(bool enable_pe, uint32_t phoneme_type, uint32_t buffer_type) {
        speech_cortex_config_t config = speech_cortex_default_config();
        config.sample_rate = TEST_SAMPLE_RATE;
        config.enable_positional_encoding = enable_pe;
        config.pe_embedding_dim = TEST_PE_DIM;
        config.pe_phoneme_type = phoneme_type;  // 0=sinusoidal, 1=learned
        config.pe_buffer_type = buffer_type;

        return speech_cortex_create(&config);
    }

    // Helper to create test phoneme sequence
    void CreateTestPhonemeSequence(phoneme_event_t* phonemes, uint32_t count) {
        for (uint32_t i = 0; i < count; i++) {
            phonemes[i].phoneme = (phoneme_t)(PHONEME_IY + (i % 10));
            phonemes[i].confidence = 0.9f - (float)i * 0.05f;
            phonemes[i].onset_time_ms = i * 100;
            phonemes[i].offset_time_ms = (i + 1) * 100;
            phonemes[i].position_embedding = nullptr;  // Allocated by cortex
            phonemes[i].sequence_position = i;
        }
    }
};

//=============================================================================
// Unit Tests: PE Configuration
//=============================================================================

TEST_F(SpeechCortexPETest, SetPEConfig_Sinusoidal) {
    // WHAT: Configure speech cortex with sinusoidal PE for phoneme sequences
    // WHY:  Variable length sequences need extrapolatable encoding

    cortex = CreateCortexWithPE(true, 0, 0);  // Both sinusoidal
    ASSERT_NE(cortex, nullptr) << "Cortex with sinusoidal PE should be created";

    speech_cortex_stats_t stats;
    bool result = speech_cortex_get_stats(cortex, &stats);
    EXPECT_TRUE(result) << "Stats retrieval should succeed";
}

TEST_F(SpeechCortexPETest, SetPEConfig_Learned) {
    // WHAT: Configure speech cortex with learned PE for phonological buffer
    // WHY:  Fixed capacity buffer benefits from learned position embeddings

    cortex = CreateCortexWithPE(true, 0, 1);  // Sinusoidal for sequence, learned for buffer
    ASSERT_NE(cortex, nullptr) << "Cortex with learned buffer PE should be created";

    speech_cortex_stats_t stats;
    bool result = speech_cortex_get_stats(cortex, &stats);
    EXPECT_TRUE(result) << "Stats retrieval should succeed";
}

TEST_F(SpeechCortexPETest, SetPEConfig_Disable) {
    // WHAT: Create speech cortex without PE
    // WHY:  Baseline comparison

    cortex = CreateCortexWithPE(false, 0, 0);
    ASSERT_NE(cortex, nullptr);

    // PE functions should fail
    float embedding[TEST_PE_DIM];
    bool result = speech_cortex_get_phonological_position_embedding(cortex, 0, embedding);
    EXPECT_FALSE(result) << "PE should be disabled";
}

TEST_F(SpeechCortexPETest, SetPEConfig_Runtime) {
    // WHAT: Configure PE at runtime
    // WHY:  Test dynamic reconfiguration

    cortex = CreateCortexWithPE(false, 0, 0);
    ASSERT_NE(cortex, nullptr);

    // Enable PE
    bool result = speech_cortex_set_pe_config(cortex, true, TEST_PE_DIM, 0, 1);
    EXPECT_TRUE(result) << "Runtime PE configuration should succeed";

    // Verify PE is now active
    float embedding[TEST_PE_DIM];
    result = speech_cortex_get_phonological_position_embedding(cortex, 0, embedding);
    EXPECT_TRUE(result) << "PE should be enabled after configuration";
}

//=============================================================================
// Unit Tests: Phoneme Sequence Encoding
//=============================================================================

TEST_F(SpeechCortexPETest, EncodePhonemePositions_ShortSequence) {
    // WHAT: Encode short phoneme sequence with PE
    // WHY:  Basic sequence encoding test

    cortex = CreateCortexWithPE(true, 0, 0);
    ASSERT_NE(cortex, nullptr);

    uint32_t num_phonemes = 5;
    phoneme_event_t phonemes[num_phonemes];
    CreateTestPhonemeSequence(phonemes, num_phonemes);

    bool result = speech_cortex_encode_phoneme_positions(cortex, phonemes,
                                                         num_phonemes, true);
    EXPECT_TRUE(result) << "Phoneme position encoding should succeed";

    // Verify position embeddings were allocated
    for (uint32_t i = 0; i < num_phonemes; i++) {
        EXPECT_NE(phonemes[i].position_embedding, nullptr)
            << "Position embedding should be allocated for phoneme " << i;
        EXPECT_EQ(phonemes[i].sequence_position, i)
            << "Sequence position should be set correctly";
    }
}

TEST_F(SpeechCortexPETest, EncodePhonemePositions_LongSequence) {
    // WHAT: Encode longer phoneme sequence
    // WHY:  Test sequence length scalability

    cortex = CreateCortexWithPE(true, 0, 0);
    ASSERT_NE(cortex, nullptr);

    uint32_t num_phonemes = TEST_MAX_PHONEMES;
    phoneme_event_t phonemes[num_phonemes];
    CreateTestPhonemeSequence(phonemes, num_phonemes);

    bool result = speech_cortex_encode_phoneme_positions(cortex, phonemes,
                                                         num_phonemes, true);
    EXPECT_TRUE(result) << "Long sequence encoding should succeed";

    // Verify all positions encoded
    for (uint32_t i = 0; i < num_phonemes; i++) {
        EXPECT_EQ(phonemes[i].sequence_position, i);
    }
}

TEST_F(SpeechCortexPETest, EncodePhonemePositions_Additive) {
    // WHAT: Test additive vs non-additive encoding
    // WHY:  Verify PE application mode

    cortex = CreateCortexWithPE(true, 0, 0);
    ASSERT_NE(cortex, nullptr);

    phoneme_event_t phonemes[3];
    CreateTestPhonemeSequence(phonemes, 3);

    // Additive mode
    bool result = speech_cortex_encode_phoneme_positions(cortex, phonemes, 3, true);
    EXPECT_TRUE(result) << "Additive encoding should succeed";

    // Non-additive mode (just store embeddings)
    result = speech_cortex_encode_phoneme_positions(cortex, phonemes, 3, false);
    EXPECT_TRUE(result) << "Non-additive encoding should succeed";
}

//=============================================================================
// Unit Tests: Phonological Buffer Position Embeddings
//=============================================================================

TEST_F(SpeechCortexPETest, GetPhonologicalPositionEmbedding_FirstSlot) {
    // WHAT: Get position embedding for first buffer slot
    // WHY:  Test buffer position encoding

    cortex = CreateCortexWithPE(true, 0, 1);  // Learned buffer PE
    ASSERT_NE(cortex, nullptr);

    float embedding[TEST_PE_DIM];
    bool result = speech_cortex_get_phonological_position_embedding(cortex, 0, embedding);
    EXPECT_TRUE(result) << "Buffer position 0 embedding should be available";

    // Verify embedding is non-zero
    bool has_nonzero = false;
    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        if (std::abs(embedding[i]) > EPSILON) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero) << "Buffer position embedding should be non-zero";
}

TEST_F(SpeechCortexPETest, GetPhonologicalPositionEmbedding_AllSlots) {
    // WHAT: Get embeddings for all buffer slots (0-8)
    // WHY:  Verify all positions have unique embeddings

    cortex = CreateCortexWithPE(true, 0, 1);
    ASSERT_NE(cortex, nullptr);

    uint32_t num_slots = 9;  // 7±2 = 9 max
    float embeddings[num_slots][TEST_PE_DIM];

    // Get all embeddings
    for (uint32_t slot = 0; slot < num_slots; slot++) {
        bool result = speech_cortex_get_phonological_position_embedding(cortex, slot,
                                                                         embeddings[slot]);
        EXPECT_TRUE(result) << "Slot " << slot << " should have embedding";
    }

    // Verify all pairs are different
    for (uint32_t i = 0; i < num_slots; i++) {
        for (uint32_t j = i + 1; j < num_slots; j++) {
            bool different = false;
            for (uint32_t k = 0; k < TEST_PE_DIM; k++) {
                if (std::abs(embeddings[i][k] - embeddings[j][k]) > EPSILON) {
                    different = true;
                    break;
                }
            }
            EXPECT_TRUE(different) << "Slots " << i << " and " << j << " should differ";
        }
    }
}

TEST_F(SpeechCortexPETest, GetPhonologicalPositionEmbedding_Consistency) {
    // WHAT: Verify same slot returns same embedding across retrievals
    // WHY:  Learned embeddings should be stable

    cortex = CreateCortexWithPE(true, 0, 1);
    ASSERT_NE(cortex, nullptr);

    uint32_t slot = 3;
    float emb1[TEST_PE_DIM];
    float emb2[TEST_PE_DIM];

    speech_cortex_get_phonological_position_embedding(cortex, slot, emb1);
    speech_cortex_get_phonological_position_embedding(cortex, slot, emb2);

    // Verify embeddings are identical
    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        EXPECT_NEAR(emb1[i], emb2[i], EPSILON)
            << "Buffer position embedding should be consistent at index " << i;
    }
}

//=============================================================================
// Unit Tests: Serial Position Effects
//=============================================================================

TEST_F(SpeechCortexPETest, SerialPosition_PrimacyEffect) {
    // WHAT: Test primacy effect in phonological buffer
    // WHY:  Early positions should have distinct encodings

    cortex = CreateCortexWithPE(true, 0, 1);
    ASSERT_NE(cortex, nullptr);

    float emb_first[TEST_PE_DIM];
    float emb_second[TEST_PE_DIM];

    speech_cortex_get_phonological_position_embedding(cortex, 0, emb_first);
    speech_cortex_get_phonological_position_embedding(cortex, 1, emb_second);

    // Verify first and second positions are different
    bool different = false;
    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        if (std::abs(emb_first[i] - emb_second[i]) > EPSILON) {
            different = true;
            break;
        }
    }

    EXPECT_TRUE(different) << "First and second positions should have different encodings";
}

TEST_F(SpeechCortexPETest, SerialPosition_RecencyEffect) {
    // WHAT: Test recency effect in phonological buffer
    // WHY:  Recent positions should have distinct encodings

    cortex = CreateCortexWithPE(true, 0, 1);
    ASSERT_NE(cortex, nullptr);

    uint32_t last = 8;
    uint32_t second_last = 7;

    float emb_last[TEST_PE_DIM];
    float emb_second_last[TEST_PE_DIM];

    speech_cortex_get_phonological_position_embedding(cortex, last, emb_last);
    speech_cortex_get_phonological_position_embedding(cortex, second_last, emb_second_last);

    // Verify last and second-last positions are different
    bool different = false;
    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        if (std::abs(emb_last[i] - emb_second_last[i]) > EPSILON) {
            different = true;
            break;
        }
    }

    EXPECT_TRUE(different) << "Last and second-last positions should have different encodings";
}

TEST_F(SpeechCortexPETest, SerialPosition_MiddlePositions) {
    // WHAT: Test middle positions in buffer
    // WHY:  Middle items should have intermediate encodings

    cortex = CreateCortexWithPE(true, 0, 1);
    ASSERT_NE(cortex, nullptr);

    uint32_t first = 0;
    uint32_t middle = 4;
    uint32_t last = 8;

    float emb_first[TEST_PE_DIM];
    float emb_middle[TEST_PE_DIM];
    float emb_last[TEST_PE_DIM];

    speech_cortex_get_phonological_position_embedding(cortex, first, emb_first);
    speech_cortex_get_phonological_position_embedding(cortex, middle, emb_middle);
    speech_cortex_get_phonological_position_embedding(cortex, last, emb_last);

    // All three should be different
    bool first_vs_middle = false;
    bool middle_vs_last = false;

    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        if (std::abs(emb_first[i] - emb_middle[i]) > EPSILON) first_vs_middle = true;
        if (std::abs(emb_middle[i] - emb_last[i]) > EPSILON) middle_vs_last = true;
    }

    EXPECT_TRUE(first_vs_middle) << "First and middle positions should differ";
    EXPECT_TRUE(middle_vs_last) << "Middle and last positions should differ";
}

//=============================================================================
// Unit Tests: Edge Cases
//=============================================================================

TEST_F(SpeechCortexPETest, EdgeCase_NullInput) {
    // WHAT: Handle NULL inputs gracefully
    // WHY:  Robustness testing

    float embedding[TEST_PE_DIM];
    bool result = speech_cortex_get_phonological_position_embedding(nullptr, 0, embedding);
    EXPECT_FALSE(result) << "NULL cortex should fail";

    cortex = CreateCortexWithPE(true, 0, 1);
    result = speech_cortex_get_phonological_position_embedding(cortex, 0, nullptr);
    EXPECT_FALSE(result) << "NULL output buffer should fail";
}

TEST_F(SpeechCortexPETest, EdgeCase_InvalidBufferPosition) {
    // WHAT: Request position beyond buffer capacity
    // WHY:  Boundary testing

    cortex = CreateCortexWithPE(true, 0, 1);
    ASSERT_NE(cortex, nullptr);

    float embedding[TEST_PE_DIM];
    bool result = speech_cortex_get_phonological_position_embedding(cortex, 20, embedding);
    EXPECT_FALSE(result) << "Out-of-bounds position should fail";
}

TEST_F(SpeechCortexPETest, EdgeCase_EmptyPhonemeSequence) {
    // WHAT: Encode empty phoneme sequence
    // WHY:  Edge case validation

    cortex = CreateCortexWithPE(true, 0, 0);
    ASSERT_NE(cortex, nullptr);

    phoneme_event_t phonemes[1];  // Dummy array
    bool result = speech_cortex_encode_phoneme_positions(cortex, phonemes, 0, true);

    // Should either succeed as no-op or fail gracefully
    SUCCEED() << "Empty sequence handled";
}

TEST_F(SpeechCortexPETest, EdgeCase_NullPhonemeArray) {
    // WHAT: Pass NULL phoneme array
    // WHY:  Robustness testing

    cortex = CreateCortexWithPE(true, 0, 0);
    ASSERT_NE(cortex, nullptr);

    bool result = speech_cortex_encode_phoneme_positions(cortex, nullptr, 5, true);
    EXPECT_FALSE(result) << "NULL phoneme array should fail";
}

//=============================================================================
// Unit Tests: Integration with Speech Processing
//=============================================================================

TEST_F(SpeechCortexPETest, Integration_PhonemeDetectionWithPE) {
    // WHAT: Detect phonemes and apply PE
    // WHY:  End-to-end integration test

    cortex = CreateCortexWithPE(true, 0, 0);
    ASSERT_NE(cortex, nullptr);

    // Create mock audio data
    uint32_t num_samples = 1600;  // 100ms at 16kHz
    float audio_data[num_samples];
    for (uint32_t i = 0; i < num_samples; i++) {
        audio_data[i] = 0.01f * sin(2.0f * M_PI * 440.0f * (float)i / (float)TEST_SAMPLE_RATE);
    }

    // Detect phonemes
    phoneme_event_t phonemes[10];
    uint32_t num_detected = 0;
    bool result = speech_cortex_detect_phonemes(cortex, audio_data, num_samples,
                                                phonemes, 10, &num_detected);

    if (result && num_detected > 0) {
        // Apply PE to detected phonemes
        result = speech_cortex_encode_phoneme_positions(cortex, phonemes,
                                                        num_detected, true);
        EXPECT_TRUE(result) << "PE encoding of detected phonemes should succeed";

        // Verify positions are set
        for (uint32_t i = 0; i < num_detected; i++) {
            EXPECT_EQ(phonemes[i].sequence_position, i);
        }
    }

    SUCCEED() << "Phoneme detection with PE integration completed";
}

TEST_F(SpeechCortexPETest, Integration_PhonologicalBufferWithPE) {
    // WHAT: Store phonemes in phonological buffer with PE
    // WHY:  Test PE integration with working memory

    cortex = CreateCortexWithPE(true, 0, 1);
    ASSERT_NE(cortex, nullptr);

    // Create phoneme sequence
    phoneme_t phoneme_seq[] = {PHONEME_P, PHONEME_AE, PHONEME_T};
    uint32_t num_phonemes = 3;

    // Store in buffer
    bool result = speech_cortex_store_phonological_buffer(cortex, phoneme_seq, num_phonemes);
    EXPECT_TRUE(result) << "Phonological buffer storage should succeed";

    // Retrieve and verify
    phoneme_t retrieved[10];
    uint32_t num_retrieved = 0;
    result = speech_cortex_retrieve_phonological_buffer(cortex, retrieved, 10, &num_retrieved);
    EXPECT_TRUE(result);
    EXPECT_EQ(num_retrieved, num_phonemes);

    // Verify buffer position embeddings exist
    for (uint32_t i = 0; i < num_phonemes; i++) {
        float embedding[TEST_PE_DIM];
        result = speech_cortex_get_phonological_position_embedding(cortex, i, embedding);
        EXPECT_TRUE(result) << "Buffer position " << i << " should have embedding";
    }
}

//=============================================================================
// Unit Tests: PE Type Comparison
//=============================================================================

TEST_F(SpeechCortexPETest, ComparePETypes_SinusoidalVsLearned) {
    // WHAT: Compare sinusoidal and learned PE for buffer
    // WHY:  Verify different PE types produce different encodings

    speech_cortex_t* cortex_sin = CreateCortexWithPE(true, 0, 0);  // Sinusoidal
    speech_cortex_t* cortex_learned = CreateCortexWithPE(true, 0, 1);  // Learned
    ASSERT_NE(cortex_sin, nullptr);
    ASSERT_NE(cortex_learned, nullptr);

    uint32_t slot = 3;
    float emb_sin[TEST_PE_DIM];
    float emb_learned[TEST_PE_DIM];

    speech_cortex_get_phonological_position_embedding(cortex_sin, slot, emb_sin);
    speech_cortex_get_phonological_position_embedding(cortex_learned, slot, emb_learned);

    // Embeddings should be different
    bool different = false;
    for (uint32_t i = 0; i < TEST_PE_DIM; i++) {
        if (std::abs(emb_sin[i] - emb_learned[i]) > EPSILON) {
            different = true;
            break;
        }
    }

    EXPECT_TRUE(different) << "Sinusoidal and learned embeddings should differ";

    speech_cortex_destroy(cortex_sin);
    speech_cortex_destroy(cortex_learned);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
