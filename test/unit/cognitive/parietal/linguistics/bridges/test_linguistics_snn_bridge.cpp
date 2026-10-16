/**
 * @file test_linguistics_snn_bridge.cpp
 * @brief Unit tests for SNN Integration Bridge for Parietal Linguistics
 * @date 2026-01-31
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "cognitive/parietal/linguistics/bridges/nimcp_parietal_linguistics_snn_bridge.h"
}

/* ============================================================================
 * TEST FIXTURES
 * ============================================================================ */

class SNNBridgeTest : public ::testing::Test {
protected:
    ling_snn_bridge_t* bridge = nullptr;

    void SetUp() override {
        ling_snn_bridge_config_t config = ling_snn_bridge_default_config();
        config.enable_mesh = false;  // Disable mesh for unit tests
        bridge = ling_snn_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);
    }

    void TearDown() override {
        if (bridge) {
            ling_snn_bridge_destroy(bridge);
            bridge = nullptr;
        }
    }
};

/* ============================================================================
 * LIFECYCLE TESTS
 * ============================================================================ */

TEST_F(SNNBridgeTest, DefaultConfigHasValidValues) {
    ling_snn_bridge_config_t config = ling_snn_bridge_default_config();

    EXPECT_EQ(config.neurons_per_phoneme, LING_SNN_NEURONS_PER_PHONEME);
    EXPECT_EQ(config.neurons_per_spatial, LING_SNN_NEURONS_PER_SPATIAL);
    EXPECT_EQ(config.neurons_per_number, LING_SNN_NEURONS_PER_NUMBER);
    EXPECT_EQ(config.phoneme_encoding, SNN_ENCODE_TEMPORAL);
    EXPECT_EQ(config.spatial_encoding, SNN_ENCODE_POPULATION);
    EXPECT_EQ(config.number_encoding, SNN_ENCODE_RATE);
    EXPECT_GT(config.max_firing_rate, 0.0f);
    EXPECT_GT(config.base_precision, 0.0f);
    EXPECT_LE(config.base_precision, 1.0f);
}

TEST_F(SNNBridgeTest, CreateWithNullConfigUsesDefaults) {
    ling_snn_bridge_t* test_bridge = ling_snn_bridge_create(nullptr);
    ASSERT_NE(test_bridge, nullptr);
    ling_snn_bridge_destroy(test_bridge);
}

TEST_F(SNNBridgeTest, DestroyNullIsSafe) {
    ling_snn_bridge_destroy(nullptr);  // Should not crash
}

/* ============================================================================
 * PHONEME ENCODING TESTS
 * ============================================================================ */

TEST_F(SNNBridgeTest, EncodePhoneme_GeneratesValidSpikeTrain) {
    ling_phoneme_encoding_t result;

    int ret = ling_snn_encode_phoneme(bridge, LING_PHONEME_P, 50.0f, &result);

    ASSERT_EQ(ret, LING_SNN_ERR_OK);
    EXPECT_EQ(result.phoneme, LING_PHONEME_P);
    EXPECT_EQ(result.duration_ms, 50.0f);
    EXPECT_GT(result.spike_train.spike_count, 0u);
    EXPECT_GE(result.encoding_confidence, 0.0f);
    EXPECT_LE(result.encoding_confidence, 1.0f);
}

TEST_F(SNNBridgeTest, EncodePhoneme_VowelHasFormants) {
    ling_phoneme_encoding_t result;

    int ret = ling_snn_encode_phoneme(bridge, LING_PHONEME_IY, 100.0f, &result);

    ASSERT_EQ(ret, LING_SNN_ERR_OK);
    EXPECT_GT(result.formants[0], 0.0f);  // F1
    EXPECT_GT(result.formants[1], 0.0f);  // F2
}

TEST_F(SNNBridgeTest, EncodePhoneme_RejectsInvalidPhoneme) {
    ling_phoneme_encoding_t result;

    int ret = ling_snn_encode_phoneme(bridge, (ling_phoneme_t)999, 50.0f, &result);

    EXPECT_EQ(ret, LING_SNN_ERR_INVALID_PHONEME);
}

TEST_F(SNNBridgeTest, EncodePhoneme_RejectsNullResult) {
    int ret = ling_snn_encode_phoneme(bridge, LING_PHONEME_P, 50.0f, nullptr);

    EXPECT_EQ(ret, LING_SNN_ERR_NULL);
}

TEST_F(SNNBridgeTest, EncodePhonemeSequence_EncodesMultiplePhonemes) {
    ling_phoneme_t phonemes[] = {LING_PHONEME_H, LING_PHONEME_EH, LING_PHONEME_L, LING_PHONEME_OW};
    float durations[] = {30.0f, 80.0f, 40.0f, 100.0f};
    ling_phoneme_encoding_t results[4];

    int ret = ling_snn_encode_phoneme_sequence(bridge, phonemes, durations, 4, results);

    ASSERT_EQ(ret, LING_SNN_ERR_OK);
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(results[i].phoneme, phonemes[i]);
        EXPECT_GT(results[i].spike_train.spike_count, 0u);
    }
}

TEST_F(SNNBridgeTest, DecodePhoneme_DecodesFromSpikeTrain) {
    // First encode a phoneme
    ling_phoneme_encoding_t encoding;
    int ret = ling_snn_encode_phoneme(bridge, LING_PHONEME_M, 50.0f, &encoding);
    ASSERT_EQ(ret, LING_SNN_ERR_OK);

    // Now decode
    ling_phoneme_t decoded;
    float confidence;
    ret = ling_snn_decode_phoneme(bridge, &encoding.spike_train, &decoded, &confidence);

    ASSERT_EQ(ret, LING_SNN_ERR_OK);
    EXPECT_GE(confidence, 0.0f);
    EXPECT_LE(confidence, 1.0f);
}

TEST_F(SNNBridgeTest, DecodePhoneme_SilenceForEmptySpikeTrain) {
    ling_spike_train_t empty_train = {0};

    ling_phoneme_t decoded;
    float confidence;
    int ret = ling_snn_decode_phoneme(bridge, &empty_train, &decoded, &confidence);

    ASSERT_EQ(ret, LING_SNN_ERR_OK);
    EXPECT_EQ(decoded, LING_PHONEME_SILENCE);
}

/* ============================================================================
 * SPATIAL WORD ENCODING TESTS
 * ============================================================================ */

TEST_F(SNNBridgeTest, EncodeSpatialWord_GeneratesPopulationActivity) {
    ling_spatial_encoding_t result;

    int ret = ling_snn_encode_spatial_word(bridge, SPATIAL_PREP_NEAR, 1.0f, &result);

    ASSERT_EQ(ret, LING_SNN_ERR_OK);
    EXPECT_EQ(result.preposition, SPATIAL_PREP_NEAR);
    EXPECT_GT(result.winner_rate, 0.0f);
    EXPECT_EQ(result.winner_idx, (uint32_t)SPATIAL_PREP_NEAR);
    EXPECT_GE(result.encoding_precision, 0.0f);
}

TEST_F(SNNBridgeTest, EncodeSpatialWord_ActivationScalesActivity) {
    ling_spatial_encoding_t result_low, result_high;

    ling_snn_encode_spatial_word(bridge, SPATIAL_PREP_LEFT, 0.5f, &result_low);
    ling_snn_encode_spatial_word(bridge, SPATIAL_PREP_LEFT, 1.0f, &result_high);

    EXPECT_GT(result_high.winner_rate, result_low.winner_rate);
}

TEST_F(SNNBridgeTest, EncodeSpatialWord_HasGaussianProfile) {
    ling_spatial_encoding_t result;

    ling_snn_encode_spatial_word(bridge, SPATIAL_PREP_IN, 1.0f, &result);

    // Activity at target should be highest
    float target_activity = result.population_activity[SPATIAL_PREP_IN];

    // Adjacent prepositions should have lower activity
    if (SPATIAL_PREP_IN > 0) {
        EXPECT_LT(result.population_activity[SPATIAL_PREP_IN - 1], target_activity);
    }
    if (SPATIAL_PREP_IN < SPATIAL_PREPOSITION_COUNT - 1) {
        EXPECT_LT(result.population_activity[SPATIAL_PREP_IN + 1], target_activity);
    }
}

TEST_F(SNNBridgeTest, DecodeSpatialWord_FindsWinner) {
    // Create population activity with clear winner
    float activity[LING_SNN_NUM_SPATIAL_WORDS] = {0};
    activity[SPATIAL_PREP_ABOVE] = 0.9f;
    activity[SPATIAL_PREP_BELOW] = 0.1f;

    spatial_preposition_t decoded;
    float confidence;
    int ret = ling_snn_decode_spatial_word(bridge, activity, &decoded, &confidence);

    ASSERT_EQ(ret, LING_SNN_ERR_OK);
    EXPECT_EQ(decoded, SPATIAL_PREP_ABOVE);
    EXPECT_GT(confidence, 0.5f);
}

TEST_F(SNNBridgeTest, GetSpatialPopulation_ReturnsActivity) {
    // First encode to populate
    ling_spatial_encoding_t encoding;
    ling_snn_encode_spatial_word(bridge, SPATIAL_PREP_ON, 1.0f, &encoding);

    // Get population activity
    float activity[16];
    int ret = ling_snn_get_spatial_population(bridge, SPATIAL_PREP_ON, activity, 16);

    ASSERT_EQ(ret, LING_SNN_ERR_OK);
    // Should have some non-zero activity
    float sum = 0.0f;
    for (int i = 0; i < 16; i++) sum += activity[i];
    EXPECT_GT(sum, 0.0f);
}

/* ============================================================================
 * NUMBER WORD ENCODING TESTS
 * ============================================================================ */

TEST_F(SNNBridgeTest, EncodeNumber_GeneratesWeberFechnerRate) {
    ling_number_encoding_t result;

    int ret = ling_snn_encode_number(bridge, 10.0f, NUM_WORD_CARDINAL, &result);

    ASSERT_EQ(ret, LING_SNN_ERR_OK);
    EXPECT_EQ(result.type, NUM_WORD_CARDINAL);
    EXPECT_EQ(result.magnitude, 10.0f);
    EXPECT_GT(result.firing_rate, 0.0f);
    EXPECT_GT(result.uncertainty, 0.0f);  // Weber uncertainty
}

TEST_F(SNNBridgeTest, EncodeNumber_LogarithmicScaling) {
    ling_number_encoding_t result_10, result_100;

    ling_snn_encode_number(bridge, 10.0f, NUM_WORD_CARDINAL, &result_10);
    ling_snn_encode_number(bridge, 100.0f, NUM_WORD_CARDINAL, &result_100);

    // Rate should increase with magnitude but sublinearly (log)
    EXPECT_GT(result_100.firing_rate, result_10.firing_rate);

    // Check sublinear: rate ratio < magnitude ratio
    float mag_ratio = 100.0f / 10.0f;  // 10x
    float rate_ratio = result_100.firing_rate / result_10.firing_rate;
    EXPECT_LT(rate_ratio, mag_ratio);
}

TEST_F(SNNBridgeTest, EncodeNumber_IsApproximateForLargeNumbers) {
    ling_number_encoding_t result_small, result_large;

    ling_snn_encode_number(bridge, 3.0f, NUM_WORD_CARDINAL, &result_small);
    ling_snn_encode_number(bridge, 100.0f, NUM_WORD_CARDINAL, &result_large);

    EXPECT_FALSE(result_small.is_approximate);  // Subitizing range
    EXPECT_TRUE(result_large.is_approximate);   // Beyond subitizing
}

TEST_F(SNNBridgeTest, DecodeNumber_InvertsEncoding) {
    // Encode
    ling_number_encoding_t encoding;
    ling_snn_encode_number(bridge, 25.0f, NUM_WORD_CARDINAL, &encoding);

    // Decode
    float magnitude, uncertainty;
    int ret = ling_snn_decode_number(bridge, encoding.firing_rate,
                                      NUM_WORD_CARDINAL, &magnitude, &uncertainty);

    ASSERT_EQ(ret, LING_SNN_ERR_OK);
    // Should recover approximate magnitude (Weber-Fechner is inexact)
    EXPECT_NEAR(magnitude, 25.0f, 10.0f);  // Within reasonable error
    EXPECT_GT(uncertainty, 0.0f);
}

/* ============================================================================
 * POPULATION MANAGEMENT TESTS
 * ============================================================================ */

TEST_F(SNNBridgeTest, StepPhonemePopulation_DecaysRates) {
    // Encode to get some activity
    ling_phoneme_encoding_t encoding;
    ling_snn_encode_phoneme(bridge, LING_PHONEME_S, 50.0f, &encoding);

    float rate_before = ling_snn_get_population_rate(bridge, 0);

    // Step simulation
    ling_snn_step_phoneme_population(bridge, 10.0f);

    float rate_after = ling_snn_get_population_rate(bridge, 0);

    // Rate should decay
    EXPECT_LT(rate_after, rate_before);
}

TEST_F(SNNBridgeTest, GetPopulationRate_ReturnsValidRate) {
    float rate = ling_snn_get_population_rate(bridge, 0);  // Phoneme population

    EXPECT_GE(rate, 0.0f);
}

TEST_F(SNNBridgeTest, GetPopulationSynchrony_ReturnsValidValue) {
    float sync = ling_snn_get_population_synchrony(bridge, 1);  // Spatial population

    EXPECT_GE(sync, 0.0f);
    EXPECT_LE(sync, 1.0f);
}

/* ============================================================================
 * MESH HANDLER TESTS
 * ============================================================================ */

TEST_F(SNNBridgeTest, MeshHandler_GetHandlerReturnsValid) {
    linguistics_mesh_handler_t handler;

    int ret = ling_snn_get_mesh_handler(bridge, &handler);

    ASSERT_EQ(ret, LING_SNN_ERR_OK);
    EXPECT_NE(handler.process, nullptr);
    EXPECT_NE(handler.update, nullptr);
    EXPECT_NE(handler.get_precision, nullptr);
    EXPECT_EQ(handler.ctx, bridge);
}

TEST_F(SNNBridgeTest, MeshHandler_ProcessSpatialRequest) {
    linguistics_mesh_handler_t handler;
    ling_snn_get_mesh_handler(bridge, &handler);

    linguistics_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = LING_REQUEST_PARSE_SPATIAL;
    request.spatial.preposition = SPATIAL_PREP_NEAR;

    linguistics_belief_t belief;
    int ret = handler.process(handler.ctx, &request, &belief);

    ASSERT_EQ(ret, LING_SNN_ERR_OK);
    EXPECT_GE(belief.certainty, 0.0f);
    EXPECT_LE(belief.certainty, 1.0f);
    EXPECT_GT(belief.precision, 0.0f);
}

TEST_F(SNNBridgeTest, MeshHandler_ProcessNumberRequest) {
    linguistics_mesh_handler_t handler;
    ling_snn_get_mesh_handler(bridge, &handler);

    linguistics_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = LING_REQUEST_PARSE_NUMBER;
    request.number.value = 42.0f;
    request.number.type = NUM_WORD_CARDINAL;

    linguistics_belief_t belief;
    int ret = handler.process(handler.ctx, &request, &belief);

    ASSERT_EQ(ret, LING_SNN_ERR_OK);
    EXPECT_GE(belief.certainty, 0.0f);
    EXPECT_GT(belief.belief_vector[0], 0.0f);  // Firing rate
}

TEST_F(SNNBridgeTest, MeshHandler_UpdateModifiesBelief) {
    linguistics_mesh_handler_t handler;
    ling_snn_get_mesh_handler(bridge, &handler);

    // First process to get initial belief
    linguistics_request_t request;
    memset(&request, 0, sizeof(request));
    request.type = LING_REQUEST_PARSE_SPATIAL;
    request.spatial.preposition = SPATIAL_PREP_NEAR;

    linguistics_belief_t initial_belief;
    handler.process(handler.ctx, &request, &initial_belief);

    // Create neighbor beliefs
    linguistics_belief_t neighbors[2];
    memset(neighbors, 0, sizeof(neighbors));
    neighbors[0].certainty = 0.9f;
    neighbors[0].precision = 0.8f;
    neighbors[1].certainty = 0.7f;
    neighbors[1].precision = 0.6f;

    // Update
    linguistics_belief_t updated;
    int ret = handler.update(handler.ctx, neighbors, 2, &updated);

    ASSERT_EQ(ret, LING_SNN_ERR_OK);
    EXPECT_GE(updated.certainty, 0.0f);
    EXPECT_LE(updated.certainty, 1.0f);
}

TEST_F(SNNBridgeTest, MeshHandler_GetPrecisionReturnsValidValue) {
    linguistics_mesh_handler_t handler;
    ling_snn_get_mesh_handler(bridge, &handler);

    float precision = handler.get_precision(handler.ctx);

    EXPECT_GE(precision, LING_SNN_PRECISION_FLOOR);
    EXPECT_LE(precision, LING_SNN_PRECISION_CEILING);
}

/* ============================================================================
 * STATISTICS TESTS
 * ============================================================================ */

TEST_F(SNNBridgeTest, GetStats_ReturnsValidMetrics) {
    // Do some operations
    ling_phoneme_encoding_t phoneme_result;
    ling_snn_encode_phoneme(bridge, LING_PHONEME_T, 50.0f, &phoneme_result);

    ling_spatial_encoding_t spatial_result;
    ling_snn_encode_spatial_word(bridge, SPATIAL_PREP_LEFT, 1.0f, &spatial_result);

    // Get stats
    ling_snn_bridge_stats_t stats;
    int ret = ling_snn_bridge_get_stats(bridge, &stats);

    ASSERT_EQ(ret, LING_SNN_ERR_OK);
    EXPECT_GT(stats.total_encodings, 0u);
    EXPECT_GT(stats.total_spikes, 0u);
}

TEST_F(SNNBridgeTest, ResetStats_ClearsCounters) {
    // Do some operations
    ling_phoneme_encoding_t result;
    ling_snn_encode_phoneme(bridge, LING_PHONEME_D, 50.0f, &result);

    // Reset
    ling_snn_bridge_reset_stats(bridge);

    // Check stats are cleared
    ling_snn_bridge_stats_t stats;
    ling_snn_bridge_get_stats(bridge, &stats);

    EXPECT_EQ(stats.total_encodings, 0u);
    EXPECT_EQ(stats.total_spikes, 0u);
}

/* ============================================================================
 * UTILITY TESTS
 * ============================================================================ */

TEST_F(SNNBridgeTest, PhonemeName_ReturnsValidStrings) {
    EXPECT_STREQ(ling_snn_phoneme_name(LING_PHONEME_P), "P");
    EXPECT_STREQ(ling_snn_phoneme_name(LING_PHONEME_IY), "IY");
    EXPECT_STREQ(ling_snn_phoneme_name(LING_PHONEME_CH), "CH");
    EXPECT_STREQ(ling_snn_phoneme_name(LING_PHONEME_SILENCE), "SIL");
}

TEST_F(SNNBridgeTest, ParsePhoneme_RecognizesValidPhonemes) {
    ling_phoneme_t phoneme;

    EXPECT_EQ(ling_snn_parse_phoneme("P", &phoneme), 0);
    EXPECT_EQ(phoneme, LING_PHONEME_P);

    EXPECT_EQ(ling_snn_parse_phoneme("TH", &phoneme), 0);
    EXPECT_EQ(phoneme, LING_PHONEME_TH);

    EXPECT_EQ(ling_snn_parse_phoneme("NG", &phoneme), 0);
    EXPECT_EQ(phoneme, LING_PHONEME_NG);
}

TEST_F(SNNBridgeTest, ParsePhoneme_RejectsInvalid) {
    ling_phoneme_t phoneme;

    EXPECT_EQ(ling_snn_parse_phoneme("INVALID", &phoneme), -1);
    EXPECT_EQ(ling_snn_parse_phoneme("", &phoneme), -1);
}

TEST_F(SNNBridgeTest, PhonemeClass_ReturnsCorrectClass) {
    EXPECT_STREQ(ling_snn_phoneme_class(LING_PHONEME_IY), "VOWEL");
    EXPECT_STREQ(ling_snn_phoneme_class(LING_PHONEME_P), "STOP");
    EXPECT_STREQ(ling_snn_phoneme_class(LING_PHONEME_S), "FRICATIVE");
    EXPECT_STREQ(ling_snn_phoneme_class(LING_PHONEME_M), "NASAL");
    EXPECT_STREQ(ling_snn_phoneme_class(LING_PHONEME_L), "APPROXIMANT");
    EXPECT_STREQ(ling_snn_phoneme_class(LING_PHONEME_CH), "AFFRICATE");
    EXPECT_STREQ(ling_snn_phoneme_class(LING_PHONEME_AY), "DIPHTHONG");
    EXPECT_STREQ(ling_snn_phoneme_class(LING_PHONEME_SILENCE), "SPECIAL");
}

TEST_F(SNNBridgeTest, PhonemeIsVoiced_ReturnsCorrectValue) {
    // Vowels are voiced
    EXPECT_TRUE(ling_snn_phoneme_is_voiced(LING_PHONEME_IY));
    EXPECT_TRUE(ling_snn_phoneme_is_voiced(LING_PHONEME_AH));

    // Voiceless stops
    EXPECT_FALSE(ling_snn_phoneme_is_voiced(LING_PHONEME_P));
    EXPECT_FALSE(ling_snn_phoneme_is_voiced(LING_PHONEME_T));
    EXPECT_FALSE(ling_snn_phoneme_is_voiced(LING_PHONEME_K));

    // Voiced stops
    EXPECT_TRUE(ling_snn_phoneme_is_voiced(LING_PHONEME_B));
    EXPECT_TRUE(ling_snn_phoneme_is_voiced(LING_PHONEME_D));
    EXPECT_TRUE(ling_snn_phoneme_is_voiced(LING_PHONEME_G));

    // Voiceless fricatives
    EXPECT_FALSE(ling_snn_phoneme_is_voiced(LING_PHONEME_F));
    EXPECT_FALSE(ling_snn_phoneme_is_voiced(LING_PHONEME_S));

    // Voiced fricatives
    EXPECT_TRUE(ling_snn_phoneme_is_voiced(LING_PHONEME_V));
    EXPECT_TRUE(ling_snn_phoneme_is_voiced(LING_PHONEME_Z));

    // Nasals are voiced
    EXPECT_TRUE(ling_snn_phoneme_is_voiced(LING_PHONEME_M));
    EXPECT_TRUE(ling_snn_phoneme_is_voiced(LING_PHONEME_N));
}

/* ============================================================================
 * ERROR HANDLING TESTS
 * ============================================================================ */

TEST_F(SNNBridgeTest, GetLastError_ReturnsNonNull) {
    const char* error = ling_snn_bridge_get_last_error();
    EXPECT_NE(error, nullptr);
}

TEST_F(SNNBridgeTest, NullBridge_ReturnsError) {
    ling_phoneme_encoding_t result;
    int ret = ling_snn_encode_phoneme(nullptr, LING_PHONEME_P, 50.0f, &result);
    EXPECT_EQ(ret, LING_SNN_ERR_NULL);

    ling_spatial_encoding_t spatial_result;
    ret = ling_snn_encode_spatial_word(nullptr, SPATIAL_PREP_NEAR, 1.0f, &spatial_result);
    EXPECT_EQ(ret, LING_SNN_ERR_NULL);

    ling_number_encoding_t number_result;
    ret = ling_snn_encode_number(nullptr, 10.0f, NUM_WORD_CARDINAL, &number_result);
    EXPECT_EQ(ret, LING_SNN_ERR_NULL);
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
